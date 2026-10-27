#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* HID boot-protocol keyboard report -> key events. No ESP-IDF dependencies, so it can be
 * unit-tested on the host (tests/host). */

typedef enum {
    KBD_KEY_NONE = 0,
    KBD_KEY_CHAR,        /* printable; see kbd_event_t.ch */
    KBD_KEY_ENTER,
    KBD_KEY_ESC,
    KBD_KEY_BACKSPACE,
    KBD_KEY_DELETE,
    KBD_KEY_TAB,
    KBD_KEY_UP,
    KBD_KEY_DOWN,
    KBD_KEY_LEFT,
    KBD_KEY_RIGHT,
    KBD_KEY_HOME,
    KBD_KEY_END,
    KBD_KEY_PGUP,
    KBD_KEY_PGDN,
    KBD_KEY_CONNECTED,   /* synthetic: a keyboard appeared */
    KBD_KEY_DISCONNECTED /* synthetic: the last keyboard went away */
} kbd_key_t;

#define KBD_MOD_CTRL   0x01
#define KBD_MOD_SHIFT  0x02
#define KBD_MOD_ALT    0x04
#define KBD_MOD_GUI    0x08

typedef struct {
    kbd_key_t key;
    char ch;        /* KBD_KEY_CHAR only. With Ctrl held this is the unshifted letter (e.g. 's'). */
    uint8_t mods;   /* KBD_MOD_* */
} kbd_event_t;

typedef struct {
    uint8_t prev[6];     /* usages down in the previous report */
    uint8_t held_usage;  /* newest usage still down, 0 if none (drives key repeat) */
    uint8_t held_mods;
} kbd_parser_t;

/* Translates one HID usage plus modifiers. Returns false for usages we do not handle. */
bool kbd_hid_translate(uint8_t usage, uint8_t mods, kbd_event_t *ev);

/* Parses an 8-byte boot report and writes events for newly pressed keys (up to max_out).
 * Returns the number of events written. A rollover-error report is ignored. */
size_t kbd_hid_parse(kbd_parser_t *p, const uint8_t *report, size_t len,
                     kbd_event_t *out, size_t max_out);
