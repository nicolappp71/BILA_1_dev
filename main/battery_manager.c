/**
 * @file battery_manager.c
 * @brief Gestione batteria - VERSIONE STANDALONE
 */

#include "esp_log.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <string.h>

static const char *TAG = "battery";

// ========== CONFIGURAZIONE ==========
#define BATTERY_ADC_UNIT        ADC_UNIT_1
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_4  // GPIO20 su ESP32-P4
#define BATTERY_ADC_ATTEN       ADC_ATTEN_DB_12

#define VOLTAGE_DIVIDER_RATIO   3.0f  // R92=200K / R93=100K → V_BAT = V_ADC * 3

#define BATTERY_VOLTAGE_MAX     4200  // 4.2V
#define BATTERY_VOLTAGE_MIN     3000  // 3.0V
#define BATTERY_FILTER_SAMPLES  10

// ========== VARIABILI GLOBALI ==========
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t adc_cali_handle = NULL;
static bool calibration_enabled = false;

static int voltage_buffer[BATTERY_FILTER_SAMPLES] = {0};
static int buffer_index = 0;
static bool buffer_filled = false;

// ========== CURVA LI-ION ==========
static const struct {
    int voltage_mv;
    int percentage;
} discharge_curve[] = {
    {4200, 100}, {4160, 95},  {4100, 90},  {4050, 85},
    {4000, 80},  {3960, 75},  {3920, 70},  {3880, 65},
    {3850, 60},  {3820, 55},  {3800, 50},  {3780, 45},
    {3760, 40},  {3740, 35},  {3710, 30},  {3680, 25},
    {3650, 20},  {3620, 15},  {3580, 10},  {3500, 5},
    {3000, 0}
};
#define CURVE_POINTS (sizeof(discharge_curve) / sizeof(discharge_curve[0]))

// ========== FUNZIONI ==========
static int voltage_to_percentage(int voltage_mv)
{
    if (voltage_mv >= BATTERY_VOLTAGE_MAX) return 100;
    if (voltage_mv <= BATTERY_VOLTAGE_MIN) return 0;

    for (int i = 0; i < CURVE_POINTS - 1; i++) {
        if (voltage_mv >= discharge_curve[i + 1].voltage_mv) {
            int v_high = discharge_curve[i].voltage_mv;
            int v_low = discharge_curve[i + 1].voltage_mv;
            int p_high = discharge_curve[i].percentage;
            int p_low = discharge_curve[i + 1].percentage;
            
            return p_low + ((voltage_mv - v_low) * (p_high - p_low)) / (v_high - v_low);
        }
    }
    return 0;
}

static void add_to_filter(int voltage_mv)
{
    voltage_buffer[buffer_index] = voltage_mv;
    buffer_index = (buffer_index + 1) % BATTERY_FILTER_SAMPLES;
    if (buffer_index == 0) buffer_filled = true;
}

static int get_filtered_voltage(void)
{
    int sum = 0;
    int count = buffer_filled ? BATTERY_FILTER_SAMPLES : buffer_index;
    if (count == 0) return 0;
    
    for (int i = 0; i < count; i++) {
        sum += voltage_buffer[i];
    }
    return sum / count;
}

esp_err_t battery_manager_init(void)
{
    esp_err_t ret;

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore init ADC: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore config canale: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(adc_handle);
        return ret;
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        calibration_enabled = true;
        ESP_LOGI(TAG, "Calibrazione ADC attivata (Curve Fitting)");
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK) {
        calibration_enabled = true;
        ESP_LOGI(TAG, "Calibrazione ADC attivata (Line Fitting)");
    }
#endif

    if (!calibration_enabled) {
        ESP_LOGW(TAG, "Calibrazione ADC non disponibile");
    }

    memset(voltage_buffer, 0, sizeof(voltage_buffer));
    buffer_index = 0;
    buffer_filled = false;

    ESP_LOGI(TAG, "✓ Battery Manager inizializzato (GPIO37, ADC_CH6)");
    return ESP_OK;
}

esp_err_t battery_get_voltage(int *voltage_mv)
{
    if (adc_handle == NULL || voltage_mv == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    int adc_raw = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &adc_raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore lettura ADC: %s", esp_err_to_name(ret));
        return ret;
    }

    int voltage_adc_mv = 0;
    if (calibration_enabled) {
        ret = adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage_adc_mv);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Errore calibrazione: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        voltage_adc_mv = (adc_raw * 3100) / 4095;
    }

    int battery_voltage = voltage_adc_mv * VOLTAGE_DIVIDER_RATIO;

    add_to_filter(battery_voltage);
    *voltage_mv = get_filtered_voltage();

    return ESP_OK;
}

esp_err_t battery_get_percentage(int *percentage)
{
    if (percentage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int voltage_mv = 0;
    esp_err_t ret = battery_get_voltage(&voltage_mv);
    if (ret != ESP_OK) {
        return ret;
    }

    *percentage = voltage_to_percentage(voltage_mv);

    if (*percentage < 0) *percentage = 0;
    if (*percentage > 100) *percentage = 100;

    return ESP_OK;
}

esp_err_t battery_is_charging(bool *is_charging)
{
    if (is_charging == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *is_charging = false;
    return ESP_OK;
}

void battery_manager_deinit(void)
{
    if (calibration_enabled && adc_cali_handle != NULL) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(adc_cali_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_delete_scheme_line_fitting(adc_cali_handle);
#endif
        adc_cali_handle = NULL;
        calibration_enabled = false;
    }

    if (adc_handle != NULL) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }

    ESP_LOGI(TAG, "Battery Manager deinizializzato");
}