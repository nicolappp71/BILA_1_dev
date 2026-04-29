#include "badge_cache.h"
#include "http_client.h"
#include "mode.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>

static const char *TAG = "BADGE_CACHE";

#define BADGE_CACHE_PATH   "/sdcard/badge_cache.json"
#define FORMATI_CACHE_PATH "/sdcard/formati_cache.json"

// ─────────────────────────────────────────────────────────────────────────────
// ALGORITMO CONVERSIONE UID HEX → BADGE KEY (porting da PHP numeroBadge)
// ─────────────────────────────────────────────────────────────────────────────
static bool uid_to_badge_key(const char *uid_hex, char *out, size_t out_len)
{
    if (!uid_hex || !out || out_len < 11) return false;

    char badge[32] = {0};
    for (int i = 0; uid_hex[i] && i < 31; i++)
        badge[i] = toupper((unsigned char)uid_hex[i]);

    // Controlla formato 10D: solo cifre, inizia con "00", niente 'E'
    bool all_digits = true;
    bool has_e = false;
    for (int j = 0; badge[j]; j++) {
        if (!isdigit((unsigned char)badge[j])) all_digits = false;
        if (badge[j] == 'E') has_e = true;
    }

    if (all_digits && !has_e && badge[0] == '0' && badge[1] == '0') {
        // Formato 10D → converte decimale→hex, pad a 10, aggiunge "00"
        uint64_t dec_val = strtoull(badge, NULL, 10);
        snprintf(badge, sizeof(badge), "%010" PRIX64 "00", dec_val);
    }

    if (strlen(badge) < 10) return false;

    // Ogni nibble → 4 bit → inverti ordine bit → ricomponi 40 bit
    uint8_t bits[40];
    for (int x = 0; x < 10; x++) {
        char c = badge[x];
        uint8_t nibble;
        if      (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else return false;

        bits[x*4+0] = (nibble >> 3) & 1;
        bits[x*4+1] = (nibble >> 2) & 1;
        bits[x*4+2] = (nibble >> 1) & 1;
        bits[x*4+3] = (nibble >> 0) & 1;

        // Inverti i 4 bit
        uint8_t tmp;
        tmp = bits[x*4+0]; bits[x*4+0] = bits[x*4+3]; bits[x*4+3] = tmp;
        tmp = bits[x*4+1]; bits[x*4+1] = bits[x*4+2]; bits[x*4+2] = tmp;
    }

    // 40 bit → uint64 → hex (10 char)
    uint64_t uid_dec = 0;
    for (int k = 0; k < 40; k++)
        uid_dec = (uid_dec << 1) | bits[k];

    char unique_id[12];
    snprintf(unique_id, sizeof(unique_id), "%010" PRIX64, uid_dec);

    // Ogni nibble hex → 2 cifre decimali (0-15 → "00"-"15")
    char dez[21] = {0};
    for (int x = 0; x < 10; x++) {
        char c = unique_id[x];
        uint8_t p;
        if      (c >= '0' && c <= '9') p = c - '0';
        else if (c >= 'A' && c <= 'F') p = c - 'A' + 10;
        else return false;
        dez[x*2]   = '0' + (p / 10);
        dez[x*2+1] = '0' + (p % 10);
    }
    dez[20] = '\0';

    // Seconda metà: posizioni 10-19
    memcpy(out, dez + 10, 10);
    out[10] = '\0';
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// UTILITY
// ─────────────────────────────────────────────────────────────────────────────

// Confronto matricola insensibile a zero-padding e tipo (stringa/numero)
static bool matricola_equal(const char *a, const char *b)
{
    if (!a || !b) return false;
    return atoi(a) == atoi(b);
}

static char *read_sd_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 128 * 1024) { fclose(f); return NULL; }
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read = fread(buf, 1, sz, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

static void save_sd_file(const char *path, const char *data)
{
    FILE *f = fopen(path, "w");
    if (!f) { ESP_LOGE(TAG, "Impossibile scrivere %s", path); return; }
    fputs(data, f);
    fclose(f);
}

// ─────────────────────────────────────────────────────────────────────────────
// REFRESH
// ─────────────────────────────────────────────────────────────────────────────
void badge_cache_refresh(const char *device_key, const char *banchetto)
{
    char url[256];
    int code = 0;
    char *body = NULL;

    // 1) get_badges
    snprintf(url, sizeof(url),
             "%s/iot/banchetti_1_9_4.php?key=%s&comando=get_badges",
             SERVER_BASE, device_key);
    if (http_get_request(url, &code, &body) == ESP_OK && code == 200 && body) {
        save_sd_file(BADGE_CACHE_PATH, body);
        ESP_LOGI(TAG, "badge_cache aggiornata (%d bytes)", (int)strlen(body));
        free(body); body = NULL;
    } else {
        ESP_LOGE(TAG, "get_badges fallito (code:%d)", code);
        if (body) { free(body); body = NULL; }
    }

    // 2) elenco_formati
    ESP_LOGI(TAG, "banchetto passato: '%s'", banchetto ? banchetto : "NULL");
    snprintf(url, sizeof(url),
             "%s/iot/banchetti_1_9_4.php?key=%s&comando=elenco_formati&banchetto=%s",
             SERVER_BASE, device_key, banchetto ? banchetto : "");
    ESP_LOGI(TAG, "elenco_formati URL: %s", url);
    if (http_get_request(url, &code, &body) == ESP_OK && code == 200 && body) {
        save_sd_file(FORMATI_CACHE_PATH, body);
        ESP_LOGI(TAG, "formati_cache aggiornata (%d bytes)", (int)strlen(body));
        free(body);
    } else {
        ESP_LOGE(TAG, "elenco_formati fallito (code:%d)", code);
        if (body) free(body);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// LOOKUP BADGE
// ─────────────────────────────────────────────────────────────────────────────
bool badge_cache_find(const char *uid_hex,
                      char *matricola_out, size_t mat_len,
                      char *nome_out,      size_t nome_len,
                      char *cognome_out,   size_t cog_len)
{
    char key[12] = {0};
    if (!uid_to_badge_key(uid_hex, key, sizeof(key))) {
        ESP_LOGE(TAG, "Conversione UID fallita: %s", uid_hex);
        return false;
    }
    ESP_LOGI(TAG, "UID %s → key %s", uid_hex, key);

    char *buf = read_sd_file(BADGE_CACHE_PATH);
    if (!buf) { ESP_LOGE(TAG, "badge_cache non disponibile"); return false; }

    cJSON *arr = cJSON_Parse(buf);
    free(buf);
    if (!arr) { ESP_LOGE(TAG, "badge_cache JSON non valido"); return false; }

    bool found = false;
    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n && !found; i++) {
        cJSON *item  = cJSON_GetArrayItem(arr, i);
        cJSON *badge = cJSON_GetObjectItem(item, "badge");
        if (!badge) continue;

        const char *badge_str = cJSON_IsString(badge) ? badge->valuestring : "";
        if (strcmp(badge_str, key) != 0) continue;

        found = true;

        cJSON *mat = cJSON_GetObjectItem(item, "matricola");
        cJSON *nom = cJSON_GetObjectItem(item, "nome");
        cJSON *cog = cJSON_GetObjectItem(item, "cognome");

        if (mat && matricola_out) {
            if (cJSON_IsString(mat))
                snprintf(matricola_out, mat_len, "%d", atoi(mat->valuestring));
            else if (cJSON_IsNumber(mat))
                snprintf(matricola_out, mat_len, "%d", mat->valueint);
        }
        if (nom && nome_out && cJSON_IsString(nom))
            strncpy(nome_out, nom->valuestring, nome_len - 1);
        if (cog && cognome_out && cJSON_IsString(cog))
            strncpy(cognome_out, cog->valuestring, cog_len - 1);
    }

    cJSON_Delete(arr);
    return found;
}

// ─────────────────────────────────────────────────────────────────────────────
// CHECK FORMAZIONE
// ─────────────────────────────────────────────────────────────────────────────
bool badge_cache_is_formato(const char *matricola, const char *cod_articolo)
{
    if (!matricola || !cod_articolo) return false;

    char *buf = read_sd_file(FORMATI_CACHE_PATH);
    if (!buf) { ESP_LOGW(TAG, "formati_cache non disponibile"); return false; }

    cJSON *arr = cJSON_Parse(buf);
    free(buf);
    if (!arr) return false;

    int mat_int = atoi(matricola);
    ESP_LOGI(TAG, "is_formato: cerco mat=%d art='%s' in %d voci", mat_int, cod_articolo, cJSON_GetArraySize(arr));
    bool found = false;
    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n && !found; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *op   = cJSON_GetObjectItem(item, "operatore");
        cJSON *art  = cJSON_GetObjectItem(item, "codice_articolo");
        if (!op || !art) continue;

        char op_buf[16] = {0};
        if (cJSON_IsString(op))       snprintf(op_buf, sizeof(op_buf), "%d", atoi(op->valuestring));
        else if (cJSON_IsNumber(op))  snprintf(op_buf, sizeof(op_buf), "%d", op->valueint);
        const char *op_str  = op_buf;
        const char *art_str = cJSON_IsString(art) ? art->valuestring : "";

        ESP_LOGI(TAG, "  [%d] op=%s(%d) art='%s'", i, op_str, atoi(op_str), art_str);

        if (atoi(op_str) == mat_int && strcmp(art_str, cod_articolo) == 0)
            found = true;
    }
    ESP_LOGI(TAG, "is_formato: risultato=%s", found ? "SI" : "NO");

    cJSON_Delete(arr);
    return found;
}
