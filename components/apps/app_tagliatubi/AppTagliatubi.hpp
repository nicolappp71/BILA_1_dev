#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"

extern "C" {
#include "tagliatubi_manager.h"
}

class AppTagliatubi : public ESP_Brookesia_PhoneApp
{
public:
    AppTagliatubi(const char *operatore = "\xe2\x80\x94", const char *macchina = "1");
    ~AppTagliatubi();

    bool init(void) override;
    bool run(void) override;
    bool back(void) override;
    bool close(void) override;

    // Called from the tagliatubi_manager cycle task via lv_async_call
    static void on_state_update(void *user_data);

    // Called after codice is entered via numpad
    void cerca_prodotto(void);

private:
    // ── Screen setup ────────────────────────────────────────────────────────
    void build_tab_impostazioni(lv_obj_t *parent);
    void build_tab_ciclo(lv_obj_t *parent);

    void show_keyboard(lv_obj_t *ta);
    void hide_keyboard(void);

    // ── UI update (called on LVGL task) ─────────────────────────────────────
    void refresh_ciclo_tab(tagliatubi_state_t state, const tagliatubi_data_t *data);
    static const char *state_label(tagliatubi_state_t s);
    static lv_color_t  state_color(tagliatubi_state_t s);

    // ── Event callbacks ─────────────────────────────────────────────────────
    static void cb_salva(lv_event_t *e);
    static void cb_ciclo(lv_event_t *e);
    static void cb_singolo(lv_event_t *e);
    static void cb_avanti(lv_event_t *e);
    static void cb_taglio(lv_event_t *e);
    static void cb_stop(lv_event_t *e);
    static void cb_ta_focused(lv_event_t *e);
    static void cb_keyboard_ready(lv_event_t *e);

    // ── Widgets — impostazioni tab ───────────────────────────────────────────
    lv_obj_t *ta_codice     = nullptr;
    lv_obj_t *lbl_descr     = nullptr;
    lv_obj_t *lbl_pill_stato = nullptr;
    lv_obj_t *ta_lunghezza  = nullptr;
    lv_obj_t *ta_quantita   = nullptr;
    lv_obj_t *ta_velocita   = nullptr;

    // ── Widgets — ciclo tab ──────────────────────────────────────────────────
    lv_obj_t *lbl_counter   = nullptr;   // "23 / 100 pz"
    lv_obj_t *lbl_stato     = nullptr;
    lv_obj_t *lbl_info      = nullptr;   // lunghezza / velocità
    lv_obj_t *btn_stop      = nullptr;

    // ── Shared ───────────────────────────────────────────────────────────────
    lv_obj_t *keyboard      = nullptr;
    lv_obj_t *tabview       = nullptr;
    int        _w           = 1024;   // visual area width
    int        _th          = 515;    // tab content height

    // ── Config from constructor ───────────────────────────────────────────────
    char _nome_operatore[32];
    char _nome_macchina[32];
};
