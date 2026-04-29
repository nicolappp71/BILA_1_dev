// #include "wifi_manager.h"
// #include <string.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/event_groups.h"
// #include "esp_system.h"
// #include "esp_wifi.h"
// #include "esp_event.h"
// #include "esp_log.h"
// #include "esp_mac.h"
// #include "lwip/err.h"
// #include "lwip/sys.h"
// #include "credential.h"
// #include "offline_queue.h"
// // --- CONFIGURAZIONE ---

// static const char *TAG = "WIFI_MANAGER";
// EventGroupHandle_t s_wifi_event_group = NULL;
// static int s_retry_num = 0;
// static bool s_is_locked = false;

// static uint8_t current_bssid[6] = {0};

// // Funzione "Radar": Scansiona TUTTO e trova il migliore in assoluto.
// // Chiamare solo con BLE sospeso (ble_manager_pause_scan).
// static bool find_best_network_global(uint8_t *bssid_out, uint8_t *channel_out, int *rssi_out)
// {
//     wifi_scan_config_t scan_config = {
//         .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = false, .scan_type = WIFI_SCAN_TYPE_ACTIVE, .scan_time.active.min = 100, .scan_time.active.max = 200};

//     esp_err_t ret;
//     int retries = 0;

//     do
//     {
//         ret = esp_wifi_scan_start(&scan_config, true);
//         if (ret == ESP_ERR_WIFI_STATE)
//         {
//             vTaskDelay(pdMS_TO_TICKS(200));
//             retries++;
//         }
//     } while (ret != ESP_OK && retries < 5);

//     if (ret != ESP_OK)
//         return false;

//     uint16_t ap_count = 0;
//     esp_wifi_scan_get_ap_num(&ap_count);
//     if (ap_count == 0)
//         return false;

//     wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
//     if (!ap_list)
//         return false;

//     esp_wifi_scan_get_ap_records(&ap_count, ap_list);

//     bool found = false;
//     int best_rssi = -128;

//     ESP_LOGD(TAG, "🔍 Scansione completa: Trovati %d AP.", ap_count);

//     for (int i = 0; i < ap_count; i++)
//     {
//         if (strcmp((char *)ap_list[i].ssid, CONFIG_WIFI_SS) == 0)
//         {
//             if (ap_list[i].rssi < 0 && ap_list[i].rssi > -95)
//             {
//                 if (ap_list[i].rssi > best_rssi)
//                 {
//                     best_rssi = ap_list[i].rssi;
//                     *channel_out = ap_list[i].primary;
//                     memcpy(bssid_out, ap_list[i].bssid, 6);
//                     *rssi_out = best_rssi;
//                     found = true;
//                 }
//             }
//         }
//     }
//     free(ap_list);
//     return found;
// }

// static void event_handler(void *arg, esp_event_base_t event_base,
//                           int32_t event_id, void *event_data)
// {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
//     {
//         esp_wifi_set_max_tx_power(8);
//         ESP_LOGI(TAG, "WiFi avviato.");
//     }
//     else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
//     {
//         s_retry_num++;

//         if (s_is_locked && s_retry_num > 3)
//         {
//             ESP_LOGE(TAG, "🚨 L'antenna bloccata non risponde più! Sblocco.");

//             s_is_locked = false;
//             s_retry_num = 0;

//             wifi_config_t conf;
//             esp_wifi_get_config(WIFI_IF_STA, &conf);
//             conf.sta.bssid_set = false;
//             memset(conf.sta.bssid, 0, 6);

//             // FIX: no stop/start (crash ESP-Hosted) — aggiorna config e riconnetti
//             // FIX: esp_wifi_connect() era mancante nella versione originale
//             esp_wifi_set_config(WIFI_IF_STA, &conf);
//             esp_wifi_connect();
//             return;
//         }

//         ESP_LOGI(TAG, "Disconnesso. Riprovo... (%d/20)", s_retry_num);
//         vTaskDelay(pdMS_TO_TICKS(2000));
//         esp_wifi_connect();
//     }
//     else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
//     {
//         ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
//         wifi_ap_record_t ap_info;
//         esp_wifi_sta_get_ap_info(&ap_info);

//         ESP_LOGI(TAG, "✓ CONNESSO!");
//         ESP_LOGI(TAG, "   Canale: %d | RSSI: %d dBm", ap_info.primary, ap_info.rssi);
//         ESP_LOGI(TAG, "   IP: " IPSTR, IP2STR(&event->ip_info.ip));

//         s_retry_num = 0;
//         xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

