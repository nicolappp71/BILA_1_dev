#include "collaudo_manager.h"
#include "bilancia_manager.h"
#include "banchetto_manager.h"
#include "http_client.h"
#include "mode.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_cpu.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define COLLAUDO_HISTORY_PATH "/sdcard/collaudi_history.jsonl"

static const char *TAG = "COLLAUDO_MGR";
////////////////////////////// UBUNTU WSL
//#define SERVER_COLLAUDO_URL "http://localhost/iot/collaudoDataIn.php"
//#define SERVER_COLLAUDO_SAVE "http://localhost/iot/collaudoSave.php"

//////////////////////////////   XAAMP
#define SERVER_COLLAUDO_URL "http://172.18.2.254/iot/collaudoDataIn.php"
#define SERVER_COLLAUDO_SAVE "http://172.18.2.254/iot/collaudoSave.php"
/////////////////////////////   CASA
//#define SERVER_COLLAUDO_URL  "http://192.168.1.58/iot/collaudoDataIn.php"
//#define SERVER_COLLAUDO_SAVE "http://192.168.1.58/iot/collaudoSave.php"

// ── RPM ───────────────────────────────────────────────────────────────────────
#define RPM_NCAMPIONI   30
#define RPM_TIMEOUT_US  1000000UL   // 1s senza scintille → motore fermo
#define CPU_FREQ_MHZ    360         // ESP32-P4

static volatile uint32_t s_rpm_letture[RPM_NCAMPIONI] = {0};
static volatile uint32_t s_last_spark_cycles           = 0;
static volatile bool     s_rpm_active                  = false;
static uint32_t          s_rpm_prev                    = 0;

// ── Stato interno ─────────────────────────────────────────────────────────────
static collaudo_state_t s_state = COLLAUDO_STATE_CHECKIN;
static collaudo_motore_t s_motore = {0};
static collaudo_operatore_t s_operatore = {0};
static SemaphoreHandle_t s_mutex = NULL;

// ── Forward: callbacks verso AppBanchetto (implementate in AppBanchetto.cpp) ──
extern void collaudo_app_on_badge_ok(void);
extern void collaudo_app_on_motore_ok(void);
extern void collaudo_app_on_error(const char *msg);
extern void collaudo_app_on_save_ok(int idcollaudi, int esito_resp);
extern void collaudo_app_on_save_error(const char *msg);
extern void collaudo_app_refresh_page2(void);

// ═══════════════════════════════════════════════════════════
// INIT
// ═══════════════════════════════════════════════════════════
void collaudo_manager_init(void)
{
    if (s_mutex == NULL)
        s_mutex = xSemaphoreCreateMutex();

    s_state = COLLAUDO_STATE_CHECKIN;
    memset(&s_motore, 0, sizeof(s_motore));
    memset(&s_operatore, 0, sizeof(s_operatore));
    ESP_LOGI(TAG, "Inizializzato. Scanner collaudo inattivo.");
}

// ═══════════════════════════════════════════════════════════
// STATE MACHINE
// ═══════════════════════════════════════════════════════════
collaudo_state_t collaudo_manager_get_state(void)
{
    return s_state;
}

void collaudo_manager_set_state(collaudo_state_t state)
{
    s_state = state;
    ESP_LOGI(TAG, "Stato → %d", state);
}

// ═══════════════════════════════════════════════════════════
// BADGE IN  —  PLACEHOLDER
// Quando l'endpoint sarà disponibile, sostituire con chiamata HTTP
// ═══════════════════════════════════════════════════════════
esp_err_t collaudo_manager_badge_in(const char *badge)
{
    if (!badge || strlen(badge) == 0)
        return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "Badge ricevuto: %s (placeholder)", badge);

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        snprintf(s_operatore.badge, sizeof(s_operatore.badge), "%s", badge);
        // PLACEHOLDER: nome fittizio finché non c'è l'endpoint
        snprintf(s_operatore.nome, sizeof(s_operatore.nome), "OP: %s", badge);
        xSemaphoreGive(s_mutex);
    }

    collaudo_manager_set_state(COLLAUDO_STATE_SCAN_MOTORE);
    collaudo_app_on_badge_ok();
    return ESP_OK;
}

