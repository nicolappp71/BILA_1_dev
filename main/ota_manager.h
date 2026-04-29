#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    extern volatile bool ota_in_progress;
    esp_err_t ota_manager_start(void);

#ifdef __cplusplus
}
#endif