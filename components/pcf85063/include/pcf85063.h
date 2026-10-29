#pragma once

#include <time.h>

#include "esp_err.h"

/*
 * PCF85063A real-time clock on the I2C bus (board.h: FT_PIN_I2C_SDA/SCL, address 0x51).
 *
 * pcf_init() reads the chip and, when its time is trustworthy, sets the system clock from it.
 * Returns ESP_ERR_INVALID_STATE when the chip answered but lost its time (first boot, empty
 * coin cell), or another error when it did not answer; in both cases the system clock is left
 * alone and the caller should ask the user to set the time.
 */
esp_err_t pcf_init(void);

/* Writes `t` (wall-clock time, no time zone) to the chip and to the system clock. */
esp_err_t pcf_set(time_t t);
