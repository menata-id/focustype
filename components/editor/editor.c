#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "utf8.h"

#define NO_BREAK ((size_t)-1)

void ed_init(ed_t *e, char *buf, size_t cap, int max_width, ed_advance_fn advance, void *ctx)
{
    memset(e, 0, sizeof(*e));
    e->buf = buf;
    e->cap = cap;
    e->max_width = max_width;
    e->advance = advance;
    e->ctx = ctx;
    e->want_x = -1;
    if (cap > 0) {
        buf[0] = '\0';
    }
}

void ed_free(ed_t *e)
{
    free(e->starts);
    e->starts = NULL;
    e->nlines = e->lines_cap = 0;
    e->layout_ok = false;
}

/* ---- helpers --------------------------------------------------------------------------- */

static size_t cp_size_at(const ed_t *e, size_t i)
{
    uint32_t cp;
    return utf8_next(e->buf + i, e->len - i, &cp);
}

static size_t prev_cp(const ed_t *e, size_t i)
{
    if (i == 0) {
        return 0;
    }
    i--;
    while (i > 0 && (((uint8_t)e->buf[i]) & 0xC0) == 0x80) {
        i--;
    }
    return i;
}

static int measure(const ed_t *e, size_t from, size_t to)
{
    int w = 0;
    while (from < to) {
        uint32_t cp;
        from += utf8_next(e->buf + from, e->len - from, &cp);
        w += e->advance(cp, e->ctx);
    }
    return w;
}

static void touch(ed_t *e)
{
    e->layout_ok = false;
    e->want_x = -1;
}

/* ---- layout ---------------------------------------------------------------------------- */

static void add_start(ed_t *e, size_t pos)
{
    if (e->nlines == e->lines_cap) {
        const size_t ncap = e->lines_cap ? e->lines_cap * 2 : 64;
        uint32_t *p = realloc(e->starts, ncap * sizeof(uint32_t));
        if (!p) {
            return;  /* out of memory: keep the lines laid out so far */
        }
        e->starts = p;
        e->lines_cap = ncap;
    }
    e->starts[e->nlines++] = (uint32_t)pos;
}

static void layout(ed_t *e)
{
    e->nlines = 0;
    add_start(e, 0);

    size_t line_start = 0;
    size_t i = 0;
    size_t brk = NO_BREAK;  /* offset just after the last space on this line */
    int x = 0;

    while (i < e->len) {
        uint32_t cp;
        const size_t sz = utf8_next(e->buf + i, e->len - i, &cp);

        if (cp == '\n') {
            i += sz;
            add_start(e, i);  /* a trailing '\n' leaves an empty last line for the cursor */
            line_start = i;
            x = 0;
            brk = NO_BREAK;
            continue;
        }

        const int w = e->advance(cp, e->ctx);
        /* Spaces may hang past the edge instead of wrapping. */
        if (cp != ' ' && x + w > e->max_width && i > line_start) {
            const size_t ns = (brk != NO_BREAK && brk > line_start) ? brk : i;
            add_start(e, ns);
            line_start = ns;
            x = measure(e, ns, i);
            brk = NO_BREAK;
            continue;  /* re-test this character on the new line */
        }

        x += w;
        i += sz;
        if (cp == ' ') {
            brk = i;
        }
    }
    e->layout_ok = true;
}

static void ensure_layout(ed_t *e)
{
    if (!e->layout_ok) {
        layout(e);
    }
}

static size_t line_end(const ed_t *e, size_t k)
{
    return (k + 1 < e->nlines) ? e->starts[k + 1] : e->len;
}

/* Largest cursor position that still belongs to line k. */
static size_t line_max_pos(const ed_t *e, size_t k)
{
    const size_t start = e->starts[k];
    const size_t end = line_end(e, k);
    if (k + 1 >= e->nlines) {
        return end;
    }
    if (end > start && e->buf[end - 1] == '\n') {
        return end - 1;
    }
    /* Wrapped line: the position after its last character is the next line's start, so stop
     * one character earlier. */
    return prev_cp(e, end);
}

static size_t pos_at_x(const ed_t *e, size_t k, int want_x)
{
    size_t p = e->starts[k];
    const size_t maxp = line_max_pos(e, k);
    int x = 0;
    while (p < maxp) {
        uint32_t cp;
        const size_t sz = utf8_next(e->buf + p, e->len - p, &cp);
        const int w = e->advance(cp, e->ctx);
        if (want_x < x + w / 2) {
            break;
        }
        x += w;
        p += sz;
    }
    return p;
}

size_t ed_line_count(ed_t *e)
{
    ensure_layout(e);
    return e->nlines;
}

size_t ed_line_start(ed_t *e, size_t i)
{
    ensure_layout(e);
    return (i < e->nlines) ? e->starts[i] : e->len;
}

void ed_line(ed_t *e, size_t i, const char **s, size_t *len)
{
    ensure_layout(e);
    if (i >= e->nlines) {
        *s = e->buf + e->len;
        *len = 0;
        return;
    }
    const size_t start = e->starts[i];
    size_t end = line_end(e, i);
    if (end > start && e->buf[end - 1] == '\n') {
        end--;
    }
    *s = e->buf + start;
    *len = end - start;
}

