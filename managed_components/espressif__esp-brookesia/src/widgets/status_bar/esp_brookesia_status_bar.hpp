/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <vector>
#include <map>
#include "lvgl.h"
#include "core/esp_brookesia_core.hpp"
#include "esp_brookesia_status_bar_type.h"
#include "esp_brookesia_status_bar_icon.hpp"

class ESP_Brookesia_StatusBar {
public:
    enum class ClockFormat {
        FORMAT_12H,
        FORMAT_24H,
    };
    enum class WifiState {
        OFF = 0,
        LEVEL_1,
        LEVEL_2,
        LEVEL_3,
    };

    ESP_Brookesia_StatusBar(const ESP_Brookesia_Core &core, const ESP_Brookesia_StatusBarData_t &data, int battery_id,
                         int wifi_id, int ble_id, int sd_id);
    ~ESP_Brookesia_StatusBar();

    bool begin(lv_obj_t *parent);
    bool del(void);
    bool setVisualMode(ESP_Brookesia_StatusBarVisualMode_t mode) const;
    bool addIcon(const ESP_Brookesia_StatusBarIconData_t &data, uint8_t area_index, int id);
    bool removeIcon(int id);
    bool setIconState(int id, int state) const;
    bool checkVisible(void) const;

    // Battery
    bool setBatteryPercent(bool charge_flag, int percent) const;
    bool showBatteryPercent(void) const;
    bool hideBatteryPercent(void) const;
    bool showBatteryIcon(void) const;
    bool hideBatteryIcon(void) const;

    // Wifi
    bool setWifiIconState(int state) const;
    bool setWifiIconState(WifiState state) const;

    // BLE
    bool setBleIconState(int state) const;
    bool showBleIcon(void) const;
    bool hideBleIcon(void) const;

    // SD Card
    bool setSdIconState(int state) const;
    bool showSdIcon(void) const;
    bool hideSdIcon(void) const;

    // Clock
    bool setClockFormat(ClockFormat format) const;
    bool setClock(int hour, int minute, bool is_pm) const;

    static bool calibrateData(const ESP_Brookesia_StyleSize_t &screen_size, const ESP_Brookesia_CoreHome &home,
                              ESP_Brookesia_StatusBarData_t &data);

    // Spostato in public per risolvere l'errore in esp_brookesia_phone_home.cpp
    static bool calibrateIconData(const ESP_Brookesia_StatusBarData_t &bar_data, const ESP_Brookesia_CoreHome &home,
                                 ESP_Brookesia_StatusBarIconData_t &icon_data);

private:
    bool checkMainInitialized(void) const { return (_main_obj != nullptr); }
    bool checkBatteryInitialized(void) const { return _is_battery_initialed; }
    bool checkClockInitialized(void) const { return (_clock_obj != nullptr); }
    
    bool beginMain(lv_obj_t *parent);
    bool updateMainByNewData(void);
    bool delMain(void);
    bool beginBattery(void);
    bool updateBatteryByNewData(void);
    bool delBattery(void);
    bool beginWifi(void);
    bool beginBle(void);
    bool beginSd(void);
    bool beginClock(void);
    bool updateClockByNewData(void);
    bool delClock(void);
    static void onDataUpdateEventCallback(lv_event_t *event);

    const ESP_Brookesia_Core &_core;
    ESP_Brookesia_StatusBarData_t _data;
    ESP_Brookesia_LvObj_t _main_obj;
    std::vector<ESP_Brookesia_LvObj_t> _area_objs;
    std::map<int, std::shared_ptr<ESP_Brookesia_StatusBarIcon>> _id_icon_map;
    
    // Ordine corretto delle variabili per evitare l'errore -Wreorder
    int _battery_id;
    int _wifi_id;
    int _ble_id;
    int _sd_id;
    bool _is_battery_initialed;
    mutable int _battery_state;
    mutable bool _is_battery_lable_out_of_area;
    ESP_Brookesia_LvObj_t _battery_label;
    
    // Clock
    mutable int _clock_hour;
    mutable int _clock_min;
    mutable bool _is_clock_out_of_area;
    ESP_Brookesia_LvObj_t _clock_obj;
    ESP_Brookesia_LvObj_t _clock_hour_label;
    ESP_Brookesia_LvObj_t _clock_dot_label;
    ESP_Brookesia_LvObj_t _clock_min_label;
    ESP_Brookesia_LvObj_t _clock_period_label;
};