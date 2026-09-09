#include "spectrum/overlay/overlay_api.h"
#include "spectrum/overlay/overlay_context.h"
#include "common/savegame/savegame_format.h"
#include "spectrum/transport/fat_clock.h"
#if defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
#include "spectrum/lowram_map.h"
#endif

#if defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
#include "spxf.h"
#include "spxn_rom.h"
#elif defined(NETCHESSZX_SPECTRANEXT)
struct spxn_regs { uint8_t a; uint16_t bc, de, hl; };
extern struct spxn_regs spxn_regs;
uint8_t spxn_rom_ixcall(uint16_t addr);
#define ROM_LSEEK 0x3ecfu
#define ROM_CARRY 1u
int16_t spxf_replace_atomic(const char *target, const char *temp,
                            const void *buf, uint16_t len);
#endif

extern uint8_t esx_handle;
#ifdef NETCHESSZX_HOST_TEST
extern uintptr_t esx_buf;
#else
extern uint16_t esx_buf;
#endif
extern uint16_t esx_count;
extern uint16_t esx_result;
void esx_fopen(const char *path) __z88dk_fastcall;
void esx_fcreate(const char *path) __z88dk_fastcall;
void esx_fcreate_new(const char *path) __z88dk_fastcall;
void esx_fread(void);
void esx_fwrite(void);
uint8_t esx_fclose(void);
void esx_funlink(const char *path) __z88dk_fastcall;
#ifdef NETCHESSZX_SPECTRANEXT
uint8_t spxn_xfs_fseek(uint16_t offset) __z88dk_fastcall;
void esx_freplace(const char *path) __z88dk_fastcall;
void esx_opendir(const char *path) __z88dk_fastcall;
void esx_mkdir(const char *path) __z88dk_fastcall;
void esx_commit(const char *path) __z88dk_fastcall;
#endif

#ifdef NETCHESSZX_SPECTRANEXT
#define SAVELOAD_DIR "/CFG/"
#define SAVELOAD_CONFIG_DIR "/CFG"
#define SAVELOAD_TEMP_PATH "/CFG/MIRSHIFT.TMP"
#else
#define SAVELOAD_DIR "/SYS/CONFIG/"
void esx_mkdir(const char *path) __z88dk_fastcall;
static const char saveload_config_dir[] = "/SYS/CONFIG";
#endif
#define SAVELOAD_EXT ".MSH"
#define SAVELOAD_NAME_MAX 8u
#define SAVELOAD_PATH_MAX 25u

typedef char saveload_path_capacity_check[
    (sizeof(SAVELOAD_DIR) - 1u + SAVELOAD_NAME_MAX +
     sizeof(SAVELOAD_EXT) - 1u + 1u <= SAVELOAD_PATH_MAX) ? 1 : -1];

static char saveload_path[SAVELOAD_PATH_MAX];

#ifdef NETCHESSZX_SPECTRANEXT
#if !defined(NETCHESSZX_HOST_TEST)
#define SAVELOAD_PATH_ARG ((char *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#define SAVELOAD_TEMP_PATH_ARG (SAVELOAD_PATH_ARG + SAVELOAD_PATH_MAX)
#define SAVELOAD_CONFIG_DIR_ARG \
    (SAVELOAD_TEMP_PATH_ARG + sizeof(SAVELOAD_TEMP_PATH))
typedef char saveload_stage_capacity_check[
    (SAVELOAD_PATH_MAX + sizeof(SAVELOAD_TEMP_PATH) +
     sizeof(SAVELOAD_CONFIG_DIR) <=
     NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE) ? 1 : -1];

static void saveload_stage_paths(void)
{
    spectrum_append_text(SAVELOAD_PATH_ARG, saveload_path);
    spectrum_append_text(SAVELOAD_TEMP_PATH_ARG, SAVELOAD_TEMP_PATH);
    spectrum_append_text(SAVELOAD_CONFIG_DIR_ARG, SAVELOAD_CONFIG_DIR);
}
#else
#define SAVELOAD_PATH_ARG saveload_path
#define SAVELOAD_TEMP_PATH_ARG SAVELOAD_TEMP_PATH
#define SAVELOAD_CONFIG_DIR_ARG SAVELOAD_CONFIG_DIR
#define saveload_stage_paths() ((void)0)
#endif
#else
#define SAVELOAD_PATH_ARG saveload_path
#endif

static uint8_t saveload_name_char_ok(char c)
{
    return (uint8_t)((c >= 'A' && c <= 'Z') ||
                     (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') ||
                     c == '_');
}

static char saveload_upper(char c) __z88dk_fastcall
{
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - ('a' - 'A'));
    }
    return c;
}

static char saveload_b32_char(uint8_t v) __z88dk_fastcall
{
    return (char)(v < 10u ? ('0' + v) : ('A' + v - 10u));
}

