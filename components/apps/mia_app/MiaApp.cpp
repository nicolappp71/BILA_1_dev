#include "MiaApp.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_lvgl_port.h"
#include "driver/jpeg_decode.h"
#include <string.h>
#include <cmath>
#include "banchetto_manager.h"

#define TAG "MiaApp"
#define BASE_URL "http://192.168.1.53/cicli/JPEG/%s/%02d.jpg"
#define JPG_BUFFER_MAX 524288
#define HTTP_TIMEOUT_MS 10000
#define JPEG_DECODE_TIMEOUT_MS 5000
#define OVERLAY_DISPLAY_TIME_MS 2000
#define SWIPE_THRESHOLD_PX 80

extern "C"
{
    extern const lv_img_dsc_t libretto;
}

typedef struct
{
    uint8_t *buffer;
    size_t size;
    size_t capacity;
} http_download_ctx_t;

MiaApp::MiaApp() : ESP_Brookesia_PhoneApp("Ciclo Lavoro", &libretto, true)
{
}

MiaApp::~MiaApp()
{
    if (overlay_timer)
    {
        lv_timer_del(overlay_timer);
        overlay_timer = nullptr;
    }
    if (download_task_handle)
    {
        vTaskDelete(download_task_handle);
        download_task_handle = nullptr;
    }
    libera_immagine_corrente();
}

bool MiaApp::init(void)
{
    return true;
}

bool MiaApp::run(void)
{
    lv_area_t area = getVisualArea();
    int width = area.x2 - area.x1;
    int height = area.y2 - area.y1;

    main_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_container, width, height);
    lv_obj_set_style_pad_all(main_container, 0, 0);
    lv_obj_set_style_bg_color(main_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(main_container, 255, 0);

    img_obj = lv_obj_create(main_container);
    lv_obj_set_size(img_obj, width, height);
    lv_obj_set_style_bg_color(img_obj, lv_color_hex(0x111111), 0);
    lv_obj_set_style_pad_all(img_obj, 0, 0);
    lv_obj_add_flag(img_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(img_obj, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_add_event_cb(img_obj, gesture_event_cb, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(img_obj, gesture_event_cb, LV_EVENT_RELEASED, this);

    loading_spinner = lv_spinner_create(img_obj, 1000, 60);
    lv_obj_set_size(loading_spinner, 60, 60);
    lv_obj_center(loading_spinner);
    lv_obj_add_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);

    error_label = lv_label_create(img_obj);
    lv_obj_center(error_label);
    lv_obj_set_style_text_color(error_label, lv_color_hex(0xFF0000), 0);
    lv_obj_add_flag(error_label, LV_OBJ_FLAG_HIDDEN);

    page_overlay_label = lv_label_create(img_obj);
    lv_label_set_text(page_overlay_label, "1");
    lv_obj_set_style_text_font(page_overlay_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(page_overlay_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(page_overlay_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(page_overlay_label, 200, 0);
    lv_obj_set_style_pad_all(page_overlay_label, 15, 0);
    lv_obj_set_style_radius(page_overlay_label, 8, 0);
    lv_obj_align(page_overlay_label, LV_ALIGN_BOTTOM_MID, 0, -30);

    carica_pagina(1);
    return true;
}

// ==================== DECODIFICA HARDWARE ====================

bool MiaApp::decode_jpg_hw(uint8_t *jpg_data, size_t jpg_len, uint8_t **out_rgb, int *width, int *height)
{
    jpeg_decoder_handle_t decoder_engine;
    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .timeout_ms = JPEG_DECODE_TIMEOUT_MS,
    };

    esp_err_t ret = jpeg_new_decoder_engine(&decode_eng_cfg, &decoder_engine);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "JPEG decoder init failed: %s", esp_err_to_name(ret));
        return false;
    }

    jpeg_decode_picture_info_t picture_info;
    ret = jpeg_decoder_get_info(jpg_data, jpg_len, &picture_info);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get JPEG info: %s", esp_err_to_name(ret));
        jpeg_del_decoder_engine(decoder_engine);
        return false;
    }

    *width = picture_info.width;
    *height = picture_info.height;

    int aligned_width = (*width + 15) & ~15;
    int aligned_height = (*height + 15) & ~15;

    ESP_LOGI(TAG, "JPEG: %dx%d -> aligned %dx%d", *width, *height, aligned_width, aligned_height);

    size_t requested_size = aligned_width * aligned_height * 2;
    size_t actual_size = 0;

    jpeg_decode_memory_alloc_cfg_t rx_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };

    uint8_t *rgb = (uint8_t *)jpeg_alloc_decoder_mem(requested_size, &rx_mem_cfg, &actual_size);
    if (!rgb)
    {
        ESP_LOGE(TAG, "Failed to allocate RGB buffer");
        jpeg_del_decoder_engine(decoder_engine);
        return false;
    }

    jpeg_decode_cfg_t decode_cfg = {};
    decode_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    decode_cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
    decode_cfg.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;

    uint32_t decoded_size = 0;
    ret = jpeg_decoder_process(decoder_engine, &decode_cfg,
                               jpg_data, jpg_len,
                               rgb, actual_size,
                               &decoded_size);

    jpeg_del_decoder_engine(decoder_engine);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "JPEG decode failed: %s", esp_err_to_name(ret));
        heap_caps_free(rgb);
        return false;
    }

    ESP_LOGI(TAG, "Decoded OK: %u bytes", decoded_size);
    *out_rgb = rgb;
    return true;
}

