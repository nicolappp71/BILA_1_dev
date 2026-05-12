#ifndef SPALMATRICE_MANAGER_H
#define SPALMATRICE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "path_chain.h"
#include "dxf_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─── GPIO — TODO: assegnare i pin reali ──────────────────────────────────────
#define SPAL_EN_X_GPIO    0   // TODO
#define SPAL_DIR_X_GPIO   0   // TODO
#define SPAL_PUL_X_GPIO   0   // TODO
#define SPAL_EN_Y_GPIO    0   // TODO
#define SPAL_DIR_Y_GPIO   0   // TODO
#define SPAL_PUL_Y_GPIO   0   // TODO
#define SPAL_HOME_X_GPIO  0   // TODO  (NC, premuto = LOW)
#define SPAL_HOME_Y_GPIO  0   // TODO  (NC, premuto = LOW)
#define SPAL_RELAY_GPIO   0   // TODO  (HIGH = pompa ON)

// ─── Parametri macchina — TODO: calibrare ────────────────────────────────────
#define SPAL_STEPS_PER_MM_X  80.0f   // TODO: steps per mm asse X
#define SPAL_STEPS_PER_MM_Y  80.0f   // TODO: steps per mm asse Y
#define SPAL_SPEED_STEPS_S   1000    // TODO: velocità in steps/sec
#define SPAL_ACCEL_STEPS_S2  2000    // TODO: accelerazione in steps/sec²
#define SPAL_HOMING_SPEED    400     // steps/sec durante homing

// ─── Layer DXF da estrarre ───────────────────────────────────────────────────
#define SPAL_DXF_LAYER       "pasta"

// ─── State machine ───────────────────────────────────────────────────────────
typedef enum {
    SPAL_STATE_IDLE = 0,
    SPAL_STATE_DXF_LOADED,      // DXF parsato, pronto
    SPAL_STATE_HOMING,          // homing in corso
    SPAL_STATE_READY,           // homing done, pronto a partire
    SPAL_STATE_RUNNING,         // percorso in esecuzione
    SPAL_STATE_PAUSED,
    SPAL_STATE_DONE,
    SPAL_STATE_ERROR,
} spalmatrice_state_t;

// ─── API pubblica ─────────────────────────────────────────────────────────────

void spalmatrice_manager_init(void);

// Scarica DXF da URL (XAMPP), parsea e prepara il percorso. Logga le coordinate.
esp_err_t spalmatrice_manager_fetch_and_parse(const char *url);

// Carica DXF da buffer già in memoria.
esp_err_t spalmatrice_manager_load_dxf(const char *buf, size_t len);

// Legge il file DXF direttamente dalla SD card (/sdcard/<filename>).
esp_err_t spalmatrice_manager_load_dxf_from_sd(const char *filename);

// Homing: muove verso home X e Y fino ai finecorsa, azzera posizione.
esp_err_t spalmatrice_manager_home(void);

// Avvia esecuzione percorso (richiede stato READY).
esp_err_t spalmatrice_manager_start(void);

// Pausa / riprendi
esp_err_t spalmatrice_manager_pause(void);
esp_err_t spalmatrice_manager_resume(void);

// Stop immediato
void spalmatrice_manager_stop(void);

// Pompa
void spalmatrice_manager_pump_on(void);
void spalmatrice_manager_pump_off(void);

// Stato e info
spalmatrice_state_t spalmatrice_manager_get_state(void);
int spalmatrice_manager_get_point_count(void);   // punti totali del percorso
int spalmatrice_manager_get_point_current(void); // punto corrente in esecuzione

// Accesso al percorso per il rendering UI (puntatore valido finché non viene
// caricato un nuovo DXF). *out_path può essere NULL se non ancora caricato.
void spalmatrice_manager_get_path(const path_point_t **out_path, int *out_npts);
void spalmatrice_manager_get_segments(const dxf_segment_t **out_segs, int *out_nsegs);

// Modifica il punto path_chain all'indice idx → (nx,ny).
// Aggiorna anche i segmenti raw corrispondenti e s_path.
esp_err_t spalmatrice_manager_edit_path_point(int idx, float nx, float ny);

// Salva i segmenti correnti su SD: /sdcard/spal_percorso.seg (formato binario).
esp_err_t spalmatrice_manager_save_to_sd(void);

// Carica segmenti da SD (sovrascrive quelli attuali). Ritorna ESP_OK se trovato.
esp_err_t spalmatrice_manager_load_from_sd(void);

#ifdef __cplusplus
}
#endif

#endif // SPALMATRICE_MANAGER_H
