#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fsutil.h"

#define PATH_MAX_LEN 160

int fs_save_atomic(const char *path, const void *data, size_t len)
{
    char tmp[PATH_MAX_LEN];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) {
        return FS_ERR;
    }

    FILE *f = fopen(tmp, "wb");
    if (!f) {
        return FS_ERR;
    }
    const size_t written = len ? fwrite(data, 1, len, f) : 0;
    const int flushed = fflush(f);
    const int synced = fsync(fileno(f));
    const int closed = fclose(f);
    if (written != len || flushed != 0 || synced != 0 || closed != 0) {
        return FS_ERR;
    }

    /* FAT cannot rename over an existing file; remove the old copy first. */
    unlink(path);
    return rename(tmp, path) == 0 ? FS_OK : FS_ERR;
}

static int read_file(const char *path, char **data, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return FS_NOT_FOUND;
    }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return FS_ERR;
    }
    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return FS_ERR;
    }
    const size_t got = size ? fread(buf, 1, (size_t)size, f) : 0;
    fclose(f);
    if (got != (size_t)size) {
        free(buf);
        return FS_ERR;
    }
    buf[size] = '\0';
    *data = buf;
    *len = (size_t)size;
    return FS_OK;
}

int fs_load(const char *path, char **data, size_t *len)
{
    int r = read_file(path, data, len);
    if (r == FS_NOT_FOUND) {
        char tmp[PATH_MAX_LEN];
        if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) < (int)sizeof(tmp)) {
            r = read_file(tmp, data, len);
        }
    }
    return r;
}
