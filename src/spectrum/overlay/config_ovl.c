#include "spectrum/overlay/overlay_api.h"
#include "spectrum/overlay/overlay_context.h"
#include "spectrum/config/app_config_format.h"
#include "spectrum/config/session.h"
#include "spectrum/lowram_map.h"
#include "spectrum/saveload/saveload.h"

#include <string.h>

#if defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
#include "spxf.h"
#elif defined(NETCHESSZX_SPECTRANEXT)
int16_t spxf_replace_atomic(const char *target, const char *temp,
                            const void *buf, uint16_t len);
#endif

extern uint8_t esx_handle;
extern uint16_t esx_buf;
extern uint16_t esx_count;
extern uint16_t esx_result;
void esx_fopen(const char *path) __z88dk_fastcall;
void esx_fcreate(const char *path) __z88dk_fastcall;
void esx_fcreate_new(const char *path) __z88dk_fastcall;
void esx_funlink(const char *path) __z88dk_fastcall;
void esx_fread(void);
void esx_fwrite(void);
uint8_t esx_fclose(void);
#ifdef NETCHESSZX_SPECTRANEXT
void esx_opendir(const char *path) __z88dk_fastcall;
void esx_mkdir(const char *path) __z88dk_fastcall;
#endif

#ifdef NETCHESSZX_SPECTRANEXT
#define CONFIG_PATH "/CFG/MIRSHIFT.CFG"
#define CONFIG_PATH_ALT "/MIRSHIFT.CFG"
#define CONFIG_TEMP_PATH "/CFG/MIRSHIFT.TMP"
#define CONFIG_DIR "/CFG"
#if !defined(NETCHESSZX_HOST_TEST) || defined(NETCHESSZX_CART_STAGE_TEST)
#ifdef NETCHESSZX_CART_STAGE_TEST
extern char netchesszx_config_stage_test[];
#define CONFIG_PATH_ARG netchesszx_config_stage_test
#else
#define CONFIG_PATH_ARG ((char *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#endif
#define CONFIG_TEMP_PATH_ARG (CONFIG_PATH_ARG + sizeof(CONFIG_PATH))
#define CONFIG_DIR_ARG (CONFIG_TEMP_PATH_ARG + sizeof(CONFIG_TEMP_PATH))
#define CONFIG_PATH_ALT_ARG (CONFIG_DIR_ARG + sizeof(CONFIG_DIR))
typedef char config_stage_capacity_check[
    (sizeof(CONFIG_PATH) + sizeof(CONFIG_TEMP_PATH) + sizeof(CONFIG_DIR) +
     sizeof(CONFIG_PATH_ALT) <= NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE)
        ? 1 : -1];

static void config_stage_paths(void)
{
    spectrum_append_text(CONFIG_PATH_ARG, CONFIG_PATH);
    spectrum_append_text(CONFIG_TEMP_PATH_ARG, CONFIG_TEMP_PATH);
    spectrum_append_text(CONFIG_DIR_ARG, CONFIG_DIR);
    spectrum_append_text(CONFIG_PATH_ALT_ARG, CONFIG_PATH_ALT);
}
#else
#define CONFIG_PATH_ARG CONFIG_PATH
#define CONFIG_TEMP_PATH_ARG CONFIG_TEMP_PATH
#define CONFIG_DIR_ARG CONFIG_DIR
#define CONFIG_PATH_ALT_ARG CONFIG_PATH_ALT
#define config_stage_paths() ((void)0)
#endif
#else
static const char config_path[] = "/SYS/MIRSHIFT.CFG";
static const char config_path_alt[] = "/SYS/MIRSHIFT.CF2";
static const char config_path_legacy[] = "/SYS/CONFIG/MIRSHIFT.CFG";
#define CONFIG_PATH config_path
#define CONFIG_PATH_ALT config_path_alt
#define CONFIG_PATH_LEGACY config_path_legacy
#endif

extern uint8_t setup_config_record[];

#if defined(NETCHESSZX_CONFIG_PACK_APPLY_TEST) || defined(NETCHESSZX_NEXT_BANKING) || \
    defined(NETCHESSZX_SPECTRANEXT)
/* esxDOS temporarily restores ROM in slot 1; its I/O buffer must stay visible. */
#define CONFIG_WIRE setup_config_record
#else
#define CONFIG_WIRE ((uint8_t *)NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR)
#endif

