#ifndef NETCHESSZX_SPECTRUM_FAT_TIMEZONE_H
#define NETCHESSZX_SPECTRUM_FAT_TIMEZONE_H

#include "fat_calendar.h"

static void spectrum_fat_apply_hour_offset(uint8_t *hour,
                                           uint16_t *date,
                                           uint16_t *time,
                                           int8_t offset)
{
    uint8_t local_hour = (uint8_t)(*hour + offset);

    if (local_hour >= 24u) {
        uint8_t year = (uint8_t)(*date >> 9);
        uint8_t month = (uint8_t)((*date >> 5) & 15u);
        uint8_t day = (uint8_t)(*date & 31u);

        if (offset < 0) {
            local_hour = (uint8_t)(local_hour + 24u);
            if (--day == 0u) {
                if (month == 1u) {
                    if (year == 0u) {
                        return;
                    }
                    month = 12u;
                    --year;
                } else {
                    --month;
                }
                day = spectrum_fat_days_in_month(year, month);
            }
        } else {
            local_hour = (uint8_t)(local_hour - 24u);
            if (++day > spectrum_fat_days_in_month(year, month)) {
                day = 1u;
                if (month == 12u) {
                    if (year == 127u) {
                        return;
                    }
                    month = 1u;
                    ++year;
                } else {
                    ++month;
                }
            }
        }
        *date = (uint16_t)(((uint16_t)year << 9) |
                           ((uint16_t)month << 5) | day);
    }
    *hour = (uint8_t)local_hour;
    *time = (uint16_t)((*time & 0x07ffu) | ((uint16_t)*hour << 11));
}

#endif
