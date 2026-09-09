#!/usr/bin/env python3
"""Execute production MENU_CONFIG marker, selective-paint and entry ABI probes."""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROBE = r"""
SECTION code_user
TEST_LINE_COUNT EQU 0x7006
TEST_HEADER_COUNT EQU 0x7007
PUBLIC _setup_choice, _setup_focus_choice, _setup_focus_board_theme
PUBLIC _setup_visible_mask, _setup_defined_mask, _setup_cursor
PUBLIC _setup_room_editing, _setup_edit_row, _setup_port_text
PUBLIC _setup_config_dirty, _setup_action_focus, _netchesszx_mqtt_code
PUBLIC _setup_game_focus
PUBLIC _netchesszx_direct_host, _last_ip, _edit_max
PUBLIC _spectrum_info_line, _spectrum_info_show_game_setup
PUBLIC _spectrum_info_show_setup, _spectrum_overlay_context, _spectrum_gui_edit_show
EXTERN _menu_config_paint_attrs_ovl_entry, _menu_config_render_ovl_entry
EXTERN _menu_config_nav_ovl_entry, _input_edit_setup_line_ovl
EXTERN setup_edit_line_buf, setup_edit_line_buf_end

test_start:
    ld sp, 0x7ff0
    xor a
    ld hl, 0x4000
    ld de, 0x4001
    ld (hl), a
    ld bc, 0x17ff
    ldir
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    ld (_setup_defined_mask), hl
    ld hl, 1
    ld (_spectrum_overlay_context + 1), hl
    xor a
    ld (_setup_cursor), a
    ld (0x5ffc), a
    ld a, 1
    ld (0x7002), a
    ld a, 0xa5
    ld (0x5016), a
    ld (0x5a16), a
    ld (0x42f5), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x58f6)
    ld b, 0x46
    call check
    ld a, (0x58fb)
    ld b, 0x07
    call check
    ld a, (0x42f5)
    ld b, 0xe5
    call check
    ld a, (0x5a16)
    ld b, 0xa5
    call check
    ld a, (0x5016)
    call check

    ; Horizontal focus leaves CREATE selected but clears its old marker.
    ld a, 2
    ld (0x7002), a
    ld a, 1
    ld (_setup_focus_choice), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x58f6)
    ld b, 0x05
    call check
    ld a, (0x58fb)
    ld b, 0x47
    call check
    ld a, (0x42f5)
    ld b, 0xa5
    call check
    ld a, (0x42fa)
    ld b, 0x40
    call check

    ; The dedicated NAV entry carries UP/DOWN at CTX_KEY.
    ; Only GAME and LINK change, and the departing marker disappears.
    ld a, 3
    ld (0x7002), a
    ld a, 0x82
    ld (0x5ff8), a
    call _menu_config_nav_ovl_entry
    ld a, (_setup_cursor)
    ld b, 1
    call check
    ld a, (0x5ff9)
    ld b, 3
    call check
    ld a, (0x5ffe)
    ld b, 2
    call check
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x42fa)
    ld b, 0
    call check
    ld a, (0x4a1a)
    ld b, 0x40
    call check
    ld a, (0x591b)
    ld b, 0x46
    call check
    ld a, (0x5a16)
    ld b, 0xa5
    call check

    ; Render uses force at data0..1, full at data2: full must paint every
    ; visible row and request both TIME/ACTION, even with zero force.
    ld a, 4
    ld (0x7002), a
    xor a
    ld (0x5ffc), a
    ld (0x5ffe), a
    ld (_setup_cursor), a
    ld (_setup_focus_choice), a
    ld hl, 0
    ld (0x5ff8), hl
    ld hl, 0xff01
    ld (0x5ffa), hl
    ld de, 0x5ff8
    call _menu_config_render_ovl_entry
    ld de, 0x5ffb
    call _input_edit_setup_line_ovl
    ld a, (0x58f6)
    ld b, 0x46
    call check
IFDEF NETCHESSZX_NEXT
    ld a, (0x5a16)
ELSE
    ld a, (0x5a16)
ENDIF
    ld b, 5
    call check

    ; Confirming SIDE reveals BOARD. Render receives already-updated
    ; visibility and no force, so the visible rows must gain their labels.
    ld a, 5
    ld (0x7002), a
    ld hl, 0x007f
    ld (_setup_visible_mask), hl
    ld hl, 0
    ld (0x5ff8), hl
    ld hl, 0xff00
    ld (0x5ffa), hl
    xor a
    ld (0x5ffe), a
    ld (0x7003), a
    ld a, 6
    ld (_setup_cursor), a
    ld de, 0x5ff8
    call _menu_config_render_ovl_entry
    ld a, (0x7003)
    ld b, 1
    call check

    ; Endpoint refresh keeps an existing ALIGN row even though its first
    ; bitmap scan is blank. Only an explicit newly-visible bit redraws it.
    ld hl, 0x003f
    ld (_setup_visible_mask), hl
    ld hl, 0x0004
    ld (0x5ff8), hl
    ld hl, 0xfe00
    ld (0x5ffa), hl
    xor a
    ld (TEST_LINE_COUNT), a
    ld (TEST_HEADER_COUNT), a
    ld de, 0x5ff8
    call _menu_config_render_ovl_entry
    ld a, (TEST_LINE_COUNT)
    ld b, 2
    call check
    ld a, (TEST_HEADER_COUNT)
    ld b, 0
    call check

    ld hl, 0x0024
    ld (0x5ff8), hl
    xor a
    ld (TEST_LINE_COUNT), a
    ld de, 0x5ff8
    call _menu_config_render_ovl_entry
    ld a, (TEST_LINE_COUNT)
    ld b, 3
    call check

    ; DISCS has different half-cell starts on Classic and Next. Moving
    ; between options must clear only its marker, preserving adjacent ink.
    ld a, 6
    ld (0x7002), a
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    ld hl, 0x0080
    ld (0x5ff9), hl
    ld a, 7
    ld (_setup_cursor), a
    call _menu_config_paint_attrs_ovl_entry
IFDEF NETCHESSZX_NEXT
    ld a, (0x5215)
    ld b, 4
ELSE
    ld a, (0x5215)
    ld b, 4
ENDIF
    call check
    ld a, 1
    ld (_setup_focus_choice + 4), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x5215)
    ld b, 0
    call check
    ld a, (0x5218)
    ld b, 4
    call check
    ld a, (0x5a19)
    ld b, 0x47
    call check

    ; Swatches retain the Mirror Shift five-theme palette. Focus moves its
    ; marker without changing either neighbouring board-preview pixels.
    ld a, 7
    ld (0x7002), a
    ld a, 6
    ld (_setup_cursor), a
    ld hl, 0x0040
    ld (0x5ff9), hl
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x4af5)
    ld b, 4
    call check
IFDEF NETCHESSZX_NEXT
    ld a, (0x59f6)
    ld b, 0x9b
    call check
    ld a, (0x59f8)
    ld b, 0xc9
    call check
    ld a, (0x48f6)
    ld b, 0xfe
    call check
ENDIF
    ld a, 1
    ld (_setup_focus_board_theme), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x4af5)
    ld b, 0
    call check
    ld a, (0x4af7)
    ld b, 4
    call check

    ; Maximum-length IP ends in cell 28. ':' in cell 29's high nibble
    ; shares PORT[0]'s attribute; marker bits must preserve both colon dots.
    ld a, 8
    ld (0x7002), a
    ld a, 3
    ld (_setup_cursor), a
    ld hl, 8
    ld (0x5ff9), hl
    ld a, 0xaa
    ld (0x4a3c), a
    ld a, 0x0b
    ld (0x493d), a
    ld (0x4a3d), a
    ld a, 0x8b
    ld (0x4b3d), a
    ld a, 0x0b
    ld (0x4c3d), a
    ld (0x4d3d), a
    ld a, 0x8b
    ld (0x4e3d), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x4a3c)
    ld b, 0xaa
    call check
    ld a, (0x4a3d)
    ld b, 0x4b
    call check
    ld a, (0x4b3d)
    ld b, 0x2b
    call check
    ld a, (0x4e3d)
    ld b, 0x0b
    call check
    ld a, 4
    ld (_setup_cursor), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x4a3d)
    ld b, 0x0b
    call check
    ld a, (0x4b3d)
    ld b, 0x8b
    call check
    ld a, (0x4e3d)
    ld b, 0x8b
    call check

    ; Exercise composed endpoint text, not only hand-seeded marker pixels.
    ld a, 9
    ld (0x7002), a
    ld hl, endpoint
    ld de, _last_ip
    ld bc, 16
    ldir
    ld hl, port
    ld de, _setup_port_text
    ld bc, 6
    ldir
    ld hl, 0
    ld (0x5ff8), hl
    ld hl, 0xff01
    ld (0x5ffa), hl
    ld de, 0x5ff8
    call _menu_config_render_ovl_entry
    ld de, 0x5ffb
    call _input_edit_setup_line_ovl
    ; The maximum endpoint must exactly fit, including row byte and NUL.
    ld hl, setup_edit_line_buf_end - setup_edit_line_buf
    ld a, h
    ld b, 0
    call check
    ld a, l
    ld b, 29
    call check
    ld hl, endpoint_line_expected
    ld de, setup_edit_line_buf
    ld c, 29
check_endpoint_line:
    ld b, (hl)
    ld a, (de)
    call check
    inc hl
    inc de
    dec c
    jr nz, check_endpoint_line
    ld a, (0x7100 + 6)
    ld b, '2'
    call check
    ld a, (0x7100 + 20)
    ld b, '5'
    call check
    ld a, (0x7100 + 21)
    ld b, ':'
    call check
    ld a, (0x7100 + 26)
    ld b, '5'
    call check
    ; Different colours remain isolated even at the maximum IP length.
    ld a, 3
    ld (_setup_cursor), a
    ld hl, 12
    ld (0x5ff9), hl
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x593c)
    ld b, 5
    call check
    ld a, (0x593d)
    ld b, 0x46
    call check
    ; Both full rendering and the resident row-3 edit entry place the
    ; port cursor at column 59, so its fifth digit is still on-screen.
    ld a, 1
    ld (_setup_room_editing), a
    ld a, 3
    ld (_setup_edit_row), a
    ld a, 5
    ld (_edit_max), a
    ld hl, 0xff01
    ld (0x5ffa), hl
    ld de, 0x5ff8
    call _menu_config_render_ovl_entry
    ld de, 0x5ffb
    call _input_edit_setup_line_ovl
    ld a, (0x7005)
    ld b, 59
    call check
    xor a
    ld (0x7005), a
    ld a, 3
    ld (0x5ff8), a
    ld de, 0x5ff8
    call _input_edit_setup_line_ovl
    ld a, (0x7005)
    ld b, 59
    call check
    jp passed
check:
    ld (0x7001), a
    cp b
    ret z
failed:
    xor a
    ld (0x7000), a
    jp 0
passed:
    ld a, 1
    ld (0x7000), a
    jp 0

_spectrum_info_line:
    push hl
    ld hl, TEST_LINE_COUNT
    inc (hl)
    pop hl
    ld a, (hl)
    cp 9
    jr nz, test_line_board
    inc hl
    ld de, 0x7100
    ld bc, 27
    ldir
    ret
test_line_board:
    cp 15
    ret nz
    ld a, 1
    ld (0x7003), a
    ret
_spectrum_info_show_game_setup:
    ld hl, TEST_HEADER_COUNT
    inc (hl)
    ret
_spectrum_info_show_setup:
    ret
_spectrum_gui_edit_show:
    ld (0x7004), hl
    ret
_spectrum_overlay_context EQU 0x5ff8
_setup_choice: DEFS 6
_setup_focus_choice: DEFS 6
_setup_focus_board_theme: DEFB 0
_setup_visible_mask: DEFS 2
_setup_defined_mask: DEFS 2
_setup_cursor: DEFB 0
_setup_room_editing: DEFB 0
_setup_edit_row: DEFB 0
_setup_port_text: DEFS 6
_setup_config_dirty: DEFB 0
_setup_action_focus: DEFB 0
_setup_game_focus: DEFB 0
_netchesszx_mqtt_code: DEFS 9
_netchesszx_direct_host: DEFS 16
_last_ip: DEFS 16
_edit_max: DEFB 0
endpoint: DEFB "255.255.255.255", 0
port: DEFB "65535", 0
endpoint_line_expected: DEFB 9, "IP    255.255.255.255:65535", 0
"""

