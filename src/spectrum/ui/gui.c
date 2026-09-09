#include "spectrum/ui/gui.h"
#include "spectrum/ui/layout.h"
#include "spectrum/ui/info_panel.h"
#include "spectrum/ui/render.h"

#include "common/reversi/reversi.h"
#include "spectrum/config/session.h"
#include "spectrum/lowram_map.h"
#include "spectrum/platform/net_runtime.h"
#include "spectrum/platform/platform.h"
#include "spectrum/platform/text.h"
#include "spectrum/platform/uart.h"
#include "spectrum/ui/timer.h"

#include <string.h>

#define ATTR_CURSOR 0x45u
#define ATTR_TEXT 0x07u
#define ATTR_SELECTED 0x47u
#define ATTR_THEME_FLASH 0xffu

#define GUI_KEY_LEFT 0x83u
#define GUI_KEY_RIGHT 0x84u
#define MENU_OPTION_COUNT 6u
#if !defined(NETCHESSZX_NEXT) && !defined(NETCHESSZX_SPECTRANEXT) && !defined(NETCHESSZX_HOST_TEST)
#define MIRRORSHIFT_CLASSIC_BOARD_ANIMATION 1
#endif
#ifdef NETCHESSZX_NEXT
extern uint8_t net_uart_direct_idle_ticks;
#define GUI_CLOCK_FRAME_LIMIT \
    ((uint8_t)(net_uart_direct_idle_ticks == 90u ? 60u : 50u))
#else
#define GUI_CLOCK_FRAME_LIMIT 50u
#endif

static void render_clock_only(void);
static uint8_t display_row(uint8_t row) NETCHESSZX_FASTCALL;
static uint8_t display_col(uint8_t col) NETCHESSZX_FASTCALL;
static char gui_board_cell(uint8_t row, uint8_t col);
static void render_square_from_board(uint8_t row, uint8_t col);

#if !defined(NETCHESSZX_NEXT) && !defined(NETCHESSZX_HOST_TEST)
#define NETCHESSZX_CLASSIC_PIECE_SHINE 1
#endif
#ifdef NETCHESSZX_NEXT_BANKING
#define NETCHESSZX_PIECE_SHINE 1
#define SPECTRUM_ROM_FRAMES ((volatile uint8_t *)0x5c78u)
static void piece_shine_cancel(void);
static void piece_shine_reset_wait(void);
#elif defined(NETCHESSZX_CLASSIC_PIECE_SHINE)
#define NETCHESSZX_PIECE_SHINE 1
#define piece_shine_cancel spectrum_piece_reflection_cancel
#endif

#ifdef NETCHESSZX_SDCC_IY
void netchesszx_asm_put_timer_digit(char *dst, uint8_t value);
void netchesszx_asm_timer_tick_one_second(uint8_t *hour,
                                          uint8_t *minute,
                                          uint8_t *second);
#define put_timer_digit netchesszx_asm_put_timer_digit
#define timer_tick_one_second netchesszx_asm_timer_tick_one_second
#endif

#define CLOCK_TEXT_SAVE_SIZE 8u
#define GAME_TIMER_SAVE_SIZE 24u

#if NETCHESSZX_MOVE_ROWS * NETCHESSZX_MOVE_SLOT_SIZE != \
    NETCHESSZX_LOWRAM_MOVE_LOG_SIZE
#error "move log size must match low-RAM map"
#endif
#if NETCHESSZX_CHAT_ROWS * NETCHESSZX_CHAT_SLOT_SIZE != \
    NETCHESSZX_LOWRAM_CHAT_LOG_SIZE
#error "chat log size must match low-RAM map"
#endif
#if CLOCK_TEXT_SAVE_SIZE != NETCHESSZX_LOWRAM_CLOCK_SAVE_SIZE
#error "clock save size must match low-RAM map"
#endif
#if GAME_TIMER_SAVE_SIZE != NETCHESSZX_LOWRAM_GAME_TIMER_SAVE_SIZE
#error "game timer save size must match low-RAM map"
#endif
#if NETCHESSZX_LOWRAM_INPUT_HISTORY_END + NETCHESSZX_NOTICE_TEXT_SIZE > \
    NETCHESSZX_LOWRAM_STATUS_ADDR
