#ifndef MIRRORSHIFT_COMMON_REVERSI_H
#define MIRRORSHIFT_COMMON_REVERSI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The Lattice: 64 fragments, row-major, row 0 = rank 8, col 0 = file A.
   Same geometry and ASCII cell view as the inherited Shatranj board so the
   Spectrum and desktop renderers keep their existing snapshot plumbing. */
#define MS_CELLS 64

#define MS_CELL_EMPTY '.'
#define MS_CELL_A     'A'
#define MS_CELL_B     'B'

#define MS_SIDE_A 0u
#define MS_SIDE_B 1u

#define MS_NO_SQUARE (-1)

/* Longest legal-move list is well under 32 squares; 4 bytes per entry. */
#define MS_MOVE_LIST_CAP 128u

#define MS_WINNER_A       0u
#define MS_WINNER_B       1u
#define MS_WINNER_PARADOX 2u
#define MS_WINNER_NONE    3u

enum {
    MS_OK = 0,
    MS_ERR_NULL = -1,
    MS_ERR_SYNTAX = -2,
    MS_ERR_ILLEGAL = -3,
    MS_ERR_BUFFER = -4,
    MS_ERR_OVER = -5
};

/* Full deterministic game state. Save/restore copies it verbatim; Echoes text
   is never part of it. */
typedef struct {
    char cells[MS_CELLS];
    uint8_t side;
    uint8_t over;
} ms_state_t;

void ms_rules_reset(void);
uint8_t ms_rules_side(void);
uint8_t ms_rules_is_over(void);
const char *ms_rules_cells(void);
void ms_rules_score(uint8_t *a, uint8_t *b);
uint8_t ms_rules_winner(void);
#ifndef NETCHESSZX_FIXED_LOW_RAM
const char *ms_rules_error_string(int code);
#endif

uint8_t ms_rules_move_syntax_ok(const char *move);
int8_t ms_rules_square_index(const char *square);
void ms_rules_square_name(uint8_t index, char out[3]);

/* Exchange the two logical realities in a validated state payload. Macro form
   keeps the implementation shared without adding a resident cross-module call. */
#define MS_STATE_SWAP_SIDES(state) do {                                      \
    uint8_t ms_swap_i_;                                                       \
    for (ms_swap_i_ = 0u; ms_swap_i_ < MS_CELLS; ++ms_swap_i_) {             \
        if ((state)->cells[ms_swap_i_] == MS_CELL_A) {                        \
            (state)->cells[ms_swap_i_] = MS_CELL_B;                           \
        } else if ((state)->cells[ms_swap_i_] == MS_CELL_B) {                 \
            (state)->cells[ms_swap_i_] = MS_CELL_A;                           \
        }                                                                     \
    }                                                                         \
    (state)->side = (uint8_t)((state)->side ^ 1u);                             \
} while (0)

/* Legality and application. A move is a single square such as "d3".
   Silence (no legal placement) is resolved by the engine, never sent: after a
   successful play the side to move is always a side that can actually move,
   unless the game is over. */
int ms_rules_can_play(const char *move);
#ifdef NETCHESSZX_FIXED_LOW_RAM
int ms_rules_can_play_index(uint8_t index);
#endif
int ms_rules_play(const char *move);
uint8_t ms_rules_side_has_moves(uint8_t side);
uint8_t ms_rules_cells_side_has_moves(const char *cells, uint8_t side);
#ifndef NETCHESSZX_FIXED_LOW_RAM
int ms_rules_legal_moves(char *out, size_t cap);
#endif

/* Presentation-only detail about the last accepted play. */
int8_t ms_rules_last_square(void);
uint8_t ms_rules_last_flip_count(void);
const uint8_t *ms_rules_last_flip_rows(void);
uint8_t ms_rules_last_silences(void);

int ms_rules_save(ms_state_t *out);
/* Internal commit primitive: callers must validate or own the snapshot. */
void ms_rules_restore_trusted(const ms_state_t *in);
int ms_rules_validate_restore(const ms_state_t *in);
#ifndef NETCHESSZX_FIXED_LOW_RAM
int ms_rules_restore(const ms_state_t *in);
#endif

#ifdef __cplusplus
}
#endif

#endif
