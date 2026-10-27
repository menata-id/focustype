#pragma once

#include <stdint.h>

/*
 * Pixel packing for the Waveshare 400x300 ST7305 panel (landscape).
 *
 * Logical framebuffer: row-major, MSB first, 1 bit per pixel, 1 = ink (black).
 * Panel buffer: 15000 bytes. Each byte holds a 2-column x 4-row block:
 *     bit7 (row0,col0) bit6 (row0,col1) bit5 (row1,col0) bit4 (row1,col1)
 *     bit3 (row2,col0) bit2 (row2,col1) bit1 (row3,col0) bit0 (row3,col1)
 * Bytes are ordered by block column, then by block row, with the vertical axis flipped
 * (block row 0 is the bottom of the screen). A cleared bit is a black pixel.
 *
 * Layout follows the Waveshare reference driver (InitLandscapeLUT), as ported in
 * kylehase/ESPHome-ST7305-RLCD (Apache-2.0).
 */

#define ST7305_WIDTH            400
#define ST7305_HEIGHT           300
#define ST7305_FB_STRIDE        (ST7305_WIDTH / 8)
#define ST7305_FB_SIZE          (ST7305_FB_STRIDE * ST7305_HEIGHT)
#define ST7305_PANEL_BUF_SIZE   ((ST7305_WIDTH * ST7305_HEIGHT) / 8)

void st7305_pack(const uint8_t *fb, uint8_t *panel);