#error "notice text must fit low-RAM gap before status"
#endif
#define move_lines ((char *)NETCHESSZX_LOWRAM_MOVE_LOG_ADDR)
#define chat_lines ((char *)NETCHESSZX_LOWRAM_CHAT_LOG_ADDR)
#define last_clock_line ((char *)NETCHESSZX_LOWRAM_CLOCK_SAVE_ADDR)
#define last_game_timer_line ((char *)NETCHESSZX_LOWRAM_GAME_TIMER_SAVE_ADDR)
#define notice_text ((char *)NETCHESSZX_LOWRAM_INPUT_HISTORY_END)
static uint8_t clock_hour;
static uint8_t clock_minute;
static uint8_t clock_second;
static uint8_t clock_valid;
uint8_t spectrum_gui_clock_frames;
#define clock_frames spectrum_gui_clock_frames
uint8_t spectrum_gui_game_timer_active_state;
#define game_timer_active spectrum_gui_game_timer_active_state
static uint8_t game_timers[SPECTRUM_TIMER_STATE_SIZE];
#define game_timer_hour game_timers[0]
#define game_timer_minute game_timers[1]
#define game_timer_second game_timers[2]
#define move_timer_hour game_timers[3]
#define move_timer_minute game_timers[4]
#define move_timer_second game_timers[5]
static uint8_t clock_force_redraw;
static uint8_t timer_force_redraw;
uint8_t spectrum_gui_menu_visible_state;
#define menu_visible spectrum_gui_menu_visible_state
static uint8_t menu_focus;
uint8_t spectrum_gui_about_visible_state;
#define about_visible spectrum_gui_about_visible_state
static uint16_t notice_ticks;
static uint8_t notice_error;
static uint8_t notice_success;
static uint16_t last_ply_seen;
static uint8_t move_line_count;
static uint8_t chat_line_count;
uint8_t spectrum_gui_board_flipped;
uint8_t spectrum_gui_board_pieces_visible_state;
#define board_pieces_visible spectrum_gui_board_pieces_visible_state
static uint8_t board_coords_dirty;
static uint8_t connected_state;
static uint8_t side_panels_visible;
uint8_t spectrum_gui_active_coord_valid;
#define active_coord_valid spectrum_gui_active_coord_valid
uint8_t spectrum_gui_active_coord_row;
uint8_t spectrum_gui_active_coord_col;
#define active_coord_row spectrum_gui_active_coord_row
#define active_coord_col spectrum_gui_active_coord_col
#ifdef NETCHESSZX_NEXT_BANKING
static uint8_t active_coord_square;
#endif
static uint8_t move_marker_mode = SPECTRUM_GUI_TURN_CLEAR;
static uint8_t move_marker_frames;
static uint8_t move_marker_visible;
static uint8_t move_marker_y;
static uint8_t move_marker_col;
#ifdef NETCHESSZX_NEXT_BANKING
static uint8_t piece_shine_wait;
static uint8_t piece_shine_targets[3];
static uint8_t piece_shine_count;
static uint8_t piece_shine_frames;
#endif
/* Sole UI-side, read-only view of the board-owned low-RAM cells. */
#define gui_live_board ((const char *)NETCHESSZX_LOWRAM_BOARD_STATE_ADDR)

static void build_status_line(char *status_line, const char *text)
{
    uint8_t i = 0u;

    while (text[i] != '\0' && i < NETCHESSZX_STATUS_LEFT_TEXT_SIZE) {
        status_line[i] = text[i];
        ++i;
    }
    /* Pad with spaces: ikkle rendering self-clears each cell, so a fixed
       width draw replaces the old text without a destructive pre-clear. */
    while (i < NETCHESSZX_STATUS_LEFT_TEXT_SIZE) {
        status_line[i] = ' ';
        ++i;
    }
    status_line[i] = '\0';
}

void spectrum_gui_set_status(const char *text) NETCHESSZX_FASTCALL
{
    char status_line[NETCHESSZX_STATUS_LEFT_TEXT_SIZE + 1u];

    build_status_line(status_line, text);
    spectrum_render_status(status_line);
    render_clock_only();
}

#ifndef NETCHESSZX_SDCC_IY
static void put_timer_digit(char *dst, uint8_t value)
{
    uint8_t tens = 0u;

    while (value >= 10u) {
        value = (uint8_t)(value - 10u);
        ++tens;
    }
    dst[0] = (char)('0' + tens);
    dst[1] = (char)('0' + value);
}
#endif

static void put_hhmm(char *dst)
{
    put_timer_digit(dst, game_timer_hour);
    dst[2] = 'h';
    put_timer_digit(dst + 3u, game_timer_minute);
    dst[5] = 'm';
}

static void put_move_timer(char *dst)
{
    if (move_timer_hour != 0u) {
        put_timer_digit(dst, move_timer_hour);
        dst[2] = 'h';
        put_timer_digit(dst + 3u, move_timer_minute);
        dst[5] = 'm';
        return;
    }

    put_timer_digit(dst, move_timer_minute);
    dst[2] = 'm';
    put_timer_digit(dst + 3u, move_timer_second);
    dst[5] = 's';
}

static void reset_move_timer(void)
{
    move_timer_hour = 0u;
    move_timer_minute = 0u;
    move_timer_second = 0u;
    clock_frames = 0u;
}

static void build_game_timer_line(char *game_timer_line)
{
    memset(game_timer_line, ' ', NETCHESSZX_GAME_TIMER_TEXT_SIZE);
    game_timer_line[NETCHESSZX_GAME_TIMER_TEXT_SIZE] = '\0';

    if (game_timer_active) {
        memcpy(game_timer_line, "GAME:", 5u);
        put_hhmm(game_timer_line + 5u);
        memcpy(game_timer_line + 11u, " TURN:", 6u);
        put_move_timer(game_timer_line + 17u);
    }
}

