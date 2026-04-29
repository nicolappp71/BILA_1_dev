#include "ui.h"
#include "screens.h"

// ui_init e ui_tick non sono usate da AppBanchetto
// (che chiama create_screens() direttamente in run())
// mantenute per compatibilità con eventuali altri include

void ui_init(void) {
    create_screens();
}

void ui_tick(void) {
    tick_screen_main();
}