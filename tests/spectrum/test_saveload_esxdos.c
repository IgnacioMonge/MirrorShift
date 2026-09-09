#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define __z88dk_fastcall

#include "common/savegame/savegame_format.h"

uint8_t esx_handle;
uintptr_t esx_buf;
uint16_t esx_count;
uint16_t esx_result;

static uint8_t failures;
static uint8_t file_present;
static uint16_t read_size;
static uint16_t read_offset;
static uint8_t read_calls;
static uint8_t fail_read;
static uint8_t fail_close;
uint8_t netchesszx_saveload_test_load(const char *, char *, uint8_t *);
static uint8_t short_write;
static uint8_t dir_present;
static uint8_t mkdir_count;
static char last_path[32];
static uint16_t stamp_date = (46u << 9) | (8u << 5) | 19u;
static uint16_t stamp_time = (15u << 11) | (26u << 5);
static uint16_t elapsed_days;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static void remember(const char *path)
{
    (void)strncpy(last_path, path, sizeof(last_path) - 1u);
    last_path[sizeof(last_path) - 1u] = '\0';
}

void esx_fopen(const char *path)
{
    read_offset = 0u;
    read_calls = 0u;
    remember(path);
    esx_handle = file_present ? 1u : 0u;
}

void esx_fcreate(const char *path)
{
    (void)path;
    esx_handle = 0u;
}

void esx_fcreate_new(const char *path)
{
    remember(path);
    esx_handle = (uint8_t)(dir_present && !file_present);
    if (esx_handle != 0u) {
        file_present = 1u;
    }
}

void esx_mkdir(const char *path)
{
    CHECK(strcmp(path, "/SYS/CONFIG") == 0);
    dir_present = 1u;
    ++mkdir_count;
}

void esx_fread(void)
{
    ++read_calls;
    esx_result = read_size - read_offset;
    if (esx_result > esx_count) esx_result = esx_count;
    if (read_calls == fail_read) esx_result = 0u;
    memset((void *)esx_buf, 'Q', esx_result);
    read_offset += esx_result;
}
void esx_fwrite(void) { esx_result = short_write ? esx_count - 1u : esx_count; }

uint8_t esx_fclose(void)
{
    esx_handle = 0u;
    return fail_close;
}

void esx_funlink(const char *path)
{
    remember(path);
    file_present = 0u;
    esx_result = 1u;
}

void spectrum_net_background_drain(void) {}

uint16_t spectrum_net_runtime_fat_elapsed_days(void) { return elapsed_days; }
void spectrum_net_runtime_set_fat_stamp(uint16_t date, uint16_t time)
{
    stamp_date = date;
    stamp_time = time;
    elapsed_days = 0u;
}

uint16_t spectrum_net_runtime_fat_date(void)
{
    return stamp_date;
}

uint16_t spectrum_net_runtime_fat_time(void)
{
    return stamp_time;
}

#include "../../src/spectrum/overlay/saveload_ovl.c"

static void test_load_length(void)
{
    struct { char data[NETCHESSZX_SAVE_WIRE_B64_SIZE]; char guard; } buffer;
    uint8_t result;
    unsigned size;
    file_present = 1u;
    for (size = 0u; size <= 62u; ++size) {
        read_size = size;
        memset(&buffer, '!', sizeof(buffer));
        CHECK(netchesszx_saveload_test_load("0168JF26", buffer.data, &result) == (size == 60u));
        CHECK(result == (size == 60u ? SPECTRUM_OVL_SAVELOAD_OK : SPECTRUM_OVL_SAVELOAD_ERR_DATA));
        CHECK(buffer.guard == '!');
        CHECK(esx_handle == 0u);
        if (size == 60u) CHECK(buffer.data[59] == 'Q');
    }
    read_size = 60u;
    for (fail_read = 1u; fail_read <= 2u; ++fail_read) {
        CHECK(!netchesszx_saveload_test_load("0168JF26", buffer.data, &result));
        CHECK(result == SPECTRUM_OVL_SAVELOAD_ERR_DATA);
        CHECK(esx_handle == 0u);
    }
    fail_read = 0u;
    fail_close = 1u;
    CHECK(!netchesszx_saveload_test_load("0168JF26", buffer.data, &result));
    CHECK(result == SPECTRUM_OVL_SAVELOAD_ERR_DATA);
    fail_close = 0u;
}

int main(void)
{
    char payload[NETCHESSZX_SAVE_WIRE_B64_SIZE] = {0};
    uint8_t result;

    CHECK(netchesszx_saveload_test_save("01", payload, &result) == 1u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(file_present);
    CHECK(dir_present && mkdir_count == 1u);
    CHECK(strcmp(last_path, "/SYS/CONFIG/0168JF26.MSH") == 0);

    CHECK(netchesszx_saveload_test_save("01", payload, &result) == 0u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
    CHECK(file_present);

    file_present = 0u;
    short_write = 1u;
    CHECK(netchesszx_saveload_test_save("02", payload, &result) == 0u);
    CHECK(result == SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(!file_present);

    short_write = 0u;
    stamp_date = (46u << 9) | (12u << 5) | 31u;
    stamp_time = 0u;
    elapsed_days = 1u;
    CHECK(netchesszx_saveload_test_save("02", payload, &result) == 1u);
    CHECK(strcmp(last_path, "/SYS/CONFIG/02711000.MSH") == 0);
    CHECK(elapsed_days == 0u);
    CHECK(netchesszx_saveload_test_save("02", payload, &result) == 0u);
    CHECK(strcmp(last_path, "/SYS/CONFIG/02711000.MSH") == 0);
    file_present = 0u;
    stamp_date = (44u << 9) | (2u << 5) | 28u;
    elapsed_days = 2u;
    CHECK(netchesszx_saveload_test_save("03", payload, &result) == 1u);
    CHECK(strcmp(last_path, "/SYS/CONFIG/03431000.MSH") == 0);

    if (failures != 0u) {
        return 1;
    }
    test_load_length();
    if (failures != 0u) return 1;
    puts("esxDOS saveload durability tests ok");
    return 0;
}