static uint8_t render_game_timer_delta(const char *game_timer_line, uint8_t menu_mode)
{
    char game_timer_char_spec[3];
    uint8_t i;
    uint8_t changed = 0u;

    for (i = 0u; i < NETCHESSZX_GAME_TIMER_TEXT_SIZE; ++i) {
        if (game_timer_line[i] == last_game_timer_line[i]) {
            continue;
        }
        changed = 1u;
        game_timer_char_spec[0] = (char)i;
        game_timer_char_spec[1] = game_timer_line[i];
        game_timer_char_spec[2] = (char)(game_timer_line[i] == ' ');
        if (menu_mode) {
            spectrum_render_menu_timer_char(game_timer_char_spec);
        } else {
            spectrum_render_game_timer_char(game_timer_char_spec);
        }
    }
    return changed;
}

static void render_game_timer_only(void)
{
    char game_timer_line[24];
    uint8_t force;

    if (menu_visible) {
        /* Timer pixels are shared between menu/closed states; the taboption
           open/close paths only retint attrs, so no forced redraw needed. */
        build_game_timer_line(game_timer_line);
        if (render_game_timer_delta(game_timer_line, 1u)) {
            memcpy(last_game_timer_line, game_timer_line, GAME_TIMER_SAVE_SIZE);
        }
        return;
    }

    build_game_timer_line(game_timer_line);

    force = timer_force_redraw;
    timer_force_redraw = 0u;
    if (force) {
        memcpy(last_game_timer_line, game_timer_line, GAME_TIMER_SAVE_SIZE);
        spectrum_render_game_timer_clear(game_timer_line);
    } else if (render_game_timer_delta(game_timer_line, 0u)) {
        memcpy(last_game_timer_line, game_timer_line, GAME_TIMER_SAVE_SIZE);
    }
}

static void render_clock_only(void)
{
    char clock_time[7];
    char clock_line[8];

    if (clock_valid) {
        put_timer_digit(clock_time, clock_hour);
        clock_time[2] = ':';
        put_timer_digit(clock_time + 3u, clock_minute);
    } else {
        memcpy(clock_time, "--:--", 5u);
    }
    clock_time[5] = ' ';
    clock_time[6] = '\0';

    if (!clock_force_redraw && spectrum_streq(clock_time, last_clock_line)) {
        return;
    }
    memcpy(last_clock_line, clock_time, 7u);
    clock_line[0] = '[';
    memcpy(clock_line + 1u, clock_time, 5u);
    clock_line[6] = ']';
    clock_line[7] = '\0';
    clock_force_redraw = 0u;
    spectrum_render_clock(clock_line);
}

static void render_menu_focus(uint8_t old_focus) NETCHESSZX_FASTCALL
{
    spectrum_render_menu((uint8_t)(0x80u | (uint8_t)(old_focus << 3) | menu_focus));
}

static uint8_t menu_action_key(uint8_t key) NETCHESSZX_FASTCALL
{
    menu_visible = 0u;
    spectrum_render_menu(0u);
    return key;
}

static void toggle_menu_bar(void)
{
    if (menu_visible) {
        menu_visible = 0u;
        spectrum_render_menu(0u);
    } else {
#ifdef NETCHESSZX_CLASSIC_PIECE_SHINE
        spectrum_piece_reflection_cancel();
#endif
        menu_visible = 1u;
        spectrum_render_menu((uint8_t)(menu_focus + 1u));
    }
}

void spectrum_gui_hide_menu(void)
{
    if (menu_visible) {
        (void)menu_action_key(0u);
    }
}

void spectrum_gui_set_clock(uint8_t hour, uint8_t minute, uint8_t second)
{
    clock_hour = hour;
    clock_minute = minute;
    clock_second = second;
    clock_valid = 1u;
    clock_frames = 0u;
    render_clock_only();
}

#ifdef NETCHESSZX_SPECTRANEXT
uint8_t spectrum_gui_shift_clock(int8_t hour_delta) NETCHESSZX_FASTCALL
{
    if (!clock_valid) {
        return 0u;
    }
    clock_hour = shifted_clock_hour(clock_hour, hour_delta);
    render_clock_only();
    return 1u;
}

#endif

void spectrum_gui_set_status_error(const char *text) NETCHESSZX_FASTCALL
{
    char status_line[NETCHESSZX_STATUS_LEFT_TEXT_SIZE + 1u];

    build_status_line(status_line, text);
    spectrum_render_status_error(status_line);
    render_clock_only();
}

void spectrum_gui_game_timer_start(void)
{
#ifdef NETCHESSZX_NEXT_BANKING
    piece_shine_count = 0u;
    piece_shine_reset_wait();
#elif defined(NETCHESSZX_CLASSIC_PIECE_SHINE)
    spectrum_piece_reflection_start();
#endif
    game_timer_active = 1u;
    game_timer_hour = 0u;
    game_timer_minute = 0u;
    game_timer_second = 0u;
    reset_move_timer();
    timer_force_redraw = 1u;
    render_game_timer_only();
    render_clock_only();
}

void spectrum_gui_game_timer_stop(void)
{
#ifdef NETCHESSZX_NEXT_BANKING
    piece_shine_cancel();
#elif defined(NETCHESSZX_CLASSIC_PIECE_SHINE)
    spectrum_piece_reflection_stop();
#endif
    game_timer_active = 0u;
    clock_frames = 0u;
    timer_force_redraw = 1u;
    render_game_timer_only();
    spectrum_gui_set_turn_label(SPECTRUM_GUI_TURN_CLEAR);
    render_clock_only();
}

