#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pcf_regs.h"

static void check_roundtrip(int y, int mo, int d, int h, int mi, int s)
{
    const time_t t = pcf_civil_to_time(y, mo, d, h, mi, s);
    uint8_t regs[PCF_REG_COUNT];
    pcf_regs_encode(t, regs);
    time_t back = 0;
    assert(pcf_regs_decode(regs, &back));
    assert(back == t);

    struct tm tm;
    pcf_time_to_civil(t, &tm);
    assert(tm.tm_year + 1900 == y && tm.tm_mon + 1 == mo && tm.tm_mday == d);
    assert(tm.tm_hour == h && tm.tm_min == mi && tm.tm_sec == s);
}

int main(void)
{
    assert(pcf_civil_to_time(1970, 1, 1, 0, 0, 0) == 0);
    assert(pcf_civil_to_time(2000, 1, 1, 0, 0, 0) == 946684800);
    assert(pcf_civil_to_time(2026, 10, 29, 14, 32, 5) == 1793284325);

    check_roundtrip(2026, 10, 29, 14, 32, 5);
    check_roundtrip(2024, 2, 29, 23, 59, 59);  /* leap day */
    check_roundtrip(2000, 12, 31, 0, 0, 0);
    check_roundtrip(2099, 1, 1, 12, 0, 0);

    /* 2026-10-29 is a Thursday; 2024-02-29 a Thursday; 2000-01-01 a Saturday. */
    struct tm tm;
    pcf_time_to_civil(pcf_civil_to_time(2026, 10, 29, 0, 0, 0), &tm);
    assert(tm.tm_wday == 4 && tm.tm_yday == 301);
    pcf_time_to_civil(pcf_civil_to_time(2024, 2, 29, 0, 0, 0), &tm);
    assert(tm.tm_wday == 4 && tm.tm_yday == 59);
    pcf_time_to_civil(pcf_civil_to_time(2000, 1, 1, 0, 0, 0), &tm);
    assert(tm.tm_wday == 6);

    /* Known register image: 2026-10-29 14:32:05, Thursday. */
    uint8_t regs[PCF_REG_COUNT];
    pcf_regs_encode(pcf_civil_to_time(2026, 10, 29, 14, 32, 5), regs);
    const uint8_t want[PCF_REG_COUNT] = {0x05, 0x32, 0x14, 0x29, 0x04, 0x10, 0x26};
    assert(memcmp(regs, want, sizeof(want)) == 0);

    /* Oscillator-stop flag and out-of-range fields are rejected. */
    time_t t;
    uint8_t bad[PCF_REG_COUNT];
    memcpy(bad, want, sizeof(want));
    bad[0] |= 0x80;
    assert(!pcf_regs_decode(bad, &t));
    memcpy(bad, want, sizeof(want));
    bad[5] = 0x13;
    assert(!pcf_regs_decode(bad, &t));
    memcpy(bad, want, sizeof(want));
    bad[2] = 0x24;
    assert(!pcf_regs_decode(bad, &t));

    puts("test_pcf: ok");
    return 0;
}
