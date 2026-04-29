#include "gpio_manager.h"
#include "banchetto_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "GPIO_MANAGER";
static TaskHandle_t gpio_task_handle = NULL;

static void gpio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Task GPIO avviato, in attesa...");
    
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        ESP_LOGI(TAG, "⚡ Interrupt ricevuto su GPIO %d!", GPIO_INPUT_PIN);
        vTaskDelay(pdMS_TO_TICKS(50));  // Debounce
        
        if (gpio_get_level(GPIO_INPUT_PIN) == 0) {
            ESP_LOGI(TAG, "✓ GPIO %d confermato a MASSA", GPIO_INPUT_PIN);
            
            // Chiama manager per versare 1 pezzo
            ESP_LOGI(TAG, "📦 Richiesta versamento 1 pezzo...");
            
            if (banchetto_manager_versa(1)) {
                ESP_LOGI(TAG, "✅ Versamento completato con successo!");
            } else {
                ESP_LOGW(TAG, "⚠️  Versamento fallito (vedi log manager)");
            }
            
            // Aspetta rilascio pin
            ESP_LOGI(TAG, "⏳ Attesa rilascio pin...");
            int timeout = 0;
            while (gpio_get_level(GPIO_INPUT_PIN) == 0 && timeout < 500) {
                vTaskDelay(pdMS_TO_TICKS(10));
                timeout++;
            }
            
            if (timeout >= 500) {
                ESP_LOGW(TAG, "⚠️ TIMEOUT - pin ancora a massa");
            }

            ESP_LOGI(TAG, "✓ GPIO %d rilasciato", GPIO_INPUT_PIN);

            // Debounce post-rilascio: scarta notifiche accumulate durante il processing
            vTaskDelay(pdMS_TO_TICKS(150));
            xTaskNotifyStateClear(NULL);
        } else {
            ESP_LOGW(TAG, "⚠ Falso interrupt");
        }
    }
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(gpio_task_handle, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void gpio_init(void)
{
    ESP_LOGI(TAG, "Inizializzazione GPIO %d...", GPIO_INPUT_PIN);
    
    // Crea task
    BaseType_t result = xTaskCreate(
        gpio_task,
        "gpio_task",
        8192,
        NULL,
        10,
        &gpio_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "✗ Errore creazione task GPIO");
        return;
    }
    
    ESP_LOGI(TAG, "✓ Task GPIO creato");
    
    // Configura GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_INPUT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ Errore configurazione GPIO: %s", esp_err_to_name(err));
        return;
    }
    
    // Installa ISR service (può essere già installato da USB)
    err = gpio_install_isr_service(0);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "ℹ️  ISR service già installato");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ Errore ISR service: %s", esp_err_to_name(err));
        return;
    } else {
        ESP_LOGI(TAG, "✓ ISR service installato");
    }
    
    // Aggiungi handler
    err = gpio_isr_handler_add(GPIO_INPUT_PIN, gpio_isr_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ Errore handler: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "✓ GPIO %d configurato come input con pull-up", GPIO_INPUT_PIN);
    ESP_LOGI(TAG, "✓ Interrupt handler aggiunto");
    ESP_LOGI(TAG, "📌 Collega GPIO %d a GND per versare 1 pezzo", GPIO_INPUT_PIN);
}