//         if (offline_queue_count() > 0)
//         {
//             ESP_LOGI(TAG, "WiFi riconnesso — avvio sync coda offline...");
//             offline_queue_process();
//         }
//     }
// }
// // static void wifi_background_scan_task(void *pvParameters)
// // {
// //     while (1)
// //     {
// //         vTaskDelay(pdMS_TO_TICKS(60000)); // Attende 60 secondi

// //         wifi_ap_record_t current_ap;
// //         if (esp_wifi_sta_get_ap_info(&current_ap) != ESP_OK)
// //         {
// //             continue; // Se non è connesso, salta il controllo
// //         }

// //         wifi_scan_config_t scan_config = {
// //             .ssid = (uint8_t *)CONFIG_WIFI_SS,
// //             .bssid = NULL,
// //             .channel = 0,
// //             .show_hidden = false,
// //             .scan_type = WIFI_SCAN_TYPE_ACTIVE};

// //         if (esp_wifi_scan_start(&scan_config, true) == ESP_OK)
// //         {
// //             uint16_t ap_count = 0;
// //             esp_wifi_scan_get_ap_num(&ap_count);

// //             if (ap_count > 0)
// //             {
// //                 wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
// //                 if (ap_list)
// //                 {
// //                     esp_wifi_scan_get_ap_records(&ap_count, ap_list);

// //                     int best_rssi = current_ap.rssi;
// //                     bool better_found = false;
// //                     uint8_t best_bssid[6] = {0};

// //                     for (int i = 0; i < ap_count; i++)
// //                     {
// //                         if (strcmp((char *)ap_list[i].ssid, CONFIG_WIFI_SS) == 0)
// //                         {
// //                             if (ap_list[i].rssi > best_rssi)
// //                             {
// //                                 best_rssi = ap_list[i].rssi;
// //                                 memcpy(best_bssid, ap_list[i].bssid, 6);
// //                                 better_found = true;
// //                             }
// //                         }
// //                     }

// //                     if (better_found)
// //                     {
// //                         ESP_LOGI(TAG, "📡 [DEBUG] Trovato AP migliore! Attuale: %d dBm | Migliore: %d dBm (BSSID: %02x:%02x:%02x:%02x:%02x:%02x)",
// //                                  current_ap.rssi, best_rssi,
// //                                  best_bssid[0], best_bssid[1], best_bssid[2],
// //                                  best_bssid[3], best_bssid[4], best_bssid[5]);
// //                     }
// //                     free(ap_list);
// //                 }
// //             }
// //         }
// //     }
// // }
// static void wifi_background_scan_task(void *pvParameters)
// {
//     while (1) {
//         vTaskDelay(pdMS_TO_TICKS(60000));

//         wifi_ap_record_t current_ap;
//         if (esp_wifi_sta_get_ap_info(&current_ap) != ESP_OK) {
//             continue;
//         }

//         wifi_scan_config_t scan_config = {
//             .ssid = (uint8_t *)CONFIG_WIFI_SS,
//             .bssid = NULL,
//             .channel = 0,
//             .show_hidden = false,
//             .scan_type = WIFI_SCAN_TYPE_ACTIVE
//         };

//         if (esp_wifi_scan_start(&scan_config, true) == ESP_OK) {
//             uint16_t ap_count = 0;
//             esp_wifi_scan_get_ap_num(&ap_count);

//             if (ap_count > 0) {
//                 wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
//                 if (ap_list) {
//                     esp_wifi_scan_get_ap_records(&ap_count, ap_list);
                    
//                     ESP_LOGI(TAG, "🔍 Scansione background: Trovati %d AP per %s", ap_count, CONFIG_WIFI_SS);

//                     int best_rssi = current_ap.rssi;
//                     bool better_found = false;
//                     uint8_t best_bssid[6] = {0};
//                     uint8_t best_channel = 0;

//                     for (int i = 0; i < ap_count; i++) {
//                         if (strcmp((char *)ap_list[i].ssid, CONFIG_WIFI_SS) == 0) {
                            
//                             // Log di ogni singola rete trovata
//                             ESP_LOGI(TAG, "  -> MAC: %02x:%02x:%02x:%02x:%02x:%02x | Canale: %2d | RSSI: %d dBm",
//                                      ap_list[i].bssid[0], ap_list[i].bssid[1], ap_list[i].bssid[2],
//                                      ap_list[i].bssid[3], ap_list[i].bssid[4], ap_list[i].bssid[5],
//                                      ap_list[i].primary, ap_list[i].rssi);

//                             if (ap_list[i].rssi > best_rssi + 5) {
//                                 best_rssi = ap_list[i].rssi;
//                                 best_channel = ap_list[i].primary;
//                                 memcpy(best_bssid, ap_list[i].bssid, 6);
//                                 better_found = true;
//                             }
//                         }
//                     }

