#!/usr/bin/env python3
"""Runtime contract for TIME_CONFIG's isolated setup paths."""

import shutil
import subprocess
from pathlib import Path


PROBE = r'''
SECTION code_user
PUBLIC _spectrum_info_line
PUBLIC _spectrum_gui_edit_bind
PUBLIC _spectrum_gui_edit_key
PUBLIC _spectrum_gui_edit_hide
PUBLIC _spectrum_gui_edit_show
PUBLIC _spectrum_append_u16
PUBLIC _spectrum_net_runtime_set_fat_stamp
PUBLIC _setup_choice
PUBLIC _setup_focus_choice
PUBLIC _setup_focus_board_theme
PUBLIC _setup_defined_mask
PUBLIC _setup_visible_mask
PUBLIC _setup_cursor
PUBLIC _setup_room_editing
PUBLIC _setup_edit_row
PUBLIC _setup_timezone_text
PUBLIC _setup_timezone_value
PUBLIC _setup_config_dirty
PUBLIC _setup_time_focus
PUBLIC _setup_action_focus
PUBLIC _setup_game_focus
PUBLIC _setup_port_text
PUBLIC _netchesszx_session_role
PUBLIC _netchesszx_transport
PUBLIC _netchesszx_local_color
PUBLIC _netchesszx_host_color
PUBLIC _netchesszx_host_color_ready
PUBLIC _netchesszx_movement_hints
PUBLIC _netchesszx_board_theme_index
PUBLIC _netchesszx_piece_set_index
PUBLIC _netchesszx_timezone
PUBLIC _netchesszx_timezone_last
PUBLIC _netchesszx_rtc_available
PUBLIC _netchesszx_direct_port
PUBLIC _edit_buf
PUBLIC _edit_max

EXTERN _time_config_step_ovl_entry
EXTERN _time_config_ui_ovl_entry
EXTERN tc_rtc_ok
EXTERN tc_rtc_decode_msdos
EXTERN tc_rtc_pcf_validate

CTX EQU 0x5ff8
STATUS EQU 0x7000
LINES EQU 0x7001
MARK EQU 0x7002
FAT_CALLS EQU 0x7003
FAT_DATE EQU 0x7005
FAT_TIME EQU 0x7007
SP0 EQU 0x7ff0

test_start:
    ld sp, SP0
    xor a
    ld (STATUS), a
    ld (_setup_cursor), a
    ld a, 4
    ld (_setup_cursor), a
    ld a, 1
    ld (_netchesszx_rtc_available), a
    ld (_setup_timezone_value), a
    ld (_setup_time_focus), a
    ld a, 0x84
    ld (CTX), a
    call _time_config_step_ovl_entry
    ld a, (_setup_time_focus)
    or a
    jp z, done
    ld a, (CTX + 6)
    cp 0x10
    jp nz, done
    ld a, 1
    ld (STATUS), a

    ld a, 13
    ld (CTX), a
    call _time_config_step_ovl_entry
    ld a, (_setup_room_editing)
    cp 1
    jp nz, done
    ld a, (CTX + 6)
    cp 0x1c
    jp nz, done
    ld a, 2
    ld (STATUS), a

    ld hl, bad_time
    ld de, _setup_timezone_text
    ld bc, 4
    ldir
    ld a, 13
    ld (CTX), a
    call _time_config_step_ovl_entry
    ld a, (_setup_timezone_value)
    cp 1
    jp nz, done
    ld a, (_setup_room_editing)
    cp 1
    jp nz, done
    ld a, 3
    ld (STATUS), a

    ld a, 0x8a
    ld (CTX), a
    call _time_config_step_ovl_entry
    ld a, (_setup_room_editing)
    or a
    jp nz, done
    ld a, (_setup_timezone_value)
    cp 1
    jp nz, done
    ld a, 4
    ld (STATUS), a

    ld a, 13
    ld (CTX), a
    call _time_config_step_ovl_entry
    ld hl, good_time
    ld de, _setup_timezone_text
    ld bc, 3
    ldir
    ld a, 13
    ld (CTX), a
    call _time_config_step_ovl_entry
    ld a, (CTX + 4)
    cp 5
    jp nz, done
    ld a, (_setup_config_dirty)
    cp 1
    jp nz, done
    ld a, (CTX + 6)
    cp 0x39
    jp nz, done
    ld a, 5
    ld (STATUS), a

    ; TIME-only routing emits row 10 and carries the focused marker glyph.
    ld a, 4
    ld (_setup_cursor), a
    ld a, 1
    ld (_setup_time_focus), a
    ld a, 0x10
    ld (CTX + 6), a
    ld a, 0x10
    ld (_setup_visible_mask), a
    xor a
    ld (_setup_visible_mask + 1), a
    ld (LINES), a
    call _time_config_ui_ovl_entry
    ld a, (LINES)
    cp 1
    jp nz, done
    ld a, (MARK)
    cp 92
    jp nz, done
    ld a, (0x7100 + 6)
    cp 'R'
    jp nz, done
    ld a, (0x7100 + 16)
    cp 'U'
    jp nz, done
    ld a, 6
    ld (STATUS), a

    ; ACTION-only routing emits only the bottom row.
    ld a, 0x20
    ld (CTX + 6), a
    ld a, 2
    ld (_setup_visible_mask + 1), a
    xor a
    ld (LINES), a
    call _time_config_ui_ovl_entry
    ld a, (LINES)
    cp 1
    jp nz, done
    ld a, (MARK)
    cp 20
    jp nz, done
    ld a, 7
    ld (STATUS), a

    ; RTC-less UTC stays on the first option column; editing targets the
    ; digits after the sign, including the second digit of +13.
    xor a
    ld (_netchesszx_rtc_available), a
    ld a, 4
    ld (_setup_cursor), a
    ld (_setup_edit_row), a
    ld a, 1
    ld (_setup_room_editing), a
    ld a, 0x10
    ld (CTX + 6), a
    call _time_config_ui_ovl_entry
    ld a, (0x7100 + 6)
    cp 'U'
    jp nz, done
    ld a, (0x7004)
    cp 48
    jp nz, done
    ld a, (0x5955)
    cp 0x47
    jp nz, done
    ld a, 1
    ld (_netchesszx_rtc_available), a
    call _time_config_ui_ovl_entry
    ld a, (0x7004)
    cp 58
    jp nz, done
    ; SAVE and START keep their text fixed and use ink-only focus.
    ld a, 9
    ld (_setup_cursor), a
    xor a
    ld (_setup_action_focus), a
    ld a, 0x20
    ld (CTX + 6), a
    call _time_config_ui_ovl_entry
    ld a, (0x7100 + 13)
    cp ' '
    jp nz, done
    ld a, (0x5a99)
    cp 0x38
    jp nz, done
    ld a, 1
    ld (_setup_action_focus), a
    call _time_config_ui_ovl_entry
    ld a, (0x7100 + 13)
    cp ' '
    jp nz, done
    ld a, (0x7100 + 21)
    cp ' '
    jp nz, done
    ld a, (0x5a9d)
    cp 0x38
    jp nz, done
    ld a, 8
    ld (STATUS), a

    ; The documented esxDOS helpers return packed MS-DOS date/time in BC/DE.
    ; Decode one leap-day value, publish it, and prove publishing preserves HMS.
    xor a
    ld (FAT_CALLS), a
    ld bc, 0x585d             ; 2024-02-29
    ld de, 0xbf7d             ; 23:59:58
    call tc_rtc_decode_msdos
    jp c, done
    ld a, (CTX)
    cp 58
    jp nz, done
    ld a, (CTX + 1)
    cp 59
    jp nz, done
    ld a, (CTX + 2)
    cp 23
    jp nz, done
    call tc_rtc_ok
    ld a, (FAT_CALLS)
    cp 1
    jp nz, done
    ld hl, (FAT_DATE)
    ld de, 0x585d
    or a
    sbc hl, de
    jp nz, done
    ld hl, (FAT_TIME)
    ld de, 0xbf7d
    or a
    sbc hl, de
    jp nz, done
    ld a, (CTX)
    cp 58
    jp nz, done
    ld a, (CTX + 1)
    cp 59
    jp nz, done
    ld a, (CTX + 2)
    cp 23
    jp nz, done
    ld a, 9
    ld (STATUS), a

    ; Direct PCF8563 bytes use BCD. Validate and pack the same instant.
    ld hl, valid_pcf
    ld de, CTX
    ld bc, 7
    ldir
    xor a
    ld (FAT_CALLS), a
    call tc_rtc_pcf_validate
    jp c, done
    call tc_rtc_ok
    ld a, (FAT_CALLS)
    cp 1
    jp nz, done
    ld hl, (FAT_DATE)
    ld de, 0x585d
    or a
    sbc hl, de
    jp nz, done
    ld hl, (FAT_TIME)
    ld de, 0xbf7d
    or a
    sbc hl, de
    jp nz, done
    ld a, (CTX)
    cp 58
    jp nz, done
    ld a, (CTX + 1)
    cp 59
    jp nz, done
    ld a, (CTX + 2)
    cp 23
    jp nz, done
    ld a, 10
    ld (STATUS), a

    ; Invalid MS-DOS seconds and a low-voltage PCF sample must not publish.
    ld hl, 0xa55a
    ld (FAT_DATE), hl
    ld hl, 0x5aa5
    ld (FAT_TIME), hl
    xor a
    ld (FAT_CALLS), a
    ld bc, 0x585d
    ld de, 0xbf7f             ; encoded seconds 62
    call tc_rtc_decode_msdos
    jp nc, done
    ld hl, invalid_pcf
    ld de, CTX
    ld bc, 7
    ldir
    call tc_rtc_pcf_validate
    jp nc, done
    ld a, (FAT_CALLS)
    or a
    jp nz, done
    ld hl, (FAT_DATE)
    ld de, 0xa55a
    or a
    sbc hl, de
    jp nz, done
    ld hl, (FAT_TIME)
    ld de, 0x5aa5
    or a
    sbc hl, de
    jp nz, done
    ld a, 11
    ld (STATUS), a
done:
    jp 0

bad_time: DEFM "+99"
          DEFB 0
good_time: DEFM "+2"
           DEFB 0
valid_pcf: DEFB 0x58, 0x59, 0x23, 0x29, 0, 0x02, 0x24
invalid_pcf: DEFB 0x80, 0x59, 0x23, 0x29, 0, 0x02, 0x24
_setup_choice: DEFS 6
_setup_focus_choice: DEFS 6
_setup_focus_board_theme: DEFS 1
_setup_defined_mask: DEFS 2
_setup_visible_mask: DEFS 2
_setup_cursor: DEFS 1
_setup_room_editing: DEFS 1
_setup_edit_row: DEFS 1
_setup_timezone_text: DEFS 4
_setup_timezone_value: DEFS 1
_setup_config_dirty: DEFS 1
_setup_time_focus: DEFS 1
_setup_action_focus: DEFS 1
_setup_game_focus: DEFS 1
_setup_port_text: DEFS 6
_netchesszx_session_role: DEFS 1
_netchesszx_transport: DEFS 1
_netchesszx_local_color: DEFS 1
_netchesszx_host_color: DEFS 1
_netchesszx_host_color_ready: DEFS 1
_netchesszx_movement_hints: DEFS 1
_netchesszx_board_theme_index: DEFS 1
_netchesszx_piece_set_index: DEFS 1
_netchesszx_timezone: DEFS 1
_netchesszx_timezone_last: DEFS 1
_netchesszx_rtc_available: DEFS 1
_netchesszx_direct_port: DEFS 2
_edit_buf: DEFS 2
_edit_max: DEFS 1
_spectrum_gui_edit_bind: ld (_edit_buf), hl
                         ret
_spectrum_gui_edit_key: ret
_spectrum_gui_edit_hide: ret
_spectrum_gui_edit_show: ld a, h
                         ld (0x7004), a
                         ret
_spectrum_append_u16: ret
_spectrum_net_runtime_set_fat_stamp:
    pop hl
    pop bc
    pop de
    ld (FAT_DATE), bc
    ld (FAT_TIME), de
    ld a, (FAT_CALLS)
    inc a
    ld (FAT_CALLS), a
    push de
    push bc
    push hl
    ret
_spectrum_info_line:
    push hl
    inc hl
    ld de, 0x7100
    ld bc, 27
    ldir
    pop hl
    ld a, (hl)
    ld (MARK), a
    ld a, (LINES)
    inc a
    ld (LINES), a
    ld a, (hl)
    cp 10
    ret nz
    ld de, 16
    add hl, de
    ld a, (hl)
    ld (MARK), a
    ret
'''

