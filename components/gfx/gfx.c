#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"

#include "gfx.h"
#include "st7305.h"

static const char *TAG = "gfx";
static uint8_t *s_fb;

esp_err_t gfx_init(void)
{
    if (!s_fb) {
        s_fb = heap_caps_calloc(1, ST7305_FB_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    ESP_RETURN_ON_FALSE(s_fb, ESP_ERR_NO_MEM, TAG, "framebuffer");
    return ESP_OK;
}

uint8_t *gfx_buffer(void)
{
    return s_fb;
}

void gfx_clear(bool ink)
{
    memset(s_fb, ink ? 0xFF : 0x00, ST7305_FB_SIZE);
}

void gfx_pixel(int x, int y, bool ink)
{
    if (x < 0 || x >= GFX_W || y < 0 || y >= GFX_H) {
        return;
    }
    uint8_t *p = &s_fb[y * ST7305_FB_STRIDE + (x >> 3)];
    const uint8_t m = (uint8_t)(0x80u >> (x & 7));
    if (ink) {
        *p |= m;
    } else {
        *p &= (uint8_t)~m;
    }
}

void gfx_hline(int x, int y, int w, bool ink)
{
    for (int i = 0; i < w; i++) {
        gfx_pixel(x + i, y, ink);
    }
}

void gfx_vline(int x, int y, int h, bool ink)
{
    for (int i = 0; i < h; i++) {
        gfx_pixel(x, y + i, ink);
    }
}

void gfx_fill_rect(int x, int y, int w, int h, bool ink)
{
    for (int j = 0; j < h; j++) {
        gfx_hline(x, y + j, w, ink);
    }
}

void gfx_rect(int x, int y, int w, int h, int t, bool ink)
{
    gfx_fill_rect(x, y, w, t, ink);
    gfx_fill_rect(x, y + h - t, w, t, ink);
    gfx_fill_rect(x, y, t, h, ink);
    gfx_fill_rect(x + w - t, y, t, h, ink);
}

void gfx_line(int x0, int y0, int x1, int y1, bool ink)
{
    const int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gfx_pixel(x0, y0, ink);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_blit_1bpp(int x, int y, int w, int h, const uint8_t *bits, int stride, bool ink)
{
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if ((bits[j * stride + (i >> 3)] >> (7 - (i & 7))) & 1) {
                gfx_pixel(x + i, y + j, ink);
            }
        }
    }
}

esp_err_t gfx_flush(void)
{
    return st7305_flush(s_fb);
}
