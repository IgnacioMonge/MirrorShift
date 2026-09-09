#include "spectrum/overlay/overlay_api.h"
#include "spectrum/config/session.h"
#include "spectrum/transport/fat_timezone.h"
#include "spectrum/transport/fat_clock.h"
#include "spectrum/transport/mqtt_min.h"
#include "spectrum/transport/net.h"
#if defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
#include "spectrum/lowram_map.h"
#endif
#ifdef NETCHESSZX_SPECTRANEXT
#include "spxtime.h"

#ifdef NETCHESSZX_TIME_HOST_TOKEN
#define NETCHESSZX_TIME_HOST NETCHESSZX_STRINGIFY(NETCHESSZX_TIME_HOST_TOKEN)
#else
#define NETCHESSZX_TIME_HOST "time.google.com"
#endif

typedef char time_host_capacity_check[
    sizeof(NETCHESSZX_TIME_HOST) <= NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE
        ? 1 : -1];

extern void net_wait_frame(void);

void spxtime_idle(void)
{
    net_wait_frame();
}

uint8_t spectranext_time_shift_ovl(uint8_t *ctx) __z88dk_fastcall
{
    int8_t offset = (int8_t)ctx[0];
    uint16_t date = spectrum_net_runtime_fat_date();
    uint16_t time = spectrum_net_runtime_fat_time();
    uint16_t days = spectrum_net_runtime_fat_elapsed_days();
    uint16_t midnight;
    uint8_t hour = (uint8_t)(time >> 11);

    if (date == 0u || !spectrum_gui_shift_clock(offset)) {
        return 0u;
    }
    while (days != 0u) {
        spectrum_fat_tick_clock(&date, &midnight, 0u, 0u, 0u);
        --days;
    }
    spectrum_fat_apply_hour_offset(&hour, &date, &time, offset);
    spectrum_net_runtime_set_fat_stamp(date, time);
    return 1u;
}

uint8_t spectranext_time_sync_ovl(void)
{
    struct spxtime_result result;
    int8_t timezone = netchesszx_timezone;
    struct spxtime_request request;
#if !defined(NETCHESSZX_HOST_TEST)
    char *host = (char *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR;

    spectrum_append_text(host, NETCHESSZX_TIME_HOST);
#else
    const char *host = NETCHESSZX_TIME_HOST;
#endif

    request.host = host;
    request.scratch = SPECTRUM_MQTT_PACKET_SCRATCH;
    request.scratch_size = SPECTRUM_MQTT_PACKET_MAX;
    request.timeout_ticks = 500u;
    request.out = &result;

    if (spxtime_sntp(&request) != SPXN_OK) return 0u;
    netchesszx_rtc_available = 0u;
    if (timezone == NETCHESSZX_TIME_RTC) {
        timezone = netchesszx_timezone_last;
        netchesszx_timezone = timezone;
    }
    if (timezone < NETCHESSZX_TIMEZONE_MIN ||
        timezone > NETCHESSZX_TIMEZONE_MAX) {
        timezone = 0;
    }
    spectrum_fat_apply_hour_offset(&result.hour, &result.fat_date,
                                   &result.fat_time, timezone);
    spectrum_net_runtime_set_clock(result.hour, result.minute, result.second);
    spectrum_net_runtime_set_fat_stamp(result.fat_date, result.fat_time);
    return 1u;
}
#else
uint8_t spectranext_time_sync_ovl(void)
{
    return 0u;
}
#endif
