#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "gfx_fonts.h"
#include "gfx_text.h"
#include "textfind.h"
#include "ui.h"
#include "utf8.h"

#define HDR_H     26   /* includes the 2 px rule */
#define FTR_Y     278  /* top of the footer, where its 1 px rule is */
#define FTR_H     (GFX_H - FTR_Y)
#define BODY_Y    HDR_H
#define MARGIN_X  12

/* ---- text helpers ---------------------------------------------------------------------- */

/* Baseline for text vertically centred in a box with CSS-like line height `lh`. */
static int baseline(const gfx_font_t *f, int y_top, int lh)
{
    return y_top + (lh - f->line_h) / 2 + f->ascent;
}

static int text_w(const gfx_font_t *f, const char *s)
{
    return gfx_text_width(f, s, strlen(s));
}

static int draw(const gfx_font_t *f, int x, int y_top, int lh, const char *s, bool ink)
{
    return gfx_draw_text(f, x, baseline(f, y_top, lh), s, strlen(s), ink) - x;
}

static void draw_right(const gfx_font_t *f, int x_right, int y_top, int lh, const char *s, bool ink)
{
    draw(f, x_right - text_w(f, s), y_top, lh, s, ink);
}

/* Copies s into out, cutting it (with an ellipsis) so it fits max_w pixels. */
static void fit(const gfx_font_t *f, const char *s, int max_w, char *out, size_t cap)
{
    if (text_w(f, s) <= max_w) {
        snprintf(out, cap, "%s", s);
        return;
    }
    const int ell_w = gfx_font_advance(f, 0x2026);
    size_t len = strlen(s), used = 0;
    int w = 0;
    while (used < len) {
        uint32_t cp;
        const size_t n = utf8_next(s + used, len - used, &cp);
        const int a = gfx_font_advance(f, cp);
        if (w + a + ell_w > max_w || used + n + 4 > cap) {
            break;
        }
        w += a;
        used += n;
    }
    memcpy(out, s, used);
    char ell[4];
    const size_t en = utf8_encode(0x2026, ell);
    memcpy(out + used, ell, en);
    out[used + en] = '\0';
}

/* Greedy word wrap. Draws when `do_draw`; returns the line count either way. */
static int paragraph(const gfx_font_t *f, int x, int y, int max_w, int lh, const char *s,
                     bool ink, bool do_draw)
{
    const int sp = gfx_font_advance(f, ' ');
    int lines = 0, line_w = 0;
    const char *line = s, *line_end = s, *p = s;
    while (*p == ' ') {
        p++;
    }
    line = line_end = p;
    while (*p) {
        const char *w = p;
        while (*p && *p != ' ') {
            p++;
        }
        const int ww = gfx_text_width(f, w, (size_t)(p - w));
        if (line_end != line && line_w + sp + ww > max_w) {
            if (do_draw) {
                gfx_draw_text(f, x, baseline(f, y + lines * lh, lh), line,
                              (size_t)(line_end - line), ink);
            }
            lines++;
            line = w;
            line_w = ww;
        } else {
            line_w = (line_end == line) ? ww : line_w + sp + ww;
        }
        line_end = p;
        while (*p == ' ') {
            p++;
        }
    }
    if (line_end != line) {
        if (do_draw) {
            gfx_draw_text(f, x, baseline(f, y + lines * lh, lh), line, (size_t)(line_end - line), ink);
        }
        lines++;
    }
    return lines;
}

/* ---- shapes ---------------------------------------------------------------------------- */

static void fill_circle(int cx, int cy, int r, bool ink)
{
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                gfx_pixel(cx + dx, cy + dy, ink);
            }
        }
    }
}

/* Right-pointing triangle ("▸"), h tall and about h/2 wide. */
static void triangle_right(int x, int y, int h, bool ink)
{
    for (int c = 0; c < (h + 1) / 2; c++) {
        gfx_vline(x + c, y + c, h - 2 * c, ink);
    }
}

/* 2 px dashes every 4 px; the 1 px dashed border of the mockup's hint boxes. */
static void dashed_hline(int x, int y, int w, bool ink)
{
    for (int i = 0; i < w; i += 4) {
        gfx_hline(x + i, y, w - i < 2 ? w - i : 2, ink);
    }
}

static void dashed_vline(int x, int y, int h, bool ink)
{
    for (int i = 0; i < h; i += 4) {
        gfx_vline(x, y + i, h - i < 2 ? h - i : 2, ink);
    }
}

static void dashed_rect(int x, int y, int w, int h, bool ink)
{
    dashed_hline(x, y, w, ink);
    dashed_hline(x, y + h - 1, w, ink);
    dashed_vline(x, y, h, ink);
    dashed_vline(x + w - 1, y, h, ink);
}

/* A small pennant on a pole, 9 x 11 px, top-left at (x, y). Replaces the unavailable flag glyph. */
static void flag_icon(int x, int y, bool ink)
{
    gfx_vline(x, y, 11, ink);
    for (int r = 0; r < 6; r++) {
        gfx_hline(x + 1, y + r, 8 - r, ink);
    }
}

static void cross(int x, int y, int s, bool ink)
{
    for (int i = 0; i < s; i++) {
        gfx_pixel(x + i, y + i, ink);
        gfx_pixel(x + i + 1, y + i, ink);
        gfx_pixel(x + s - 1 - i, y + i, ink);
        gfx_pixel(x + s - i, y + i, ink);
    }
}

/* A key cap: 1 px outline, 2 px corner radius, 12 px semibold text. Returns its width. */
static int keycap_w(const char *s)
{
    return text_w(&font_plex_sans_12_semibold, s) + 6 + 2;
}

static int keycap(int x, int y_center, const char *s, bool ink)
{
    const int w = keycap_w(s), h = 16;
    const int y = y_center - h / 2;
    gfx_rect(x, y, w, h, 1, ink);
    gfx_pixel(x, y, !ink);
    gfx_pixel(x + w - 1, y, !ink);
    gfx_pixel(x, y + h - 1, !ink);
    gfx_pixel(x + w - 1, y + h - 1, !ink);
    draw(&font_plex_sans_12_semibold, x + 4, y, h, s, ink);
    return w;
}

