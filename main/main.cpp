
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "bsp/touch.h"
#include "esp_brookesia.hpp"
#include "app_examples/phone/squareline/src/phone_app_squareline.hpp"
#include "apps.h"
#include "esp_heap_caps.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_app_desc.h" // Necessario per leggere i metadati
#include "esp_wifi.h"
#include "esp_netif.h"
extern "C"
{
#include "mode.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "web_server.h"
#include "banchetto_manager.h"
#include "settings_manager.h"
#include "json_parser.h"
#include "http_client.h"
#include "key_manager.h"
#include "rfid_manager.h"
#include "gpio_manager.h"
#include "i2c_manager.h"
#include "scanner_manager.h"
#include "battery_manager.h"
#include "log_manager.h"
#include "ble_manager.h"
#include "ota_manager.h"
#include "offline_journal.h"
#include "tagliatubi_manager.h"
#include "collaudo_manager.h"
#include "bilancia_manager.h"
#include "spalmatrice_manager.h"
}

// ─── UI Spalmatrice ───────────────────────────────────────────────────────────

static lv_obj_t   *s_spal_screen     = NULL;
static lv_obj_t   *s_spal_status_lbl = NULL;
static lv_obj_t   *s_spal_canvas     = NULL;
static lv_color_t *s_spal_canvas_buf = NULL;
static lv_obj_t   *s_spal_zoom_lbl   = NULL;

#define SPAL_CANVAS_W 1024
#define SPAL_CANVAS_H  560

// ── View state (zoom/pan persistente) ────────────────────────────────────────
static float s_fit_x_min = 0, s_fit_x_max = 1;
static float s_fit_y_min = 0, s_fit_y_max = 1;
static float s_fit_scale = 1.0f;    // scala base per fit-to-screen
static float s_fit_off_x = 0, s_fit_off_y = 0;  // offsets base
static float s_zoom      = 1.0f;    // moltiplicatore zoom corrente
static float s_pan_cx    = 0, s_pan_cy = 0;  // centro vista in coord DXF
static bool  s_view_init = false;   // true dopo il primo fit

// ── Drag / pan ────────────────────────────────────────────────────────────────
static lv_coord_t s_drag_last_x  = -1, s_drag_last_y = -1;
static lv_coord_t s_press_start_x = -1, s_press_start_y = -1;
static bool       s_dragging     = false;
static bool       s_draw_labels  = true;   // false durante drag per velocità
static uint32_t   s_last_draw_ms = 0;      // throttle ridisegno

// ── Selezione punto ───────────────────────────────────────────────────────────
static float s_sel_x = 0, s_sel_y = 0;
static int   s_sel_idx = -1;
static bool  s_sel_valid = false;

// ── Search / goto punto ───────────────────────────────────────────────────────
static lv_obj_t *s_search_lbl    = NULL;   // label nel campo header
static lv_obj_t *s_search_popup  = NULL;   // popup tastiera numerica
static char      s_search_buf[8] = {0};

// ── Popup editing coordinate ──────────────────────────────────────────────────
static lv_obj_t *s_edit_popup      = NULL;  // overlay scuro
static lv_obj_t *s_edit_popup_inner = NULL; // popup vero (sopra overlay)
static lv_obj_t *s_edit_x_lbl   = NULL;
static lv_obj_t *s_edit_y_lbl   = NULL;
static bool      s_edit_x_active = true;  // quale campo è attivo
static char      s_edit_x_buf[16] = {0};
static char      s_edit_y_buf[16] = {0};
static float     s_edit_orig_x = 0, s_edit_orig_y = 0;

// ── Storia modifiche (undo/redo) ──────────────────────────────────────────────
#define SPAL_HIST_MAX 32
struct SpalHistEntry { int idx; float old_x, old_y, new_x, new_y; };
static SpalHistEntry s_hist[SPAL_HIST_MAX];
static int s_hist_count = 0;  // voci valide
static int s_hist_pos   = 0;  // posizione corrente (0 = prima voce)
static bool s_hist_inhibit = false;  // true durante undo/redo per non pushare di nuovo

// ── Trasformazioni DXF ↔ schermo ─────────────────────────────────────────────
static lv_coord_t spal_to_sx(float x)
{
    float scale = s_fit_scale * s_zoom;
    return (lv_coord_t)(SPAL_CANVAS_W * 0.5f + (x - s_pan_cx) * scale);
}
static lv_coord_t spal_to_sy(float y)
{
    float scale = s_fit_scale * s_zoom;
    return (lv_coord_t)(SPAL_CANVAS_H * 0.5f - (y - s_pan_cy) * scale);
}
static float spal_to_dx(lv_coord_t sx)
{
    float scale = s_fit_scale * s_zoom;
    return s_pan_cx + (sx - SPAL_CANVAS_W * 0.5f) / scale;
}
static float spal_to_dy(lv_coord_t sy)
{
    float scale = s_fit_scale * s_zoom;
    return s_pan_cy - (sy - SPAL_CANVAS_H * 0.5f) / scale;
}

// Forward declarations
static void spal_draw_path_on_canvas(void);
static void spal_edit_popup_open(float x, float y);
static void spal_search_popup_open(void);

