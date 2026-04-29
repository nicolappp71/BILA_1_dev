
#include "audio_manager.h"
#include "i2c_manager.h"  // ← AGGIUNTO

// Tutti gli include driver
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev.h"
#include <math.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "settings_manager.h"

// TAG e variabili private
static const char *TAG = "AUDIO";
static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;
static esp_codec_dev_handle_t codec_handle = NULL;
static bool audio_initialized = false;

esp_err_t audio_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   INIZIALIZZAZIONE AUDIO ES8311");
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret;

    // 1. USA I2C CONDIVISO (NON creare nuovo bus)
    i2c_master_bus_handle_t i2c_bus_handle = i2c_manager_get_handle();
    if (i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C manager non inizializzato!");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ I2C condiviso ottenuto (SDA:%d SCL:%d)", AUDIO_I2C_SDA, AUDIO_I2C_SCL);

    // 2. Inizializza I2S
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(AUDIO_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    ret = i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore creazione canale I2S: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = AUDIO_I2S_MCLK,
            .bclk = AUDIO_I2S_BCLK,
            .ws = AUDIO_I2S_WS,
            .dout = AUDIO_I2S_DOUT,
            .din = AUDIO_I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = 384;

    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore init I2S std mode: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore enable I2S: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✓ I2S inizializzato");
    ESP_LOGI(TAG, "  MCLK:%d BCLK:%d WS:%d DOUT:%d DIN:%d",
             AUDIO_I2S_MCLK, AUDIO_I2S_BCLK, AUDIO_I2S_WS, AUDIO_I2S_DOUT, AUDIO_I2S_DIN);

    // 3. Configura ES8311 Codec (usa I2C condiviso)
    audio_codec_i2c_cfg_t i2c_codec_cfg = {
        .port = AUDIO_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus_handle,  // ← Usa handle condiviso
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_codec_cfg);
    if (!ctrl_if) {
        ESP_LOGE(TAG, "Errore creazione I2C ctrl interface");
        return ESP_FAIL;
    }

    audio_codec_i2s_cfg_t i2s_codec_cfg = {
        .port = AUDIO_I2S_NUM,
        .rx_handle = rx_handle,
        .tx_handle = tx_handle,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_codec_cfg);
    if (!data_if) {
        ESP_LOGE(TAG, "Errore creazione I2S data interface");
        return ESP_FAIL;
    }

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    if (!gpio_if) {
        ESP_LOGE(TAG, "Errore creazione GPIO interface");
        return ESP_FAIL;
    }

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .master_mode = false,
        .use_mclk = true,
        .pa_pin = AUDIO_PA_CTRL,
        .pa_reverted = false,
        .hw_gain = {
            .pa_voltage = 5.0,
            .codec_dac_voltage = 3.3,
        },
        .mclk_div = 384,
    };
    const audio_codec_if_t *es8311_if = es8311_codec_new(&es8311_cfg);
    if (!es8311_if) {
        ESP_LOGE(TAG, "Errore creazione ES8311 codec");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✓ ES8311 codec configurato");

    // 4. Crea device codec
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = es8311_if,
        .data_if = data_if,
    };
    codec_handle = esp_codec_dev_new(&dev_cfg);
    if (!codec_handle) {
        ESP_LOGE(TAG, "Errore creazione codec device");
        return ESP_FAIL;
    }

    // 5. Apri codec con configurazione sample
    esp_codec_dev_sample_info_t sample_cfg = {
        .bits_per_sample = 16,
        .channel = 2,
        .channel_mask = 0x03,
        .sample_rate = AUDIO_SAMPLE_RATE,
    };

    ret = esp_codec_dev_open(codec_handle, &sample_cfg);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Errore apertura codec device");
        return ESP_FAIL;
    }

    // 6. Imposta volume
    ret = esp_codec_dev_set_out_vol(codec_handle, settings_get_volume());
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Errore impostazione volume");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✓ Codec aperto e volume impostato a %d", settings_get_volume());
    ESP_LOGI(TAG, "✓ Amplificatore abilitato (GPIO %d)", AUDIO_PA_CTRL);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   AUDIO SISTEMA PRONTO");
    ESP_LOGI(TAG, "========================================");

    audio_initialized = true;
    return ESP_OK;
}

void buzzer_set_volume(uint8_t volume)
{
    if (!audio_initialized || !codec_handle) {
        ESP_LOGW(TAG, "Audio non inizializzato!");
        return;
    }
    
    if (volume > 100) {
        volume = 100;
    }
    
    esp_err_t ret = esp_codec_dev_set_out_vol(codec_handle, volume);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "❌ Errore impostazione volume");
    } else {
        ESP_LOGI(TAG, "✓ Volume aggiornato in tempo reale: %d%%", volume);
    }
}

void beep_play(uint16_t frequency, uint16_t duration_ms)
{
    if (!audio_initialized) {
        ESP_LOGW(TAG, "Audio non inizializzato!");
        return;
    }

    ESP_LOGI(TAG, "🔊 Beep: %d Hz, %d ms", frequency, duration_ms);

    // Genera buffer tono sinusoidale
    const int samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    const int buffer_size = samples * 4; // 2 canali * 2 bytes
    int16_t *buffer = malloc(buffer_size);

    if (!buffer) {
        ESP_LOGE(TAG, "Errore allocazione buffer audio");
        return;
    }

    // Genera onda sinusoidale
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        int16_t sample = (int16_t)(sin(2.0 * M_PI * frequency * t) * 32000);
        buffer[i * 2] = sample;     // Left
        buffer[i * 2 + 1] = sample; // Right
    }

    // Invia a I2S
    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_handle, buffer, buffer_size, &bytes_written, portMAX_DELAY);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore scrittura I2S: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✓ Beep riprodotto (%d bytes)", bytes_written);
    }

    free(buffer);
}

bool audio_is_initialized(void)
{
    return audio_initialized;
}