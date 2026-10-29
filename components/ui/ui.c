#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "gfx_fonts.h"
#include "gfx_text.h"
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

static void footer(const hint_t *hints, size_t n)
{
    footer_clear();
    const int mid = FTR_Y + 1 + (FTR_H - 1) / 2;
    int x = MARGIN_X;
    for (size_t i = 0; i < n; i++) {
        x += keycap(x, mid, hints[i].key, true) + 4;
        x += draw(&font_plex_sans_12, x, mid - 7, 14, hints[i].label, true) + 14;
    }
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
        draw(&font_plex_sans_12, MARGIN_X, y + 5 + 19 + 1, 15, rows[i].sub, ink);
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
        const int y = SEARCH_ROWS_Y + (int)row * SEARCH_ROW_H;
        const bool selected = i == sel;
        if (selected) {
            gfx_fill_rect(0, y, GFX_W, SEARCH_ROW_H, true);
        } else {
            if (row == 0) {
                gfx_hline(0, y, GFX_W, true);
            }
            gfx_hline(0, y + SEARCH_ROW_H - 1, GFX_W, true);
        }
        const bool ink = !selected;
        const int when_w = text_w(&font_plex_sans_12, hits[i].when);
        int tx = MARGIN_X;
        if (selected) {
            triangle_right(tx, y + 4 + (18 - 10) / 2, 10, false);
            tx += 14;
        }
        char title[64];
        fit(&font_plex_sans_15_bold, hits[i].title, GFX_W - tx - MARGIN_X - when_w - 8, title,
            sizeof(title));
        draw(&font_plex_sans_15_bold, tx, y + 4, 18, title, ink);
        draw_right(&font_plex_sans_12, GFX_W - MARGIN_X, y + 4, 18, hits[i].when, ink);
        snippet_line(&hits[i], MARGIN_X, y + 4 + 18, GFX_W - 2 * MARGIN_X, ink);
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
    if (save == UI_SAVE_FAILED) {
        footer(hints_fail, 2);
    } else {
        footer(hints, 3);
    }
}

/* ---- Keyboard not detected ------------------------------------------------------------- */

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
