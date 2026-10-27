#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "kbd_hid.h"

/* Starts the USB host stack and the HID class driver, and begins collecting key events from a
 * boot-protocol keyboard. Takes over the native USB PHY, so the USB Serial/JTAG console stops
 * working (use the UART0 console, see sdkconfig.usbhost). */
esp_err_t usb_kbd_start(void);

/* Waits up to `timeout` for the next key event. Includes KBD_KEY_CONNECTED/DISCONNECTED. */
bool usb_kbd_next(kbd_event_t *ev, TickType_t timeout);
