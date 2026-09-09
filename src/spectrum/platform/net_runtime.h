#ifndef NETCHESSZX_SPECTRUM_NET_RUNTIME_H
#define NETCHESSZX_SPECTRUM_NET_RUNTIME_H

#include <stdint.h>

/* Narrow transport -> app bridge. It lives under platform/ so transport can call
   timing/UI composition hooks without including app/ or ui/ directly. */
void spectrum_net_runtime_wait_frame(void);
void spectrum_net_runtime_wait_frame_plain(void);
void spectrum_net_runtime_set_clock(uint8_t hour, uint8_t minute, uint8_t second);
uint8_t spectrum_net_runtime_clock_ready(void);
#ifdef NETCHESSZX_SPECTRANEXT
uint8_t spectrum_net_runtime_shift_clock(int8_t hour_delta);
#endif
void spectrum_net_runtime_set_fat_stamp(uint16_t date, uint16_t time);
void spectrum_net_runtime_tick_clock(uint8_t hour, uint8_t minute,
                                     uint8_t second);
uint16_t spectrum_net_runtime_fat_date(void);
uint16_t spectrum_net_runtime_fat_time(void);
/* Days since fat_date; SAVELOAD resolves them and resets through set_fat_stamp. */
uint16_t spectrum_net_runtime_fat_elapsed_days(void);

#endif