void spectrum_gui_move_timer_reset(void)
{
    reset_move_timer();
    render_game_timer_only();
}

void spectrum_gui_game_timer_save(uint8_t *timers) NETCHESSZX_FASTCALL
{
    spectrum_timer_state_copy(timers, game_timers);
}

void spectrum_gui_game_timer_restore(const uint8_t *timers) NETCHESSZX_FASTCALL
{
    spectrum_timer_state_copy(game_timers, timers);
    clock_frames = 0u;
    timer_force_redraw = 1u;
#ifndef NETCHESSZX_HOST_TEST
    render_game_timer_only();
#endif
}

static void move_marker_render(uint8_t visible)
{
    char spec[5];

#ifndef NETCHESSZX_NEXT_BANKING
    if (about_visible == 1u) {
        return;
    }
#endif
    spec[0] = (char)move_marker_y;
    spec[1] = (char)move_marker_col;
    spec[2] = (char)ATTR_TEXT;
    spec[3] = visible ? '>' : ' ';
    spec[4] = '\0';
    spectrum_render_ikkle_abs_at(spec);
}

static void move_marker_clear(void)
{
    if (move_marker_mode != SPECTRUM_GUI_TURN_CLEAR) {
        move_marker_render(0u);
        move_marker_mode = SPECTRUM_GUI_TURN_CLEAR;
        move_marker_visible = 0u;
    }
}

static void move_marker_place(uint8_t white_to_move)
{
    uint8_t row = move_line_count;
    uint8_t col;

    if (white_to_move) {
        row = move_line_count == 0u
            ? 0u
            : (uint8_t)(move_line_count - 1u);
        col = NETCHESSZX_MOVE_WHITE_COL;
    } else {
        col = NETCHESSZX_INFO_TEXT_COL;
    }
    if (row >= NETCHESSZX_MOVE_ROWS) {
        row = NETCHESSZX_MOVE_ROWS - 1u;
    }
    move_marker_y = (uint8_t)(NETCHESSZX_INFO_MOVES_FIRST_Y +
                              (row * NETCHESSZX_INFO_TIGHT_LINE_STEP));
    move_marker_col = col;
}

void spectrum_gui_set_turn_label(uint8_t mode) NETCHESSZX_FASTCALL
{
    uint8_t label_mode;

#ifdef NETCHESSZX_PIECE_SHINE
    piece_shine_cancel();
#endif

    if (mode == SPECTRUM_GUI_TURN_MARKER_CLEAR) {
        move_marker_clear();
        return;
    }
    label_mode = (uint8_t)(mode & (uint8_t)~SPECTRUM_GUI_TURN_MARKER_WHITE);
    move_marker_clear();
    move_marker_mode = label_mode;
    move_marker_frames = 0u;
    move_marker_visible = 1u;
    spectrum_render_turn_label(label_mode);
    if (label_mode < SPECTRUM_GUI_TURN_CLEAR) {
        move_marker_place((uint8_t)(mode & SPECTRUM_GUI_TURN_MARKER_WHITE));
        move_marker_render(1u);
    }
}

void spectrum_gui_set_connected(uint8_t connected) NETCHESSZX_FASTCALL
{
    if (connected > 2u) {
        connected = 2u;
    }
    if (connected == 0u) {
        if (menu_visible) {
            (void)menu_action_key(0u);
        }
    }
    connected_state = connected;
    spectrum_render_connection(connected);
}

static void notify_internal(const char *text,
                            uint8_t is_error,
                            uint8_t is_success,
                            uint8_t ticks)
{
    strncpy(notice_text, text, NETCHESSZX_NOTICE_TEXT_SIZE - 1u);
    notice_text[NETCHESSZX_NOTICE_TEXT_SIZE - 1u] = '\0';
    notice_error = is_error;
    notice_success = is_success;
    notice_ticks = ticks;
    if (is_error) {
        spectrum_render_notice_error(notice_text);
    } else if (is_success) {
        spectrum_render_notice_success(notice_text);
    } else {
        spectrum_render_notice(notice_text);
    }
}

void spectrum_gui_notify(const char *text, uint8_t is_error)
{
    notify_internal(text, is_error, 0u, is_error ? 0u : 250u);
}

void spectrum_gui_notify_persistent(const char *text) NETCHESSZX_FASTCALL
{
    notify_internal(text, 0u, 0u, 0u);
}

void spectrum_gui_notify_success(const char *text) NETCHESSZX_FASTCALL
{
    notify_internal(text, 0u, 1u, 250u);
}

#ifdef NETCHESSZX_NEXT_BANKING
static uint8_t piece_shine_entropy(void)
{
    return (uint8_t)(SPECTRUM_ROM_FRAMES[0] ^ SPECTRUM_ROM_FRAMES[1]);
}

static void piece_shine_reset_wait(void)
{
    /* 15..22 seconds; the existing 50-frame GUI clock supplies the epoch. */
    piece_shine_wait = (uint8_t)(15u + (piece_shine_entropy() & 7u));
}

