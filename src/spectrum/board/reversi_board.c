#include "spectrum/board/board.h"

#include "common/reversi/reversi.h"
#include "spectrum/lowram_map.h"
#ifndef NETCHESSZX_HOST_TEST
#include "spectrum/overlay/overlay.h"
#endif

/* Compile the exact common engine in this platform translation unit so its
   fixed storage binding can use the canonical Spectrum low-RAM map without
   introducing a common -> Spectrum dependency. Host tests link it normally. */
#ifdef NETCHESSZX_FIXED_LOW_RAM
#define MIRRORSHIFT_FIXED_GAME_ADDR NETCHESSZX_LOWRAM_BOARD_STATE_ADDR
#define MIRRORSHIFT_FIXED_DETAIL_ADDR NETCHESSZX_LOWRAM_RULES_DETAIL_ADDR
#include "common/reversi/reversi.c"
#endif

#ifndef NETCHESSZX_HOST_TEST
typedef char spectrum_board_state_size_check[
    sizeof(ms_state_t) == NETCHESSZX_LOWRAM_BOARD_STATE_SIZE ? 1 : -1];
typedef char spectrum_board_undo_state_size_check[
    sizeof(ms_state_t) == NETCHESSZX_LOWRAM_BOARD_UNDO_SIZE ? 1 : -1];
#define board_undo_state \
    (*(ms_state_t *)NETCHESSZX_LOWRAM_BOARD_UNDO_ADDR)
#else
static ms_state_t board_undo_state;
#endif

void spectrum_board_reset(void)
{
    ms_rules_reset();
}

void spectrum_board_clear(void)
{
    ms_state_t state;
    uint8_t i;

    for (i = 0u; i < MS_CELLS; ++i) {
        state.cells[i] = MS_CELL_EMPTY;
    }
    state.side = MS_SIDE_A;
    state.over = 0u;
    ms_rules_restore_trusted(&state);
}

const char *spectrum_board_cells(void)
{
    return ms_rules_cells();
}

uint8_t spectrum_board_side(void)
{
    return ms_rules_side();
}

uint8_t spectrum_board_winner(void)
{
    return ms_rules_winner();
}

uint8_t spectrum_board_is_legal_move(const char *move) NETCHESSZX_FASTCALL
{
    return (uint8_t)(ms_rules_can_play(move) == MS_OK);
}

uint8_t spectrum_board_is_legal_index(uint8_t index) NETCHESSZX_FASTCALL
{
#ifdef NETCHESSZX_FIXED_LOW_RAM
    return (uint8_t)(ms_rules_can_play_index(index) == MS_OK);
#else
    char name[3];

    if (index >= MS_CELLS) {
        return 0u;
    }
    ms_rules_square_name(index, name);
    return (uint8_t)(ms_rules_can_play(name) == MS_OK);
#endif
}

uint8_t spectrum_board_check_state(void)
{
    return ms_rules_is_over() != 0u ? SPECTRUM_BOARD_GAME_OVER
                                    : SPECTRUM_BOARD_ACTIVE;
}

uint8_t spectrum_board_last_flip_count(void)
{
    return ms_rules_last_flip_count();
}

uint8_t spectrum_board_last_silences(void)
{
    return ms_rules_last_silences();
}

void spectrum_reversi_snapshot_save(spectrum_reversi_snapshot_t *out)
    NETCHESSZX_FASTCALL
{
    (void)ms_rules_save(out);
}

uint8_t spectrum_reversi_snapshot_restore(
    const spectrum_reversi_snapshot_t *snapshot)
    NETCHESSZX_FASTCALL
{
#ifdef NETCHESSZX_HOST_TEST
    return (uint8_t)(ms_rules_restore(snapshot) == MS_OK);
#else
    uint16_t addr;

    if (snapshot == 0) {
        return 0u;
    }
    addr = (uint16_t)snapshot;
    spectrum_overlay_context[SPECTRUM_OVL_CTX_RULES_RESTORE_SNAP_LO] =
        (uint8_t)addr;
    spectrum_overlay_context[SPECTRUM_OVL_CTX_RULES_RESTORE_SNAP_HI] =
        (uint8_t)(addr >> 8);
    spectrum_overlay_context[SPECTRUM_OVL_CTX_RULES_RESTORE_RESULT] = 0u;
    if (!spectrum_overlay_exec_cached(SPECTRUM_OVL_RULES,
                                      SPECTRUM_OVL_RULES_RESTORE_VALIDATE) ||
        spectrum_overlay_context[SPECTRUM_OVL_CTX_RULES_RESTORE_RESULT] == 0u) {
        return 0u;
    }
    ms_rules_restore_trusted(snapshot);
    return 1u;
#endif
}

uint8_t spectrum_board_apply_trusted_move_with_undo(
    const char *move, spectrum_board_undo_t *undo)
{
    if (undo == 0) {
        return 0u;
    }
    (void)ms_rules_save(&board_undo_state);
    if (ms_rules_play(move) != MS_OK) {
        undo->from = 0u;
        return 0u;
    }
    undo->from = 1u;
    return 1u;
}

uint8_t spectrum_board_apply_trusted_move(const char *move)
    NETCHESSZX_FASTCALL
{
    spectrum_board_undo_t ignored;

    return spectrum_board_apply_trusted_move_with_undo(move, &ignored);
}

void spectrum_board_undo_restore(const spectrum_board_undo_t *undo)
    NETCHESSZX_FASTCALL
{
    if (undo != 0 && undo->from != 0u) {
        ms_rules_restore_trusted(&board_undo_state);
    }
}

#ifdef NETCHESSZX_HOST_TEST
uint8_t spectrum_board_apply_move(const char *move) NETCHESSZX_FASTCALL
{
    return spectrum_board_apply_trusted_move(move);
}
#endif
