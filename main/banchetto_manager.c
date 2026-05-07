#include "banchetto_manager.h"
#include "json_parser.h"
#include "lvgl.h"
#include "bsp/display.h"
#include "esp_lvgl_port.h"
#include "bsp/esp-bsp.h"
#include "screens.h"
#include "web_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "http_client.h"
#include "key_manager.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "audio_player.h"
#include "tagliatubi_manager.h"
#include "tastiera.h"
#include "mode.h"
#include "offline_journal.h"
#include "wifi_manager.h"
#include "badge_cache.h"
#include "time_manager.h"
#include "freertos/event_groups.h"

static bool is_online(void)
{
    return (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}

static volatile bool s_server_reachable = true;

bool banchetto_is_server_reachable(void)
{
    return s_server_reachable;
}

extern void app_banchetto_update_page1(void);
extern void app_banchetto_update_page2(void);
extern void app_banchetto_update_page3(void);
extern void app_banchetto_nuova_scatola(void);
extern void app_assegna_banchetto_close(void);
extern void deep_sleep_reset_timer(void);

static char s_formazione_cod_art[32] = {0};
static char s_formazione_badge[64] = {0};
static const char *TAG = "BANCHETTO_MGR";

#define SERVER_URL SERVER_BASE "/iot/banchetti_1_9_4.php"
#define SERVER_BADGE_URL SERVER_BASE "/iot/badge.php"
#define SD_CACHE_PATH "/sdcard/banchetti_cache.json"

static banchetto_list_t s_list = {0};
static uint8_t s_current_idx = 0;
static SemaphoreHandle_t data_mutex = NULL;
static char device_key[17] = {0};
static char s_device_banchetto_id[32] = {0};  // ID banchetto del dispositivo (anche senza ordini)
static banchetto_state_t s_state = BANCHETTO_STATE_CHECKIN;
static bool s_versa_abilitato = true;  // toggle conteggio pezzi

void banchetto_manager_set_versa_abilitato(bool abilitato) { s_versa_abilitato = abilitato; }
bool banchetto_manager_get_versa_abilitato(void)           { return s_versa_abilitato; }

// ═══════════════════════════════════════════════════════════
// UI CALLBACKS E AUDIO
// ═══════════════════════════════════════════════════════════

static void reset_panel_style(lv_timer_t *timer)
{
    lv_obj_t *obj = (lv_obj_t *)timer->user_data;
    if (obj != NULL)
    {
        lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_invalidate(obj);
    }
}

static void assegna_ok_btn_cb(lv_event_t *e)
{
    lv_obj_t *p = (lv_obj_t *)lv_event_get_user_data(e);
    if (p)
        lv_obj_del(p);
    app_assegna_banchetto_close();
}

void visual_feedback_ok(void)
{
    lv_obj_t *obj = objects[s_current_idx].obj11;
    if (obj != NULL)
    {
        lv_obj_set_style_outline_color(obj, lv_color_hex(0x2ECC71), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_opa(obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_timer_t *timer = lv_timer_create(reset_panel_style, 500, obj);
        lv_timer_set_repeat_count(timer, 1);
    }
}

void myBeep(void)
{
    if (tagliatubi_manager_is_motor_busy()) return;  // sopprime audio durante ciclo tagliatubi

    char *path = BSP_SPIFFS_MOUNT_POINT "/music/beep.mp3";
    FILE *f = fopen(path, "rb");

    if (f)
    {
        if (audio_player_play(f) == ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            audio_player_stop(); // Questo ferma l'audio e chiude il file in sicurezza
        }
        else
        {
            ESP_LOGE("AUDIO", "Errore avvio riproduzione");
            fclose(f); // Chiudiamo a mano solo se il player fallisce l'avvio
        }
    }
    else
    {
        ESP_LOGE("AUDIO", "File non trovato: %s", path);
    }
}
// ═══════════════════════════════════════════════════════════
// JOURNAL HELPER
// ═══════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════
// JOURNAL HELPER (OTTIMIZZATO PER RISPARMIO RAM)
// ═══════════════════════════════════════════════════════════
static void journal_op(const char *op, uint8_t idx, const char *url,
                       int qta, const char *mat_override,
                       const char *nome, const char *cognome, const char *barcode)
{
    char ts[32] = "??:??:??";
    time_manager_get_ts(ts, sizeof(ts));

    banchetto_data_t *d = &s_list.items[idx];
    const char *mat = (mat_override && mat_override[0]) ? mat_override : d->matricola;
    const char *banc = d->banchetto;

    // Buffer ridotto e allocato con calloc per evitare spazzatura
    char *line = calloc(1, 1024);
    if (!line)
    {
        ESP_LOGE(TAG, "journal_op: calloc line fallito");
        return;
    }

    if (strcmp(op, "login") == 0)
    {
        snprintf(line, 1024,
                 "{\"ts\":\"%s\",\"op\":\"login\",\"idx\":%d,\"banchetto\":\"%s\","
                 "\"matricola\":\"%s\",\"nome\":\"%s\",\"cognome\":\"%s\",\"url\":\"%s\"}",
                 ts, idx, banc, mat, nome ? nome : "", cognome ? cognome : "", url ? url : "");
    }
    else if (strcmp(op, "logout") == 0)
    {
        snprintf(line, 1024,
                 "{\"ts\":\"%s\",\"op\":\"logout\",\"idx\":%d,\"banchetto\":\"%s\","
                 "\"matricola\":\"%s\",\"url\":\"%s\"}",
                 ts, idx, banc, mat, url ? url : "");
    }
    else if (strcmp(op, "versa") == 0 || strcmp(op, "scarto") == 0)
    {
        char ts_enc[32];
        snprintf(ts_enc, sizeof(ts_enc), "%s", ts);
        for (int i = 0; ts_enc[i]; i++)
            if (ts_enc[i] == ' ')
            {
                ts_enc[i] = '+';
                break;
            }

        char *url_salvati = calloc(1, 512); // Ridotto a 512 byte (più che sufficiente)
        if (!url_salvati)
        {
            ESP_LOGE(TAG, "journal_op: calloc url fallito");
            free(line);
            return;
        }

        snprintf(url_salvati, 512,
                 "%s?key=%s&comando=versa_salvati&qta=%d&banchetto=%s"
                 "&dataora=%s&fase=%s&operatore=%s&matr_scatola=%s&cod_art=%s&ord_prod=%lu&ciclo=%s",
                 SERVER_URL, device_key, qta, banc,
                 ts_enc, d->fase, mat, d->matr_scatola_corrente,
                 d->codice_articolo, (unsigned long)d->ord_prod, d->ciclo);

        snprintf(line, 1024,
                 "{\"ts\":\"%s\",\"op\":\"%s\",\"idx\":%d,\"banchetto\":\"%s\","
                 "\"matricola\":\"%s\",\"qta\":%d,\"url\":\"%s\"}",
                 ts, op, idx, banc, mat, qta, url_salvati);
        free(url_salvati);
    }
    else if (strcmp(op, "barcode") == 0)
    {
        snprintf(line, 1024,
                 "{\"ts\":\"%s\",\"op\":\"barcode\",\"idx\":%d,\"banchetto\":\"%s\","
                 "\"matricola\":\"%s\",\"barcode\":\"%s\",\"url\":\"%s\"}",
                 ts, idx, banc, mat, barcode ? barcode : "", url ? url : "");
    }
    else
    {
        free(line);
        return;
    }

    offline_journal_append(line);
    free(line);
}
// ═══════════════════════════════════════════════════════════
// STATE MACHINE E NAVIGAZIONE
// ═══════════════════════════════════════════════════════════
const char *banchetto_manager_get_banchetto_id(void)
{
    static char id[32] = {0};
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        if (s_list.count > 0)
            snprintf(id, sizeof(id), "%s", s_list.items[0].banchetto);
        else if (s_device_banchetto_id[0] != '\0')
            snprintf(id, sizeof(id), "%s", s_device_banchetto_id);
        xSemaphoreGive(data_mutex);
    }
    return id;
}

void banchetto_manager_set_state(banchetto_state_t state)
{
    s_state = state;
    const char *names[] = {"CHECKIN", "CONTEGGIO", "CONTROLLO", "ASSEGNA_BANCHETTO", "ATTESA_FORMATORE", "ATTESA_CONFERMA_FORMAZIONE"};
    ESP_LOGI(TAG, "Stato → %s", names[state]);
}

banchetto_state_t banchetto_manager_get_state(void)
{
    return s_state;
}

uint8_t banchetto_manager_get_current_index(void)
{
    return s_current_idx;
}

void banchetto_manager_set_current_index(uint8_t index)
{
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        if (index < s_list.count)
            s_current_idx = index;
        else
            ESP_LOGW(TAG, "set_current_index: indice %d fuori range (count=%d)", index, s_list.count);
        xSemaphoreGive(data_mutex);
    }
}

uint8_t banchetto_manager_get_count(void)
{
    uint8_t count = 0;
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        count = s_list.count;
        xSemaphoreGive(data_mutex);
    }
    return count;
}

