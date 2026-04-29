


// #include <stdbool.h>
// #include "esp_err.h"
// #include "scanner_manager.h"
// #include "banchetto_manager.h"  // <--- AGGIUNTO
// #include "esp_log.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/event_groups.h"

// static char barcode_buffer[128];
// static int barcode_index = 0;
// static const char *TAG_USB = "SCANNER";

// void hid_host_interface_callback(hid_host_device_handle_t device_handle,
//                                  const hid_host_interface_event_t event,
//                                  void *arg)
// {
//     if (event == HID_HOST_INTERFACE_EVENT_INPUT_REPORT)
//     {
//         uint8_t data[64];
//         size_t data_length = 0;

//         esp_err_t ret = hid_host_device_get_raw_input_report_data(device_handle,
//                                                                   data,
//                                                                   sizeof(data),
//                                                                   &data_length);

//         if (ret == ESP_OK && data_length >= 3)
//         {
//             uint8_t keycode = data[2];

//             if (keycode > 0)
//             {
//                 char c = hid_code_to_ascii(keycode);

//                 if (c == '\n')
//                 {
//                     // Fine del codice - stampa tutto
//                     barcode_buffer[barcode_index] = '\0';
//                     ESP_LOGI(TAG_USB, "═══════════════════════════════════");
//                     ESP_LOGI(TAG_USB, "BARCODE: %s", barcode_buffer);
//                     ESP_LOGI(TAG_USB, "Lunghezza: %d caratteri", barcode_index);
//                     ESP_LOGI(TAG_USB, "═══════════════════════════════════");
                    
//                     // *** CHIAMATA AL SERVER ***
//                     banchetto_manager_set_barcode(barcode_buffer);
                    
//                     barcode_index = 0; // Reset per il prossimo codice
//                 }
//                 else if (c != 0 && barcode_index < 127)
//                 {
//                     // Accumula il carattere
//                     barcode_buffer[barcode_index++] = c;
//                 }
//             }
//         }
//     }
// }

// char hid_code_to_ascii(uint8_t code)
// {
//     // Numeri: 1-9, 0
//     if (code >= 0x1E && code <= 0x26)
//         return '1' + (code - 0x1E); // 1-9
//     if (code == 0x27)
//         return '0'; // 0

//     // Lettere A-Z
//     if (code >= 0x04 && code <= 0x1D)
//         return 'A' + (code - 0x04);

//     // Caratteri speciali
//     if (code == 0x2C)
//         return ' '; // Spazio
//     if (code == 0x28)
//         return '\n'; // Enter
//     if (code == 0x2D)
//         return '-';
//     if (code == 0x2E)
//         return '=';
//     if (code == 0x2F)
//         return '[';
//     if (code == 0x30)
//         return ']';
//     if (code == 0x33)
//         return ';';
//     if (code == 0x34)
//         return '\'';
//     if (code == 0x36)
//         return ',';
//     if (code == 0x37)
//         return '.';
//     if (code == 0x38)
//         return '/';

//     return 0; // Carattere non riconosciuto
// }

// void hid_host_device_callback(hid_host_device_handle_t device_handle,
//                               const hid_host_driver_event_t event,
//                               void *arg)
// {
//     if (event == HID_HOST_DRIVER_EVENT_CONNECTED)
//     {
//         ESP_LOGI(TAG_USB, "SCANNER CONNESSO! (Handle: %p)", device_handle);

//         const hid_host_device_config_t dev_config = {
//             .callback = hid_host_interface_callback,
//             .callback_arg = NULL};

//         hid_host_device_open(device_handle, &dev_config);
//         hid_host_device_start(device_handle);
//         ESP_LOGI(TAG_USB, "Scanner Pronto e in ascolto.");
//     }
//     else
//     {
//         ESP_LOGW(TAG_USB, "Scanner Disconnesso.");
//         hid_host_device_close(device_handle);
//     }
// }

// void usb_lib_task(void *arg)
// {
//     while (1)
//     {
//         uint32_t event_flags;
//         usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

//         if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
//         {
//             ESP_LOGI(TAG_USB, "Nessun client USB registrato (pulizia...)");
//             usb_host_device_free_all();
//         }
//         if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
//         {
//             ESP_LOGI(TAG_USB, "Tutti i dispositivi liberati");
//         }
//     }
// }

// void init_usb_driver(void)
// {
//     ESP_LOGI(TAG_USB, "Avvio USB Completo...");

//     // 1. PHY MANUAL (Accende i 5V)
//     const usb_phy_config_t phy_config = {
//         .controller = USB_PHY_CTRL_OTG,
//         .target = USB_PHY_TARGET_INT,
//         .otg_mode = USB_OTG_MODE_HOST,
//         .otg_speed = USB_PHY_SPEED_FULL,
//     };
//     usb_phy_handle_t phy_handle;
//     usb_new_phy(&phy_config, &phy_handle);

//     // 2. DRIVER BASE
//     const usb_host_config_t host_config = {
//         .skip_phy_setup = false,
//         .intr_flags = ESP_INTR_FLAG_LEVEL1,
//     };

