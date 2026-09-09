#!/usr/bin/env python3
"""Verify the runtime-asset / MQTT buffer layout against the linker map.

Classic retains its original low-RAM scratch below the 2 KiB overlay slot.
Next keeps assets and scratch in permanent MMU slot 1, independent of the
4 KiB executable overlay and immutable resident mirror in MMU slot 3.
"""

import argparse
from pathlib import Path
import re
import sys

UI_RUNTIME_BYTES = 812
PIECE_SPRITES_SIZE = 2 * 32
PIECE_REFLECTION_SIZE = 32
PIECE_BUNDLE_SIZE = PIECE_SPRITES_SIZE + PIECE_REFLECTION_SIZE
PIECE_SPRITES_ADDR = 0x662B
OVERLAY_SLOT = 0x6800
MQTT_STREAM_MAX = 373
MQTT_PACKET_MAX = 160
NEXT_SPRITE_STAGE_BYTES = 256
OVERLAY_SCRATCH_ADDR = 0x672B
SPECTRANEXT_LINE_BUF_SIZE = 48
MIN_SP_GAP = 512
MIN_SP_GAP_WARN = 768


def parse_symbol(text, name):
    m = re.search(r"^%s\s*=\s*\$([0-9A-Fa-f]+)" % re.escape(name), text, re.M)
    if not m:
        raise SystemExit("[ERR] symbol %s not found in map" % name)
    return int(m.group(1), 16)


def parse_first_symbol(text, names):
    for name in names:
        m = re.search(r"^%s\s*=\s*\$([0-9A-Fa-f]+)" % re.escape(name),
                      text, re.M)
        if m:
            return int(m.group(1), 16)
    raise SystemExit("[ERR] none of symbols %s found in map" % ", ".join(names))


def parse_optional_symbol(text, name):
    m = re.search(r"^%s\s*=\s*\$([0-9A-Fa-f]+)" % re.escape(name), text, re.M)
    return int(m.group(1), 16) if m else None


def select_source(text, target):
    """Select nested C/ASM IFDEF blocks; never guess expression conditions."""
    defines = {"NETCHESSZX_NEXT", "NETCHESSZX_NEXT_BANKING",
               "NETCHESSZX_NEXT_RESIDENT"} if target == "next" else set()
    if target == "spectranext":
        defines.update(("NETCHESSZX_SPECTRANEXT", "NETCHESSZX_FS_XFS"))
    active = True
    stack = []
    result = []
    for line in text.splitlines():
        directive = re.match(r"\s*(#?ifn?def|#if|IFN?DEF)\s+(.+)", line)
        if directive:
            kind, name = directive.groups()
            condition = None if kind == "#if" else (
                (name.strip() in defines) != ("ifndef" in kind.lower()))
            stack.append((active, condition))
            active = active and condition is True
        elif re.match(r"\s*(?:#else|ELSE)\s*$", line):
            outer, condition = stack[-1]
            condition = None if condition is None else not condition
            stack[-1] = (outer, condition)
            active = outer and condition is True
        elif re.match(r"\s*(?:#endif|ENDIF)\s*$", line):
            active = stack.pop()[0]
        elif re.match(r"\s*#elif\b", line):
            raise SystemExit("[ERR] unsupported #elif in lowmem constants")
        elif active:
            result.append(line)
            defined = re.match(r"\s*#define\s+(\w+)", line)
            if defined:
                defines.add(defined[1])
        elif stack and stack[-1][0] and stack[-1][1] is None:
            if re.match(r"\s*(?:#define\s+|\w+\s+EQU\s+)", line):
                raise SystemExit("[ERR] constant under unsupported #if condition")
    if stack:
        raise SystemExit("[ERR] unbalanced lowmem source conditionals")
    return "\n".join(result)


def parse_const(path, name, target="classic"):
    text = select_source(path.read_text(encoding="utf-8", errors="replace"), target)
    m = re.search(
        r"^(?:#define\s+)?%s\s+(?:EQU\s+)?(?:0x([0-9A-Fa-f]+)|([0-9]+))u?"
        % re.escape(name),
        text,
        re.M,
    )
    if not m:
        raise SystemExit("[ERR] %s not found in %s" % (name, path))
    return int(m.group(1), 16) if m.group(1) else int(m.group(2), 10)