// ═══════════════════════════════════════════════════════════
// TASK HTTP: scarica parametri motore dal server
// ═══════════════════════════════════════════════════════════
static void fetch_motore_task(void *arg)
{
    char *barcode = (char *)arg;

    char codice_tipo[4] = {0};
    char matricola[8] = {0};

    size_t blen = strlen(barcode);
    if (blen < 10)
    {
        ESP_LOGE(TAG, "Barcode troppo corto: %s", barcode);
        collaudo_app_on_error("Barcode non valido (minimo 10 caratteri)");
        free(barcode);
        vTaskDelete(NULL);
        return;
    }

    strncpy(codice_tipo, barcode, 3);
    strncpy(matricola, barcode + 3, 7);

    ESP_LOGI(TAG, "Tipo motore: %s  Matricola: %s", codice_tipo, matricola);

    char url[256];
    snprintf(url, sizeof(url), "%s?barcode=%s", SERVER_COLLAUDO_URL, barcode);

    int resp_code = 0;
    char *body = NULL;

    esp_err_t ret = http_get_request(url, &resp_code, &body);
    if (ret != ESP_OK || resp_code != 200 || body == NULL)
    {
        ESP_LOGE(TAG, "Errore HTTP: ret=%d code=%d", ret, resp_code);
        collaudo_app_on_error("Server non raggiungibile");
        if (body)
            free(body);
        free(barcode);
        vTaskDelete(NULL);
        return;
    }

    cJSON *json = cJSON_Parse(body);
    free(body);

    if (json == NULL)
    {
        ESP_LOGE(TAG, "JSON non valido");
        collaudo_app_on_error("Risposta server non valida");
        free(barcode);
        vTaskDelete(NULL);
        return;
    }

    cJSON *err = cJSON_GetObjectItem(json, "errore");
    if (err && cJSON_IsString(err) && err->valuestring[0] != '\0')
    {
        ESP_LOGW(TAG, "Errore server: %s", err->valuestring);
        collaudo_app_on_error(err->valuestring);
        cJSON_Delete(json);
        free(barcode);
        vTaskDelete(NULL);
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        memset(&s_motore, 0, sizeof(s_motore));

        memcpy(s_motore.codice_tipo, codice_tipo, 3);
        s_motore.codice_tipo[3] = '\0';
        memcpy(s_motore.matricola, matricola, 7);
        s_motore.matricola[7] = '\0';

        cJSON *desc = cJSON_GetObjectItem(json, "descrizione");
        if (desc && cJSON_IsString(desc))
            strncpy(s_motore.descrizione, desc->valuestring, sizeof(s_motore.descrizione) - 1);

        cJSON *c = cJSON_GetObjectItem(json, "carico");
        if (c)
        {
            s_motore.carico_consumo_min = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(c, "consumo_min"));
            s_motore.carico_consumo_max = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(c, "consumo_max"));
            s_motore.carico_giri_min = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(c, "giri_min"));
            s_motore.carico_giri_max = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(c, "giri_max"));
        }

        cJSON *m = cJSON_GetObjectItem(json, "minimo");
        if (m)
        {
            s_motore.minimo_consumo_min = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(m, "consumo_min"));
            s_motore.minimo_consumo_max = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(m, "consumo_max"));
            s_motore.minimo_giri_min = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(m, "giri_min"));
            s_motore.minimo_giri_max = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(m, "giri_max"));
        }

        cJSON *t = cJSON_GetObjectItem(json, "top");
        if (t)
        {
            s_motore.top_consumo_min = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(t, "consumo_min"));
            s_motore.top_consumo_max = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(t, "consumo_max"));
            s_motore.top_giri_min = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(t, "giri_min"));
            s_motore.top_giri_max = (float)cJSON_GetNumberValue(cJSON_GetObjectItem(t, "giri_max"));
        }

        xSemaphoreGive(s_mutex);
    }

    cJSON_Delete(json);
    free(barcode);

    ESP_LOGI(TAG, "Motore caricato: [%s] %s  mat=%s",
             s_motore.codice_tipo, s_motore.descrizione, s_motore.matricola);

    collaudo_manager_set_state(COLLAUDO_STATE_IN_CORSO);
    collaudo_app_on_motore_ok();

    vTaskDelete(NULL);
}

