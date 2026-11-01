#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Text editor core: a UTF-8 buffer, a cursor, and soft word-wrapping. No ESP-IDF or display
 * dependencies; glyph widths come from a callback, so it works with any font and is unit-tested
 * on the host (tests/host/test_editor.c).
 *
 * The cursor is a byte offset that always sits on a codepoint boundary. Wrapping breaks after
 * the last space that fits, or inside a word that is wider than the line.
 */

typedef int (*ed_advance_fn)(uint32_t cp, void *ctx);

typedef struct {
    char *buf;            /* caller-owned storage, NUL-terminated after every edit */
    size_t cap;
    size_t len;
    size_t cursor;
    bool dirty;           /* changed since ed_set_text() or ed_mark_clean() */

    int max_width;        /* line width in pixels */
    ed_advance_fn advance;
    void *ctx;

    size_t top_line;      /* first visible line, maintained by ed_scroll_to_cursor() */

    /* internals */
    uint32_t *starts;     /* byte offset where each visual line starts */
    size_t nlines;
    size_t lines_cap;
    bool layout_ok;
    int want_x;           /* column to keep while moving up/down; -1 = not set */

    bool sel;             /* a selection exists: bytes between anchor and cursor */
    size_t anchor;
} ed_t;

void ed_init(ed_t *e, char *buf, size_t cap, int max_width, ed_advance_fn advance, void *ctx);
void ed_free(ed_t *e);

/* Replaces the whole text and puts the cursor at the end. Returns false if it does not fit. */
bool ed_set_text(ed_t *e, const char *text, size_t len);
void ed_mark_clean(ed_t *e);

/* Editing at the cursor. Insert returns false when the buffer is full. */
bool ed_insert(ed_t *e, const char *s, size_t len);
bool ed_insert_cp(ed_t *e, uint32_t cp);
void ed_backspace(ed_t *e);
void ed_delete(ed_t *e);

/* Movement */
void ed_left(ed_t *e);
void ed_right(ed_t *e);
void ed_up(ed_t *e);
void ed_down(ed_t *e);
void ed_home(ed_t *e);      /* start of the visual line */
void ed_end(ed_t *e);       /* end of the visual line */
void ed_page_up(ed_t *e, size_t lines);
void ed_page_down(ed_t *e, size_t lines);

/* Word movement and deletion (a word is a run of non-whitespace). */
void ed_word_left(ed_t *e);
void ed_word_right(ed_t *e);      /* to the start of the next word */
void ed_delete_word_back(ed_t *e);
void ed_delete_word_fwd(ed_t *e);

/* Selection. Call ed_sel_begin() before a shift-move (it keeps an existing anchor) and
 * ed_sel_clear() before a plain move. Typing, Backspace and Delete replace/remove the selection. */
void ed_sel_begin(ed_t *e);
void ed_sel_clear(ed_t *e);
void ed_select_all(ed_t *e);
bool ed_sel_range(const ed_t *e, size_t *from, size_t *to);   /* false when empty */
bool ed_delete_selection(ed_t *e);                            /* true if something was removed */

/* Layout queries (they lay the text out first if it changed). */
size_t ed_line_count(ed_t *e);
void ed_line(ed_t *e, size_t i, const char **s, size_t *len);  /* without the trailing '\n' */
size_t ed_line_start(ed_t *e, size_t i);                       /* byte offset */
size_t ed_cursor_line(ed_t *e);
int ed_cursor_x(ed_t *e);                                      /* pixels from the line start */
void ed_scroll_to_cursor(ed_t *e, size_t visible_lines);

/* Document facts */
size_t ed_word_count(const ed_t *e);

/* Title from the first non-blank line, trimmed, never splitting a UTF-8 sequence.
 * "Untitled" if the text is blank. Returns the length written (without the NUL). */
size_t ed_title(const ed_t *e, char *out, size_t cap);

/* Same rule applied to a plain text buffer (used to title notes in listings). */
size_t ed_title_from(const char *text, size_t len, char *out, size_t cap);
