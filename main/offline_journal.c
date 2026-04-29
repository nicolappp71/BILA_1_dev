#include "offline_journal.h"
#include "http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "mode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define JOURNAL_PATH "/sdcard/offline_journal.jsonl"
#define JOURNAL_TMP_PATH "/sdcard/offline_journal.tmp"
static const char *TAG = "JOURNAL";

// Helper per leggere una riga senza saturare lo stack
static char *alloc_line_buffer()
{
    return (char *)malloc(1024);
}

// ─────────────────────────────────────────────────────────────────────────────

void offline_journal_append(const char *json_line)
{
    FILE *f = fopen(JOURNAL_PATH, "a");
    if (!f)
    {
        ESP_LOGE(TAG, "Impossibile aprire journal");
        return;
    }
    fprintf(f, "%s\n", json_line);
    fclose(f);
    ESP_LOGI(TAG, "Registrato: %.120s", json_line);
}

// ─────────────────────────────────────────────────────────────────────────────

int offline_journal_count(void)
{
    FILE *f = fopen(JOURNAL_PATH, "r");
    if (!f)
        return 0;
    int count = 0;
    char small_buf[32]; // Buffer piccolo solo per contare
    while (fgets(small_buf, sizeof(small_buf), f))
    {
        if (strchr(small_buf, '\n'))
            count++;
    }
    fclose(f);
    return count;
}

// ─────────────────────────────────────────────────────────────────────────────

void offline_journal_print_all(void)
{
    int total = offline_journal_count();
    if (total == 0)
        return;

    FILE *f = fopen(JOURNAL_PATH, "r");
    if (!f)
        return;

    char *line = alloc_line_buffer();
    if (!line)
    {
        fclose(f);
        return;
    }

    ESP_LOGI(TAG, "══════════ JOURNAL OFFLINE: %d ══════════", total);

    while (fgets(line, 1024, f))
    {
        cJSON *j = cJSON_Parse(line);
        if (!j)
            continue;
        // ... (resto della stampa semplificata per brevità nel log) ...
        const char *op = cJSON_GetStringValue(cJSON_GetObjectItem(j, "op")) ?: "?";
        ESP_LOGI(TAG, "  Op: %s", op);
        cJSON_Delete(j);
    }

    free(line);
    fclose(f);
}

// ─────────────────────────────────────────────────────────────────────────────

int offline_journal_replay(void)
{
    int total = offline_journal_count();
    if (total == 0)
        return 0;

    FILE *src = fopen(JOURNAL_PATH, "r");
    if (!src)
        return 0;
    FILE *tmp = fopen(JOURNAL_TMP_PATH, "w");
    if (!tmp)
    {
        fclose(src);
        return 0;
    }

    // Alloco i buffer pesanti nello HEAP, non nello STACK
    char *line = alloc_line_buffer();
    char *url_fixed = alloc_line_buffer();

    int sent = 0;
    int current = 0;
    bool failed = false;

    ESP_LOGI(TAG, "Replay journal: %d operazioni da inviare", total);

    if (!line || !url_fixed)
    {
        ESP_LOGE(TAG, "Memory allocation failed for replay");
        if (line)
            free(line);
        if (url_fixed)
            free(url_fixed);
        fclose(src);
        fclose(tmp);
        return 0;
    }

    while (fgets(line, 1024, src))
    {
        if (failed)
        {
            fprintf(tmp, "%s", line);
            continue;
        }

        current++;
        cJSON *j = cJSON_Parse(line);
        if (!j)
        {
            ESP_LOGW(TAG, "[%d/%d] Riga non valida, mantenuta", current, total);
            fprintf(tmp, "%s", line);
            continue;
        }

        const char *op_preview = cJSON_GetStringValue(cJSON_GetObjectItem(j, "op")) ?: "?";
        const char *ts_preview = cJSON_GetStringValue(cJSON_GetObjectItem(j, "ts")) ?: "?";
        ESP_LOGI(TAG, "[%d/%d] Invio: op=%s ts=%s", current, total, op_preview, ts_preview);

        const char *url_raw = cJSON_GetStringValue(cJSON_GetObjectItem(j, "url"));
        const char *op = cJSON_GetStringValue(cJSON_GetObjectItem(j, "op"));

        if (url_raw && op)
        {
#ifdef CASA
            const char *wrong_host = "intranet.cifarelli.loc";
            const char *right_host = "192.168.1.58:10000";
            char *pos = strstr(url_raw, wrong_host);
            if (pos)
            {
                int prefix_len = pos - url_raw;
                snprintf(url_fixed, 1024, "%.*s%s%s", prefix_len, url_raw, right_host, pos + strlen(wrong_host));
            }
            else
            {
                snprintf(url_fixed, 1024, "%s", url_raw);
            }
#else
            // Se il journal è stato scritto in modalità CASA, correggi l'host
            const char *casa_host = "192.168.1.58:10000";
            const char *prod_host = "intranet.cifarelli.loc";
            char *pos = strstr(url_raw, casa_host);
            if (pos)
            {
                int prefix_len = pos - url_raw;
                snprintf(url_fixed, 1024, "%.*s%s%s", prefix_len, url_raw, prod_host, pos + strlen(casa_host));
            }
            else
            {
                snprintf(url_fixed, 1024, "%s", url_raw);
            }
#endif
            int response_code = 0;
            char *body = NULL;
            if (http_get_request(url_fixed, &response_code, &body) == ESP_OK && response_code == 200)
            {
                sent++;
                ESP_LOGI(TAG, "[%d/%d] ✓ Inviato OK", current, total);
            }
            else
            {
                ESP_LOGW(TAG, "[%d/%d] ✗ Fallito (http=%d), rimane in journal", current, total, response_code);
                fprintf(tmp, "%s", line);
                failed = true;
            }
            if (body)
                free(body);
        }
        cJSON_Delete(j);
    }

    free(line);
    free(url_fixed);
    fclose(src);
    fclose(tmp);
    remove(JOURNAL_PATH);
    rename(JOURNAL_TMP_PATH, JOURNAL_PATH);
    return sent;
}