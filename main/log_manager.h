#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Inizializza il log manager:
 *        - alloca ring buffer in PSRAM
 *        - installa vprintf hook
 *        - avvia task SD writer
 *        Chiamare PRIMA di qualsiasi ESP_LOGI per catturare tutto.
 */
esp_err_t log_manager_init(void);

/**
 * @brief Notifica che la SD è montata e pronta.
 *        Da chiamare dopo bsp_sdcard_mount() con esito OK.
 */
void log_manager_sd_ready(void);

/**
 * @brief Numero totale di righe scritte dall'avvio (indice progressivo).
 */
uint32_t log_manager_get_total(void);

/**
 * @brief Restituisce le righe dal log a partire da `from`.
 *
 * @param from      Indice progressivo da cui iniziare
 * @param out_buf   Buffer di output
 * @param buf_size  Dimensione del buffer
 * @param last_idx  [out] Indice dell'ultima riga disponibile dopo la chiamata
 * @return          Numero di righe copiate
 */
uint32_t log_manager_get_lines(uint32_t from, char *out_buf, size_t buf_size, uint32_t *last_idx);