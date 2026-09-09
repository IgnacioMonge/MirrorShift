#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "spectrum/config/session.h"

static void expect_session(uint8_t role,
                           uint8_t transport,
                           uint8_t host_color,
                           uint8_t local_color,
                           uint8_t host_color_ready)
{
    assert(netchesszx_session_role == role);
    assert(netchesszx_transport == transport);
    assert(netchesszx_host_color == host_color);
    assert(netchesszx_local_color == local_color);
    assert(netchesszx_host_color_ready == host_color_ready);
}

int main(void)
{
    expect_session(NETCHESSZX_SESSION_ROLE_HOST,
                   NETCHESSZX_TRANSPORT_MQTT,
                   NETCHESSZX_COLOR_BLACK,
                   NETCHESSZX_COLOR_BLACK,
                   1u);

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_BLACK);
    expect_session(NETCHESSZX_SESSION_ROLE_HOST,
                   NETCHESSZX_TRANSPORT_MQTT,
                   NETCHESSZX_COLOR_BLACK,
                   NETCHESSZX_COLOR_BLACK,
                   1u);

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_BLACK);
    expect_session(NETCHESSZX_SESSION_ROLE_JOIN,
                   NETCHESSZX_TRANSPORT_MQTT,
                   NETCHESSZX_COLOR_BLACK,
                   NETCHESSZX_COLOR_WHITE,
                   0u);

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    expect_session(NETCHESSZX_SESSION_ROLE_JOIN,
                   NETCHESSZX_TRANSPORT_MQTT,
                   NETCHESSZX_COLOR_WHITE,
                   NETCHESSZX_COLOR_BLACK,
                   0u);

    netchesszx_session_configure(99u, 99u, 99u);
    expect_session(NETCHESSZX_SESSION_ROLE_HOST,
                   NETCHESSZX_TRANSPORT_MQTT,
                   NETCHESSZX_COLOR_BLACK,
                   NETCHESSZX_COLOR_BLACK,
                   1u);

    puts("session config tests ok");
    return 0;
}
