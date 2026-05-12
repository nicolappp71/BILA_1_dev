#ifndef MODE_H
#define MODE_H

// Decommenta questa riga quando sei a casa, commentala quando sei in ufficio
#define CASA

// Decommenta per vedere solo WARNING ed ERROR nel monitor seriale
// #define LOG_QUIET

// Codice banchetto che ha le pagine collaudo (bilancia simulazione web)
#define COLLAUDI_BANCHETTO_ID "222"
// Codice banchetto spalmatrice 2 assi
#define SPAL_BANCHETTO_ID "177"
// commemntare per contagiri
#define TEST
#ifdef CASA
#define SERVER_BASE "http://192.168.1.58"
#else
#define SERVER_BASE "http://intranet.cifarelli.loc"
#endif

#endif // MODE_H
