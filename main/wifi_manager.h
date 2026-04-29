#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bit per EventGroup
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// EventGroup globale
extern EventGroupHandle_t s_wifi_event_group;

void wifi_init_sta(void);
int wifi_get_rssi(void);
bool wifi_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H