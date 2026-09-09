#include "spectrum/board/board.h"
#include "spectrum/app/input_cmd.h"
#include "spectrum/saveload/saveload.h"
#include "spectrum/fileui/fileui.h"
#include "spectrum/restore/restore.h"
#include "spectrum/ui/gui.h"
#include "spectrum/ui/layout.h"
#include "spectrum/ui/render.h"
#include "spectrum/ui/info_panel.h"
#include "spectrum/config/session.h"
#include "spectrum/config/setup_menu.h"
#include "spectrum/session/direct.h"
#include "spectrum/session/event.h"
#include "spectrum/session/mqtt.h"
#include "spectrum/session/outgoing.h"
#include "spectrum/session/ping.h"
#include "spectrum/session/poll.h"
#include "spectrum/transport/link.h"
#include "spectrum/platform/platform.h"
#include "spectrum/platform/input.h"
#include "spectrum/lowram_map.h"
#include "spectrum/platform/net_runtime.h"
#include "spectrum/platform/text.h"
#include "spectrum/render_status.h"
#include "common/protocol/game_protocol.h"
#include "common/protocol/mqtt_session_protocol.h"
#include "common/savegame/savegame_format.h"
#include "common/ui_messages.h"

#if defined(NETCHESSZX_SPECTRANEXT) && \
    !defined(NETCHESSZX_HOST_SESSION_TEST)
#include "spxn.h"
#endif

#include <stddef.h>
#include <string.h>

uint8_t netchesszx_asm_mqtt_strlen8(const char *text) NETCHESSZX_FASTCALL;
uint8_t netchesszx_asm_restore_chunk_step(uint8_t *mask,
                                          char *cache,
                                          const char *frame);


#define KEY_UP 0x81u
#define KEY_DOWN 0x82u
#define KEY_LEFT 0x83u
#define KEY_RIGHT 0x84u
#define KEY_HOME 0x88u
#define KEY_END 0x89u
#define KEY_CANCEL 0x8au
#define LOCAL_MOVE_NET_FAIL 0u
#define LOCAL_MOVE_REJECTED 1u
#define LOCAL_MOVE_SENT 2u
#define LOCAL_KEY_OK 1u
#define LOCAL_KEY_CHAT_SENT 2u
#define LOCAL_CHAT_TEXT_MAX NETCHESSZX_CHAT_MESSAGE_TEXT_MAX
#define LOCAL_INPUT_MAX LOCAL_CHAT_TEXT_MAX
typedef char local_chat_payload_capacity_check[
    sizeof("CHAT ") + LOCAL_CHAT_TEXT_MAX == SPECTRUM_LINK_PAYLOAD_MAX ? 1 : -1];
#define INPUT_HISTORY_SIZE 2u
#define INPUT_HISTORY_TEXT_MAX 31u
#define STATUS_PHASE_CONNECTION_SETUP 0u
#define STATUS_PHASE_GAME_SETUP 1u
#define STATUS_PHASE_CONNECTING 2u
#define STATUS_PHASE_CONNECTED 3u
#define STATUS_PHASE_GAME 4u
#define STATUS_LINE_MAX 53u
#define INPUT_HISTORY_NONE 0xffu
#define MQTT_SETUP_REANNOUNCE_TICKS 200u
#define DIRECT_HELLO_REANNOUNCE_POLLS NETCHESSZX_SESSION_DIRECT_3S_POLLS
#if defined(NETCHESSZX_SPECTRANEXT)
#define LOCAL_MACH_TEXT NETCHESS_PROTO_MACH_PREFIX "SPCX"
#elif defined(NETCHESSZX_NEXT)
#define LOCAL_MACH_TEXT NETCHESS_PROTO_MACH_PREFIX "NXT"
#else
#define LOCAL_MACH_TEXT NETCHESS_PROTO_MACH_PREFIX "ZX"
#endif
#define PENDING_RETRY_POLLS \
    (netchesszx_transport_is_mqtt() \
         ? NETCHESSZX_SESSION_MQTT_REPLY_POLLS \
         : NETCHESSZX_SESSION_DIRECT_REPLY_POLLS)
#define CONTROL_REPLY_RETRIES 5u
#define CONTROL_CANCEL_POLLS \
    (netchesszx_transport_is_mqtt() \
         ? NETCHESSZX_SESSION_MQTT_GRACE_POLLS \
         : NETCHESSZX_SESSION_DIRECT_GRACE_POLLS)
#define SESSION_DISPATCH_UNHANDLED 0u
#define SESSION_DISPATCH_HANDLED 1u
#define SESSION_DISPATCH_EXIT 2u
#define SESSION_DISPATCH_HANDLED_LIVE 3u
#define SESSION_DISPATCH_RETRY_RESET 4u
#define CONTROL_PENDING_RESET 1u
#define CONTROL_PENDING_RESET_WAIT 4u
#define CONTROL_PENDING_RESET_CANCEL 5u
#define CONTROL_IS_RESET(value) \
    ((value) == CONTROL_PENDING_RESET || \
     (value) == CONTROL_PENDING_RESET_WAIT || \
     (value) == CONTROL_PENDING_RESET_CANCEL)
#define CONTROL_ACCEPT_NONE 0u
#define CONTROL_ACCEPT_RESIGN 3u
/* Empty session polls after ACK RESET during which a late sender retry is
   re-ACKed instead of opening a second prompt. 16 * WAIT_POLL frames is
   under a second: enough for the post-animation UART drain, while a later
   genuine RESET still asks. Host parity leaves this disabled except for its
   dedicated replay-window check. */
#define RESET_ACK_REPLAY_POLLS 16u
#ifdef NETCHESSZX_HOST_SESSION_TEST
static uint8_t reset_ack_replay_enabled;
#else
#define reset_ack_replay_enabled 1u
#endif
#define SETUP_ROW_GAME 0u
#define SETUP_ROW_LINK 1u
#define SETUP_ROW_ROOM 2u
#define SETUP_ROW_MQTT 3u
#define SETUP_ROW_TIME 4u
#define SETUP_ROW_SIDE 5u
#define SETUP_ROW_BOARD 6u
#define SETUP_ROW_SET 7u
#define SETUP_ROW_HINTS 8u
#define SETUP_ROW_ACTION 9u
#define SETUP_ROW_COUNT 10u
#define SETUP_MASK_GAME 0x0001u
#define SETUP_MASK_LINK 0x0002u
#define SETUP_MASK_ROOM 0x0004u
#define SETUP_MASK_MQTT 0x0008u
#define SETUP_MASK_TIME 0x0010u
#define SETUP_MASK_SIDE 0x0020u
#define SETUP_MASK_BOARD 0x0040u
#define SETUP_MASK_SET 0x0080u
#define SETUP_MASK_HINTS 0x0100u
#define SETUP_MASK_ACTION 0x0200u
#define SETUP_MASK_ALL 0x03ffu
#define SETUP_MASK_REQUIRED_JOIN (SETUP_MASK_GAME | SETUP_MASK_LINK | SETUP_MASK_ROOM | SETUP_MASK_MQTT | SETUP_MASK_BOARD | SETUP_MASK_HINTS)
#define SETUP_MASK_REQUIRED_HOST_DIRECT (SETUP_MASK_GAME | SETUP_MASK_LINK | SETUP_MASK_MQTT | SETUP_MASK_SIDE | SETUP_MASK_BOARD | SETUP_MASK_HINTS)
#define SETUP_MASK_REQUIRED_HOST_MQTT (SETUP_MASK_REQUIRED_JOIN | SETUP_MASK_SIDE)
#define SETUP_VALUE_ROLE_JOIN 0x01u
#define SETUP_VALUE_TRANSPORT_MQTT 0x02u
#define SETUP_VALUE_COLOR_BLACK 0x04u
#define SETUP_VALUE_HINTS_ON 0x08u
#define SETUP_CHOICE_ROLE 0u
#define SETUP_CHOICE_TRANSPORT 1u
#define SETUP_CHOICE_COLOR 2u
#define SETUP_CHOICE_HINTS 3u
#define SETUP_CHOICE_SET 4u
#define SETUP_CHOICE_TIME 5u
#define SETUP_DIRTY_FULL 0x80u
#define SETUP_ATTR_TEXT 0x07u
#define SETUP_ATTR_HEADER 0x03u
#define SETUP_ATTR_CURSOR 0x38u
#define SETUP_ATTR_SELECTED 0x06u
#define SETUP_ATTR_SELECTED_CURSOR 0x39u
#define SETUP_ATTR_START 0x44u
#define SETUP_ATTR_START_CURSOR 0x38u
#define SETUP_ATTR_BASE NETCHESSZX_ATTR_BASE
#define SETUP_CLEAR_NONE 0xffu
#define CONFIRM_NONE 0u
#define CONFIRM_DISCONNECT 1u
#define CONFIRM_RESET_SEND 2u
#define CONFIRM_RESET_ACCEPT 3u
#define CONFIRM_RESTART_GAME 4u
#define CONFIRM_RESIGN_SEND 5u
#define CONFIRM_TAKEBACK_SEND 6u
#define CONFIRM_TAKEBACK_ACCEPT 7u
#define CONFIRM_RESTORE_ACCEPT 8u
#define RESTORE_RX_ALL 0x03u
#define RESTORE_RX_RECEIVE 0x10u
#define RESTORE_RX_APPLIED 0x20u
#define RESTORE_TX_AWAIT_ACK 0x40u
#define RESTORE_TX_PENDING 0x80u
#define RESTORE_CHUNK_REJECT 0u
#define RESTORE_CHUNK_PARTIAL 1u
#define RESTORE_CHUNK_COMPLETE 2u
#define RESTORE_CHUNK_REACK 3u
#define SETUP_ROOM_INPUT_MAX 6u
#define SETUP_PORT_INPUT_MAX 5u
#define setup_row_bit(row) ((uint16_t)((uint16_t)1u << (row)))
#define setup_screen_row(row) ((uint8_t)((row) == SETUP_ROW_ACTION ? NETCHESSZX_SETUP_ACTION_ROW_WITH_SIDE : (NETCHESSZX_SETUP_SCREEN_ROW_BASE + (row) + ((row) >= SETUP_ROW_SIDE ? 3u : 0u))))
#define setup_clear_row(row) ((uint8_t)((row) == SETUP_ROW_ACTION ? (NETCHESSZX_SETUP_ACTION_ROW_WITH_SIDE - NETCHESSZX_INFO_SETUP_LINE_BASE_ROW) : ((row) + ((row) >= SETUP_ROW_SIDE ? 3u : 0u))))
#define setup_role setup_choice[SETUP_CHOICE_ROLE]
#define setup_transport setup_choice[SETUP_CHOICE_TRANSPORT]
#define setup_host_color setup_choice[SETUP_CHOICE_COLOR]
#define setup_hints setup_choice[SETUP_CHOICE_HINTS]
#define setup_time_source setup_choice[SETUP_CHOICE_TIME]
#define setup_focus_role setup_focus_choice[SETUP_CHOICE_ROLE]
#define setup_focus_transport setup_focus_choice[SETUP_CHOICE_TRANSPORT]
#define setup_focus_color setup_focus_choice[SETUP_CHOICE_COLOR]
#define setup_focus_hints setup_focus_choice[SETUP_CHOICE_HINTS]
#define setup_focus_piece_set setup_focus_choice[SETUP_CHOICE_SET]
#define setup_focus_time_source setup_focus_choice[SETUP_CHOICE_TIME]
#define side_to_move_is_white() \
    ((uint8_t)(spectrum_board_side() == 0u))
#define is_input_text_char(key) ((uint8_t)((key) >= 32u && (key) < 127u))
#define SETUP_EDIT_FLAG_MQTT 0x01u
#define SETUP_EDIT_FLAG_CURSOR 0x02u
#define SETUP_EDIT_FLAG_LOCAL 0x04u

#ifdef NETCHESSZX_HOST_SESSION_TEST
static char session_test_local_input[NETCHESSZX_LOWRAM_LOCAL_INPUT_SIZE];
static char session_test_input_history[NETCHESSZX_LOWRAM_INPUT_HISTORY_SIZE];
#define local_input session_test_local_input
#define input_history session_test_input_history
void netchesszx_host_session_observe_move_result(const char *notation);
#define HOST_SESSION_OBSERVE_MOVE_RESULT(notation) \
    netchesszx_host_session_observe_move_result(notation)
void netchesszx_host_session_observe_move_rejection(const char *reason);
#define HOST_SESSION_OBSERVE_MOVE_REJECTION(reason) \
    netchesszx_host_session_observe_move_rejection(reason)
extern netchesszx_session_ping_t *netchesszx_host_session_ping;
#define HOST_SESSION_EXPOSE_PING(ping) \
    (netchesszx_host_session_ping = (ping))
#else
#define local_input ((char *)NETCHESSZX_LOWRAM_LOCAL_INPUT_ADDR)
#define input_history ((char *)NETCHESSZX_LOWRAM_INPUT_HISTORY_ADDR)
#define HOST_SESSION_OBSERVE_MOVE_RESULT(notation) ((void)0)
#define HOST_SESSION_OBSERVE_MOVE_REJECTION(reason) ((void)0)
#define HOST_SESSION_EXPOSE_PING(ping) ((void)0)
#endif
#if LOCAL_INPUT_MAX + 1u > NETCHESSZX_LOWRAM_LOCAL_INPUT_SIZE
#error "local input exceeds fixed low-RAM region"
#endif
#if INPUT_HISTORY_SIZE * (INPUT_HISTORY_TEXT_MAX + 1u) > NETCHESSZX_LOWRAM_INPUT_HISTORY_SIZE
#error "input history exceeds fixed low-RAM region"
#endif
#define input_history_slot(index) \
    (input_history + ((uint16_t)(index) * (INPUT_HISTORY_TEXT_MAX + 1u)))
uint8_t local_input_len;
uint8_t local_input_cursor;
uint8_t local_input_mode;
uint8_t input_history_count;
uint8_t input_history_pos;
static uint8_t confirm_action;
static uint8_t setup_restart_requested;
static uint8_t cursor_row;
static uint8_t cursor_col;
static uint8_t local_turn;
static uint16_t game_ply;
static uint16_t pending_local_ply;
static char pending_local_move[SPECTRUM_BOARD_MOVE_TEXT_LEN + 1u];
static spectrum_board_undo_t takeback_undo;
static uint16_t takeback_snapshot_ply;
static uint16_t takeback_pending_ply;
static uint16_t last_accepted_takeback_ply;
/* Dup guard: which control action we last ACKed, so a retransmission (peer
   missed the ACK) gets an idempotent re-ACK instead of a NACK/re-prompt.
   Cleared with the takeback guard: next applied move or session teardown. */
static uint8_t last_control_accept;
static uint8_t reset_ack_replay_polls;
/* RESIGN is retransmitted until the peer sends ACK RESIGN; without this a
   resign lost in a blocked-UART window desyncs the game forever. */
static uint8_t resign_pending;
static uint8_t takeback_snapshot_local;
static uint8_t restore_rx_mask;
static uint8_t game_status_active;
static uint8_t game_over;
static uint8_t start_pending;
static uint8_t control_pending;
static uint8_t mqtt_seat_probed;
/* Low bits are the UI phase; high bits hold optional MACH peer identity. */
static uint8_t status_phase_current;

static void session_send_mach(void);

typedef struct {
    uint8_t choice[6];
    uint8_t focus_choice[6];
    uint8_t focus_board_theme;
    uint16_t defined_mask;
    uint16_t visible_mask;
    uint8_t cursor;
    uint8_t room_editing;
    uint8_t edit_row;
    char port_text[SETUP_PORT_INPUT_MAX + 1u];
    char timezone_text[4];
    int8_t timezone_value;
    uint8_t config_dirty;
    uint8_t selected_board_theme;
    uint8_t time_focus;
    uint8_t action_focus;
    uint8_t edit_was_dirty;
    char edit_backup[NETCHESSZX_MQTT_CODE_MAX + 1u];
} app_setup_workspace_t;

