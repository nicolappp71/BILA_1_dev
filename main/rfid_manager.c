#include "rfid_manager.h"
#include "banchetto_manager.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG_RFID = "RFID";

#define RFID_UART_NUM       UART_NUM_1
#define RFID_TX_PIN         GPIO_NUM_30
#define RFID_RX_PIN         GPIO_NUM_28
#define RFID_BAUD_RATE      9600
#define BUF_SIZE            256

#define RFID_DEBOUNCE_MS    2000
#define RFID_TAG_LEN        12

static void rfid_reader_task(void *arg)
{
    uint8_t data[BUF_SIZE];
    char last_tag[BUF_SIZE] = {0};
    TickType_t last_read_tick = 0;
    
    ESP_LOGI(TAG_RFID, "Task RFID avviato (Lunghezza UID: %d, Debounce: %dms)", 
             RFID_TAG_LEN, RFID_DEBOUNCE_MS);
    
    while (1)
    {
        TickType_t loop_start = xTaskGetTickCount();
        
        int len = uart_read_bytes(RFID_UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(20));
        
        if (len > 0)
        {
            char clean_data[BUF_SIZE];
            int clean_idx = 0;
            
            for (int i = 0; i < len; i++)
            {
                if (data[i] == 0x02 || data[i] == 0x03) {
                    continue;
                }
                
                if ((data[i] >= '0' && data[i] <= '9') || 
                    (data[i] >= 'A' && data[i] <= 'F') ||
                    (data[i] >= 'a' && data[i] <= 'f'))
                {
                    if (data[i] >= 'a' && data[i] <= 'f') {
                        clean_data[clean_idx++] = data[i] - 32;
                    } else {
                        clean_data[clean_idx++] = data[i];
                    }
                    
                    if (clean_idx >= RFID_TAG_LEN) {
                        break;
                    }
                }
            }
            clean_data[clean_idx] = '\0';
            
            if (clean_idx == RFID_TAG_LEN)
            {
                TickType_t now = xTaskGetTickCount();

                if (strcmp(clean_data, last_tag) == 0) 
                {
                    if ((now - last_read_tick) < pdMS_TO_TICKS(RFID_DEBOUNCE_MS)) 
                    {
                        continue;
                    }
                }

                strcpy(last_tag, clean_data);
                last_read_tick = now;
                
                TickType_t total_time = xTaskGetTickCount() - loop_start;

                ESP_LOGI(TAG_RFID, "═══════════════════════════════════");
                ESP_LOGI(TAG_RFID, "✅ UID RFID: %s", clean_data);
                ESP_LOGI(TAG_RFID, "⏱️  Tempo: %lu ms", pdTICKS_TO_MS(total_time));
                ESP_LOGI(TAG_RFID, "═══════════════════════════════════");
                
                banchetto_manager_login_badge(clean_data);
                
                uart_flush_input(RFID_UART_NUM);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void init_rfid_uart(void)
{
    ESP_LOGI(TAG_RFID, "Inizializzazione RFID UART...");
    
    uart_config_t uart_config = {
        .baud_rate = RFID_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(RFID_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(RFID_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(RFID_UART_NUM, RFID_TX_PIN, RFID_RX_PIN, 
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    xTaskCreate(rfid_reader_task, "rfid_reader", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG_RFID, "✓ RFID Pronto (Len: %d, Debounce: %dms)", RFID_TAG_LEN, RFID_DEBOUNCE_MS);
}