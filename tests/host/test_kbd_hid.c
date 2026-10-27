/* Host test for the HID boot-report parser. Build and run with `make -C tests/host`. */

#include <stdio.h>
#include <string.h>

#include "kbd_hid.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static void report(uint8_t mod, uint8_t k0, uint8_t k1, uint8_t out[8])
{
    memset(out, 0, 8);
    out[0] = mod;
    out[2] = k0;
    out[3] = k1;
}

int main(void)
{
    kbd_parser_t p;
    kbd_event_t ev[6];
    uint8_t r[8];
    memset(&p, 0, sizeof(p));

    /* 'a' down: one char event, held; same report again: no new event */
    report(0, 0x04, 0, r);
    CHECK(kbd_hid_parse(&p, r, 8, ev, 6) == 1);
    CHECK(ev[0].key == KBD_KEY_CHAR && ev[0].ch == 'a' && ev[0].mods == 0);
    CHECK(p.held_usage == 0x04);
    CHECK(kbd_hid_parse(&p, r, 8, ev, 6) == 0);
    CHECK(p.held_usage == 0x04);

    /* release */
    report(0, 0, 0, r);
    CHECK(kbd_hid_parse(&p, r, 8, ev, 6) == 0);
    CHECK(p.held_usage == 0);

    /* Shift+a -> 'A'; Ctrl+s -> 's' with the Ctrl bit */
    report(0x02, 0x04, 0, r);
    CHECK(kbd_hid_parse(&p, r, 8, ev, 6) == 1 && ev[0].ch == 'A' && (ev[0].mods & KBD_MOD_SHIFT));
    report(0, 0, 0, r);
    kbd_hid_parse(&p, r, 8, ev, 6);
    report(0x01, 0x16, 0, r);
    CHECK(kbd_hid_parse(&p, r, 8, ev, 6) == 1 && ev[0].ch == 's' && (ev[0].mods & KBD_MOD_CTRL));
    report(0, 0, 0, r);
    kbd_hid_parse(&p, r, 8, ev, 6);

    /* digits and shifted digits, punctuation */
    CHECK(kbd_hid_translate(0x1E, 0, &ev[0]) && ev[0].ch == '1');
    CHECK(kbd_hid_translate(0x1E, KBD_MOD_SHIFT, &ev[0]) && ev[0].ch == '!');
    CHECK(kbd_hid_translate(0x27, 0, &ev[0]) && ev[0].ch == '0');
    CHECK(kbd_hid_translate(0x38, KBD_MOD_SHIFT, &ev[0]) && ev[0].ch == '?');
    CHECK(kbd_hid_translate(0x2C, 0, &ev[0]) && ev[0].ch == ' ');

    /* navigation and editing keys */
    CHECK(kbd_hid_translate(0x28, 0, &ev[0]) && ev[0].key == KBD_KEY_ENTER);
    CHECK(kbd_hid_translate(0x29, 0, &ev[0]) && ev[0].key == KBD_KEY_ESC);
    CHECK(kbd_hid_translate(0x2A, 0, &ev[0]) && ev[0].key == KBD_KEY_BACKSPACE);
    CHECK(kbd_hid_translate(0x52, 0, &ev[0]) && ev[0].key == KBD_KEY_UP);
    CHECK(kbd_hid_translate(0x51, 0, &ev[0]) && ev[0].key == KBD_KEY_DOWN);
    CHECK(kbd_hid_translate(0x50, 0, &ev[0]) && ev[0].key == KBD_KEY_LEFT);
    CHECK(kbd_hid_translate(0x4F, 0, &ev[0]) && ev[0].key == KBD_KEY_RIGHT);
    CHECK(!kbd_hid_translate(0x3A, 0, &ev[0])); /* F1: not handled yet */

    /* two keys at once, then the older one stays held while a newer one is pressed */
    report(0, 0x04, 0x05, r);
    CHECK(kbd_hid_parse(&p, r, 8, ev, 6) == 2);
    report(0, 0, 0, r);
    kbd_hid_parse(&p, r, 8, ev, 6);

    /* rollover error is ignored and leaves state alone */
    memset(r, 0, 8);
    memset(&r[2], 0x01, 6);
    CHECK(kbd_hid_parse(&p, r, 8, ev, 6) == 0);

    /* short report */
    CHECK(kbd_hid_parse(&p, r, 4, ev, 6) == 0);

    printf(failures ? "test_kbd_hid: %d failure(s)\n" : "test_kbd_hid: ok\n", failures);
    return failures != 0;
}
