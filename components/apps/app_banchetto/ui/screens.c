#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"
#include "banchetto_manager.h"

// Forward declaration (implemented in AppBanchetto.cpp)
void add_versa_switch_c(lv_obj_t *sidebar);

// Definizione array globale
objects_t objects[BANCHETTO_MAX_ITEMS];

void create_screen_main(uint8_t idx)
{
    // ── Screen ───────────────────────────────────────────────
    lv_obj_t *obj = lv_obj_create(NULL);
    objects[idx].main = obj;
    lv_obj_set_pos(obj, 0, 0);
    //lv_obj_set_size(obj, 1024, 600);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xF0F0F0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *parent_obj = obj;

    // ── SIDEBAR (0,0 260x549) ────────────────────────────────
    {
        lv_obj_t *sb = lv_obj_create(parent_obj);
        lv_obj_set_pos(sb, 0, 0);
        lv_obj_set_size(sb, 260, 555);
        lv_obj_set_style_bg_color(sb, lv_color_hex(0x1C1C1C), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(sb, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(sb, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(sb, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(sb, 24, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(sb, LV_OBJ_FLAG_EVENT_BUBBLE);
        add_versa_switch_c(sb);

        lv_obj_t *tit = lv_label_create(sb);
        lv_label_set_text(tit, "OPERATORE");
        lv_obj_set_style_text_font(tit, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(tit, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(tit, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(tit, LV_ALIGN_TOP_LEFT, 0, 0);

        // Matricola — obj17
        lv_obj_t *matr = lv_label_create(sb);
        objects[idx].obj17 = matr;
        lv_label_set_text(matr, "0000");
        lv_obj_set_style_text_font(matr, &ui_font_my_font75, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(matr, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(matr, LV_ALIGN_TOP_LEFT, 0, 28);

        // Pill stato — obj18 (contenitore), obj19 (label)
        lv_obj_t *pill = lv_obj_create(sb);
        objects[idx].obj18 = pill;
        lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(pill, lv_color_hex(0xE11D48), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(pill, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(pill, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(pill, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(pill, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(pill, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(pill, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(pill, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(pill, LV_ALIGN_TOP_LEFT, 0, 130);
        lv_obj_add_flag(pill, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t *stato = lv_label_create(pill);
        objects[idx].obj19 = stato;
        lv_label_set_text(stato, "NON LOGGATO");
        lv_obj_set_style_text_font(stato, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(stato, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(stato, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(stato, LV_ALIGN_CENTER, 0, 0);

        // Separatore
        lv_obj_t *sep = lv_obj_create(sb);
        lv_obj_set_pos(sep, 0, 340);
        lv_obj_set_size(sep, 212, 1);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(sep, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(sep, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(sep, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t *banc_tit = lv_label_create(sb);
        lv_label_set_text(banc_tit, "BANCHETTO");
        lv_obj_set_style_text_font(banc_tit, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(banc_tit, lv_color_hex(0x555555), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(banc_tit, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(banc_tit, LV_ALIGN_TOP_LEFT, 0, 358);

        lv_obj_t *banc_val = lv_label_create(sb);
        lv_label_set_text(banc_val, "");
        lv_obj_set_style_text_font(banc_val, &ui_font_my_font75, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(banc_val, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(banc_val, LV_ALIGN_TOP_LEFT, 0, 382);
        // non serve obj* — il valore banchetto è fisso per tutti gli articoli,
        // viene scritto da update_page2(idx)
        objects[idx].obj13 = banc_val;
    }

    // ── BOX ARC HERO (276,16 480x260) ────────────────────────
    {
        lv_obj_t *box = lv_obj_create(parent_obj);
        lv_obj_set_pos(box, 276, 16);
        lv_obj_set_size(box, 480, 260);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x1E3A5F), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(box, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(box, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);

        // Indicatore articolo N/tot — obj14
        lv_obj_t *idx_lbl = lv_label_create(box);
        objects[idx].obj14 = idx_lbl;
        lv_obj_set_style_text_font(idx_lbl, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(idx_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(idx_lbl, 180, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(idx_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);

        lv_obj_t *arc_tit = lv_label_create(box);
        lv_label_set_text(arc_tit, "AVANZAMENTO FASE");
        lv_obj_set_style_text_font(arc_tit, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(arc_tit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(arc_tit, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(arc_tit, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(arc_tit, LV_ALIGN_TOP_LEFT, 0, 0);

        // Arc — obj5
        lv_obj_t *arc = lv_arc_create(box);
        objects[idx].obj5 = arc;
        lv_obj_set_size(arc, 180, 180);
        lv_arc_set_rotation(arc, 270);
        lv_arc_set_bg_angles(arc, 0, 360);
        lv_arc_set_range(arc, 0, 100);
        lv_arc_set_value(arc, 0);
        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x1E3A8A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_width(arc, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x3B82F6), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_width(arc, 14, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_align(arc, LV_ALIGN_CENTER, 0, 12);
        lv_obj_add_flag(arc, LV_OBJ_FLAG_EVENT_BUBBLE);

        // Valore prodotti fase — obj7
        lv_obj_t *arc_val = lv_label_create(box);
        objects[idx].obj7 = arc_val;
        lv_label_set_text(arc_val, "0");
        lv_obj_set_style_text_font(arc_val, &ui_font_my_font75, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(arc_val, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align_to(arc_val, arc, LV_ALIGN_CENTER, 0, -12);

        // "/ totale" — obj6
        lv_obj_t *arc_tot = lv_label_create(box);
        objects[idx].obj6 = arc_tot;
        lv_label_set_text(arc_tot, "/ 0");
        lv_obj_set_style_text_font(arc_tot, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(arc_tot, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(arc_tot, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align_to(arc_tot, arc, LV_ALIGN_CENTER, 0, 36);
    }

    // ── BOX PROD SESSIONE — giallo (772,16 236x125) ──────────
    {
        lv_obj_t *box = lv_obj_create(parent_obj);
        lv_obj_set_pos(box, 772, 16);
        lv_obj_set_size(box, 236, 125);
        lv_obj_set_style_bg_color(box, lv_color_hex(0xFFDD00), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(box, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(box, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t *t = lv_label_create(box);
        lv_label_set_text(t, "PROD SESSIONE");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000066), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(t, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        // obj9
        lv_obj_t *val = lv_label_create(box);
        objects[idx].obj9 = val;
        lv_label_set_text(val, "0");
        lv_obj_set_style_text_font(val, &ui_font_my_font75, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(val, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(val, LV_ALIGN_TOP_LEFT, 0, 26);
    }

    // ── BOX SCATOLA — bianco (772,157 236x119) ───────────────
    {
        lv_obj_t *box = lv_obj_create(parent_obj);
        lv_obj_set_pos(box, 772, 157);
        lv_obj_set_size(box, 236, 119);
        lv_obj_set_style_bg_color(box, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(box, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(box, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t *t = lv_label_create(box);
        lv_label_set_text(t, "SCATOLA");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000066), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(t, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        // obj12
        lv_obj_t *val = lv_label_create(box);
        objects[idx].obj12 = val;
        lv_label_set_text(val, "0/0");
        lv_obj_set_style_text_font(val, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(val, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(val, LV_ALIGN_TOP_LEFT, 0, 28);

        lv_obj_t *sub = lv_label_create(box);
        lv_label_set_text(sub, "PEZZI / CAPACITA'");
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }

    // ── BOX MATRICOLA SCATOLA — bianco (276,292 732x116) ─────
    {
        lv_obj_t *box = lv_obj_create(parent_obj);
        // obj11 — usato per visual_feedback_ok
        objects[idx].obj11 = box;
        lv_obj_set_pos(box, 276, 292);
        lv_obj_set_size(box, 732, 116);
        lv_obj_set_style_bg_color(box, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(box, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(box, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(box, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(box, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);

        lv_obj_t *t = lv_label_create(box);
        lv_label_set_text(t, "MATRICOLA SCATOLA CORRENTE");
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(t, lv_color_hex(0x00000044), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(t, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        // obj16
        lv_obj_t *val = lv_label_create(box);
        objects[idx].obj16 = val;
        lv_label_set_text(val, "---");
        lv_obj_set_style_text_font(val, &ui_font_my_font75, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(val, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_width(val, 700);
        lv_label_set_long_mode(val, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(val, LV_ALIGN_TOP_LEFT, 0, 28);
    }

    // ── BOTTONE SCARTI — rosso (276,424 732x110) ─────────────
    {
        lv_obj_t *btn = lv_btn_create(parent_obj);
        objects[idx].obj0 = btn;
        lv_obj_set_pos(btn, 276, 424);
        lv_obj_set_size(btn, 732, 110);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xB91C1C), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x991B1B), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(btn, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *ico = lv_label_create(btn);
        lv_label_set_text(ico, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_font(ico, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ico, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 40, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, "REGISTRA SCARTI");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(lbl, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t *arrow = lv_label_create(btn);
        lv_label_set_text(arrow, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(arrow, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -40, 0);
    }

    // EVENT_BUBBLE su tutti i figli diretti
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); i++)
        lv_obj_add_flag(lv_obj_get_child(obj, i), LV_OBJ_FLAG_EVENT_BUBBLE);
}

void tick_screen_main(void) {}

void create_screens(void)
{
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(
        dispp,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        true,
        LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    uint8_t count = banchetto_manager_get_count();
    if (count == 0) count = 1; // fallback sicuro

    for (uint8_t i = 0; i < count; i++) {
        create_screen_main(i);
        ESP_LOGI("SCREENS", "Schermata page2[%d] creata", i);
    }
}