#include "common/session/session.h"

#include <stdio.h>
#include <string.h>

typedef char session_u8_tag_check[(sizeof(((SessionState *)0)->phase) == 1u)
                                      ? 1
                                      : -1];
typedef char session_action_capacity_check[(SESSION_ACTION_CAPACITY == 5u)
                                               ? 1
                                               : -1];
typedef char session_payload_bound_check[(SESSION_PAYLOAD_MAX <= 255u) ? 1 : -1];
typedef char session_restore_workspace_check[
    (sizeof(SessionWorkspace) == SESSION_RESTORE_BYTES +
                                SESSION_CHAT_TEXT_MAX + 1u + 6u) ? 1 : -1];

static int failures;

uint8_t direct_session_step(SessionState *state,
                            const SessionEvent *event,
                            SessionWorkspace *workspace,
                            uint8_t *tx_scratch,
                            uint8_t tx_capacity,
                            SessionAction *actions,
                            uint8_t action_capacity)
{
    (void)state;
    (void)event;
    (void)workspace;
    (void)tx_scratch;
    (void)tx_capacity;
    (void)actions;
    (void)action_capacity;
    return 0u;
}

static void check(int ok, const char *label)
{
    if (!ok) {
        printf("FAIL: %s\n", label);
        ++failures;
    }
}

static SessionConfig direct_host_config(void)
{
    SessionConfig config;

    config.transport = SESSION_TRANSPORT_DIRECT;
    config.role = SESSION_ROLE_HOST;
    config.host_color = SESSION_COLOR_BLACK;
    config.session_id = 17u;
    return config;
}

static void test_init_and_reset(void)
{
    SessionConfig config = direct_host_config();
    SessionState state;

    check(session_init(&state, &config), "valid init");
    check(state.phase == SESSION_PHASE_IDLE, "initial phase");
    check(state.local_color == SESSION_COLOR_BLACK, "host color");
    check(state.session_id == 17u, "initial session id");
    check(state.next_tx_id == 1u, "initial tx id");

    state.phase = SESSION_PHASE_ACTIVE;
    state.deferred_decision = SESSION_DECISION_ACCEPT;
    state.peer_ready = 1u;
    state.timer_mask = 7u;
    session_reset(&state);
    check(state.phase == SESSION_PHASE_IDLE, "reset phase");
    check(state.deferred_decision == 0u && state.peer_ready == 0u,
          "reset readiness");
    check(state.timer_mask == 0u, "reset timers");
    check(state.config.transport == SESSION_TRANSPORT_DIRECT,
          "reset keeps config");

    config.transport = 9u;
    check(!session_init(&state, &config), "reject transport");
    config = direct_host_config();
    config.role = 9u;
    check(!session_init(&state, &config), "reject role");
    check(!session_init(0, &config), "reject null state");
    check(!session_init(&state, 0), "reject null config");
}

static void test_link_down_order_and_capacity(void)
{
    SessionConfig config = direct_host_config();
    SessionState state;
    SessionState before;
    SessionEvent event;
    SessionAction actions[SESSION_ACTION_CAPACITY];
    SessionWorkspace workspace;
    uint8_t count;

    check(session_init(&state, &config), "link-down init");
    state.link_up = 1u;
    state.phase = SESSION_PHASE_ACTIVE;
    state.deferred_decision = SESSION_DECISION_ACCEPT;
    state.peer_ready = 1u;
    state.pending_tx_kind = SESSION_REQUEST_MOVE;
    state.timer_mask = (uint8_t)((1u << SESSION_TIMER_TX_GUARD) |
                                 (1u << SESSION_TIMER_LIVENESS));
    event.type = SESSION_EV_LINK_DOWN;
    event.data.link.link_id = 3u;
    state.active_link = 3u;

    before = state;
    count = session_step(&state, &event, &workspace, 0, 0u, actions, 2u);
    check(count == 0u, "small action buffer rejected");
    check(memcmp(&state, &before, sizeof(state)) == 0,
          "capacity failure is atomic");

    count = session_step(&state, &event, &workspace, 0, 0u,
                         actions, SESSION_ACTION_CAPACITY);
    check(count == 3u, "link-down action count");
    check(actions[0].type == SESSION_ACT_TIMER_CANCEL &&
              actions[0].data.timer_cancel.timer_id == SESSION_TIMER_TX_GUARD,
          "cancel tx guard first");
    check(actions[1].type == SESSION_ACT_TIMER_CANCEL &&
              actions[1].data.timer_cancel.timer_id == SESSION_TIMER_LIVENESS,
          "cancel liveness second");
    check(actions[2].type == SESSION_ACT_SESSION_CHANGED &&
              actions[2].data.session.status == SESSION_CHANGED_ENDED,
          "ended last");
    check(state.phase == SESSION_PHASE_IDLE && state.link_up == 0u,
          "link-down resets session");
    check(session_step(&state, &event, &workspace, 0, 0u,
                       actions, SESSION_ACTION_CAPACITY) == 0u,
          "repeated link-down is idempotent");
}

