#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t cp;     /* Unicode codepoint */
    uint16_t off;    /* offset into the bitmap; rows are padded to whole bytes, MSB first */
    uint8_t w, h;    /* ink box */
    int8_t xoff;     /* ink box left edge relative to the pen */
    int8_t yoff;     /* ink box top edge relative to the baseline (negative = above) */
    uint8_t adv;     /* pen advance */
} gfx_glyph_t;

typedef struct {
    const gfx_glyph_t *glyphs;  /* sorted by cp */
    size_t count;
    const uint8_t *bitmap;
    uint8_t ascent;
    uint8_t line_h;
} gfx_font_t;

/* Finds a glyph, falling back to '?' and then NULL. */
const gfx_glyph_t *gfx_font_glyph(const gfx_font_t *f, uint32_t cp);

/* Pen advance of one codepoint (the space width for codepoints the font lacks). */
int gfx_font_advance(const gfx_font_t *f, uint32_t cp);

/* Width in pixels of `len` bytes of UTF-8 text. */
int gfx_text_width(const gfx_font_t *f, const char *s, size_t len);

/* Draws UTF-8 text with the baseline at y_baseline. Returns the x after the last glyph. */
int gfx_draw_text(const gfx_font_t *f, int x, int y_baseline, const char *s, size_t len, bool ink);
