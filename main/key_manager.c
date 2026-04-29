#include "key_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>

static const char *TAG = "KEY_MANAGER";
static char device_key[KEY_LENGTH + 1] = {0};
static bool key_initialized = false;

/**
 * @brief Genera una chiave alfanumerica casuale (0-9, A-Z)
 * Equivalente alla funzione Arduino createKey()
 */
static esp_err_t generate_key(char *key_buffer, size_t buffer_size)
{
    if (buffer_size < KEY_LENGTH + 1) {
        ESP_LOGE(TAG, "Buffer troppo piccolo");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Generazione nuova chiave univoca...");
    
    for (int i = 0; i < KEY_LENGTH; i++) {
        // Genera numero casuale tra 48 e 82 (equivalente Arduino: 48-83)
        // 48-57 = '0'-'9'
        // 65-90 = 'A'-'Z' (dopo l'aggiustamento)
        uint32_t rand_num = esp_random() % 35;  // 0-34
        char c = rand_num + 48;  // 48-82
        
        // Se > 57 (dopo '9'), aggiungi 7 per arrivare ad 'A'-'Z'
        if (c > 57) {
            c += 7;
        }
        
        key_buffer[i] = c;
    }
    
    key_buffer[KEY_LENGTH] = '\0';
    
    ESP_LOGI(TAG, "✓ Chiave generata: %s", key_buffer);
    return ESP_OK;
}

/**
 * @brief Salva la chiave in NVS
 */
static esp_err_t save_key_to_nvs(const char *key)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    // Apri NVS in modalità scrittura
    err = nvs_open(KEY_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ Errore apertura NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    // Scrivi la chiave
    err = nvs_set_str(nvs_handle, KEY_NVS_KEY, key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ Errore scrittura chiave: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    // Commit delle modifiche
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ Errore commit NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "✓ Chiave salvata in NVS");
    }
    
    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief Legge la chiave da NVS
 */
static esp_err_t load_key_from_nvs(char *key_buffer, size_t buffer_size)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    // Apri NVS in modalità lettura
    err = nvs_open(KEY_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS non accessibile: %s", esp_err_to_name(err));
        return err;
    }
    
    // Leggi la chiave
    size_t required_size = buffer_size;
    err = nvs_get_str(nvs_handle, KEY_NVS_KEY, key_buffer, &required_size);
    
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✓ Chiave caricata da NVS: %s", key_buffer);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "⚠ Chiave non trovata in NVS");
    } else {
        ESP_LOGE(TAG, "✗ Errore lettura chiave: %s", esp_err_to_name(err));
    }
    
    return err;
}

esp_err_t key_manager_init(void)
{
    esp_err_t err;
    
    ESP_LOGI(TAG, "Inizializzazione Key Manager...");
    
    // Prova a caricare la chiave esistente da NVS
    err = load_key_from_nvs(device_key, sizeof(device_key));
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Chiave non esiste, generane una nuova
        ESP_LOGI(TAG, "Prima inizializzazione - generazione chiave...");
        
        err = generate_key(device_key, sizeof(device_key));
        if (err != ESP_OK) {
            return err;
        }
        
        // Salva in NVS
        err = save_key_to_nvs(device_key);
        if (err != ESP_OK) {
            return err;
        }
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ Errore inizializzazione");
        return err;
    }
    
    key_initialized = true;
    ESP_LOGI(TAG, "✓ Key Manager inizializzato");
    ESP_LOGI(TAG, "📌 %s", device_key);
    
    return ESP_OK;
}

esp_err_t key_manager_get_key(char *key_buffer, size_t buffer_size)
{
    if (!key_initialized) {
        ESP_LOGE(TAG, "✗ Key Manager non inizializzato");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (buffer_size < KEY_LENGTH + 1) {
        ESP_LOGE(TAG, "✗ Buffer troppo piccolo");
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(key_buffer, device_key, buffer_size);
    return ESP_OK;
}

esp_err_t key_manager_generate_new_key(char *key_buffer, size_t buffer_size)
{
    esp_err_t err;
    
    ESP_LOGW(TAG, "⚠ Generazione NUOVA chiave (sovrascrive quella esistente)");
    
    err = generate_key(device_key, sizeof(device_key));
    if (err != ESP_OK) {
        return err;
    }
    
    err = save_key_to_nvs(device_key);
    if (err != ESP_OK) {
        return err;
    }
    
    key_initialized = true;
    
    if (key_buffer && buffer_size >= KEY_LENGTH + 1) {
        strncpy(key_buffer, device_key, buffer_size);
    }
    
    return ESP_OK;
}

esp_err_t key_manager_erase_key(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    ESP_LOGW(TAG, "⚠ Cancellazione chiave da NVS");
    
    err = nvs_open(KEY_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ Errore apertura NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_erase_key(nvs_handle, KEY_NVS_KEY);
    if (err == ESP_OK) {
        nvs_commit(nvs_handle);
        ESP_LOGI(TAG, "✓ Chiave cancellata");
        memset(device_key, 0, sizeof(device_key));
        key_initialized = false;
    } else {
        ESP_LOGE(TAG, "✗ Errore cancellazione: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}