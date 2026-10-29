#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/*
 * Per-note metadata kept beside the text in <id>.meta, so the .txt is never touched by it.
 * Plain "key=value" lines; unknown keys are ignored and missing ones keep their defaults, so the
 * format can grow. No filesystem or ESP-IDF code here (host-tested).
 */

#define NOTE_STATUS_COUNT 4
extern const char *const note_status_names[NOTE_STATUS_COUNT];  /* Draft, Developing, Revising, Done */

typedef struct {
    char title[64];      /* manual title; empty = use the first line of the text */
    uint8_t status;      /* index into note_status_names */
    char category[24];
    char tags[48];       /* "#community #idea", see note_tags_normalize() */
    bool flag;
    bool archived;
    time_t created;      /* 0 = unknown (notes written before metadata existed) */
} note_meta_t;

void note_meta_defaults(note_meta_t *m);

/* Fills `m` from `text` (len bytes). Starts from the defaults. Never fails; garbage lines are skipped. */
void note_meta_parse(const char *text, size_t len, note_meta_t *m);

/* Writes the file text. Returns the length (without the NUL), or 0 if `cap` is too small. */
size_t note_meta_format(const note_meta_t *m, char *out, size_t cap);

/* "foo, #Bar  baz" -> "#foo #Bar #baz": words split on spaces and commas, one '#' each, no
 * duplicates (ASCII case-insensitive), cut to what fits in `cap` without splitting a tag. */
void note_tags_normalize(const char *in, char *out, size_t cap);
