#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "st7305_pack.h"

#define GFX_W ST7305_WIDTH
#define GFX_H ST7305_HEIGHT

/* Single 1-bit framebuffer, row-major, MSB first, 1 = ink (black). Not thread-safe. */
esp_err_t gfx_init(void);
uint8_t *gfx_buffer(void);

void gfx_clear(bool ink);
void gfx_pixel(int x, int y, bool ink);
void gfx_hline(int x, int y, int w, bool ink);
void gfx_vline(int x, int y, int h, bool ink);
void gfx_fill_rect(int x, int y, int w, int h, bool ink);
void gfx_rect(int x, int y, int w, int h, int thickness, bool ink);
void gfx_line(int x0, int y0, int x1, int y1, bool ink);

/* Draws a w x h bitmap where a set bit is ink (rows padded to `stride` bytes, MSB first).
 * Glyphs from a generated font go through this. */
void gfx_blit_1bpp(int x, int y, int w, int h, const uint8_t *bits, int stride, bool ink);

/* Sends the framebuffer to the panel. */
esp_err_t gfx_flush(void);