// ==================== DOWNLOAD HTTP ====================

esp_err_t MiaApp::http_event_handler(esp_http_client_event_t *evt)
{
    http_download_ctx_t *ctx = (http_download_ctx_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA)
    {
        if (ctx->size + evt->data_len <= ctx->capacity)
        {
            memcpy(ctx->buffer + ctx->size, evt->data, evt->data_len);
            ctx->size += evt->data_len;
        }
        else
        {
            ESP_LOGW(TAG, "HTTP buffer overflow");
        }
    }
    return ESP_OK;
}

bool MiaApp::download_jpg(const char *url, uint8_t **out_buffer, size_t *out_size)
{
    uint8_t *buffer = (uint8_t *)heap_caps_malloc(JPG_BUFFER_MAX, MALLOC_CAP_SPIRAM);
    if (!buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate download buffer");
        return false;
    }

    http_download_ctx_t ctx = {
        .buffer = buffer,
        .size = 0,
        .capacity = JPG_BUFFER_MAX};

    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.user_data = &ctx;
    config.timeout_ms = HTTP_TIMEOUT_MS;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        heap_caps_free(buffer);
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status_code != 200)
    {
        ESP_LOGE(TAG, "HTTP failed: %s (status %d)", esp_err_to_name(err), status_code);
        heap_caps_free(buffer);
        return false;
    }

    *out_buffer = buffer;
    *out_size = ctx.size;
    ESP_LOGI(TAG, "Downloaded %zu bytes", ctx.size);
    return true;
}

// ==================== GESTIONE IMMAGINE ====================

void MiaApp::libera_immagine_corrente()
{
    if (current_img_widget)
    {
        lv_obj_del(current_img_widget);
        current_img_widget = nullptr;
    }

    if (rgb_buffer)
    {
        heap_caps_free(rgb_buffer);
        rgb_buffer = nullptr;
        ESP_LOGI(TAG, "Image memory freed");
    }
}

void MiaApp::carica_pagina(int pagina)
{
    if (download_task_handle != nullptr)
    {
        ESP_LOGW(TAG, "Task still running, ignoring request for page %d", pagina);
        return;
    }

    download_in_progress = true;
    ESP_LOGI(TAG, "Starting load for page %d", pagina);

    if (lvgl_port_lock(0))
    {
        mostra_loading(true);
        lvgl_port_unlock();
    }

    task_params_t *params = (task_params_t *)malloc(sizeof(task_params_t));
    if (!params)
    {
        ESP_LOGE(TAG, "Failed to allocate task params");
        download_in_progress = false;
        return;
    }

    params->app = this;
    params->pagina = pagina;

    BaseType_t result = xTaskCreatePinnedToCore(download_task, "download", 8192, params, 5, &download_task_handle, 1);
    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create download task");
        free(params);
        download_task_handle = nullptr;
        download_in_progress = false;
    }
}