// ═══════════════════════════════════════════════════════════
// INIT
// ═══════════════════════════════════════════════════════════

void banchetto_manager_init(void)
{
    ESP_LOGI(TAG, "Inizializzazione Manager");

    data_mutex = xSemaphoreCreateMutex();
    if (data_mutex == NULL)
    {
        ESP_LOGE(TAG, "Impossibile creare mutex");
        return;
    }

    memset(&s_list, 0, sizeof(banchetto_list_t));
    s_current_idx = 0;
    s_state = BANCHETTO_STATE_CHECKIN;

    esp_err_t ret = key_manager_get_key(device_key, sizeof(device_key));
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "Chiave dispositivo: %s", device_key);
    else
        ESP_LOGE(TAG, "Errore caricamento chiave");

    ESP_LOGI(TAG, "Manager pronto");
}

// ═══════════════════════════════════════════════════════════
// FETCH
// ═══════════════════════════════════════════════════════════

esp_err_t banchetto_manager_fetch_from_server(void)
{
    if (device_key[0] == '\0')
    {
        ESP_LOGE(TAG, "Chiave dispositivo non configurata");
        return ESP_FAIL;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s?key=%s&comando=inizializza", SERVER_URL, device_key);
    ESP_LOGI(TAG, "GET: %s", url);

    int response_code = 0;
    char *response_body = NULL;
    esp_err_t ret = http_get_request(url, &response_code, &response_body);

    if (ret != ESP_OK || response_code != 200)
    {
        ESP_LOGE(TAG, "Errore HTTP: %s (code: %d)", esp_err_to_name(ret), response_code);
        if (response_body)
            free(response_body);
        return ESP_FAIL;
    }
    if (!response_body)
    {
        ESP_LOGE(TAG, "Response body NULL");
        return ESP_FAIL;
    }

    FILE *cache_f = fopen(SD_CACHE_PATH, "w");
    if (cache_f)
    {
        fwrite(response_body, 1, strlen(response_body), cache_f);
        fclose(cache_f);
        ESP_LOGI(TAG, "Cache salvata su SD: %s", SD_CACHE_PATH);
    }
    else
    {
        ESP_LOGW(TAG, "Impossibile salvare cache su SD (SD assente?)");
    }

    // Salva ID banchetto del dispositivo prima di parsare (funziona anche senza ordini)
    {
        cJSON *j = cJSON_Parse(response_body);
        if (j) {
            cJSON *b = cJSON_GetObjectItem(j, "banchetto");
            if (b && cJSON_IsString(b) && b->valuestring[0] != '\0')
                snprintf(s_device_banchetto_id, sizeof(s_device_banchetto_id), "%s", b->valuestring);
            cJSON_Delete(j);
        }
    }

    static banchetto_list_t temp_list;
    memset(&temp_list, 0, sizeof(banchetto_list_t));
    ret = parse_banchetto_list(response_body, &temp_list);
    free(response_body);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Errore parsing JSON");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        // Preserva qta_prod_sessione locale se la sessione è aperta
        struct { char cod_art[32]; uint32_t qta_sess; bool aperta; } saved[BANCHETTO_MAX_ITEMS];
        int saved_count = s_list.count;
        for (int i = 0; i < saved_count; i++)
        {
            strncpy(saved[i].cod_art, s_list.items[i].codice_articolo, sizeof(saved[i].cod_art) - 1);
            saved[i].cod_art[sizeof(saved[i].cod_art) - 1] = '\0';
            saved[i].qta_sess = s_list.items[i].qta_prod_sessione;
            saved[i].aperta   = s_list.items[i].sessione_aperta;
        }

        memcpy(&s_list, &temp_list, sizeof(banchetto_list_t));

        for (int i = 0; i < s_list.count; i++)
        {
            for (int j = 0; j < saved_count; j++)
            {
                if (saved[j].aperta &&
                    strcmp(s_list.items[i].codice_articolo, saved[j].cod_art) == 0)
                {
                    s_list.items[i].qta_prod_sessione = saved[j].qta_sess;
                    break;
                }
            }
        }

        if (s_current_idx >= s_list.count)
            s_current_idx = 0;
        xSemaphoreGive(data_mutex);
        ESP_LOGI(TAG, "Fetch OK — %d articoli", s_list.count);
        web_server_broadcast_update();

        if (s_list.count > 0 && s_list.items[0].banchetto[0] != '\0')
            badge_cache_refresh(device_key, s_list.items[0].banchetto);

        return ESP_OK;
    }
    ESP_LOGE(TAG, "Timeout mutex");
    return ESP_FAIL;
}

