/* Host test: st7305_pack() against an independent implementation of the Waveshare reference
 * lookup-table method (landscape). Build and run with `make -C tests/host`. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "st7305_pack.h"

#define W ST7305_WIDTH
#define H ST7305_HEIGHT

/* Reference: per-pixel (byte index, bit mask) tables, as in InitLandscapeLUT(). */
static void reference_pack(const uint8_t *fb, uint8_t *out)
{
    const int H4 = H >> 2;
    memset(out, 0xFF, ST7305_PANEL_BUF_SIZE);
    for (int y = 0; y < H; y++) {
        const int inv_y = H - 1 - y;
        const int block_y = inv_y >> 2;
        const int local_y = inv_y & 3;
        for (int x = 0; x < W; x++) {
            const int idx = (x >> 1) * H4 + block_y;
            const int bit = 7 - ((local_y << 1) | (x & 1));
            if ((fb[y * ST7305_FB_STRIDE + (x >> 3)] >> (7 - (x & 7))) & 1) {
                out[idx] &= (uint8_t)~(1u << bit);
            }
        }
    }
}

int main(void)
{
    static uint8_t fb[ST7305_FB_SIZE];
    static uint8_t got[ST7305_PANEL_BUF_SIZE], want[ST7305_PANEL_BUF_SIZE];
    int failures = 0;

    /* all white, all black, random */
    for (int round = 0; round < 3; round++) {
        if (round == 0) memset(fb, 0x00, sizeof(fb));
        if (round == 1) memset(fb, 0xFF, sizeof(fb));
        if (round == 2) { srand(1); for (size_t i = 0; i < sizeof(fb); i++) fb[i] = (uint8_t)rand(); }
        st7305_pack(fb, got);
        reference_pack(fb, want);
        if (memcmp(got, want, sizeof(got)) != 0) {
            printf("FAIL: round %d differs from reference\n", round);
            failures++;
        }
    }

    /* single pixels: every pixel must land on exactly one cleared bit */
    for (int y = 0; y < H; y += 7) {
        for (int x = 0; x < W; x += 5) {
            memset(fb, 0, sizeof(fb));
            fb[y * ST7305_FB_STRIDE + (x >> 3)] |= (uint8_t)(0x80u >> (x & 7));
            st7305_pack(fb, got);
            int cleared = 0;
            for (size_t i = 0; i < sizeof(got); i++) {
                for (int b = 0; b < 8; b++) cleared += !((got[i] >> b) & 1);
            }
            if (cleared != 1) {
                printf("FAIL: pixel (%d,%d) cleared %d bits\n", x, y, cleared);
                failures++;
            }
        }
    }

    printf(failures ? "test_pack: %d failure(s)\n" : "test_pack: ok\n", failures);
    return failures != 0;
}
