#include "common/reversi/reversi.h"

/* Mirror Shift core rules. Portable C99, no allocation, no recursion, fixed
   state: the Spectrum resident build links the same object as the desktop
   client so both resolve identical state from identical move sequences. */

static const int8_t ms_dir_row[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
static const int8_t ms_dir_col[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };

#ifdef NETCHESSZX_FIXED_LOW_RAM
#if !defined(MIRRORSHIFT_FIXED_GAME_ADDR) || \
    !defined(MIRRORSHIFT_FIXED_DETAIL_ADDR)
#error "fixed Reversi storage addresses must be supplied by the platform"
#endif
#define ms_game \
    (*(ms_state_t *)MIRRORSHIFT_FIXED_GAME_ADDR)
#define ms_flip_rows \
    ((uint8_t *)MIRRORSHIFT_FIXED_DETAIL_ADDR)
#define ms_flip_count \
    (*(uint8_t *)(MIRRORSHIFT_FIXED_DETAIL_ADDR + 8u))
#define ms_last_square \
    (*(int8_t *)(MIRRORSHIFT_FIXED_DETAIL_ADDR + 9u))
#define ms_last_silences \
    (*(uint8_t *)(MIRRORSHIFT_FIXED_DETAIL_ADDR + 10u))
#else
static ms_state_t ms_game;
static uint8_t ms_flip_rows[8];
static uint8_t ms_flip_count;
static int8_t ms_last_square;
static uint8_t ms_last_silences;
#endif

static char ms_own_cell(uint8_t side)
{
    return side == MS_SIDE_A ? MS_CELL_A : MS_CELL_B;
}

static char ms_foe_cell(uint8_t side)
{
    return side == MS_SIDE_A ? MS_CELL_B : MS_CELL_A;
}

static void ms_clear_flips(void)
{
    uint8_t i;

    for (i = 0u; i < 8u; ++i) {
        ms_flip_rows[i] = 0u;
    }
    ms_flip_count = 0u;
}

/* Count the fragments a placement would rewrite; optionally rewrite them and
   record the affected rows for the Reality Shift animation. */
static uint8_t ms_scan(const char *cells,
                       char *apply_cells,
                       uint8_t index,
                       uint8_t side)
{
    const char own = ms_own_cell(side);
    const char foe = ms_foe_cell(side);
    const int8_t row = (int8_t)(index / 8u);
    const int8_t col = (int8_t)(index % 8u);
    uint8_t total = 0u;
    uint8_t d;

    if (cells[index] != MS_CELL_EMPTY) {
        return 0u;
    }

    for (d = 0u; d < 8u; ++d) {
        const int8_t dr = ms_dir_row[d];
        const int8_t dc = ms_dir_col[d];
        int8_t r = (int8_t)(row + dr);
        int8_t c = (int8_t)(col + dc);
        uint8_t run = 0u;

        while (r >= 0 && r < 8 && c >= 0 && c < 8 &&
               cells[(uint8_t)(r * 8 + c)] == foe) {
            ++run;
            r = (int8_t)(r + dr);
            c = (int8_t)(c + dc);
        }
        if (run == 0u || r < 0 || r >= 8 || c < 0 || c >= 8) {
            continue;
        }
        if (cells[(uint8_t)(r * 8 + c)] != own) {
            continue;
        }

        total = (uint8_t)(total + run);
        if (apply_cells != 0) {
            int8_t br = (int8_t)(row + dr);
            int8_t bc = (int8_t)(col + dc);

            while (br != r || bc != c) {
                apply_cells[(uint8_t)(br * 8 + bc)] = own;
                ms_flip_rows[(uint8_t)br] |= (uint8_t)(1u << (uint8_t)bc);
                br = (int8_t)(br + dr);
                bc = (int8_t)(bc + dc);
            }
        }
    }
    return total;
}

void ms_rules_reset(void)
{
    uint8_t i;

    for (i = 0u; i < MS_CELLS; ++i) {
        ms_game.cells[i] = MS_CELL_EMPTY;
    }
    /* WOF opening: A (white) D4/E5, B (black) D5/E4; black first. */
    ms_game.cells[27] = MS_CELL_B; /* d5 */
    ms_game.cells[36] = MS_CELL_B; /* e4 */
    ms_game.cells[28] = MS_CELL_A; /* e5 */
    ms_game.cells[35] = MS_CELL_A; /* d4 */
    ms_game.side = MS_SIDE_B;
    ms_game.over = 0u;

    ms_clear_flips();
    ms_last_square = MS_NO_SQUARE;
    ms_last_silences = 0u;
}

uint8_t ms_rules_side(void)
{
    return ms_game.side;
}

uint8_t ms_rules_is_over(void)
{
    return ms_game.over;
}

const char *ms_rules_cells(void)
{
    return ms_game.cells;
}

void ms_rules_score(uint8_t *a, uint8_t *b)
{
    uint8_t i;
    uint8_t ca = 0u;
    uint8_t cb = 0u;

    for (i = 0u; i < MS_CELLS; ++i) {
        if (ms_game.cells[i] == MS_CELL_A) {
            ++ca;
        } else if (ms_game.cells[i] == MS_CELL_B) {
            ++cb;
        }
    }
    if (a != 0) {
        *a = ca;
    }
    if (b != 0) {
        *b = cb;
    }
}

uint8_t ms_rules_winner(void)
{
    uint8_t a;
    uint8_t b;

    if (ms_game.over == 0u) {
        return MS_WINNER_NONE;
    }
    ms_rules_score(&a, &b);
    if (a > b) {
        return MS_WINNER_A;
    }
    return b > a ? MS_WINNER_B : MS_WINNER_PARADOX;
}

#ifndef NETCHESSZX_FIXED_LOW_RAM
const char *ms_rules_error_string(int code)
{
    switch (code) {
    case MS_OK: return "ok";
    case MS_ERR_NULL: return "null argument";
    case MS_ERR_SYNTAX: return "bad square";
    case MS_ERR_ILLEGAL: return "illegal move";
    case MS_ERR_BUFFER: return "buffer too small";
    case MS_ERR_OVER: return "game over";
    default: return "unknown error";
    }
}
#endif

uint8_t ms_rules_move_syntax_ok(const char *move)
{
    if (move == 0) {
        return 0u;
    }
    if (move[0] < 'a' || move[0] > 'h') {
        return 0u;
    }
    if (move[1] < '1' || move[1] > '8') {
        return 0u;
    }
    return (uint8_t)(move[2] == '\0');
}

int8_t ms_rules_square_index(const char *square)
{
    if (ms_rules_move_syntax_ok(square) == 0u) {
        return MS_NO_SQUARE;
    }
    return (int8_t)(('8' - square[1]) * 8 + (square[0] - 'a'));
}

#ifndef NETCHESSZX_FIXED_LOW_RAM
void ms_rules_square_name(uint8_t index, char out[3])
{
    out[0] = (char)('a' + (index % 8u));
    out[1] = (char)('8' - (index / 8u));
    out[2] = '\0';
}
#endif

uint8_t ms_rules_cells_side_has_moves(const char *cells, uint8_t side)
{
    uint8_t i;

    for (i = 0u; i < MS_CELLS; ++i) {
        if (ms_scan(cells, 0, i, side) != 0u) {
            return 1u;
        }
    }
    return 0u;
}

uint8_t ms_rules_side_has_moves(uint8_t side)
{
    return ms_rules_cells_side_has_moves(ms_game.cells, side);
}

int ms_rules_can_play(const char *move)
{
    int8_t index;

    if (move == 0) {
        return MS_ERR_NULL;
    }
    if (ms_game.over != 0u) {
        return MS_ERR_OVER;
    }
    index = ms_rules_square_index(move);
    if (index < 0) {
        return MS_ERR_SYNTAX;
    }
    return ms_scan(ms_game.cells, 0, (uint8_t)index, ms_game.side) != 0u
               ? MS_OK : MS_ERR_ILLEGAL;
}

#ifdef NETCHESSZX_FIXED_LOW_RAM
int ms_rules_can_play_index(uint8_t index)
{
    if (ms_game.over != 0u) {
        return MS_ERR_OVER;
    }
    if (index >= MS_CELLS) {
        return MS_ERR_SYNTAX;
    }
    return ms_scan(ms_game.cells, 0, index, ms_game.side) != 0u
               ? MS_OK : MS_ERR_ILLEGAL;
}
#endif

int ms_rules_play(const char *move)
{
    const int check = ms_rules_can_play(move);
    const uint8_t index = (uint8_t)ms_rules_square_index(move);
    uint8_t next;

    if (check != MS_OK) {
        return check;
    }

    ms_clear_flips();
    ms_flip_count = ms_scan(ms_game.cells, ms_game.cells, index, ms_game.side);
    ms_game.cells[index] = ms_own_cell(ms_game.side);
    ms_last_square = (int8_t)index;
    ms_last_silences = 0u;

    /* Silence is forced, so the engine resolves it here: the transcript stays
       a pure list of placements and no pass message can be lost in flight. */
    next = (uint8_t)(ms_game.side ^ 1u);
    if (ms_rules_side_has_moves(next) == 0u) {
        ms_last_silences = 1u;
        next = ms_game.side;
        if (ms_rules_side_has_moves(next) == 0u) {
            ms_last_silences = 2u;
            ms_game.over = 1u;
        }
    }
    ms_game.side = next;
    return MS_OK;
}

#ifndef NETCHESSZX_FIXED_LOW_RAM
int ms_rules_legal_moves(char *out, size_t cap)
{
    uint8_t i;
    size_t used = 0u;
    int count = 0;

    if (out == 0) {
        return MS_ERR_NULL;
    }
    if (cap == 0u) {
        return MS_ERR_BUFFER;
    }
    out[0] = '\0';
    if (ms_game.over != 0u) {
        return 0;
    }

    for (i = 0u; i < MS_CELLS; ++i) {
        char name[3];

        if (ms_scan(ms_game.cells, 0, i, ms_game.side) == 0u) {
            continue;
        }
        ms_rules_square_name(i, name);
        if (used + (used != 0u ? 1u : 0u) + 3u > cap) {
            out[0] = '\0';
            return MS_ERR_BUFFER;
        }
        if (used != 0u) {
            out[used++] = ' ';
        }
        out[used++] = name[0];
        out[used++] = name[1];
        out[used] = '\0';
        ++count;
    }
    return count;
}
#endif

int8_t ms_rules_last_square(void)
{
    return ms_last_square;
}

uint8_t ms_rules_last_flip_count(void)
{
    return ms_flip_count;
}

const uint8_t *ms_rules_last_flip_rows(void)
{
    return ms_flip_rows;
}

uint8_t ms_rules_last_silences(void)
{
    return ms_last_silences;
}

int ms_rules_save(ms_state_t *out)
{
    if (out == 0) {
        return MS_ERR_NULL;
    }
    *out = ms_game;
    return MS_OK;
}

void ms_rules_restore_trusted(const ms_state_t *in)
{
    ms_game = *in;
    ms_clear_flips();
    ms_last_square = MS_NO_SQUARE;
    ms_last_silences = 0u;
}

#ifndef NETCHESSZX_FIXED_LOW_RAM
#include "common/reversi/reversi_restore.inc"

int ms_rules_restore(const ms_state_t *in)
{
    const int check = ms_rules_validate_restore(in);

    if (check != MS_OK) {
        return check;
    }
    ms_rules_restore_trusted(in);
    return MS_OK;
}
#endif
