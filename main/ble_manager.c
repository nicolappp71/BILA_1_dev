#include "ble_manager.h"
#include "banchetto_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_gatt.h"
#include "freertos/queue.h"
#include <string.h>
#include "cJSON.h"

extern void collaudo_app_on_side_change(const char *side_id);

/* ID banchetto che abilita la modalità collaudi (multi-target) */
#define COLLAUDI_BANCHETTO_ID "222"

static const char *TAG = "BLE_MGR";
static volatile bool s_scan_paused = false;

/* Modalità determinata una volta in ble_manager_init() */
static bool s_collaudi_mode = false;

/* UUID del servizio e characteristic */
static const ble_uuid128_t svc_uuid =
    BLE_UUID128_INIT(0xbb, 0xaa, 0x33, 0x22, 0x11, 0x00, 0xcd, 0xab,
                     0x78, 0x56, 0x34, 0x12, 0x00, 0xa0, 0xe5, 0xd0);

static const ble_uuid128_t chr_uuid =
    BLE_UUID128_INIT(0xbb, 0xaa, 0x33, 0x22, 0x11, 0x00, 0xcd, 0xab,
                     0x78, 0x56, 0x34, 0x12, 0x01, 0xa0, 0xe5, 0xd0);

/* ═══════════════════════════════════════════════════════════
 * STATO MODALITÀ COLLAUDI  (banchetto 222 — multi-target)
 * ═══════════════════════════════════════════════════════════ */
#define MAX_TARGETS 3

typedef struct {
    const char *name;
    uint16_t    conn_handle;
    bool        connected;
} ble_target_t;

static ble_target_t s_targets[MAX_TARGETS] = {
    { "BTN_DX",  0, false },
    { "BTN_SX",  0, false },
    { "BTN_CTR", 0, false },
};
static const int NUM_TARGETS = 3;

/* Target in corso di connessione (impostato nel DISC, letto nel CONNECT) */
static ble_target_t *s_connecting_target = NULL;

/* ═══════════════════════════════════════════════════════════
 * STATO MODALITÀ STANDARD  (altri banchetti — target singolo)
 * ═══════════════════════════════════════════════════════════ */
static char     s_target_name[32] = {0};
static uint16_t s_conn_handle     = 0;
static bool     s_is_connected    = false;
static QueueHandle_t s_ble_action_queue = NULL;

/* Extern */
extern void ui_update_ble_status(bool connected);
extern void deep_sleep_reset_timer(void);

/* Forward declarations */
static void ble_start_scan(void);
static int  ble_gap_event_cb(struct ble_gap_event *event, void *arg);

/* ═══════════════════════════════════════════════════════════
 * HELPER MODALITÀ COLLAUDI
 * ═══════════════════════════════════════════════════════════ */
static ble_target_t *find_target_by_name(const char *name)
{
    for (int i = 0; i < NUM_TARGETS; i++) {
        if (strcmp(s_targets[i].name, name) == 0)
            return &s_targets[i];
    }
    return NULL;
}

static ble_target_t *find_target_by_handle(uint16_t handle)
{
    for (int i = 0; i < NUM_TARGETS; i++) {
        if (s_targets[i].connected && s_targets[i].conn_handle == handle)
            return &s_targets[i];
    }
    return NULL;
}

