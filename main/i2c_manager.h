#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include "esp_err.h"
#include "driver/i2c_master.h"

/**
 * @brief Inizializza I2C bus condiviso (chiamare UNA sola volta all'avvio)
 * 
 * @return ESP_OK se successo, ESP_ERR_INVALID_STATE se già inizializzato
 */
esp_err_t i2c_manager_init(void);

/**
 * @brief Ottieni handle I2C bus condiviso
 * 
 * @return Handle I2C, NULL se non inizializzato
 */
i2c_master_bus_handle_t i2c_manager_get_handle(void);

#endif