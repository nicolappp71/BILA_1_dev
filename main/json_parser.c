

// #include "json_parser.h"
// #include "cJSON.h"
// #include "esp_log.h"
// #include <string.h>

// static const char *TAG = "JSON_PARSER";

// static void safe_copy_string(char *dest, size_t dest_size, cJSON *json_item)
// {
//     if (json_item && cJSON_IsString(json_item) && json_item->valuestring) {
//         strncpy(dest, json_item->valuestring, dest_size - 1);
//         dest[dest_size - 1] = '\0';
//     } else if (json_item && cJSON_IsNumber(json_item)) {
//         snprintf(dest, dest_size, "%d", json_item->valueint);
//     } else {
//         dest[0] = '\0';
//     }
// }

// static uint32_t safe_get_uint32(cJSON *json_item)
// {
//     if (json_item && cJSON_IsNumber(json_item)) {
//         return (uint32_t)json_item->valueint;
//     }
//     if (json_item && cJSON_IsString(json_item) && json_item->valuestring) {
//         return (uint32_t)atoi(json_item->valuestring);
//     }
//     return 0;
// }

// static bool safe_get_bool(cJSON *json_item)
// {
//     if (json_item && cJSON_IsNumber(json_item)) {
//         return json_item->valueint != 0;
//     }
//     if (json_item && cJSON_IsBool(json_item)) {
//         return cJSON_IsTrue(json_item);
//     }
//     return false;
// }

// esp_err_t parse_banchetto_response(const char *json_string, banchetto_data_t *data)
// {
//     if (!json_string || !data) {
//         ESP_LOGE(TAG, "Parametri NULL");
//         return ESP_ERR_INVALID_ARG;
//     }
    
//     memset(data, 0, sizeof(banchetto_data_t));
    
//     cJSON *json = cJSON_Parse(json_string);
//     if (json == NULL) {
//         ESP_LOGE(TAG, "Errore parsing JSON");
//         return ESP_FAIL;
//     }
    
//     safe_copy_string(data->banchetto, sizeof(data->banchetto), 
//                      cJSON_GetObjectItem(json, "banchetto"));
//     safe_copy_string(data->operatore, sizeof(data->operatore), 
//                      cJSON_GetObjectItem(json, "operatore"));
//     data->qta_totale_scatola = safe_get_uint32(cJSON_GetObjectItem(json, "qta_totale_scatola"));
//     safe_copy_string(data->matr_scatola_corrente, sizeof(data->matr_scatola_corrente), 
//                      cJSON_GetObjectItem(json, "matr_scatola_corrente"));
//     safe_copy_string(data->matricola, sizeof(data->matricola), 
//                      cJSON_GetObjectItem(json, "matricola"));
//     data->sessione_aperta = safe_get_bool(cJSON_GetObjectItem(json, "sessione_aperta"));
//     data->blocca_qta = safe_get_bool(cJSON_GetObjectItem(json, "blocca_qta"));
//     safe_copy_string(data->codice_articolo, sizeof(data->codice_articolo), 
//                      cJSON_GetObjectItem(json, "codice_articolo"));
//     safe_copy_string(data->descrizione_articolo, sizeof(data->descrizione_articolo), 
//                      cJSON_GetObjectItem(json, "descrizione_articolo"));
//     data->qta_totale = safe_get_uint32(cJSON_GetObjectItem(json, "qta_totale"));
//     data->ord_prod = safe_get_uint32(cJSON_GetObjectItem(json, "ord_prod"));
//     safe_copy_string(data->ciclo, sizeof(data->ciclo), 
//                      cJSON_GetObjectItem(json, "ciclo"));
//     safe_copy_string(data->fase, sizeof(data->fase), 
//                      cJSON_GetObjectItem(json, "fase"));
//     safe_copy_string(data->descr_fase, sizeof(data->descr_fase), 
//                      cJSON_GetObjectItem(json, "descr_fase"));
//     data->qta_prod_fase = safe_get_uint32(cJSON_GetObjectItem(json, "qta_prod_fase"));
//     data->qta_totale_giornaliera = safe_get_uint32(cJSON_GetObjectItem(json, "qta_totale_giornaliera"));
//     data->qta_prod_sessione = safe_get_uint32(cJSON_GetObjectItem(json, "qta_prod_sessione"));
//     data->qta_scatola = safe_get_uint32(cJSON_GetObjectItem(json, "qta_scatola"));
//     data->giorno = (uint8_t)safe_get_uint32(cJSON_GetObjectItem(json, "giorno"));
//     data->mese = (uint8_t)safe_get_uint32(cJSON_GetObjectItem(json, "mese"));
//     data->anno = (uint16_t)safe_get_uint32(cJSON_GetObjectItem(json, "anno"));
//     data->ore = (uint8_t)safe_get_uint32(cJSON_GetObjectItem(json, "ore"));
//     data->minuti = (uint8_t)safe_get_uint32(cJSON_GetObjectItem(json, "minuti"));
//     data->secondi = (uint8_t)safe_get_uint32(cJSON_GetObjectItem(json, "secondi"));
    