/* ---- header and footer ----------------------------------------------------------------- */

static void header_frame(void)
{
    gfx_fill_rect(0, HDR_H - 2, GFX_W, 2, true);
}

static void header(const gfx_font_t *lf, const char *left, const gfx_font_t *rf, const char *right)
{
    header_frame();
    char buf[64];
    const int right_w = right ? text_w(rf, right) : 0;
    fit(lf, left, GFX_W - 2 * MARGIN_X - right_w - 8, buf, sizeof(buf));
    draw(lf, MARGIN_X, 0, HDR_H - 2, buf, true);
    if (right) {
        draw_right(rf, GFX_W - MARGIN_X, 0, HDR_H - 2, right, true);
    }
}

typedef struct {
    const char *key;
    const char *label;
} hint_t;

static void footer_clear(void)
{
    gfx_fill_rect(0, FTR_Y, GFX_W, FTR_H, false);
    gfx_hline(0, FTR_Y, GFX_W, true);
}

static void footer_at(int x0, int y, int w, int h, const hint_t *hints, size_t n, int pad)
{
    gfx_fill_rect(x0, y, w, h, false);
    gfx_hline(x0, y, w, true);
    const int mid = y + 1 + (h - 1) / 2;
    int x = x0 + pad;
    for (size_t i = 0; i < n; i++) {
        x += keycap(x, mid, hints[i].key, true) + 4;
        x += draw(&font_plex_sans_12, x, mid - 7, 14, hints[i].label, true) + 14;
    }
}

static void footer(const hint_t *hints, size_t n)
{
    footer_at(0, FTR_Y, GFX_W, FTR_H, hints, n, MARGIN_X);
}

/* ---- Home ------------------------------------------------------------------------------ */

#define CARD_H   57
#define ROW_H    39
#define ITEM_GAP 5

static void home_item(int y, int h, bool selected, const gfx_font_t *lf, int lh, const char *label,
                      const char *sub, const char *count, const char *key)
{
    const int x = MARGIN_X, w = GFX_W - 2 * MARGIN_X;
    const bool ink = !selected;  /* text colour: paper on a selected (black) item */
    if (selected) {
        gfx_fill_rect(x, y, w, h, true);
    } else {
        gfx_rect(x, y, w, h, 2, true);
    }

    const int tx = x + 2 + 10;
    const int ty = y + 2 + 7;
    int lx = tx;
    if (selected) {
        triangle_right(tx, ty + (lh - 10) / 2, 10, false);
        lx += 12;
    }
    draw(lf, lx, ty, lh, label, ink);
    if (sub) {
        draw(&font_plex_sans_12, tx, ty + lh + 1, 15, sub, ink);
    }

    int xr = x + w - 2 - 10;
    if (key) {
        const int kw = keycap_w(key);
        keycap(xr - kw, y + h / 2, key, ink);
        xr -= kw + 8;
    }
    if (count) {
        draw_right(&font_plex_sans_12, xr, y + h / 2 - 7, 14, count, ink);
    }
}

void ui_home(int sel, bool has_continue, const char *cont_title, const char *cont_sub,
             uint32_t note_count, const char *date, const char *last_wrote)
{
    gfx_clear(false);
    header(&font_plex_sans_13_bold, "FocusType", &font_plex_sans_13, date);

    const int items = has_continue ? 4 : 3;
    const int total = (has_continue ? CARD_H : 0) + 3 * ROW_H + (items - 1) * ITEM_GAP + 3 + 15;
    int y = BODY_Y + (FTR_Y - BODY_Y - total) / 2;
    if (y < BODY_Y + 4) {
        y = BODY_Y + 4;
    }

    char count[16], sub[80];
    snprintf(count, sizeof(count), "%lu", (unsigned long)note_count);

    if (has_continue) {
        snprintf(sub, sizeof(sub), "%s", cont_sub ? cont_sub : "");
        char title[64];
        fit(&font_plex_sans_12, cont_title ? cont_title : "", 300, title, sizeof(title));
        char line[160];
        snprintf(line, sizeof(line), "%s%s%s", title, sub[0] ? " · " : "", sub);
        home_item(y, CARD_H, sel == 0, &font_plex_sans_18_bold, 23, "Continue writing", line,
                  NULL, NULL);
        y += CARD_H + ITEM_GAP;
    }
    home_item(y, ROW_H, sel == 1, &font_plex_sans_16_semibold, 21, "New note", NULL, NULL, "Ctrl+N");
    y += ROW_H + ITEM_GAP;
    home_item(y, ROW_H, sel == 2, &font_plex_sans_16_semibold, 21, "All writing", NULL, count, "Ctrl+O");
    y += ROW_H + ITEM_GAP;
    home_item(y, ROW_H, sel == 3, &font_plex_sans_16_semibold, 21, "Search", NULL, NULL, "Ctrl+K");
    y += ROW_H + 3;
    if (last_wrote) {
        draw(&font_plex_sans_12, MARGIN_X + 2, y, 15, last_wrote, true);
    }

    static const hint_t hints[] = {{"\xE2\x86\x91\xE2\x86\x93", "Select"}, {"Enter", "Open"}};
    footer(hints, 2);
}

/* ---- All writing ----------------------------------------------------------------------- */

#define LIST_ROW_H 45

static void scrollbar(int x, int y, int h, size_t total, size_t visible, size_t top)
{
    if (total <= visible || h < 8) {
        return;
    }
    gfx_vline(x + 1, y, h, true);
    int th = (int)((size_t)h * visible / total);
    if (th < 8) {
        th = 8;
    }
    const int ty = y + (int)(((size_t)(h - th) * top) / (total - visible));
    gfx_fill_rect(x, ty, 3, th, true);
}

