#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST
#define __z88dk_fastcall

static const char *last_message;
static uint8_t last_kind;

#include "../../src/spectrum/overlay/notices_ovl.c"

void spectrum_gui_notify_persistent(const char *text)
{
    last_message = text;
    last_kind = SPECTRUM_GUI_MSG_KIND_WAIT;
}

void spectrum_gui_notify_success(const char *text)
{
    last_message = text;
    last_kind = SPECTRUM_GUI_MSG_KIND_SUCCESS;
}

void spectrum_gui_notify(const char *text, uint8_t is_error)
{
    last_message = text;
    last_kind = is_error ? SPECTRUM_GUI_MSG_KIND_ERROR
                         : SPECTRUM_GUI_MSG_KIND_INFO;
}

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void check_notice(uint8_t id, uint8_t kind, const char *text)
{
    uint8_t ctx[SPECTRUM_OVERLAY_CONTEXT_SIZE] = {0u};

    ctx[SPECTRUM_OVL_CTX_GUI_MSG_ID] = id;
    ctx[SPECTRUM_OVL_CTX_GUI_MSG_KIND] = kind;
    last_message = 0;
    last_kind = 0xffu;
    check(notices_notify_msg_ovl(ctx) == 1u, "notice id accepted");
    check(last_message != 0 && strcmp(last_message, text) == 0,
          "notice text exact");
    check(last_kind == kind, "notice style exact");
}

int main(void)
{
    uint8_t ctx[SPECTRUM_OVERLAY_CONTEXT_SIZE] = {0u};
    uint8_t id;

    for (id = 0u; id < SPECTRUM_GUI_MSG_COUNT; ++id) {
        ctx[SPECTRUM_OVL_CTX_GUI_MSG_ID] = id;
        ctx[SPECTRUM_OVL_CTX_GUI_MSG_KIND] = SPECTRUM_GUI_MSG_KIND_INFO;
        last_message = 0;
        if ((id >= 15u && id <= 17u) || (id >= 59u && id <= 60u)) {
            check(notices_notify_msg_ovl(ctx) == 0u,
                  "removed notice id is a tombstone");
            continue;
        }
        check(notices_notify_msg_ovl(ctx) == 1u, "live notice id accepted");
        check(last_message != 0 && last_message[0] != '\0',
              "every notice id resolves");
    }

    check_notice(SPECTRUM_GUI_MSG_CONFIG_INVALID,
                 SPECTRUM_GUI_MSG_KIND_ERROR, "CONFIG INVALID");
    check_notice(SPECTRUM_GUI_MSG_LOAD_WAITING_APPROVAL,
                 SPECTRUM_GUI_MSG_KIND_WAIT, "Waiting opponent approval");
    check_notice(SPECTRUM_GUI_MSG_YOUR_TURN,
                 SPECTRUM_GUI_MSG_KIND_INFO, "SELF TO ALIGN");
    check_notice(SPECTRUM_GUI_MSG_LOAD_CANCELLED,
                 SPECTRUM_GUI_MSG_KIND_INFO, "Load cancelled");
    check_notice(SPECTRUM_GUI_MSG_CONTROL_RESET_CANCELLED,
                 SPECTRUM_GUI_MSG_KIND_INFO,
                 "RESET cancelled: no response");
    check_notice(SPECTRUM_GUI_MSG_CONTROL_RESET_EXPIRED,
                 SPECTRUM_GUI_MSG_KIND_INFO, "RESET request expired");
    check_notice(SPECTRUM_GUI_MSG_WAITING_RESIGN_ACK,
                 SPECTRUM_GUI_MSG_KIND_WAIT, "WAITING RESIGN ACK");
    check_notice(SPECTRUM_GUI_MSG_RESTARTING_GAME,
                 SPECTRUM_GUI_MSG_KIND_WAIT, "RESTARTING GAME");
    check_notice(SPECTRUM_GUI_MSG_WAITING_HOST,
                 SPECTRUM_GUI_MSG_KIND_WAIT, "Waiting host");

    ctx[SPECTRUM_OVL_CTX_GUI_MSG_ID] = SPECTRUM_GUI_MSG_COUNT;
    check(notices_notify_msg_ovl(ctx) == 0u, "out-of-range id rejected");
    puts("notices: all checks passed");
    return 0;
}
