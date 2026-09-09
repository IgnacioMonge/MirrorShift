#ifndef NETCHESSZX_SPECTRUM_MSDOS_TIME_H
#define NETCHESSZX_SPECTRUM_MSDOS_TIME_H

/* Shared MS-DOS date/time validator used by the MQTT_TX overlay (runtime RTC)
   and the host test build. Static-in-header so it never lands in resident. */

#include <stdint.h>

/* Includer must declare spectrum_net_runtime_set_clock/set_fat_stamp first
   (esp_at.c via net_runtime.h, mqtt_tx_ovl.c via overlay_api.h); including
   net_runtime.h here trips the transport-net-runtime-bridge-only layering
   rule. */

/* Pointer input keeps the Next overlay independent of an incidental resident
   ____sdcc_4_push_hlix helper. Byte-wise field extraction is also smaller than
   16-bit shifts. */
static uint8_t capture_msdos_time_bytes_mode(const volatile uint8_t raw[4],
                                              uint8_t apply)
{
    uint8_t dl = raw[0];
    uint8_t dh = raw[1];
    uint8_t tl = raw[2];
    uint8_t th = raw[3];
    uint16_t date = (uint16_t)(((uint16_t)dh << 8) | dl);
    uint16_t time = (uint16_t)(((uint16_t)th << 8) | tl);
    uint8_t month = (uint8_t)(((dh & 1u) << 3) | (dl >> 5));
    uint8_t minute = (uint8_t)(((th & 7u) << 3) | (tl >> 5));
    uint8_t sec2 = (uint8_t)(tl & 31u);

    if (dh < 80u || dh >= 144u) { /* year 2020..2051 */
        return 0u;
    }
    if (month == 0u || month > 12u) {
        return 0u;
    }
    if ((dl & 31u) == 0u) { /* day */
        return 0u;
    }
    if (th >= 192u) { /* hour < 24 */
        return 0u;
    }
    if (minute >= 60u) {
        return 0u;
    }
    if (sec2 >= 30u) {
        return 0u;
    }
    if (apply) {
        spectrum_net_runtime_set_clock((uint8_t)(th >> 3), minute,
                                       (uint8_t)(sec2 << 1));
        spectrum_net_runtime_set_fat_stamp(date, time);
    }
    return 1u;
}

static uint8_t capture_msdos_time_bytes(const volatile uint8_t raw[4])
{
    return capture_msdos_time_bytes_mode(raw, 1u);
}

#ifdef NETCHESSZX_HOST_TEST
static uint8_t capture_msdos_time(uint16_t date, uint16_t time)
{
    uint8_t raw[4];

    raw[0] = (uint8_t)date;
    raw[1] = (uint8_t)(date >> 8);
    raw[2] = (uint8_t)time;
    raw[3] = (uint8_t)(time >> 8);
    return capture_msdos_time_bytes(raw);
}
#endif

#endif
