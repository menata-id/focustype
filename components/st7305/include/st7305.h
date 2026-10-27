#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "st7305_pack.h"

typedef struct {
    spi_host_device_t host;
    gpio_num_t sclk;
    gpio_num_t mosi;
    gpio_num_t cs;
    gpio_num_t dc;
    gpio_num_t rst;
    int clock_hz;       /* 10 MHz is a safe bring-up value */
} st7305_config_t;

/* Resets the controller, runs the init sequence and turns the display on. */
esp_err_t st7305_init(const st7305_config_t *cfg);

/* Packs the logical framebuffer (ST7305_FB_SIZE bytes, 1 = ink) and writes the whole frame.
 * Not thread-safe: call from one task. */
esp_err_t st7305_flush(const uint8_t *fb);

/* Sleep in / out. The panel keeps showing the last image while the MCU sleeps. */
esp_err_t st7305_sleep(void);
esp_err_t st7305_wake(void);

/* Low power mode drops the panel to a ~1 Hz refresh; high power mode (~32 Hz) suits typing.
 * Only the mode command is sent; the datasheet's voltage re-setting steps are not applied yet. */
esp_err_t st7305_set_low_power(bool low);
