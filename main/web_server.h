#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inizializza e avvia il web server con WebSocket
 *        Serve dashboard HTML su http://IP/
 *        Gestisce connessioni WebSocket per aggiornamenti real-time
 * 
 * @return ESP_OK se avvio riuscito
 */
esp_err_t web_server_init(void);

/**
 * @brief Invia aggiornamento stato a tutti i client connessi
 *        Chiamata automaticamente quando cambiano i dati
 * 
 * @note Non blocca se mutex occupato (priorità al banchetto)
 */
void web_server_broadcast_update(void);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H