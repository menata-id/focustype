#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "editor.h"
#include "fsutil.h"
#include "notes.h"
#include "textfind.h"

#define DIR_MAX 96
#define MAX_IDS 256
#define HEAD_BYTES 256

static char s_dir[DIR_MAX];

static int make_path(const char *id, const char *suffix, char *out, size_t cap)
{
    return snprintf(out, cap, "%s/%s%s", s_dir, id, suffix) < (int)cap ? 0 : -1;
}

int notes_init(const char *dir)
{
    if (snprintf(s_dir, sizeof(s_dir), "%s", dir) >= (int)sizeof(s_dir)) {
        return -1;
    }
    struct stat st;
    if (stat(s_dir, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return mkdir(s_dir, 0777) == 0 ? 0 : -1;
}

/* A note file is "ft-" + digits + ".txt", or ".txt.tmp" if a save was interrupted. Extracts the
 * id, or returns -1 for other names. */
static int parse_name(const char *name, char id[NOTE_ID_LEN])
{
    if (strncmp(name, "ft-", 3) != 0) {
        return -1;
    }
    size_t i = 3;
    while (isdigit((unsigned char)name[i])) {
        i++;
    }
    if (i == 3 || i >= NOTE_ID_LEN) {
        return -1;
    }
    if (strcmp(name + i, ".txt") != 0 && strcmp(name + i, ".txt.tmp") != 0) {
        return -1;
    }
    memcpy(id, name, i);
    id[i] = '\0';
    return 0;
}

static int scan_ids(char ids[][NOTE_ID_LEN], size_t max, size_t *count)
{
    DIR *d = opendir(s_dir);
    if (!d) {
        return -1;
    }
    size_t n = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && n < max) {
        char id[NOTE_ID_LEN];
        if (parse_name(de->d_name, id) != 0) {
            continue;
        }
        bool seen = false;
        for (size_t i = 0; i < n; i++) {
            if (strcmp(ids[i], id) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            memcpy(ids[n++], id, NOTE_ID_LEN);
        }
    }
    closedir(d);
    *count = n;
    return 0;
}

static int fill_info(const char *id, note_info_t *info)
{
    char path[DIR_MAX + NOTE_ID_LEN + 16];
    if (make_path(id, ".txt", path, sizeof(path)) != 0) {
        return -1;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        /* interrupted save: the complete new text is in the .tmp file */
        if (make_path(id, ".txt.tmp", path, sizeof(path)) != 0 || !(f = fopen(path, "rb"))) {
            return -1;
        }
    }

    memset(info, 0, sizeof(*info));
    memcpy(info->id, id, NOTE_ID_LEN);
    struct stat st;
    if (stat(path, &st) == 0) {
        info->mtime = st.st_mtime;
    }

    char head[HEAD_BYTES];
    size_t head_len = 0;
    char chunk[512];
    bool in_word = false;
    size_t got;
    while ((got = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        for (size_t i = 0; i < got; i++) {
            const char c = chunk[i];
            const bool space = c == ' ' || c == '\n' || c == '\t' || c == '\r';
            if (space) {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                info->words++;
            }
            if (head_len < sizeof(head)) {
                head[head_len++] = c;
            }
        }
    }
    fclose(f);
    ed_title_from(head, head_len, info->auto_title, sizeof(info->auto_title));
    notes_meta_load(id, &info->meta);
    snprintf(info->title, sizeof(info->title), "%s", info->meta.title[0] ? info->meta.title : info->auto_title);
    return 0;
}

static int newer_first(const void *a, const void *b)
{
    const note_info_t *x = a, *y = b;
    if (x->mtime != y->mtime) {
        return x->mtime < y->mtime ? 1 : -1;
    }
    return strcmp(y->id, x->id);  /* same time: higher id is newer */
}

int notes_list(note_info_t *out, size_t max, size_t *count)
{
    static char ids[MAX_IDS][NOTE_ID_LEN];
    size_t n = 0;
    if (scan_ids(ids, MAX_IDS, &n) != 0) {
        return -1;
    }
    size_t k = 0;
    for (size_t i = 0; i < n && k < max; i++) {
        if (fill_info(ids[i], &out[k]) == 0) {
            k++;
        }
    }
    qsort(out, k, sizeof(out[0]), newer_first);
    *count = k;
    return 0;
}

#define SNIPPET_BEFORE 24
#define SNIPPET_AFTER  48
#define ELLIPSIS       "\xE2\x80\xA6"

static bool utf8_cont(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

/* Copies the text around [pos, pos+qlen) without cutting a UTF-8 character, with "…" where text was cut. */
static void make_snippet(const char *text, size_t len, size_t pos, size_t qlen, note_hit_t *hit)
{
    size_t start = pos > SNIPPET_BEFORE ? pos - SNIPPET_BEFORE : 0;
    while (start < pos && utf8_cont((unsigned char)text[start])) {
        start++;
    }
    size_t end = pos + qlen + SNIPPET_AFTER;
    if (end > len) {
        end = len;
    }
    while (end > pos + qlen && end < len && utf8_cont((unsigned char)text[end])) {
        end--;
    }

    size_t o = 0;
    char *out = hit->snippet;
    if (start > 0) {
        memcpy(out, ELLIPSIS, 3);
        o = 3;
    }
    hit->hl_off = (uint8_t)(o + (pos - start));
    hit->hl_len = (uint8_t)qlen;
    for (size_t i = start; i < end && o + 4 < NOTE_SNIPPET_LEN; i++) {
        const char c = text[i];
        out[o++] = (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
    }
    if (end < len) {
        memcpy(out + o, ELLIPSIS, 3);
        o += 3;
    }
    out[o] = '\0';
}

static int hit_newer_first(const void *a, const void *b)
{
    return newer_first(&((const note_hit_t *)a)->info, &((const note_hit_t *)b)->info);
}

int notes_search(const char *query, note_hit_t *out, size_t max, size_t *count)
{
    static char ids[MAX_IDS][NOTE_ID_LEN];
    *count = 0;
    const size_t qlen = strlen(query);
    if (qlen == 0) {
        return 0;
    }
    size_t n = 0;
    if (scan_ids(ids, MAX_IDS, &n) != 0) {
        return -1;
    }
    size_t k = 0;
    for (size_t i = 0; i < n && k < max; i++) {
        char *text = NULL;
        size_t len = 0;
        if (notes_load(ids[i], &text, &len) != 0) {
            continue;
        }
        const size_t pos = tf_find_nocase(text, len, query, qlen);
        if (pos != TF_NOT_FOUND && fill_info(ids[i], &out[k].info) == 0) {
            make_snippet(text, len, pos, qlen, &out[k]);
            k++;
        }
        free(text);
    }
    qsort(out, k, sizeof(out[0]), hit_newer_first);
    *count = k;
    return 0;
}

int notes_info(const char *id, note_info_t *out)
{
    return fill_info(id, out);
}

int notes_new_id(char id[NOTE_ID_LEN])
{
    static char ids[MAX_IDS][NOTE_ID_LEN];
    size_t n = 0;
    if (scan_ids(ids, MAX_IDS, &n) != 0) {
        return -1;
    }
    unsigned max = 0;
    for (size_t i = 0; i < n; i++) {
        const unsigned v = (unsigned)strtoul(ids[i] + 3, NULL, 10);
        if (v > max) {
            max = v;
        }
    }
    snprintf(id, NOTE_ID_LEN, "ft-%04u", max + 1);
    return 0;
}

int notes_load(const char *id, char **text, size_t *len)
{
    char path[DIR_MAX + NOTE_ID_LEN + 16];
    if (make_path(id, ".txt", path, sizeof(path)) != 0) {
        return -1;
    }
    return fs_load(path, text, len);
}

int notes_meta_load(const char *id, note_meta_t *m)
{
    char path[DIR_MAX + NOTE_ID_LEN + 16];
    note_meta_defaults(m);
    if (make_path(id, ".meta", path, sizeof(path)) != 0) {
        return -1;
    }
    char *text = NULL;
    size_t len = 0;
    const int rc = fs_load(path, &text, &len);
    if (rc != 0) {
        return rc == FS_NOT_FOUND ? 1 : -1;
    }
    note_meta_parse(text, len, m);
    free(text);
    return 0;
}

int notes_meta_save(const char *id, const note_meta_t *m)
{
    char path[DIR_MAX + NOTE_ID_LEN + 16];
    char buf[320];
    const size_t n = note_meta_format(m, buf, sizeof(buf));
    if (n == 0 || make_path(id, ".meta", path, sizeof(path)) != 0) {
        return -1;
    }
    return fs_save_atomic(path, buf, n);
}

int notes_save(const char *id, const char *text, size_t len)
{
    char path[DIR_MAX + NOTE_ID_LEN + 16];
    if (make_path(id, ".txt", path, sizeof(path)) != 0) {
        return -1;
    }
    return fs_save_atomic(path, text, len);
}

int notes_remove(const char *id)
{
    char path[DIR_MAX + NOTE_ID_LEN + 16];
    if (make_path(id, ".txt", path, sizeof(path)) != 0) {
        return -1;
    }
    unlink(path);
    static const char *const others[] = {".txt.tmp", ".meta", ".meta.tmp"};
    for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); i++) {
        if (make_path(id, others[i], path, sizeof(path)) == 0) {
            unlink(path);
        }
    }
    return 0;
}