SIZE_STUBS = """
SECTION code_user
PUBLIC _spectrum_info_line, _spectrum_info_show_game_setup, _spectrum_overlay_context
PUBLIC _spectrum_info_show_setup, _setup_choice, _setup_focus_choice, _setup_focus_board_theme
PUBLIC _setup_visible_mask, _setup_defined_mask, _setup_cursor, _setup_room_editing
PUBLIC _setup_edit_row, _setup_port_text, _setup_config_dirty, _setup_action_focus, _edit_max
PUBLIC _setup_game_focus
PUBLIC _netchesszx_mqtt_code, _netchesszx_direct_host, _last_ip, _spectrum_gui_edit_show
_spectrum_info_line:
_spectrum_info_show_game_setup:
_spectrum_overlay_context:
_spectrum_info_show_setup:
_setup_choice:
_setup_focus_choice:
_setup_focus_board_theme:
_setup_visible_mask:
_setup_defined_mask:
_setup_cursor:
_setup_room_editing:
_setup_edit_row:
_setup_port_text:
_setup_config_dirty:
_setup_action_focus:
_setup_game_focus:
_edit_max:
_netchesszx_mqtt_code:
_netchesszx_direct_host:
_last_ip:
_spectrum_gui_edit_show:
"""


def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, check=False)


