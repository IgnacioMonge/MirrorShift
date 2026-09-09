#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define NETCHESSZX_SPECTRANEXT 1
#define __z88dk_fastcall

#define SPXN_OK         0
#define SPXN_POLLCON    1
#define SPXN_POLLHUP    2
#define SPXN_POLLIN     4
#define SPXN_POLLNVAL 128

#define SCRIPT_MAX 8u

int16_t spxn_poll(void);
int16_t spxn_recv(void *buffer, uint16_t maximum);
int16_t spxn_accept(void);
int16_t spxn_listen(uint16_t port);
int16_t spxn_resolve(const char *host, uint8_t *ip4be);
int16_t spxn_connect(const uint8_t *ip4be, uint16_t port);
void spxn_close(void);
int16_t spxn_send_all(const void *buffer, uint16_t length,
                      uint16_t zero_budget);

#include "spectrum/overlay/direct_ovl.c"
#include "spectrum/transport/spectranext_direct_stage.c"

char direct_rx_payload[SPECTRUM_NET_PAYLOAD_MAX];
char direct_rx_payload2[SPECTRUM_NET_PAYLOAD_MAX];
uint8_t active_link;
uint8_t direct_rx_count;
uint8_t direct_rx_head;
uint8_t direct_rx_payload_len;
uint8_t direct_rx_discard;
uint8_t direct_link_closed;
char netchesszx_direct_host[NETCHESSZX_DIRECT_HOST_MAX + 1u] = "127.0.0.1";
uint16_t netchesszx_direct_port = 5000u;

static int failures;
static uint16_t wait_calls;
static uint8_t close_calls;
static uint8_t listen_calls;
static uint8_t accept_calls;
static int16_t listen_result;
static int16_t accept_result;
static int16_t poll_script[SCRIPT_MAX];
static uint8_t poll_script_len;
static uint8_t poll_script_pos;
static const uint8_t *recv_data;
static uint16_t recv_data_len;
static uint8_t recv_calls;

static void check(int condition, const char *label)
{
    if (!condition) {
        ++failures;
        fprintf(stderr, "FAIL: %s\n", label);
    }
}

void net_wait_frame(void)
{
    ++wait_calls;
}

uint8_t spectrum_key_poll(void)
{
    return 0u;
}

int16_t spxn_poll(void)
{
    if (poll_script_pos >= poll_script_len) {
        return 0;
    }
    return poll_script[poll_script_pos++];
}

int16_t spxn_recv(void *buffer, uint16_t maximum)
{
    uint16_t count = recv_data_len;

    ++recv_calls;

    if (count > maximum) {
        count = maximum;
    }
    if (count != 0u) {
        memcpy(buffer, recv_data, count);
    }
    recv_data += count;
    recv_data_len = (uint16_t)(recv_data_len - count);
    return (int16_t)count;
}

int16_t spxn_accept(void)
{
    ++accept_calls;
    return accept_result;
}

int16_t spxn_listen(uint16_t port)
{
    (void)port;
    ++listen_calls;
    return listen_result;
}

int16_t spxn_resolve(const char *host, uint8_t *ip4be)
{
    (void)host;
    memset(ip4be, 0, 4u);
    return SPXN_OK;
}

int16_t spxn_connect(const uint8_t *ip4be, uint16_t port)
{
    (void)ip4be;
    (void)port;
    return SPXN_OK;
}

void spxn_close(void)
{
    ++close_calls;
}

int16_t spxn_send_all(const void *buffer, uint16_t length,
                      uint16_t zero_budget)
{
    (void)buffer;
    (void)length;
    (void)zero_budget;
    return SPXN_OK;
}

static void reset_state(void)
{
    memset(direct_rx_payload, 0, sizeof(direct_rx_payload));
    memset(direct_rx_payload2, 0, sizeof(direct_rx_payload2));
    active_link = 0u;
    direct_rx_count = 0u;
    direct_rx_head = 0u;
    direct_rx_payload_len = 0u;
    direct_rx_discard = 0u;
    direct_link_closed = 0u;
    wait_calls = 0u;
    close_calls = 0u;
    listen_calls = 0u;
    accept_calls = 0u;
    listen_result = SPXN_OK;
    accept_result = SPXN_OK;
    poll_script_len = 0u;
    poll_script_pos = 0u;
    recv_data = 0;
    recv_data_len = 0u;
    recv_calls = 0u;
    SPXN_DIRECT_PENDING_CURSOR = 0u;
    SPXN_DIRECT_PENDING_LENGTH = 0u;
}

