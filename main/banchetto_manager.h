
#ifndef BANCHETTO_MANAGER_H
#define BANCHETTO_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════
// STATE MACHINE
// ═══════════════════════════════════════════════════════════
typedef enum
{
    BANCHETTO_STATE_CHECKIN,
    BANCHETTO_STATE_CONTEGGIO,
    BANCHETTO_STATE_CONTROLLO,
    BANCHETTO_STATE_ASSEGNA_BANCHETTO,
    BANCHETTO_STATE_ATTESA_FORMATORE,
    BANCHETTO_STATE_ATTESA_CONFERMA_FORMAZIONE,
} banchetto_state_t;

// ═══════════════════════════════════════════════════════════
// STRUTTURA DATI SINGOLO ARTICOLO
// ═══════════════════════════════════════════════════════════
typedef struct
{
    char     banchetto[16];
    char     operatore[64];
    uint32_t qta_totale_scatola;
    char     matr_scatola_corrente[32];
    char     matricola[16];
    bool     sessione_aperta;
    bool     blocca_qta;
    char     codice_articolo[32];
    char     descrizione_articolo[128];
    uint32_t qta_totale;
    uint32_t ord_prod;
    char     ciclo[16];
    char     fase[16];
    char     descr_fase[128];
    uint32_t qta_prod_fase;
    uint32_t qta_totale_giornaliera;
    uint32_t qta_prod_sessione;
    uint32_t qta_scatola;
    uint32_t qta_pezzi;
    uint8_t  giorno;
    uint8_t  mese;
    uint16_t anno;
    uint8_t  ore;
    uint8_t  minuti;
    uint8_t  secondi;
} banchetto_data_t;

// ═══════════════════════════════════════════════════════════
// FUNZIONI PUBBLICHE
// ═══════════════════════════════════════════════════════════

void          banchetto_manager_init(void);
esp_err_t     banchetto_manager_fetch_from_server(void);
void          banchetto_manager_start_periodic_refresh(void);
esp_err_t     banchetto_manager_load_from_sd(void);
esp_err_t     banchetto_manager_assegna_banchetto(const char *barcode);
esp_err_t     banchetto_manager_login_badge(const char *badge);
esp_err_t     banchetto_manager_login_by_matricola(uint16_t matricola);
esp_err_t     banchetto_manager_controllo(const char *badge);
void          banchetto_manager_set_barcode(const char *barcode);
bool          banchetto_manager_versa(uint32_t qta);
bool          banchetto_manager_scarto(uint32_t qta_scarti);
void          banchetto_manager_set_versa_abilitato(bool abilitato);
bool          banchetto_manager_get_versa_abilitato(void);

// Formazione
esp_err_t     banchetto_manager_formazione_formatore(const char *badge);
esp_err_t     banchetto_manager_formazione_accettazione(const char *badge);

// Accesso dati
bool          banchetto_manager_get_data(banchetto_data_t *out_data);
bool          banchetto_manager_get_item(uint8_t index, banchetto_data_t *out_data);
uint8_t       banchetto_manager_get_count(void);

// Navigazione UI
uint8_t       banchetto_manager_get_current_index(void);
void          banchetto_manager_set_current_index(uint8_t index);

// State machine
void              banchetto_manager_set_state(banchetto_state_t state);
banchetto_state_t banchetto_manager_get_state(void);

// Utility
void          banchetto_manager_print_status(void);
bool          banchetto_manager_is_ready(void);
void          banchetto_manager_reset_data(void);
void          banchetto_manager_reconstruct_from_journal(void);
bool          banchetto_is_server_reachable(void);

#ifdef __cplusplus
}
#endif

#endif // BANCHETTO_MANAGER_H