static void spal_draw_path_on_canvas(void)
{
    const dxf_segment_t *segs = NULL;
    int nsegs = 0;
    spalmatrice_manager_get_segments(&segs, &nsegs);
    if (!segs || nsegs < 1) return;

    // ── Calcola bounds (solo al primo caricamento o reset zoom) ──────────────
    if (!s_view_init) {
        float *xs = (float*)heap_caps_malloc(nsegs * 2 * sizeof(float), MALLOC_CAP_SPIRAM);
        float *ys = (float*)heap_caps_malloc(nsegs * 2 * sizeof(float), MALLOC_CAP_SPIRAM);
        if (!xs || !ys) { if (xs) heap_caps_free(xs); if (ys) heap_caps_free(ys); return; }
        for (int i = 0; i < nsegs; i++) {
            xs[i*2] = segs[i].x0; xs[i*2+1] = segs[i].x1;
            ys[i*2] = segs[i].y0; ys[i*2+1] = segs[i].y1;
        }
        int np = nsegs * 2;
        std::sort(xs, xs + np);
        std::sort(ys, ys + np);

        auto gap_bounds = [](const float *v, int n, float *lo, float *hi) {
            float total = v[n-1] - v[0];
            if (total < 0.001f) { *lo = v[0]; *hi = v[n-1]; return; }
            int gi = 1; float mg = 0;
            for (int i = 1; i < n; i++) { float g = v[i]-v[i-1]; if (g > mg) { mg = g; gi = i; } }
            if (mg > 0.1f * total) {
                if (gi >= n - gi) { *lo = v[0];  *hi = v[gi-1]; }
                else              { *lo = v[gi]; *hi = v[n-1];  }
            } else { *lo = v[0]; *hi = v[n-1]; }
        };

        gap_bounds(xs, np, &s_fit_x_min, &s_fit_x_max);
        gap_bounds(ys, np, &s_fit_y_min, &s_fit_y_max);
        heap_caps_free(xs); heap_caps_free(ys);

        float rx = s_fit_x_max - s_fit_x_min; if (rx < 0.001f) rx = 0.001f;
        float ry = s_fit_y_max - s_fit_y_min; if (ry < 0.001f) ry = 0.001f;
        const int pad = 48;
        float draw_w = (float)(SPAL_CANVAS_W - 2 * pad);
        float draw_h = (float)(SPAL_CANVAS_H - 2 * pad);
        s_fit_scale = (draw_w / rx < draw_h / ry) ? (draw_w / rx) : (draw_h / ry);
        // Centro vista = centro dei bounds
        s_pan_cx = (s_fit_x_min + s_fit_x_max) * 0.5f;
        s_pan_cy = (s_fit_y_min + s_fit_y_max) * 0.5f;
        s_zoom   = 1.0f;
        s_view_init = true;
    }

    float x_min = s_fit_x_min, x_max = s_fit_x_max;
    float y_min = s_fit_y_min, y_max = s_fit_y_max;

    lv_canvas_fill_bg(s_spal_canvas, lv_color_hex(0x0A0A0A), LV_OPA_COVER);

    // ── Griglia centrata sull'origine ────────────────────────────────────────
    {
        lv_draw_line_dsc_t gd;
        lv_draw_line_dsc_init(&gd);
        gd.color = lv_color_hex(0x1A2A1A);
        gd.width = 1;
        gd.opa   = LV_OPA_COVER;
        // Linee orizzontali e verticali di riferimento ogni 50mm
        float scale = s_fit_scale * s_zoom;
        float step_mm = 50.0f;
        // X lines
        float x0g = floorf(spal_to_dx(0) / step_mm) * step_mm;
        float x1g = ceilf (spal_to_dx(SPAL_CANVAS_W) / step_mm) * step_mm;
        for (float gx = x0g; gx <= x1g; gx += step_mm) {
            lv_coord_t sx = spal_to_sx(gx);
            if (sx < 0 || sx >= SPAL_CANVAS_W) continue;
            lv_point_t pts[2] = {{sx, 0}, {sx, SPAL_CANVAS_H}};
            lv_canvas_draw_line(s_spal_canvas, pts, 2, &gd);
        }
        // Y lines
        float y0g = floorf(spal_to_dy(SPAL_CANVAS_H) / step_mm) * step_mm;
        float y1g = ceilf (spal_to_dy(0) / step_mm) * step_mm;
        for (float gy = y0g; gy <= y1g; gy += step_mm) {
            lv_coord_t sy = spal_to_sy(gy);
            if (sy < 0 || sy >= SPAL_CANVAS_H) continue;
            lv_point_t pts[2] = {{0, sy}, {SPAL_CANVAS_W, sy}};
            lv_canvas_draw_line(s_spal_canvas, pts, 2, &gd);
        }
        (void)scale;
    }

    // ── Percorso ─────────────────────────────────────────────────────────────
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = lv_color_hex(0x00DD55);
    ld.width = 2;
    ld.opa   = LV_OPA_COVER;

    lv_point_t *poly = (lv_point_t *)heap_caps_malloc(
        (nsegs + 1) * sizeof(lv_point_t), MALLOC_CAP_SPIRAM);
    if (!poly) { ESP_LOGE("MAIN", "Alloc poly fallito"); return; }

    int poly_n = 0;
    int prev_i = -1;
    for (int i = 0; i < nsegs; i++) {
        bool out = (segs[i].x0 > x_max && segs[i].x1 > x_max) ||
                   (segs[i].x0 < x_min && segs[i].x1 < x_min) ||
                   (segs[i].y0 > y_max && segs[i].y1 > y_max) ||
                   (segs[i].y0 < y_min && segs[i].y1 < y_min);
        if (out) {
            if (poly_n >= 2) lv_canvas_draw_line(s_spal_canvas, poly, (uint32_t)poly_n, &ld);
            poly_n = 0; prev_i = -1;
            continue;
        }
        bool connected = false;
        if (poly_n > 0 && prev_i >= 0) {
            float ddx = segs[i].x0 - segs[prev_i].x1;
            float ddy = segs[i].y0 - segs[prev_i].y1;
            connected = (ddx*ddx + ddy*ddy < 0.01f * 0.01f);
        }
        if (!connected) {
            if (poly_n >= 2) lv_canvas_draw_line(s_spal_canvas, poly, (uint32_t)poly_n, &ld);
            poly[0].x = spal_to_sx(segs[i].x0);
            poly[0].y = spal_to_sy(segs[i].y0);
            poly_n = 1;
        }
        poly[poly_n].x = spal_to_sx(segs[i].x1);
        poly[poly_n].y = spal_to_sy(segs[i].y1);
        poly_n++;
        prev_i = i;
    }
    if (poly_n >= 2)
        lv_canvas_draw_line(s_spal_canvas, poly, (uint32_t)poly_n, &ld);
    heap_caps_free(poly);

    // ── Origine (0,0) ────────────────────────────────────────────────────────
    {
        lv_coord_t ox = spal_to_sx(0.0f);
        lv_coord_t oy = spal_to_sy(0.0f);
        if (ox >= 0 && ox < SPAL_CANVAS_W && oy >= 0 && oy < SPAL_CANVAS_H) {
            lv_draw_line_dsc_t cd;
            lv_draw_line_dsc_init(&cd);
            cd.color = lv_color_hex(0x00FFFF); cd.width = 1; cd.opa = LV_OPA_COVER;
            lv_point_t h[2] = {{(lv_coord_t)(ox-8), oy}, {(lv_coord_t)(ox+8), oy}};
            lv_canvas_draw_line(s_spal_canvas, h, 2, &cd);
            lv_point_t v[2] = {{ox, (lv_coord_t)(oy-8)}, {ox, (lv_coord_t)(oy+8)}};
            lv_canvas_draw_line(s_spal_canvas, v, 2, &cd);
            lv_draw_label_dsc_t ld2; lv_draw_label_dsc_init(&ld2);
            ld2.color = lv_color_hex(0x00FFFF); ld2.font = &lv_font_montserrat_12;
            lv_coord_t tx = (ox + 10 + 60 > SPAL_CANVAS_W) ? ox - 70 : ox + 10;
            lv_coord_t ty = (oy < 10) ? oy + 4 : oy - 16;
            lv_canvas_draw_text(s_spal_canvas, tx, ty, 60, &ld2, "0,0");
        }
    }


    // ── Punto 0 (path_chain) ─────────────────────────────────────────────────
    {
        const path_point_t *path = NULL; int npts = 0;
        spalmatrice_manager_get_path(&path, &npts);
        if (path && npts > 0) {
            float p0x = path[0].x, p0y = path[0].y;
            if (p0x >= x_min && p0x <= x_max && p0y >= y_min && p0y <= y_max) {
                lv_coord_t sx = spal_to_sx(p0x), sy = spal_to_sy(p0y);
                lv_draw_arc_dsc_t ad; lv_draw_arc_dsc_init(&ad);
                ad.color = lv_color_hex(0xFF8800); ad.width = 2;
                lv_canvas_draw_arc(s_spal_canvas, sx, sy, 6, 0, 360, &ad);
                lv_draw_label_dsc_t ld2; lv_draw_label_dsc_init(&ld2);
                ld2.color = lv_color_hex(0xFF8800); ld2.font = &lv_font_montserrat_12;
                char txt[40]; snprintf(txt, sizeof(txt), "P0 %.1f,%.1f", p0x, p0y);
                lv_coord_t tx = (sx + 10 + 130 > SPAL_CANVAS_W) ? sx - 140 : sx + 10;
                lv_coord_t ty = (sy < 10) ? sy + 10 : sy - 16;
                lv_canvas_draw_text(s_spal_canvas, tx, ty, 130, &ld2, txt);
            }
        }
    }

    // ── Punti percorso: cerchietto + numero, scalati con zoom ────────────────
    if (!s_draw_labels) goto draw_selected;
    {
        const path_point_t *path = NULL; int npts = 0;
        spalmatrice_manager_get_path(&path, &npts);
        if (path && npts > 0) {
            // Raggio cerchio e font proporzionali allo zoom
            int   dot_r;
            const lv_font_t *num_font;
            if      (s_zoom >= 4.0f) { dot_r = 6; num_font = &lv_font_montserrat_20; }
            else if (s_zoom >= 2.0f) { dot_r = 4; num_font = &lv_font_montserrat_16; }
            else                     { dot_r = 3; num_font = &lv_font_montserrat_12; }

            int min_px = (int)(dot_r * 3);
            if (min_px < 8) min_px = 8;

            lv_draw_arc_dsc_t ptarc; lv_draw_arc_dsc_init(&ptarc);
            ptarc.color = lv_color_hex(0x4488FF);
            ptarc.width = 1;

            lv_draw_label_dsc_t ldn; lv_draw_label_dsc_init(&ldn);
            ldn.color = lv_color_hex(0xCCDDFF);
            ldn.font  = num_font;

            lv_point_t *used = (lv_point_t *)heap_caps_malloc(
                npts * sizeof(lv_point_t), MALLOC_CAP_SPIRAM);
            int n_used = 0;

            for (int i = 0; i < npts; i++) {
                if (path[i].x < x_min || path[i].x > x_max ||
                    path[i].y < y_min || path[i].y > y_max) continue;
                lv_coord_t sx = spal_to_sx(path[i].x);
                lv_coord_t sy = spal_to_sy(path[i].y);
                if (sx < 0 || sx >= SPAL_CANVAS_W || sy < 0 || sy >= SPAL_CANVAS_H) continue;
                // Dedup
                bool skip = false;
                if (used) {
                    for (int u = 0; u < n_used && !skip; u++) {
                        int ddx = (int)sx - (int)used[u].x;
                        int ddy = (int)sy - (int)used[u].y;
                        if (ddx*ddx + ddy*ddy < min_px*min_px) skip = true;
                    }
                }
                if (skip) continue;
                if (used) { used[n_used].x = sx; used[n_used].y = sy; n_used++; }
                // Cerchietto azzurro
                lv_canvas_draw_arc(s_spal_canvas, sx, sy, (lv_coord_t)dot_r, 0, 360, &ptarc);
                // Numero sopra
                char num[12]; snprintf(num, sizeof(num), "%d", i);
                lv_canvas_draw_text(s_spal_canvas, (lv_coord_t)(sx + dot_r + 2),
                                    (lv_coord_t)(sy - dot_r - 14), 44, &ldn, num);
            }
            if (used) heap_caps_free(used);
        }
    }

    draw_selected:
    // ── Punto selezionato (cerchio rosso) ────────────────────────────────────
    if (s_sel_valid) {
        lv_coord_t sx = spal_to_sx(s_sel_x), sy = spal_to_sy(s_sel_y);
        lv_draw_arc_dsc_t ad; lv_draw_arc_dsc_init(&ad);
        ad.color = lv_color_hex(0xFF3333); ad.width = 3;
        lv_canvas_draw_arc(s_spal_canvas, sx, sy, 9, 0, 360, &ad);
    }
}

