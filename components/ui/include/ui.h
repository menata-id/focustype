#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"
#include "editor.h"

/*
 * Screens from the FocusType mockup, drawn into the gfx framebuffer. Each ui_* call clears the
 * screen and draws one complete screen; call ui_present() to send it to the panel.
 * Layout follows the mockup: 400x300, 26 px header with a 2 px rule, 22 px footer with a 1 px rule.
 */

#define UI_EDITOR_TEXT_W        372   /* width available to editor text, in pixels */
#define UI_EDITOR_LINES         11    /* visible text lines */
#define UI_EDITOR_LINES_BANNER  7     /* visible lines while an error banner is shown */
#define UI_LIST_PAGE            5     /* rows moved by PgUp/PgDn on the list */
#define UI_SEARCH_PAGE          5     /* result rows visible under the search box */
#define UI_LOOKUP_PAGE          4     /* result rows visible in the look-up panel */
#define UI_PREVIEW_LINES        10    /* text lines visible in the preview */
#define NOTE_SNIPPET_LEN_UI     112   /* size of ui_hit_t.snippet; same as NOTE_SNIPPET_LEN in notes.h */

typedef struct {
    char title[64];
    char when[16];   /* "02:34", "Yesterday", "12 Oct" */
    char sub[96];    /* "Developing · #community · 1,104 words"; cut to fit when drawn */
    bool flag;       /* draw the flag shape before sub */
} ui_row_t;

typedef enum {
    UI_SAVED,
    UI_UNSAVED,
    UI_SAVING,
    UI_SAVE_FAILED,
} ui_save_t;

/* Glyph width callback for ed_init(): matches the editor font used by ui_editor(). */
int ui_editor_advance(uint32_t cp, void *ctx);

/* Home. sel: 0 Continue, 1 New note, 2 All writing, 3 Search. With has_continue false the
 * first item is not shown and sel must be >= 1. */
void ui_home(int sel, bool has_continue, const char *cont_title, const char *cont_sub,
             uint32_t note_count, const char *date, const char *last_wrote);

/* All writing. `rows` holds n rows; the row at `sel` is highlighted and `top` is the first
 * visible row. n is also shown in the header. */
void ui_list(const ui_row_t *rows, size_t n, size_t sel, size_t top);

typedef struct {
    char title[64];
    char when[32];                      /* "Developing · 28 Sep" */
    char snippet[NOTE_SNIPPET_LEN_UI];  /* text around the match */
    uint8_t hl_off, hl_len;             /* matched bytes inside snippet, drawn bold and underlined */
    bool flag;                          /* draw the flag shape before `when` */
} ui_hit_t;

/* Search. `query` is shown in the box; `hits` holds the n matches (highlighted row `sel`, first
 * visible row `top`). With an empty query a hint is shown instead of results. */
void ui_search(const char *query, const ui_hit_t *hits, size_t n, size_t sel, size_t top);

/* Look up while writing (D1): draw the editor, call ui_dim(), then ui_lookup() on top of it. */
void ui_dim(void);
void ui_lookup(const char *query, const ui_hit_t *hits, size_t n, size_t sel, size_t top);

/* Read-only preview (D2). `e` holds the other note's text laid out at UI_EDITOR_TEXT_W; its
 * top_line is the scroll position (the caller keeps it within 0 .. lines - UI_PREVIEW_LINES). The
 * matches of `query` are drawn underlined. pos/total are the note's place in the results. */
void ui_preview(const char *title, const char *query, size_t pos, size_t total, const char *meta, ed_t *e);

/* Details (E1): facts on the left, actions on the right. `sel` is the highlighted action. */
#define UI_DETAIL_ITEMS 7   /* Continue writing, Rename, Change status, Tags & category, Flag, Export, Archive */
typedef struct {
    char title[64];
    char status[16], category[24], tags[48];
    bool flag, archived;
    char created[24], edited[24], length[24], id[16];
} ui_detail_t;
void ui_details(const ui_detail_t *d, int sel);

/* Panels drawn on top of the current screen (call ui_dim() first, as for ui_lookup). */
typedef struct {
    const char *label;
    const char *value;
} ui_field_t;
void ui_panel_form(const char *title, const ui_field_t *fields, size_t n, size_t active,
                   const char *note, const char *enter_label);
void ui_panel_pick(const char *title, const char *const *items, size_t n, size_t sel);
/* enter_label NULL: only Esc is offered. */
void ui_panel_message(const char *title, const char *text, const char *enter_label, const char *esc_label);

/* Editor. saved_time is shown after "Saved"; if it is NULL or empty the header says "New note". Pass banner_head/banner_text (or NULL) to show
 * an error banner above the footer; then only UI_EDITOR_LINES_BANNER lines are visible. */
void ui_editor(ed_t *e, const char *title, ui_save_t save, const char *saved_time,
               const char *banner_head, const char *banner_text);

/* "Keyboard not detected" (F2). */
void ui_kbd_error(const char *saved_time);

/* Set clock. Fields: 0 year, 1 month, 2 day, 3 hour, 4 minute; `field` is the highlighted one.
 * `first_run` shows the "clock lost its time" explanation instead of the plain hint. */
#define UI_CLOCK_FIELDS 5
void ui_clock(int year, int month, int day, int hour, int minute, int field, bool first_run);

esp_err_t ui_present(void);

/* Text helpers (ui_format.c, no display dependency) */
void ui_format_when(time_t t, time_t now, char *out, size_t cap);
/* "20 Oct" and "Yest. 21:40" for the Details panel; "\xE2\x80\x94" when the time is unknown or the
 * clock was never set (before 2025). */
void ui_format_date(time_t t, char *out, size_t cap);
void ui_format_stamp(time_t t, time_t now, char *out, size_t cap);
void ui_format_words(uint32_t n, char *out, size_t cap);   /* "1,104 words" */