def main() -> int:
    z80asm = shutil.which("z80asm") or "z80asm"
    ticks = shutil.which("z88dk-ticks") or "z88dk-ticks"
    renderer = ROOT / "asm/overlay/menu_config/entry_menu_config.asm"
    input_edit = ROOT / "asm/overlay/input_edit/setup_edit_line.asm"
    with tempfile.TemporaryDirectory(prefix="mirrorshift-render-") as name:
        work = Path(name)
        source = work / "render_probe.asm"
        stubs = work / "render_size_stubs.asm"
        binary = work / "render_probe.bin"
        ram = work / "render_probe.ram"
        renderer_binary = work / "menu_config.bin"
        source.write_text(PROBE, encoding="ascii")
        stubs.write_text(SIZE_STUBS, encoding="ascii")
        result = run([z80asm, "-b", "-r0x6800", "-O=.", "-o=" + renderer_binary.name,
                      str(renderer), stubs.name], work)
        if result.returncode or not renderer_binary.is_file():
            sys.stdout.write(result.stdout)
            print("[ERR] MENU_CONFIG renderer size link failed")
            return 1
        renderer_size = renderer_binary.stat().st_size
        if renderer_size > 2048:
            print(f"[ERR] MENU_CONFIG renderer {renderer_size} > 2048")
            return 1
        for target, defines in (("Classic", []), ("Next", ["-DNETCHESSZX_NEXT"])):
            result = run([z80asm, *defines, "-b", "-r0x8000", "-O=.",
                          "-o=" + binary.name, source.name, str(renderer),
                          str(input_edit)], work)
            if result.returncode or not binary.is_file():
                sys.stdout.write(result.stdout)
                print(f"[ERR] MENU_CONFIG {target} probe assembly failed")
                return 1
            result = run([ticks, "-mz80", "-l", "0x8000", "-pc", "8000", "-end", "0",
                          "-counter", "1000000", "-output", ram.name, binary.name], work)
            if result.returncode or not ram.is_file():
                sys.stdout.write(result.stdout)
                print(f"[ERR] MENU_CONFIG {target} probe execution failed")
                return 1
            image = ram.read_bytes()
            if len(image) < 65536 or image[0x7000] != 1:
                print(f"[ERR] MENU_CONFIG {target} checkpoint={image[0x7002]} "
                      f"actual={image[0x7001]:#04x} "
                      f"board={image[0x7003]} lines={image[0x7006]} "
                      f"header={image[0x7007]}")
                return 1
            print(f"[OK] MENU_CONFIG {target}: marker set/clear, ink, selective paint, render/input ABI")
        print(f"[OK] MENU_CONFIG Classic linked bytes={renderer_size}/2048")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
