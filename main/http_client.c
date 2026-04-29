#include "http_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
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