/* The restore overlay stages the complete wire form before producing either
   snapshot or b64 output, so those two views may alias safely. Setup is not
   live once save/load and session restore become reachable. */
typedef union {
    char restore_b64[NETCHESSZX_SAVE_WIRE_B64_SIZE];
    spectrum_reversi_snapshot_t restore_snapshot;
    app_setup_workspace_t setup;
    uint8_t config_record[SPECTRUM_CONFIG_RECORD_SIZE];
} app_workspace_t;

app_workspace_t app_workspace;

#ifndef NETCHESSZX_HOST_SESSION_TEST
typedef char app_setup_workspace_layout_check[
    sizeof(app_workspace_t) == sizeof(spectrum_reversi_snapshot_t) &&
    sizeof(app_setup_workspace_t) == 53u &&
    offsetof(app_setup_workspace_t, focus_choice) == 6u &&
    offsetof(app_setup_workspace_t, focus_board_theme) == 12u &&
    offsetof(app_setup_workspace_t, defined_mask) == 13u &&
    offsetof(app_setup_workspace_t, visible_mask) == 15u &&
    offsetof(app_setup_workspace_t, cursor) == 17u &&
    offsetof(app_setup_workspace_t, room_editing) == 18u &&
    offsetof(app_setup_workspace_t, edit_row) == 19u &&
    offsetof(app_setup_workspace_t, port_text) == 20u &&
    offsetof(app_setup_workspace_t, timezone_text) == 26u &&
    offsetof(app_setup_workspace_t, timezone_value) == 30u &&
    offsetof(app_setup_workspace_t, config_dirty) == 31u &&
    offsetof(app_setup_workspace_t, selected_board_theme) == 32u &&
    offsetof(app_setup_workspace_t, time_focus) == 33u &&
    offsetof(app_setup_workspace_t, action_focus) == 34u &&
    offsetof(app_setup_workspace_t, edit_was_dirty) == 35u &&
    offsetof(app_setup_workspace_t, edit_backup) == 36u ? 1 : -1];
#endif

#define restore_b64_pending app_workspace.restore_b64
#define restore_snapshot app_workspace.restore_snapshot
#define setup_choice app_workspace.setup.choice
#define setup_focus_choice app_workspace.setup.focus_choice
#define setup_focus_board_theme app_workspace.setup.focus_board_theme
#define setup_defined_mask app_workspace.setup.defined_mask
#define setup_visible_mask app_workspace.setup.visible_mask
#define setup_cursor app_workspace.setup.cursor
#define setup_room_editing app_workspace.setup.room_editing
#define setup_edit_row app_workspace.setup.edit_row
#define setup_port_text app_workspace.setup.port_text
#define setup_timezone_text app_workspace.setup.timezone_text
#define setup_timezone_value app_workspace.setup.timezone_value
#define setup_config_dirty app_workspace.setup.config_dirty
#define setup_game_focus app_workspace.setup.selected_board_theme
#define setup_time_focus app_workspace.setup.time_focus
#define setup_action_focus app_workspace.setup.action_focus
#define setup_edit_was_dirty app_workspace.setup.edit_was_dirty
#define setup_edit_backup app_workspace.setup.edit_backup
#define setup_config_record app_workspace.config_record

/* User-facing notice strings (<= 28 chars). Transport-agnostic and framed as
   PLAYER (local) vs OPPONENT (remote); never device identity or role-specific
   host/peer wording. Sentence case, with CAPS reserved for critical events. */
static const char msg_connect_failed[] = NETCHESSZX_UI_ERROR_CONNECTION_FAILED;
static const char msg_preflight_failed[] = NETCHESSZX_UI_ERROR_PREFLIGHT;
static const char msg_overlay_failed[] = NETCHESSZX_UI_ERROR_OVERLAY_LOAD;
static const char msg_connection_lost[] = NETCHESSZX_UI_ERROR_CONNECTION_LOST;
static const char msg_opponent_disconnected[] =
    NETCHESSZX_UI_ERROR_OPPONENT_DISCONNECTED;
static const char msg_host_busy[] = NETCHESSZX_UI_SPECTRUM_ERROR_HOST_BUSY;
static const char msg_alignment_won[] = "SELF WINS";
static const char msg_alignment_lost[] = "ECHO WINS";
static const char msg_paradox[] = "PARADOX";
#define msg_game_start_wire NETCHESS_PROTO_GAME_START
#define msg_move_prefix NETCHESS_PROTO_MOVE_PREFIX
#define msg_reset_wire NETCHESS_PROTO_RESET
#define msg_ack_reset NETCHESS_PROTO_ACK_RESET
#define msg_nack_reset NETCHESS_PROTO_NACK_RESET
#define msg_ack_resign NETCHESS_PROTO_ACK_RESIGN
#define msg_takeback_wire NETCHESS_PROTO_TAKEBACK_PREFIX
#define msg_bye NETCHESS_PROTO_BYE
#define msg_resign_wire NETCHESS_PROTO_RESIGN
static const char msg_nack_reset_busy[] = "NACK RESET BUSY";
static const char msg_ack_game_start[] = "ACK GAME START";
static const char msg_restore_rq[] = NETCHESS_PROTO_RESTORE_RQ;
static const char msg_restore_ry[] = NETCHESS_PROTO_RESTORE_RY;
static const char msg_restore_rn[] = NETCHESS_PROTO_RESTORE_RN;
static const char msg_restore_ra[] = NETCHESS_PROTO_RESTORE_RA;
static const char msg_opponent_resign[] = NETCHESSZX_UI_EVENT_OPPONENT_RESIGN;
static const char msg_retrying[] = "RETRYING...";
#ifdef NETCHESSZX_SPECTRANEXT
static char msg_config_save_failed[] = "SAVE FAILED E0/00";
static const char config_error_hex[] = "0123456789ABCDEF";
#else
static char msg_config_save_failed[] = "SAVE FAILED E0";
#endif
static const char *preflight_retry_msg;

static void handle_opponent_disconnected(void);
static void handle_opponent_disconnected_with(const char *message);
static void end_game_over(const char *message);
static void local_controls_reset(uint8_t is_local_turn);
static uint8_t tcp_required(uint8_t sent) __z88dk_fastcall;
static uint8_t session_tcp_send(const char *text) __z88dk_fastcall;
static uint8_t session_tcp_move_reply(uint8_t ack, uint16_t ply);
static void cursor_hide(void);
static void about_restore_full_board(void);
static void notify_info(const char *text)
{
    /* Compile-time aliases: setup overlays keep their resident ABI while the
       setup state shares storage with the restore buffer. No Z80 is emitted. */
#ifndef NETCHESSZX_HOST_SESSION_TEST
#asm
    PUBLIC _setup_choice
    PUBLIC _setup_focus_choice
    PUBLIC _setup_focus_board_theme
    PUBLIC _setup_defined_mask
    PUBLIC _setup_visible_mask
    PUBLIC _setup_cursor
    PUBLIC _setup_room_editing
    PUBLIC _setup_edit_row
    PUBLIC _setup_port_text
    PUBLIC _setup_timezone_text
    PUBLIC _setup_timezone_value
    PUBLIC _setup_config_dirty
    PUBLIC _setup_game_focus
    PUBLIC _setup_time_focus
    PUBLIC _setup_action_focus
    PUBLIC _setup_edit_was_dirty
    PUBLIC _setup_edit_backup
    PUBLIC _setup_config_record
    defc _setup_choice = _app_workspace
    defc _setup_focus_choice = _app_workspace + 6
    defc _setup_focus_board_theme = _app_workspace + 12
    defc _setup_defined_mask = _app_workspace + 13
    defc _setup_visible_mask = _app_workspace + 15
    defc _setup_cursor = _app_workspace + 17
    defc _setup_room_editing = _app_workspace + 18
    defc _setup_edit_row = _app_workspace + 19
    defc _setup_port_text = _app_workspace + 20
    defc _setup_timezone_text = _app_workspace + 26
    defc _setup_timezone_value = _app_workspace + 30
    defc _setup_config_dirty = _app_workspace + 31
    defc _setup_game_focus = _app_workspace + 32
    defc _setup_time_focus = _app_workspace + 33
    defc _setup_action_focus = _app_workspace + 34
    defc _setup_edit_was_dirty = _app_workspace + 35
    defc _setup_edit_backup = _app_workspace + 36
    defc _setup_config_record = _app_workspace
#endasm
#endif
    spectrum_gui_notify(text, 0u);
}

static void notify_error(const char *text)
{
    spectrum_gui_notify(text, 1u);
}

#define notify_info_msg(id) \
    spectrum_gui_notify_msg(SPECTRUM_GUI_MSG_PACK((id), SPECTRUM_GUI_MSG_KIND_INFO))
#define notify_error_msg(id) \
    spectrum_gui_notify_msg(SPECTRUM_GUI_MSG_PACK((id), SPECTRUM_GUI_MSG_KIND_ERROR))
#define notify_wait_msg(id) \
    spectrum_gui_notify_msg(SPECTRUM_GUI_MSG_PACK((id), SPECTRUM_GUI_MSG_KIND_WAIT))
#define notify_success_msg(id) \
    spectrum_gui_notify_msg(SPECTRUM_GUI_MSG_PACK((id), SPECTRUM_GUI_MSG_KIND_SUCCESS))
static void notify_wait_opponent(void)
{
    notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_OPPONENT);
}

static void notify_wait_opponent_ack(void)
{
    notify_wait_msg(SPECTRUM_GUI_MSG_RESET_WAIT);
}

static void notify_move_rejected(void)
{
    notify_error_msg(SPECTRUM_GUI_MSG_MOVE_REJECTED);
}

static void notify_reset_timeout(uint8_t local)
{
    uint8_t id = SPECTRUM_GUI_MSG_CONTROL_RESET_CANCELLED;

    if (!local) {
        ++id;
    }
    notify_info_msg(id);
}

static void wait_after_notice(void);

static uint8_t wait_notice_frames(uint8_t cancel_key)
{
    uint16_t wait_frames = 250u;

    while (wait_frames-- != 0u) {
        spectrum_frame_wait();
        spectrum_link_background_drain();
        spectrum_gui_tick();
        spectrum_link_background_drain();
        if (cancel_key && spectrum_gui_poll_key() == KEY_CANCEL) {
            return 1u;
        }
    }
    return 0u;
}

static uint8_t retry_after_error(const char *msg)
{
    notify_error(msg);
    return wait_notice_frames(1u);
}

static void wait_after_notice(void)
{
    (void)wait_notice_frames(0u);
}

static uint8_t preflight_clock_line(const char *line)
{
    spectrum_info_line(line);
    spectrum_frame_wait();
    spectrum_gui_tick();
    return 1u;
}

#if defined(NETCHESSZX_NEXT) || defined(NETCHESSZX_SPECTRANEXT)
#define clock_sync_run spectrum_link_sync_time
#else
static uint8_t clock_sync_run(void)
{
    if (netchesszx_timezone == NETCHESSZX_TIME_RTC) {
        netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_KEY] = 0xffu;
        if (spectrum_overlay_exec_cached(SPECTRUM_OVL_TIME_CONFIG,
                                         SPECTRUM_OVL_TIME_CONFIG_INIT)) {
            netchesszx_rtc_available = 1u;
            spectrum_net_runtime_set_clock(
                netchesszx_setup_overlay_context[2],
                netchesszx_setup_overlay_context[1],
                netchesszx_setup_overlay_context[0]);
            return 1u;
        }
        netchesszx_rtc_available = 0u;
    }
    return spectrum_link_sync_time();
}
#endif

static uint8_t preflight_clock_run(void)
{
    if (!preflight_clock_line(NETCHESSZX_PREFLIGHT_ROW_TIME_PREFIX "CLOCK WAIT")) {
        return 0u;
    }
    if (clock_sync_run()) {
        (void)preflight_clock_line(NETCHESSZX_PREFLIGHT_ROW_TIME_PREFIX "CLOCK OK  ");
    } else {
        (void)preflight_clock_line(NETCHESSZX_PREFLIGHT_ROW_TIME_PREFIX "CLOCK FAIL");
    }
    return 1u;
}

static uint8_t connection_preflight_run(uint8_t quiet)
{
    uint8_t rc = spectrum_link_preflight_run(quiet);

    if (rc == SPECTRUM_LINK_PREFLIGHT_OK) {
        if (spectrum_net_runtime_clock_ready()) {
            return 1u;
        }
#ifdef NETCHESSZX_SPECTRANEXT
        if (!quiet) {
            (void)clock_sync_run();
        }
        return 1u;
#else
        return quiet ? 1u : preflight_clock_run();
#endif
    }
    if (rc == SPECTRUM_LINK_PREFLIGHT_RETRYING) {
        preflight_retry_msg = msg_retrying;
    } else if (rc == SPECTRUM_LINK_PREFLIGHT_OVL_FAIL) {
        preflight_retry_msg = msg_overlay_failed;
        spectrum_gui_set_status_error(msg_overlay_failed);
        spectrum_gui_draw_status();
    } else {
        preflight_retry_msg = msg_preflight_failed;
    }
    return 0u;
}

static void status_show_phase(uint8_t phase)
{
    status_phase_current = SPECTRUM_STATUS_WITH_PHASE(status_phase_current,
                                                      phase);
    spectrum_gui_status_phase(status_phase_current);
}

static void status_show_endpoint(void)
{
    status_show_phase(STATUS_PHASE_CONNECTED);
}

static void status_show_connecting(void)
{
    status_show_phase(STATUS_PHASE_CONNECTING);
}

static void status_show_connection_setup(void)
{
    status_show_phase(STATUS_PHASE_CONNECTION_SETUP);
}

static void status_refresh_game(void)
{
    uint8_t mode;
    uint8_t white_to_move;
    uint8_t self_to_move;

    if (!game_status_active) {
        return;
    }
    status_show_phase(STATUS_PHASE_GAME);
    white_to_move = side_to_move_is_white();
    self_to_move = netchesszx_session_has_local_turn(white_to_move);
    if (spectrum_board_last_silences() == 1u) {
        mode = self_to_move
            ? SPECTRUM_GUI_TURN_SELF_SILENCE
            : SPECTRUM_GUI_TURN_ECHO_SILENCE;
    } else {
        mode = self_to_move
            ? SPECTRUM_GUI_TURN_SELF
            : SPECTRUM_GUI_TURN_ECHO;
    }
    spectrum_gui_set_turn_label((uint8_t)(mode |
        (white_to_move ? SPECTRUM_GUI_TURN_MARKER_WHITE : 0u)));
}

static void pending_local_clear(void)
{
    pending_local_ply = 0u;
    pending_local_move[0] = '\0';
}

static void takeback_clear(void)
{
    takeback_pending_ply = 0u;
    takeback_snapshot_ply = 0u;
    takeback_snapshot_local = 0u;
    last_accepted_takeback_ply = 0u;
    last_control_accept = CONTROL_ACCEPT_NONE;
}

static void pending_takeback_clear(void)
{
    pending_local_clear();
    takeback_clear();
}

static void takeback_snapshot_save(uint16_t ply, uint8_t local_move)
{
    takeback_snapshot_ply = ply;
    takeback_snapshot_local = local_move;
    last_accepted_takeback_ply = 0u;
    last_control_accept = CONTROL_ACCEPT_NONE;
}

static void restore_about_full_board_if_visible(void)
{
    if (spectrum_gui_about_visible()) {
        about_restore_full_board();
    }
}

static void reset_board_moves_chat(void)
{
    spectrum_board_reset();
    spectrum_gui_reset_logs();
}

static void clear_disconnected_session_state(void)
{
    local_turn = 0u;
    game_status_active = 0u;
    game_over = 0u;
    start_pending = 0u;
    resign_pending = 0u;
    control_pending = 0u;
    restore_rx_mask = 0u;
    netchesszx_session_peer_reset();
    status_phase_current &= SPECTRUM_STATUS_PHASE_MASK;
    if (!netchesszx_transport_is_mqtt() &&
        !netchesszx_session_is_host()) {
        netchesszx_host_color_ready = 0u;
    }
    pending_takeback_clear();
    spectrum_input_flush_until_release();
    spectrum_gui_game_timer_stop();
}

