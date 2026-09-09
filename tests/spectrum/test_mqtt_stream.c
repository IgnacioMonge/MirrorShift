#include <stdio.h>
#include <string.h>

#ifndef __at
#define __at(address)
#endif

#include "spectrum/transport/mqtt_min.h"
static uint8_t host_mqtt_packet[SPECTRUM_MQTT_PACKET_MAX];
#undef SPECTRUM_MQTT_PACKET_SCRATCH
#define SPECTRUM_MQTT_PACKET_SCRATCH host_mqtt_packet
#ifdef NETCHESSZX_NEXT
#include "spectrum/overlay/overlay_context.h"
static volatile uint8_t host_overlay_context[32];
#undef spectrum_overlay_context
#define spectrum_overlay_context host_overlay_context
#endif
#include "spectrum/transport/net.c"
#include "spectrum/transport/mqtt_min.c"

static int failures;
static uint8_t at_ok;
static uint8_t send_ok;
static uint8_t prompt_ok;
static unsigned ensure_calls;
static const uint8_t *uart_data;
static uint16_t uart_left;
#ifdef NETCHESSZX_NEXT
static uint8_t uart_fault;
static uint8_t inject_uart_fault;
uint8_t spectrum_uart_error(void) { return uart_fault; }
uint8_t spectrum_overlay_exec_cached(uint8_t overlay_id, uint8_t entry_id)
{
    (void)overlay_id;
    (void)entry_id;
    uart_fault |= inject_uart_fault;
    return 0u;
}
#endif
#ifdef NETCHESSZX_SPECTRANEXT
static int16_t poll_flags;
int16_t spxn_poll(void) { return uart_left ? SPXN_POLLIN | poll_flags : poll_flags; }
int16_t spxn_recv(void *buffer, uint16_t maximum)
{
    uint16_t count = uart_left < maximum ? uart_left : maximum;
    memcpy(buffer, uart_data, count);
    uart_data += count;
    uart_left -= count;
    return (int16_t)count;
}
void spxn_close(void) { ++ensure_calls; }
int16_t spxn_send_all(const void *buffer, uint16_t length, uint16_t budget)
{
    (void)buffer; (void)length; (void)budget;
    return SPXN_OK;
}
#endif

void spectrum_net_runtime_wait_frame_plain(void) {}

uint8_t spectrum_uart_ready(void)
{
#ifdef NETCHESSZX_NEXT
    uart_fault |= inject_uart_fault;
#endif
    return uart_left != 0u;
}
uint8_t spectrum_uart_read(void)
{
    --uart_left;
    return *uart_data++;
}

uint8_t spectrum_net_at_cmd(const char *cmd, uint16_t frames)
{
    (void)cmd;
    (void)frames;
    return at_ok;
}

uint8_t spectrum_net_ensure_command_mode(void)
{
    ++ensure_calls;
    return 1u;
}

uint8_t spectrum_uart_send_string(const char *text)
{
    (void)text;
    return send_ok;
}

uint8_t spectrum_uart_send_crlf(void)
{
    return send_ok;
}

uint8_t spectrum_uart_send_bytes(const uint8_t *data, uint8_t len)
{
    (void)data;
    (void)len;
    return 1u;
}

uint8_t wait_for_prompt(uint16_t frames)
{
    (void)frames;
    return prompt_ok;
}

void reset_line_buf(void)
{
}

static void check(int ok, const char *label)
{
    if (!ok) {
        printf("FAIL: %s\n", label);
        ++failures;
    }
}

static void set_stream(const uint8_t *data, uint8_t len)
{
    memcpy(MQTT_STREAM, data, len);
    mqtt_stream_len = len;
}

static void test_garbage_compacts_once_to_packet(void)
{
    static const uint8_t stream[] = { 0x01u, 0xffu, 0x30u, 0x02u, 0u, 1u };

    set_stream(stream, sizeof(stream));
    check(mqtt_take_stream_packet() == 4, "garbage: finds MQTT packet");
    check(mqtt_stream_len == 4u, "garbage: discarded prefix once");
    check(MQTT_STREAM[0] == 0x30u && MQTT_STREAM[3] == 1u,
          "garbage: packet stays intact");
}

