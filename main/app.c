/*
 * App state machine: Home, All writing, Editor and the keyboard-error screen from the mockup.
 * Search, Details/Rename, recovery drafts and the status screen are not built yet.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "app.h"
#include "board.h"
#include "editor.h"
#include "gfx.h"
#include "notes.h"
#include "pcf85063.h"
#include "pcf_regs.h"
#include "st7305.h"
#include "storage.h"
#include "textfind.h"
#include "ui.h"
#include "usb_kbd.h"

static const char *TAG = "app";

#define BUF_CAP        (64 * 1024)
#define MAX_NOTES      64
#define CLOCK_SET_AFTER 1735689600  /* 2025-01-01: earlier means the clock was never set */
#define AUTOSAVE_MS    3000
#define KBD_WAIT_MS    5000
#define SEARCH_DELAY_MS 250   /* wait after the last keystroke before scanning the notes */
#define QUERY_MAX      48
#define NOTES_DIR      STORAGE_MOUNT_POINT "/notes"

typedef enum { S_HOME, S_LIST, S_EDITOR, S_KBD_ERR, S_CLOCK, S_SEARCH, S_LOOKUP, S_PREVIEW, S_DETAIL } state_t;

static struct {
    state_t state, prev_state;
    int home_sel;

    note_info_t notes[MAX_NOTES];
    size_t nnotes;
    size_t list_sel, list_top;

    ed_t ed;
    char cur_id[NOTE_ID_LEN];
    char cur_title[64];      /* manual title of the open note; empty = first line */
    bool cur_new;
    ui_save_t save;
    time_t saved_at;
    int64_t last_edit_ms;

    struct {
        char q[QUERY_MAX];
        size_t qlen;
        note_hit_t res[MAX_NOTES];
        size_t n, sel, top;
        size_t page;         /* result rows visible on the current screen */
        bool pending;        /* the query changed and the results are stale */
        int64_t last_key_ms;
    } search;

    struct {
        char id[NOTE_ID_LEN];
        note_info_t info;        /* refreshed after every change */
        state_t from;            /* S_LIST or S_SEARCH: where Esc goes */
        int sel;                 /* highlighted action */
        enum { P_NONE, P_RENAME, P_STATUS, P_TAGS, P_ARCHIVE, P_MSG } panel;
        char input[2][64];       /* Rename: [0] title. Tags & category: [0] category, [1] tags */
        size_t field;
        int pick;                /* highlighted status */
        char msg_title[32];
        char msg[200];
    } det;

    struct {
        ed_t ed;             /* the note being previewed; top_line is the scroll position */
        char *buf;
        char title[64];
        char meta[64];
    } pv;

    struct {
        int v[UI_CLOCK_FIELDS];  /* year, month, day, hour, minute */
        int field;
        bool first_run;
    } clk;

    time_t home_minute;      /* minute last drawn on Home */
    bool kbd_seen;
    bool dirty_screen;
} A;

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void hhmm(time_t t, char *out, size_t cap)
{
    if (t <= 0) {
        snprintf(out, cap, "--:--");
        return;
    }
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(out, cap, "%02d:%02d", tm.tm_hour, tm.tm_min);
}

/* ---- notes and navigation -------------------------------------------------------------- */

static void refresh_notes(void)
{
    size_t n = 0;
    if (notes_list(A.notes, MAX_NOTES, &n) != 0) {
        n = 0;
    }
    /* archived notes leave All writing and the Home shortcut; Search still finds them */
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (!A.notes[i].meta.archived) {
            A.notes[k++] = A.notes[i];
        }
    }
    n = k;
    A.nnotes = n;
    if (A.list_sel >= n) {
        A.list_sel = n ? n - 1 : 0;
    }
}

static void list_clamp(void)
{
    if (A.list_sel < A.list_top) {
        A.list_top = A.list_sel;
    } else if (A.list_sel >= A.list_top + UI_LIST_PAGE) {
        A.list_top = A.list_sel - UI_LIST_PAGE + 1;
    }
}

static void go_home(void)
{
    refresh_notes();
    A.state = S_HOME;
    if (A.nnotes == 0 && A.home_sel == 0) {
        A.home_sel = 1;
    }
}

static void open_note(const char *id, bool is_new)
{
    snprintf(A.cur_id, sizeof(A.cur_id), "%s", id);
    A.cur_new = is_new;
    A.saved_at = 0;
    A.save = UI_SAVED;
    A.cur_title[0] = '\0';

    if (is_new) {
        ed_set_text(&A.ed, "", 0);
    } else {
        char *text = NULL;
        size_t len = 0;
        note_meta_t meta;
        notes_meta_load(id, &meta);
        snprintf(A.cur_title, sizeof(A.cur_title), "%s", meta.title);
        if (notes_load(id, &text, &len) == 0 && ed_set_text(&A.ed, text, len)) {
            for (size_t i = 0; i < A.nnotes; i++) {
                if (strcmp(A.notes[i].id, id) == 0) {
                    A.saved_at = A.notes[i].mtime;
                }
            }
        } else {
            ESP_LOGW(TAG, "could not open %s", id);
            ed_set_text(&A.ed, "", 0);
        }
        free(text);
    }
    A.state = S_EDITOR;
}