// ─── Popup editing coordinate ─────────────────────────────────────────────────

static void spal_edit_update_display(void)
{
    if (s_edit_x_lbl) {
        lv_obj_set_style_border_color(s_edit_x_lbl,
            s_edit_x_active ? lv_color_hex(0x4A90E2) : lv_color_hex(0x333333), 0);
        lv_obj_t *xl = lv_obj_get_child(s_edit_x_lbl, 0);
        if (xl) lv_label_set_text_fmt(xl, "X: %s", strlen(s_edit_x_buf) ? s_edit_x_buf : "-");
    }
    if (s_edit_y_lbl) {
        lv_obj_set_style_border_color(s_edit_y_lbl,
            !s_edit_x_active ? lv_color_hex(0x4A90E2) : lv_color_hex(0x333333), 0);
        lv_obj_t *yl = lv_obj_get_child(s_edit_y_lbl, 0);
        if (yl) lv_label_set_text_fmt(yl, "Y: %s", strlen(s_edit_y_buf) ? s_edit_y_buf : "-");
    }
}

static void spal_edit_key_cb(lv_event_t *e)
{
    char digit = (char)(intptr_t)lv_event_get_user_data(e);
    char *buf = s_edit_x_active ? s_edit_x_buf : s_edit_y_buf;
    size_t len = strlen(buf);
    if (len >= 10) return;
    if (digit == '.') {
        if (strchr(buf, '.')) return;  // già presente
    }
    if (digit == '-') {
        if (len > 0) return;  // solo all'inizio
    }
    buf[len] = digit; buf[len+1] = '\0';
    spal_edit_update_display();
}

static void spal_edit_bs_cb(lv_event_t *e)
{
    (void)e;
    char *buf = s_edit_x_active ? s_edit_x_buf : s_edit_y_buf;
    size_t len = strlen(buf);
    if (len > 0) { buf[len-1] = '\0'; spal_edit_update_display(); }
}

static void spal_edit_field_cb(lv_event_t *e)
{
    bool *which = (bool *)lv_event_get_user_data(e);
    s_edit_x_active = *which;
    spal_edit_update_display();
}

static void spal_edit_close(void)
{
    if (s_edit_popup_inner) { lv_obj_del(s_edit_popup_inner); s_edit_popup_inner = NULL; }
    if (s_edit_popup)       { lv_obj_del(s_edit_popup);       s_edit_popup = NULL; }
    s_edit_x_lbl = s_edit_y_lbl = NULL;
}

static void spal_edit_confirm_cb(lv_event_t *e)
{
    (void)e;
    float nx = strlen(s_edit_x_buf) ? strtof(s_edit_x_buf, NULL) : s_edit_orig_x;
    float ny = strlen(s_edit_y_buf) ? strtof(s_edit_y_buf, NULL) : s_edit_orig_y;
    spal_edit_close();

    esp_err_t err = spalmatrice_manager_edit_path_point(s_sel_idx, nx, ny);
    if (err == ESP_OK) {
        // Salva in storia (troncando eventuali redo in avanti)
        if (!s_hist_inhibit) {
            if (s_hist_pos < SPAL_HIST_MAX) {
                s_hist[s_hist_pos] = { s_sel_idx, s_edit_orig_x, s_edit_orig_y, nx, ny };
                s_hist_pos++;
                s_hist_count = s_hist_pos;
            } else {
                // Stack pieno: shifta tutto a sinistra
                memmove(&s_hist[0], &s_hist[1], (SPAL_HIST_MAX-1) * sizeof(SpalHistEntry));
                s_hist[SPAL_HIST_MAX-1] = { s_sel_idx, s_edit_orig_x, s_edit_orig_y, nx, ny };
                s_hist_count = SPAL_HIST_MAX;
            }
        }
        s_sel_x = nx; s_sel_y = ny;
        spalmatrice_manager_save_to_sd();
        spal_draw_path_on_canvas();
        if (s_spal_status_lbl)
            lv_label_set_text(s_spal_status_lbl, "Salvato su SD");
    }
}

static void spal_edit_annulla_cb(lv_event_t *e) { (void)e; spal_edit_close(); }

static lv_obj_t *spal_edit_btn(lv_obj_t *parent, const char *lbl, lv_color_t bg,
                                lv_event_cb_t cb, void *ud)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, lbl);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);
    return btn;
}

static bool s_edit_x_flag = true, s_edit_y_flag = false;