SIZE_STUBS = r'''
SECTION code_user
PUBLIC _spectrum_info_line, _spectrum_gui_edit_hide, _spectrum_gui_edit_show
PUBLIC _spectrum_append_u16
PUBLIC _setup_choice, _setup_focus_choice, _setup_focus_board_theme
PUBLIC _setup_defined_mask, _setup_visible_mask, _setup_cursor, _setup_room_editing
PUBLIC _setup_edit_row, _setup_timezone_text, _setup_timezone_value
PUBLIC _setup_config_dirty, _setup_time_focus, _setup_action_focus, _setup_game_focus
PUBLIC _setup_port_text
PUBLIC _netchesszx_session_role, _netchesszx_transport, _netchesszx_local_color
PUBLIC _netchesszx_host_color, _netchesszx_host_color_ready, _netchesszx_movement_hints
PUBLIC _netchesszx_board_theme_index, _netchesszx_piece_set_index, _netchesszx_timezone
PUBLIC _netchesszx_timezone_last, _netchesszx_rtc_available, _netchesszx_direct_port
PUBLIC _edit_buf, _edit_max
PUBLIC _edit_len, _edit_pos
PUBLIC _spectrum_net_runtime_set_fat_stamp
_spectrum_info_line:
_spectrum_gui_edit_hide:
_spectrum_gui_edit_show:
_spectrum_append_u16:
_setup_choice:
_setup_focus_choice:
_setup_focus_board_theme:
_setup_defined_mask:
_setup_visible_mask:
_setup_cursor:
_setup_room_editing:
_setup_edit_row:
_setup_timezone_text:
_setup_timezone_value:
_setup_config_dirty:
_setup_time_focus:
_setup_action_focus:
_setup_game_focus:
_setup_port_text:
_netchesszx_session_role:
_netchesszx_transport:
_netchesszx_local_color:
_netchesszx_host_color:
_netchesszx_host_color_ready:
_netchesszx_movement_hints:
_netchesszx_board_theme_index:
_netchesszx_piece_set_index:
_netchesszx_timezone:
_netchesszx_timezone_last:
_netchesszx_rtc_available:
_netchesszx_direct_port:
_edit_buf:
_edit_max:
_edit_len:
_edit_pos:
_spectrum_net_runtime_set_fat_stamp:
    ret
'''


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    work = root / "build" / "setup_time_probe"
    work.mkdir(parents=True, exist_ok=True)
    probe, kernel, stubs = work / "probe.asm", work / "time.asm", work / "stubs.asm"
    binary, ram, overlay = work / "probe.bin", work / "probe.ram", work / "time.bin"
    probe.write_text(PROBE, encoding="ascii")
    kernel_source = (root / "asm/overlay/time_config/entry_time_config.asm").read_text()
    kernel.write_text(kernel_source.replace(
        "SECTION code_user\n",
        "SECTION code_user\n"
        "PUBLIC tc_rtc_ok, tc_rtc_decode_msdos, tc_rtc_pcf_validate\n",
        1,
    ))
    stubs.write_text(SIZE_STUBS, encoding="ascii")
    for path in (binary, ram, overlay):
        path.unlink(missing_ok=True)
    z80asm = shutil.which("z80asm") or "z80asm"
    ticks = shutil.which("z88dk-ticks") or "z88dk-ticks"
    edit_kernel = root / "asm" / "overlay" / "edit" / "edit_buf.asm"
    size = subprocess.run([z80asm, "-DNETCHESSZX_SDCC_IY", "-b", "-r0x6800", "-O=.",
                           "-o=" + overlay.name, kernel.name, str(edit_kernel), stubs.name], cwd=work, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if size.returncode or not overlay.is_file() or overlay.stat().st_size > 2048:
        print(size.stdout, end="")
        print("[ERR] TIME_CONFIG standalone overlay size link failed")
        return 1
    build = subprocess.run([z80asm, "-b", "-r0x8000", "-O=.", "-o=" + binary.name,
                            probe.name, kernel.name], cwd=work, text=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if build.returncode:
        print(build.stdout, end="")
        return 1
    run = subprocess.run([ticks, "-mz80", "-l", "0x8000", "-pc", "8000", "-end", "0",
                          "-counter", "100000", "-output", ram.name, binary.name], cwd=work,
                         text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    image = ram.read_bytes() if ram.exists() else b""
    if run.returncode or len(image) < 65536 or image[0x7000] != 11:
        print(run.stdout, end="")
        status = image[0x7000] if len(image) >= 65536 else -1
        print(f"[ERR] TIME_CONFIG runtime failed after checkpoint {status}")
        return 1
    print(f"[OK] TIME_CONFIG edit/action/UI; linked bytes={overlay.stat().st_size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