static void new_note(void)
{
    char id[NOTE_ID_LEN];
    if (notes_new_id(id) != 0) {
        ESP_LOGE(TAG, "no id for a new note");
        return;
    }
    open_note(id, true);
}

/* Writes the editor text. A new note that is still empty is never written. */
static bool save_now(void)
{
    if (A.ed.len == 0 && A.cur_new) {
        A.save = UI_SAVED;
        ed_mark_clean(&A.ed);
        return true;
    }
    if (notes_save(A.cur_id, A.ed.buf, A.ed.len) == 0) {
        if (A.cur_new) {
            /* first save: remember when the note was started (only if the clock has been set) */
            note_meta_t meta;
            notes_meta_load(A.cur_id, &meta);
            const time_t now = time(NULL);
            if (meta.created == 0 && now >= CLOCK_SET_AFTER) {
                meta.created = now;
                notes_meta_save(A.cur_id, &meta);
            }
        }
        A.save = UI_SAVED;
        A.saved_at = time(NULL);
        A.cur_new = false;
        ed_mark_clean(&A.ed);
        return true;
    }
    ESP_LOGE(TAG, "save of %s failed", A.cur_id);
    A.save = UI_SAVE_FAILED;
    return false;
}

static void show_kbd_error(void)
{
    if (A.state == S_KBD_ERR) {
        return;
    }
    if (A.state == S_EDITOR && A.ed.dirty) {
        save_now();
    }
    A.prev_state = A.state;
    A.state = S_KBD_ERR;
}

/* ---- search ---------------------------------------------------------------------------- */

static void edited(void);
static void open_details(const char *id, state_t from);

static void reset_search(size_t page)
{
    A.search.q[0] = '\0';
    A.search.qlen = 0;
    A.search.n = A.search.sel = A.search.top = 0;
    A.search.page = page;
    A.search.pending = false;
}

static void open_search(void)
{
    reset_search(UI_SEARCH_PAGE);
    A.state = S_SEARCH;
}

/* Ctrl+K in the editor. The draft is saved first, so nothing is lost whatever happens next. */
static void open_lookup(void)
{
    if (A.ed.dirty) {
        save_now();
    }
    reset_search(UI_LOOKUP_PAGE);
    A.state = S_LOOKUP;
}

static void run_search(void)
{
    size_t n = 0;
    if (notes_search(A.search.q, A.search.res, MAX_NOTES, &n) != 0) {
        n = 0;
    }
    if (A.state == S_LOOKUP) {
        /* the note being written is not a useful result for itself */
        size_t k = 0;
        for (size_t i = 0; i < n; i++) {
            if (strcmp(A.search.res[i].info.id, A.cur_id) != 0) {
                A.search.res[k++] = A.search.res[i];
            }
        }
        n = k;
    }
    A.search.n = n;
    A.search.sel = A.search.top = 0;
    A.search.pending = false;
}

static void search_clamp(void)
{
    if (A.search.sel < A.search.top) {
        A.search.top = A.search.sel;
    } else if (A.search.sel >= A.search.top + A.search.page) {
        A.search.top = A.search.sel - A.search.page + 1;
    }
}

/* Query editing and result selection, shared by Search and Look up. True if the key was used. */
static bool search_input(const kbd_event_t *ev)
{
    switch (ev->key) {
    case KBD_KEY_CHAR:
        if (!(ev->mods & KBD_MOD_CTRL) && A.search.qlen + 1 < sizeof(A.search.q)) {
            A.search.q[A.search.qlen++] = ev->ch;
            A.search.q[A.search.qlen] = '\0';
            A.search.pending = true;
            A.search.last_key_ms = now_ms();
        }
        return true;
    case KBD_KEY_BACKSPACE:
        if (A.search.qlen > 0) {
            A.search.q[--A.search.qlen] = '\0';
            A.search.pending = true;
            A.search.last_key_ms = now_ms();
        }
        return true;
    case KBD_KEY_UP:
        if (A.search.sel > 0) {
            A.search.sel--;
        }
        search_clamp();
        return true;
    case KBD_KEY_DOWN:
        if (A.search.sel + 1 < A.search.n) {
            A.search.sel++;
        }
        search_clamp();
        return true;
    default:
        return false;
    }
}

static void key_search(const kbd_event_t *ev)
{
    if (search_input(ev)) {
        return;
    }
    switch (ev->key) {
    case KBD_KEY_ENTER:
        if (A.search.pending) {
            run_search();
        }
        if (A.search.n > 0) {
            open_note(A.search.res[A.search.sel].info.id, false);
        }
        break;
    case KBD_KEY_RIGHT:  /* Details, e.g. to unarchive a note Search found */
        if (A.search.pending) {
            run_search();
        }
        if (A.search.n > 0) {
            open_details(A.search.res[A.search.sel].info.id, S_SEARCH);
        }
        break;
    case KBD_KEY_ESC:
        go_home();
        break;
    default:
        break;
    }
}

/* ---- look up while writing --------------------------------------------------------------- */

static void pv_clamp(void)
{
    const size_t n = ed_line_count(&A.pv.ed);
    const size_t max_top = n > UI_PREVIEW_LINES ? n - UI_PREVIEW_LINES : 0;
    if (A.pv.ed.top_line > max_top) {
        A.pv.ed.top_line = max_top;
    }
}

