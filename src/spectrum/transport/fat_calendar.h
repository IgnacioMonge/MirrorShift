#ifndef NETCHESSZX_SPECTRUM_FAT_CALENDAR_H
#define NETCHESSZX_SPECTRUM_FAT_CALENDAR_H

#include <stdint.h>

static uint8_t spectrum_fat_days_in_month(uint8_t year, uint8_t month)
{
    if (month == 2u) {
        return (uint8_t)(28u + ((year & 3u) == 0u && year != 120u));
    }
    return (uint8_t)(30u + ((month + (month > 7u)) & 1u));
}

#endif