// ═══════════════════════════════════════════════════════════
// RICOSTRUZIONE STATO DA JOURNAL
// ═══════════════════════════════════════════════════════════
void banchetto_manager_reconstruct_from_journal(void)
{
    FILE *f = fopen("/sdcard/offline_journal.jsonl", "r");
    if (!f)
    {
        ESP_LOGI(TAG, "Nessun journal da ricostruire");
        return;
    }

    ESP_LOGI(TAG, "Ricostruzione stato da journal...");
    char line[512];

    while (fgets(line, sizeof(line), f))
    {
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';
        if (strlen(line) < 5)
            continue;

        cJSON *j = cJSON_Parse(line);
        if (!j)
            continue;

        const char *op = cJSON_GetStringValue(cJSON_GetObjectItem(j, "op"));
        int idx = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(j, "idx"));
        if (!op)
        {
            cJSON_Delete(j);
            continue;
        }
        if (idx < 0 || idx >= s_list.count)
            idx = 0;

        if (strcmp(op, "login") == 0)
        {
            const char *mat = cJSON_GetStringValue(cJSON_GetObjectItem(j, "matricola")) ?: "";
            const char *nome = cJSON_GetStringValue(cJSON_GetObjectItem(j, "nome")) ?: "";
            const char *cog = cJSON_GetStringValue(cJSON_GetObjectItem(j, "cognome")) ?: "";
            for (int i = 0; i < s_list.count; i++)
            {
                snprintf(s_list.items[i].matricola, sizeof(s_list.items[i].matricola), "%s", mat);
                snprintf(s_list.items[i].operatore, sizeof(s_list.items[i].operatore), "%.30s %.30s", nome, cog);
                s_list.items[i].sessione_aperta = true;
                s_list.items[i].qta_prod_sessione = 0;
            }
        }
        else if (strcmp(op, "logout") == 0)
        {
            for (int i = 0; i < s_list.count; i++)
            {
                s_list.items[i].sessione_aperta = false;
                s_list.items[i].matricola[0] = '\0';
                s_list.items[i].operatore[0] = '\0';
                s_list.items[i].qta_prod_sessione = 0;
            }
        }
        else if (strcmp(op, "versa") == 0)
        {
            int qta = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(j, "qta"));
            for (int i = 0; i < s_list.count; i++)
            {
                uint32_t qta_reale = (uint32_t)qta * s_list.items[i].qta_pezzi;
                s_list.items[i].qta_scatola += qta_reale;
                s_list.items[i].qta_prod_fase += qta_reale;
                s_list.items[i].qta_prod_sessione += qta_reale;
            }
        }
        else if (strcmp(op, "barcode") == 0)
        {
            const char *bc = cJSON_GetStringValue(cJSON_GetObjectItem(j, "barcode")) ?: "";
            banchetto_data_t *d = &s_list.items[idx];
            if (strncmp(d->matr_scatola_corrente, bc, 31) != 0)
                d->qta_scatola = 0;
            snprintf(d->matr_scatola_corrente, sizeof(d->matr_scatola_corrente), "%s", bc);
        }

        cJSON_Delete(j);
    }
    fclose(f);
    ESP_LOGI(TAG, "Ricostruzione da journal completata");
}

// ═══════════════════════════════════════════════════════════
// PERIODIC REFRESH TASK
// ═══════════════════════════════════════════════════════════

static void periodic_refresh_task(void *pvParameters)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000)); // 1 ora
        if (is_online())
        {
            ESP_LOGI(TAG, "Refresh periodico cache SD...");
            banchetto_manager_fetch_from_server();
        }
        else
        {
            ESP_LOGW(TAG, "Refresh periodico saltato (offline)");
        }
    }
}

void banchetto_manager_start_periodic_refresh(void)
{
    xTaskCreate(periodic_refresh_task, "banchetto_refresh", 4096, NULL, 2, NULL);
}

// ═══════════════════════════════════════════════════════════
// LOAD FROM SD CACHE
// ═══════════════════════════════════════════════════════════

