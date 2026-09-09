#!/usr/bin/env python3
"""Runtime check for the SETUP overlay's decimal port parser."""

import argparse
import shutil
import subprocess
from pathlib import Path


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
PUBLIC _netchesszx_mqtt_code
PUBLIC _netchesszx_direct_host
PUBLIC _netchesszx_direct_port
PUBLIC _spectrum_gui_edit_bind
PUBLIC _spectrum_gui_edit_key
PUBLIC _spectrum_gui_edit_hide
PUBLIC _spectrum_append_u16
PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _spectrum_net_send_text
PUBLIC _edit_buf
PUBLIC _edit_max

EXTERN su_parse_port
EXTERN su_compute_visible
EXTERN _setup_step_ovl_entry

TEST_STATUS EQU 0x7000
TEST_SP     EQU 0x7ff0
CTX_KEY     EQU 0x5ff8
CTX_FORCE_LO EQU 0x5ff9
CTX_FORCE_HI EQU 0x5ffa
CTX_CLEAR   EQU 0x5ffb
CTX_FLAGS   EQU 0x5ffe

test_start:
    ld sp, TEST_SP
    xor a
    ld (TEST_STATUS), a

    ld hl, text_65535
    call test_set_text
    ld de, 5000
    ld (_netchesszx_direct_port), de
    call su_parse_port
    jp z, test_done
    ld hl, (_netchesszx_direct_port)
    ld de, 65535
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 1
    ld (TEST_STATUS), a

    ld hl, text_65536
    call test_reject_unchanged
    jp nz, test_done
    ld a, 2
    ld (TEST_STATUS), a

    ld hl, text_99999
    call test_reject_unchanged
    jp nz, test_done
    ld a, 3
    ld (TEST_STATUS), a

    ld hl, text_zero
    call test_reject_unchanged
    jp nz, test_done
    ld a, 4
    ld (TEST_STATUS), a

    ld hl, text_one
    call test_set_text
    call su_parse_port
    jp z, test_done
    ld hl, (_netchesszx_direct_port)
    ld de, 1
    or a
    sbc hl, de
    jp nz, test_done
    ld hl, 0
    add hl, sp
    ld de, TEST_SP
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 5
    ld (TEST_STATUS), a

    ; A fully-defined JOIN/MQTT path must expose ACTION without SIDE.
    ld a, 1
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld bc, 0x01df
    call su_compute_visible
    ld de, 0x03df
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 6
    ld (TEST_STATUS), a

    ; Re-selecting an already configured CREATE/DIRECT path only advances
    ; focus: it must not dirty, clear or render the endpoint/time rows.
    xor a
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_focus_choice), a
    ld (_setup_focus_choice + 1), a
    ld (_setup_cursor), a
    ld (_setup_config_dirty), a
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld bc, 0x03ff
    call su_compute_visible
    ld (_setup_visible_mask), hl
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 1
    jp nz, test_done
    ld a, (CTX_FLAGS)
    cp 0x0a
    jp nz, test_done
    ld a, (CTX_CLEAR)
    inc a
    jp nz, test_done
    ld a, (_setup_config_dirty)
    or a
    jp nz, test_done

    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_done
    ld a, (CTX_FLAGS)
    cp 0x0a
    jp nz, test_done
    ld hl, (_setup_defined_mask)
    ld de, 0x03ff
    or a
    sbc hl, de
    jp nz, test_done
    ld a, (_setup_config_dirty)
    or a
    jp nz, test_done
    ld a, 7
    ld (TEST_STATUS), a

    ; Switching a complete CREATE/MQTT config to DIRECT keeps TIME/GAME SETUP,
    ; skips the fixed local IP and focuses the editable PORT control.
    xor a
    ld (_setup_choice), a
    ld (_setup_focus_choice), a
    ld (_setup_focus_choice + 1), a
    ld (_setup_room_editing), a
    ld (_setup_config_dirty), a
    ld hl, 0x0101
    ld (_setup_choice + 2), hl
    ld (_setup_focus_choice + 2), hl
    ld hl, 0x0102
    ld (_setup_choice + 4), hl
    ld (_setup_focus_choice + 4), hl
    ld a, 3
    ld (_setup_focus_board_theme), a
    ld a, 1
    ld (_setup_choice + 1), a
    ld (_setup_cursor), a
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld bc, 0x03ff
    call su_compute_visible
    ld (_setup_visible_mask), hl
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_done
    ld a, (_setup_room_editing)
    or a
    jp nz, test_done
    ld a, (CTX_FLAGS)
    cp 0x39
    jp nz, test_done
    ld a, (CTX_CLEAR)
    cp 0xfe
    jp nz, test_done
    ld a, (CTX_FORCE_LO)
    cp 4
    jp nz, test_done
    ld a, (CTX_FORCE_HI)
    or a
    jp nz, test_done
    ld hl, (_setup_defined_mask)
    ld de, 0x03ff
    or a
    sbc hl, de
    jp nz, test_done
    ld hl, (_setup_choice + 2)
    ld de, 0x0101
    or a
    sbc hl, de
    jp nz, test_done
    ld hl, (_setup_choice + 4)
    ld de, 0x0102
    or a
    sbc hl, de
    jp nz, test_done
    ld hl, (_setup_focus_choice + 2)
    ld de, 0x0101
    or a
    sbc hl, de
    jp nz, test_done
    ld hl, (_setup_focus_choice + 4)
    ld de, 0x0102
    or a
    sbc hl, de
    jp nz, test_done
    ld a, (_setup_focus_board_theme)
    cp 3
    jp nz, test_done
    ld bc, 0x03ff
    call su_compute_visible
    ld de, 0x03ff
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 0x81
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 1
    jp nz, test_done
    ld a, 0x82
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_done
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_done
    ld a, (_setup_edit_row)
    cp 3
    jp nz, test_done
    ld a, (_edit_max)
    cp 5
    jp nz, test_done
    ld a, 8
    ld (TEST_STATUS), a

    ; An empty JOIN/DIRECT host opens IP immediately; PORT remains the next
    ; independent control on the same physical line.
    ld a, 1
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_cursor), a
    xor a
    ld (_setup_focus_choice + 1), a
    ld (_setup_room_editing), a
    ld (_setup_config_dirty), a
    ld (_netchesszx_direct_host), a
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld bc, 0x03ff
    call su_compute_visible
    ld (_setup_visible_mask), hl
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_done
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_done
    ld a, (_setup_edit_row)
    cp 2
    jp nz, test_done
    ld a, (_edit_max)
    cp 15
    jp nz, test_done
    ; Full RENDER already repaints the editor; PAINT would reload both UI
    ; overlays a second time.
    ld a, (CTX_FLAGS)
    cp 0x3d
    jp nz, test_done
    ld a, (CTX_CLEAR)
    cp 0xfe
    jp nz, test_done
    ld a, (CTX_FORCE_LO)
    cp 4
    jp nz, test_done
    ld a, (CTX_FORCE_HI)
    or a
    jp nz, test_done
    ld hl, (_setup_defined_mask)
    ld de, 0x03f3
    or a
    sbc hl, de
    jp nz, test_done
    ld bc, 0x03f3
    call su_compute_visible
    ld de, 0x01df
    or a
    sbc hl, de
    jp nz, test_done
    ld hl, direct_ip
    ld de, _netchesszx_direct_host
    ld bc, 12
    ldir
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_done
    ld a, (_setup_room_editing)
    or a
    jp nz, test_done
    ld hl, (_setup_defined_mask)
    ld de, 0x03f7
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_done
    ld a, (_setup_edit_row)
    cp 3
    jp nz, test_done
    ld a, (_edit_max)
    cp 5
    jp nz, test_done
    ld a, 9
    ld (TEST_STATUS), a

    ; A complete JOIN/DIRECT -> MQTT transition always focuses the stored ROOM,
    ; even when it is valid; ENTER then opens the four-character suffix editor.
    call _spectrum_gui_edit_hide
    xor a
    ld (_setup_room_editing), a
    ld a, 1
    ld (_setup_choice), a
    xor a
    ld (_setup_choice + 1), a
    ld (_setup_focus_choice), a
    ld a, 1
    ld (_setup_focus_choice + 1), a
    ld (_setup_cursor), a
    ld hl, room_ms1234
    ld de, _netchesszx_mqtt_code
    ld bc, 7
    ldir
    ld hl, 0x03ff
    ld (_setup_defined_mask), hl
    ld bc, 0x03ff
    call su_compute_visible
    ld (_setup_visible_mask), hl
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_done
    ld a, (_setup_room_editing)
    or a
    jp nz, test_done
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_done
    ld a, (_setup_edit_row)
    cp 2
    jp nz, test_done
    ld a, (_edit_max)
    cp 4
    jp nz, test_done
    ld a, 10
    ld (TEST_STATUS), a

    ; The inline editor writes through its bound pointer, so CANCEL and a
    ; rejected room must restore the accepted endpoint, not leave its draft.
    ld hl, bad_room
    ld de, _netchesszx_mqtt_code
    ld bc, 7
    ldir
    ld a, 0x8a
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, room_ms1234
    call test_room_is_accepted
    jp nz, test_done
    ld a, 11
    ld (TEST_STATUS), a

    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, bad_room
    ld de, _netchesszx_mqtt_code
    ld bc, 7
    ldir
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_done
    ld hl, room_ms1234
    call test_room_is_accepted
    jp nz, test_done
    ld a, 12
    ld (TEST_STATUS), a

    ; JOIN/DIRECT treats its visible IP and PORT rows as one horizontal pair.
    xor a
    ld (_setup_room_editing), a
    ld a, 1
    ld (_setup_choice), a
    xor a
    ld (_setup_choice + 1), a
    ld a, 2
    ld (_setup_cursor), a
    ld hl, 0x000c
    ld (_setup_visible_mask), hl
    ld a, 0x83
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 3
    jp nz, test_done
    ld a, (_setup_room_editing)
    or a
    jp nz, test_done
    ld a, (CTX_FLAGS)
    and 0x12
    cp 0x02
    jp nz, test_done
    ld hl, (CTX_FORCE_LO)
    ld de, 0x000c
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 0x84
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 2
    jp nz, test_done
    ld a, (_setup_room_editing)
    or a
    jp nz, test_done
    ld a, (CTX_FLAGS)
    and 0x12
    cp 0x02
    jp nz, test_done
    ld hl, (CTX_FORCE_LO)
    ld de, 0x000c
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 13
    ld (TEST_STATUS), a

    ; DIRECT IP uses the separate 16-byte resident backup on cancel/reject.
    ld hl, direct_ip_15
    ld de, _netchesszx_direct_host
    ld bc, 16
    ldir
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, bad_ip
    ld de, _netchesszx_direct_host
    ld bc, 16
    ldir
    ld a, 0x8a
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, direct_ip_15
    call test_host_is_accepted
    jp nz, test_done
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld hl, bad_ip
    ld de, _netchesszx_direct_host
    ld bc, 16
    ldir
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, test_done
    ld hl, direct_ip_15
    call test_host_is_accepted
    jp nz, test_done
    ld a, 14
    ld (TEST_STATUS), a

    ; Entering ACTION must route ACTION_UI only: never drag TIME_UI along.
    xor a
    ld (_setup_room_editing), a
    ld a, 8
    ld (_setup_cursor), a
    ld hl, 0x0300
    ld (_setup_visible_mask), hl
    ld a, 0x82
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, (_setup_cursor)
    cp 9
    jp nz, test_done
    ld a, (CTX_FLAGS)
    and 0x32
    cp 0x22
    jp nz, test_done
    ld hl, (CTX_FORCE_LO)
    ld de, 0x0300
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 15
    ld (TEST_STATUS), a

    ; A completed menu stays complete while an invalid endpoint hides ACTION.
    ; Reproduce MQTT -> invalid DIRECT -> cancel -> MQTT using the actual mask.
    ld a, 1
    ld (_setup_choice), a
    ld (_setup_choice + 1), a
    ld (_setup_cursor), a
    xor a
    ld (_setup_focus_choice + 1), a
    ld (_setup_room_editing), a
    ld (_netchesszx_direct_host), a
    ld hl, 0x01df
    ld (_setup_defined_mask), hl
    ld bc, 0x01df
    call su_compute_visible
    ld (_setup_visible_mask), hl
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld bc, (_setup_defined_mask)
    call su_compute_visible
    ld (_setup_visible_mask), hl
    ld de, 0x01df
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 0x8a
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld a, 1
    ld (_setup_cursor), a
    ld (_setup_focus_choice + 1), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld bc, (_setup_defined_mask)
    call su_compute_visible
    ld de, 0x03df
    or a
    sbc hl, de
    jp nz, test_done
    ld a, (CTX_CLEAR)
    cp 0xfe
    jp nz, test_done
    ld a, 16
    ld (TEST_STATUS), a

    ; JOIN never defined SIDE. CREATE reveals it and hides ACTION until SIDE
    ; is accepted, but switching back must retain every completed game row.
    ld bc, (_setup_defined_mask)
    call su_compute_visible
    ld (_setup_visible_mask), hl
    xor a
    ld (_setup_cursor), a
    ld (_setup_focus_choice), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld bc, (_setup_defined_mask)
    call su_compute_visible
    ld (_setup_visible_mask), hl
    ld de, 0x01ff
    or a
    sbc hl, de
    jp nz, test_done
    xor a
    ld (_setup_cursor), a
    inc a
    ld (_setup_focus_choice), a
    ld a, 13
    ld (CTX_KEY), a
    call _setup_step_ovl_entry
    ld bc, (_setup_defined_mask)
    call su_compute_visible
    ld de, 0x03df
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 17
    ld (TEST_STATUS), a

