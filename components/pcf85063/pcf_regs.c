#include "pcf_regs.h"

/* Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm). */
static int64_t days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = (int)(y - era * 400);
    const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

time_t pcf_civil_to_time(int year, int month, int day, int hour, int minute, int second)
{
    return (time_t)(days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second);
}

void pcf_time_to_civil(time_t t, struct tm *out)
{
    int64_t days = (int64_t)(t / 86400);
    int64_t rem = (int64_t)(t % 86400);
    if (rem < 0) {
        rem += 86400;
        days--;
    }
    const int64_t z = days + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int doe = (int)(z - era * 146097);
    const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int mp = (5 * doy + 2) / 153;
    const int d = doy - (153 * mp + 2) / 5 + 1;
    const int m = mp < 10 ? mp + 3 : mp - 9;
    const int y = (int)(yoe + era * 400) + (m <= 2);

    out->tm_year = y - 1900;
    out->tm_mon = m - 1;
    out->tm_mday = d;
    out->tm_hour = (int)(rem / 3600);
    out->tm_min = (int)(rem % 3600 / 60);
    out->tm_sec = (int)(rem % 60);
    out->tm_wday = (int)((days % 7 + 11) % 7);  /* 1970-01-01 was a Thursday (4) */
    out->tm_yday = (int)(days - days_from_civil(y, 1, 1));
    out->tm_isdst = 0;
}

static uint8_t to_bcd(int v)
{
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

static int from_bcd(uint8_t v)
{
    return (v >> 4) * 10 + (v & 0x0F);
}

void pcf_regs_encode(time_t t, uint8_t regs[PCF_REG_COUNT])
{
    struct tm tm;
    pcf_time_to_civil(t, &tm);
    regs[0] = to_bcd(tm.tm_sec) & 0x7F;  /* OS flag cleared */
    regs[1] = to_bcd(tm.tm_min);
    regs[2] = to_bcd(tm.tm_hour);
    regs[3] = to_bcd(tm.tm_mday);
    regs[4] = (uint8_t)tm.tm_wday;
    regs[5] = to_bcd(tm.tm_mon + 1);
    regs[6] = to_bcd((tm.tm_year + 1900) % 100);
}

bool pcf_regs_decode(const uint8_t regs[PCF_REG_COUNT], time_t *out)
{
    if (regs[0] & 0x80) {
        return false;
    }
    const int sec = from_bcd(regs[0] & 0x7F);
    const int min = from_bcd(regs[1] & 0x7F);
    const int hour = from_bcd(regs[2] & 0x3F);
    const int day = from_bcd(regs[3] & 0x3F);
    const int mon = from_bcd(regs[5] & 0x1F);
    const int year = 2000 + from_bcd(regs[6]);
    if (sec > 59 || min > 59 || hour > 23 || day < 1 || day > 31 || mon < 1 || mon > 12) {
        return false;
    }
    *out = pcf_civil_to_time(year, mon, day, hour, min, sec);
    return true;
}
