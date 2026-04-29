#include "ota_manager.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"

static const char *TAG = "OTA_MANAGER";

//#define OTA_URL "http://192.168.1.58/ota/MULTI_ORDINI_BLE.bin"
#define OTA_URL "http://172.18.2.254/ota/MULTI_ORDINI_BLE.bin"


/* Flag globale: quando true, gli altri task non fanno chiamate HTTP */
volatile bool ota_in_progress = false;

esp_err_t ota_manager_start(void)
{
    ESP_LOGW(TAG, "OTA in corso — blocco altre chiamate HTTP");
    ota_in_progress = true;

    /* Aspetta che eventuali chiamate HTTP in corso finiscano */
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Avvio aggiornamento da: %s", OTA_URL);

    esp_http_client_config_t http_config = {
        .url = OTA_URL,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
        .partial_http_download = true,
    };

    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "OTA riuscito! Riavvio...");
        esp_restart();
    }
    else
    {
        ESP_LOGE(TAG, "Errore OTA: %s (0x%x)", esp_err_to_name(ret), ret);
        ota_in_progress = false;
    }
    return ret;
}