uint8_t config_defaults_ovl(uint8_t *ctx) __z88dk_fastcall
{
    (void)ctx;
    strncpy(netchesszx_mqtt_code, NETCHESSZX_MQTT_CODE,
            NETCHESSZX_MQTT_CODE_MAX);
    netchesszx_mqtt_code[NETCHESSZX_MQTT_CODE_MAX] = '\0';
    return 1u;
}

static void config_pack(uint8_t *record)
{
    uint8_t flags = 0u;

    memset(record, 0, NETCHESSZX_APP_CONFIG_READ_SIZE);
    record[NETCHESSZX_APP_CONFIG_MAGIC_0] = 'M';
    record[NETCHESSZX_APP_CONFIG_MAGIC_1] = 'S';
    record[NETCHESSZX_APP_CONFIG_MAGIC_2] = 'C';
    record[NETCHESSZX_APP_CONFIG_MAGIC_3] = 'F';
    record[NETCHESSZX_APP_CONFIG_VERSION_OFF] = NETCHESSZX_APP_CONFIG_VERSION;
    record[NETCHESSZX_APP_CONFIG_LENGTH_OFF] = NETCHESSZX_APP_CONFIG_SIZE;
    if (netchesszx_session_role == NETCHESSZX_SESSION_ROLE_JOIN) {
        flags |= NETCHESSZX_APP_CONFIG_FLAG_ROLE;
    }
    if (netchesszx_transport == NETCHESSZX_TRANSPORT_MQTT) {
        flags |= NETCHESSZX_APP_CONFIG_FLAG_TRANSPORT;
    }
    if (netchesszx_host_color == NETCHESSZX_COLOR_BLACK) {
        flags |= NETCHESSZX_APP_CONFIG_FLAG_COLOR;
    }
    if (netchesszx_movement_hints != 0u) {
        flags |= NETCHESSZX_APP_CONFIG_FLAG_HINTS;
    }
    flags |= (uint8_t)(netchesszx_piece_set_index <<
                       NETCHESSZX_APP_CONFIG_FLAG_SET_SHIFT);
    record[NETCHESSZX_APP_CONFIG_FLAGS_OFF] = flags;
    record[NETCHESSZX_APP_CONFIG_THEME_OFF] = netchesszx_board_theme_index;
    record[NETCHESSZX_APP_CONFIG_TZ_OFF] = (uint8_t)netchesszx_timezone;
    record[NETCHESSZX_APP_CONFIG_TZ_LAST_OFF] =
        (uint8_t)netchesszx_timezone_last;
    record[NETCHESSZX_APP_CONFIG_PORT_LO_OFF] = (uint8_t)netchesszx_direct_port;
    record[NETCHESSZX_APP_CONFIG_PORT_HI_OFF] =
        (uint8_t)(netchesszx_direct_port >> 8);
    strcpy((char *)(record + NETCHESSZX_APP_CONFIG_ROOM_OFF),
           netchesszx_mqtt_code);
    strcpy((char *)(record + NETCHESSZX_APP_CONFIG_HOST_OFF),
           netchesszx_direct_host);
    record[NETCHESSZX_APP_CONFIG_CRC_OFF] = netchesszx_app_config_crc8(record);
}

static void config_apply(const uint8_t *record)
{
    uint8_t flags = record[NETCHESSZX_APP_CONFIG_FLAGS_OFF];

    netchesszx_timezone = (int8_t)record[NETCHESSZX_APP_CONFIG_TZ_OFF];

    netchesszx_session_configure(
        (uint8_t)(flags & NETCHESSZX_APP_CONFIG_FLAG_ROLE),
        (uint8_t)((flags & NETCHESSZX_APP_CONFIG_FLAG_TRANSPORT) != 0u),
        (uint8_t)((flags & NETCHESSZX_APP_CONFIG_FLAG_COLOR) != 0u));
    netchesszx_movement_hints =
        (uint8_t)((flags & NETCHESSZX_APP_CONFIG_FLAG_HINTS) != 0u);
    netchesszx_piece_set_index =
        (uint8_t)((flags & NETCHESSZX_APP_CONFIG_FLAG_SET_MASK) >>
                  NETCHESSZX_APP_CONFIG_FLAG_SET_SHIFT);
    netchesszx_board_theme_index = record[NETCHESSZX_APP_CONFIG_THEME_OFF];
    netchesszx_timezone_last =
        (int8_t)record[NETCHESSZX_APP_CONFIG_TZ_LAST_OFF];
    netchesszx_direct_port =
        (uint16_t)record[NETCHESSZX_APP_CONFIG_PORT_LO_OFF] |
        ((uint16_t)record[NETCHESSZX_APP_CONFIG_PORT_HI_OFF] << 8);
    memcpy(netchesszx_mqtt_code,
           record + NETCHESSZX_APP_CONFIG_ROOM_OFF,
           NETCHESSZX_MQTT_CODE_MAX + 1u);
    memcpy(netchesszx_direct_host,
           record + NETCHESSZX_APP_CONFIG_HOST_OFF,
           NETCHESSZX_DIRECT_HOST_MAX + 1u);
}

