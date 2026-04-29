#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Scarica dal server e salva su SD:
//   - get_badges     → /sdcard/badge_cache.json
//   - elenco_formati → /sdcard/formati_cache.json
// Va chiamato dopo banchetto_manager_fetch_from_server (quando online).
void badge_cache_refresh(const char *device_key, const char *banchetto);

// Cerca un badge nella cache usando l'UID raw del reader (es. "3700AD04C856").
// Ritorna true se trovato; popola matricola_out, nome_out, cognome_out.
bool badge_cache_find(const char *uid_hex,
                      char *matricola_out, size_t mat_len,
                      char *nome_out,      size_t nome_len,
                      char *cognome_out,   size_t cog_len);

// Controlla se l'operatore è formato per l'articolo specificato.
// Confronto normalizzato: "157" == "0157" == 157.
bool badge_cache_is_formato(const char *matricola, const char *cod_articolo);

#ifdef __cplusplus
}
#endif
