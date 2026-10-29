#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * PCF85063A time registers (0x04..0x0A: seconds, minutes, hours, days, weekday, months, years, all
 * BCD, 24 h mode). Pure logic, no ESP-IDF headers. Times are "wall clock" values: the firmware
 * never sets a time zone, so time_t here is the local time as typed on the Set clock screen.
 */

#define PCF_REG_COUNT 7

time_t pcf_civil_to_time(int year, int month, int day, int hour, int minute, int second);

/* Broken-down time without any time zone lookup (tm_wday and tm_yday are filled in). */
void pcf_time_to_civil(time_t t, struct tm *out);

void pcf_regs_encode(time_t t, uint8_t regs[PCF_REG_COUNT]);

/* False when the oscillator-stop flag is set (the time cannot be trusted) or a field is out of range. */
bool pcf_regs_decode(const uint8_t regs[PCF_REG_COUNT], time_t *out);