esp_err_t banchetto_manager_load_from_sd(void)
{
    FILE *f = fopen(SD_CACHE_PATH, "r");
    if (!f)
    {
        ESP_LOGE(TAG, "Nessuna cache SD disponibile: %s", SD_CACHE_PATH);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0)
    {
        ESP_LOGE(TAG, "Cache SD vuota o non leggibile");
        fclose(f);
        return ESP_FAIL;
    }

    char *buf = malloc(size + 1);
    if (!buf)
    {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t read = fread(buf, 1, size, f);
    fclose(f);
    buf[read] = '\0';

    if (read == 0)
    {
        ESP_LOGE(TAG, "Errore lettura cache SD");
        free(buf);
        return ESP_FAIL;
    }

    ESP_LOGI("SD_CACHE", "Risposta (%ld bytes):", read);
    for (size_t i = 0; i < read; i += 200)
    {
        ESP_LOGI("SD_CACHE", "%.*s", (int)((read - i) > 200 ? 200 : (read - i)), buf + i);
    }

    // Salva ID banchetto anche dalla cache (funziona senza ordini)
    {
        cJSON *j = cJSON_Parse(buf);
        if (j) {
            cJSON *b = cJSON_GetObjectItem(j, "banchetto");
            if (b && cJSON_IsString(b) && b->valuestring[0] != '\0')
                snprintf(s_device_banchetto_id, sizeof(s_device_banchetto_id), "%s", b->valuestring);
            cJSON_Delete(j);
        }
    }

    static banchetto_list_t temp_list;
    memset(&temp_list, 0, sizeof(banchetto_list_t));
    esp_err_t ret = parse_banchetto_list(buf, &temp_list);
    free(buf);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Errore parsing cache SD");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        memcpy(&s_list, &temp_list, sizeof(banchetto_list_t));
        if (s_current_idx >= s_list.count)
            s_current_idx = 0;
        xSemaphoreGive(data_mutex);
        ESP_LOGI(TAG, "Cache SD caricata — %d articoli", s_list.count);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Timeout mutex");
    return ESP_FAIL;
}

// ═══════════════════════════════════════════════════════════
// GET DATA
// ═══════════════════════════════════════════════════════════

bool banchetto_manager_get_data(banchetto_data_t *out_data)
{
    if (!out_data)
        return false;
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        if (s_list.count == 0)
        {
            xSemaphoreGive(data_mutex);
            return false;
        }
        memcpy(out_data, &s_list.items[s_current_idx], sizeof(banchetto_data_t));
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

bool banchetto_manager_get_item(uint8_t index, banchetto_data_t *out_data)
{
    if (!out_data)
        return false;
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        if (index >= s_list.count)
        {
            ESP_LOGW(TAG, "get_item: indice %d fuori range", index);
            xSemaphoreGive(data_mutex);
            return false;
        }
        memcpy(out_data, &s_list.items[index], sizeof(banchetto_data_t));
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════
// VERSA
// ═══════════════════════════════════════════════════════════

bool banchetto_manager_versa(uint32_t qta)
{
    if (!s_versa_abilitato) {
        ESP_LOGI(TAG, "Versa inibito (toggle OFF)");
        return true;
    }
    ESP_LOGI(TAG, "Versa: %lu", qta);

    if (!xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
        return false;

    if (!s_list.items[0].sessione_aperta)
    {
        ESP_LOGW(TAG, "Sessione NON aperta");
        xSemaphoreGive(data_mutex);
        if (lvgl_port_lock(0))
        {
            popup_avviso_open(LV_SYMBOL_WARNING " Timbratura mancante",
                              "Effettuare il login con\nil badge prima di continuare.",
                              !is_online());
            lvgl_port_unlock();
        }
        return false;
    }

    banchetto_data_t *cur = &s_list.items[s_current_idx];
    if (cur->matr_scatola_corrente[0] == '\0')
    {
        ESP_LOGW(TAG, "Scatola non impostata su articolo %d", s_current_idx);
        xSemaphoreGive(data_mutex);
        return false;
    }

    // Macchina manuale (1 ordine): blocca il versamento se la scatola è piena
    if (s_list.count == 1)
    {
        uint32_t qta_reale = qta * cur->qta_pezzi;
        if (cur->qta_scatola + qta_reale > cur->qta_totale_scatola)
        {
            ESP_LOGW(TAG, "Scatola piena (%"PRIu32"/%"PRIu32") — versamento bloccato", cur->qta_scatola, cur->qta_totale_scatola);
            xSemaphoreGive(data_mutex);
            if (lvgl_port_lock(pdMS_TO_TICKS(100)))
            {
                popup_avviso_open(LV_SYMBOL_WARNING " Contenitore Pieno",
                                  "Il contenitore e' pieno!\nCambiare il contenitore prima\ndi continuare.",
                                  !is_online());
                lvgl_port_unlock();
            }
            return false;
        }
    }

    xSemaphoreGive(data_mutex);

    char url[512];
    snprintf(url, sizeof(url), "%s?key=%s&comando=versa&qta=%lu", SERVER_URL, device_key, qta);

    if (!is_online())
    {
        ESP_LOGW(TAG, "OFFLINE — versa accodata: qta=%lu", qta);
        journal_op("versa", s_current_idx, url, (int)qta, NULL, NULL, NULL, NULL);
    }
    else
    {
        int response_code = 0;
        char *response_body = NULL;
        esp_err_t ret = http_get_request(url, &response_code, &response_body);
        if (response_body)
            free(response_body);

        if (ret != ESP_OK || response_code != 200)
        {
            s_server_reachable = false;
            ESP_LOGW(TAG, "Errore HTTP versa, fallback a OFFLINE");
            journal_op("versa", s_current_idx, url, (int)qta, NULL, NULL, NULL, NULL);
        }
        else
        {
            s_server_reachable = true;
        }
    }

    bool all_ok = true;
    char contenitori_pieni[256] = {0};

    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        for (int i = 0; i < s_list.count; i++)
        {
            banchetto_data_t *item = &s_list.items[i];
            uint32_t qta_reale = qta * item->qta_pezzi;

            item->qta_scatola += qta_reale;
            item->qta_prod_fase += qta_reale;
            item->qta_prod_sessione += qta_reale;
            item->qta_totale_giornaliera += qta_reale;

            if (item->qta_scatola >= item->qta_totale_scatola)
            {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%s%s", contenitori_pieni[0] ? "\n" : "", item->codice_articolo);
                if (strlen(contenitori_pieni) + strlen(tmp) < sizeof(contenitori_pieni))
                {
                    strcat(contenitori_pieni, tmp);
                }
            }

            ESP_LOGI(TAG, "[%d] Versamento OK — scatola:%lu/%lu fase:%lu sess:%lu",
                     i, item->qta_scatola, item->qta_totale_scatola,
                     item->qta_prod_fase, item->qta_prod_sessione);
        }
        xSemaphoreGive(data_mutex);
    }
    else
    {
        all_ok = false;
    }

    if (all_ok)
    {
        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            app_banchetto_update_page2();
            if (contenitori_pieni[0])
            {
                char msg[320];
                snprintf(msg, sizeof(msg), "Il contenitore del codice e' pieno!:\n%s\nCambiare il contenitore!", contenitori_pieni);
                popup_avviso_open(LV_SYMBOL_WARNING " Contenitore Pieno", msg, !is_online());
            }
            visual_feedback_ok(); // fix: chiamata con lock LVGL (era senza lock → race condition)
            lvgl_port_unlock();
        }
        deep_sleep_reset_timer();
        myBeep();
        web_server_broadcast_update();
    }

    return all_ok;
}

// ═══════════════════════════════════════════════════════════
// SCARTO
// ═══════════════════════════════════════════════════════════

bool banchetto_manager_scarto(uint32_t qta_scarti)
{
    ESP_LOGI(TAG, "Scarto [idx=%d]: %lu", s_current_idx, qta_scarti);

    if (qta_scarti == 0)
        return false;

    if (!xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
        return false;
    if (!s_list.items[0].sessione_aperta)
    {
        ESP_LOGW(TAG, "Sessione NON aperta");
        xSemaphoreGive(data_mutex);
        return false;
    }
    uint32_t ord_prod = s_list.items[s_current_idx].ord_prod;
    xSemaphoreGive(data_mutex);

    char url[256];
    snprintf(url, sizeof(url), "%s?key=%s&comando=scarto&qta=%lu&ord_prod=%lu", SERVER_URL, device_key, qta_scarti, ord_prod);

    if (!is_online())
    {
        ESP_LOGW(TAG, "OFFLINE — scarto accodato");
        journal_op("scarto", s_current_idx, url, (int)qta_scarti, NULL, NULL, NULL, NULL);
    }
    else
    {
        int response_code = 0;
        char *response_body = NULL;
        esp_err_t ret = http_get_request(url, &response_code, &response_body);

        if (ret != ESP_OK || response_code != 200)
        {
            if (response_body)
                free(response_body);
            s_server_reachable = false;
            ESP_LOGW(TAG, "Errore HTTP scarto, fallback a OFFLINE");
            journal_op("scarto", s_current_idx, url, (int)qta_scarti, NULL, NULL, NULL, NULL);
        }
        else
        {
            s_server_reachable = true;
            cJSON *json = cJSON_Parse(response_body);
            free(response_body);
            if (json)
            {
                cJSON *ok_item = cJSON_GetObjectItem(json, "OK");
                bool ok = false;
                if (ok_item)
                {
                    if (cJSON_IsNumber(ok_item))
                        ok = (ok_item->valueint > 0);
                    else if (cJSON_IsString(ok_item))
                        ok = (atoi(ok_item->valuestring) > 0);
                }
                cJSON_Delete(json);
                if (!ok)
                    return false;
            }
        }
    }

    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        banchetto_data_t *item = &s_list.items[s_current_idx];
        item->qta_prod_fase = (item->qta_prod_fase >= qta_scarti) ? item->qta_prod_fase - qta_scarti : 0;
        item->qta_scatola = (item->qta_scatola >= qta_scarti) ? item->qta_scatola - qta_scarti : 0;
        item->qta_totale_giornaliera = (item->qta_totale_giornaliera >= qta_scarti) ? item->qta_totale_giornaliera - qta_scarti : 0;
        item->qta_prod_sessione = (item->qta_prod_sessione >= qta_scarti) ? item->qta_prod_sessione - qta_scarti : 0;
        xSemaphoreGive(data_mutex);
    }

    if (lvgl_port_lock(pdMS_TO_TICKS(100)))
    {
        app_banchetto_update_page2();
        lvgl_port_unlock();
    }
    web_server_broadcast_update();

    return true;
}

// ═══════════════════════════════════════════════════════════
// SET BARCODE
// ═══════════════════════════════════════════════════════════

void banchetto_manager_set_barcode(const char *barcode)
{
    if (!barcode)
        return;

    ESP_LOGI(TAG, "Scatola [idx=%d]: %s", s_current_idx, barcode);

    char url[256];
    snprintf(url, sizeof(url), "%s?key=%s&comando=scatola&scatola=%s&ord_prod=%lu",
             SERVER_URL, device_key, barcode, s_list.items[s_current_idx].ord_prod);

    if (!is_online())
    {
        ESP_LOGW(TAG, "OFFLINE — scatola accodata");
        journal_op("barcode", s_current_idx, url, 0, NULL, NULL, NULL, barcode);

        if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
        {
            banchetto_data_t *item = &s_list.items[s_current_idx];
            if (strncmp(item->matr_scatola_corrente, barcode, 31) != 0)
                item->qta_scatola = 0;
            snprintf(item->matr_scatola_corrente, sizeof(item->matr_scatola_corrente), "%s", barcode);
            xSemaphoreGive(data_mutex);
        }
    }
    else
    {
        int response_code = 0;
        char *response_body = NULL;
        esp_err_t ret = http_get_request(url, &response_code, &response_body);

        if (ret == ESP_OK && response_code == 200 && response_body != NULL)
        {
            cJSON *json = cJSON_Parse(response_body);
            if (json)
            {
                cJSON *ok_item = cJSON_GetObjectItem(json, "OK");
                bool ok = (ok_item && cJSON_IsNumber(ok_item) && ok_item->valueint == 1);

                if (ok)
                {
                    cJSON *qta_scatola_item = cJSON_GetObjectItem(json, "qta_scatola");
                    uint32_t qta_scatola = (qta_scatola_item && cJSON_IsNumber(qta_scatola_item)) ? qta_scatola_item->valueint : 0;

                    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
                    {
                        banchetto_data_t *item = &s_list.items[s_current_idx];
                        snprintf(item->matr_scatola_corrente, sizeof(item->matr_scatola_corrente), "%s", barcode);
                        item->qta_scatola = qta_scatola;
                        xSemaphoreGive(data_mutex);
                    }
                }
                cJSON_Delete(json);
            }
        }
        else
        {
            ESP_LOGW(TAG, "Errore HTTP scatola, fallback a OFFLINE");
            journal_op("barcode", s_current_idx, url, 0, NULL, NULL, NULL, barcode);
            if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
            {
                banchetto_data_t *item = &s_list.items[s_current_idx];
                if (strncmp(item->matr_scatola_corrente, barcode, 31) != 0)
                    item->qta_scatola = 0;
                snprintf(item->matr_scatola_corrente, sizeof(item->matr_scatola_corrente), "%s", barcode);
                xSemaphoreGive(data_mutex);
            }
        }
        if (response_body)
            free(response_body);
    }

    if (lvgl_port_lock(pdMS_TO_TICKS(100)))
    {
        app_banchetto_update_page2();
        app_banchetto_nuova_scatola();
        lvgl_port_unlock();
    }
    web_server_broadcast_update();
}

// ═══════════════════════════════════════════════════════════
// LOGIN BADGE
// ═══════════════════════════════════════════════════════════
esp_err_t banchetto_manager_login_badge(const char *badge)
{
    if (!badge || strlen(badge) == 0)
        return ESP_FAIL;

    ESP_LOGW(TAG, "login_badge — stato attuale: %d", s_state);

    switch (s_state)
    {
    case BANCHETTO_STATE_CONTROLLO:
        banchetto_manager_set_state(BANCHETTO_STATE_CONTEGGIO);
        return banchetto_manager_controllo(badge);

    case BANCHETTO_STATE_ATTESA_FORMATORE:
        return banchetto_manager_formazione_formatore(badge);

    case BANCHETTO_STATE_ATTESA_CONFERMA_FORMAZIONE:
        return banchetto_manager_formazione_accettazione(badge);

    default:
        break;
    }

    ESP_LOGI(TAG, "Badge: %s", badge);

    char url[256];
    snprintf(url, sizeof(url), "%s?key=%s&comando=badge&badge=%s", SERVER_URL, device_key, badge);

    if (!is_online())
    {
        char mat[16] = {0};
        char n[48] = {0};
        char c[48] = {0};

        if (!badge_cache_find(badge, mat, sizeof(mat), n, sizeof(n), c, sizeof(c)))
        {
            if (lvgl_port_lock(pdMS_TO_TICKS(100)))
            {
                popup_avviso_open(LV_SYMBOL_WARNING " Badge non riconosciuto", "Badge non presente in cache.\nConnettiti alla rete almeno una volta.", true);
                lvgl_port_unlock();
            }
            return ESP_FAIL;
        }

        if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
        {
            bool same_op = s_list.items[0].sessione_aperta && atoi(s_list.items[0].matricola) == atoi(mat);
            if (same_op)
            {
                for (int i = 0; i < s_list.count; i++)
                {
                    s_list.items[i].sessione_aperta = false;
                    s_list.items[i].operatore[0] = '\0';
                    s_list.items[i].matricola[0] = '\0';
                    s_list.items[i].qta_prod_sessione = 0;
                }
                xSemaphoreGive(data_mutex);
                banchetto_manager_set_state(BANCHETTO_STATE_CHECKIN);
                journal_op("logout", 0, url, 0, mat, NULL, NULL, NULL);
                if (lvgl_port_lock(pdMS_TO_TICKS(100)))
                {
                    app_banchetto_update_page1();
                    app_banchetto_update_page2();
                    app_banchetto_update_page3();
                    lvgl_port_unlock();
                }
                myBeep();
                return ESP_OK;
            }
            xSemaphoreGive(data_mutex);
        }

        const char *cod_art = s_list.items[s_current_idx].codice_articolo;
        if (!badge_cache_is_formato(mat, cod_art))
        {
            if (lvgl_port_lock(pdMS_TO_TICKS(100)))
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "Non hai la formazione\nsul codice %s.\nChiama il responsabile.", cod_art);
                popup_formazione_open("Formazione mancante", msg);
                lvgl_port_unlock();
            }
            myBeep();
            vTaskDelay(pdMS_TO_TICKS(200));
            myBeep();
            return ESP_FAIL;
        }

        if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
        {
            for (int i = 0; i < s_list.count; i++)
            {
                snprintf(s_list.items[i].operatore, sizeof(s_list.items[i].operatore), "%.30s %.30s", n, c);
                snprintf(s_list.items[i].matricola, sizeof(s_list.items[i].matricola), "%s", mat);
                s_list.items[i].sessione_aperta = true;
                s_list.items[i].qta_prod_sessione = 0;
            }
            xSemaphoreGive(data_mutex);
        }

        journal_op("login", s_current_idx, url, 0, NULL, n, c, NULL);
        banchetto_manager_set_state(BANCHETTO_STATE_CONTEGGIO);

        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            app_banchetto_update_page1();
            app_banchetto_update_page2();
            app_banchetto_update_page3();
            lvgl_port_unlock();
        }
        myBeep();
        web_server_broadcast_update();
        return ESP_OK;
    }

    int response_code = 0;
    char *response_body = NULL;
    esp_err_t ret = http_get_request(url, &response_code, &response_body);

    if (ret != ESP_OK || response_code != 200)
    {
        if (response_body)
            free(response_body);
        return ESP_FAIL;
    }

    badge_response_t badge_resp;
    if (parse_badge_response(response_body, &badge_resp) != ESP_OK)
    {
        if (response_body)
            free(response_body);
        return ESP_FAIL;
    }

    if (badge_resp.errore[0] != '\0')
    {
        free(response_body);
        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            popup_avviso_open(LV_SYMBOL_WARNING " Badge non riconosciuto", badge_resp.errore, false);
            lvgl_port_unlock();
        }
        myBeep();
        return ESP_FAIL;
    }

    if (badge_resp.formazione == 999)
    {
        cJSON *json = cJSON_Parse(response_body);
        if (json)
        {
            cJSON *cod = cJSON_GetObjectItem(json, "cod_art");
            if (cod && cJSON_IsString(cod) && cod->valuestring)
            {
                snprintf(s_formazione_cod_art, sizeof(s_formazione_cod_art), "%s", cod->valuestring);
            }
            else
            {
                snprintf(s_formazione_cod_art, sizeof(s_formazione_cod_art), "sconosciuto");
            }
            cJSON_Delete(json);
        }
        free(response_body);

        snprintf(s_formazione_badge, sizeof(s_formazione_badge), "%s", badge);
        banchetto_manager_set_state(BANCHETTO_STATE_ATTESA_FORMATORE);

        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Non hai la formazione\nsul codice %s.\nChiama il responsabile.", s_formazione_cod_art);
            popup_formazione_open("Formazione mancante", msg);
            lvgl_port_unlock();
        }

        myBeep();
        vTaskDelay(pdMS_TO_TICKS(200));
        myBeep();
        return ESP_FAIL;
    }

    free(response_body);

    if (badge_resp.matricola == 0)
    {
        if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
        {
            for (int i = 0; i < s_list.count; i++)
            {
                s_list.items[i].sessione_aperta = false;
                s_list.items[i].operatore[0] = '\0';
                s_list.items[i].matricola[0] = '\0';
                s_list.items[i].qta_prod_sessione = 0;
            }
            xSemaphoreGive(data_mutex);
            banchetto_manager_set_state(BANCHETTO_STATE_CHECKIN);
            myBeep();
            if (lvgl_port_lock(pdMS_TO_TICKS(100)))
            {
                app_banchetto_update_page1();
                app_banchetto_update_page2();
                app_banchetto_update_page3();
                lvgl_port_unlock();
            }
            web_server_broadcast_update();
            return ESP_OK;
        }
        return ESP_FAIL;
    }

    if (!badge_resp.success)
    {
        myBeep();
        vTaskDelay(pdMS_TO_TICKS(200));
        myBeep();
        return ESP_FAIL;
    }

    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        for (int i = 0; i < s_list.count; i++)
        {
            snprintf(s_list.items[i].operatore, sizeof(s_list.items[i].operatore), "%s", badge_resp.operatore);
            snprintf(s_list.items[i].matricola, sizeof(s_list.items[i].matricola), "%d", badge_resp.matricola);
            s_list.items[i].sessione_aperta = true;
            s_list.items[i].qta_prod_sessione = 0;
        }
        xSemaphoreGive(data_mutex);
        banchetto_manager_set_state(BANCHETTO_STATE_CONTEGGIO);

        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            app_banchetto_update_page1();
            app_banchetto_update_page2();
            app_banchetto_update_page3();
            lvgl_port_unlock();
        }
        myBeep();
        web_server_broadcast_update();
        return ESP_OK;
    }
    return ESP_FAIL;
}

