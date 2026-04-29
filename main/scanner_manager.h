#ifndef SCANNER_MANAGER_H
#define SCANNER_MANAGER_H
#include "esp_private/usb_phy.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
void hid_host_interface_callback(hid_host_device_handle_t device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg);
char hid_code_to_ascii(uint8_t code);
void hid_host_device_callback(hid_host_device_handle_t device_handle,
                              const hid_host_driver_event_t event,
                              void *arg);
void init_usb_driver(void);

#endif //
