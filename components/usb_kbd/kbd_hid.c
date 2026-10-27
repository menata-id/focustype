#include <string.h>

#include "kbd_hid.h"

static const char k_digits[]       = "1234567890";
static const char k_digits_shift[] = "!@#$%^&*()";

static uint8_t mods_from_hid(uint8_t m)
{
    uint8_t r = 0;
    if (m & 0x11) r |= KBD_MOD_CTRL;
    if (m & 0x22) r |= KBD_MOD_SHIFT;
    if (m & 0x44) r |= KBD_MOD_ALT;
    if (m & 0x88) r |= KBD_MOD_GUI;
    return r;
}

static bool set_char(kbd_event_t *ev, char c, uint8_t mods)
{
    ev->key = KBD_KEY_CHAR;
    ev->ch = c;
    ev->mods = mods;
    return true;
}

bool kbd_hid_translate(uint8_t usage, uint8_t mods, kbd_event_t *ev)
{
    const bool shift = mods & KBD_MOD_SHIFT;
    memset(ev, 0, sizeof(*ev));
    ev->mods = mods;

    if (usage >= 0x04 && usage <= 0x1D) {
        char c = (char)('a' + (usage - 0x04));
        if (shift && !(mods & KBD_MOD_CTRL)) {
            c = (char)(c - 'a' + 'A');
        }
        return set_char(ev, c, mods);
    }
    if (usage >= 0x1E && usage <= 0x27) {
        const int i = usage - 0x1E;
        return set_char(ev, shift ? k_digits_shift[i] : k_digits[i], mods);
    }

    switch (usage) {
    case 0x28: case 0x58: ev->key = KBD_KEY_ENTER; return true;
    case 0x29: ev->key = KBD_KEY_ESC; return true;
    case 0x2A: ev->key = KBD_KEY_BACKSPACE; return true;
    case 0x2B: ev->key = KBD_KEY_TAB; return true;
    case 0x2C: return set_char(ev, ' ', mods);
    case 0x4A: ev->key = KBD_KEY_HOME; return true;
    case 0x4B: ev->key = KBD_KEY_PGUP; return true;
    case 0x4C: ev->key = KBD_KEY_DELETE; return true;
    case 0x4D: ev->key = KBD_KEY_END; return true;
    case 0x4E: ev->key = KBD_KEY_PGDN; return true;
    case 0x4F: ev->key = KBD_KEY_RIGHT; return true;
    case 0x50: ev->key = KBD_KEY_LEFT; return true;
    case 0x51: ev->key = KBD_KEY_DOWN; return true;
    case 0x52: ev->key = KBD_KEY_UP; return true;
    default: break;
    }

    /* 0x2D '-', 0x2E '=', 0x2F '[', 0x30 ']', 0x31 '\\', 0x33 ';', 0x34 '\'', 0x35 '`',
     * 0x36 ',', 0x37 '.', 0x38 '/'. 0x32 is the non-US '#', which we skip. */
    static const struct { uint8_t usage; char base; char shifted; } k_map[] = {
        {0x2D, '-', '_'}, {0x2E, '=', '+'}, {0x2F, '[', '{'}, {0x30, ']', '}'},
        {0x31, '\\', '|'}, {0x33, ';', ':'}, {0x34, '\'', '"'}, {0x35, '`', '~'},
        {0x36, ',', '<'}, {0x37, '.', '>'}, {0x38, '/', '?'},
    };
    for (size_t i = 0; i < sizeof(k_map) / sizeof(k_map[0]); i++) {
        if (k_map[i].usage == usage) {
            return set_char(ev, shift ? k_map[i].shifted : k_map[i].base, mods);
        }
    }
    return false;
}

size_t kbd_hid_parse(kbd_parser_t *p, const uint8_t *report, size_t len,
                     kbd_event_t *out, size_t max_out)
{
    if (len < 8) {
        return 0;
    }
    const uint8_t mods = mods_from_hid(report[0]);
    const uint8_t *keys = &report[2];

    /* 0x01 in every slot = rollover error (too many keys): ignore the report. */
    if (keys[0] == 0x01) {
        return 0;
    }

    size_t n = 0;
    uint8_t held = 0;
    for (int i = 0; i < 6; i++) {
        const uint8_t u = keys[i];
        if (u == 0) {
            continue;
        }
        bool was_down = false;
        for (int j = 0; j < 6; j++) {
            if (p->prev[j] == u) {
                was_down = true;
                break;
            }
        }
        if (!was_down) {
            held = u;
            if (n < max_out && kbd_hid_translate(u, mods, &out[n])) {
                n++;
            }
        } else if (held == 0 && u == p->held_usage) {
            held = u;
        }
    }

    memcpy(p->prev, keys, 6);
    p->held_usage = held;
    p->held_mods = mods;
    return n;
}
