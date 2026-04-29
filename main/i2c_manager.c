#include "i2c_manager.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "I2C_MGR";

#define I2C_NUM         I2C_NUM_0
#define I2C_SDA         GPIO_NUM_7
#define I2C_SCL         GPIO_NUM_8
#define I2C_FREQ_HZ     400000

static i2c_master_bus_handle_t i2c_handle = NULL;
static bool initialized = false;

esp_err_t i2c_manager_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "I2C già inizializzato");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Inizializzazione I2C bus condiviso...");
    
    i2c_master_bus_config_t i2c_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .i2c_port = I2C_NUM,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(&i2c_conf, &i2c_handle), 
        TAG, "I2C bus init failed"
    );
    
    initialized = true;
    ESP_LOGI(TAG, "I2C OK (SDA=%d, SCL=%d, %dkHz)", I2C_SDA, I2C_SCL, I2C_FREQ_HZ/1000);
    
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_manager_get_handle(void)
{
    if (!initialized) {
        ESP_LOGE(TAG, "I2C non inizializzato! Chiamare i2c_manager_init() prima");
        return NULL;
    }
    return i2c_handle;
}