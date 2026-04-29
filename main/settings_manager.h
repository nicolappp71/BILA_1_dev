#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <stdint.h>
#include "esp_err.h"

// Inizializza settings con valori di default
void settings_init(void);

// Volume audio (0-100)
uint8_t settings_get_volume(void);
esp_err_t settings_set_volume(uint8_t volume);

#endif // SETTINGS_MANAGER_H