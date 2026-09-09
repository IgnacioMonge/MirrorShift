#include "spectrum/platform/net_runtime.h"

#include "spectrum/platform/platform.h"
#include "spectrum/transport/link.h"
#include "spectrum/ui/gui.h"

static uint8_t runtime_clock_ready;
static uint16_t runtime_fat_date;
static uint16_t runtime_fat_time;
static uint16_t runtime_fat_days;

void spectrum_net_runtime_wait_frame(void)
{
    spectrum_frame_wait();
    spectrum_link_background_drain();
    spectrum_gui_tick();
}

void spectrum_net_runtime_wait_frame_plain(void)
{
    spectrum_frame_wait();
#ifndef NETCHESSZX_SPECTRANEXT
    spectrum_uart_background_pump();
#endif
    spectrum_gui_tick();
}

#ifdef NETCHESSZX_SPECTRANEXT
void spxn_send_idle(void)
{
    spectrum_net_runtime_wait_frame_plain();
}
#endif

void spectrum_net_runtime_set_clock(uint8_t hour, uint8_t minute, uint8_t second)
{
    if (hour >= 24u || minute >= 60u || second >= 60u) {
        return;
    }
    spectrum_gui_set_clock(hour, minute, second);
    runtime_clock_ready = 1u;
}

uint8_t spectrum_net_runtime_clock_ready(void)
{
    return runtime_clock_ready;
}

void spectrum_net_runtime_set_fat_stamp(uint16_t date, uint16_t time)
{
    runtime_fat_date = date;
    runtime_fat_time = time;
    runtime_fat_days = 0u;
}

void spectrum_net_runtime_tick_clock(uint8_t hour, uint8_t minute,
                                     uint8_t second)
{
    if (runtime_clock_ready && runtime_fat_date != 0u) {
        /* Calendar conversion belongs to SAVELOAD. This tick also runs while
           other overlays are active, so it must never load an overlay. */
        if (hour == 0u && minute == 0u && second == 0u) {
            ++runtime_fat_days;
        }
        runtime_fat_time = (uint16_t)(((uint16_t)hour << 11) |
                                     ((uint16_t)minute << 5) | (second >> 1));
    }
}

uint16_t spectrum_net_runtime_fat_date(void)
{
    return runtime_fat_date;
}

uint16_t spectrum_net_runtime_fat_time(void)
{
    return runtime_fat_time;
}

uint16_t spectrum_net_runtime_fat_elapsed_days(void)
{
    return runtime_fat_days;
}