static void test_invalid_and_local_tx_semantics(void)
{
    SessionConfig config = direct_host_config();
    SessionState state;
    SessionState before;
    SessionEvent event;
    SessionAction actions[SESSION_ACTION_CAPACITY];
    SessionWorkspace workspace;
    uint8_t rx[] = {'P', 'I', 'N', 'G'};
    uint8_t scratch[SESSION_PAYLOAD_MAX];

    check(session_init(&state, &config), "invalid-event init");
    before = state;
    event.type = 0xFFu;
    check(session_step(&state, &event, &workspace, 0, 0u,
                       actions, SESSION_ACTION_CAPACITY) == 0u,
          "reject unknown event");
    check(memcmp(&state, &before, sizeof(state)) == 0,
          "unknown event leaves state");

    event.type = SESSION_EV_TX_RESULT;
    event.data.tx.tx_id = 7u;
    event.data.tx.result = SESSION_TX_OK;
    check(session_step(&state, &event, &workspace, 0, 0u,
                       actions, SESSION_ACTION_CAPACITY) == 0u,
          "unsolicited local tx result ignored");
    check(state.phase == SESSION_PHASE_IDLE,
          "local tx result does not prove peer state");

    event.type = SESSION_EV_RX;
    event.data.rx.payload = rx;
    event.data.rx.length = sizeof(rx);
    event.data.rx.route = SESSION_ROUTE_DEFAULT;
    event.data.rx.flags = SESSION_RX_LIVE;
    event.data.rx.link_id = 1u;
    check(session_step(&state, &event, &workspace, 0, 0u,
                       actions, SESSION_ACTION_CAPACITY) == 0u,
          "unimplemented rx is bounded");
    check(memcmp(rx, "PING", sizeof(rx)) == 0, "rx input remains caller-owned");
    config.transport = SESSION_TRANSPORT_MQTT;
    check(session_init(&state, &config), "removed local MQTT request init");
    state.link_up = state.peer_ready = 1u;
    state.active_link = 1u;
    state.phase = SESSION_PHASE_ACTIVE;
    before = state;
    memset(&workspace, 0, sizeof(workspace));
    memset(&event, 0, sizeof(event));
    event.type = SESSION_EV_LOCAL_REQUEST;
    event.data.local.request = 5u; /* Removed negotiated-DRAW API ID. */
    check(session_step(&state, &event, &workspace, scratch, sizeof(scratch),
                         actions, SESSION_ACTION_CAPACITY) == 0u &&
              memcmp(&state, &before, sizeof(state)) == 0,
          "removed local MQTT request is inert");
}

static void test_mqtt_guest_liveness_timeout_recovery(void)
{
    SessionConfig config;
    SessionState state;
    SessionEvent event;
    SessionAction actions[SESSION_ACTION_CAPACITY];
    SessionWorkspace workspace;
    uint8_t tx_scratch[64];
    uint8_t count;

    memset(&config, 0, sizeof(config));
    config.transport = SESSION_TRANSPORT_MQTT;
    config.role = SESSION_ROLE_GUEST;
    config.host_color = SESSION_COLOR_WHITE;
    config.session_id = 77u;

    check(session_init(&state, &config), "mqtt guest init");
    state.link_up = 1u;
    state.active_link = 1u;
    state.phase = SESSION_PHASE_ACTIVE;
    state.peer_ready = 1u;
    state.liveness_misses = 1u;
    state.timer_mask = (uint8_t)(1u << SESSION_TIMER_LIVENESS);

    memset(&event, 0, sizeof(event));
    event.type = SESSION_EV_TIMEOUT;
    event.data.timeout.timer_id = SESSION_TIMER_LIVENESS;

    count = session_step(&state, &event, &workspace, tx_scratch,
                         sizeof(tx_scratch), actions, SESSION_ACTION_CAPACITY);
    check(count > 0u, "liveness timeout step produces actions");
    check(actions[count - 1u].type == SESSION_ACT_SESSION_CHANGED &&
              actions[count - 1u].data.session.status == SESSION_CHANGED_ENDED &&
              actions[count - 1u].data.session.reason ==
                  SESSION_END_REASON_REMOTE_BYE,
          "liveness timeout end reason is REMOTE_BYE");
    check(state.phase == SESSION_PHASE_HANDSHAKE,
          "guest phase after liveness timeout remains HANDSHAKE");
    check(state.peer_ready == 0u, "peer_ready cleared after liveness timeout");
    check(state.link_up == 1u, "broker link remains up");
    check(state.active_link == 1u, "broker link identity remains active");
}

