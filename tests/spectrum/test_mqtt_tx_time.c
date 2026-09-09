#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "spectrum/transport/mqtt_min.h"

static uint8_t host_packet[SPECTRUM_MQTT_PACKET_MAX];
#undef SPECTRUM_MQTT_PACKET_SCRATCH
#define SPECTRUM_MQTT_PACKET_SCRATCH host_packet

uint16_t mqtt_next_id = 1u;
char line_buf[48];
int8_t netchesszx_timezone = -5;
int8_t netchesszx_timezone_last;
char netchesszx_mqtt_code[17];

#include "../../src/spectrum/overlay/mqtt_tx_ovl.c"

static char at_command[2][SPECTRUM_MQTT_PACKET_MAX];
static char uart_command[32];
static const char *line_text;
static uint8_t at_count;
static uint8_t line_ready;
static uint16_t fat_date;
static uint16_t fat_time;
static char retry_state[4];

char *spectrum_net_payload_scratch(void) { return retry_state; }

const char NETCHESS_PROTO_GAME_START[] = "GAME START";

const char *netchess_after_prefix(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (*text++ != *prefix++) {
            return 0;
        }
    }
    return text;
}

uint8_t spectrum_mqtt_publish(uint8_t *out, uint8_t cap, uint16_t packet_id,
                              const char *topic, const char *payload,
                              uint8_t retain)
{
    (void)out;
    (void)cap;
    (void)packet_id;
    (void)topic;
    (void)payload;
    (void)retain;
    return 0u;
}

uint8_t mqtt_send_raw_packet(const uint8_t *packet, uint8_t len)
{
    (void)packet;
    (void)len;
    return 0u;
}

const char *spectrum_net_mqtt_out_suffix(void) { return ""; }
const char *spectrum_net_mqtt_out_ack_suffix(void) { return ""; }
const char *spectrum_net_mqtt_presence_suffix(void) { return ""; }
const char *spectrum_net_mqtt_presence_payload(void) { return ""; }
void spectrum_net_mqtt_setup_payload(char *out) { *out = '\0'; }

char *spectrum_append_text(char *out, const char *text)
{
    while (*text != '\0') {
        *out++ = *text++;
    }
    *out = '\0';
    return out;
}

char *spectrum_append_u16(char *out, uint16_t value)
{
    char digits[5];
    uint8_t count = 0u;

    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    while (count != 0u) {
        *out++ = digits[--count];
    }
    *out = '\0';
    return out;
}

uint8_t spectrum_net_at_cmd(const char *command, uint16_t frames)
{
    (void)frames;
    if (at_count < 2u) {
        strcpy(at_command[at_count], command);
    }
    ++at_count;
    return (uint8_t)(at_count != 1u);
}

void spectrum_net_guard_wait(uint16_t frames)
{
    (void)frames;
}

void reset_line_buf(void)
{
    line_buf[0] = '\0';
}

void net_wait_frame(void) {}

uint8_t read_line(uint16_t frames)
{
    (void)frames;
    if (line_ready == 0u) {
        return 0u;
    }
    line_ready = 0u;
    strcpy(line_buf, line_text);
    return 1u;
}

uint8_t spectrum_uart_send_string(const char *text)
{
    strcpy(uart_command, text);
    return 1u;
}

uint8_t spectrum_uart_send_crlf(void)
{
    return 1u;
}

void spectrum_net_runtime_set_clock(uint8_t hour, uint8_t minute,
                                    uint8_t second)
{
    (void)hour;
    (void)minute;
    (void)second;
}

void spectrum_net_runtime_set_fat_stamp(uint16_t date, uint16_t time)
{
    fat_date = date;
    fat_time = time;
}

int main(void)
{
    const char *full = "AT+CIPSNTPCFG=1,-5,\"pool.ntp.org\",\"time.google.com\"";
    const char *fallback = "AT+CIPSNTPCFG=1,-5";
    uint16_t expected_date = (uint16_t)(((uint16_t)46u << 9) |
                                        ((uint16_t)6u << 5) | 5u);
    uint16_t expected_time = (uint16_t)(((uint16_t)12u << 11) |
                                        ((uint16_t)34u << 5));

    line_text = "+CIPSNTPTIME:Fri Jun  5 12:34:56 2026";
    line_ready = 1u;
    if (!mqtt_tx_sync_time_ovl() || at_count != 2u ||
        strcmp(at_command[0], full) != 0 ||
        strcmp(at_command[1], fallback) != 0 ||
        strcmp(uart_command, "AT+CIPSNTPTIME?") != 0 ||
        fat_date != expected_date || fat_time != expected_time) {
        puts("[ERR] MQTT SNTP scratch fallback or FAT stamp");
        return 1;
    }
    memset(retry_state, 0, sizeof(retry_state));
    line_text = "OK";
    line_ready = 1u;
    if (!mqtt_tx_clock_retry_start_ovl() ||
        strcmp(uart_command, "AT+CIPSNTPCFG=1,-5") != 0 ||
        !mqtt_tx_clock_retry_poll_ovl()) {
        puts("[ERR] MQTT SNTP retry start");
        return 1;
    }
    for (at_count = 0u; at_count < 100u; ++at_count) {
        if (!mqtt_tx_clock_retry_poll_ovl()) {
            puts("[ERR] MQTT SNTP retry delay");
            return 1;
        }
    }
    line_text = "+CIPSNTPTIME:Fri Jun  5 12:34:56 2026";
    line_ready = 1u;
    if (strcmp(uart_command, "AT+CIPSNTPTIME?") != 0 ||
        mqtt_tx_clock_retry_poll_ovl()) {
        puts("[ERR] MQTT SNTP retry completion");
        return 1;
    }
    puts("[OK] MQTT SNTP scratch fallback and FAT stamp");
    return 0;
}
