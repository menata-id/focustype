/* Host test for the notes store and atomic file helpers. Uses a temp directory.
 * Build and run with `make -C tests/host`. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <utime.h>

#include "fsutil.h"
#include "notes.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static void set_mtime(const char *id, time_t t)
{
    char path[200];
    snprintf(path, sizeof(path), "%s/%s.txt", getenv("NOTES_TEST_DIR"), id);
    struct utimbuf ub = {t, t};
    utime(path, &ub);
}

int main(void)
{
    char dir[] = "/tmp/ft_notes_test_XXXXXX";
    if (!mkdtemp(dir)) {
        printf("FAIL: mkdtemp\n");
        return 1;
    }
    setenv("NOTES_TEST_DIR", dir, 1);
    CHECK(notes_init(dir) == 0);

    note_info_t list[8];
    size_t n = 99;
    CHECK(notes_list(list, 8, &n) == 0 && n == 0);

    /* ids count up and the file only appears on save */
    char id1[NOTE_ID_LEN], id2[NOTE_ID_LEN];
    CHECK(notes_new_id(id1) == 0 && strcmp(id1, "ft-0001") == 0);
    CHECK(notes_new_id(id2) == 0 && strcmp(id2, "ft-0001") == 0);
    const char *t1 = "Chili prices\n\nAt the market this morning.";
    CHECK(notes_save(id1, t1, strlen(t1)) == 0);
    CHECK(notes_new_id(id2) == 0 && strcmp(id2, "ft-0002") == 0);
    const char *t2 = "  Second note  \nwith three words";
    CHECK(notes_save(id2, t2, strlen(t2)) == 0);

    /* load round-trips; a missing note reports "not found" */
    char *text = NULL;
    size_t len = 0;
    CHECK(notes_load(id1, &text, &len) == 0 && len == strlen(t1) && strcmp(text, t1) == 0);
    free(text);
    CHECK(notes_load("ft-0099", &text, &len) == FS_NOT_FOUND);

    /* listing: titles, word counts, newest first by mtime */
    set_mtime(id1, 2000000);
    set_mtime(id2, 1000000);
    CHECK(notes_list(list, 8, &n) == 0 && n == 2);
    CHECK(strcmp(list[0].id, "ft-0001") == 0 && strcmp(list[0].title, "Chili prices") == 0);
    CHECK(list[0].words == 7);
    CHECK(strcmp(list[1].id, "ft-0002") == 0 && strcmp(list[1].title, "Second note") == 0);
    CHECK(list[1].words == 5);

    /* overwrite keeps one file and no leftover .tmp */
    const char *t3 = "Replaced";
    CHECK(notes_save(id1, t3, strlen(t3)) == 0);
    CHECK(notes_load(id1, &text, &len) == 0 && strcmp(text, "Replaced") == 0);
    free(text);
    char tmp[200];
    snprintf(tmp, sizeof(tmp), "%s/%s.txt.tmp", dir, id1);
    CHECK(access(tmp, F_OK) != 0);

    /* an interrupted save (only the .tmp exists) still loads and lists */
    snprintf(tmp, sizeof(tmp), "%s/ft-0007.txt.tmp", dir);
    FILE *f = fopen(tmp, "wb");
    fputs("Recovered text", f);
    fclose(f);
    CHECK(notes_load("ft-0007", &text, &len) == 0 && strcmp(text, "Recovered text") == 0);
    free(text);
    CHECK(notes_list(list, 8, &n) == 0 && n == 3);
    char id3[NOTE_ID_LEN];
    CHECK(notes_new_id(id3) == 0 && strcmp(id3, "ft-0008") == 0);

    /* an empty note is valid */
    CHECK(notes_save("ft-0008", "", 0) == 0);
    CHECK(notes_load("ft-0008", &text, &len) == 0 && len == 0);
    free(text);

    /* search: whole text, any letter case; empty or unmatched queries find nothing */
    note_hit_t hits[8];
    CHECK(notes_search("THREE Words", hits, 8, &n) == 0 && n == 1 && strcmp(hits[0].info.id, "ft-0002") == 0);
    CHECK(notes_search("recovered", hits, 8, &n) == 0 && n == 1 && strcmp(hits[0].info.id, "ft-0007") == 0);
    CHECK(notes_search("re", hits, 8, &n) == 0 && n == 3);  /* Replaced, three, Recovered */
    CHECK(notes_search("", hits, 8, &n) == 0 && n == 0);
    CHECK(notes_search("zzz", hits, 8, &n) == 0 && n == 0);

    /* short text: no ellipsis, newline becomes a space, the highlight points at the match */
    CHECK(notes_search("three", hits, 8, &n) == 0 && n == 1);
    CHECK(strcmp(hits[0].snippet, "  Second note   with three words") == 0);
    CHECK(hits[0].hl_len == 5 && strncmp(hits[0].snippet + hits[0].hl_off, "three", 5) == 0);

    /* long text: both ends cut with an ellipsis, and a cut never lands inside a UTF-8 character */
    char longtext[400];
    size_t lo = 0;
    for (int i = 0; i < 40; i++) {
        longtext[lo++] = (char)0xC3;  /* "é" */
        longtext[lo++] = (char)0xA9;
    }
    lo += (size_t)snprintf(longtext + lo, sizeof(longtext) - lo, " a Needle here %s", "and a long tail that keeps going on and on and on and on and on");
    CHECK(notes_save("ft-0009", longtext, lo) == 0);
    CHECK(notes_search("NEEDLE", hits, 8, &n) == 0 && n == 1);
    const char *sn = hits[0].snippet;
    CHECK(strncmp(sn, "\xE2\x80\xA6", 3) == 0);
    CHECK(((unsigned char)sn[3] & 0xC0) != 0x80);
    CHECK(strlen(sn) > 6 && strcmp(sn + strlen(sn) - 3, "\xE2\x80\xA6") == 0);
    CHECK(hits[0].hl_len == 6 && strncasecmp(sn + hits[0].hl_off, "Needle", 6) == 0);
    CHECK(notes_remove("ft-0009") == 0);

    /* metadata: a manual title replaces the first line in listings; the text is untouched */
    note_meta_t mm;
    CHECK(notes_meta_load("ft-0001", &mm) == 1 && mm.status == 0 && mm.title[0] == '\0');
    mm.status = 2;
    mm.flag = true;
    mm.created = 1793284325;
    strcpy(mm.title, "My own title");
    strcpy(mm.tags, "#x");
    CHECK(notes_meta_save("ft-0001", &mm) == 0);
    note_meta_t back;
    CHECK(notes_meta_load("ft-0001", &back) == 0 && back.status == 2 && back.flag && strcmp(back.title, "My own title") == 0);
    CHECK(notes_list(list, 8, &n) == 0);
    int found = 0;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(list[i].id, "ft-0001") == 0) {
            found = 1;
            CHECK(strcmp(list[i].title, "My own title") == 0 && strcmp(list[i].auto_title, "Replaced") == 0);
            CHECK(list[i].meta.flag && list[i].meta.status == 2);
        }
    }
    CHECK(found && n == 4);  /* ft-0001, 0002, 0007, 0008: a .meta file is not a note */
    CHECK(notes_load("ft-0001", &text, &len) == 0 && strcmp(text, "Replaced") == 0);
    free(text);
    CHECK(notes_search("replaced", hits, 8, &n) == 0 && n == 1 && strcmp(hits[0].info.title, "My own title") == 0);
    /* an empty manual title falls back to the first line */
    mm.title[0] = '\0';
    CHECK(notes_meta_save("ft-0001", &mm) == 0);
    CHECK(notes_list(list, 8, &n) == 0);
    for (size_t i = 0; i < n; i++) {
        if (strcmp(list[i].id, "ft-0001") == 0) {
            CHECK(strcmp(list[i].title, "Replaced") == 0);
        }
    }

    /* remove */
    CHECK(notes_meta_save(id2, &mm) == 0);
    CHECK(notes_remove(id2) == 0);
    snprintf(tmp, sizeof(tmp), "%s/%s.meta", dir, id2);
    CHECK(access(tmp, F_OK) != 0);
    CHECK(notes_list(list, 8, &n) == 0 && n == 3);  /* ft-0001, ft-0007, ft-0008 */

    char cmd[200];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
    if (system(cmd) != 0) {
        printf("note: could not remove %s\n", dir);
    }

    printf(failures ? "test_notes: %d failure(s)\n" : "test_notes: ok\n", failures);
    return failures != 0;
}
