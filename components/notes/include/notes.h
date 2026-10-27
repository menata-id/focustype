#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/*
 * Notes stored as plain UTF-8 files, one per note: <dir>/ft-0001.txt. Titles, word counts and
 * dates are derived from the files, so there is no index to corrupt. Saves are atomic
 * (see fsutil). POSIX only, so it is host-testable.
 */

#define NOTE_ID_LEN 12

typedef struct {
    char id[NOTE_ID_LEN];   /* "ft-0001" */
    char title[64];
    uint32_t words;
    time_t mtime;
} note_info_t;

/* Sets the directory (created if missing). Returns 0 on success. */
int notes_init(const char *dir);

/* Lists notes, newest first. Returns 0 on success; *count is the number written (<= max). */
int notes_list(note_info_t *out, size_t max, size_t *count);

/* Allocates the next free id. The file is created by the first notes_save(). */
int notes_new_id(char id[NOTE_ID_LEN]);

/* Loads a note into a malloc'd NUL-terminated buffer (caller frees). Returns 1 if missing. */
int notes_load(const char *id, char **text, size_t *len);

/* Atomically saves a note. Returns 0 on success. */
int notes_save(const char *id, const char *text, size_t len);

int notes_remove(const char *id);