// ═══════════════════════════════════════════════════════════
// SCAN BARCODE MOTORE
// ═══════════════════════════════════════════════════════════
esp_err_t collaudo_manager_scan_barcode(const char *barcode)
{
    if (!barcode || strlen(barcode) == 0)
        return ESP_ERR_INVALID_ARG;

    if (s_state == COLLAUDO_STATE_IN_CORSO)
    {
        ESP_LOGW(TAG, "scan_barcode durante IN_CORSO: reset e nuova scansione");
        collaudo_manager_reset();
    }
    else if (s_state != COLLAUDO_STATE_SCAN_MOTORE)
    {
        ESP_LOGW(TAG, "scan_barcode ignorato: stato=%d", s_state);
        return ESP_FAIL;
    }

    char *bc_copy = strdup(barcode);
    if (!bc_copy)
        return ESP_ERR_NO_MEM;

    xTaskCreate(fetch_motore_task, "collaudo_fetch", 6144, bc_copy, 5, NULL);
    return ESP_OK;
}

// ═══════════════════════════════════════════════════════════
// ACCESSO DATI
// ═══════════════════════════════════════════════════════════
bool collaudo_manager_get_motore(collaudo_motore_t *out)
{
    if (!out)
        return false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        memcpy(out, &s_motore, sizeof(collaudo_motore_t));
        xSemaphoreGive(s_mutex);
        return true;
    }
    return false;
}

bool collaudo_manager_get_operatore(collaudo_operatore_t *out)
{
    if (!out)
        return false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        memcpy(out, &s_operatore, sizeof(collaudo_operatore_t));
        xSemaphoreGive(s_mutex);
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════
// RESET
// ═══════════════════════════════════════════════════════════
void collaudo_manager_reset(void)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        memset(&s_motore, 0, sizeof(s_motore));
        memset(&s_operatore, 0, sizeof(s_operatore));
        xSemaphoreGive(s_mutex);
    }
    s_state = COLLAUDO_STATE_SCAN_MOTORE;
    ESP_LOGI(TAG, "Reset sessione collaudo → attendo barcode motore.");
}

// ═══════════════════════════════════════════════════════════
// SD CARD — salva/legge storico collaudi
// ═══════════════════════════════════════════════════════════
static void collaudo_save_to_sd(const collaudo_risultato_t *r)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[20];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);

    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "ts",   ts);
    cJSON_AddStringToObject(j, "tipo", r->codice_tipo);
    cJSON_AddStringToObject(j, "mat",  r->matricola);
    cJSON_AddStringToObject(j, "op",   r->operatore);
    cJSON_AddNumberToObject(j, "esito", r->esito);

    char *line = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    if (!line) return;

    FILE *f = fopen(COLLAUDO_HISTORY_PATH, "a");
    if (f) {
        fprintf(f, "%s\n", line);
        fclose(f);
        ESP_LOGI(TAG, "Storico SD: %s", line);
    } else {
        ESP_LOGW(TAG, "SD non disponibile, storico non salvato");
    }
    free(line);
}

int collaudo_manager_read_history(collaudo_record_t *out, int max_count)
{
    if (!out || max_count <= 0) return 0;

    FILE *f = fopen(COLLAUDO_HISTORY_PATH, "r");
    if (!f) return 0;

    char line[256];
    int total = 0;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] != '\0' && line[0] != '\n')
            total++;
    }
    if (total == 0) { fclose(f); return 0; }

    int skip = (total > max_count) ? (total - max_count) : 0;
    int to_read = total - skip;

    rewind(f);
    for (int i = 0; i < skip; i++) {
        if (!fgets(line, sizeof(line), f)) break;
    }

    int count = 0;
    while (count < to_read && fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n') continue;
        cJSON *j = cJSON_Parse(line);
        if (!j) continue;

        collaudo_record_t *rec = &out[count];
        memset(rec, 0, sizeof(*rec));

        const char *ts   = cJSON_GetStringValue(cJSON_GetObjectItem(j, "ts"));
        const char *tipo = cJSON_GetStringValue(cJSON_GetObjectItem(j, "tipo"));
        const char *mat  = cJSON_GetStringValue(cJSON_GetObjectItem(j, "mat"));
        const char *op   = cJSON_GetStringValue(cJSON_GetObjectItem(j, "op"));
        cJSON *esito_item = cJSON_GetObjectItem(j, "esito");

        if (ts)   snprintf(rec->data_ora,    sizeof(rec->data_ora),    "%s", ts);
        if (tipo) snprintf(rec->codice_tipo, sizeof(rec->codice_tipo), "%s", tipo);
        if (mat)  snprintf(rec->matricola,   sizeof(rec->matricola),   "%s", mat);
        if (op)   snprintf(rec->operatore,   sizeof(rec->operatore),   "%s", op);
        rec->esito = (esito_item && cJSON_IsNumber(esito_item)) ? esito_item->valueint : 0;

        cJSON_Delete(j);
        count++;
    }
    fclose(f);

    for (int i = 0; i < count / 2; i++) {
        collaudo_record_t tmp = out[i];
        out[i] = out[count - 1 - i];
        out[count - 1 - i] = tmp;
    }

    return count;
}