static void set_poll2(int16_t first, int16_t second)
{
    poll_script[0] = first;
    poll_script[1] = second;
    poll_script_len = 2u;
    poll_script_pos = 0u;
}

static uint8_t read_payload(char *out)
{
    uint8_t ctx[3] = { 0u, 0u, sizeof(direct_rx_payload) };

    direct_host_test_ptr = out;
    return direct_read_payload_ovl(ctx);
}

static void test_idle_listener_survives(void)
{
    reset_state();
    check(direct_listen_ovl() == 1u, "idle: listen starts");
    check(direct_wait_pc_connect_ovl() == 0u, "idle: bounded wait times out");
    check(listen_calls == 1u, "idle: timeout preserves listener");
    poll_script[0] = SPXN_POLLCON;
    poll_script_len = 1u;
    poll_script_pos = 0u;
    check(direct_wait_pc_connect_ovl() == 1u,
          "idle: later peer is accepted without external relisten");
    check(listen_calls == 1u, "idle: listener was not recreated unnecessarily");
}

static void test_listener_recovers_invalid_descriptor(void)
{
    reset_state();
    check(direct_listen_ovl() == 1u, "nval listener: listen starts");
    set_poll2(SPXN_POLLNVAL, SPXN_POLLCON);
    check(direct_wait_pc_connect_ovl() == 1u,
          "nval listener: stale descriptor is recreated");
    check(listen_calls == 2u, "nval listener: relisten called once");
    check(accept_calls == 1u, "nval listener: peer accepted after relisten");
}

static void test_listener_recovers_negative_poll(void)
{
    reset_state();
    check(direct_listen_ovl() == 1u, "negative listener: listen starts");
    set_poll2(-1, SPXN_POLLCON);
    check(direct_wait_pc_connect_ovl() == 1u,
          "negative listener: poll error recreates listener");
    check(listen_calls == 2u, "negative listener: relisten called once");
}

static void test_failed_relisten_retries_next_batch(void)
{
    reset_state();
    check(direct_listen_ovl() == 1u, "retry listener: listen starts");
    listen_result = -1;
    poll_script[0] = SPXN_POLLHUP;
    poll_script_len = 1u;
    check(direct_wait_pc_connect_ovl() == 0u,
          "retry listener: failed relisten ends current batch");
    check(active_link == DIRECT_LISTENER_RETRY,
          "retry listener: failed relisten records retry state");

    listen_result = SPXN_OK;
    poll_script[0] = SPXN_POLLCON;
    poll_script_len = 1u;
    poll_script_pos = 0u;
    check(direct_wait_pc_connect_ovl() == 1u,
          "retry listener: next batch recreates listener");
    check(listen_calls == 3u, "retry listener: second relisten attempted");
}

static void test_active_invalid_descriptor_closes(void)
{
    char out[SPECTRUM_NET_PAYLOAD_MAX];

    reset_state();
    poll_script[0] = SPXN_POLLNVAL;
    poll_script_len = 1u;
    check(read_payload(out) == 0xfeu,
          "nval connection: invalid descriptor closes session");
    check(direct_link_closed == 1u, "nval connection: closed state recorded");
    check(close_calls == 1u, "nval connection: backend descriptor closed");
}

static void test_pollin_hup_delivers_final_frame(void)
{
    static const uint8_t frame[] = "BYE\n";
    char out[SPECTRUM_NET_PAYLOAD_MAX];

    reset_state();
    recv_data = frame;
    recv_data_len = sizeof(frame) - 1u;
    set_poll2(SPXN_POLLIN | SPXN_POLLHUP, SPXN_POLLHUP);
    check(read_payload(out) == 0u, "hup data: final frame is delivered");
    check(strcmp(out, "BYE") == 0, "hup data: final frame content");
    check(direct_link_closed == 0u, "hup data: close waits for next poll");
    check(read_payload(out) == 0xfeu, "hup data: later HUP closes session");
}

static void test_nul_frame_is_discarded(void)
{
    static const uint8_t frames[] = { 'B', 'A', 0u, 'D', '\n',
                                      'O', 'K', '\n' };
    char out[SPECTRUM_NET_PAYLOAD_MAX];

    reset_state();
    recv_data = frames;
    recv_data_len = sizeof(frames);
    poll_script[0] = SPXN_POLLIN;
    poll_script_len = 1u;
    check(read_payload(out) == 0u, "nul frame: later valid frame is delivered");
    check(strcmp(out, "OK") == 0, "nul frame: corrupt prefix and suffix rejected");
    check(direct_rx_count == 0u, "nul frame: no ghost frame queued");
}