// ═══════════════════════════════════════════════════════════
// FORMAZIONE FORMATORE
// ═══════════════════════════════════════════════════════════

esp_err_t banchetto_manager_formazione_formatore(const char *badge)
{
    if (!badge || strlen(badge) == 0)
        return ESP_ERR_INVALID_ARG;

    char url[256];
    snprintf(url, sizeof(url), "%s?key=%s&comando=formazione_formatore&badge=%s", SERVER_URL, device_key, badge);

    int response_code = 0;
    char *body = NULL;
    esp_err_t ret = http_get_request(url, &response_code, &body);

    if (ret != ESP_OK || response_code != 200)
    {
        if (body)
            free(body);
        return ESP_FAIL;
    }

    bool ok = false;
    char msg_errore[64] = {0};

    cJSON *json = cJSON_Parse(body ? body : "");
    if (json)
    {
        cJSON *risp = cJSON_GetObjectItem(json, "risposta");
        if (risp && cJSON_IsString(risp) && risp->valuestring)
        {
            if (strcmp(risp->valuestring, "Ok del formatore") == 0)
                ok = true;
            else
                snprintf(msg_errore, sizeof(msg_errore), "%s", risp->valuestring);
        }
        cJSON_Delete(json);
    }
    if (body)
        free(body);

    if (ok)
    {
        banchetto_manager_set_state(BANCHETTO_STATE_ATTESA_CONFERMA_FORMAZIONE);
        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            popup_formazione_close();
            popup_formazione_open("Conferma formazione", "Dichiaro di avere ricevuto\nla formazione necessaria.\nConferma tramite badge.");
            lvgl_port_unlock();
        }
        myBeep();
        return ESP_OK;
    }
    else
    {
        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            popup_avviso_open("Non sei un formatore", strlen(msg_errore) > 0 ? msg_errore : "Badge non riconosciuto\ncome formatore.", false);
            lvgl_port_unlock();
        }
        myBeep();
        vTaskDelay(pdMS_TO_TICKS(200));
        myBeep();
        return ESP_FAIL;
    }
}

