#include <stdio.h>
#include <string.h>

#include "ui.h"

static const char *const k_months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static bool same_day(const struct tm *a, const struct tm *b)
{
    return a->tm_year == b->tm_year && a->tm_yday == b->tm_yday;
}

void ui_format_when(time_t t, time_t now, char *out, size_t cap)
{
    if (t <= 0) {
        snprintf(out, cap, "%s", "");
        return;
    }
    struct tm tt, today, yesterday;
    const time_t yday = now - 24 * 60 * 60;
    localtime_r(&t, &tt);
    localtime_r(&now, &today);
    localtime_r(&yday, &yesterday);

    if (same_day(&tt, &today)) {
        snprintf(out, cap, "%02d:%02d", tt.tm_hour, tt.tm_min);
    } else if (same_day(&tt, &yesterday)) {
        snprintf(out, cap, "Yesterday");
    } else {
        snprintf(out, cap, "%d %s", tt.tm_mday, k_months[tt.tm_mon % 12]);
    }
}

#define CLOCK_SET_AFTER 1735689600  /* 2025-01-01: earlier times mean the clock was never set */

void ui_format_date(time_t t, char *out, size_t cap)
{
    if (t < CLOCK_SET_AFTER) {
        snprintf(out, cap, "%s", "\xE2\x80\x94");
        return;
    }
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(out, cap, "%d %s", tm.tm_mday, k_months[tm.tm_mon % 12]);
}

void ui_format_stamp(time_t t, time_t now, char *out, size_t cap)
{
    if (t < CLOCK_SET_AFTER) {
        snprintf(out, cap, "%s", "\xE2\x80\x94");
        return;
    }
    struct tm tt, today, yesterday;
    const time_t yday = now - 24 * 60 * 60;
    localtime_r(&t, &tt);
    localtime_r(&now, &today);
    localtime_r(&yday, &yesterday);
    if (same_day(&tt, &today)) {
        snprintf(out, cap, "Today %02d:%02d", tt.tm_hour, tt.tm_min);
    } else if (same_day(&tt, &yesterday)) {
        snprintf(out, cap, "Yest. %02d:%02d", tt.tm_hour, tt.tm_min);
    } else {
        snprintf(out, cap, "%d %s %02d:%02d", tt.tm_mday, k_months[tt.tm_mon % 12], tt.tm_hour, tt.tm_min);
    }
}

void ui_format_words(uint32_t n, char *out, size_t cap)
{
    char digits[16];
    snprintf(digits, sizeof(digits), "%lu", (unsigned long)n);
    const size_t len = strlen(digits);

    char grouped[24];
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) {
            grouped[o++] = ',';
        }
        grouped[o++] = digits[i];
    }
    grouped[o] = '\0';
    snprintf(out, cap, "%s %s", grouped, n == 1 ? "word" : "words");
}