static void open_preview(void)
{
    const note_hit_t *h = &A.search.res[A.search.sel];
    char *text = NULL;
    size_t len = 0;
    if (!A.pv.buf || notes_load(h->info.id, &text, &len) != 0) {
        return;
    }
    const bool fits = ed_set_text(&A.pv.ed, text, len);
    free(text);
    if (!fits) {
        ESP_LOGW(TAG, "%s is too large to preview", h->info.id);
        return;
    }
    char when[16], words[24];
    ui_format_when(h->info.mtime, time(NULL), when, sizeof(when));
    ui_format_words(h->info.words, words, sizeof(words));
    snprintf(A.pv.title, sizeof(A.pv.title), "%s", h->info.title);
    snprintf(A.pv.meta, sizeof(A.pv.meta), "edited %s \xC2\xB7 %s", when, words);

    /* start one line above the first match so it is in view */
    const size_t off = tf_find_nocase(A.pv.ed.buf, A.pv.ed.len, A.search.q, A.search.qlen);
    size_t line = 0;
    if (off != TF_NOT_FOUND) {
        const size_t n = ed_line_count(&A.pv.ed);
        for (size_t i = 0; i < n && ed_line_start(&A.pv.ed, i) <= off; i++) {
            line = i;
        }
    }
    A.pv.ed.top_line = line > 0 ? line - 1 : 0;
    pv_clamp();
    A.state = S_PREVIEW;
}

static void key_lookup(const kbd_event_t *ev)
{
    if (search_input(ev)) {
        return;
    }
    switch (ev->key) {
    case KBD_KEY_ENTER:
        if (A.search.pending) {
            run_search();
        }
        if (A.search.n > 0) {
            open_preview();
        }
        break;
    case KBD_KEY_ESC:
        A.state = S_EDITOR;  /* the draft and its cursor were never touched */
        break;
    default:
        break;
    }
}

/* Puts "[[Title]]" at the cursor of the draft and returns to it. */
static void insert_reference(void)
{
    char ref[sizeof(A.pv.title) + 4];
    const int n = snprintf(ref, sizeof(ref), "[[%s]]", A.pv.title);
    if (n > 0 && ed_insert(&A.ed, ref, (size_t)n)) {
        edited();
    }
    A.state = S_EDITOR;
}

static void key_preview(const kbd_event_t *ev)
{
    size_t *top = &A.pv.ed.top_line;
    switch (ev->key) {
    case KBD_KEY_UP:
        *top = *top > 0 ? *top - 1 : 0;
        break;
    case KBD_KEY_DOWN:
        (*top)++;
        break;
    case KBD_KEY_PGUP:
        *top = *top > UI_PREVIEW_LINES ? *top - UI_PREVIEW_LINES : 0;
        break;
    case KBD_KEY_PGDN:
        *top += UI_PREVIEW_LINES;
        break;
    case KBD_KEY_HOME:
        *top = 0;
        break;
    case KBD_KEY_END:
        *top = (size_t)-1;
        break;
    case KBD_KEY_ENTER:
        if (ev->mods & KBD_MOD_CTRL) {
            insert_reference();
            return;
        }
        break;
    case KBD_KEY_ESC:
        A.state = S_LOOKUP;
        return;
    default:
        break;
    }
    pv_clamp();
}

/* ---- details, rename and the other note actions ----------------------------------------- */

#define DET_ITEMS UI_DETAIL_ITEMS

static void det_reload(void)
{
    if (notes_info(A.det.id, &A.det.info) != 0) {
        return;
    }
    if (A.det.from == S_SEARCH && A.search.sel < A.search.n) {
        A.search.res[A.search.sel].info = A.det.info;  /* the results show the new title and status */
    }
}

static void open_details(const char *id, state_t from)
{
    snprintf(A.det.id, sizeof(A.det.id), "%s", id);
    A.det.from = from;
    A.det.sel = 0;
    A.det.panel = P_NONE;
    det_reload();
    A.state = S_DETAIL;
}

static void det_message(const char *title, const char *text)
{
    snprintf(A.det.msg_title, sizeof(A.det.msg_title), "%s", title);
    snprintf(A.det.msg, sizeof(A.det.msg), "%s", text);
    A.det.panel = P_MSG;
}

/* Saves changed metadata and re-reads the note; tells the user if the card refused it. */
static void det_apply(const note_meta_t *m)
{
    if (notes_meta_save(A.det.id, m) != 0) {
        det_message("Can't write to storage", "The change was not saved. Check the card, then try again.");
        return;
    }
    A.det.panel = P_NONE;
    det_reload();
}

static void trim(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && s[len - 1] == ' ') {
        s[--len] = '\0';
    }
    size_t lead = 0;
    while (s[lead] == ' ') {
        lead++;
    }
    memmove(s, s + lead, len - lead + 1);
}

