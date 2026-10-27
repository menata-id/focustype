#include "gfx.h"
#include "gfx_text.h"
#include "utf8.h"

static const gfx_glyph_t *find(const gfx_font_t *f, uint32_t cp)
{
    size_t lo = 0, hi = f->count;
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (f->glyphs[mid].cp == cp) {
            return &f->glyphs[mid];
        }
        if (f->glyphs[mid].cp < cp) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return NULL;
}

const gfx_glyph_t *gfx_font_glyph(const gfx_font_t *f, uint32_t cp)
{
    const gfx_glyph_t *g = find(f, cp);
    return g ? g : find(f, '?');
}

int gfx_font_advance(const gfx_font_t *f, uint32_t cp)
{
    const gfx_glyph_t *g = find(f, cp);
    if (!g) {
        g = find(f, ' ');
    }
    return g ? g->adv : 0;
}

int gfx_text_width(const gfx_font_t *f, const char *s, size_t len)
{
    int w = 0;
    while (len > 0) {
        uint32_t cp;
        const size_t n = utf8_next(s, len, &cp);
        w += gfx_font_advance(f, cp);
        s += n;
        len -= n;
    }
    return w;
}

int gfx_draw_text(const gfx_font_t *f, int x, int y_baseline, const char *s, size_t len, bool ink)
{
    while (len > 0) {
        uint32_t cp;
        const size_t n = utf8_next(s, len, &cp);
        const gfx_glyph_t *g = find(f, cp);
        if (g && g->w > 0) {
            gfx_blit_1bpp(x + g->xoff, y_baseline + g->yoff, g->w, g->h, &f->bitmap[g->off],
                          (g->w + 7) / 8, ink);
        }
        x += gfx_font_advance(f, cp);
        s += n;
        len -= n;
    }
    return x;
}
