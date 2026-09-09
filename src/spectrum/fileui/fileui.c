#include "spectrum/fileui/fileui.h"
#include "spectrum/lowram_map.h"
#include "spectrum/overlay/overlay_api.h"
#include "spectrum/overlay/overlay.h"
#include "spectrum/ui/render.h"

/* Grid geometry mirrors fileui_ovl.c. Navigation runs fully resident so a
   cursor move never reloads the FILEUI overlay from SD (network polling
   keeps swapping transport overlays into the slot). */
#define FILEUI_SLOTS 10u
#define FILEUI_COUNT_MASK 0x0fu
#define FILEUI_MORE 0x80u


#define FILEUI_KEY_UP 0x81u
#define FILEUI_KEY_DOWN 0x82u

static uint8_t fileui_sel;
/* Shared with the FILEUI overlay (written there during RENDER) and the
   screen.asm selection-attr helper. */
uint8_t spectrum_fileui_count;
uint16_t spectrum_fileui_used_mask;
/* Filled by the overlay while the local input editor is inactive. */
#define fileui_name ((char *)NETCHESSZX_LOWRAM_LOCAL_INPUT_ADDR)
/* FILE owns the inactive editor buffer. PICK uses bytes 0..8 for a name;
   byte 9 retains the valid-entry page ordinal between overlay calls.
   ponytail: 8-bit paging covers 255 valid saves; widen only if needed. */
#define fileui_page (*(uint8_t *)(NETCHESSZX_LOWRAM_LOCAL_INPUT_ADDR + 9u))

static uint8_t fileui_exec(uint8_t entry, uint8_t mode)
{
    volatile uint8_t *ctx = spectrum_overlay_context;

    uint16_t name_ptr = (uint16_t)fileui_name;

    ctx[SPECTRUM_OVL_CTX_FILEUI_KEY] = mode;
    ctx[SPECTRUM_OVL_CTX_FILEUI_SEL] = fileui_sel;
    ctx[SPECTRUM_OVL_CTX_FILEUI_NAME_LO] = (uint8_t)name_ptr;
    ctx[SPECTRUM_OVL_CTX_FILEUI_NAME_HI] = (uint8_t)(name_ptr >> 8);
    ctx[SPECTRUM_OVL_CTX_FILEUI_PAGE_LO] = fileui_page;
    if (!spectrum_overlay_exec_cached(SPECTRUM_OVL_FILEUI, entry)) {
        return SPECTRUM_FILEUI_ACT_FAIL;
    }
    fileui_page = ctx[SPECTRUM_OVL_CTX_FILEUI_PAGE_LO];
    return ctx[SPECTRUM_OVL_CTX_FILEUI_ACTION];
}

uint8_t spectrum_fileui_open_render(void)
{
    fileui_sel = 0u;
    fileui_page = 0u;
    return (uint8_t)(fileui_exec(SPECTRUM_OVL_FILEUI_RENDER, 0u) !=
                     SPECTRUM_FILEUI_ACT_FAIL);
}

uint8_t spectrum_fileui_rerender(void)
{
    /* List-only refresh: no frame wipe, rows self-clear in place. */
    return (uint8_t)(fileui_exec(SPECTRUM_OVL_FILEUI_RENDER, 1u) !=
                     SPECTRUM_FILEUI_ACT_FAIL);
}

uint8_t spectrum_fileui_send_key(uint8_t key)
{
    uint8_t sel = fileui_sel;
    uint8_t prev = sel;
    uint8_t saved;
    uint8_t picked;
    uint8_t count = (uint8_t)(spectrum_fileui_count & FILEUI_COUNT_MASK);

    if (key == FILEUI_KEY_UP || key == 'q') {
        if (sel != 0u) {
            --sel;
        } else if (fileui_page != 0u) {
            fileui_page = (uint8_t)(fileui_page - FILEUI_SLOTS);
            fileui_sel = FILEUI_SLOTS - 1u;
            (void)fileui_exec(SPECTRUM_OVL_FILEUI_RENDER, 1u);
            return SPECTRUM_FILEUI_ACT_NONE;
        }
    } else if ((key == FILEUI_KEY_DOWN || key == 'a') &&
               (uint8_t)(sel + 1u) < count) {
        ++sel;
    } else if ((key == FILEUI_KEY_DOWN || key == 'a') &&
               (spectrum_fileui_count & FILEUI_MORE) != 0u) {
        fileui_page = (uint8_t)(fileui_page + count);
        fileui_sel = 0u;
        (void)fileui_exec(SPECTRUM_OVL_FILEUI_RENDER, 1u);
        return SPECTRUM_FILEUI_ACT_NONE;
    } else if (key == 13u || key == ' ' || key == 'e' || key == 'E') {
        saved = (uint8_t)(sel < count);
        if ((key == 'e' || key == 'E') && !saved) {
            return SPECTRUM_FILEUI_ACT_NONE;
        }
        picked = fileui_exec(SPECTRUM_OVL_FILEUI_PICK, 0u);
        if (picked == 1u) {
            return (key == 'e' || key == 'E') ? SPECTRUM_FILEUI_ACT_ERASE
                                              : SPECTRUM_FILEUI_ACT_LOAD;
        }
        if (picked == 2u) {
            return SPECTRUM_FILEUI_ACT_SAVE;
        }
        return SPECTRUM_FILEUI_ACT_NONE;
    } else {
        return SPECTRUM_FILEUI_ACT_NONE;
    }

    if (sel != prev) {
        fileui_sel = sel;
        /* Attr-only repaint (taboption-style inversion), fully
           resident: no overlay reload, no flicker. */
        spectrum_render_fileui_select(prev);
        spectrum_render_fileui_select((uint16_t)(1u << 8) | sel);
    }
    return SPECTRUM_FILEUI_ACT_NONE;
}

const char *spectrum_fileui_selected_name(void)
{
    return fileui_name;
}
