#include <stdio.h>
#include <string.h>

#include "note_meta.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(void)
{
    note_meta_t m;

    /* defaults for a missing or empty file */
    note_meta_parse("", 0, &m);
    CHECK(m.title[0] == '\0' && m.status == 0 && !m.flag && !m.archived && m.created == 0);

    /* round trip */
    note_meta_t a;
    note_meta_defaults(&a);
    strcpy(a.title, "RW meeting \xE2\x80\x94 March agenda");
    a.status = 1;
    strcpy(a.category, "Community");
    strcpy(a.tags, "#community #idea");
    a.flag = true;
    a.archived = true;
    a.created = 1793284325;
    char buf[512];
    const size_t n = note_meta_format(&a, buf, sizeof(buf));
    CHECK(n > 0 && n == strlen(buf));
    note_meta_parse(buf, n, &m);
    CHECK(strcmp(m.title, a.title) == 0 && m.status == 1 && strcmp(m.category, "Community") == 0);
    CHECK(strcmp(m.tags, "#community #idea") == 0 && m.flag && m.archived && m.created == 1793284325);
    CHECK(strstr(buf, "status=Developing\n") != NULL);

    /* too small a buffer is reported, not truncated silently */
    CHECK(note_meta_format(&a, buf, 10) == 0);

    /* unknown keys, bad status, CRLF, missing '=' and stray lines are tolerated */
    const char *odd = "future=1\r\nstatus=Nonsense\r\ntitle=Hi there\r\ngarbage\n\nflag=0\ntags=#a\n";
    note_meta_parse(odd, strlen(odd), &m);
    CHECK(strcmp(m.title, "Hi there") == 0 && m.status == 0 && !m.flag && strcmp(m.tags, "#a") == 0);

    /* a long value is cut without splitting a UTF-8 character */
    char longline[200] = "title=";
    for (int i = 0; i < 40; i++) {
        strcat(longline, "\xC3\xA9");  /* 80 bytes, the field holds 63 */
    }
    note_meta_parse(longline, strlen(longline), &m);
    CHECK(strlen(m.title) <= 63 && (strlen(m.title) % 2) == 0);

    /* newlines inside a value cannot inject keys */
    note_meta_defaults(&a);
    strcpy(a.title, "x\nflag=1");
    const size_t n2 = note_meta_format(&a, buf, sizeof(buf));
    note_meta_parse(buf, n2, &m);
    CHECK(!m.flag);

    /* tags */
    char t[48];
    note_tags_normalize("foo, #Bar  baz", t, sizeof(t));
    CHECK(strcmp(t, "#foo #Bar #baz") == 0);
    note_tags_normalize("a A #a ,, b", t, sizeof(t));
    CHECK(strcmp(t, "#a #b") == 0);
    note_tags_normalize("", t, sizeof(t));
    CHECK(t[0] == '\0');
    note_tags_normalize("###", t, sizeof(t));
    CHECK(t[0] == '\0');
    char small[12];
    note_tags_normalize("alpha beta gamma", small, sizeof(small));
    CHECK(strcmp(small, "#alpha") == 0);  /* "#alpha #beta" would not fit in 12 with the NUL */

    printf(failures ? "test_meta: %d failure(s)\n" : "test_meta: ok\n", failures);
    return failures != 0;
}