// ═══════════════════════════════════════════════════════════
// SAVE RESULT — task HTTP POST verso collaudoSave.php
// ═══════════════════════════════════════════════════════════
static void save_result_task(void *arg)
{
    collaudo_risultato_t *r = (collaudo_risultato_t *)arg;

    collaudo_save_to_sd(r);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "idmatricola",   r->matricola);
    cJSON_AddStringToObject(json, "idtipoMotore",  r->codice_tipo);
    cJSON_AddStringToObject(json, "Operatore",     r->operatore);
    cJSON_AddNumberToObject(json, "esito",         r->esito);
    cJSON_AddNumberToObject(json, "ConsumoTop",    (double)r->consumo[0]);
    cJSON_AddNumberToObject(json, "ConsumoMin",    (double)r->consumo[1]);
    cJSON_AddNumberToObject(json, "ConsumoCarico", (double)r->consumo[2]);
    cJSON_AddNumberToObject(json, "GiriTop",       (double)r->giri[0]);
    cJSON_AddNumberToObject(json, "GiriMin",       (double)r->giri[1]);
    cJSON_AddNumberToObject(json, "GiriCarico",    (double)r->giri[2]);

    char *body_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    free(r);

    if (!body_str) {
        collaudo_app_on_save_error("Errore JSON");
        vTaskDelete(NULL);
        return;
    }

    int resp_code = 0;
    char *resp_body = NULL;
    esp_err_t ret = http_post_request(SERVER_COLLAUDO_SAVE, body_str, &resp_code, &resp_body);
    free(body_str);

    if (ret != ESP_OK || resp_code != 200 || resp_body == NULL) {
        ESP_LOGE(TAG, "Save error: ret=%d code=%d", ret, resp_code);
        if (resp_body) free(resp_body);
        collaudo_app_on_save_error("Server non raggiungibile");
        vTaskDelete(NULL);
        return;
    }

    cJSON *resp = cJSON_Parse(resp_body);
    free(resp_body);

    if (!resp) {
        collaudo_app_on_save_error("Risposta non valida");
        vTaskDelete(NULL);
        return;
    }

    cJSON *err_item = cJSON_GetObjectItem(resp, "errore");
    if (err_item && cJSON_IsString(err_item) && err_item->valuestring[0] != '\0') {
        ESP_LOGW(TAG, "Save server error: %s", err_item->valuestring);
        collaudo_app_on_save_error(err_item->valuestring);
        cJSON_Delete(resp);
        vTaskDelete(NULL);
        return;
    }

    cJSON *id_item    = cJSON_GetObjectItem(resp, "idcollaudi");
    cJSON *esito_item = cJSON_GetObjectItem(resp, "esito");
    int idcollaudi   = (id_item    && cJSON_IsNumber(id_item))    ? id_item->valueint    : 0;
    int esito_resp   = (esito_item && cJSON_IsNumber(esito_item)) ? esito_item->valueint : 1;
    cJSON_Delete(resp);

    ESP_LOGI(TAG, "Collaudo salvato, idcollaudi=%d esito_resp=%d", idcollaudi, esito_resp);
    collaudo_app_on_save_ok(idcollaudi, esito_resp);

    if (esito_resp == 1) {
        banchetto_manager_versa(1);
        collaudo_app_refresh_page2();
    }

    vTaskDelete(NULL);
}

