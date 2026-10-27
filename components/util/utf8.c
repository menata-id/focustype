#include "utf8.h"

size_t utf8_next(const char *s, size_t len, uint32_t *cp)
{
    const uint8_t *p = (const uint8_t *)s;
    if (len == 0) {
        *cp = 0;
        return 0;
    }
    const uint8_t b = p[0];
    size_t n;
    uint32_t c;
    if (b < 0x80) {
        *cp = b;
        return 1;
    } else if ((b & 0xE0) == 0xC0) {
        n = 2;
        c = b & 0x1F;
    } else if ((b & 0xF0) == 0xE0) {
        n = 3;
        c = b & 0x0F;
    } else if ((b & 0xF8) == 0xF0) {
        n = 4;
        c = b & 0x07;
    } else {
        *cp = 0xFFFD;
        return 1;
    }
    if (len < n) {
        *cp = 0xFFFD;
        return 1;
    }
    for (size_t i = 1; i < n; i++) {
        if ((p[i] & 0xC0) != 0x80) {
            *cp = 0xFFFD;
            return 1;
        }
        c = (c << 6) | (p[i] & 0x3F);
    }
    *cp = c;
    return n;
}

size_t utf8_encode(uint32_t cp, char out[4])
{
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        cp = 0xFFFD;
    }
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}
