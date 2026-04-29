#include "Logged.hpp"
#include "esp_log.h"
#include "esp_lvgl_port.h"

extern "C"
{
#include "banchetto_manager.h"
}

extern "C" {
    extern const lv_img_dsc_t scanner;
}
extern "C" void app_banchetto_update_page1(void);
extern "C" void app_banchetto_update_page2(void);
static const char *TAG = "AppAssegnaBanchetto";

// ─── Singleton ────────────────────────────────────────────
Logged *Logged::instance = nullptr;

// ─── Bridge C ─────────────────────────────────────────────
extern "C" void app_assegna_banchetto_close(void)
{
    if (Logged::instance)
        Logged::instance->chiudi();
}

// ─────────────────────────────────────────────────────────
// COSTRUTTORE / DISTRUTTORE
// ─────────────────────────────────────────────────────────
Logged::Logged() : ESP_Brookesia_PhoneApp("Associa Banchetto", &scanner, true)
{
}

Logged::~Logged()
{
}

bool Logged::init(void)
{
    return true;
}

// ─────────────────────────────────────────────────────────
// CHIUDI — wrapper pubblico per notifyCoreClosed()
// ─────────────────────────────────────────────────────────
void Logged::chiudi(void)
{
    banchetto_manager_set_state(BANCHETTO_STATE_CHECKIN);
    if (overlay) { lv_obj_del(overlay); overlay = nullptr; }
    if (popup)   { lv_obj_del(popup);   popup   = nullptr; }
    instance = nullptr;

    esp_err_t ret = banchetto_manager_fetch_from_server();
    if (ret != ESP_OK)
        banchetto_manager_reset_data(); // ← azzera i dati se fetch fallisce

    if (lvgl_port_lock(pdMS_TO_TICKS(100)))
    {
        app_banchetto_update_page1();
        app_banchetto_update_page2();
        lvgl_port_unlock();
    }

    notifyCoreClosed();
}

// ─────────────────────────────────────────────────────────
// RUN
// ─────────────────────────────────────────────────────────
bool Logged::run(void)
{
    instance = this;

    ESP_LOGI(TAG, "Run — imposto stato ASSEGNA_BANCHETTO");
    banchetto_manager_set_state(BANCHETTO_STATE_ASSEGNA_BANCHETTO);

    lv_obj_t *scr = lv_scr_act();

    overlay = lv_obj_create(scr);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, 180, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    popup = lv_obj_create(scr);
    lv_obj_set_size(popup, 520, 260);
    lv_obj_center(popup);
    lv_obj_move_foreground(popup);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0x141E30), 0);
    lv_obj_set_style_bg_opa(popup, 255, 0);
    lv_obj_set_style_border_color(popup, lv_color_hex(0x4A90E2), 0);
    lv_obj_set_style_border_width(popup, 2, 0);
    lv_obj_set_style_radius(popup, 12, 0);
    lv_obj_set_style_pad_all(popup, 16, 0);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(popup, 30, 0);
    lv_obj_set_style_shadow_color(popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(popup, 200, 0);

    lv_obj_t *tit = lv_label_create(popup);
    lv_label_set_text(tit, "ASSOCIA BANCHETTO");
    lv_obj_set_style_text_font(tit, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(tit, lv_color_hex(0x4A90E2), 0);
    lv_obj_set_width(tit, LV_PCT(100));
    lv_obj_set_style_text_align(tit, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(tit, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *msg = lv_label_create(popup);
    lv_label_set_text(msg, "Scansiona il barcode del banchetto");
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(msg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(msg, LV_PCT(100));
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *btn = lv_btn_create(popup);
    lv_obj_set_size(btn, 160, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFA0000), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, annulla_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Annulla");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(btn_lbl);

    return true;
}

// ─────────────────────────────────────────────────────────
// ANNULLA CALLBACK
// ─────────────────────────────────────────────────────────
void Logged::annulla_cb(lv_event_t *e)
{
    Logged *app = (Logged *)lv_event_get_user_data(e);
    if (!app) return;
    app->chiudi();
}

// ─────────────────────────────────────────────────────────
// BACK / CLOSE
// ─────────────────────────────────────────────────────────
bool Logged::back(void)
{
    chiudi();
    return true;
}

bool Logged::close(void)
{
    banchetto_manager_set_state(BANCHETTO_STATE_CHECKIN);
    if (overlay) { lv_obj_del(overlay); overlay = nullptr; }
    if (popup)   { lv_obj_del(popup);   popup   = nullptr; }
    instance = nullptr;
    return true;
}