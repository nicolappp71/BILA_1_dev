#ifndef OFFLINE_JOURNAL_H
#define OFFLINE_JOURNAL_H

#ifdef __cplusplus
extern "C" {
#endif

// Aggiunge una riga JSON al journal su SD
void offline_journal_append(const char *json_line);

// Stampa tutte le righe formattate al boot (ESP_LOGI)
void offline_journal_print_all(void);

// Quante operazioni pendenti ci sono
int  offline_journal_count(void);

// Replay: invia al server tutte le operazioni pendenti (chiamare quando online).
// Ritorna il numero di operazioni inviate con successo.
int  offline_journal_replay(void);

#ifdef __cplusplus
}
#endif

#endif // OFFLINE_JOURNAL_H