test_done:
    jp 0

; HL = six-byte, NUL-padded test input.
test_set_text:
    ld de, _setup_port_text
    ld bc, 6
    ldir
    ret

; HL = rejected input. Returns Z only when parsing fails and the old port stays.
test_reject_unchanged:
    call test_set_text
    ld de, 5000
    ld (_netchesszx_direct_port), de
    call su_parse_port
    jr nz, test_reject_fail
    ld hl, (_netchesszx_direct_port)
    ld de, 5000
    or a
    sbc hl, de
    ret
test_reject_fail:
    ld a, 1
    or a
    ret

; HL = expected seven-byte room, returns Z on exact match.
test_room_is_accepted:
    ld de, _netchesszx_mqtt_code
    ld b, 7
test_room_compare:
    ld a, (de)
    cp (hl)
    ret nz
    inc de
    inc hl
    djnz test_room_compare
    xor a
    ret

; HL = expected 16-byte direct host, returns Z on exact match.
test_host_is_accepted:
    ld de, _netchesszx_direct_host
    ld b, 16
test_host_compare:
    ld a, (de)
    cp (hl)
    ret nz
    inc de
    inc hl
    djnz test_host_compare
    xor a
    ret

text_65535: DEFM "65535"
            DEFB 0
text_65536: DEFM "65536"
            DEFB 0