static uint16_t parse_u16(const char *text)
{
    uint16_t value;
    return netchess_mqtt_session_parse_u16_token(text, &value) == 0 ? 0u : value;
}

#ifdef NETCHESSZX_HOST_SESSION_TEST
static uint8_t nav_key_alias(uint8_t key)
{
    if (key == '5' || key == 'o') {
        return KEY_LEFT;
    }
    if (key == '6' || key == 'a') {
        return KEY_DOWN;
    }
    if (key == '7' || key == 'q') {
        return KEY_UP;
    }
    if (key == '8' || key == 'p') {
        return KEY_RIGHT;
    }
    return key;
}
#else
#define nav_key_alias netchesszx_setup_nav_key_alias
#endif

static void suppress_current_key(void)
{
    spectrum_input_flush_until_release();
}

static void suppress_key_until_release(uint8_t key);
static void session_setup_preview_piece_set(void);
static void session_setup_sync_board_view(void);


static uint16_t mqtt_new_session_id(void)
{
    uint16_t id = *(volatile uint16_t *)0x5c78u;
    return id == 0u ? 1u : id;
}

#ifdef NETCHESSZX_HOST_SESSION_TEST
static void session_setup_config_ui(void)
{
    (void)spectrum_overlay_exec_cached(SPECTRUM_OVL_TIME_CONFIG,
                                       SPECTRUM_OVL_TIME_CONFIG_UI);
}

static uint8_t session_setup_dispatch(uint8_t key)
{
    if ((setup_room_editing && setup_edit_row == SETUP_ROW_TIME) ||
        (!setup_room_editing &&
         (setup_cursor == SETUP_ROW_TIME || setup_cursor == SETUP_ROW_ACTION) &&
         key != KEY_UP && key != KEY_DOWN)) {
        netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_KEY] = key;
        return spectrum_overlay_exec_cached(SPECTRUM_OVL_TIME_CONFIG,
                                            SPECTRUM_OVL_TIME_CONFIG_STEP);
    }
    return netchesszx_setup_step_overlay(key);
}
#else
#define session_setup_config_ui netchesszx_setup_time_ui
#define session_setup_dispatch netchesszx_setup_dispatch_overlay
#endif

static void session_setup_render(uint8_t full,
                                 uint16_t force_dirty,
                                 uint8_t clear_from)
{
    setup_visible_mask = netchesszx_setup_compute_visible(setup_defined_mask);
    netchesszx_setup_render_overlay(force_dirty,
        (uint16_t)full | ((uint16_t)clear_from << 8));
    netchesszx_setup_render_edit_line(0xffu);
    session_setup_config_ui();
}

static uint8_t session_setup_start(uint8_t key)
{
    if (!netchesszx_setup_time_commit()) {
        return 0u;
    }
    if (setup_transport == NETCHESSZX_TRANSPORT_MQTT &&
        setup_role == NETCHESSZX_SESSION_ROLE_HOST) {
        netchesszx_mqtt_session_id = mqtt_new_session_id();
    } else {
        netchesszx_mqtt_session_id = 0u;
    }
    mqtt_seat_probed = 0u;
    suppress_key_until_release(key);
    return 1u;
}

#ifdef NETCHESSZX_SPECTRANEXT
static void session_setup_apply_time(void)
{
    int8_t timezone = setup_timezone_value;
    int8_t previous_timezone = netchesszx_timezone;

    if (setup_choice[SETUP_CHOICE_TIME] && netchesszx_rtc_available) {
        timezone = NETCHESSZX_TIME_RTC;
    }
    if (timezone == netchesszx_timezone) {
        return;
    }
    netchesszx_timezone_last = setup_timezone_value;
    netchesszx_timezone = timezone;
    if (previous_timezone != NETCHESSZX_TIME_RTC &&
        timezone != NETCHESSZX_TIME_RTC &&
        spectrum_net_runtime_shift_clock((int8_t)(timezone - previous_timezone))) {
        return;
    }
    (void)clock_sync_run();
}
#endif

static void session_setup_preview_piece_set(void)
{
    if (setup_focus_piece_set == netchesszx_piece_set_index) {
        return;
    }
#ifndef NETCHESSZX_HOST_SESSION_TEST
    spectrum_piece_masks_stash();
#endif
    if (!netchesszx_piece_set_load(setup_focus_piece_set)) {
        notify_error_msg(SPECTRUM_GUI_MSG_SET_LOAD_FAILED);
        return;
    }
    netchesszx_piece_set_index = setup_focus_piece_set;
#ifndef NETCHESSZX_HOST_SESSION_TEST
    spectrum_animate_set_morph();
#ifdef NETCHESSZX_NEXT_BANKING
    (void)netchesszx_piece_set_finalize();
#endif
#endif
}

static void session_setup_sync_board_view(void)
{
    uint8_t local_black = setup_focus_color;

    if (setup_focus_role == NETCHESSZX_SESSION_ROLE_JOIN) {
        local_black ^= 1u;
    }
    spectrum_gui_set_board_view(local_black);
    spectrum_gui_sync_board_coords();
}

static void session_setup_apply_set(uint8_t key)
{
    session_setup_preview_piece_set();
    if (setup_focus_piece_set != netchesszx_piece_set_index) {
        return;
    }
    setup_choice[SETUP_CHOICE_SET] = setup_focus_piece_set;
    setup_defined_mask |= SETUP_MASK_SET;
    setup_config_dirty = 1u;
    setup_action_focus = 0u;
    setup_cursor = SETUP_ROW_HINTS;
    netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_FLAGS] =
        NETCHESSZX_SETUP_FLAG_ACTION_UI;
    session_setup_render(0u, SETUP_MASK_SET, SETUP_CLEAR_NONE);
    suppress_key_until_release(key);
}

#ifdef NETCHESSZX_HOST_SESSION_TEST
static void session_setup_init(uint8_t flags)
{
    netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_KEY] = flags;
    (void)spectrum_overlay_exec_cached(SPECTRUM_OVL_TIME_CONFIG,
                                       SPECTRUM_OVL_TIME_CONFIG_INIT);
}
#else
#define session_setup_init netchesszx_setup_time_init
#endif

static void session_setup_save(uint8_t key)
{
    uint16_t visible_mask = setup_visible_mask;
    uint8_t saved;
    uint8_t save_stage;
#if defined(NETCHESSZX_SPECTRANEXT) && \
    !defined(NETCHESSZX_HOST_SESSION_TEST)
    uint8_t rom_error;
#endif

    if (!netchesszx_setup_time_commit()) {
        return;
    }
    /* CONFIG aliases app_workspace and overwrites live Setup fields. Reinit
       restores them; the visible rows and their pixels did not change. */
    spectrum_overlay_context[SPECTRUM_OVL_CTX_CONFIG_STAGE] = 0xffu;
    saved = netchesszx_config_save_overlay();
    save_stage = spectrum_overlay_context[SPECTRUM_OVL_CTX_CONFIG_STAGE];
#if defined(NETCHESSZX_SPECTRANEXT) && \
    !defined(NETCHESSZX_HOST_SESSION_TEST)
    /* Snapshot immediately: any later ROM call replaces the shared byte. */
    rom_error = spxn_rom_error();
#endif
    session_setup_init((uint8_t)(saved | 2u));
    setup_visible_mask = visible_mask;
    session_setup_config_ui();
    if (!saved) {
        msg_config_save_failed[13] =
            save_stage <= SPECTRUM_CONFIG_SAVE_STAGE_FILE_COMMIT
                ? (char)('0' + save_stage)
                : '?';
#if defined(NETCHESSZX_SPECTRANEXT) && \
    !defined(NETCHESSZX_HOST_SESSION_TEST)
        msg_config_save_failed[15] = config_error_hex[rom_error >> 4];
        msg_config_save_failed[16] = config_error_hex[rom_error & 0x0fu];
#endif
        notify_error(msg_config_save_failed);
    }
    suppress_key_until_release(key);
}

static uint8_t session_setup_step(uint8_t key)
{
    uint8_t flags;
    uint8_t action;
    uint8_t notice;
    uint16_t force_dirty;
    uint8_t clear_from;

    if (!session_setup_dispatch(key)) {
        notify_error(msg_overlay_failed);
        return 0u;
    }
    flags = netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_FLAGS];
    action = netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_ACTION];
    notice = netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_NOTICE];
    force_dirty = (uint16_t)netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_FORCE_LO] |
                  ((uint16_t)netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_FORCE_HI] << 8);
    clear_from = netchesszx_setup_overlay_context[NETCHESSZX_SETUP_CTX_CLEAR_FROM];

    if (action == NETCHESSZX_SETUP_ACTION_START) {
        return session_setup_start(key);
    }
    if (action == NETCHESSZX_SETUP_ACTION_SAVE) {
        session_setup_save(key);
        return 0u;
    }
#ifndef NETCHESSZX_SPECTRANEXT
    if (action == NETCHESSZX_SETUP_ACTION_TIMEZONE &&
        netchesszx_setup_time_commit()) {
        spectrum_link_clock_retry_start();
    }
#endif
    if (force_dirty & SETUP_MASK_BOARD) {
        volatile uint8_t board_theme = setup_focus_board_theme;

        if (netchesszx_board_theme_index != board_theme) {
            netchesszx_board_theme_apply(board_theme);
#ifdef NETCHESSZX_NEXT_BANKING
            spectrum_gui_redraw_board_squares();
#endif
        }
    }
    if (action == NETCHESSZX_SETUP_ACTION_SET) {
        session_setup_apply_set(key);
        return 0u;
    }
    if (flags & NETCHESSZX_SETUP_FLAG_RENDER) {
        session_setup_render(0u, force_dirty, clear_from);
    }
    if (flags & NETCHESSZX_SETUP_FLAG_EDIT) {
        if (setup_edit_row != SETUP_ROW_TIME) {
            netchesszx_setup_render_edit_line(setup_edit_row);
        }
    }
    if (flags & NETCHESSZX_SETUP_FLAG_PAINT) {
        netchesszx_setup_paint_attrs();
    }
    if (!(flags & NETCHESSZX_SETUP_FLAG_RENDER) &&
        (flags & (NETCHESSZX_SETUP_FLAG_TIME_UI |
                  NETCHESSZX_SETUP_FLAG_ACTION_UI))) {
        session_setup_config_ui();
    }
    session_setup_sync_board_view();
    if (setup_cursor == SETUP_ROW_SET) {
        session_setup_preview_piece_set();
    }
    if (notice == NETCHESSZX_SETUP_NOTICE_SELECT) {
        notify_info_msg(SPECTRUM_GUI_MSG_SELECT_OPTIONS);
    } else if (notice == NETCHESSZX_SETUP_NOTICE_BAD_IP) {
        notify_error_msg(SPECTRUM_GUI_MSG_BAD_IP);
    } else if (notice == NETCHESSZX_SETUP_NOTICE_BAD_ROOM) {
        notify_error_msg(SPECTRUM_GUI_MSG_INVALID_MQTT_ROOM);
    } else if (notice == NETCHESSZX_SETUP_NOTICE_BAD_PORT) {
        notify_error_msg(SPECTRUM_GUI_MSG_BAD_PORT);
    } else if (notice == NETCHESSZX_SETUP_NOTICE_BAD_TIMEZONE) {
        notify_error_msg(SPECTRUM_GUI_MSG_BAD_TIMEZONE);
    }
    if (flags & NETCHESSZX_SETUP_FLAG_SUPPRESS) {
        suppress_key_until_release(key);
    }
    /* Clock-source probing may reuse the overlay context: paint first. */
    if (action == NETCHESSZX_SETUP_ACTION_TIMEZONE) {
#ifdef NETCHESSZX_SPECTRANEXT
        session_setup_apply_time();
#endif
    }
    return 0u;
}

static uint8_t session_setup_run(uint8_t config_state)
{
    uint8_t key;

    session_setup_init(config_state);
    session_setup_render(1u, 0u, SETUP_CLEAR_NONE);
    session_setup_sync_board_view();
    spectrum_board_reset();
    spectrum_gui_animate_board_pieces();
    status_show_phase(STATUS_PHASE_GAME_SETUP);
    if (config_state == SPECTRUM_CONFIG_STATE_INVALID) {
        notify_error_msg(SPECTRUM_GUI_MSG_CONFIG_INVALID);
    } else {
        notify_info_msg(SPECTRUM_GUI_MSG_SELECT_OPTIONS);
    }
    suppress_current_key();

    while (1) {
        key = spectrum_gui_poll_key();
        if (key == 0u) {
            spectrum_frame_wait();
            spectrum_gui_tick();
            continue;
        }
        if (!setup_room_editing) {
            key = nav_key_alias(key);
        }
        if (session_setup_step(key)) {
            return (uint8_t)(SPECTRUM_CONFIG_STATE_SAVED ^ setup_config_dirty);
        }
    }
}

static uint8_t input_has_text(const char *text)
{
    while (*text != '\0') {
        if (*text != ' ') {
            return 1u;
        }
        ++text;
    }
    return 0u;
}

static void edit_stop_clear(void)
{
    netchesszx_input_edit_stop_clear_overlay();
}

static void disconnect_to_setup(void)
{
    setup_restart_requested = 1u;
    (void)spectrum_link_send_text(msg_bye);
    /* Clear our retained seat before leaving: publish a retained offline
       marker (F <side> <sid>) over the retained presence O so a guest
       rejoining the room doesn't see a stale MQTT_SEAT_TAKEN and bounce with
       BUSY. Only meaningful once we actually claimed a seat (session id set);
       BUSY exits happen pre-activation and never published an O. */
    if (netchesszx_transport_is_mqtt()) {
        if (netchesszx_mqtt_session_id != 0u) {
            (void)spectrum_link_mqtt_publish_offline(
                SPECTRUM_LINK_ROUTE_PRESENCE);
        }
        /* BYE/offline have already been handed to the UART. Leave transparent
           TCP mode so the warm Setup preflight starts in command mode. */
        spectrum_link_mqtt_stop();
    }
#ifdef NETCHESSZX_SPECTRANEXT
    else {
        /* DIRECT owns the cartridge's long-lived TCP socket.  Release it
           before Setup can open the TIME overlay's SNTP socket. */
        spectrum_link_start_uart();
    }
#endif
    clear_disconnected_session_state();
    edit_stop_clear();
    spectrum_gui_set_connected(0u);
}

static uint8_t active_peer_ready(void)
{
    return (uint8_t)(!game_status_active || netchesszx_session_peer_ready_state);
}

static void square_from_cursor(char *square, uint8_t row, uint8_t col)
{
    square[0] = (char)('a' + col);
    square[1] = (char)('8' - row);
    square[2] = '\0';
}

static uint8_t cursor_try_legal_square(uint8_t row, uint8_t col)
{
    if (spectrum_board_is_legal_index((uint8_t)((row << 3) + col))) {
        cursor_row = row;
        cursor_col = col;
        return 1u;
    }
    return 0u;
}

static void cursor_reset_default_square(void)
{
    uint8_t row;
    uint8_t col;

    for (row = 0u; row < 8u; ++row) {
        for (col = 0u; col < 8u; ++col) {
            if (cursor_try_legal_square(row, col)) {
                return;
            }
        }
    }

    cursor_row = 6u;
    cursor_col = 4u;
}

static uint8_t movement_hints_clear(void)
{
    uint8_t row;
    uint8_t has_hints = 0u;

    for (row = 0u; row < 8u; ++row) {
        if (netchesszx_hinted_rows[row] != 0u) {
            has_hints = 1u;
            break;
        }
    }
    if (has_hints) {
        spectrum_board_clear_legal_hints();
    }
    return has_hints;
}

static void movement_hints_show(void)
{
    uint8_t row;

    if (netchesszx_movement_hints == 0u || local_turn == 0u ||
        pending_local_ply != 0u) {
        return;
    }
    for (row = 0u; row < 8u; ++row) {
        if (netchesszx_hinted_rows[row] != 0u) {
            return;
        }
    }
    spectrum_board_show_legal_hints(0u, 0u);
}

