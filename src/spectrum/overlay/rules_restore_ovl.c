#include "common/reversi/reversi.h"
#include "spectrum/overlay/overlay_context.h"

#include "common/reversi/reversi_restore.inc"

uint8_t rules_restore_validate_ovl(uint8_t *ctx) __z88dk_fastcall
{
    const ms_state_t *snapshot =
        (const ms_state_t *)((uint16_t)ctx[
            SPECTRUM_OVL_CTX_RULES_RESTORE_SNAP_LO] |
            ((uint16_t)ctx[SPECTRUM_OVL_CTX_RULES_RESTORE_SNAP_HI] << 8));

    ctx[SPECTRUM_OVL_CTX_RULES_RESTORE_RESULT] =
        (uint8_t)(ms_rules_validate_restore(snapshot) == MS_OK);
    return 1u;
}