static void det_action(int item)
{
    note_meta_t m = A.det.info.meta;
    switch (item) {
    case 0:
        open_note(A.det.id, false);
        break;
    case 1:
        snprintf(A.det.input[0], sizeof(A.det.input[0]), "%s", A.det.info.title);
        A.det.panel = P_RENAME;
        break;
    case 2:
        A.det.pick = m.status < NOTE_STATUS_COUNT ? m.status : 0;
        A.det.panel = P_STATUS;
        break;
    case 3:
        snprintf(A.det.input[0], sizeof(A.det.input[0]), "%s", m.category);
        snprintf(A.det.input[1], sizeof(A.det.input[1]), "%s", m.tags);
        A.det.field = 0;
        A.det.panel = P_TAGS;
        break;
    case 4:
        m.flag = !m.flag;
        det_apply(&m);
        break;
    case 5:
        det_message("Export to card", storage_on_sd()
            ? "Your notes are already stored on the microSD card, so there is nothing to export."
            : "No microSD card is in use. Insert a card and restart the device; notes written meanwhile stay in the internal memory.");
        break;
    case 6:
        A.det.panel = P_ARCHIVE;
        break;
    }
}

static void det_text_key(const kbd_event_t *ev, size_t nfields)
{
    static const size_t caps[2][2] = {{63, 0}, {23, 47}};  /* [Rename or Tags][field] */
    const size_t cap = caps[nfields > 1 ? 1 : 0][A.det.field];
    char *in = A.det.input[A.det.field];
    const size_t len = strlen(in);
    if (ev->key == KBD_KEY_CHAR && !(ev->mods & KBD_MOD_CTRL) && (unsigned char)ev->ch >= 0x20 &&
        (unsigned char)ev->ch < 0x7F && len < cap) {
        in[len] = ev->ch;
        in[len + 1] = '\0';
    } else if (ev->key == KBD_KEY_BACKSPACE && len > 0) {
        in[len - 1] = '\0';
    } else if (nfields > 1 && (ev->key == KBD_KEY_UP || ev->key == KBD_KEY_DOWN || ev->key == KBD_KEY_TAB)) {
        A.det.field = (A.det.field + 1) % nfields;
    }
}

static void key_detail(const kbd_event_t *ev)
{
    note_meta_t m = A.det.info.meta;
    switch (A.det.panel) {
    case P_NONE:
        if (ev->key == KBD_KEY_UP && A.det.sel > 0) {
            A.det.sel--;
        } else if (ev->key == KBD_KEY_DOWN && A.det.sel + 1 < DET_ITEMS) {
            A.det.sel++;
        } else if (ev->key == KBD_KEY_ENTER) {
            det_action(A.det.sel);
        } else if (ev->key == KBD_KEY_ESC) {
            if (A.det.from == S_LIST) {
                refresh_notes();
                list_clamp();
            }
            A.state = A.det.from;
        }
        return;
    case P_RENAME:
        if (ev->key == KBD_KEY_ENTER) {
            trim(A.det.input[0]);
            /* empty, or the same as the first line: back to the automatic title */
            const bool auto_title = A.det.input[0][0] == '\0' || strcmp(A.det.input[0], A.det.info.auto_title) == 0;
            snprintf(m.title, sizeof(m.title), "%s", auto_title ? "" : A.det.input[0]);
            det_apply(&m);
        } else if (ev->key == KBD_KEY_ESC) {
            A.det.panel = P_NONE;
        } else {
            det_text_key(ev, 1);
        }
        return;
    case P_TAGS:
        if (ev->key == KBD_KEY_ENTER) {
            trim(A.det.input[0]);
            snprintf(m.category, sizeof(m.category), "%.*s", (int)sizeof(m.category) - 1, A.det.input[0]);
            note_tags_normalize(A.det.input[1], m.tags, sizeof(m.tags));
            det_apply(&m);
        } else if (ev->key == KBD_KEY_ESC) {
            A.det.panel = P_NONE;
        } else {
            det_text_key(ev, 2);
        }
        return;
    case P_STATUS:
        if (ev->key == KBD_KEY_UP && A.det.pick > 0) {
            A.det.pick--;
        } else if (ev->key == KBD_KEY_DOWN && A.det.pick + 1 < NOTE_STATUS_COUNT) {
            A.det.pick++;
        } else if (ev->key == KBD_KEY_ENTER) {
            m.status = (uint8_t)A.det.pick;
            det_apply(&m);
        } else if (ev->key == KBD_KEY_ESC) {
            A.det.panel = P_NONE;
        }
        return;
    case P_ARCHIVE:
        if (ev->key == KBD_KEY_ENTER) {
            m.archived = !m.archived;
            det_apply(&m);
        } else if (ev->key == KBD_KEY_ESC) {
            A.det.panel = P_NONE;
        }
        return;
    case P_MSG:
        if (ev->key == KBD_KEY_ESC || ev->key == KBD_KEY_ENTER) {
            A.det.panel = P_NONE;
        }
        return;
    }
}

/* ---- clock ----------------------------------------------------------------------------- */

#define CLOCK_YEAR_MIN 2025
#define CLOCK_YEAR_MAX 2099

static int days_in_month(int year, int month)
{
    static const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return month == 2 && leap ? 29 : dim[month - 1];
}

/* Compile time, used to pre-fill the screen when the clock has never been set. */
static time_t build_time(void)
{
    static const char *const mon = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char m[4] = {0};
    int d = 1, y = CLOCK_YEAR_MIN, hh = 0, mm = 0;
    sscanf(__DATE__, "%3s %d %d", m, &d, &y);
    sscanf(__TIME__, "%d:%d", &hh, &mm);
    const char *p = strstr(mon, m);
    const int mo = p ? (int)(p - mon) / 3 + 1 : 1;
    return pcf_civil_to_time(y, mo, d, hh, mm, 0);
}

