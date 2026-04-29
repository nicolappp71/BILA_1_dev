#include "offline_queue.h"
#include "http_client.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "OFFLINE_Q";

esp_err_t offline_queue_push(const char *url)
{
    if (!url || strlen(url) == 0)
        return ESP_ERR_INVALID_ARG;

    FILE *f = fopen(OFFLINE_QUEUE_PATH, "a");
    if (!f)
    {
        ESP_LOGE(TAG, "Impossibile aprire coda: %s", OFFLINE_QUEUE_PATH);
        return ESP_FAIL;
    }

    fprintf(f, "%s\n", url);
    fclose(f);
    ESP_LOGI(TAG, "Operazione accodata: %s", url);
    return ESP_OK;
}

int offline_queue_count(void)
{
    FILE *f = fopen(OFFLINE_QUEUE_PATH, "r");
    if (!f)
        return 0;

    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        if (strlen(line) > 2)
            count++;
    }
    fclose(f);
    return count;
}

esp_err_t offline_queue_process(void)
{
    FILE *f = fopen(OFFLINE_QUEUE_PATH, "r");
    if (!f)
    {
        ESP_LOGI(TAG, "Nessuna coda pendente");
        return ESP_OK;
    }

    int ok_count = 0, fail_count = 0;
    char line[512];

    while (fgets(line, sizeof(line), f))
    {
        // Rimuovi newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        if (strlen(line) < 5)
            continue;

        int response_code = 0;
        char *response_body = NULL;
        esp_err_t ret = http_get_request(line, &response_code, &response_body);
        if (response_body)
            free(response_body);

        if (ret == ESP_OK && response_code == 200)
        {
            ESP_LOGI(TAG, "Sync OK: %s", line);
            ok_count++;
        }
        else
        {
            ESP_LOGE(TAG, "Sync FAIL (code:%d): %s", response_code, line);
            fail_count++;
        }
    }

    fclose(f);

    if (fail_count == 0)
    {
        remove(OFFLINE_QUEUE_PATH);
        ESP_LOGI(TAG, "Sync completato: %d operazioni inviate, coda svuotata", ok_count);
    }
    else
    {
        ESP_LOGW(TAG, "Sync parziale: %d OK, %d falliti — coda mantenuta", ok_count, fail_count);
    }

    return (fail_count == 0) ? ESP_OK : ESP_FAIL;
}
