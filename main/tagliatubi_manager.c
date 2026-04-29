#include "tagliatubi_manager.h"
#include "banchetto_manager.h"
#include "http_client.h"
#include "bsp/esp-bsp.h"

// Dichiarata nel BSP locale (waveshare_bsp) — non presente nel managed_components
extern esp_err_t bsp_display_panel_recover(void);

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mode.h"

static const char *TAG = "tagliatubi";

// ─── Server ──────────────────────────────────────────────────────────────────
#define TAGL_PHP_URL    SERVER_BASE "/iot/tagliatubi.php"

// ─── Internal events ─────────────────────────────────────────────────────────
#define EVT_START           BIT0
#define EVT_STOP            BIT1
#define EVT_SINGOLO         BIT2
#define EVT_AVANTI          BIT3
#define EVT_TAGLIO          BIT4
#define EVT_PCNT_OVERFLOW   BIT5

#define EVT_ANY_CMD  (EVT_START | EVT_SINGOLO | EVT_AVANTI | EVT_TAGLIO)

// ─── State ───────────────────────────────────────────────────────────────────
static tagliatubi_state_t    s_state     = TAGL_STATE_IDLE;
static tagliatubi_data_t     s_data      = {0};
static tagliatubi_state_cb_t s_callback  = NULL;
static EventGroupHandle_t    s_evg       = NULL;

// ─── PCNT ────────────────────────────────────────────────────────────────────
static pcnt_unit_handle_t s_pcnt_unit   = NULL;
static volatile int32_t   s_pcnt_accum  = 0;

// ─── RMT stepper ─────────────────────────────────────────────────────────────
static rmt_channel_handle_t  s_rmt_chan     = NULL;
static rmt_encoder_handle_t  s_rmt_encoder = NULL;
static rmt_symbol_word_t     s_step_sym    = {0};  // updated before each move

// ─── Versa async queue ───────────────────────────────────────────────────────
static QueueHandle_t s_versa_queue = NULL;