static void open_clock(bool first_run)
{
    time_t t = time(NULL);
    struct tm tm;
    pcf_time_to_civil(t, &tm);
    if (tm.tm_year + 1900 < CLOCK_YEAR_MIN) {
        pcf_time_to_civil(build_time(), &tm);
    }
    A.clk.v[0] = tm.tm_year + 1900;
    A.clk.v[1] = tm.tm_mon + 1;
    A.clk.v[2] = tm.tm_mday;
    A.clk.v[3] = tm.tm_hour;
    A.clk.v[4] = tm.tm_min;
    A.clk.field = 3;  /* the date is usually close; the hour is what drifts */
    A.clk.first_run = first_run;
    A.state = S_CLOCK;
}

static void clock_change(int delta)
{
    static const int lo[UI_CLOCK_FIELDS] = {CLOCK_YEAR_MIN, 1, 1, 0, 0};
    int *v = A.clk.v;
    const int f = A.clk.field;
    int hi = f == 0 ? CLOCK_YEAR_MAX : f == 1 ? 12 : f == 2 ? days_in_month(v[0], v[1]) : f == 3 ? 23 : 59;
    const int span = hi - lo[f] + 1;
    v[f] = lo[f] + ((v[f] - lo[f] + delta) % span + span) % span;
    if (v[2] > days_in_month(v[0], v[1])) {
        v[2] = days_in_month(v[0], v[1]);
    }
}

static void key_clock(const kbd_event_t *ev)
{
    switch (ev->key) {
    case KBD_KEY_LEFT:
        A.clk.field = (A.clk.field + UI_CLOCK_FIELDS - 1) % UI_CLOCK_FIELDS;
        break;
    case KBD_KEY_RIGHT:
    case KBD_KEY_TAB:
        A.clk.field = (A.clk.field + 1) % UI_CLOCK_FIELDS;
        break;
    case KBD_KEY_UP:
        clock_change(1);
        break;
    case KBD_KEY_DOWN:
        clock_change(-1);
        break;
    case KBD_KEY_PGUP:
        clock_change(10);
        break;
    case KBD_KEY_PGDN:
        clock_change(-10);
        break;
    case KBD_KEY_ENTER: {
        const int *v = A.clk.v;
        if (pcf_set(pcf_civil_to_time(v[0], v[1], v[2], v[3], v[4], 0)) != ESP_OK) {
            ESP_LOGW(TAG, "clock set for this session only; the RTC chip did not accept it");
        }
        go_home();
        break;
    }
    case KBD_KEY_ESC:
        go_home();
        break;
    default:
        break;
    }
}

/* ---- rendering ------------------------------------------------------------------------- */

static void render_home(void)
{
    const bool has = A.nnotes > 0;
    char date[40], last[48] = "", sub[40] = "";
    const time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(date, sizeof(date), "%a, %d %b \xC2\xB7 %H:%M", &tm);

    if (has) {
        char when[16];
        ui_format_when(A.notes[0].mtime, now, when, sizeof(when));
        snprintf(sub, sizeof(sub), "edited %s", when);
        if (strchr(when, ':')) {
            snprintf(last, sizeof(last), "Last wrote today at %s", when);
        } else if (strcmp(when, "Yesterday") == 0) {
            snprintf(last, sizeof(last), "Last wrote yesterday");
        } else {
            snprintf(last, sizeof(last), "Last wrote on %s", when);
        }
    }
    ui_home(A.home_sel, has, has ? A.notes[0].title : NULL, has ? sub : NULL, (uint32_t)A.nnotes,
            date, has ? last : NULL);
}

static void render_list(void)
{
    static ui_row_t rows[MAX_NOTES];
    const time_t now = time(NULL);
    for (size_t i = 0; i < A.nnotes; i++) {
        const note_info_t *n = &A.notes[i];
        char words[24];
        snprintf(rows[i].title, sizeof(rows[i].title), "%s", n->title);
        ui_format_when(n->mtime, now, rows[i].when, sizeof(rows[i].when));
        ui_format_words(n->words, words, sizeof(words));
        rows[i].flag = n->meta.flag;
        snprintf(rows[i].sub, sizeof(rows[i].sub), "%s%s%s \xC2\xB7 %s",
                 n->meta.flag ? "Flagged" : note_status_names[n->meta.status % NOTE_STATUS_COUNT],
                 n->meta.tags[0] ? " \xC2\xB7 " : "", n->meta.tags, words);
    }
    ui_list(rows, A.nnotes, A.list_sel, A.list_top);
}

static void fill_hits(ui_hit_t *hits, bool with_status)
{
    const time_t now = time(NULL);
    for (size_t i = 0; i < A.search.n; i++) {
        const note_hit_t *h = &A.search.res[i];
        snprintf(hits[i].title, sizeof(hits[i].title), "%s", h->info.title);
        char when[16];
        ui_format_when(h->info.mtime, now, when, sizeof(when));
        if (!with_status) {
            snprintf(hits[i].when, sizeof(hits[i].when), "%s", when);
        } else if (h->info.meta.flag) {
            snprintf(hits[i].when, sizeof(hits[i].when), "\xC2\xB7 %s", when);  /* the flag shape goes before it */
        } else {
            snprintf(hits[i].when, sizeof(hits[i].when), "%s \xC2\xB7 %s",
                     note_status_names[h->info.meta.status % NOTE_STATUS_COUNT], when);
        }
        hits[i].flag = with_status && h->info.meta.flag;
        snprintf(hits[i].snippet, sizeof(hits[i].snippet), "%s", h->snippet);
        hits[i].hl_off = h->hl_off;
        hits[i].hl_len = h->hl_len;
    }
}

