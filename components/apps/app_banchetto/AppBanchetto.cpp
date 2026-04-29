#include "AppBanchetto.hpp"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "screens.h"
#include "fonts.h"
#include <string.h>
#include <stdlib.h>

extern "C"
{
#include "banchetto_manager.h"
#include "tastiera.h"
#include "wifi_manager.h"
#include "collaudo_manager.h"
}
extern "C" void myBeep(void);
extern "C" void popup_controllo_open(void);
extern "C" void popup_avviso_open(const char *titolo, const char *messaggio, bool offline);
extern "C" void popup_ok_open(const char *titolo, const char *messaggio, const char *footer, void (*on_dismiss)(void));

static const char *TAG = "AppBanchetto";

// ─── Sensibilità swipe (pixel minimi) ────────────────────
#define SWIPE_H_THRESHOLD 80 // orizzontale: cambia pagina
#define SWIPE_V_THRESHOLD 80 // verticale: cambia articolo

// ─── Toggle versa ─────────────────────────────────────────
#define VERSA_SW_MAX 4
static lv_obj_t *s_versa_switches[VERSA_SW_MAX] = {};
static uint8_t s_versa_sw_count = 0;

static void versa_switch_cb(lv_event_t *e)
{
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    banchetto_manager_set_versa_abilitato(on);
    // Sincronizza tutti gli altri switch
    for (uint8_t i = 0; i < s_versa_sw_count; i++)
    {
        if (s_versa_switches[i] && s_versa_switches[i] != sw)
        {
            if (on)
                lv_obj_add_state(s_versa_switches[i], LV_STATE_CHECKED);
            else
                lv_obj_clear_state(s_versa_switches[i], LV_STATE_CHECKED);
        }
    }
}

// ─── Variabili statiche — array per articolo ─────────────
lv_obj_t *AppBanchetto::page1_scr[BANCHETTO_MAX_ITEMS] = {};
lv_obj_t *AppBanchetto::lbl_matricola[BANCHETTO_MAX_ITEMS] = {};
lv_obj_t *AppBanchetto::lbl_ciclo[BANCHETTO_MAX_ITEMS] = {};
lv_obj_t *AppBanchetto::lbl_codice[BANCHETTO_MAX_ITEMS] = {};
lv_obj_t *AppBanchetto::lbl_descr[BANCHETTO_MAX_ITEMS] = {};
lv_obj_t *AppBanchetto::lbl_odp[BANCHETTO_MAX_ITEMS] = {};
lv_obj_t *AppBanchetto::lbl_fase[BANCHETTO_MAX_ITEMS] = {};
lv_obj_t *AppBanchetto::lbl_sessione_stato[BANCHETTO_MAX_ITEMS] = {};
lv_obj_t *AppBanchetto::lbl_banc[BANCHETTO_MAX_ITEMS] = {};
lv_obj_t *AppBanchetto::current_scr = nullptr;
lv_obj_t *AppBanchetto::offline_banner = nullptr;
lv_timer_t *AppBanchetto::offline_timer = nullptr;

// ─── Variabili statiche — pagine tagliatubi ──────────────
lv_obj_t *AppBanchetto::page3_scr = nullptr;
lv_obj_t *AppBanchetto::page4_scr = nullptr;
lv_obj_t *AppBanchetto::p3_lbl_codice = nullptr;
lv_obj_t *AppBanchetto::p3_lbl_descr = nullptr;
lv_obj_t *AppBanchetto::p3_lbl_lunghezza = nullptr;
lv_obj_t *AppBanchetto::p3_lbl_quantita = nullptr;
lv_obj_t *AppBanchetto::p3_lbl_velocita = nullptr;
lv_obj_t *AppBanchetto::p3_lbl_pill = nullptr;
lv_obj_t *AppBanchetto::p3_pill = nullptr;
lv_obj_t *AppBanchetto::p3_lbl_op_val = nullptr;
lv_obj_t *AppBanchetto::p4_lbl_counter = nullptr;
lv_obj_t *AppBanchetto::p4_lbl_stato = nullptr;
lv_obj_t *AppBanchetto::p4_lbl_avanzamento = nullptr;
lv_obj_t *AppBanchetto::p4_pill_uomo_morto = nullptr;
lv_obj_t *AppBanchetto::p4_lbl_uomo_morto = nullptr;
lv_obj_t *AppBanchetto::p4_pill_materiale = nullptr;
lv_obj_t *AppBanchetto::p4_lbl_materiale = nullptr;
lv_obj_t *AppBanchetto::p4_pill_carter = nullptr;
lv_obj_t *AppBanchetto::p4_lbl_carter = nullptr;
lv_timer_t *AppBanchetto::p4_uomo_morto_timer = nullptr;
uint8_t AppBanchetto::s_tagl_idx = 255;

// ─── Variabili statiche — pagine collaudo ────────────────
lv_obj_t *AppBanchetto::page5_scr = nullptr;
lv_obj_t *AppBanchetto::page6_scr = nullptr;
lv_obj_t *AppBanchetto::s_coll_scan_panel = nullptr;
lv_obj_t *AppBanchetto::s_coll_scan_lbl = nullptr;
lv_obj_t *AppBanchetto::s_coll_data_panel = nullptr;
lv_obj_t *AppBanchetto::s_coll_pill_sx = nullptr;
lv_obj_t *AppBanchetto::s_coll_pill_dx = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_operatore = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_matricola = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_consumo_ist = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_giri_ist = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_consumo_min = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_consumo_min_val = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_consumo_max = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_consumo_max_val = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_giri_min     = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_giri_min_val = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_giri_max     = nullptr;
lv_obj_t *AppBanchetto::s_coll_lbl_giri_max_val = nullptr;
lv_obj_t *AppBanchetto::s_coll_dot[3]           = {};
lv_obj_t *AppBanchetto::s_coll_lbl_scatola      = nullptr;
lv_obj_t *AppBanchetto::s_coll_btn_bar          = nullptr;
lv_obj_t *AppBanchetto::s_coll_action_btn[4]    = {};
uint8_t   AppBanchetto::s_coll_current_fase     = 0;
static lv_timer_t *s_rpm_timer                  = nullptr;
bool      AppBanchetto::s_coll_fase_ok[3]       = {};
float     AppBanchetto::s_coll_live_consumo     = 0.0f;
float     AppBanchetto::s_coll_live_giri        = 0.0f;
float     AppBanchetto::s_coll_fase_consumo[3]  = {};
float     AppBanchetto::s_coll_fase_giri[3]     = {};
uint8_t   AppBanchetto::s_coll_idx              = 255;

// ─── Numpad inline (page3) ───────────────────────────────
struct TNumpadTarget
{
    lv_obj_t *label;
    const char *title;
    int max_digits;
};
static TNumpadTarget t_np_targets[2]; // [0]=lunghezza [1]=velocita

static lv_obj_t *t_np_overlay = nullptr;
static lv_obj_t *t_np_popup = nullptr;
static lv_obj_t *t_np_display = nullptr;
static lv_obj_t *t_np_target_lbl = nullptr;
static int t_np_maxdig = 5;
static char t_np_buf[8] = {0};

static void t_np_update_display(void)
{
    if (t_np_display)
        lv_label_set_text(t_np_display, t_np_buf[0] ? t_np_buf : "0");
}
static void t_np_close(void)
{
    if (t_np_overlay)
    {
        lv_obj_del(t_np_overlay);
        t_np_overlay = nullptr;
    }
    if (t_np_popup)
    {
        lv_obj_del(t_np_popup);
        t_np_popup = nullptr;
    }
    t_np_display = t_np_target_lbl = nullptr;
    memset(t_np_buf, 0, sizeof(t_np_buf));
}
static void t_cb_np_digit(lv_event_t *e)
{
    char d = (char)(intptr_t)lv_event_get_user_data(e);
    size_t len = strlen(t_np_buf);
    if ((int)len >= t_np_maxdig)
        return;
    if (len == 0 && d == '0')
        return;
    t_np_buf[len] = d;
    t_np_buf[len + 1] = '\0';
    t_np_update_display();
}
static void t_cb_np_bs(lv_event_t *e)
{
    (void)e;
    size_t len = strlen(t_np_buf);
    if (len > 0)
        t_np_buf[len - 1] = '\0';
    t_np_update_display();
}
static void t_cb_np_ok(lv_event_t *e)
{
    (void)e;
    if (t_np_target_lbl && t_np_buf[0])
        lv_label_set_text(t_np_target_lbl, t_np_buf);
    t_np_close();
}
static void t_cb_np_cancel(lv_event_t *e)
{
    (void)e;
    t_np_close();
}