// ═══════════════════════════════════════════════════════════
// FORMAZIONE ACCETTAZIONE
// ═══════════════════════════════════════════════════════════

esp_err_t banchetto_manager_formazione_accettazione(const char *badge)
{
    if (!badge || strlen(badge) == 0)
        return ESP_ERR_INVALID_ARG;

    char url[256];
    snprintf(url, sizeof(url), "%s?key=%s&comando=formazione_accettazione&badge=%s", SERVER_URL, device_key, badge);

    int response_code = 0;
    char *body = NULL;
    esp_err_t ret = http_get_request(url, &response_code, &body);

    if (ret != ESP_OK || response_code != 200)
    {
        if (body)
            free(body);
        return ESP_FAIL;
    }

    bool ok = false;
    char msg_errore[64] = {0};

    cJSON *json = cJSON_Parse(body ? body : "");
    if (json)
    {
        cJSON *risp = cJSON_GetObjectItem(json, "risposta");
        if (risp && cJSON_IsString(risp) && risp->valuestring)
        {
            if (strcmp(risp->valuestring, "Formazione registrata") == 0)
                ok = true;
            else
                snprintf(msg_errore, sizeof(msg_errore), "%s", risp->valuestring);
        }
        cJSON_Delete(json);
    }
    if (body)
        free(body);

    if (ok)
    {
        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            popup_formazione_close();
            lvgl_port_unlock();
        }
        banchetto_manager_set_state(BANCHETTO_STATE_CHECKIN);
        return banchetto_manager_login_badge(s_formazione_badge);
    }
    else
    {
        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            popup_avviso_open("Errore formazione", strlen(msg_errore) > 0 ? msg_errore : "Formazione non registrata.", false);
            lvgl_port_unlock();
        }
        myBeep();
        vTaskDelay(pdMS_TO_TICKS(200));
        myBeep();
        return ESP_FAIL;
    }
}

