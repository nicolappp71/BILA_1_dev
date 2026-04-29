#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    void ble_manager_init(void);
    void ble_manager_pause_scan(void);
    void ble_manager_resume_scan(void);
    const char *banchetto_manager_get_banchetto_id(void);

#ifdef __cplusplus
}
#endif