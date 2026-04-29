idf_component_register(
    SRCS "collaudo_manager.c" "main.cpp"
    "bilancia_manager.c"
    "tagliatubi_manager.c"
    "wifi_manager.c"
    "web_server.c"
    "settings_manager.c"
    "banchetto_manager.c"
    "json_parser.c"
    "key_manager.c"
    "rfid_manager.c"
    "gpio_manager.c"
    "i2c_manager.c"
    "http_client.c"
    "scanner_manager.c"
    "battery_manager.c"
    "log_manager.c"
    "ble_manager.c"
    "ota_manager.c"
    "offline_queue.c"
    "time_manager.c"
    "badge_cache.c"
    "offline_journal.c"
    "ff_memalloc_dma.c"
    INCLUDE_DIRS "."
    REQUIRES 
        espressif__esp_hosted 
        esp_wifi 
        nvs_flash 
        esp_event 
        esp_netif
        waveshare__esp32_p4_wifi6_touch_lcd_7b
        esp_lcd_ek79007
        bsp_extra
        apps
        esp_http_server
        esp_http_client
        json
        esp_hw_support        # <--- Fondamentale per esp_private/usb_phy.h
        usb
        lwip
        esp_phy 
        log 
        freertos 
        driver 
        usb_host_hid
        driver esp_adc
        bt
        app_update    # <--- AGGIUNGI QUESTO (è quello che contiene esp_ota_ops.h)
        esp_https_ota 
        esp_http_client
        

)

# Questa riga dice al compilatore di ignorare i warning sui campi mancanti in main.cpp
set_source_files_properties("main.cpp" PROPERTIES COMPILE_FLAGS "-Wno-missing-field-initializers")
idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE 
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)

if(CONFIG_ESP_BROOKESIA_MEMORY_USE_CUSTOM)
    idf_build_get_property(build_components BUILD_COMPONENTS)
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "esp-brookesia" OR COMPONENT MATCHES "espressif__esp-brookesia")
            set(TARGET_COMPONENT ${COMPONENT})
            break()
        endif()
    endforeach()

    if(TARGET_COMPONENT STREQUAL "")
        message(FATAL_ERROR "Component 'esp-brookesia' not found.")
    else()
        idf_component_get_property(ESP_BROOKESIA_LIB ${TARGET_COMPONENT} COMPONENT_LIB)
    endif()

    target_compile_options(
        ${ESP_BROOKESIA_LIB}
        PUBLIC
            "-DESP_BROOKESIA_MEMORY_CUSTOM_MALLOC(x)=heap_caps_aligned_alloc(1, x, MALLOC_CAP_SPIRAM)"  "-DESP_BROOKESIA_MEMORY_CUSTOM_FREE=free"
    )
endif()

if(CONFIG_LV_MEM_CUSTOM)
    target_compile_options(
        ${LVGL_LIB} 
        PRIVATE 
            "-DLV_MEM_CUSTOM_ALLOC(x)=heap_caps_malloc(x, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)"
    )
endif()

idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=ff_memalloc" APPEND)
idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=ff_memfree" APPEND)
idf_build_set_property(LINK_OPTIONS "-u__wrap_ff_memalloc" APPEND)
idf_build_set_property(LINK_OPTIONS "-u__wrap_ff_memfree" APPEND)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)