//     cJSON_Delete(json);
    
//     if (strlen(data->codice_articolo) == 0) {
//         ESP_LOGE(TAG, "codice_articolo vuoto");
//         return ESP_FAIL;
//     }
    
//     return ESP_OK;
// }

// void print_banchetto_data(const banchetto_data_t *data)
// {
//     if (!data) return;
    
//     ESP_LOGI(TAG, "");
//     ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
//     ESP_LOGI(TAG, "║      DATI BANCHETTO COMPLETI           ║");
//     ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
//     ESP_LOGI(TAG, "");
//     ESP_LOGI(TAG, "📍 POSTAZIONE:");
//     ESP_LOGI(TAG, "   Banchetto: %s", data->banchetto);
//     ESP_LOGI(TAG, "   Operatore: %s", data->operatore);
//     ESP_LOGI(TAG, "   Matricola: %s", data->matricola);
//     ESP_LOGI(TAG, "");
//     ESP_LOGI(TAG, "📦 ARTICOLO:");
//     ESP_LOGI(TAG, "   Codice: %s", data->codice_articolo);
//     ESP_LOGI(TAG, "   Descrizione: %s", data->descrizione_articolo);
//     ESP_LOGI(TAG, "");
//     ESP_LOGI(TAG, "🏭 ORDINE PRODUZIONE:");
//     ESP_LOGI(TAG, "   OdP: %lu", data->ord_prod);
//     ESP_LOGI(TAG, "   Ciclo: %s", data->ciclo);
//     ESP_LOGI(TAG, "   Fase: %s", data->fase);
//     ESP_LOGI(TAG, "   Descr Fase: %s", data->descr_fase);
//     ESP_LOGI(TAG, "");
//     ESP_LOGI(TAG, "📊 QUANTITÀ:");
//     ESP_LOGI(TAG, "   Totale OdP: %lu", data->qta_totale);
//     ESP_LOGI(TAG, "   Prodotta Fase: %lu", data->qta_prod_fase);
//     uint32_t rimanente = data->qta_totale > data->qta_prod_fase ? 
//                          data->qta_totale - data->qta_prod_fase : 0;
//     ESP_LOGI(TAG, "   Rimanente: %lu", rimanente);
//     ESP_LOGI(TAG, "   Giornaliera: %lu", data->qta_totale_giornaliera);
//     ESP_LOGI(TAG, "   Sessione: %lu", data->qta_prod_sessione);
//     ESP_LOGI(TAG, "");
//     ESP_LOGI(TAG, "📦 SCATOLA:");
//     ESP_LOGI(TAG, "   Matricola: %s", data->matr_scatola_corrente);
//     ESP_LOGI(TAG, "   Pezzi: %lu / %lu", data->qta_scatola, data->qta_totale_scatola);
//     ESP_LOGI(TAG, "");
//     ESP_LOGI(TAG, "🕐 TIMESTAMP:");
//     ESP_LOGI(TAG, "   Data: %02d/%02d/%04d", data->giorno, data->mese, data->anno);
//     ESP_LOGI(TAG, "   Ora: %02d:%02d:%02d", data->ore, data->minuti, data->secondi);
//     ESP_LOGI(TAG, "");
//     ESP_LOGI(TAG, "ℹ️  STATO:");
//     ESP_LOGI(TAG, "   Sessione aperta: %s", data->sessione_aperta ? "SÌ" : "NO");
//     ESP_LOGI(TAG, "   Quantità bloccata: %s", data->blocca_qta ? "SÌ" : "NO");
//     ESP_LOGI(TAG, "");
// }

