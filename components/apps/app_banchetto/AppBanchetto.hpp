#pragma once
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "json_parser.h"  // BANCHETTO_MAX_ITEMS
#include "mode.h"

extern "C" {
#include "tagliatubi_manager.h"
#include "collaudo_manager.h"
}

// Codice banchetto che ha le pagine tagliatubi extra
#define TAGL_BANCHETTO_ID     "233"//"233"

// Codice banchetto che ha le pagine collaudo extra (definito in mode.h)
// #define COLLAUDI_BANCHETTO_ID "222"  // <-- spostato in mode.h

class AppBanchetto : public ESP_Brookesia_PhoneApp
{
public:
    AppBanchetto();
    ~AppBanchetto();
    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool init(void) override;
    bool resume(void) override;

    static void update_page1(uint8_t idx);
    static void update_page2(uint8_t idx);
    static void update_page3(uint8_t idx);
    static void update_page4_scatola(void);
    static void update_page5_collaudo(void);
    static void add_versa_switch(lv_obj_t *sidebar, int y_offset = 220);
    static void update_collaudo_badge_ok(void);
    static void update_collaudo_motore_ok(void);
    static void coll_reset_for_next(void);
    static void coll_redo_fase(int fase);
    static void coll_check_logout(void);
    static void check_page2_logout(void);
    static void reset_box_full(void);
    static void update_collaudo_side(const char *side_id);
    static void update_collaudo_consumo(float val);
    static void update_collaudo_giri(float val);
    static void update_collaudo_error_ui(const char *msg);

    // Tagliatubi state callback (via lv_async_call)
    static void on_tagl_state_update(void *user_data);

    // Accessibili dal timer callback (file scope)
    static lv_obj_t   *p4_pill_uomo_morto;
    static lv_obj_t   *p4_lbl_uomo_morto;
    static lv_obj_t   *p4_pill_materiale;
    static lv_obj_t   *p4_lbl_materiale;
    static lv_obj_t   *p4_pill_carter;
    static lv_obj_t   *p4_lbl_carter;
    static lv_timer_t *p4_uomo_morto_timer;
    static uint8_t     s_tagl_idx;
    static uint8_t     s_coll_idx;

private:
    // ── Page 1 ──────────────────────────────────────────────────────────────
    static lv_obj_t *page1_scr[BANCHETTO_MAX_ITEMS];
    static lv_obj_t *lbl_matricola[BANCHETTO_MAX_ITEMS];
    static lv_obj_t *lbl_ciclo[BANCHETTO_MAX_ITEMS];
    static lv_obj_t *lbl_codice[BANCHETTO_MAX_ITEMS];
    static lv_obj_t *lbl_descr[BANCHETTO_MAX_ITEMS];
    static lv_obj_t *lbl_odp[BANCHETTO_MAX_ITEMS];
    static lv_obj_t *lbl_fase[BANCHETTO_MAX_ITEMS];
    static lv_obj_t *lbl_sessione_stato[BANCHETTO_MAX_ITEMS];
    static lv_obj_t *lbl_banc[BANCHETTO_MAX_ITEMS];

    // ── Page 3 — Impostazioni tagliatubi (solo banchetto 233) ───────────────
    static lv_obj_t *page3_scr;
    static lv_obj_t *p3_lbl_codice;
    static lv_obj_t *p3_lbl_descr;
    static lv_obj_t *p3_lbl_lunghezza;
    static lv_obj_t *p3_lbl_quantita;
    static lv_obj_t *p3_lbl_velocita;
    static lv_obj_t *p3_lbl_pill;
    static lv_obj_t *p3_pill;
    static lv_obj_t *p3_lbl_op_val;

    // ── Page 4 — Ciclo tagliatubi (solo banchetto 233) ─────────────────────
    static lv_obj_t   *page4_scr;
    static lv_obj_t   *p4_lbl_counter;
    static lv_obj_t   *p4_lbl_stato;
    static lv_obj_t   *p4_lbl_avanzamento;

    // ── Page 5 — Collaudo (solo banchetto 222) ─────────────────────────────
    static lv_obj_t *page5_scr;
    // ── Page 6 — Storico collaudi (SD card) ────────────────────────────────
    static lv_obj_t *page6_scr;
    static lv_obj_t *s_coll_scan_panel;
    static lv_obj_t *s_coll_scan_lbl;
    static lv_obj_t *s_coll_data_panel;
    // Header
    static lv_obj_t *s_coll_pill_sx;
    static lv_obj_t *s_coll_pill_dx;
    static lv_obj_t *s_coll_lbl_operatore;
    static lv_obj_t *s_coll_lbl_matricola;
    // Live values
    static lv_obj_t *s_coll_lbl_consumo_ist;
    static lv_obj_t *s_coll_lbl_giri_ist;
    static lv_obj_t *s_coll_lbl_consumo_min;      // etichetta "MIN" (testo fisso)
    static lv_obj_t *s_coll_lbl_consumo_min_val;  // valore numerico MIN consumo
    static lv_obj_t *s_coll_lbl_consumo_max;      // etichetta "MAX" (testo fisso)
    static lv_obj_t *s_coll_lbl_consumo_max_val;  // valore numerico MAX consumo
    static lv_obj_t *s_coll_lbl_giri_min;         // etichetta "MIN" (testo fisso)
    static lv_obj_t *s_coll_lbl_giri_min_val;     // valore numerico MIN giri
    static lv_obj_t *s_coll_lbl_giri_max;         // etichetta "MAX" (testo fisso)
    static lv_obj_t *s_coll_lbl_giri_max_val;     // valore numerico MAX giri
    // Phase progress dots
    static lv_obj_t *s_coll_dot[3];
    static lv_obj_t *s_coll_lbl_scatola;   // "qta/tot" scatola vicino ai dot
    // Button bar
    static lv_obj_t *s_coll_btn_bar;
    static lv_obj_t *s_coll_action_btn[4];
    // State
    static uint8_t   s_coll_current_fase;
    static bool      s_coll_fase_ok[3];
    static float     s_coll_live_consumo;
    static float     s_coll_live_giri;
    static float     s_coll_fase_consumo[3];  // valore confermato per fase
    static float     s_coll_fase_giri[3];

    // ── Shared ───────────────────────────────────────────────────────────────
    static lv_obj_t *current_scr;
    static lv_obj_t *offline_banner;
    static lv_timer_t *offline_timer;

    // ── Methods ──────────────────────────────────────────────────────────────
    static void crea_page1(uint8_t idx);
    static void crea_page3(uint8_t idx);
    static void crea_page4(uint8_t idx);
    static void refresh_page4(tagliatubi_state_t state, const tagliatubi_data_t *data);
    static void crea_page5_collaudo(uint8_t idx);
    static void crea_page6_history(void);
    static void refresh_collaudo_fase(void);
    static void coll_on_btn(int btn_idx);
    static void swipe_event_cb(lv_event_t *e);
    static void offline_timer_cb(lv_timer_t *t);

    lv_obj_t *container;
    lv_obj_t *test_button;
};
