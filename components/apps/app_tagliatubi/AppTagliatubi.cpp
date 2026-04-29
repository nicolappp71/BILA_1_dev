#include "AppTagliatubi.hpp"
#include "esp_log.h"
#include "fonts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "AppTagliatubi";

// File-scope instance pointer and payload (avoids private-access issues)
static AppTagliatubi *g_instance = nullptr;

struct UpdatePayload {
    tagliatubi_state_t state;
    tagliatubi_data_t  data;
};

// ─── Inline numpad popup ──────────────────────────────────────────────────────

struct NumpadTarget { lv_obj_t *label; const char *title; int max_digits; };
static NumpadTarget  s_np_targets[4];   // [0]=lunghezza [1]=quantita [2]=velocita [3]=codice

static lv_obj_t *s_np_overlay    = nullptr;
static lv_obj_t *s_np_popup      = nullptr;
static lv_obj_t *s_np_display    = nullptr;
static lv_obj_t *s_np_target_lbl = nullptr;
static int       s_np_maxdig     = 5;
static char      s_np_buf[8]     = {0};

static void np_update_display(void) {
    if (s_np_display)
        lv_label_set_text(s_np_display, s_np_buf[0] ? s_np_buf : "0");
}
static void np_close(void) {
    if (s_np_overlay) { lv_obj_del(s_np_overlay); s_np_overlay = nullptr; }
    if (s_np_popup)   { lv_obj_del(s_np_popup);   s_np_popup   = nullptr; }
    s_np_display = s_np_target_lbl = nullptr;
    memset(s_np_buf, 0, sizeof(s_np_buf));
}
static void cb_np_digit(lv_event_t *e) {
    char d = (char)(intptr_t)lv_event_get_user_data(e);
    size_t len = strlen(s_np_buf);
    if ((int)len >= s_np_maxdig) return;
    if (len == 0 && d == '0') return;
    s_np_buf[len] = d; s_np_buf[len+1] = '\0';
    np_update_display();
}
static void cb_np_bs(lv_event_t *e) {
    (void)e;
    size_t len = strlen(s_np_buf);
    if (len > 0) s_np_buf[len-1] = '\0';
    np_update_display();
}
static void cb_np_ok(lv_event_t *e) {
    (void)e;
    bool is_codice = (g_instance && s_np_target_lbl == s_np_targets[3].label);
    if (s_np_target_lbl && s_np_buf[0])
        lv_label_set_text(s_np_target_lbl, s_np_buf);
    np_close();
    if (is_codice && g_instance)
        g_instance->cerca_prodotto();
}
static void cb_np_cancel(lv_event_t *e) { (void)e; np_close(); }

