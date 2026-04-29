#include "time_manager.h"
#include "http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mode.h"
#include "wifi_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#define TIME_SAVE_PATH       "/sdcard/last_time.bin"
#define TIME_SAVE_INTERVAL_MS 60000
#define TIME_RESTORE_OFFSET_S 60
#define TIME_MIN_VALID        1672531200   /* 2023-01-01 — sotto questo è spazzatura */

static const char *TAG = "TIME_MGR";
static bool s_synced = false;

// Mesi in inglese come li restituisce il server
static const char *MONTHS[] = {
    "January", "February", "March",    "April",   "May",      "June",
    "July",    "August",   "September","October",  "November", "December"
};

// Estrae l'anno dalla macro __DATE__ (es. "Mar 11 2026" → 2026)
static int compile_year(void)
{
    return atoi(__DATE__ + 7);
}

static int parse_month(const char *name)
{
    for (int i = 0; i < 12; i++) {
        if (strncasecmp(name, MONTHS[i], strlen(MONTHS[i])) == 0)
            return i + 1;
    }
    return 1;
}

// Parsa "16 February 13:56" → struct tm
static bool parse_server_time(const char *s, struct tm *out)
{
    if (!s || !out) return false;

    char buf[64];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Rimuovi newline/spazi finali
    int len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r' || buf[len-1] == ' '))
        buf[--len] = '\0';

    // Formato atteso: "DD Month HH:MM"
    char tok_day[4]   = {0};
    char tok_month[16] = {0};
    char tok_time[8]  = {0};

    if (sscanf(buf, "%3s %15s %7s", tok_day, tok_month, tok_time) != 3)
        return false;

    int day  = atoi(tok_day);
    int mon  = parse_month(tok_month);
    int hour = 0, min = 0;
    sscanf(tok_time, "%d:%d", &hour, &min);

    if (day < 1 || day > 31 || mon < 1 || mon > 12) return false;
    if (hour < 0 || hour > 23 || min < 0 || min > 59) return false;

    memset(out, 0, sizeof(struct tm));
    out->tm_mday = day;
    out->tm_mon  = mon - 1;
    out->tm_year = compile_year() - 1900;
    out->tm_hour = hour;
    out->tm_min  = min;
    out->tm_sec  = 0;

    return true;
}

bool time_manager_sync(void)
{
    const char *path = "/iot/orario.php";
    char url[256];
    snprintf(url, sizeof(url), "%s%s", SERVER_BASE, path);

    int response_code = 0;
    char *body = NULL;
    esp_err_t err = http_get_request(url, &response_code, &body);

    if (err != ESP_OK || response_code != 200 || !body) {
        ESP_LOGE(TAG, "Sync fallito (code:%d)", response_code);
        if (body) free(body);
        return false;
    }

    struct tm t;
    if (!parse_server_time(body, &t)) {
        ESP_LOGE(TAG, "Parsing fallito: '%s'", body);
        free(body);
        return false;
    }
    free(body);

    time_t epoch = mktime(&t);
    if (epoch == (time_t)-1) {
        ESP_LOGE(TAG, "mktime fallito");
        return false;
    }

    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);

    s_synced = true;
    ESP_LOGI(TAG, "RTC sincronizzato: %04d-%02d-%02d %02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
    return true;
}

bool time_manager_is_synced(void)
{
    return s_synced;
}

void time_manager_restore_from_sd(void)
{
    /* Se l'RTC è già valido (risveglio da deep sleep), non toccare nulla */
    if (time(NULL) >= TIME_MIN_VALID) {
        ESP_LOGI(TAG, "RTC già valido dopo wakeup, restore SD saltato");
        return;
    }

    FILE *f = fopen(TIME_SAVE_PATH, "rb");
    if (!f) {
        ESP_LOGW(TAG, "Nessun file orario su SD, parto da epoch");
        return;
    }
    time_t saved = 0;
    size_t n = fread(&saved, sizeof(time_t), 1, f);
    fclose(f);

    if (n != 1 || saved < TIME_MIN_VALID) {
        ESP_LOGW(TAG, "File orario SD non valido (%lld), ignorato", (long long)saved);
        return;
    }

    time_t raw = saved;
    struct tm *t_raw = localtime(&raw);
    ESP_LOGI(TAG, "Ultima ora salvata su SD:  %04d-%02d-%02d %02d:%02d:%02d",
             t_raw->tm_year + 1900, t_raw->tm_mon + 1, t_raw->tm_mday,
             t_raw->tm_hour, t_raw->tm_min, t_raw->tm_sec);

    int64_t boot_s = (int64_t)(esp_timer_get_time() / 1000000);
    saved += TIME_RESTORE_OFFSET_S + boot_s;

    struct timeval tv = { .tv_sec = saved, .tv_usec = 0 };
    settimeofday(&tv, NULL);

    struct tm *t = localtime(&saved);
    ESP_LOGI(TAG, "Ora impostata (+ %ds offset + %llds boot): %04d-%02d-%02d %02d:%02d:%02d",
             TIME_RESTORE_OFFSET_S, (long long)boot_s,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

static void time_save_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(TIME_SAVE_INTERVAL_MS));

        if (wifi_is_connected()) continue;

        time_t now = time(NULL);
        if (now < TIME_MIN_VALID) {
            ESP_LOGW(TAG, "Orario non valido, salvataggio SD saltato");
            continue;
        }

        FILE *f = fopen(TIME_SAVE_PATH, "wb");
        if (!f) { ESP_LOGE(TAG, "Impossibile salvare orario su SD"); continue; }
        fwrite(&now, sizeof(time_t), 1, f);
        fclose(f);

        struct tm *t = localtime(&now);
        ESP_LOGI(TAG, "Orario salvato su SD: %04d-%02d-%02d %02d:%02d:%02d",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec);
    }
}

void time_manager_start_periodic_save(void)
{
    xTaskCreate(time_save_task, "TimeSave", 4096, NULL, 1, NULL);
}

void time_manager_get_ts(char *buf, size_t len)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t)
        strftime(buf, len, "%Y-%m-%d %H:%M:%S", t);
    else
        snprintf(buf, len, "0000-00-00 00:00:00");
}
