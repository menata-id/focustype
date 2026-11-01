/* Host tests for the editor core. Build and run with `make -C tests/host`.
 * Glyphs are 10 px wide and lines 100 px wide, so a line holds 10 characters. */

#include <stdio.h>
#include <string.h>

#include "editor.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static int adv10(uint32_t cp, void *ctx)
{
    (void)cp;
    (void)ctx;
    return 10;
}

static char storage[512];

static void fresh(ed_t *e, const char *text)
{
    ed_init(e, storage, sizeof(storage), 100, adv10, NULL);
    if (text) {
        ed_set_text(e, text, strlen(text));
    }
}

static int line_is(ed_t *e, size_t i, const char *want)
{
    const char *s;
    size_t n;
    ed_line(e, i, &s, &n);
    return n == strlen(want) && memcmp(s, want, n) == 0;
}

int main(void)
{
    ed_t e;

    /* empty document */
    fresh(&e, NULL);
    CHECK(ed_line_count(&e) == 1);
    CHECK(e.cursor == 0 && !e.dirty);
    CHECK(ed_cursor_line(&e) == 0 && ed_cursor_x(&e) == 0);
    ed_free(&e);

    /* typing */
    fresh(&e, NULL);
    for (const char *p = "hello"; *p; p++) {
        CHECK(ed_insert_cp(&e, (uint32_t)*p));
    }
    CHECK(e.len == 5 && e.cursor == 5 && e.dirty);
    CHECK(ed_line_count(&e) == 1 && ed_cursor_x(&e) == 50);
    ed_free(&e);

    /* wrap at the last space that fits */
    fresh(&e, "aaaa bbbbbb cc");
    CHECK(ed_line_count(&e) == 2);
    CHECK(line_is(&e, 0, "aaaa ") && line_is(&e, 1, "bbbbbb cc"));
    ed_free(&e);

    /* a word wider than the line is split */
    fresh(&e, "xxxxxxxxxxxxxxxxxxxxxxxxx");  /* 25 */
    CHECK(ed_line_count(&e) == 3);
    CHECK(ed_line_start(&e, 1) == 10 && ed_line_start(&e, 2) == 20);
    ed_free(&e);

    /* hard newlines, including a trailing one that leaves an empty last line */
    fresh(&e, "ab\ncd\n");
    CHECK(ed_line_count(&e) == 3);
    CHECK(line_is(&e, 0, "ab") && line_is(&e, 1, "cd") && line_is(&e, 2, ""));
    CHECK(ed_cursor_line(&e) == 2);
    ed_free(&e);

    /* up/down keep the column */
    fresh(&e, "abcdef\nab\nabcdef");
    CHECK(e.cursor == 16 && ed_cursor_x(&e) == 60);
    ed_up(&e);
    CHECK(e.cursor == 9 && ed_cursor_line(&e) == 1);
    ed_up(&e);
    CHECK(e.cursor == 6 && ed_cursor_line(&e) == 0);
    ed_down(&e);
    CHECK(e.cursor == 9);
    ed_down(&e);
    CHECK(e.cursor == 16);
    ed_down(&e);  /* already on the last line */
    CHECK(e.cursor == 16);
    ed_free(&e);

    /* home/end and vertical movement on a wrapped line */
    fresh(&e, "aaaa bbbbbb cc");
    CHECK(ed_cursor_line(&e) == 1);
    ed_home(&e);
    CHECK(e.cursor == 5);
    ed_end(&e);
    CHECK(e.cursor == 14);
    ed_up(&e);  /* x = 90 -> clamps before the line's trailing space */
    CHECK(e.cursor == 4 && ed_cursor_line(&e) == 0);
    ed_end(&e);
    CHECK(e.cursor == 4);
    ed_down(&e);
    CHECK(e.cursor == 9 && ed_cursor_line(&e) == 1);
    ed_free(&e);

    /* UTF-8: the cursor and deletes work on whole codepoints */
    fresh(&e, NULL);
    ed_insert_cp(&e, 'a');
    ed_insert_cp(&e, 0xE9);  /* é, two bytes */
    ed_insert_cp(&e, 'b');
    CHECK(e.len == 4 && e.cursor == 4);
    ed_left(&e);
    CHECK(e.cursor == 3);
    ed_left(&e);
    CHECK(e.cursor == 1);
    ed_right(&e);
    CHECK(e.cursor == 3);
    ed_delete(&e);  /* removes 'b' */
    CHECK(e.len == 3);
    ed_backspace(&e);  /* removes both bytes of é */
    CHECK(e.len == 1 && e.cursor == 1 && strcmp(e.buf, "a") == 0);
    ed_free(&e);

    /* edges are no-ops */
    fresh(&e, "x");
    ed_home(&e);
    ed_backspace(&e);
    CHECK(e.len == 1);
    ed_end(&e);
    ed_delete(&e);
    CHECK(e.len == 1);
    ed_left(&e);
    ed_left(&e);
    CHECK(e.cursor == 0);
    ed_free(&e);

    /* insert in the middle */
    fresh(&e, "ac");
    ed_left(&e);
    ed_insert_cp(&e, 'b');
    CHECK(strcmp(e.buf, "abc") == 0 && e.cursor == 2);
    ed_free(&e);

    /* capacity: 8 bytes holds 7 characters plus the NUL */
    char small[8];
    ed_init(&e, small, sizeof(small), 100, adv10, NULL);
    CHECK(ed_insert(&e, "1234567", 7));
    CHECK(!ed_insert_cp(&e, 'x'));
    CHECK(e.len == 7 && strcmp(small, "1234567") == 0);
    CHECK(!ed_set_text(&e, "12345678", 8));
    ed_free(&e);

    /* titles */
    char title[32];
    fresh(&e, "  \n  Hello world  \nbody");
    CHECK(ed_title(&e, title, sizeof(title)) == 11 && strcmp(title, "Hello world") == 0);
    ed_free(&e);
    fresh(&e, "   \n ");
    CHECK(strcmp((ed_title(&e, title, sizeof(title)), title), "Untitled") == 0);
    ed_free(&e);
    fresh(&e, "\xC3\xA9\xC3\xA9\xC3\xA9");  /* ééé */
    CHECK(ed_title(&e, title, 4) == 2);  /* room for 3 bytes: only one whole é fits */
    CHECK(strcmp(title, "\xC3\xA9") == 0);
    ed_free(&e);

    /* word count */
    fresh(&e, "hello  world\nfoo");
    CHECK(ed_word_count(&e) == 3);
    ed_free(&e);

    /* scrolling follows the cursor */
    char many[64];
    many[0] = '\0';
    for (int i = 0; i < 29; i++) {
        strcat(many, "x\n");
    }
    fresh(&e, many);
    CHECK(ed_line_count(&e) == 30);
    ed_scroll_to_cursor(&e, 5);
    CHECK(e.top_line == 25);
    ed_page_up(&e, 100);
    CHECK(e.cursor == 0);
    ed_scroll_to_cursor(&e, 5);
    CHECK(e.top_line == 0);
    ed_free(&e);

    /* word movement, word deletion, selection */
    fresh(&e, "one two  three");
    ed_word_left(&e);
    CHECK(e.cursor == 9);
    ed_word_left(&e);
    CHECK(e.cursor == 4);
    ed_word_right(&e);
    CHECK(e.cursor == 9);
    ed_word_right(&e);
    ed_word_right(&e);
    CHECK(e.cursor == e.len);
    ed_delete_word_back(&e);
    CHECK(strcmp(e.buf, "one two  ") == 0);
    e.cursor = 0;
    ed_delete_word_fwd(&e);
    CHECK(strcmp(e.buf, "two  ") == 0 && e.cursor == 0);
    ed_free(&e);

    fresh(&e, "hello world");
    e.cursor = 5;
    ed_sel_begin(&e);
    ed_word_right(&e);
    ed_word_right(&e);
    size_t a, b;
    CHECK(ed_sel_range(&e, &a, &b) && a == 5 && b == 11);
    ed_insert_cp(&e, 'X');  /* typing replaces the selection */
    CHECK(strcmp(e.buf, "helloX") == 0 && !e.sel);
    ed_select_all(&e);
    CHECK(ed_sel_range(&e, &a, &b) && a == 0 && b == 6);
    ed_backspace(&e);
    CHECK(e.len == 0 && e.cursor == 0);
    ed_free(&e);

    printf(failures ? "test_editor: %d failure(s)\n" : "test_editor: ok\n", failures);
    return failures != 0;
}