static lv_obj_t *np_btn(lv_obj_t *parent, const char *txt, lv_color_t bg,
                         lv_event_cb_t cb, void *ud) {
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_bg_opa(b, 255, 0);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

static void cb_open_numpad(lv_event_t *e) {
    NumpadTarget *t = static_cast<NumpadTarget *>(lv_event_get_user_data(e));
    if (s_np_popup) return;
    s_np_target_lbl = t->label;
    s_np_maxdig     = t->max_digits;
    memset(s_np_buf, 0, sizeof(s_np_buf));
    const char *cur = lv_label_get_text(t->label);
    if (cur && cur[0] && strcmp(cur, "0") != 0 && strcmp(cur, "---") != 0)
        snprintf(s_np_buf, sizeof(s_np_buf), "%s", cur);

    s_np_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_np_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_np_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_np_overlay, 160, 0);
    lv_obj_set_style_border_width(s_np_overlay, 0, 0);
    lv_obj_set_style_radius(s_np_overlay, 0, 0);
    lv_obj_clear_flag(s_np_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_np_overlay, LV_OBJ_FLAG_CLICKABLE);

    s_np_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_np_popup, 400, 500);
    lv_obj_center(s_np_popup);
    lv_obj_move_foreground(s_np_popup);
    lv_obj_set_style_bg_color(s_np_popup, lv_color_hex(0x141E30), 0);
    lv_obj_set_style_bg_opa(s_np_popup, 255, 0);
    lv_obj_set_style_border_color(s_np_popup, lv_color_hex(0x4A90E2), 0);
    lv_obj_set_style_border_width(s_np_popup, 2, 0);
    lv_obj_set_style_radius(s_np_popup, 16, 0);
    lv_obj_set_style_pad_all(s_np_popup, 16, 0);
    lv_obj_clear_flag(s_np_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(s_np_popup, 40, 0);
    lv_obj_set_style_shadow_color(s_np_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(s_np_popup, 200, 0);

    lv_obj_t *titolo = lv_label_create(s_np_popup);
    lv_label_set_text(titolo, t->title);
    lv_obj_set_style_text_font(titolo, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(titolo, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(titolo, LV_PCT(100));
    lv_obj_set_style_text_align(titolo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(titolo, LV_ALIGN_TOP_MID, 0, 0);

    s_np_display = lv_label_create(s_np_popup);
    lv_label_set_text(s_np_display, s_np_buf[0] ? s_np_buf : "0");
    lv_obj_set_style_text_font(s_np_display, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_np_display, lv_color_hex(0x2ECC71), 0);
    lv_obj_set_style_bg_color(s_np_display, lv_color_hex(0x0D1526), 0);
    lv_obj_set_style_bg_opa(s_np_display, 255, 0);
    lv_obj_set_style_radius(s_np_display, 8, 0);
    lv_obj_set_style_pad_all(s_np_display, 10, 0);
    lv_obj_set_style_text_align(s_np_display, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(s_np_display, LV_PCT(100));
    lv_obj_align(s_np_display, LV_ALIGN_TOP_MID, 0, 36);

    lv_obj_t *grid = lv_obj_create(s_np_popup);
    lv_obj_set_size(grid, LV_PCT(100), 240);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_set_style_bg_opa(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_gap(grid, 8, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);

    const char *digits[] = {"1","2","3","4","5","6","7","8","9"};
    for (int i = 0; i < 9; i++) {
        lv_obj_t *b = np_btn(grid, digits[i], lv_color_hex(0x2C5282),
                             cb_np_digit, (void*)(intptr_t)(digits[i][0]));
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i%3, 1, LV_GRID_ALIGN_STRETCH, i/3, 1);
    }
    lv_obj_t *b_bs = np_btn(grid, LV_SYMBOL_BACKSPACE, lv_color_hex(0xF39C12), cb_np_bs, nullptr);
    lv_obj_set_grid_cell(b_bs, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
    lv_obj_t *b0 = np_btn(grid, "0", lv_color_hex(0x2C5282), cb_np_digit, (void*)(intptr_t)'0');
    lv_obj_set_grid_cell(b0, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
    lv_obj_t *dummy = lv_obj_create(grid);
    lv_obj_set_style_bg_opa(dummy, 0, 0);
    lv_obj_set_style_border_width(dummy, 0, 0);
    lv_obj_set_grid_cell(dummy, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 3, 1);

    lv_obj_t *btn_c = np_btn(s_np_popup, "Annulla", lv_color_hex(0xFA0000), cb_np_cancel, nullptr);
    lv_obj_set_size(btn_c, 160, 55);
    lv_obj_align(btn_c, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *btn_ok = np_btn(s_np_popup, "OK", lv_color_hex(0x2ECC71), cb_np_ok, nullptr);
    lv_obj_set_size(btn_ok, 160, 55);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

LV_IMG_DECLARE(b2);

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

AppTagliatubi::AppTagliatubi(const char *operatore, const char *macchina)
    : ESP_Brookesia_PhoneApp("Tagliatubi", &b2, true)
{
    snprintf(_nome_operatore, sizeof(_nome_operatore), "%s", operatore);
    snprintf(_nome_macchina,  sizeof(_nome_macchina),  "%s", macchina);
}

AppTagliatubi::~AppTagliatubi()
{
    if (g_instance == this) g_instance = nullptr;
}

bool AppTagliatubi::init(void)
{
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Manager state callback → lv_async_call → LVGL task
// ─────────────────────────────────────────────────────────────────────────────

static void tagl_state_cb(tagliatubi_state_t state, const tagliatubi_data_t *data)
{
    if (!g_instance) return;
    auto *p = new UpdatePayload{state, *data};
    lv_async_call(AppTagliatubi::on_state_update, p);
}

void AppTagliatubi::on_state_update(void *user_data)
{
    auto *p = static_cast<UpdatePayload *>(user_data);
    if (g_instance) {
        g_instance->refresh_ciclo_tab(p->state, &p->data);
    }
    delete p;
}

// ─────────────────────────────────────────────────────────────────────────────
//  run() — builds the full UI
// ─────────────────────────────────────────────────────────────────────────────

bool AppTagliatubi::run(void)
{
    g_instance = this;
    tagliatubi_manager_set_callback(tagl_state_cb);

    lv_area_t area = getVisualArea();
    int w = area.x2 - area.x1;
    int h = area.y2 - area.y1;
    _w  = w;
    _th = h;

    lv_obj_t *root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(root, w, h);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    tabview = lv_tabview_create(root, LV_DIR_TOP, 0);
    lv_obj_set_size(tabview, w, h);
    lv_obj_set_style_bg_color(tabview, lv_color_hex(0x1A1A2E), 0);

    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "Impostazioni");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "Ciclo");

    lv_obj_set_style_bg_color(tab1, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_color(tab2, lv_color_hex(0xF0F0F0), 0);

    build_tab_impostazioni(tab1);
    build_tab_ciclo(tab2);

    refresh_ciclo_tab(tagliatubi_manager_get_state(), tagliatubi_manager_get_data());

    ESP_LOGI(TAG, "UI built — operatore:%s macchina:%s", _nome_operatore, _nome_macchina);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tab 1 — Impostazioni  (layout identico a AppBanchetto crea_page1)
// ─────────────────────────────────────────────────────────────────────────────

void AppTagliatubi::build_tab_impostazioni(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xF0F0F0), 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    const int H = _th;

    // ── SIDEBAR NERA ─────────────────────────────────────────────────────────
    lv_obj_t *sidebar = lv_obj_create(parent);
    lv_obj_set_pos(sidebar, 0, 0);
    lv_obj_set_size(sidebar, 260, H);
    lv_obj_set_style_bg_color(sidebar, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_bg_opa(sidebar, 255, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);
    lv_obj_set_style_radius(sidebar, 0, 0);
    lv_obj_set_style_pad_all(sidebar, 24, 0);
    lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_op_tit = lv_label_create(sidebar);
    lv_label_set_text(lbl_op_tit, "OPERATORE");
    lv_obj_set_style_text_font(lbl_op_tit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_op_tit, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_letter_space(lbl_op_tit, 4, 0);
    lv_obj_align(lbl_op_tit, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *lbl_op_val = lv_label_create(sidebar);
    lv_label_set_text(lbl_op_val, _nome_operatore);
    lv_obj_set_style_text_font(lbl_op_val, &ui_font_my_font75, 0);
    lv_obj_set_style_text_color(lbl_op_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_op_val, LV_ALIGN_TOP_LEFT, 0, 28);

    // Pill stato prodotto
    lv_obj_t *pill = lv_obj_create(sidebar);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pill, lv_color_hex(0xE11D48), 0);
    lv_obj_set_style_bg_opa(pill, 255, 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_radius(pill, 4, 0);
    lv_obj_set_style_pad_top(pill, 4, 0);
    lv_obj_set_style_pad_bottom(pill, 4, 0);
    lv_obj_set_style_pad_left(pill, 10, 0);
    lv_obj_set_style_pad_right(pill, 10, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(pill, LV_ALIGN_TOP_LEFT, 0, 130);

    lbl_pill_stato = lv_label_create(pill);
    lv_label_set_text(lbl_pill_stato, "NON CARICATO");
    lv_obj_set_style_text_font(lbl_pill_stato, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_pill_stato, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_letter_space(lbl_pill_stato, 2, 0);
    lv_obj_align(lbl_pill_stato, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *sep = lv_obj_create(sidebar);
    lv_obj_set_pos(sep, 0, 200);
    lv_obj_set_size(sep, 212, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(sep, 255, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    lv_obj_t *lbl_banc_tit = lv_label_create(sidebar);
    lv_label_set_text(lbl_banc_tit, "BANCHETTO");
    lv_obj_set_style_text_font(lbl_banc_tit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_banc_tit, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_letter_space(lbl_banc_tit, 3, 0);
    lv_obj_align(lbl_banc_tit, LV_ALIGN_TOP_LEFT, 0, 218);

    lv_obj_t *lbl_banc_val = lv_label_create(sidebar);
    lv_label_set_text(lbl_banc_val, _nome_macchina);
    lv_obj_set_style_text_font(lbl_banc_val, &ui_font_my_font75, 0);
    lv_obj_set_style_text_color(lbl_banc_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_banc_val, LV_ALIGN_TOP_LEFT, 0, 242);

    // ── CODICE ARTICOLO — hero giallo (tappable → apre numpad) ───────────────
    s_np_targets[3] = { nullptr, "CODICE ARTICOLO", 8 };

    lv_obj_t *box_codice = lv_btn_create(parent);
    lv_obj_set_pos(box_codice, 276, 16);
    lv_obj_set_size(box_codice, 732, 120);
    lv_obj_set_style_bg_color(box_codice, lv_color_hex(0xFFDD00), 0);
    lv_obj_set_style_bg_color(box_codice, lv_color_hex(0xEECC00), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(box_codice, 255, 0);
    lv_obj_set_style_border_width(box_codice, 0, 0);
    lv_obj_set_style_radius(box_codice, 12, 0);
    lv_obj_set_style_shadow_width(box_codice, 0, 0);
    lv_obj_set_style_pad_left(box_codice, 20, 0);
    lv_obj_set_style_pad_top(box_codice, 12, 0);
    lv_obj_clear_flag(box_codice, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *t = lv_label_create(box_codice);
        lv_label_set_text(t, "CODICE ARTICOLO");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000066), 0);
        lv_obj_set_style_text_letter_space(t, 4, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        ta_codice = lv_label_create(box_codice);
        lv_label_set_text(ta_codice, "---");
        lv_obj_set_style_text_font(ta_codice, &ui_font_my_font75, 0);
        lv_obj_set_style_text_color(ta_codice, lv_color_hex(0x000000), 0);
        lv_obj_align(ta_codice, LV_ALIGN_TOP_LEFT, 0, 26);
    }
    s_np_targets[3].label = ta_codice;
    lv_obj_add_event_cb(box_codice, cb_open_numpad, LV_EVENT_CLICKED, &s_np_targets[3]);

    // ── LUNGHEZZA — blu (come CICLO in banchetto) ─────────────────────────────
    s_np_targets[0] = { nullptr, "LUNGHEZZA (mm)", 5 };

    lv_obj_t *box_lung = lv_btn_create(parent);
    lv_obj_set_pos(box_lung, 276, 152);
    lv_obj_set_size(box_lung, 200, 110);
    lv_obj_set_style_bg_color(box_lung, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_bg_color(box_lung, lv_color_hex(0x2C70E0), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(box_lung, 255, 0);
    lv_obj_set_style_border_width(box_lung, 0, 0);
    lv_obj_set_style_radius(box_lung, 12, 0);
    lv_obj_set_style_shadow_width(box_lung, 0, 0);
    lv_obj_set_style_pad_all(box_lung, 14, 0);
    lv_obj_clear_flag(box_lung, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *t = lv_label_create(box_lung);
        lv_label_set_text(t, "LUNGHEZZA");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF99), 0);
        lv_obj_set_style_text_letter_space(t, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        ta_lunghezza = lv_label_create(box_lung);
        lv_label_set_text(ta_lunghezza, "0");
        lv_obj_set_style_text_font(ta_lunghezza, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(ta_lunghezza, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(ta_lunghezza, LV_ALIGN_TOP_LEFT, 0, 28);
    }
    s_np_targets[0].label = ta_lunghezza;
    lv_obj_add_event_cb(box_lung, cb_open_numpad, LV_EVENT_CLICKED, &s_np_targets[0]);

    // ── QUANTITA' — bianco (come ORD PRODUZIONE in banchetto) ────────────────
    s_np_targets[1] = { nullptr, "QUANTITA'", 5 };

    lv_obj_t *box_qta = lv_btn_create(parent);
    lv_obj_set_pos(box_qta, 492, 152);
    lv_obj_set_size(box_qta, 360, 110);
    lv_obj_set_style_bg_color(box_qta, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(box_qta, lv_color_hex(0xEEEEEE), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(box_qta, 255, 0);
    lv_obj_set_style_border_width(box_qta, 0, 0);
    lv_obj_set_style_radius(box_qta, 12, 0);
    lv_obj_set_style_shadow_width(box_qta, 0, 0);
    lv_obj_set_style_pad_all(box_qta, 14, 0);
    lv_obj_clear_flag(box_qta, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *t = lv_label_create(box_qta);
        lv_label_set_text(t, "QUANTITA'");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000066), 0);
        lv_obj_set_style_text_letter_space(t, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        ta_quantita = lv_label_create(box_qta);
        lv_label_set_text(ta_quantita, "0");
        lv_obj_set_style_text_font(ta_quantita, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(ta_quantita, lv_color_hex(0x000000), 0);
        lv_obj_align(ta_quantita, LV_ALIGN_TOP_LEFT, 0, 28);
    }
    s_np_targets[1].label = ta_quantita;
    lv_obj_add_event_cb(box_qta, cb_open_numpad, LV_EVENT_CLICKED, &s_np_targets[1]);

    // ── VELOCITA' — rosso (come FASE in banchetto) ────────────────────────────
    s_np_targets[2] = { nullptr, "VELOCITA' (1-99)", 2 };

    lv_obj_t *box_vel = lv_btn_create(parent);
    lv_obj_set_pos(box_vel, 868, 152);
    lv_obj_set_size(box_vel, 140, 110);
    lv_obj_set_style_bg_color(box_vel, lv_color_hex(0xE11D48), 0);
    lv_obj_set_style_bg_color(box_vel, lv_color_hex(0xC01038), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(box_vel, 255, 0);
    lv_obj_set_style_border_width(box_vel, 0, 0);
    lv_obj_set_style_radius(box_vel, 12, 0);
    lv_obj_set_style_shadow_width(box_vel, 0, 0);
    lv_obj_set_style_pad_all(box_vel, 14, 0);
    lv_obj_clear_flag(box_vel, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *t = lv_label_create(box_vel);
        lv_label_set_text(t, "VELOCITA'");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF99), 0);
        lv_obj_set_style_text_letter_space(t, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        ta_velocita = lv_label_create(box_vel);
        lv_label_set_text(ta_velocita, "0");
        lv_obj_set_style_text_font(ta_velocita, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(ta_velocita, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(ta_velocita, LV_ALIGN_TOP_LEFT, 0, 28);
    }
    s_np_targets[2].label = ta_velocita;
    lv_obj_add_event_cb(box_vel, cb_open_numpad, LV_EVENT_CLICKED, &s_np_targets[2]);

    // ── DESCRIZIONE ARTICOLO ──────────────────────────────────────────────────
    lv_obj_t *box_descr = lv_obj_create(parent);
    lv_obj_set_pos(box_descr, 276, 278);
    lv_obj_set_size(box_descr, 732, 110);
    lv_obj_set_style_bg_color(box_descr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(box_descr, 255, 0);
    lv_obj_set_style_border_width(box_descr, 0, 0);
    lv_obj_set_style_radius(box_descr, 12, 0);
    lv_obj_set_style_pad_all(box_descr, 16, 0);
    lv_obj_clear_flag(box_descr, LV_OBJ_FLAG_SCROLLABLE);
    {
        lv_obj_t *t = lv_label_create(box_descr);
        lv_label_set_text(t, "DESCRIZIONE ARTICOLO");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000044), 0);
        lv_obj_set_style_text_letter_space(t, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        lbl_descr = lv_label_create(box_descr);
        lv_label_set_text(lbl_descr, "---");
        lv_obj_set_style_text_font(lbl_descr, &lv_font_montserrat_30, 0);
        lv_obj_set_style_text_color(lbl_descr, lv_color_hex(0x000000), 0);
        lv_obj_set_width(lbl_descr, 700);
        lv_label_set_long_mode(lbl_descr, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(lbl_descr, LV_ALIGN_TOP_LEFT, 0, 28);
    }

    // ── BOTTONE SALVA ─────────────────────────────────────────────────────────
    lv_obj_t *btn_salva = lv_btn_create(parent);
    lv_obj_set_pos(btn_salva, 276, 404);
    lv_obj_set_size(btn_salva, 732, H - 404 - 11);
    lv_obj_set_style_bg_color(btn_salva, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_salva, lv_color_hex(0x222222), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_salva, 12, 0);
    lv_obj_set_style_border_width(btn_salva, 0, 0);
    lv_obj_set_style_shadow_width(btn_salva, 0, 0);
    lv_obj_add_event_cb(btn_salva, cb_salva, LV_EVENT_CLICKED, this);
    {
        lv_obj_t *ico = lv_label_create(btn_salva);
        lv_label_set_text(ico, LV_SYMBOL_SAVE);
        lv_obj_set_style_text_font(ico, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(ico, lv_color_hex(0xFFDD00), 0);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 40, 0);

        lv_obj_t *lbl = lv_label_create(btn_salva);
        lv_label_set_text(lbl, "SALVA PARAMETRI");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_letter_space(lbl, 4, 0);
        lv_obj_center(lbl);

        lv_obj_t *arrow = lv_label_create(btn_salva);
        lv_label_set_text(arrow, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(arrow, lv_color_hex(0xFFDD00), 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -40, 0);
    }

    // ── Popola con dati correnti ──────────────────────────────────────────────
    const tagliatubi_data_t *d = tagliatubi_manager_get_data();
    if (d->id > 0) {
        lv_label_set_text(ta_codice,    d->codice);
        lv_label_set_text(lbl_descr,    d->descrizione);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)d->lunghezza);
        lv_label_set_text(ta_lunghezza, buf);
        snprintf(buf, sizeof(buf), "%d", (int)d->quantita);
        lv_label_set_text(ta_quantita,  buf);
        snprintf(buf, sizeof(buf), "%d", d->velocita);
        lv_label_set_text(ta_velocita,  buf);
        lv_label_set_text(lbl_pill_stato, "CARICATO " LV_SYMBOL_OK);
        lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_pill_stato), lv_color_hex(0x16A34A), 0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tab 2 — Ciclo
// ─────────────────────────────────────────────────────────────────────────────

void AppTagliatubi::build_tab_ciclo(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xF0F0F0), 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    const int SB  = 260;
    const int GAP = 16;
    const int CX  = SB + GAP;
    const int CW  = _w - CX - GAP;
    const int H   = _th;

    // ── SIDEBAR SCURA ────────────────────────────────────────────────────────
    lv_obj_t *sidebar = lv_obj_create(parent);
    lv_obj_set_pos(sidebar, 0, 0);
    lv_obj_set_size(sidebar, SB, H);
    lv_obj_set_style_bg_color(sidebar, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_bg_opa(sidebar, 255, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);
    lv_obj_set_style_radius(sidebar, 0, 0);
    lv_obj_set_style_pad_all(sidebar, 24, 0);
    lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ts = lv_label_create(sidebar);
    lv_label_set_text(ts, "STATO");
    lv_obj_set_style_text_font(ts, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ts, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_letter_space(ts, 4, 0);
    lv_obj_align(ts, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *pill = lv_obj_create(sidebar);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pill, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_opa(pill, 255, 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_radius(pill, 4, 0);
    lv_obj_set_style_pad_top(pill, 6, 0);
    lv_obj_set_style_pad_bottom(pill, 6, 0);
    lv_obj_set_style_pad_left(pill, 12, 0);
    lv_obj_set_style_pad_right(pill, 12, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(pill, LV_ALIGN_TOP_LEFT, 0, 28);

    lbl_stato = lv_label_create(pill);
    lv_label_set_text(lbl_stato, "IDLE");
    lv_obj_set_style_text_font(lbl_stato, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_stato, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_letter_space(lbl_stato, 2, 0);
    lv_obj_align(lbl_stato, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *sep = lv_obj_create(sidebar);
    lv_obj_set_pos(sep, 0, 100);
    lv_obj_set_size(sep, SB - 48, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(sep, 255, 0);
    lv_obj_set_style_border_width(sep, 0, 0);

    lv_obj_t *tp = lv_label_create(sidebar);
    lv_label_set_text(tp, "PEZZI");
    lv_obj_set_style_text_font(tp, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(tp, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_letter_space(tp, 4, 0);
    lv_obj_align(tp, LV_ALIGN_TOP_LEFT, 0, 118);

    lbl_counter = lv_label_create(sidebar);
    lv_label_set_text(lbl_counter, "0 / 0");
    lv_obj_set_style_text_font(lbl_counter, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_counter, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_counter, LV_ALIGN_TOP_LEFT, 0, 144);

    lbl_info = lv_label_create(sidebar);
    lv_label_set_text(lbl_info, "L: — mm\nV: —\nDiam: — mm");
    lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_info, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_info, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // ── CARD GIALLA — contatore grande ───────────────────────────────────────
    lv_obj_t *card_count = lv_obj_create(parent);
    lv_obj_set_pos(card_count, CX, GAP);
    lv_obj_set_size(card_count, CW, 110);
    lv_obj_set_style_bg_color(card_count, lv_color_hex(0xFFDD00), 0);
    lv_obj_set_style_bg_opa(card_count, 255, 0);
    lv_obj_set_style_border_width(card_count, 0, 0);
    lv_obj_set_style_radius(card_count, 12, 0);
    lv_obj_set_style_pad_all(card_count, 14, 0);
    lv_obj_clear_flag(card_count, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    {
        lv_obj_t *tit = lv_label_create(card_count);
        lv_label_set_text(tit, "PRODOTTI");
        lv_obj_set_style_text_font(tit, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(tit, lv_color_hex(0x00000066), 0);
        lv_obj_set_style_text_letter_space(tit, 4, 0);
        lv_obj_align(tit, LV_ALIGN_TOP_LEFT, 0, 0);
    }

    // ── 4 BOTTONI AZIONE ─────────────────────────────────────────────────────
    int btn_y  = GAP + 110 + GAP;
    int btn_h  = 90;
    int btn_w4 = (CW - 3 * GAP) / 4;

    auto make_action_btn = [&](const char *txt, lv_color_t bg,
                                lv_event_cb_t cb, int bx) {
        lv_obj_t *b = lv_btn_create(parent);
        lv_obj_set_pos(b, bx, btn_y);
        lv_obj_set_size(b, btn_w4, btn_h);
        lv_obj_set_style_bg_color(b, bg, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_70, LV_STATE_PRESSED);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_letter_space(l, 2, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, this);
        return b;
    };

    make_action_btn(LV_SYMBOL_PLAY " CICLO",  lv_color_hex(0x16A34A), cb_ciclo,   CX);
    make_action_btn("SINGOLO",                 lv_color_hex(0x3B82F6), cb_singolo, CX + (btn_w4 + GAP));
    make_action_btn("AVANTI",                  lv_color_hex(0x6366F1), cb_avanti,  CX + (btn_w4 + GAP) * 2);
    make_action_btn(LV_SYMBOL_CUT " TAGLIO",  lv_color_hex(0xD97706), cb_taglio,  CX + (btn_w4 + GAP) * 3);

    // ── BOTTONE STOP ─────────────────────────────────────────────────────────
    int stop_y = btn_y + btn_h + GAP;
    lv_obj_t *btn_stop_obj = lv_btn_create(parent);
    lv_obj_set_pos(btn_stop_obj, CX, stop_y);
    lv_obj_set_size(btn_stop_obj, CW, H - stop_y - GAP);
    lv_obj_set_style_bg_color(btn_stop_obj, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_radius(btn_stop_obj, 12, 0);
    lv_obj_set_style_border_width(btn_stop_obj, 0, 0);
    lv_obj_set_style_shadow_width(btn_stop_obj, 0, 0);
    lv_obj_add_event_cb(btn_stop_obj, cb_stop, LV_EVENT_CLICKED, this);
    btn_stop = btn_stop_obj;
    {
        lv_obj_t *ico = lv_label_create(btn_stop_obj);
        lv_label_set_text(ico, LV_SYMBOL_STOP);
        lv_obj_set_style_text_font(ico, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(ico, lv_color_hex(0xE11D48), 0);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 40, 0);

        lv_obj_t *ls = lv_label_create(btn_stop_obj);
        lv_label_set_text(ls, "STOP");
        lv_obj_set_style_text_font(ls, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(ls, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_letter_space(ls, 6, 0);
        lv_obj_center(ls);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI refresh (runs on LVGL task via lv_async_call)
// ─────────────────────────────────────────────────────────────────────────────

void AppTagliatubi::refresh_ciclo_tab(tagliatubi_state_t state, const tagliatubi_data_t *data)
{
    if (!lbl_counter || !lbl_stato || !lbl_info) return;

    char buf[64];
    snprintf(buf, sizeof(buf), "%d / %d pz", (int)data->prodotti, (int)data->quantita);
    lv_label_set_text(lbl_counter, buf);

    lv_label_set_text(lbl_stato, state_label(state));
    lv_obj_set_style_text_color(lbl_stato, state_color(state), 0);

    snprintf(buf, sizeof(buf), "L: %d mm  |  V: %d  |  diam: %d mm",
             (int)data->lunghezza, data->velocita, (int)data->diametro);
    lv_label_set_text(lbl_info, buf);
}

const char *AppTagliatubi::state_label(tagliatubi_state_t s)
{
    switch (s) {
        case TAGL_STATE_IDLE:               return "IDLE";
        case TAGL_STATE_RUNNING:            return "IN CORSO";
        case TAGL_STATE_CUTTING:            return "TAGLIO";
        case TAGL_STATE_DONE:               return "COMPLETATO";
        case TAGL_STATE_ERROR_NO_MATERIAL:  return "NO MATERIALE";
        case TAGL_STATE_ERROR_SAFETY:       return "SICUREZZA";
        case TAGL_STATE_ERROR_LENGTH:       return "ERRORE MISURA";
        default:                            return "—";
    }
}

lv_color_t AppTagliatubi::state_color(tagliatubi_state_t s)
{
    switch (s) {
        case TAGL_STATE_RUNNING:            return lv_color_hex(0x00FF88);
        case TAGL_STATE_CUTTING:            return lv_color_hex(0xFF8800);
        case TAGL_STATE_DONE:               return lv_color_hex(0x00D4FF);
        case TAGL_STATE_ERROR_NO_MATERIAL:
        case TAGL_STATE_ERROR_SAFETY:
        case TAGL_STATE_ERROR_LENGTH:       return lv_color_hex(0xFF4444);
        default:                            return lv_color_hex(0x888888);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Keyboard helpers
// ─────────────────────────────────────────────────────────────────────────────

void AppTagliatubi::show_keyboard(lv_obj_t *ta)
{
    if (!keyboard) return;
    lv_keyboard_set_textarea(keyboard, ta);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keyboard);
}

void AppTagliatubi::hide_keyboard(void)
{
    if (!keyboard) return;
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Event callbacks
// ─────────────────────────────────────────────────────────────────────────────

void AppTagliatubi::cb_ta_focused(lv_event_t *e)
{
    AppTagliatubi *app = static_cast<AppTagliatubi *>(lv_event_get_user_data(e));
    app->show_keyboard(static_cast<lv_obj_t *>(lv_event_get_target(e)));
}

void AppTagliatubi::cb_keyboard_ready(lv_event_t *e)
{
    AppTagliatubi *app = static_cast<AppTagliatubi *>(lv_event_get_user_data(e));
    app->hide_keyboard();
}

// Cerca prodotto dal server usando il codice attuale (chiamato dopo numpad OK)
void AppTagliatubi::cerca_prodotto(void)
{
    if (!ta_codice) return;
    const char *codice = lv_label_get_text(ta_codice);
    if (!codice || strlen(codice) == 0 || strcmp(codice, "---") == 0) return;

    ESP_LOGI(TAG, "Cerca prodotto: %s", codice);

    esp_err_t ret = tagliatubi_manager_load_product(codice);
    if (ret == ESP_OK) {
        const tagliatubi_data_t *d = tagliatubi_manager_get_data();
        if (lbl_descr)    lv_label_set_text(lbl_descr, d->descrizione);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)d->lunghezza);
        if (ta_lunghezza) lv_label_set_text(ta_lunghezza, buf);
        snprintf(buf, sizeof(buf), "%d", (int)d->quantita);
        if (ta_quantita)  lv_label_set_text(ta_quantita, buf);
        snprintf(buf, sizeof(buf), "%d", d->velocita);
        if (ta_velocita)  lv_label_set_text(ta_velocita, buf);
        if (lbl_pill_stato) {
            lv_label_set_text(lbl_pill_stato, "CARICATO " LV_SYMBOL_OK);
            lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_pill_stato), lv_color_hex(0x16A34A), 0);
        }
        ESP_LOGI(TAG, "Prodotto caricato: %s — L:%d Q:%d V:%d",
                 d->descrizione, (int)d->lunghezza, (int)d->quantita, d->velocita);
    } else {
        if (lbl_descr)    lv_label_set_text(lbl_descr, "Prodotto non trovato");
        if (lbl_pill_stato) {
            lv_label_set_text(lbl_pill_stato, "NON TROVATO");
            lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_pill_stato), lv_color_hex(0xE11D48), 0);
        }
        ESP_LOGW(TAG, "Prodotto '%s' non trovato", codice);
    }
}

void AppTagliatubi::cb_salva(lv_event_t *e)
{
    AppTagliatubi *app = static_cast<AppTagliatubi *>(lv_event_get_user_data(e));

    int32_t l = atoi(lv_label_get_text(app->ta_lunghezza));
    int32_t q = atoi(lv_label_get_text(app->ta_quantita));
    int      v = atoi(lv_label_get_text(app->ta_velocita));

    tagliatubi_manager_set_lunghezza(l);
    tagliatubi_manager_set_quantita(q);
    tagliatubi_manager_set_velocita(v);

    tagliatubi_manager_send_lunghezza();
    tagliatubi_manager_send_quantita();
    tagliatubi_manager_send_velocita();

    ESP_LOGI(TAG, "Parametri salvati — L:%d Q:%d V:%d", (int)l, (int)q, v);
}

void AppTagliatubi::cb_ciclo(lv_event_t *e)
{
    tagliatubi_state_t stato = tagliatubi_manager_get_state();
    ESP_LOGI(TAG, "[CICLO] premuto — stato attuale: %d", (int)stato);
    esp_err_t ret = tagliatubi_manager_start_ciclo();
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "[CICLO] avviato OK");
    else
        ESP_LOGW(TAG, "[CICLO] rifiutato: %s (stato=%d)", esp_err_to_name(ret), (int)stato);
}

void AppTagliatubi::cb_singolo(lv_event_t *e)
{
    tagliatubi_state_t stato = tagliatubi_manager_get_state();
    ESP_LOGI(TAG, "[SINGOLO] premuto — stato attuale: %d", (int)stato);
    esp_err_t ret = tagliatubi_manager_singolo();
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "[SINGOLO] avviato OK");
    else
        ESP_LOGW(TAG, "[SINGOLO] rifiutato: %s (stato=%d)", esp_err_to_name(ret), (int)stato);
}

void AppTagliatubi::cb_avanti(lv_event_t *e)
{
    tagliatubi_state_t stato = tagliatubi_manager_get_state();
    ESP_LOGI(TAG, "[AVANTI] premuto — stato attuale: %d", (int)stato);
    esp_err_t ret = tagliatubi_manager_avanti();
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "[AVANTI] avviato OK — 1000 step");
    else
        ESP_LOGW(TAG, "[AVANTI] rifiutato: %s (stato=%d)", esp_err_to_name(ret), (int)stato);
}

void AppTagliatubi::cb_taglio(lv_event_t *e)
{
    tagliatubi_state_t stato = tagliatubi_manager_get_state();
    ESP_LOGI(TAG, "[TAGLIO] premuto — stato attuale: %d", (int)stato);
    esp_err_t ret = tagliatubi_manager_taglio();
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "[TAGLIO] avviato OK");
    else
        ESP_LOGW(TAG, "[TAGLIO] rifiutato: %s (stato=%d)", esp_err_to_name(ret), (int)stato);
}

void AppTagliatubi::cb_stop(lv_event_t *e)
{
    tagliatubi_state_t stato = tagliatubi_manager_get_state();
    ESP_LOGI(TAG, "[STOP] premuto — stato attuale: %d", (int)stato);
    tagliatubi_manager_stop();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

bool AppTagliatubi::back(void)
{
    g_instance = nullptr;
    tagliatubi_manager_stop();
    notifyCoreClosed();
    return true;
}

bool AppTagliatubi::close(void)
{
    g_instance = nullptr;
    return true;
}
