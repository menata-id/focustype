/* Host test for the UI text formatters. Build and run with `make -C tests/host`. */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ui.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); failures++; } } while (0)

static time_t utc(int y, int mo, int d, int h, int mi)
{
    struct tm tm = {.tm_year = y - 1900, .tm_mon = mo - 1, .tm_mday = d, .tm_hour = h, .tm_min = mi};
    return timegm(&tm);
}

int main(void)
{
    char out[32];

    ui_format_words(0, out, sizeof(out));
    CHECK(strcmp(out, "0 words") == 0);
    ui_format_words(1, out, sizeof(out));
    CHECK(strcmp(out, "1 word") == 0);
    ui_format_words(412, out, sizeof(out));
    CHECK(strcmp(out, "412 words") == 0);
    ui_format_words(1104, out, sizeof(out));
    CHECK(strcmp(out, "1,104 words") == 0);
    ui_format_words(1234567, out, sizeof(out));
    CHECK(strcmp(out, "1,234,567 words") == 0);

    setenv("TZ", "UTC", 1);
    tzset();
    const time_t now = utc(2026, 10, 27, 2, 35);

    ui_format_when(utc(2026, 10, 27, 2, 34), now, out, sizeof(out));
    CHECK(strcmp(out, "02:34") == 0);
    ui_format_when(utc(2026, 10, 26, 21, 40), now, out, sizeof(out));
    CHECK(strcmp(out, "Yesterday") == 0);
    ui_format_when(utc(2026, 10, 12, 9, 0), now, out, sizeof(out));
    CHECK(strcmp(out, "12 Oct") == 0);
    ui_format_when(utc(2026, 9, 9, 9, 0), now, out, sizeof(out));
    CHECK(strcmp(out, "9 Sep") == 0);
    ui_format_when(0, now, out, sizeof(out));
    CHECK(out[0] == '\0');

    ui_format_date(utc(2026, 10, 20, 9, 0), out, sizeof(out));
    CHECK(strcmp(out, "20 Oct") == 0);
    ui_format_date(0, out, sizeof(out));
    CHECK(strcmp(out, "\xE2\x80\x94") == 0);
    ui_format_stamp(utc(2026, 10, 27, 2, 34), now, out, sizeof(out));
    CHECK(strcmp(out, "Today 02:34") == 0);
    ui_format_stamp(utc(2026, 10, 26, 21, 40), now, out, sizeof(out));
    CHECK(strcmp(out, "Yest. 21:40") == 0);
    ui_format_stamp(utc(2026, 10, 12, 9, 5), now, out, sizeof(out));
    CHECK(strcmp(out, "12 Oct 09:05") == 0);
    ui_format_stamp(1000, now, out, sizeof(out));  /* clock never set */
    CHECK(strcmp(out, "\xE2\x80\x94") == 0);

    printf(failures ? "test_format: %d failure(s)\n" : "test_format: ok\n", failures);
    return failures != 0;
}