static void versa_task(void *arg)
{
    uint32_t qta;
    while (1) {
        if (xQueueReceive(s_versa_queue, &qta, portMAX_DELAY) == pdTRUE) {
            banchetto_manager_versa(qta);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

static void notify(tagliatubi_state_t new_state)
{
    s_state = new_state;
    if (s_callback) {
        s_callback(new_state, &s_data);
    }
}

// Sportello chiuso (NC: HIGH=ok, LOW=aperto)
static inline bool is_safe(void)
{
#if TAGL_SAFETY_DEBUG
    return true;
#else
    return gpio_get_level(TAGL_MICRO_CARTER_GPIO) == 1;
#endif
}

// Presenza materiale - micro a leva NC/NO, pull-up (HIGH=materiale presente)
static inline bool has_material(void)
{
#if TAGL_SAFETY_DEBUG
    return true;
#else
    return gpio_get_level(TAGL_MICRO_MATERIALE) == 1;
#endif
}

// Uomo morto - contatto NO, pull-up (LOW=premuto)
static inline bool is_uomo_morto(void)
{
#if TAGL_SAFETY_DEBUG
    return true;
#else
    return gpio_get_level(TAGL_SICUREZZA_GPIO) == 0;
#endif
}

// Controlla le precondizioni e restituisce lo stato di errore specifico,
// oppure TAGL_STATE_IDLE se tutto ok.
static tagliatubi_state_t check_start(void)
{
    if (!is_uomo_morto()) return TAGL_STATE_ERROR_UOMO_MORTO;
    if (!is_safe())       return TAGL_STATE_ERROR_SAFETY;
    if (!has_material())  return TAGL_STATE_ERROR_NO_MATERIAL;
    return TAGL_STATE_IDLE;
}

// ─────────────────────────────────────────────────────────────────────────────
//  PCNT accumulation (handles counter overflow for long tubes)
// ─────────────────────────────────────────────────────────────────────────────

static bool IRAM_ATTR pcnt_overflow_cb(pcnt_unit_handle_t unit,
                                        const pcnt_watch_event_data_t *edata,
                                        void *user_ctx)
{
    BaseType_t hp = pdFALSE;
    xEventGroupSetBitsFromISR(s_evg, EVT_PCNT_OVERFLOW, &hp);
    return hp == pdTRUE;
}

// Call from task context only
static void pcnt_flush(void)
{
    int raw = 0;
    pcnt_unit_get_count(s_pcnt_unit, &raw);
    s_pcnt_accum += raw;
    pcnt_unit_clear_count(s_pcnt_unit);
}

static void pcnt_reset_all(void)
{
    pcnt_unit_clear_count(s_pcnt_unit);
    s_pcnt_accum = 0;
    xEventGroupClearBits(s_evg, EVT_PCNT_OVERFLOW);
}

static int32_t pcnt_total(void)
{
    if (xEventGroupGetBits(s_evg) & EVT_PCNT_OVERFLOW) {
        xEventGroupClearBits(s_evg, EVT_PCNT_OVERFLOW);
        pcnt_flush();
    }
    int raw = 0;
    pcnt_unit_get_count(s_pcnt_unit, &raw);
    return s_pcnt_accum + raw;
}

static float pcnt_to_mm(int32_t counts)
{
    if (counts < 0) counts = -counts;
    return (float)counts / (float)TAGL_ENC_COUNTS_PER_REV * TAGL_ENC_WHEEL_CIRC_MM;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Motor (RMT, chunked for safe mid-movement abort)
// ─────────────────────────────────────────────────────────────────────────────

static uint32_t calc_nstep(void)
{
    if (s_data.diametro <= 0) return 0;
    float circ = (float)s_data.diametro * TAGL_PI / 10.0f;
    return (uint32_t)((float)s_data.lunghezza / circ * (float)TAGL_STEPS_PER_REV);
}

// velocita 1-99 → half-period 800-75 µs (same mapping as Arduino map())
static uint32_t calc_half_period_us(void)
{
    int v = s_data.velocita;
    if (v < 1)  v = 1;
    if (v > 99) v = 99;
    return (uint32_t)(800 - (800 - 40) * (v - 1) / 98);
}

static void motor_move(uint32_t total_pulses)
{
    // Update symbol timing for current velocity
    uint32_t hp = calc_half_period_us();
    ESP_LOGI(TAG, "[motor_move] steps=%lu  velocita=%d  half_period=%lu us",
             (unsigned long)total_pulses, s_data.velocita, (unsigned long)hp);
    s_step_sym.level0    = 1;
    s_step_sym.duration0 = hp;
    s_step_sym.level1    = 0;
    s_step_sym.duration1 = hp;

    gpio_set_level(TAGL_EN_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(5));

    // Send pulses in chunks of 200 so we can check sensors between chunks.
    // At max speed (75 µs/step): 200 steps = 30 ms per chunk → responsive enough.
    const uint32_t CHUNK = 200;
    uint32_t remaining = total_pulses;

    while (remaining > 0) {
        // Safety / abort check — debounced: require 3 consecutive LOW reads (6 ms) to confirm
        if (!is_safe()) {
            vTaskDelay(pdMS_TO_TICKS(2));
            if (!is_safe()) {
                vTaskDelay(pdMS_TO_TICKS(2));
                if (!is_safe()) {
                    ESP_LOGW(TAG, "Motor stopped: sportello aperto (remaining=%lu  uomo_morto=%d  carter=%d  mat=%d)",
                             (unsigned long)remaining,
                             gpio_get_level(TAGL_SICUREZZA_GPIO),
                             gpio_get_level(TAGL_MICRO_CARTER_GPIO),
                             gpio_get_level(TAGL_MICRO_MATERIALE));
                    break;
                }
            }
        }
        if (!has_material()) {
            ESP_LOGW(TAG, "Motor stopped: materiale assente");
            break;
        }
        if (xEventGroupGetBits(s_evg) & EVT_STOP) {
            ESP_LOGW(TAG, "Motor stopped: STOP da utente (remaining=%lu)", (unsigned long)remaining);
            break;
        }

        // Handle PCNT overflow flag from ISR
        if (xEventGroupGetBits(s_evg) & EVT_PCNT_OVERFLOW) {
            xEventGroupClearBits(s_evg, EVT_PCNT_OVERFLOW);
            pcnt_flush();
        }

        uint32_t chunk = (remaining < CHUNK) ? remaining : CHUNK;

        rmt_transmit_config_t tx_cfg = {
            .loop_count = (int)chunk,
            .flags.eot_level = 0,
        };
        rmt_transmit(s_rmt_chan, s_rmt_encoder, &s_step_sym, sizeof(s_step_sym), &tx_cfg);
        // Wait for this chunk to complete (timeout generous: chunk * 2 * hp µs + margin)
        uint32_t timeout_ms = (chunk * 2 * hp) / 1000 + 50;
        rmt_tx_wait_all_done(s_rmt_chan, timeout_ms);

        remaining -= chunk;
    }

    gpio_set_level(TAGL_EN_GPIO, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cutting sequence
// ─────────────────────────────────────────────────────────────────────────────

static void do_taglio(bool conta)
{
    notify(TAGL_STATE_CUTTING);

    vTaskDelay(pdMS_TO_TICKS(TAGL_TEMPO_TAGLIO_MS));
    gpio_set_level(TAGL_VALVOLA_A_GPIO, 1);
    gpio_set_level(TAGL_VALVOLA_C_GPIO, 0);

    vTaskDelay(pdMS_TO_TICKS(TAGL_TEMPO_TAGLIO_MS));
    gpio_set_level(TAGL_VALVOLA_C_GPIO, 1);
    gpio_set_level(TAGL_VALVOLA_A_GPIO, 0);

    vTaskDelay(pdMS_TO_TICKS(TAGL_TEMPO_TAGLIO_MS));
    gpio_set_level(TAGL_VALVOLA_C_GPIO, 1);
    gpio_set_level(TAGL_VALVOLA_A_GPIO, 1);

    if (conta) {
        s_data.prodotti++;
        uint32_t q = 1;
        xQueueSend(s_versa_queue, &q, 0);  // asincrono: non blocca il task motore
        ESP_LOGI(TAG, "Taglio OK — prodotti: %d/%d", (int)s_data.prodotti, (int)s_data.quantita);
    } else {
        ESP_LOGI(TAG, "Taglio manuale (non conteggiato)");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Encoder length check after movement
//  Returns true if length is within tolerance (same logic as Arduino: >= 80%)
// ─────────────────────────────────────────────────────────────────────────────

static bool encoder_check_ok(void)
{
    if (s_data.diametro <= 0 || s_data.lunghezza <= 0) {
        return true;  // skip check if no dimensions set
    }
    int32_t cnt = pcnt_total();
    float measured = pcnt_to_mm(cnt);
    s_data.lunghezza_misurata = (int32_t)measured;

    float min_ok = (float)s_data.lunghezza * 0.85f;  // -15%
    float max_ok = (float)s_data.lunghezza * 1.15f;  // +15%

    ESP_LOGI(TAG, "Encoder: %.1f mm (counts=%ld), target: %d mm (±15%%: %.1f–%.1f) → %s",
             measured, (long)cnt, (int)s_data.lunghezza, min_ok, max_ok,
             (measured >= min_ok && measured <= max_ok) ? "OK" : "FAIL");

    return (measured >= min_ok && measured <= max_ok);
}

// ─────────────────────────────────────────────────────────────────────────────
//  HTTP helpers
// ─────────────────────────────────────────────────────────────────────────────

static esp_err_t http_send_int(int com, int valore)
{
    char url[160];
    snprintf(url, sizeof(url), TAGL_PHP_URL "?com=%d&nid=%d&valore=%d",
             com, s_data.id, valore);
    int code = 0;
    char *body = NULL;
    esp_err_t ret = http_get_request(url, &code, &body);
    if (body) free(body);
    return ret;
}

// Parse semicolon-delimited response: #;nid;codice;descrizione;lunghezza;diametro;quantita;prodotti;velocita
static esp_err_t parse_product_response(const char *body)
{
    if (!body || strlen(body) == 0) return ESP_FAIL;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s", body);

    char *tok = strtok(buf, ";");
    int field = 1;
    while (tok) {
        switch (field) {
            case 1: /* # marker — skip */                                                                  break;
            case 2: s_data.id        = atoi(tok);                                                          break;
            case 3: snprintf(s_data.codice,      sizeof(s_data.codice),      "%s", tok);                  break;
            case 4: snprintf(s_data.descrizione, sizeof(s_data.descrizione), "%s", tok);                  break;
            case 5: s_data.lunghezza = atoi(tok);                                                          break;
            case 6: s_data.diametro  = atoi(tok);                                                          break;
            case 7: s_data.quantita  = atoi(tok);                                                          break;
            case 8: s_data.prodotti  = atoi(tok);                                                          break;
            case 9: s_data.velocita  = atoi(tok);                                                          break;
        }
        tok = strtok(NULL, ";");
        field++;
    }
    return (s_data.id > 0) ? ESP_OK : ESP_FAIL;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main cycle task (runs on Core 1, priority 5)
// ─────────────────────────────────────────────────────────────────────────────

static void tagliatubi_task(void *arg)
{
    while (1) {
        EventBits_t ev = xEventGroupWaitBits(s_evg, EVT_ANY_CMD,
                                              pdTRUE, pdFALSE, portMAX_DELAY);

        // ── Cut only (no movement) ───────────────────────────────────────────
        if (ev & EVT_TAGLIO) {
            if (!is_safe()) {
                notify(TAGL_STATE_ERROR_SAFETY);
                vTaskDelay(pdMS_TO_TICKS(500));
                notify(TAGL_STATE_IDLE);
                continue;
            }
            if (!has_material()) {
                notify(TAGL_STATE_ERROR_NO_MATERIAL);
                vTaskDelay(pdMS_TO_TICKS(500));
                notify(TAGL_STATE_IDLE);
                continue;
            }
            do_taglio(false);  // manuale: non conta
            notify(TAGL_STATE_IDLE);
            continue;
        }

        // ── Advance only (no cut) — fixed steps ─────────────────────────────
        if (ev & EVT_AVANTI) {
            if (!is_safe()) {
                notify(TAGL_STATE_ERROR_SAFETY);
                vTaskDelay(pdMS_TO_TICKS(500));
                notify(TAGL_STATE_IDLE);
                continue;
            }
            if (!has_material()) {
                notify(TAGL_STATE_ERROR_NO_MATERIAL);
                vTaskDelay(pdMS_TO_TICKS(500));
                notify(TAGL_STATE_IDLE);
                continue;
            }
            pcnt_reset_all();
            motor_move(TAGL_AVANTI_STEPS);
            vTaskDelay(pdMS_TO_TICKS(50));
            {
                int32_t cnt = pcnt_total();
                float mm = pcnt_to_mm(cnt);
                ESP_LOGI(TAG, "[AVANTI] encoder: %d count → %.1f mm", (int)cnt, mm);
            }
            if (!is_safe()) {
                notify(TAGL_STATE_ERROR_SAFETY);
                vTaskDelay(pdMS_TO_TICKS(500));
                notify(TAGL_STATE_IDLE);
                continue;
            }
            if (!has_material()) {
                notify(TAGL_STATE_ERROR_NO_MATERIAL);
                vTaskDelay(pdMS_TO_TICKS(500));
                notify(TAGL_STATE_IDLE);
                continue;
            }
            notify(TAGL_STATE_IDLE);
            continue;
        }

        // ── Single cycle (advance + cut, once) ──────────────────────────────
        if (ev & EVT_SINGOLO) {
            if (!is_safe() || !has_material()) {
                notify(TAGL_STATE_ERROR_NO_MATERIAL);
                vTaskDelay(pdMS_TO_TICKS(500));
                notify(TAGL_STATE_IDLE);
                continue;
            }
            {
                banchetto_data_t bd;
                banchetto_manager_get_data(&bd);
                if (bd.qta_totale_scatola > 0 && bd.qta_scatola >= bd.qta_totale_scatola) {
                    ESP_LOGI(TAG, "Scatola piena (%lu/%lu) — singolo bloccato",
                             bd.qta_scatola, bd.qta_totale_scatola);
                    notify(TAGL_STATE_BOX_FULL);
                    continue;
                }
            }
            uint32_t nstep = calc_nstep();
            pcnt_reset_all();
            motor_move(nstep * TAGL_LOOP_MULTIPLIER);
            vTaskDelay(pdMS_TO_TICKS(50));   // attendi fine inerzia meccanica

            if (!has_material()) {
                notify(TAGL_STATE_ERROR_NO_MATERIAL);
                vTaskDelay(pdMS_TO_TICKS(500));
                notify(TAGL_STATE_IDLE);
                continue;
            }
            if (!is_safe()) {
                notify(TAGL_STATE_ERROR_SAFETY);
                vTaskDelay(pdMS_TO_TICKS(500));
                notify(TAGL_STATE_IDLE);
                continue;
            }
            if (!encoder_check_ok()) {
                notify(TAGL_STATE_ERROR_LENGTH);
                vTaskDelay(pdMS_TO_TICKS(500));
                notify(TAGL_STATE_IDLE);
                continue;
            }
            do_taglio(true);   // singolo: conta il pezzo
            notify(TAGL_STATE_IDLE);
            continue;
        }

        // ── Full automatic cycle ─────────────────────────────────────────────
        if (ev & EVT_START) {
            notify(TAGL_STATE_RUNNING);

            while (s_data.prodotti < s_data.quantita) {

                // Abort?
                if (xEventGroupGetBits(s_evg) & EVT_STOP) {
                    xEventGroupClearBits(s_evg, EVT_STOP);
                    ESP_LOGI(TAG, "Cycle stopped by user");
                    break;
                }

                // Safety checks
                if (!is_safe()) {
                    notify(TAGL_STATE_ERROR_SAFETY);
                    break;
                }
                if (!has_material()) {
                    notify(TAGL_STATE_ERROR_NO_MATERIAL);
                    break;
                }

                // Scatola piena? Sospendi il ciclo prima di avanzare
                banchetto_data_t bd;
                banchetto_manager_get_data(&bd);
                if (bd.qta_totale_scatola > 0 && bd.qta_scatola >= bd.qta_totale_scatola) {
                    ESP_LOGI(TAG, "Scatola piena (%lu/%lu) — ciclo sospeso",
                             bd.qta_scatola, bd.qta_totale_scatola);
                    notify(TAGL_STATE_BOX_FULL);
                    break;
                }

                // Move
                uint32_t nstep = calc_nstep();
                pcnt_reset_all();
                motor_move(nstep * TAGL_LOOP_MULTIPLIER);
                vTaskDelay(pdMS_TO_TICKS(50));   // attendi fine inerzia meccanica

                // STOP premuto durante il movimento → torna IDLE senza errori
                if (xEventGroupGetBits(s_evg) & EVT_STOP) {
                    xEventGroupClearBits(s_evg, EVT_STOP);
                    ESP_LOGI(TAG, "Cycle stopped by user (post-move)");
                    notify(TAGL_STATE_IDLE);
                    break;
                }

                // Post-move checks
                if (!has_material()) {
                    notify(TAGL_STATE_ERROR_NO_MATERIAL);
                    break;
                }
                if (!is_safe()) {
                    notify(TAGL_STATE_ERROR_SAFETY);
                    break;
                }

                // Encoder length verification
                if (!encoder_check_ok()) {
                    ESP_LOGW(TAG, "Encoder fail — pezzo scartato");
                    notify(TAGL_STATE_ERROR_LENGTH);
                    break;
                }

                do_taglio(true);  // ciclo automatico: conta
                notify(TAGL_STATE_RUNNING);
            }

            // Batch complete
            if (s_data.prodotti >= s_data.quantita && s_data.quantita > 0) {
                http_send_int(3, s_data.prodotti);  // com=3: report final count
                ESP_LOGI(TAG, "Batch done: %d pezzi", (int)s_data.prodotti);
                s_data.prodotti = 0;
                notify(TAGL_STATE_DONE);
            } else {
                notify(TAGL_STATE_IDLE);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hardware init
// ─────────────────────────────────────────────────────────────────────────────

static esp_err_t gpio_init_hw(void)
{
    // Outputs: STEP, DIR, EN, VALVOLA_A, VALVOLA_C
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << TAGL_STEP_GPIO)        |
                        (1ULL << TAGL_DIR_GPIO)          |
                        (1ULL << TAGL_EN_GPIO)           |
                        (1ULL << TAGL_VALVOLA_A_GPIO)    |
                        (1ULL << TAGL_VALVOLA_C_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out));

    // Safe initial state (mirrors Arduino setup())
    gpio_set_level(TAGL_EN_GPIO,         0);
    gpio_set_level(TAGL_STEP_GPIO,       0);
    gpio_set_level(TAGL_DIR_GPIO,        1);   // forward direction
    gpio_set_level(TAGL_VALVOLA_C_GPIO,  1);
    gpio_set_level(TAGL_VALVOLA_A_GPIO,  0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(TAGL_VALVOLA_C_GPIO,  1);
    gpio_set_level(TAGL_VALVOLA_A_GPIO,  1);

    // Inputs with pull-up: SICUREZZA (uomo morto), MICRO_CARTER, MICRO_MATERIALE
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << TAGL_SICUREZZA_GPIO)      |
                        (1ULL << TAGL_MICRO_CARTER_GPIO)    |
                        (1ULL << TAGL_MICRO_MATERIALE),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    return gpio_config(&in);
}

static esp_err_t rmt_init_hw(void)
{
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num           = TAGL_STEP_GPIO,
        .clk_src            = RMT_CLK_SRC_DEFAULT,
        .resolution_hz      = 1000000,   // 1 MHz → 1 µs resolution
        .mem_block_symbols  = 64,
        .trans_queue_depth  = 4,
        .flags.invert_out   = false,
        .flags.with_dma     = false,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &s_rmt_chan));

    rmt_copy_encoder_config_t enc_cfg = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&enc_cfg, &s_rmt_encoder));

    ESP_ERROR_CHECK(rmt_enable(s_rmt_chan));
    return ESP_OK;
}

static esp_err_t pcnt_init_hw(void)
{
    // Limits generous enough for long tubes before accumulation kicks in
    pcnt_unit_config_t unit_cfg = {
        .low_limit  = -30000,
        .high_limit =  30000,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_cfg, &s_pcnt_unit));

    // Channel A: edges on A, level gated by B → quadrature decoding
    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num  = TAGL_ENC_A_GPIO,
        .level_gpio_num = TAGL_ENC_B_GPIO,
    };
    pcnt_channel_handle_t chan_a;
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt_unit, &chan_a_cfg, &chan_a));
    pcnt_channel_set_edge_action(chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // Channel B: edges on B, level gated by A → quadrature decoding
    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num  = TAGL_ENC_B_GPIO,
        .level_gpio_num = TAGL_ENC_A_GPIO,
    };
    pcnt_channel_handle_t chan_b;
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt_unit, &chan_b_cfg, &chan_b));
    pcnt_channel_set_edge_action(chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // Watch points to detect overflow and accumulate into s_pcnt_accum
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(s_pcnt_unit,  20000));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(s_pcnt_unit, -20000));

    pcnt_event_callbacks_t cbs = { .on_reach = pcnt_overflow_cb };
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(s_pcnt_unit, &cbs, NULL));

    ESP_ERROR_CHECK(pcnt_unit_enable(s_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_pcnt_unit));

    // Pull-up interni per encoder open-collector (GPIO non configurati altrove)
    gpio_pullup_en(TAGL_ENC_A_GPIO);
    gpio_pullup_en(TAGL_ENC_B_GPIO);

    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

