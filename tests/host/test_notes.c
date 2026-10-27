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

    /* remove */
    CHECK(notes_remove(id2) == 0);
    CHECK(notes_list(list, 8, &n) == 0 && n == 3);  /* ft-0001, ft-0007, ft-0008 */

    char cmd[200];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
    if (system(cmd) != 0) {
        printf("note: could not remove %s\n", dir);
    }

    printf(failures ? "test_notes: %d failure(s)\n" : "test_notes: ok\n", failures);
    return failures != 0;
}
