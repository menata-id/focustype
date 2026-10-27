#pragma once

/*
 * Pin map for the Waveshare ESP32-S3-RLCD-4.2.
 *
 * Source: ESPHome device page (devices.esphome.io/devices/waveshare-esp32-s3-rlcd-42).
 * Pins marked UNKNOWN are not published there and must be taken from the Waveshare schematic.
 */

/* ST7305 reflective LCD, 4-line SPI (A0 = DC) */
#define FT_LCD_SPI_HOST   SPI2_HOST
#define FT_LCD_PIN_CLK    11
#define FT_LCD_PIN_MOSI   12
#define FT_LCD_PIN_CS     40
#define FT_LCD_PIN_DC     5
#define FT_LCD_PIN_RST    41

/* Buttons (active low). PWR is UNKNOWN: it may be wired to a power latch instead of a GPIO. */
#define FT_PIN_BOOT       0
#define FT_PIN_KEY        18

/* Battery voltage divider -> ADC1_CH3 */
#define FT_PIN_BAT_ADC    4

/* I2C bus (PCF85063 RTC, SHTC3, ES8311/ES7210 codecs) */
#define FT_PIN_I2C_SDA    13
#define FT_PIN_I2C_SCL    14

/* microSD: UNKNOWN. -1 makes storage_mount() return ESP_ERR_NOT_SUPPORTED. */
#define FT_SD_PIN_CLK     (-1)
#define FT_SD_PIN_CMD     (-1)
#define FT_SD_PIN_D0      (-1)

/* Native USB (GPIO19 = D-, GPIO20 = D+) is used as host PHY for the keyboard. */