void MiaApp::download_task(void *param)
{
    task_params_t *p = (task_params_t *)param;
    MiaApp *app = p->app;
    int pagina = p->pagina;
    free(p);

    // Recupera dati banchetto tramite API con mutex
    banchetto_data_t data;
    if (!banchetto_manager_get_data(&data))
    {
        ESP_LOGE(TAG, "Dati banchetto non disponibili");
        app->download_in_progress = false;
        app->download_task_handle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    // Log diagnostico
    ESP_LOGI(TAG, "Ciclo raw: '%s' (len=%d)", data.ciclo, strlen(data.ciclo));

    // Skip zero iniziale: "0431" -> "431"
    char *ciclo_ptr = data.ciclo;
    if (ciclo_ptr[0] == '0' && ciclo_ptr[1] != '\0')
    {
        ciclo_ptr++;
    }

    ESP_LOGI(TAG, "Ciclo usato: '%s'", ciclo_ptr);
    ESP_LOGI(TAG, "Download task started for page %d", pagina);

    char url[128];
    snprintf(url, sizeof(url), BASE_URL, ciclo_ptr, pagina);
    ESP_LOGW("DEBUG_URL", "URL: %s", url);

    uint8_t *jpg_buffer = nullptr;
    size_t jpg_size = 0;
    bool success = false;

    if (app->download_jpg(url, &jpg_buffer, &jpg_size))
    {
        uint8_t *new_rgb_buffer = nullptr;
        int width, height;

        if (app->decode_jpg_hw(jpg_buffer, jpg_size, &new_rgb_buffer, &width, &height))
        {
            app->pagina_corrente = pagina;

            if (lvgl_port_lock(0))
            {
                uint8_t *old_rgb_buffer = app->rgb_buffer;
                lv_obj_t *old_img_widget = app->current_img_widget;

                app->rgb_buffer = new_rgb_buffer;

                int aligned_width = (width + 15) & ~15;
                int aligned_height = (height + 15) & ~15;

                app->img_dsc.header.w = aligned_width;
                app->img_dsc.header.h = aligned_height;
                app->img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
                app->img_dsc.data = new_rgb_buffer;
                app->img_dsc.data_size = aligned_width * aligned_height * 2;

                lv_img_cache_invalidate_src(&app->img_dsc);

                app->current_img_widget = lv_img_create(app->img_obj);
                lv_img_set_src(app->current_img_widget, &app->img_dsc);
                lv_obj_set_size(app->current_img_widget, width, height);
                lv_obj_align(app->current_img_widget, LV_ALIGN_TOP_MID, 0, 0);

                lv_obj_scroll_to_y(app->img_obj, 0, LV_ANIM_OFF);

                if (app->loading_spinner)
                    lv_obj_add_flag(app->loading_spinner, LV_OBJ_FLAG_HIDDEN);
                if (app->error_label)
                    lv_obj_add_flag(app->error_label, LV_OBJ_FLAG_HIDDEN);

                app->mostra_numero_pagina();

                lvgl_port_unlock();

                if (old_img_widget)
                {
                    if (lvgl_port_lock(0))
                    {
                        lv_obj_del(old_img_widget);
                        lvgl_port_unlock();
                    }
                }

                if (old_rgb_buffer)
                {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    heap_caps_free(old_rgb_buffer);
                }

                ESP_LOGI(TAG, "Page %d displayed successfully", pagina);
                success = true;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to acquire LVGL lock");
                heap_caps_free(new_rgb_buffer);
            }
        }

        heap_caps_free(jpg_buffer);
    }

    if (!success)
    {
        ESP_LOGE(TAG, "Failed to load page %d", pagina);
        if (lvgl_port_lock(0))
        {
            if (app->loading_spinner)
                lv_obj_add_flag(app->loading_spinner, LV_OBJ_FLAG_HIDDEN);
            if (app->error_label)
            {
                lv_label_set_text(app->error_label, "Errore caricamento");
                lv_obj_clear_flag(app->error_label, LV_OBJ_FLAG_HIDDEN);
            }
            lvgl_port_unlock();
        }
    }

    app->download_in_progress = false;
    app->download_task_handle = nullptr;

    ESP_LOGI(TAG, "Download task ending for page %d", pagina);
    vTaskDelete(NULL);
}

// ==================== UI HELPERS ====================

void MiaApp::mostra_loading(bool show)
{
    if (!loading_spinner)
        return;

    if (show)
        lv_obj_clear_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);
}