/* Draws up to max_rows result rows starting at y0; the row at `sel` is highlighted. */
static void list_rows(const ui_row_t *rows, size_t n, size_t sel, size_t top, int y0, size_t max_rows)
{
    const int x = 0, w = GFX_W - 10;
    for (size_t i = top, row = 0; i < n && row < max_rows; i++, row++) {
        const int y = y0 + (int)row * LIST_ROW_H;
        const bool selected = i == sel;
        if (selected) {
            gfx_fill_rect(x, y, w, LIST_ROW_H, true);
        } else {
            gfx_hline(x, y + LIST_ROW_H - 1, w, true);
        }
        const bool ink = !selected;
        const gfx_font_t *tf = selected ? &font_plex_sans_15_bold : &font_plex_sans_15_semibold;

        const int when_w = text_w(&font_plex_sans_12, rows[i].when);
        char title[64];
        char shown[80];
        fit(tf, rows[i].title, w - MARGIN_X - MARGIN_X - when_w - 8 - (selected ? 14 : 0), title,
            sizeof(title));
        snprintf(shown, sizeof(shown), "%s", title);

        int tx = MARGIN_X;
        if (selected) {
            triangle_right(tx, y + 5 + (19 - 10) / 2, 10, false);
            tx += 14;
        }
        draw(tf, tx, y + 5, 19, shown, ink);
        draw_right(&font_plex_sans_12, w - MARGIN_X, y + 5, 19, rows[i].when, ink);
        int sx = MARGIN_X;
        if (rows[i].flag) {
            flag_icon(sx, y + 5 + 19 + 3, ink);
            sx += 13;
        }
        char sub[96];
        fit(&font_plex_sans_12, rows[i].sub, w - MARGIN_X - sx, sub, sizeof(sub));
        draw(&font_plex_sans_12, sx, y + 5 + 19 + 1, 15, sub, ink);
    }

}

void ui_list(const ui_row_t *rows, size_t n, size_t sel, size_t top)
{
    gfx_clear(false);
    char right[32];
    snprintf(right, sizeof(right), "%lu \xC2\xB7 newest first", (unsigned long)n);
    header(&font_plex_sans_13_bold, "All writing", &font_plex_sans_13, right);

    if (n == 0) {
        draw(&font_plex_sans_15_semibold, MARGIN_X, BODY_Y + 12, 19, "No writing yet", true);
        draw(&font_plex_sans_12, MARGIN_X, BODY_Y + 34, 15, "Press Ctrl+N on Home to start a new note.", true);
    }

    list_rows(rows, n, sel, top, BODY_Y, 6);

    /* The 6th row may be cut by the footer; clear that area and redraw the footer on top. */
    static const hint_t hints[] = {
        {"Enter", "Open to edit"}, {"\xE2\x86\x92", "Details"}, {"Esc", "Home"}};
    footer(hints, 3);
    scrollbar(GFX_W - 6, BODY_Y + 6, FTR_Y - BODY_Y - 12, n, UI_LIST_PAGE, top);
}

/* ---- Search ---------------------------------------------------------------------------- */

#define SEARCH_FIND_Y   (BODY_Y + 6)   /* top of the query box */
#define SEARCH_FIND_H   26
#define SEARCH_ROWS_Y   (BODY_Y + 6 + SEARCH_FIND_H + 6)
#define SEARCH_ROW_H    41

/* One line of text with the matched bytes in semibold and underlined, cut with an ellipsis at max_w. */
static void snippet_line(const ui_hit_t *h, int x, int y_top, int max_w, bool ink)
{
    const gfx_font_t *rf = &font_plex_sans_12, *mf = &font_plex_sans_12_semibold;
    const size_t len = strlen(h->snippet);
    const size_t off = h->hl_off <= len ? h->hl_off : len;
    const size_t mlen = off + h->hl_len <= len ? h->hl_len : len - off;
    char buf[NOTE_SNIPPET_LEN_UI];

    memcpy(buf, h->snippet, off);
    buf[off] = '\0';
    char cut[NOTE_SNIPPET_LEN_UI];
    fit(rf, buf, max_w, cut, sizeof(cut));
    int used = draw(rf, x, y_top, 15, cut, ink);

    memcpy(buf, h->snippet + off, mlen);
    buf[mlen] = '\0';
    if (mlen > 0 && used < max_w) {
        const int mw = draw(mf, x + used, y_top, 15, buf, ink);
        gfx_hline(x + used, baseline(mf, y_top, 15) + 2, mw, ink);
        used += mw;
    }
    if (off + mlen < len && used < max_w) {
        fit(rf, h->snippet + off + mlen, max_w - used, cut, sizeof(cut));
        draw(rf, x + used, y_top, 15, cut, ink);
    }
}

/* One search result: title and date on the first line, the snippet on the second. The row is
 * 2 * pad_y + 33 px tall; the selected one is filled. */
static void hit_row(const ui_hit_t *h, int x0, int w, int y, int pad_x, int pad_y, bool selected, bool first)
{
    const int row_h = 2 * pad_y + 33;
    if (selected) {
        gfx_fill_rect(x0, y, w, row_h, true);
    } else {
        if (first) {
            gfx_hline(x0, y, w, true);
        }
        gfx_hline(x0, y + row_h - 1, w, true);
    }
    const bool ink = !selected;
    int when_w = text_w(&font_plex_sans_12, h->when);
    if (h->flag) {
        when_w += 13;
    }
    int tx = x0 + pad_x;
    if (selected) {
        triangle_right(tx, y + pad_y + (18 - 10) / 2, 10, false);
        tx += 14;
    }
    char title[64];
    fit(&font_plex_sans_15_bold, h->title, x0 + w - tx - pad_x - when_w - 8, title, sizeof(title));
    draw(&font_plex_sans_15_bold, tx, y + pad_y, 18, title, ink);
    draw_right(&font_plex_sans_12, x0 + w - pad_x, y + pad_y, 18, h->when, ink);
    if (h->flag) {
        flag_icon(x0 + w - pad_x - when_w, y + pad_y + 3, ink);
    }
    snippet_line(h, x0 + pad_x, y + pad_y + 18, w - 2 * pad_x, ink);
}