static void test_mqtt_host_liveness_timeout_recovery(void)
{
    SessionConfig config;
    SessionState state;
    SessionEvent event;
    SessionAction actions[SESSION_ACTION_CAPACITY];
    SessionWorkspace workspace;
    uint8_t tx_scratch[64];
    uint8_t count;
    uint8_t tx_id;

    memset(&config, 0, sizeof(config));
    config.transport = SESSION_TRANSPORT_MQTT;
    config.role = SESSION_ROLE_HOST;
    config.host_color = SESSION_COLOR_WHITE;
    config.session_id = 77u;
    check(session_init(&state, &config), "mqtt host init");
    state.link_up = 1u;
    state.active_link = 1u;
    state.phase = SESSION_PHASE_ACTIVE;
    state.peer_ready = 1u;
    state.local_color = SESSION_COLOR_WHITE;
    state.deferred_decision = 0x02u;
    state.liveness_misses = 1u;
    state.timer_mask = (uint8_t)(1u << SESSION_TIMER_LIVENESS);

    memset(&event, 0, sizeof(event));
    event.type = SESSION_EV_TIMEOUT;
    event.data.timeout.timer_id = SESSION_TIMER_LIVENESS;
    count = session_step(&state, &event, &workspace, tx_scratch,
                         sizeof(tx_scratch), actions, SESSION_ACTION_CAPACITY);
    check(count == 2u && actions[0].type == SESSION_ACT_SEND,
          "host timeout releases peer seat");
    tx_id = actions[0].data.send.tx_id;

    event.type = SESSION_EV_TX_RESULT;
    event.data.tx.tx_id = tx_id;
    event.data.tx.result = SESSION_TX_OK;
    count = session_step(&state, &event, &workspace, tx_scratch,
                         sizeof(tx_scratch), actions, SESSION_ACTION_CAPACITY);
    check(count == 3u &&
              actions[1].type == SESSION_ACT_SESSION_CHANGED &&
              actions[1].data.session.status == SESSION_CHANGED_ENDED &&
              actions[1].data.session.reason == SESSION_END_REASON_REMOTE_BYE,
          "host peer expiry reports REMOTE_BYE");
    check(state.phase == SESSION_PHASE_HANDSHAKE && !state.peer_ready &&
              state.link_up && state.active_link == 1u &&
              state.deferred_decision == 0x02u,
          "host waits on broker with own seat retained");
    check(actions[2].type == SESSION_ACT_TIMER_SET &&
              actions[2].data.timer_set.timer_id == SESSION_TIMER_CONTROL,
          "host wait rearms setup timer");
}

