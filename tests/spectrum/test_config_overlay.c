#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define NETCHESSZX_CONFIG_PACK_APPLY_TEST 1
#define NETCHESSZX_CART_STAGE_TEST 1
#define __z88dk_fastcall

#include "../../src/spectrum/config/app_config_format.h"
#include "../../src/spectrum/lowram_map.h"

uint8_t esx_handle;
uint16_t esx_buf;
uint16_t esx_count;
uint16_t esx_result;
uint8_t setup_config_record[NETCHESSZX_APP_CONFIG_READ_SIZE];
char netchesszx_config_stage_test[NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE];
static uint8_t mock_create_ok;
static uint8_t mock_close_result;
static uint8_t mock_close_count;
static uint8_t mock_close_fail_at;
static uint8_t mock_write_ok;
#ifdef NETCHESSZX_SPECTRANEXT
static uint8_t mock_dir_present;
static uint8_t mock_mkdir_visible;
static uint8_t mock_opendir_count;
static uint8_t mock_commit_ok;
static uint8_t mock_mkdir_count;
static uint8_t mock_commit_count;
static uint8_t mock_unlink_count;
static int16_t mock_atomic_result;
static uint8_t mock_atomic_count;
#endif

typedef struct {
    uint8_t present;
    uint16_t size;
    uint8_t data[NETCHESSZX_APP_CONFIG_READ_SIZE];
} mock_config_file_t;

static mock_config_file_t mock_primary;
static mock_config_file_t mock_alt;
static mock_config_file_t mock_legacy;
static mock_config_file_t *mock_read_file;
static mock_config_file_t *mock_write_file;
static mock_config_file_t *mock_unlink_fail_file;
static uint8_t mock_primary_open_count;
static uint8_t mock_alt_open_count;
static uint8_t mock_open_error;
#ifdef NETCHESSZX_SPECTRANEXT
static uint8_t mock_replace_count;
static uint8_t mock_expect_workspace_record;
#endif

#ifndef NETCHESSZX_SPECTRANEXT
static mock_config_file_t *mock_file_for_path(const char *path)
{
    if (strstr(path, "/CONFIG/") != NULL) {
        return &mock_legacy;
    }
    if (strstr(path, ".CF2") != NULL) {
        return &mock_alt;
    }
    return &mock_primary;
}
#endif

void esx_fopen(const char *path)
{
#ifdef NETCHESSZX_SPECTRANEXT
    if (strcmp(path, "/CFG/MIRSHIFT.CFG") == 0) {
        mock_read_file = &mock_primary;
        ++mock_primary_open_count;
    } else {
        mock_read_file = &mock_alt;
        ++mock_alt_open_count;
    }
#else
    mock_read_file = mock_file_for_path(path);
    if (mock_read_file == &mock_primary) {
        ++mock_primary_open_count;
    } else if (mock_read_file == &mock_alt) {
        ++mock_alt_open_count;
    }
#endif
    esx_handle = mock_read_file->present;
    esx_result = esx_handle ? 0u : mock_open_error;
}

#ifdef NETCHESSZX_SPECTRANEXT
uint8_t spxn_rom_error(void)
{
    return mock_open_error;
}
#endif

void esx_fcreate(const char *path)
{
    (void)path;
    esx_handle = mock_create_ok;
}

char *spectrum_append_text(char *dst, const char *src)
{
    while ((*dst = *src) != '\0') {
        ++dst;
        ++src;
    }
    return dst;
}

#ifndef NETCHESSZX_SPECTRANEXT
void esx_fcreate_new(const char *path)
{
    mock_write_file = mock_file_for_path(path);
    if (!mock_create_ok || mock_write_file->present) {
        esx_handle = 0u;
        return;
    }
    mock_write_file->present = 1u;
    mock_write_file->size = 0u;
    esx_handle = 1u;
}