// esp_err_t parse_badge_response(const char *json_string, badge_response_t *response)
// {
//     if (!json_string || !response) {
//         ESP_LOGE(TAG, "Parametri NULL");
//         return ESP_ERR_INVALID_ARG;
//     }
    
//     // Inizializza con valori di default
//     memset(response, 0, sizeof(badge_response_t));
//     response->matricola = -1;
//     response->formazione = 999;
//     response->success = false;
    
//     cJSON *json = cJSON_Parse(json_string);
//     if (json == NULL) {
//         ESP_LOGE(TAG, "Errore parsing JSON badge");
//         strncpy(response->errore, "JSON malformato", sizeof(response->errore) - 1);
//         return ESP_FAIL;
//     }
    
//     // Estrai operatore
//     cJSON *operatore_item = cJSON_GetObjectItem(json, "operatore");
//     if (operatore_item && cJSON_IsString(operatore_item) && operatore_item->valuestring) {
//         strncpy(response->operatore, operatore_item->valuestring, sizeof(response->operatore) - 1);
//         response->operatore[sizeof(response->operatore) - 1] = '\0';
//     }
    
//     // Estrai matricola
//     cJSON *matricola_item = cJSON_GetObjectItem(json, "matricola");
//     if (matricola_item) {
//         if (cJSON_IsNumber(matricola_item)) {
//             response->matricola = matricola_item->valueint;
//         } else if (cJSON_IsString(matricola_item) && matricola_item->valuestring) {
//             response->matricola = atoi(matricola_item->valuestring);
//         }
//     }
    
//     // Estrai formazione
//     cJSON *formazione_item = cJSON_GetObjectItem(json, "formazione");
//     if (formazione_item) {
//         if (cJSON_IsNumber(formazione_item)) {
//             response->formazione = formazione_item->valueint;
//         } else if (cJSON_IsString(formazione_item) && formazione_item->valuestring) {
//             response->formazione = atoi(formazione_item->valuestring);
//         }
//     }
    
//     // Controlla errori
//     cJSON *errore_item = cJSON_GetObjectItem(json, "errore");
//     if (errore_item && cJSON_IsString(errore_item) && errore_item->valuestring) {
//         strncpy(response->errore, errore_item->valuestring, sizeof(response->errore) - 1);
//         response->errore[sizeof(response->errore) - 1] = '\0';
        
//         if (strlen(response->errore) > 0) {
//             ESP_LOGW(TAG, "Errore badge: %s", response->errore);
//             cJSON_Delete(json);
//             return ESP_OK;
//         }
//     }
    
//     cJSON_Delete(json);
    
//     // Validazione finale
//     if (response->matricola > 0 && response->formazione != 999 && strlen(response->errore) == 0) {
//         response->success = true;
//         ESP_LOGI(TAG, "✓ Badge valido: matricola=%d, formazione=%d", 
//                  response->matricola, response->formazione);
//     } else if (response->matricola == 0 && strlen(response->errore) == 0) {
//         ESP_LOGI(TAG, "✓ Comando chiusura sessione (matricola=0)");
//     } else {
//         ESP_LOGW(TAG, "✗ Badge non valido: matricola=%d, formazione=%d, errore='%s'",
//                  response->matricola, response->formazione, response->errore);
//     }
    
