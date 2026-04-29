#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>
#include "json_parser.h"  // BANCHETTO_MAX_ITEMS

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct _objects_t
    {
        lv_obj_t *main;
        lv_obj_t *obj0;   // btn scarti
        lv_obj_t *obj1;
        lv_obj_t *obj2;
        lv_obj_t *obj3;
        lv_obj_t *obj4;
        lv_obj_t *obj5;   // arc avanzamento fase
        lv_obj_t *obj6;   // label "/ totale"
        lv_obj_t *obj7;   // label valore fase
        lv_obj_t *obj8;
        lv_obj_t *obj9;   // label prod sessione
        lv_obj_t *obj10;
        lv_obj_t *obj11;  // panel feedback visivo
        lv_obj_t *obj12;  // label scatola pezzi/capacità
        lv_obj_t *obj13;
        lv_obj_t *obj14;
        lv_obj_t *obj15;
        lv_obj_t *obj16;  // label matricola scatola
        lv_obj_t *obj17;  // label matricola operatore
        lv_obj_t *obj18;  // pill stato (contenitore)
        lv_obj_t *obj19;  // pill stato (label)
    } objects_t;

    // Array indicizzato per articolo
    extern objects_t objects[BANCHETTO_MAX_ITEMS];

    void create_screen_main(uint8_t idx);
    void tick_screen_main(void);
    void create_screens(void);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/