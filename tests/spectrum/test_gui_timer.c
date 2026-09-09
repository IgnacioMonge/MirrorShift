#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/spectrum/ui/timer.h"

static void check_time(uint8_t hour,
                       uint8_t minute,
                       uint8_t second,
                       uint8_t expected_hour,
                       uint8_t expected_minute,
                       uint8_t expected_second,
                       const char *message)
{
    if (hour != expected_hour || minute != expected_minute ||
        second != expected_second) {
        fprintf(stderr,
                "FAIL: %s (got %u:%02u:%02u, expected %u:%02u:%02u)\n",
                message,
                (unsigned)hour,
                (unsigned)minute,
                (unsigned)second,
                (unsigned)expected_hour,
                (unsigned)expected_minute,
                (unsigned)expected_second);
        exit(1);
    }
}

int main(void)
{
    uint8_t hour = 99u;
    uint8_t minute = 59u;
    uint8_t second = 0u;
    uint8_t timers[6] = { 1u, 2u, 3u, 4u, 5u, 6u };
    uint8_t saved[6] = { 0u };
    int old_zone;
    int new_zone;
    int h;
    spectrum_timer_state_copy(saved, timers);
    if (memcmp(timers, saved, sizeof(timers)) != 0) {
        fputs("FAIL: save/restore timer state\n", stderr);
        return 1;
    }


    for (old_zone = -11; old_zone <= 13; ++old_zone) {
        for (new_zone = -11; new_zone <= 13; ++new_zone) {
            for (h = 0; h < 24; ++h) {
                check_time(shifted_clock_hour((uint8_t)h,
                                             (int8_t)(new_zone - old_zone)),
                           0u, 0u,
                           (uint8_t)((h + new_zone - old_zone + 48) % 24),
                           0u, 0u, "UTC rebase wraps without losing a day");
            }
        }
    }

    timer_tick_one_second(&hour, &minute, &second);
    check_time(hour, minute, second, 99u, 59u, 1u,
               "99:59 advances before the saturation second");

    second = 58u;
    timer_tick_one_second(&hour, &minute, &second);
    check_time(hour, minute, second, 99u, 59u, 59u,
               "timer reaches its 99:59:59 maximum");

    timer_tick_one_second(&hour, &minute, &second);
    check_time(hour, minute, second, 99u, 59u, 59u,
               "timer remains saturated at 99:59:59");

    hour = 98u;
    minute = 59u;
    second = 59u;
    timer_tick_one_second(&hour, &minute, &second);
    check_time(hour, minute, second, 99u, 0u, 0u,
               "timer carries into hour 99");

    puts("gui timer tests passed");
    return 0;
}