text_99999: DEFM "99999"
            DEFB 0
text_zero:  DEFB '0', 0, 0, 0, 0, 0
text_one:   DEFB '1', 0, 0, 0, 0, 0
room_ms1234: DEFM "MS1234"
             DEFB 0
bad_room:    DEFM "MSZZZZ"
             DEFB 0
direct_ip:   DEFM "192.168.1.1"
             DEFB 0
direct_ip_15: DEFM "123.123.123.123"
              DEFB 0
bad_ip:      DEFM "999.999.999.999"
             DEFB 0

_setup_choice:             DEFS 6
_setup_focus_choice:       DEFS 6
_setup_focus_board_theme:  DEFS 1
_setup_defined_mask:       DEFS 2
_setup_visible_mask:       DEFS 2
_setup_cursor:             DEFS 1
_setup_room_editing:       DEFS 1
_setup_edit_row:           DEFS 1
_setup_port_text:          DEFS 6
_setup_config_dirty:       DEFS 1
_setup_action_focus:       DEFS 1
_setup_game_focus:         DEFS 1
_setup_edit_backup:        DEFS 17
_netchesszx_mqtt_code:     DEFS 17
_netchesszx_direct_host:   DEFS 16
_netchesszx_direct_port:   DEFS 2
_edit_buf:                 DEFS 2
_edit_max:                 DEFS 1