def assert_no_overlap(label, start, size, other_label, other_start, other_size):
    end = start + size
    other_end = other_start + other_size
    if start < other_end and other_start < end:
        raise SystemExit(
            "[ERR] %s 0x%04x..0x%04x overlaps %s 0x%04x..0x%04x"
            % (label, start, end - 1, other_label, other_start, other_end - 1)
        )


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", required=True)
    ap.add_argument("--target", choices=("classic", "next", "spectranext"), default="classic")
    args = ap.parse_args(argv)
    target = args.target
    is_next = target == "next"
    is_cart = target == "spectranext"
    expected_assets = 0x3500 if is_next else 0x6000
    expected_stream = 0x383C if is_next else 0x633C
    expected_packet = 0x3A8B if is_next else 0x658B
    expected_sprites = 0x3B2B if is_next else PIECE_SPRITES_ADDR
    expected_scratch = 0x3C2B if is_next else OVERLAY_SCRATCH_ADDR
    expected_slot = 0x2000 if is_cart else (0x6000 if is_next else OVERLAY_SLOT)
    work_limit = 0x4000 if is_next else OVERLAY_SLOT
    min_sp_gap = 3072 if is_next else MIN_SP_GAP

    def source_const(path, name):
        return parse_const(path, name, target)


    with open(args.map, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()

    asset_base = parse_symbol(text, "asset_load_addr" if is_next else "expand_2x")
    sprites = parse_symbol(text, "piece_sprites_16x16")
    assets_end = asset_base + UI_RUNTIME_BYTES
    stream = parse_symbol(text, "_mqtt_stream")
    packet = source_const(Path("src/spectrum/transport/mqtt_min.h"),
                         "SPECTRUM_MQTT_SCRATCH_BASE")
    about_path = Path("asm/overlay/about/entry_about.asm")
    if not is_next:
        about = source_const(about_path, "about_input")
        about_size = source_const(about_path, "about_input_size")
    lowram_path = Path("src/spectrum/lowram_map.h")
    lowram_scratch = source_const(
        lowram_path, "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR")
    overlay_scratch = parse_symbol(text, "_overlay_scratch_base")
    overlay_slot = parse_symbol(text, "_overlay_code_slot")
    overlay_scratch_size = source_const(
        lowram_path, "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_SIZE")
    spectranext_lowram_path = Path(
        "src/spectrum/platform/spectranext_lowram.h")
    xfs_state_expected = source_const(
        spectranext_lowram_path, "NETCHESSZX_LOWRAM_XFS_STATE_ADDR")
    xfs_state_end_expected = source_const(
        spectranext_lowram_path, "NETCHESSZX_LOWRAM_XFS_STATE_END")
    xfs_dir_expected = source_const(
        spectranext_lowram_path, "NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_ADDR")
    xfs_dir_size = source_const(
        spectranext_lowram_path, "NETCHESSZX_LOWRAM_XFS_DIR_SCRATCH_SIZE")
    piece_backup = source_const(
        spectranext_lowram_path, "NETCHESSZX_LOWRAM_PIECE_MASKS_BACKUP_ADDR")
    piece_backup_size = source_const(
        spectranext_lowram_path, "NETCHESSZX_LOWRAM_PIECE_MASKS_BACKUP_SIZE")
    mqtt_overlay_scratch = source_const(
        Path("asm/overlay/mqtt_connect/entry_mqtt_connect.asm"),
        "mqtt_packet_ovl")
    rules_path = Path("asm/overlay/rules/rules_stub.asm")
    rules_tmp = source_const(rules_path, "r_tmp")
    rules_tmp_size = source_const(
        Path("asm/overlay/rules/rules_stub.asm"), "rules_tmp_size")

    source_assets = source_const(Path("asm/spectrum/screen.asm"), "NETCHESSZX_ASSET_BASE")
    source_stream = source_const(Path("src/spectrum/transport/mqtt_min.h"),
                                 "SPECTRUM_MQTT_RUNTIME_ASSETS_END")
    source_masks = source_const(lowram_path, "NETCHESSZX_LOWRAM_PIECE_MASKS_ADDR")
    context = parse_symbol(text, "_spectrum_overlay_context")
    source_context = source_const(lowram_path, "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR")
    context_size = source_const(lowram_path, "NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_SIZE")
    for label, values, expected in (
        ("asset base", (asset_base, source_assets), expected_assets),
        ("MQTT stream", (stream, source_stream), expected_stream),
        ("MQTT packet", (packet,), expected_packet),
        ("piece masks", (sprites, source_masks), expected_sprites),
        ("overlay context", (context, source_context), 0x5FF8),
        ("overlay context size", (context_size,), 8),
    ):
        if any(value != expected for value in values):
            raise SystemExit("[ERR] %s mismatch for %s: %s, expected 0x%x"
                             % (label, target, values, expected))

    if stream < assets_end:
        raise SystemExit(
            "[ERR] mqtt_stream at 0x%04x overlaps runtime assets ending at "
            "0x%04x (piece sprites corrupt under MQTT). Raise "
            "SPECTRUM_MQTT_RUNTIME_ASSETS_END in mqtt_min.h." % (stream, assets_end)
        )
    if assets_end > work_limit:
        raise SystemExit(
            "[ERR] runtime assets end 0x%04x beyond overlay slot 0x%04x"
            % (assets_end, overlay_slot)
        )
    if sprites != expected_sprites:
        raise SystemExit(
            "[ERR] piece masks at 0x%04x, expected fixed 0x%04x"
            % (sprites, expected_sprites)
        )
    assert_no_overlap("piece_masks", sprites, PIECE_SPRITES_SIZE,
                      "mqtt_stream", stream, MQTT_STREAM_MAX)
    assert_no_overlap("piece_masks", sprites, PIECE_SPRITES_SIZE,
                      "MQTT packet scratch", packet, MQTT_PACKET_MAX)
    if not is_next:
        assert_no_overlap("about_work", about, about_size,
                          "mqtt_stream", stream, MQTT_STREAM_MAX)
        assert_no_overlap("about_work", about, about_size,
                          "MQTT packet scratch", packet, MQTT_PACKET_MAX)
        if about + about_size > work_limit:
            raise SystemExit(
                "[ERR] about_work 0x%04x..0x%04x reaches overlay slot"
                % (about, about + about_size - 1)
            )
    if overlay_slot != expected_slot:
        raise SystemExit(
            "[ERR] linked overlay slot 0x%04x != fixed 0x%04x"
            % (overlay_slot, expected_slot)
        )
    if (overlay_scratch != expected_scratch or
            lowram_scratch != overlay_scratch or
            mqtt_overlay_scratch != overlay_scratch or
            rules_tmp != overlay_scratch):
        raise SystemExit(
            "[ERR] overlay scratch mismatch: map 0x%04x, header 0x%04x, "
            "MQTT 0x%04x, rules 0x%04x, fixed 0x%04x"
            % (overlay_scratch, lowram_scratch, mqtt_overlay_scratch,
               rules_tmp, expected_scratch)
        )
    if overlay_scratch_size != MQTT_PACKET_MAX:
        raise SystemExit(
            "[ERR] overlay scratch size %d != MQTT packet size %d"
            % (overlay_scratch_size, MQTT_PACKET_MAX)
        )
    if sprites + PIECE_BUNDLE_SIZE > overlay_scratch:
        raise SystemExit(
            "[ERR] piece bundle 0x%04x..0x%04x reaches overlay scratch 0x%04x"
            % (sprites, sprites + PIECE_BUNDLE_SIZE - 1, overlay_scratch)
        )
    if rules_tmp_size <= 0 or rules_tmp_size > overlay_scratch_size:
        raise SystemExit(
            "[ERR] rules scratch size %d invalid for %d-byte overlay scratch"
            % (rules_tmp_size, overlay_scratch_size)
        )
    overlay_scratch_end = overlay_scratch + overlay_scratch_size
    if overlay_scratch_end > work_limit:
        raise SystemExit(
            "[ERR] overlay scratch 0x%04x..0x%04x reaches overlay slot"
            % (overlay_scratch, overlay_scratch_end - 1)
        )
    if not is_next and not (about <= overlay_scratch and
            overlay_scratch_end <= about + about_size):
        raise SystemExit(
            "[ERR] time-disjoint ABOUT work no longer contains overlay scratch"
        )

    xfs_state = parse_optional_symbol(text, "_esx_handle")
    if xfs_state is not None:
        piece_stage = parse_symbol(text, "piece_sprite_addr")
        line_buf = parse_symbol(text, "_line_buf")
        xfs_state_end = parse_symbol(text, "_spxn_xfs_state_end")
        xfs_dir_scratch = parse_symbol(text, "_spxn_xfs_dir_scratch")
        if xfs_state != xfs_state_expected or \
                xfs_state_end != xfs_state_end_expected:
            raise SystemExit(
                "[ERR] Spectranext XFS state 0x%04x..0x%04x != fixed "
                "0x%04x..0x%04x"
                % (xfs_state, xfs_state_end - 1,
                   xfs_state_expected, xfs_state_end_expected - 1)
            )
        if is_cart:
            bss_user = parse_symbol(text, "__bss_user_head")
            bss_user_end = parse_symbol(text, "__bss_user_tail")
            low_bss_end = parse_symbol(text, "__BSS_END_tail")
            if not (bss_user == line_buf == 0x5B00 and
                    bss_user_end <= low_bss_end <= xfs_state):
                raise SystemExit("[ERR] Spectranext low BSS overlaps XFS state")
        elif line_buf + SPECTRANEXT_LINE_BUF_SIZE != xfs_state:
            raise SystemExit("[ERR] Spectranext line buffer/XFS state mismatch")
        if parse_optional_symbol(text, "_uart_ring") is not None:
            raise SystemExit(
                "[ERR] Spectranext must omit the UART ring that aliases XFS state"
            )
        if piece_stage != sprites:
            raise SystemExit(
                "[ERR] Spectranext DAT piece destination must alias live masks"
            )
        if xfs_dir_scratch != xfs_dir_expected or \
                xfs_dir_scratch + xfs_dir_size != overlay_scratch:
            raise SystemExit(
                "[ERR] Spectranext XFS directory scratch must end at overlay scratch"
            )
        if xfs_dir_scratch != sprites:
            raise SystemExit(
                "[ERR] Spectranext XFS scratch/piece-bundle alias changed unexpectedly"
            )
        if piece_backup < overlay_scratch or \
                piece_backup + piece_backup_size > overlay_scratch_end or \
                piece_backup_size != PIECE_BUNDLE_SIZE:
            raise SystemExit(
                "[ERR] Spectranext piece-bundle backup is outside overlay scratch"
            )
        # Paged cart's resident subset never scans directories. The full
        # adapter's preservation symbols are checked in each linked overlay.
        if not is_cart:
            if (parse_symbol(text, "_spxn_xfs_scratch_preserve_base") != sprites or
                    parse_symbol(text, "_spxn_xfs_scratch_preserve_size") != PIECE_BUNDLE_SIZE or
                    parse_symbol(text, "_spxn_xfs_scratch_preserve_backup") != piece_backup):
                raise SystemExit("[ERR] XFS scratch alias lacks linked piece-bundle preservation")

    next_layout = Path("asm/next/extension_bank_layout.asm")
    if is_next:
        stage = source_const(next_layout, "next_sprite_stage")
        linked_stage = parse_symbol(text, "next_sprite_stage")
        if linked_stage != stage:
            raise SystemExit("[ERR] linked/source Next sprite stage mismatch")
    else:
        # Classic keeps the same transient 256-byte directory/staging range.
        stage = sprites
    assert_no_overlap("next_sprite_stage", stage, NEXT_SPRITE_STAGE_BYTES,
                      "mqtt_stream", stream, MQTT_STREAM_MAX)
    assert_no_overlap("next_sprite_stage", stage, NEXT_SPRITE_STAGE_BYTES,
                      "MQTT packet scratch", packet, MQTT_PACKET_MAX)
    if packet + MQTT_PACKET_MAX != stage:
        raise SystemExit(
            "[ERR] MQTT packet scratch must end at Next sprite stage"
        )
    if stage + NEXT_SPRITE_STAGE_BYTES != overlay_scratch:
        raise SystemExit(
            "[ERR] Next sprite stage must end at overlay scratch 0x%04x"
            % overlay_scratch
        )
    if stage + NEXT_SPRITE_STAGE_BYTES > work_limit:
        raise SystemExit(
            "[ERR] next_sprite_stage 0x%04x..0x%04x reaches overlay slot"
            % (stage, stage + NEXT_SPRITE_STAGE_BYTES - 1)
        )
    if is_next:
        palette = source_const(next_layout, "next_palette_stage")
        linked_palette = parse_symbol(text, "next_palette_stage")
        palette_bytes = source_const(next_layout, "next_sprite_palette_size") * 2
        palette_bytes += source_const(next_layout, "next_ula_standard_palette_size")
        if palette != 0x3CCB or linked_palette != palette or palette_bytes != 640:
            raise SystemExit("[ERR] Next graphics staging must occupy 0x3ccb..0x3f4a")
        if palette != overlay_scratch_end or palette + palette_bytes > work_limit:
            raise SystemExit("[ERR] Next graphics staging exceeds permanent slot 1")
        if source_const(next_layout, "next_extension_code_limit") != asset_base:
            raise SystemExit("[ERR] Next extension code limit must meet runtime assets")

    bss_end = max(parse_optional_symbol(text, name) or 0 for name in
                  ("__BSS_END_tail", "__BSS_END", "__bss_end",
                   "__bss_compiler_tail", "__bss_user_tail"))
    stack_top = parse_first_symbol(text,
                                  ("__register_sp", "TAR__register_sp"))
    if stack_top == 0:
        stack_top = 0x10000
    if is_cart:
        table = parse_symbol(text, "_spxn_overlay_page_table")
        backend_start = parse_symbol(text, "_spxn_overlay_loader_start")
        backend_end = parse_symbol(text, "_spxn_overlay_loader_end")
        if table != 0x6800 or not (backend_start == 0x6E00 < backend_end <= 0x7000):
            raise SystemExit("[ERR] Spectranext page table/backend geometry mismatch")
        if parse_symbol(text, "ovl_atlas_count") != 19:
            raise SystemExit("[ERR] Spectranext requires 19 reserved overlay pages")
    if is_next:
        data_head = parse_symbol(text, "__DATA_head")
        data_end = parse_symbol(text, "__DATA_END_tail")
        bss_head = parse_symbol(text, "__BSS_head")
        if not (0x8000 <= data_head <= data_end <= bss_head <= bss_end <= stack_top):
            raise SystemExit("[ERR] Next writable DATA/BSS must stay above 0x8000")
    stack_gap = stack_top - bss_end
    if stack_gap < min_sp_gap:
        raise SystemExit(
            "[ERR] SP 0x%04x minus BSS_END 0x%04x leaves %d bytes, "
            "below %d byte floor"
            % (stack_top, bss_end, stack_gap, min_sp_gap)
        )
    if stack_gap < MIN_SP_GAP_WARN:
        print(
            "[WARN] SP 0x%04x minus BSS_END 0x%04x leaves %d bytes, "
            "below %d byte warning"
            % (stack_top, bss_end, stack_gap, MIN_SP_GAP_WARN)
        )
    print("[OK] SP gap hard floor: %d >= %d bytes"
          % (stack_gap, min_sp_gap))
    print(
        "[OK] lowmem layout: assets end 0x%04x <= mqtt_stream 0x%04x"
        % (assets_end, stream)
    )
    print(
        "[OK] linked overlay scratch: 0x%04x..0x%04x; header/slot match"
        % (overlay_scratch, overlay_scratch_end - 1)
    )
    if xfs_state is not None:
        print(
            "[OK] Spectranext XFS state/scratch and mask backup: "
            "0x%04x..0x%04x, 0x%04x..0x%04x, 0x%04x..0x%04x"
            % (xfs_state, xfs_state_end - 1, xfs_dir_scratch,
               overlay_scratch - 1, piece_backup,
               piece_backup + piece_backup_size - 1)
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