static bool all_connected(void)
{
    for (int i = 0; i < NUM_TARGETS; i++) {
        if (!s_targets[i].connected)
            return false;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════
 * HELPER MODALITÀ STANDARD
 * ═══════════════════════════════════════════════════════════ */
static void update_target_name(void)
{
    const char *id = banchetto_manager_get_banchetto_id();
    if (id && id[0] != '\0')
        snprintf(s_target_name, sizeof(s_target_name), "CNC_%s", id);
}

/* Task isolato per eseguire versa fuori dal contesto BLE */
static void ble_action_task(void *param)
{
    uint32_t action_cmd;
    while (1) {
        if (xQueueReceive(s_ble_action_queue, &action_cmd, portMAX_DELAY) == pdTRUE) {
            if (action_cmd == 1) {
                ESP_LOGW(TAG, "Comando VERSA ricevuto. Attendo che il BLE si liberi...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                ESP_LOGW(TAG, "Eseguo VERSA ora.");
                deep_sleep_reset_timer();
                banchetto_manager_versa(1);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 * GATT — callbacks condivise
 * ═══════════════════════════════════════════════════════════ */
static int ble_on_subscribe(uint16_t conn, const struct ble_gatt_error *error,
                            struct ble_gatt_attr *attr, void *arg)
{
    const char *name = s_target_name;
    if (s_collaudi_mode) {
        ble_target_t *t = find_target_by_handle(conn);
        if (t) name = t->name;
    }
    if (error->status == 0)
        ESP_LOGI(TAG, "[%s] Subscribed alle indication OK", name);
    else
        ESP_LOGE(TAG, "[%s] Errore subscribe: %d", name, error->status);
    return 0;
}

static int ble_on_disc_chr(uint16_t conn, const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg)
{
    if (error->status == 0 && chr != NULL) {
        uint8_t value[2] = {0x02, 0x00};
        int rc = ble_gattc_write_flat(conn, chr->val_handle + 1,
                                      value, sizeof(value),
                                      ble_on_subscribe, NULL);
        if (rc != 0)
            ESP_LOGE(TAG, "Errore write CCCD: %d", rc);
    } else if (error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "Errore discovery chr: %d", error->status);
    }
    return 0;
}

static int ble_on_disc_svc(uint16_t conn, const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *svc, void *arg)
{
    if (error->status == 0 && svc != NULL) {
        int rc = ble_gattc_disc_chrs_by_uuid(conn, svc->start_handle,
                                             svc->end_handle,
                                             &chr_uuid.u,
                                             ble_on_disc_chr, NULL);
        if (rc != 0)
            ESP_LOGE(TAG, "Errore disc chr: %d", rc);
    } else if (error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "Errore discovery svc: %d", error->status);
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 * GAP event callback
 * ═══════════════════════════════════════════════════════════ */
static int ble_gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    /* ── DISC ── */
    case BLE_GAP_EVENT_DISC:
    {
        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields, event->disc.data,
                                    event->disc.length_data) != 0)
            return 0;
        if (fields.name == NULL || fields.name_len == 0)
            return 0;

        char name[32] = {0};
        int len = fields.name_len < (int)(sizeof(name) - 1)
                  ? fields.name_len : (int)(sizeof(name) - 1);
        memcpy(name, fields.name, len);

        if (s_collaudi_mode) {
            ble_target_t *t = find_target_by_name(name);
            if (t == NULL || t->connected)
                return 0;

            ESP_LOGI(TAG, "[COLLAUDI] Trovato %s! RSSI: %d — connessione...",
                     name, event->disc.rssi);
            s_connecting_target = t;
            ble_gap_disc_cancel();
            int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr,
                                     10000, NULL, ble_gap_event_cb, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Errore connect a %s: %d", name, rc);
                ble_start_scan();
            }
        } else {
            if (s_target_name[0] == '\0' || strcmp(name, s_target_name) != 0)
                return 0;

            ESP_LOGI(TAG, "Trovato %s! RSSI: %d, connessione...",
                     s_target_name, event->disc.rssi);
            ble_gap_disc_cancel();
            int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr,
                                     10000, NULL, ble_gap_event_cb, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Errore connect: %d", rc);
                ble_start_scan();
            }
        }
        return 0;
    }

    /* ── CONNECT ── */
    case BLE_GAP_EVENT_CONNECT:
    {
        if (event->connect.status == 0) {
            uint16_t h = event->connect.conn_handle;

            if (s_collaudi_mode) {
                ble_target_t *t = s_connecting_target;
                s_connecting_target = NULL;
                if (t) {
                    t->conn_handle = h;
                    t->connected   = true;
                    ESP_LOGI(TAG, "[COLLAUDI] [%s] Connesso! handle=%d", t->name, h);
                    ble_gattc_exchange_mtu(h, NULL, NULL);
                    ble_gattc_disc_svc_by_uuid(h, &svc_uuid.u, ble_on_disc_svc, NULL);
                }
                for (int i = 0; i < NUM_TARGETS; i++) {
                    ESP_LOGI(TAG, "  %s → %s", s_targets[i].name,
                             s_targets[i].connected ? "CONNESSO" : "non connesso");
                }
                if (!all_connected())
                    ble_start_scan();
            } else {
                s_conn_handle  = h;
                s_is_connected = true;
                ui_update_ble_status(true);
                ESP_LOGI(TAG, "Connesso a %s! handle: %d", s_target_name, h);
                int rc = ble_gattc_disc_svc_by_uuid(h, &svc_uuid.u, ble_on_disc_svc, NULL);
                if (rc != 0)
                    ESP_LOGE(TAG, "Errore disc svc: %d", rc);
            }
        } else {
            ESP_LOGW(TAG, "Connessione fallita: %d, riscan...", event->connect.status);
            if (!s_collaudi_mode) {
                s_is_connected = false;
                ui_update_ble_status(false);
            }
            ble_start_scan();
        }
        return 0;
    }

    /* ── DISCONNECT ── */
    case BLE_GAP_EVENT_DISCONNECT:
    {
        if (s_collaudi_mode) {
            ble_target_t *t = find_target_by_handle(event->disconnect.conn.conn_handle);
            if (t) {
                ESP_LOGW(TAG, "[COLLAUDI] [%s] Disconnesso (reason=%d), riscan...",
                         t->name, event->disconnect.reason);
                t->connected   = false;
                t->conn_handle = 0;
            } else {
                ESP_LOGW(TAG, "[COLLAUDI] Disconnect da handle sconosciuto %d",
                         event->disconnect.conn.conn_handle);
            }
        } else {
            ESP_LOGW(TAG, "Disconnesso da %s (reason: %d), riscan...",
                     s_target_name, event->disconnect.reason);
            s_is_connected = false;
            s_conn_handle  = 0;
            ui_update_ble_status(false);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        ble_start_scan();
        return 0;
    }

    /* ── NOTIFY RX ── */
    case BLE_GAP_EVENT_NOTIFY_RX:
    {
        if (event->notify_rx.om == NULL)
            return 0;

        uint16_t pkt_len = OS_MBUF_PKTLEN(event->notify_rx.om);
        char buf[128] = {0};
        if (pkt_len >= sizeof(buf)) {
            ESP_LOGE(TAG, "Payload troppo grande (%d bytes), ignorato", pkt_len);
            return 0;
        }
        os_mbuf_copydata(event->notify_rx.om, 0, pkt_len, buf);
        buf[pkt_len] = '\0';

        if (s_collaudi_mode) {
            ble_target_t *t = find_target_by_handle(event->notify_rx.conn_handle);
            const char   *src = t ? t->name : "UNKNOWN";
            ESP_LOGW(TAG, "[COLLAUDI] [%s] RICEVUTO: %s", src, buf);

            // Estrai lato da btn {"v":1,"id":"CTR","btn":"LATO_SX","press":"SHORT"}
            cJSON *json = cJSON_Parse(buf);
            if (json) {
                cJSON *btn_item = cJSON_GetObjectItem(json, "btn");
                if (btn_item && cJSON_IsString(btn_item)) {
                    const char *btn = btn_item->valuestring;
                    if (strcmp(btn, "LATO_SX") == 0)
                        collaudo_app_on_side_change("SX");
                    else if (strcmp(btn, "LATO_DX") == 0)
                        collaudo_app_on_side_change("DX");
                }
                cJSON_Delete(json);
            }
        } else {
            ESP_LOGI(TAG, "RICEVUTO: %s (indication: %d)",
                     buf, event->notify_rx.indication);

            const char *banc_id = banchetto_manager_get_banchetto_id();
            char id_check[48];
            snprintf(id_check, sizeof(id_check), "\"id\":\"%s\"", banc_id);

            if (strstr(buf, "\"v\":1") && strstr(buf, id_check)) {
                uint32_t cmd = 1;
                if (s_ble_action_queue)
                    xQueueSend(s_ble_action_queue, &cmd, 0);
            } else {
                ESP_LOGW(TAG, "Payload non riconosciuto o ID errato: %s", buf);
            }
        }
        return 0;
    }

    /* ── DISC COMPLETE ── */
    case BLE_GAP_EVENT_DISC_COMPLETE:
    {
        if (s_collaudi_mode) {
            if (!all_connected()) {
                ESP_LOGI(TAG, "[COLLAUDI] Scan completata, device mancanti. Riscan...");
                vTaskDelay(pdMS_TO_TICKS(2000));
                ble_start_scan();
            }
        } else {
            if (!s_is_connected) {
                if (s_target_name[0] == '\0') {
                    ESP_LOGW(TAG, "Banchetto non ancora assegnato, riprovo tra 5s...");
                    vTaskDelay(pdMS_TO_TICKS(5000));
                } else {
                    ESP_LOGI(TAG, "Scan completato, %s non trovato. Riscan...", s_target_name);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                }
                ble_start_scan();
            }
        }
        return 0;
    }

    default:
        return 0;
    }
}

/* ═══════════════════════════════════════════════════════════
 * Avvia scan
 * ═══════════════════════════════════════════════════════════ */
static void ble_start_scan(void)
{
    if (s_scan_paused) {
        ESP_LOGD(TAG, "Scan BLE sospesa (WiFi scan in corso).");
        return;
    }

    if (s_collaudi_mode) {
        if (all_connected()) {
            ESP_LOGD(TAG, "[COLLAUDI] Tutti i device connessi, scan non necessaria.");
            return;
        }
        for (int i = 0; i < NUM_TARGETS; i++) {
            if (!s_targets[i].connected)
                ESP_LOGI(TAG, "[COLLAUDI] Cerco: %s...", s_targets[i].name);
        }
    } else {
        if (s_target_name[0] == '\0') {
            ESP_LOGW(TAG, "Nessun banchetto configurato, scan rinviata.");
            return;
        }
        ESP_LOGI(TAG, "Scan BLE per %s...", s_target_name);
    }

    struct ble_gap_disc_params scan_params = {
        .itvl              = 160,
        .window            = 80,
        .filter_policy     = 0,
        .limited           = 0,
        .passive           = 0,
        .filter_duplicates = s_collaudi_mode ? 0 : 1,
    };

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 15000, &scan_params,
                          ble_gap_event_cb, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY)
        ESP_LOGE(TAG, "Errore avvio scan: %d", rc);
}

/* ═══════════════════════════════════════════════════════════
 * Sync / Reset
 * ═══════════════════════════════════════════════════════════ */
static void ble_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Errore config indirizzo BLE: %d", rc);
        return;
    }

    if (s_collaudi_mode) {
        ble_att_set_preferred_mtu(128);
        ESP_LOGI(TAG, "[COLLAUDI] Stack BLE pronto. Cerco %d target...", NUM_TARGETS);
    } else {
        ESP_LOGI(TAG, "Stack BLE pronto, target: %s", s_target_name);
    }
    ble_start_scan();
}

