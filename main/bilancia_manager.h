#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ── Parametri fisici serbatoio (grammi) ───────────────────
#define BILANCIA_MAX_G          452.2f   // serbatoio pieno
#define BILANCIA_DISPLAY_MIN_G  200.0f   // soglia minima barra
#define BILANCIA_REFILL_G       330.0f   // sotto questa soglia → SAVA auto

void bilancia_manager_init(void);
void bilancia_manager_start_poll(void);         // avvia polling consumo (AIMW ogni 1s)
void bilancia_manager_stop_poll(void);          // ferma polling
void bilancia_manager_check_level(void);        // legge livello, aggiorna benzina, eventuale SAVA (BLOCCANTE)
void bilancia_manager_check_level_async(void);  // versione non bloccante — sicura da chiamare da task LVGL

#ifdef __cplusplus
}
#endif
