/*
 * Bring-up firmware: draws an orientation test pattern, then (optionally) shows keyboard
 * activity as a progress bar along the bottom edge.
 */

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "app.h"
#include "board.h"
#include "gfx.h"
#include "gfx_fonts.h"
#include "gfx_text.h"
#include "st7305.h"
#include "storage.h"
#include "usb_kbd.h"

static const char *TAG = "focustype";

/* Squares of different sizes in three corners make mirroring and rotation obvious:
 * 24 px top-left, 12 px top-right, 6 px bottom-left, nothing bottom-right. */
static void draw_test_pattern(void)
{
    gfx_clear(false);
    gfx_rect(0, 0, GFX_W, GFX_H, 2, true);
    gfx_line(0, 0, GFX_W - 1, GFX_H - 1, true);
    gfx_line(GFX_W - 1, 0, 0, GFX_H - 1, true);

    gfx_fill_rect(6, 6, 24, 24, true);
    gfx_fill_rect(GFX_W - 6 - 12, 6, 12, 12, true);
    gfx_fill_rect(6, GFX_H - 6 - 6, 6, 6, true);

    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            if (((x >> 3) + (y >> 3)) & 1) {
                gfx_pixel(GFX_W / 2 - 32 + x, GFX_H / 2 - 32 + y, true);
            }
        }
    }

    /* Text, to check the fonts and the baseline placement. */
    const char *title = "FocusType";
    const int tw = gfx_text_width(&font_plex_sans_19_bold, title, strlen(title));
    gfx_draw_text(&font_plex_sans_19_bold, (GFX_W - tw) / 2, 70, title, strlen(title), true);
    const char *body = "The quick brown fox jumps over the lazy dog.";
    const int bw = gfx_text_width(&font_plex_mono_17, body, strlen(body));
    gfx_draw_text(&font_plex_mono_17, (GFX_W - bw) / 2, 230, body, strlen(body), true);
}

#if CONFIG_FT_ENABLE_USB_KBD
static void keyboard_demo(void)
{
    int bar = 0;
    for (;;) {
        kbd_event_t ev;
        if (!usb_kbd_next(&ev, portMAX_DELAY)) {
            continue;
        }
        switch (ev.key) {
        case KBD_KEY_CONNECTED:
            ESP_LOGI(TAG, "keyboard connected");
            break;
        case KBD_KEY_DISCONNECTED:
            ESP_LOGW(TAG, "keyboard disconnected");
            break;
        case KBD_KEY_CHAR:
            ESP_LOGI(TAG, "char '%c' mods=0x%02x", ev.ch, ev.mods);
            break;
        default:
            ESP_LOGI(TAG, "key %d mods=0x%02x", (int)ev.key, ev.mods);
            break;
        }
        if (ev.key == KBD_KEY_BACKSPACE && bar > 0) {
            bar -= 4;
            gfx_fill_rect(8 + bar, GFX_H - 14, 4, 6, false);
        } else if (ev.key != KBD_KEY_CONNECTED && ev.key != KBD_KEY_DISCONNECTED &&
                   8 + bar + 4 < GFX_W - 8) {
            gfx_fill_rect(8 + bar, GFX_H - 14, 4, 6, true);
            bar += 4;
        }
        gfx_flush();
    }
}
#endif

void app_main(void)
{
#if CONFIG_FT_APP
    app_run();
    return;
#endif
    ESP_ERROR_CHECK(gfx_init());

    const st7305_config_t lcd = {
        .host = FT_LCD_SPI_HOST,
        .sclk = FT_LCD_PIN_CLK,
        .mosi = FT_LCD_PIN_MOSI,
        .cs = FT_LCD_PIN_CS,
        .dc = FT_LCD_PIN_DC,
        .rst = FT_LCD_PIN_RST,
        .clock_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(st7305_init(&lcd));

    draw_test_pattern();
    ESP_ERROR_CHECK(gfx_flush());
    ESP_LOGI(TAG, "test pattern drawn");

#if CONFIG_FT_ENABLE_SD
    storage_mount();
#endif

#if CONFIG_FT_ENABLE_USB_KBD
    ESP_ERROR_CHECK(usb_kbd_start());
    keyboard_demo();
#else
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
#endif
}
