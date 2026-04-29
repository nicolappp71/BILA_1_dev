#include "DocBrowser.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_lvgl_port.h"
#include "driver/jpeg_decode.h"
#include "cJSON.h"
#include <string.h>
#include <cmath>
//ciaone
#define TAG "DocBrowser"
#define BASE_URL "http://172.18.2.254/docs/"
#define JPG_BUFFER_MAX 524288
#define HTTP_TIMEOUT_MS 10000
#define JPEG_DECODE_TIMEOUT_MS 5000
#define OVERLAY_DISPLAY_TIME_MS 2000
#define SWIPE_THRESHOLD_PX 80

extern "C" {
    extern const lv_img_dsc_t doc;
}

typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t capacity;
} http_download_ctx_t;

// ==================== CONSTRUCTOR / DESTRUCTOR ====================

DocBrowser::DocBrowser() : ESP_Brookesia_PhoneApp("Documenti", &doc, true) {
}

DocBrowser::~DocBrowser() {
    if (overlay_timer) {
        lv_timer_del(overlay_timer);
        overlay_timer = nullptr;
    }
    if (task_handle) {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }
    libera_immagine_corrente();
}

bool DocBrowser::init(void) {
    return true;
}

// ==================== RUN ====================

bool DocBrowser::run(void) {
    lv_area_t area = getVisualArea();
    screen_w = area.x2 - area.x1;
    screen_h = area.y2 - area.y1;

    main_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_container, screen_w, screen_h);
    lv_obj_set_style_pad_all(main_container, 0, 0);
    lv_obj_set_style_bg_color(main_container, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(main_container, 255, 0);
    lv_obj_clear_flag(main_container, LV_OBJ_FLAG_SCROLLABLE);

    crea_browser_ui();
    crea_viewer_ui();
    mostra_browser();

    current_path[0] = '\0';
    carica_directory("");

    return true;
}

// ==================== UI CREATION ====================