void ui_search(const char *query, const ui_hit_t *hits, size_t n, size_t sel, size_t top)
{
    gfx_clear(false);
    char right[32] = "";
    if (query[0]) {
        snprintf(right, sizeof(right), "%lu %s", (unsigned long)n, n == 1 ? "result" : "results");
    }
    header(&font_plex_sans_13_bold, "Search", &font_plex_sans_13, right[0] ? right : NULL);

    /* "Find" label and the query box with a beam caret */
    const gfx_font_t *lf = &font_plex_sans_12_semibold;
    const int lw = draw(lf, MARGIN_X, SEARCH_FIND_Y, SEARCH_FIND_H, "Find", true);
    const int bx = MARGIN_X + lw + 8, bw = GFX_W - MARGIN_X - bx;
    gfx_rect(bx, SEARCH_FIND_Y, bw, SEARCH_FIND_H, 3, true);
    char shown[64];
    fit(&font_plex_mono_15, query, bw - 6 - 6 - 4, shown, sizeof(shown));
    const int tw = draw(&font_plex_mono_15, bx + 6, SEARCH_FIND_Y, SEARCH_FIND_H, shown, true);
    gfx_fill_rect(bx + 6 + tw + 1, SEARCH_FIND_Y + 6, 2, SEARCH_FIND_H - 12, true);

    if (query[0] == '\0') {
        draw(&font_plex_sans_12, MARGIN_X, SEARCH_ROWS_Y + 8, 15,
             "Type to search the full text of your writing.", true);
    } else if (n == 0) {
        draw(&font_plex_sans_15_semibold, MARGIN_X, SEARCH_ROWS_Y + 8, 19, "Nothing found", true);
    }

    for (size_t i = top, row = 0; i < n && row < UI_SEARCH_PAGE; i++, row++) {
        hit_row(&hits[i], 0, GFX_W, SEARCH_ROWS_Y + (int)row * SEARCH_ROW_H, MARGIN_X, 4, i == sel, row == 0);
    }

    static const hint_t hints[] = {{"\xE2\x86\x91\xE2\x86\x93", "Select"}, {"Enter", "Open to edit"}, {"Esc", "Home"}};
    footer(hints, 3);
}

/* ---- Editor ---------------------------------------------------------------------------- */

int ui_editor_advance(uint32_t cp, void *ctx)
{
    (void)ctx;
    return gfx_font_advance(&font_plex_mono_17, cp);
}

/* Pixel width of the first n bytes of an editor line. */
static int text_span_w(const char *s, size_t n)
{
    int w = 0;
    size_t i = 0;
    while (i < n) {
        uint32_t cp;
        i += utf8_next(s + i, n - i, &cp);
        w += gfx_font_advance(&font_plex_mono_17, cp);
    }
    return w;
}

#define ED_TEXT_Y   34
#define ED_LINE_H   22
#define BANNER_H    68

