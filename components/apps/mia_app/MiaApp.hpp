#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

struct esp_http_client_event;
typedef struct esp_http_client_event esp_http_client_event_t;

class MiaApp : public ESP_Brookesia_PhoneApp {
public:
    MiaApp();
    ~MiaApp();

    bool init(void) override;
    bool run(void) override;
    bool back(void) override;
    bool close(void) override;

private:
    // UI objects
    lv_obj_t *main_container = nullptr;
    lv_obj_t *img_obj = nullptr;
    lv_obj_t *loading_spinner = nullptr;
    lv_obj_t *error_label = nullptr;
    lv_obj_t *current_img_widget = nullptr;
    lv_obj_t *page_overlay_label = nullptr;
    lv_timer_t *overlay_timer = nullptr;
    
    int pagina_corrente = 1;
    
    // Current image data
    uint8_t *rgb_buffer = nullptr;
    int img_width = 0;
    int img_height = 0;
    lv_img_dsc_t img_dsc = {};
    
    // Task control
    TaskHandle_t download_task_handle = nullptr;
    volatile bool download_in_progress = false;

    // Core functions
    void carica_pagina(int pagina);
    bool download_jpg(const char *url, uint8_t **out_buffer, size_t *out_size);
    bool decode_jpg_hw(uint8_t *jpg_data, size_t jpg_len, uint8_t **out_rgb, int *width, int *height);
    void libera_immagine_corrente();
    
    // UI helpers
    void mostra_loading(bool show);
    void mostra_errore(const char *msg);
    void mostra_numero_pagina();
    void nascondi_numero_pagina();
    
    // Callbacks
    static void gesture_event_cb(lv_event_t *e);
    static void overlay_timer_cb(lv_timer_t *timer);
    static esp_err_t http_event_handler(esp_http_client_event_t *evt);
    static void download_task(void *param);
    
    typedef struct {
        MiaApp *app;
        int pagina;
    } task_params_t;
};

