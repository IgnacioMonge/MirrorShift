#include "spectrum/transport/net.h"

#include "spectrum/platform/spectranext_lowram.h"
#include "spectrum/transport/mqtt_min.h"

#ifndef NETCHESSZX_HOST_TEST
#include "spxn.h"
#define DIRECT_PENDING_CURSOR \
    (*(uint8_t *)NETCHESSZX_LOWRAM_DIRECT_PENDING_CURSOR_ADDR)
#define DIRECT_PENDING_LENGTH \
    (*(uint8_t *)NETCHESSZX_LOWRAM_DIRECT_PENDING_LENGTH_ADDR)
#define DIRECT_SCRATCH ((void *)NETCHESSZX_LOWRAM_DIRECT_STAGE_ADDR)
#else
#define DIRECT_PENDING_CURSOR spxn_direct_pending_cursor
#define DIRECT_PENDING_LENGTH spxn_direct_pending_length
#define DIRECT_SCRATCH spxn_direct_scratch
#endif

typedef char direct_stage_capacity_check[
    NETCHESSZX_LOWRAM_DIRECT_STAGE_SIZE >= SPECTRUM_MQTT_PACKET_MAX ? 1 : -1];

extern uint8_t direct_link_closed;

void spectrum_net_direct_background_drain(void)
{
    uint8_t events;
    uint8_t got;

    if (DIRECT_PENDING_LENGTH != 0u) {
        return;
    }
    /* SPXN errors are -1..-8: their low byte carries POLLNVAL. RECV errors
       likewise fall above the valid 0..160 byte range. */
    events = (uint8_t)spxn_poll();
    if ((events & SPXN_POLLNVAL) != 0u) {
        direct_link_closed = 1u;
        return;
    }
    if ((events & SPXN_POLLIN) != 0u) {
        got = (uint8_t)spxn_recv(DIRECT_SCRATCH,
                                 SPECTRUM_MQTT_PACKET_MAX);
        if (got == 0u || got > SPECTRUM_MQTT_PACKET_MAX) {
            direct_link_closed = 1u;
            return;
        }
        DIRECT_PENDING_LENGTH = got;
        return;
    }
    if ((events & SPXN_POLLHUP) != 0u) {
        direct_link_closed = 1u;
    }
}