void ui_editor(ed_t *e, const char *title, ui_save_t save, const char *saved_time,
               const char *banner_head, const char *banner_text)
{
    gfx_clear(false);
    header_frame();

    /* title, left */
    char shown[64];
    const char *ts = (title && title[0]) ? title : "Untitled";

    /* status, right */
    char status[32];
    int right_w = 0;
    switch (save) {
    case UI_SAVED:
        if (saved_time && saved_time[0]) {
            snprintf(status, sizeof(status), "Saved %s", saved_time);
        } else {
            snprintf(status, sizeof(status), "New note");
        }
        right_w = text_w(&font_plex_sans_13, status);
        draw_right(&font_plex_sans_13, GFX_W - MARGIN_X, 0, HDR_H - 2, status, true);
        break;
    case UI_UNSAVED:
        snprintf(status, sizeof(status), "Unsaved");
        right_w = text_w(&font_plex_sans_13_bold, status) + 14;
        draw_right(&font_plex_sans_13_bold, GFX_W - MARGIN_X, 0, HDR_H - 2, status, true);
        fill_circle(GFX_W - MARGIN_X - right_w + 4, (HDR_H - 2) / 2, 4, true);
        break;
    case UI_SAVING:
        snprintf(status, sizeof(status), "Saving\xE2\x80\xA6");
        right_w = text_w(&font_plex_sans_13, status);
        draw_right(&font_plex_sans_13, GFX_W - MARGIN_X, 0, HDR_H - 2, status, true);
        break;
    case UI_SAVE_FAILED: {
        snprintf(status, sizeof(status), "Save failed");
        const int tw = text_w(&font_plex_sans_13_bold, status);
        right_w = tw + 12 + 14 + 12;
        gfx_fill_rect(GFX_W - right_w, 0, right_w, HDR_H - 2, true);
        cross(GFX_W - right_w + 12, (HDR_H - 2) / 2 - 4, 8, false);
        draw(&font_plex_sans_13_bold, GFX_W - right_w + 12 + 14, 0, HDR_H - 2, status, false);
        break;
    }
    }
    fit(&font_plex_sans_13_bold, ts, GFX_W - MARGIN_X - right_w - 16 - MARGIN_X, shown, sizeof(shown));
    draw(&font_plex_sans_13_bold, MARGIN_X, 0, HDR_H - 2, shown, true);

    /* text */
    const bool banner = banner_head != NULL;
    const int lines = banner ? UI_EDITOR_LINES_BANNER : UI_EDITOR_LINES;
    ed_scroll_to_cursor(e, (size_t)lines);
    const size_t nlines = ed_line_count(e);
    for (int i = 0; i < lines; i++) {
        const size_t li = e->top_line + (size_t)i;
        if (li >= nlines) {
            break;
        }
        const char *s;
        size_t len;
        ed_line(e, li, &s, &len);
        const int y = ED_TEXT_Y + i * ED_LINE_H;
        gfx_draw_text(&font_plex_mono_17, MARGIN_X, baseline(&font_plex_mono_17, y, ED_LINE_H), s, len, true);

        /* selection: the covered part is drawn inverted (white text on a black block) */
        size_t sa, sb;
        if (ed_sel_range(e, &sa, &sb)) {
            const size_t ls = (size_t)(s - e->buf);
            const size_t a = sa > ls ? sa : ls;
            const size_t b = sb < ls + len ? sb : ls + len;
            if (a < b) {
                const int x0 = MARGIN_X + text_span_w(s, a - ls);
                const int x1 = MARGIN_X + text_span_w(s, b - ls);
                gfx_fill_rect(x0, y + 1, x1 - x0, ED_LINE_H - 2, true);
                gfx_draw_text(&font_plex_mono_17, x0, baseline(&font_plex_mono_17, y, ED_LINE_H), e->buf + a,
                              b - a, false);
            }
        }
    }

    /* B1: a brand-new note shows a placeholder after the cursor and a hint box under it */
    const bool empty_new = e->len == 0 && save == UI_SAVED && !(saved_time && saved_time[0]) && !banner;
    if (empty_new) {
        draw(&font_plex_mono_15, MARGIN_X + 2 + 6, ED_TEXT_Y, ED_LINE_H, "Start typing\xE2\x80\xA6", true);
        const int bx = MARGIN_X, bw = GFX_W - MARGIN_X - 16, by = ED_TEXT_Y + ED_LINE_H + 10;
        const int n = paragraph(&font_plex_sans_12, 0, 0, bw - 2 - 16, 16,
                                "The first line becomes the title. You can rename it later in Details.", true, false);
        dashed_rect(bx, by, bw, 2 + 12 + n * 16, true);
        paragraph(&font_plex_sans_12, bx + 1 + 8, by + 1 + 6, bw - 2 - 16, 16,
                  "The first line becomes the title. You can rename it later in Details.", true, true);
    }

    /* cursor: a 2 px beam */
    const size_t cl = ed_cursor_line(e);
    if (cl >= e->top_line && cl < e->top_line + (size_t)lines) {
        const int cy = ED_TEXT_Y + (int)(cl - e->top_line) * ED_LINE_H;
        gfx_fill_rect(MARGIN_X + ed_cursor_x(e), cy + 1, 2, 20, true);
    }

    scrollbar(GFX_W - 6, ED_TEXT_Y, FTR_Y - ED_TEXT_Y - 8, nlines, (size_t)lines, e->top_line);

    /* error banner above the footer */
    if (banner) {
        const int by = FTR_Y - 6 - BANNER_H;
        gfx_fill_rect(5, by - 3, GFX_W - 10, BANNER_H + 6, false);
        gfx_rect(5, by - 3, GFX_W - 10, BANNER_H + 6, 1, true);
        gfx_rect(8, by, GFX_W - 16, BANNER_H, 2, true);
        draw(&font_plex_sans_13_bold, 8 + 2 + 8, by + 2 + 6, 16, banner_head, true);
        if (banner_text) {
            paragraph(&font_plex_sans_12, 8 + 2 + 8, by + 2 + 6 + 18, GFX_W - 16 - 4 - 16, 16,
                      banner_text, true, true);
        }
    }

    static const hint_t hints[] = {{"Ctrl+S", "Save"}, {"Ctrl+K", "Look up"}, {"Esc", "Menu"}};
    static const hint_t hints_fail[] = {{"Ctrl+S", "Retry"}, {"Esc", "Keep writing"}};
    static const hint_t hints_empty[] = {{"Ctrl+K", "Look up"}, {"Esc", "Discard empty note"}};
    if (save == UI_SAVE_FAILED) {
        footer(hints_fail, 2);
    } else if (empty_new) {
        footer(hints_empty, 2);
    } else {
        footer(hints, 3);
    }
}

/* ---- Keyboard not detected ------------------------------------------------------------- */

/* ---- Look up (D1) ---------------------------------------------------------------------- */

#define LK_X     14
#define LK_Y     12
#define LK_W     (GFX_W - 14 - 18)
#define LK_H     (GFX_H - 12 - 16)
#define LK_HDR_H 24
#define LK_FTR_H 22
#define LK_ROW_H 39

/* Keeps every other pixel (a checkerboard of paper), so what is drawn next to it reads as the background. */
void ui_dim(void)
{
    uint8_t *fb = gfx_buffer();
    for (int y = 0; y < GFX_H; y++) {
        const uint8_t keep = (y & 1) ? 0xAA : 0x55;
        for (int i = 0; i < GFX_W / 8; i++) {
            fb[y * (GFX_W / 8) + i] &= keep;
        }
    }
}