static void render_search(void)
{
    static ui_hit_t hits[MAX_NOTES];
    fill_hits(hits, true);
    ui_search(A.search.q, hits, A.search.n, A.search.sel, A.search.top);
}

static void render_detail(void)
{
    const note_info_t *n = &A.det.info;
    ui_detail_t d;
    memset(&d, 0, sizeof(d));
    snprintf(d.title, sizeof(d.title), "%s", n->title);
    snprintf(d.status, sizeof(d.status), "%s", note_status_names[n->meta.status % NOTE_STATUS_COUNT]);
    snprintf(d.category, sizeof(d.category), "%s", n->meta.category);
    snprintf(d.tags, sizeof(d.tags), "%s", n->meta.tags);
    d.flag = n->meta.flag;
    d.archived = n->meta.archived;
    ui_format_date(n->meta.created, d.created, sizeof(d.created));
    ui_format_stamp(n->mtime, time(NULL), d.edited, sizeof(d.edited));
    ui_format_words(n->words, d.length, sizeof(d.length));
    snprintf(d.id, sizeof(d.id), "%s", n->id);
    ui_details(&d, A.det.sel);

    if (A.det.panel == P_NONE) {
        return;
    }
    ui_dim();
    switch (A.det.panel) {
    case P_RENAME: {
        const ui_field_t f[] = {{"Title", A.det.input[0]}};
        ui_panel_form("Rename", f, 1, 0,
                      "Only the title changes \xE2\x80\x94 the text stays as written. Leave empty to use the first line again.",
                      "Save title");
        break;
    }
    case P_TAGS: {
        const ui_field_t f[] = {{"Category", A.det.input[0]}, {"Tags", A.det.input[1]}};
        ui_panel_form("Tags & category", f, 2, A.det.field, "Separate tags with spaces.", "Save");
        break;
    }
    case P_STATUS:
        ui_panel_pick("Change status", note_status_names, NOTE_STATUS_COUNT, (size_t)A.det.pick);
        break;
    case P_ARCHIVE:
        if (n->meta.archived) {
            ui_panel_message("Unarchive this note?", "It returns to All writing and the Home shortcut.", "Unarchive", "Cancel");
        } else {
            ui_panel_message("Archive this note?",
                             "It leaves All writing and the Home shortcut but stays on the card. Search still finds it, "
                             "and you can unarchive it from Details.",
                             "Archive", "Cancel");
        }
        break;
    case P_MSG:
        ui_panel_message(A.det.msg_title, A.det.msg, NULL, "Close");
        break;
    case P_NONE:
        break;
    }
}

static void render_editor(void)
{
    char title[64], when[16];
    if (A.cur_title[0]) {
        snprintf(title, sizeof(title), "%s", A.cur_title);
    } else {
        ed_title(&A.ed, title, sizeof(title));
    }
    hhmm(A.saved_at, when, sizeof(when));
    const bool failed = A.save == UI_SAVE_FAILED;
    ui_editor(&A.ed, title, A.save, A.cur_new && A.saved_at == 0 ? "" : when,
              failed ? "Can't write to storage" : NULL,
              failed ? "Your writing is still in memory. Don't power off, then press Ctrl+S to retry."
                     : NULL);
}

static void render(void)
{
    switch (A.state) {
    case S_HOME:
        render_home();
        break;
    case S_LIST:
        render_list();
        break;
    case S_EDITOR:
        render_editor();
        break;
    case S_KBD_ERR: {
        char when[16];
        hhmm(A.saved_at, when, sizeof(when));
        ui_kbd_error(when);
        break;
    }
    case S_SEARCH:
        render_search();
        break;
    case S_LOOKUP: {
        static ui_hit_t hits[MAX_NOTES];
        render_editor();  /* the draft stays visible behind the panel */
        ui_dim();
        fill_hits(hits, false);
        ui_lookup(A.search.q, hits, A.search.n, A.search.sel, A.search.top);
        break;
    }
    case S_DETAIL:
        render_detail();
        break;
    case S_PREVIEW:
        ui_preview(A.pv.title, A.search.q, A.search.sel + 1, A.search.n, A.pv.meta, &A.pv.ed);
        break;
    case S_CLOCK:
        ui_clock(A.clk.v[0], A.clk.v[1], A.clk.v[2], A.clk.v[3], A.clk.v[4], A.clk.field,
                 A.clk.first_run);
        break;
    }
    ui_present();
    A.dirty_screen = false;
}

/* ---- key handling ---------------------------------------------------------------------- */