//                     if (better_found) {
//                         ESP_LOGI(TAG, "📡 Cambio AP in corso! Migliore: %d dBm (Canale: %d)", best_rssi, best_channel);
                        
//                         wifi_config_t conf;
//                         esp_wifi_get_config(WIFI_IF_STA, &conf);
//                         memcpy(conf.sta.bssid, best_bssid, 6);
//                         conf.sta.bssid_set = true;
//                         conf.sta.channel = best_channel;

//                         s_is_locked = true; // <--- AGGIUNTO
//                         s_retry_num = 0;    // <--- AGGIUNTO

//                         esp_wifi_disconnect();
//                         esp_wifi_set_config(WIFI_IF_STA, &conf);
//                         esp_wifi_connect();
//                     }
//                     free(ap_list);
//                 }
//             }
//         }
//     }
// }
// void wifi_init_sta(void)
// {
//     s_wifi_event_group = xEventGroupCreate();

//     esp_netif_init();
//     esp_event_loop_create_default();
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     wifi_country_t country = {.cc = "IT", .schan = 1, .nchan = 13, .max_tx_power = 20, .policy = WIFI_COUNTRY_POLICY_AUTO};
//     esp_wifi_set_country(&country);

//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = CONFIG_WIFI_SS,
//             .password = CONFIG_WIFI_PASSWORD,
//             .threshold.authmode = WIFI_AUTH_WPA2_PSK,
//             .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
//         },
//     };

//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     // --- FASE AVVIO: scan iniziale (BLE non ancora avviato) ---
//     uint8_t best_bssid[6];
//     uint8_t best_channel;
//     int best_rssi;

//     vTaskDelay(pdMS_TO_TICKS(1000));

//     ESP_LOGI(TAG, "🌍 Scansione GLOBALE iniziale...");

//     bool initial_lock = false;
//     for (int i = 0; i < 3; i++)
//     {
//         if (find_best_network_global(best_bssid, &best_channel, &best_rssi))
//         {
//             ESP_LOGI(TAG, "👑 VINCITORE: Canale %d con %d dBm.", best_channel, best_rssi);

//             memcpy(wifi_config.sta.bssid, best_bssid, 6);
//             wifi_config.sta.bssid_set = true;
//             wifi_config.sta.channel = best_channel;

//             ESP_ERROR_CHECK(esp_wifi_stop());
//             ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//             ESP_ERROR_CHECK(esp_wifi_start());

//             s_is_locked = true;
//             memcpy(current_bssid, best_bssid, 6);
//             initial_lock = true;
//             break;
//         }
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }

//     if (!initial_lock)
//     {
//         ESP_LOGW(TAG, "⚠ Nessuna rete trovata. Avvio Auto-Connect.");
//     }

//     ESP_LOGI(TAG, "Connessione...");
//     esp_wifi_connect();

//     xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
//     xTaskCreate(wifi_background_scan_task, "wifi_bg_scan", 4096, NULL, 3, NULL);
// }

// int wifi_get_rssi(void)
// {
//     wifi_ap_record_t ap_info;
//     if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
//         return ap_info.rssi;
//     return 0;
// }
#include "wifi_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "credential.h"
#include "offline_journal.h"

// --- CONFIGURAZIONE ---

static const char *TAG = "WIFI_MANAGER";
EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_num = 0;
static bool s_is_locked = false;

static uint8_t current_bssid[6] = {0};
static TaskHandle_t s_reconnect_task = NULL;

