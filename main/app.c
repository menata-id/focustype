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
#include "ui.h"
#include "usb_kbd.h"

static const char *TAG = "app";

#define BUF_CAP        (64 * 1024)
#define MAX_NOTES      64
#define AUTOSAVE_MS    3000
#define KBD_WAIT_MS    5000
#define SEARCH_DELAY_MS 250   /* wait after the last keystroke before scanning the notes */
#define QUERY_MAX      48
#define NOTES_DIR      STORAGE_MOUNT_POINT "/notes"

typedef enum { S_HOME, S_LIST, S_EDITOR, S_KBD_ERR, S_CLOCK, S_SEARCH } state_t;

static struct {
    state_t state, prev_state;
    int home_sel;

    note_info_t notes[MAX_NOTES];
    size_t nnotes;
    size_t list_sel, list_top;

    ed_t ed;
    char cur_id[NOTE_ID_LEN];
    bool cur_new;
    ui_save_t save;
    time_t saved_at;
    int64_t last_edit_ms;

    struct {
        char q[QUERY_MAX];
        size_t qlen;
        note_info_t res[MAX_NOTES];
        size_t n, sel, top;
        bool pending;        /* the query changed and the results are stale */
        int64_t last_key_ms;
    } search;

    struct {
        int v[UI_CLOCK_FIELDS];  /* year, month, day, hour, minute */
        int field;
        bool first_run;
    } clk;

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

    if (is_new) {
        ed_set_text(&A.ed, "", 0);
    } else {
        char *text = NULL;
        size_t len = 0;
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

static void open_search(void)
{
    A.search.q[0] = '\0';
    A.search.qlen = 0;
    A.search.n = A.search.sel = A.search.top = 0;
    A.search.pending = false;
    A.state = S_SEARCH;
}

static void run_search(void)
{
    size_t n = 0;
    if (notes_search(A.search.q, A.search.res, MAX_NOTES, &n) != 0) {
        n = 0;
    }
    A.search.n = n;
    A.search.sel = A.search.top = 0;
    A.search.pending = false;
}

static void search_clamp(void)
{
    if (A.search.sel < A.search.top) {
        A.search.top = A.search.sel;
    } else if (A.search.sel >= A.search.top + UI_SEARCH_PAGE) {
        A.search.top = A.search.sel - UI_SEARCH_PAGE + 1;
    }
}

static void key_search(const kbd_event_t *ev)
{
    switch (ev->key) {
    case KBD_KEY_CHAR:
        if (!(ev->mods & KBD_MOD_CTRL) && A.search.qlen + 1 < sizeof(A.search.q)) {
            A.search.q[A.search.qlen++] = ev->ch;
            A.search.q[A.search.qlen] = '\0';
            A.search.pending = true;
            A.search.last_key_ms = now_ms();
        }
        break;
    case KBD_KEY_BACKSPACE:
        if (A.search.qlen > 0) {
            A.search.q[--A.search.qlen] = '\0';
            A.search.pending = true;
            A.search.last_key_ms = now_ms();
        }
        break;
    case KBD_KEY_UP:
        if (A.search.sel > 0) {
            A.search.sel--;
        }
        break;
    case KBD_KEY_DOWN:
        if (A.search.sel + 1 < A.search.n) {
            A.search.sel++;
        }
        break;
    case KBD_KEY_ENTER:
        if (A.search.pending) {
            run_search();
        }
        if (A.search.n > 0) {
            open_note(A.search.res[A.search.sel].id, false);
        }
        return;
    case KBD_KEY_ESC:
        go_home();
        return;
    default:
        break;
    }
    search_clamp();
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
        snprintf(rows[i].title, sizeof(rows[i].title), "%s", A.notes[i].title);
        ui_format_when(A.notes[i].mtime, now, rows[i].when, sizeof(rows[i].when));
        ui_format_words(A.notes[i].words, rows[i].sub, sizeof(rows[i].sub));
    }
    ui_list(rows, A.nnotes, A.list_sel, A.list_top);
}

static void render_search(void)
{
    static ui_row_t rows[MAX_NOTES];
    const time_t now = time(NULL);
    for (size_t i = 0; i < A.search.n; i++) {
        snprintf(rows[i].title, sizeof(rows[i].title), "%s", A.search.res[i].title);
        ui_format_when(A.search.res[i].mtime, now, rows[i].when, sizeof(rows[i].when));
        ui_format_words(A.search.res[i].words, rows[i].sub, sizeof(rows[i].sub));
    }
    ui_search(A.search.q, rows, A.search.n, A.search.sel, A.search.top);
}

static void render_editor(void)
{
    char title[64], when[16];
    ed_title(&A.ed, title, sizeof(title));
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

static void key_editor(const kbd_event_t *ev)
{
    if ((ev->mods & KBD_MOD_CTRL) && ev->key == KBD_KEY_CHAR) {
        if (ev->ch == 's') {
            save_now();
        }
        return;  /* Ctrl+K look-up is not built yet */
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
        ed_backspace(&A.ed);
        edited();
        break;
    case KBD_KEY_DELETE:
        ed_delete(&A.ed);
        edited();
        break;
    case KBD_KEY_LEFT:
        ed_left(&A.ed);
        break;
    case KBD_KEY_RIGHT:
        ed_right(&A.ed);
        break;
    case KBD_KEY_UP:
        ed_up(&A.ed);
        break;
    case KBD_KEY_DOWN:
        ed_down(&A.ed);
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
    case KBD_KEY_PGDN:
        ed_page_down(&A.ed, UI_EDITOR_LINES - 1);
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
    case S_CLOCK:
        key_clock(ev);
        break;
    case S_KBD_ERR:
        break;  /* waits for the keyboard to come back */
    }
}

static void tick(void)
{
    /* Autosave a few seconds after the last edit. */
    if (A.state == S_EDITOR && A.ed.dirty && A.save == UI_UNSAVED &&
        now_ms() - A.last_edit_ms >= AUTOSAVE_MS) {
        save_now();
        A.dirty_screen = true;
    }
    if (A.state == S_SEARCH && A.search.pending &&
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
