#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Sincronizza il RTC con l'orario del server.
// Va chiamata una volta all'avvio, quando il WiFi è connesso.
bool time_manager_sync(void);

// Ritorna true se il RTC è stato sincronizzato almeno una volta.
bool time_manager_is_synced(void);

// Scrive in buf il timestamp corrente: "YYYY-MM-DD HH:MM:SS"
void time_manager_get_ts(char *buf, size_t len);

// Ripristina l'orario dall'ultimo salvataggio su SD (chiamare dopo mount SD, se non ancora sincronizzato).
void time_manager_restore_from_sd(void);

// Avvia un task che salva l'orario su SD ogni 60s (chiamare dopo mount SD).
void time_manager_start_periodic_save(void);

#ifdef __cplusplus
}
#endif