static void key_home(const kbd_event_t *ev)
{
    const int min_sel = A.nnotes > 0 ? 0 : 1;
    if ((ev->mods & KBD_MOD_CTRL) && ev->key == KBD_KEY_CHAR) {
        if (ev->ch == 'n') {
            new_note();
        } else if (ev->ch == 'o') {
            refresh_notes();
            list_clamp();
            A.state = S_LIST;
        } else if (ev->ch == 't') {
            open_clock(false);
        } else if (ev->ch == 'k') {
            open_search();
        }
        return;
    }
    switch (ev->key) {
    case KBD_KEY_UP:
        if (A.home_sel > min_sel) {
            A.home_sel--;
        }
        break;
    case KBD_KEY_DOWN:
        if (A.home_sel < 3) {
            A.home_sel++;
        }
        break;
    case KBD_KEY_ENTER:
        if (A.home_sel == 0 && A.nnotes > 0) {
            open_note(A.notes[0].id, false);
        } else if (A.home_sel == 1) {
            new_note();
        } else if (A.home_sel == 2) {
            list_clamp();
            A.state = S_LIST;
        } else if (A.home_sel == 3) {
            open_search();
        }
        break;
    default:
        break;
    }
}

static void key_list(const kbd_event_t *ev)
{
    switch (ev->key) {
    case KBD_KEY_UP:
        if (A.list_sel > 0) {
            A.list_sel--;
        }
        break;
    case KBD_KEY_DOWN:
        if (A.list_sel + 1 < A.nnotes) {
            A.list_sel++;
        }
        break;
    case KBD_KEY_PGUP:
        A.list_sel = A.list_sel > UI_LIST_PAGE ? A.list_sel - UI_LIST_PAGE : 0;
        break;
    case KBD_KEY_PGDN:
        A.list_sel = A.list_sel + UI_LIST_PAGE < A.nnotes ? A.list_sel + UI_LIST_PAGE
                                                           : (A.nnotes ? A.nnotes - 1 : 0);
        break;
    case KBD_KEY_ENTER:
        if (A.nnotes > 0) {
            open_note(A.notes[A.list_sel].id, false);
        }
        return;
    case KBD_KEY_RIGHT:
        if (A.nnotes > 0) {
            open_details(A.notes[A.list_sel].id, S_LIST);
        }
        return;
    case KBD_KEY_ESC:
        go_home();
        return;
    default:
        break;
    }
    list_clamp();
}

static void edited(void)
{
    A.save = A.ed.dirty ? UI_UNSAVED : A.save;
    A.last_edit_ms = now_ms();
}

/* The clipboard lives only in RAM and is shared by all notes; it is lost at power-off. */
static struct {
    char *text;
    size_t len;
} clip;

static void clip_copy(void)
{
    size_t a, b;
    if (!ed_sel_range(&A.ed, &a, &b)) {
        return;
    }
    char *p = realloc(clip.text, b - a);
    if (!p) {
        return;
    }
    memcpy(p, A.ed.buf + a, b - a);
    clip.text = p;
    clip.len = b - a;
}

static void clip_paste(void)
{
    if (clip.len > 0) {
        ed_insert(&A.ed, clip.text, clip.len);
        edited();
    }
}

/* Movement keys, with Shift extending the selection and Ctrl moving by word or page. */
static bool editor_move(const kbd_event_t *ev)
{
    const bool ctrl = ev->mods & KBD_MOD_CTRL;
    switch (ev->key) {
    case KBD_KEY_LEFT: case KBD_KEY_RIGHT: case KBD_KEY_UP: case KBD_KEY_DOWN:
    case KBD_KEY_HOME: case KBD_KEY_END: case KBD_KEY_PGUP: case KBD_KEY_PGDN:
        break;
    default:
        return false;
    }
    if (ev->mods & KBD_MOD_SHIFT) {
        ed_sel_begin(&A.ed);
    } else {
        ed_sel_clear(&A.ed);
    }
    switch (ev->key) {
    case KBD_KEY_LEFT:
        ctrl ? ed_word_left(&A.ed) : ed_left(&A.ed);
        break;
    case KBD_KEY_RIGHT:
        ctrl ? ed_word_right(&A.ed) : ed_right(&A.ed);
        break;
    case KBD_KEY_UP:
        ctrl ? ed_page_up(&A.ed, UI_EDITOR_LINES - 1) : ed_up(&A.ed);
        break;
    case KBD_KEY_DOWN:
        ctrl ? ed_page_down(&A.ed, UI_EDITOR_LINES - 1) : ed_down(&A.ed);
        break;
    case KBD_KEY_HOME:
        ed_home(&A.ed);
        break;
    case KBD_KEY_END:
        ed_end(&A.ed);
        break;
    case KBD_KEY_PGUP:
        ed_page_up(&A.ed, UI_EDITOR_LINES - 1);
        break;
    default:
        ed_page_down(&A.ed, UI_EDITOR_LINES - 1);
        break;
    }
    return true;
}