static void test_overlong_frame_is_discarded(void)
{
    uint8_t frames[SPECTRUM_NET_PAYLOAD_MAX + 4u];
    char out[SPECTRUM_NET_PAYLOAD_MAX];
    uint8_t i;

    for (i = 0u; i < SPECTRUM_NET_PAYLOAD_MAX; ++i) {
        frames[i] = 'X';
    }
    frames[i++] = '\n';
    frames[i++] = 'O';
    frames[i++] = 'K';
    frames[i++] = '\n';

    reset_state();
    recv_data = frames;
    recv_data_len = i;
    poll_script[0] = SPXN_POLLIN;
    poll_script_len = 1u;
    check(read_payload(out) == 0u,
          "overlong frame: later valid frame is delivered");
    check(strcmp(out, "OK") == 0,
          "overlong frame: overflow suffix is not accepted");
    check(direct_rx_count == 0u, "overlong frame: no ghost frame queued");
}

static void test_queue_full_resumes_same_recv(void)
{
    static const uint8_t frames[] = "A\nB\nC\nD\n";
    static const char *const expected[] = { "A", "B", "C", "D" };
    char out[SPECTRUM_NET_PAYLOAD_MAX];
    uint8_t i;

    reset_state();
    recv_data = frames;
    recv_data_len = sizeof(frames) - 1u;
    poll_script[0] = SPXN_POLLIN;
    poll_script_len = 1u;
    for (i = 0u; i < 4u; ++i) {
        check(read_payload(out) == 0u,
              "queue full: each frame is delivered");
        check(strcmp(out, expected[i]) == 0,
              "queue full: frame order is preserved");
    }
    check(recv_calls == 1u,
          "queue full: pending bytes resume without a second recv");
    check(poll_script_pos == 1u,
          "queue full: pending bytes resume without a second poll");
    check(direct_rx_count == 0u &&
              SPXN_DIRECT_PENDING_LENGTH == 0u,
          "queue full: no pending bytes remain");
}

static void test_background_drain_stages_one_recv(void)
{
    static const uint8_t frame[] = "MOVE\n";
    char out[SPECTRUM_NET_PAYLOAD_MAX];

    reset_state();
    recv_data = frame;
    recv_data_len = sizeof(frame) - 1u;
    poll_script[0] = SPXN_POLLIN;
    poll_script_len = 1u;
    spectrum_net_direct_background_drain();
    check(recv_calls == 1u && direct_rx_count == 0u &&
              SPXN_DIRECT_PENDING_LENGTH == sizeof(frame) - 1u,
          "background: recv is staged without parsing");
    check(read_payload(out) == 0u && strcmp(out, "MOVE") == 0,
          "background: staged frame is delivered by DIRECT overlay");
    check(recv_calls == 1u && poll_script_pos == 1u,
          "background: delivery does not poll or recv twice");

    reset_state();
    poll_script[0] = -1;
    poll_script_len = 1u;
    spectrum_net_direct_background_drain();
    check(direct_link_closed == 1u && recv_calls == 0u,
          "background: negative poll fails closed");
}

static void test_stage_is_separate_from_payload_scratch(void)
{
    check(NETCHESSZX_LOWRAM_DIRECT_STAGE_ADDR +
              NETCHESSZX_LOWRAM_DIRECT_STAGE_SIZE <=
              SPECTRUM_MQTT_SCRATCH_BASE,
          "staging: production range ends before payload/TX scratch");
    check(NETCHESSZX_LOWRAM_DIRECT_STAGE_ADDR ==
              NETCHESSZX_LOWRAM_DIRECT_PENDING_LENGTH_ADDR + 1u,
          "staging: production range follows spill and pending state");
}

int main(void)
{
    test_idle_listener_survives();
    test_listener_recovers_invalid_descriptor();
    test_listener_recovers_negative_poll();
    test_failed_relisten_retries_next_batch();
    test_active_invalid_descriptor_closes();
    test_pollin_hup_delivers_final_frame();
    test_nul_frame_is_discarded();
    test_overlong_frame_is_discarded();
    test_queue_full_resumes_same_recv();
    test_background_drain_stages_one_recv();
    test_stage_is_separate_from_payload_scratch();

    if (failures != 0) {
        fprintf(stderr, "%d Spectranext DIRECT test(s) failed\n", failures);
        return 1;
    }
    puts("Spectranext DIRECT tests passed");
    return 0;
}