_line_buf:
_NETCHESS_PROTO_NACK_PREFIX:
_spectrum_net_payload_scratch:
    DEFB 0
_spectrum_net_send_text:
    ret

_spectrum_gui_edit_bind:
    ld (_edit_buf), hl
    ret
_spectrum_gui_edit_key:
_spectrum_gui_edit_hide:
_spectrum_append_u16:
    ret
"""

SIZE_STUBS = r"""
SECTION code_user
PUBLIC _setup_choice, _setup_focus_choice, _setup_focus_board_theme
PUBLIC _setup_defined_mask, _setup_visible_mask, _setup_cursor, _setup_room_editing
PUBLIC _setup_edit_row, _setup_port_text, _setup_config_dirty, _setup_action_focus
PUBLIC _setup_game_focus, _setup_edit_backup, _netchesszx_mqtt_code
PUBLIC _netchesszx_direct_host, _netchesszx_direct_port, _spectrum_gui_edit_hide
PUBLIC _spectrum_append_u16
PUBLIC _netchess_mqtt_session_parse_u16_token, _edit_buf, _edit_max
PUBLIC _edit_len, _edit_pos
PUBLIC _NETCHESS_PROTO_NACK_PREFIX, _spectrum_net_payload_scratch
PUBLIC _spectrum_net_send_text, _line_buf
_setup_choice:
_setup_focus_choice:
_setup_focus_board_theme:
_setup_defined_mask:
_setup_visible_mask:
_setup_cursor:
_setup_room_editing:
_setup_edit_row:
_setup_port_text:
_setup_config_dirty:
_setup_action_focus:
_setup_game_focus:
_setup_edit_backup:
_netchesszx_mqtt_code:
_netchesszx_direct_host:
_netchesszx_direct_port:
_spectrum_gui_edit_hide:
_spectrum_append_u16:
_netchess_mqtt_session_parse_u16_token:
_edit_buf:
_edit_max:
_edit_len:
_edit_pos:
_NETCHESS_PROTO_NACK_PREFIX:
_spectrum_net_payload_scratch:
_spectrum_net_send_text:
_line_buf:
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--build-dir", default="build")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    work = Path(args.build_dir).resolve() / "setup_port_probe"
    work.mkdir(parents=True, exist_ok=True)
    probe = work / "setup_port_probe.asm"
    kernel = work / "entry_setup_probe.asm"
    stubs = work / "setup_size_stubs.asm"
    binary = work / "setup_port_probe.bin"
    ram = work / "setup_port_probe.ram"
    overlay = work / "setup.bin"
    probe.write_text(PROBE, encoding="ascii")
    stubs.write_text(SIZE_STUBS, encoding="ascii")
    production = (root / "asm" / "overlay" / "setup" / "entry_setup.asm").read_text(
        encoding="utf-8"
    )
    production = production.replace(
        "PUBLIC _setup_step_ovl_entry",
        "PUBLIC _setup_step_ovl_entry\nPUBLIC su_parse_port\nPUBLIC su_compute_visible",
        1,
    )
    kernel.write_text(production, encoding="utf-8")
    for old in (binary, ram, overlay):
        old.unlink(missing_ok=True)

    z80asm = shutil.which("z80asm") or "z80asm"
    ticks = shutil.which("z88dk-ticks") or "z88dk-ticks"
    u16_kernel = root / "asm" / "spectrum" / "shrink_kernels.asm"
    edit_kernel = root / "asm" / "overlay" / "edit" / "edit_buf.asm"
    result = subprocess.run(
        [z80asm, "-DNETCHESSZX_SDCC_IY", "-b", "-r0x6800", "-O=.",
         "-o=" + overlay.name, kernel.name, str(edit_kernel), stubs.name],
        cwd=work, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode or not overlay.exists() or overlay.stat().st_size > 2048:
        print(result.stdout, end="")
        size = overlay.stat().st_size if overlay.exists() else 0
        print(f"[ERR] SETUP standalone overlay size link failed ({size} bytes)")
        return 1
    result = subprocess.run(
        [
            z80asm,
            "-b",
            "-r0x8000",
            "-O=.",
            "-o=" + binary.name,
            probe.name,
            kernel.name,
            str(u16_kernel),
        ],
        cwd=work,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode:
        print(result.stdout, end="")
        print("[ERR] SETUP port probe assembly failed")
        return 1
    result = subprocess.run(
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
            "100000",
            "-output",
            ram.name,
            binary.name,
        ],
        cwd=work,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode or not ram.exists():
        print(result.stdout, end="")
        print("[ERR] SETUP port probe execution failed")
        return 1
    image = ram.read_bytes()
    step = image[0x7000] if len(image) >= 65536 else 0
    if step != 17:
        print(f"[ERR] SETUP port runtime failed after checkpoint {step}")
        return 1
    print(f"[OK] SETUP port/SP/action; linked bytes={overlay.stat().st_size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
