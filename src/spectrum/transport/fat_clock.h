#ifndef NETCHESSZX_SPECTRUM_FAT_CLOCK_H
#define NETCHESSZX_SPECTRUM_FAT_CLOCK_H

#include "fat_calendar.h"

static void spectrum_fat_tick_clock(uint16_t *date, uint16_t *time,
                                    uint8_t hour, uint8_t minute,
                                    uint8_t second)
{
    if (*date == 0u) {
        return;
    }
    if (hour == 0u && minute == 0u && second == 0u) {
        uint8_t year = (uint8_t)(*date >> 9);
        uint8_t month = (uint8_t)((*date >> 5) & 15u);
        uint8_t day = (uint8_t)(*date & 31u);

        if (++day > spectrum_fat_days_in_month(year, month)) {
            if (month < 12u) {
                ++month;
                day = 1u;
            } else if (year < 127u) {
                month = 1u;
                ++year;
                day = 1u;
            } else {
                day = 31u;
            }
        }
        *date = (uint16_t)(((uint16_t)year << 9) |
                           ((uint16_t)month << 5) | day);
    }
    *time = (uint16_t)(((uint16_t)hour << 11) |
                       ((uint16_t)minute << 5) | (second >> 1));
}

#endif
