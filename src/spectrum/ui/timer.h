#ifndef NETCHESSZX_SPECTRUM_UI_TIMER_H
#define NETCHESSZX_SPECTRUM_UI_TIMER_H

#include <stdint.h>

#define SPECTRUM_TIMER_STATE_SIZE 6u

static void spectrum_timer_state_copy(uint8_t *dst, const uint8_t *src)
{
    uint8_t i;

    for (i = 0u; i < SPECTRUM_TIMER_STATE_SIZE; ++i) {
        dst[i] = src[i];
    }
}

#if defined(NETCHESSZX_SPECTRANEXT) || !defined(NETCHESSZX_FIXED_LOW_RAM)
/* Setup UTC offsets span -11..+13: a delta is at most one full day. */
static uint8_t shifted_clock_hour(uint8_t hour, int8_t delta)
{
    int8_t shifted = (int8_t)((int8_t)hour + delta);

    if (shifted < 0) {
        shifted += 24;
    } else if (shifted >= 24) {
        shifted -= 24;
    }
    return (uint8_t)shifted;
}

#endif

#ifndef NETCHESSZX_SDCC_IY
static void timer_tick_one_second(uint8_t *hour,
                                  uint8_t *minute,
                                  uint8_t *second)
{
    if (*hour == 99u && *minute == 59u && *second == 59u) {
        return;
    }
    ++*second;
    if (*second < 60u) {
        return;
    }
    *second = 0u;
    ++*minute;
    if (*minute < 60u) {
        return;
    }
    *minute = 0u;
    ++*hour;
}
#endif

#endif