static void test_garbage_keeps_fragment_start(void)
{
    static const uint8_t stream[] = { 0x01u, 0xffu, 0xd0u };

    set_stream(stream, sizeof(stream));
    check(mqtt_take_stream_packet() == 0, "fragment: waits for final byte");
    check(mqtt_stream_len == 1u && MQTT_STREAM[0] == 0xd0u,
          "fragment: keeps possible packet start");
    MQTT_STREAM[mqtt_stream_len++] = 0u;
    check(mqtt_take_stream_packet() == 2, "fragment: completes PINGRESP");
}

static void test_full_stream_consumes_packet_and_preserves_trailing(void)
{
    uint8_t i;

    /* PUBLISH with a two-byte remaining length: 3-byte header + 157 bytes. */
    MQTT_STREAM[0] = 0x30u;
    MQTT_STREAM[1] = 0x9du;
    MQTT_STREAM[2] = 0x01u;
    for (i = 3u; i < 160u; ++i) {
        MQTT_STREAM[i] = (uint8_t)(0x40u + (uint8_t)(i - 3u));
    }
    for (i = 160u; i < MQTT_STREAM_MAX; ++i) {
        MQTT_STREAM[i] = (uint8_t)(0xc0u + (uint8_t)(i - 160u));
    }
    mqtt_stream_len = MQTT_STREAM_MAX;

    check(mqtt_take_stream_packet() == 160, "full stream: takes 160-byte packet");
    mqtt_consume_stream_packet(160u);
    check(mqtt_stream_len == 63u, "full stream: leaves trailing bytes");
    for (i = 0u; i < 63u; ++i) {
        check(MQTT_STREAM[i] == (uint8_t)(0xc0u + i),
              "full stream: trailing bytes stay ordered");
    }
}

static void test_malformed_two_byte_remaining_length_resets(void)
{
    uint8_t i;

    for (i = 0u; i < MQTT_STREAM_MAX; ++i) {
        MQTT_STREAM[i] = 0u;
    }
    MQTT_STREAM[0] = 0x30u;
    MQTT_STREAM[1] = 0x80u;
    MQTT_STREAM[2] = 0x80u;
    mqtt_stream_len = MQTT_STREAM_MAX;

    check(mqtt_take_stream_packet() < 0,
          "malformed length: rejects at full capacity");
    check(mqtt_stream_len == 0u, "malformed length: resets stream");
}

static void test_oversized_packet_preserves_boundary(void)
{
    uint8_t stream[200];

    mqtt_reset_session_state();
    mqtt_stream_active = 1u;
    stream[0] = 0x30u;
    stream[1] = 0xa1u;
    stream[2] = 1u; /* 161 body bytes, above the packet capacity. */
    memset(stream + 3, 0xd0, 161u);
    stream[164] = 0xd0u;
    stream[165] = 0u;
    set_stream(stream, 166u);
    check(mqtt_take_stream_packet() == 0 && mqtt_stream_len == 2u,
          "oversize: discards only the complete body");
    check(mqtt_take_stream_packet() == 2,
          "oversize: preserves following PINGRESP");

    set_stream(stream, 103u);
    check(mqtt_take_stream_packet() == 0,
          "fragmented oversize: starts discard");
    /* Repeated parser calls must not interpret the discard counter or stale
       body as buffered bytes. Body intentionally resembles MQTT packets. */
    check(mqtt_take_stream_packet() == 0 && mqtt_stream_len == 61u,
          "fragmented oversize: parser leaves discard state intact");
    memset(stream, 0, 61u);
    stream[0] = 0xd0u;
    stream[40] = 0xd0u;
    stream[61] = 0xd0u;
    stream[62] = 0u;
    uart_data = stream;
    uart_left = 63u;
    check(mqtt_drain_uart_budget(40u) == 1u && uart_left == 23u &&
              mqtt_take_stream_packet() == 0 && mqtt_stream_len == 21u,
          "fragmented oversize: ignores fake packets in middle fragment");
    check(mqtt_drain_uart_budget(23u) == 1u,
          "fragmented oversize: drains final body fragment");
    (void)mqtt_drain_uart_budget(2u);
    check(uart_left == 0u &&
              mqtt_stream_len == 2u && mqtt_take_stream_packet() == 2,
          "fragmented oversize: resumes at exact boundary");
}

