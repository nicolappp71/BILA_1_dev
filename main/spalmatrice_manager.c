#include "spalmatrice_manager.h"
#include "dxf_parser.h"
#include "path_chain.h"
#include "http_client.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "SPAL";

// ─── Stato interno ────────────────────────────────────────────────────────────
static spalmatrice_state_t s_state     = SPAL_STATE_IDLE;
static path_point_t       *s_path      = NULL;
static int                 s_path_pts  = 0;
static int                 s_path_cur  = 0;
static SemaphoreHandle_t   s_mutex     = NULL;
static TaskHandle_t        s_run_task  = NULL;
static volatile bool       s_stop_req  = false;
static volatile bool       s_pause_req = false;

// ─── Posizione assi in steps ──────────────────────────────────────────────────
static int32_t s_pos_x = 0;
static int32_t s_pos_y = 0;

// ─── GPIO helpers ─────────────────────────────────────────────────────────────
static inline void step_x(int dir)
{
    gpio_set_level(SPAL_DIR_X_GPIO, dir > 0 ? 1 : 0);
    gpio_set_level(SPAL_PUL_X_GPIO, 1);
    esp_rom_delay_us(2);
    gpio_set_level(SPAL_PUL_X_GPIO, 0);
    s_pos_x += dir;
}

static inline void step_y(int dir)
{
    gpio_set_level(SPAL_DIR_Y_GPIO, dir > 0 ? 1 : 0);
    gpio_set_level(SPAL_PUL_Y_GPIO, 1);
    esp_rom_delay_us(2);
    gpio_set_level(SPAL_PUL_Y_GPIO, 0);
    s_pos_y += dir;
}

// ─── Bresenham 2D ─────────────────────────────────────────────────────────────
// Muove da posizione corrente a (tx, ty) in steps. Blocca il task chiamante.
static void move_to_steps(int32_t tx, int32_t ty)
{
    int32_t dx = llabs(tx - s_pos_x);
    int32_t dy = llabs(ty - s_pos_y);
    int sx = (tx > s_pos_x) ? 1 : -1;
    int sy = (ty > s_pos_y) ? 1 : -1;
    int32_t err = dx - dy;

    uint32_t period_us = 1000000UL / SPAL_SPEED_STEPS_S;

    while (!s_stop_req) {
        if (s_pos_x == tx && s_pos_y == ty) break;

        int32_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; step_x(sx); }
        if (e2 <  dx) { err += dx; step_y(sy); }

        esp_rom_delay_us(period_us);

        while (s_pause_req && !s_stop_req)
            vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ─── Task esecuzione percorso ─────────────────────────────────────────────────
static void run_task(void *arg)
{
    gpio_set_level(SPAL_EN_X_GPIO, 0);   // enable driver (active low TB6600)
    gpio_set_level(SPAL_EN_Y_GPIO, 0);
    spalmatrice_manager_pump_on();

    for (int i = 1; i < s_path_pts && !s_stop_req; i++) {
        s_path_cur = i;
        int32_t tx = (int32_t)(s_path[i].x * SPAL_STEPS_PER_MM_X);
        int32_t ty = (int32_t)(s_path[i].y * SPAL_STEPS_PER_MM_Y);
        move_to_steps(tx, ty);
    }

    spalmatrice_manager_pump_off();
    gpio_set_level(SPAL_EN_X_GPIO, 1);   // disable driver
    gpio_set_level(SPAL_EN_Y_GPIO, 1);

    s_state = s_stop_req ? SPAL_STATE_IDLE : SPAL_STATE_DONE;
    ESP_LOGI(TAG, "Percorso %s", s_stop_req ? "interrotto" : "completato");
    s_run_task = NULL;
    vTaskDelete(NULL);
}

// ─── Homing task ─────────────────────────────────────────────────────────────
static void homing_task(void *arg)
{
    uint32_t period_us = 1000000UL / SPAL_HOMING_SPEED;

    // Homing X: muovi verso home finché finecorsa LOW
    gpio_set_level(SPAL_DIR_X_GPIO, 0);
    while (gpio_get_level(SPAL_HOME_X_GPIO) != 0) {
        gpio_set_level(SPAL_PUL_X_GPIO, 1);
        esp_rom_delay_us(2);
        gpio_set_level(SPAL_PUL_X_GPIO, 0);
        esp_rom_delay_us(period_us);
    }

    // Homing Y
    gpio_set_level(SPAL_DIR_Y_GPIO, 0);
    while (gpio_get_level(SPAL_HOME_Y_GPIO) != 0) {
        gpio_set_level(SPAL_PUL_Y_GPIO, 1);
        esp_rom_delay_us(2);
        gpio_set_level(SPAL_PUL_Y_GPIO, 0);
        esp_rom_delay_us(period_us);
    }

    s_pos_x = 0;
    s_pos_y = 0;
    s_state = SPAL_STATE_READY;
    ESP_LOGI(TAG, "Homing completato");
    vTaskDelete(NULL);
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void spalmatrice_manager_init(void)
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();

    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << SPAL_EN_X_GPIO)  |
                        (1ULL << SPAL_DIR_X_GPIO)  |
                        (1ULL << SPAL_PUL_X_GPIO)  |
                        (1ULL << SPAL_EN_Y_GPIO)   |
                        (1ULL << SPAL_DIR_Y_GPIO)  |
                        (1ULL << SPAL_PUL_Y_GPIO)  |
                        (1ULL << SPAL_RELAY_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << SPAL_HOME_X_GPIO) |
                        (1ULL << SPAL_HOME_Y_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_cfg);

    // Driver disabilitati all'avvio
    gpio_set_level(SPAL_EN_X_GPIO, 1);
    gpio_set_level(SPAL_EN_Y_GPIO, 1);
    gpio_set_level(SPAL_RELAY_GPIO, 0);

    s_state = SPAL_STATE_IDLE;
    ESP_LOGI(TAG, "Inizializzato");
}

