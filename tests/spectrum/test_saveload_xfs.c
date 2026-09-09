#include "spectrum/overlay/overlay_api.h"
#include "common/savegame/savegame_format.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint8_t esx_handle;
uintptr_t esx_buf;
uint16_t esx_count;
uint16_t esx_result;

static const char expected_path[] = "/CFG/0168JF26.MSH";
static uint8_t dir_present;
static uint8_t file_present;
static uint16_t read_size;
static uint16_t read_offset;
static uint8_t read_calls;
static uint8_t fail_read;
static uint8_t fail_close;
static uint8_t fail_seek;
static uint8_t seek_calls;
static uint16_t size_high;
uint8_t netchesszx_saveload_test_load(const char *, char *, uint8_t *);
static uint8_t atomic_fail;
static uint8_t unlink_count;
static uint8_t commit_count;
static uint8_t readonly_open_count;
static uint8_t replace_count;
static uint8_t opendir_count;

uint8_t netchesszx_saveload_test_save(const char *name,
                                      const char *buf,
                                      uint8_t *result);

void esx_opendir(const char *path)
{
    assert(strcmp(path, "/CFG") == 0);
    ++opendir_count;
    esx_handle = dir_present;
}

void esx_mkdir(const char *path)
{
    assert(strcmp(path, "/CFG") == 0);
    dir_present = 1u;
    esx_result = 1u;
}

void esx_fopen(const char *path)
{
    read_offset = 0u;
    read_calls = 0u;
    assert(strcmp(path, expected_path) == 0);
    ++readonly_open_count;
    esx_handle = file_present;
}

void esx_freplace(const char *path)
{
    assert(strcmp(path, expected_path) == 0);
    ++replace_count;
    esx_handle = 1u;
    esx_result = file_present;
}

int16_t spxf_replace_atomic(const char *target, const char *temp,
                            const void *buf, uint16_t len)
{
    assert(strcmp(temp, "/CFG/MIRSHIFT.TMP") == 0);
    assert(strcmp(target, expected_path) == 0);
    assert(buf != NULL && len == NETCHESSZX_SAVE_WIRE_B64_SIZE);
    ++replace_count;
    if (atomic_fail) {
        return -1;
    }
    file_present = 1u;
    return 0;
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

struct spxn_regs { uint8_t a; uint16_t bc, de, hl; } spxn_regs;
uint8_t spxn_rom_ixcall(uint16_t addr)
{
    assert(addr == 0x3ecfu && spxn_regs.a == 0u);
    assert(spxn_regs.bc == 2u && spxn_regs.de == 0u && spxn_regs.hl == 0u);
    ++seek_calls;
    if (fail_seek == 1u) return 1u;
    spxn_regs.hl = read_size;
    spxn_regs.de = size_high;
    read_offset = read_size;
    return 0u;
}
uint8_t spxn_xfs_fseek(uint16_t offset)
{
    assert(offset == 0u);
    if (fail_seek == 2u) return 0u;
    read_offset = 0u;
    return 1u;
}

void esx_fwrite(void)
{
    assert(esx_handle == 1u);
    assert(esx_count == NETCHESSZX_SAVE_WIRE_B64_SIZE);
    esx_result = NETCHESSZX_SAVE_WIRE_B64_SIZE;
}

uint8_t esx_fclose(void)
{
    esx_handle = 0u;
    return fail_close;
}

void esx_funlink(const char *path)
{
    assert(strcmp(path, expected_path) == 0);
    file_present = 0u;
    ++unlink_count;
    esx_result = 1u;
}

void esx_commit(const char *path)
{
    assert(strcmp(path, expected_path) == 0);
    file_present = 1u;
    ++commit_count;
    esx_result = 1u;
}

void spectrum_net_background_drain(void)
{
}

uint16_t spectrum_net_runtime_fat_elapsed_days(void) { return 0u; }
void spectrum_net_runtime_set_fat_stamp(uint16_t date, uint16_t time)
{
    (void)date;
    (void)time;
}

uint16_t spectrum_net_runtime_fat_date(void)
{
    return (uint16_t)((46u << 9) | (8u << 5) | 19u);
}

uint16_t spectrum_net_runtime_fat_time(void)
{
    return (uint16_t)((15u << 11) | (26u << 5));
}

static void test_load_length(void)
{
    struct { char data[NETCHESSZX_SAVE_WIRE_B64_SIZE]; char guard; } buffer;
    uint8_t result;
    unsigned size;
    file_present = 1u;
    for (size = 0u; size <= 62u; ++size) {
        read_size = size;
        memset(&buffer, '!', sizeof(buffer));
        assert(netchesszx_saveload_test_load("0168JF26", buffer.data, &result) == (size == 60u));
        assert(result == (size == 60u ? SPECTRUM_OVL_SAVELOAD_OK : SPECTRUM_OVL_SAVELOAD_ERR_DATA));
        assert(buffer.guard == '!');
        assert(esx_handle == 0u);
        if (size == 60u) assert(buffer.data[59] == 'Q');
    }
    read_size = 60u;
    for (fail_read = 1u; fail_read <= 1u; ++fail_read) {
        assert(!netchesszx_saveload_test_load("0168JF26", buffer.data, &result));
        assert(result == SPECTRUM_OVL_SAVELOAD_ERR_DATA);
        assert(esx_handle == 0u);
    }
    fail_read = 0u;
    for (fail_seek = 1u; fail_seek <= 2u; ++fail_seek) {
        assert(!netchesszx_saveload_test_load("0168JF26", buffer.data, &result));
        assert(result == SPECTRUM_OVL_SAVELOAD_ERR_DATA);
        assert(esx_handle == 0u);
    }
    fail_seek = 0u;
    size_high = 1u;
    assert(!netchesszx_saveload_test_load("0168JF26", buffer.data, &result));
    assert(result == SPECTRUM_OVL_SAVELOAD_ERR_DATA && esx_handle == 0u);
    size_high = 0u;
    fail_close = 1u;
    assert(!netchesszx_saveload_test_load("0168JF26", buffer.data, &result));
    assert(result == SPECTRUM_OVL_SAVELOAD_ERR_DATA);
    fail_close = 0u;
}

int main(void)
{
    uint8_t result = 0u;
    char wire[NETCHESSZX_SAVE_WIRE_B64_SIZE] = { 0 };

    atomic_fail = 1u;
    assert(!netchesszx_saveload_test_save("01", wire, &result));
    assert(result == SPECTRUM_OVL_SAVELOAD_ERR_IO);
    assert(dir_present == 1u);
    assert(opendir_count == 2u);
    assert(unlink_count == 0u);
    assert(commit_count == 0u);

    atomic_fail = 0u;
    assert(netchesszx_saveload_test_save("01", wire, &result));
    assert(result == SPECTRUM_OVL_SAVELOAD_OK);
    assert(file_present == 1u);
    assert(unlink_count == 0u);
    assert(commit_count == 0u);

    atomic_fail = 1u;
    assert(!netchesszx_saveload_test_save("01", wire, &result));
    assert(result == SPECTRUM_OVL_SAVELOAD_ERR_IO);
    assert(file_present == 1u);
    assert(unlink_count == 0u);
    assert(commit_count == 0u);
    assert(readonly_open_count == 0u);
    assert(replace_count == 3u);
    assert(opendir_count == 4u);

    test_load_length();
    puts("Spectranext save commit/rollback rules hold");
    return 0;
}