esp_err_t tagliatubi_manager_init(void)
{
    s_evg = xEventGroupCreate();
    if (!s_evg) return ESP_ERR_NO_MEM;

    s_versa_queue = xQueueCreate(8, sizeof(uint32_t));
    if (!s_versa_queue) return ESP_ERR_NO_MEM;
    xTaskCreatePinnedToCore(versa_task, "tagl_versa", 4096, NULL, 3, NULL, 0);

    ESP_ERROR_CHECK(gpio_init_hw());
    ESP_ERROR_CHECK(rmt_init_hw());
    ESP_ERROR_CHECK(pcnt_init_hw());

    // Pin core 1 so UI (core 0) is never blocked by the motor loop
    xTaskCreatePinnedToCore(tagliatubi_task, "tagl_cycle",
                            12288, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "Tagliatubi manager initialized");
    return ESP_OK;
}

esp_err_t tagliatubi_manager_set_callback(tagliatubi_state_cb_t cb)
{
    s_callback = cb;
    return ESP_OK;
}

esp_err_t tagliatubi_manager_load_product(const char *codice)
{
    char url[160];
    snprintf(url, sizeof(url), TAGL_PHP_URL "?com=5&cod=%s", codice);
    int code = 0;
    char *body = NULL;
    esp_err_t ret = http_get_request(url, &code, &body);
    ESP_LOGI(TAG, "[DBG load] url=%s  http_code=%d  body='%s'", url, code, body ? body : "(null)");
    if (ret == ESP_OK && body) {
        ret = parse_product_response(body);
        ESP_LOGI(TAG, "[DBG load] parse→ id=%d lunghezza=%ld diametro=%ld quantita=%ld velocita=%d ret=%d",
                 s_data.id, s_data.lunghezza, s_data.diametro, s_data.quantita, s_data.velocita, ret);
    }
    if (body) free(body);
    return ret;
}