void DocBrowser::crea_browser_ui() {
    browser_panel = lv_obj_create(main_container);
    lv_obj_set_size(browser_panel, screen_w, screen_h);
    lv_obj_set_style_pad_all(browser_panel, 0, 0);
    lv_obj_set_style_bg_color(browser_panel, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(browser_panel, 255, 0);
    lv_obj_set_style_border_width(browser_panel, 0, 0);
    lv_obj_clear_flag(browser_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Header bar
    lv_obj_t *header = lv_obj_create(browser_panel);
    lv_obj_set_size(header, screen_w, 50);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x0f3460), 0);
    lv_obj_set_style_bg_opa(header, 255, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 8, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Bottone indietro
    back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 40, 34);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(back_btn, 6, 0);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_lbl);

    // Path label
    path_label = lv_label_create(header);
    lv_label_set_text(path_label, "/");
    lv_obj_set_style_text_color(path_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(path_label, &lv_font_montserrat_18, 0);
    lv_obj_align(path_label, LV_ALIGN_LEFT_MID, 50, 0);
    lv_label_set_long_mode(path_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(path_label, screen_w - 120);

    // Bottone cerca
    search_btn = lv_btn_create(header);
    lv_obj_set_size(search_btn, 40, 34);
    lv_obj_align(search_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(search_btn, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_radius(search_btn, 6, 0);
    lv_obj_add_event_cb(search_btn, search_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *search_lbl = lv_label_create(search_btn);
    lv_label_set_text(search_lbl, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_color(search_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(search_lbl);

    // Barra di ricerca (nascosta inizialmente)
    search_input = lv_textarea_create(browser_panel);
    lv_obj_set_size(search_input, screen_w - 8, 42);
    lv_obj_align(search_input, LV_ALIGN_TOP_LEFT, 4, 52);
    lv_textarea_set_placeholder_text(search_input, "Cerca...");
    lv_textarea_set_one_line(search_input, true);
    lv_obj_set_style_bg_color(search_input, lv_color_hex(0x0D1526), 0);
    lv_obj_set_style_text_color(search_input, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(search_input, lv_color_hex(0x667eea), 0);
    lv_obj_set_style_border_width(search_input, 2, 0);
    lv_obj_set_style_radius(search_input, 8, 0);
    lv_obj_set_style_text_font(search_input, &lv_font_montserrat_18, 0);
    lv_obj_add_flag(search_input, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(search_input, search_input_focus_cb, LV_EVENT_FOCUSED, this);

    // Tastiera (nascosta inizialmente)
    keyboard = lv_keyboard_create(browser_panel);
    lv_obj_set_size(keyboard, screen_w, screen_h / 2);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(keyboard, search_input);
    lv_obj_add_event_cb(keyboard, search_ready_cb, LV_EVENT_READY, this);

    // Lista file/cartelle
    file_list = lv_list_create(browser_panel);
    lv_obj_set_size(file_list, screen_w, screen_h - 50);
    lv_obj_align(file_list, LV_ALIGN_TOP_LEFT, 0, 50);
    lv_obj_set_style_bg_color(file_list, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(file_list, 255, 0);
    lv_obj_set_style_border_width(file_list, 0, 0);
    lv_obj_set_style_pad_all(file_list, 4, 0);

    // Spinner caricamento
    browser_spinner = lv_spinner_create(browser_panel, 1000, 60);
    lv_obj_set_size(browser_spinner, 50, 50);
    lv_obj_center(browser_spinner);
    lv_obj_add_flag(browser_spinner, LV_OBJ_FLAG_HIDDEN);
}

void DocBrowser::crea_viewer_ui() {
    viewer_panel = lv_obj_create(main_container);
    lv_obj_set_size(viewer_panel, screen_w, screen_h);
    lv_obj_set_style_pad_all(viewer_panel, 0, 0);
    lv_obj_set_style_bg_color(viewer_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(viewer_panel, 255, 0);
    lv_obj_set_style_border_width(viewer_panel, 0, 0);
    lv_obj_add_flag(viewer_panel, LV_OBJ_FLAG_HIDDEN);

    img_obj = lv_obj_create(viewer_panel);
    lv_obj_set_size(img_obj, screen_w, screen_h);
    lv_obj_set_style_bg_color(img_obj, lv_color_hex(0x111111), 0);
    lv_obj_set_style_pad_all(img_obj, 0, 0);
    lv_obj_set_style_border_width(img_obj, 0, 0);
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
    lv_label_set_text(page_overlay_label, "");
    lv_obj_set_style_text_font(page_overlay_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(page_overlay_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(page_overlay_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(page_overlay_label, 200, 0);
    lv_obj_set_style_pad_all(page_overlay_label, 15, 0);
    lv_obj_set_style_radius(page_overlay_label, 8, 0);
    lv_obj_align(page_overlay_label, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_flag(page_overlay_label, LV_OBJ_FLAG_HIDDEN);
}

// ==================== MODE SWITCH ====================

void DocBrowser::mostra_browser() {
    current_mode = MODE_BROWSER;
    lv_obj_clear_flag(browser_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(viewer_panel, LV_OBJ_FLAG_HIDDEN);
}

void DocBrowser::mostra_viewer() {
    current_mode = MODE_VIEWER;
    lv_obj_add_flag(browser_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(viewer_panel, LV_OBJ_FLAG_HIDDEN);
}

// ==================== DIRECTORY LOADING ====================

void DocBrowser::carica_directory(const char *path) {
    if (task_handle != nullptr) {
        ESP_LOGW(TAG, "Task in corso, ignoro");
        return;
    }

    task_in_progress = true;
    snprintf(current_path, DOC_MAX_PATH, "%s", path);

    if (lvgl_port_lock(0)) {
        lv_obj_clear_flag(browser_spinner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clean(file_list);

        if (strlen(current_path) == 0)
            lv_label_set_text(path_label, "/ Documenti");
        else
            lv_label_set_text_fmt(path_label, "/ %s", current_path);

        lvgl_port_unlock();
    }

    dir_task_params_t *params = (dir_task_params_t *)malloc(sizeof(dir_task_params_t));
    if (!params) {
        task_in_progress = false;
        return;
    }
    params->app = this;
    snprintf(params->path, DOC_MAX_PATH, "%s", path);

    BaseType_t res = xTaskCreatePinnedToCore(directory_task, "dir_task", 8192,
                                              params, 5, &task_handle, 1);
    if (res != pdPASS) {
        free(params);
        task_handle = nullptr;
        task_in_progress = false;
    }
}

void DocBrowser::directory_task(void *param) {
    dir_task_params_t *p = (dir_task_params_t *)param;
    DocBrowser *app = p->app;
    char path[DOC_MAX_PATH];
    snprintf(path, DOC_MAX_PATH, "%s", p->path);
    free(p);

    char url[384];
    if (strlen(path) > 0)
        snprintf(url, sizeof(url), "%s?path=%s", BASE_URL, path);
    else
        snprintf(url, sizeof(url), "%s", BASE_URL);

    ESP_LOGI(TAG, "Caricamento directory: %s", url);

    char *body = nullptr;
    bool success = false;

    if (app->download_json(url, &body) && body) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            cJSON *folders = cJSON_GetObjectItem(root, "folders");
            app->folder_count = 0;
            if (folders && cJSON_IsArray(folders)) {
                int n = cJSON_GetArraySize(folders);
                if (n > DOC_MAX_FOLDERS) n = DOC_MAX_FOLDERS;
                for (int i = 0; i < n; i++) {
                    cJSON *item = cJSON_GetArrayItem(folders, i);
                    if (item && cJSON_IsString(item)) {
                        snprintf(app->folder_names[app->folder_count],
                                DOC_MAX_NAME, "%s", item->valuestring);
                        app->folder_count++;
                    }
                }
            }

            cJSON *files = cJSON_GetObjectItem(root, "files");
            app->file_count = 0;
            if (files && cJSON_IsArray(files)) {
                int n = cJSON_GetArraySize(files);
                if (n > DOC_MAX_FILES) n = DOC_MAX_FILES;
                for (int i = 0; i < n; i++) {
                    cJSON *item = cJSON_GetArrayItem(files, i);
                    if (item && cJSON_IsString(item)) {
                        snprintf(app->file_names[app->file_count],
                                DOC_MAX_NAME, "%s", item->valuestring);
                        app->file_count++;
                    }
                }
            }

            cJSON_Delete(root);
            success = true;
            ESP_LOGI(TAG, "Directory OK: %d cartelle, %d file",
                     app->folder_count, app->file_count);
        }
        free(body);
    }

    if (lvgl_port_lock(0)) {
        lv_obj_add_flag(app->browser_spinner, LV_OBJ_FLAG_HIDDEN);

        if (success) {
            app->popola_lista();
        } else {
            lv_obj_t *btn = lv_list_add_btn(app->file_list, LV_SYMBOL_WARNING, "Errore caricamento");
            lv_obj_set_style_text_color(btn, lv_color_hex(0xFF4444), 0);
        }
        lvgl_port_unlock();
    }

    app->task_in_progress = false;
    app->task_handle = nullptr;
    vTaskDelete(NULL);
}

void DocBrowser::popola_lista() {
    lv_obj_clean(file_list);

    for (int i = 0; i < folder_count; i++) {
        lv_obj_t *btn = lv_list_add_btn(file_list, LV_SYMBOL_DIRECTORY, folder_names[i]);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x0f3460), 0);
        lv_obj_set_style_bg_opa(btn, 255, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_pad_ver(btn, 12, 0);
        lv_obj_add_event_cb(btn, folder_click_cb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
    }

    for (int i = 0; i < file_count; i++) {
        lv_obj_t *btn = lv_list_add_btn(file_list, LV_SYMBOL_IMAGE, file_names[i]);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x16213e), 0);
        lv_obj_set_style_bg_opa(btn, 255, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xDDDDDD), 0);
        lv_obj_set_style_pad_ver(btn, 12, 0);
        lv_obj_add_event_cb(btn, file_click_cb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
    }

    if (folder_count == 0 && file_count == 0) {
        lv_list_add_text(file_list, "Cartella vuota");
    }
}

// ==================== SEARCH ====================

void DocBrowser::esegui_ricerca(const char *query) {
    if (task_handle != nullptr) {
        ESP_LOGW(TAG, "Task in corso, ignoro ricerca");
        return;
    }

    if (!query || strlen(query) == 0) {
        // Query vuota: torna al listing normale
        chiudi_ricerca();
        carica_directory(current_path);
        return;
    }

    task_in_progress = true;
    search_mode = true;

    if (lvgl_port_lock(0)) {
        lv_obj_clear_flag(browser_spinner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clean(file_list);
        lv_label_set_text_fmt(path_label, "Cerca: %s", query);
        lvgl_port_unlock();
    }

    /* Riusa dir_task_params_t: path contiene la query di ricerca */
    dir_task_params_t *params = (dir_task_params_t *)malloc(sizeof(dir_task_params_t));
    if (!params) {
        task_in_progress = false;
        return;
    }
    params->app = this;
    snprintf(params->path, DOC_MAX_PATH, "%s", query);

    BaseType_t res = xTaskCreatePinnedToCore(
        [](void *param) {
            dir_task_params_t *p = (dir_task_params_t *)param;
            DocBrowser *app = p->app;
            char query[DOC_MAX_PATH];
            snprintf(query, DOC_MAX_PATH, "%s", p->path);
            free(p);

            // URL con parametro search, ricerca dalla root
            char url[512];
            snprintf(url, sizeof(url), "%s?search=%s", BASE_URL, query);

            ESP_LOGI(TAG, "Ricerca: %s", url);

            char *body = nullptr;
            bool success = false;

            if (app->download_json(url, &body) && body) {
                cJSON *root = cJSON_Parse(body);
                if (root) {
                    cJSON *folders = cJSON_GetObjectItem(root, "folders");
                    app->folder_count = 0;
                    if (folders && cJSON_IsArray(folders)) {
                        int n = cJSON_GetArraySize(folders);
                        if (n > DOC_MAX_FOLDERS) n = DOC_MAX_FOLDERS;
                        for (int i = 0; i < n; i++) {
                            cJSON *item = cJSON_GetArrayItem(folders, i);
                            if (item && cJSON_IsString(item)) {
                                snprintf(app->folder_names[app->folder_count],
                                        DOC_MAX_NAME, "%s", item->valuestring);
                                app->folder_count++;
                            }
                        }
                    }

                    cJSON *files = cJSON_GetObjectItem(root, "files");
                    app->file_count = 0;
                    if (files && cJSON_IsArray(files)) {
                        int n = cJSON_GetArraySize(files);
                        if (n > DOC_MAX_FILES) n = DOC_MAX_FILES;
                        for (int i = 0; i < n; i++) {
                            cJSON *item = cJSON_GetArrayItem(files, i);
                            if (item && cJSON_IsString(item)) {
                                snprintf(app->file_names[app->file_count],
                                        DOC_MAX_NAME, "%s", item->valuestring);
                                app->file_count++;
                            }
                        }
                    }

                    cJSON_Delete(root);
                    success = true;
                    ESP_LOGI(TAG, "Ricerca OK: %d cartelle, %d file",
                             app->folder_count, app->file_count);
                }
                free(body);
            }

            if (lvgl_port_lock(0)) {
                lv_obj_add_flag(app->browser_spinner, LV_OBJ_FLAG_HIDDEN);

                if (success) {
                    lv_obj_clean(app->file_list);

                    /* Risultati ricerca: i nomi sono path completi relativi.
                     * Usano callback diversi che navigano con path assoluto */
                    for (int i = 0; i < app->folder_count; i++) {
                        lv_obj_t *btn = lv_list_add_btn(app->file_list,
                            LV_SYMBOL_DIRECTORY, app->folder_names[i]);
                        lv_obj_set_style_bg_color(btn, lv_color_hex(0x0f3460), 0);
                        lv_obj_set_style_bg_opa(btn, 255, 0);
                        lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
                        lv_obj_set_style_pad_ver(btn, 12, 0);
                        lv_obj_add_event_cb(btn, search_result_folder_cb,
                            LV_EVENT_CLICKED, app);
                        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
                    }

                    for (int i = 0; i < app->file_count; i++) {
                        lv_obj_t *btn = lv_list_add_btn(app->file_list,
                            LV_SYMBOL_IMAGE, app->file_names[i]);
                        lv_obj_set_style_bg_color(btn, lv_color_hex(0x16213e), 0);
                        lv_obj_set_style_bg_opa(btn, 255, 0);
                        lv_obj_set_style_text_color(btn, lv_color_hex(0xDDDDDD), 0);
                        lv_obj_set_style_pad_ver(btn, 12, 0);
                        lv_obj_add_event_cb(btn, search_result_file_cb,
                            LV_EVENT_CLICKED, app);
                        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
                    }

                    if (app->folder_count == 0 && app->file_count == 0) {
                        lv_list_add_text(app->file_list, "Nessun risultato");
                    }
                } else {
                    lv_obj_t *btn = lv_list_add_btn(app->file_list,
                        LV_SYMBOL_WARNING, "Errore ricerca");
                    lv_obj_set_style_text_color(btn, lv_color_hex(0xFF4444), 0);
                }
                lvgl_port_unlock();
            }

            app->task_in_progress = false;
            app->task_handle = nullptr;
            vTaskDelete(NULL);
        },
        "search_task", 8192, params, 5, &task_handle, 1
    );

    if (res != pdPASS) {
        free(params);
        task_handle = nullptr;
        task_in_progress = false;
    }
}

void DocBrowser::chiudi_ricerca() {
    search_mode = false;

    if (lvgl_port_lock(0)) {
        lv_textarea_set_text(search_input, "");
        lv_obj_add_flag(search_input, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);

        // Ripristina dimensione lista (senza barra ricerca)
        lv_obj_set_size(file_list, screen_w, screen_h - 50);
        lv_obj_align(file_list, LV_ALIGN_TOP_LEFT, 0, 50);

        lvgl_port_unlock();
    }
}

// ==================== NAVIGATION ====================

void DocBrowser::naviga_cartella(const char *folder) {
    char new_path[DOC_MAX_PATH];
    if (strlen(current_path) > 0)
        snprintf(new_path, sizeof(new_path), "%s/%s", current_path, folder);
    else
        snprintf(new_path, sizeof(new_path), "%s", folder);

    carica_directory(new_path);
}

void DocBrowser::naviga_su() {
    if (strlen(current_path) == 0) return;

    char *last_slash = strrchr(current_path, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        current_path[0] = '\0';
    }
    carica_directory(current_path);
}

// ==================== IMAGE VIEWER ====================

void DocBrowser::apri_file(int index) {
    if (index < 0 || index >= file_count) return;
    current_file_index = index;
    mostra_viewer();
    carica_immagine(index);
}

void DocBrowser::carica_immagine(int index) {
    if (task_handle != nullptr) {
        ESP_LOGW(TAG, "Task in corso, ignoro");
        return;
    }

    task_in_progress = true;

    if (lvgl_port_lock(0)) {
        mostra_loading(true);
        lvgl_port_unlock();
    }

    img_task_params_t *params = (img_task_params_t *)malloc(sizeof(img_task_params_t));
    if (!params) {
        task_in_progress = false;
        return;
    }
    params->app = this;
    params->file_index = index;

    BaseType_t res = xTaskCreatePinnedToCore(image_task, "img_task", 8192,
                                              params, 5, &task_handle, 1);
    if (res != pdPASS) {
        free(params);
        task_handle = nullptr;
        task_in_progress = false;
    }
}

void DocBrowser::image_task(void *param) {
    img_task_params_t *p = (img_task_params_t *)param;
    DocBrowser *app = p->app;
    int index = p->file_index;
    free(p);

    /* In search_mode file_names contiene path completi relativi,
     * altrimenti solo il nome file nella directory corrente */
    char url[512];
    if (app->search_mode) {
        // Path completo dal server
        snprintf(url, sizeof(url), "%s%s", BASE_URL, app->file_names[index]);
    } else if (strlen(app->current_path) > 0) {
        snprintf(url, sizeof(url), "%s%s/%s", BASE_URL,
                 app->current_path, app->file_names[index]);
    } else {
        snprintf(url, sizeof(url), "%s%s", BASE_URL, app->file_names[index]);
    }

    ESP_LOGI(TAG, "Caricamento immagine: %s", url);

    uint8_t *jpg_buffer = nullptr;
    size_t jpg_size = 0;
    bool success = false;

    if (app->download_jpg(url, &jpg_buffer, &jpg_size)) {
        uint8_t *new_rgb = nullptr;
        int width, height;

        if (app->decode_jpg_hw(jpg_buffer, jpg_size, &new_rgb, &width, &height)) {
            app->current_file_index = index;

            if (lvgl_port_lock(0)) {
                uint8_t *old_rgb = app->rgb_buffer;
                lv_obj_t *old_widget = app->current_img_widget;

                app->rgb_buffer = new_rgb;

                int aw = (width + 15) & ~15;
                int ah = (height + 15) & ~15;

                app->img_dsc.header.w = aw;
                app->img_dsc.header.h = ah;
                app->img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
                app->img_dsc.data = new_rgb;
                app->img_dsc.data_size = aw * ah * 2;

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

                if (old_widget) {
                    if (lvgl_port_lock(0)) {
                        lv_obj_del(old_widget);
                        lvgl_port_unlock();
                    }
                }
                if (old_rgb) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    heap_caps_free(old_rgb);
                }

                ESP_LOGI(TAG, "Immagine %d visualizzata", index);
                success = true;
            } else {
                heap_caps_free(new_rgb);
            }
        }
        heap_caps_free(jpg_buffer);
    }

    if (!success) {
        ESP_LOGE(TAG, "Errore caricamento immagine %d", index);
        if (lvgl_port_lock(0)) {
            if (app->loading_spinner)
                lv_obj_add_flag(app->loading_spinner, LV_OBJ_FLAG_HIDDEN);
            if (app->error_label) {
                lv_label_set_text(app->error_label, "Errore caricamento");
                lv_obj_clear_flag(app->error_label, LV_OBJ_FLAG_HIDDEN);
            }
            lvgl_port_unlock();
        }
    }

    app->task_in_progress = false;
    app->task_handle = nullptr;
    vTaskDelete(NULL);
}

// ==================== HTTP ====================

esp_err_t DocBrowser::http_event_handler(esp_http_client_event_t *evt) {
    http_download_ctx_t *ctx = (http_download_ctx_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (ctx->size + evt->data_len <= ctx->capacity) {
            memcpy(ctx->buffer + ctx->size, evt->data, evt->data_len);
            ctx->size += evt->data_len;
        }
    }
    return ESP_OK;
}

bool DocBrowser::download_json(const char *url, char **out_body) {
    uint8_t *buffer = (uint8_t *)heap_caps_malloc(16384, MALLOC_CAP_SPIRAM);
    if (!buffer) return false;

    http_download_ctx_t ctx = { .buffer = buffer, .size = 0, .capacity = 16384 };

    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.user_data = &ctx;
    config.timeout_ms = HTTP_TIMEOUT_MS;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { heap_caps_free(buffer); return false; }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        heap_caps_free(buffer);
        return false;
    }

    buffer[ctx.size] = '\0';
    *out_body = (char *)buffer;
    return true;
}

bool DocBrowser::download_jpg(const char *url, uint8_t **out_buffer, size_t *out_size) {
    uint8_t *buffer = (uint8_t *)heap_caps_malloc(JPG_BUFFER_MAX, MALLOC_CAP_SPIRAM);
    if (!buffer) return false;

    http_download_ctx_t ctx = { .buffer = buffer, .size = 0, .capacity = JPG_BUFFER_MAX };

    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.user_data = &ctx;
    config.timeout_ms = HTTP_TIMEOUT_MS;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { heap_caps_free(buffer); return false; }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "HTTP failed: %s (status %d)", esp_err_to_name(err), status);
        heap_caps_free(buffer);
        return false;
    }

    *out_buffer = buffer;
    *out_size = ctx.size;
    return true;
}

bool DocBrowser::decode_jpg_hw(uint8_t *jpg_data, size_t jpg_len,
                                uint8_t **out_rgb, int *width, int *height) {
    jpeg_decoder_handle_t decoder;
    jpeg_decode_engine_cfg_t eng_cfg = { .timeout_ms = JPEG_DECODE_TIMEOUT_MS };

    if (jpeg_new_decoder_engine(&eng_cfg, &decoder) != ESP_OK) return false;

    jpeg_decode_picture_info_t info;
    if (jpeg_decoder_get_info(jpg_data, jpg_len, &info) != ESP_OK) {
        jpeg_del_decoder_engine(decoder);
        return false;
    }

    *width = info.width;
    *height = info.height;
    int aw = (*width + 15) & ~15;
    int ah = (*height + 15) & ~15;

    size_t req_size = aw * ah * 2;
    size_t actual = 0;
    jpeg_decode_memory_alloc_cfg_t mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER
    };

    uint8_t *rgb = (uint8_t *)jpeg_alloc_decoder_mem(req_size, &mem_cfg, &actual);
    if (!rgb) { jpeg_del_decoder_engine(decoder); return false; }

    jpeg_decode_cfg_t dec_cfg = {};
    dec_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    dec_cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
    dec_cfg.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;

    uint32_t decoded = 0;
    esp_err_t ret = jpeg_decoder_process(decoder, &dec_cfg, jpg_data, jpg_len,
                                          rgb, actual, &decoded);
    jpeg_del_decoder_engine(decoder);

    if (ret != ESP_OK) { heap_caps_free(rgb); return false; }

    *out_rgb = rgb;
    return true;
}

// ==================== IMAGE HELPERS ====================

void DocBrowser::libera_immagine_corrente() {
    if (current_img_widget) {
        lv_obj_del(current_img_widget);
        current_img_widget = nullptr;
    }
    if (rgb_buffer) {
        heap_caps_free(rgb_buffer);
        rgb_buffer = nullptr;
    }
}

void DocBrowser::mostra_loading(bool show) {
    if (!loading_spinner) return;
    if (show) lv_obj_clear_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(loading_spinner, LV_OBJ_FLAG_HIDDEN);
}

void DocBrowser::mostra_errore(const char *msg) {
    if (!error_label) return;
    lv_label_set_text(error_label, msg);
    lv_obj_clear_flag(error_label, LV_OBJ_FLAG_HIDDEN);
}

void DocBrowser::mostra_numero_pagina() {
    if (!page_overlay_label) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d / %d", current_file_index + 1, file_count);
    lv_label_set_text(page_overlay_label, buf);
    lv_obj_clear_flag(page_overlay_label, LV_OBJ_FLAG_HIDDEN);

    if (overlay_timer) lv_timer_del(overlay_timer);
    overlay_timer = lv_timer_create(overlay_timer_cb, OVERLAY_DISPLAY_TIME_MS, this);
    lv_timer_set_repeat_count(overlay_timer, 1);
}

void DocBrowser::nascondi_numero_pagina() {
    if (page_overlay_label)
        lv_obj_add_flag(page_overlay_label, LV_OBJ_FLAG_HIDDEN);
    if (overlay_timer) {
        lv_timer_del(overlay_timer);
        overlay_timer = nullptr;
    }
}

// ==================== CALLBACKS ====================

void DocBrowser::folder_click_cb(lv_event_t *e) {
    DocBrowser *app = (DocBrowser *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);

    if (app && idx >= 0 && idx < app->folder_count) {
        ESP_LOGI(TAG, "Apro cartella: %s", app->folder_names[idx]);
        app->naviga_cartella(app->folder_names[idx]);
    }
}

void DocBrowser::file_click_cb(lv_event_t *e) {
    DocBrowser *app = (DocBrowser *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);

    if (app && idx >= 0 && idx < app->file_count) {
        ESP_LOGI(TAG, "Apro file: %s", app->file_names[idx]);
        app->apri_file(idx);
    }
}

void DocBrowser::back_btn_cb(lv_event_t *e) {
    DocBrowser *app = (DocBrowser *)lv_event_get_user_data(e);
    if (!app) return;

    /* Se siamo in modalita' ricerca, esci dalla ricerca */
    if (app->search_mode) {
        app->chiudi_ricerca();
        app->carica_directory(app->current_path);
        return;
    }
    app->naviga_su();
}

void DocBrowser::search_btn_cb(lv_event_t *e) {
    DocBrowser *app = (DocBrowser *)lv_event_get_user_data(e);
    if (!app) return;

    bool is_visible = !lv_obj_has_flag(app->search_input, LV_OBJ_FLAG_HIDDEN);

    if (is_visible) {
        /* Chiudi barra ricerca */
        app->chiudi_ricerca();
        app->carica_directory(app->current_path);
    } else {
        /* Mostra barra ricerca e ridimensiona lista */
        lv_obj_clear_flag(app->search_input, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(app->file_list, app->screen_w, app->screen_h - 96);
        lv_obj_align(app->file_list, LV_ALIGN_TOP_LEFT, 0, 96);
    }
}

void DocBrowser::search_input_focus_cb(lv_event_t *e) {
    DocBrowser *app = (DocBrowser *)lv_event_get_user_data(e);
    if (!app) return;

    /* Mostra tastiera e riduci lista per fare spazio */
    lv_obj_clear_flag(app->keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(app->file_list, app->screen_w,
                    app->screen_h / 2 - 96);
}

void DocBrowser::search_ready_cb(lv_event_t *e) {
    DocBrowser *app = (DocBrowser *)lv_event_get_user_data(e);
    if (!app) return;

    /* Nascondi tastiera */
    lv_obj_add_flag(app->keyboard, LV_OBJ_FLAG_HIDDEN);

    /* Ripristina dimensione lista */
    lv_obj_set_size(app->file_list, app->screen_w, app->screen_h - 96);
    lv_obj_align(app->file_list, LV_ALIGN_TOP_LEFT, 0, 96);

    /* Lancia ricerca */
    const char *query = lv_textarea_get_text(app->search_input);
    app->esegui_ricerca(query);
}

void DocBrowser::search_result_folder_cb(lv_event_t *e) {
    DocBrowser *app = (DocBrowser *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);

    if (!app || idx < 0 || idx >= app->folder_count) return;

    ESP_LOGI(TAG, "Ricerca -> cartella: %s", app->folder_names[idx]);

    /* Esci dalla ricerca e naviga al path completo */
    app->chiudi_ricerca();
    app->carica_directory(app->folder_names[idx]);
}

void DocBrowser::search_result_file_cb(lv_event_t *e) {
    DocBrowser *app = (DocBrowser *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);

    if (!app || idx < 0 || idx >= app->file_count) return;

    ESP_LOGI(TAG, "Ricerca -> file: %s", app->file_names[idx]);

    /* In search_mode file_names contiene path completi,
     * apri direttamente tramite il viewer */
    app->mostra_viewer();
    app->carica_immagine(idx);
}

void DocBrowser::gesture_event_cb(lv_event_t *e) {
    DocBrowser *app = (DocBrowser *)lv_event_get_user_data(e);
    if (!app || app->task_in_progress || app->task_handle != nullptr) return;

    static lv_point_t start_point = {0, 0};
    static bool is_pressing = false;

    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    if (code == LV_EVENT_PRESSING) {
        if (!is_pressing) {
            start_point = pt;
            is_pressing = true;
        }
    } else if (code == LV_EVENT_RELEASED) {
        if (is_pressing) {
            int32_t dx = pt.x - start_point.x;
            int32_t dy = pt.y - start_point.y;

            if (abs(dx) > abs(dy) && abs(dx) > SWIPE_THRESHOLD_PX) {
                if (dx < 0) {
                    if (app->current_file_index < app->file_count - 1) {
                        app->carica_immagine(app->current_file_index + 1);
                    }
                } else {
                    if (app->current_file_index > 0) {
                        app->carica_immagine(app->current_file_index - 1);
                    }
                }
            }
            is_pressing = false;
            start_point = {0, 0};
        }
    }
}

void DocBrowser::overlay_timer_cb(lv_timer_t *timer) {
    DocBrowser *app = (DocBrowser *)timer->user_data;
    if (app) app->nascondi_numero_pagina();
}

// ==================== LIFECYCLE ====================

bool DocBrowser::back() {
    if (current_mode == MODE_VIEWER) {
        libera_immagine_corrente();
        if (lvgl_port_lock(0)) {
            mostra_browser();
            lvgl_port_unlock();
        }
        return false;
    }

    /* Se in ricerca, esci dalla ricerca */
    if (search_mode) {
        chiudi_ricerca();
        carica_directory(current_path);
        return false;
    }

    if (strlen(current_path) > 0) {
        naviga_su();
        return false;
    }

    if (main_container) {
        lv_obj_del(main_container);
        main_container = nullptr;
    }
    notifyCoreClosed();
    return true;
}

bool DocBrowser::close() {
    return true;
}