static lv_obj_t *t_np_btn(lv_obj_t *parent, const char *txt, lv_color_t bg,
                          lv_event_cb_t cb, void *ud)
{
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
    if (cb)
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}
static void t_cb_open_numpad(lv_event_t *e)
{
    TNumpadTarget *t = static_cast<TNumpadTarget *>(lv_event_get_user_data(e));
    if (t_np_popup)
        return;
    t_np_target_lbl = t->label;
    t_np_maxdig = t->max_digits;
    memset(t_np_buf, 0, sizeof(t_np_buf)); // sempre vuoto all'apertura

    t_np_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(t_np_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(t_np_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(t_np_overlay, 160, 0);
    lv_obj_set_style_border_width(t_np_overlay, 0, 0);
    lv_obj_set_style_radius(t_np_overlay, 0, 0);
    lv_obj_clear_flag(t_np_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(t_np_overlay, LV_OBJ_FLAG_CLICKABLE);

    t_np_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(t_np_popup, 400, 500);
    lv_obj_center(t_np_popup);
    lv_obj_move_foreground(t_np_popup);
    lv_obj_set_style_bg_color(t_np_popup, lv_color_hex(0x141E30), 0);
    lv_obj_set_style_bg_opa(t_np_popup, 255, 0);
    lv_obj_set_style_border_color(t_np_popup, lv_color_hex(0x4A90E2), 0);
    lv_obj_set_style_border_width(t_np_popup, 2, 0);
    lv_obj_set_style_radius(t_np_popup, 16, 0);
    lv_obj_set_style_pad_all(t_np_popup, 16, 0);
    lv_obj_clear_flag(t_np_popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *titolo = lv_label_create(t_np_popup);
    lv_label_set_text(titolo, t->title);
    lv_obj_set_style_text_font(titolo, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(titolo, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(titolo, LV_PCT(100));
    lv_obj_set_style_text_align(titolo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(titolo, LV_ALIGN_TOP_MID, 0, 0);

    t_np_display = lv_label_create(t_np_popup);
    lv_label_set_text(t_np_display, t_np_buf[0] ? t_np_buf : "0");
    lv_obj_set_style_text_font(t_np_display, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(t_np_display, lv_color_hex(0x2ECC71), 0);
    lv_obj_set_style_bg_color(t_np_display, lv_color_hex(0x0D1526), 0);
    lv_obj_set_style_bg_opa(t_np_display, 255, 0);
    lv_obj_set_style_radius(t_np_display, 8, 0);
    lv_obj_set_style_pad_all(t_np_display, 10, 0);
    lv_obj_set_style_text_align(t_np_display, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(t_np_display, LV_PCT(100));
    lv_obj_align(t_np_display, LV_ALIGN_TOP_MID, 0, 36);

    lv_obj_t *grid = lv_obj_create(t_np_popup);
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

    const char *digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
    for (int i = 0; i < 9; i++)
    {
        lv_obj_t *b = t_np_btn(grid, digits[i], lv_color_hex(0x2C5282),
                               t_cb_np_digit, (void *)(intptr_t)(digits[i][0]));
        lv_obj_set_grid_cell(b, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
    }
    lv_obj_t *b_bs = t_np_btn(grid, LV_SYMBOL_BACKSPACE, lv_color_hex(0xF39C12), t_cb_np_bs, nullptr);
    lv_obj_set_grid_cell(b_bs, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
    lv_obj_t *b0 = t_np_btn(grid, "0", lv_color_hex(0x2C5282), t_cb_np_digit, (void *)(intptr_t)'0');
    lv_obj_set_grid_cell(b0, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
    lv_obj_t *dummy = lv_obj_create(grid);
    lv_obj_set_style_bg_opa(dummy, 0, 0);
    lv_obj_set_style_border_width(dummy, 0, 0);
    lv_obj_set_grid_cell(dummy, LV_GRID_ALIGN_STRETCH, 2, 1, LV_GRID_ALIGN_STRETCH, 3, 1);

    lv_obj_t *btn_c = t_np_btn(t_np_popup, "Annulla", lv_color_hex(0xFA0000), t_cb_np_cancel, nullptr);
    lv_obj_set_size(btn_c, 160, 55);
    lv_obj_align(btn_c, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *btn_ok = t_np_btn(t_np_popup, "OK", lv_color_hex(0x2ECC71), t_cb_np_ok, nullptr);
    lv_obj_set_size(btn_ok, 160, 55);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

// ─── Tagliatubi state payload ─────────────────────────────
struct TaqlUpdatePayload
{
    tagliatubi_state_t state;
    tagliatubi_data_t data;
};

LV_IMG_DECLARE(b2);

// ─────────────────────────────────────────────────────────
// TAGLIATUBI STATE CALLBACK
// ─────────────────────────────────────────────────────────

static void banchetto_tagl_state_cb(tagliatubi_state_t state, const tagliatubi_data_t *data)
{
    auto *p = new TaqlUpdatePayload{state, *data};
    lv_async_call(AppBanchetto::on_tagl_state_update, p);
}

void AppBanchetto::on_tagl_state_update(void *user_data)
{
    auto *p = static_cast<TaqlUpdatePayload *>(user_data);
    refresh_page4(p->state, &p->data);
    delete p;
}

static const char *tagl_state_label(tagliatubi_state_t s)
{
    switch (s)
    {
    case TAGL_STATE_RUNNING:
        return "IN CORSO";
    case TAGL_STATE_CUTTING:
        return "TAGLIO";
    case TAGL_STATE_DONE:
        return "COMPLETATO";
    case TAGL_STATE_ERROR_NO_MATERIAL:
        return "NO MATERIALE";
    case TAGL_STATE_ERROR_SAFETY:
        return "SPORTELLO APERTO";
    case TAGL_STATE_ERROR_LENGTH:
        return "ERRORE MISURA";
    case TAGL_STATE_BOX_FULL:
        return "SCATOLA PIENA";
    case TAGL_STATE_ERROR_UOMO_MORTO:
        return "SICUREZZA";
    default:
        return "IDLE";
    }
}
static lv_color_t tagl_state_color(tagliatubi_state_t s)
{
    switch (s)
    {
    case TAGL_STATE_RUNNING:
        return lv_color_hex(0x00FF88);
    case TAGL_STATE_CUTTING:
        return lv_color_hex(0xFF8800);
    case TAGL_STATE_DONE:
        return lv_color_hex(0x00D4FF);
    case TAGL_STATE_ERROR_NO_MATERIAL:
    case TAGL_STATE_ERROR_SAFETY:
    case TAGL_STATE_ERROR_LENGTH:
    case TAGL_STATE_ERROR_UOMO_MORTO:
        return lv_color_hex(0xFF4444);
    case TAGL_STATE_BOX_FULL:
        return lv_color_hex(0xFFAA00);
    default:
        return lv_color_hex(0x888888);
    }
}

// ─────────────────────────────────────────────────────────
// TIMER — polling pulsante uomo morto (200ms)
// ─────────────────────────────────────────────────────────
static void update_pin_pill(lv_obj_t *pill, lv_obj_t *lbl, bool ok)
{
    lv_label_set_text(lbl, ok ? LV_SYMBOL_OK " OK" : LV_SYMBOL_CLOSE " NO");
    lv_obj_set_style_bg_color(pill, ok ? lv_color_hex(0x16A34A) : lv_color_hex(0xE11D48), 0);
}

static void p4_uomo_morto_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!AppBanchetto::p4_pill_uomo_morto ||
        !AppBanchetto::p4_pill_materiale ||
        !AppBanchetto::p4_pill_carter)
    {
        ESP_LOGW("AppBanchetto", "[p4 timer] pill nullptr — cancello timer");
        lv_timer_del(t);
        AppBanchetto::p4_uomo_morto_timer = nullptr;
        return;
    }

    bool sic = tagliatubi_manager_is_uomo_morto();
    bool mat = tagliatubi_manager_is_materiale();
    bool car = tagliatubi_manager_is_carter();

    // Log solo quando cambia stato (evita flood)
    static bool prev_sic = false, prev_mat = false, prev_car = false;
    if (sic != prev_sic || mat != prev_mat || car != prev_car)
    {
        ESP_LOGI("AppBanchetto", "[pin] SICUREZZA=%s  MATERIALE=%s  CARTER=%s",
                 sic ? "OK" : "NO", mat ? "OK" : "NO", car ? "OK" : "NO");
        prev_sic = sic;
        prev_mat = mat;
        prev_car = car;
    }

    update_pin_pill(AppBanchetto::p4_pill_uomo_morto, AppBanchetto::p4_lbl_uomo_morto, sic);
    update_pin_pill(AppBanchetto::p4_pill_materiale, AppBanchetto::p4_lbl_materiale, mat);
    update_pin_pill(AppBanchetto::p4_pill_carter, AppBanchetto::p4_lbl_carter, car);
}

// ─────────────────────────────────────────────────────────
// CREA PAGE 3 — Impostazioni tagliatubi
// ─────────────────────────────────────────────────────────

void AppBanchetto::crea_page3(uint8_t idx)
{
    banchetto_data_t d;
    banchetto_manager_get_item(idx, &d);

    lv_obj_t *scr = lv_obj_create(NULL);
    page3_scr = scr;
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(scr, 255, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, swipe_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(scr, swipe_event_cb, LV_EVENT_RELEASED, NULL);

    // ── SIDEBAR NERA ─────────────────────────────────────────
    lv_obj_t *sidebar = lv_obj_create(scr);
    lv_obj_set_pos(sidebar, 0, 0);
    lv_obj_set_size(sidebar, 260, 549);
    lv_obj_set_style_bg_color(sidebar, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_bg_opa(sidebar, 255, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);
    lv_obj_set_style_radius(sidebar, 0, 0);
    lv_obj_set_style_pad_all(sidebar, 24, 0);
    lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sidebar, LV_OBJ_FLAG_EVENT_BUBBLE);
    add_versa_switch(sidebar);

    lv_obj_t *lbl_op_tit = lv_label_create(sidebar);
    lv_label_set_text(lbl_op_tit, "OPERATORE");
    lv_obj_set_style_text_font(lbl_op_tit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_op_tit, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_letter_space(lbl_op_tit, 4, 0);
    lv_obj_align(lbl_op_tit, LV_ALIGN_TOP_LEFT, 0, 0);

    p3_lbl_op_val = lv_label_create(sidebar);
    lv_label_set_text(p3_lbl_op_val, d.sessione_aperta ? d.matricola : "0000");
    lv_obj_set_style_text_font(p3_lbl_op_val, &ui_font_my_font75, 0);
    lv_obj_set_style_text_color(p3_lbl_op_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(p3_lbl_op_val, LV_ALIGN_TOP_LEFT, 0, 28);

    p3_pill = lv_obj_create(sidebar);
    lv_obj_t *pill = p3_pill;
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pill, d.sessione_aperta ? lv_color_hex(0x16A34A) : lv_color_hex(0xE11D48), 0);
    lv_obj_set_style_bg_opa(pill, 255, 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_radius(pill, 4, 0);
    lv_obj_set_style_pad_top(pill, 4, 0);
    lv_obj_set_style_pad_bottom(pill, 4, 0);
    lv_obj_set_style_pad_left(pill, 10, 0);
    lv_obj_set_style_pad_right(pill, 10, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(pill, LV_ALIGN_TOP_LEFT, 0, 130);
    lv_obj_add_flag(pill, LV_OBJ_FLAG_EVENT_BUBBLE);

    p3_lbl_pill = lv_label_create(pill);
    lv_label_set_text(p3_lbl_pill, d.sessione_aperta ? "LOGGATO " LV_SYMBOL_OK : "NON LOGGATO");
    lv_obj_set_style_text_font(p3_lbl_pill, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(p3_lbl_pill, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_letter_space(p3_lbl_pill, 2, 0);
    lv_obj_align(p3_lbl_pill, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *sep = lv_obj_create(sidebar);
    lv_obj_set_pos(sep, 0, 340);
    lv_obj_set_size(sep, 212, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(sep, 255, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_add_flag(sep, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *lbl_banc_tit = lv_label_create(sidebar);
    lv_label_set_text(lbl_banc_tit, "BANCHETTO");
    lv_obj_set_style_text_font(lbl_banc_tit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_banc_tit, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_letter_space(lbl_banc_tit, 3, 0);
    lv_obj_align(lbl_banc_tit, LV_ALIGN_TOP_LEFT, 0, 358);

    lv_obj_t *lbl_banc_val = lv_label_create(sidebar);
    lv_label_set_text(lbl_banc_val, d.banchetto);
    lv_obj_set_style_text_font(lbl_banc_val, &ui_font_my_font75, 0);
    lv_obj_set_style_text_color(lbl_banc_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_banc_val, LV_ALIGN_TOP_LEFT, 0, 382);

    // ── CODICE ARTICOLO — hero giallo (readonly, da banchetto) ───────────────
    lv_obj_t *box_codice = lv_obj_create(scr);
    lv_obj_set_pos(box_codice, 276, 16);
    lv_obj_set_size(box_codice, 732, 120);
    lv_obj_set_style_bg_color(box_codice, lv_color_hex(0xFFDD00), 0);
    lv_obj_set_style_bg_opa(box_codice, 255, 0);
    lv_obj_set_style_border_width(box_codice, 0, 0);
    lv_obj_set_style_radius(box_codice, 12, 0);
    lv_obj_set_style_pad_left(box_codice, 20, 0);
    lv_obj_set_style_pad_top(box_codice, 12, 0);
    lv_obj_clear_flag(box_codice, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(box_codice, LV_OBJ_FLAG_EVENT_BUBBLE);
    {
        lv_obj_t *t = lv_label_create(box_codice);
        lv_label_set_text(t, "CODICE ARTICOLO");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000066), 0);
        lv_obj_set_style_text_letter_space(t, 4, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        p3_lbl_codice = lv_label_create(box_codice);
        lv_label_set_text(p3_lbl_codice, d.codice_articolo);
        lv_obj_set_style_text_font(p3_lbl_codice, &ui_font_my_font75, 0);
        lv_obj_set_style_text_color(p3_lbl_codice, lv_color_hex(0x000000), 0);
        lv_obj_align(p3_lbl_codice, LV_ALIGN_TOP_LEFT, 0, 26);
    }

    // ── LUNGHEZZA — blu, tappabile ────────────────────────────────────────────
    t_np_targets[0] = {nullptr, "LUNGHEZZA (mm)", 5};
    lv_obj_t *box_lung = lv_btn_create(scr);
    lv_obj_set_pos(box_lung, 276, 152);
    lv_obj_set_size(box_lung, 342, 110);
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
        p3_lbl_lunghezza = lv_label_create(box_lung);
        lv_label_set_text(p3_lbl_lunghezza, "0");
        lv_obj_set_style_text_font(p3_lbl_lunghezza, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(p3_lbl_lunghezza, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(p3_lbl_lunghezza, LV_ALIGN_TOP_LEFT, 0, 28);
    }
    t_np_targets[0].label = p3_lbl_lunghezza;
    lv_obj_add_event_cb(box_lung, t_cb_open_numpad, LV_EVENT_CLICKED, &t_np_targets[0]);

    // ── SEPARATORE (vuoto) ────────────────────────────────────────────────────
    lv_obj_t *box_qta = lv_obj_create(scr);
    lv_obj_set_pos(box_qta, 634, 152);
    lv_obj_set_size(box_qta, 16, 110);
    lv_obj_set_style_bg_opa(box_qta, 0, 0);
    lv_obj_set_style_border_width(box_qta, 0, 0);
    lv_obj_clear_flag(box_qta, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(box_qta, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_label_create(box_qta);

    // ── VELOCITA' — rosso, tappabile ─────────────────────────────────────────
    t_np_targets[1] = {nullptr, "VELOCITA' (1-99)", 2};
    lv_obj_t *box_vel = lv_btn_create(scr);
    lv_obj_set_pos(box_vel, 666, 152);
    lv_obj_set_size(box_vel, 342, 110);
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
        p3_lbl_velocita = lv_label_create(box_vel);
        lv_label_set_text(p3_lbl_velocita, "0");
        lv_obj_set_style_text_font(p3_lbl_velocita, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(p3_lbl_velocita, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(p3_lbl_velocita, LV_ALIGN_TOP_LEFT, 0, 28);
    }
    t_np_targets[1].label = p3_lbl_velocita;
    lv_obj_add_event_cb(box_vel, t_cb_open_numpad, LV_EVENT_CLICKED, &t_np_targets[1]);

    // ── DESCRIZIONE ARTICOLO ──────────────────────────────────────────────────
    lv_obj_t *box_descr = lv_obj_create(scr);
    lv_obj_set_pos(box_descr, 276, 278);
    lv_obj_set_size(box_descr, 732, 130);
    lv_obj_set_style_bg_color(box_descr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(box_descr, 255, 0);
    lv_obj_set_style_border_width(box_descr, 0, 0);
    lv_obj_set_style_radius(box_descr, 12, 0);
    lv_obj_set_style_pad_all(box_descr, 16, 0);
    lv_obj_clear_flag(box_descr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box_descr, LV_OBJ_FLAG_EVENT_BUBBLE);
    {
        lv_obj_t *t = lv_label_create(box_descr);
        lv_label_set_text(t, "DESCRIZIONE ARTICOLO");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000044), 0);
        lv_obj_set_style_text_letter_space(t, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);
        p3_lbl_descr = lv_label_create(box_descr);
        lv_label_set_text(p3_lbl_descr, d.descrizione_articolo);
        lv_obj_set_style_text_font(p3_lbl_descr, &lv_font_montserrat_30, 0);
        lv_obj_set_style_text_color(p3_lbl_descr, lv_color_hex(0x000000), 0);
        lv_obj_set_width(p3_lbl_descr, 700);
        lv_label_set_long_mode(p3_lbl_descr, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(p3_lbl_descr, LV_ALIGN_TOP_LEFT, 0, 28);
    }

    // ── BOTTONE SALVA ─────────────────────────────────────────────────────────
    lv_obj_t *btn_salva = lv_btn_create(scr);
    lv_obj_set_pos(btn_salva, 276, 424);
    lv_obj_set_size(btn_salva, 732, 110);
    lv_obj_set_style_bg_color(btn_salva, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_salva, lv_color_hex(0x222222), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_salva, 12, 0);
    lv_obj_set_style_border_width(btn_salva, 0, 0);
    lv_obj_set_style_shadow_width(btn_salva, 0, 0);
    lv_obj_add_event_cb(btn_salva, [](lv_event_t *)
                        {
        // Se id=0 il prodotto non è stato caricato: ricarica dal codice banchetto
        if (tagliatubi_manager_get_data()->id == 0 && AppBanchetto::s_tagl_idx != 255) {
            banchetto_data_t bd;
            banchetto_manager_get_item(AppBanchetto::s_tagl_idx, &bd);
            ESP_LOGI("AppBanchetto", "[SALVA] id=0, ricarico prodotto cod=%s", bd.codice_articolo);
            tagliatubi_manager_load_product(bd.codice_articolo);
        }
        const char *ltxt = lv_label_get_text(AppBanchetto::p3_lbl_lunghezza);
        int32_t l = (ltxt && ltxt[0] != '\0') ? atoi(ltxt) : -1;
        int      v = atoi(lv_label_get_text(AppBanchetto::p3_lbl_velocita));
        tagliatubi_manager_set_velocita(v);
        if (l > 0) tagliatubi_manager_set_lunghezza(l);
        if (l > 0) tagliatubi_manager_send_lunghezza();
        tagliatubi_manager_send_quantita();
        tagliatubi_manager_send_velocita();
        ESP_LOGI("AppBanchetto", "[SALVA] L:%ld V:%d id:%d", l, v, tagliatubi_manager_get_data()->id); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_salva, LV_OBJ_FLAG_EVENT_BUBBLE);
    {
        lv_obj_t *ico_R = lv_label_create(btn_salva);
        lv_label_set_text(ico_R, LV_SYMBOL_SAVE);
        lv_obj_set_style_text_font(ico_R, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(ico_R, lv_color_hex(0xFFDD00), 0);
        lv_obj_align(ico_R, LV_ALIGN_LEFT_MID, 40, 0);

        lv_obj_t *ico_L = lv_label_create(btn_salva);
        lv_label_set_text(ico_L, LV_SYMBOL_SAVE);
        lv_obj_set_style_text_font(ico_L, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(ico_L, lv_color_hex(0xFFDD00), 0);
        lv_obj_align(ico_L, LV_ALIGN_RIGHT_MID, -40, 0);
        lv_obj_t *lbl = lv_label_create(btn_salva);

        lv_label_set_text(lbl, "SALVA PARAMETRI");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_letter_space(lbl, 4, 0);
        lv_obj_center(lbl);
        // lv_obj_t *ico = lv_label_create(btn_salva);
    }

    for (uint32_t i = 0; i < lv_obj_get_child_cnt(scr); i++)
        lv_obj_add_flag(lv_obj_get_child(scr, i), LV_OBJ_FLAG_EVENT_BUBBLE);
}

// ─────────────────────────────────────────────────────────
// UPDATE PAGE 3
// ─────────────────────────────────────────────────────────
void AppBanchetto::update_page3(uint8_t idx)
{
    ESP_LOGI("AppBanchetto", "[DBG] update_page3 chiamata, idx=%d page3_scr=%p", idx, page3_scr);
    if (!page3_scr)
        return;
    banchetto_data_t d;
    banchetto_manager_get_item(idx, &d);
    ESP_LOGI("AppBanchetto", "[DBG] update_page3 codice=%s banchetto=%s", d.codice_articolo, d.banchetto);

    // Carica dal server solo se il prodotto non è ancora stato caricato
    const tagliatubi_data_t *td = tagliatubi_manager_get_data();
    if (td->id == 0)
    {
        esp_err_t ret = tagliatubi_manager_load_product(d.codice_articolo);
        if (ret != ESP_OK)
        {
            ESP_LOGW("AppBanchetto", "tagliatubi_manager_load_product(%s) failed", d.codice_articolo);
            return;
        }
        td = tagliatubi_manager_get_data();
    }

    ESP_LOGI("AppBanchetto", "[DBG page3] id=%d codice=%s desc=%s",
             td->id, td->codice, td->descrizione);
    ESP_LOGI("AppBanchetto", "[DBG page3] lunghezza=%ld diametro=%ld quantita=%ld prodotti=%ld velocita=%d",
             td->lunghezza, td->diametro, td->quantita, td->prodotti, td->velocita);

    // Lunghezza dal DB (vuota se 0 — operatore deve inserirla)
    if (p3_lbl_lunghezza)
    {
        if (td->lunghezza > 0)
        {
            char lbuf[16];
            snprintf(lbuf, sizeof(lbuf), "%ld", td->lunghezza);
            lv_label_set_text(p3_lbl_lunghezza, lbuf);
        }
        else
        {
            lv_label_set_text(p3_lbl_lunghezza, "");
        }
    }
    // Velocita dal DB
    if (p3_lbl_velocita)
    {
        char vbuf[8];
        snprintf(vbuf, sizeof(vbuf), "%d", td->velocita);
        lv_label_set_text(p3_lbl_velocita, vbuf);
    }
    tagliatubi_manager_set_quantita((int32_t)d.qta_totale);
    ESP_LOGI("AppBanchetto", "[DBG page3] banchetto qta_totale=%lu", d.qta_totale);

    if (p3_lbl_op_val)
        lv_label_set_text(p3_lbl_op_val, d.sessione_aperta ? d.matricola : "0000");
    if (p3_pill)
        lv_obj_set_style_bg_color(p3_pill, d.sessione_aperta ? lv_color_hex(0x16A34A) : lv_color_hex(0xE11D48), 0);
    if (p3_lbl_pill)
        lv_label_set_text(p3_lbl_pill, d.sessione_aperta ? "LOGGATO " LV_SYMBOL_OK : "NON LOGGATO");
}

// ─────────────────────────────────────────────────────────
// CREA PAGE 4 — Ciclo tagliatubi
// ─────────────────────────────────────────────────────────
void AppBanchetto::crea_page4(uint8_t idx)
{
    ESP_LOGI("AppBanchetto", "[crea_page4] idx=%d", idx);
    banchetto_data_t d;
    banchetto_manager_get_item(idx, &d);

    // Cleanup timer precedente se page4 viene ricreata
    if (p4_uomo_morto_timer)
    {
        lv_timer_del(p4_uomo_morto_timer);
        p4_uomo_morto_timer = nullptr;
    }
    p4_pill_uomo_morto = nullptr;
    p4_lbl_uomo_morto = nullptr;
    p4_pill_materiale = nullptr;
    p4_lbl_materiale = nullptr;
    p4_pill_carter = nullptr;
    p4_lbl_carter = nullptr;

    lv_obj_t *scr = lv_obj_create(NULL);
    page4_scr = scr;
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(scr, 255, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, swipe_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(scr, swipe_event_cb, LV_EVENT_RELEASED, NULL);

    const int SB = 260;
    const int GAP = 16;
    const int CX = SB + GAP;
    const int CW = 1024 - CX - GAP;
    const int H = 549;

    // ── SIDEBAR SCURA ────────────────────────────────────────────────────────
    lv_obj_t *sidebar = lv_obj_create(scr);
    lv_obj_set_pos(sidebar, 0, 0);
    lv_obj_set_size(sidebar, SB, H);
    lv_obj_set_style_bg_color(sidebar, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_bg_opa(sidebar, 255, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);
    lv_obj_set_style_radius(sidebar, 0, 0);
    lv_obj_set_style_pad_all(sidebar, 24, 0);
    lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sidebar, LV_OBJ_FLAG_EVENT_BUBBLE);
    add_versa_switch(sidebar, 255); // spostato sotto i 3 indicatori sicurezza

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
    lv_obj_add_flag(pill, LV_OBJ_FLAG_EVENT_BUBBLE);

    p4_lbl_stato = lv_label_create(pill);
    lv_label_set_text(p4_lbl_stato, "IDLE");
    lv_obj_set_style_text_font(p4_lbl_stato, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(p4_lbl_stato, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_letter_space(p4_lbl_stato, 2, 0);
    lv_obj_align(p4_lbl_stato, LV_ALIGN_CENTER, 0, 0);

    // ── Helper lambda per creare una pill di stato pin ───────────────────────
    auto make_pin_indicator = [&](const char *titolo, int y_lbl,
                                  lv_obj_t **out_pill, lv_obj_t **out_lbl)
    {
        ESP_LOGI("AppBanchetto", "[p4 pin indicator] crea '%s' y=%d", titolo, y_lbl);

        lv_obj_t *t = lv_label_create(sidebar);
        lv_label_set_text(t, titolo);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x555555), 0);
        lv_obj_set_style_text_letter_space(t, 2, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, y_lbl);
        lv_obj_add_flag(t, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t *p = lv_obj_create(sidebar);
        lv_obj_set_size(p, 150, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(p, lv_color_hex(0xE11D48), 0);
        lv_obj_set_style_bg_opa(p, 255, 0);
        lv_obj_set_style_border_width(p, 0, 0);
        lv_obj_set_style_radius(p, 4, 0);
        lv_obj_set_style_pad_top(p, 5, 0);
        lv_obj_set_style_pad_bottom(p, 5, 0);
        lv_obj_set_style_pad_left(p, 10, 0);
        lv_obj_set_style_pad_right(p, 10, 0);
        lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(p, LV_ALIGN_TOP_LEFT, 0, y_lbl + 22);
        lv_obj_add_flag(p, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t *l = lv_label_create(p);
        lv_label_set_text(l, LV_SYMBOL_CLOSE " NO");
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

        *out_pill = p;
        *out_lbl = l;
        ESP_LOGI("AppBanchetto", "[p4 pin indicator] '%s' pill=%p lbl=%p", titolo, p, l);
    };

    // SICUREZZA (uomo morto) y=68, MATERIALE y=128, CARTER y=188
    make_pin_indicator("", 68, &p4_pill_uomo_morto, &p4_lbl_uomo_morto);
    make_pin_indicator("MATERIALE", 128, &p4_pill_materiale, &p4_lbl_materiale);
    make_pin_indicator("CARTER", 188, &p4_pill_carter, &p4_lbl_carter);
    ESP_LOGI("AppBanchetto", "[p4] indicatori pin creati — avvio timer 200ms");

    // Avvia timer polling (200ms) per tutti e 3 i pin
    if (p4_uomo_morto_timer)
    {
        lv_timer_del(p4_uomo_morto_timer);
    }
    p4_uomo_morto_timer = lv_timer_create(p4_uomo_morto_timer_cb, 200, nullptr);
    ESP_LOGI("AppBanchetto", "[p4] timer=%p", p4_uomo_morto_timer);

    lv_obj_t *sep = lv_obj_create(sidebar);
    lv_obj_set_pos(sep, 0, 350);
    lv_obj_set_size(sep, SB - 48, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(sep, 255, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_add_flag(sep, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *tp = lv_label_create(sidebar);
    lv_label_set_text(tp, "L ENCODER");
    lv_obj_set_style_text_font(tp, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(tp, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_letter_space(tp, 4, 0);
    lv_obj_align(tp, LV_ALIGN_TOP_LEFT, 0, 358);

    p4_lbl_counter = lv_label_create(sidebar);
    lv_label_set_text(p4_lbl_counter, "");
    lv_obj_set_style_text_font(p4_lbl_counter, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(p4_lbl_counter, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(p4_lbl_counter, LV_ALIGN_TOP_LEFT, 0, 382);

    // ── CARD GIALLA — scatola pezzi/capacità ─────────────────────────────────
    lv_obj_t *card_top = lv_obj_create(scr);
    lv_obj_set_pos(card_top, CX, GAP);
    lv_obj_set_size(card_top, CW, 110);
    lv_obj_set_style_bg_color(card_top, lv_color_hex(0xFFDD00), 0);
    lv_obj_set_style_bg_opa(card_top, 255, 0);
    lv_obj_set_style_border_width(card_top, 0, 0);
    lv_obj_set_style_radius(card_top, 12, 0);
    lv_obj_set_style_pad_all(card_top, 14, 0);
    lv_obj_clear_flag(card_top, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(card_top, LV_OBJ_FLAG_EVENT_BUBBLE);
    {
        lv_obj_t *tit = lv_label_create(card_top);
        lv_label_set_text(tit, "SCATOLA");
        lv_obj_set_style_text_font(tit, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(tit, lv_color_hex(0x00000066), 0);
        lv_obj_set_style_text_letter_space(tit, 4, 0);
        lv_obj_align(tit, LV_ALIGN_TOP_LEFT, 0, 0);

        p4_lbl_avanzamento = lv_label_create(card_top);
        char buf[32];
        snprintf(buf, sizeof(buf), "%lu / %lu", d.qta_scatola, d.qta_totale_scatola);
        lv_label_set_text(p4_lbl_avanzamento, buf);
        lv_obj_set_style_text_font(p4_lbl_avanzamento, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(p4_lbl_avanzamento, lv_color_hex(0x000000), 0);
        lv_obj_align(p4_lbl_avanzamento, LV_ALIGN_TOP_LEFT, 0, 26);
    }

    // ── BOTTONI AZIONE: 3 righe ───────────────────────────────────────────────
    int btn_y = GAP + 110 + GAP;                 // y=142
    int btn_h = (H - btn_y - GAP - 2 * GAP) / 3; // ~119px, uguale per tutte e 3 le righe
    int btn_w2 = (CW - GAP) / 2;                 // larghezza per 2 bottoni = 358

    auto make_btn = [&](const char *txt, lv_color_t bg, lv_event_cb_t cb, int bx, int by)
    {
        lv_obj_t *b = lv_btn_create(scr);
        lv_obj_set_pos(b, bx, by);
        lv_obj_set_size(b, btn_w2, btn_h);
        lv_obj_set_style_bg_color(b, bg, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_70, LV_STATE_PRESSED);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_30, 0);
        lv_obj_set_style_text_letter_space(l, 2, 0);
        lv_obj_center(l);
        if (cb)
            lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_flag(b, LV_OBJ_FLAG_EVENT_BUBBLE);
    };

    // Riga 1: CICLO | SINGOLO
    make_btn(LV_SYMBOL_PLAY " CICLO", lv_color_hex(0x16A34A), [](lv_event_t *)
             {
        tagliatubi_state_t s = tagliatubi_manager_get_state();
        esp_err_t r = tagliatubi_manager_start_ciclo();
        if (r == ESP_OK) ESP_LOGI("AppBanchetto", "[CICLO] avviato OK");
        else ESP_LOGW("AppBanchetto", "[CICLO] rifiutato: %s (stato=%d)", esp_err_to_name(r), (int)s); }, CX, btn_y);
    make_btn("SINGOLO", lv_color_hex(0x3B82F6), [](lv_event_t *)
             {
        tagliatubi_state_t s = tagliatubi_manager_get_state();
        esp_err_t r = tagliatubi_manager_singolo();
        if (r == ESP_OK) ESP_LOGI("AppBanchetto", "[SINGOLO] avviato OK");
        else ESP_LOGW("AppBanchetto", "[SINGOLO] rifiutato: %s (stato=%d)", esp_err_to_name(r), (int)s); }, CX + btn_w2 + GAP, btn_y);

    // Riga 2: AVANTI | TAGLIO
    int row2_y = btn_y + btn_h + GAP;
    make_btn("AVANTI", lv_color_hex(0x6366F1), [](lv_event_t *)
             {
        tagliatubi_state_t s = tagliatubi_manager_get_state();
        esp_err_t r = tagliatubi_manager_avanti();
        if (r == ESP_OK) ESP_LOGI("AppBanchetto", "[AVANTI] avviato OK (stato=%d)", (int)s);
        else ESP_LOGW("AppBanchetto", "[AVANTI] rifiutato: %s (stato=%d)", esp_err_to_name(r), (int)s); }, CX, row2_y);
    make_btn(LV_SYMBOL_CUT " TAGLIO", lv_color_hex(0xD97706), [](lv_event_t *)
             {
        tagliatubi_state_t s = tagliatubi_manager_get_state();
        esp_err_t r = tagliatubi_manager_taglio();
        if (r == ESP_OK) ESP_LOGI("AppBanchetto", "[TAGLIO] avviato OK");
        else ESP_LOGW("AppBanchetto", "[TAGLIO] rifiutato: %s (stato=%d)", esp_err_to_name(r), (int)s); }, CX + btn_w2 + GAP, row2_y);

    // Riga 3: STOP (unico, larghezza piena)
    int stop_y = row2_y + btn_h + GAP;
    lv_obj_t *btn_stop = lv_btn_create(scr);
    lv_obj_set_pos(btn_stop, CX, stop_y);
    lv_obj_set_size(btn_stop, CW, btn_h);
    lv_obj_set_style_bg_color(btn_stop, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_radius(btn_stop, 12, 0);
    lv_obj_set_style_border_width(btn_stop, 0, 0);
    lv_obj_set_style_shadow_width(btn_stop, 0, 0);
    lv_obj_add_event_cb(btn_stop, [](lv_event_t *)
                        { tagliatubi_manager_stop(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_stop, LV_OBJ_FLAG_EVENT_BUBBLE);
    {
        lv_obj_t *ico = lv_label_create(btn_stop);
        lv_label_set_text(ico, LV_SYMBOL_STOP);
        lv_obj_set_style_text_font(ico, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(ico, lv_color_hex(0xE11D48), 0);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 40, 0);
        lv_obj_t *ls = lv_label_create(btn_stop);
        lv_label_set_text(ls, "STOP");
        lv_obj_set_style_text_font(ls, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(ls, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_letter_space(ls, 6, 0);
        lv_obj_center(ls);
    }

    for (uint32_t i = 0; i < lv_obj_get_child_cnt(scr); i++)
        lv_obj_add_flag(lv_obj_get_child(scr, i), LV_OBJ_FLAG_EVENT_BUBBLE);
}

// ─────────────────────────────────────────────────────────
// REFRESH PAGE 4
// ─────────────────────────────────────────────────────────
void AppBanchetto::add_versa_switch(lv_obj_t *sidebar, int y_offset)
{
    // Label
    lv_obj_t *lbl = lv_label_create(sidebar);
    lv_label_set_text(lbl, "CONTEGGIO");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_letter_space(lbl, 3, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, y_offset);

    // Switch
    lv_obj_t *sw = lv_switch_create(sidebar);
    lv_obj_set_size(sw, 80, 40);
    lv_obj_align_to(sw, lbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
    if (banchetto_manager_get_versa_abilitato())
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, versa_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (s_versa_sw_count < VERSA_SW_MAX)
        s_versa_switches[s_versa_sw_count++] = sw;
}

void AppBanchetto::update_page4_scatola(void)
{
    if (!p4_lbl_avanzamento || s_tagl_idx == 255)
        return;
    banchetto_data_t bd;
    banchetto_manager_get_item(s_tagl_idx, &bd);
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu / %lu", bd.qta_scatola, bd.qta_totale_scatola);
    lv_label_set_text(p4_lbl_avanzamento, buf);
}

void AppBanchetto::update_page5_collaudo(void)
{
    if (page5_scr)
        refresh_collaudo_fase();
}

void AppBanchetto::refresh_page4(tagliatubi_state_t state, const tagliatubi_data_t *data)
{
    if (!p4_lbl_counter || !p4_lbl_stato)
        return;
    char buf[64];
    // L ENCODER — lunghezza rilevata dall'encoder
    if (data->lunghezza_misurata > 0)
        snprintf(buf, sizeof(buf), "%ld mm", data->lunghezza_misurata);
    else
        buf[0] = '\0';
    lv_label_set_text(p4_lbl_counter, buf);
    lv_label_set_text(p4_lbl_stato, tagl_state_label(state));
    lv_obj_set_style_text_color(p4_lbl_stato, tagl_state_color(state), 0);

    if (state == TAGL_STATE_BOX_FULL)
        popup_avviso_open(LV_SYMBOL_WARNING " Scatola piena",
                          "Sostituire la scatola\nquindi riavviare il ciclo.", false);
    else if (state == TAGL_STATE_ERROR_NO_MATERIAL)
        popup_avviso_open(LV_SYMBOL_WARNING " Mancanza materiale",
                          "Controllare presenza materiale\ne riavviare.", false);
    else if (state == TAGL_STATE_ERROR_SAFETY)
        popup_avviso_open(LV_SYMBOL_WARNING " Sportello aperto",
                          "Chiudere lo sportello di protezione\nprima di continuare.", false);
    else if (state == TAGL_STATE_ERROR_UOMO_MORTO)
        popup_avviso_open(LV_SYMBOL_WARNING " Sicurezza",
                          "Tenere premuto il pulsante di sicurezza\nprima di avviare.", false);

    // Avanzamento fase — aggiornato dopo ogni versa
    if (p4_lbl_avanzamento && s_tagl_idx != 255)
    {
        banchetto_data_t bd;
        banchetto_manager_get_item(s_tagl_idx, &bd);
        snprintf(buf, sizeof(buf), "%lu / %lu", bd.qta_scatola, bd.qta_totale_scatola);
        lv_label_set_text(p4_lbl_avanzamento, buf);
    }
}

// ─────────────────────────────────────────────────────────
// COSTRUTTORE / DISTRUTTORE
// ─────────────────────────────────────────────────────────
AppBanchetto::AppBanchetto() : ESP_Brookesia_PhoneApp("Banchetto", &b2, true),
                               container(nullptr),
                               test_button(nullptr)
{
}

AppBanchetto::~AppBanchetto() {}

bool AppBanchetto::init(void)
{
    ESP_LOGI(TAG, "Init app");
    return true;
}

extern "C" void app_banchetto_update_page1(void)
{
    uint8_t count = banchetto_manager_get_count();
    for (uint8_t i = 0; i < count; i++)
        AppBanchetto::update_page1(i);
}

extern "C" void app_banchetto_update_page2(void)
{
    uint8_t count = banchetto_manager_get_count();
    for (uint8_t i = 0; i < count; i++)
        AppBanchetto::update_page2(i);

    AppBanchetto::update_page4_scatola();
    AppBanchetto::check_page2_logout();

#ifdef COLLAUDI_BANCHETTO_ID
    AppBanchetto::coll_check_logout();
#endif
}

extern "C" void app_banchetto_update_page3(void)
{
    AppBanchetto::update_page3(AppBanchetto::s_tagl_idx);
}

void AppBanchetto::reset_box_full(void)
{
    // Chiudi popup avviso se aperto (es. "Scatola piena")
    if (t_np_popup)
    {
        lv_obj_del(t_np_popup);
        t_np_popup = nullptr;
    }
    // Riporta stato page4 a IDLE
    if (p4_lbl_stato)
    {
        lv_label_set_text(p4_lbl_stato, "IDLE");
        lv_obj_set_style_text_color(p4_lbl_stato, lv_color_hex(0x888888), 0);
    }
}

extern "C" void app_banchetto_nuova_scatola(void)
{
    AppBanchetto::reset_box_full();
}

extern "C" void add_versa_switch_c(lv_obj_t *sidebar)
{
    AppBanchetto::add_versa_switch(sidebar);
}

// ─────────────────────────────────────────────────────────
// OFFLINE BANNER — timer callback (ogni 2s)
// ─────────────────────────────────────────────────────────
void AppBanchetto::offline_timer_cb(lv_timer_t *t)
{
    if (!offline_banner)
        return;
    if (wifi_is_connected() && banchetto_is_server_reachable())
        lv_obj_add_flag(offline_banner, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(offline_banner, LV_OBJ_FLAG_HIDDEN);
}

// ═════════════════════════════════════════════════════════
// COLLAUDO — PAGE 5  v2
// ═════════════════════════════════════════════════════════

#define COLL_CLR_BG 0x1A1A1A
#define COLL_CLR_HEADER 0x2C2C2C
#define COLL_CLR_BAND 0x333333
#define COLL_CLR_VERDE 0x00CC00
#define COLL_CLR_ROSSO 0xCC0000
#define COLL_CLR_GIALLO 0xFFE000
#define COLL_CLR_ARANCIONE 0xFF8800
#define COLL_CLR_TESTO_SCURO 0x111111

static const char *COLL_FASE_NOMI[3] = {"TOP", "MIN", "MAX"};

// Mapping fase→limiti server: 0=TOP, 1=MIN(minimo), 2=MAX(carico)
static void coll_get_limits(const collaudo_motore_t *m, int f,
                            float *cmin, float *cmax,
                            float *gmin, float *gmax)
{
    const float cm[3] = {m->top_consumo_min, m->minimo_consumo_min, m->carico_consumo_min};
    const float cx[3] = {m->top_consumo_max, m->minimo_consumo_max, m->carico_consumo_max};
    const float gm[3] = {m->top_giri_min, m->minimo_giri_min, m->carico_giri_min};
    const float gx[3] = {m->top_giri_max, m->minimo_giri_max, m->carico_giri_max};
    *cmin = cm[f];
    *cmax = cx[f];
    *gmin = gm[f];
    *gmax = gx[f];
}

// ─────────────────────────────────────────────────────────
// REFRESH FASE — aggiorna dots, limiti, colori bottoni
// ─────────────────────────────────────────────────────────
void AppBanchetto::refresh_collaudo_fase(void)
{
    collaudo_motore_t mot;
    if (!collaudo_manager_get_motore(&mot))
        return;

    uint8_t f = s_coll_current_fase;

    // Limiti fase corrente
    if (f <= 2)
    {
        float cmin, cmax, gmin, gmax;
        coll_get_limits(&mot, f, &cmin, &cmax, &gmin, &gmax);
        char buf[32];
        if (s_coll_lbl_consumo_min_val)
        {
            snprintf(buf, sizeof(buf), "%.3f", cmin);
            lv_label_set_text(s_coll_lbl_consumo_min_val, buf);
        }
        if (s_coll_lbl_consumo_max_val)
        {
            snprintf(buf, sizeof(buf), "%.3f", cmax);
            lv_label_set_text(s_coll_lbl_consumo_max_val, buf);
        }
        if (s_coll_lbl_giri_min_val)
        {
            snprintf(buf, sizeof(buf), "%.0f", gmin);
            lv_label_set_text(s_coll_lbl_giri_min_val, buf);
        }
        if (s_coll_lbl_giri_max_val)
        {
            snprintf(buf, sizeof(buf), "%.0f", gmax);
            lv_label_set_text(s_coll_lbl_giri_max_val, buf);
        }

        // Ricalcola colore istantanei: verde=ok, grigio=fuori tolleranza
        if (s_coll_lbl_consumo_ist && s_coll_live_consumo > 0.0f)
        {
            bool ok = (s_coll_live_consumo >= cmin && s_coll_live_consumo <= cmax);
            lv_obj_set_style_text_color(s_coll_lbl_consumo_ist,
                                        lv_color_hex(ok ? COLL_CLR_VERDE : 0x666666), 0);
        }
        if (s_coll_lbl_giri_ist && s_coll_live_giri > 0.0f)
        {
            bool ok = (s_coll_live_giri >= gmin && s_coll_live_giri <= gmax);
            lv_obj_set_style_text_color(s_coll_lbl_giri_ist,
                                        lv_color_hex(ok ? COLL_CLR_VERDE : 0x666666), 0);
        }
    }

    // Scatola counter
    if (s_coll_lbl_scatola) {
        banchetto_data_t bd = {};
        if (banchetto_manager_get_item(s_coll_idx, &bd) && bd.qta_totale_scatola > 0)
            lv_label_set_text_fmt(s_coll_lbl_scatola, "%lu/%lu", bd.qta_scatola, bd.qta_totale_scatola);
        else
            lv_label_set_text(s_coll_lbl_scatola, "-/-");
    }

    // Dots: verde=ok, bianco=attiva, grigio=futura
    for (int i = 0; i < 3; i++)
    {
        if (!s_coll_dot[i])
            continue;
        uint32_t bg, bd;
        if (s_coll_fase_ok[i])
        {
            bg = bd = COLL_CLR_VERDE;
        }
        else if (i == (int)f)
        {
            bg = bd = 0xFFFFFF;
        }
        else
        {
            bg = 0x555555;
            bd = 0x333333;
        }
        lv_obj_set_style_bg_color(s_coll_dot[i], lv_color_hex(bg), 0);
        lv_obj_set_style_border_color(s_coll_dot[i], lv_color_hex(bd), 0);
    }

    // Bottoni fase: TOP=giallo, MIN=azzurro, MAX=verde; inattivi=grigio; done=verde scuro
    const uint32_t col_active[3] = {0xF9A825, 0x29B6F6, 0x66BB6A};
    bool all_ok = s_coll_fase_ok[0] && s_coll_fase_ok[1] && s_coll_fase_ok[2];
    for (int i = 0; i < 3; i++)
    {
        if (!s_coll_action_btn[i])
            continue;
        uint32_t c;
        if (s_coll_fase_ok[i])
            c = 0x2E7D32;
        else if (i == (int)f)
            c = col_active[i];
        else
            c = 0x444444;
        lv_obj_set_style_bg_color(s_coll_action_btn[i], lv_color_hex(c), 0);
    }
    if (s_coll_action_btn[3])
        lv_obj_set_style_bg_color(s_coll_action_btn[3],
                                  lv_color_hex(all_ok ? 0x1565C0 : 0x444444), 0);
}

// ─────────────────────────────────────────────────────────
// COLL ON BTN — logica pressione tasto fase / FIN
// ─────────────────────────────────────────────────────────
void AppBanchetto::coll_on_btn(int btn_idx)
{
    if (btn_idx == 3)
    {
        // FIN
        bool all_ok = s_coll_fase_ok[0] && s_coll_fase_ok[1] && s_coll_fase_ok[2];
        if (!all_ok)
        {
            myBeep();
            ESP_LOGW("AppBanchetto", "FIN premuto ma non tutte le fasi OK");
            return;
        }
        collaudo_motore_t mot;
        collaudo_operatore_t op;
        collaudo_manager_get_motore(&mot);
        collaudo_manager_get_operatore(&op);

        banchetto_data_t bd2 = {};
        banchetto_manager_get_data(&bd2);
        collaudo_risultato_t res = {};
        snprintf(res.matricola, sizeof(res.matricola), "%s", mot.matricola);
        snprintf(res.codice_tipo, sizeof(res.codice_tipo), "%s", mot.codice_tipo);
        snprintf(res.operatore, sizeof(res.operatore), "%s",
                 bd2.matricola[0] ? bd2.matricola : (op.badge[0] ? op.badge : "---"));
        res.esito = 1;
        res.consumo[0] = s_coll_fase_consumo[0];
        res.consumo[1] = s_coll_fase_consumo[1];
        res.consumo[2] = s_coll_fase_consumo[2];
        res.giri[0] = s_coll_fase_giri[0];
        res.giri[1] = s_coll_fase_giri[1];
        res.giri[2] = s_coll_fase_giri[2];

        collaudo_bilancia_stop();
        collaudo_bilancia_check_async();
        collaudo_manager_save_result(&res);
        ESP_LOGI("AppBanchetto", "FIN: invio collaudo al server");
        return;
    }

    // Tasti fase 0/1/2
    if (btn_idx != (int)s_coll_current_fase)
        return;

    collaudo_motore_t mot;
    if (!collaudo_manager_get_motore(&mot))
        return;

    float cmin, cmax, gmin, gmax;
    coll_get_limits(&mot, btn_idx, &cmin, &cmax, &gmin, &gmax);

    bool c_ok = (s_coll_live_consumo >= cmin && s_coll_live_consumo <= cmax);
    bool g_ok = (s_coll_live_giri >= gmin && s_coll_live_giri <= gmax);

    if (!c_ok || !g_ok)
    {
        myBeep();
        ESP_LOGW("AppBanchetto", "Fase %d: valori fuori range (c=%.3f g=%.0f)",
                 btn_idx, s_coll_live_consumo, s_coll_live_giri);
        // Flash rosso 500ms sui display fuori tolleranza
        if (!c_ok && s_coll_lbl_consumo_ist)
            lv_obj_set_style_text_color(s_coll_lbl_consumo_ist, lv_color_hex(COLL_CLR_ROSSO), 0);
        if (!g_ok && s_coll_lbl_giri_ist)
            lv_obj_set_style_text_color(s_coll_lbl_giri_ist, lv_color_hex(COLL_CLR_ROSSO), 0);
        lv_timer_t *tmr = lv_timer_create([](lv_timer_t *t)
                                          {
            // Ripristina: verde se in tolleranza, grigio se fuori
            if (AppBanchetto::s_coll_current_fase > 2) return;
            collaudo_motore_t m;
            if (!collaudo_manager_get_motore(&m)) return;
            float cmin, cmax, gmin, gmax;
            coll_get_limits(&m, AppBanchetto::s_coll_current_fase, &cmin, &cmax, &gmin, &gmax);
            if (AppBanchetto::s_coll_lbl_consumo_ist) {
                bool ok = (AppBanchetto::s_coll_live_consumo >= cmin &&
                           AppBanchetto::s_coll_live_consumo <= cmax);
                lv_obj_set_style_text_color(AppBanchetto::s_coll_lbl_consumo_ist,
                    lv_color_hex(ok ? COLL_CLR_VERDE : 0x666666), 0);
            }
            if (AppBanchetto::s_coll_lbl_giri_ist) {
                bool ok = (AppBanchetto::s_coll_live_giri >= gmin &&
                           AppBanchetto::s_coll_live_giri <= gmax);
                lv_obj_set_style_text_color(AppBanchetto::s_coll_lbl_giri_ist,
                    lv_color_hex(ok ? COLL_CLR_VERDE : 0x666666), 0);
            } }, 500, nullptr);
        lv_timer_set_repeat_count(tmr, 1);
        return;
    }

    myBeep();
    s_coll_fase_ok[btn_idx] = true;
    s_coll_fase_consumo[btn_idx] = s_coll_live_consumo;
    s_coll_fase_giri[btn_idx] = s_coll_live_giri;
    ESP_LOGI("AppBanchetto", "Fase %d OK: c=%.3f g=%.0f", btn_idx,
             s_coll_live_consumo, s_coll_live_giri);

    // Avanza alla prossima fase non ancora completata
    if (btn_idx < 2) {
        uint8_t next = btn_idx + 1;
        while (next < 3 && s_coll_fase_ok[next])
            next++;
        s_coll_current_fase = next;
    }

    refresh_collaudo_fase();
}

// ─────────────────────────────────────────────────────────
// UPDATE SIDE — chiamata dal BLE quando arriva BTN_DX/SX/CTR
// ─────────────────────────────────────────────────────────
void AppBanchetto::update_collaudo_side(const char *side_id)
{
    const uint32_t ACTIVE = COLL_CLR_GIALLO; // giallo quando attivo
    const uint32_t INACTIVE = 0x444444;

    bool sx_active = (strcmp(side_id, "SX") == 0);
    bool dx_active = (strcmp(side_id, "DX") == 0);

    if (s_coll_pill_sx)
        lv_obj_set_style_bg_color(s_coll_pill_sx,
                                  lv_color_hex(sx_active ? ACTIVE : INACTIVE), 0);
    if (s_coll_pill_dx)
        lv_obj_set_style_bg_color(s_coll_pill_dx,
                                  lv_color_hex(dx_active ? ACTIVE : INACTIVE), 0);
}

// ─────────────────────────────────────────────────────────
// UPDATE CONSUMO (chiamata dalla simulazione / UART)
// ─────────────────────────────────────────────────────────
void AppBanchetto::update_collaudo_consumo(float val)
{
    s_coll_live_consumo = val;
    if (!s_coll_lbl_consumo_ist || !s_coll_data_panel)
        return;
    if (lv_obj_has_flag(s_coll_data_panel, LV_OBJ_FLAG_HIDDEN))
        return;

    char buf_c[16];
    snprintf(buf_c, sizeof(buf_c), "%.3f", val);
    lv_label_set_text(s_coll_lbl_consumo_ist, buf_c);

    if (s_coll_current_fase > 2)
        return;
    collaudo_motore_t mot;
    if (!collaudo_manager_get_motore(&mot))
        return;
    float cmin, cmax, gmin, gmax;
    coll_get_limits(&mot, s_coll_current_fase, &cmin, &cmax, &gmin, &gmax);
    bool ok = (val >= cmin && val <= cmax);
    lv_obj_set_style_text_color(s_coll_lbl_consumo_ist,
                                lv_color_hex(ok ? COLL_CLR_VERDE : 0x666666), 0);
}

// ─────────────────────────────────────────────────────────
// UPDATE GIRI (chiamata dalla simulazione / GPIO IRQ)
// ─────────────────────────────────────────────────────────
void AppBanchetto::update_collaudo_giri(float val)
{
    s_coll_live_giri = val;
    if (!s_coll_lbl_giri_ist || !s_coll_data_panel)
        return;
    if (lv_obj_has_flag(s_coll_data_panel, LV_OBJ_FLAG_HIDDEN))
        return;

    char buf_g[16];
    snprintf(buf_g, sizeof(buf_g), "%.0f", val);
    lv_label_set_text(s_coll_lbl_giri_ist, buf_g);

    if (s_coll_current_fase > 2)
        return;
    collaudo_motore_t mot;
    if (!collaudo_manager_get_motore(&mot))
        return;
    float cmin, cmax, gmin, gmax;
    coll_get_limits(&mot, s_coll_current_fase, &cmin, &cmax, &gmin, &gmax);
    bool ok = (val >= gmin && val <= gmax);
    lv_obj_set_style_text_color(s_coll_lbl_giri_ist,
                                lv_color_hex(ok ? COLL_CLR_VERDE : 0x666666), 0);
}

// ─────────────────────────────────────────────────────────
// SIMULAZIONE WEB — chiamate dal web server (Core 1)
// Usano lv_async_call per aggiornare l'UI sul task LVGL
// ─────────────────────────────────────────────────────────
#ifdef COLLAUDI_BANCHETTO_ID

struct coll_sim_arg
{
    float consumo;
    float giri;
};

extern "C" void collaudo_sim_set_consumo(float val)
{
    coll_sim_arg *a = (coll_sim_arg *)malloc(sizeof(coll_sim_arg));
    if (!a)
        return;
    a->consumo = val;
    a->giri = -1.0f; // sentinella: non aggiornare giri
    lv_async_call([](void *arg)
                  {
        coll_sim_arg *aa = (coll_sim_arg *)arg;
        AppBanchetto::update_collaudo_consumo(aa->consumo);
        free(aa); }, a);
}

extern "C" void collaudo_sim_set_giri(float val)
{
    coll_sim_arg *a = (coll_sim_arg *)malloc(sizeof(coll_sim_arg));
    if (!a)
        return;
    a->consumo = -1.0f; // sentinella
    a->giri = val;
    lv_async_call([](void *arg)
                  {
        coll_sim_arg *aa = (coll_sim_arg *)arg;
        AppBanchetto::update_collaudo_giri(aa->giri);
        free(aa); }, a);
}

static float s_sim_benzina_val = 452.2f;

extern "C" void collaudo_sim_set_benzina(float val)
{
    s_sim_benzina_val = val;
}

extern "C" float collaudo_sim_get_benzina(void)
{
    return s_sim_benzina_val;
}

extern "C" void collaudo_app_refresh_page2(void)
{
    lv_async_call([](void *) { app_banchetto_update_page2(); }, nullptr);
}

#endif // COLLAUDI_BANCHETTO_ID

// ─────────────────────────────────────────────────────────
// CREA PAGE 5 — nuovo layout
// ─────────────────────────────────────────────────────────
void AppBanchetto::crea_page5_collaudo(uint8_t idx)
{
    const int W = 1024, H = 549;
    const int BTN_H = 92;
    const int HDR_H = 60;

    s_coll_current_fase = 0;
    memset(s_coll_fase_ok, 0, sizeof(s_coll_fase_ok));
    s_coll_live_consumo = 0.0f;
    s_coll_live_giri = 0.0f;

    lv_obj_t *scr = lv_obj_create(NULL);
    page5_scr = scr;
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLL_CLR_BG), 0);
    lv_obj_set_style_bg_opa(scr, 255, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ════════════════════════════════════════════════════
    // PANNELLO SCAN
    // ════════════════════════════════════════════════════
    s_coll_scan_panel = lv_obj_create(scr);
    lv_obj_set_size(s_coll_scan_panel, W, H - BTN_H);
    lv_obj_set_pos(s_coll_scan_panel, 0, 0);
    lv_obj_set_style_bg_color(s_coll_scan_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_coll_scan_panel, 210, 0);
    lv_obj_set_style_border_width(s_coll_scan_panel, 0, 0);
    lv_obj_set_style_pad_all(s_coll_scan_panel, 0, 0);
    lv_obj_clear_flag(s_coll_scan_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_coll_scan_panel, swipe_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_coll_scan_panel, swipe_event_cb, LV_EVENT_RELEASED, NULL);

    // ── popup stile popup_avviso centrato sull'overlay ──
    lv_obj_t *card = lv_obj_create(s_coll_scan_panel);
    lv_obj_set_size(card, 520, 300);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x141E30), 0);
    lv_obj_set_style_bg_opa(card, 255, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLL_CLR_ARANCIONE), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(card, 30, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, 200, 0);

    lv_obj_t *card_title_lbl = lv_label_create(card);
    lv_label_set_text(card_title_lbl, LV_SYMBOL_SETTINGS "  INSERISCI MATRICOLA");
    lv_obj_set_style_text_font(card_title_lbl, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(card_title_lbl, lv_color_hex(COLL_CLR_ARANCIONE), 0);
    lv_obj_set_width(card_title_lbl, LV_PCT(100));
    lv_obj_set_style_text_align(card_title_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(card_title_lbl, LV_ALIGN_TOP_MID, 0, 0);

    s_coll_scan_lbl = lv_label_create(card);
    lv_label_set_text(s_coll_scan_lbl, "Scansiona etichetta\nmotore");
    lv_obj_set_style_text_font(s_coll_scan_lbl, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(s_coll_scan_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(s_coll_scan_lbl, LV_PCT(100));
    lv_obj_set_style_text_align(s_coll_scan_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_coll_scan_lbl, LV_ALIGN_CENTER, 0, -10);

    // ════════════════════════════════════════════════════
    // PANNELLO DATI
    // ════════════════════════════════════════════════════
    s_coll_data_panel = lv_obj_create(scr);
    lv_obj_set_size(s_coll_data_panel, W, H - BTN_H);
    lv_obj_set_pos(s_coll_data_panel, 0, 0);
    lv_obj_set_style_bg_color(s_coll_data_panel, lv_color_hex(COLL_CLR_BG), 0);
    lv_obj_set_style_bg_opa(s_coll_data_panel, 255, 0);
    lv_obj_set_style_border_width(s_coll_data_panel, 0, 0);
    lv_obj_set_style_pad_all(s_coll_data_panel, 0, 0);
    lv_obj_clear_flag(s_coll_data_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_coll_data_panel, swipe_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_coll_data_panel, swipe_event_cb, LV_EVENT_RELEASED, NULL);

    // ── HEADER ──────────────────────────────────────────
    lv_obj_t *hdr = lv_obj_create(s_coll_data_panel);
    lv_obj_set_size(hdr, W, HDR_H);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(COLL_CLR_HEADER), 0);
    lv_obj_set_style_bg_opa(hdr, 255, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    // ── Lato SX (sinistra) ──────────────────────────────
    s_coll_pill_sx = lv_obj_create(hdr);
    lv_obj_set_size(s_coll_pill_sx, 16, 16);
    lv_obj_set_pos(s_coll_pill_sx, 10, (HDR_H - 16) / 2);
    lv_obj_set_style_bg_color(s_coll_pill_sx, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_opa(s_coll_pill_sx, 255, 0);
    lv_obj_set_style_border_width(s_coll_pill_sx, 0, 0);
    lv_obj_set_style_radius(s_coll_pill_sx, 8, 0);

    lv_obj_t *lbl_sx = lv_label_create(hdr);
    lv_label_set_text(lbl_sx, "LATO SX");
    lv_obj_set_style_text_font(lbl_sx, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_sx, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(lbl_sx, 32, (HDR_H - 20) / 2);

    // ── Info motore (centro, riga unica) ─────────────────
    s_coll_lbl_operatore = lv_label_create(hdr);
    lv_label_set_text(s_coll_lbl_operatore, "---   ---   ---");
    lv_obj_set_style_text_font(s_coll_lbl_operatore, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_coll_lbl_operatore, lv_color_hex(0xCCCCCC), 0);
    lv_label_set_long_mode(s_coll_lbl_operatore, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(s_coll_lbl_operatore, W - 280);
    lv_obj_set_style_text_align(s_coll_lbl_operatore, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_coll_lbl_operatore, LV_ALIGN_CENTER, 0, 0);

    // ── Lato DX (destra) ───────────────────────────────
    lv_obj_t *lbl_dx = lv_label_create(hdr);
    lv_label_set_text(lbl_dx, "LATO DX");
    lv_obj_set_style_text_font(lbl_dx, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_dx, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_dx, LV_ALIGN_RIGHT_MID, -32, 0);

    s_coll_pill_dx = lv_obj_create(hdr);
    lv_obj_set_size(s_coll_pill_dx, 16, 16);
    lv_obj_align(s_coll_pill_dx, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(s_coll_pill_dx, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_opa(s_coll_pill_dx, 255, 0);
    lv_obj_set_style_border_width(s_coll_pill_dx, 0, 0);
    lv_obj_set_style_radius(s_coll_pill_dx, 8, 0);

    // ── ETICHETTE SEZIONE ────────────────────────────────
    int y = HDR_H; // y=60

    lv_obj_t *lbl_c_tit = lv_label_create(s_coll_data_panel);
    lv_label_set_text(lbl_c_tit, "CONSUMO  [kg/h]");
    lv_obj_set_style_text_font(lbl_c_tit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_c_tit, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(lbl_c_tit, W / 4 - 70, y + 5);

    lv_obj_t *lbl_g_tit = lv_label_create(s_coll_data_panel);
    lv_label_set_text(lbl_g_tit, "GIRI  [rpm]");
    lv_obj_set_style_text_font(lbl_g_tit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_g_tit, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(lbl_g_tit, W * 3 / 4 - 50, y + 5);

    y += 28; // y=88

    // ── DISPLAY BOX CONSUMO ───────────────────────────────
    int BOX_W = W / 2 - 40;
    int BOX_H = 170;

    lv_obj_t *box_c = lv_obj_create(s_coll_data_panel);
    lv_obj_set_size(box_c, BOX_W, BOX_H);
    lv_obj_set_pos(box_c, 20, y);
    lv_obj_set_style_bg_color(box_c, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(box_c, 255, 0);
    lv_obj_set_style_border_color(box_c, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(box_c, 2, 0);
    lv_obj_set_style_radius(box_c, 6, 0);
    lv_obj_set_style_pad_all(box_c, 0, 0);
    lv_obj_clear_flag(box_c, LV_OBJ_FLAG_SCROLLABLE);
    s_coll_lbl_consumo_ist = lv_label_create(box_c);
    lv_label_set_text(s_coll_lbl_consumo_ist, "0.000");
    lv_obj_set_style_text_font(s_coll_lbl_consumo_ist, &ui_font_my_font, 0);
    lv_obj_set_style_text_color(s_coll_lbl_consumo_ist, lv_color_hex(0x666666), 0);
    lv_obj_align(s_coll_lbl_consumo_ist, LV_ALIGN_CENTER, 0, 0);

    // ── DISPLAY BOX GIRI ──────────────────────────────────
    lv_obj_t *box_g = lv_obj_create(s_coll_data_panel);
    lv_obj_set_size(box_g, BOX_W, BOX_H);
    lv_obj_set_pos(box_g, W / 2 + 20, y);
    lv_obj_set_style_bg_color(box_g, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(box_g, 255, 0);
    lv_obj_set_style_border_color(box_g, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(box_g, 2, 0);
    lv_obj_set_style_radius(box_g, 6, 0);
    lv_obj_set_style_pad_all(box_g, 0, 0);
    lv_obj_clear_flag(box_g, LV_OBJ_FLAG_SCROLLABLE);
    s_coll_lbl_giri_ist = lv_label_create(box_g);
    lv_label_set_text(s_coll_lbl_giri_ist, "0000");
    lv_obj_set_style_text_font(s_coll_lbl_giri_ist, &ui_font_my_font, 0);
    lv_obj_set_style_text_color(s_coll_lbl_giri_ist, lv_color_hex(0x666666), 0);
    lv_obj_align(s_coll_lbl_giri_ist, LV_ALIGN_CENTER, 0, 0);

    y += BOX_H + 8; // y=266

    // ── LIMITI FASE CORRENTE ─────────────────────────────────────────────────
    // Layout: "MIN" (font_14, grigio) | valore (font_22, chiaro) a sinistra del box
    //         "MAX" (font_14, grigio) | valore (font_22, chiaro) spostato a destra
    // Consumo box: x=20..492  |  Giri box: x=532..1004
    // Posizioni tag/val:
    //   consumo MIN tag=26  val=56   MAX tag=310  val=340
    //   giri    MIN tag=538 val=568  MAX tag=822  val=852
    const int LBL_Y = y + 10; // riga unica centrata nell'area 52px
    const int TAG_Y_OFF = 6;  // font_14 più basso per allineamento baseline

    // ── Consumo MIN ──
    s_coll_lbl_consumo_min = lv_label_create(s_coll_data_panel);
    lv_label_set_text(s_coll_lbl_consumo_min, "MIN");
    lv_obj_set_style_text_font(s_coll_lbl_consumo_min, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_coll_lbl_consumo_min, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(s_coll_lbl_consumo_min, 26, LBL_Y + TAG_Y_OFF);

    s_coll_lbl_consumo_min_val = lv_label_create(s_coll_data_panel);
    lv_label_set_text(s_coll_lbl_consumo_min_val, "---");
    lv_obj_set_style_text_font(s_coll_lbl_consumo_min_val, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(s_coll_lbl_consumo_min_val, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_pos(s_coll_lbl_consumo_min_val, 56, LBL_Y);

    // ── Consumo MAX (spostato a destra del box) ──
    s_coll_lbl_consumo_max = lv_label_create(s_coll_data_panel);
    lv_label_set_text(s_coll_lbl_consumo_max, "MAX");
    lv_obj_set_style_text_font(s_coll_lbl_consumo_max, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_coll_lbl_consumo_max, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(s_coll_lbl_consumo_max, 310, LBL_Y + TAG_Y_OFF);

    s_coll_lbl_consumo_max_val = lv_label_create(s_coll_data_panel);
    lv_label_set_text(s_coll_lbl_consumo_max_val, "---");
    lv_obj_set_style_text_font(s_coll_lbl_consumo_max_val, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(s_coll_lbl_consumo_max_val, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_pos(s_coll_lbl_consumo_max_val, 344, LBL_Y);

    // ── Giri MIN ──
    s_coll_lbl_giri_min = lv_label_create(s_coll_data_panel);
    lv_label_set_text(s_coll_lbl_giri_min, "MIN");
    lv_obj_set_style_text_font(s_coll_lbl_giri_min, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_coll_lbl_giri_min, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(s_coll_lbl_giri_min, 538, LBL_Y + TAG_Y_OFF);

    s_coll_lbl_giri_min_val = lv_label_create(s_coll_data_panel);
    lv_label_set_text(s_coll_lbl_giri_min_val, "---");
    lv_obj_set_style_text_font(s_coll_lbl_giri_min_val, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(s_coll_lbl_giri_min_val, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_pos(s_coll_lbl_giri_min_val, 568, LBL_Y);

    // ── Giri MAX (spostato a destra del box) ──
    s_coll_lbl_giri_max = lv_label_create(s_coll_data_panel);
    lv_label_set_text(s_coll_lbl_giri_max, "MAX");
    lv_obj_set_style_text_font(s_coll_lbl_giri_max, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_coll_lbl_giri_max, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(s_coll_lbl_giri_max, 822, LBL_Y + TAG_Y_OFF);

    s_coll_lbl_giri_max_val = lv_label_create(s_coll_data_panel);
    lv_label_set_text(s_coll_lbl_giri_max_val, "---");
    lv_obj_set_style_text_font(s_coll_lbl_giri_max_val, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(s_coll_lbl_giri_max_val, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_pos(s_coll_lbl_giri_max_val, 858, LBL_Y);

    y += 52; // y=318

    // ── SEPARATORE ───────────────────────────────────────
    lv_obj_t *sep = lv_obj_create(s_coll_data_panel);
    lv_obj_set_size(sep, W - 40, 1);
    lv_obj_set_pos(sep, 20, y);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_opa(sep, 255, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    y += 6; // y=316

    // ── FASE DOTS ─────────────────────────────────────────
    int dot_cy = y + (H - BTN_H - y) / 2;
    int dot_xs[3] = {W / 4, W / 2, W * 3 / 4};

    // Linea connettrice
    lv_obj_t *conn = lv_obj_create(s_coll_data_panel);
    lv_obj_set_size(conn, dot_xs[2] - dot_xs[0], 3);
    lv_obj_set_pos(conn, dot_xs[0], dot_cy - 1);
    lv_obj_set_style_bg_color(conn, lv_color_hex(0x555555), 0);
    lv_obj_set_style_bg_opa(conn, 255, 0);
    lv_obj_set_style_border_width(conn, 0, 0);
    lv_obj_set_style_radius(conn, 0, 0);

    for (int i = 0; i < 3; i++)
    {
        s_coll_dot[i] = lv_obj_create(s_coll_data_panel);
        lv_obj_set_size(s_coll_dot[i], 24, 24);
        lv_obj_set_pos(s_coll_dot[i], dot_xs[i] - 12, dot_cy - 12);
        lv_obj_set_style_radius(s_coll_dot[i], 12, 0);
        lv_obj_set_style_border_width(s_coll_dot[i], 3, 0);
        lv_obj_set_style_bg_opa(s_coll_dot[i], 255, 0);
        uint32_t bg = (i == 0) ? 0xFFFFFF : 0x555555;
        uint32_t bd = (i == 0) ? 0xFFFFFF : 0x333333;
        lv_obj_set_style_bg_color(s_coll_dot[i], lv_color_hex(bg), 0);
        lv_obj_set_style_border_color(s_coll_dot[i], lv_color_hex(bd), 0);

        lv_obj_t *fl = lv_label_create(s_coll_data_panel);
        lv_label_set_text(fl, COLL_FASE_NOMI[i]);
        lv_obj_set_style_text_font(fl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(fl, lv_color_hex(i == 0 ? 0xFFFFFF : 0x888888), 0);
        lv_obj_set_pos(fl, dot_xs[i] - 14, dot_cy + 16);
    }

    // Scatola counter vicino ai dot (a destra del terzo dot)
    s_coll_lbl_scatola = lv_label_create(s_coll_data_panel);
    lv_label_set_text(s_coll_lbl_scatola, "-/-");
    lv_obj_set_style_text_font(s_coll_lbl_scatola, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_coll_lbl_scatola, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_pos(s_coll_lbl_scatola, dot_xs[2] + 84, dot_cy - 20);

    // ── BUTTON BAR ───────────────────────────────────────
    s_coll_btn_bar = lv_obj_create(scr);
    lv_obj_set_size(s_coll_btn_bar, W, BTN_H);
    lv_obj_set_pos(s_coll_btn_bar, 0, H - BTN_H);
    lv_obj_set_style_bg_color(s_coll_btn_bar, lv_color_hex(COLL_CLR_HEADER), 0);
    lv_obj_set_style_bg_opa(s_coll_btn_bar, 255, 0);
    lv_obj_set_style_border_width(s_coll_btn_bar, 0, 0);
    lv_obj_set_style_pad_all(s_coll_btn_bar, 0, 0);
    lv_obj_clear_flag(s_coll_btn_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_coll_btn_bar, LV_OBJ_FLAG_HIDDEN);

    const char *btn_lbl_arr[4] = {"TOP", "MIN", "MAX", "FIN"};
    const uint32_t btn_col_arr[4] = {0xF9A825, 0x444444, 0x444444, 0x444444};
    const uint32_t btn_tc_arr[4] = {0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF};

    int btn_w = 220;
    int GAP = (W - 4 * btn_w) / 5;

    for (int i = 0; i < 4; i++)
    {
        s_coll_action_btn[i] = lv_btn_create(s_coll_btn_bar);
        lv_obj_set_size(s_coll_action_btn[i], btn_w, 80);
        lv_obj_set_pos(s_coll_action_btn[i], GAP + i * (btn_w + GAP), 6);
        lv_obj_set_style_bg_color(s_coll_action_btn[i], lv_color_hex(btn_col_arr[i]), 0);
        lv_obj_set_style_radius(s_coll_action_btn[i], 10, 0);
        lv_obj_set_style_shadow_width(s_coll_action_btn[i], 0, 0);
        lv_obj_t *bl = lv_label_create(s_coll_action_btn[i]);
        lv_label_set_text(bl, btn_lbl_arr[i]);
        lv_obj_set_style_text_font(bl, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(bl, lv_color_hex(btn_tc_arr[i]), 0);
        lv_obj_align(bl, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_event_cb(s_coll_action_btn[i], [](lv_event_t *e)
        {
            static uint32_t press_tick[4] = {};
            int bi = (int)(intptr_t)lv_event_get_user_data(e);
            lv_event_code_t code = lv_event_get_code(e);
            if (code == LV_EVENT_PRESSED) {
                press_tick[bi] = lv_tick_get();
            } else if (code == LV_EVENT_RELEASED) {
                uint32_t elapsed = lv_tick_elaps(press_tick[bi]);
                if (bi < 3 && elapsed >= 2500) {
                    // Long press: ripeti questa fase
                    AppBanchetto::coll_redo_fase(bi);
                } else {
                    // Pressione normale
                    AppBanchetto::coll_on_btn(bi);
                }
            }
        }, LV_EVENT_ALL, (void *)(intptr_t)i);
    }

    // scan_panel deve stare sopra data_panel (z-order)
    lv_obj_move_foreground(s_coll_scan_panel);

    ESP_LOGI(TAG, "Collaudo page5 v2 built for banchetto %s (idx %d)",
             COLLAUDI_BANCHETTO_ID, idx);
}

// ─────────────────────────────────────────────────────────
// UPDATE dopo motore OK
// ─────────────────────────────────────────────────────────
void AppBanchetto::update_collaudo_motore_ok(void)
{
    collaudo_motore_t mot = {};
    collaudo_operatore_t op = {};
    collaudo_manager_get_motore(&mot);
    collaudo_manager_get_operatore(&op);

    if (s_coll_scan_panel)
        lv_obj_add_flag(s_coll_scan_panel, LV_OBJ_FLAG_HIDDEN);
    if (s_coll_btn_bar)
        lv_obj_clear_flag(s_coll_btn_bar, LV_OBJ_FLAG_HIDDEN);

    s_coll_current_fase = 0;
    memset(s_coll_fase_ok, 0, sizeof(s_coll_fase_ok));
    s_coll_live_consumo = 0.0f;
    s_coll_live_giri = 0.0f;

    if (s_coll_lbl_operatore)
    {
        banchetto_data_t bd = {};
        const char *op_name = "---";
        if (banchetto_manager_get_data(&bd) && bd.matricola[0])
            op_name = bd.matricola;
        lv_label_set_text_fmt(s_coll_lbl_operatore, "OP: %s   %s   MAT: %s",
                              op_name, mot.descrizione, mot.matricola);
    }
    if (s_coll_lbl_consumo_ist)
    {
        lv_label_set_text(s_coll_lbl_consumo_ist, "0.000");
        lv_obj_set_style_text_color(s_coll_lbl_consumo_ist, lv_color_hex(0x666666), 0);
    }
    if (s_coll_lbl_giri_ist)
    {
        lv_label_set_text(s_coll_lbl_giri_ist, "0000");
        lv_obj_set_style_text_color(s_coll_lbl_giri_ist, lv_color_hex(0x666666), 0);
    }
    refresh_collaudo_fase();
}

void AppBanchetto::update_collaudo_badge_ok(void)
{
    // Non usato: il badge viene già gestito dal flusso banchetto su page1
}

// ─── Callbacks FreeRTOS → LVGL ───────────────────────────
extern "C" void collaudo_app_on_badge_ok(void)
{
    // Non usato in questo progetto (badge gestito su page1)
}

#ifndef TEST
static void rpm_timer_cb(lv_timer_t *)
{
    AppBanchetto::update_collaudo_giri((float)collaudo_manager_get_rpm());
}
#endif

extern "C" void collaudo_app_on_motore_ok(void)
{
#ifndef TEST
    collaudo_manager_rpm_start();
#endif
    collaudo_bilancia_start();

    if (lvgl_port_lock(pdMS_TO_TICKS(100))) {
        AppBanchetto::update_collaudo_motore_ok();
        AppBanchetto::update_collaudo_side("SX");
#ifndef TEST
        if (!s_rpm_timer)
            s_rpm_timer = lv_timer_create(rpm_timer_cb, 500, nullptr);
#endif
        lvgl_port_unlock();
    }
}

static void coll_side_async_cb(void *data)
{
    AppBanchetto::update_collaudo_side((const char *)data);
    free(data);
}

extern "C" void collaudo_app_on_side_change(const char *side_id)
{
    char *copy = strdup(side_id);
    if (copy)
        lv_async_call(coll_side_async_cb, copy);
}

void AppBanchetto::update_collaudo_error_ui(const char *msg)
{
    // Must be called with LVGL lock held
    popup_avviso_open(LV_SYMBOL_WARNING " Errore collaudo", msg, false);
    if (s_coll_scan_lbl)
        lv_label_set_text_fmt(s_coll_scan_lbl, LV_SYMBOL_WARNING " %s", msg);
}

extern "C" void collaudo_app_on_error(const char *msg)
{
    if (lvgl_port_lock(pdMS_TO_TICKS(100)))
    {
        AppBanchetto::update_collaudo_error_ui(msg);
        lvgl_port_unlock();
    }
}

// Chiamata dal timer di popup_ok (LVGL task) dopo i 5 secondi: ripristina scan overlay
static void collaudo_ok_dismissed(void)
{
    AppBanchetto::coll_reset_for_next();
}

void AppBanchetto::coll_reset_for_next(void)
{
    collaudo_manager_reset();
    s_coll_current_fase = 0;
    memset(s_coll_fase_ok, 0, sizeof(s_coll_fase_ok));
    s_coll_live_consumo = 0.0f;
    s_coll_live_giri = 0.0f;
    if (s_coll_scan_lbl)
        lv_label_set_text(s_coll_scan_lbl, "Scansiona etichetta\nmotore");
    if (s_coll_scan_panel)
        lv_obj_clear_flag(s_coll_scan_panel, LV_OBJ_FLAG_HIDDEN);
    if (s_coll_btn_bar)
        lv_obj_add_flag(s_coll_btn_bar, LV_OBJ_FLAG_HIDDEN);

    // Se scatola piena → torna a page2 per forzare scansione nuova scatola
    banchetto_data_t bd = {};
    if (banchetto_manager_get_item(s_coll_idx, &bd) &&
        bd.qta_totale_scatola > 0 &&
        bd.qta_scatola >= bd.qta_totale_scatola)
    {
        collaudo_manager_set_state(COLLAUDO_STATE_CHECKIN);
        lv_scr_load_anim(objects[s_coll_idx].main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
        current_scr = objects[s_coll_idx].main;
        lv_async_call([](void *) {
            popup_avviso_open(LV_SYMBOL_WARNING " Scatola piena",
                              "Scansionare la nuova scatola\nprima di continuare.", false);
        }, nullptr);
    }
}

void AppBanchetto::coll_redo_fase(int fase)
{
    if (fase < 0 || fase > 2) return;
    // Reset questa fase e tutte le successive
    for (int i = fase; i <= 2; i++) {
        s_coll_fase_ok[i]      = false;
        s_coll_fase_consumo[i] = 0.0f;
        s_coll_fase_giri[i]    = 0.0f;
    }
    s_coll_current_fase = (uint8_t)fase;
    refresh_collaudo_fase();
}

void AppBanchetto::check_page2_logout(void)
{
    uint8_t count = banchetto_manager_get_count();
    lv_obj_t *cur = lv_scr_act();
    for (uint8_t i = 0; i < count; i++)
    {
        if (cur != objects[i].main)
            continue;
        banchetto_data_t d = {};
        banchetto_manager_get_item(i, &d);
        if (!d.sessione_aperta && page1_scr[i])
        {
            lv_scr_load_anim(page1_scr[i], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
            current_scr = page1_scr[i];
        }
        break;
    }
}

void AppBanchetto::coll_check_logout(void)
{
    banchetto_data_t bd = {};
    banchetto_manager_get_data(&bd);
    if (!bd.sessione_aperta)
    {
        lv_obj_t *cur = lv_scr_act();
        if ((page5_scr && cur == page5_scr) || (page6_scr && cur == page6_scr))
        {
            coll_reset_for_next();
            lv_scr_load_anim(objects[s_coll_idx].main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
            current_scr = objects[s_coll_idx].main;
        }
    }
}

extern "C" void collaudo_app_on_save_ok(int idcollaudi, int esito_resp)
{
    ESP_LOGI("AppBanchetto", "Collaudo salvato OK, id=%d esito=%d", idcollaudi, esito_resp);
    if (lvgl_port_lock(pdMS_TO_TICKS(100)))
    {
        collaudo_motore_t mot = {};
        collaudo_manager_get_motore(&mot);
        char msg[64];
        char footer[16];
        if (esito_resp == 2)
            snprintf(msg, sizeof(msg), "ID collaudo: %d  " LV_SYMBOL_WARNING " Ri-collaudo", idcollaudi);
        else
            snprintf(msg, sizeof(msg), "ID collaudo: %d", idcollaudi);
        snprintf(footer, sizeof(footer), "%s  %s",
                 mot.codice_tipo[0] ? mot.codice_tipo : "---",
                 mot.matricola[0] ? mot.matricola : "---");
        myBeep();
        popup_ok_open(LV_SYMBOL_OK " Collaudo salvato", msg, footer, collaudo_ok_dismissed);
        lvgl_port_unlock();
    }
}

extern "C" void collaudo_app_on_save_error(const char *msg)
{
    myBeep();
    ESP_LOGE("AppBanchetto", "Errore salvataggio collaudo: %s", msg);
    if (lvgl_port_lock(pdMS_TO_TICKS(100)))
    {
        AppBanchetto::update_collaudo_error_ui(msg);
        lvgl_port_unlock();
    }
}

// ─────────────────────────────────────────────────────────
// CREA PAGE 6 — Storico collaudi (SD card)
// ─────────────────────────────────────────────────────────
void AppBanchetto::crea_page6_history(void)
{
    const int W = 1024, H = 549;
    const int SB = 48;   // altezza status bar Brookesia
    const int HDR_H = 44;
    const int ROW_H = 44;

    // Carica dati (al massimo 50 record)
    static collaudo_record_t recs[50];
    int n = collaudo_manager_read_history(recs, 50);

    // Distruggi eventuale pagina precedente
    if (page6_scr) {
        lv_obj_del(page6_scr);
        page6_scr = nullptr;
    }

    lv_obj_t *scr = lv_obj_create(NULL);
    page6_scr = scr;
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(scr, 255, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── Riga header ──────────────────────────────────────
    lv_obj_t *hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, W, HDR_H);
    lv_obj_set_pos(hdr, 0, SB);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_opa(hdr, 255, 0);
    lv_obj_set_style_border_width(hdr, 2, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(hdr, lv_color_hex(0x4A6D8C), 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(hdr, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Colonne: DATA/ORA | TIPO+MAT (es. "999 1234567") | OPERATORE | ESITO
    const int CX0 = 16, CX1 = 260, CX2 = 560, CX3 = 780;
    const lv_font_t *font_hdr = &lv_font_montserrat_20;
    const lv_font_t *font_row = &lv_font_montserrat_20;
    const int LBL_Y_HDR = (HDR_H - 20) / 2;
    const int LBL_Y_ROW = (ROW_H - 20) / 2;

    auto make_lbl = [&](lv_obj_t *parent, const char *text, int x, int y,
                        const lv_font_t *font, uint32_t color) {
        lv_obj_t *l = lv_label_create(parent);
        lv_label_set_text(l, text);
        lv_obj_set_style_text_font(l, font, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
        lv_obj_set_pos(l, x, y);
    };

    make_lbl(hdr, "DATA / ORA",  CX0, LBL_Y_HDR, font_hdr, 0xCCCCCC);
    make_lbl(hdr, "MATRICOLA",   CX1, LBL_Y_HDR, font_hdr, 0xCCCCCC);
    make_lbl(hdr, "OPERATORE",   CX2, LBL_Y_HDR, font_hdr, 0xCCCCCC);
    make_lbl(hdr, "ESITO",       CX3, LBL_Y_HDR, font_hdr, 0xCCCCCC);

    // ── Area scrollabile ─────────────────────────────────
    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_set_size(list, W, H - SB - HDR_H);
    lv_obj_set_pos(list, 0, SB + HDR_H);
    lv_obj_set_style_bg_opa(list, 0, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(list, 4, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x555555), LV_PART_SCROLLBAR);
    lv_obj_add_event_cb(list, swipe_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(list, swipe_event_cb, LV_EVENT_RELEASED, NULL);

    if (n == 0) {
        lv_obj_t *lbl = lv_label_create(list);
        lv_label_set_text(lbl, "Nessun collaudo registrato");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_26, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x555555), 0);
        lv_obj_center(lbl);
        return;
    }

    for (int i = 0; i < n; i++) {
        uint32_t bg = (i % 2 == 0) ? 0x222222 : 0x2A2A2A;

        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, W, ROW_H);
        lv_obj_set_pos(row, 0, i * ROW_H);
        lv_obj_set_style_bg_color(row, lv_color_hex(bg), 0);
        lv_obj_set_style_bg_opa(row, 255, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE);  // propaga al list

        // data_ora: mostra solo "YYYY-MM-DD HH:MM" (senza secondi) per brevità
        char data_ora_short[17] = {};
        snprintf(data_ora_short, sizeof(data_ora_short), "%s", recs[i].data_ora);

        char tipo_mat[13] = {};
        snprintf(tipo_mat, sizeof(tipo_mat), "%s %s",
                 recs[i].codice_tipo[0] ? recs[i].codice_tipo : "---",
                 recs[i].matricola[0]   ? recs[i].matricola   : "-------");

        uint32_t esito_color = recs[i].esito ? 0x00CC44 : 0xFF4444;
        const char *esito_str = recs[i].esito ? "OK" : "FAIL";

        make_lbl(row, data_ora_short,                             CX0, LBL_Y_ROW, font_row, 0xFFFFFF);
        make_lbl(row, tipo_mat,                                   CX1, LBL_Y_ROW, font_row, 0xFFFFFF);
        make_lbl(row, recs[i].operatore[0] ? recs[i].operatore : "---", CX2, LBL_Y_ROW, font_row, 0xFFFFFF);
        make_lbl(row, esito_str,                                  CX3, LBL_Y_ROW, font_row, esito_color);
    }

}

// ─────────────────────────────────────────────────────────
// SWIPE CALLBACK
// ─────────────────────────────────────────────────────────
void AppBanchetto::swipe_event_cb(lv_event_t *e)
{
    static lv_point_t start = {0, 0};
    static bool pressing = false;

    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    if (code == LV_EVENT_PRESSING)
    {
        if (!pressing)
        {
            start = pt;
            pressing = true;
            ESP_LOGI("SWIPE", "DOWN x=%d y=%d", (int)pt.x, (int)pt.y);
        }
        return;
    }

    if (code != LV_EVENT_RELEASED || !pressing)
        return;
    pressing = false;

    int32_t dx = pt.x - start.x;
    int32_t dy = pt.y - start.y;
    int32_t start_x = start.x;
    int32_t start_y = start.y;
    start = {0, 0};
    ESP_LOGI("SWIPE", "UP x=%d y=%d  dx=%d dy=%d", (int)pt.x, (int)pt.y, (int)dx, (int)dy);

    uint8_t idx = banchetto_manager_get_current_index();
    uint8_t count = banchetto_manager_get_count();

    // ── SWIPE ORIZZONTALE — cambia pagina ─────────────────
    if (abs(dx) > abs(dy) && abs(dx) > SWIPE_H_THRESHOLD)
    {
        lv_obj_t *cur = lv_scr_act();
        bool on_page1 = (cur == page1_scr[idx]);
        bool on_page2 = (cur == objects[idx].main);
        bool on_page3 = (page3_scr && cur == page3_scr);
        bool on_page4 = (page4_scr && cur == page4_scr);
        bool on_page5 = (page5_scr && cur == page5_scr);
        bool on_page6 = (page6_scr && cur == page6_scr);

        // Su page5/page6 (collaudi) lo swipe è libero su tutta l'altezza
        // Su tutte le altre pagine solo nella zona header (codice articolo)
        if (!on_page5 && !on_page6)
        {
            if (start_y < 16 || start_y > 160)
                return;
        }

        if (dx < 0)
        {
            // ── Swipe SINISTRA ──
            if (on_page1)
            {
                // page1 → page2
                banchetto_data_t d;
                banchetto_manager_get_data(&d);
                if (!d.sessione_aperta)
                {
                    popup_avviso_open(LV_SYMBOL_WARNING " Timbratura mancante",
                                      "Effettuare il login con\nil badge prima di continuare.",
                                      !wifi_is_connected());
                    return;
                }
                lv_scr_load_anim(objects[idx].main, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
                current_scr = objects[idx].main;
            }
            else if (on_page2 && page3_scr)
            {
                // page2 → page3 (solo banchetto 233)
                banchetto_data_t d;
                banchetto_manager_get_item(idx, &d);
                if (strcmp(d.banchetto, TAGL_BANCHETTO_ID) == 0)
                {
                    update_page3(idx);
                    lv_scr_load_anim(page3_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
                    current_scr = page3_scr;
                }
            }
            else if (on_page2 && page5_scr)
            {
                // page2 → page5 (solo banchetto 222)
                banchetto_data_t d;
                banchetto_manager_get_item(idx, &d);
                if (strcmp(d.banchetto, COLLAUDI_BANCHETTO_ID) == 0)
                {
                    if (!d.sessione_aperta)
                    {
                        popup_avviso_open(LV_SYMBOL_WARNING " Timbratura mancante",
                                          "Effettuare il login con\nil badge prima di continuare.",
                                          !wifi_is_connected());
                        return;
                    }
                    collaudo_manager_set_state(COLLAUDO_STATE_SCAN_MOTORE);
                    lv_scr_load_anim(page5_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
                    current_scr = page5_scr;
                }
            }
            else if (on_page3 && page4_scr)
            {
                // page3 → page4
                lv_scr_load_anim(page4_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
                current_scr = page4_scr;
            }
            else if (on_page5)
            {
                // page5 → page6 (storico collaudi)
                crea_page6_history();
                if (page6_scr) {
                    lv_obj_add_event_cb(page6_scr, swipe_event_cb, LV_EVENT_PRESSING, NULL);
                    lv_obj_add_event_cb(page6_scr, swipe_event_cb, LV_EVENT_RELEASED, NULL);
                    lv_scr_load_anim(page6_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
                    current_scr = page6_scr;
                }
            }
        }
        else
        {
            // ── Swipe DESTRA ──
            if (on_page2)
            {
                // page2 → page1
                lv_scr_load_anim(page1_scr[idx], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
                current_scr = page1_scr[idx];
            }
            else if (on_page3)
            {
                // page3 → page2 (banchetto 233)
                lv_scr_load_anim(objects[s_tagl_idx].main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
                current_scr = objects[s_tagl_idx].main;
            }
            else if (on_page4)
            {
                // page4 → page3
                lv_scr_load_anim(page3_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
                current_scr = page3_scr;
            }
            else if (on_page5)
            {
                // page5 → page2 (banchetto 222): disattiva intercettazione scanner
                collaudo_manager_set_state(COLLAUDO_STATE_CHECKIN);
                lv_scr_load_anim(objects[s_coll_idx].main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
                current_scr = objects[s_coll_idx].main;
            }
            else if (on_page6)
            {
                // page6 → page5
                lv_scr_load_anim(page5_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
                current_scr = page5_scr;
            }
        }
        return;
    }

    // ── SWIPE VERTICALE — cambia articolo (stessa pagina) ─
    if (abs(dy) > abs(dx) && abs(dy) > SWIPE_V_THRESHOLD)
    {
        if (start_x > 260)
            return; // zona swipe: solo sidebar sinistra
        if (count <= 1)
            return; // un solo articolo, niente da fare
        // Su pag5/pag6 (collaudo) lo swipe verticale è riservato allo scroll interno
        {
            lv_obj_t *cur2 = lv_scr_act();
            if ((page5_scr && cur2 == page5_scr) || (page6_scr && cur2 == page6_scr))
                return;
        }

        bool on_page1 = (lv_scr_act() == page1_scr[idx]);
        int8_t new_idx = (int8_t)idx;

        if (dy < 0)
        {
            // swipe su → articolo successivo
            new_idx++;
            if (new_idx >= (int8_t)count)
                new_idx = 0; // wrap
        }
        else
        {
            // swipe giù → articolo precedente
            new_idx--;
            if (new_idx < 0)
                new_idx = (int8_t)(count - 1); // wrap
        }

        banchetto_manager_set_current_index((uint8_t)new_idx);

        lv_obj_t *dest = on_page1 ? page1_scr[new_idx] : objects[new_idx].main;
        // lv_scr_load_anim_t anim = (dy < 0) ? LV_SCR_LOAD_ANIM_MOVE_TOP : LV_SCR_LOAD_ANIM_MOVE_BOTTOM;
        // Nuovo
        lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_FADE_IN;
        lv_scr_load_anim(dest, anim, 300, 0, false);
        current_scr = dest;

        ESP_LOGI(TAG, "Articolo %d → %d (%s)", idx, new_idx, on_page1 ? "page1" : "page2");
    }
}

// ─────────────────────────────────────────────────────────
// LONG PRESS sul numero banchetto → re-fetch server + refresh
// ─────────────────────────────────────────────────────────
static lv_timer_t *s_banc_lp_timer = nullptr;

static void force_refresh_task(void *)
{
    banchetto_manager_fetch_from_server();
    lv_async_call([](void *) {
        uint8_t n = banchetto_manager_get_count();
        for (uint8_t i = 0; i < n; i++) {
            AppBanchetto::update_page1(i);
            AppBanchetto::update_page2(i);
        }
        AppBanchetto::update_page5_collaudo();
    }, nullptr);
    vTaskDelete(nullptr);
}

static void banc_lp_timer_cb(lv_timer_t *)
{
    s_banc_lp_timer = nullptr;
    xTaskCreate(force_refresh_task, "force_ref", 4096, nullptr, 5, nullptr);
}

static void banc_lp_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        if (!s_banc_lp_timer) {
            s_banc_lp_timer = lv_timer_create(banc_lp_timer_cb, 3000, nullptr);
            lv_timer_set_repeat_count(s_banc_lp_timer, 1);
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (s_banc_lp_timer) {
            lv_timer_del(s_banc_lp_timer);
            s_banc_lp_timer = nullptr;
        }
    }
}

// ─────────────────────────────────────────────────────────
// CREA PAGE 1
// ─────────────────────────────────────────────────────────
void AppBanchetto::crea_page1(uint8_t idx)
{
    uint8_t count = banchetto_manager_get_count();

    lv_obj_t *scr = lv_obj_create(NULL);
    page1_scr[idx] = scr;
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(scr, 255, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(scr, swipe_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(scr, swipe_event_cb, LV_EVENT_RELEASED, NULL);

    // ── SIDEBAR NERA ─────────────────────────────────────────
    lv_obj_t *sidebar = lv_obj_create(scr);
    lv_obj_set_pos(sidebar, 0, 0);
    lv_obj_set_size(sidebar, 260, 549);
    lv_obj_set_style_bg_color(sidebar, lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_bg_opa(sidebar, 255, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);
    lv_obj_set_style_radius(sidebar, 0, 0);
    lv_obj_set_style_pad_all(sidebar, 24, 0);
    lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sidebar, LV_OBJ_FLAG_EVENT_BUBBLE);
    add_versa_switch(sidebar);

    lv_obj_t *lbl_op_tit = lv_label_create(sidebar);
    lv_label_set_text(lbl_op_tit, "OPERATORE");
    lv_obj_set_style_text_font(lbl_op_tit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_op_tit, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_letter_space(lbl_op_tit, 4, 0);
    lv_obj_align(lbl_op_tit, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_matricola[idx] = lv_label_create(sidebar);
    lv_label_set_text(lbl_matricola[idx], "");
    lv_obj_set_style_text_font(lbl_matricola[idx], &ui_font_my_font75, 0);
    lv_obj_set_style_text_color(lbl_matricola[idx], lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_matricola[idx], LV_ALIGN_TOP_LEFT, 0, 28);

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
    lv_obj_add_flag(pill, LV_OBJ_FLAG_EVENT_BUBBLE);

    lbl_sessione_stato[idx] = lv_label_create(pill);
    lv_label_set_text(lbl_sessione_stato[idx], "NON LOGGATO");
    lv_obj_set_style_text_font(lbl_sessione_stato[idx], &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_sessione_stato[idx], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_letter_space(lbl_sessione_stato[idx], 2, 0);
    lv_obj_align(lbl_sessione_stato[idx], LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *sep = lv_obj_create(sidebar);
    lv_obj_set_pos(sep, 0, 340);
    lv_obj_set_size(sep, 212, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(sep, 255, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_add_flag(sep, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *lbl_banc_tit = lv_label_create(sidebar);
    lv_label_set_text(lbl_banc_tit, "BANCHETTO");
    lv_obj_set_style_text_font(lbl_banc_tit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_banc_tit, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_letter_space(lbl_banc_tit, 3, 0);
    lv_obj_align(lbl_banc_tit, LV_ALIGN_TOP_LEFT, 0, 358);

    lbl_banc[idx] = lv_label_create(sidebar);
    lv_label_set_text(lbl_banc[idx], "");
    lv_obj_set_style_text_font(lbl_banc[idx], &ui_font_my_font75, 0);
    lv_obj_set_style_text_color(lbl_banc[idx], lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_banc[idx], LV_ALIGN_TOP_LEFT, 0, 382);
    lv_obj_add_flag(lbl_banc[idx], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(lbl_banc[idx], LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_add_event_cb(lbl_banc[idx], banc_lp_event_cb, LV_EVENT_ALL, nullptr);

    // ── CODICE ARTICOLO — hero giallo ─────────────────────────
    lv_obj_t *box_codice = lv_obj_create(scr);
    lv_obj_set_pos(box_codice, 276, 16);
    lv_obj_set_size(box_codice, 732, 120);
    lv_obj_set_style_bg_color(box_codice, lv_color_hex(0xFFDD00), 0);
    lv_obj_set_style_bg_opa(box_codice, 255, 0);
    lv_obj_set_style_border_width(box_codice, 0, 0);
    lv_obj_set_style_radius(box_codice, 12, 0);
    lv_obj_set_style_pad_left(box_codice, 20, 0);
    lv_obj_set_style_pad_top(box_codice, 12, 0);
    lv_obj_clear_flag(box_codice, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box_codice, LV_OBJ_FLAG_EVENT_BUBBLE);
    {
        lv_obj_t *t = lv_label_create(box_codice);
        lv_label_set_text(t, "CODICE ARTICOLO");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000066), 0);
        lv_obj_set_style_text_letter_space(t, 4, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        lbl_codice[idx] = lv_label_create(box_codice);
        lv_label_set_text(lbl_codice[idx], "---");
        lv_obj_set_style_text_font(lbl_codice[idx], &ui_font_my_font75, 0);
        lv_obj_set_style_text_color(lbl_codice[idx], lv_color_hex(0x000000), 0);
        lv_obj_align(lbl_codice[idx], LV_ALIGN_TOP_LEFT, 0, 26);
    }
    // ── INDICATORE ARTICOLO N/tot ─────────────────────────────
    lv_obj_t *lbl_idx = lv_label_create(scr);
    lv_label_set_text_fmt(lbl_idx, "%d/%d", idx + 1, count);
    lv_obj_set_style_text_font(lbl_idx, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_idx, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl_idx, LV_ALIGN_TOP_RIGHT, -36, 28);

    // ── CICLO — blu ───────────────────────────────────────────
    lv_obj_t *box_ciclo = lv_obj_create(scr);
    lv_obj_set_pos(box_ciclo, 276, 152);
    lv_obj_set_size(box_ciclo, 200, 110);
    lv_obj_set_style_bg_color(box_ciclo, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_bg_opa(box_ciclo, 255, 0);
    lv_obj_set_style_border_width(box_ciclo, 0, 0);
    lv_obj_set_style_radius(box_ciclo, 12, 0);
    lv_obj_set_style_pad_all(box_ciclo, 14, 0);
    lv_obj_clear_flag(box_ciclo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box_ciclo, LV_OBJ_FLAG_EVENT_BUBBLE);
    {
        lv_obj_t *t = lv_label_create(box_ciclo);
        lv_label_set_text(t, "CICLO");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF99), 0);
        lv_obj_set_style_text_letter_space(t, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        lbl_ciclo[idx] = lv_label_create(box_ciclo);
        lv_label_set_text(lbl_ciclo[idx], "---");
        lv_obj_set_style_text_font(lbl_ciclo[idx], &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(lbl_ciclo[idx], lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(lbl_ciclo[idx], LV_ALIGN_TOP_LEFT, 0, 28);
    }

    // ── ORD PRODUZIONE — bianco ───────────────────────────────
    lv_obj_t *box_odp = lv_obj_create(scr);
    lv_obj_set_pos(box_odp, 492, 152);
    lv_obj_set_size(box_odp, 360, 110);
    lv_obj_set_style_bg_color(box_odp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(box_odp, 255, 0);
    lv_obj_set_style_border_width(box_odp, 0, 0);
    lv_obj_set_style_radius(box_odp, 12, 0);
    lv_obj_set_style_pad_all(box_odp, 14, 0);
    lv_obj_clear_flag(box_odp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box_odp, LV_OBJ_FLAG_EVENT_BUBBLE);
    {
        lv_obj_t *t = lv_label_create(box_odp);
        lv_label_set_text(t, "ORD PRODUZIONE");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000066), 0);
        lv_obj_set_style_text_letter_space(t, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        lbl_odp[idx] = lv_label_create(box_odp);
        lv_label_set_text(lbl_odp[idx], "");
        lv_obj_set_style_text_font(lbl_odp[idx], &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(lbl_odp[idx], lv_color_hex(0x000000), 0);
        lv_obj_align(lbl_odp[idx], LV_ALIGN_TOP_LEFT, 0, 28);
    }

    // ── FASE — rosso ──────────────────────────────────────────
    lv_obj_t *box_fase = lv_obj_create(scr);
    lv_obj_set_pos(box_fase, 868, 152);
    lv_obj_set_size(box_fase, 140, 110);
    lv_obj_set_style_bg_color(box_fase, lv_color_hex(0xE11D48), 0);
    lv_obj_set_style_bg_opa(box_fase, 255, 0);
    lv_obj_set_style_border_width(box_fase, 0, 0);
    lv_obj_set_style_radius(box_fase, 12, 0);
    lv_obj_set_style_pad_all(box_fase, 14, 0);
    lv_obj_clear_flag(box_fase, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box_fase, LV_OBJ_FLAG_EVENT_BUBBLE);
    {
        lv_obj_t *t = lv_label_create(box_fase);
        lv_label_set_text(t, "FASE");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0xFFFFFF99), 0);
        lv_obj_set_style_text_letter_space(t, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        lbl_fase[idx] = lv_label_create(box_fase);
        lv_label_set_text(lbl_fase[idx], "---");
        lv_obj_set_style_text_font(lbl_fase[idx], &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(lbl_fase[idx], lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(lbl_fase[idx], LV_ALIGN_TOP_LEFT, 0, 28);
    }

    // ── DESCRIZIONE ───────────────────────────────────────────
    lv_obj_t *box_descr = lv_obj_create(scr);
    lv_obj_set_pos(box_descr, 276, 278);
    lv_obj_set_size(box_descr, 732, 130);
    lv_obj_set_style_bg_color(box_descr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(box_descr, 255, 0);
    lv_obj_set_style_border_width(box_descr, 0, 0);
    lv_obj_set_style_radius(box_descr, 12, 0);
    lv_obj_set_style_pad_all(box_descr, 16, 0);
    lv_obj_clear_flag(box_descr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box_descr, LV_OBJ_FLAG_EVENT_BUBBLE);
    {
        lv_obj_t *t = lv_label_create(box_descr);
        lv_label_set_text(t, "DESCRIZIONE ARICOLO");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000044), 0);
        lv_obj_set_style_text_letter_space(t, 3, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        lbl_descr[idx] = lv_label_create(box_descr);
        lv_label_set_text(lbl_descr[idx], "---");
        lv_obj_set_style_text_font(lbl_descr[idx], &lv_font_montserrat_30, 0);
        lv_obj_set_style_text_color(lbl_descr[idx], lv_color_hex(0x000000), 0);
        lv_obj_set_width(lbl_descr[idx], 700);
        lv_label_set_long_mode(lbl_descr[idx], LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(lbl_descr[idx], LV_ALIGN_TOP_LEFT, 0, 28);
    }

    // ── BOTTONE CONTROLLO QUALITA' ────────────────────────────
    lv_obj_t *btn_ctrl = lv_btn_create(scr);
    lv_obj_set_pos(btn_ctrl, 276, 424);
    lv_obj_set_size(btn_ctrl, 732, 110);
    lv_obj_set_style_bg_color(btn_ctrl, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_ctrl, lv_color_hex(0x222222), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_ctrl, 12, 0);
    lv_obj_set_style_border_width(btn_ctrl, 0, 0);
    lv_obj_set_style_shadow_width(btn_ctrl, 0, 0);
    lv_obj_add_event_cb(btn_ctrl, [](lv_event_t *e)
                        { popup_controllo_open(); }, LV_EVENT_CLICKED, NULL);
    {
        lv_obj_t *ico = lv_label_create(btn_ctrl);
        lv_label_set_text(ico, LV_SYMBOL_SETTINGS);
        lv_obj_set_style_text_font(ico, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(ico, lv_color_hex(0xFFDD00), 0);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 40, 0);

        lv_obj_t *lbl = lv_label_create(btn_ctrl);
        lv_label_set_text(lbl, "CONTROLLO QUALITA'");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_letter_space(lbl, 4, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t *arrow = lv_label_create(btn_ctrl);
        lv_label_set_text(arrow, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(arrow, lv_color_hex(0xFFDD00), 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -40, 0);
    }

    // EVENT_BUBBLE su tutti i figli diretti
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(scr); i++)
        lv_obj_add_flag(lv_obj_get_child(scr, i), LV_OBJ_FLAG_EVENT_BUBBLE);
}

// ─────────────────────────────────────────────────────────
// UPDATE PAGE 1
// ─────────────────────────────────────────────────────────
void AppBanchetto::update_page1(uint8_t idx)
{
    if (!page1_scr[idx])
        return;

    banchetto_data_t d;
    if (!banchetto_manager_get_item(idx, &d))
        return;

    if (lbl_ciclo[idx])
        lv_label_set_text(lbl_ciclo[idx], d.ciclo);
    if (lbl_codice[idx])
        lv_label_set_text(lbl_codice[idx], d.codice_articolo);
    if (lbl_odp[idx])
        lv_label_set_text_fmt(lbl_odp[idx], "%lu", d.ord_prod);
    if (lbl_fase[idx])
        lv_label_set_text(lbl_fase[idx], d.fase);
    if (lbl_descr[idx])
        lv_label_set_text(lbl_descr[idx], d.descrizione_articolo);
    if (lbl_banc[idx])
        lv_label_set_text(lbl_banc[idx], d.banchetto);

    if (d.sessione_aperta)
    {
        if (lbl_matricola[idx])
            lv_label_set_text(lbl_matricola[idx], d.matricola);
        if (lbl_sessione_stato[idx])
        {
            lv_label_set_text(lbl_sessione_stato[idx], "LOGGATO " LV_SYMBOL_OK);
            lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_sessione_stato[idx]),
                                      lv_color_hex(0x16A34A), 0);
        }
    }
    else
    {
        if (lbl_matricola[idx])
            lv_label_set_text(lbl_matricola[idx], "0000");
        if (lbl_sessione_stato[idx])
        {
            lv_label_set_text(lbl_sessione_stato[idx], "NON LOGGATO");
            lv_obj_set_style_bg_color(lv_obj_get_parent(lbl_sessione_stato[idx]),
                                      lv_color_hex(0xE11D48), 0);
        }
    }
}

// ─────────────────────────────────────────────────────────
// UPDATE PAGE 2
// ─────────────────────────────────────────────────────────
void AppBanchetto::update_page2(uint8_t idx)
{
    if (!objects[idx].main)
        return;

    banchetto_data_t d;
    if (!banchetto_manager_get_item(idx, &d))
        return;

    // Sidebar
    if (d.sessione_aperta)
    {
        if (objects[idx].obj17)
            lv_label_set_text(objects[idx].obj17, d.matricola);
        if (objects[idx].obj19)
            lv_label_set_text(objects[idx].obj19, "LOGGATO " LV_SYMBOL_OK);
        if (objects[idx].obj18)
            lv_obj_set_style_bg_color(objects[idx].obj18, lv_color_hex(0x16A34A), 0);
    }
    else
    {
        if (objects[idx].obj17)
            lv_label_set_text(objects[idx].obj17, "0000");
        if (objects[idx].obj19)
            lv_label_set_text(objects[idx].obj19, "NON LOGGATO");
        if (objects[idx].obj18)
            lv_obj_set_style_bg_color(objects[idx].obj18, lv_color_hex(0xE11D48), 0);
    }

    if (objects[idx].obj13)
        lv_label_set_text(objects[idx].obj13, d.banchetto);

    // Indicatore posizione
    if (objects[idx].obj14)
        lv_label_set_text_fmt(objects[idx].obj14, "%d/%d", idx + 1, banchetto_manager_get_count());

    // Arc avanzamento fase
    if (objects[idx].obj5 && d.qta_totale > 0)
    {
        lv_arc_set_range(objects[idx].obj5, 0, (int16_t)d.qta_totale);
        lv_arc_set_value(objects[idx].obj5, (int16_t)d.qta_prod_fase);
    }
    if (objects[idx].obj7)
    {
        lv_label_set_text_fmt(objects[idx].obj7, "%lu", d.qta_prod_fase);
        lv_obj_align_to(objects[idx].obj7, objects[idx].obj5, LV_ALIGN_CENTER, 0, -12);
    }
    if (objects[idx].obj6)
        lv_label_set_text_fmt(objects[idx].obj6, "/ %lu", d.qta_totale);
    if (objects[idx].obj9)
        lv_label_set_text_fmt(objects[idx].obj9, "%lu", d.qta_prod_sessione);
    if (objects[idx].obj12)
        lv_label_set_text_fmt(objects[idx].obj12, "%lu/%lu",
                              d.qta_scatola, d.qta_totale_scatola);
    if (objects[idx].obj16)
        lv_label_set_text(objects[idx].obj16,
                          d.matr_scatola_corrente[0] ? d.matr_scatola_corrente : "---");

    ESP_LOGI(TAG, "Page2[%d] aggiornata: prod_fase=%lu qta_totale=%lu", idx, d.qta_prod_fase, d.qta_totale);
}

// ─────────────────────────────────────────────────────────
// HELPER
// ─────────────────────────────────────────────────────────
static void check_ordine_e_avvisa(void)
{
    banchetto_data_t d;
    if (banchetto_manager_get_data(&d) && d.ord_prod == 0)
        popup_avviso_open(LV_SYMBOL_WARNING " Nessun ordine",
                          "Nessun ordine attivo.\nTornare alla schermata principale\ne avviare un nuovo ordine.",
                          !wifi_is_connected());
}

// ─────────────────────────────────────────────────────────
// RUN
// ─────────────────────────────────────────────────────────
bool AppBanchetto::run(void)
{
    ESP_LOGI(TAG, "Run app");

    uint8_t count = banchetto_manager_get_count();
    if (count == 0)
        count = 1;

    // Costruisce e popola tutte le coppie di schermate
    create_screens(); // costruisce objects[0..count-1].main

    for (uint8_t i = 0; i < count; i++)
    {
        crea_page1(i);
        update_page1(i);
        update_page2(i);

        // Collega swipe a page2
        lv_obj_add_event_cb(objects[i].main, swipe_event_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(objects[i].main, swipe_event_cb, LV_EVENT_RELEASED, NULL);

        // Collega bottone scarti
        if (objects[i].obj0)
            lv_obj_add_event_cb(objects[i].obj0, [](lv_event_t *e)
                                { tastiera_scarti_open(); }, LV_EVENT_CLICKED, NULL);
    }

    // Costruisce page3/page4 se esiste il banchetto 233
    for (uint8_t i = 0; i < count; i++)
    {
        banchetto_data_t d;
        banchetto_manager_get_item(i, &d);
        if (strcmp(d.banchetto, TAGL_BANCHETTO_ID) == 0)
        {
            s_tagl_idx = i;
            crea_page3(i);
            crea_page4(i);
            tagliatubi_manager_set_callback(banchetto_tagl_state_cb);
            // Collega swipe a page3/page4
            lv_obj_add_event_cb(page3_scr, swipe_event_cb, LV_EVENT_PRESSING, NULL);
            lv_obj_add_event_cb(page3_scr, swipe_event_cb, LV_EVENT_RELEASED, NULL);
            lv_obj_add_event_cb(page4_scr, swipe_event_cb, LV_EVENT_PRESSING, NULL);
            lv_obj_add_event_cb(page4_scr, swipe_event_cb, LV_EVENT_RELEASED, NULL);
            ESP_LOGI(TAG, "Tagliatubi pages built for banchetto %s (idx %d)", TAGL_BANCHETTO_ID, i);
            break;
        }
    }

    // Costruisce page5 se esiste il banchetto 222
    for (uint8_t i = 0; i < count; i++)
    {
        banchetto_data_t d;
        banchetto_manager_get_item(i, &d);
        if (strcmp(d.banchetto, COLLAUDI_BANCHETTO_ID) == 0)
        {
            s_coll_idx = i;
            crea_page5_collaudo(i);
            lv_obj_add_event_cb(page5_scr, swipe_event_cb, LV_EVENT_PRESSING, NULL);
            lv_obj_add_event_cb(page5_scr, swipe_event_cb, LV_EVENT_RELEASED, NULL);
            ESP_LOGI(TAG, "Collaudo page built for banchetto %s (idx %d)", COLLAUDI_BANCHETTO_ID, i);
            break;
        }
    }

    // Parte sempre da page1 articolo 0
    banchetto_manager_set_current_index(0);
    lv_disp_load_scr(page1_scr[0]);
    current_scr = page1_scr[0];

    // ── BANNER OFFLINE fisso su lv_layer_top() ────────────────
    offline_banner = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(offline_banner, 820, 360);
    lv_obj_set_size(offline_banner, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(offline_banner, lv_color_hex(0xFF6600), 0);
    lv_obj_set_style_bg_opa(offline_banner, 230, 0);
    lv_obj_set_style_border_width(offline_banner, 0, 0);
    lv_obj_set_style_radius(offline_banner, 6, 0);
    lv_obj_set_style_pad_top(offline_banner, 6, 0);
    lv_obj_set_style_pad_bottom(offline_banner, 6, 0);
    lv_obj_set_style_pad_left(offline_banner, 14, 0);
    lv_obj_set_style_pad_right(offline_banner, 14, 0);
    lv_obj_clear_flag(offline_banner, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    {
        lv_obj_t *lbl = lv_label_create(offline_banner);
        lv_label_set_text(lbl, LV_SYMBOL_WARNING " OFFLINE");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_26, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl);
    }
    // stato iniziale
    if (wifi_is_connected())
        lv_obj_add_flag(offline_banner, LV_OBJ_FLAG_HIDDEN);

    offline_timer = lv_timer_create(offline_timer_cb, 2000, NULL);

    check_ordine_e_avvisa();
    ESP_LOGI(TAG, "App loaded — %d articoli, showing page1[0]", count);
    return true;
}

// ─────────────────────────────────────────────────────────
// RESUME
// ─────────────────────────────────────────────────────────
bool AppBanchetto::resume(void)
{
    ESP_LOGI(TAG, "Resume app");
    uint8_t count = banchetto_manager_get_count();
    for (uint8_t i = 0; i < count; i++)
    {
        update_page1(i);
        update_page2(i);
    }
    check_ordine_e_avvisa();
    return true;
}

// ─────────────────────────────────────────────────────────
// BACK
// ─────────────────────────────────────────────────────────
bool AppBanchetto::back(void)
{
    uint8_t idx = banchetto_manager_get_current_index();

    if (lv_scr_act() == objects[idx].main)
    {
        lv_scr_load_anim(page1_scr[idx], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
        current_scr = page1_scr[idx];
        return true;
    }

    // Su page1 → chiudi app, reset puntatori
    for (uint8_t i = 0; i < BANCHETTO_MAX_ITEMS; i++)
    {
        page1_scr[i] = nullptr;
        lbl_matricola[i] = lbl_ciclo[i] = lbl_codice[i] = nullptr;
        lbl_descr[i] = lbl_odp[i] = lbl_fase[i] = nullptr;
        lbl_sessione_stato[i] = lbl_banc[i] = nullptr;
    }
    page3_scr = page4_scr = nullptr;
    p3_lbl_codice = p3_lbl_descr = p3_lbl_lunghezza = nullptr;
    p3_lbl_quantita = p3_lbl_velocita = p3_lbl_pill = p3_pill = p3_lbl_op_val = nullptr;
    p4_lbl_counter = p4_lbl_stato = p4_lbl_avanzamento = nullptr;
    s_tagl_idx = 255;
    // Reset collaudo
#ifndef TEST
    if (s_rpm_timer) { lv_timer_del(s_rpm_timer); s_rpm_timer = nullptr; }
    collaudo_manager_rpm_stop();
#endif
    page5_scr = nullptr;
    s_coll_scan_panel = s_coll_scan_lbl = s_coll_data_panel = nullptr;
    s_coll_pill_sx = s_coll_pill_dx = nullptr;
    s_coll_lbl_operatore = s_coll_lbl_matricola = nullptr;
    s_coll_lbl_consumo_ist = s_coll_lbl_giri_ist = nullptr;
    s_coll_lbl_consumo_min = s_coll_lbl_consumo_max = nullptr;
    s_coll_lbl_giri_min = s_coll_lbl_giri_max = nullptr;
    for (int i = 0; i < 3; i++)
        s_coll_dot[i] = nullptr;
    s_coll_lbl_scatola = nullptr;
    s_coll_btn_bar = nullptr;
    for (int i = 0; i < 4; i++)
        s_coll_action_btn[i] = nullptr;
    s_coll_current_fase = 0;
    memset(s_coll_fase_ok, 0, sizeof(s_coll_fase_ok));
    s_coll_live_consumo = 0.0f;
    s_coll_live_giri = 0.0f;
    s_coll_idx = 255;
    notifyCoreClosed();
    return true;
}

// ─────────────────────────────────────────────────────────
// CLOSE
// ─────────────────────────────────────────────────────────
bool AppBanchetto::close(void)
{
    ESP_LOGI(TAG, "Close app");
    if (offline_timer)
    {
        lv_timer_del(offline_timer);
        offline_timer = nullptr;
    }
    if (offline_banner)
    {
        lv_obj_del(offline_banner);
        offline_banner = nullptr;
    }
    for (uint8_t i = 0; i < BANCHETTO_MAX_ITEMS; i++)
    {
        page1_scr[i] = nullptr;
        lbl_matricola[i] = lbl_ciclo[i] = lbl_codice[i] = nullptr;
        lbl_descr[i] = lbl_odp[i] = lbl_fase[i] = nullptr;
        lbl_sessione_stato[i] = lbl_banc[i] = nullptr;
    }
    page3_scr = page4_scr = nullptr;
    p3_lbl_codice = p3_lbl_descr = p3_lbl_lunghezza = nullptr;
    p3_lbl_quantita = p3_lbl_velocita = p3_lbl_pill = p3_pill = p3_lbl_op_val = nullptr;
    p4_lbl_counter = p4_lbl_stato = p4_lbl_avanzamento = nullptr;
    s_tagl_idx = 255;
    // Reset collaudo
#ifndef TEST
    if (s_rpm_timer) { lv_timer_del(s_rpm_timer); s_rpm_timer = nullptr; }
    collaudo_manager_rpm_stop();
#endif
    page5_scr = nullptr;
    s_coll_scan_panel = s_coll_scan_lbl = s_coll_data_panel = nullptr;
    s_coll_pill_sx = s_coll_pill_dx = nullptr;
    s_coll_lbl_operatore = s_coll_lbl_matricola = nullptr;
    s_coll_lbl_consumo_ist = s_coll_lbl_giri_ist = nullptr;
    s_coll_lbl_consumo_min = s_coll_lbl_consumo_max = nullptr;
    s_coll_lbl_giri_min = s_coll_lbl_giri_max = nullptr;
    for (int i = 0; i < 3; i++)
        s_coll_dot[i] = nullptr;
    s_coll_lbl_scatola = nullptr;
    s_coll_btn_bar = nullptr;
    for (int i = 0; i < 4; i++)
        s_coll_action_btn[i] = nullptr;
    s_coll_current_fase = 0;
    memset(s_coll_fase_ok, 0, sizeof(s_coll_fase_ok));
    s_coll_live_consumo = 0.0f;
    s_coll_live_giri = 0.0f;
    s_coll_idx = 255;
    tagliatubi_manager_set_callback(nullptr);
    return true;
}