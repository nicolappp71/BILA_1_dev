#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Invia una richiesta HTTP GET al server
 * 
 * @param url URL completo (es: "http://192.168.1.100/api/event")
 * @param response_code Output: codice di risposta HTTP (es: 200, 404)
 * @param response_body Output: corpo della risposta (JSON), allocato dinamicamente - ricorda di fare free()
 * @return esp_err_t ESP_OK se successo
 */
esp_err_t http_get_request(const char *url, int *response_code, char **response_body);

/**
 * @brief Invia una richiesta HTTP POST al server
 * 
 * @param url URL completo
 * @param post_data Dati da inviare (JSON, form-data, etc)
 * @param response_code Output: codice di risposta HTTP
 * @param response_body Output: corpo della risposta, allocato dinamicamente
 * @return esp_err_t ESP_OK se successo
 */
esp_err_t http_post_request(const char *url, const char *post_data, int *response_code, char **response_body);
esp_err_t http_get_server_time(int *ore, int *minuti);

// Scarica un file di grandi dimensioni in un buffer allocato in PSRAM.
// Timeout esteso (30s). Il chiamante deve free() il buffer.
esp_err_t http_get_file_psram(const char *url, char **out_buf, size_t *out_len);
#ifdef __cplusplus
}
#endif

#endif // HTTP_CLIENT_H