// ═══════════════════════════════════════════════════════════
// CONTROLLO
// ═══════════════════════════════════════════════════════════
esp_err_t banchetto_manager_controllo(const char *badge)
{
    if (!badge || strlen(badge) == 0)
        return ESP_ERR_INVALID_ARG;

    banchetto_data_t d;
    if (!banchetto_manager_get_data(&d))
        return ESP_FAIL;

    char url[300];
    snprintf(url, sizeof(url), "%s?key=%s&comando=controllo&badge=%s&ord_prod=%lu", SERVER_URL, device_key, badge, (unsigned long)d.ord_prod);

    int response_code = 0;
    char *body = NULL;
    esp_err_t ret = http_get_request(url, &response_code, &body);

    if (ret != ESP_OK || response_code != 200)
    {
        if (body)
            free(body);
        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            popup_controllo_close();
            popup_avviso_open(LV_SYMBOL_WARNING " Errore", "Impossibile contattare il server.", !is_online());
            lvgl_port_unlock();
        }
        return ESP_FAIL;
    }

    bool ok = false;
    char msg_buf[128] = "Operazione completata.";

    cJSON *json = cJSON_Parse(body ? body : "");
    if (json)
    {
        cJSON *err_item = cJSON_GetObjectItem(json, "errore");
        cJSON *op_item = cJSON_GetObjectItem(json, "operatore");
        cJSON *mat_item = cJSON_GetObjectItem(json, "matricola");

        if (err_item && cJSON_IsString(err_item) && err_item->valuestring[0])
        {
            ok = false;
            snprintf(msg_buf, sizeof(msg_buf), "%s", err_item->valuestring);
        }
        else if (op_item && mat_item)
        {
            ok = true;
            snprintf(msg_buf, sizeof(msg_buf), "Operazione completata.\n%s\nMatricola: %d",
                     cJSON_IsString(op_item) ? op_item->valuestring : "",
                     cJSON_IsNumber(mat_item) ? mat_item->valueint : 0);
        }
        cJSON_Delete(json);
    }
    if (body)
        free(body);

    if (lvgl_port_lock(pdMS_TO_TICKS(100)))
    {
        popup_controllo_close();
        popup_avviso_open(ok ? LV_SYMBOL_OK " Controllo OK" : LV_SYMBOL_WARNING " Controllo rifiutato", msg_buf, false);
        lvgl_port_unlock();
    }
    return ok ? ESP_OK : ESP_FAIL;
}

// ═══════════════════════════════════════════════════════════
// ASSEGNA BANCHETTO
// ═══════════════════════════════════════════════════════════