#ifndef NETCHESSZX_SPECTRANEXT
static void test_stream_entry_failure_restores_command_mode(void)
{
    at_ok = 0u;
    send_ok = 1u;
    prompt_ok = 1u;
    ensure_calls = 0u;
    mqtt_stream_active = 1u;
    mqtt_stream_len = 7u;

    check(!mqtt_enter_stream_mode(), "entry failure: reports failure");
    check(ensure_calls == 1u, "entry failure: restores command mode");
    check(!mqtt_stream_active && mqtt_stream_len == 0u,
          "entry failure: clears software stream state");

    at_ok = 1u;
    send_ok = 0u;
    ensure_calls = 0u;
    check(!mqtt_enter_stream_mode(), "send failure: reports failure");
    check(ensure_calls == 1u, "send failure: restores command mode");

    send_ok = 1u;
    prompt_ok = 0u;
    ensure_calls = 0u;
    check(!mqtt_enter_stream_mode(), "prompt failure: reports failure");
    check(ensure_calls == 1u, "prompt failure: restores command mode");

    prompt_ok = 1u;
    ensure_calls = 0u;
    check(mqtt_enter_stream_mode(), "entry success: enters stream mode");
    check(ensure_calls == 0u && mqtt_stream_active,
          "entry success: leaves command recovery idle");
}
#else
static void test_spectranext_poll_terminal_flags(void)
{
    uint8_t bytes[100];

    memset(bytes, 0xd0, sizeof(bytes));
    mqtt_reset_session_state();
    mqtt_stream_active = MQTT_STREAM_ACTIVE;
    uart_data = bytes;
    uart_left = sizeof(bytes);
    poll_flags = SPXN_POLLHUP;
    ensure_calls = 0u;
    check(mqtt_drain_uart_budget(64u) == 1u && uart_left == 36u &&
              mqtt_stream_active == MQTT_STREAM_ACTIVE,
          "Spectranext HUP: first readable batch remains active");
    check(mqtt_drain_uart_budget(64u) == 1u && uart_left == 0u &&
              mqtt_stream_len == sizeof(bytes) &&
              mqtt_stream_active == MQTT_STREAM_ACTIVE,
          "Spectranext HUP: all readable bytes are drained");
    check(mqtt_drain_uart_budget(64u) == 2u && !mqtt_stream_active &&
              ensure_calls == 1u,
          "Spectranext HUP: closes after readable data is exhausted");

    mqtt_reset_session_state();
    mqtt_stream_active = MQTT_STREAM_ACTIVE;
    poll_flags = SPXN_POLLNVAL;
    ensure_calls = 0u;
    check(mqtt_drain_uart_budget(64u) == 2u && !mqtt_stream_active &&
              ensure_calls == 1u,
          "Spectranext NVAL: positive invalid descriptor closes");

    mqtt_reset_session_state();
    mqtt_stream_active = MQTT_STREAM_ACTIVE;
    uart_data = bytes;
    uart_left = sizeof(bytes);
    poll_flags = SPXN_POLLNVAL | SPXN_POLLHUP;
    ensure_calls = 0u;
    check(mqtt_drain_uart_budget(64u) == 2u && !mqtt_stream_active &&
              ensure_calls == 1u && uart_left == sizeof(bytes),
          "Spectranext NVAL: invalid descriptor is never read even with IN/HUP");

    mqtt_reset_session_state();
    mqtt_stream_active = MQTT_STREAM_ACTIVE;
    uart_left = 0u;
    poll_flags = -1;
    ensure_calls = 0u;
    check(mqtt_drain_uart_budget(64u) == 2u && !mqtt_stream_active &&
              ensure_calls == 1u,
          "Spectranext poll error: negative result closes");
}
#endif