static bool find_best_network_global(uint8_t *bssid_out, uint8_t *channel_out, int *rssi_out)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100, .scan_time.active.max = 200
    };

    esp_err_t ret;
    int retries = 0;

    do {
        ret = esp_wifi_scan_start(&scan_config, true);
        if (ret == ESP_ERR_WIFI_STATE) {
            vTaskDelay(pdMS_TO_TICKS(200));
            retries++;
        }
    } while (ret != ESP_OK && retries < 5);

    if (ret != ESP_OK) return false;

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) return false;

    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_list) return false;

    esp_wifi_scan_get_ap_records(&ap_count, ap_list);

    bool found = false;
    int best_rssi = -128;

    ESP_LOGI(TAG, "🔍 Scansione completa: Trovati %d AP per %s.", ap_count, CONFIG_WIFI_SS);

    for (int i = 0; i < ap_count; i++) {
        if (strcmp((char *)ap_list[i].ssid, CONFIG_WIFI_SS) == 0) {
            
            ESP_LOGI(TAG, "  -> MAC: %02x:%02x:%02x:%02x:%02x:%02x | Canale: %2d | RSSI: %d dBm",
                     ap_list[i].bssid[0], ap_list[i].bssid[1], ap_list[i].bssid[2],
                     ap_list[i].bssid[3], ap_list[i].bssid[4], ap_list[i].bssid[5],
                     ap_list[i].primary, ap_list[i].rssi);

            if (ap_list[i].rssi < 0 && ap_list[i].rssi > -95) {
                if (ap_list[i].rssi > best_rssi) {
                    best_rssi = ap_list[i].rssi;
                    *channel_out = ap_list[i].primary;
                    memcpy(bssid_out, ap_list[i].bssid, 6);
                    *rssi_out = best_rssi;
                    found = true;
                }
            }
        }
    }
    free(ap_list);
    return found;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_set_max_tx_power(8);
        ESP_LOGI(TAG, "WiFi avviato.");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_retry_num++;

        if (s_is_locked && s_retry_num > 3) {
            ESP_LOGE(TAG, "🚨 Antenna bloccata non risponde! Sblocco BSSID.");
            s_is_locked = false;
            s_retry_num = 0;

            wifi_config_t conf;
            esp_wifi_get_config(WIFI_IF_STA, &conf);
            conf.sta.bssid_set = false;
            memset(conf.sta.bssid, 0, 6);
            esp_wifi_set_config(WIFI_IF_STA, &conf);
            esp_wifi_connect();
            return;
        }

        if (s_retry_num > 5) {
            ESP_LOGW(TAG, "WiFi: troppi tentativi falliti, entro in modalità OFFLINE.");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            return;
        }

        ESP_LOGI(TAG, "Disconnesso. Riprovo... (%d/5)", s_retry_num);
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        wifi_ap_record_t ap_info;
        esp_wifi_sta_get_ap_info(&ap_info);

        ESP_LOGI(TAG, "✓ CONNESSO!");
        ESP_LOGI(TAG, "   Canale: %d | RSSI: %d dBm", ap_info.primary, ap_info.rssi);
        ESP_LOGI(TAG, "   IP: " IPSTR, IP2STR(&event->ip_info.ip));

        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (offline_journal_count() > 0 && s_reconnect_task) {
            ESP_LOGI(TAG, "WiFi riconnesso — notifico task replay journal...");
            xTaskNotifyGive(s_reconnect_task);
        }
    }
}

static void wifi_reconnect_task(void *pvParameters)
{
    while (1) {
        // Attende notifica (da GOT_IP) o timeout 30s (per riconnessione dopo FAIL_BIT)
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000));

        if (notified > 0) {
            // Notificato da event_handler: WiFi tornato, replay journal
            ESP_LOGI(TAG, "wifi_reconnect_task: replay journal offline...");
            offline_journal_replay();
        } else {
            // Timeout: se siamo in FAIL_BIT, riprova connessione
            if (xEventGroupGetBits(s_wifi_event_group) & WIFI_FAIL_BIT) {
                ESP_LOGI(TAG, "wifi_reconnect_task: riprovo connessione WiFi...");
                xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
                s_retry_num = 0;
                esp_wifi_connect();
            }
        }
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_country_t country = { .cc = "IT", .schan = 1, .nchan = 13, .max_tx_power = 20, .policy = WIFI_COUNTRY_POLICY_AUTO };
    esp_wifi_set_country(&country);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SS,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // --- FASE AVVIO: scan iniziale (BLE non ancora avviato) ---
    uint8_t best_bssid[6];
    uint8_t best_channel;
    int best_rssi;

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "🌍 Scansione GLOBALE iniziale...");

    bool initial_lock = false;
    for (int i = 0; i < 3; i++) {
        if (find_best_network_global(best_bssid, &best_channel, &best_rssi)) {
            ESP_LOGI(TAG, "👑 VINCITORE: Canale %d con %d dBm.", best_channel, best_rssi);

            memcpy(wifi_config.sta.bssid, best_bssid, 6);
            wifi_config.sta.bssid_set = true;
            wifi_config.sta.channel = best_channel;

            ESP_ERROR_CHECK(esp_wifi_stop());
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            ESP_ERROR_CHECK(esp_wifi_start());

            s_is_locked = true;
            memcpy(current_bssid, best_bssid, 6);
            initial_lock = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!initial_lock) {
        ESP_LOGW(TAG, "⚠ Nessuna rete trovata. Avvio Auto-Connect.");
    }

    ESP_LOGI(TAG, "Connessione...");
    esp_wifi_connect();

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    xTaskCreate(wifi_reconnect_task, "wifi_reconnect", 8192, NULL, 3, &s_reconnect_task);
}

int wifi_get_rssi(void)
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) return ap_info.rssi;
    return 0;
}

bool wifi_is_connected(void)
{
    if (!s_wifi_event_group) return false;
    return (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}