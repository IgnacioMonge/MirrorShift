#include "spectrum/platform/net_runtime.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char call_order[4];
static uint8_t call_order_len;

static void record_call(char call)
{
    if (call_order_len < sizeof(call_order)) {
        call_order[call_order_len] = call;
    }
    ++call_order_len;
}

void spectrum_frame_wait(void)
{
    record_call('F');
}

void spectrum_uart_background_pump(void)
{
    record_call('U');
}

void spectrum_net_background_drain(void)
{
    record_call('D');
}

void spectrum_net_background_drain_clock(void)
{
    record_call('D');
}

void spectrum_gui_tick(void)
{
    record_call('G');
}

void spectrum_gui_set_clock(uint8_t hour, uint8_t minute, uint8_t second)
{
    (void)hour;
    (void)minute;
    (void)second;
}

static void check_order(const char *expected, const char *label)
{
    size_t expected_len = strlen(expected);

    if (call_order_len != expected_len ||
        memcmp(call_order, expected, expected_len) != 0) {
        fprintf(stderr, "%s: got %.*s want %s\n",
                label,
                (int)call_order_len,
                call_order,
                expected);
        exit(1);
    }
}

static void reset_call_order(void)
{
    memset(call_order, 0, sizeof(call_order));
    call_order_len = 0u;
}

static void test_wait_frame_order(void)
{
    reset_call_order();
    spectrum_net_runtime_wait_frame();
    check_order("FDG", "normal wait order");
}

static void test_wait_frame_plain_order(void)
{
    reset_call_order();
    spectrum_net_runtime_wait_frame_plain();
    check_order("FUG", "plain wait order");
}

int main(void)
{
    if (spectrum_net_runtime_clock_ready()) {
        return 1;
    }
    spectrum_net_runtime_set_clock(12u, 34u, 56u);
    if (!spectrum_net_runtime_clock_ready()) {
        return 1;
    }
    spectrum_net_runtime_tick_clock(0u, 0u, 0u);
    assert(spectrum_net_runtime_fat_elapsed_days() == 0u);
    spectrum_net_runtime_set_fat_stamp(0x5c9fu, 0u);
    spectrum_net_runtime_tick_clock(23u, 59u, 59u);
    assert(spectrum_net_runtime_fat_time() == 0xbf7du);
    assert(spectrum_net_runtime_fat_elapsed_days() == 0u);
    spectrum_net_runtime_tick_clock(0u, 0u, 0u);
    spectrum_net_runtime_tick_clock(0u, 0u, 1u);
    spectrum_net_runtime_tick_clock(0u, 0u, 0u);
    assert(spectrum_net_runtime_fat_elapsed_days() == 2u);
    assert(spectrum_net_runtime_fat_date() == 0x5c9fu);
    spectrum_net_runtime_set_fat_stamp(0x5ca2u, 0x1000u);
    assert(spectrum_net_runtime_fat_elapsed_days() == 0u);
    test_wait_frame_order();
    test_wait_frame_plain_order();
    return 0;
}
