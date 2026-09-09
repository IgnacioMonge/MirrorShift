#include <assert.h>
#include <stdint.h>

#include "spxtime.h"
#include "spectrum/ui/timer.h"

static uint16_t runtime_date;
static uint16_t runtime_time;
static uint16_t runtime_days;
static uint8_t fat_stamp_calls;
static uint8_t gui_calls;
static uint8_t gui_ok;
static int8_t gui_delta;
static uint8_t gui_hour;
static uint8_t gui_minute;
static uint8_t gui_second;

int8_t netchesszx_timezone;
int8_t netchesszx_timezone_last;
uint8_t netchesszx_rtc_available;

void net_wait_frame(void) {}

int16_t spxtime_sntp(const struct spxtime_request *request)
{
    (void)request;
    return SPXTIME_ETIMEOUT;
}

uint16_t spectrum_net_runtime_fat_date(void) { return runtime_date; }
uint16_t spectrum_net_runtime_fat_time(void) { return runtime_time; }
uint16_t spectrum_net_runtime_fat_elapsed_days(void) { return runtime_days; }

void spectrum_net_runtime_set_clock(uint8_t hour, uint8_t minute,
                                    uint8_t second)
{
    gui_hour = hour;
    gui_minute = minute;
    gui_second = second;
}

void spectrum_net_runtime_set_fat_stamp(uint16_t date, uint16_t time)
{
    runtime_date = date;
    runtime_time = time;
    ++fat_stamp_calls;
}

uint8_t spectrum_gui_shift_clock(int8_t delta)
{
    ++gui_calls;
    gui_delta = delta;
    if (!gui_ok) return 0u;
    gui_hour = shifted_clock_hour(gui_hour, delta);
    return 1u;
}

#include "../../src/spectrum/overlay/time_ovl.c"

static uint16_t fat_date(uint16_t year, uint8_t month, uint8_t day)
{
    return (uint16_t)(((uint16_t)(year - 1980u) << 9) |
                      ((uint16_t)month << 5) | day);
}

static uint16_t fat_time(uint8_t hour, uint8_t minute, uint8_t second)
{
    return (uint16_t)(((uint16_t)hour << 11) |
                      ((uint16_t)minute << 5) | (second >> 1));
}

static void reset_case(uint16_t date, uint16_t time, uint16_t days,
                       uint8_t hour, uint8_t minute, uint8_t second)
{
    runtime_date = date;
    runtime_time = time;
    runtime_days = days;
    fat_stamp_calls = 0u;
    gui_calls = 0u;
    gui_ok = 1u;
    gui_delta = 0;
    gui_hour = hour;
    gui_minute = minute;
    gui_second = second;
}

static void test_plus_24_crosses_year(void)
{
    uint8_t ctx[] = {24u};

    reset_case(fat_date(2023u, 12u, 31u), fat_time(23u, 58u, 58u), 0u,
               23u, 58u, 59u);
    assert(spectranext_time_shift_ovl(ctx) == 1u);
    assert(gui_calls == 1u && gui_delta == 24);
    assert(gui_hour == 23u && gui_minute == 58u && gui_second == 59u);
    assert(fat_stamp_calls == 1u);
    assert(runtime_date == fat_date(2024u, 1u, 1u));
    assert(runtime_time == fat_time(23u, 58u, 58u));
}

static void test_minus_24_resolves_pending_leap_midnights(void)
{
    uint8_t ctx[] = {(uint8_t)-24};

    reset_case(fat_date(2024u, 2u, 28u), fat_time(1u, 2u, 58u), 2u,
               1u, 2u, 59u);
    assert(spectranext_time_shift_ovl(ctx) == 1u);
    assert(gui_calls == 1u && gui_delta == -24);
    assert(gui_hour == 1u && gui_minute == 2u && gui_second == 59u);
    assert(fat_stamp_calls == 1u);
    assert(runtime_date == fat_date(2024u, 2u, 29u));
    assert(runtime_time == fat_time(1u, 2u, 58u));
}

static void test_failures_do_not_mutate_fat_stamp(void)
{
    uint8_t ctx[] = {3u};
    uint16_t date;
    uint16_t time;

    reset_case(0u, fat_time(5u, 6u, 58u), 4u, 5u, 6u, 59u);
    time = runtime_time;
    assert(spectranext_time_shift_ovl(ctx) == 0u);
    assert(gui_calls == 0u && fat_stamp_calls == 0u);
    assert(runtime_date == 0u && runtime_time == time);

    reset_case(fat_date(2024u, 6u, 7u), fat_time(5u, 6u, 58u), 4u,
               5u, 6u, 59u);
    gui_ok = 0u;
    date = runtime_date;
    time = runtime_time;
    assert(spectranext_time_shift_ovl(ctx) == 0u);
    assert(gui_calls == 1u && fat_stamp_calls == 0u);
    assert(runtime_date == date && runtime_time == time);
    assert(gui_hour == 5u && gui_minute == 6u && gui_second == 59u);
}

int main(void)
{
    test_plus_24_crosses_year();
    test_minus_24_resolves_pending_leap_midnights();
    test_failures_do_not_mutate_fat_stamp();
    return 0;
}