static void cursor_redraw_square(uint8_t row, uint8_t col)
{
    spectrum_gui_redraw_square(row, col);
}

static void cursor_hide(void)
{
    movement_hints_clear();
    spectrum_gui_clear_cursor_coords();
    spectrum_gui_redraw_square(cursor_row, cursor_col);
}

static void cursor_show(void)
{
    if (!active_peer_ready() || !local_turn || pending_local_ply != 0u) {
        return;
    }
    spectrum_gui_mark_cursor(cursor_row, cursor_col, 0u);
}

static void input_stop_and_show_cursor(void)
{
    edit_stop_clear();
    cursor_show();
}

static void about_restore_game(void)
{
#ifdef NETCHESSZX_NEXT_BANKING
    spectrum_render_about_off();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_restore_board_area();
#else
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    if (spectrum_gui_fileui_visible()) {
        spectrum_gui_restore_board_area();
    } else {
        spectrum_gui_restore_game_center();
    }
#endif
    if (local_input_mode) {
        netchesszx_input_edit_render_overlay();
    } else if (local_turn) {
        movement_hints_show();
        cursor_show();
    }
    suppress_current_key();
}

static void about_restore_full_board(void)
{
#ifdef NETCHESSZX_NEXT_BANKING
    spectrum_render_about_off();
#endif
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_draw_board();
    status_show_phase(status_phase_current);
    spectrum_gui_restore_side_panels();
    if (local_input_mode) {
        netchesszx_input_edit_render_overlay();
    }
}

static void about_open(void)
{
    movement_hints_clear();
    spectrum_gui_clear_cursor_coords();
    if (!spectrum_gui_show_about()) {
        spectrum_gui_set_board_snapshot(spectrum_board_cells());
        spectrum_gui_restore_board_area();
        notify_error(msg_overlay_failed);
        return;
    }
    suppress_current_key();
}

static void turn_set_notice(uint8_t is_local_turn, uint8_t show_notice)
{
    if (is_local_turn) {
        if (local_turn) {
            cursor_hide();
        } else {
            movement_hints_clear();
        }
        cursor_reset_default_square();
        local_turn = 1u;
        status_refresh_game();
        if (show_notice) {
            notify_info_msg(SPECTRUM_GUI_MSG_YOUR_TURN);
        }
        movement_hints_show();
        cursor_show();
    } else {
        cursor_hide();
        local_turn = 0u;
        status_refresh_game();
        if (show_notice) {
            notify_info_msg(SPECTRUM_GUI_MSG_OPPONENT_TURN);
        }
    }
}

static void local_controls_reset(uint8_t is_local_turn)
{
    edit_stop_clear();
    spectrum_input_flush_until_release();
    cursor_row = 6u;
    cursor_col = 4u;
    if (is_local_turn) {
        turn_set_notice(1u, 1u);
    } else {
        local_turn = 0u;
        if (game_status_active) {
            status_refresh_game();
            notify_info_msg(SPECTRUM_GUI_MSG_OPPONENT_TURN);
        }
    }
}

static void suppress_key_until_release(uint8_t key)
{
    spectrum_input_suppress_until_release(key);
}

static void apply_takeback_snapshot(void)
{
    if (takeback_snapshot_ply == 0u) {
        return;
    }
    movement_hints_clear();
    pending_local_clear();
    spectrum_board_undo_restore(&takeback_undo);
    game_ply = (uint16_t)(takeback_snapshot_ply - 1u);
    game_over = 0u;
    game_status_active = 1u;
    spectrum_gui_add_chat(
        (char)((uint8_t)(takeback_snapshot_local
                             ? netchesszx_local_side_char()
                             : netchesszx_remote_side_char()) |
               NETCHESSZX_CHAT_EVENT_SIDE_FLAG),
        NETCHESS_PROTO_TAKEBACK);
    spectrum_gui_remove_last_move(takeback_snapshot_ply);
    takeback_clear();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_redraw_board_squares();
    spectrum_gui_move_timer_reset();
    turn_set_notice(netchesszx_session_has_local_turn(side_to_move_is_white()),
                    1u);
}

static uint8_t saveload_flags(void)
{
    uint8_t flags = 0u;

    if (game_status_active) {
        flags |= NETCHESSZX_SAVE_FLAG_ACTIVE;
    }
    if (game_over) {
        flags |= NETCHESSZX_SAVE_FLAG_GAME_OVER;
    }
    return flags;
}

static void local_save_game(const char *name)
{
    netchesszx_save_meta_t meta;

    spectrum_reversi_snapshot_save(&restore_snapshot);
    meta.ply = game_ply;
    meta.flags = saveload_flags();
    meta.host_color = netchesszx_host_color;
    meta.view_flags = spectrum_gui_is_board_flipped()
        ? NETCHESSZX_SAVE_VIEW_FLIPPED
        : 0u;
    spectrum_gui_game_timer_save(meta.timers);
    if (spectrum_restore_build_b64(&restore_snapshot, &meta,
                                   restore_b64_pending) &&
        spectrum_saveload_write(name, restore_b64_pending)) {
        notify_info_msg(SPECTRUM_GUI_MSG_SAVE_OK);
    } else {
        notify_error_msg(SPECTRUM_GUI_MSG_SAVE_FAIL);
    }
}

static uint8_t saveload_apply_snapshot(const spectrum_reversi_snapshot_t *snap,
                                       const netchesszx_save_meta_t *meta)
{
    if (!spectrum_reversi_snapshot_restore(snap)) {
        return 0u;
    }
    movement_hints_clear();
    pending_takeback_clear();
    spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
#ifdef NETCHESSZX_NEXT_BANKING
    spectrum_next_sprites_hide_all();
#endif
    game_ply = meta->ply;
    game_status_active = (uint8_t)(meta->flags & NETCHESSZX_SAVE_FLAG_ACTIVE);
    game_over = (uint8_t)(meta->flags & NETCHESSZX_SAVE_FLAG_GAME_OVER);
    start_pending = 0u;
    resign_pending = 0u;
    control_pending = 0u;
    confirm_action = CONFIRM_NONE;
    spectrum_gui_reset_logs();
    spectrum_gui_set_board_pieces_visible(1u);
    spectrum_gui_redraw_board_squares();
    if (game_status_active && !game_over) {
        spectrum_gui_game_timer_start();
        spectrum_gui_game_timer_restore(meta->timers);
        turn_set_notice(netchesszx_session_has_local_turn(side_to_move_is_white()), 0u);
    } else {
        spectrum_gui_game_timer_stop();
        spectrum_gui_game_timer_restore(meta->timers);
        cursor_hide();
        local_turn = 0u;
        status_refresh_game();
    }
    return 1u;
}

static uint8_t restore_host_color_ok(const netchesszx_save_meta_t *meta)
{
    return (uint8_t)(meta->host_color == netchesszx_host_color);
}

/* Offer in the current Host/Guest seating. A save from when this machine
   was host stores the opposite host_color; swap A/B so SELF keeps its discs. */
static void restore_adapt_host_color(spectrum_reversi_snapshot_t *snap,
                                     netchesszx_save_meta_t *meta)
{
    if (meta->host_color == netchesszx_host_color) {
        return;
    }
    MS_STATE_SWAP_SIDES(snap);
    meta->host_color = netchesszx_host_color;
}

static uint8_t restore_send_chunks(void)
{
    uint8_t chunk;
    uint8_t off;

    if ((restore_rx_mask & (RESTORE_TX_PENDING | RESTORE_TX_AWAIT_ACK)) == 0u) {
        return 0u;
    }
    local_input[0] = 'R';
    local_input[1] = 'S';
    local_input[2] = '0';
    local_input[4] = ' ';
    local_input[NETCHESSZX_SAVE_RESTORE_FRAME_MAX] = '\0';
    for (chunk = 0u; chunk < 2u; ++chunk) {
        off = (uint8_t)(chunk * NETCHESSZX_SAVE_WIRE_CHUNK_SIZE);
        local_input[3] = (char)('0' + chunk);
        memcpy(local_input + 5u, restore_b64_pending + off,
               NETCHESSZX_SAVE_WIRE_CHUNK_SIZE);
        if (!session_tcp_send(local_input)) {
            restore_rx_mask = 0u;
            return 0u;
        }
    }
    restore_rx_mask = RESTORE_TX_AWAIT_ACK;
    notify_wait_msg(SPECTRUM_GUI_MSG_RESET_WAIT);
    return 1u;
}

static uint8_t send_takeback_wire(uint16_t ply)
{
    char payload[24];
    char *p = spectrum_append_text(payload, msg_takeback_wire);

    p = spectrum_append_u16(p, ply);
    *p = '\0';
    return spectrum_link_send_text(payload);
}

static void local_load_game(const char *name)
{
    netchesszx_save_meta_t meta;

    if (!spectrum_saveload_read(name, restore_b64_pending) ||
        !spectrum_restore_decode(restore_b64_pending, &restore_snapshot,
                                 &meta)) {
        notify_error_msg(SPECTRUM_GUI_MSG_LOAD_FAIL);
        return;
    }
    restore_adapt_host_color(&restore_snapshot, &meta);
    if (!netchesszx_session_peer_ready_state) {
        if (saveload_apply_snapshot(&restore_snapshot, &meta)) {
            notify_info_msg(SPECTRUM_GUI_MSG_LOAD_OK);
        } else {
            notify_error_msg(SPECTRUM_GUI_MSG_LOAD_FAIL);
        }
        return;
    }
    /* Decode consumes all b64 into overlay-local wire storage before writing
       the aliased snapshot. Rebuild the wire text needed by the peer. */
    if (!spectrum_restore_build_b64(&restore_snapshot, &meta,
                                    restore_b64_pending)) {
        notify_error_msg(SPECTRUM_GUI_MSG_LOAD_FAIL);
        return;
    }
    restore_rx_mask = RESTORE_TX_PENDING;
    if (!session_tcp_send(msg_restore_rq)) {
        restore_rx_mask = 0u;
        return;
    }
    notify_wait_msg(SPECTRUM_GUI_MSG_LOAD_WAITING_APPROVAL);
}

#define FILEUI_CLOSE_KEY 0x8au

static void fileui_open(void)
{
    movement_hints_clear();
    spectrum_gui_clear_cursor_coords();
#ifdef NETCHESSZX_NEXT_BANKING
    spectrum_next_sprites_hide_all();
#endif
    spectrum_gui_show_fileui();
    if (!spectrum_fileui_open_render()) {
        spectrum_gui_set_board_snapshot(spectrum_board_cells());
        spectrum_gui_restore_board_area();
        notify_error(msg_overlay_failed);
        return;
    }
    suppress_current_key();
}

static uint8_t fileui_process_key(uint8_t key) NETCHESSZX_FASTCALL
{
    uint8_t action;

    if (key == 0u) {
        return 1u;
    }
    if (key == FILEUI_CLOSE_KEY || key == SPECTRUM_GUI_KEY_MENU_FILE ||
        key == SPECTRUM_GUI_KEY_MENU) {
        about_restore_game();
        return 1u;
    }
    action = spectrum_fileui_send_key(key);
    if (action == SPECTRUM_FILEUI_ACT_LOAD) {
        about_restore_game();
        local_load_game(spectrum_fileui_selected_name());
    } else if (action == SPECTRUM_FILEUI_ACT_SAVE) {
        local_save_game(spectrum_fileui_selected_name());
        spectrum_fileui_rerender();
    } else if (action == SPECTRUM_FILEUI_ACT_ERASE) {
        spectrum_saveload_erase(spectrum_fileui_selected_name());
        spectrum_fileui_rerender();
    }
    suppress_current_key();
    return 1u;
}

static void alignment_display_text(const char *move, char out[8], uint8_t state)
{
    char *p = spectrum_append_text(out, move);
    uint8_t flips = spectrum_board_last_flip_count();

    if (flips != 0u) {
        *p++ = 'x';
        p = spectrum_append_u16(p, flips);
    }
    if (spectrum_board_last_silences() == 1u) {
        *p++ = '!';
    }
    if (state == SPECTRUM_BOARD_GAME_OVER) {
        *p++ = '#';
    }
    *p = '\0';
}

static void finish_applied_move(const char *ply_text,
                                const char *move,
                                uint8_t author)
{
    char display[8];
    uint8_t state;
    uint8_t winner;
    uint8_t local_winner;

    state = spectrum_board_check_state();
    alignment_display_text(move, display, state);
    spectrum_overlay_context[SPECTRUM_OVL_CTX_GUI_MOVE_AUTHOR] =
        (uint8_t)(SPECTRUM_OVL_GUI_MOVE_AUTHOR_VALID |
                  (author == NETCHESSZX_COLOR_BLACK
                       ? SPECTRUM_OVL_GUI_MOVE_AUTHOR_BLACK
                       : 0u));
    spectrum_gui_add_move(ply_text, display);
    spectrum_gui_apply_move(move);
    pending_local_clear();
    if (state == SPECTRUM_BOARD_GAME_OVER) {
        winner = spectrum_board_winner();
        if (winner == SPECTRUM_BOARD_WINNER_PARADOX) {
            end_game_over(msg_paradox);
            return;
        }
        local_winner = netchesszx_local_is_white()
            ? SPECTRUM_BOARD_WINNER_A : SPECTRUM_BOARD_WINNER_B;
        end_game_over(winner == local_winner ? msg_alignment_won
                                             : msg_alignment_lost);
        return;
    }
    /* The compact log groups consecutive plies in pairs, independently of
       Reversi's forced-silence side changes. Reserve after the second column. */
    if (author == NETCHESSZX_COLOR_WHITE) {
        spectrum_gui_prepare_move_row();
    }
    spectrum_gui_move_timer_reset();
    turn_set_notice(netchesszx_session_has_local_turn(side_to_move_is_white()),
                    1u);
}

static uint8_t tcp_required(uint8_t sent) __z88dk_fastcall
{
    if (sent) {
        return 1u;
    }
    handle_opponent_disconnected();
    return 0u;
}

static uint8_t session_tcp_send(const char *text) __z88dk_fastcall
{
    return tcp_required(spectrum_link_send_text(text));
}

static uint8_t session_tcp_move_reply(uint8_t ack, uint16_t ply)
{
    char ply_text[8];

    (void)spectrum_append_u16(ply_text, ply);
    return tcp_required(ack
        ? netchesszx_session_send_ack_move(ply_text)
        : netchesszx_session_send_nack_move(ply_text));
}

static void end_game_over(const char *message)
{
    const char *chat_event = 0;
    char chat_who = '\0';

    if (message == msg_opponent_resign) {
        chat_event = msg_opponent_resign;
        chat_who = netchesszx_remote_side_char();
    } else if (*message == 'R') {
        chat_event = msg_resign_wire;
        chat_who = netchesszx_local_side_char();
    }
    if (chat_event != 0) {
        spectrum_gui_add_chat(
            (char)((uint8_t)chat_who | NETCHESSZX_CHAT_EVENT_SIDE_FLAG),
            chat_event);
    }
    edit_stop_clear();
    cursor_hide();
    local_turn = 0u;
    game_status_active = 0u;
    game_over = 1u;
    pending_takeback_clear();
    control_pending = 0u;
    restore_rx_mask = 0u;
    confirm_action = CONFIRM_NONE;
    spectrum_gui_game_timer_stop();
    status_show_endpoint();
#ifndef NETCHESSZX_HOST_SESSION_TEST
    spectrum_animate_convergence();
    spectrum_gui_hide_board_pieces();
#endif
    notify_error(message);
}

static uint8_t local_action_ready(void)
{
    if (resign_pending) {
        notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_RESIGN_ACK);
        return 0u;
    }
    if (game_over && last_control_accept == CONTROL_ACCEPT_RESIGN) {
        notify_info_msg(SPECTRUM_GUI_MSG_RESIGN_ALREADY_APPLIED);
        return 0u;
    }
    if (!game_status_active) {
        notify_error_msg(SPECTRUM_GUI_MSG_GAME_NOT_STARTED);
        return 0u;
    }
    if (control_pending || takeback_pending_ply != 0u) {
        notify_wait_opponent_ack();
        return 0u;
    }
    if (!netchesszx_session_peer_ready_state) {
        notify_wait_opponent();
        return 0u;
    }
    return 1u;
}

