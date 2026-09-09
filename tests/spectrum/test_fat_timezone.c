#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "spectrum/transport/fat_timezone.h"
#include "spectrum/transport/fat_clock.h"

static uint16_t fat_date(uint16_t year, uint8_t month, uint8_t day)
{
    return (uint16_t)(((uint16_t)(year - 1980u) << 9) |
                      ((uint16_t)month << 5) | day);
}

static void check(uint8_t condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static uint8_t reference_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] = {
        31u, 28u, 31u, 30u, 31u, 30u,
        31u, 31u, 30u, 31u, 30u, 31u
    };
    uint8_t result = days[month - 1u];

    if (month == 2u && (year % 4u) == 0u &&
        ((year % 100u) != 0u || (year % 400u) == 0u)) {
        ++result;
    }
    return result;
}

static void check_case(uint8_t hour, uint16_t date, int8_t offset,
                       uint8_t expected_hour, uint16_t expected_date)
{
    uint16_t time = (uint16_t)(((uint16_t)hour << 11) | (34u << 5) | 15u);

    spectrum_fat_apply_hour_offset(&hour, &date, &time, offset);
    check(hour == expected_hour, "hour offset");
    check(date == expected_date, "date rollover");
    check((uint8_t)(time >> 11) == expected_hour, "FAT hour");
    check((time & 0x07ffu) == (uint16_t)((34u << 5) | 15u),
          "FAT minute/seconds preserved");
}

static void reference_apply(uint8_t *hour, uint16_t *date, uint16_t *time,
                            int8_t offset)
{
    int16_t local_hour = (int16_t)*hour + offset;
    uint8_t year = (uint8_t)(*date >> 9);
    uint8_t month = (uint8_t)((*date >> 5) & 15u);
    uint8_t day = (uint8_t)(*date & 31u);

    if (local_hour < 0) {
        local_hour += 24;
        if (--day == 0u) {
            if (--month == 0u) {
                if (year == 0u) {
                    return;
                }
                month = 12u;
                --year;
            }
            day = reference_days_in_month((uint16_t)(1980u + year), month);
        }
    } else if (local_hour >= 24) {
        local_hour -= 24;
        if (++day > reference_days_in_month((uint16_t)(1980u + year), month)) {
            day = 1u;
            if (++month > 12u) {
                if (year == 127u) {
                    return;
                }
                month = 1u;
                ++year;
            }
        }
    }
    *hour = (uint8_t)local_hour;
    *date = (uint16_t)(((uint16_t)year << 9) |
                       ((uint16_t)month << 5) | day);
    *time = (uint16_t)((*time & 0x07ffu) | ((uint16_t)*hour << 11));
}

static void check_exhaustive_equivalence(void)
{
    uint8_t year;

    for (year = 0u; year < 128u; ++year) {
        uint8_t month;

        for (month = 1u; month <= 12u; ++month) {
            uint8_t day;
            uint8_t days = reference_days_in_month(
                (uint16_t)(1980u + year), month);

            for (day = 1u; day <= days; ++day) {
                uint8_t hour;

                for (hour = 0u; hour < 24u; ++hour) {
                    int8_t offset;

                    for (offset = -11; offset <= 13; ++offset) {
                        uint8_t actual_hour = hour;
                        uint8_t expected_hour = hour;
                        uint16_t actual_date =
                            (uint16_t)(((uint16_t)year << 9) |
                                       ((uint16_t)month << 5) | day);
                        uint16_t expected_date = actual_date;
                        uint16_t actual_time =
                            (uint16_t)(((uint16_t)hour << 11) | 0x0455u);
                        uint16_t expected_time = actual_time;

                        spectrum_fat_apply_hour_offset(
                            &actual_hour, &actual_date, &actual_time, offset);
                        reference_apply(&expected_hour, &expected_date,
                                        &expected_time, offset);
                        check(actual_hour == expected_hour,
                              "exhaustive hour equivalence");
                        check(actual_date == expected_date,
                              "exhaustive date equivalence");
                        check(actual_time == expected_time,
                              "exhaustive time equivalence");
                    }
                }
            }
        }
    }
}

static void check_clock_ticks(void)
{
    uint16_t date = fat_date(2024u, 2u, 28u);
    uint16_t time = (uint16_t)((23u << 11) | (59u << 5) | 29u);

    spectrum_fat_tick_clock(&date, &time, 23u, 59u, 59u);
    check(date == fat_date(2024u, 2u, 28u), "odd second keeps date");
    check((time & 31u) == 29u, "odd second uses FAT two-second precision");
    spectrum_fat_tick_clock(&date, &time, 0u, 0u, 0u);
    check(date == fat_date(2024u, 2u, 29u), "midnight reaches leap day");
    check(time == 0u, "midnight clears FAT time");

    date = fat_date(2026u, 12u, 31u);
    spectrum_fat_tick_clock(&date, &time, 0u, 0u, 0u);
    check(date == fat_date(2027u, 1u, 1u), "midnight rolls year");

    date = fat_date(2100u, 2u, 28u);
    spectrum_fat_tick_clock(&date, &time, 0u, 0u, 0u);
    check(date == fat_date(2100u, 3u, 1u), "2100 is not a leap year");

    date = fat_date(2107u, 12u, 31u);
    spectrum_fat_tick_clock(&date, &time, 0u, 0u, 0u);
    check(date == fat_date(2107u, 12u, 31u),
          "FAT maximum date does not wrap");
}

static void check_all_midnights(void)
{
    uint8_t year;
    for (year = 0u; year < 128u; ++year) {
        uint8_t month;
        for (month = 1u; month <= 12u; ++month) {
            uint8_t day;
            uint8_t days = reference_days_in_month(
                (uint16_t)(1980u + year), month);

            for (day = 1u; day <= days; ++day) {
                uint16_t date = fat_date((uint16_t)(1980u + year), month, day);
                uint16_t expected_date = date;
                uint16_t time = 0u;
                uint16_t expected_time = 0u;
                uint8_t hour = 23u;
                reference_apply(&hour, &expected_date, &expected_time, 1);
                spectrum_fat_tick_clock(&date, &time, 0u, 0u, 0u);
                check(date == expected_date, "all FAT midnights preserve calendar");
                check(time == 0u, "all FAT midnights clear time");
            }
        }
    }
}

int main(void)
{
    check(spectrum_fat_days_in_month(120u, 2u) == 28u,
          "FAT year 120 is Gregorian 2100");
    check_case(12u, fat_date(2026u, 8u, 24u), 2,
               14u, fat_date(2026u, 8u, 24u));
    check_case(1u, fat_date(2026u, 3u, 1u), -2,
               23u, fat_date(2026u, 2u, 28u));
    check_case(23u, fat_date(2024u, 2u, 28u), 2,
               1u, fat_date(2024u, 2u, 29u));
    check_case(23u, fat_date(2026u, 12u, 31u), 2,
               1u, fat_date(2027u, 1u, 1u));
    check_case(0u, fat_date(1980u, 1u, 1u), -1,
               0u, fat_date(1980u, 1u, 1u));
    check_case(23u, fat_date(2107u, 12u, 31u), 1,
               23u, fat_date(2107u, 12u, 31u));
    check_clock_ticks();
    check_all_midnights();
    check_exhaustive_equivalence();
    puts("FAT timezone tests ok");
    return 0;
}