static void ble_on_reset(int reason)
{
    ESP_LOGW(TAG, "Stack BLE reset, reason: %d", reason);
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "Task NimBLE host avviato");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ═══════════════════════════════════════════════════════════
 * Pause / Resume scan (coesistenza WiFi)
 * ═══════════════════════════════════════════════════════════ */
void ble_manager_pause_scan(void)
{
    s_scan_paused = true;
    if (ble_gap_disc_active()) {
        ble_gap_disc_cancel();
        ESP_LOGI(TAG, "Scan BLE sospesa per WiFi scan.");
    }
}

void ble_manager_resume_scan(void)
{
    s_scan_paused = false;
    bool needs_scan = s_collaudi_mode ? !all_connected() : !s_is_connected;
    if (needs_scan) {
        ESP_LOGI(TAG, "Scan BLE ripresa.");
        ble_start_scan();
    }
}

/* ═══════════════════════════════════════════════════════════
 * Init
 * ═══════════════════════════════════════════════════════════ */
void ble_manager_init(void)
{
    const char *bid = banchetto_manager_get_banchetto_id();

    /* Determina la modalità una volta sola — l'ID è già disponibile qui */
    s_collaudi_mode = false;

    /* Banchetto 233 (tagliatubi) e 222 (collaudo): nessun BLE */
    if (bid && (strcmp(bid, "233") == 0 || strcmp(bid, COLLAUDI_BANCHETTO_ID) == 0)) {
        ESP_LOGW(TAG, "Banchetto %s: BLE disabilitato", bid);
        return;
    }

    update_target_name();
    ESP_LOGI(TAG, "BLE Manager — target: %s",
             s_target_name[0] ? s_target_name : "(banchetto non ancora caricato)");

    /* Coda e task separato per la modalità standard */
    s_ble_action_queue = xQueueCreate(5, sizeof(uint32_t));
    xTaskCreate(ble_action_task, "ble_action", 8192, NULL, 5, NULL);

    int rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Errore nimble_port_init: %d", rc);
        return;
    }

    ble_hs_cfg.sync_cb  = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE Manager inizializzato");
}
