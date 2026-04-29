// #ifndef JSON_PARSER_H
// #define JSON_PARSER_H

// #include "esp_err.h"
// #include "banchetto_manager.h"

// #ifdef __cplusplus
// extern "C" {
// #endif
// typedef struct {
//     char operatore[64];   // AGGIUNTO: nome operatore dal server
//     int matricola;
//     int formazione;
//     char errore[128];
//     bool success;
// } badge_response_t;

// esp_err_t parse_badge_response(const char *json_string, badge_response_t *response);
// esp_err_t parse_banchetto_response(const char *json_string, banchetto_data_t *data);
// void print_banchetto_data(const banchetto_data_t *data);

// #ifdef __cplusplus
// }
// #endif

// #endif // JSON_PARSER_H

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "esp_err.h"
#include "banchetto_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BANCHETTO_MAX_ITEMS 10

// ═══════════════════════════════════════════════════════════
// LISTA ARTICOLI (risposta array dal server)
// ═══════════════════════════════════════════════════════════
typedef struct {
    banchetto_data_t items[BANCHETTO_MAX_ITEMS];
    uint8_t count;
} banchetto_list_t;

// ═══════════════════════════════════════════════════════════
// BADGE
// ═══════════════════════════════════════════════════════════
typedef struct {
    char operatore[64];
    int  matricola;
    int  formazione;
    char errore[128];
    bool success;
} badge_response_t;

// ═══════════════════════════════════════════════════════════
// API
// ═══════════════════════════════════════════════════════════

// Parsing principale — gestisce sia oggetto singolo che array.
// Popola sempre banchetto_list_t (count=1 per oggetto singolo).
esp_err_t parse_banchetto_list(const char *json_string, banchetto_list_t *list);

// Mantenuta per retrocompatibilità — wrappa parse_banchetto_list
esp_err_t parse_banchetto_response(const char *json_string, banchetto_data_t *data);

esp_err_t parse_badge_response(const char *json_string, badge_response_t *response);

void print_banchetto_data(const banchetto_data_t *data);
void print_banchetto_list(const banchetto_list_t *list);

#ifdef __cplusplus
}
#endif

#endif // JSON_PARSER_H