static void piece_shine_render(uint8_t square, uint8_t palette_offset)
{
    char spec[3];
    uint8_t row = (uint8_t)(square >> 3);
    uint8_t col = (uint8_t)(square & 7u);

    spec[0] = (char)display_row(row);
    spec[1] = (char)display_col(col);
    spec[2] = (char)palette_offset;
    spectrum_render_piece_palette(spec);
}

static void piece_shine_cancel(void)
{
    uint8_t i;

    for (i = 0u; i < piece_shine_count; ++i) {
        uint8_t target = piece_shine_targets[i];
        char cell = gui_live_board[target];

        if (board_pieces_visible && !about_visible &&
            (cell == MS_CELL_A || cell == MS_CELL_B)) {
            piece_shine_render(target, SPECTRUM_NEXT_PIECE_PALETTE_NORMAL);
        }
    }
    piece_shine_count = 0u;
    piece_shine_reset_wait();
}

static void piece_shine_pick(uint8_t side)
{
    uint8_t count;
    uint8_t square = (uint8_t)(piece_shine_entropy() & 63u);
    char wanted = side == MS_SIDE_A ? MS_CELL_A : MS_CELL_B;

    piece_shine_count = 0u;
    /* Step 13 visits each square once: no duplicate targets. */
    for (count = 0u; count < 64u; ++count) {
        if (gui_live_board[square] == wanted &&
            (!active_coord_valid || square != active_coord_square)) {
            piece_shine_targets[piece_shine_count++] = square;
            if (piece_shine_count == 3u) {
                return;
            }
        }
        square = (uint8_t)((square + 13u) & 63u);
    }
}

static void piece_shine_tick(void)
{
    uint8_t side = ms_rules_side();
    uint8_t i;

    if (!game_timer_active || !board_pieces_visible || menu_visible ||
        about_visible || ms_rules_is_over()) {
        if (piece_shine_count) {
            piece_shine_cancel();
        }
        return;
    }

    if (piece_shine_count) {
        char wanted = side == MS_SIDE_A ? MS_CELL_A : MS_CELL_B;

        for (i = 0u; i < piece_shine_count; ++i) {
            if (gui_live_board[piece_shine_targets[i]] != wanted) {
                piece_shine_cancel();
                return;
            }
        }
        --piece_shine_frames;
        if (piece_shine_frames == 4u) {
            for (i = 0u; i < piece_shine_count; ++i) {
                piece_shine_render(piece_shine_targets[i],
                    SPECTRUM_NEXT_PIECE_PALETTE_EXIT);
            }
        } else if (piece_shine_frames == 0u) {
            piece_shine_cancel();
        }
        return;
    }

    if (clock_frames != 0u || --piece_shine_wait != 0u) {
        return;
    }

    piece_shine_pick(side);
    if (!piece_shine_count) {
        piece_shine_reset_wait();
        return;
    }
    piece_shine_frames = 8u;
    for (i = 0u; i < piece_shine_count; ++i) {
        piece_shine_render(piece_shine_targets[i],
            SPECTRUM_NEXT_PIECE_PALETTE_ENTRY);
    }
}
#endif

void spectrum_gui_tick(void)
{
#ifdef NETCHESSZX_NEXT_BANKING
    piece_shine_tick();
#elif defined(NETCHESSZX_CLASSIC_PIECE_SHINE)
    spectrum_piece_reflection_tick();
#endif
    if (move_marker_mode != SPECTRUM_GUI_TURN_CLEAR) {
        ++move_marker_frames;
        if (move_marker_frames >= 25u) {
            move_marker_frames = 0u;
            move_marker_visible ^= 1u;
            move_marker_render(move_marker_visible);
        }
    }

    if (!notice_error && notice_ticks != 0u) {
        --notice_ticks;
        if (notice_ticks == 0u) {
            notice_text[0] = '\0';
            notice_success = 0u;
            spectrum_render_notice(notice_text);
        }
    }

    if (!game_timer_active && !clock_valid) {
        return;
    }
    ++clock_frames;
    if (clock_frames < GUI_CLOCK_FRAME_LIMIT) {
        return;
    }
    clock_frames = 0u;
    if (game_timer_active) {
        timer_tick_one_second(&game_timer_hour, &game_timer_minute,
                              &game_timer_second);
        timer_tick_one_second(&move_timer_hour, &move_timer_minute,
                              &move_timer_second);
        render_game_timer_only();
    }

    if (clock_valid) {
        ++clock_second;
        if (clock_second >= 60u) {
            clock_second = 0u;
            ++clock_minute;
            if (clock_minute >= 60u) {
                clock_minute = 0u;
                ++clock_hour;
                if (clock_hour >= 24u) {
                    clock_hour = 0u;
                }
            }
        }
        spectrum_net_runtime_tick_clock(clock_hour, clock_minute,
                                        clock_second);
        if (clock_second != 0u) {
            return;
        }
        render_clock_only();
    }
}

void spectrum_gui_reset_moves(void)
{
    memset(move_lines, 0, NETCHESSZX_MOVE_ROWS * NETCHESSZX_MOVE_SLOT_SIZE);
    move_line_count = 0u;
    last_ply_seen = 0u;
    if (side_panels_visible) {
        spectrum_render_moves(move_lines);
    }
}

