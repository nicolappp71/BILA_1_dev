#include "dxf_parser.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static const char *TAG = "DXF";

#define INITIAL_CAPACITY  256
#define ARC_DEG_PER_SEG   5.0f   /* degrees per tessellation segment */

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ─── Vertex for LWPOLYLINE (temp buffer on normal heap) ─────────────────── */
typedef struct { float x, y, bulge; } poly_vert_t;

/* ─── Line reader ─────────────────────────────────────────────────────────── */
static const char *next_line(const char *buf, size_t len, size_t *pos, size_t *line_len)
{
    if (*pos >= len) return NULL;
    const char *start = buf + *pos;
    size_t i = *pos;
    while (i < len && buf[i] != '\n') i++;
    *line_len = i - *pos;
    *pos = (i < len) ? i + 1 : i;
    return start;
}

static void trim(char *s, size_t n)
{
    size_t end = 0;
    for (size_t i = 0; i < n; i++)
        if (s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n')
            end = i + 1;
    s[end] = '\0';
    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    if (start) memmove(s, s + start, strlen(s + start) + 1);
}

/* ─── Output helpers ──────────────────────────────────────────────────────── */
static esp_err_t add_seg(dxf_segment_t **segs, int *count, int *cap,
                          float x0, float y0, float x1, float y1)
{
    if (*count >= *cap) {
        int new_cap = *cap * 2;
        dxf_segment_t *tmp = heap_caps_realloc(*segs,
            new_cap * sizeof(dxf_segment_t), MALLOC_CAP_SPIRAM);
        if (!tmp) return ESP_ERR_NO_MEM;
        *segs = tmp;
        *cap  = new_cap;
    }
    (*segs)[(*count)++] = (dxf_segment_t){x0, y0, x1, y1};
    return ESP_OK;
}

/* ─── Bulge → arc tessellation ────────────────────────────────────────────── *
 * DXF bulge = tan(theta/4), positive = CCW, negative = CW.                  *
 * Formula for center:                                                         *
 *   d = chord * (1 - b²) / (4b)    (signed dist. midpoint→center, left +)   *
 *   perp_left = (-dy, dx) / chord                                            *
 * ─────────────────────────────────────────────────────────────────────────── */
static esp_err_t tess_bulge(dxf_segment_t **segs, int *count, int *cap,
                             float px1, float py1, float px2, float py2, float b)
{
    if (fabsf(b) < 1e-4f)
        return add_seg(segs, count, cap, px1, py1, px2, py2);

    float dx    = px2 - px1,  dy    = py2 - py1;
    float chord = sqrtf(dx*dx + dy*dy);
    if (chord < 1e-6f) return ESP_OK;

    float d  = chord * (1.0f - b*b) / (4.0f * b);
    float cx = (px1+px2)*0.5f + d * (-dy / chord);
    float cy = (py1+py2)*0.5f + d * ( dx / chord);
    float r  = chord * (1.0f + b*b) / (4.0f * fabsf(b));

    float a1 = atan2f(py1 - cy, px1 - cx);
    float a2 = atan2f(py2 - cy, px2 - cx);

    float span;
    if (b > 0.0f) {          /* CCW */
        if (a2 < a1) a2 += 2.0f * (float)M_PI;
        span = a2 - a1;
    } else {                  /* CW  */
        if (a1 < a2) a1 += 2.0f * (float)M_PI;
        span = -(a1 - a2);   /* negative = CW direction */
    }

    int n = (int)(fabsf(span) * 180.0f / (float)M_PI / ARC_DEG_PER_SEG);
    if (n < 4)  n = 4;
    if (n > 72) n = 72;

    float fpx = px1, fpy = py1;
    for (int i = 1; i <= n; i++) {
        float t  = (float)i / (float)n;
        float a  = a1 + t * span;
        float nx = cx + r * cosf(a);
        float ny = cy + r * sinf(a);
        esp_err_t err = add_seg(segs, count, cap, fpx, fpy, nx, ny);
        if (err != ESP_OK) return err;
        fpx = nx; fpy = ny;
    }
    return ESP_OK;
}

/* ─── ARC entity tessellation ─────────────────────────────────────────────── */
static esp_err_t tess_arc(dxf_segment_t **segs, int *count, int *cap,
                           float cx, float cy, float r,
                           float start_deg, float end_deg)
{
    float s = start_deg * (float)M_PI / 180.0f;
    float e = end_deg   * (float)M_PI / 180.0f;
    if (e < s) e += 2.0f * (float)M_PI;   /* always CCW in DXF */
    float span = e - s;

    int n = (int)(span * 180.0f / (float)M_PI / ARC_DEG_PER_SEG);
    if (n < 4)   n = 4;
    if (n > 256) n = 256;

    float fpx = cx + r * cosf(s);
    float fpy = cy + r * sinf(s);
    for (int i = 1; i <= n; i++) {
        float t  = (float)i / (float)n;
        float a  = s + t * span;
        float nx = cx + r * cosf(a);
        float ny = cy + r * sinf(a);
        add_seg(segs, count, cap, fpx, fpy, nx, ny);
        fpx = nx; fpy = ny;
    }
    return ESP_OK;
}

/* ─── Main parser ─────────────────────────────────────────────────────────── */
esp_err_t dxf_parse(const char *buf, size_t len,
                    const char *layer_filter,
                    dxf_segment_t **out_segs, int *out_count)
{
    if (!buf || !out_segs || !out_count) return ESP_ERR_INVALID_ARG;

    int cap = INITIAL_CAPACITY;
    dxf_segment_t *segs = heap_caps_malloc(cap * sizeof(dxf_segment_t), MALLOC_CAP_SPIRAM);
    if (!segs) return ESP_ERR_NO_MEM;
    int count = 0;
    size_t pos = 0;

    typedef enum { S_IDLE, S_IN_LINE, S_IN_ARC, S_IN_LWPOLY } pstate_t;
    pstate_t state = S_IDLE;

    char group_str[16];
    char value_str[128];
    char layer[64] = {0};

    /* LINE */
    float lx0=0, ly0=0, lx1=0, ly1=0;
    bool  lhx0=false, lhy0=false, lhx1=false, lhy1=false;

    /* ARC */
    float acx=0, acy=0, ar=0, astart=0, aend=0;
    bool  ahcx=false, ahcy=false, ahr=false, ahs=false, ahe=false;

    /* LWPOLYLINE */
    poly_vert_t *pverts  = NULL;
    int          pcount  = 0;
    int          pcap    = 0;
    bool         pclosed = false;
    bool         phx     = false;   /* waiting for code 20 */
    float        pcurx   = 0;

    /* ── flush helper (called when group 0 signals end of entity) ── */
#define FLUSH_ENTITY() do {                                                     \
    if (strcmp(layer, layer_filter) == 0) {                                     \
        int _before = count;                                                     \
        if (state == S_IN_LINE && lhx0 && lhy0 && lhx1 && lhy1) {             \
            add_seg(&segs, &count, &cap, lx0, ly0, lx1, ly1);                  \
            ESP_LOGD("DXF", "LINE  layer=%s → %d seg", layer, count-_before);  \
        } else if (state == S_IN_ARC && ahcx && ahcy && ahr && ahs && ahe) {   \
            tess_arc(&segs, &count, &cap, acx, acy, ar, astart, aend);         \
            ESP_LOGI("DXF", "ARC   layer=%s r=%.2f %.1f°→%.1f° → %d seg",     \
                     layer, ar, astart, aend, count-_before);                   \
        } else if (state == S_IN_LWPOLY && pcount >= 2) {                       \
            for (int _i = 0; _i < pcount-1; _i++)                              \
                tess_bulge(&segs, &count, &cap,                                 \
                    pverts[_i].x, pverts[_i].y,                                 \
                    pverts[_i+1].x, pverts[_i+1].y, pverts[_i].bulge);         \
            if (pclosed)                                                         \
                tess_bulge(&segs, &count, &cap,                                 \
                    pverts[pcount-1].x, pverts[pcount-1].y,                     \
                    pverts[0].x, pverts[0].y, pverts[pcount-1].bulge);          \
            ESP_LOGI("DXF", "LWPOLY layer=%s verts=%d closed=%d → %d seg",     \
                     layer, pcount, (int)pclosed, count-_before);               \
        }                                                                        \
    }                                                                            \
    /* reset per-entity state */                                                 \
    layer[0]='\0';                                                               \
    lhx0=lhy0=lhx1=lhy1=false;                                                  \
    ahcx=ahcy=ahr=ahs=ahe=false;                                                 \
    pclosed=false; pcount=0; phx=false;                                          \
} while(0)

    while (pos < len) {
        const char *gl; size_t gl_len;
        gl = next_line(buf, len, &pos, &gl_len);
        if (!gl) break;
        if (gl_len == 0 || gl_len >= sizeof(group_str)) continue;
        memcpy(group_str, gl, gl_len);
        group_str[gl_len] = '\0';
        trim(group_str, gl_len);

        const char *vl; size_t vl_len;
        vl = next_line(buf, len, &pos, &vl_len);
        if (!vl) break;
        size_t vc = vl_len < sizeof(value_str)-1 ? vl_len : sizeof(value_str)-1;
        memcpy(value_str, vl, vc);
        value_str[vc] = '\0';
        trim(value_str, vc);

        int grp = atoi(group_str);

        /* ── Entity separator ── */
        if (grp == 0) {
            FLUSH_ENTITY();
            if      (strcmp(value_str, "LINE")       == 0) state = S_IN_LINE;
            else if (strcmp(value_str, "ARC")        == 0) state = S_IN_ARC;
            else if (strcmp(value_str, "LWPOLYLINE") == 0) state = S_IN_LWPOLY;
            else                                           state = S_IDLE;
            continue;
        }

        /* ── Layer (common to all entities) ── */
        if (grp == 8) {
            size_t l = strlen(value_str);
            if (l >= sizeof(layer)) l = sizeof(layer) - 1;
            memcpy(layer, value_str, l);
            layer[l] = '\0';
            continue;
        }

        /* ── Entity-specific codes ── */
        switch (state) {

        case S_IN_LINE:
            switch (grp) {
                case 10: lx0 = strtof(value_str, NULL); lhx0 = true; break;
                case 20: ly0 = strtof(value_str, NULL); lhy0 = true; break;
                case 11: lx1 = strtof(value_str, NULL); lhx1 = true; break;
                case 21: ly1 = strtof(value_str, NULL); lhy1 = true; break;
            }
            break;

        case S_IN_ARC:
            switch (grp) {
                case 10: acx    = strtof(value_str, NULL); ahcx = true; break;
                case 20: acy    = strtof(value_str, NULL); ahcy = true; break;
                case 40: ar     = strtof(value_str, NULL); ahr  = true; break;
                case 50: astart = strtof(value_str, NULL); ahs  = true; break;
                case 51: aend   = strtof(value_str, NULL); ahe  = true; break;
            }
            break;

        case S_IN_LWPOLY:
            switch (grp) {
                case 70: {
                    int flags = atoi(value_str);
                    pclosed = (flags & 1) != 0;
                    break;
                }
                case 90: {
                    int n = atoi(value_str);
                    if (n > pcap) {
                        poly_vert_t *tmp = realloc(pverts, n * sizeof(poly_vert_t));
                        if (tmp) { pverts = tmp; pcap = n; }
                    }
                    break;
                }
                case 10:
                    pcurx = strtof(value_str, NULL);
                    phx   = true;
                    break;
                case 20:
                    if (phx) {
                        if (pcount >= pcap) {
                            int nc = pcap ? pcap * 2 : 64;
                            poly_vert_t *tmp = realloc(pverts, nc * sizeof(poly_vert_t));
                            if (tmp) { pverts = tmp; pcap = nc; }
                        }
                        if (pcount < pcap) {
                            pverts[pcount].x     = pcurx;
                            pverts[pcount].y     = strtof(value_str, NULL);
                            pverts[pcount].bulge = 0.0f;
                            pcount++;
                        }
                        phx = false;
                    }
                    break;
                case 42:
                    /* bulge applies to the segment from the previous vertex to the next */
                    if (pcount > 0)
                        pverts[pcount-1].bulge = strtof(value_str, NULL);
                    break;
            }
            break;

        default:
            break;
        }
    }

    /* Flush last entity (file may not end with group 0) */
    FLUSH_ENTITY();
#undef FLUSH_ENTITY

    if (pverts) free(pverts);

    ESP_LOGI(TAG, "Parse completato: %d segmenti layer '%s'", count, layer_filter);
    *out_segs  = segs;
    *out_count = count;
    return ESP_OK;
}