esp_err_t tagliatubi_manager_send_lunghezza(void)  {
    ESP_LOGI(TAG, "[DBG send] id=%d lunghezza=%ld", s_data.id, s_data.lunghezza);
    return http_send_int(1, s_data.lunghezza);
}
esp_err_t tagliatubi_manager_send_quantita(void)   {
    ESP_LOGI(TAG, "[DBG send] id=%d quantita=%ld", s_data.id, s_data.quantita);
    return http_send_int(2, s_data.quantita);
}
esp_err_t tagliatubi_manager_send_velocita(void)   {
    ESP_LOGI(TAG, "[DBG send] id=%d velocita=%d", s_data.id, s_data.velocita);
    return http_send_int(4, s_data.velocita);
}

void tagliatubi_manager_set_lunghezza(int32_t mm)  { s_data.lunghezza = mm; }
void tagliatubi_manager_set_quantita(int32_t q)    { s_data.quantita  = q;  }
void tagliatubi_manager_set_velocita(int v)        { s_data.velocita  = v;  }

esp_err_t tagliatubi_manager_start_ciclo(void)
{
    if (s_state != TAGL_STATE_IDLE && s_state != TAGL_STATE_DONE)
        return ESP_ERR_INVALID_STATE;
    {
        tagliatubi_state_t err = check_start();
        if (err != TAGL_STATE_IDLE) {
            notify(err);
            vTaskDelay(pdMS_TO_TICKS(500));
            notify(TAGL_STATE_IDLE);
            return ESP_ERR_NOT_ALLOWED;
        }
    }
    xEventGroupSetBits(s_evg, EVT_START);
    return ESP_OK;
}