void spectrum_gui_reset_logs(void)
{
    spectrum_gui_reset_moves();
    memset(chat_lines, 0, NETCHESSZX_CHAT_ROWS * NETCHESSZX_CHAT_SLOT_SIZE);
    chat_line_count = 0u;
    if (side_panels_visible) {
        spectrum_render_chat(chat_lines);
    }
}

void spectrum_gui_set_input(const char *text) NETCHESSZX_FASTCALL
{
    spectrum_gui_edit_hide();
    if (about_visible) {
        return;
    }
    spectrum_render_input(text);
}

void spectrum_gui_set_input_edit(const char *text, uint8_t len, uint8_t cursor)
{
    spectrum_gui_edit_hide();
    if (about_visible) {
        return;
    }
    if (cursor > len) {
        cursor = len;
    }
    spectrum_render_input(text);
    spectrum_gui_input_cell(cursor, cursor < len ? text[cursor] : ' ', 1u);
}

void spectrum_gui_input_cell(uint8_t pos, char c, uint8_t cursor)
{
    char spec[3];

    if (about_visible) {
        return;
    }
    spec[0] = (char)pos;
    spec[1] = c;
    spec[2] = (char)cursor;
    spectrum_render_input_cell(spec);
}

static uint8_t display_row(uint8_t row) NETCHESSZX_FASTCALL
{
    return spectrum_gui_board_flipped ? row : (uint8_t)(7u - row);
}

static uint8_t display_col(uint8_t col) NETCHESSZX_FASTCALL
{
    return spectrum_gui_board_flipped ? (uint8_t)(7u - col) : col;
}

static char gui_board_cell(uint8_t row, uint8_t col)
{
    if (row >= 8u || col >= 8u) {
        return '.';
    }
    return gui_live_board[(uint8_t)((row << 3) + col)];
}

void spectrum_gui_set_board_view(uint8_t local_black) NETCHESSZX_FASTCALL
{
    uint8_t flipped = 0u;

    /* Kept as a compatibility setter: a player's side no longer changes the
       Othello-standard A1-at-top-left orientation. */
    (void)local_black;

    if (flipped == spectrum_gui_board_flipped) {
        return;
    }
    spectrum_gui_clear_cursor_coords();
    spectrum_gui_board_flipped = flipped;
    board_coords_dirty = 1u;
}

uint8_t spectrum_gui_is_board_flipped(void)
{
    return spectrum_gui_board_flipped;
}

void spectrum_gui_toggle_board_view(void)
{
#ifdef NETCHESSZX_CLASSIC_PIECE_SHINE
    piece_shine_cancel();
#endif
    spectrum_gui_clear_cursor_coords();
    spectrum_gui_board_flipped = (uint8_t)!spectrum_gui_board_flipped;
    spectrum_gui_redraw_board_view();
}

void spectrum_gui_set_board_pieces_visible(uint8_t visible) NETCHESSZX_FASTCALL
{
#ifdef NETCHESSZX_CLASSIC_PIECE_SHINE
    if (!visible) {
        piece_shine_cancel();
    }
#endif
    board_pieces_visible = (uint8_t)(visible != 0u);
}

uint8_t spectrum_gui_board_pieces_visible(void)
{
    return board_pieces_visible;
}

void spectrum_gui_sync_board_coords(void)
{
    if (!board_coords_dirty) {
        return;
    }
    spectrum_render_board_coords();
    active_coord_valid = 0u;
    board_coords_dirty = 0u;
}

void spectrum_gui_clear_cursor_coords(void)
{
    char coord_mark_spec[3];

    if (!active_coord_valid) {
        return;
    }
    coord_mark_spec[0] = (char)active_coord_row;
    coord_mark_spec[1] = (char)active_coord_col;
    coord_mark_spec[2] = 0;
    spectrum_render_board_coord_mark(coord_mark_spec);
    active_coord_valid = 0u;
}

uint8_t spectrum_gui_show_about(void)
{
    uint8_t was_menu_visible = menu_visible;

#ifdef NETCHESSZX_CLASSIC_PIECE_SHINE
    piece_shine_cancel();
#endif
    spectrum_gui_edit_hide();
    menu_visible = 0u;
    active_coord_valid = 0u;
    if (was_menu_visible) {
        spectrum_render_menu(0u);
    }
#ifndef NETCHESSZX_NEXT_BANKING
    side_panels_visible = 0u;
#endif
    about_visible = 1u;
    if (!spectrum_render_about()) {
        about_visible = 0u;
        return 0u;
    }
    return 1u;
}

uint8_t spectrum_gui_about_visible(void)
{
    return about_visible;
}

/* The FILE browser shares the about_visible gate (value 2) so every
   board-area suppression path keeps working unchanged. */
uint8_t spectrum_gui_show_fileui(void)
{
    uint8_t was_menu_visible = menu_visible;

#ifdef NETCHESSZX_CLASSIC_PIECE_SHINE
    piece_shine_cancel();
#endif
    menu_visible = 0u;
    active_coord_valid = 0u;
    if (was_menu_visible) {
        spectrum_render_menu(0u);
    }
    about_visible = 2u;
    return 1u;
}

