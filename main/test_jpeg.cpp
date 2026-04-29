I (24176) file_iterator: File : For Elise.mp3
I (24182) file_iterator: File : Something.mp3
I (24188) file_iterator: File : TETRIS.mp3
I (24229) file_iterator: File : Waka Waka.mp3
I (24268) ESP32_P4_EV: Setting LCD backlight: 100%
I (24268) main_task: Returned from app_main()
I (24340) SCANNER: SCANNER CONNESSO! (Handle: 0x4ff31e84)
I (24340) SCANNER: Scanner Pronto e in ascolto.
Guru Meditation Error: Core  1 panic'ed (Load access fault). Exception was unhandled.

Core  1 register dump:
MEPC    : 0x48067640  RA      : 0x48067640  SP      : 0x4ff60690  GP      : 0x4ff1d900
--- 0x48067640: get_prop_core at C:/Users/npuppo/copia/managed_components/lvgl__lvgl/src/core/lv_obj_style.c:608
--- (inlined by) lv_obj_get_style_prop at C:/Users/npuppo/copia/managed_components/lvgl__lvgl/src/core/lv_obj_style.c:229
--- 0x48067640: get_prop_core at C:/Users/npuppo/copia/managed_components/lvgl__lvgl/src/core/lv_obj_style.c:608
--- (inlined by) lv_obj_get_style_prop at C:/Users/npuppo/copia/managed_components/lvgl__lvgl/src/core/lv_obj_style.c:229
TP      : 0x4ff60800  T0      : 0x4ff6069c  T1      : 0x4ff006de  T2      : 0x00000001
--- 0x4ff006de: heap_caps_alloc_failed at C:/Espressif/frameworks/esp-idf-v5.5.2/components/heap/heap_caps.c:51
S0/FP   : 0x48419154  S1      : 0x00ffffff  A0      : 0x00000000  A1      : 0x00000000
A2      : 0x0000000a  A3      : 0x48418f88  A4      : 0x4840b3d8  A5      : 0x00000000
A6      : 0x00000110  A7      : 0x0000000c  S2      : 0x0000000a  S3      : 0x00000000