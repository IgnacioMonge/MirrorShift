#include "spectrum/platform/uart.h"

#include <stdio.h>
#include <stdlib.h>

static uint8_t tx_failures;
static uint8_t tx_always_fail;
static uint8_t tx_calls;
static uint8_t rx_fifo[256];
static uint16_t rx_head;
static uint16_t rx_tail;
static uint16_t rx_read_calls;
static uint8_t rx_inject_on_send;
#ifdef NETCHESSZX_NEXT
uint8_t net_uart_rx_error;
static uint8_t inject_tx_error;
static uint8_t inject_rx_error;
static uint8_t inject_read_error;
void net_uart_hard_reset(void) {}
void net_uart_set_baud_230400(void) {}
#endif

static void check(uint8_t condition, const char *message);
static uint8_t rx_value(uint16_t index);

void net_uart_init(void)
{
    rx_head = rx_tail = 0u;
#ifdef NETCHESSZX_NEXT
    net_uart_rx_error = 0u;
#endif
}

uint8_t net_uart_send(uint8_t c)
{
    (void)c;
    ++tx_calls;
#ifdef NETCHESSZX_NEXT
    net_uart_rx_error |= inject_tx_error;
#endif
    if (rx_inject_on_send) {
        rx_fifo[rx_tail++ & 0xffu] = rx_value((uint16_t)(tx_calls - 1u));
    }
    if (tx_always_fail) {
        return 1u;
    }
    if (tx_failures != 0u) {
        --tx_failures;
        return 1u;
    }
    return 0u;
}

uint8_t net_uart_ready(void)
{
#ifdef NETCHESSZX_NEXT
    net_uart_rx_error |= inject_rx_error;
#endif
    return (uint8_t)(rx_head != rx_tail);
}

uint8_t net_uart_read(void)
{
#ifdef NETCHESSZX_NEXT
    net_uart_rx_error |= inject_read_error;
#endif
    ++rx_read_calls;
    return rx_fifo[rx_head++ & 0xffu];
}

static uint8_t rx_value(uint16_t index)
{
    return (uint8_t)(19u + (uint8_t)(index * 73u));
}

static void test_tx_pumps_concurrent_rx(void)
{
    static const uint8_t data[] = {'A', 'B'};

    spectrum_uart_init();
    tx_calls = 0u;
    tx_failures = 0u;
    tx_always_fail = 0u;
    rx_head = 0u;
    rx_tail = 0u;
    rx_read_calls = 0u;
    rx_inject_on_send = 1u;

    check(spectrum_uart_send_bytes(data, sizeof(data)),
          "TX pump: concurrent send succeeds");
    rx_inject_on_send = 0u;
    check(rx_read_calls == 2u, "TX pump: drains RX after each byte");
    check(spectrum_uart_read() == rx_value(0u),
          "TX pump: preserves first concurrent byte");
    check(spectrum_uart_read() == rx_value(1u),
          "TX pump: preserves second concurrent byte");
}

static void test_rx_ring_preserves_full_burst(void)
{
    uint16_t i;

    spectrum_uart_init();
    rx_head = 0u;
    rx_tail = 256u;
    rx_read_calls = 0u;
    for (i = 0u; i < 256u; ++i) {
        rx_fifo[i] = rx_value(i);
    }

    spectrum_uart_background_pump();
    check(rx_read_calls == 255u, "RX ring: fills 255 usable slots");
    check(spectrum_uart_ready(), "RX ring: remains ready while full");
    spectrum_uart_background_pump();
    check(rx_read_calls == 255u, "RX ring: full pump does not consume backend");

    for (i = 0u; i < 255u; ++i) {
        check(spectrum_uart_read() == rx_value(i),
              "RX ring: drains in exact order");
    }
    check(rx_read_calls == 255u, "RX ring: drain does not duplicate backend read");

    spectrum_uart_background_pump();
    check(rx_read_calls == 256u, "RX ring: moves pending byte after freeing slot");
    check(spectrum_uart_read() == rx_value(255u),
          "RX ring: pending byte arrives exactly once");
    check(!spectrum_uart_ready(), "RX ring: no bytes remain");
}