void esx_funlink(const char *path)
{
    mock_config_file_t *file = mock_file_for_path(path);

    if (file == mock_unlink_fail_file) {
        mock_unlink_fail_file = NULL;
        esx_result = 0u;
        return;
    }
    esx_result = file->present;
    file->present = 0u;
    file->size = 0u;
}
#endif
void esx_fread(void)
{
    uint16_t count;

    if (esx_handle == 0u || mock_read_file == NULL) {
        esx_result = 0u;
        return;
    }
    count = mock_read_file->size;
    if (count > esx_count) {
        count = esx_count;
    }
    memcpy(setup_config_record, mock_read_file->data, count);
    esx_result = count;
}

void esx_fwrite(void)
{
    esx_result = mock_write_ok ? esx_count : 0u;
#ifndef NETCHESSZX_SPECTRANEXT
    if (mock_write_file != NULL && mock_write_ok) {
        mock_write_file->size = esx_count;
        memcpy(mock_write_file->data, setup_config_record, esx_count);
    }
#endif
}

uint8_t esx_fclose(void)
{
    ++mock_close_count;
    if (mock_close_fail_at == mock_close_count) {
        return 0xffu;
    }
    return mock_close_result;
}

#ifdef NETCHESSZX_SPECTRANEXT
void esx_opendir(const char *path)
{
    assert(strcmp(path, "/CFG") == 0);
    ++mock_opendir_count;
    esx_handle = mock_dir_present;
}

int16_t spxf_replace_atomic(const char *target, const char *temp,
                            const void *buf, uint16_t len)
{
    assert(strcmp(target, "/CFG/MIRSHIFT.CFG") == 0);
    assert(strcmp(temp, "/CFG/MIRSHIFT.TMP") == 0);
    assert(len == NETCHESSZX_APP_CONFIG_SIZE);
    if (mock_expect_workspace_record) {
        assert(buf == setup_config_record);
        assert(netchesszx_app_config_validate(buf));
    }
    ++mock_atomic_count;
    if (mock_atomic_result == 0) {
        mock_primary.present = 1u;
        mock_primary.size = len;
        memcpy(mock_primary.data, buf, len);
    }
    return mock_atomic_result;
}

void spectrum_net_background_drain(void)
{
}

void esx_freplace(const char *path)
{
    assert(strcmp(path, "/CFG/MIRSHIFT.CFG") == 0);
    ++mock_replace_count;
    esx_handle = mock_create_ok;
    esx_result = mock_primary.present;
}

void esx_mkdir(const char *path)
{
    assert(strcmp(path, "/CFG") == 0);
    ++mock_mkdir_count;
    mock_dir_present = mock_mkdir_visible;
    esx_result = 1u;
}

void esx_commit(const char *path)
{
    assert(strcmp(path, "/CFG/MIRSHIFT.CFG") == 0);
    ++mock_commit_count;
    esx_result = mock_commit_ok;
}

void esx_funlink(const char *path)
{
    assert(strcmp(path, "/CFG/MIRSHIFT.CFG") == 0);
    ++mock_unlink_count;
    esx_result = 1u;
}
#endif

uint8_t netchesszx_piece_set_load(uint8_t set)
{
    return (uint8_t)(set < NETCHESSZX_PIECE_SET_COUNT);
}

#ifdef NETCHESSZX_NEXT_BANKING
uint8_t netchesszx_piece_set_finalize(void)
{
    return 1u;
}
#endif

void netchesszx_board_theme_apply(uint8_t theme)
{
    (void)theme;
}

#include "../../src/spectrum/overlay/config_ovl.c"

