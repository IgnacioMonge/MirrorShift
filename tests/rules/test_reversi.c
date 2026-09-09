/* Mirror Shift M0 gate: directed positions, a full legal game and an identical
   replay, exactly as required by docs/mirror-shift-sdd.md section 21.7. */

#include "common/reversi/reversi.h"

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

static void check_str(const char *got, const char *want, const char *what)
{
    if (strcmp(got, want) != 0) {
        printf("FAIL: %s\n  got  '%s'\n  want '%s'\n", what, got, want);
        ++failures;
    }
}

/* layout: 64 chars, row-major, row 0 = rank 8, col 0 = file A. */
static void load(const char *layout, uint8_t side)
{
    ms_state_t st;

    memcpy(st.cells, layout, MS_CELLS);
    st.side = side;
    st.over = 0u;
    check(ms_rules_restore(&st) == MS_OK, "restore layout");
}

static uint8_t score_of(uint8_t side)
{
    uint8_t a;
    uint8_t b;

    ms_rules_score(&a, &b);
    return side == MS_SIDE_A ? a : b;
}

static void test_initial_position(void)
{
    char list[MS_MOVE_LIST_CAP];

    ms_rules_reset();
    check(ms_rules_side() == MS_SIDE_B, "black B moves first");
    check(!ms_rules_is_over(), "initial game running");
    check(score_of(MS_SIDE_A) == 2u && score_of(MS_SIDE_B) == 2u, "2-2 start");
    check(ms_rules_cells()[27] == MS_CELL_B, "d5 is black");
    check(ms_rules_cells()[36] == MS_CELL_B, "e4 is black");
    check(ms_rules_cells()[28] == MS_CELL_A, "e5 is white");
    check(ms_rules_cells()[35] == MS_CELL_A, "d4 is white");

    check(ms_rules_legal_moves(list, sizeof(list)) == 4, "four opening moves");
    check_str(list, "e6 f5 c4 d3", "WOF black opening move list");
}

static void test_illegal_leaves_state(void)
{
    ms_state_t before;
    ms_state_t after;

    ms_rules_reset();
    check(ms_rules_save(&before) == MS_OK, "save before");

    check(ms_rules_play("d4") == MS_ERR_ILLEGAL, "occupied square rejected");
    check(ms_rules_play("a1") == MS_ERR_ILLEGAL, "non-enclosing square rejected");
    check(ms_rules_play("z9") == MS_ERR_SYNTAX, "bad syntax rejected");
    check(ms_rules_play("d33") == MS_ERR_SYNTAX, "long token rejected");
    check(ms_rules_play(0) == MS_ERR_NULL, "null move rejected");

    check(ms_rules_save(&after) == MS_OK, "save after");
    check(memcmp(&before, &after, sizeof(before)) == 0, "state untouched");
}

static void test_invalid_restore_leaves_state(void)
{
    ms_state_t before;
    ms_state_t invalid;
    ms_state_t after;

    ms_rules_reset();
    check(ms_rules_save(&before) == MS_OK, "save before invalid restore");
    invalid = before;
    invalid.cells[0] = 'K';
    check(ms_rules_restore(&invalid) == MS_ERR_ILLEGAL,
          "reject foreign restore cell");
    invalid = before;
    invalid.side = 2u;
    check(ms_rules_restore(&invalid) == MS_ERR_ILLEGAL,
          "reject invalid restore side");
    invalid = before;
    invalid.over = 2u;
    check(ms_rules_restore(&invalid) == MS_ERR_ILLEGAL,
          "reject invalid restore over");
    invalid = before;
    invalid.over = 1u;
    check(ms_rules_restore(&invalid) == MS_ERR_ILLEGAL,
          "reject terminal restore with legal moves");
    memset(invalid.cells, MS_CELL_A, sizeof(invalid.cells));
    invalid.side = MS_SIDE_A;
    invalid.over = 0u;
    check(ms_rules_restore(&invalid) == MS_ERR_ILLEGAL,
          "reject running restore without legal moves");
    memcpy(invalid.cells,
           "........"
           "........"
           "........"
           "........"
           "........"
           "........"
           "B......."
           "AAA.....", MS_CELLS);
    invalid.side = MS_SIDE_B;
    invalid.over = 0u;
    check(ms_rules_restore(&invalid) == MS_ERR_ILLEGAL,
          "reject unresolved forced Silence");
    check(ms_rules_save(&after) == MS_OK, "save after invalid restore");
    check(memcmp(&before, &after, sizeof(before)) == 0,
          "invalid restore leaves state untouched");

    memset(invalid.cells, MS_CELL_A, sizeof(invalid.cells));
    invalid.side = MS_SIDE_A;
    invalid.over = 1u;
    check(ms_rules_restore(&invalid) == MS_OK,
          "accept terminal restore without legal moves");
}