#ifdef NETCHESSZX_NEXT
static void test_next_uart_fault_boundaries(void)
{
    spectrum_uart_init();
    rx_tail = 4u;
    spectrum_uart_background_pump();
    net_uart_rx_error = 0x04u;
    check(!spectrum_uart_ready(), "fault suppresses buffered bytes");
    check(!spectrum_uart_send_string("A"), "fault prevents TX");
    check(spectrum_uart_error() == 0x04u, "fault remains sticky");
    spectrum_uart_flush(0u);
    check(spectrum_uart_error() == 0x04u,
          "zero-frame flush retains hardware fault");

    spectrum_uart_set_baud_230400();
    check(!spectrum_uart_error() && !spectrum_uart_ready(),
          "baud probe initialization clears ring and fault");

    rx_tail = 4u;
    inject_rx_error = 0x40u;
    check(!spectrum_uart_ready(),
          "fault discovered during status suppresses ready");
    inject_rx_error = 0u;
    spectrum_uart_flush(1u);
    check(!spectrum_uart_error() && !spectrum_uart_ready(),
          "positive flush discards bytes and clears fault");

    inject_tx_error = 0x04u;
    tx_calls = 0u;
    check(!spectrum_uart_send_string("AB") && tx_calls == 1u,
          "fault discovered during TX aborts remaining bytes");
    inject_tx_error = 0u;
    check(spectrum_uart_error() == 0x04u, "TX poll retains RX fault");

    spectrum_uart_init();
    rx_fifo[0] = 'Q';
    rx_tail = 1u;
    inject_read_error = 0x40u;
    check(spectrum_uart_read() == 0u && spectrum_uart_error() == 0x40u,
          "uncached read suppresses byte when read detects error");
    inject_read_error = 0u;

    spectrum_uart_init();
    rx_inject_on_send = 1u;
    inject_rx_error = 0x04u;
    tx_calls = 0u;
    rx_read_calls = 0u;
    check(!spectrum_uart_send_string("AB") && tx_calls == 1u,
          "fault discovered during TX RX-pump aborts remaining bytes");
    check(rx_read_calls == 0u,
          "faulted TX pump does not consume the discontinuous byte");
    rx_inject_on_send = 0u;
    inject_rx_error = 0u;
    spectrum_uart_init();
}
#endif

static void check(uint8_t condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

int main(void)
{
    static const uint8_t data[] = {'A', 'B'};

    check(spectrum_uart_send_string("AB"), "string TX succeeds");
    check(tx_calls == 2u, "string TX byte count");

    tx_calls = 0u;
    tx_failures = 4u;
    check(!spectrum_uart_send_string("A"), "string TX failure propagates");
    check(tx_calls == 4u, "string TX failure retry count");

    tx_calls = 0u;
    tx_failures = 0u;
    check(spectrum_uart_send_crlf(), "CRLF TX succeeds");
    check(tx_calls == 2u, "CRLF TX byte count");

    tx_calls = 0u;
    tx_failures = 3u;
    check(spectrum_uart_send_bytes(data, sizeof(data)),
          "transient TX busy recovers");
    check(tx_calls == 5u, "transient retry count");

    tx_calls = 0u;
    tx_always_fail = 1u;
    check(!spectrum_uart_send_bytes(data, sizeof(data)),
          "persistent TX busy fails");
    check(tx_calls == 4u, "persistent failure aborts first byte");
    tx_always_fail = 0u;

    test_tx_pumps_concurrent_rx();
    test_rx_ring_preserves_full_burst();
#ifdef NETCHESSZX_NEXT
    test_next_uart_fault_boundaries();
#endif

    puts("uart platform tests ok");
    return 0;
}
