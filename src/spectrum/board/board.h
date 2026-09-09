#ifndef NETCHESSZX_SPECTRUM_BOARD_H
#define NETCHESSZX_SPECTRUM_BOARD_H

#include "common/reversi/reversi.h"

#include <stdint.h>

#ifndef NETCHESSZX_FASTCALL
#ifdef NETCHESSZX_SDCC_IY
#define NETCHESSZX_FASTCALL __z88dk_fastcall
#else
#define NETCHESSZX_FASTCALL
#endif
#endif

void spectrum_board_reset(void);
void spectrum_board_clear(void);
uint8_t spectrum_board_is_legal_move(const char *move) NETCHESSZX_FASTCALL;
/* Values 1 and 3 are historical chess ABI tombstones. */
#define SPECTRUM_BOARD_ACTIVE 0u
#define SPECTRUM_BOARD_GAME_OVER 2u
#define SPECTRUM_BOARD_MOVE_TEXT_LEN 2u
#define SPECTRUM_BOARD_WINNER_A 0u
#define SPECTRUM_BOARD_WINNER_B 1u
#define SPECTRUM_BOARD_WINNER_PARADOX 2u
typedef ms_state_t spectrum_reversi_snapshot_t;

typedef struct spectrum_board_undo {
    uint8_t from;
} spectrum_board_undo_t;

typedef char spectrum_board_undo_size_check[
    sizeof(spectrum_board_undo_t) == 1u ? 1 : -1];

uint8_t spectrum_board_check_state(void);
uint8_t spectrum_board_side(void);
uint8_t spectrum_board_winner(void);
uint8_t spectrum_board_is_legal_index(uint8_t index) NETCHESSZX_FASTCALL;
uint8_t spectrum_board_last_flip_count(void);
uint8_t spectrum_board_last_silences(void);
void spectrum_board_clear_legal_hints(void);
void spectrum_board_show_legal_hints(uint8_t from_row, uint8_t from_col);
uint8_t spectrum_board_apply_trusted_move(const char *move) NETCHESSZX_FASTCALL;
uint8_t spectrum_board_apply_trusted_move_with_undo(
    const char *move, spectrum_board_undo_t *undo);
void spectrum_board_undo_restore(const spectrum_board_undo_t *undo)
    NETCHESSZX_FASTCALL;
void spectrum_reversi_snapshot_save(spectrum_reversi_snapshot_t *out)
    NETCHESSZX_FASTCALL;
uint8_t spectrum_reversi_snapshot_restore(
    const spectrum_reversi_snapshot_t *snapshot) NETCHESSZX_FASTCALL;
/* Snapshot export for app-owned UI sync:
   64 sequential chars, row-major, row 0 = rank 8, col 0 = file A.
   Consumers may copy this buffer but must not cache or mutate it. */
const char *spectrum_board_cells(void);

#endif