static void game_start_state(void)
{
    uint8_t setup_seeds_visible = (uint8_t)(!game_status_active && !game_over &&
        spectrum_gui_board_pieces_visible());

    spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
    if (!setup_seeds_visible) {
        spectrum_gui_hide_board_pieces();
    }
    spectrum_board_reset();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_reset_moves();
    game_ply = 0u;
    pending_takeback_clear();
    game_status_active = 1u;
    game_over = 0u;
    start_pending = 0u;
    resign_pending = 0u;
    spectrum_gui_game_timer_start();
    if (!setup_seeds_visible) {
        spectrum_gui_animate_board_pieces();
    }
    spectrum_gui_set_connected(2u);
    status_refresh_game();
    local_controls_reset(netchesszx_session_has_local_turn(side_to_move_is_white()));
    cursor_show();
}

/* Shared local start: reset board, timers, and show GAME STARTED.
   Used after accepted RESET/rematch, ACK GAME START, and guest GAME START.
   Wire GAME START itself remains the initial-start path only. */
static void reset_auto_start(void)
{
    game_start_state();
    notify_success_msg(SPECTRUM_GUI_MSG_GAME_STARTED);
}

static void note_reset_ack_replay(void)
{
    if (reset_ack_replay_enabled) {
        reset_ack_replay_polls = RESET_ACK_REPLAY_POLLS;
    }
}

static void accepted_reset_start(void)
{
    confirm_action = CONFIRM_NONE;
    control_pending = 0u;
    reset_auto_start();
    last_control_accept = CONTROL_ACCEPT_NONE;
    note_reset_ack_replay();
}

static uint8_t game_start_local(uint8_t start_key)
{
    if (game_status_active) {
        return 1u;
    }
    if (!netchesszx_session_can_start_game()) {
        notify_wait_msg(SPECTRUM_GUI_MSG_OPPONENT_STARTS_GAME);
        return 1u;
    }
    if (!netchesszx_session_peer_ready_state) {
        notify_wait_opponent();
        return 1u;
    }
    if (start_pending) {
        notify_wait_opponent_ack();
        return 1u;
    }
    notify_wait_msg(SPECTRUM_GUI_MSG_STARTING_GAME);
    if (!netchesszx_session_send_start_game()) {
        notify_error_msg(SPECTRUM_GUI_MSG_START_FAILED);
        return 0u;
    }
    game_over = 0u;
    start_pending = 1u;
    /* Keep the calm starting-game notice until ACK GAME START flips it to
       "GAME STARTED". Showing the yellow "Waiting opponent ACK" here only
       flashes for one round-trip and reads like an error to the user. */
    suppress_key_until_release(start_key);
    cursor_show();
    return 1u;
}

static uint8_t send_local_chat(const char *text)
{
    char *payload = spectrum_link_payload_scratch();

    if (!input_has_text(text)) {
        return 1u;
    }
    {
        char *p = spectrum_append_text(payload, NETCHESS_PROTO_CHAT_PREFIX);
        char *end = payload + SPECTRUM_LINK_PAYLOAD_MAX - 1u;

        while (*text != '\0' && p < end) {
            *p++ = *text++;
        }
        *p = '\0';
    }
    if (!spectrum_link_send_text(payload)) {
        return 0u;
    }
    spectrum_gui_add_chat(netchesszx_local_side_char(), payload + 5u);
    return 1u;
}

static uint8_t send_move_wire(uint16_t ply, const char *move)
{
    char payload[24];
    char *p = spectrum_append_text(payload, msg_move_prefix);

    p = spectrum_append_u16(p, ply);
    *p++ = ' ';
    (void)spectrum_append_text(p, move);
    return spectrum_link_send_text(payload);
}

static uint8_t send_local_move(const char *move)
{
    uint16_t ply;

    if (!game_status_active) {
        notify_info_msg(SPECTRUM_GUI_MSG_GAME_NOT_STARTED);
        return LOCAL_MOVE_REJECTED;
    }
    if (control_pending) {
        goto wait_opponent_ack;
    }
    if (!active_peer_ready()) {
        notify_wait_opponent();
        return LOCAL_MOVE_REJECTED;
    }

    if (pending_local_ply != 0u || takeback_pending_ply != 0u) {
        goto wait_opponent_ack;
    }

    if (game_ply == 65535u || !spectrum_board_is_legal_move(move)) {
        notify_move_rejected();
        return LOCAL_MOVE_REJECTED;
    }

    ply = (uint16_t)(game_ply + 1u);
    spectrum_gui_notify_persistent(move);
    if (!send_move_wire(ply, move)) {
        if (netchesszx_transport_is_mqtt()) {
            (void)spectrum_link_mqtt_publish_offline(
                SPECTRUM_LINK_ROUTE_PRESENCE);
        }
        return LOCAL_MOVE_NET_FAIL;
    }

    pending_local_ply = ply;
    (void)spectrum_append_text(pending_local_move, move);
    return LOCAL_MOVE_SENT;

wait_opponent_ack:
    notify_wait_opponent_ack();
    return LOCAL_MOVE_REJECTED;
}

static uint8_t retry_pending_outgoing(void)
{
    if (resign_pending) {
        return spectrum_link_send_text(msg_resign_wire);
    }
    if (pending_local_ply != 0u) {
        return send_move_wire(pending_local_ply, pending_local_move);
    }
    if (start_pending) {
        return netchesszx_session_send_start_game();
    }
    if (CONTROL_IS_RESET(control_pending)) {
        return spectrum_link_send_text(
            control_pending == CONTROL_PENDING_RESET_CANCEL
                ? NETCHESS_PROTO_CANCEL_RESET : msg_reset_wire);
    }
    if (takeback_pending_ply != 0u && takeback_snapshot_local) {
        return send_takeback_wire(takeback_pending_ply);
    }
    if ((restore_rx_mask & RESTORE_TX_PENDING) != 0u) {
        return spectrum_link_send_text(msg_restore_rq);
    }
    if ((restore_rx_mask & RESTORE_TX_AWAIT_ACK) != 0u) {
        return restore_send_chunks();
    }
    return 1u;
}

static uint8_t local_retry_pending(void)
{
    return (uint8_t)(pending_local_ply != 0u || start_pending ||
        resign_pending || CONTROL_IS_RESET(control_pending) ||
        (takeback_pending_ply != 0u && takeback_snapshot_local) ||
        (restore_rx_mask &
         (RESTORE_TX_PENDING | RESTORE_TX_AWAIT_ACK)) != 0u);
}

static void apply_pending_local_move(const char *notation)
{
    char ply_text[8];
    uint8_t author = netchesszx_local_color;

    (void)notation;

    if (pending_local_ply == 0u) {
        return;
    }

    (void)spectrum_append_u16(ply_text, pending_local_ply);
    spectrum_gui_prepare_move(pending_local_move);
    if (!spectrum_board_apply_trusted_move_with_undo(pending_local_move,
                                                      &takeback_undo)) {
        pending_local_clear();
        notify_move_rejected();
        cursor_show();
        return;
    }
    takeback_snapshot_save(pending_local_ply, 1u);
    spectrum_gui_set_board_snapshot(spectrum_board_cells());

    game_ply = pending_local_ply;
    finish_applied_move(ply_text, pending_local_move, author);
    HOST_SESSION_OBSERVE_MOVE_RESULT(notation);
}

static void cursor_move(uint8_t key)
{
    uint8_t old_row = cursor_row;
    uint8_t old_col = cursor_col;
    uint8_t flipped = spectrum_gui_is_board_flipped();

    if (!active_peer_ready() || !local_turn || pending_local_ply != 0u) {
        return;
    }

    if (key == KEY_UP || key == 'q') {
        if (flipped) {
            if (cursor_row > 0u) {
                --cursor_row;
            }
        } else if (cursor_row < 7u) {
            ++cursor_row;
        }
    } else if (key == KEY_DOWN || key == 'a') {
        if (flipped) {
            if (cursor_row < 7u) {
                ++cursor_row;
            }
        } else if (cursor_row > 0u) {
            --cursor_row;
        }
    } else if (key == KEY_LEFT || key == 'o') {
        if (flipped) {
            if (cursor_col < 7u) {
                ++cursor_col;
            }
        } else if (cursor_col > 0u) {
            --cursor_col;
        }
    } else if (key == KEY_RIGHT || key == 'p') {
        if (flipped) {
            if (cursor_col > 0u) {
                --cursor_col;
            }
        } else if (cursor_col < 7u) {
            ++cursor_col;
        }
    }

    if (old_row != cursor_row || old_col != cursor_col) {
        cursor_redraw_square(old_row, old_col);
        if (pending_local_ply == 0u) {
            spectrum_gui_mark_cursor(cursor_row, cursor_col, 0u);
        }
    }
}

static uint8_t cursor_select_or_move(uint8_t key)
{
    uint8_t move_rc;
    char move[SPECTRUM_BOARD_MOVE_TEXT_LEN + 1u];

    if (!game_status_active) {
        if (!game_start_local(key)) {
            return 1u;
        }
        return 1u;
    }
    if (control_pending) {
        notify_wait_opponent_ack();
        return 1u;
    }
    if (!active_peer_ready()) {
        notify_wait_opponent();
        return 1u;
    }

    if (!local_turn) {
        notify_info_msg(SPECTRUM_GUI_MSG_OPPONENT_TURN);
        return 1u;
    }
    if (pending_local_ply != 0u) {
        notify_wait_opponent_ack();
        return 1u;
    }

    square_from_cursor(move, cursor_row, cursor_col);
    move_rc = send_local_move(move);
    if (move_rc == LOCAL_MOVE_NET_FAIL) {
        return 0u;
    }
    if (move_rc == LOCAL_MOVE_REJECTED) {
        movement_hints_show();
        cursor_show();
        return 1u;
    }
    /* Hint removal already restores a legal cursor square. Without hints,
       restore that square here exactly once. */
    if (!movement_hints_clear()) {
        spectrum_gui_redraw_square(cursor_row, cursor_col);
    }
    spectrum_gui_clear_cursor_coords();
    return 1u;
}

static uint8_t restore_transfer_pending(void)
{
    return (uint8_t)((restore_rx_mask &
        (RESTORE_TX_PENDING | RESTORE_TX_AWAIT_ACK | RESTORE_RX_RECEIVE)) != 0u);
}

static uint8_t send_busy_move_reply(const char *ply)
{
    char *out = spectrum_link_payload_scratch();

    out = spectrum_append_text(out, "NACK ");
    out = spectrum_append_text(out, ply);
    (void)spectrum_append_text(out, " BUSY");
    return session_tcp_send(spectrum_link_payload_scratch());
}

static uint8_t restore_cancel_pending_request(void)
{
    if ((restore_rx_mask & RESTORE_TX_PENDING) == 0u) {
        notify_wait_msg(SPECTRUM_GUI_MSG_RESET_WAIT);
        suppress_current_key();
        return 1u;
    }
    if (!session_tcp_send(msg_restore_rn)) {
        return 0u;
    }
    restore_rx_mask = 0u;
    notify_info_msg(SPECTRUM_GUI_MSG_LOAD_CANCELLED);
    suppress_current_key();
    return 1u;
}

static uint8_t input_submit(void)
{
    char move[SPECTRUM_BOARD_MOVE_TEXT_LEN + 1u];
    uint8_t rc;

    if (!input_has_text(local_input)) {
        input_stop_and_show_cursor();
        return LOCAL_KEY_OK;
    }
    if (restore_transfer_pending()) {
        notify_wait_msg(SPECTRUM_GUI_MSG_LOAD_WAITING_APPROVAL);
        return 1u;
    }

    if (spectrum_input_parse_move(local_input, move)) {
        if (game_over) {
            notify_error_msg(SPECTRUM_GUI_MSG_GAME_NOT_STARTED);
            input_stop_and_show_cursor();
            return 1u;
        }
        if (control_pending || takeback_pending_ply != 0u) {
            notify_wait_opponent_ack();
            input_stop_and_show_cursor();
            return 1u;
        }
        if (!game_status_active) {
            if (!game_start_local(0u)) {
                return 1u;
            }
            if (!game_status_active) {
                input_stop_and_show_cursor();
                return 1u;
            }
        }
        if (!local_turn) {
            notify_error_msg(SPECTRUM_GUI_MSG_OPPONENT_TURN);
            input_stop_and_show_cursor();
            return 1u;
        }
        if (pending_local_ply != 0u) {
            notify_wait_opponent_ack();
            input_stop_and_show_cursor();
            return 1u;
        }
        netchesszx_input_edit_history_add_overlay(local_input);
        rc = send_local_move(move);
        if (rc == LOCAL_MOVE_REJECTED) {
            cursor_show();
        } else {
            edit_stop_clear();
        }
        return (uint8_t)(rc != LOCAL_MOVE_NET_FAIL);
    }

    if (spectrum_streq(local_input, "/resign")) {
        if (!local_action_ready()) {
            return 1u;
        }
        confirm_action = CONFIRM_RESIGN_SEND;
        edit_stop_clear();
        notify_error_msg(SPECTRUM_GUI_MSG_RESIGN_CONFIRM);
        return 1u;
    }

    if (spectrum_streq(local_input, "/takeback")) {
        if (!local_action_ready()) {
            input_stop_and_show_cursor();
            return 1u;
        }
        if (pending_local_ply != 0u) {
            notify_wait_opponent_ack();
            input_stop_and_show_cursor();
            return 1u;
        }
        if (!takeback_snapshot_local || takeback_snapshot_ply != game_ply) {
            notify_error_msg(SPECTRUM_GUI_MSG_NO_TAKEBACK);
            input_stop_and_show_cursor();
            return 1u;
        }
        confirm_action = CONFIRM_TAKEBACK_SEND;
        edit_stop_clear();
        notify_error_msg(SPECTRUM_GUI_MSG_TAKEBACK_CONFIRM);
        return 1u;
    }

    if (!netchesszx_session_peer_ready_state) {
        notify_wait_opponent();
        return 1u;
    }
    if (pending_local_ply != 0u || restore_transfer_pending()) {
        notify_wait_opponent_ack();
        return 1u;
    }
    rc = send_local_chat(local_input);
    if (rc) {
        netchesszx_input_edit_history_add_overlay(local_input);
        input_stop_and_show_cursor();
        return LOCAL_KEY_CHAT_SENT;
    }
    return 0u;
}

