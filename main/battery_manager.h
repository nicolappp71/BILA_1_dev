/**
 * @file battery_manager.h
 * @brief Gestione monitoraggio batteria con ADC calibrato
 * 
 * Hardware: GPIO37 collegato a partitore resistivo 1:3
 *           R92 (200K) - BAT - R93 (100K) - GND
 *           V_GPIO37 = V_BAT / 3
 */

#pragma once

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configurazione Hardware
#define BATTERY_ADC_UNIT        ADC_UNIT_1
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_6  // GPIO37 su ESP32-P4
#define BATTERY_ADC_ATTEN       ADC_ATTEN_DB_12 // 0-3.3V range

// Partitore Resistivo (R92=200K, R93=100K)
//#define VOLTAGE_DIVIDER_RATIO   3.0f  // V_BAT = V_ADC * 3

// Limiti Batteria Li-Ion/Li-Po (mV)
#define BATTERY_VOLTAGE_MAX     4200  // 4.2V (carica completa)
#define BATTERY_VOLTAGE_MIN     3000  // 3.0V (scarica completa)

// Configurazione Filtraggio
#define BATTERY_FILTER_SAMPLES  10    // Media mobile su N campioni

/**
 * @brief Inizializza il gestore batteria con ADC calibrato
 * @return ESP_OK se successo, ESP_FAIL altrimenti
 */
esp_err_t battery_manager_init(void);

/**
 * @brief Legge la tensione batteria grezza (mV)
 * @param[out] voltage_mv Tensione in millivolt
 * @return ESP_OK se successo
 */
esp_err_t battery_get_voltage(int *voltage_mv);

/**
 * @brief Legge la percentuale batteria (0-100%)
 * @param[out] percentage Percentuale carica
 * @return ESP_OK se successo
 */
esp_err_t battery_get_percentage(int *percentage);

/**
 * @brief Verifica se la batteria è in carica
 * @param[out] is_charging true se in carica
 * @return ESP_OK se successo
 * @note Implementazione dipende da GPIO disponibile dal charger IC
 */
esp_err_t battery_is_charging(bool *is_charging);

/**
 * @brief Deinizializza il gestore batteria
 */
void battery_manager_deinit(void);

#ifdef __cplusplus
}
#endif