esp_err_t banchetto_manager_assegna_banchetto(const char *barcode)
{
    if (!barcode || strlen(barcode) == 0)
        return ESP_ERR_INVALID_ARG;

    char url[256];
    snprintf(url, sizeof(url), "%s?key=%s&comando=assegna&banchetto=%s", SERVER_URL, device_key, barcode);

    int response_code = 0;
    char *body = NULL;
    esp_err_t ret = http_get_request(url, &response_code, &body);

    if (ret != ESP_OK || response_code != 200)
    {
        if (body)
            free(body);
        if (lvgl_port_lock(pdMS_TO_TICKS(100)))
        {
            popup_avviso_open(LV_SYMBOL_WARNING " Errore", "Impossibile contattare il server.", !is_online());
            lvgl_port_unlock();
        }
        return ESP_FAIL;
    }

    char msg_buf[128] = {0};
    bool ok = false;

    cJSON *json = cJSON_Parse(body ? body : "");
    if (json)
    {
        cJSON *ok_item = cJSON_GetObjectItem(json, "OK");
        cJSON *banc_item = cJSON_GetObjectItem(json, "banchetto");
        cJSON *err_item = cJSON_GetObjectItem(json, "errore");

        // Salva sempre l'ID banchetto del dispositivo, anche in caso di errore
        if (banc_item && cJSON_IsString(banc_item) && banc_item->valuestring[0] != '\0')
            snprintf(s_device_banchetto_id, sizeof(s_device_banchetto_id), "%s", banc_item->valuestring);

        if (ok_item && cJSON_IsNumber(ok_item) && ok_item->valueint == 1)
        {
            ok = true;
            snprintf(msg_buf, sizeof(msg_buf), "Banchetto: %s\nInserire ordine per continuare",
                     (banc_item && cJSON_IsString(banc_item)) ? banc_item->valuestring : barcode);

            if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
            {
                for (int i = 0; i < s_list.count; i++)
                {
                    snprintf(s_list.items[i].banchetto, sizeof(s_list.items[i].banchetto), "%s",
                             (banc_item && cJSON_IsString(banc_item)) ? banc_item->valuestring : barcode);
                }
                xSemaphoreGive(data_mutex);
            }
        }
        else
        {
            snprintf(msg_buf, sizeof(msg_buf), "%s", (err_item && cJSON_IsString(err_item)) ? err_item->valuestring : "Errore sconosciuto");
        }
        cJSON_Delete(json);
    }
    if (body)
        free(body);

    if (lvgl_port_lock(pdMS_TO_TICKS(100)))
    {
        if (ok)
        {
            lv_obj_t *p = lv_obj_create(lv_scr_act());
            lv_obj_set_size(p, 520, 300);
            lv_obj_center(p);
            lv_obj_move_foreground(p);
            lv_obj_set_style_bg_color(p, lv_color_hex(0x141E30), 0);
            lv_obj_set_style_bg_opa(p, 255, 0);
            lv_obj_set_style_border_color(p, lv_color_hex(0x2ECC71), 0);
            lv_obj_set_style_border_width(p, 2, 0);
            lv_obj_set_style_radius(p, 12, 0);
            lv_obj_set_style_pad_all(p, 16, 0);
            lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t *tit = lv_label_create(p);
            lv_label_set_text(tit, LV_SYMBOL_OK " Assegnato");
            lv_obj_set_style_text_font(tit, &lv_font_montserrat_30, 0);
            lv_obj_set_style_text_color(tit, lv_color_hex(0x2ECC71), 0);
            lv_obj_set_width(tit, LV_PCT(100));
            lv_obj_set_style_text_align(tit, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(tit, LV_ALIGN_TOP_MID, 0, 0);

            lv_obj_t *lbl = lv_label_create(p);
            lv_label_set_text(lbl, msg_buf);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_26, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_width(lbl, LV_PCT(100));
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -10);

            lv_obj_t *btn = lv_btn_create(p);
            lv_obj_set_size(btn, 120, 44);
            lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2C5282), 0);
            lv_obj_set_style_radius(btn, 8, 0);
            lv_obj_add_event_cb(btn, assegna_ok_btn_cb, LV_EVENT_CLICKED, p);

            lv_obj_t *btn_lbl = lv_label_create(btn);
            lv_label_set_text(btn_lbl, "OK");
            lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_30, 0);
            lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(btn_lbl);

            myBeep();
        }
        else
        {
            popup_avviso_open(LV_SYMBOL_WARNING " Errore", msg_buf, false);
        }
        lvgl_port_unlock();
    }
    return ok ? ESP_OK : ESP_FAIL;
}

// ═══════════════════════════════════════════════════════════
// BADGE DEZ→HEX
// ═══════════════════════════════════════════════════════════

static esp_err_t badge_dez_to_hex_inline(const char *dez, char *hex_out, size_t hex_size)
{
    if (!dez || !hex_out || hex_size < 11 || strlen(dez) != 10)
        return ESP_ERR_INVALID_ARG;

    char dez_full[21];
    snprintf(dez_full, sizeof(dez_full), "0000000000%s", dez);

    char unique_id1[11] = {0};
    for (int i = 0; i < 10; i++)
    {
        char pair[3] = {dez_full[i * 2], dez_full[i * 2 + 1], 0};
        snprintf(&unique_id1[i], 2, "%X", atoi(pair));
    }

    unsigned long long uid_dec = strtoull(unique_id1, NULL, 16);

    char bin_uid1[41] = {0};
    for (int i = 0; i < 40; i++)
        bin_uid1[39 - i] = ((uid_dec >> i) & 1) ? '1' : '0';

    char bin_original[41] = {0};
    for (int g = 0; g < 10; g++)
        for (int b = 0; b < 4; b++)
            bin_original[g * 4 + b] = bin_uid1[g * 4 + (3 - b)];

    for (int i = 0; i < 10; i++)
    {
        int val = 0;
        for (int j = 0; j < 4; j++)
            val = (val << 1) | (bin_original[i * 4 + j] - '0');
        snprintf(&hex_out[i], 2, "%X", val);
    }
    hex_out[10] = '\0';
    return ESP_OK;
}

// ═══════════════════════════════════════════════════════════
// LOGIN BY MATRICOLA
// ═══════════════════════════════════════════════════════════

esp_err_t banchetto_manager_login_by_matricola(uint16_t matricola)
{
    char url[256];
    snprintf(url, sizeof(url), "%s?operatore=%d", SERVER_BADGE_URL, matricola);

    int response_code = 0;
    char *response_body = NULL;
    esp_err_t ret = http_get_request(url, &response_code, &response_body);

    if (ret != ESP_OK || response_code != 200 || !response_body || strlen(response_body) == 0)
    {
        if (response_body)
            free(response_body);
        return ESP_FAIL;
    }

    char badge_dez[32];
    snprintf(badge_dez, sizeof(badge_dez), "%s", response_body);
    free(response_body);

    char *end = badge_dez + strlen(badge_dez) - 1;
    while (end > badge_dez && (*end == ' ' || *end == '\n' || *end == '\r'))
        *end-- = '\0';

    char badge_hex[16];
    if (badge_dez_to_hex_inline(badge_dez, badge_hex, sizeof(badge_hex)) != ESP_OK)
        return ESP_FAIL;

    return banchetto_manager_login_badge(badge_hex);
}

// ═══════════════════════════════════════════════════════════
// UTILITY
// ═══════════════════════════════════════════════════════════

void banchetto_manager_print_status(void)
{
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        ESP_LOGI(TAG, "Articoli: %d | CurrentIdx: %d | Stato: %d", s_list.count, s_current_idx, s_state);
        xSemaphoreGive(data_mutex);
    }
}

bool banchetto_manager_is_ready(void)
{
    bool ready = false;
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        if (s_list.count > 0)
        {
            banchetto_data_t *cur = &s_list.items[s_current_idx];
            ready = cur->sessione_aperta && (cur->matr_scatola_corrente[0] != '\0') && (cur->operatore[0] != '\0' || cur->matricola[0] != '\0');
        }
        xSemaphoreGive(data_mutex);
    }
    return ready;
}

void banchetto_manager_reset_data(void)
{
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(1000)))
    {
        memset(&s_list, 0, sizeof(banchetto_list_t));
        s_current_idx = 0;
        xSemaphoreGive(data_mutex);
    }
}