#include "st7305_pack.h"

#define H4 (ST7305_HEIGHT / 4)

static inline int fb_ink(const uint8_t *fb, int x, int y)
{
    return (fb[y * ST7305_FB_STRIDE + (x >> 3)] >> (7 - (x & 7))) & 1;
}

void st7305_pack(const uint8_t *fb, uint8_t *panel)
{
    for (int byte_x = 0; byte_x < ST7305_WIDTH / 2; byte_x++) {
        for (int block_y = 0; block_y < H4; block_y++) {
            uint8_t b = 0xFF;
            for (int ly = 0; ly < 4; ly++) {
                const int y = ST7305_HEIGHT - 1 - (block_y * 4 + ly);
                for (int lx = 0; lx < 2; lx++) {
                    const int x = byte_x * 2 + lx;
                    if (fb_ink(fb, x, y)) {
                        b &= (uint8_t)~(1u << (7 - ((ly << 1) | lx)));
                    }
                }
            }
            panel[byte_x * H4 + block_y] = b;
        }
    }
}
