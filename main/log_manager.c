#include "log_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

#define LOG_LINE_LEN 160
#define LOG_RING_SIZE 500
#define LOG_SD_QUEUE_LEN 64
#define LOG_SD_PATH "/sdcard/logs"

// ─── Ring buffer in PSRAM ─────────────────────────────────
static char (*s_ring)[LOG_LINE_LEN] = NULL;
static uint32_t s_ring_head = 0;
static uint32_t s_total = 0;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// ─── Queue verso task SD ──────────────────────────────────
static QueueHandle_t s_sd_queue = NULL;
static bool s_sd_ready = false;

// ─── Task Handle per scudo anti-ricorsione ────────────────
static TaskHandle_t s_sd_task_handle = NULL;

// ─── vprintf originale ───────────────────────────────────
static vprintf_like_t s_orig_vprintf = NULL;

// ─────────────────────────────────────────────────────────
// HOOK vprintf
// ─────────────────────────────────────────────────────────
static int log_vprintf_hook(const char *fmt, va_list args)
{
    // Scrivi sulla UART come prima
    int ret = 0;
    if (s_orig_vprintf)
        ret = s_orig_vprintf(fmt, args);

    if (!s_ring)
        return ret;

    // SCUDO ANTI-RICORSIONE: Se il log è generato dal task che scrive sulla SD,
    // ignoralo per l'accodamento su SD (altrimenti loop infinito FatFS -> Log -> FatFS).
    if (xTaskGetCurrentTaskHandle() == s_sd_task_handle)
    {
        return ret;
    }

    char tmp[LOG_LINE_LEN];
    va_list args2;
    va_copy(args2, args);
    vsnprintf(tmp, sizeof(tmp), fmt, args2);
    va_end(args2);

    // Rimuovi newline finale
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';

    // Sezione critica leggera — non usa stack FreeRTOS
    taskENTER_CRITICAL(&s_mux);
    snprintf(s_ring[s_ring_head], LOG_LINE_LEN, "%s", tmp);
    s_ring_head = (s_ring_head + 1) % LOG_RING_SIZE;
    s_total++;
    taskEXIT_CRITICAL(&s_mux);

    // Invia alla queue SD (non bloccante, fuori dalla sezione critica)
    if (s_sd_ready && s_sd_queue)
    {
        char *sd_line = malloc(LOG_LINE_LEN);
        if (sd_line)
        {
            snprintf(sd_line, LOG_LINE_LEN, "%s", tmp);
            if (xQueueSend(s_sd_queue, &sd_line, 0) != pdTRUE)
                free(sd_line); // queue piena, droppa
        }
    }

    return ret;
}

// ─────────────────────────────────────────────────────────
// TASK SD WRITER
// ─────────────────────────────────────────────────────────
static void sd_writer_task(void *arg)
{
    FILE *f = NULL;

    while (1)
    {
        char *line = NULL;
        if (xQueueReceive(s_sd_queue, &line, pdMS_TO_TICKS(5000)) == pdTRUE)
        {
            uint32_t tick_s = xTaskGetTickCount() / configTICK_RATE_HZ;
            uint32_t hh = (tick_s / 3600) % 24;
            uint32_t mm = (tick_s / 60) % 60;

            if (f == NULL)
            {
                mkdir(LOG_SD_PATH, 0755);
                f = fopen("/sdcard/logs/current.log", "a");
            }

            if (f)
            {
                fprintf(f, "[%02lu:%02lu] %s\n", hh, mm, line);
                fflush(f);
            }

            free(line);
        }
        else
        {
            // Timeout — chiudi file per flush sicuro
            if (f)
            {
                fclose(f);
                f = NULL;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────
// API PUBBLICA
// ─────────────────────────────────────────────────────────
esp_err_t log_manager_init(void)
{
    s_ring = heap_caps_malloc(LOG_RING_SIZE * LOG_LINE_LEN, MALLOC_CAP_SPIRAM);
    if (!s_ring)
        return ESP_ERR_NO_MEM;

    memset(s_ring, 0, LOG_RING_SIZE * LOG_LINE_LEN);

    s_sd_queue = xQueueCreate(LOG_SD_QUEUE_LEN, sizeof(char *));
    if (!s_sd_queue)
        return ESP_ERR_NO_MEM;

    // Salviamo l'handle del task per l'anti-ricorsione
    xTaskCreatePinnedToCore(sd_writer_task, "log_sd", 4096, NULL, 2, &s_sd_task_handle, 1);

    // Settiamo l'hook solo DOPO aver creato il task, così s_sd_task_handle è valido
    s_orig_vprintf = esp_log_set_vprintf(log_vprintf_hook);

    return ESP_OK;
}

void log_manager_sd_ready(void)
{
    s_sd_ready = true;
}

uint32_t log_manager_get_total(void)
{
    return s_total;
}

uint32_t log_manager_get_lines(uint32_t from, char *out_buf, size_t buf_size, uint32_t *last_idx)
{
    if (!s_ring)
        return 0;

    uint32_t count = 0;
    size_t written = 0;
    out_buf[0] = '\0';

    uint32_t oldest_idx = (s_total >= LOG_RING_SIZE) ? (s_total - LOG_RING_SIZE) : 0;
    if (from < oldest_idx)
        from = oldest_idx;

    taskENTER_CRITICAL(&s_mux);
    for (uint32_t i = from; i < s_total && written < buf_size - LOG_LINE_LEN; i++)
    {
        uint32_t ring_pos = i % LOG_RING_SIZE;
        const char *line = s_ring[ring_pos];
        if (strlen(line) == 0)
            continue;
        written += snprintf(out_buf + written, buf_size - written, "%s\n", line);
        count++;
    }
    taskEXIT_CRITICAL(&s_mux);

    *last_idx = s_total;
    return count;
}