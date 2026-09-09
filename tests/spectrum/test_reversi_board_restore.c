#include "spectrum/board/board.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char *what)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        ++failures;
    }
}

static void test_invalid_restore_is_atomic(void)
{
    spectrum_reversi_snapshot_t before;
    spectrum_reversi_snapshot_t invalid;
    spectrum_reversi_snapshot_t after;
    uint8_t flip_count;

    spectrum_board_reset();
    check(spectrum_board_apply_trusted_move("d3"), "seed accepted move");
    spectrum_reversi_snapshot_save(&before);
    flip_count = spectrum_board_last_flip_count();
    invalid = before;
    invalid.cells[0] = 'K';

    check(!spectrum_reversi_snapshot_restore(&invalid),
          "invalid snapshot rejected");
    spectrum_reversi_snapshot_save(&after);
    check(memcmp(&before, &after, sizeof(before)) == 0,
          "invalid snapshot leaves board unchanged");
    check(spectrum_board_last_flip_count() == flip_count,
          "invalid snapshot leaves detail unchanged");

    invalid = before;
    invalid.over = 1u;
    check(!spectrum_reversi_snapshot_restore(&invalid),
          "terminal snapshot with moves rejected");
    spectrum_reversi_snapshot_save(&after);
    check(memcmp(&before, &after, sizeof(before)) == 0,
          "terminal rejection leaves board unchanged");

    memset(invalid.cells, MS_CELL_A, sizeof(invalid.cells));
    invalid.side = MS_SIDE_A;
    invalid.over = 0u;
    check(!spectrum_reversi_snapshot_restore(&invalid),
          "running snapshot without moves rejected");
    spectrum_reversi_snapshot_save(&after);
    check(memcmp(&before, &after, sizeof(before)) == 0,
          "no-move rejection leaves board unchanged");
}

static void test_valid_restore_and_clear(void)
{
    spectrum_reversi_snapshot_t initial;
    spectrum_reversi_snapshot_t restored;
    uint8_t i;

    spectrum_board_reset();
    spectrum_reversi_snapshot_save(&initial);
    check(spectrum_board_apply_trusted_move("d3"), "change board before restore");
    check(spectrum_reversi_snapshot_restore(&initial),
          "valid snapshot accepted");
    spectrum_reversi_snapshot_save(&restored);
    check(memcmp(&initial, &restored, sizeof(initial)) == 0,
          "valid snapshot committed exactly");
    check(spectrum_board_last_flip_count() == 0u,
          "valid restore clears flip detail");

    /* Pre-WOF saved boards keep their original cells and legal continuation. */
    initial.cells[27] = initial.cells[36] = MS_CELL_A;
    initial.cells[28] = initial.cells[35] = MS_CELL_B;
    check(spectrum_reversi_snapshot_restore(&initial),
          "pre-WOF saved position remains loadable");
    spectrum_reversi_snapshot_save(&restored);
    check(memcmp(&initial, &restored, sizeof(initial)) == 0 &&
              spectrum_board_is_legal_move("d6"),
          "pre-WOF restore preserves coordinates without migration");

    spectrum_board_clear();
    for (i = 0u; i < MS_CELLS; ++i) {
        check(spectrum_board_cells()[i] == MS_CELL_EMPTY,
              "clear empties every cell");
    }
    check(spectrum_board_side() == MS_SIDE_A, "clear restores side A");
}

static void test_undo_restores_saved_state(void)
{
    spectrum_board_undo_t undo;
    spectrum_reversi_snapshot_t before;
    spectrum_reversi_snapshot_t after;

    spectrum_board_reset();
    spectrum_reversi_snapshot_save(&before);
    check(spectrum_board_apply_trusted_move_with_undo("d3", &undo),
          "move with undo accepted");
    spectrum_board_undo_restore(&undo);
    spectrum_reversi_snapshot_save(&after);
    check(memcmp(&before, &after, sizeof(before)) == 0,
          "undo restores exact saved state");
    check(spectrum_board_last_flip_count() == 0u,
          "undo clears flip detail");
}

int main(void)
{
    test_invalid_restore_is_atomic();
    test_valid_restore_and_clear();
    test_undo_restores_saved_state();

    if (failures != 0) {
        printf("reversi board restore: %d failure(s)\n", failures);
        return 1;
    }
    printf("reversi board restore: all checks passed\n");
    return 0;
}