static uint8_t process_local_key(uint8_t key) NETCHESSZX_FASTCALL
{
    if (key == 0u) {
        return 1u;
    }
    /* Full-width About hides pending notices, so dismiss it before handling
       a confirmation. FILE leaves the notice panel visible and stays behind
       the confirmation gate below. */
    if (spectrum_gui_about_visible() == 1u) {
        about_restore_game();
        return 1u;
    }
    /* Key-priority layers: CONFIRM > FILE > MENU > GAME. A pending Y/N
       prompt lives in the right-hand panel and can be answered with the
       browser still on screen; accepting closes the browser first
       (see the 'y' branch) so the action repaints a visible board,
       declining leaves the browser untouched. */
    if (confirm_action != CONFIRM_NONE) {
        if (key == 'y') {
            /* Light overlay dismissal: only the board area needs repair
               (the browser wipes interior and coords); banner, timers and
               side panels were untouched while the prompt was pending, so
               the full-screen restore (and its black flash) is overkill
               here. The accepted action repaints whatever it changes. */
            if (spectrum_gui_about_visible()) {
                about_restore_game();
            }
            if (confirm_action == CONFIRM_DISCONNECT) {
                confirm_action = CONFIRM_NONE;
                disconnect_to_setup();
                suppress_current_key();
            } else if (confirm_action == CONFIRM_RESET_SEND ||
                confirm_action == CONFIRM_RESTART_GAME) {
                if (!session_tcp_send(msg_reset_wire)) {
                    return 1u;
                }
                control_pending = CONTROL_PENDING_RESET;
                confirm_action = CONFIRM_NONE;
                notify_wait_opponent_ack();
                suppress_current_key();
            } else if (confirm_action == CONFIRM_RESET_ACCEPT) {
                if (!session_tcp_send(msg_ack_reset)) {
                    return 1u;
                } else {
                    accepted_reset_start();
                }
                suppress_current_key();
            } else if (confirm_action == CONFIRM_RESIGN_SEND) {
                if (!session_tcp_send(msg_resign_wire)) {
                    return 1u;
                }
                confirm_action = CONFIRM_NONE;
                end_game_over(msg_resign_wire);
                resign_pending = 1u;
                last_control_accept = CONTROL_ACCEPT_RESIGN;
                notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_RESIGN_ACK);
                suppress_current_key();
            } else if (confirm_action == CONFIRM_TAKEBACK_SEND) {
                if (!tcp_required(send_takeback_wire(takeback_snapshot_ply))) {
                    return 1u;
                }
                takeback_pending_ply = takeback_snapshot_ply;
                confirm_action = CONFIRM_NONE;
                notify_wait_msg(SPECTRUM_GUI_MSG_TAKEBACK_SENT);
                suppress_current_key();
            } else if (confirm_action == CONFIRM_RESTORE_ACCEPT) {
                if (!session_tcp_send(msg_restore_ry)) {
                    return 1u;
                }
                restore_rx_mask = RESTORE_RX_RECEIVE;
                confirm_action = CONFIRM_NONE;
                notify_info_msg(SPECTRUM_GUI_MSG_LOAD_ACCEPTED);
                suppress_current_key();
            } else if (confirm_action == CONFIRM_TAKEBACK_ACCEPT) {
                uint16_t accepted_ply = takeback_pending_ply;

                apply_takeback_snapshot();
                if (!session_tcp_move_reply(1u, accepted_ply)) {
                    return 1u;
                }
                last_accepted_takeback_ply = accepted_ply;
                confirm_action = CONFIRM_NONE;
                notify_info_msg(SPECTRUM_GUI_MSG_TAKEBACK_DONE);
                suppress_current_key();
            }
        } else if (key == 'n') {
            uint8_t was_restart = (uint8_t)(confirm_action == CONFIRM_RESTART_GAME);

            if (confirm_action == CONFIRM_RESET_ACCEPT) {
                if (!session_tcp_send(msg_nack_reset)) {
                    return 1u;
                } else {
                    if (!game_over) {
                        status_refresh_game();
                    }
                }
            } else if (confirm_action == CONFIRM_RESTART_GAME) {
                disconnect_to_setup();
            } else if (confirm_action == CONFIRM_RESTORE_ACCEPT) {
                if (!spectrum_link_send_text(msg_restore_rn)) {
                    if (netchesszx_transport_is_mqtt()) {
                        (void)spectrum_link_mqtt_publish_offline(
                            SPECTRUM_LINK_ROUTE_PRESENCE);
                    }
                    return 0u;
                }
                restore_rx_mask = 0u;
                notify_error_msg(SPECTRUM_GUI_MSG_LOAD_DECLINED);
                was_restart = 1u;
            } else if (confirm_action == CONFIRM_TAKEBACK_ACCEPT) {
                if (!session_tcp_move_reply(0u, takeback_pending_ply)) {
                    return 1u;
                }
                takeback_pending_ply = 0u;
            }
            confirm_action = CONFIRM_NONE;
            if (!was_restart) {
                notify_info("");
                status_refresh_game();
            }
            suppress_current_key();
        }
        return 1u;
    }

    if (!netchesszx_session_peer_ready_state) {
        if (key == KEY_CANCEL) {
            disconnect_to_setup();
            suppress_current_key();
            return 1u;
        }
        notify_wait_opponent();
        return 1u;
    }

    if (restore_transfer_pending()) {
        if (key == KEY_CANCEL) {
            return restore_cancel_pending_request();
        }
        notify_wait_msg(SPECTRUM_GUI_MSG_LOAD_WAITING_APPROVAL);
        suppress_current_key();
        return 1u;
    }

    if (spectrum_gui_fileui_visible()) {
        return fileui_process_key(key);
    }
    if (spectrum_gui_about_visible()) {
        if (key != 0u) {
            about_restore_game();
        }
        return 1u;
    }

    if (local_input_mode && key == SPECTRUM_GUI_KEY_MENU) {
        input_stop_and_show_cursor();
    }
    key = spectrum_gui_handle_menu_key(key);
    if (key == 0u) {
        return 1u;
    }

    if (local_input_mode) {
        if (key == KEY_CANCEL) {
            input_stop_and_show_cursor();
            return 1u;
        }
        if (key == 13u) {
            return input_submit();
        }
        netchesszx_input_edit_key_overlay(key);
        return 1u;
    }

    if (key == 13u) {
        if (pending_local_ply != 0u) {
            notify_wait_opponent_ack();
            return 1u;
        }
        cursor_hide();
        netchesszx_input_edit_begin_empty_overlay();
        notify_info_msg(SPECTRUM_GUI_MSG_TYPE_MOVE_CHAT);
        return 1u;
    }

    if (game_over) {
        spectrum_gui_hide_menu();
        if (resign_pending) {
            notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_RESIGN_ACK);
        } else if (last_control_accept == CONTROL_ACCEPT_RESIGN) {
            notify_wait_msg(SPECTRUM_GUI_MSG_RESTARTING_GAME);
        } else if (control_pending) {
            notify_wait_opponent_ack();
        } else {
            confirm_action = CONFIRM_RESTART_GAME;
            notify_error_msg(SPECTRUM_GUI_MSG_RESTART_GAME_CONFIRM);
        }
        suppress_current_key();
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_ABOUT) {
        about_open();
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_FILE) {
        fileui_open();
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_DISCC) {
        spectrum_gui_hide_menu();
        confirm_action = CONFIRM_DISCONNECT;
        notify_error_msg(SPECTRUM_GUI_MSG_DISCONNECT_CONFIRM);
        suppress_current_key();
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_REST) {
        if (!game_status_active) {
            notify_error_msg(SPECTRUM_GUI_MSG_GAME_NOT_STARTED);
        } else if (netchesszx_transport_is_mqtt() &&
            !netchesszx_session_peer_ready_state) {
            notify_error_msg(SPECTRUM_GUI_MSG_NO_OPPONENT);
        } else if (control_pending || pending_local_ply != 0u ||
                   takeback_pending_ply != 0u) {
            notify_error_msg(SPECTRUM_GUI_MSG_RESET_WAIT);
        } else {
            spectrum_gui_hide_menu();
            confirm_action = CONFIRM_RESET_SEND;
            notify_error_msg(SPECTRUM_GUI_MSG_RESET_CONFIRM);
            suppress_current_key();
        }
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_FLIP) {
        spectrum_gui_redraw_square(cursor_row, cursor_col);
        spectrum_gui_toggle_board_view();
        if (!local_input_mode) {
            movement_hints_show();
            cursor_show();
        }
        return 1u;
    }

    if (key == SPECTRUM_GUI_KEY_MENU_THEME) {
        spectrum_gui_clear_cursor_coords();
        netchesszx_board_theme_apply((uint8_t)(netchesszx_board_theme_index + 1u));
#ifdef NETCHESSZX_NEXT_BANKING
        spectrum_gui_redraw_board_squares();
#endif
        if (!local_input_mode) {
            cursor_show();
        }
        return 1u;
    }

    key = nav_key_alias(key);

    if (key == 32u) {
        return cursor_select_or_move(key);
    }
    if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT ||
        key == KEY_RIGHT) {
        cursor_move(key);
    }
    return 1u;
}

static void handle_opponent_disconnected_with(const char *message)
{
    confirm_action = CONFIRM_NONE;
    cursor_hide();
    clear_disconnected_session_state();
    edit_stop_clear();
    spectrum_gui_hide_board_pieces();
    reset_board_moves_chat();
    restore_about_full_board_if_visible();
    spectrum_gui_set_connected(0u);
    spectrum_gui_set_status_error(message);
    notify_error(message);
}

static void handle_opponent_disconnected(void)
{
    handle_opponent_disconnected_with(msg_connection_lost);
}

static void mqtt_peer_reset_wait_state(void)
{
    status_phase_current &= SPECTRUM_STATUS_PHASE_MASK;
    confirm_action = CONFIRM_NONE;
    spectrum_gui_hide_menu();
    if (local_turn) {
        cursor_hide();
    }
    netchesszx_session_peer_reset();
    start_pending = 0u;
    control_pending = 0u;
    restore_rx_mask = 0u;
    pending_takeback_clear();
    game_over = 0u;
    game_status_active = 0u;
    game_ply = 0u;
    pending_local_clear();
    spectrum_gui_game_timer_stop();
    spectrum_gui_hide_board_pieces();
    reset_board_moves_chat();
    local_controls_reset(0u);
    restore_about_full_board_if_visible();
    spectrum_gui_set_connected(1u);
    status_show_endpoint();
}

static void mqtt_peer_disconnected_wait(void)
{
    mqtt_peer_reset_wait_state();
    notify_error(msg_connection_lost);
    wait_after_notice();
    notify_wait_opponent();
}

static void session_send_mach(void)
{
#ifndef NETCHESSZX_HOST_SESSION_TEST
    /* Advisory identity never changes session control flow. */
    (void)spectrum_link_send_text(LOCAL_MACH_TEXT);
#endif
}

static uint8_t session_has_live_peer(void)
{
    return (uint8_t)(game_status_active || start_pending ||
                     netchesszx_session_peer_ready_state);
}

static uint8_t session_show_ready(uint8_t wait_msg)
{
    session_send_mach();
    spectrum_gui_set_connected(2u);
    status_show_endpoint();
    notify_wait_msg(wait_msg);
    cursor_show();
    return SESSION_DISPATCH_HANDLED;
}

static uint8_t session_presence_reannounce(uint8_t *mqtt_setup_wait,
                                           uint8_t *direct_hello_wait)
{
    uint8_t mqtt_is_host = netchesszx_session_is_host();

    if (netchesszx_transport_is_mqtt() &&
        !game_status_active &&
        !game_over &&
        mqtt_is_host != netchesszx_session_peer_ready_state) {
        if (*mqtt_setup_wait != 0u) {
            --*mqtt_setup_wait;
        }
        if (*mqtt_setup_wait == 0u) {
            if (!spectrum_link_mqtt_publish_setup(
                    mqtt_is_host ? SPECTRUM_LINK_MQTT_SETUP_RETAINED : 0u)) {
                handle_opponent_disconnected();
                return 0u;
            }
            *mqtt_setup_wait = MQTT_SETUP_REANNOUNCE_TICKS;
        }
    } else {
        *mqtt_setup_wait = (uint8_t)(!mqtt_is_host &&
                                     !game_status_active &&
                                     !game_over)
            ? MQTT_SETUP_REANNOUNCE_TICKS : 0u;
    }

    if (!netchesszx_transport_is_mqtt() &&
        !game_status_active &&
        !netchesszx_session_peer_ready_state) {
        if (*direct_hello_wait != 0u) {
            --*direct_hello_wait;
        }
        if (*direct_hello_wait == 0u) {
            if (!netchesszx_session_direct_send_hello()) {
                handle_opponent_disconnected();
                return 0u;
            }
            *direct_hello_wait = DIRECT_HELLO_REANNOUNCE_POLLS;
        }
    } else {
        *direct_hello_wait = 0u;
    }
    return 1u;
}