void ui_lookup(const char *query, const ui_hit_t *hits, size_t n, size_t sel, size_t top)
{
    gfx_fill_rect(LK_X + 4, LK_Y + 4, LK_W, LK_H, true);  /* hard shadow */
    gfx_fill_rect(LK_X, LK_Y, LK_W, LK_H, false);
    gfx_rect(LK_X, LK_Y, LK_W, LK_H, 2, true);

    const int ix = LK_X + 2, iw = LK_W - 4;
    int y = LK_Y + 2;
    draw(&font_plex_sans_13_bold, ix + 10, y, LK_HDR_H, "Look up \xC2\xB7 draft kept", true);
    if (query[0]) {
        char right[24];
        snprintf(right, sizeof(right), "%lu %s", (unsigned long)n, n == 1 ? "result" : "results");
        draw_right(&font_plex_sans_13, ix + iw - 10, y, LK_HDR_H, right, true);
    }
    gfx_hline(ix, y + LK_HDR_H, iw, true);
    y += LK_HDR_H + 1;

    const int fy = y + 6;
    const int lw = draw(&font_plex_sans_12_semibold, ix + 10, fy, SEARCH_FIND_H, "Find", true);
    const int bx = ix + 10 + lw + 8, bw = ix + iw - 10 - bx;
    gfx_rect(bx, fy, bw, SEARCH_FIND_H, 3, true);
    char shown[64];
    fit(&font_plex_mono_15, query, bw - 6 - 6 - 4, shown, sizeof(shown));
    const int tw = draw(&font_plex_mono_15, bx + 6, fy, SEARCH_FIND_H, shown, true);
    gfx_fill_rect(bx + 6 + tw + 1, fy + 6, 2, SEARCH_FIND_H - 12, true);
    const int y0 = fy + SEARCH_FIND_H + 6;

    if (query[0] == '\0') {
        draw(&font_plex_sans_12, ix + 10, y0 + 8, 15, "Type to look through your other writing.", true);
    } else if (n == 0) {
        draw(&font_plex_sans_15_semibold, ix + 10, y0 + 8, 19, "Nothing found", true);
    }
    for (size_t i = top, row = 0; i < n && row < UI_LOOKUP_PAGE; i++, row++) {
        hit_row(&hits[i], ix, iw, y0 + (int)row * LK_ROW_H, 10, 3, i == sel, row == 0);
    }

    static const hint_t hints[] = {{"\xE2\x86\x91\xE2\x86\x93", "Select"}, {"Enter", "Preview"}, {"Esc", "Back to draft"}};
    footer_at(ix, LK_Y + LK_H - 2 - LK_FTR_H, iw, LK_FTR_H, hints, 3, 10);
}

/* ---- Read-only preview (D2) ------------------------------------------------------------ */

#define PV_TEXT_Y 50

/* A text line in the editor font with every match of q drawn in the medium weight and underlined. */
static void marked_line(const char *s, size_t len, int x, int y, const char *q, size_t qlen)
{
    const gfx_font_t *rf = &font_plex_mono_17, *mf = &font_plex_mono_17_medium;
    const int base = baseline(rf, y, ED_LINE_H);
    size_t i = 0;
    while (i < len) {
        const size_t m = tf_find_nocase(s + i, len - i, q, qlen);
        const size_t seg = m == TF_NOT_FOUND ? len - i : m;
        x = gfx_draw_text(rf, x, base, s + i, seg, true);
        i += seg;
        if (m == TF_NOT_FOUND) {
            break;
        }
        const int mx = x;
        x = gfx_draw_text(mf, x, base, s + i, qlen, true);
        gfx_hline(mx, base + 2, x - mx, true);
        i += qlen;
    }
}

void ui_preview(const char *title, const char *query, size_t pos, size_t total, const char *meta, ed_t *e)
{
    gfx_clear(false);
    header_frame();

    const gfx_font_t *cf = &font_plex_sans_12_semibold;
    const int chip_w = text_w(cf, "READ") + 12 + 8;
    gfx_fill_rect(0, 0, chip_w, HDR_H - 2, true);
    draw(cf, 12, 0, HDR_H - 2, "READ", false);

    char q[24], right[48];
    fit(&font_plex_sans_13, query, 90, q, sizeof(q));
    snprintf(right, sizeof(right), "\xE2\x80\x9C%s\xE2\x80\x9D %lu/%lu", q, (unsigned long)pos, (unsigned long)total);
    draw_right(&font_plex_sans_13, GFX_W - MARGIN_X, 0, HDR_H - 2, right, true);
    char shown[64];
    fit(&font_plex_sans_13_bold, title, GFX_W - MARGIN_X - text_w(&font_plex_sans_13, right) - 8 - chip_w - 8,
        shown, sizeof(shown));
    draw(&font_plex_sans_13_bold, chip_w + 8, 0, HDR_H - 2, shown, true);

    draw(&font_plex_sans_12, MARGIN_X, HDR_H + 4, 16, meta, true);

    const size_t nlines = ed_line_count(e);
    const size_t qlen = strlen(query);
    for (int i = 0; i < UI_PREVIEW_LINES; i++) {
        const size_t li = e->top_line + (size_t)i;
        if (li >= nlines) {
            break;
        }
        const char *s;
        size_t len;
        ed_line(e, li, &s, &len);
        marked_line(s, len, MARGIN_X, PV_TEXT_Y + i * ED_LINE_H, query, qlen);
    }
    scrollbar(GFX_W - 6, PV_TEXT_Y, FTR_Y - PV_TEXT_Y - 8, nlines, UI_PREVIEW_LINES, e->top_line);

    static const hint_t hints[] = {{"\xE2\x86\x91\xE2\x86\x93", "Scroll"}, {"Ctrl+Enter", "Insert reference"}, {"Esc", "Results"}};
    footer(hints, 3);
}

/* ---- Details (E1) ---------------------------------------------------------------------- */

#define DT_LEFT_W  168
#define DT_ITEM_H  31