esp_err_t tagliatubi_manager_stop(void)
{
    xEventGroupSetBits(s_evg, EVT_STOP);
    return ESP_OK;
}

esp_err_t tagliatubi_manager_singolo(void)
{
    if (s_state != TAGL_STATE_IDLE) return ESP_ERR_INVALID_STATE;
    {
        tagliatubi_state_t err = check_start();
        if (err != TAGL_STATE_IDLE) {
            notify(err);
            vTaskDelay(pdMS_TO_TICKS(500));
            notify(TAGL_STATE_IDLE);
            return ESP_ERR_NOT_ALLOWED;
        }
    }
    xEventGroupSetBits(s_evg, EVT_SINGOLO);
    return ESP_OK;
}

esp_err_t tagliatubi_manager_avanti(void)
{
    if (s_state != TAGL_STATE_IDLE && s_state != TAGL_STATE_BOX_FULL) return ESP_ERR_INVALID_STATE;
    {
        tagliatubi_state_t err = check_start();
        if (err != TAGL_STATE_IDLE) {
            notify(err);
            vTaskDelay(pdMS_TO_TICKS(500));
            notify(TAGL_STATE_IDLE);
            return ESP_ERR_NOT_ALLOWED;
        }
    }
    xEventGroupSetBits(s_evg, EVT_AVANTI);
    return ESP_OK;
}

esp_err_t tagliatubi_manager_taglio(void)
{
    if (s_state != TAGL_STATE_IDLE && s_state != TAGL_STATE_BOX_FULL) return ESP_ERR_INVALID_STATE;
    {
        tagliatubi_state_t err = check_start();
        if (err != TAGL_STATE_IDLE) {
            notify(err);
            vTaskDelay(pdMS_TO_TICKS(500));
            notify(TAGL_STATE_IDLE);
            return ESP_ERR_NOT_ALLOWED;
        }
    }
    xEventGroupSetBits(s_evg, EVT_TAGLIO);
    return ESP_OK;
}

tagliatubi_state_t tagliatubi_manager_get_state(void)
{
    return s_state;
}

bool tagliatubi_manager_is_uomo_morto(void)  { return is_uomo_morto();   }
bool tagliatubi_manager_is_carter(void)       { return is_safe();         }
bool tagliatubi_manager_is_materiale(void)    { return has_material();    }
bool tagliatubi_manager_is_motor_busy(void)
{
    return (s_state == TAGL_STATE_RUNNING || s_state == TAGL_STATE_CUTTING);
}

const tagliatubi_data_t *tagliatubi_manager_get_data(void)
{
    return &s_data;
}