static void key_editor(const kbd_event_t *ev)
{
    const bool ctrl = ev->mods & KBD_MOD_CTRL;
    if (ctrl && ev->key == KBD_KEY_CHAR) {
        switch (ev->ch) {
        case 's':
            save_now();
            break;
        case 'k':
            open_lookup();
            break;
        case 'a':
            ed_select_all(&A.ed);
            break;
        case 'c':
            clip_copy();
            break;
        case 'x':
            clip_copy();
            if (ed_delete_selection(&A.ed)) {
                edited();
            }
            break;
        case 'v':
            clip_paste();
            break;
        default:
            break;
        }
        return;
    }
    if (editor_move(ev)) {
        return;
    }

    switch (ev->key) {
    case KBD_KEY_CHAR:
        ed_insert_cp(&A.ed, (uint8_t)ev->ch);
        edited();
        break;
    case KBD_KEY_ENTER:
        ed_insert_cp(&A.ed, '\n');
        edited();
        break;
    case KBD_KEY_BACKSPACE:
        ctrl ? ed_delete_word_back(&A.ed) : ed_backspace(&A.ed);
        edited();
        break;
    case KBD_KEY_DELETE:
        ctrl ? ed_delete_word_fwd(&A.ed) : ed_delete(&A.ed);
        edited();
        break;
    case KBD_KEY_ESC:
        if (A.save == UI_SAVE_FAILED) {
            A.save = UI_UNSAVED;  /* "Keep writing": dismiss the banner */
            return;
        }
        if (A.ed.dirty && !save_now()) {
            return;  /* stay in the editor so the text is not lost */
        }
        go_home();
        return;
    default:
        break;
    }
}

static void handle_key(const kbd_event_t *ev)
{
    if (ev->key == KBD_KEY_CONNECTED) {
        A.kbd_seen = true;
        if (A.state == S_KBD_ERR) {
            A.state = A.prev_state;
        }
        return;
    }
    if (ev->key == KBD_KEY_DISCONNECTED) {
        show_kbd_error();
        return;
    }

    switch (A.state) {
    case S_HOME:
        key_home(ev);
        break;
    case S_LIST:
        key_list(ev);
        break;
    case S_EDITOR:
        key_editor(ev);
        break;
    case S_SEARCH:
        key_search(ev);
        break;
    case S_LOOKUP:
        key_lookup(ev);
        break;
    case S_PREVIEW:
        key_preview(ev);
        break;
    case S_DETAIL:
        key_detail(ev);
        break;
    case S_CLOCK:
        key_clock(ev);
        break;
    case S_KBD_ERR:
        break;  /* waits for the keyboard to come back */
    }
}

static void tick(void)
{
    /* Keep the Home clock current: redraw when the minute changes. A reflective panel holds its
     * image without power, so one refresh a minute costs next to nothing. */
    if (A.state == S_HOME) {
        const time_t m = time(NULL) / 60;
        if (m != A.home_minute) {
            A.home_minute = m;
            A.dirty_screen = true;
        }
    }
    /* Autosave a few seconds after the last edit. */
    if (A.state == S_EDITOR && A.ed.dirty && A.save == UI_UNSAVED &&
        now_ms() - A.last_edit_ms >= AUTOSAVE_MS) {
        save_now();
        A.dirty_screen = true;
    }
    if ((A.state == S_SEARCH || A.state == S_LOOKUP) && A.search.pending &&
        now_ms() - A.search.last_key_ms >= SEARCH_DELAY_MS) {
        run_search();
        A.dirty_screen = true;
    }
#if CONFIG_FT_ENABLE_USB_KBD
    if (!A.kbd_seen && A.state == S_HOME && now_ms() >= KBD_WAIT_MS) {
        show_kbd_error();
        A.dirty_screen = true;
    }
#endif
}

/* ---- entry ----------------------------------------------------------------------------- */

void app_run(void)
{
    char *buf = heap_caps_malloc(BUF_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        buf = malloc(BUF_CAP);
    }
    if (!buf) {
        ESP_LOGE(TAG, "no memory for the editor buffer");
        return;
    }
    ed_init(&A.ed, buf, BUF_CAP, UI_EDITOR_TEXT_W, ui_editor_advance, NULL);

    /* a second buffer for the read-only preview; without it Look up still works but cannot preview */
    A.pv.buf = heap_caps_malloc(BUF_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!A.pv.buf) {
        A.pv.buf = malloc(BUF_CAP);
    }
    if (A.pv.buf) {
        ed_init(&A.pv.ed, A.pv.buf, BUF_CAP, UI_EDITOR_TEXT_W, ui_editor_advance, NULL);
    } else {
        ESP_LOGW(TAG, "no memory for the preview buffer");
    }

    ESP_ERROR_CHECK(gfx_init());
    const st7305_config_t lcd = {
        .host = FT_LCD_SPI_HOST,
        .sclk = FT_LCD_PIN_CLK,
        .mosi = FT_LCD_PIN_MOSI,
        .cs = FT_LCD_PIN_CS,
        .dc = FT_LCD_PIN_DC,
        .rst = FT_LCD_PIN_RST,
        .clock_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(st7305_init(&lcd));

    if (storage_mount() != ESP_OK || notes_init(NOTES_DIR) != 0) {
        ESP_LOGE(TAG, "storage is not available; notes cannot be saved");
    }

#if CONFIG_FT_ENABLE_USB_KBD
    ESP_ERROR_CHECK(usb_kbd_start());
#endif

    const esp_err_t rtc_err = pcf_init();

    go_home();
    if (rtc_err != ESP_OK) {
        open_clock(true);
    }
    render();

    for (;;) {
        kbd_event_t ev;
        if (usb_kbd_next(&ev, pdMS_TO_TICKS(250))) {
            handle_key(&ev);
            A.dirty_screen = true;
        }
        tick();
        if (A.dirty_screen) {
            render();
        }
    }
}
