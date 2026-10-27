#pragma once

#include <stddef.h>

/* Plain POSIX file helpers, no ESP-IDF dependency (host-testable). */

#define FS_OK         0
#define FS_ERR       (-1)
#define FS_NOT_FOUND  1

/* Writes `len` bytes to `path` so a power cut never leaves a half-written file behind:
 * data goes to "<path>.tmp", is synced, then replaces `path`. If power fails between the
 * unlink and the rename, "<path>.tmp" holds the complete new content and fs_load() falls
 * back to it. */
int fs_save_atomic(const char *path, const void *data, size_t len);

/* Reads a whole file into a malloc'd, NUL-terminated buffer (caller frees). Falls back to
 * "<path>.tmp". Returns FS_NOT_FOUND if neither exists. */
int fs_load(const char *path, char **data, size_t *len);
