#ifndef TAGLIATUBI_MANAGER_H
#define TAGLIATUBI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ─── Safety Debug ────────────────────────────────────────────────────────────
// 1 = debug mode: bypass all safety checks (hardware non necessario)
// 0 = produzione: hardware reale richiesto
#define TAGL_SAFETY_DEBUG 0

// ─── GPIO Pin Definitions ────────────────────────────────────────────────────
#define TAGL_STEP_GPIO 46
#define TAGL_DIR_GPIO 47
#define TAGL_EN_GPIO 48
#define TAGL_ENC_A_GPIO 49
#define TAGL_ENC_B_GPIO 50
#define TAGL_VALVOLA_A_GPIO 51
#define TAGL_VALVOLA_C_GPIO 52
#define TAGL_SICUREZZA_GPIO 2       // Uomo morto: NO, pull-up, premuto = LOW
#define TAGL_MICRO_CARTER_GPIO 3    // Carter: NC, pull-up, chiuso = HIGH
#define TAGL_MICRO_MATERIALE 4      // Materiale: leva NC/NO, pull-up, presente = HIGH

// ─── Motor Constants ─────────────────────────────────────────────────────────
#define TAGL_STEPS_PER_REV 6400 // steps per motor revolution (32x microstepping)
#define TAGL_LOOP_MULTIPLIER 5  // riduzione meccanica: nstep * 5 (cambia qui se cambia il riduttore)
#define TAGL_PI 3.14f

// ─── Encoder Constants ───────────────────────────────────────────────────────
#define TAGL_ENC_PPR 600             // pulses per revolution (600P/R encoder)
#define TAGL_ENC_WHEEL_CIRC_MM 40.9f // PATCH: ricalibrato su misura reale (nominale 40.6mm, da correggere dopo fix segnali encoder)
#define TAGL_ENC_COUNTS_PER_REV 2400 // PCNT quadrature: 4 * 600

// ─── Cutting Timing ──────────────────────────────────────────────────────────
#define TAGL_TEMPO_TAGLIO_MS 500 // pause between valve operations (ms)
#define TAGL_AVANTI_STEPS 1000   // passi per comando AVANTI manuale

    // ─── State ───────────────────────────────────────────────────────────────────
    typedef enum
    {
        TAGL_STATE_IDLE = 0,
        TAGL_STATE_RUNNING,
        TAGL_STATE_CUTTING,
        TAGL_STATE_DONE,
        TAGL_STATE_ERROR_NO_MATERIAL,
        TAGL_STATE_ERROR_SAFETY,
        TAGL_STATE_ERROR_LENGTH,
        TAGL_STATE_BOX_FULL,
        TAGL_STATE_ERROR_UOMO_MORTO,  // Pulsante sicurezza non premuto all'avvio
    } tagliatubi_state_t;

    // ─── Product Data ────────────────────────────────────────────────────────────
    typedef struct
    {
        int id;
        char codice[32];
        char descrizione[64];
        int32_t lunghezza; // mm target
        int32_t diametro;  // mm
        int32_t quantita;
        int32_t prodotti;
        int velocita;               // 1-99
        int32_t lunghezza_misurata; // mm rilevati dall'encoder nell'ultimo ciclo
    } tagliatubi_data_t;

    // ─── UI Callback ─────────────────────────────────────────────────────────────
    // Called from the cycle task whenever state or prodotti changes.
    // Must NOT call LVGL directly — use lv_async_call() or bsp_display_lock().
    typedef void (*tagliatubi_state_cb_t)(tagliatubi_state_t state,
                                          const tagliatubi_data_t *data);

    // ─── Public API ──────────────────────────────────────────────────────────────

    // Call once at startup (after wifi_init_sta)
    esp_err_t tagliatubi_manager_init(void);

    // Register UI update callback
    esp_err_t tagliatubi_manager_set_callback(tagliatubi_state_cb_t cb);

    // Fetch product from server by product code (com=5)
    esp_err_t tagliatubi_manager_load_product(const char *codice);

    // Send single parameters to server
    esp_err_t tagliatubi_manager_send_lunghezza(void);
    esp_err_t tagliatubi_manager_send_quantita(void);
    esp_err_t tagliatubi_manager_send_velocita(void);

    // Update local data (call before send_*)
    void tagliatubi_manager_set_lunghezza(int32_t mm);
    void tagliatubi_manager_set_quantita(int32_t q);
    void tagliatubi_manager_set_velocita(int v);

    // Cycle control (must be IDLE to start)
    // Precondizione start: uomo morto premuto + carter chiuso + materiale presente
    esp_err_t tagliatubi_manager_start_ciclo(void); // full automatic cycle
    esp_err_t tagliatubi_manager_singolo(void);     // one advance + cut
    esp_err_t tagliatubi_manager_avanti(void);      // advance only, no cut
    esp_err_t tagliatubi_manager_taglio(void);      // cut only, no advance
    esp_err_t tagliatubi_manager_stop(void);        // abort (sempre attivo, no precondizioni)

    // Safety: stato pin di sicurezza (usabile da UI per indicatori)
    bool tagliatubi_manager_is_uomo_morto(void);  // GPIO2 NO premuto = true
    bool tagliatubi_manager_is_carter(void);       // GPIO3 NC chiuso = true
    bool tagliatubi_manager_is_materiale(void);    // GPIO4 leva materiale presente = true

    // Getters
    tagliatubi_state_t tagliatubi_manager_get_state(void);
    const tagliatubi_data_t *tagliatubi_manager_get_data(void);
    bool tagliatubi_manager_is_motor_busy(void);  // true quando il ciclo è attivo (sopprime audio)

#ifdef __cplusplus
}
#endif

#endif // TAGLIATUBI_MANAGER_H
