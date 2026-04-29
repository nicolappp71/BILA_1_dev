#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OFFLINE_QUEUE_PATH "/sdcard/offline_queue.jsonl"

// Aggiunge un'operazione alla coda (salva la URL completa)
esp_err_t offline_queue_push(const char *url);

// Conta le operazioni pendenti nella coda
int offline_queue_count(void);

// Invia tutte le operazioni pendenti al server e svuota la coda
esp_err_t offline_queue_process(void);

#ifdef __cplusplus
}
#endif
