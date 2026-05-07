#include "dxf_parser.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "DXF";

#define INITIAL_CAPACITY 64

// Legge la prossima riga dal buffer, ritorna puntatore alla riga e avanza *pos.
// Ritorna NULL a fine buffer.
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
    // Rimuove spazi, \r, \n in-place (lavora su copia nul-terminata)
    size_t end = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n')
            end = i + 1;
    }
    s[end] = '\0';
    // ltrim
    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    if (start) memmove(s, s + start, strlen(s + start) + 1);
}

esp_err_t dxf_parse(const char *buf, size_t len,
                    const char *layer_filter,
                    dxf_segment_t **out_segs, int *out_count)
{
    if (!buf || !out_segs || !out_count) return ESP_ERR_INVALID_ARG;

    int capacity = INITIAL_CAPACITY;
    dxf_segment_t *segs = heap_caps_malloc(capacity * sizeof(dxf_segment_t), MALLOC_CAP_SPIRAM);
    if (!segs) return ESP_ERR_NO_MEM;

    int count = 0;
    size_t pos = 0;

    // Stato parser
    typedef enum { S_IDLE, S_IN_LINE } pstate_t;
    pstate_t state = S_IDLE;

    char group_str[16];
    char value_str[64];
    char layer[64] = {0};
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool has_x0 = false, has_y0 = false, has_x1 = false, has_y1 = false;

    while (pos < len) {
        // Leggi gruppo
        const char *gl; size_t gl_len;
        gl = next_line(buf, len, &pos, &gl_len);
        if (!gl) break;
        if (gl_len == 0 || gl_len >= sizeof(group_str)) continue;
        memcpy(group_str, gl, gl_len);
        group_str[gl_len] = '\0';
        trim(group_str, gl_len);

        // Leggi valore
        const char *vl; size_t vl_len;
        vl = next_line(buf, len, &pos, &vl_len);
        if (!vl) break;
        size_t vl_copy = vl_len < sizeof(value_str) - 1 ? vl_len : sizeof(value_str) - 1;
        memcpy(value_str, vl, vl_copy);
        value_str[vl_copy] = '\0';
        trim(value_str, vl_copy);

        int grp = atoi(group_str);

        if (grp == 0) {
            // Fine entità precedente
            if (state == S_IN_LINE) {
                if (strcmp(layer, layer_filter) == 0 &&
                    has_x0 && has_y0 && has_x1 && has_y1) {
                    if (count >= capacity) {
                        capacity *= 2;
                        dxf_segment_t *tmp = heap_caps_realloc(segs,
                            capacity * sizeof(dxf_segment_t), MALLOC_CAP_SPIRAM);
                        if (!tmp) { free(segs); return ESP_ERR_NO_MEM; }
                        segs = tmp;
                    }
                    segs[count++] = (dxf_segment_t){x0, y0, x1, y1};
                }
            }
            // Nuova entità
            layer[0] = '\0';
            x0 = y0 = x1 = y1 = 0;
            has_x0 = has_y0 = has_x1 = has_y1 = false;
            state = (strcmp(value_str, "LINE") == 0) ? S_IN_LINE : S_IDLE;
            continue;
        }

        if (state != S_IN_LINE) continue;

        switch (grp) {
            case  8: snprintf(layer, sizeof(layer), "%s", value_str); break;
            case 10: x0 = strtof(value_str, NULL); has_x0 = true; break;
            case 20: y0 = strtof(value_str, NULL); has_y0 = true; break;
            case 11: x1 = strtof(value_str, NULL); has_x1 = true; break;
            case 21: y1 = strtof(value_str, NULL); has_y1 = true; break;
            default: break;
        }
    }

    ESP_LOGI(TAG, "Parse completato: %d segmenti layer '%s'", count, layer_filter);
    *out_segs  = segs;
    *out_count = count;
    return ESP_OK;
}
