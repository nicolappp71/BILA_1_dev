#ifndef COLLAUDO_MANAGER_H
#define COLLAUDO_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════
// STATE MACHINE
// ═══════════════════════════════════════════════════════════
typedef enum {
    COLLAUDO_STATE_CHECKIN,       // attesa badge operatore
    COLLAUDO_STATE_SCAN_MOTORE,   // operatore ok, attesa barcode motore
    COLLAUDO_STATE_IN_CORSO,      // parametri caricati, collaudo attivo
} collaudo_state_t;

// ═══════════════════════════════════════════════════════════
// STRUTTURA DATI MOTORE
// ═══════════════════════════════════════════════════════════
typedef struct {
    char  codice_tipo[4];         // 3 cifre tipo (es. "999")
    char  matricola[8];           // 7 cifre matricola (es. "1234567")
    char  descrizione[64];        // DescrizioneModello dal DB

    float carico_consumo_min;
    float carico_consumo_max;
    float carico_giri_min;
    float carico_giri_max;

    float minimo_consumo_min;
    float minimo_consumo_max;
    float minimo_giri_min;
    float minimo_giri_max;

    float top_consumo_min;
    float top_consumo_max;
    float top_giri_min;
    float top_giri_max;
} collaudo_motore_t;

// ═══════════════════════════════════════════════════════════
// STRUTTURA DATI OPERATORE
// ═══════════════════════════════════════════════════════════
typedef struct {
    char badge[16];
    char nome[64];
} collaudo_operatore_t;

// ═══════════════════════════════════════════════════════════
// STRUTTURA RISULTATO COLLAUDO
// ═══════════════════════════════════════════════════════════
typedef struct {
    char  matricola[8];
    char  codice_tipo[4];
    char  operatore[64];
    int   esito;
    float consumo[3];   // [0]=TOP, [1]=MIN, [2]=Carico
    float giri[3];      // [0]=TOP, [1]=MIN, [2]=Carico
} collaudo_risultato_t;

// ═══════════════════════════════════════════════════════════
// HARDWARE
// ═══════════════════════════════════════════════════════════
#define COLLAUDO_RPM_GPIO   2   // Segnale TTL contagiri Electroil (via partitore 3.3k/6k)

// ═══════════════════════════════════════════════════════════
// FUNZIONI PUBBLICHE
// ═══════════════════════════════════════════════════════════

void             collaudo_manager_init(void);
collaudo_state_t collaudo_manager_get_state(void);
void             collaudo_manager_set_state(collaudo_state_t state);

// Badge operatore — PLACEHOLDER (endpoint non ancora disponibile)
esp_err_t        collaudo_manager_badge_in(const char *badge);

// Barcode motore → chiama collaudoDataIn.php
esp_err_t        collaudo_manager_scan_barcode(const char *barcode);

// Accesso dati
bool             collaudo_manager_get_motore(collaudo_motore_t *out);
bool             collaudo_manager_get_operatore(collaudo_operatore_t *out);

// RPM contagiri Electroil
void             collaudo_manager_rpm_start(void);
void             collaudo_manager_rpm_stop(void);
uint32_t         collaudo_manager_get_rpm(void);

// Reset sessione
void             collaudo_manager_reset(void);

// Proxy bilancia (chiamati da AppBanchetto — risolvono la dipendenza apps→main)
void             collaudo_bilancia_start(void);
void             collaudo_bilancia_stop(void);
void             collaudo_bilancia_check_async(void);

// Invia risultato collaudo al server (POST collaudoSave.php) — async task
esp_err_t        collaudo_manager_save_result(const collaudo_risultato_t *r);

// ═══════════════════════════════════════════════════════════
// STORICO COLLAUDI (SD card)
// ═══════════════════════════════════════════════════════════
typedef struct {
    char data_ora[20];      // "2026-04-17 14:30:00"
    char codice_tipo[4];    // "999"
    char matricola[8];      // "1234567"
    char operatore[16];     // badge operatore
    int  esito;             // 1=OK, 0=FAIL
} collaudo_record_t;

// Legge gli ultimi max_count collaudi dalla SD (ordine: più recente prima).
// Ritorna il numero di record effettivamente letti.
int collaudo_manager_read_history(collaudo_record_t *out, int max_count);

// ═══════════════════════════════════════════════════════════
// SIMULAZIONE WEB (solo build con COLLAUDI_BANCHETTO_ID)
// Chiamate dal web server per variare consumo/giri via browser
// ═══════════════════════════════════════════════════════════
#ifdef COLLAUDI_BANCHETTO_ID
void  collaudo_sim_set_consumo(float val);
void  collaudo_sim_set_giri(float val);
void  collaudo_sim_set_benzina(float val);
float collaudo_sim_get_benzina(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // COLLAUDO_MANAGER_H
