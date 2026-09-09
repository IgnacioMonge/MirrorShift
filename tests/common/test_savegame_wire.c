#include "common/savegame/savegame_wire.h"
#include "spectrum/restore/restore.h"

#include <stdio.h>
#include <string.h>

static int failures;

typedef struct {
    const char *name;
    uint8_t offset;
    uint8_t and_mask;
    uint8_t or_mask;
    int expected_error;
} negative_restore_vector_t;

static const negative_restore_vector_t negative_restore_vectors[] = {
    {"reserved cell nibble", 0u, 0x0fu, 0x30u, NETCHESSZX_SAVE_ERR_BOARD},
    {"reserved metadata bit", 32u, 0xffu, 0x08u, NETCHESSZX_SAVE_ERR_FIELD},
    {"reserved metadata byte", 33u, 0u, 1u, NETCHESSZX_SAVE_ERR_FIELD},
    {"unknown state flag", 36u, 0xffu, 0x04u, NETCHESSZX_SAVE_ERR_FIELD},
    {"over mismatch", 32u, 0xfbu, 0x04u, NETCHESSZX_SAVE_ERR_FIELD},
    {"game hour 100", 37u, 0u, 100u, NETCHESSZX_SAVE_ERR_FIELD},
    {"game minute 60", 38u, 0u, 60u, NETCHESSZX_SAVE_ERR_FIELD},
    {"move second 60", 42u, 0u, 60u, NETCHESSZX_SAVE_ERR_FIELD}
};

static void expect_int(const char *name, int got, int want)
{
    if (got != want) {
        printf("FAIL %s: got %d want %d\n", name, got, want);
        ++failures;
    }
}

static void expect_true(const char *name, int got)
{
    if (!got) {
        printf("FAIL %s\n", name);
        ++failures;
    }
}

