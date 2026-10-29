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

typedef struct {
    char title[64];
    char when[16];   /* "02:34", "Yesterday", "12 Oct" */
    char sub[32];    /* "412 words" */
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
void ui_format_words(uint32_t n, char *out, size_t cap);   /* "1,104 words" */