static uint8_t saveload_stamp_valid(uint8_t year,
                                    uint8_t month,
                                    uint8_t day,
                                    uint8_t hour,
                                    uint8_t minute)
{
    if (year < 40u) {
        return 0u;
    }
    if (year > 71u) {
        return 0u;
    }
    if (month == 0u || month > 12u) {
        return 0u;
    }
    if (day == 0u || day > 31u) {
        return 0u;
    }
    if (hour >= 24u || minute >= 60u) {
        return 0u;
    }
    return 1u;
}

static void saveload_write_stamp(char *out) __z88dk_fastcall
{
    uint16_t date = spectrum_net_runtime_fat_date();
    uint16_t time = spectrum_net_runtime_fat_time();
    uint16_t days = spectrum_net_runtime_fat_elapsed_days();
    uint16_t midnight;
    uint8_t year;

    while (days != 0u) {
        spectrum_fat_tick_clock(&date, &midnight, 0u, 0u, 0u);
        --days;
    }
    spectrum_net_runtime_set_fat_stamp(date, time);
    year = (uint8_t)(date >> 9);
    uint8_t month = (uint8_t)((date >> 5) & 15u);
    uint8_t day = (uint8_t)(date & 31u);
    uint8_t hour = (uint8_t)(time >> 11);
    uint8_t minute = (uint8_t)((time >> 5) & 63u);
    char tens = '0';

    if (saveload_stamp_valid(year, month, day, hour, minute)) {
        year = (uint8_t)(year - 40u);
    } else {
        year = month = day = hour = minute = 0u;
    }
    while (minute >= 10u) {
        minute = (uint8_t)(minute - 10u);
        ++tens;
    }
    out[0] = saveload_b32_char(year);
    out[1] = saveload_b32_char(month);
    out[2] = saveload_b32_char(day);
    out[3] = saveload_b32_char(hour);
    out[4] = tens;
    out[5] = (char)('0' + minute);
}

static uint8_t saveload_build_path(const char *name, uint8_t stamp_slot)
{
    static const char dir[] = SAVELOAD_DIR;
    static const char ext[] = SAVELOAD_EXT;
    uint8_t i;
    uint8_t out = 0u;

    if (name == 0 || name[0] == '\0') {
        return 0u;
    }
    for (i = 0u; dir[i] != '\0'; ++i) {
        saveload_path[out++] = dir[i];
    }
    if (stamp_slot && name[0] >= '0' && name[0] <= '9' &&
        name[1] >= '0' && name[1] <= '9' && name[2] == '\0') {
        saveload_path[out++] = name[0];
        saveload_path[out++] = name[1];
        saveload_write_stamp(saveload_path + out);
        out = (uint8_t)(out + 6u);
    } else {
        for (i = 0u; name[i] != '\0'; ++i) {
            if (i >= SAVELOAD_NAME_MAX || !saveload_name_char_ok(name[i])) {
                return 0u;
            }
            saveload_path[out++] = saveload_upper(name[i]);
        }
    }
    for (i = 0u; ext[i] != '\0'; ++i) {
        saveload_path[out++] = ext[i];
    }
    saveload_path[out] = '\0';
    return 1u;
}
static const char *saveload_ctx_name(uint8_t *ctx)
{
    return (const char *)((uint16_t)ctx[SPECTRUM_OVL_CTX_SAVELOAD_NAME_LO] |
                          ((uint16_t)ctx[SPECTRUM_OVL_CTX_SAVELOAD_NAME_HI] << 8));
}

#ifndef NETCHESSZX_HOST_TEST
static char *saveload_ctx_buf(uint8_t *ctx)
{
    return (char *)((uint16_t)ctx[SPECTRUM_OVL_CTX_SAVELOAD_BUF_LO] |
                    ((uint16_t)ctx[SPECTRUM_OVL_CTX_SAVELOAD_BUF_HI] << 8));
}

#endif

uint8_t saveload_erase_nczs_ovl(uint8_t *ctx) __z88dk_fastcall
{
    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = SPECTRUM_OVL_SAVELOAD_ERR_NAME;
    if (!saveload_build_path(saveload_ctx_name(ctx), 0u)) {
        return 0u;
    }
    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = SPECTRUM_OVL_SAVELOAD_ERR_OPEN;
#ifdef NETCHESSZX_SPECTRANEXT
    saveload_stage_paths();
#endif
    esx_funlink(SAVELOAD_PATH_ARG);
    if (!esx_result) {
        return 0u;
    }
    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = SPECTRUM_OVL_SAVELOAD_OK;
    return 1u;
}

#ifdef NETCHESSZX_HOST_TEST
#define SAVELOAD_SET_RESULT(value) (*result = (value))
uint8_t netchesszx_saveload_test_load(const char *name, char *buf, uint8_t *result)
#else
#define SAVELOAD_SET_RESULT(value) \
    (ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = (value))
uint8_t saveload_load_nczs_ovl(uint8_t *ctx) __z88dk_fastcall
#endif
{
#ifndef NETCHESSZX_HOST_TEST
    char *buf = saveload_ctx_buf(ctx);
    const char *name = saveload_ctx_name(ctx);
#endif
#ifndef NETCHESSZX_SPECTRANEXT
    char tail[2];
#endif
    uint8_t valid = 0u;

    SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_ERR_NAME);
    if (buf == 0 || !saveload_build_path(name, 0u)) {
        return 0u;
    }
