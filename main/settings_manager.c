// #include "settings_manager.h"
// #include "esp_log.h"
// #include "nvs_flash.h"
// #include "nvs.h"
// #include "bsp_board_extra.h"
// #include "bsp/esp-bsp.h" // Aggiungi questo se non c'è
// static const char *TAG = "SETTINGS";
// static const char *NVS_NAMESPACE = "settings";

// // Variabili globali settings
// static uint8_t audio_volume = 100; // Default produzione

// void settings_init(void)
// {
//     ESP_LOGI(TAG, "Inizializzazione settings manager");
    
//     // Inizializza NVS se non fatto
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_LOGW(TAG, "NVS partition corrotta, cancello...");
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);
    
//     // Apri NVS
//     nvs_handle_t nvs;
//     ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    
//     if (ret == ESP_OK) {
//         // Leggi volume salvato
//         uint8_t saved_volume;
//         ret = nvs_get_u8(nvs, "volume", &saved_volume);
        
//         if (ret == ESP_OK) {
//             audio_volume = saved_volume;
//             ESP_LOGI(TAG, "Volume caricato da NVS: %d", audio_volume);
//         } else {
//             ESP_LOGI(TAG, "Volume non trovato in NVS, uso default: %d", audio_volume);
//         }
        
//         nvs_close(nvs);
//     } else {
//         ESP_LOGI(TAG, "Primo avvio, uso volume default: %d", audio_volume);
//     }
// }

// uint8_t settings_get_volume(void)
// {
//     return audio_volume;
// }

// esp_err_t settings_set_volume(uint8_t volume)
// {
//     if (volume > 100) {
//         ESP_LOGE(TAG, "Volume invalido: %d (max 100)", volume);
//         return ESP_ERR_INVALID_ARG;
//     }
    
//     audio_volume = volume;

//     // --- AGGIUNTA: Applica il volume all'hardware immediatamente ---
// // Sostituisci la riga che dà errore con questa:
// bsp_extra_codec_volume_set(audio_volume, NULL);    // --------------------------------------------------------------
    
//     // Salva in NVS (il codice che avevi già va bene)
//     nvs_handle_t nvs;
//     esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    
//     if (ret == ESP_OK) {
//         nvs_set_u8(nvs, "volume", volume);
//         nvs_commit(nvs);
//         nvs_close(nvs);
//         ESP_LOGI(TAG, "✓ Volume %d applicato e salvato", volume);
//     }
    
//     return ret;
// }
#include "settings_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "bsp_board_extra.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "SETTINGS";
static const char *NVS_NAMESPACE = "settings";

static uint8_t audio_volume = 100; // Default

void settings_init(void)
{
    ESP_LOGI(TAG, "Inizializzazione settings manager");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition corrotta, cancello...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_handle_t nvs;
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);

    if (ret == ESP_OK) {
        uint8_t saved_volume;
        ret = nvs_get_u8(nvs, "volume", &saved_volume);
        if (ret == ESP_OK) {
            audio_volume = saved_volume;
            ESP_LOGI(TAG, "Volume caricato da NVS: %d", audio_volume);
        } else {
            ESP_LOGI(TAG, "Volume non trovato in NVS, uso default: %d", audio_volume);
        }
        nvs_close(nvs);
    } else {
        ESP_LOGI(TAG, "Primo avvio, uso volume default: %d", audio_volume);
    }

    // Applica il volume all'hardware subito dopo il caricamento
    bsp_extra_codec_volume_set(audio_volume, NULL);
    ESP_LOGI(TAG, "✓ Volume %d applicato all'hardware", audio_volume);
}

uint8_t settings_get_volume(void)
{
    return audio_volume;
}

esp_err_t settings_set_volume(uint8_t volume)
{
    if (volume > 100) {
        ESP_LOGE(TAG, "Volume invalido: %d (max 100)", volume);
        return ESP_ERR_INVALID_ARG;
    }

    audio_volume = volume;
    bsp_extra_codec_volume_set(audio_volume, NULL);

    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (ret == ESP_OK) {
        nvs_set_u8(nvs, "volume", volume);
        nvs_commit(nvs);
        nvs_close(nvs);
        ESP_LOGI(TAG, "✓ Volume %d applicato e salvato", volume);
    }

    return ret;
}