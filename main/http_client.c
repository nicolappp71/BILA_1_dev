#include "http_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#include "mode.h"

static const char *TAG = "HTTP_CLIENT";

// Ridotto timeout per maggiore reattività
#define HTTP_TIMEOUT_MS 3000

/**
 * Struttura per gestire la risposta in modo dinamico e thread-safe
 */
typedef struct
{
    char *buffer;
    int len;
} http_response_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_t *res = (http_response_t *)evt->user_data;

    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            int new_len = res->len + evt->data_len;
            char *new_ptr = realloc(res->buffer, new_len + 1);
            if (new_ptr)
            {
                res->buffer = new_ptr;
                memcpy(res->buffer + res->len, evt->data, evt->data_len);
                res->len = new_len;
                res->buffer[res->len] = '\0';
            }
            else
            {
                ESP_LOGE(TAG, "MALLOC fallita nel ricezione dati");
            }
        }
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t http_get_request(const char *url, int *response_code, char **response_body)
{
    if (!url || !response_code || !response_body)
        return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "🌐 HTTP GET: %s", url);

    http_response_t res = {.buffer = NULL, .len = 0};

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &res,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .method = HTTP_METHOD_GET,
        .disable_auto_redirect = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Impossibile inizializzare client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        *response_code = esp_http_client_get_status_code(client);
        if (res.buffer)
        {
            *response_body = res.buffer; // Passiamo la proprietà della memoria al chiamante
            ESP_LOGI(TAG, "Risposta GET: %s", *response_body);
        }
        else
        {
            *response_body = NULL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "✗ HTTP GET fallito: %s", esp_err_to_name(err));
        if (res.buffer)
            free(res.buffer);
        *response_body = NULL;
        *response_code = 0;
    }

    esp_http_client_cleanup(client);
    return err;
}

esp_err_t http_post_request(const char *url, const char *post_data, int *response_code, char **response_body)
{
    if (!url || !post_data || !response_code || !response_body)
        return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "🌐 HTTP POST: %s", url);

    http_response_t res = {.buffer = NULL, .len = 0};

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &res,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
        return ESP_FAIL;

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        *response_code = esp_http_client_get_status_code(client);
        *response_body = res.buffer;

        if (*response_body)
        {
            ESP_LOGI(TAG, "Risposta POST: %s", *response_body);
        }
    }
    else
    {
        ESP_LOGE(TAG, "✗ HTTP POST fallito");
        if (res.buffer)
            free(res.buffer);
        *response_body = NULL;
        *response_code = 0;
    }

    esp_http_client_cleanup(client);
    return err;
}

// ─── Download file grande in PSRAM ───────────────────────────────────────────
// Usa perform() (più efficiente su SDIO hosted WiFi) con buffer pre-allocato
// in PSRAM — niente realloc incrementali (causa originale dei download lenti).

typedef struct {
    char   *buf;
    size_t  capacity;
    size_t  len;
    size_t  next_log;
} psram_buf_t;

static esp_err_t file_event_handler(esp_http_client_event_t *evt)
{
    psram_buf_t *p = (psram_buf_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (p->len + (size_t)evt->data_len > p->capacity) {
            ESP_LOGE("HTTP_FILE", "Buffer PSRAM esaurito (%zu/%zu)", p->len, p->capacity);
            return ESP_FAIL;
        }
        memcpy(p->buf + p->len, evt->data, evt->data_len);
        p->len += (size_t)evt->data_len;
        // Log progresso ogni 256 KB
        if (p->len >= p->next_log) {
            ESP_LOGI("HTTP_FILE", "  ... %zu KB ricevuti", p->len / 1024);
            p->next_log += 256 * 1024;
        }
    }
    return ESP_OK;
}

esp_err_t http_get_file_psram(const char *url, char **out_buf, size_t *out_len)
{
    if (!url || !out_buf || !out_len) return ESP_ERR_INVALID_ARG;

    // Pre-alloca 6 MB di PSRAM in un colpo solo (nessun realloc durante il download)
    const size_t MAX_SIZE = 6u * 1024 * 1024;
    char *buf = (char *)heap_caps_malloc(MAX_SIZE + 1, MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_LOGE("HTTP_FILE", "PSRAM malloc fallita (%zu byte)", MAX_SIZE);
        return ESP_ERR_NO_MEM;
    }

    psram_buf_t p = { .buf = buf, .capacity = MAX_SIZE, .len = 0, .next_log = 256*1024 };

    esp_http_client_config_t cfg = {
        .url           = url,
        .event_handler = file_event_handler,
        .user_data     = &p,
        .timeout_ms    = 120000,
        .method        = HTTP_METHOD_GET,
        .buffer_size   = 4096,
    };

    ESP_LOGI("HTTP_FILE", "Inizio download: %s", url);
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) { free(buf); return ESP_FAIL; }

    esp_err_t err = esp_http_client_perform(client);
    int code      = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || code != 200) {
        ESP_LOGE("HTTP_FILE", "Errore: err=%s code=%d", esp_err_to_name(err), code);
        free(buf);
        return ESP_FAIL;
    }
    if (p.len == 0) {
        ESP_LOGE("HTTP_FILE", "Nessun dato ricevuto");
        free(buf);
        return ESP_FAIL;
    }

    buf[p.len] = '\0';
    ESP_LOGI("HTTP_FILE", "Download OK: %zu byte", p.len);
    *out_buf = buf;
    *out_len = p.len;
    return ESP_OK;
}

esp_err_t http_get_server_time(int *ore, int *minuti)
{
    char url[128];
    snprintf(url, sizeof(url), "%s/iot/orario.php", SERVER_BASE);

    int response_code = 0;
    char *response_body = NULL;

    esp_err_t err = http_get_request(url, &response_code, &response_body);

    if (err == ESP_OK && response_body != NULL)
    {
        char *p = strchr(response_body, ':');
        if (p != NULL && (p - response_body) >= 2)
        {
            *ore = atoi(p - 2);
            *minuti = atoi(p + 1);
            free(response_body);
            return ESP_OK;
        }
        free(response_body);
    }
    return ESP_FAIL;
}