void ui_details(const ui_detail_t *d, int sel)
{
    gfx_clear(false);
    header(&font_plex_sans_13_bold, d->title, &font_plex_sans_13, "Details");

    /* facts */
    const gfx_font_t *lf = &font_plex_sans_12, *bf = &font_plex_sans_12_semibold;
    struct {
        const char *label;
        const char *value;
        const gfx_font_t *font;
        bool flag_icon;
    } rows[] = {
        {"Status", d->status, bf, false},
        {"Category", d->category[0] ? d->category : "\xE2\x80\x94", bf, false},
        {"Tags", d->tags[0] ? d->tags : "\xE2\x80\x94", bf, false},
        {"Flag", d->flag ? "Flagged" : "\xE2\x80\x94", lf, d->flag},
        {"Created", d->created, lf, false},
        {"Edited", d->edited, lf, false},
        {"Length", d->length, lf, false},
        {"ID", d->id, &font_plex_mono_15, false},
    };
    int label_w = 0;
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        const int w = text_w(lf, rows[i].label);
        label_w = w > label_w ? w : label_w;
    }
    const int vx = 12 + label_w + 8;
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        const int y = BODY_Y + 10 + (int)i * 21;
        draw(lf, 12, y, 16, rows[i].label, true);
        int x = vx;
        if (rows[i].flag_icon) {
            flag_icon(x, y + 3, true);
            x += 13;
        }
        char val[64];
        fit(rows[i].font, rows[i].value, DT_LEFT_W - 10 - x, val, sizeof(val));
        draw(rows[i].font, x, y, 16, val, true);
    }
    gfx_vline(DT_LEFT_W, BODY_Y, FTR_Y - BODY_Y, true);

    /* actions */
    static const char *const names[UI_DETAIL_ITEMS] = {"Continue writing", "Rename", "Change status",
                                                       "Tags & category", "Flag", "Export to card", "Archive\xE2\x80\xA6"};
    const int ax = DT_LEFT_W + 1, aw = GFX_W - ax;
    int y = BODY_Y + 8;
    for (int i = 0; i < UI_DETAIL_ITEMS; i++) {
        if (i == UI_DETAIL_ITEMS - 1) {
            y += 4;
            if (i != sel) {
                dashed_hline(ax, y, aw, true);
            }
        }
        const bool selected = i == sel;
        const gfx_font_t *f = selected ? &font_plex_sans_15_bold : &font_plex_sans_15_semibold;
        if (selected) {
            gfx_fill_rect(ax, y, aw, DT_ITEM_H, true);
        }
        int tx = ax + 12;
        if (selected) {
            triangle_right(tx, y + (DT_ITEM_H - 10) / 2, 10, false);
            tx += 14;
        }
        const char *label = names[i];
        if (i == UI_DETAIL_ITEMS - 1 && d->archived) {
            label = "Unarchive\xE2\x80\xA6";
        }
        draw(f, tx, y, DT_ITEM_H, label, !selected);
        y += DT_ITEM_H;
    }

    static const hint_t hints[] = {{"\xE2\x86\x91\xE2\x86\x93", "Select"}, {"Enter", "Do"}, {"Esc", "List"}};
    footer(hints, 3);
}

/* ---- Panels over a dimmed screen (E2 Rename and the other Details actions) -------------- */

#define PN_X   20
#define PN_Y   48
#define PN_W   (GFX_W - 20 - 24)
#define PN_HDR 24
#define PN_FTR 22

/* Draws the shadow, frame, header and footer for a body of body_h px; returns the body's top y. */
static int panel_frame(const char *title, int body_h, const hint_t *hints, size_t nhints)
{
    const int h = 2 + PN_HDR + 1 + body_h + PN_FTR + 2;
    gfx_fill_rect(PN_X + 4, PN_Y + 4, PN_W, h, true);
    gfx_fill_rect(PN_X, PN_Y, PN_W, h, false);
    gfx_rect(PN_X, PN_Y, PN_W, h, 2, true);
    draw(&font_plex_sans_13_bold, PN_X + 2 + 10, PN_Y + 2, PN_HDR, title, true);
    gfx_hline(PN_X + 2, PN_Y + 2 + PN_HDR, PN_W - 4, true);
    footer_at(PN_X + 2, PN_Y + h - 2 - PN_FTR, PN_W - 4, PN_FTR, hints, nhints, 10);
    return PN_Y + 2 + PN_HDR + 1;
}

/* The end of `s` that fits in max_w, so what is being typed stays visible. */
static const char *tail_fit(const gfx_font_t *f, const char *s, int max_w)
{
    const size_t len = strlen(s);
    size_t start = 0;
    while (start < len && text_w(f, s + start) > max_w) {
        start++;
        while (start < len && ((unsigned char)s[start] & 0xC0) == 0x80) {
            start++;
        }
    }
    return s + start;
}

void ui_panel_form(const char *title, const ui_field_t *fields, size_t n, size_t active,
                   const char *note, const char *enter_label)
{
    const int inner_w = PN_W - 4 - 20;
    const int note_lines = note ? paragraph(&font_plex_sans_12, 0, 0, inner_w, 16, note, false, false) : 0;
    const int body_h = 10 + (int)n * (15 + 5 + 30) + (int)(n - 1) * 8 + (note ? 5 + note_lines * 16 : 0) + 8;

    char enter_hint[24];
    snprintf(enter_hint, sizeof(enter_hint), "%s", enter_label);
    const hint_t hints_one[] = {{"Enter", enter_hint}, {"Esc", "Cancel"}};
    const hint_t hints_many[] = {{"\xE2\x86\x91\xE2\x86\x93", "Field"}, {"Enter", enter_hint}, {"Esc", "Cancel"}};
    int y = panel_frame(title, body_h, n > 1 ? hints_many : hints_one, n > 1 ? 3 : 2) + 10;

    for (size_t i = 0; i < n; i++) {
        const bool on = i == active;
        draw(&font_plex_sans_12_semibold, PN_X + 2 + 10, y, 15, fields[i].label, true);
        y += 15 + 5;
        const int bx = PN_X + 2 + 10;
        gfx_rect(bx, y, inner_w, 30, on ? 3 : 1, true);
        const char *shown = tail_fit(&font_plex_mono_15, fields[i].value, inner_w - 6 - 6 - 4);
        const int tw = draw(&font_plex_mono_15, bx + 6, y, 30, shown, true);
        if (on) {
            gfx_fill_rect(bx + 6 + tw + 1, y + 7, 2, 16, true);
        }
        y += 30 + 8;
    }
    if (note) {
        y -= 8;
        paragraph(&font_plex_sans_12, PN_X + 2 + 10, y + 5, inner_w, 16, note, true, true);
    }
}

void ui_panel_pick(const char *title, const char *const *items, size_t n, size_t sel)
{
    static const hint_t hints[] = {{"\xE2\x86\x91\xE2\x86\x93", "Select"}, {"Enter", "Set"}, {"Esc", "Cancel"}};
    int y = panel_frame(title, 8 + (int)n * 26 + 8, hints, 3) + 8;
    for (size_t i = 0; i < n; i++, y += 26) {
        const bool on = i == sel;
        if (on) {
            gfx_fill_rect(PN_X + 2, y, PN_W - 4, 26, true);
        }
        int tx = PN_X + 2 + 10;
        if (on) {
            triangle_right(tx, y + 8, 10, false);
            tx += 14;
        }
        draw(on ? &font_plex_sans_15_bold : &font_plex_sans_15_semibold, tx, y, 26, items[i], !on);
    }
}