static void test_side_swap(void)
{
    static const char original[MS_CELLS + 1] =
        "A.B....."
        ".BA....."
        "........"
        "...A...."
        "....B..."
        "........"
        ".....AB."
        "B......A";
    static const char swapped[MS_CELLS + 1] =
        "B.A....."
        ".AB....."
        "........"
        "...B...."
        "....A..."
        "........"
        ".....BA."
        "A......B";
    ms_state_t state;

    memcpy(state.cells, original, MS_CELLS);
    state.side = MS_SIDE_A;
    state.over = 0u;
    MS_STATE_SWAP_SIDES(&state);
    check(memcmp(state.cells, swapped, MS_CELLS) == 0 &&
              state.side == MS_SIDE_B && state.over == 0u,
          "fixed side-swap vector");
    MS_STATE_SWAP_SIDES(&state);
    check(memcmp(state.cells, original, MS_CELLS) == 0 &&
              state.side == MS_SIDE_A && state.over == 0u,
          "side swap round trip is identity");
}

static void test_network_opening_sequence(void)
{
    static const char *const moves[] = { "d3", "c3", "c4", "c5" };
    uint8_t i;

    ms_rules_reset();
    for (i = 0u; i < (uint8_t)(sizeof(moves) / sizeof(moves[0])); ++i) {
        check(ms_rules_play(moves[i]) == MS_OK,
              "network transcript opening remains legal");
    }
}

/* WOF official-rules sample: black C4, then white C3. */
static void test_wof_sample_opening(void)
{
    ms_rules_reset();
    check(ms_rules_play("c4") == MS_OK, "WOF black C4 accepted");
    check(ms_rules_cells()[35] == MS_CELL_B &&
              ms_rules_last_flip_count() == 1u && ms_rules_side() == MS_SIDE_A,
          "WOF C4 flips D4 and hands turn to white");
    check(ms_rules_play("c3") == MS_OK, "WOF white C3 accepted");
    check(ms_rules_cells()[35] == MS_CELL_A &&
              ms_rules_last_flip_count() == 1u && ms_rules_side() == MS_SIDE_B,
          "WOF C3 flips D4 back and hands turn to black");
}

static void test_directed_captures(void)
{
    /* Horizontal: A c4, B d4, B e4; A plays f4. */
    load("........"
         "........"
         "........"
         "........"
         "..ABB..."
         "........"
         "........"
         "........", MS_SIDE_A);
    check(ms_rules_play("f4") == MS_OK, "horizontal play accepted");
    check(ms_rules_last_flip_count() == 2u, "horizontal flips 2");
    check(score_of(MS_SIDE_B) == 0u, "horizontal wipes B");

    /* Vertical: A d4, B d5, B d6; A plays d7. */
    load("........"
         "........"
         "...B...."
         "...B...."
         "...A...."
         "........"
         "........"
         "........", MS_SIDE_A);
    check(ms_rules_play("d7") == MS_OK, "vertical play accepted");
    check(ms_rules_last_flip_count() == 2u, "vertical flips 2");

    /* Diagonal: A c3, B d4, B e5; A plays f6. */
    load("........"
         "........"
         "........"
         "....B..."
         "...B...."
         "..A....."
         "........"
         "........", MS_SIDE_A);
    check(ms_rules_play("f6") == MS_OK, "diagonal play accepted");
    check(ms_rules_last_flip_count() == 2u, "diagonal flips 2");

    /* Three directions at once: left, up and up-left from e4. */
    load("........"
         "........"
         "..A.A..."
         "...BB..."
         "..AB...."
         "........"
         "........"
         "........", MS_SIDE_A);
    check(ms_rules_play("e4") == MS_OK, "multi-direction play accepted");
    check(ms_rules_last_flip_count() == 3u, "multi-direction flips 3");
    check(ms_rules_last_square() == 36, "last square is e4");
    /* Mirrorlock rows: d4 (row 4), d5 and e5 (row 3). */
    check(ms_rules_last_flip_rows()[4] == (1u << 3), "flip row 4 is d4");
    check(ms_rules_last_flip_rows()[3] == ((1u << 3) | (1u << 4)), "flip row 3 is d5+e5");
}

