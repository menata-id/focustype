#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "note_meta.h"

const char *const note_status_names[NOTE_STATUS_COUNT] = {"Draft", "Developing", "Revising", "Done"};

void note_meta_defaults(note_meta_t *m)
{
    memset(m, 0, sizeof(*m));
}

static void copy_value(char *dst, size_t cap, const char *src, size_t len)
{
    if (len >= cap) {
        len = cap - 1;
        /* do not leave half a UTF-8 character at the end */
        while (len > 0 && ((unsigned char)src[len] & 0xC0) == 0x80) {
            len--;
        }
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

void note_meta_parse(const char *text, size_t len, note_meta_t *m)
{
    note_meta_defaults(m);
    size_t i = 0;
    while (i < len) {
        size_t end = i;
        while (end < len && text[end] != '\n') {
            end++;
        }
        size_t vend = end;
        if (vend > i && text[vend - 1] == '\r') {
            vend--;
        }
        const char *eq = memchr(text + i, '=', vend - i);
        if (eq) {
            const size_t klen = (size_t)(eq - (text + i));
            const char *v = eq + 1;
            const size_t vlen = (size_t)(text + vend - v);
            if (klen == 5 && strncmp(text + i, "title", 5) == 0) {
                copy_value(m->title, sizeof(m->title), v, vlen);
            } else if (klen == 6 && strncmp(text + i, "status", 6) == 0) {
                for (uint8_t s = 0; s < NOTE_STATUS_COUNT; s++) {
                    if (strlen(note_status_names[s]) == vlen && strncmp(note_status_names[s], v, vlen) == 0) {
                        m->status = s;
                    }
                }
            } else if (klen == 8 && strncmp(text + i, "category", 8) == 0) {
                copy_value(m->category, sizeof(m->category), v, vlen);
            } else if (klen == 4 && strncmp(text + i, "tags", 4) == 0) {
                copy_value(m->tags, sizeof(m->tags), v, vlen);
            } else if (klen == 4 && strncmp(text + i, "flag", 4) == 0) {
                m->flag = vlen > 0 && v[0] == '1';
            } else if (klen == 8 && strncmp(text + i, "archived", 8) == 0) {
                m->archived = vlen > 0 && v[0] == '1';
            } else if (klen == 7 && strncmp(text + i, "created", 7) == 0) {
                char num[24];
                copy_value(num, sizeof(num), v, vlen);
                m->created = (time_t)strtoll(num, NULL, 10);
            }
        }
        i = end + 1;
    }
}

/* Values are one line: a newline would start a new key. */
static void one_line(char *dst, size_t cap, const char *src)
{
    size_t o = 0;
    for (; *src && o + 1 < cap; src++) {
        dst[o++] = (*src == '\n' || *src == '\r') ? ' ' : *src;
    }
    dst[o] = '\0';
}

size_t note_meta_format(const note_meta_t *m, char *out, size_t cap)
{
    char title[sizeof(m->title)], category[sizeof(m->category)], tags[sizeof(m->tags)];
    one_line(title, sizeof(title), m->title);
    one_line(category, sizeof(category), m->category);
    one_line(tags, sizeof(tags), m->tags);
    const uint8_t status = m->status < NOTE_STATUS_COUNT ? m->status : 0;
    const int n = snprintf(out, cap,
                           "title=%s\nstatus=%s\ncategory=%s\ntags=%s\nflag=%d\narchived=%d\ncreated=%lld\n",
                           title, note_status_names[status], category, tags, m->flag ? 1 : 0,
                           m->archived ? 1 : 0, (long long)m->created);
    return n > 0 && (size_t)n < cap ? (size_t)n : 0;
}

void note_tags_normalize(const char *in, char *out, size_t cap)
{
    size_t o = 0;
    out[0] = '\0';
    const char *p = in;
    while (*p) {
        while (*p == ' ' || *p == ',' || *p == '\t' || *p == '#') {
            p++;
        }
        const char *start = p;
        while (*p && *p != ' ' && *p != ',' && *p != '\t') {
            p++;
        }
        const size_t len = (size_t)(p - start);
        if (len == 0) {
            continue;
        }
        /* skip a tag already present */
        bool dup = false;
        for (size_t i = 0; i < o && !dup; i++) {
            if (out[i] == '#' && (i == 0 || out[i - 1] == ' ')) {
                size_t j = 0;
                while (j < len && out[i + 1 + j] && tolower((unsigned char)out[i + 1 + j]) == tolower((unsigned char)start[j])) {
                    j++;
                }
                dup = j == len && (out[i + 1 + j] == ' ' || out[i + 1 + j] == '\0');
            }
        }
        if (dup) {
            continue;
        }
        const size_t need = (o ? 1 : 0) + 1 + len;
        if (o + need + 1 > cap) {
            break;
        }
        if (o) {
            out[o++] = ' ';
        }
        out[o++] = '#';
        memcpy(out + o, start, len);
        o += len;
        out[o] = '\0';
    }
}