static uint8_t config_read_record(const char *path, uint8_t *record);

static uint8_t config_save_record(uint8_t *record, uint8_t *ctx)
{
#ifndef NETCHESSZX_SPECTRANEXT
    const char *old_path;
    const char *new_path;
    uint8_t result;
    uint8_t write_ok;
#endif

    ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] = SPECTRUM_CONFIG_SAVE_STAGE_DATA;
#ifdef NETCHESSZX_SPECTRANEXT
    config_pack(record);
    if (!netchesszx_app_config_validate(record)) {
        return SPECTRUM_OVL_SAVELOAD_ERR_DATA;
    }
    config_stage_paths();
    esx_opendir(CONFIG_DIR_ARG);
    if (esx_handle == 0u) {
        ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] =
            SPECTRUM_CONFIG_SAVE_STAGE_DIR_CREATE;
        esx_mkdir(CONFIG_DIR_ARG);
        if (!esx_result) {
            return SPECTRUM_OVL_SAVELOAD_ERR_OPEN;
        }
        ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] =
            SPECTRUM_CONFIG_SAVE_STAGE_DIR_VERIFY;
        esx_opendir(CONFIG_DIR_ARG);
        if (esx_handle == 0u) {
            return SPECTRUM_OVL_SAVELOAD_ERR_OPEN;
        }
    }
    ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] =
        SPECTRUM_CONFIG_SAVE_STAGE_DIR_CLOSE;
    if (esx_fclose()) {
        return SPECTRUM_OVL_SAVELOAD_ERR_OPEN;
    }
    ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] =
        SPECTRUM_CONFIG_SAVE_STAGE_FILE_WRITE;
    spectrum_net_background_drain();
    if (spxf_replace_atomic(CONFIG_PATH_ARG, CONFIG_TEMP_PATH_ARG, record,
                            NETCHESSZX_APP_CONFIG_SIZE) != 0) {
        return SPECTRUM_OVL_SAVELOAD_ERR_IO;
    }
#else
    /* esxDOS cannot replace atomically. Keep the valid copy until its
       replacement has been written and closed successfully. */
    result = config_read_record(CONFIG_PATH, record);
    if (result == SPECTRUM_OVL_SAVELOAD_ERR_IO) {
        return result;
    }
    if (result == SPECTRUM_OVL_SAVELOAD_OK) {
        old_path = CONFIG_PATH;
        new_path = CONFIG_PATH_ALT;
    } else {
        result = config_read_record(CONFIG_PATH_ALT, record);
        if (result == SPECTRUM_OVL_SAVELOAD_ERR_IO) {
            return result;
        }
        old_path = result == SPECTRUM_OVL_SAVELOAD_OK
                       ? CONFIG_PATH_ALT : 0;
        new_path = CONFIG_PATH;
    }
    config_pack(record);
    if (!netchesszx_app_config_validate(record)) {
        return SPECTRUM_OVL_SAVELOAD_ERR_DATA;
    }
    ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] =
        SPECTRUM_CONFIG_SAVE_STAGE_FILE_CREATE;
    esx_funlink(new_path);
    esx_fcreate_new(new_path);
    if (esx_handle == 0u) {
        return SPECTRUM_OVL_SAVELOAD_ERR_OPEN;
    }
#if defined(NETCHESSZX_CONFIG_PACK_APPLY_TEST)
    esx_buf = 0u;
#else
    spectrum_net_background_drain();
    esx_buf = (uint16_t)record;
#endif
    esx_count = NETCHESSZX_APP_CONFIG_SIZE;
    ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] =
        SPECTRUM_CONFIG_SAVE_STAGE_FILE_WRITE;
    esx_fwrite();
    write_ok = (uint8_t)(esx_result == NETCHESSZX_APP_CONFIG_SIZE);
    ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] =
        SPECTRUM_CONFIG_SAVE_STAGE_FILE_CLOSE;
    if (esx_fclose()) {
        write_ok = 0u;
    }
    if (!write_ok) {
        esx_funlink(new_path);
        ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] =
            SPECTRUM_CONFIG_SAVE_STAGE_FILE_WRITE;
        return SPECTRUM_OVL_SAVELOAD_ERR_IO;
    }
    if (old_path != 0) {
        esx_funlink(old_path);
        if (!esx_result) {
            esx_funlink(new_path);
            return SPECTRUM_OVL_SAVELOAD_ERR_IO;
        }
    }