static int failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static void mock_config_files_reset(void)
{
    memset(&mock_primary, 0, sizeof(mock_primary));
    memset(&mock_alt, 0, sizeof(mock_alt));
    memset(&mock_legacy, 0, sizeof(mock_legacy));
    mock_read_file = NULL;
    mock_write_file = NULL;
    mock_unlink_fail_file = NULL;
    mock_primary_open_count = 0u;
    mock_alt_open_count = 0u;
#ifdef NETCHESSZX_SPECTRANEXT
    mock_open_error = 2u;
#else
    mock_open_error = 5u;
#endif
    mock_close_result = 0u;
    mock_close_count = 0u;
    mock_close_fail_at = 0u;
#ifdef NETCHESSZX_SPECTRANEXT
    mock_dir_present = 1u;
    mock_mkdir_visible = 1u;
    mock_opendir_count = 0u;
    mock_commit_ok = 1u;
    mock_mkdir_count = 0u;
    mock_commit_count = 0u;
    mock_unlink_count = 0u;
    mock_replace_count = 0u;
    mock_atomic_result = 0;
    mock_atomic_count = 0u;
    mock_expect_workspace_record = 0u;
#endif
}

static void mock_config_file_set(mock_config_file_t *file,
                                 const uint8_t *record)
{
    file->present = 1u;
    file->size = NETCHESSZX_APP_CONFIG_SIZE;
    memcpy(file->data, record, NETCHESSZX_APP_CONFIG_SIZE);
}

static void config_runtime_sentinel(void)
{
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_timezone = 7;
    netchesszx_direct_port = 1234u;
    strcpy(netchesszx_mqtt_code, "MS0000");
    strcpy(netchesszx_direct_host, "10.0.0.1");
}

static void test_config_load_paths(const uint8_t *valid_record)
{
    uint8_t ctx[SPECTRUM_OVERLAY_CONTEXT_SIZE] = {0u};

    mock_config_files_reset();
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == 0u);
    CHECK(ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] ==
          SPECTRUM_OVL_SAVELOAD_ERR_MISSING);
    CHECK(netchesszx_timezone == 7);

    mock_config_files_reset();
    mock_open_error = 3u;
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == SPECTRUM_CONFIG_STATE_INVALID);
    CHECK(ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] ==
          SPECTRUM_OVL_SAVELOAD_ERR_IO);

    mock_config_files_reset();
    mock_config_file_set(&mock_primary, valid_record);
    mock_primary.data[NETCHESSZX_APP_CONFIG_CRC_OFF] ^= 0x80u;
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == 4u);
    CHECK(ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] ==
          SPECTRUM_OVL_SAVELOAD_ERR_DATA);
    CHECK(netchesszx_timezone == 7);
    CHECK(netchesszx_session_role == NETCHESSZX_SESSION_ROLE_HOST);

    mock_config_files_reset();
    mock_config_file_set(&mock_primary, valid_record);
    mock_primary.data[NETCHESSZX_APP_CONFIG_CRC_OFF] ^= 0x80u;
    mock_config_file_set(&mock_alt, valid_record);
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == 3u);
    CHECK(ctx[SPECTRUM_OVL_CTX_SAVELOAD_RESULT] ==
          SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_alt_open_count == 1u);
    CHECK(netchesszx_session_role == NETCHESSZX_SESSION_ROLE_JOIN);
    CHECK(netchesszx_transport == NETCHESSZX_TRANSPORT_DIRECT);
    CHECK(netchesszx_timezone == -11);
    CHECK(netchesszx_direct_port == 65535u);
    CHECK(strcmp(netchesszx_direct_host, "255.255.255.255") == 0);
    mock_config_files_reset();
    mock_config_file_set(&mock_primary, valid_record);
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == 3u);
    CHECK(mock_alt_open_count == 0u);

#ifndef NETCHESSZX_SPECTRANEXT
    mock_config_files_reset();
    mock_config_file_set(&mock_legacy, valid_record);
    config_runtime_sentinel();
    CHECK(config_load_ovl(ctx) == SPECTRUM_CONFIG_STATE_SAVED);
    CHECK(netchesszx_session_role == NETCHESSZX_SESSION_ROLE_JOIN);
#endif
}