//     if (usb_host_install(&host_config) != ESP_OK)
//     {
//         usb_host_install(NULL);
//     }

//     // 3. AVVIO MANOVELLA (Task Background)
//     xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096, NULL, 2, NULL, 0);

//     // 5. DRIVER SCANNER
//     const hid_host_driver_config_t hid_config = {
//         .create_background_task = true,
//         .task_priority = 5,
//         .stack_size = 4096,
//         .core_id = 0,
//         .callback = hid_host_device_callback,
//         .callback_arg = NULL};

//     ESP_ERROR_CHECK(hid_host_install(&hid_config));
//     ESP_LOGI(TAG_USB, "Driver Attivo.");
// }


#include <stdbool.h>
#include "esp_err.h"
#include "scanner_manager.h"
#include "banchetto_manager.h"
#include "collaudo_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static char barcode_buffer[128];
static int barcode_index = 0;
static const char *TAG_USB = "SCANNER";

void hid_host_interface_callback(hid_host_device_handle_t device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    if (event == HID_HOST_INTERFACE_EVENT_INPUT_REPORT)
    {
        uint8_t data[64];
        size_t data_length = 0;

        esp_err_t ret = hid_host_device_get_raw_input_report_data(device_handle,
                                                                  data,
                                                                  sizeof(data),
                                                                  &data_length);

        if (ret == ESP_OK && data_length >= 3)
        {
            uint8_t keycode = data[2];

            if (keycode > 0)
            {
                char c = hid_code_to_ascii(keycode);

                if (c == '\n')
                {
                    barcode_buffer[barcode_index] = '\0';
                    ESP_LOGI(TAG_USB, "═══════════════════════════════════");
                    ESP_LOGI(TAG_USB, "BARCODE: %s", barcode_buffer);
                    ESP_LOGI(TAG_USB, "Lunghezza: %d caratteri", barcode_index);
                    ESP_LOGI(TAG_USB, "═══════════════════════════════════");

                    collaudo_state_t coll_st = collaudo_manager_get_state();
                    if (coll_st == COLLAUDO_STATE_SCAN_MOTORE ||
                        coll_st == COLLAUDO_STATE_IN_CORSO)
                    {
                        collaudo_manager_scan_barcode(barcode_buffer);
                    }
                    else
                    {
                        switch (banchetto_manager_get_state())
                        {
                            case BANCHETTO_STATE_ASSEGNA_BANCHETTO:
                                banchetto_manager_assegna_banchetto(barcode_buffer);
                                break;
                            default:
                                banchetto_manager_set_barcode(barcode_buffer);
                                break;
                        }
                    }

                    barcode_index = 0;
                }
                else if (c != 0 && barcode_index < 127)
                {
                    barcode_buffer[barcode_index++] = c;
                }
            }
        }
    }
}

char hid_code_to_ascii(uint8_t code)
{
    if (code >= 0x1E && code <= 0x26)
        return '1' + (code - 0x1E);
    if (code == 0x27)
        return '0';
    if (code >= 0x04 && code <= 0x1D)
        return 'A' + (code - 0x04);
    if (code == 0x2C) return ' ';
    if (code == 0x28) return '\n';
    if (code == 0x2D) return '-';
    if (code == 0x2E) return '=';
    if (code == 0x2F) return '[';
    if (code == 0x30) return ']';
    if (code == 0x33) return ';';
    if (code == 0x34) return '\'';
    if (code == 0x36) return ',';
    if (code == 0x37) return '.';
    if (code == 0x38) return '/';
    return 0;
}

void hid_host_device_callback(hid_host_device_handle_t device_handle,
                              const hid_host_driver_event_t event,
                              void *arg)
{
    if (event == HID_HOST_DRIVER_EVENT_CONNECTED)
    {
        ESP_LOGI(TAG_USB, "SCANNER CONNESSO! (Handle: %p)", device_handle);

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback,
            .callback_arg = NULL};

        hid_host_device_open(device_handle, &dev_config);
        hid_host_device_start(device_handle);
        ESP_LOGI(TAG_USB, "Scanner Pronto e in ascolto.");
    }
    else
    {
        ESP_LOGW(TAG_USB, "Scanner Disconnesso.");
        hid_host_device_close(device_handle);
    }
}

void usb_lib_task(void *arg)
{
    while (1)
    {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            ESP_LOGI(TAG_USB, "Nessun client USB registrato (pulizia...)");
            usb_host_device_free_all();
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
            ESP_LOGI(TAG_USB, "Tutti i dispositivi liberati");
        }
    }
}

void init_usb_driver(void)
{
    ESP_LOGI(TAG_USB, "Avvio USB Completo...");

    const usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_HOST,
        .otg_speed = USB_PHY_SPEED_FULL,
    };
    usb_phy_handle_t phy_handle;
    usb_new_phy(&phy_config, &phy_handle);

    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    if (usb_host_install(&host_config) != ESP_OK)
        usb_host_install(NULL);

    xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096, NULL, 2, NULL, 0);

    const hid_host_driver_config_t hid_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL};

    ESP_ERROR_CHECK(hid_host_install(&hid_config));
    ESP_LOGI(TAG_USB, "Driver Attivo.");
}