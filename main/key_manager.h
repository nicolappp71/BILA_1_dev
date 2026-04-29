#ifndef KEY_MANAGER_H
#define KEY_MANAGER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KEY_LENGTH 16
#define KEY_NVS_NAMESPACE "storage"
#define KEY_NVS_KEY "device_key"

/**
 * @brief Inizializza il gestore chiavi
 * Se la chiave non esiste in NVS, ne crea una nuova
 * 
 * @return esp_err_t ESP_OK se successo
 */
esp_err_t key_manager_init(void);

/**
 * @brief Ottiene la chiave univoca del dispositivo
 * 
 * @param key_buffer Buffer dove copiare la chiave (minimo 17 bytes per null terminator)
 * @param buffer_size Dimensione del buffer
 * @return esp_err_t ESP_OK se successo
 */
esp_err_t key_manager_get_key(char *key_buffer, size_t buffer_size);

/**
 * @brief Genera una nuova chiave e la salva in NVS (sovrascrive quella esistente)
 * 
 * @param key_buffer Buffer dove copiare la nuova chiave (minimo 17 bytes)
 * @param buffer_size Dimensione del buffer
 * @return esp_err_t ESP_OK se successo
 */
esp_err_t key_manager_generate_new_key(char *key_buffer, size_t buffer_size);

/**
 * @brief Cancella la chiave dalla NVS
 * 
 * @return esp_err_t ESP_OK se successo
 */
esp_err_t key_manager_erase_key(void);

#ifdef __cplusplus
}
#endif

#endif // KEY_MANAGER_H