int main(void)
{
    uint8_t record[NETCHESSZX_APP_CONFIG_READ_SIZE];
    uint8_t valid_record[NETCHESSZX_APP_CONFIG_READ_SIZE];
    uint8_t save_ctx[SPECTRUM_OVERLAY_CONTEXT_SIZE] = {0u};

    CHECK(netchesszx_timezone == NETCHESSZX_TIME_RTC);
    CHECK(netchesszx_timezone_last == NETCHESSZX_TZ);

    strcpy(netchesszx_mqtt_code, "MS12AB");
    CHECK(config_defaults_ovl(NULL) == 1u);
    CHECK(strcmp(netchesszx_mqtt_code, NETCHESSZX_MQTT_CODE) == 0);
    CHECK(strcmp(netchesszx_mqtt_code, "MS0000") == 0);

    mock_config_files_reset();
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_BLACK);
    netchesszx_movement_hints = 1u;
    netchesszx_piece_set_index = 2u;
    netchesszx_board_theme_index = 4u;
    netchesszx_timezone = -11;
    netchesszx_timezone_last = -11;
    netchesszx_direct_port = 65535u;
    strcpy(netchesszx_mqtt_code, "MS12AB");
    strcpy(netchesszx_direct_host, "255.255.255.255");

    config_pack(record);
    CHECK(netchesszx_app_config_validate(record));
    memcpy(valid_record, record, sizeof(valid_record));
    CHECK(record[NETCHESSZX_APP_CONFIG_SIZE] == 0u);
    CHECK((record[NETCHESSZX_APP_CONFIG_FLAGS_OFF] &
           NETCHESSZX_APP_CONFIG_FLAG_ROLE) != 0u);
    CHECK((record[NETCHESSZX_APP_CONFIG_FLAGS_OFF] &
           NETCHESSZX_APP_CONFIG_FLAG_TRANSPORT) == 0u);
    CHECK(record[NETCHESSZX_APP_CONFIG_THEME_OFF] == 4u);
    CHECK((int8_t)record[NETCHESSZX_APP_CONFIG_TZ_OFF] == -11);

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    netchesszx_movement_hints = 0u;
    netchesszx_piece_set_index = 0u;
    netchesszx_board_theme_index = 0u;
    netchesszx_timezone = 0;
    netchesszx_timezone_last = 0;
    netchesszx_direct_port = 1u;
    netchesszx_mqtt_code[0] = '\0';
    netchesszx_direct_host[0] = '\0';

    config_apply(record);
    CHECK(netchesszx_session_role == NETCHESSZX_SESSION_ROLE_JOIN);
    CHECK(netchesszx_transport == NETCHESSZX_TRANSPORT_DIRECT);
    CHECK(netchesszx_host_color == NETCHESSZX_COLOR_BLACK);
    CHECK(netchesszx_movement_hints == 1u);
    CHECK(netchesszx_piece_set_index == 2u);
    CHECK(netchesszx_board_theme_index == 4u);
    CHECK(netchesszx_timezone == -11);
    CHECK(netchesszx_timezone_last == -11);
    CHECK(netchesszx_direct_port == 65535u);
    CHECK(strcmp(netchesszx_mqtt_code, "MS12AB") == 0);
    CHECK(strcmp(netchesszx_direct_host, "255.255.255.255") == 0);

    test_config_load_paths(valid_record);

    record[NETCHESSZX_APP_CONFIG_TZ_OFF] = NETCHESSZX_TIME_RTC;
    record[NETCHESSZX_APP_CONFIG_TZ_LAST_OFF] = 3u;
    record[NETCHESSZX_APP_CONFIG_CRC_OFF] = netchesszx_app_config_crc8(record);
    CHECK(netchesszx_app_config_validate(record));
    config_apply(record);
    CHECK(netchesszx_timezone == NETCHESSZX_TIME_RTC);
    CHECK(netchesszx_timezone_last == 3);

    mock_config_files_reset();
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_DIRECT,
                                 NETCHESSZX_COLOR_WHITE);
    strcpy(netchesszx_mqtt_code, "MS12AB");
    strcpy(netchesszx_direct_host, "192.168.0.122");
    netchesszx_direct_port = 5000u;
    netchesszx_timezone = 5;
    netchesszx_timezone_last = 5;
    netchesszx_piece_set_index = 0u;
    netchesszx_board_theme_index = 0u;
    mock_create_ok = 1u;
    mock_write_ok = 1u;
    mock_close_result = 0xffu;