static uint8_t session_presence_handle_event(netchesszx_session_event_t event,
                                             char *payload,
                                             uint8_t retained)
{
    uint8_t host_flags;
    uint8_t bad_color;

    if (NETCHESSZX_SESSION_EVENT_IS_MACH(event)) {
        uint8_t platform = NETCHESSZX_SESSION_EVENT_MACH_PLATFORM(event);
        uint8_t platform_bits = SPECTRUM_STATUS_PLATFORM_BITS(platform);

        if ((status_phase_current & SPECTRUM_STATUS_PLATFORM_MASK) !=
            platform_bits) {
            status_phase_current =
                (uint8_t)((status_phase_current & SPECTRUM_STATUS_PHASE_MASK) |
                          platform_bits);
            spectrum_gui_status_phase(status_phase_current);
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_HOST_BUSY) {
        notify_error_msg(SPECTRUM_GUI_MSG_HOST_BUSY);
        goto dispatch_exit;
    }

    if (event == NETCHESSZX_SESSION_EVENT_DIRECT_HELLO) {
        uint8_t was_ready;

        if (!netchesszx_session_direct_apply_hello(payload)) {
            notify_error_msg(SPECTRUM_GUI_MSG_BAD_DIRECT_HELLO);
            (void)spectrum_link_send_text(msg_bye);
            handle_opponent_disconnected();
            goto dispatch_exit;
        }
        spectrum_link_direct_peer_mark_valid();
        was_ready = netchesszx_session_peer_ready_state;
        netchesszx_session_peer_mark_ready();
        /* Answer the first valid HELLO after link-up. This recovers either
           side's initial lost HELLO before both peers stop re-announcing.
           Once ready, consume duplicates without creating an echo loop. */
        if (was_ready) {
            goto dispatch_handled;
        }
        if (!tcp_required(netchesszx_session_direct_send_hello())) {
            goto dispatch_exit;
        }
        session_send_mach();
        status_show_endpoint();
        if (!netchesszx_session_is_host()) {
            spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
            notify_wait_msg(SPECTRUM_GUI_MSG_OPPONENT_READY_WAIT);
        } else {
            notify_wait_msg(SPECTRUM_GUI_MSG_OPPONENT_READY_GO);
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_MQTT_EMPTY) {
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_PEER_OFFLINE) {
        if (session_has_live_peer()) {
            mqtt_peer_disconnected_wait();
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_LOCAL_OFFLINE) {
        if (netchesszx_mqtt_session_id != 0u &&
            (game_status_active || netchesszx_session_peer_ready_state)) {
            (void)spectrum_link_mqtt_publish_presence();
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_FOREIGN_HOST) {
        if (session_has_live_peer()) {
            goto dispatch_handled;
        }
        if (local_turn) {
            cursor_hide();
        }
        netchesszx_session_peer_reset();
        start_pending = 0u;
        spectrum_gui_set_connected(1u);
        status_show_endpoint();
        notify_error_msg(SPECTRUM_GUI_MSG_ROOM_CONFLICT);
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_HOST) {
        host_flags = netchesszx_session_mqtt_host_flags(payload,
                                                        (uint8_t)(game_status_active |
                                                                  game_over),
                                                        retained,
                                                        &bad_color);
        if (bad_color) {
            notify_error_msg(SPECTRUM_GUI_MSG_BAD_COLOR);
            goto dispatch_handled;
        }
        if (host_flags & NETCHESSZX_SESSION_MQTT_HOST_SESSION_CHANGED) {
            uint8_t restore_pending = (uint8_t)(
                restore_transfer_pending() ||
                confirm_action == CONFIRM_RESTORE_ACCEPT);

            restore_rx_mask = 0u;
            if (confirm_action == CONFIRM_RESTORE_ACCEPT) {
                confirm_action = CONFIRM_NONE;
            }
            if (restore_pending) {
                notify_info_msg(SPECTRUM_GUI_MSG_LOAD_CANCELLED);
            }
        }
        if ((host_flags & NETCHESSZX_SESSION_MQTT_HOST_ACTIVATE_SIDE) &&
            !spectrum_link_mqtt_activate_side()) {
            handle_opponent_disconnected();
            goto dispatch_exit;
        }
        if (host_flags & NETCHESSZX_SESSION_MQTT_HOST_RETAINED_WAIT) {
            /* Retained host announcement: the host won't re-announce live
               mid-game, so learn which seat we'd take and probe it (subscribe
               only, no claim). An occupied seat replies with a retained O ->
               MQTT_SEAT_TAKEN -> BUSY, instead of hanging on "waiting". */
            if (!mqtt_seat_probed && !game_status_active &&
                !netchesszx_host_color_ready &&
                !netchesszx_session_is_host()) {
                uint8_t host_color;
                uint16_t probe_session;

                if (netchesszx_session_mqtt_parse_host_payload(payload,
                                                               &host_color,
                                                               &probe_session)) {
                    netchesszx_local_color = (uint8_t)(host_color ^ 1u);
                    /* Until the live H arrives, this retained H defines the
                       exact session whose seat snapshot we are probing. */
                    netchesszx_mqtt_session_id = probe_session;
                    if (!spectrum_link_mqtt_probe_seat()) {
                        handle_opponent_disconnected();
                        goto dispatch_exit;
                    }
                    mqtt_seat_probed = 1u;
                }
            }
            spectrum_gui_set_connected(1u);
            status_show_endpoint();
            notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_OPPONENT);
            goto dispatch_handled;
        }
        if ((host_flags & NETCHESSZX_SESSION_MQTT_HOST_PUBLISH_SETUP) &&
             !spectrum_link_mqtt_publish_setup(0u)) {
            handle_opponent_disconnected();
            goto dispatch_exit;
        }
        if (host_flags & NETCHESSZX_SESSION_MQTT_HOST_READY_WAIT) {
            return session_show_ready(SPECTRUM_GUI_MSG_OPPONENT_READY_WAIT);
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_SEAT_TAKEN) {
        /* Guest joining a room whose seat is already held (retained O for
           our side): report BUSY and leave; announcing would steal the
           legitimate guest's presence slot. */
        if (game_status_active || netchesszx_session_peer_ready_state) {
            goto dispatch_handled;
        }
        handle_opponent_disconnected_with(msg_host_busy);
        wait_after_notice();
        setup_restart_requested = 1u;
        goto dispatch_exit;
    }
    if (event == NETCHESSZX_SESSION_EVENT_MQTT_PEER_READY) {
        if (game_status_active) {
            /* Don't answer with BYE: the shared out-topic also reaches the
               legitimate guest and would kill the live game. The stray
               joiner self-detects via the retained presence slot. */
            notify_error_msg(SPECTRUM_GUI_MSG_GAME_ALREADY_ACTIVE);
            goto dispatch_handled;
        }
        if (netchesszx_session_peer_ready_state) {
            if (!spectrum_link_mqtt_publish_setup(0u)) {
                handle_opponent_disconnected();
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }
        netchesszx_session_peer_mark_ready();
        if (!spectrum_link_mqtt_publish_setup(0u)) {
            handle_opponent_disconnected();
            goto dispatch_exit;
        }
        return session_show_ready(SPECTRUM_GUI_MSG_OPPONENT_READY_GO);
    }
    if (event == NETCHESSZX_SESSION_EVENT_BYE) {
        if (netchesszx_transport_is_mqtt()) {
            if (session_has_live_peer()) {
                if (payload[0] != '\0' &&
                    netchesszx_session_is_host() &&
                    !spectrum_link_mqtt_publish_offline(
                        SPECTRUM_LINK_ROUTE_PRESENCE_PEER)) {
                    (void)spectrum_link_mqtt_publish_offline(
                        SPECTRUM_LINK_ROUTE_PRESENCE);
                    handle_opponent_disconnected();
                    goto dispatch_exit;
                }
                mqtt_peer_disconnected_wait();
            }
            goto dispatch_handled;
        }
        handle_opponent_disconnected_with(msg_opponent_disconnected);
        goto dispatch_exit;
    }
    return SESSION_DISPATCH_UNHANDLED;
dispatch_handled:
    return SESSION_DISPATCH_HANDLED;
dispatch_exit:
    return SESSION_DISPATCH_EXIT;
}

static uint8_t restore_dispatch_reply(const char *text) __z88dk_fastcall
{
    return session_tcp_send(text)
        ? SESSION_DISPATCH_HANDLED : SESSION_DISPATCH_EXIT;
}

static uint8_t restore_handle_event(netchesszx_session_event_t event,
                                    char *payload)
{
    if (event == NETCHESSZX_SESSION_EVENT_RESTORE_RQ) {
        if (!netchesszx_session_peer_ready_state) {
            goto dispatch_handled;
        }
        if ((restore_rx_mask & RESTORE_RX_APPLIED) != 0u) {
            restore_rx_mask = 0u;
        }
        if (confirm_action == CONFIRM_RESTORE_ACCEPT) {
            goto dispatch_handled;
        }
        if ((restore_rx_mask & RESTORE_RX_RECEIVE) != 0u) {
            return restore_dispatch_reply(msg_restore_ry);
        }
        if (restore_transfer_pending() ||
            control_pending || pending_local_ply != 0u ||
            takeback_pending_ply != 0u || confirm_action != CONFIRM_NONE ||
            restore_rx_mask != 0u) {
            return restore_dispatch_reply(msg_restore_rn);
        }
        spectrum_gui_hide_menu();
        confirm_action = CONFIRM_RESTORE_ACCEPT;
        notify_error_msg(SPECTRUM_GUI_MSG_RESTORE_REQUEST);
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_RESTORE_RY) {
        if ((restore_rx_mask & RESTORE_TX_PENDING) == 0u) {
            goto dispatch_handled;
        }
        if (restore_send_chunks()) {
            goto dispatch_retry;
        }
        goto dispatch_exit;
    }
    if (event == NETCHESSZX_SESSION_EVENT_RESTORE_RN) {
        if (restore_transfer_pending() ||
            confirm_action == CONFIRM_RESTORE_ACCEPT ||
            (restore_rx_mask & RESTORE_RX_RECEIVE) != 0u) {
            confirm_action = CONFIRM_NONE;
            restore_rx_mask = 0u;
            notify_error_msg(SPECTRUM_GUI_MSG_LOAD_DECLINED);
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_RESTORE_RA) {
        if ((restore_rx_mask & RESTORE_TX_AWAIT_ACK) != 0u) {
            netchesszx_save_meta_t meta;

            if (spectrum_restore_decode(restore_b64_pending,
                                         &restore_snapshot, &meta) &&
                restore_host_color_ok(&meta) &&
                saveload_apply_snapshot(&restore_snapshot, &meta)) {
                notify_info_msg(SPECTRUM_GUI_MSG_LOAD_OK);
            } else {
                notify_error_msg(SPECTRUM_GUI_MSG_LOAD_FAIL);
            }
            restore_rx_mask = 0u;
        }
        goto dispatch_handled;
    }
    if (event == NETCHESSZX_SESSION_EVENT_RESTORE_RS) {
        uint8_t result;

        result = netchesszx_asm_restore_chunk_step(
            &restore_rx_mask, restore_b64_pending, payload);
        if (result == RESTORE_CHUNK_REJECT) {
            return restore_dispatch_reply(msg_restore_rn);
        }
        if (result == RESTORE_CHUNK_REACK) {
            return restore_dispatch_reply(msg_restore_ra);
        }
        if (result == RESTORE_CHUNK_COMPLETE) {
            netchesszx_save_meta_t meta;

            if (!spectrum_restore_decode(restore_b64_pending,
                                         &restore_snapshot, &meta) ||
                !restore_host_color_ok(&meta) ||
                !saveload_apply_snapshot(&restore_snapshot, &meta)) {
                uint8_t rc = restore_dispatch_reply(msg_restore_rn);

                restore_rx_mask = 0u;
                if (rc != SESSION_DISPATCH_EXIT) {
                    notify_error_msg(SPECTRUM_GUI_MSG_LOAD_FAIL);
                }
                return rc;
            }
            restore_rx_mask = spectrum_restore_build_b64(
                &restore_snapshot, &meta, restore_b64_pending)
                ? RESTORE_RX_APPLIED : 0u;
            if (restore_dispatch_reply(msg_restore_ra) ==
                SESSION_DISPATCH_EXIT) {
                goto dispatch_exit;
            }
            notify_info_msg(SPECTRUM_GUI_MSG_LOAD_OK);
        }
        goto dispatch_handled;
    }
    return SESSION_DISPATCH_UNHANDLED;
dispatch_handled:
    return SESSION_DISPATCH_HANDLED;
dispatch_exit:
    return SESSION_DISPATCH_EXIT;
dispatch_retry:
    return SESSION_DISPATCH_RETRY_RESET;
}

static uint8_t session_control_handle_event(netchesszx_session_event_t event,
                                           char *payload)
{
    if (event >= NETCHESSZX_SESSION_EVENT_RESTORE_RQ &&
        event <= NETCHESSZX_SESSION_EVENT_RESTORE_RA) {
        return restore_handle_event(event, payload);
    }
    if (event == NETCHESSZX_SESSION_EVENT_ACK_GAME_START) {
        if (start_pending) {
            reset_auto_start();
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_ACK_RESET) {
        if (CONTROL_IS_RESET(control_pending)) {
            control_pending = 0u;
            reset_auto_start();
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_ACK_MOVE) {
        char ply[6];
        char notation[8];

        if (netchess_proto_parse_ack(payload,
                                     ply,
                                     sizeof(ply),
                                     notation,
                                     sizeof(notation))) {
            uint16_t ack_ply = parse_u16(ply);

            if (takeback_pending_ply != 0u &&
                ack_ply == takeback_pending_ply &&
                takeback_snapshot_local) {
                apply_takeback_snapshot();
                notify_info_msg(SPECTRUM_GUI_MSG_TAKEBACK_DONE);
                goto dispatch_live;
            } else if (pending_local_ply != 0u &&
                       ack_ply == pending_local_ply) {
                apply_pending_local_move(notation);
                goto dispatch_live;
            }
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_NACK_GAME_START) {
        if (start_pending) {
            start_pending = 0u;
            notify_error_msg(SPECTRUM_GUI_MSG_START_REJECTED);
            status_show_endpoint();
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_NACK_RESET) {
        if (CONTROL_IS_RESET(control_pending)) {
            uint8_t cancelled = (uint8_t)(
                control_pending == CONTROL_PENDING_RESET_CANCEL);
            uint8_t resign_restart = (uint8_t)(
                last_control_accept == CONTROL_ACCEPT_RESIGN);

            control_pending = 0u;
            if (resign_restart) {
                last_control_accept = CONTROL_ACCEPT_NONE;
                notify_error_msg(SPECTRUM_GUI_MSG_RESTART_FAILED_GAME_OVER);
            } else if (cancelled) {
                notify_reset_timeout(1u);
            } else {
                notify_error_msg(SPECTRUM_GUI_MSG_RESET_REJECTED);
            }
            if (!game_over) {
                status_refresh_game();
            }
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_NACK_MOVE) {
        char ply[6];
        char notation[8];

        if (netchess_proto_parse_nack(payload,
                                      ply,
                                      sizeof(ply),
                                      notation,
                                      0u)) {
            uint16_t nack_ply = parse_u16(ply);

            if (takeback_pending_ply != 0u &&
                nack_ply == takeback_pending_ply && takeback_snapshot_local) {
                takeback_pending_ply = 0u;
                notify_error_msg(SPECTRUM_GUI_MSG_TAKEBACK_REJECTED);
                status_refresh_game();
                goto dispatch_live;
            } else if (pending_local_ply != 0u &&
                       nack_ply == pending_local_ply) {
                pending_local_clear();
                notify_move_rejected();
                cursor_show();
                HOST_SESSION_OBSERVE_MOVE_REJECTION(payload);
                goto dispatch_live;
            }
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_CANCEL_RESET) {
        uint8_t matched = (uint8_t)(confirm_action == CONFIRM_RESET_ACCEPT &&
                                    control_pending == 0u);

        if (matched) {
            confirm_action = CONFIRM_NONE;
            control_pending = 0u;
        }
        if (!session_tcp_send(msg_nack_reset)) {
            goto dispatch_exit;
        }
        if (matched) {
            notify_reset_timeout(0u);
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_RESET) {
        if (reset_ack_replay_polls != 0u) {
            if (!session_tcp_send(msg_ack_reset)) {
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }
        if (confirm_action == CONFIRM_RESET_ACCEPT && control_pending == 0u) {
            goto dispatch_handled;
        }
        if (game_over) {
            if (last_control_accept == CONTROL_ACCEPT_RESIGN) {
                if (!spectrum_link_send_text(msg_ack_reset)) {
                    notify_wait_msg(SPECTRUM_GUI_MSG_RESTARTING_GAME);
                    goto dispatch_handled;
                }
                accepted_reset_start();
            } else if (control_pending) {
                if (!session_tcp_send(msg_ack_reset)) {
                    goto dispatch_exit;
                }
                accepted_reset_start();
            } else if (confirm_action != CONFIRM_NONE) {
                if (!session_tcp_send(msg_nack_reset)) {
                    goto dispatch_exit;
                }
            } else {
                spectrum_gui_hide_menu();
                confirm_action = CONFIRM_RESET_ACCEPT;
                notify_error_msg(SPECTRUM_GUI_MSG_RESTART_REQUEST);
            }
        } else if (game_status_active &&
                  (CONTROL_IS_RESET(control_pending) ||
                    pending_local_ply != 0u || takeback_pending_ply != 0u)) {
            if (!session_tcp_send(msg_nack_reset_busy)) {
                goto dispatch_exit;
            }
        } else if (!game_status_active || control_pending ||
            confirm_action != CONFIRM_NONE) {
            if (!session_tcp_send(msg_nack_reset)) {
                goto dispatch_exit;
            }
        } else {
            spectrum_gui_hide_menu();
            confirm_action = CONFIRM_RESET_ACCEPT;
            notify_error_msg(SPECTRUM_GUI_MSG_RESET_REQUEST);
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_RESIGN) {
        uint8_t crossed = resign_pending;

        /* ACK unconditionally: a retransmitted RESIGN (our ACK was lost)
           must be re-ACKed or the peer retries forever. */
        if (!session_tcp_send(msg_ack_resign)) {
            goto dispatch_exit;
        }
        resign_pending = 0u;
        if (game_status_active) {
            end_game_over(msg_opponent_resign);
        }
        last_control_accept = CONTROL_ACCEPT_RESIGN;
        notify_wait_msg(SPECTRUM_GUI_MSG_RESTARTING_GAME);
        if (crossed && netchesszx_session_is_host()) {
            control_pending = CONTROL_PENDING_RESET;
            (void)spectrum_link_send_text(msg_reset_wire);
            goto dispatch_retry;
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_ACK_RESIGN) {
        if (resign_pending) {
            resign_pending = 0u;
            last_control_accept = CONTROL_ACCEPT_RESIGN;
            control_pending = CONTROL_PENDING_RESET;
            notify_wait_msg(SPECTRUM_GUI_MSG_RESTARTING_GAME);
            (void)spectrum_link_send_text(msg_reset_wire);
            goto dispatch_retry;
        }
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_TAKEBACK) {
        const char *requested_text = netchess_after_prefix(payload, msg_takeback_wire);
        uint16_t requested_ply = requested_text == 0 ? 0u : parse_u16(requested_text);

        if (requested_ply != 0u && confirm_action == CONFIRM_TAKEBACK_ACCEPT &&
            takeback_pending_ply == requested_ply) {
            goto dispatch_handled;
        }

        /* Retransmit of an already-applied takeback (peer missed our ACK
           on the split ack-topic). Re-ACK idempotently instead of NACKing.
           Tracked explicitly: cleared on the next applied move and on
           reset/restore/game-over. */
        if (requested_ply != 0u &&
            requested_ply == last_accepted_takeback_ply) {
            if (!session_tcp_move_reply(1u, requested_ply)) {
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }

        if (requested_ply == 0u || !game_status_active || game_over ||
            control_pending || pending_local_ply != 0u || takeback_pending_ply != 0u ||
            confirm_action != CONFIRM_NONE || takeback_snapshot_local ||
            requested_ply != game_ply ||
            requested_ply != takeback_snapshot_ply) {
            if (!session_tcp_move_reply(0u, requested_ply)) {
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }
        spectrum_gui_hide_menu();
        takeback_pending_ply = requested_ply;
        confirm_action = CONFIRM_TAKEBACK_ACCEPT;
        notify_error_msg(SPECTRUM_GUI_MSG_TAKEBACK_REQUEST);
        goto dispatch_handled;
    }

    if (event == NETCHESSZX_SESSION_EVENT_GAME_START) {
        if (netchesszx_session_is_host()) {
            goto dispatch_handled;
        }
        /* Direct GAME START carries WHITE= and is enough to bind the peer
           if the host HELLO was lost; dropping it leaves the PC waiting
           forever for ACK GAME START. MQTT still needs a ready seat. */
        if (netchesszx_transport_is_mqtt() &&
            !netchesszx_session_mqtt_can_accept_game_start()) {
            notify_wait_opponent();
            goto dispatch_handled;
        }
        if (start_pending || control_pending != 0u ||
            pending_local_ply != 0u || takeback_pending_ply != 0u ||
            confirm_action != CONFIRM_NONE || resign_pending ||
            restore_transfer_pending()) {
            if (!session_tcp_send("NACK GAME START BUSY")) {
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }
        if (!netchesszx_session_direct_apply_start_side(payload)) {
            notify_error_msg(SPECTRUM_GUI_MSG_BAD_START);
            if (!session_tcp_send("NACK GAME START BAD")) {
                goto dispatch_exit;
            }
            goto dispatch_handled;
        }
        {
            uint8_t was_ready = netchesszx_session_peer_ready_state;

            netchesszx_session_peer_mark_ready();
            if (!was_ready) {
                session_send_mach();
            }
        }
        /* ACK before the blocking piece reveal so the host starts in parallel. */
        if (!session_tcp_send(msg_ack_game_start)) {
            goto dispatch_exit;
        }
        if (!game_status_active || game_over) {
            reset_auto_start();
        }
        goto dispatch_handled;
    }

    return SESSION_DISPATCH_UNHANDLED;
dispatch_handled:
    return SESSION_DISPATCH_HANDLED;
dispatch_exit:
    return SESSION_DISPATCH_EXIT;
dispatch_live:
    return SESSION_DISPATCH_HANDLED_LIVE;
dispatch_retry:
    return SESSION_DISPATCH_RETRY_RESET;
}

static void game_message_loop(void)
{
    char *payload = spectrum_link_payload_scratch();
    char ply[6];
    char move[SPECTRUM_BOARD_MOVE_TEXT_LEN + 1u];
    char notation[8];
    netchesszx_session_ping_t ping;
    uint8_t poll_status;
    uint8_t dispatch_status;
    uint8_t mqtt_setup_reannounce_wait = MQTT_SETUP_REANNOUNCE_TICKS;
    uint8_t direct_hello_reannounce_wait = 0u;
    uint8_t pending_retry_period = PENDING_RETRY_POLLS;
    uint8_t pending_retry_wait = pending_retry_period;
    uint8_t control_retry_count = 0u;
    uint16_t control_cancel_period = CONTROL_CANCEL_POLLS;
    uint16_t control_cancel_wait = control_cancel_period;
    netchesszx_session_event_t event;
    netchesszx_session_poll_result_t poll;

    reset_board_moves_chat();
    game_ply = 0u;
    clear_disconnected_session_state();
    spectrum_gui_set_board_view(
        (uint8_t)(netchesszx_host_color_ready &&
                  !netchesszx_local_is_white()));
    spectrum_gui_set_connected(netchesszx_transport_is_mqtt() ? 1u : 2u);
    status_show_endpoint();
    /* Peer is not confirmed at connect time: show "waiting" until the direct
       guest HELLO arrives (host) or the host HELLO arrives (guest). The
       "press SPACE" prompt is raised later, once peer ready is set. */
    notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_OPPONENT);
    local_controls_reset(0u);
    netchesszx_session_ping_reset(&ping);
    HOST_SESSION_EXPOSE_PING(&ping);
    if (!netchesszx_session_direct_send_hello()) {
        handle_opponent_disconnected();
        return;
    }

    while (1) {
        dispatch_status = process_local_key(spectrum_gui_poll_key());
        if (!dispatch_status) {
            handle_opponent_disconnected();
            return;
        }
        if (dispatch_status == LOCAL_KEY_CHAT_SENT) {
            netchesszx_session_ping_reset(&ping);
        }
        if (setup_restart_requested) {
            return;
        }
        poll_status = netchesszx_session_poll(&ping,
                                               payload,
                                               SPECTRUM_LINK_PAYLOAD_MAX,
                                               &poll);
        if (poll_status == NETCHESSZX_SESSION_POLL_DISCONNECTED) {
            handle_opponent_disconnected();
            return;
        }
#if !defined(NETCHESSZX_NEXT) && !defined(NETCHESSZX_HOST_TEST)
        if (poll_status == NETCHESSZX_SESSION_POLL_NONE) {
            spectrum_piece_reflection_dispatch_pending();
        }
#endif
        if (poll_status != NETCHESSZX_SESSION_POLL_EVENT) {
            if (reset_ack_replay_polls != 0u) {
                --reset_ack_replay_polls;
            }
            if (control_pending == CONTROL_PENDING_RESET_WAIT) {
                if (control_cancel_wait != 0u) {
                    --control_cancel_wait;
                }
                if (control_cancel_wait == 0u) {
                    control_pending = CONTROL_PENDING_RESET_CANCEL;
                    control_retry_count = 0u;
                    if (!tcp_required(retry_pending_outgoing())) {
                        return;
                    }
                    pending_retry_wait = pending_retry_period;
                }
            } else if (local_retry_pending()) {
                if (pending_retry_wait != 0u) {
                    --pending_retry_wait;
                }
                if (pending_retry_wait == 0u) {
                    if (control_retry_count >= CONTROL_REPLY_RETRIES) {
                        if (CONTROL_IS_RESET(control_pending)) {
                            control_pending = CONTROL_PENDING_RESET_WAIT;
                            control_retry_count = 0u;
                            control_cancel_wait = control_cancel_period;
                        } else if ((restore_rx_mask &
                                    RESTORE_TX_PENDING) != 0u) {
                            if (!session_tcp_send(msg_restore_rn)) {
                                return;
                            }
                            restore_rx_mask = 0u;
                            notify_info_msg(SPECTRUM_GUI_MSG_LOAD_CANCELLED);
                        } else {
                            if (netchesszx_transport_is_mqtt()) {
                                (void)spectrum_link_mqtt_publish_offline(
                                    SPECTRUM_LINK_ROUTE_PRESENCE);
                            }
                            handle_opponent_disconnected();
                            return;
                        }
                    } else {
                        if (!tcp_required(retry_pending_outgoing())) {
                            return;
                        }
                        ++control_retry_count;
                    }
                    pending_retry_wait = pending_retry_period;
                }
            } else {
                pending_retry_wait = pending_retry_period;
                control_retry_count = 0u;
                control_cancel_wait = control_cancel_period;
            }
            if (!session_presence_reannounce(&mqtt_setup_reannounce_wait,
                                             &direct_hello_reannounce_wait)) {
                return;
            }
            continue;
        }
        event = poll.event;
        if (event != NETCHESSZX_SESSION_EVENT_RESET) {
            reset_ack_replay_polls = 0u;
        }
        dispatch_status = session_presence_handle_event(event,
                                                        payload,
                                                        poll.retained);
        if (dispatch_status == SESSION_DISPATCH_EXIT) {
            return;
        }
        if (dispatch_status == SESSION_DISPATCH_HANDLED) {
            continue;
        }
        if (!netchesszx_transport_is_mqtt() &&
            !netchesszx_session_peer_ready_state) {
            continue;
        }
        dispatch_status = session_control_handle_event(event, payload);
        if (dispatch_status == SESSION_DISPATCH_EXIT) {
            return;
        }
        if (dispatch_status == SESSION_DISPATCH_RETRY_RESET) {
            control_retry_count = 0u;
            pending_retry_wait = pending_retry_period;
            continue;
        }
        if (dispatch_status == SESSION_DISPATCH_HANDLED_LIVE) {
            netchesszx_session_ping_rx_data(&ping);
            continue;
        }
        if (dispatch_status == SESSION_DISPATCH_HANDLED) {
            continue;
        }
        if (event == NETCHESSZX_SESSION_EVENT_MOVE &&
            netchess_proto_parse_move(payload,
                                      ply,
                                      sizeof(ply),
                                      move,
                                      sizeof(move),
                                      notation,
                                      sizeof(notation))) {
            uint16_t incoming_ply = parse_u16(ply);

            if (incoming_ply == 0u) {
                continue;
            }
            if (incoming_ply <= game_ply) {
                if (!tcp_required(netchesszx_session_send_ack_move(ply))) {
                    return;
                }
                continue;
            }
            if (control_pending || takeback_pending_ply != 0u ||
                (confirm_action != CONFIRM_NONE &&
                 confirm_action != CONFIRM_TAKEBACK_SEND) || game_over ||
                !game_status_active) {
                notify_wait_opponent_ack();
                if (!send_busy_move_reply(ply)) {
                    return;
                }
                continue;
            }
            if (pending_local_ply != 0u) {
                if (incoming_ply == pending_local_ply &&
                    spectrum_streq(move, pending_local_move)) {
                    notify_wait_opponent_ack();
                    if (!send_busy_move_reply(ply)) {
                        return;
                    }
                    continue;
                }
                if (incoming_ply == (uint16_t)(pending_local_ply + 1u)) {
                    apply_pending_local_move(0);
                }
                if (pending_local_ply != 0u) {
                    notify_wait_opponent_ack();
                    goto nack_move;
                }
            }
            if (local_turn) {
                notify_error_msg(SPECTRUM_GUI_MSG_OPPONENT_TURN);
                goto nack_move;
            }
            if (incoming_ply != (uint16_t)(game_ply + 1u)) {
                notify_move_rejected();
                memcpy(payload, "NACK", 4u);
                memcpy(payload + 6u + netchesszx_asm_mqtt_strlen8(ply),
                       "SYNC", 5u);
                if (!session_tcp_send(payload)) {
                    return;
                }
                continue;
            }
            if (!spectrum_board_is_legal_move(move)) {
                notify_move_rejected();
                goto nack_move;
            }
            {
                uint8_t author = netchesszx_remote_color();

                spectrum_gui_prepare_move(move);
                if (spectrum_board_apply_trusted_move_with_undo(
                        move, &takeback_undo)) {
                    /* An unsent local confirmation cannot reserve the peer's turn. */
                    if (confirm_action == CONFIRM_TAKEBACK_SEND) {
                        confirm_action = CONFIRM_NONE;
                    }
                    takeback_snapshot_save(incoming_ply, 0u);
                    spectrum_gui_set_board_snapshot(spectrum_board_cells());
                    game_ply = incoming_ply;
                    finish_applied_move(ply, move, author);
                    if (!tcp_required(netchesszx_session_send_ack_move(ply))) {
                        return;
                    }
                } else {
                    notify_move_rejected();
                    goto nack_move;
                }
            }
            continue;

nack_move:
            if (!tcp_required(netchesszx_session_send_nack_move(ply))) {
                return;
            }
            continue;
        } else if (event == NETCHESSZX_SESSION_EVENT_CHAT &&
                   netchess_proto_parse_chat(payload,
                                             payload,
                                             SPECTRUM_LINK_PAYLOAD_MAX)) {
            spectrum_gui_add_chat(netchesszx_remote_side_char(), payload);
        }
    }
}

#ifndef NETCHESSZX_HOST_SESSION_TEST
int main(void)
{
    uint8_t warm_restart;
    uint8_t config_state;

    if (!spectrum_assets_load()) {
        spectrum_assets_fatal();
    }
    config_state = netchesszx_config_load_overlay();
    /* Cold boot always probes and prefers a hardware RTC. The sync overlay
       replaces this with the saved UTC offset when no RTC is present. */
    netchesszx_timezone = NETCHESSZX_TIME_RTC;

connection_setup:
    confirm_action = CONFIRM_NONE;
    control_pending = 0u;
    spectrum_gui_set_board_view(0u);
    spectrum_gui_set_board_pieces_visible(0u);
    memset(netchesszx_hinted_rows, 0, sizeof(netchesszx_hinted_rows));
    spectrum_board_clear();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_reset_logs();
    spectrum_gui_set_connected(0u);
    notify_info("");
    warm_restart = setup_restart_requested;
    if (warm_restart) {
        setup_restart_requested = 0u;
        spectrum_gui_restore_board_area();
        status_show_connection_setup();
    } else {
        spectrum_gui_draw_board();
        status_show_connection_setup();
    }
    while (!connection_preflight_run(warm_restart)) {
        warm_restart = 0u;
        retry_after_error(preflight_retry_msg);
    }
#ifndef NETCHESSZX_SPECTRANEXT
    if (!spectrum_net_runtime_clock_ready()) {
        spectrum_link_clock_retry_start();
    }
#endif
    config_state = session_setup_run(config_state);
#ifndef NETCHESSZX_SPECTRANEXT
    spectrum_link_clock_retry_cancel();
#endif
    spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white());
    spectrum_board_reset();
    spectrum_gui_set_board_snapshot(spectrum_board_cells());
    spectrum_gui_restore_side_panels();
    spectrum_gui_set_connected(1u);
    status_show_endpoint();

    if (netchesszx_transport_is_mqtt()) {
        while (1) {
            mqtt_seat_probed = 0u;
            status_show_connecting();
            notify_wait_msg(SPECTRUM_GUI_MSG_CONNECTING);
            if (spectrum_link_mqtt_start()) {
                status_show_endpoint();
                notify_info_msg(SPECTRUM_GUI_MSG_CONNECTED);
                game_message_loop();
                if (setup_restart_requested) {
                    goto connection_setup;
                }
            } else {
                if (retry_after_error(msg_connect_failed)) {
                    suppress_key_until_release(KEY_CANCEL);
                    setup_restart_requested = 1u;
                    goto connection_setup;
                }
            }
        }
    } else if (netchesszx_session_is_host()) {
        while (1) {
            status_show_connecting();
            notify_wait_msg(SPECTRUM_GUI_MSG_CONNECTING);
            if (spectrum_link_listen()) {
                status_show_endpoint();
                notify_info_msg(SPECTRUM_GUI_MSG_CONNECTED);
                while (1) {
                    uint8_t wait_rc;

                    status_show_endpoint();
                    notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_OPPONENT);
                    wait_rc = spectrum_link_wait_pc_connect();
                    if (wait_rc == SPECTRUM_LINK_CANCELLED) {
                        suppress_key_until_release(KEY_CANCEL);
                        setup_restart_requested = 1u;
                        goto connection_setup;
                    }
                    if (wait_rc) {
                        game_message_loop();
                        if (setup_restart_requested) {
                            goto connection_setup;
                        }
                        wait_after_notice();
                        /* Session over (peer lost or left): restart the listen
                           from scratch so the ESP server drops stale links
                           before the next accept. */
                        break;
                    }
                }
            } else {
                if (retry_after_error(msg_connect_failed)) {
                    suppress_key_until_release(KEY_CANCEL);
                    setup_restart_requested = 1u;
                    goto connection_setup;
                }
            }
        }
    } else {
        while (1) {
            uint8_t connect_rc;

            status_show_connecting();
            notify_wait_msg(SPECTRUM_GUI_MSG_CONNECTING);
            connect_rc = spectrum_link_connect_host();
            if (connect_rc == SPECTRUM_LINK_CANCELLED) {
                suppress_key_until_release(KEY_CANCEL);
                setup_restart_requested = 1u;
                goto connection_setup;
            }
            if (connect_rc) {
                status_show_endpoint();
                notify_info_msg(SPECTRUM_GUI_MSG_CONNECTED);
                game_message_loop();
                if (setup_restart_requested) {
                    goto connection_setup;
                }
                wait_after_notice();
            } else {
                notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_HOST);
                if (wait_notice_frames(1u)) {
                    suppress_key_until_release(KEY_CANCEL);
                    setup_restart_requested = 1u;
                    goto connection_setup;
                }
            }
        }
    }
}
#endif