static void test_mqtt_crossed_moves_use_five_actions_atomically(void)
{
    static const uint8_t remote_move[] = "MOVE 2 c6";
    SessionConfig config;
    SessionState state;
    SessionState before;
    SessionEvent event;
    SessionAction actions[SESSION_ACTION_CAPACITY];
    SessionWorkspace workspace;
    uint8_t tx_scratch[64];
    uint8_t count;

    memset(&config, 0, sizeof(config));
    config.transport = SESSION_TRANSPORT_MQTT;
    config.role = SESSION_ROLE_GUEST;
    config.host_color = SESSION_COLOR_WHITE;
    config.session_id = 77u;
    check(session_init(&state, &config), "mqtt crossed move init");
    memset(&workspace, 0, sizeof(workspace));
    memcpy(workspace.move, "d6", 3u);
    state.link_up = 1u;
    state.active_link = 1u;
    state.phase = SESSION_PHASE_ACTIVE;
    state.peer_ready = 1u;
    state.pending_control = SESSION_REQUEST_MOVE;
    state.pending_origin = 1u;
    state.pending_value = 1u;
    state.timer_mask = (uint8_t)((1u << SESSION_TIMER_CONTROL) |
                                 (1u << SESSION_TIMER_LIVENESS));
    memset(&event, 0, sizeof(event));
    event.type = SESSION_EV_RX;
    event.data.rx.payload = remote_move;
    event.data.rx.length = (uint8_t)(sizeof(remote_move) - 1u);
    event.data.rx.route = SESSION_ROUTE_GAME;
    event.data.rx.flags = SESSION_RX_LIVE;
    event.data.rx.link_id = 1u;

    before = state;
    count = session_step(&state, &event, &workspace, tx_scratch,
                         sizeof(tx_scratch), actions, 4u);
    check(count == 0u && memcmp(&state, &before, sizeof(state)) == 0,
          "four-action crossed move buffer is atomic");

    count = session_step(&state, &event, &workspace, tx_scratch,
                         sizeof(tx_scratch), actions, SESSION_ACTION_CAPACITY);
    check(count == 5u, "crossed moves emit five actions");
    check(actions[0].type == SESSION_ACT_TIMER_CANCEL &&
              actions[0].data.timer_cancel.timer_id == SESSION_TIMER_CONTROL &&
              actions[1].type == SESSION_ACT_DELIVER_GAME &&
              actions[1].data.game.kind == SESSION_DELIVER_LOCAL_MOVE &&
              actions[2].type == SESSION_ACT_TIMER_CANCEL &&
              actions[2].data.timer_cancel.timer_id == SESSION_TIMER_LIVENESS &&
              actions[3].type == SESSION_ACT_DELIVER_GAME &&
              actions[3].data.game.kind == SESSION_DELIVER_REMOTE_MOVE &&
              actions[4].type == SESSION_ACT_TIMER_SET &&
              actions[4].data.timer_set.timer_id == SESSION_TIMER_CONTROL,
          "crossed move action order");
    check(state.current_ply == 1u &&
              state.pending_control == SESSION_REQUEST_MOVE &&
              state.pending_origin == 2u && state.pending_value == 2u,
          "crossed move commits both plies");
}

static void test_mqtt_valid_move_outside_game_is_busy(void)
{
    static const uint8_t remote_move[] = "MOVE 7 d3";
    SessionConfig config;
    SessionState state;
    SessionEvent event;
    SessionAction actions[SESSION_ACTION_CAPACITY];
    SessionWorkspace workspace;
    uint8_t tx_scratch[64];
    uint8_t count;

    memset(&config, 0, sizeof(config));
    config.transport = SESSION_TRANSPORT_MQTT;
    config.role = SESSION_ROLE_HOST;
    config.host_color = SESSION_COLOR_WHITE;
    config.session_id = 77u;
    check(session_init(&state, &config), "mqtt outside-game move init");
    memset(&workspace, 0, sizeof(workspace));
    state.link_up = 1u;
    state.active_link = 1u;
    state.phase = SESSION_PHASE_READY;
    state.peer_ready = 1u;
    memset(&event, 0, sizeof(event));
    event.type = SESSION_EV_RX;
    event.data.rx.payload = remote_move;
    event.data.rx.length = (uint8_t)(sizeof(remote_move) - 1u);
    event.data.rx.route = SESSION_ROUTE_GAME;
    event.data.rx.flags = SESSION_RX_LIVE;
    event.data.rx.link_id = 1u;

    count = session_step(&state, &event, &workspace, tx_scratch,
                         sizeof(tx_scratch), actions, SESSION_ACTION_CAPACITY);
    check(count == 2u && actions[0].type == SESSION_ACT_SEND &&
              actions[0].data.send.length == 11u &&
              memcmp(actions[0].data.send.payload, "NACK 7 BUSY", 11u) == 0,
          "MQTT valid move outside game receives correlated BUSY");
}

int main(void)
{
    test_init_and_reset();
    test_link_down_order_and_capacity();
    test_invalid_and_local_tx_semantics();
    test_mqtt_guest_liveness_timeout_recovery();
    test_mqtt_host_liveness_timeout_recovery();
    test_mqtt_crossed_moves_use_five_actions_atomically();
    test_mqtt_valid_move_outside_game_is_busy();

    printf("session core sizes: config=%lu state=%lu event=%lu action=%lu workspace=%lu\n",
           (unsigned long)sizeof(SessionConfig),
           (unsigned long)sizeof(SessionState),
           (unsigned long)sizeof(SessionEvent),
           (unsigned long)sizeof(SessionAction),
           (unsigned long)sizeof(SessionWorkspace));
    check(sizeof(SessionConfig) <= 6u, "config size bound");
    check(sizeof(SessionState) <= 40u, "state size bound");
    check(sizeof(SessionEvent) <= 24u, "event size bound");
    check(sizeof(SessionAction) <= 24u, "action size bound");

    if (failures != 0) {
        printf("session core tests failed: %d\n", failures);
        return 1;
    }
    printf("session core tests ok\n");
    return 0;
}