static void test_connect_publish_queue_is_fifo_and_tx_safe(void)
{
    static const char *payloads[] = { "META", "PEER", "OWN" };
    uint8_t packet[SPECTRUM_MQTT_PACKET_MAX];
    char payload[SPECTRUM_NET_PAYLOAD_MAX];
    uint8_t i;

    mqtt_reset_session_state();
    for (i = 0u; i < MQTT_PUBLISH_QUEUE_COUNT; ++i) {
        uint8_t len = spectrum_mqtt_publish(
            packet, sizeof(packet), (uint16_t)(i + 1u),
            "netchesszx/v1/ROOM/pres_w", payloads[i], 1u);

        check(len != 0u && mqtt_stash_packet(packet, len),
              "connect queue: accepts interleaved publish");
    }
    check(mqtt_pending_len == 0u,
          "connect queue: never aliases MQTT TX scratch");
    memset(MQTT_PACKET, 0xa5, SPECTRUM_MQTT_PACKET_MAX);
    for (i = 0u; i < MQTT_PUBLISH_QUEUE_COUNT; ++i) {
        check(mqtt_read_queued_payload(payload, sizeof(payload)) == 0 &&
                  strcmp(payload, payloads[i]) == 0,
              "connect queue: FIFO survives MQTT TX scratch overwrite");
        check((spectrum_net_payload_flags() &
               SPECTRUM_LINK_PAYLOAD_RETAINED) != 0u,
              "connect queue: retained flag survives");
    }
    check(mqtt_publish_queue_count == 0u,
          "connect queue: drains completely");
}

#ifdef NETCHESSZX_NEXT
static void test_uart_discontinuity(void)
{
    static const uint8_t reply[] = {0xd0u, 0u};
    uint8_t packet[SPECTRUM_MQTT_PACKET_MAX];
    char payload[32] = "old";
    uint8_t len;

    mqtt_reset_session_state();
    mqtt_stream_active = 1u;
    uart_data = reply;
    uart_left = sizeof(reply);
    uart_fault = 0x04u;
    check(mqtt_read_stream_packet() == -2 && uart_left == sizeof(reply),
          "RX error before drain rejects packet without consuming bytes");

    uart_fault = 0u;
    inject_uart_fault = 0x40u;
    check(mqtt_read_stream_packet() == -2,
          "RX error discovered during drain rejects packet");

    inject_uart_fault = uart_fault = 0u;
    mqtt_reset_session_state();
    len = spectrum_mqtt_publish(packet, sizeof(packet), 1u,
                                "netchesszx/v1/ROOM/pres_w", "QUEUED", 1u);
    check(len != 0u && mqtt_stash_packet(packet, len),
          "queued fault fixture is valid");
    uart_fault = 0x04u;
    check(spectrum_net_mqtt_read_payload(payload, sizeof(payload)) == -2 &&
              mqtt_publish_queue_count == 1u,
          "RX error before queued payload rejects without consuming it");
    uart_fault = 0u;
    check(spectrum_net_mqtt_read_payload(payload, sizeof(payload)) == 0 &&
              strcmp(payload, "QUEUED") == 0,
          "queued payload survives a cleared fault boundary");

    inject_uart_fault = 0x40u;
    check(direct_read_payload(payload, sizeof(payload)) == -2 &&
              payload[0] == '\0',
          "DIRECT checks RX error after overlay and suppresses payload");
    inject_uart_fault = uart_fault = 0u;
    mqtt_reset_session_state();
}
#endif

int main(void)
{
    test_garbage_compacts_once_to_packet();
    test_garbage_keeps_fragment_start();
    test_full_stream_consumes_packet_and_preserves_trailing();
    test_malformed_two_byte_remaining_length_resets();
    test_oversized_packet_preserves_boundary();
#ifndef NETCHESSZX_SPECTRANEXT
    test_stream_entry_failure_restores_command_mode();
#else
    test_spectranext_poll_terminal_flags();
#endif
    test_connect_publish_queue_is_fifo_and_tx_safe();
#ifdef NETCHESSZX_NEXT
    test_uart_discontinuity();
#endif

    if (failures != 0) {
        printf("%d MQTT stream tests failed\n", failures);
        return 1;
    }
    puts("MQTT stream tests passed");
    return 0;
}