#ifdef NETCHESSZX_SPECTRANEXT
    saveload_stage_paths();
#endif
    esx_fopen(SAVELOAD_PATH_ARG);
    if (esx_handle == 0u) {
        SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
        return 0u;
    }
    spectrum_net_background_drain();
#ifdef NETCHESSZX_SPECTRANEXT
    /* The compatibility handle is the ROM descriptor plus one. Use the
       existing bridge to bind length; hardware EOF must not be probed. */
    spxn_regs.a = (uint8_t)(esx_handle - 1u);
    spxn_regs.bc = 2u; /* SEEK_END */
    spxn_regs.de = spxn_regs.hl = 0u;
    if (!(spxn_rom_ixcall(ROM_LSEEK) & ROM_CARRY) &&
        spxn_regs.de == 0u && spxn_regs.hl == NETCHESSZX_SAVE_WIRE_B64_SIZE &&
        spxn_xfs_fseek(0u)) {
        esx_buf = (uintptr_t)buf;
        esx_count = NETCHESSZX_SAVE_WIRE_B64_SIZE;
        esx_fread();
        valid = (uint8_t)(esx_result == NETCHESSZX_SAVE_WIRE_B64_SIZE);
    }
#else
    esx_buf = (uintptr_t)buf;
    /* Read the final byte together with an overflow probe. A read error
       returns zero, so it cannot be mistaken for a valid end of file. */
    esx_count = NETCHESSZX_SAVE_WIRE_B64_SIZE - 1u;
    esx_fread();
    if (esx_result == NETCHESSZX_SAVE_WIRE_B64_SIZE - 1u) {
        esx_buf = (uintptr_t)tail;
        esx_count = sizeof(tail);
        esx_fread();
        if (esx_result == 1u) {
            buf[NETCHESSZX_SAVE_WIRE_B64_SIZE - 1u] = tail[0];
            valid = 1u;
        }
    }
#endif
    if (esx_fclose()) valid = 0u;
    if (!valid) {
        SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_ERR_DATA);
        return 0u;
    }
    SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_OK);
    return 1u;
}

#undef SAVELOAD_SET_RESULT

#ifdef NETCHESSZX_HOST_TEST
#define SAVELOAD_SET_RESULT(value) (*result = (value))
uint8_t netchesszx_saveload_test_save(const char *name,
                                      const char *buf,
                                      uint8_t *result)
#else
#define SAVELOAD_SET_RESULT(value) \
    (ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = (value))
uint8_t saveload_save_nczs_ovl(uint8_t *ctx) __z88dk_fastcall
#endif
{
#ifndef NETCHESSZX_HOST_TEST
    const char *buf = saveload_ctx_buf(ctx);
#endif
    SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_ERR_NAME);
#ifdef NETCHESSZX_HOST_TEST
    if (buf == 0 || !saveload_build_path(name, 1u)) {
#else
    if (buf == 0 || !saveload_build_path(saveload_ctx_name(ctx), 1u)) {
#endif
        return 0u;
    }
#ifdef NETCHESSZX_SPECTRANEXT
    /* Saves may be the first mutable write, so establish /CFG here too. */
    saveload_stage_paths();
    esx_opendir(SAVELOAD_CONFIG_DIR_ARG);
    if (esx_handle == 0u) {
        esx_mkdir(SAVELOAD_CONFIG_DIR_ARG);
        if (!esx_result) {
            SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
            return 0u;
        }
        esx_opendir(SAVELOAD_CONFIG_DIR_ARG);
        if (esx_handle == 0u) {
            SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
            return 0u;
        }
    }
    if (esx_fclose()) {
        SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
        return 0u;
    }
    spectrum_net_background_drain();
    if (spxf_replace_atomic(SAVELOAD_PATH_ARG, SAVELOAD_TEMP_PATH_ARG, buf,
                            NETCHESSZX_SAVE_WIRE_B64_SIZE) != 0) {
        SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_ERR_IO);
        return 0u;
    }
#else
    /* Save slots are append-only. Never truncate a valid save before the
       replacement is durable. */
    esx_mkdir(saveload_config_dir);
    esx_fcreate_new(saveload_path);
    if (esx_handle == 0u) {
        SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
        return 0u;
    }
    spectrum_net_background_drain();
    esx_buf = (uint16_t)buf;
    esx_count = NETCHESSZX_SAVE_WIRE_B64_SIZE;
    esx_fwrite();
    if (esx_fclose() || esx_result != NETCHESSZX_SAVE_WIRE_B64_SIZE) {
        esx_funlink(saveload_path);
        SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_ERR_IO);
        return 0u;
    }
#endif
    SAVELOAD_SET_RESULT(SPECTRUM_OVL_SAVELOAD_OK);
    return 1u;
}

#undef SAVELOAD_SET_RESULT