#endif
    ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] = SPECTRUM_CONFIG_SAVE_STAGE_OK;
    return SPECTRUM_OVL_SAVELOAD_OK;
}

static uint8_t config_read_record(const char *path, uint8_t *record)
{
    esx_fopen(path);
    if (esx_handle == 0u) {
#ifdef NETCHESSZX_SPECTRANEXT
        return spxn_rom_error() == 2u
                   ? SPECTRUM_OVL_SAVELOAD_ERR_MISSING
                   : SPECTRUM_OVL_SAVELOAD_ERR_IO;
#else
        return esx_result == 5u
                   ? SPECTRUM_OVL_SAVELOAD_ERR_MISSING
                   : SPECTRUM_OVL_SAVELOAD_ERR_IO;
#endif
    }
#ifdef NETCHESSZX_CONFIG_PACK_APPLY_TEST
    esx_buf = 0u;
#else
    spectrum_net_background_drain();
    esx_buf = (uint16_t)record;
#endif
    esx_count = NETCHESSZX_APP_CONFIG_READ_SIZE;
    esx_fread();
    if (esx_fclose()) {
        return SPECTRUM_OVL_SAVELOAD_ERR_IO;
    }
    if (esx_result != NETCHESSZX_APP_CONFIG_SIZE ||
        !netchesszx_app_config_validate(record)) {
        return SPECTRUM_OVL_SAVELOAD_ERR_DATA;
    }
    return SPECTRUM_OVL_SAVELOAD_OK;
}

uint8_t config_load_ovl(uint8_t *ctx) __z88dk_fastcall
{
    uint8_t *record = CONFIG_WIRE;
    uint8_t result;
    uint8_t alt_result;

#ifdef NETCHESSZX_SPECTRANEXT
    config_stage_paths();
    result = config_read_record(CONFIG_PATH_ARG, record);
#else
    result = config_read_record(CONFIG_PATH, record);
#endif
    if (result != SPECTRUM_OVL_SAVELOAD_OK) {
#ifdef NETCHESSZX_SPECTRANEXT
        alt_result = config_read_record(CONFIG_PATH_ALT_ARG, record);
#else
        alt_result = config_read_record(CONFIG_PATH_ALT, record);
#endif
        if (alt_result == SPECTRUM_OVL_SAVELOAD_OK ||
            result == SPECTRUM_OVL_SAVELOAD_ERR_MISSING) {
            result = alt_result;
        }
    }
#ifndef NETCHESSZX_SPECTRANEXT
    if (result != SPECTRUM_OVL_SAVELOAD_OK) {
        alt_result = config_read_record(CONFIG_PATH_LEGACY, record);
        if (alt_result == SPECTRUM_OVL_SAVELOAD_OK ||
            result == SPECTRUM_OVL_SAVELOAD_ERR_MISSING) {
            result = alt_result;
        }
    }
#endif
    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = result;
    if (result != SPECTRUM_OVL_SAVELOAD_OK) {
        (void)config_defaults_ovl(ctx);
        return result == SPECTRUM_OVL_SAVELOAD_ERR_MISSING
                   ? 0u
                   : SPECTRUM_CONFIG_STATE_INVALID;
    }
    config_apply(record);
    if (!netchesszx_piece_set_load(netchesszx_piece_set_index)) {
        netchesszx_piece_set_index = NETCHESSZX_PIECE_SET_BW_L;
    }
#ifdef NETCHESSZX_NEXT_BANKING
    else {
        (void)netchesszx_piece_set_finalize();
    }
#endif
    netchesszx_board_theme_apply(netchesszx_board_theme_index);
    return SPECTRUM_CONFIG_STATE_SAVED;
}

uint8_t config_save_ovl(uint8_t *ctx) __z88dk_fastcall
{
    uint8_t *record = CONFIG_WIRE;
    uint8_t result;

    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = SPECTRUM_OVL_SAVELOAD_ERR_DATA;
    result = config_save_record(record, ctx);
    ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] = result;
    return (uint8_t)(result == SPECTRUM_OVL_SAVELOAD_OK);
}
