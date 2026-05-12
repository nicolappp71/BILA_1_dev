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
#include <math.h>
#include <stdio.h>

static const char *TAG = "SPAL";

// ─── Stato interno ────────────────────────────────────────────────────────────
static spalmatrice_state_t s_state     = SPAL_STATE_IDLE;
static path_point_t       *s_path      = NULL;
static int                 s_path_pts  = 0;
static int                 s_path_cur  = 0;
static dxf_segment_t      *s_segs      = NULL;   // segmenti raw per rendering UI
static int                 s_nsegs     = 0;
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

// ─── Carica DXF da SD card ────────────────────────────────────────────────────
esp_err_t spalmatrice_manager_load_dxf_from_sd(const char *filename)
{
    char path[64];
    snprintf(path, sizeof(path), "/sdcard/%s", filename);

    FILE *f = fopen(path, "rb");
    if (!f) { ESP_LOGE(TAG, "File SD non trovato: %s", path); return ESP_FAIL; }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 8 * 1024 * 1024) {
        ESP_LOGE(TAG, "Dimensione file non valida: %ld byte", fsize);
        fclose(f); return ESP_FAIL;
    }

    char *buf = (char *)heap_caps_malloc((size_t)fsize + 1, MALLOC_CAP_SPIRAM);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }

    size_t read = fread(buf, 1, (size_t)fsize, f);
    fclose(f);
    buf[read] = '\0';

    ESP_LOGI(TAG, "Letti %zu byte da %s", read, path);
    esp_err_t ret = spalmatrice_manager_load_dxf(buf, read);
    free(buf);
    return ret;
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
    ESP_LOGI(TAG, "xTaskCreate spal_fetch: %s", ok == pdPASS ? "OK" : "FAIL");
    return ESP_OK;
}

// ─── Load DXF ────────────────────────────────────────────────────────────────
esp_err_t spalmatrice_manager_load_dxf(const char *buf, size_t len)
{
    dxf_segment_t *segs  = NULL;
    int            nsegs = 0;

    esp_err_t ret = dxf_parse(buf, len, SPAL_DXF_LAYER, &segs, &nsegs);
    ESP_LOGI(TAG, "=== SEGMENTI RAW (%d) ===", nsegs);
    for (int _i = 0; _i < nsegs && _i < 30; _i++)
        ESP_LOGI(TAG, "  [%3d] (%.2f,%.2f)->(%.2f,%.2f)",
                 _i, segs[_i].x0, segs[_i].y0, segs[_i].x1, segs[_i].y1);
    if (ret != ESP_OK || nsegs == 0) {
        ESP_LOGE(TAG, "Parse DXF fallito o nessun segmento layer '%s'", SPAL_DXF_LAYER);
        if (segs) free(segs);
        return ESP_FAIL;
    }

    path_point_t *path  = NULL;
    int           npts  = 0;
    ret = path_chain(segs, nsegs, &path, &npts);
    // Non liberiamo segs qui: verrà salvato in s_segs e liberato al prossimo caricamento

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
        // Conserva i segmenti raw per il rendering UI (senza travel moves)
        if (s_segs) free(s_segs);
        s_segs  = segs;
        s_nsegs = nsegs;
        segs = NULL;   // trasferisce ownership — non liberare dopo
        s_state = SPAL_STATE_DXF_LOADED;
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

void spalmatrice_manager_get_path(const path_point_t **out_path, int *out_npts)
{
    *out_path = s_path;
    *out_npts = s_path_pts;
}

void spalmatrice_manager_get_segments(const dxf_segment_t **out_segs, int *out_nsegs)
{
    *out_segs  = s_segs;
    *out_nsegs = s_nsegs;
}

// ─── Edit point ───────────────────────────────────────────────────────────────
esp_err_t spalmatrice_manager_edit_path_point(int idx, float nx, float ny)
{
    if (!s_path || idx < 0 || idx >= s_path_pts) return ESP_ERR_INVALID_ARG;

    float ox = s_path[idx].x;
    float oy = s_path[idx].y;

    // Aggiorna s_path
    s_path[idx].x = nx;
    s_path[idx].y = ny;

    // Aggiorna anche i segmenti raw corrispondenti (tolleranza 0.1mm)
    int count = 0;
    if (s_segs) {
        for (int i = 0; i < s_nsegs; i++) {
            if (fabsf(s_segs[i].x0 - ox) < 0.1f && fabsf(s_segs[i].y0 - oy) < 0.1f) {
                s_segs[i].x0 = nx; s_segs[i].y0 = ny; count++;
            }
            if (fabsf(s_segs[i].x1 - ox) < 0.1f && fabsf(s_segs[i].y1 - oy) < 0.1f) {
                s_segs[i].x1 = nx; s_segs[i].y1 = ny; count++;
            }
        }
    }
    ESP_LOGI(TAG, "edit_path_point[%d] (%.3f,%.3f)→(%.3f,%.3f), segs aggiornati: %d",
             idx, ox, oy, nx, ny, count);
    return ESP_OK;
}

// ─── Save / Load SD ───────────────────────────────────────────────────────────
#define SPAL_SD_PATH "/sdcard/spal_percorso.seg"
#define SPAL_SD_MAGIC 0x53454753u  // "SEGS"

esp_err_t spalmatrice_manager_save_to_sd(void)
{
    if (!s_segs || s_nsegs <= 0) return ESP_FAIL;
    FILE *f = fopen(SPAL_SD_PATH, "wb");
    if (!f) { ESP_LOGE(TAG, "Impossibile aprire %s in scrittura", SPAL_SD_PATH); return ESP_FAIL; }
    uint32_t magic = SPAL_SD_MAGIC;
    fwrite(&magic,   sizeof(magic),          1,        f);
    fwrite(&s_nsegs, sizeof(s_nsegs),        1,        f);
    fwrite(s_segs,   sizeof(dxf_segment_t),  s_nsegs,  f);
    fclose(f);
    ESP_LOGI(TAG, "Salvati %d segmenti su %s", s_nsegs, SPAL_SD_PATH);
    return ESP_OK;
}

esp_err_t spalmatrice_manager_load_from_sd(void)
{
    FILE *f = fopen(SPAL_SD_PATH, "rb");
    if (!f) { ESP_LOGW(TAG, "File %s non trovato", SPAL_SD_PATH); return ESP_FAIL; }

    uint32_t magic = 0;
    int nsegs = 0;
    fread(&magic,  sizeof(magic),  1, f);
    fread(&nsegs,  sizeof(nsegs),  1, f);
    if (magic != SPAL_SD_MAGIC || nsegs <= 0 || nsegs > 200000) {
        ESP_LOGE(TAG, "File SD corrotto (magic=%08lx nsegs=%d)", (unsigned long)magic, nsegs);
        fclose(f); return ESP_FAIL;
    }

    dxf_segment_t *segs = (dxf_segment_t *)heap_caps_malloc(
        nsegs * sizeof(dxf_segment_t), MALLOC_CAP_SPIRAM);
    if (!segs) { fclose(f); return ESP_ERR_NO_MEM; }
    fread(segs, sizeof(dxf_segment_t), nsegs, f);
    fclose(f);

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000))) {
        if (s_segs) free(s_segs);
        s_segs  = segs;
        s_nsegs = nsegs;
        s_state = SPAL_STATE_DXF_LOADED;
        xSemaphoreGive(s_mutex);
    } else {
        free(segs); return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Caricati %d segmenti da SD", nsegs);
    return ESP_OK;
}
