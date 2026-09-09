#!/usr/bin/env python3
"""Run the production Spectrum setup reducer through z88dk-ticks."""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PROBE = r"""
SECTION code_user

PUBLIC _setup_choice
PUBLIC _setup_focus_choice
PUBLIC _setup_focus_board_theme
PUBLIC _setup_defined_mask
PUBLIC _setup_visible_mask
PUBLIC _setup_cursor
PUBLIC _setup_room_editing
PUBLIC _setup_edit_row
PUBLIC _setup_port_text
PUBLIC _setup_config_dirty
PUBLIC _setup_action_focus
PUBLIC _setup_game_focus
PUBLIC _setup_edit_backup
PUBLIC _last_ip
PUBLIC _netchesszx_mqtt_code
PUBLIC _netchesszx_direct_host
PUBLIC _netchesszx_direct_port
PUBLIC _last_ip
PUBLIC _spectrum_overlay_context
PUBLIC _edit_buf
PUBLIC _edit_max
PUBLIC _spectrum_gui_edit_bind
PUBLIC _spectrum_gui_edit_key
PUBLIC _spectrum_gui_edit_hide
PUBLIC _spectrum_gui_edit_show
PUBLIC _spectrum_info_line
PUBLIC _spectrum_info_show_game_setup
PUBLIC _spectrum_info_show_setup
PUBLIC _spectrum_append_u16
PUBLIC _netchess_mqtt_session_parse_u16_token
PUBLIC _spectrum_info_line
PUBLIC _spectrum_info_show_game_setup
PUBLIC _spectrum_info_show_setup
PUBLIC _spectrum_overlay_context
PUBLIC _spectrum_gui_edit_show

EXTERN _setup_step_ovl_entry
EXTERN _input_edit_setup_line_ovl
EXTERN _menu_config_nav_ovl_entry
EXTERN _menu_config_paint_attrs_ovl_entry
EXTERN _menu_config_render_ovl_entry

CTX         EQU 0x5ff8
TEST_STATUS EQU 0x7000
TEST_EDITING EQU 0x7001
TEST_LINES  EQU 0x7002
TEST_GAME_HEADER EQU 0x7003
TEST_SP     EQU 0x7ff0

test_start:
    ld sp, TEST_SP
    xor a
    ld (TEST_STATUS), a

    ; Existing JOIN+MQTT ROOM: confirming LINK focuses it without editing.
    call test_prepare_initial_mqtt_link
    call test_set_mqtt_room
    call _setup_step_ovl_entry
    ld a, (_setup_defined_mask)
    cp 0x03
    jp nz, test_done
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_done
    ld a, (_setup_room_editing)
    ld (TEST_EDITING), a
    or a
    jp nz, test_done
    ld a, (CTX + 6)
    and 0x04
    jp nz, test_done
    ld a, 1
    ld (TEST_STATUS), a

    ; Explicit ENTER on ROOM still starts editing.
    ld a, 13
    ld (CTX), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_done
    ld a, (_setup_edit_row)
    cp 2
    jp nz, test_done
    ld a, (CTX + 6)
    and 0x04
    jp z, test_done
    ld a, 2
    ld (TEST_STATUS), a

    ; Empty JOIN+MQTT ROOM: confirming LINK starts its editor immediately.
    call test_prepare_initial_mqtt_link
    xor a
    ld (_netchesszx_mqtt_code + 2), a
    call _setup_step_ovl_entry
    ld a, (_setup_defined_mask)
    cp 0x03
    jp nz, test_done
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_done
    ld a, (_setup_room_editing)
    ld (TEST_EDITING), a
    cp 1
    jp nz, test_done
    ld a, (_setup_edit_row)
    cp 2
    jp nz, test_done
    ld a, (CTX + 6)
    and 0x04
    jp z, test_done
    ld a, 3
    ld (TEST_STATUS), a

    ; JOIN+DIRECT advances from ROOM to its visible PORT row.
    call test_prepare_direct_room
    call _setup_step_ovl_entry
    ld a, (_setup_defined_mask)
    cp 0x07
    jp nz, test_done
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_done
    call test_confirm_result
    jp nz, test_done
    ld a, 4
    ld (TEST_STATUS), a

    ; Confirming an MQTT room completes the hidden compatibility row and
    ; advances HOST to SIDE.
    xor a
    call test_prepare_mqtt_room
    call _setup_step_ovl_entry
    ld a, (_setup_defined_mask)
    cp 0x0f
    jp nz, test_done
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_done
    call test_confirm_result
    jp nz, test_done
    ld a, 5
    ld (TEST_STATUS), a

    ; JOIN also advances to TIME; TIME_CONFIG owns the later SIDE skip.
    ld a, 1
    call test_prepare_mqtt_room
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_done
    call test_confirm_result
    jp nz, test_done
    ld a, 6
    ld (TEST_STATUS), a

    ld a, 7
    ld (TEST_STATUS), a

    ; DIRECT PORT shares row 9 with IP and owns attrs 29..31. It must not
    ; spill into the following attribute row.
    ld hl, 0x000c
    ld (_setup_visible_mask), hl
    xor a
    ld (_setup_defined_mask), a
    ld (_setup_defined_mask + 1), a
    ld (_setup_choice + 1), a
    ld hl, 0x000c
    ld (CTX + 1), hl
    ld a, 0x6c
    ld (0x5940), a
    call _menu_config_paint_attrs_ovl_entry
    ld a, (0x5940)
    cp 0x6c
    jp nz, test_done
    ld a, (0x593d)
    cp 0x07
    jp nz, test_done

    ld a, 8
    ld (TEST_STATUS), a

    ; DIRECT keeps IP:PORT on row 9. IP starts at col 43 and PORT at col 59;
    ; the PORT editor must never start on the fixed ':' at col 58.
    ld a, 1
    ld (_setup_choice), a
    xor a
    ld (_setup_choice + 1), a
    ld hl, direct_ip_max
    ld de, _netchesszx_direct_host
    ld bc, 16
    ldir
    ld a, 1
    ld (_setup_room_editing), a
    ld a, 2
    ld (_setup_edit_row), a
    ld a, 15
    ld (_edit_max), a
    ld de, test_row_ip
    call _input_edit_setup_line_ovl
    ld hl, (test_edit_geometry)
    ld (0x7010), hl
    ld de, 0x2b09
    or a
    sbc hl, de
    jp nz, test_done
    ld hl, (test_line_ptr)
    ld de, 22
    add hl, de
    ld a, (hl)
    ld (0x7012), a
    cp ':'
    jp nz, test_done

    ld hl, port_max
    ld de, _setup_port_text
    ld bc, 6
    ldir
    ld a, 3
    ld (_setup_edit_row), a
    ld a, 5
    ld (_edit_max), a
    ld de, test_row_port
    call _input_edit_setup_line_ovl
    ld hl, (test_edit_geometry)
    ld (0x7013), hl
    ld de, 0x3b09
    or a
    sbc hl, de
    jp nz, test_done
    ld hl, (test_line_ptr)
    ld de, 28
    add hl, de
    ld a, (hl)
    or a
    jp nz, test_done

    ld a, 1
    ld (_setup_choice + 1), a
    ld a, 2
    ld (_setup_edit_row), a
    ld a, 4
    ld (_edit_max), a
    ld de, test_row_ip
    call _input_edit_setup_line_ovl
    ld hl, (test_edit_geometry)
    ld de, 0x2d09
    or a
    sbc hl, de
    jp nz, test_done

    ld a, 9
    ld (TEST_STATUS), a

    ; Vertical navigation now runs in MENU_CONFIG, so advancing and painting
    ; share one cold load. Direct keeps its PORT row.
    ld hl, 0x03ff
    ld (_setup_visible_mask), hl
    xor a
    ld (_setup_choice + 1), a
    ld a, 2
    ld (_setup_cursor), a
    ld a, 0x82
    ld (CTX), a
    call _menu_config_nav_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_done
    ld a, (CTX + 6)
    cp 0x02
    jp nz, test_done
    ld a, (CTX + 1)
    cp 0x0c
    jp nz, test_done
    ld a, 8
    ld (TEST_STATUS), a

    ; MQTT hides the compatibility PORT row and crossing into TIME requests
    ; only the TIME_CONFIG UI refresh after MENU_CONFIG has painted focus.
    ld a, 1
    ld (_setup_choice + 1), a
    ld a, 2
    ld (_setup_cursor), a
    ld a, 0x82
    ld (CTX), a
    call _menu_config_nav_ovl_entry
    ld a, (_setup_cursor)
    cp 4
    jp nz, test_done
    ld a, (CTX + 6)
    cp 0x12
    jp nz, test_done
    ld a, 9
    ld (TEST_STATUS), a

    ; Reverse traversal applies the same hidden-row and TIME refresh rules.
    ld a, 0x81
    ld (CTX), a
    call _menu_config_nav_ovl_entry
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_done
    ld a, (CTX + 6)
    cp 0x12
    jp nz, test_done
    ld a, 10
    ld (TEST_STATUS), a

    ; Entering ACTION derives its focus from dirty state exactly as SETUP did.
    ld a, 8
    ld (_setup_cursor), a
    ld a, 1
    ld (_setup_config_dirty), a
    ld (_setup_action_focus), a
    ld a, 0x82
    ld (CTX), a
    call _menu_config_nav_ovl_entry
    ld a, (_setup_cursor)
    cp 9
    jp nz, test_done
    ld a, (_setup_action_focus)
    or a
    jp nz, test_done
    ld a, (CTX + 6)
    cp 0x22
    jp nz, test_done
    ld a, 11
    ld (TEST_STATUS), a

    ; In an incomplete menu, changing GAME keeps only GAME defined and
    ; returns to LINK, matching the upstream progressive setup flow.
    xor a
    ld (_setup_choice), a
    ld a, 1
    ld (_setup_choice + 1), a
    ld (_setup_focus_choice), a
    ld hl, 0x0003
    ld (_setup_defined_mask), hl
    ld hl, 0x000f
    ld (_setup_visible_mask), hl
    xor a
    ld (_setup_cursor), a
    ld (_setup_room_editing), a
    call test_set_mqtt_room
    call test_paint_connection_focus
    ld a, 13
    ld (CTX), a
    call _setup_step_ovl_entry
    ld a, (_setup_defined_mask)
    ld (0x7004), a
    ld a, (_setup_cursor)
    ld (0x7005), a
    ld a, (CTX + 3)
    ld (0x7006), a
    ld a, (CTX + 1)
    ld (0x7007), a
    ld a, (_setup_defined_mask)
    cp 1
    jp nz, test_done
    ld a, (_setup_cursor)
    cp 1
    jp nz, test_done
    ld a, (CTX + 3)
    cp 1
    jp nz, test_done
    ld a, (CTX + 1)
    cp 0xfe
    jp nz, test_done
    ld a, (CTX + 2)
    cp 0xff
    jp nz, test_done
    ld a, 12
    ld (TEST_STATUS), a

    ; A complete CREATE->JOIN change keeps GAME SETUP pixels. Only the
    ; connection rows redraw and the now-hidden SIDE row is cleared.
    xor a
    ld (_setup_choice), a
    ld a, 1
    ld (_setup_choice + 1), a
    ld (_setup_focus_choice), a
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld (_setup_visible_mask), hl
    xor a
    ld (_setup_cursor), a
    ld (_setup_room_editing), a
    call test_set_mqtt_room
    call test_paint_connection_focus
    ld a, 13
    ld (CTX), a
    call _setup_step_ovl_entry
    ld a, (_setup_defined_mask)
    ld (0x7004), a
    ld a, (_setup_cursor)
    ld (0x7005), a
    ld a, (CTX + 3)
    ld (0x7006), a
    ld a, (CTX + 1)
    ld (0x7007), a
    ld hl, (_setup_defined_mask)
    ld a, h
    cp 3
    jp nz, test_done
    ld a, l
    cp 0xff
    jp nz, test_done
    ld a, (_setup_cursor)
    cp 1
    jp nz, test_done
    ld a, (CTX + 3)
    cp 0xfe
    jp nz, test_done
    ld a, (CTX + 1)
    cp 0x04
    jp nz, test_done
    ld hl, 0x03d7
    ld (_setup_visible_mask), hl
    ld a, 0xaa
    ld (0x48d2), a
    ld (0x48f2), a
    call test_prepare_render_context
    call test_reset_render_counts
    ld de, CTX
    call _menu_config_render_ovl_entry
    ld a, 0xff
    ld (CTX), a
    ld de, CTX
    call _input_edit_setup_line_ovl
    ld a, (0x42fa)
    and 6
    jp nz, test_done
    ld a, (TEST_LINES)
    cp 3
    jp nz, test_done
    ld a, (TEST_GAME_HEADER)
    or a
    jp nz, test_done
    ld a, (0x48d2)
    or a
    jp nz, test_done
    ld a, (0x48f2)
    cp 0xaa
    jp nz, test_done
    ld a, 13
    ld (TEST_STATUS), a

    ; A complete JOIN->CREATE change reveals ALIGN during endpoint refresh.
    ; The reducer must mark that newly-visible row explicitly for MENU_CONFIG.
    xor a
    ld (_setup_focus_choice), a
    ld (_setup_cursor), a
    ld a, 13
    ld (CTX), a
    call _setup_step_ovl_entry
    ld a, (_setup_choice)
    or a
    jp nz, test_done
    ld a, (CTX + 3)
    cp 0xfe
    jp nz, test_done
    ld a, (CTX + 1)
    cp 0x24
    jp nz, test_done
    ld a, (CTX + 2)
    or a
    jp nz, test_done
    ld a, 14
    ld (TEST_STATUS), a

    ; A manually completed JOIN+MQTT menu has ACTION visible but not defined.
    ; Switching to DIRECT without saved IP/port keeps GAME SETUP on screen and
    ; redraws the connection rows including the departed LINK marker.
    ld a, 1
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    xor a
    ld (_setup_focus_choice + 1), a
    ld hl, 0x01ff
    ld (_setup_defined_mask), hl
    ld hl, 0x03df
    ld (_setup_visible_mask), hl
    ld a, 1
    ld (_setup_cursor), a
    xor a
    ld (_setup_room_editing), a
    ld (_netchesszx_direct_host), a
    ld (_netchesszx_direct_port), a
    ld (_netchesszx_direct_port + 1), a
    call test_set_mqtt_room
    call test_paint_connection_focus
    ld a, 13
    ld (CTX), a
    call _setup_step_ovl_entry
    ld a, (_setup_choice + 1)
    or a
    jp nz, test_done
    ld hl, (_setup_defined_mask)
    ld a, h
    cp 1
    jp nz, test_done
    ld a, l
    cp 0xf3
    jp nz, test_done
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_done
    ld a, (CTX + 3)
    cp 0xfe
    jp nz, test_done
    ld a, (CTX + 1)
    cp 4
    jp nz, test_done
    ld hl, 0x03df
    ld (_setup_visible_mask), hl
    ld a, 0xaa
    ld (0x48f2), a
    ld (0x5012), a
    ld (0x5032), a
    ld hl, 0x5932
    ld de, 0x5933
    ld (hl), 0x05
    ld bc, 13
    ldir
    call test_prepare_render_context
    call test_reset_render_counts
    ld de, CTX
    call _menu_config_render_ovl_entry
    ld a, 0xff
    ld (CTX), a
    ld de, CTX
    call _input_edit_setup_line_ovl
    ld a, (0x4a1a)
    and 6
    jp nz, test_done
    ld a, (TEST_LINES)
    cp 4
    jp nz, test_done
    ld a, (TEST_GAME_HEADER)
    or a
    jp nz, test_done
    ld a, (0x48f2)
    cp 0xaa
    jp nz, test_done
    ld a, (0x5012)
    cp 0xaa
    jp nz, test_done
    ld a, (0x5032)
    cp 0xaa
    jp nz, test_done
    ld a, 15
    ld (TEST_STATUS), a

    ; Replacing the MQTT endpoint line must also discard every stale MQTT
    ; attribute. The DIRECT label is a header; the invalid/editing endpoint
    ; and the unused tail return to ordinary text colour.
    ld hl, 0x5932
    ld b, 3
test_endpoint_header_attrs:
    ld a, (hl)
    cp 0x03
    jp nz, test_done
    inc hl
    djnz test_endpoint_header_attrs
    ld b, 11
test_endpoint_text_attrs:
    ld a, (hl)
    cp 0x07
    jp nz, test_done
    inc hl
    djnz test_endpoint_text_attrs
    ld a, 16
    ld (TEST_STATUS), a

    ; DISCS uses the same selected cursor attribute as every other setup row.
    ; Painting attributes must not redraw or invert its text pixels.
    xor a
    ld (_setup_choice + 4), a
    ld (_setup_focus_choice + 4), a
    ld (_setup_room_editing), a
    ld (CTX + 4), a
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld (_setup_visible_mask), hl
    ld a, 7
    ld (_setup_cursor), a
    ld a, 0xff
    ld (CTX + 3), a
    ld hl, 0x0080
    ld (CTX + 1), hl
    call test_reset_render_counts
    call _menu_config_paint_attrs_ovl_entry
    ld hl, 0x5a16
    ld b, 2
test_discs_cursor_attrs:
    ld a, (hl)
    cp 0x46
    jp nz, test_done
    inc hl
    djnz test_discs_cursor_attrs
    ld a, (TEST_LINES)
    or a
    jp nz, test_done
    ld a, 17
    ld (TEST_STATUS), a
test_done:
    jp 0

test_reset_render_counts:
    xor a
    ld (TEST_LINES), a
    ld (TEST_GAME_HEADER), a
    ret

test_prepare_render_context:
    ; MENU_CONFIG receives its dirty mask in the first context word.
    ld hl, (CTX + 1)
    ld (CTX), hl
    xor a
    ld (CTX + 2), a
    ret

test_prepare_initial_mqtt_link:
    ld a, 1
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_focus_choice), a
    ld (_setup_focus_choice + 1), a
    ld hl, 0x0001
    ld (_setup_defined_mask), hl
    ld hl, 0x0003
    ld (_setup_visible_mask), hl
    ld (_setup_cursor), a
    xor a
    ld (_setup_room_editing), a
    ld (_setup_config_dirty), a
    ld (_setup_action_focus), a
    ld a, 13
    ld (CTX), a
    ret

; A = role. Leaves ENTER in the fixed overlay context.
test_prepare_mqtt_room:
    ld (_setup_choice), a
    ld a, 1
    ld (_setup_choice + 1), a
    ld hl, 0x0003
    ld (_setup_defined_mask), hl
    ld (_setup_room_editing), a
    ld a, 2
    ld (_setup_edit_row), a
    ld (_setup_cursor), a
    call test_set_mqtt_room
    ld hl, _netchesszx_mqtt_code + 2
    ld (_edit_buf), hl
    ld a, 4
    ld (_edit_max), a
    ld a, 13
    ld (CTX), a
    ret

; Draw the production compact marker before confirming GAME/LINK. The
; renderer stubs never erase pixels, so the old marker must be cleared by
; the real production attribute/marker pass after the reducer advances.
test_paint_connection_focus:
    xor a
    ld (CTX + 4), a
    ld hl, 3
    ld (CTX + 1), hl
    jp _menu_config_paint_attrs_ovl_entry

test_set_mqtt_room:
    ld a, 'M'
    ld (_netchesszx_mqtt_code), a
    ld a, 'S'
    ld (_netchesszx_mqtt_code + 1), a
    ld a, '1'
    ld (_netchesszx_mqtt_code + 2), a
    ld a, '2'
    ld (_netchesszx_mqtt_code + 3), a
    ld a, '3'
    ld (_netchesszx_mqtt_code + 4), a
    ld a, '4'
    ld (_netchesszx_mqtt_code + 5), a
    xor a
    ld (_netchesszx_mqtt_code + 6), a
    ret

test_prepare_direct_room:
    ld a, 1
    ld (_setup_choice), a
    xor a
    ld (_setup_choice + 1), a
    ld hl, 0x0003
    ld (_setup_defined_mask), hl
    inc a
    ld (_setup_room_editing), a
    ld a, 2
    ld (_setup_edit_row), a
    ld (_setup_cursor), a
    ld hl, _netchesszx_direct_host
    ld (_edit_buf), hl
    ld a, 15
    ld (_edit_max), a
    ld a, 13
    ld (CTX), a
    ret

test_confirm_result:
    ld a, (_setup_room_editing)
    or a
    ret nz
    ld a, (CTX + 6)
    cp 0x39
    ret nz
    ld a, (CTX + 5)
    cp 1
    ret

_spectrum_gui_edit_bind:
    ld (_edit_buf), hl
    ret

_spectrum_gui_edit_key:
    ld l, 0
    ret

_spectrum_gui_edit_hide:
    ret
_spectrum_gui_edit_show:
    ld (test_edit_geometry), hl
    ret
_spectrum_info_line:
    ld (test_line_ptr), hl
    ld a, (TEST_LINES)
    add a, 0x08
    ld e, a
    ld d, 0x70
    ld a, (hl)
    ld (de), a
    ld hl, TEST_LINES
    inc (hl)
    ret

_spectrum_info_show_game_setup:
    ld hl, TEST_GAME_HEADER
    inc (hl)
    ret

_spectrum_info_show_setup:
_spectrum_append_u16:
_netchess_mqtt_session_parse_u16_token:
    ret

_spectrum_overlay_context EQU CTX

_setup_choice:            DEFS 6
_setup_focus_choice:      DEFS 6
_setup_focus_board_theme: DEFS 1
_setup_defined_mask:      DEFS 2
_setup_visible_mask:      DEFS 2
_setup_cursor:            DEFS 1
_setup_room_editing:      DEFS 1
_setup_edit_row:          DEFS 1
_setup_port_text:         DEFS 6
_setup_config_dirty:      DEFS 1
_setup_action_focus:      DEFS 1
_setup_game_focus:        DEFS 1
_setup_edit_backup:       DEFS 17
_last_ip:                 DEFS 16
_netchesszx_mqtt_code:    DEFS 9
_netchesszx_direct_host:  DEFM "127.0.0.1"
                           DEFB 0
                           DEFS 6
_netchesszx_direct_port:  DEFS 2
_edit_buf:                DEFS 2
_edit_max:                DEFS 1
test_edit_geometry:       DEFS 2
test_line_ptr:            DEFS 2
test_row_ip:              DEFB 2
test_row_port:            DEFB 3
direct_ip_max:            DEFM "255.255.255.255"
                           DEFB 0
port_max:                 DEFM "65535"
                           DEFB 0
"""