size_t ed_cursor_line(ed_t *e)
{
    ensure_layout(e);
    size_t lo = 0, hi = e->nlines;  /* find the last start <= cursor */
    while (hi - lo > 1) {
        const size_t mid = (lo + hi) / 2;
        if (e->starts[mid] <= e->cursor) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

int ed_cursor_x(ed_t *e)
{
    const size_t k = ed_cursor_line(e);
    return measure(e, e->starts[k], e->cursor);
}

void ed_scroll_to_cursor(ed_t *e, size_t visible_lines)
{
    if (visible_lines == 0) {
        return;
    }
    const size_t k = ed_cursor_line(e);
    if (k < e->top_line) {
        e->top_line = k;
    } else if (k >= e->top_line + visible_lines) {
        e->top_line = k - visible_lines + 1;
    }
    if (e->top_line >= e->nlines) {
        e->top_line = e->nlines - 1;
    }
}

/* ---- editing --------------------------------------------------------------------------- */

bool ed_set_text(ed_t *e, const char *text, size_t len)
{
    if (len + 1 > e->cap) {
        return false;
    }
    memcpy(e->buf, text, len);
    e->buf[len] = '\0';
    e->len = len;
    e->cursor = len;
    e->top_line = 0;
    e->dirty = false;
    touch(e);
    return true;
}

void ed_mark_clean(ed_t *e)
{
    e->dirty = false;
}

bool ed_insert(ed_t *e, const char *s, size_t len)
{
    if (len == 0) {
        return true;
    }
    if (e->len + len + 1 > e->cap) {
        return false;
    }
    memmove(e->buf + e->cursor + len, e->buf + e->cursor, e->len - e->cursor);
    memcpy(e->buf + e->cursor, s, len);
    e->len += len;
    e->cursor += len;
    e->buf[e->len] = '\0';
    e->dirty = true;
    touch(e);
    return true;
}

bool ed_insert_cp(ed_t *e, uint32_t cp)
{
    char tmp[4];
    const size_t n = utf8_encode(cp, tmp);
    return ed_insert(e, tmp, n);
}

void ed_backspace(ed_t *e)
{
    if (e->cursor == 0) {
        return;
    }
    const size_t p = prev_cp(e, e->cursor);
    memmove(e->buf + p, e->buf + e->cursor, e->len - e->cursor);
    e->len -= e->cursor - p;
    e->cursor = p;
    e->buf[e->len] = '\0';
    e->dirty = true;
    touch(e);
}

void ed_delete(ed_t *e)
{
    if (e->cursor >= e->len) {
        return;
    }
    const size_t sz = cp_size_at(e, e->cursor);
    memmove(e->buf + e->cursor, e->buf + e->cursor + sz, e->len - e->cursor - sz);
    e->len -= sz;
    e->buf[e->len] = '\0';
    e->dirty = true;
    touch(e);
}

/* ---- movement -------------------------------------------------------------------------- */

void ed_left(ed_t *e)
{
    e->cursor = prev_cp(e, e->cursor);
    e->want_x = -1;
}

void ed_right(ed_t *e)
{
    if (e->cursor < e->len) {
        e->cursor += cp_size_at(e, e->cursor);
    }
    e->want_x = -1;
}

static void move_vertical(ed_t *e, bool up)
{
    ensure_layout(e);
    const size_t k = ed_cursor_line(e);
    if (e->want_x < 0) {
        e->want_x = measure(e, e->starts[k], e->cursor);
    }
    if (up) {
        if (k == 0) {
            return;
        }
        e->cursor = pos_at_x(e, k - 1, e->want_x);
    } else {
        if (k + 1 >= e->nlines) {
            return;
        }
        e->cursor = pos_at_x(e, k + 1, e->want_x);
    }
}

void ed_up(ed_t *e)
{
    move_vertical(e, true);
}

void ed_down(ed_t *e)
{
    move_vertical(e, false);
}

void ed_home(ed_t *e)
{
    ensure_layout(e);
    e->cursor = e->starts[ed_cursor_line(e)];
    e->want_x = -1;
}

void ed_end(ed_t *e)
{
    ensure_layout(e);
    e->cursor = line_max_pos(e, ed_cursor_line(e));
    e->want_x = -1;
}

void ed_page_up(ed_t *e, size_t lines)
{
    for (size_t i = 0; i < lines; i++) {
        move_vertical(e, true);
    }
}

void ed_page_down(ed_t *e, size_t lines)
{
    for (size_t i = 0; i < lines; i++) {
        move_vertical(e, false);
    }
}

/* ---- document facts -------------------------------------------------------------------- */

static bool is_space(char c)
{
    return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

size_t ed_word_count(const ed_t *e)
{
    size_t words = 0;
    bool in_word = false;
    for (size_t i = 0; i < e->len; i++) {
        if (is_space(e->buf[i])) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            words++;
        }
    }
    return words;
}

size_t ed_title(const ed_t *e, char *out, size_t cap)
{
    return ed_title_from(e->buf, e->len, out, cap);
}

size_t ed_title_from(const char *text, size_t len, char *out, size_t cap)
{
    if (cap == 0) {
        return 0;
    }
    size_t i = 0;
    while (i < len && is_space(text[i])) {
        i++;
    }
    size_t end = i;
    while (end < len && text[end] != '\n') {
        end++;
    }
    while (end > i && is_space(text[end - 1])) {
        end--;
    }

    if (end == i) {
        const char *def = "Untitled";
        size_t n = strlen(def);
        if (n > cap - 1) {
            n = cap - 1;
        }
        memcpy(out, def, n);
        out[n] = '\0';
        return n;
    }

    size_t n = end - i;
    if (n > cap - 1) {
        n = cap - 1;
        /* Do not cut a UTF-8 sequence in half. */
        while (n > 0 && (((uint8_t)text[i + n]) & 0xC0) == 0x80) {
            n--;
        }
    }
    memcpy(out, text + i, n);
    out[n] = '\0';
    return n;
}
