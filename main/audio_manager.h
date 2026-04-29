#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Configurazione hardware
#define AUDIO_I2C_NUM 0
#define AUDIO_I2C_SCL 8
#define AUDIO_I2C_SDA 7
#define AUDIO_I2S_NUM 0
#define AUDIO_I2S_MCLK 13
#define AUDIO_I2S_BCLK 12
#define AUDIO_I2S_WS 10
#define AUDIO_I2S_DOUT 9
#define AUDIO_I2S_DIN 11
#define AUDIO_PA_CTRL 53
#define AUDIO_SAMPLE_RATE 16000
//#define AUDIO_VOLUME 100

// API pubblica
esp_err_t audio_init(void);
void beep_play(uint16_t frequency, uint16_t duration_ms);
bool audio_is_initialized(void);
void buzzer_set_volume(uint8_t volume);

#endif