void ui_panel_message(const char *title, const char *text, const char *enter_label, const char *esc_label)
{
    const int inner_w = PN_W - 4 - 20;
    const int lines = paragraph(&font_plex_sans_13, 0, 0, inner_w, 18, text, false, false);
    hint_t hints[2];
    size_t nh = 0;
    if (enter_label) {
        hints[nh++] = (hint_t){"Enter", enter_label};
    }
    hints[nh++] = (hint_t){"Esc", esc_label};
    const int y = panel_frame(title, 10 + lines * 18 + 10, hints, nh) + 10;
    paragraph(&font_plex_sans_13, PN_X + 2 + 10, y, inner_w, 18, text, true, true);
}

/* ---- Set clock ------------------------------------------------------------------------- */

void ui_clock(int year, int month, int day, int hour, int minute, int field, bool first_run)
{
    gfx_clear(false);
    header(&font_plex_sans_13_bold, "Set clock", NULL, NULL);

    const int x = 24, w = GFX_W - 2 * x;
    int y = BODY_Y + 24;
    const char *msg = first_run
        ? "The clock has lost its time, so dates and times on your notes would be wrong. "
          "Set it once; it keeps running while the device is off."
        : "Move between the fields, change the value, then press Enter.";
    y += paragraph(&font_plex_sans_13, x, y, w, 18, msg, true, true) * 18 + 24;

    const gfx_font_t *f = &font_plex_mono_17_medium;
    const int pad = 4, h = 28;
    char parts[UI_CLOCK_FIELDS][8];
    snprintf(parts[0], sizeof(parts[0]), "%04d", year);
    snprintf(parts[1], sizeof(parts[1]), "%02d", month);
    snprintf(parts[2], sizeof(parts[2]), "%02d", day);
    snprintf(parts[3], sizeof(parts[3]), "%02d", hour);
    snprintf(parts[4], sizeof(parts[4]), "%02d", minute);
    static const char *const seps[UI_CLOCK_FIELDS] = {"-", "-", "   ", ":", ""};

    int cx = x;
    for (int i = 0; i < UI_CLOCK_FIELDS; i++) {
        const int tw = text_w(f, parts[i]);
        const bool sel = i == field;
        if (sel) {
            gfx_fill_rect(cx, y, tw + 2 * pad, h, true);
        } else {
            gfx_rect(cx, y, tw + 2 * pad, h, 1, true);
        }
        draw(f, cx + pad, y, h, parts[i], !sel);
        cx += tw + 2 * pad;
        draw(f, cx + 2, y, h, seps[i], true);
        cx += text_w(f, seps[i]) + 4;
    }
    y += h + 10;
    static const char *const names[UI_CLOCK_FIELDS] = {"Year", "Month", "Day", "Hour", "Minute"};
    draw(&font_plex_sans_12, x, y, 14, names[field < 0 || field >= UI_CLOCK_FIELDS ? 0 : field], true);

    static const hint_t hints[] = {{"\xE2\x86\x90\xE2\x86\x92", "Field"},
                                   {"\xE2\x86\x91\xE2\x86\x93", "Change"},
                                   {"Enter", "Set"},
                                   {"Esc", "Cancel"}};
    footer(hints, 4);
}

void ui_kbd_error(const char *saved_time)
{
    gfx_clear(false);
    char status[32];
    snprintf(status, sizeof(status), "Saved %s", saved_time ? saved_time : "");
    header(&font_plex_sans_13_bold, "FocusType", &font_plex_sans_13, status);

    const int x = 24, w = GFX_W - 2 * x;
    int y = BODY_Y + 50;

    /* keyboard icon, 56 x 28 */
    gfx_rect(x, y, 56, 28, 3, true);
    for (int row = 0; row < 2; row++) {
        for (int k = 0; k < 5; k++) {
            gfx_fill_rect(x + 7 + k * 8, y + 7 + row * 6, 4, 3, true);
        }
    }
    gfx_fill_rect(x + 12, y + 20, 32, 3, true);
    y += 28 + 10;

    draw(&font_plex_sans_19_bold, x, y, 24, "Keyboard not detected", true);
    y += 24 + 4;

    char msg[120];
    snprintf(msg, sizeof(msg),
             "Your writing is safe \xE2\x80\x94 last saved at %s. Reconnect the keyboard to pick up "
             "right where your cursor was.",
             saved_time ? saved_time : "--:--");
    const int n = paragraph(&font_plex_sans_13, x, y, w, 18, msg, true, true);
    y += n * 18 + 10;

    /* button legends */
    const int chip_w = 70, chip_h = 18;
    gfx_fill_rect(x, y, chip_w, chip_h, true);
    draw(&font_plex_sans_13_bold, x + (chip_w - text_w(&font_plex_sans_13_bold, "KEY")) / 2, y,
         chip_h, "KEY", false);
    draw(&font_plex_sans_13, x + chip_w + 8, y, chip_h, "Scan for keyboard", true);
    y += chip_h + 4;
    gfx_rect(x, y, chip_w, chip_h, 1, true);
    draw(&font_plex_sans_13_bold, x + (chip_w - text_w(&font_plex_sans_13_bold, "Hold PWR")) / 2, y,
         chip_h, "Hold PWR", true);
    draw(&font_plex_sans_13, x + chip_w + 8, y, chip_h, "Power off", true);

    footer_clear();
    draw(&font_plex_sans_12, MARGIN_X, FTR_Y + 4, 14, "Use the physical buttons on the device", true);
}

esp_err_t ui_present(void)
{
    return gfx_flush();
}