#ifdef NETCHESSZX_SPECTRANEXT
    mock_close_result = 0u;
    CHECK(config_save_record(record, save_ctx) == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(save_ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] ==
          SPECTRUM_CONFIG_SAVE_STAGE_OK);
    CHECK(netchesszx_app_config_validate(record));
    CHECK(mock_opendir_count == 1u);
    CHECK(mock_atomic_count == 1u);

    mock_config_files_reset();
    mock_dir_present = 0u;
    CHECK(config_save_record(record, save_ctx) == SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_mkdir_count == 1u);
    CHECK(mock_opendir_count == 2u);
    CHECK(mock_atomic_count == 1u);

    mock_atomic_result = -1;
    CHECK(config_save_record(setup_config_record, save_ctx) ==
          SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(save_ctx[SPECTRUM_OVL_CTX_CONFIG_STAGE] ==
          SPECTRUM_CONFIG_SAVE_STAGE_FILE_WRITE);
    CHECK(mock_atomic_count == 2u);

    mock_config_files_reset();
    mock_expect_workspace_record = 1u;
    CHECK(config_save_ovl(save_ctx) == 1u);
    CHECK(mock_atomic_count == 1u);
#else
    CHECK(config_save_record(record, save_ctx) == SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(!mock_primary.present && !mock_alt.present);

    mock_close_result = 0u;
    CHECK(config_save_record(setup_config_record, save_ctx) ==
          SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_primary.present && !mock_alt.present);

    mock_close_result = 0xffu;
    CHECK(config_save_record(setup_config_record, save_ctx) ==
          SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(mock_primary.present && !mock_alt.present);

    mock_close_result = 0u;
    mock_write_ok = 0u;
    CHECK(config_save_record(setup_config_record, save_ctx) ==
          SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(mock_primary.present && !mock_alt.present);

    mock_write_ok = 1u;
    CHECK(config_save_record(setup_config_record, save_ctx) ==
          SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(!mock_primary.present && mock_alt.present);

    mock_create_ok = 0u;
    CHECK(config_save_record(setup_config_record, save_ctx) ==
          SPECTRUM_OVL_SAVELOAD_ERR_OPEN);
    CHECK(mock_alt.present);

    mock_config_files_reset();
    mock_create_ok = 1u;
    mock_write_ok = 1u;
    mock_config_file_set(&mock_legacy, valid_record);
    CHECK(config_save_record(setup_config_record, save_ctx) ==
          SPECTRUM_OVL_SAVELOAD_OK);
    CHECK(mock_primary.present && mock_legacy.present);

    mock_config_files_reset();
    mock_create_ok = 1u;
    mock_write_ok = 0u;
    mock_config_file_set(&mock_primary, valid_record);
    mock_primary.data[NETCHESSZX_APP_CONFIG_CRC_OFF] ^= 0x80u;
    mock_config_file_set(&mock_alt, valid_record);
    CHECK(config_save_record(setup_config_record, save_ctx) ==
          SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(!mock_primary.present && mock_alt.present);
    CHECK(netchesszx_app_config_validate(mock_alt.data));

    mock_config_files_reset();
    mock_create_ok = 1u;
    mock_write_ok = 1u;
    mock_config_file_set(&mock_primary, valid_record);
    mock_unlink_fail_file = &mock_primary;
    CHECK(config_save_record(setup_config_record, save_ctx) ==
          SPECTRUM_OVL_SAVELOAD_ERR_IO);
    CHECK(mock_primary.present && !mock_alt.present);
#endif

    if (failures != 0) {
        return 1;
    }
    puts("config overlay load/pack/apply tests ok");
    return 0;
}
