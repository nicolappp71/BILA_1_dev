#pragma once
#include "lvgl.h"
#include "esp_brookesia.hpp"

class Logged : public ESP_Brookesia_PhoneApp
{
public:
    Logged();
    ~Logged();

    bool init(void) override;
    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    void chiudi(void);          // wrapper pubblico per notifyCoreClosed()
    static Logged *instance;    // pubblico: accessibile dal bridge C

private:
    lv_obj_t *overlay  = nullptr;  // overlay scuro a schermo intero
    lv_obj_t *popup    = nullptr;  // box popup centrale

    static void annulla_cb(lv_event_t *e);
    
};