esp_err_t collaudo_manager_save_result(const collaudo_risultato_t *r)
{
    if (!r) return ESP_ERR_INVALID_ARG;

    collaudo_risultato_t *copy = (collaudo_risultato_t *)malloc(sizeof(collaudo_risultato_t));
    if (!copy) return ESP_ERR_NO_MEM;
    memcpy(copy, r, sizeof(collaudo_risultato_t));

    xTaskCreate(save_result_task, "coll_save", 6144, copy, 5, NULL);
    return ESP_OK;
}

// ═══════════════════════════════════════════════════════════
// RPM — contagiri Electroil su GPIO COLLAUDO_RPM_GPIO
// Segnale: TTL (via partitore 3.3k/6k → 3.22V), 1 impulso/giro, 2.5ms HIGH
// Algoritmo: filtro mediano con inserimento ordinato (vettorifica)
// ═══════════════════════════════════════════════════════════

static IRAM_ATTR void vettorifica(uint32_t cur_rpm)
{
    int x = 0;
    while (x < RPM_NCAMPIONI && cur_rpm > s_rpm_letture[x]) x++;

    uint32_t val1 = cur_rpm, val2;
    if (x < RPM_NCAMPIONI / 2) {
        for (int y = x; y < RPM_NCAMPIONI; y++) {
            val2 = s_rpm_letture[y]; s_rpm_letture[y] = val1; val1 = val2;
        }
    } else {
        if (--x >= 0) {
            for (int y = x; y >= 0; y--) {
                val2 = s_rpm_letture[y]; s_rpm_letture[y] = val1; val1 = val2;
            }
        }
    }
}

static void IRAM_ATTR spark_isr(void *arg)
{
    uint32_t now         = esp_cpu_get_cycle_count();
    uint32_t interval    = now - s_last_spark_cycles;
    uint32_t interval_us = interval / CPU_FREQ_MHZ;
    if (interval_us > 3000) {
        s_last_spark_cycles = now;
        vettorifica(60000000UL / interval_us);
    }
}

void collaudo_manager_rpm_start(void)
{
    if (s_rpm_active) return;

    memset((void *)s_rpm_letture, 0, sizeof(s_rpm_letture));
    s_last_spark_cycles = esp_cpu_get_cycle_count();
    s_rpm_prev = 0;

    gpio_config_t io = {
        .pin_bit_mask  = 1ULL << COLLAUDO_RPM_GPIO,
        .mode          = GPIO_MODE_INPUT,
        .pull_up_en    = GPIO_PULLUP_DISABLE,
        .pull_down_en  = GPIO_PULLDOWN_ENABLE,
        .intr_type     = GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(gpio_isr_handler_add(COLLAUDO_RPM_GPIO, spark_isr, NULL));
    s_rpm_active = true;
    ESP_LOGI(TAG, "RPM ISR avviato su GPIO %d", COLLAUDO_RPM_GPIO);
}

void collaudo_manager_rpm_stop(void)
{
    if (!s_rpm_active) return;
    gpio_isr_handler_remove(COLLAUDO_RPM_GPIO);
    gpio_set_intr_type(COLLAUDO_RPM_GPIO, GPIO_INTR_DISABLE);
    s_rpm_active = false;
    ESP_LOGI(TAG, "RPM ISR fermato");
}

uint32_t collaudo_manager_get_rpm(void)
{
    if (!s_rpm_active) return 0;
    uint32_t elapsed_us = (esp_cpu_get_cycle_count() - s_last_spark_cycles) / CPU_FREQ_MHZ;
    if (elapsed_us > RPM_TIMEOUT_US) return 0;
    uint32_t median = s_rpm_letture[RPM_NCAMPIONI / 2];
    uint32_t result = (s_rpm_prev + median) / 2;
    s_rpm_prev = result;
    return result;
}

// ── Proxy bilancia ────────────────────────────────────────
void collaudo_bilancia_start(void)       { bilancia_manager_start_poll(); }
void collaudo_bilancia_stop(void)        { bilancia_manager_stop_poll(); }
void collaudo_bilancia_check_async(void) { bilancia_manager_check_level_async(); }