//     return ESP_OK;
// }

#include "json_parser.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "JSON_PARSER";

// ═══════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════

static void safe_copy_string(char *dest, size_t dest_size, cJSON *item)
{
    if (item && cJSON_IsString(item) && item->valuestring) {
        strncpy(dest, item->valuestring, dest_size - 1);
        dest[dest_size - 1] = '\0';
    } else if (item && cJSON_IsNumber(item)) {
        snprintf(dest, dest_size, "%d", item->valueint);
    } else {
        dest[0] = '\0';
    }
}

static uint32_t safe_get_uint32(cJSON *item)
{
    if (item && cJSON_IsNumber(item))
        return (uint32_t)item->valueint;
    if (item && cJSON_IsString(item) && item->valuestring)
        return (uint32_t)atoi(item->valuestring);
    return 0;
}

static bool safe_get_bool(cJSON *item)
{
    if (item && cJSON_IsNumber(item))
        return item->valueint != 0;
    if (item && cJSON_IsBool(item))
        return cJSON_IsTrue(item);
    return false;
}

// ═══════════════════════════════════════════════════════════
// PARSING SINGOLO ELEMENTO (interno)
// ═══════════════════════════════════════════════════════════

static esp_err_t parse_single_item(cJSON *obj, banchetto_data_t *data)
{
    if (!obj || !data) return ESP_ERR_INVALID_ARG;

    memset(data, 0, sizeof(banchetto_data_t));

    safe_copy_string(data->banchetto,            sizeof(data->banchetto),            cJSON_GetObjectItem(obj, "banchetto"));
    safe_copy_string(data->operatore,            sizeof(data->operatore),            cJSON_GetObjectItem(obj, "operatore"));
    safe_copy_string(data->matr_scatola_corrente,sizeof(data->matr_scatola_corrente),cJSON_GetObjectItem(obj, "matr_scatola_corrente"));
    safe_copy_string(data->matricola,            sizeof(data->matricola),            cJSON_GetObjectItem(obj, "matricola"));
    safe_copy_string(data->codice_articolo,      sizeof(data->codice_articolo),      cJSON_GetObjectItem(obj, "codice_articolo"));
    safe_copy_string(data->descrizione_articolo, sizeof(data->descrizione_articolo), cJSON_GetObjectItem(obj, "descrizione_articolo"));
    safe_copy_string(data->ciclo,                sizeof(data->ciclo),                cJSON_GetObjectItem(obj, "ciclo"));
    safe_copy_string(data->fase,                 sizeof(data->fase),                 cJSON_GetObjectItem(obj, "fase"));
    safe_copy_string(data->descr_fase,           sizeof(data->descr_fase),           cJSON_GetObjectItem(obj, "descr_fase"));

    data->qta_totale_scatola    = safe_get_uint32(cJSON_GetObjectItem(obj, "qta_totale_scatola"));
    data->qta_totale            = safe_get_uint32(cJSON_GetObjectItem(obj, "qta_totale"));
    data->ord_prod              = safe_get_uint32(cJSON_GetObjectItem(obj, "ord_prod"));
    data->qta_prod_fase         = safe_get_uint32(cJSON_GetObjectItem(obj, "qta_prod_fase"));
    data->qta_totale_giornaliera= safe_get_uint32(cJSON_GetObjectItem(obj, "qta_totale_giornaliera"));
    data->qta_prod_sessione     = safe_get_uint32(cJSON_GetObjectItem(obj, "qta_prod_sessione"));
    data->qta_scatola           = safe_get_uint32(cJSON_GetObjectItem(obj, "qta_scatola"));
    data->qta_pezzi             = safe_get_uint32(cJSON_GetObjectItem(obj, "qta_pezzi"));
    if (data->qta_pezzi == 0) data->qta_pezzi = 1;
    data->giorno                = (uint8_t) safe_get_uint32(cJSON_GetObjectItem(obj, "giorno"));
    data->mese                  = (uint8_t) safe_get_uint32(cJSON_GetObjectItem(obj, "mese"));
    data->anno                  = (uint16_t)safe_get_uint32(cJSON_GetObjectItem(obj, "anno"));
    data->ore                   = (uint8_t) safe_get_uint32(cJSON_GetObjectItem(obj, "ore"));
    data->minuti                = (uint8_t) safe_get_uint32(cJSON_GetObjectItem(obj, "minuti"));
    data->secondi               = (uint8_t) safe_get_uint32(cJSON_GetObjectItem(obj, "secondi"));

    data->sessione_aperta = safe_get_bool(cJSON_GetObjectItem(obj, "sessione_aperta"));
    data->blocca_qta      = safe_get_bool(cJSON_GetObjectItem(obj, "blocca_qta"));

    if (strlen(data->codice_articolo) == 0) {
        ESP_LOGE(TAG, "codice_articolo vuoto");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// ═══════════════════════════════════════════════════════════
// PARSE LIST (array o oggetto singolo)
// ═══════════════════════════════════════════════════════════

esp_err_t parse_banchetto_list(const char *json_string, banchetto_list_t *list)
{
    if (!json_string || !list) {
        ESP_LOGE(TAG, "Parametri NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(list, 0, sizeof(banchetto_list_t));

    cJSON *root = cJSON_Parse(json_string);
    if (!root) {
        ESP_LOGE(TAG, "Errore parsing JSON");
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;

    if (cJSON_IsArray(root)) {
        int n = cJSON_GetArraySize(root);
        if (n > BANCHETTO_MAX_ITEMS) {
            ESP_LOGW(TAG, "Array %d elementi, troncato a %d", n, BANCHETTO_MAX_ITEMS);
            n = BANCHETTO_MAX_ITEMS;
        }
        for (int i = 0; i < n; i++) {
            cJSON *item = cJSON_GetArrayItem(root, i);
            if (parse_single_item(item, &list->items[i]) == ESP_OK) {
                list->count++;
                ESP_LOGI(TAG, "[%d/%d] %s — OdP:%lu", i + 1, n,
                         list->items[i].codice_articolo, list->items[i].ord_prod);
            } else {
                ESP_LOGW(TAG, "Elemento [%d] non valido, saltato", i);
            }
        }
        if (list->count == 0) {
            ESP_LOGE(TAG, "Nessun elemento valido nell'array");
            ret = ESP_FAIL;
        }
    } else if (cJSON_IsObject(root)) {
        // oggetto singolo — retrocompatibilità
        if (parse_single_item(root, &list->items[0]) == ESP_OK) {
            list->count = 1;
            ESP_LOGI(TAG, "Oggetto singolo: %s — OdP:%lu",
                     list->items[0].codice_articolo, list->items[0].ord_prod);
        } else {
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "JSON non è né array né oggetto");
        ret = ESP_FAIL;
    }

    cJSON_Delete(root);
    return ret;
}

// ═══════════════════════════════════════════════════════════
// RETROCOMPATIBILITÀ
// ═══════════════════════════════════════════════════════════

esp_err_t parse_banchetto_response(const char *json_string, banchetto_data_t *data)
{
    if (!json_string || !data) return ESP_ERR_INVALID_ARG;

    banchetto_list_t list;
    esp_err_t ret = parse_banchetto_list(json_string, &list);
    if (ret != ESP_OK) return ret;

    char myciclo[16];
    strncpy(myciclo, list.items[0].ciclo, sizeof(myciclo) - 1);
    myciclo[sizeof(myciclo) - 1] = '\0';

    memcpy(data, &list.items[0], sizeof(banchetto_data_t));
    return ESP_OK;
}

// ═══════════════════════════════════════════════════════════
// PRINT
// ═══════════════════════════════════════════════════════════

void print_banchetto_data(const banchetto_data_t *data)
{
    if (!data) return;

    ESP_LOGI(TAG, "--- ARTICOLO ---");
    ESP_LOGI(TAG, "Banchetto: %s | Operatore: %s | Matricola: %s",
             data->banchetto, data->operatore, data->matricola);
    ESP_LOGI(TAG, "Codice: %s | %s", data->codice_articolo, data->descrizione_articolo);
    ESP_LOGI(TAG, "OdP: %lu | Ciclo: %s | Fase: %s", data->ord_prod, data->ciclo, data->fase);
    ESP_LOGI(TAG, "Qta: totale=%lu fase=%lu giorn=%lu sess=%lu",
             data->qta_totale, data->qta_prod_fase,
             data->qta_totale_giornaliera, data->qta_prod_sessione);
    ESP_LOGI(TAG, "Scatola: %s %lu/%lu",
             data->matr_scatola_corrente, data->qta_scatola, data->qta_totale_scatola);
    ESP_LOGI(TAG, "Sessione: %s | BloccaQta: %s",
             data->sessione_aperta ? "APERTA" : "CHIUSA",
             data->blocca_qta ? "SI" : "NO");
    ESP_LOGI(TAG, "Data: %02d/%02d/%04d %02d:%02d:%02d",
             data->giorno, data->mese, data->anno,
             data->ore, data->minuti, data->secondi);
}

void print_banchetto_list(const banchetto_list_t *list)
{
    if (!list) return;
    ESP_LOGI(TAG, "═══ LISTA BANCHETTO (%d articoli) ═══", list->count);
    for (int i = 0; i < list->count; i++) {
        ESP_LOGI(TAG, "[ %d ]", i + 1);
        print_banchetto_data(&list->items[i]);
    }
}

// ═══════════════════════════════════════════════════════════
// BADGE
// ═══════════════════════════════════════════════════════════

esp_err_t parse_badge_response(const char *json_string, badge_response_t *response)
{
    if (!json_string || !response) {
        ESP_LOGE(TAG, "Parametri NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(response, 0, sizeof(badge_response_t));
    response->matricola = -1;
    response->formazione = 999;
    response->success = false;

    cJSON *json = cJSON_Parse(json_string);
    if (!json) {
        ESP_LOGE(TAG, "Errore parsing JSON badge");
        strncpy(response->errore, "JSON malformato", sizeof(response->errore) - 1);
        return ESP_FAIL;
    }

    safe_copy_string(response->operatore, sizeof(response->operatore),
                     cJSON_GetObjectItem(json, "operatore"));

    cJSON *mat = cJSON_GetObjectItem(json, "matricola");
    if (mat) {
        if (cJSON_IsNumber(mat))       response->matricola = mat->valueint;
        else if (cJSON_IsString(mat))  response->matricola = atoi(mat->valuestring);
    }

    cJSON *form = cJSON_GetObjectItem(json, "formazione");
    if (form) {
        if (cJSON_IsNumber(form))      response->formazione = form->valueint;
        else if (cJSON_IsString(form)) response->formazione = atoi(form->valuestring);
    }

    cJSON *err = cJSON_GetObjectItem(json, "errore");
    if (err && cJSON_IsString(err) && err->valuestring) {
        strncpy(response->errore, err->valuestring, sizeof(response->errore) - 1);
        response->errore[sizeof(response->errore) - 1] = '\0';
        if (strlen(response->errore) > 0) {
            ESP_LOGW(TAG, "Errore badge: %s", response->errore);
            cJSON_Delete(json);
            return ESP_OK;
        }
    }

    cJSON_Delete(json);

    if (response->matricola > 0 && response->formazione != 999 && strlen(response->errore) == 0) {
        response->success = true;
        ESP_LOGI(TAG, "Badge valido: matricola=%d formazione=%d",
                 response->matricola, response->formazione);
    } else if (response->matricola == 0 && strlen(response->errore) == 0) {
        ESP_LOGI(TAG, "Comando chiusura sessione (matricola=0)");
    } else {
        ESP_LOGW(TAG, "Badge non valido: matricola=%d formazione=%d errore='%s'",
                 response->matricola, response->formazione, response->errore);
    }

    return ESP_OK;
}