// ─── Fetch DXF da XAMPP, parsea e logga coordinate ───────────────────────────
static void fetch_parse_task(void *arg)
{
    char *url = (char *)arg;
    char *buf = NULL;
    size_t len = 0;

    ESP_LOGI(TAG, "Download DXF: %s", url);
    esp_err_t ret = http_get_file_psram(url, &buf, &len);
    free(url);

    if (ret != ESP_OK || !buf) {
        ESP_LOGE(TAG, "Download fallito");
        vTaskDelete(NULL);
        return;
    }

    ret = spalmatrice_manager_load_dxf(buf, len);
    free(buf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Parse DXF fallito");
        vTaskDelete(NULL);
        return;
    }

    // Logga i primi 20 punti del percorso per verifica
    ESP_LOGI(TAG, "=== PERCORSO (%d punti) ===", s_path_pts);
    int show = s_path_pts < 20 ? s_path_pts : 20;
    for (int i = 0; i < show; i++) {
        ESP_LOGI(TAG, "  [%3d] X=%.3f  Y=%.3f", i, s_path[i].x, s_path[i].y);
    }
    if (s_path_pts > 20)
        ESP_LOGI(TAG, "  ... altri %d punti", s_path_pts - 20);
    ESP_LOGI(TAG, "=========================");

    vTaskDelete(NULL);
}

esp_err_t spalmatrice_manager_fetch_and_parse(const char *url)
{
    if (!url) return ESP_ERR_INVALID_ARG;
    char *url_copy = strdup(url);
    if (!url_copy) return ESP_ERR_NO_MEM;
    BaseType_t ok = xTaskCreate(fetch_parse_task, "spal_fetch", 12288, url_copy, 5, NULL);
    ESP_LOGE(TAG, "xTaskCreate spal_fetch: %s", ok == pdPASS ? "OK" : "FAIL");
    return ESP_OK;
}

// ─── Load DXF ────────────────────────────────────────────────────────────────
esp_err_t spalmatrice_manager_load_dxf(const char *buf, size_t len)
{
    dxf_segment_t *segs  = NULL;
    int            nsegs = 0;

    esp_err_t ret = dxf_parse(buf, len, SPAL_DXF_LAYER, &segs, &nsegs);
    if (ret != ESP_OK || nsegs == 0) {
        ESP_LOGE(TAG, "Parse DXF fallito o nessun segmento layer '%s'", SPAL_DXF_LAYER);
        if (segs) free(segs);
        return ESP_FAIL;
    }

    path_point_t *path  = NULL;
    int           npts  = 0;
    ret = path_chain(segs, nsegs, &path, &npts);
    free(segs);

    if (ret != ESP_OK || npts == 0) {
        ESP_LOGE(TAG, "Path chain fallito");
        if (path) free(path);
        return ESP_FAIL;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000))) {
        if (s_path) free(s_path);
        s_path     = path;
        s_path_pts = npts;
        s_path_cur = 0;
        s_state    = SPAL_STATE_DXF_LOADED;
        xSemaphoreGive(s_mutex);
    }

    ESP_LOGI(TAG, "DXF caricato: %d punti", npts);
    return ESP_OK;
}

// ─── Homing ───────────────────────────────────────────────────────────────────
esp_err_t spalmatrice_manager_home(void)
{
    if (s_state == SPAL_STATE_RUNNING) return ESP_FAIL;
    s_state = SPAL_STATE_HOMING;
    xTaskCreate(homing_task, "spal_home", 4096, NULL, 6, NULL);
    return ESP_OK;
}

// ─── Start ────────────────────────────────────────────────────────────────────
esp_err_t spalmatrice_manager_start(void)
{
    if (s_state != SPAL_STATE_READY && s_state != SPAL_STATE_DXF_LOADED) return ESP_FAIL;
    if (!s_path || s_path_pts < 2) return ESP_FAIL;
    s_stop_req  = false;
    s_pause_req = false;
    s_path_cur  = 0;
    s_state     = SPAL_STATE_RUNNING;
    xTaskCreate(run_task, "spal_run", 4096, NULL, 6, &s_run_task);
    return ESP_OK;
}

// ─── Pause / Resume / Stop ───────────────────────────────────────────────────
esp_err_t spalmatrice_manager_pause(void)
{
    if (s_state != SPAL_STATE_RUNNING) return ESP_FAIL;
    s_pause_req = true;
    s_state = SPAL_STATE_PAUSED;
    return ESP_OK;
}

esp_err_t spalmatrice_manager_resume(void)
{
    if (s_state != SPAL_STATE_PAUSED) return ESP_FAIL;
    s_pause_req = false;
    s_state = SPAL_STATE_RUNNING;
    return ESP_OK;
}

void spalmatrice_manager_stop(void)
{
    s_stop_req = true;
    s_state = SPAL_STATE_IDLE;
}

// ─── Pompa ────────────────────────────────────────────────────────────────────
void spalmatrice_manager_pump_on(void)  { gpio_set_level(SPAL_RELAY_GPIO, 1); }
void spalmatrice_manager_pump_off(void) { gpio_set_level(SPAL_RELAY_GPIO, 0); }

// ─── Getters ──────────────────────────────────────────────────────────────────
spalmatrice_state_t spalmatrice_manager_get_state(void)        { return s_state; }
int spalmatrice_manager_get_point_count(void)                  { return s_path_pts; }
int spalmatrice_manager_get_point_current(void)                { return s_path_cur; }