void MiaApp::mostra_errore(const char *msg)
{
    if (!error_label)
        return;
    lv_label_set_text(error_label, msg);
    lv_obj_clear_flag(error_label, LV_OBJ_FLAG_HIDDEN);
}

void MiaApp::mostra_numero_pagina()
{
    if (!page_overlay_label)
        return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", pagina_corrente);
    lv_label_set_text(page_overlay_label, buf);
    lv_obj_clear_flag(page_overlay_label, LV_OBJ_FLAG_HIDDEN);

    if (overlay_timer)
    {
        lv_timer_del(overlay_timer);
    }

    overlay_timer = lv_timer_create(overlay_timer_cb, OVERLAY_DISPLAY_TIME_MS, this);
    lv_timer_set_repeat_count(overlay_timer, 1);
}

void MiaApp::nascondi_numero_pagina()
{
    if (page_overlay_label)
    {
        lv_obj_add_flag(page_overlay_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (overlay_timer)
    {
        lv_timer_del(overlay_timer);
        overlay_timer = nullptr;
    }
}

// ==================== CALLBACKS ====================

void MiaApp::gesture_event_cb(lv_event_t *e)
{
    MiaApp *app = (MiaApp *)lv_event_get_user_data(e);
    if (!app || app->download_in_progress || app->download_task_handle != nullptr)
    {
        return;
    }

    static lv_point_t start_point = {0, 0};
    static bool is_pressing = false;

    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t current_point;
    lv_indev_get_point(indev, &current_point);

    if (code == LV_EVENT_PRESSING)
    {
        if (!is_pressing)
        {
            start_point = current_point;
            is_pressing = true;
        }
    }
    else if (code == LV_EVENT_RELEASED)
    {
        if (is_pressing)
        {
            int32_t delta_x = current_point.x - start_point.x;
            int32_t delta_y = current_point.y - start_point.y;

            if (abs(delta_x) > abs(delta_y) && abs(delta_x) > SWIPE_THRESHOLD_PX)
            {
                if (delta_x < 0)
                {
                    ESP_LOGI(TAG, "Swipe left (delta=%d): %d -> %d", delta_x, app->pagina_corrente, app->pagina_corrente + 1);
                    app->carica_pagina(app->pagina_corrente + 1);
                }
                else
                {
                    if (app->pagina_corrente > 1)
                    {
                        ESP_LOGI(TAG, "Swipe right (delta=%d): %d -> %d", delta_x, app->pagina_corrente, app->pagina_corrente - 1);
                        app->carica_pagina(app->pagina_corrente - 1);
                    }
                }
            }

            is_pressing = false;
            start_point = {0, 0};
        }
    }
}

void MiaApp::overlay_timer_cb(lv_timer_t *timer)
{
    MiaApp *app = (MiaApp *)timer->user_data;
    if (app)
    {
        app->nascondi_numero_pagina();
    }
}

// ==================== LIFECYCLE ====================

bool MiaApp::back()
{
    if (main_container)
    {
        lv_obj_del(main_container);
        main_container = nullptr;
    }
    notifyCoreClosed();
    return true;
}

bool MiaApp::close()
{
    return true;
}