static uint8_t test_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0u;
    uint8_t i;
    uint8_t bit;

    for (i = 0u; i < len; ++i) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            crc = (crc & 0x80u) != 0u ?
                (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static netchesszx_save_state_t sample_state(void)
{
    static const char board[] =
        "........"
        "........"
        "...A...."
        "...AA..."
        "...AB..."
        "....B..."
        "........"
        "........";
    netchesszx_save_state_t state;

    memset(&state, 0, sizeof(state));
    memcpy(state.cells, board, sizeof(state.cells));
    state.ply = 3u;
    state.side = NETCHESSZX_SAVE_SIDE_BLACK;
    state.over = 0u;
    state.host_color = NETCHESSZX_SAVE_HOST_BLACK;
    state.flags = NETCHESSZX_SAVE_FLAG_ACTIVE;
    state.game_hour = 1u;
    state.game_minute = 2u;
    state.game_second = 3u;
    state.move_minute = 4u;
    state.move_second = 5u;
    state.view_flags = NETCHESSZX_SAVE_VIEW_FLIPPED;
    return state;
}

static void snapshot_from_state(spectrum_reversi_snapshot_t *snapshot,
                                netchesszx_save_meta_t *meta,
                                const netchesszx_save_state_t *state)
{
    memset(snapshot, 0, sizeof(*snapshot));
    memset(meta, 0, sizeof(*meta));
    memcpy(snapshot->cells, state->cells, sizeof(snapshot->cells));
    snapshot->side = state->side;
    snapshot->over = state->over;
    meta->ply = state->ply;
    meta->flags = state->flags;
    meta->host_color = state->host_color;
    meta->view_flags = state->view_flags;
    meta->timers[0] = state->game_hour;
    meta->timers[1] = state->game_minute;
    meta->timers[2] = state->game_second;
    meta->timers[3] = state->move_hour;
    meta->timers[4] = state->move_minute;
    meta->timers[5] = state->move_second;
}

static void test_rejects_bad_state(void)
{
    uint8_t wire[NETCHESSZX_SAVE_WIRE_SIZE];
    netchesszx_save_state_t state = sample_state();

    expect_int("short wire pack", netchesszx_save_wire_pack(wire, 4u, &state),
               NETCHESSZX_SAVE_ERR_BUFFER);
    state.side = 2u;
    expect_int("bad side", netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_ERR_FIELD);
    state = sample_state();
    state.over = 2u;
    expect_int("bad over", netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_ERR_FIELD);
    state = sample_state();
    state.cells[0] = 'K';
    expect_int("chess piece", netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_ERR_BOARD);
    state = sample_state();
    state.flags = NETCHESSZX_SAVE_FLAG_GAME_OVER;
    expect_int("over flag mismatch",
               netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_ERR_FIELD);
    state = sample_state();
    state.over = 1u;
    state.flags = NETCHESSZX_SAVE_FLAG_ACTIVE | NETCHESSZX_SAVE_FLAG_GAME_OVER;
    expect_int("active game over",
               netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_ERR_FIELD);
    state = sample_state();
    state.move_second = 60u;
    expect_int("bad timer", netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_ERR_FIELD);
    state = sample_state();
    state.view_flags = 0x02u;
    expect_int("bad view flags",
               netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_ERR_FIELD);

    state = sample_state();
    state.side = NETCHESSZX_SAVE_SIDE_WHITE;
    expect_int("Silence permits side and ply parity mismatch",
               netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_OK);
}

static void test_wire_roundtrip(void)
{
    uint8_t wire[NETCHESSZX_SAVE_WIRE_SIZE];
    uint8_t decoded_wire[NETCHESSZX_SAVE_WIRE_SIZE];
    char b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    netchesszx_save_state_t in = sample_state();
    netchesszx_save_state_t out;

    memset(&out, 0, sizeof(out));

    expect_int("wire pack", netchesszx_save_wire_pack(wire, sizeof(wire), &in),
               NETCHESSZX_SAVE_OK);
    expect_int("wire unpack", netchesszx_save_wire_unpack(&out, wire, sizeof(wire)),
               NETCHESSZX_SAVE_OK);
    expect_true("wire state", memcmp(&out, &in, sizeof(in)) == 0);
    expect_int("b64 encode",
               netchesszx_save_wire_b64_encode(b64, sizeof(b64), wire, sizeof(wire)),
               NETCHESSZX_SAVE_OK);
    expect_int("b64 decode",
               netchesszx_save_wire_b64_decode(decoded_wire, sizeof(decoded_wire),
                                               b64, sizeof(b64)),
               NETCHESSZX_SAVE_OK);
    expect_true("b64 wire", memcmp(decoded_wire, wire, sizeof(wire)) == 0);
    expect_true("chunk frame length",
                NETCHESSZX_SAVE_RESTORE_FRAME_MAX <= 47u &&
                (2u + 2u + 1u + NETCHESSZX_SAVE_WIRE_CHUNK_SIZE) ==
                    NETCHESSZX_SAVE_RESTORE_FRAME_MAX);
}

static void test_common_spectrum_identity(void)
{
    uint8_t wire[NETCHESSZX_SAVE_WIRE_SIZE];
    char common_b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    char spectrum_b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    netchesszx_save_state_t state = sample_state();
    spectrum_reversi_snapshot_t snapshot;
    spectrum_reversi_snapshot_t decoded_snapshot;
    netchesszx_save_meta_t meta;
    netchesszx_save_meta_t decoded_meta;

    memset(&decoded_snapshot, 0, sizeof(decoded_snapshot));
    memset(&decoded_meta, 0, sizeof(decoded_meta));
    snapshot_from_state(&snapshot, &meta, &state);
    expect_int("identity common pack",
               netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_OK);
    expect_int("identity common encode",
               netchesszx_save_wire_b64_encode(common_b64, sizeof(common_b64),
                                               wire, sizeof(wire)),
               NETCHESSZX_SAVE_OK);
    expect_true("identity Spectrum encode",
                spectrum_restore_build_b64(&snapshot, &meta, spectrum_b64));
    expect_true("common and Spectrum byte identity",
                memcmp(common_b64, spectrum_b64, sizeof(common_b64)) == 0);
    expect_true("Spectrum decode",
                spectrum_restore_decode(common_b64, &decoded_snapshot,
                                        &decoded_meta));
    expect_true("Spectrum snapshot roundtrip",
                memcmp(&decoded_snapshot, &snapshot, sizeof(snapshot)) == 0);
    expect_true("Spectrum metadata roundtrip",
                memcmp(&decoded_meta, &meta, sizeof(meta)) == 0);
}

static void test_rejects_bad_wire(void)
{
    static const char legacy_chess_b64[] =
        "uazam4iIiIgAAAAAAAAAAAAAAAAAAAAAEREREUI1YyR8_wAAAQEAAAAAOwGm";
    uint8_t wire[NETCHESSZX_SAVE_WIRE_SIZE];
    uint8_t decoded_wire[NETCHESSZX_SAVE_WIRE_SIZE];
    char b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    netchesszx_save_state_t state = sample_state();
    netchesszx_save_state_t out;
    spectrum_reversi_snapshot_t snapshot;
    netchesszx_save_meta_t meta;

    expect_int("wire pack good", netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_OK);
    wire[0] ^= 0x01u;
    expect_int("bad crc", netchesszx_save_wire_unpack(&out, wire, sizeof(wire)),
               NETCHESSZX_SAVE_ERR_CRC);

    expect_int("wire repack", netchesszx_save_wire_pack(wire, sizeof(wire), &state),
               NETCHESSZX_SAVE_OK);
    expect_int("b64 encode good",
               netchesszx_save_wire_b64_encode(b64, sizeof(b64), wire, sizeof(wire)),
               NETCHESSZX_SAVE_OK);
    b64[3] = '=';
    expect_int("bad b64",
               netchesszx_save_wire_b64_decode(decoded_wire, sizeof(decoded_wire),
                                               b64, sizeof(b64)),
               NETCHESSZX_SAVE_ERR_TOKEN);

    expect_int("legacy b64 envelope",
               netchesszx_save_wire_b64_decode(
                   wire, sizeof(wire), legacy_chess_b64,
                   sizeof(legacy_chess_b64) - 1u),
               NETCHESSZX_SAVE_OK);
    expect_true("common rejects legacy chess v1",
                netchesszx_save_wire_unpack(&out, wire, sizeof(wire)) !=
                    NETCHESSZX_SAVE_OK);
    expect_true("Spectrum rejects legacy chess v1",
                !spectrum_restore_decode(legacy_chess_b64, &snapshot, &meta));
}

static void test_negative_restore_vectors(void)
{
    uint8_t wire[NETCHESSZX_SAVE_WIRE_SIZE];
    char b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    netchesszx_save_state_t source = sample_state();
    netchesszx_save_state_t decoded;
    spectrum_reversi_snapshot_t spectrum_snapshot;
    netchesszx_save_meta_t spectrum_meta;
    size_t i;
    int rc;

    for (i = 0u; i < sizeof(negative_restore_vectors) /
                         sizeof(negative_restore_vectors[0]); ++i) {
        const negative_restore_vector_t *vector = &negative_restore_vectors[i];

        expect_int("negative vector base pack",
                   netchesszx_save_wire_pack(wire, sizeof(wire), &source),
                   NETCHESSZX_SAVE_OK);
        wire[vector->offset] =
            (uint8_t)((wire[vector->offset] & vector->and_mask) |
                      vector->or_mask);
        wire[44] = test_crc8(wire, 44u);
        expect_int("negative vector b64 encode",
                   netchesszx_save_wire_b64_encode(b64, sizeof(b64),
                                                  wire, sizeof(wire)),
                   NETCHESSZX_SAVE_OK);
        rc = netchesszx_save_wire_unpack(&decoded, wire, sizeof(wire));
        if (rc != vector->expected_error) {
            printf("FAIL common rejects %s: got %d want %d\n",
                   vector->name, rc, vector->expected_error);
            ++failures;
        }
        if (spectrum_restore_decode(b64, &spectrum_snapshot, &spectrum_meta)) {
            printf("FAIL Spectrum rejects %s\n", vector->name);
            ++failures;
        }
    }
}

int main(void)
{
    test_rejects_bad_state();
    test_wire_roundtrip();
    test_common_spectrum_identity();
    test_rejects_bad_wire();
    test_negative_restore_vectors();

    if (failures != 0) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("savegame wire tests ok\n");
    return 0;
}