uint8_t spectrum_gui_fileui_visible(void)
{
    return (uint8_t)(about_visible == 2u);
}

static void spectrum_gui_mark_cursor_coords(uint8_t row, uint8_t col)
{
    char coord_mark_spec[3];
#ifdef NETCHESSZX_NEXT_BANKING
    uint8_t square = (uint8_t)((row << 3) + col);
#endif

    row = display_row(row);
    col = display_col(col);
    if (active_coord_valid &&
        active_coord_row == row &&
        active_coord_col == col) {
        return;
    }
    spectrum_gui_clear_cursor_coords();
    coord_mark_spec[0] = (char)row;
    coord_mark_spec[1] = (char)col;
    coord_mark_spec[2] = 1;
    spectrum_render_board_coord_mark(coord_mark_spec);
    active_coord_row = row;
    active_coord_col = col;
#ifdef NETCHESSZX_NEXT_BANKING
    active_coord_square = square;
#endif
    active_coord_valid = 1u;
}

void spectrum_gui_hide_board_pieces(void)
{
    if (!board_pieces_visible) {
        return;
    }
#ifdef NETCHESSZX_CLASSIC_PIECE_SHINE
    piece_shine_cancel();
#endif
    board_pieces_visible = 0u;
    if (about_visible) {
        return;
    }
    spectrum_gui_redraw_board_squares();
}

static void render_square_from_board(uint8_t row, uint8_t col)
{
    char piece;
    char square_spec[6];

    piece = board_pieces_visible ? gui_board_cell(row, col) : '.';
    square_spec[0] = (char)display_row(row);
    square_spec[1] = (char)display_col(col);
    square_spec[2] = piece;
    if (board_pieces_visible) {
        square_spec[3] = (char)row;
        square_spec[4] = (char)col;
        spectrum_render_square_with_hint(square_spec);
    } else {
        spectrum_render_square(square_spec);
    }
}

void spectrum_gui_redraw_square(uint8_t row, uint8_t col)
{
    if (about_visible) {
        return;
    }
    render_square_from_board(row, col);
}

void spectrum_gui_redraw_board_squares(void)
{
    uint8_t row;
    uint8_t col;

    if (about_visible) {
        return;
    }
    for (row = 0u; row < 8u; ++row) {
        for (col = 0u; col < 8u; ++col) {
            render_square_from_board(row, col);
        }
    }
}

static void spectrum_gui_redraw_board_flip_squares(void)
{
    uint8_t pass;
    uint8_t row;
    uint8_t col;

    for (pass = 0u; pass < 2u; ++pass) {
        for (row = 0u; row < 8u; ++row) {
            for (col = 0u; col < 8u; ++col) {
                if ((uint8_t)(gui_live_board[(uint8_t)((row << 3) + col)] != '.') == pass) {
                    render_square_from_board(row, col);
                }
            }
        }
    }
}

void spectrum_gui_mark_cursor(uint8_t row, uint8_t col, uint8_t selected)
{
    char square_spec[6];

    if (about_visible) {
        return;
    }
    spectrum_gui_mark_cursor_coords(row, col);
    square_spec[0] = (char)display_row(row);
    square_spec[1] = (char)display_col(col);
    square_spec[2] = (char)(selected ? 1u : 0u);
    if (board_pieces_visible) {
        square_spec[3] = (char)row;
        square_spec[4] = (char)col;
        square_spec[5] = gui_board_cell(row, col);
        spectrum_render_square_mark_with_hint(square_spec);
    } else {
        spectrum_render_square_mark(square_spec);
    }
}

uint8_t spectrum_gui_poll_key(void)
{
    uint8_t key = spectrum_key_poll();

#ifdef NETCHESSZX_PIECE_SHINE
    if (key != 0u) {
        piece_shine_cancel();
    }
#endif
    return key;
}

uint8_t spectrum_gui_handle_menu_key(uint8_t key) NETCHESSZX_FASTCALL
{
    if (!about_visible && key == SPECTRUM_GUI_KEY_MENU) {
        toggle_menu_bar();
        return 0u;
    }
    if (!menu_visible) {
        return key;
    }
    if (key == GUI_KEY_LEFT || key == '5' || key == 'o') {
        uint8_t old_focus = menu_focus;
        menu_focus = menu_focus == 0u ? (MENU_OPTION_COUNT - 1u) : (uint8_t)(menu_focus - 1u);
        render_menu_focus(old_focus);
    } else if (key == GUI_KEY_RIGHT || key == '8' || key == 'p') {
        uint8_t old_focus = menu_focus;
        menu_focus = (uint8_t)(menu_focus + 1u);
        if (menu_focus >= MENU_OPTION_COUNT) {
            menu_focus = 0u;
        }
        render_menu_focus(old_focus);
    } else if (key == 13u || key == 32u) {
        if (menu_focus == 0u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_FILE);
        }
        if (menu_focus == 1u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_DISCC);
        }
        if (menu_focus == 2u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_REST);
        }
        if (menu_focus == 3u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_FLIP);
        }
        if (menu_focus == 4u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_THEME);
        }
        if (menu_focus == 5u) {
            return menu_action_key(SPECTRUM_GUI_KEY_MENU_ABOUT);
        }
    }
    return 0u;
}

