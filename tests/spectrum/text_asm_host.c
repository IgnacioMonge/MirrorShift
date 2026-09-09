#include "spectrum/platform/text.h"

#include <stdint.h>

char *spectrum_append_text(char *dst, const char *src)
{
    while ((*dst = *src) != '\0') {
        ++dst;
        ++src;
    }
    return dst;
}

static uint8_t count_digit(uint16_t *value, uint16_t place)
{
    uint8_t digit = 0u;

    while (*value >= place) {
        *value = (uint16_t)(*value - place);
        ++digit;
    }
    return digit;
}

char *spectrum_append_u16(char *dst, uint16_t value)
{
    static const uint16_t places[] = { 10000u, 1000u, 100u, 10u };
    uint8_t emitted = 0u;
    uint8_t i;

    for (i = 0u; i < (uint8_t)(sizeof(places) / sizeof(places[0])); ++i) {
        uint8_t digit = count_digit(&value, places[i]);

        if (digit != 0u || emitted) {
            *dst++ = (char)('0' + digit);
            emitted = 1u;
        }
    }
    *dst++ = (char)('0' + (uint8_t)value);
    *dst = '\0';
    return dst;
}

#ifdef NETCHESSZX_HOST_SESSION_TEST
uint8_t spectrum_overlay_exec_cached(uint8_t ovl_id, uint8_t entry_id)
{
    (void)ovl_id;
    (void)entry_id;
    return 1u;
}

uint8_t netchesszx_setup_time_commit(void)
{
    return 1u;
}

uint8_t netchesszx_config_save_overlay(void)
{
    return 1u;
}
#endif
