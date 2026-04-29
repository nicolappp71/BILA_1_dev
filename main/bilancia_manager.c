#include "bilancia_manager.h"
#include "mode.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include <string.h>
#include <stdlib.h>

// abilita collaudo_sim_set_consumo / collaudo_sim_set_benzina / collaudo_sim_get_benzina
#define COLLAUDI_BANCHETTO_ID "222"
#include "collaudo_manager.h"

static const char *TAG = "BILANCIA";

// ── UART ───────────────────────────────────────────────────
#define BILANCIA_UART_NUM   UART_NUM_2
#define BILANCIA_TX_GPIO    UART_PIN_NO_CHANGE  // TBD — assegnare prima del collaudo HW
#define BILANCIA_RX_GPIO    UART_PIN_NO_CHANGE  // TBD — assegnare prima del collaudo HW
#define BILANCIA_BAUD       9600
#define BILANCIA_BUF_SIZE   256

// ── Stato interno ──────────────────────────────────────────
static SemaphoreHandle_t s_uart_mutex  = NULL;
static TaskHandle_t      s_poll_handle = NULL;
static volatile bool     s_poll_active = false;

// ── Protocollo: invia [STX SP cmd ETX] ────────────────────
static void send_cmd(const char *cmd)
{
    char frame[32];
    int len = snprintf(frame, sizeof(frame), "\x02 %s\x03", cmd);
    uart_write_bytes(BILANCIA_UART_NUM, frame, len);
}

// ── Legge risposta fino a ETX, stripping STX/ETX ──────────
// Ritorna numero di caratteri nel buffer (senza terminatore)
static int read_response(char *buf, int maxlen, int timeout_ms)
{
    int pos = 0;
    int elapsed = 0;
    const int STEP_MS = 5;

    while (pos < maxlen - 1 && elapsed < timeout_ms) {
        uint8_t c;
        int r = uart_read_bytes(BILANCIA_UART_NUM, &c, 1, pdMS_TO_TICKS(STEP_MS));
        elapsed += STEP_MS;
        if (r <= 0) continue;
        if (c == 0x02) continue; // STX — salta
        if (c == 0x03) break;    // ETX — fine trama
        buf[pos++] = (char)c;
    }
    buf[pos] = '\0';
    return pos;
}

// ── Trim sinistro in-place (come Arduino .trim()) ─────────
static void trim_inplace(char *s)
{
    int len = strlen(s);
    int start = 0;
    while (start < len && (s[start] == ' ' || s[start] == '\r' || s[start] == '\n'))
        start++;
    if (start > 0)
        memmove(s, s + start, len - start + 1);
    len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\r' || s[len - 1] == '\n'))
        s[--len] = '\0';
}

// ── Task polling consumo (AIMW ogni 1s) ───────────────────
// Risposta: "AIMW 1 XXXX ..." → valore g a offset [7..10]
static void poll_task(void *arg)
{
    char resp[64];

    while (s_poll_active) {
        if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            send_cmd("AIMW");
            int n = read_response(resp, sizeof(resp), 300);
            xSemaphoreGive(s_uart_mutex);

            if (n >= 11) {
                trim_inplace(resp);
                if (strncmp(resp, "AIMW", 4) == 0 && (int)strlen(resp) >= 11) {
                    char val_str[6] = {0};
                    memcpy(val_str, resp + 7, 4);
                    float val = strtof(val_str, NULL);
                    if (val >= 0.0f) {
                        ESP_LOGD(TAG, "Consumo: %.1f g", val);
                        collaudo_sim_set_consumo(val);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    s_poll_handle = NULL;
    vTaskDelete(NULL);
}

// ── Init ──────────────────────────────────────────────────
void bilancia_manager_init(void)
{
    s_uart_mutex = xSemaphoreCreateMutex();

    uart_config_t cfg = {
        .baud_rate           = BILANCIA_BAUD,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk          = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(BILANCIA_UART_NUM, BILANCIA_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(BILANCIA_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(BILANCIA_UART_NUM,
                                 BILANCIA_TX_GPIO, BILANCIA_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Attiva remote mode
    vTaskDelay(pdMS_TO_TICKS(100));
    send_cmd("SREM");
    ESP_LOGI(TAG, "Init OK (TX=%d RX=%d %d baud) — SREM inviato",
             BILANCIA_TX_GPIO, BILANCIA_RX_GPIO, BILANCIA_BAUD);
}

// ── Start / Stop polling ──────────────────────────────────
void bilancia_manager_start_poll(void)
{
    if (s_poll_handle != NULL) return;
    s_poll_active = true;
    xTaskCreate(poll_task, "bilancia_poll", 4096, NULL, 4, &s_poll_handle);
    ESP_LOGI(TAG, "Polling consumo avviato");
}

void bilancia_manager_stop_poll(void)
{
    s_poll_active = false;
    ESP_LOGI(TAG, "Polling consumo fermato");
}

// ── Check livello async (one-shot task) ───────────────────
static void check_level_task(void *arg)
{
    bilancia_manager_check_level();
    vTaskDelete(NULL);
}

void bilancia_manager_check_level_async(void)
{
    xTaskCreate(check_level_task, "bilancia_chk", 4096, NULL, 4, NULL);
}

// ── Check livello a fine collaudo ─────────────────────────
// TEST: legge il valore corrente del simulatore web.
// HW:   invia SMES0+SINT+AWRT, parsa peso (g).
// In entrambi i casi: clamp, aggiorna benzina, SAVA se sotto soglia.
void bilancia_manager_check_level(void)
{
    float grams;

#ifdef TEST
    grams = collaudo_sim_get_benzina();
    ESP_LOGI(TAG, "check_level (SIM): %.1f g", grams);
#else
    char resp[64];

    if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "check_level: timeout mutex");
        return;
    }

    send_cmd("SMES 0");
    vTaskDelay(pdMS_TO_TICKS(200));
    send_cmd("SINT");
    vTaskDelay(pdMS_TO_TICKS(50));
    send_cmd("AWRT");
    int n = read_response(resp, sizeof(resp), 500);

    xSemaphoreGive(s_uart_mutex);

    if (n < 7) {
        ESP_LOGW(TAG, "check_level: risposta troppo corta (%d)", n);
        return;
    }

    trim_inplace(resp);

    if (strncmp(resp, "AWRT", 4) != 0) {
        ESP_LOGW(TAG, "check_level: risposta inattesa: %s", resp);
        return;
    }

    char val_str[8] = {0};
    memcpy(val_str, resp + 7, 6);
    grams = strtof(val_str, NULL);
#endif

    if (grams < BILANCIA_DISPLAY_MIN_G) grams = BILANCIA_DISPLAY_MIN_G;
    if (grams > BILANCIA_MAX_G)         grams = BILANCIA_MAX_G;

    ESP_LOGI(TAG, "Livello bilancia: %.1f g", grams);
    collaudo_sim_set_benzina(grams);

    if (grams < BILANCIA_REFILL_G) {
#ifdef TEST
        ESP_LOGW(TAG, "Livello basso (%.1f g) — TEST: SAVA non inviato", grams);
#else
        ESP_LOGW(TAG, "Livello basso (%.1f g < %.1f g) — invio SAVA", grams, BILANCIA_REFILL_G);
        if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            send_cmd("SAVA");
            xSemaphoreGive(s_uart_mutex);
        }
#endif
    }
}