#ifndef MIRRORSHIFT_CLASSIC_BOARD_ANIMATION
static void wait_frames(uint8_t frames) NETCHESSZX_FASTCALL
{
    while (frames-- != 0u) {
        spectrum_frame_wait();
        spectrum_uart_background_pump();
        spectrum_gui_tick();
        spectrum_uart_background_pump();
    }
}

static void flash_square(uint8_t row, uint8_t col)
{
    uint8_t i;
    char square_spec[4];

    /* Remove the interactive cursor/marker before the first pulse. Each OFF
       redraw below then leaves the following pulse on the same clean base. */
    render_square_from_board(row, col);
    for (i = 0u; i < 2u; ++i) {
        square_spec[0] = (char)display_row(row);
        square_spec[1] = (char)display_col(col);
        /* Classic uses NetChessZX's attribute pulse; Next maps the same
           request to a native solid-colour silhouette of the active asset. */
        square_spec[2] = (char)ATTR_THEME_FLASH;
        square_spec[3] = gui_board_cell(row, col);
        spectrum_render_square_attr(square_spec);
        wait_frames(6u);
        render_square_from_board(row, col);
        wait_frames(6u);
    }
}
#endif

void spectrum_gui_prepare_move(const char *move) NETCHESSZX_FASTCALL
{
#ifdef NETCHESSZX_PIECE_SHINE
    piece_shine_cancel();
#endif
    /* Reversi has no source piece to flash before applying the placement. */
    (void)move;
}

void spectrum_gui_apply_move(const char *move) NETCHESSZX_FASTCALL
{
#ifndef MIRRORSHIFT_CLASSIC_BOARD_ANIMATION
    int8_t placed;
#endif

#ifdef NETCHESSZX_PIECE_SHINE
    piece_shine_cancel();
#endif
    (void)move;
    if (about_visible) {
        return;
    }
#ifndef MIRRORSHIFT_CLASSIC_BOARD_ANIMATION
    placed = ms_rules_last_square();
    if (placed < 0) {
        return;
    }
    flash_square((uint8_t)placed >> 3, (uint8_t)placed & 7u);
#endif
    spectrum_animate_last_flip();
}

void spectrum_gui_draw_board(void)
{
    about_visible = 0u;
    spectrum_render_board(gui_live_board);
    /* render_board wipes row 2 via hide_menu; keep the C state in sync so
       the timer force-redraw below actually repaints it. */
    menu_visible = 0u;
    active_coord_valid = 0u;
    side_panels_visible = 0u;
    board_coords_dirty = 0u;
    if (!board_pieces_visible) {
        spectrum_gui_redraw_board_squares();
    }
    clock_force_redraw = 1u;
    spectrum_gui_set_connected(connected_state);
    timer_force_redraw = 1u;
    render_game_timer_only();
    spectrum_gui_set_input("");
}

void spectrum_gui_redraw_board_view(void)
{
    spectrum_render_board_coords();
    active_coord_valid = 0u;
    board_coords_dirty = 0u;
    spectrum_gui_redraw_board_flip_squares();
}

void spectrum_gui_restore_board_area(void)
{
    about_visible = 0u;
    /* Every caller clears hints before covering the board. */
    spectrum_render_board_area(gui_live_board);
    if (!board_pieces_visible) {
        spectrum_gui_redraw_board_squares();
    }
    active_coord_valid = 0u;
    board_coords_dirty = 0u;
}

#ifndef NETCHESSZX_NEXT_BANKING
void spectrum_gui_restore_game_center(void)
{
    about_visible = 0u;
    spectrum_restore_game_center(gui_live_board);
    active_coord_valid = 0u;
    board_coords_dirty = 0u;
    spectrum_gui_restore_side_panels();
    spectrum_gui_draw_status();
}
#endif

#ifndef MIRRORSHIFT_CLASSIC_BOARD_ANIMATION
void spectrum_gui_animate_board_pieces(void)
{
    uint8_t i;

    spectrum_gui_sync_board_coords();
    if (board_pieces_visible) {
        spectrum_gui_hide_board_pieces();
    }
    spectrum_gui_set_board_pieces_visible(1u);

    /* A reset Lattice has exactly the central 2x2 seed fragments. */
    for (i = 3u; i <= 4u; ++i) {
        render_square_from_board(i, 3u);
        wait_frames(5u);
        render_square_from_board(i, 4u);
        wait_frames(5u);
    }
}
#endif

void spectrum_gui_draw_status(void)
{
    render_clock_only();
    if (notice_error) {
        spectrum_render_notice_error(notice_text);
    } else if (notice_success) {
        spectrum_render_notice_success(notice_text);
    } else {
        spectrum_render_notice(notice_text);
    }
}

void spectrum_gui_restore_side_panels(void)
{
    about_visible = 0u;
    side_panels_visible = 1u;
    spectrum_info_show_game();
    spectrum_render_moves(move_lines);
    spectrum_render_chat(chat_lines);
}

uint8_t spectrum_gui_side_panels_visible(void)
{
    return side_panels_visible;
}