static void test_forced_silence(void)
{
    /* A a1, B b1, A c1 after the play; B is left with no projection at all. */
    load("........"
         "........"
         "........"
         "........"
         "........"
         "........"
         "B......."
         "AB......", MS_SIDE_A);
    check(ms_rules_play("c1") == MS_OK, "silence-triggering play accepted");
    check(ms_rules_last_silences() == 1u, "one forced Silence");
    check(ms_rules_side() == MS_SIDE_A, "turn returns to A");
    check(!ms_rules_is_over(), "game continues after single Silence");

    /* A takes the last B fragment: both sides fall silent, Convergence. */
    check(ms_rules_play("a3") == MS_OK, "closing play accepted");
    check(ms_rules_last_silences() == 2u, "double Silence ends the game");
    check(ms_rules_is_over(), "game over on double Silence");
    check(ms_rules_winner() == MS_WINNER_A, "A wins the double Silence game");
    check(score_of(MS_SIDE_A) == 5u && score_of(MS_SIDE_B) == 0u, "5-0 score");
    check(ms_rules_play("d4") == MS_ERR_OVER, "no play after Convergence");
}

static void test_full_board(void)
{
    load(".BAAAAAA"
         "AAAAAAAA"
         "AAAAAAAA"
         "AAAAAAAA"
         "AAAAAAAA"
         "AAAAAAAA"
         "AAAAAAAA"
         "AAAAAAAA", MS_SIDE_A);
    check(ms_rules_play("a8") == MS_OK, "last empty fragment accepted");
    check(ms_rules_is_over(), "full Lattice ends the game");
    check(score_of(MS_SIDE_A) == 64u, "full board scores 64");
    check(ms_rules_winner() == MS_WINNER_A, "full board winner");
}

static void test_paradox(void)
{
    /* Two isolated groups: A closes its own, B can never reach it. */
    load(".....BBB"
         "........"
         "........"
         "........"
         "........"
         "........"
         "........"
         ".BA.....", MS_SIDE_A);
    check(ms_rules_play("a1") == MS_OK, "paradox-closing play accepted");
    check(ms_rules_is_over(), "isolated groups end the game");
    check(score_of(MS_SIDE_A) == 3u && score_of(MS_SIDE_B) == 3u, "3-3 score");
    check(ms_rules_winner() == MS_WINNER_PARADOX, "tie reports PARADOX");
}

/* Deterministic self-play: always take the first legal square in board order.
   One person driving both sides produces exactly this transcript. */
static int play_full_game(char transcript[128][3])
{
    int plies = 0;

    ms_rules_reset();
    while (!ms_rules_is_over() && plies < 128) {
        char list[MS_MOVE_LIST_CAP];
        const int count = ms_rules_legal_moves(list, sizeof(list));

        check(count > 0, "running game always has a legal move");
        if (count <= 0) {
            break;
        }
        transcript[plies][0] = list[0];
        transcript[plies][1] = list[1];
        transcript[plies][2] = '\0';
        check(ms_rules_play(transcript[plies]) == MS_OK, "self-play move accepted");
        ++plies;
    }
    return plies;
}

static void test_full_game_and_replay(void)
{
    char transcript[128][3];
    ms_state_t first;
    ms_state_t second;
    const int plies = play_full_game(transcript);
    uint8_t a;
    uint8_t b;
    int i;

    check(ms_rules_is_over(), "self-play reaches Convergence");
    check(plies >= 20, "self-play game is a real game");
    check(ms_rules_save(&first) == MS_OK, "save first run");
    ms_rules_score(&a, &b);
    check((int)a + (int)b >= 4, "final score is populated");

    ms_rules_reset();
    for (i = 0; i < plies; ++i) {
        check(ms_rules_play(transcript[i]) == MS_OK, "replayed move accepted");
    }
    check(ms_rules_save(&second) == MS_OK, "save replay");
    check(memcmp(&first, &second, sizeof(first)) == 0, "replay is identical");

    printf("self-play: %d plies, A=%u B=%u, winner=%u\n", plies,
           (unsigned)a, (unsigned)b, (unsigned)ms_rules_winner());
}

static void test_buffer_guard(void)
{
    char small[4];

    ms_rules_reset();
    check(ms_rules_legal_moves(small, sizeof(small)) == MS_ERR_BUFFER,
          "short buffer rejected");
    check(small[0] == '\0', "short buffer left empty");
    check(ms_rules_legal_moves(0, 16u) == MS_ERR_NULL, "null buffer rejected");
}

int main(void)
{
    test_initial_position();
    test_illegal_leaves_state();
    test_invalid_restore_leaves_state();
    test_side_swap();
    test_network_opening_sequence();
    test_wof_sample_opening();
    test_directed_captures();
    test_forced_silence();
    test_full_board();
    test_paradox();
    test_full_game_and_replay();
    test_buffer_guard();

    if (failures != 0) {
        printf("reversi: %d failure(s)\n", failures);
        return 1;
    }
    printf("reversi: all checks passed\n");
    return 0;
}
