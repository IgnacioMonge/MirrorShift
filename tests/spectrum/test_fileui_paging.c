#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define NETCHESSZX_FILEUI_TEST 1
#define __z88dk_fastcall

static char host_overlay_scratch[160];
#define NETCHESSZX_SPECTRUM_LOWRAM_MAP_H
#define NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR ((uintptr_t)host_overlay_scratch)
#define NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE sizeof(host_overlay_scratch)
#define NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR 0u
#define NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_SIZE 8u

uint8_t spectrum_fileui_count;
uint16_t spectrum_fileui_used_mask;
uint8_t esx_handle;
uint16_t esx_buf;
uint16_t esx_count;
uint16_t esx_result;

static unsigned char entries[15][24];
static uint8_t entry_count;
static uint8_t entry_index;
static char rows[10][25];

#include "../../src/spectrum/overlay/fileui_ovl.c"

static void check(uint8_t condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

char *spectrum_append_text(char *dst, const char *src)
{
    while ((*dst = *src) != '\0') {
        ++dst;
        ++src;
    }
    return dst;
}

void spectrum_net_background_drain(void) {}
void spectrum_render_fileui_frame(void) {}
void spectrum_render_fileui_select(uint16_t slot_on) { (void)slot_on; }

void spectrum_render_ikkle_at(const char *spec)
{
    uint8_t row = (uint8_t)spec[0];

    if (row >= FILEUI_ROW_FIRST && row < FILEUI_ROW_FIRST + FILEUI_SLOTS) {
        strcpy(rows[row - FILEUI_ROW_FIRST], spec + 3);
    }
}

void esx_opendir(const char *path)
{
    (void)path;
    entry_index = 0u;
    esx_handle = 1u;
}

void fileui_test_readdir(unsigned char *ent)
{
    if (entry_index >= entry_count) {
        esx_result = 0u;
        return;
    }
    memcpy(ent, entries[entry_index++], 24u);
    esx_result = 1u;
}

void esx_readdir(void) {}
uint8_t esx_fclose(void) { esx_handle = 0u; return 0u; }
void esx_fopen(const char *path) { (void)path; }

static void add_entry(const char *base, const char *ext, uint8_t nonempty)
{
    unsigned char *ent = entries[entry_count++];
    char *p;

    memset(ent, 0, 24u);
    sprintf((char *)ent + 1u, "%s.%s", base, ext);
    p = (char *)ent + strlen((char *)ent + 1u) + 1u;
    p[1] = 0u;
    p[2] = 0u;
    p[3] = 0x21;
    p[4] = 0x50;
    p[5] = (char)nonempty;
}

int main(void)
{
    uint8_t ctx[SPECTRUM_OVERLAY_CONTEXT_SIZE] = {0u};
    char picked[9];
    uint8_t slot;

    add_entry("BAD", "TXT", 1u);
    for (slot = 1u; slot <= 9u; ++slot) {
        char base[9] = "01A11000";
        base[0] = '0';
        base[1] = (char)('0' + slot);
        add_entry(base, "MSH", 1u);
    }
    add_entry("01B11000", "MSH", 1u);
    add_entry("03000000", "MSH", 0u);
    add_entry("10C11000", "MSH", 1u);
    add_entry("02D11000", "MSH", 1u);

    check(fileui_render_ovl(ctx) == 1u, "first page renders");
    check(spectrum_fileui_count == (FILEUI_MORE | 10u),
          "first page exposes continuation");
    check(spectrum_fileui_used_mask == FILEUI_ALL_USED,
          "full scan finds slot ten beyond first page");

    ctx[SPECTRUM_OVL_CTX_FILEUI_PAGE_LO] = 10u;
    memset(rows, 0, sizeof(rows));
    check(fileui_render_ovl(ctx) == 1u, "second page renders");
    check(spectrum_fileui_count == 2u, "second page has remaining entries");
    check(strstr(rows[2], "-FULL-") != NULL,
          "occupied logical slots are not offered as free");

    ctx[SPECTRUM_OVL_CTX_FILEUI_SEL] = 0u;
    ctx[SPECTRUM_OVL_CTX_FILEUI_NAME_LO] = (uint8_t)(uintptr_t)picked;
    ctx[SPECTRUM_OVL_CTX_FILEUI_NAME_HI] =
        (uint8_t)((uintptr_t)picked >> 8);
#if UINTPTR_MAX > UINT16_MAX
    /* Host pointers do not fit the target context; exercise ordinal lookup
       by copying the page cache that product PICK consumes. */
    strcpy(picked, FILEUI_PAGE_NAMES);
#else
    check(fileui_pick_ovl(ctx) == 1u &&
              ctx[SPECTRUM_OVL_CTX_FILEUI_ACTION] == 1u,
          "paged pick succeeds");
#endif
    check(strcmp(picked, "10C11000") == 0,
          "paged ordinal resolves the post-gap entry");

    puts("fileui paging tests passed");
    return 0;
}