def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )


def main() -> int:
    z80asm = shutil.which("z80asm") or "z80asm"
    ticks = shutil.which("z88dk-ticks") or "z88dk-ticks"
    setup = ROOT / "asm/overlay/setup/entry_setup.asm"
    menu_config = ROOT / "asm/overlay/menu_config/entry_menu_config.asm"
    input_edit = ROOT / "asm/overlay/input_edit/setup_edit_line.asm"
    input_edit_source = input_edit.read_text(encoding="ascii")
    if 'setup_edit_label_ip:\n    DEFB "IP    "' not in input_edit_source or (
        "setup_edit_copy_label:\n    ld b, 6" not in input_edit_source
    ):
        print("[ERR] DIRECT IP must start at column 43 so five port digits fit")
        return 1
    if "ld h, 59" not in input_edit_source:
        print("[ERR] DIRECT PORT editor must start after the fixed colon")
        return 1
    time_source = (ROOT / "asm/overlay/time_config/entry_time_config.asm").read_text(
        encoding="ascii"
    )
    if 'DEFB 10, "TIME  UTC "' not in time_source or "ld l, 10" not in time_source:
        print("[ERR] TIME must retain the compact CONNECTION SETUP layout")
        return 1
    app_source = (ROOT / "src/spectrum/app/app.c").read_text(encoding="utf-8")
    preflight = app_source.split(
        "static uint8_t connection_preflight_run(uint8_t quiet)", 1
    )[1].split("static void status_show_phase", 1)[0]
    if (
        "CLOCK WAIT" not in app_source
        or "CLOCK OK" not in app_source
        or "CLOCK FAIL" not in app_source
        or "preflight_clock_run()" not in preflight
    ):
        print("[ERR] Classic/Next preflight must keep CLOCK WAIT/OK/FAIL")
        return 1
    spectranext = preflight.split("NETCHESSZX_SPECTRANEXT", 1)
    if (
        len(spectranext) < 2
        or "preflight_clock_run()" in spectranext[1].split("#else", 1)[0]
        or "(void)clock_sync_run();" not in spectranext[1]
    ):
        print("[ERR] Spectranext must sync the clock without preflight CLOCK lines")
        return 1
    setup_step = app_source.split("static uint8_t session_setup_step", 1)[1].split(
        "static uint8_t session_setup_run", 1
    )[0]
    setup_call = app_source.rindex("config_state = session_setup_run(config_state);")
    before_setup = app_source[
        app_source.rindex("while (!connection_preflight_run", 0, setup_call) : setup_call
    ]
    after_setup = app_source[setup_call : setup_call + 250]
    if (
        "spectrum_link_clock_retry_start();" not in setup_step
        or "spectrum_link_clock_retry_start();" not in before_setup
        or "spectrum_link_clock_retry_cancel();" not in after_setup
    ):
        print("[ERR] failed clock sync must retry asynchronously while Setup runs")
        return 1
    disconnect = app_source.split("static void disconnect_to_setup(void)", 1)[
        1
    ].split("static uint8_t active_peer_ready", 1)[0]
    if (
        "#ifdef NETCHESSZX_SPECTRANEXT" not in disconnect
        or "else {" not in disconnect
        or "spectrum_link_start_uart();" not in disconnect
    ):
        print("[ERR] Spectranext DIRECT must close TCP before Setup/SNTP")
        return 1
    save_flow = app_source.split("static void session_setup_save(uint8_t key)", 1)[
        1
    ].split("static uint8_t session_setup_step", 1)[0]
    if (
        '"SAVE FAILED E0/00"' not in app_source
        or "rom_error = spxn_rom_error();" not in save_flow
        or save_flow.find("rom_error = spxn_rom_error();")
        > save_flow.find("session_setup_init(")
    ):
        print("[ERR] Spectranext save failure must snapshot stage/ROM error before reinit")
        return 1

    with tempfile.TemporaryDirectory(prefix="mirrorshift-setup-") as name:
        work = Path(name)
        source = work / "setup_flow_probe.asm"
        binary = work / "setup_flow_probe.bin"
        ram = work / "setup_flow_probe.ram"
        source.write_text(PROBE, encoding="ascii")

        result = run(
            [
                z80asm,
                "-b",
                "-r0x8000",
                "-O=.",
                "-o=setup_flow_probe.bin",
                source.name,
                str(setup),
                str(menu_config),
                str(input_edit),
            ],
            work,
        )
        if result.returncode or not binary.is_file():
            sys.stdout.write(result.stdout)
            print("[ERR] setup flow probe assembly failed")
            return 1

        result = run(
            [
                ticks,
                "-mz80",
                "-l",
                "0x8000",
                "-pc",
                "8000",
                "-end",
                "0",
                "-counter",
                "200000",
                "-output",
                ram.name,
                binary.name,
            ],
            work,
        )
        if result.returncode or not ram.is_file():
            sys.stdout.write(result.stdout)
            print("[ERR] setup flow probe execution failed")
            return 1
        memory = ram.read_bytes()
        if len(memory) < 65536 or memory[0x7000] != 17:
            checkpoint = memory[0x7000] if len(memory) >= 65536 else -1
            editing = memory[0x7001] if len(memory) >= 65536 else -1
            defined = memory[0x7004] if len(memory) >= 65536 else -1
            cursor = memory[0x7005] if len(memory) >= 65536 else -1
            clear_from = memory[0x7006] if len(memory) >= 65536 else -1
            force = memory[0x7007] if len(memory) >= 65536 else -1
            force_hi = memory[0x5ffa] if len(memory) >= 65536 else -1
            lines = memory[0x7002] if len(memory) >= 65536 else -1
            header = memory[0x7003] if len(memory) >= 65536 else -1
            rows = list(memory[0x7008:0x700c]) if len(memory) >= 65536 else []
            geometry = list(memory[0x7010:0x7015]) if len(memory) >= 65536 else []
            print(
                f"[ERR] setup flow regression: checkpoint={checkpoint}, "
                f"LINK->ROOM editing={editing}, defined={defined:#04x}, "
                f"cursor={cursor}, clear={clear_from:#04x}, "
                f"force={force:#04x}/{force_hi:#04x}, "
                f"lines={lines}, header={header}, rows={rows}, geometry={geometry}"
            )
            return 1

    screen = (ROOT / "asm/spectrum/screen.asm").read_text(encoding="utf-8")
    dispatch = screen[
        screen.index("_netchesszx_setup_dispatch_overlay:") :
        screen.index("_netchesszx_setup_time_ui:")
    ]
    if "setup_dispatch_menu_nav:" not in dispatch or \
            "SPECTRUM_OVL_MENU_CONFIG_NAV_PRIVATE" not in dispatch:
        print("[ERR] vertical setup navigation does not use MENU_CONFIG/NAV")
        return 1
    if "cp 9\n    jr nz, _netchesszx_setup_step_overlay\nsetup_dispatch_time:" not in dispatch:
        print("[ERR] ACTION input does not use NetChessZX TIME_CONFIG fallthrough")
        return 1

    print(
        "[OK] setup flow: CLOCK/preflight; compact IP:PORT geometry; "
        "selections; DIRECT/MQTT vertical navigation; TIME/ACTION refresh"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
