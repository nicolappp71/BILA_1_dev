# MULTI_ORDINI_SEMI

Sistema di gestione banchetti di produzione industriale per **ESP32-P4** con display touchscreen Waveshare 7".
Progetto **ESP-IDF** scritto in C/C++ misto, con interfaccia grafica LVGL tramite framework ESP-Brookesia.

---

## Indice

- [Panoramica](#panoramica)
- [Hardware](#hardware)
- [Architettura Software](#architettura-software)
- [State Machine](#state-machine)
- [Componenti principali (main/)](#componenti-principali-main)
- [Applicazioni (components/apps/)](#applicazioni-componentsapps)
- [Componenti Hardware (components/)](#componenti-hardware-components)
- [Comunicazione con il Server](#comunicazione-con-il-server)
- [Partizionamento Flash](#partizionamento-flash)
- [File System SPIFFS](#file-system-spiffs)
- [Configurazione ambiente](#configurazione-ambiente)
- [Compilazione e Flash](#compilazione-e-flash)
- [Dipendenze IDF Component Manager](#dipendenze-idf-component-manager)

---

## Panoramica

Il sistema è un **terminale industriale** per la gestione delle postazioni di produzione (banchetti) in un'azienda manifatturiera.
Ogni dispositivo è installato fisicamente su un banchetto operatore e consente di:

- **Autenticare** gli operatori tramite badge RFID o scanner USB a codice a barre
- **Associare** il banchetto fisico a un ordine di produzione scaricato dal server ERP aziendale
- **Contare** la quantità prodotta per fase/ciclo con gestione scatole e scarti
- **Gestire il flusso qualità**: controllo da parte del responsabile, formazione operatori
- **Monitorare** lo stato del dispositivo (batteria, WiFi, ora) nella status bar
- **Esporre una dashboard web** via WebSocket per supervisori in rete locale
- **Navigare documenti** (immagini JPEG) ospitati sul server intranet
- **Registrare log** su SD card per diagnostica

---

## Hardware

| Componente | Dettaglio |
|---|---|
| **SoC** | ESP32-P4 |
| **Display** | Waveshare ESP32-P4 WiFi6 Touch LCD 7B — 1024×600 px, touch capacitivo |
| **Driver LCD** | EK79007 (DSI) |
| **Connettività** | Wi-Fi 6 (via `esp_hosted` + `esp_wifi_remote`) |
| **Lettore RFID** | UART (porta seriale dedicata) |
| **Scanner barcode** | USB HID (`usb_host_hid`) |
| **Batteria** | Li-Ion/Li-Po — monitoraggio ADC su GPIO37 (partitore R92=200kΩ, R93=100kΩ, rapporto 1:3) |
| **Audio** | Codec BSP (`bsp_extra_codec_init`) — file MP3 su SPIFFS |
| **Storage** | SD card (log e dati) + SPIFFS (audio, asset) |
| **I2C** | Bus condiviso per periferiche hardware |

---

## Architettura Software

```
┌─────────────────────────────────────────────────────────────────┐
│                        main/main.cpp                            │
│  app_main(): init hardware → init networking → init periferiche │
│              → crea UI Phone → installa app → avvia task        │
└───────────────────────┬─────────────────────────────────────────┘
                        │ FreeRTOS Tasks
          ┌─────────────┼──────────────┐
          ▼             ▼              ▼
   WiFi Status    Orario Server   Battery Status
   (ogni 2s)      (ogni 60s)      (ogni 60s)
          │
          └── aggiorna ESP_Brookesia_StatusBar (LVGL thread-safe)

┌─────────────────────────────────────────────────────────────────┐
│                    ESP-Brookesia Phone UI                        │
│                                                                  │
│  ┌──────────────┐  ┌────────────┐  ┌─────────┐  ┌───────────┐  │
│  │ AppBanchetto │  │   Logged   │  │DocBrowser│  │Calculator │  │
│  │  (core app)  │  │(popup scan)│  │(immagini)│  │           │  │
│  └──────┬───────┘  └─────┬──────┘  └─────────┘  └───────────┘  │
│         │                │                                        │
│  ┌──────▼───────────────▼──────────────────────────────────┐   │
│  │               banchetto_manager.c                        │   │
│  │         State Machine + dati produzione                  │   │
│  └──────────────┬───────────────────────────────────────────┘   │
└─────────────────┼───────────────────────────────────────────────┘
                  │
     ┌────────────┼─────────────┐
     ▼            ▼             ▼
http_client   web_server    json_parser
(GET/POST)   (WS dashboard)  (cJSON)
     │
     ▼
  Intranet Server (PHP/ERP)
  http://intranet.cifarelli.loc
```

### Flusso di avvio (`app_main`)

1. `log_manager_init()` — ring buffer PSRAM + hook vprintf, cattura tutto dal primo istante
2. `nvs_flash_init()` — Non-Volatile Storage per chiave dispositivo e settings
3. `bsp_spiffs_mount()` — monta partizione audio/asset
4. `bsp_extra_codec_init()` — inizializza codec audio
5. `bsp_display_start_with_config()` — avvia LVGL con buffer in SPIRAM
6. `wifi_init_sta()` + `esp_netif_init()` — connette alla rete aziendale
7. `gpio_init()`, `init_usb_driver()`, `init_rfid_uart()` — periferiche di input
8. `key_manager_init()` — chiave univoca dispositivo (NVS)
9. `banchetto_manager_init()` + `banchetto_manager_fetch_from_server()` — carica ordini dal server
10. `web_server_init()` — dashboard HTTP/WebSocket
11. `battery_manager_init()` — ADC calibrato per livello batteria
12. Creazione `ESP_Brookesia_Phone` con stylesheet dark 1024×600
13. Task FreeRTOS: WiFi status, orario server, batteria
14. `bsp_sdcard_mount()` + `log_manager_sd_ready()` — abilita log su SD
15. Installazione app nel launcher
16. `bsp_display_unlock()` + `bsp_display_backlight_on()` — display visibile

---

## State Machine

Il `banchetto_manager` governa il flusso operativo tramite una state machine:

```
                    ┌───────────────────────────────────┐
                    │         CHECKIN                   │
                    │  Attesa badge operatore / RFID    │
                    └────────────┬──────────────────────┘
                                 │ badge valido
                    ┌────────────▼──────────────────────┐
                    │    ASSEGNA_BANCHETTO               │
                    │  Scan barcode banchetto fisico     │
                    └────────────┬──────────────────────┘
                                 │ barcode valido
                    ┌────────────▼──────────────────────┐
                    │         CONTEGGIO                  │
                    │  Sessione aperta, conta produzione │
                    │  versa() / scarto()                │
                    └────────────┬──────────────────────┘
                                 │ badge controllore
                    ┌────────────▼──────────────────────┐
                    │         CONTROLLO                  │
                    │  Verifica qualità responsabile     │
                    └────────────┬──────────────────────┘
                                 │ oppure percorso formazione
              ┌──────────────────▼──────────────────────────┐
              │           ATTESA_FORMATORE                   │
              │  Badge formatore richiesto                   │
              └──────────────────┬──────────────────────────┘
                                 │
              ┌──────────────────▼──────────────────────────┐
              │      ATTESA_CONFERMA_FORMAZIONE              │
              │  Conferma accettazione formazione            │
              └─────────────────────────────────────────────┘
```

Ogni transizione chiama `banchetto_manager_set_state()` e aggiorna la UI LVGL in modo thread-safe.

---

## Componenti principali (main/)

| File | Responsabilità |
|---|---|
| `main.cpp` | Entry point, orchestrazione avvio, task FreeRTOS |
| `banchetto_manager.c/h` | State machine, dati ordini, login badge, versamento/scarto |
| `json_parser.c/h` | Parsing JSON server (lista banchetti, risposta badge) via cJSON |
| `http_client.c/h` | HTTP GET/POST verso server ERP + recupero orario |
| `web_server.c/h` | Server HTTP locale con WebSocket per dashboard supervisori |
| `wifi_manager.c/h` | Connessione WiFi STA, EventGroup `WIFI_CONNECTED_BIT` |
| `rfid_manager.c/h` | Lettore RFID su UART |
| `scanner_manager.c/h` | Scanner barcode USB HID (keycode → ASCII) |
| `battery_manager.c/h` | ADC calibrato GPIO37, percentuale con media mobile su 10 campioni |
| `key_manager.c/h` | Chiave univoca 16 byte in NVS (`storage/device_key`) |
| `gpio_manager.c/h` | Configurazione GPIO |
| `i2c_manager.c/h` | Bus I2C condiviso |
| `settings_manager.c/h` | Persistenza impostazioni |
| `log_manager.c/h` | Ring buffer PSRAM + scrittura asincrona su SD card |
| `audio_manager.c/h` | Riproduzione audio |
| `mode.h` | Switch URL server: `CASA` (192.168.1.58:10000) vs ufficio (intranet.cifarelli.loc) |
| `credential.h` | SSID/password WiFi per le due reti |

### Struttura dati principale: `banchetto_data_t`

```c
typedef struct {
    char     banchetto[16];           // ID banchetto fisico
    char     operatore[64];           // Nome operatore loggato
    uint32_t qta_totale_scatola;      // Quantità totale per scatola
    char     matr_scatola_corrente[32];
    char     matricola[16];           // Matricola operatore
    bool     sessione_aperta;
    bool     blocca_qta;
    char     codice_articolo[32];     // Codice ERP articolo
    char     descrizione_articolo[128];
    uint32_t qta_totale;              // Quantità ordine totale
    uint32_t ord_prod;                // Numero ordine di produzione
    char     ciclo[16];               // Codice ciclo lavorazione
    char     fase[16];                // Fase corrente
    char     descr_fase[128];
    uint32_t qta_prod_fase;           // Quantità prodotta in fase
    uint32_t qta_totale_giornaliera;
    uint32_t qta_prod_sessione;       // Prodotto in sessione corrente
    uint32_t qta_scatola;             // Pezzi per scatola
    uint32_t qta_pezzi;               // Moltiplicatore ciclo macchina
    // timestamp
    uint8_t  giorno, mese; uint16_t anno;
    uint8_t  ore, minuti, secondi;
} banchetto_data_t;
```

Il sistema gestisce fino a **10 articoli** contemporaneamente (`BANCHETTO_MAX_ITEMS = 10`), navigabili con swipe nella UI.

---

## Applicazioni (components/apps/)

Tutte le app ereditano da `ESP_Brookesia_PhoneApp` e implementano `init()`, `run()`, `back()`, `close()`.

### AppBanchetto *(app principale)*
- Visualizza gli ordini di produzione con swipe orizzontale tra articoli
- Mostra matricola, ciclo, codice articolo, OdP, fase, stato sessione
- Due pagine LVGL: riepilogo ordine (page1) e contatori produzione (page2)
- Aggiornamento thread-safe via `bsp_display_lock/unlock`

### Logged *(Associa Banchetto)*
- Popup modale con overlay scuro: attende la scansione del barcode del banchetto fisico
- Singleton (`Logged::instance`) accessibile da bridge C (`app_assegna_banchetto_close`)
- Alla chiusura: ricarica dati dal server e aggiorna AppBanchetto

### DocBrowser *(Browser documenti)*
- Naviga la struttura cartelle del server intranet tramite API HTTP/JSON
- Visualizza immagini JPEG con decodifica hardware (JPEG HW decoder P4)
- Modalità browser (lista cartelle/file) + modalità viewer (immagine a schermo intero)
- Gesture swipe per navigazione tra immagini, ricerca per nome file
- Download asincrono con task FreeRTOS dedicato

### Calculator
- Calcolatrice standard

### MiaApp
- App custom di utilità aziendale

### MusicPlayer
- Riproduce file MP3 da SPIFFS (`/spiffs/music/`)
- UI LVGL con lista tracce, controlli play/pause/next/prev, slider avanzamento

### Setting
- Configurazione WiFi (scan reti, connessione)
- Regolazione luminosità display
- Impostazioni volume audio
- Informazioni dispositivo

### Camera *(inclusa ma non installata di default)*
- Pipeline video con rilevamento visi (`human_face_detect`) e pedoni (`pedestrian_detect`)
- Modelli AI `.espdl` per ESP32-P4 e ESP32-S3

### VideoPlayer *(incluso ma non installato di default)*
- Player video con `esp_lvgl_simple_player`

---

## Componenti Hardware (components/)

| Componente | Descrizione |
|---|---|
| `waveshare_bsp` | BSP ufficiale Waveshare: display EK79007 DSI, touch GT911, codec audio, SD card, SPIFFS |
| `bsp_extra` | Estensioni BSP: inizializzazione codec, player audio MP3 |
| `esp_lcd_ek79007` | Driver LCD per pannello EK79007 (MIPI DSI) |
| `human_face_detect` | Modello AI rilevamento visi (`.espdl` per P4/S3) |
| `pedestrian_detect` | Modello AI rilevamento pedoni (`.espdl` per P4/S3) |

---

## Comunicazione con il Server

### URL di base
Configurato in `main/mode.h`:
```c
// Sviluppo (home):
#define SERVER_BASE "http://192.168.1.58:10000"
// Produzione (ufficio):
#define SERVER_BASE "http://intranet.cifarelli.loc"
```
Per passare da un ambiente all'altro, decommentare/commentare `#define CASA` in `mode.h`.

### Endpoint principali

| Endpoint | Protocollo | Uso |
|---|---|---|
| `SERVER_BASE/iot/banchetti_1_9_4.php` | HTTP GET | Scarica lista ordini banchetto |
| `SERVER_BASE/iot/badge.php` | HTTP POST | Autenticazione badge operatore |
| Endpoint orario | HTTP GET | Sincronizzazione orologio status bar |

### Web Server locale
- Dashboard HTML servita su `http://<IP_dispositivo>/`
- WebSocket per aggiornamenti real-time ai supervisori
- `web_server_broadcast_update()` chiamato automaticamente a ogni cambio stato

---

## Partizionamento Flash

```
# partitions.csv
nvs,      data, nvs,    0x11000,  0x6000   (24 KB  — NVS chiave, settings)
otadata,  data, ota,              0x2000   (8 KB   — OTA metadata)
phy_init, data, phy,              0x1000   (4 KB   — PHY calibration)
ota_0,    app,  ota_0,            10M      (app principale)
ota_1,    app,  ota_1,            10M      (OTA — aggiornamento firmware)
storage,  data, spiffs,           11M      (audio MP3, asset grafici)
```

Il sistema supporta **OTA (Over-The-Air)** update grazie alla doppia partizione app.

---

## File System SPIFFS

Montato in `/spiffs/`. Contiene:

```
spiffs/
├── music/
│   ├── beep.mp3
│   ├── BGM 1.mp3
│   ├── BGM 2.mp3
│   ├── For Elise.mp3
│   ├── Something.mp3
│   ├── TETRIS.mp3
│   └── Waka Waka.mp3
└── 2048/
    ├── excellent.mp3
    ├── good.mp3
    ├── normal.mp3
    └── weak.mp3
```

---

## Configurazione ambiente

### Prerequisiti

- **ESP-IDF** v5.3 o superiore (con supporto ESP32-P4)
- Toolchain `xtensa-esp32p4` (inclusa in IDF)
- Target: `esp32p4`

### File di configurazione importanti

| File | Scopo |
|---|---|
| `main/mode.h` | Switch ambiente (casa/ufficio) |
| `main/credential.h` | Credenziali WiFi per i due ambienti |
| `sdkconfig` / `sdkconfig.defaults` | Configurazione IDF (PSRAM, display, ecc.) |
| `partitions.csv` | Mappa flash personalizzata |

> **Nota sicurezza**: `credential.h` contiene password WiFi in chiaro. Non committare su repository pubblici.

---

## Compilazione e Flash

```bash
# 1. Configurare ambiente IDF
. $IDF_PATH/export.sh          # Linux/macOS
# oppure
%IDF_PATH%\export.bat          # Windows CMD

# 2. Impostare il target
idf.py set-target esp32p4

# 3. (Opzionale) Aprire menuconfig per personalizzazioni
idf.py menuconfig

# 4. Compilare
idf.py build

# 5. Flash + monitor seriale
idf.py -p COM_PORT flash monitor
# Su Linux: idf.py -p /dev/ttyUSB0 flash monitor

# 6. Solo monitor (senza reflash)
idf.py -p COM_PORT monitor
```

### Flash del filesystem SPIFFS

Se si modificano i file audio o gli asset:
```bash
idf.py -p COM_PORT spiffs-flash
# oppure manualmente con esptool:
python -m esptool --port COM_PORT write_flash <offset_spiffs> spiffs.bin
```

---

## Dipendenze IDF Component Manager

Gestite tramite `main/idf_component.yml` e `dependencies.lock`:

| Componente | Versione | Uso |
|---|---|---|
| `espressif/esp_wifi_remote` | `0.14.*` | WiFi su ESP32-P4 via host |
| `espressif/esp_hosted` | `1.4.*` | Stack WiFi hosted |
| `espressif/esp-brookesia` | `0.4.*` | Framework UI phone-style |
| `esp_video` | `0.8.*` | Pipeline video/camera |
| `espressif/esp_h264` | `==1.0.3` | Codec H.264 |
| `espressif/usb_host_hid` | `*` | Scanner barcode USB HID |

Le dipendenze vengono scaricate automaticamente con `idf.py build` tramite IDF Component Manager.
