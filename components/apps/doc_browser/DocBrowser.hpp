#pragma once
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct esp_http_client_event;
typedef struct esp_http_client_event esp_http_client_event_t;

#define DOC_MAX_FILES 64
#define DOC_MAX_FOLDERS 32
#define DOC_MAX_PATH 256
#define DOC_MAX_NAME 64

class DocBrowser : public ESP_Brookesia_PhoneApp {
public:
    DocBrowser();
    ~DocBrowser();

    bool init(void) override;
    bool run(void) override;
    bool back(void) override;
    bool close(void) override;

private:
    // ── Modalità ──
    enum Mode { MODE_BROWSER, MODE_VIEWER };
    Mode current_mode = MODE_BROWSER;

    // ── Dati directory ──
    char current_path[DOC_MAX_PATH] = {0};
    char folder_names[DOC_MAX_FOLDERS][DOC_MAX_NAME];
    char file_names[DOC_MAX_FILES][DOC_MAX_NAME];
    int folder_count = 0;
    int file_count = 0;
    int current_file_index = 0;

    // ── UI: container principale ──
    lv_obj_t *main_container = nullptr;

    // ── UI: browser ──
    lv_obj_t *browser_panel = nullptr;
    lv_obj_t *path_label = nullptr;
    lv_obj_t *back_btn = nullptr;
    lv_obj_t *file_list = nullptr;
    lv_obj_t *browser_spinner = nullptr;
    lv_obj_t *search_input = nullptr;
    lv_obj_t *search_btn = nullptr;
    lv_obj_t *keyboard = nullptr;
    bool search_mode = false;

    // ── UI: viewer ──
    lv_obj_t *viewer_panel = nullptr;
    lv_obj_t *img_obj = nullptr;
    lv_obj_t *loading_spinner = nullptr;
    lv_obj_t *error_label = nullptr;
    lv_obj_t *current_img_widget = nullptr;
    lv_obj_t *page_overlay_label = nullptr;
    lv_timer_t *overlay_timer = nullptr;

    // ── Image data ──
    uint8_t *rgb_buffer = nullptr;
    lv_img_dsc_t img_dsc = {};

    // ── Task ──
    TaskHandle_t task_handle = nullptr;
    volatile bool task_in_progress = false;

    // ── Dimensioni schermo ──
    int screen_w = 0;
    int screen_h = 0;

    // ── Browser functions ──
    void crea_browser_ui();
    void crea_viewer_ui();
    void mostra_browser();
    void mostra_viewer();
    void carica_directory(const char *path);
    void popola_lista();
    void naviga_cartella(const char *folder);
    void naviga_su();
    void esegui_ricerca(const char *query);
    void chiudi_ricerca();

    // ── Viewer functions ──
    void apri_file(int index);
    void carica_immagine(int index);
    void libera_immagine_corrente();
    void mostra_loading(bool show);
    void mostra_errore(const char *msg);
    void mostra_numero_pagina();
    void nascondi_numero_pagina();

    // ── HTTP ──
    bool download_json(const char *url, char **out_body);
    bool download_jpg(const char *url, uint8_t **out_buffer, size_t *out_size);
    bool decode_jpg_hw(uint8_t *jpg_data, size_t jpg_len,
                       uint8_t **out_rgb, int *width, int *height);

    // ── Callbacks ──
    static void folder_click_cb(lv_event_t *e);
    static void file_click_cb(lv_event_t *e);
    static void back_btn_cb(lv_event_t *e);
    static void search_btn_cb(lv_event_t *e);
    static void search_ready_cb(lv_event_t *e);
    static void search_input_focus_cb(lv_event_t *e);
    static void search_result_folder_cb(lv_event_t *e);
    static void search_result_file_cb(lv_event_t *e);
    static void gesture_event_cb(lv_event_t *e);
    static void overlay_timer_cb(lv_timer_t *timer);
    static esp_err_t http_event_handler(esp_http_client_event_t *evt);
    static void directory_task(void *param);
    static void image_task(void *param);

    typedef struct {
        DocBrowser *app;
        char path[DOC_MAX_PATH];
    } dir_task_params_t;

    typedef struct {
        DocBrowser *app;
        int file_index;
    } img_task_params_t;
};