static void spal_edit_popup_open(float x, float y)
{
    if (s_edit_popup) return;
    s_edit_orig_x = x; s_edit_orig_y = y;
    s_edit_x_active = true;
    snprintf(s_edit_x_buf, sizeof(s_edit_x_buf), "%.3f", x);
    snprintf(s_edit_y_buf, sizeof(s_edit_y_buf), "%.3f", y);

    // Overlay scuro — su lv_layer_sys per stare sopra l'header fisso
    lv_obj_t *ov = lv_obj_create(lv_layer_sys());
    s_edit_popup = ov;
    lv_obj_set_size(ov, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ov, 180, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_set_style_radius(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);

    // Popup centrato — su lv_layer_sys per stare sopra l'header fisso
    lv_obj_t *pop = lv_obj_create(lv_layer_sys());
    s_edit_popup_inner = pop;
    lv_obj_set_size(pop, 420, 530);
    lv_obj_center(pop);
    lv_obj_move_foreground(pop);
    lv_obj_set_style_bg_color(pop, lv_color_hex(0x141E30), 0);
    lv_obj_set_style_bg_opa(pop, 255, 0);
    lv_obj_set_style_border_color(pop, lv_color_hex(0x4A90E2), 0);
    lv_obj_set_style_border_width(pop, 2, 0);
    lv_obj_set_style_radius(pop, 14, 0);
    lv_obj_set_style_pad_all(pop, 14, 0);
    lv_obj_clear_flag(pop, LV_OBJ_FLAG_SCROLLABLE);

    // Titolo
    lv_obj_t *tit = lv_label_create(pop);
    lv_label_set_text_fmt(tit, "Modifica punto %d", s_sel_idx);
    lv_obj_set_style_text_font(tit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(tit, lv_color_hex(0xAABBFF), 0);
    lv_obj_set_width(tit, LV_PCT(100));
    lv_obj_set_style_text_align(tit, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(tit, LV_ALIGN_TOP_MID, 0, 0);

    // Campo X
    s_edit_x_lbl = lv_obj_create(pop);
    lv_obj_set_size(s_edit_x_lbl, LV_PCT(100), 44);
    lv_obj_align(s_edit_x_lbl, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_bg_color(s_edit_x_lbl, lv_color_hex(0x0D1526), 0);
    lv_obj_set_style_bg_opa(s_edit_x_lbl, 255, 0);
    lv_obj_set_style_border_width(s_edit_x_lbl, 2, 0);
    lv_obj_set_style_border_color(s_edit_x_lbl, lv_color_hex(0x4A90E2), 0);
    lv_obj_set_style_radius(s_edit_x_lbl, 6, 0);
    lv_obj_set_style_pad_all(s_edit_x_lbl, 6, 0);
    lv_obj_clear_flag(s_edit_x_lbl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_edit_x_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_edit_x_lbl, spal_edit_field_cb, LV_EVENT_CLICKED, &s_edit_x_flag);
    {
        lv_obj_t *xl = lv_label_create(s_edit_x_lbl);
        lv_label_set_text_fmt(xl, "X: %s", s_edit_x_buf);
        lv_obj_set_style_text_font(xl, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(xl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(xl);
    }

    // Campo Y
    s_edit_y_lbl = lv_obj_create(pop);
    lv_obj_set_size(s_edit_y_lbl, LV_PCT(100), 44);
    lv_obj_align(s_edit_y_lbl, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_set_style_bg_color(s_edit_y_lbl, lv_color_hex(0x0D1526), 0);
    lv_obj_set_style_bg_opa(s_edit_y_lbl, 255, 0);
    lv_obj_set_style_border_width(s_edit_y_lbl, 2, 0);
    lv_obj_set_style_border_color(s_edit_y_lbl, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(s_edit_y_lbl, 6, 0);
    lv_obj_set_style_pad_all(s_edit_y_lbl, 6, 0);
    lv_obj_clear_flag(s_edit_y_lbl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_edit_y_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_edit_y_lbl, spal_edit_field_cb, LV_EVENT_CLICKED, &s_edit_y_flag);
    {
        lv_obj_t *yl = lv_label_create(s_edit_y_lbl);
        lv_label_set_text_fmt(yl, "Y: %s", s_edit_y_buf);
        lv_obj_set_style_text_font(yl, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(yl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(yl);
    }

    // Tastiera numerica 4×4 (0-9, ., -, ⌫)
    lv_obj_t *grid = lv_obj_create(pop);
    lv_obj_set_size(grid, LV_PCT(100), 260);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 130);
    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_gap(grid, 6, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    // 4 colonne: 7 8 9 / 4 5 6 / 1 2 3 / - 0 . ⌫
    static lv_coord_t cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t rows[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    // Righe 0-2: 7 8 9 / 4 5 6 / 1 2 3  (colonne 0-2, colonna 3 vuota)
    const char *keys9[] = {"7","8","9","4","5","6","1","2","3"};
    for (int i = 0; i < 9; i++) {
        lv_obj_t *btn = spal_edit_btn(grid, keys9[i], lv_color_hex(0x2C5282),
                                      spal_edit_key_cb, (void*)(intptr_t)keys9[i][0]);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, i%3, 1,
                                  LV_GRID_ALIGN_STRETCH, i/3, 1);
    }
    // Riga 3: - 0 . ⌫
    struct { const char *l; lv_color_t c; lv_event_cb_t cb; char ch; } row3[] = {
        {"-",              lv_color_hex(0x2C5282), spal_edit_key_cb, '-'},
        {"0",              lv_color_hex(0x2C5282), spal_edit_key_cb, '0'},
        {".",              lv_color_hex(0x2C5282), spal_edit_key_cb, '.'},
        {LV_SYMBOL_BACKSPACE, lv_color_hex(0xF39C12), spal_edit_bs_cb,  0 },
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = spal_edit_btn(grid, row3[i].l, row3[i].c,
                                      row3[i].cb, (void*)(intptr_t)row3[i].ch);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, i, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
    }

    // Annulla / Conferma
    lv_obj_t *btn_ann = spal_edit_btn(pop, "Annulla", lv_color_hex(0xAA2222),
                                      spal_edit_annulla_cb, NULL);
    lv_obj_set_size(btn_ann, 160, 50);
    lv_obj_align(btn_ann, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *btn_ok = spal_edit_btn(pop, "Conferma", lv_color_hex(0x1A7A3A),
                                     spal_edit_confirm_cb, NULL);
    lv_obj_set_size(btn_ok, 160, 50);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

// ─── Touch sul canvas → pan (PRESSING) + selezione (CLICKED) ─────────────────
static void spal_canvas_touch_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);
    lv_coord_t cx = pt.x;
    lv_coord_t cy = (lv_coord_t)(pt.y - 90);  // 50 brookesia + 40 header
    ESP_LOGI("TOUCH", "event=%d cx=%d cy=%d dragging=%d edit_popup=%p",
             (int)code, (int)cx, (int)cy, (int)s_dragging, s_edit_popup);

    // ── Pan via drag ─────────────────────────────────────────────────────────
    if (code == LV_EVENT_PRESSING) {
        if (s_edit_popup) return;
        // Salva posizione iniziale del tocco (solo al primo PRESSING)
        if (s_press_start_x < 0) { s_press_start_x = cx; s_press_start_y = cy; }
        if (s_drag_last_x >= 0) {
            float scale = s_fit_scale * s_zoom;
            float ddx = (cx - s_drag_last_x) / scale;
            float ddy = (cy - s_drag_last_y) / scale;
            if (ddx*ddx + ddy*ddy > 0.5f) {
                s_pan_cx -= ddx;
                s_pan_cy += ddy;
                s_dragging = true;
                uint32_t now = lv_tick_get();
                if (now - s_last_draw_ms >= 60) {
                    s_draw_labels = false;
                    spal_draw_path_on_canvas();
                    s_draw_labels = true;
                    s_last_draw_ms = now;
                }
            }
        }
        s_drag_last_x = cx;
        s_drag_last_y = cy;
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        lv_coord_t px = s_press_start_x, py = s_press_start_y;
        s_drag_last_x = s_drag_last_y = -1;
        s_press_start_x = s_press_start_y = -1;

        if (s_dragging) {
            s_dragging = false;
            spal_draw_path_on_canvas();
            return;
        }

        // Tap: usa la posizione di inizio pressione per la selezione
        if (s_edit_popup) return;
        if (px < 0 || px >= SPAL_CANVAS_W || py < 0 || py >= SPAL_CANVAS_H) return;
        cx = px; cy = py;  // usa coordinate di inizio tocco
        goto do_select;
    }

    if (code == LV_EVENT_CLICKED) {
        if (s_edit_popup) return;
        if (cx < 0 || cx >= SPAL_CANVAS_W || cy < 0 || cy >= SPAL_CANVAS_H) return;
        goto do_select;
    }

    return;

    do_select:
    {
        const path_point_t *path = NULL;
        int npts = 0;
        spalmatrice_manager_get_path(&path, &npts);
        if (!path || npts == 0) return;

        // Cerca in pixel — indipendente dallo zoom, tolleranza fissa 30px
        // Editing abilitato solo a zoom 16x o 32x
        if (s_zoom < 16.0f) {
            ESP_LOGI("MAIN", "Editing disabilitato sotto 16x (zoom=%.0fx)", s_zoom);
            return;
        }
        // Tolleranza maggiore ad alto zoom perché i punti sono ben distanziati
        const int TOL_PX = (s_zoom >= 32.0f) ? 80 : 60;
        int best_d2 = TOL_PX * TOL_PX;
        float best_x = 0, best_y = 0;
        int   best_i = -1;

        float x_min = s_fit_x_min, x_max = s_fit_x_max;
        float y_min = s_fit_y_min, y_max = s_fit_y_max;

        for (int i = 0; i < npts; i++) {
            if (path[i].x < x_min || path[i].x > x_max ||
                path[i].y < y_min || path[i].y > y_max) continue;
            lv_coord_t sx = spal_to_sx(path[i].x);
            lv_coord_t sy = spal_to_sy(path[i].y);
            int ddx = (int)cx - (int)sx;
            int ddy = (int)cy - (int)sy;
            int d2  = ddx*ddx + ddy*ddy;
            if (d2 < best_d2) { best_d2 = d2; best_x = path[i].x; best_y = path[i].y; best_i = i; }
        }

        // Log sempre del punto più vicino (anche fuori tolleranza) per debug
        {
            int abs_best_d2 = TOL_PX * TOL_PX * 100;
            int abs_best_i = -1;
            for (int i = 0; i < npts; i++) {
                if (path[i].x < x_min || path[i].x > x_max ||
                    path[i].y < y_min || path[i].y > y_max) continue;
                lv_coord_t sx = spal_to_sx(path[i].x);
                lv_coord_t sy = spal_to_sy(path[i].y);
                int ddx = (int)cx - (int)sx;
                int ddy = (int)cy - (int)sy;
                int d2  = ddx*ddx + ddy*ddy;
                if (d2 < abs_best_d2) { abs_best_d2 = d2; abs_best_i = i; }
            }
            if (abs_best_i >= 0) {
                lv_coord_t bsx = spal_to_sx(path[abs_best_i].x);
                lv_coord_t bsy = spal_to_sy(path[abs_best_i].y);
                ESP_LOGI("MAIN", "Punto piu vicino: [%d] screen=(%d,%d) tocco=(%d,%d) dist=%dpx",
                         abs_best_i, (int)bsx, (int)bsy, (int)cx, (int)cy, (int)sqrtf((float)abs_best_d2));
            }
        }

        if (best_i >= 0) {
            ESP_LOGI("MAIN", "Selezionato punto %d (%.3f, %.3f)", best_i, best_x, best_y);
            s_sel_x = best_x; s_sel_y = best_y; s_sel_idx = best_i; s_sel_valid = true;
            spal_draw_path_on_canvas();
            spal_edit_popup_open(best_x, best_y);
        } else {
            ESP_LOGI("MAIN", "Nessun punto entro %dpx", TOL_PX);
        }
    }
}

// ─── Forward declarations ─────────────────────────────────────────────────────
static void spal_update_zoom_lbl(void);
static void spal_draw_path_on_canvas(void);

// ─── Search / goto punto ──────────────────────────────────────────────────────
static void spal_search_update_lbl(void)
{
    if (!s_search_lbl) return;
    if (strlen(s_search_buf) == 0)
        lv_label_set_text(s_search_lbl, "---");
    else
        lv_label_set_text(s_search_lbl, s_search_buf);
}

static void spal_search_goto(void)
{
    if (strlen(s_search_buf) == 0) return;
    int idx = atoi(s_search_buf);
    const path_point_t *path = NULL;
    int npts = 0;
    spalmatrice_manager_get_path(&path, &npts);
    if (!path || idx < 0 || idx >= npts) {
        ESP_LOGW("MAIN", "Punto %d non esiste (max %d)", idx, npts-1);
        return;
    }
    // Centra la vista sul punto a 32x
    s_zoom   = 32.0f;
    s_pan_cx = path[idx].x;
    s_pan_cy = path[idx].y;
    spal_update_zoom_lbl();
    // Seleziona anche il punto
    s_sel_x = path[idx].x; s_sel_y = path[idx].y;
    s_sel_idx = idx; s_sel_valid = true;
    ESP_LOGI("MAIN", "Goto punto %d (%.3f, %.3f)", idx, path[idx].x, path[idx].y);
    spal_draw_path_on_canvas();
}

static void spal_search_close(void)
{
    if (s_search_popup) { lv_obj_del(s_search_popup); s_search_popup = NULL; }
}

static void spal_search_key_cb(lv_event_t *e)
{
    char digit = (char)(intptr_t)lv_event_get_user_data(e);
    size_t len = strlen(s_search_buf);
    if (len >= 5) return;
    s_search_buf[len] = digit; s_search_buf[len+1] = '\0';
    spal_search_update_lbl();
    // aggiorna anche il display nel popup
    lv_obj_t *disp = (lv_obj_t *)lv_obj_get_user_data(s_search_popup);
    if (disp) lv_label_set_text(disp, s_search_buf);
}

static void spal_search_bs_cb(lv_event_t *e)
{
    (void)e;
    size_t len = strlen(s_search_buf);
    if (len > 0) { s_search_buf[len-1] = '\0'; spal_search_update_lbl(); }
    lv_obj_t *disp = (lv_obj_t *)lv_obj_get_user_data(s_search_popup);
    if (disp) lv_label_set_text(disp, strlen(s_search_buf) ? s_search_buf : "---");
}

static void spal_search_confirm_cb(lv_event_t *e)
{
    (void)e;
    spal_search_close();
    spal_search_goto();
}

static void spal_search_annulla_cb(lv_event_t *e)
{
    (void)e;
    spal_search_close();
}

static void spal_search_popup_open(void)
{
    if (s_search_popup) return;
    memset(s_search_buf, 0, sizeof(s_search_buf));
    spal_search_update_lbl();

    lv_obj_t *pop = lv_obj_create(lv_layer_sys());
    s_search_popup = pop;
    lv_obj_set_size(pop, 320, 420);
    lv_obj_center(pop);
    lv_obj_move_foreground(pop);
    lv_obj_set_style_bg_color(pop, lv_color_hex(0x141E30), 0);
    lv_obj_set_style_bg_opa(pop, 255, 0);
    lv_obj_set_style_border_color(pop, lv_color_hex(0x4A90E2), 0);
    lv_obj_set_style_border_width(pop, 2, 0);
    lv_obj_set_style_radius(pop, 14, 0);
    lv_obj_set_style_pad_all(pop, 14, 0);
    lv_obj_clear_flag(pop, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *tit = lv_label_create(pop);
    lv_label_set_text(tit, "Vai al punto");
    lv_obj_set_style_text_font(tit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(tit, lv_color_hex(0xAABBFF), 0);
    lv_obj_set_width(tit, LV_PCT(100));
    lv_obj_set_style_text_align(tit, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(tit, LV_ALIGN_TOP_MID, 0, 0);

    // Display numero inserito
    lv_obj_t *disp = lv_label_create(pop);
    lv_label_set_text(disp, "---");
    lv_obj_set_style_text_font(disp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(disp, lv_color_hex(0x2ECC71), 0);
    lv_obj_set_style_bg_color(disp, lv_color_hex(0x0D1526), 0);
    lv_obj_set_style_bg_opa(disp, 255, 0);
    lv_obj_set_style_radius(disp, 8, 0);
    lv_obj_set_style_pad_all(disp, 8, 0);
    lv_obj_set_style_text_align(disp, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(disp, LV_PCT(100));
    lv_obj_align(disp, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_user_data(pop, disp);  // lo salviamo per aggiornarlo dai tasti

    // Tastiera numerica 3 colonne
    lv_obj_t *grid = lv_obj_create(pop);
    lv_obj_set_size(grid, LV_PCT(100), 200);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 110);
    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_gap(grid, 6, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    static lv_coord_t sc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t sr[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, sc, sr);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    const char *keys9[] = {"7","8","9","4","5","6","1","2","3"};
    for (int i = 0; i < 9; i++) {
        lv_obj_t *b = spal_edit_btn(grid, keys9[i], lv_color_hex(0x2C5282),
                                    spal_search_key_cb, (void*)(intptr_t)keys9[i][0]);
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i%3, 1, LV_GRID_ALIGN_STRETCH, i/3, 1);
    }
    lv_obj_t *b0 = spal_edit_btn(grid, "0", lv_color_hex(0x2C5282),
                                  spal_search_key_cb, (void*)(intptr_t)'0');
    lv_obj_set_grid_cell(b0, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
    lv_obj_t *bbs = spal_edit_btn(grid, LV_SYMBOL_BACKSPACE, lv_color_hex(0xF39C12),
                                   spal_search_bs_cb, NULL);
    lv_obj_set_grid_cell(bbs, LV_GRID_ALIGN_STRETCH, 1, 2, LV_GRID_ALIGN_STRETCH, 3, 1);

    lv_obj_t *bann = spal_edit_btn(pop, "Annulla", lv_color_hex(0xAA2222),
                                    spal_search_annulla_cb, NULL);
    lv_obj_set_size(bann, 120, 44);
    lv_obj_align(bann, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *bok = spal_edit_btn(pop, "Vai", lv_color_hex(0x1A7A3A),
                                   spal_search_confirm_cb, NULL);
    lv_obj_set_size(bok, 120, 44);
    lv_obj_align(bok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

static void spal_search_field_cb(lv_event_t *e)
{
    (void)e;
    spal_search_popup_open();
}

// ─── Storia modifiche: undo / redo ────────────────────────────────────────────
static void spal_hist_back_cb(lv_event_t *e)
{
    (void)e;
    if (s_hist_pos == 0) return;  // niente da annullare
    s_hist_pos--;
    SpalHistEntry &h = s_hist[s_hist_pos];
    s_hist_inhibit = true;
    spalmatrice_manager_edit_path_point(h.idx, h.old_x, h.old_y);
    s_hist_inhibit = false;
    // Aggiorna selezione e vista
    s_sel_idx = h.idx; s_sel_x = h.old_x; s_sel_y = h.old_y; s_sel_valid = true;
    s_zoom = 32.0f; s_pan_cx = h.old_x; s_pan_cy = h.old_y;
    spal_update_zoom_lbl();
    spalmatrice_manager_save_to_sd();
    spal_draw_path_on_canvas();
    // Aggiorna search label col numero del punto
    snprintf(s_search_buf, sizeof(s_search_buf), "%d", h.idx);
    spal_search_update_lbl();
    if (s_spal_status_lbl)
        lv_label_set_text_fmt(s_spal_status_lbl, "Undo pt%d → (%.2f,%.2f)", h.idx, h.old_x, h.old_y);
}

static void spal_hist_fwd_cb(lv_event_t *e)
{
    (void)e;
    if (s_hist_pos >= s_hist_count) return;  // niente da rifare
    SpalHistEntry &h = s_hist[s_hist_pos];
    s_hist_pos++;
    s_hist_inhibit = true;
    spalmatrice_manager_edit_path_point(h.idx, h.new_x, h.new_y);
    s_hist_inhibit = false;
    // Aggiorna selezione e vista
    s_sel_idx = h.idx; s_sel_x = h.new_x; s_sel_y = h.new_y; s_sel_valid = true;
    s_zoom = 32.0f; s_pan_cx = h.new_x; s_pan_cy = h.new_y;
    spal_update_zoom_lbl();
    spalmatrice_manager_save_to_sd();
    spal_draw_path_on_canvas();
    snprintf(s_search_buf, sizeof(s_search_buf), "%d", h.idx);
    spal_search_update_lbl();
    if (s_spal_status_lbl)
        lv_label_set_text_fmt(s_spal_status_lbl, "Redo pt%d → (%.2f,%.2f)", h.idx, h.new_x, h.new_y);
}

// ─── Zoom buttons ─────────────────────────────────────────────────────────────
static void spal_update_zoom_lbl(void)
{
    if (!s_spal_zoom_lbl) return;
    lv_label_set_text_fmt(s_spal_zoom_lbl, "%dx", (int)(s_zoom + 0.5f));
}
static void spal_zoom_in_cb(lv_event_t *e)
{
    (void)e;
    if (s_zoom < 32.0f) { s_zoom *= 2.0f; spal_update_zoom_lbl(); spal_draw_path_on_canvas(); }
}
static void spal_zoom_out_cb(lv_event_t *e)
{
    (void)e;
    if (s_zoom > 1.0f) { s_zoom *= 0.5f; if (s_zoom < 1.0f) s_zoom = 1.0f; spal_update_zoom_lbl(); spal_draw_path_on_canvas(); }
}
static void spal_zoom_fit_cb(lv_event_t *e)
{
    (void)e;
    s_zoom = 1.0f;
    if (s_view_init) {
        s_pan_cx = (s_fit_x_min + s_fit_x_max) * 0.5f;
        s_pan_cy = (s_fit_y_min + s_fit_y_max) * 0.5f;
    }
    spal_update_zoom_lbl();
    spal_draw_path_on_canvas();
}

// Timer LVGL: polling finché DXF è pronto, poi disegna
static void spal_poll_timer_cb(lv_timer_t *t)
{
    spalmatrice_state_t st = spalmatrice_manager_get_state();
    if (st == SPAL_STATE_DXF_LOADED ||
        st == SPAL_STATE_READY      ||
        st == SPAL_STATE_RUNNING    ||
        st == SPAL_STATE_DONE) {
        int npts = spalmatrice_manager_get_point_count();
        char buf[48];
        snprintf(buf, sizeof(buf), "Percorso: %d punti", npts);
        lv_label_set_text(s_spal_status_lbl, buf);
        spal_draw_path_on_canvas();
        lv_timer_del(t);
    } else if (st == SPAL_STATE_ERROR) {
        lv_label_set_text(s_spal_status_lbl, "Errore caricamento DXF");
        lv_timer_del(t);
    }
}

// Chiamata da AppBanchetto::run() quando il dispositivo è spalmatrice (ID 177)
extern "C" void app_banchetto_spalmatrice_start(void)
{
    ESP_LOGI("MAIN", "Spalmatrice: avvio UI + fetch DXF");

    spalmatrice_manager_init();

    // ── Crea schermo full-screen ──────────────────────────────────────────────
    s_spal_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_spal_screen, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(s_spal_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_spal_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Barra header — su lv_layer_sys() così resta fissa sopra tutto
    lv_obj_t *header = lv_obj_create(lv_layer_sys());
    lv_obj_set_pos(header, 0, 50);
    lv_obj_set_size(header, 1024, 40);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1A3A5C), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "SPALMATRICE");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, 0);

    // Bottoni zoom: [-] [livello] [FIT] [+]  — da destra verso sinistra
    // Bottoni: [+] [FIT] [-]  con 10px di gap tra loro, larghi 60px
    // Da destra: + a -10, FIT a -80, - a -150, label a -220
    struct ZoomBtnDef { const char *lbl; lv_event_cb_t cb; lv_coord_t roff; };
    ZoomBtnDef zbdefs[] = {
        {"+",   spal_zoom_in_cb,  -10},
        {"FIT", spal_zoom_fit_cb, -80},
        {"-",   spal_zoom_out_cb, -150},
    };
    for (auto &zb : zbdefs) {
        lv_obj_t *b = lv_btn_create(header);
        lv_obj_set_size(b, 60, 30);
        lv_obj_align(b, LV_ALIGN_RIGHT_MID, zb.roff, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x2C4A6C), 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, zb.lbl);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, zb.cb, LV_EVENT_CLICKED, NULL);
    }
    // Label livello zoom
    s_spal_zoom_lbl = lv_label_create(header);
    lv_label_set_text(s_spal_zoom_lbl, "1x");
    lv_obj_set_style_text_font(s_spal_zoom_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_spal_zoom_lbl, lv_color_hex(0xAADDFF), 0);
    lv_obj_align(s_spal_zoom_lbl, LV_ALIGN_RIGHT_MID, -222, 0);

    s_spal_status_lbl = lv_label_create(header);
    lv_label_set_text(s_spal_status_lbl, "Caricamento DXF...");
    lv_obj_set_style_text_color(s_spal_status_lbl, lv_color_hex(0xAACCFF), 0);
    lv_obj_set_style_text_font(s_spal_status_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(s_spal_status_lbl, LV_ALIGN_LEFT_MID, 140, 0);

    // Bottone < (undo) — a sinistra del campo search
    {
        lv_obj_t *b = lv_btn_create(header);
        lv_obj_set_size(b, 36, 28);
        lv_obj_align(b, LV_ALIGN_CENTER, -70, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x3A2060), 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, "<");
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xDDBBFF), 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, spal_hist_back_cb, LV_EVENT_CLICKED, NULL);
    }

    // Campo search — tocca per aprire tastiera numerica e andare al punto
    lv_obj_t *search_box = lv_obj_create(header);
    lv_obj_set_size(search_box, 90, 28);
    lv_obj_align(search_box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(search_box, lv_color_hex(0x0D2340), 0);
    lv_obj_set_style_bg_opa(search_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(search_box, lv_color_hex(0x4488BB), 0);
    lv_obj_set_style_border_width(search_box, 1, 0);
    lv_obj_set_style_radius(search_box, 4, 0);
    lv_obj_set_style_pad_all(search_box, 2, 0);
    lv_obj_clear_flag(search_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(search_box, LV_OBJ_FLAG_CLICKABLE);
    s_search_lbl = lv_label_create(search_box);
    lv_label_set_text(s_search_lbl, "---");
    lv_obj_set_style_text_font(s_search_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_search_lbl, lv_color_hex(0x88DDFF), 0);
    lv_obj_center(s_search_lbl);
    lv_obj_add_event_cb(search_box, spal_search_field_cb, LV_EVENT_CLICKED, NULL);

    // Bottone > (redo) — a destra del campo search
    {
        lv_obj_t *b = lv_btn_create(header);
        lv_obj_set_size(b, 36, 28);
        lv_obj_align(b, LV_ALIGN_CENTER, 70, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x3A2060), 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, ">");
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xDDBBFF), 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, spal_hist_fwd_cb, LV_EVENT_CLICKED, NULL);
    }

    // Canvas DXF: buffer in PSRAM (1024×560×2 byte ≈ 1.1 MB)
    s_spal_canvas_buf = (lv_color_t *)heap_caps_malloc(
        SPAL_CANVAS_W * SPAL_CANVAS_H * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!s_spal_canvas_buf) {
        ESP_LOGE("MAIN", "Alloc canvas buffer fallito");
        return;
    }
    s_spal_canvas = lv_canvas_create(s_spal_screen);
    lv_canvas_set_buffer(s_spal_canvas, s_spal_canvas_buf,
                         SPAL_CANVAS_W, SPAL_CANVAS_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(s_spal_canvas, 0, 90);  // 50 (brookesia) + 40 (header)
    lv_canvas_fill_bg(s_spal_canvas, lv_color_hex(0x0A0A0A), LV_OPA_COVER);
    lv_obj_add_flag(s_spal_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_spal_canvas, spal_canvas_touch_cb, LV_EVENT_CLICKED,   NULL);
    lv_obj_add_event_cb(s_spal_canvas, spal_canvas_touch_cb, LV_EVENT_PRESSING,  NULL);
    lv_obj_add_event_cb(s_spal_canvas, spal_canvas_touch_cb, LV_EVENT_RELEASED,  NULL);

    lv_disp_load_scr(s_spal_screen);

    // Timer polling: controlla ogni 500ms se il DXF è pronto
    lv_timer_create(spal_poll_timer_cb, 500, NULL);

    // Prima prova a leggere il DXF dalla SD; se non presente scarica da HTTP
    if (spalmatrice_manager_load_dxf_from_sd("percorsi/sc605Completa.dxf") == ESP_OK) {
        ESP_LOGI("MAIN", "DXF caricato da SD");
        if (s_spal_status_lbl)
            lv_label_set_text(s_spal_status_lbl, "DXF da SD card");
    } else {
        ESP_LOGI("MAIN", "SD non disponibile, scarico da HTTP");
        spalmatrice_manager_fetch_and_parse(SERVER_BASE "/iot/percorsi/sc605Completa.dxf");
    }
}

static const char *TAG = "MAIN";
static lv_obj_t *s_ip_label = NULL;

#define BACKLIGHT_TIMEOUT_MS 600000   // 30s → backlight off
#define DEEP_SLEEP_TIMEOUT_MS 3600000 // 1 min per test → deep sleep
#define WAKEUP_GPIO GPIO_NUM_5        // Bottone fisico

extern EventGroupHandle_t s_wifi_event_group;
extern int wifi_get_rssi(void);

/* Timestamp ultimo versa — aggiornato da banchetto_manager */
static volatile uint32_t last_versa_tick = 0;

/* Chiamata da banchetto_manager.c dopo versa OK */
extern "C" void deep_sleep_reset_timer(void)
{
    last_versa_tick = xTaskGetTickCount();
    ESP_LOGW(TAG, "AZZERATO timer deep sleep");
    if (bsp_display_lock(0))
    {
        lv_disp_trig_activity(NULL);
        bsp_display_unlock();
    }
}

void backlight_auto_task(void *arg)
{
    lv_display_t *disp = (lv_display_t *)arg;
    if (disp == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    bool backlight_on = true;
    last_versa_tick = xTaskGetTickCount();

    /* Aspetta che LVGL sia stabile prima di cominciare a controllare l'inattività */
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1)
    {
        uint32_t inactive_ms = 0;

        if (bsp_display_lock(100))
        {
            inactive_ms = lv_disp_get_inactive_time(disp);
            bsp_display_unlock();
        }

        /* Ignora valori anomali (LVGL non ancora pronto o display non inizializzato) */
        if (inactive_ms > 3600000UL)
        {
            inactive_ms = 0;
        }

        /* Controlla inattività produttiva (tempo dall'ultimo versa) */
        uint32_t now = xTaskGetTickCount();
        uint32_t ms_since_versa = (now - last_versa_tick) * portTICK_PERIOD_MS;

        if (ms_since_versa > DEEP_SLEEP_TIMEOUT_MS)
        {
            ESP_LOGW(TAG, "Nessun versa da %lu ms, entro in DEEP SLEEP!",
                     (unsigned long)ms_since_versa);
            bsp_display_backlight_off();
            vTaskDelay(pdMS_TO_TICKS(500));

            esp_err_t ret = esp_sleep_enable_ext1_wakeup_io(
                1ULL << WAKEUP_GPIO, ESP_EXT1_WAKEUP_ANY_LOW);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Errore config wakeup: %s", esp_err_to_name(ret));
                continue;
            }

            rtc_gpio_pullup_en(WAKEUP_GPIO);
            rtc_gpio_pulldown_dis(WAKEUP_GPIO);

            ESP_LOGW(TAG, "DEEP SLEEP NOW - premi GPIO%d per risvegliare", WAKEUP_GPIO);
            esp_deep_sleep_start();
        }

        /* Backlight gestito dal touch display */
        if (inactive_ms > BACKLIGHT_TIMEOUT_MS && backlight_on)
        {
            bsp_display_backlight_off();
            backlight_on = false;
            ESP_LOGI(TAG, "Backlight OFF (inattivo %lu ms)", (unsigned long)inactive_ms);
        }
        else if (inactive_ms < BACKLIGHT_TIMEOUT_MS && !backlight_on)
        {
            bsp_display_backlight_on();
            backlight_on = true;
            last_versa_tick = xTaskGetTickCount(); // ← aggiungi questa riga
            ESP_LOGI(TAG, "Backlight ON (touch rilevato)");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
// --- TASK BATTERIA ---
void battery_status_update_task(void *arg)
{
    ESP_Brookesia_StatusBar *status_bar = (ESP_Brookesia_StatusBar *)arg;
    if (status_bar == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    int percentuale = 0, voltage_mv = 0;
    bool in_carica = false;

    while (1)
    {
        if (ota_in_progress)
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        if (battery_get_percentage(&percentuale) == ESP_OK)
        {
            battery_get_voltage(&voltage_mv);
            battery_is_charging(&in_carica);
            bsp_display_lock(0);
            status_bar->setBatteryPercent(in_carica, percentuale);
            bsp_display_unlock();
            ESP_LOGI(TAG, "Batteria: %d%% (%dmV) %s",
                     percentuale, voltage_mv, in_carica ? "[CARICA]" : "");
        }
        else
        {
            ESP_LOGE(TAG, "Errore lettura batteria");
        }
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

// --- TASK WIFI ---
void wifi_status_update_task(void *arg)
{
    ESP_Brookesia_StatusBar *status_bar = (ESP_Brookesia_StatusBar *)arg;
    if (status_bar == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        if (ota_in_progress)
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        if (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT)
        {
            int rssi = wifi_get_rssi();
            int wifi_state = (rssi > -60) ? 3 : (rssi > -80) ? 2
                                            : (rssi > -100)  ? 1
                                                             : 0;
            char ip_str[20] = "---";
            esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (sta)
            {
                esp_netif_ip_info_t ip_info;
                if (esp_netif_get_ip_info(sta, &ip_info) == ESP_OK && ip_info.ip.addr != 0)
                {
                    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
                }
            }
            bsp_display_lock(0);
            status_bar->setWifiIconState(wifi_state);
            if (s_ip_label)
                lv_label_set_text(s_ip_label, ip_str);
            bsp_display_unlock();
        }
        else
        {
            bsp_display_lock(0);
            status_bar->setWifiIconState(0);
            if (s_ip_label)
                lv_label_set_text(s_ip_label, "---");
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// --- TASK ORARIO ---
void orario_server_update_task(void *arg)
{
    ESP_Brookesia_StatusBar *status_bar = (ESP_Brookesia_StatusBar *)arg;
    if (status_bar == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    int ore, minuti;
    while (1)
    {
        if (ota_in_progress)
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        if (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT)
        {
            if (http_get_server_time(&ore, &minuti) == ESP_OK)
            {
                bool is_pm = (ore >= 12);
                bsp_display_lock(0);
                status_bar->setClock(ore, minuti, is_pm);
                bsp_display_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
// --- TASK BATTERIA ---

void inizializza_testi_gui()
{
    banchetto_data_t dati;
    banchetto_manager_get_data(&dati);
}

extern "C" void app_main(void)
{
#ifdef LOG_QUIET
    esp_log_level_set("*", ESP_LOG_WARN);
#endif
    {
        const char *reason_str = "unknown";
        switch (esp_reset_reason())
        {
        case ESP_RST_POWERON:
            reason_str = "POWER ON";
            break;
        case ESP_RST_BROWNOUT:
            reason_str = "BROWNOUT";
            break;
        case ESP_RST_INT_WDT:
            reason_str = "INTERRUPT WDT";
            break;
        case ESP_RST_TASK_WDT:
            reason_str = "TASK WDT";
            break;
        case ESP_RST_PANIC:
            reason_str = "PANIC";
            break;
        case ESP_RST_SW:
            reason_str = "SOFTWARE";
            break;
        case ESP_RST_DEEPSLEEP:
            reason_str = "DEEP SLEEP";
            break;
        default:
            break;
        }
        ESP_LOGW(TAG, "*** RESET REASON: %s ***", reason_str);
    }
    log_manager_init();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(bsp_spiffs_mount());
    ESP_ERROR_CHECK(bsp_extra_codec_init());

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {.buff_dma = false, .buff_spiram = true, .sw_rotate = true}};
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (disp)
    {
        ESP_LOGI(TAG, "Display inizializzato OK");
        bsp_display_rotate(disp, LV_DISPLAY_ROTATION_0);
    }
    else
    {
        ESP_LOGE(TAG, "ERRORE: Display non inizializzato (disp == NULL)!");
    }

    bsp_display_lock(0);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_sta();

    if (wifi_is_connected())
        time_manager_sync();

    gpio_init();
    init_usb_driver();
    init_rfid_uart();
    key_manager_init();
    banchetto_manager_init();
    tagliatubi_manager_init();
    collaudo_manager_init();
    bilancia_manager_init();

    esp_err_t sd_ret = bsp_sdcard_mount();
    if (sd_ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SD Card montata correttamente!");
        log_manager_sd_ready();
        if (!time_manager_is_synced())
            time_manager_restore_from_sd();
        time_manager_start_periodic_save();
        offline_journal_print_all();
    }
    else
    {
        ESP_LOGE(TAG, "Fallimento montaggio SD: %s", esp_err_to_name(sd_ret));
    }

    if (offline_journal_count() > 0)
    {
        ESP_LOGI(TAG, "Replay journal offline prima di inizializzare...");
        offline_journal_replay();
    }

    if (banchetto_manager_fetch_from_server() != ESP_OK)
    {
        ESP_LOGW(TAG, "Fetch server fallito, carico da SD cache...");
        banchetto_manager_load_from_sd();
        banchetto_manager_reconstruct_from_journal();
    }

    banchetto_manager_start_periodic_refresh();

    settings_init();
    web_server_init();
    ble_manager_init();

    if (battery_manager_init() != ESP_OK)
        ESP_LOGE(TAG, "Errore init Battery Manager - continuando senza monitoraggio");

    ESP_Brookesia_Phone *banchetto = new ESP_Brookesia_Phone(disp);
    if (banchetto == nullptr)
    {
        ESP_LOGE(TAG, "Errore creazione Phone");
        return;
    }

    ESP_Brookesia_PhoneStylesheet_t *banchetto_stylesheet =
        new ESP_Brookesia_PhoneStylesheet_t ESP_BROOKESIA_PHONE_1024_600_DARK_STYLESHEET();

    banchetto->addStylesheet(*banchetto_stylesheet);
    banchetto->activateStylesheet(*banchetto_stylesheet);

    lv_indev_t *touch = bsp_display_get_input_dev();
    if (touch != nullptr)
    {
        banchetto->setTouchDevice(touch);
    }

    if (!banchetto->begin())
    {
        ESP_LOGE(TAG, "Errore inizializzazione Phone");
        return;
    }

    ESP_Brookesia_StatusBar *status_bar = banchetto->getHome().getStatusBar();
    if (status_bar == nullptr)
    {
        ESP_LOGE(TAG, "Status bar non disponibile");
    }
    else
    {
        bsp_display_lock(0);
        s_ip_label = lv_label_create(lv_layer_top());
        lv_label_set_text(s_ip_label, "---");
        lv_obj_set_style_text_color(s_ip_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(s_ip_label, &lv_font_montserrat_22, 0);
        lv_obj_set_style_bg_opa(s_ip_label, LV_OPA_TRANSP, 0);
        lv_obj_align(s_ip_label, LV_ALIGN_TOP_RIGHT, -260, 14);
        bsp_display_unlock();

        xTaskCreate(wifi_status_update_task, "WiFi Status", 4096, status_bar, 1, NULL);
        xTaskCreate(orario_server_update_task, "Orario Server", 4096, status_bar, 1, NULL);
        xTaskCreate(battery_status_update_task, "Battery Status", 4096, status_bar, 1, NULL);
        status_bar->setBleIconState(1);
    }

    /* Backlight task: fuori dall'if della status_bar, serve sempre */
    xTaskCreate(backlight_auto_task, "Backlight Auto", 4096, disp, 1, NULL);

    // esp_err_t sd_ret = bsp_sdcard_mount();
    // if (sd_ret == ESP_OK)
    // {
    //     ESP_LOGI(TAG, "SD Card montata correttamente!");
    //     log_manager_sd_ready();
    // }
    // else
    // {
    //     ESP_LOGE(TAG, "Fallimento montaggio SD: %s", esp_err_to_name(sd_ret));
    // }

    // FILE *f = fopen("/sdcard/test_sd.txt", "r");
    // if (f == NULL)
    // {
    //     ESP_LOGE(TAG, "Impossibile aprire il file su SD!");
    // }
    // else
    // {
    //     char line[64];
    //     fgets(line, sizeof(line), f);
    //     fclose(f);
    //     ESP_LOGI(TAG, "Contenuto file SD: %s", line);
    // }
    if (status_bar != nullptr)
    {
        status_bar->setSdIconState(sd_ret == ESP_OK ? 1 : 0);
    }
    banchetto->installApp(new AppBanchetto());
    inizializza_testi_gui();
    banchetto->installApp(new Calculator());
    banchetto->installApp(new MiaApp());
    banchetto->installApp(new Logged());
    banchetto->installApp(new DocBrowser());
    // banchetto->installApp(new AppTagliatubi());

    if (bsp_extra_player_init() != ESP_OK)
        ESP_LOGE(TAG, "bsp_extra_player_init failed - audio non disponibile");
    else
        ESP_LOGI(TAG, "Audio player inizializzato");

    bsp_display_unlock();
    bsp_display_backlight_on();
    ESP_LOGW("RAM_MONITOR", "RAM Interna libera: %zu byte | PSRAM libera: %zu byte",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    const esp_app_desc_t *app_desc = esp_app_get_description();

    ESP_LOGI("APP_INFO", "=============================================");
    ESP_LOGI("APP_INFO", "Project Name:      %s", app_desc->project_name);
    ESP_LOGI("APP_INFO", "App Version:       %s", app_desc->version);
    ESP_LOGI("APP_INFO", "Compile Time:      %s", app_desc->time);
    ESP_LOGI("APP_INFO", "Compile Date:      %s", app_desc->date);
    ESP_LOGI("APP_INFO", "IDF Version:       %s", app_desc->idf_ver);
    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK)
        ESP_LOGI("APP_INFO", "MAC WiFi STA:      %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI("APP_INFO", "=============================================");

    ESP_LOGI(TAG, "Sistema avviato completamente");
}