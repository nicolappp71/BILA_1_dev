#pragma once

#include "lvgl.h"
#include <stdbool.h>

void tastiera_scarti_open(void);
void popup_avviso_open(const char *titolo, const char *msg, bool offline);
void popup_ok_open(const char *titolo, const char *msg, const char *footer, void (*on_dismiss)(void));
void popup_controllo_open(void);
void popup_controllo_close(void);
void popup_formazione_open(const char *titolo, const char *msg);
void popup_formazione_close(void);