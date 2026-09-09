#!/usr/bin/env python3
"""Check the SDCC/IY assumptions used by handwritten Z80 stubs."""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


PROBE_SOURCE = r"""
#include <stdint.h>

extern uint8_t abi_pair(uint8_t first, uint8_t second);

uint8_t abi_probe(void)
{
    return abi_pair(0x12u, 0x34u);
}
"""

COPY_TOKEN_PROBE_SOURCE = r"""
SECTION code_user

PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _spectrum_net_send_text

EXTERN _netchesszx_asm_proto_copy_token
EXTERN _netchesszx_asm_move_tail_ok

TEST_STATUS EQU 0x7000
TEST_PTR    EQU 0x7001
TEST_OUT    EQU 0x7010
TEST_INPUT  EQU 0x7020
TEST_SP     EQU 0x7ff0

test_start:
    ld sp, TEST_SP
    xor a
    ld (TEST_STATUS), a
    ld a, 'd'
    ld (TEST_INPUT), a
    ld a, '3'
    ld (TEST_INPUT + 1), a
    xor a
    ld (TEST_INPUT + 2), a
    ld hl, TEST_INPUT
    ld (TEST_PTR), hl

    ld hl, 3
    push hl
    ld hl, TEST_OUT
    push hl
    ld hl, TEST_PTR
    push hl
    call _netchesszx_asm_proto_copy_token
    pop bc
    pop bc
    pop bc

    ld a, l
    cp 2
    jr nz, test_done
    ld hl, (TEST_PTR)
    ld de, TEST_INPUT + 2
    or a
    sbc hl, de
    jr nz, test_done
    ld a, (TEST_OUT)
    cp 'd'
    jr nz, test_done
    ld a, (TEST_OUT + 1)
    cp '3'
    jr nz, test_done
    ld a, (TEST_OUT + 2)
    or a
    jr nz, test_done

    ld a, ' '
    ld (TEST_INPUT), a
    ld a, 'x'
    ld (TEST_INPUT + 1), a
    ld a, '4'
    ld (TEST_INPUT + 2), a
    xor a
    ld (TEST_INPUT + 3), a
    ld hl, TEST_INPUT
    call _netchesszx_asm_move_tail_ok
    dec l
    jr nz, test_done

    ld a, ' '
    ld (TEST_INPUT + 3), a
    ld a, 'j'
    ld (TEST_INPUT + 4), a
    xor a
    ld (TEST_INPUT + 5), a
    ld hl, TEST_INPUT
    call _netchesszx_asm_move_tail_ok
    ld a, l
    or a
    jr nz, test_done

    xor a
    ld (TEST_INPUT + 1), a
    ld hl, TEST_INPUT
    call _netchesszx_asm_move_tail_ok
    ld a, l
    or a
    jr nz, test_done
    ld hl, 0
    add hl, sp
    ld de, TEST_SP
    or a
    sbc hl, de
    jr nz, test_done
    ld a, 1
    ld (TEST_STATUS), a

test_done:
    jp 0

_line_buf:
    DEFS 1
_NETCHESS_PROTO_ACK_PREFIX:
    DEFM "ACK "
    DEFB 0
_NETCHESS_PROTO_NACK_PREFIX:
    DEFM "NACK "
    DEFB 0
_spectrum_net_payload_scratch:
    DEFS 256
_spectrum_net_send_text:
    ld l, 1
    ret
"""

TIMER_TICK_PROBE_SOURCE = r"""
SECTION code_user

PUBLIC _line_buf
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX
PUBLIC _spectrum_net_payload_scratch
PUBLIC _spectrum_net_send_text

EXTERN _netchesszx_asm_timer_tick_one_second

TEST_STATUS EQU 0x7000
TEST_HOUR   EQU 0x7001
TEST_MINUTE EQU 0x7002
TEST_SECOND EQU 0x7003
TEST_SP     EQU 0x7ff0

test_start:
    ld sp, TEST_SP
    xor a
    ld (TEST_STATUS), a

    ld a, 99
    ld (TEST_HOUR), a
    ld a, 59
    ld (TEST_MINUTE), a
    xor a
    ld (TEST_SECOND), a
    call test_tick
    ld a, (TEST_HOUR)
    cp 99
    jp nz, test_done
    ld a, (TEST_MINUTE)
    cp 59
    jp nz, test_done
    ld a, (TEST_SECOND)
    cp 1
    jp nz, test_done

    ld a, 58
    ld (TEST_SECOND), a
    call test_tick
    ld a, (TEST_SECOND)
    cp 59
    jp nz, test_done

    call test_tick
    ld a, (TEST_HOUR)
    cp 99
    jp nz, test_done
    ld a, (TEST_MINUTE)
    cp 59
    jp nz, test_done
    ld a, (TEST_SECOND)
    cp 59
    jp nz, test_done

    ld a, 98
    ld (TEST_HOUR), a
    ld a, 59
    ld (TEST_MINUTE), a
    ld (TEST_SECOND), a
    call test_tick
    ld a, (TEST_HOUR)
    cp 99
    jp nz, test_done
    ld a, (TEST_MINUTE)
    or a
    jp nz, test_done
    ld a, (TEST_SECOND)
    or a
    jp nz, test_done

    ld hl, 0
    add hl, sp
    ld de, TEST_SP
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 1
    ld (TEST_STATUS), a
test_done:
    jp 0

test_tick:
    ld hl, TEST_SECOND
    push hl
    ld hl, TEST_MINUTE
    push hl
    ld hl, TEST_HOUR
    push hl
    call _netchesszx_asm_timer_tick_one_second
    pop bc
    pop bc
    pop bc
    ret

_line_buf:
    DEFS 1
_NETCHESS_PROTO_ACK_PREFIX:
    DEFM "ACK "
    DEFB 0
_NETCHESS_PROTO_NACK_PREFIX:
    DEFM "NACK "
    DEFB 0
_spectrum_net_payload_scratch:
    DEFS 256
_spectrum_net_send_text:
    ld l, 1
    ret
"""

OUTGOING_C_PROBE_SOURCE = r"""
#include <stdint.h>

extern uint8_t netchesszx_session_send_ack_move(const char *ply);
extern uint8_t netchesszx_session_send_nack_move(const char *ply);

uint8_t outgoing_ack_probe(void)
{
    const char *ply = "37";
    return netchesszx_session_send_ack_move(ply);
}

uint8_t outgoing_nack_probe(void)
{
    const char *ply = "37";
    return netchesszx_session_send_nack_move(ply);
}
"""

OUTGOING_RUNTIME_PROBE_SOURCE = r"""
SECTION code_user

PUBLIC _line_buf
PUBLIC _spectrum_net_payload_scratch
PUBLIC _spectrum_net_send_text
PUBLIC _NETCHESS_PROTO_ACK_PREFIX
PUBLIC _NETCHESS_PROTO_NACK_PREFIX

EXTERN _netchesszx_session_send_ack_move
EXTERN _netchesszx_session_send_nack_move
EXTERN _outgoing_ack_probe
EXTERN _outgoing_nack_probe

TEST_STATUS EQU 0x7000
TEST_CALLS  EQU 0x7001
TEST_SP     EQU 0x7ff0
TEST_SCRATCH EQU 0x7010
TEST_ACK    EQU 0x7020
TEST_NACK   EQU 0x7030
TEST_ACK_RESULT EQU 0x7040
TEST_NACK_RESULT EQU 0x7041

test_start:
    ld sp, TEST_SP
    xor a
    ld (TEST_STATUS), a
    ld (TEST_CALLS), a
    call _outgoing_ack_probe
    ld a, l
    ld (TEST_ACK_RESULT), a
    cp 0x5a
    jr nz, outgoing_done
    ld hl, 0
    add hl, sp
    ld de, TEST_SP
    or a
    sbc hl, de
    jr nz, outgoing_done

    call _outgoing_nack_probe
    ld a, l
    ld (TEST_NACK_RESULT), a
    cp 0xa5
    jr nz, outgoing_done
    ld hl, 0
    add hl, sp
    ld de, TEST_SP
    or a
    sbc hl, de
    jr nz, outgoing_done

    ld hl, TEST_ACK
    ld de, TEST_EXPECT_ACK
    call outgoing_compare_z
    jr nz, outgoing_done
    ld hl, TEST_NACK
    ld de, TEST_EXPECT_NACK
    call outgoing_compare_z
    jr nz, outgoing_done
    ld a, (TEST_ACK_RESULT)
    cp 0x5a
    jr nz, outgoing_done
    ld a, (TEST_NACK_RESULT)
    cp 0xa5
    jr nz, outgoing_done
    ld a, (TEST_CALLS)
    cp 2
    jr nz, outgoing_done
    ld a, 1
    ld (TEST_STATUS), a
outgoing_done:
    jp 0

outgoing_compare_z:
    ld a, (de)
    inc de
    cp (hl)
    ret nz
    inc hl
    or a
    jr nz, outgoing_compare_z
    ret

_spectrum_net_payload_scratch:
    ld hl, TEST_SCRATCH
    ret

_spectrum_net_send_text:
    ld a, (TEST_CALLS)
    inc a
    ld (TEST_CALLS), a
    dec a
    jr nz, outgoing_store_nack
    ld de, TEST_ACK
    jr outgoing_store
outgoing_store_nack:
    ld de, TEST_NACK
outgoing_store:
    call outgoing_copy_z
    ld a, (TEST_CALLS)
    cp 1
    jr nz, outgoing_ret_nack
    ld l, 0x5a
    ret
outgoing_ret_nack:
    ld l, 0xa5
    ret

outgoing_copy_z:
    ld a, (hl)
    inc hl
    ld (de), a
    inc de
    or a
    jr nz, outgoing_copy_z
    ret

_NETCHESS_PROTO_ACK_PREFIX:
    DEFM "ACK "
    DEFB 0
_NETCHESS_PROTO_NACK_PREFIX:
    DEFM "NACK "
    DEFB 0
_line_buf:
    DEFS 1
TEST_EXPECT_ACK:
    DEFM "ACK 37"
    DEFB 0
TEST_EXPECT_NACK:
    DEFM "NACK 37"
    DEFB 0
"""

STREQ_PROBE_SOURCE = r"""
SECTION code_user

EXTERN _spectrum_streq

TEST_STATUS EQU 0x7000
TEST_SP     EQU 0x7ff0

test_start:
    ld sp, TEST_SP
    ld iy, 0x5C3A
    xor a
    ld (TEST_STATUS), a

    ld hl, s_resign
    ld de, s_resign_dup
    call streq_call
    ld a, l
    dec a
    jp nz, streq_done

    ld hl, s_resign
    ld de, s_resignx
    call streq_call
    ld a, l
    or a
    jp nz, streq_done

    ld hl, s_resignx
    ld de, s_resign
    call streq_call
    ld a, l
    or a
    jp nz, streq_done

    ld hl, s_empty
    ld de, s_empty
    call streq_call
    ld a, l
    dec a
    jp nz, streq_done

    ld hl, s_empty
    ld de, s_x
    call streq_call
    ld a, l
    or a
    jp nz, streq_done

    ld hl, s_x
    ld de, s_empty
    call streq_call
    ld a, l
    or a
    jp nz, streq_done

    ld hl, s_draw
    ld de, s_draw_dup
    call streq_call
    ld a, l
    dec a
    jp nz, streq_done

    ld hl, s_takeback
    ld de, s_takeback_dup
    call streq_call
    ld a, l
    dec a
    jp nz, streq_done

    ld hl, s_d3
    ld de, s_d3_dup
    call streq_call
    ld a, l
    dec a
    jp nz, streq_done

    ld hl, s_d3
    ld de, s_d4
    call streq_call
    ld a, l
    or a
    jp nz, streq_done

    ld hl, 0
    add hl, sp
    ld de, TEST_SP
    or a
    sbc hl, de
    jp nz, streq_done

    push iy
    pop hl
    ld de, 0x5C3A
    or a
    sbc hl, de
    jp nz, streq_done

    ld a, 1
    ld (TEST_STATUS), a
streq_done:
    jp 0

streq_call:
    push de
    push hl
    call _spectrum_streq
    ret

s_resign:
    DEFM "/resign"
    DEFB 0
s_resign_dup:
    DEFM "/resign"
    DEFB 0
s_resignx:
    DEFM "/resignx"
    DEFB 0
s_empty:
    DEFB 0
s_x:
    DEFM "x"
    DEFB 0
s_draw:
    DEFM "/draw"
    DEFB 0
s_draw_dup:
    DEFM "/draw"
    DEFB 0
s_takeback:
    DEFM "/takeback"
    DEFB 0
s_takeback_dup:
    DEFM "/takeback"
    DEFB 0
s_d3:
    DEFM "d3"
    DEFB 0
s_d3_dup:
    DEFM "d3"
    DEFB 0
s_d4:
    DEFM "d4"
    DEFB 0
"""


def run(argv, cwd):
    return subprocess.run(
        argv,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def normalize(text):
    return "\n".join(line.strip().lower() for line in text.splitlines())


def check_rst8_iy_file(path, label):
    lines = []
    source = path.read_text(encoding="utf-8")
    # Classic has no MMU transition. Preserve checks across both FILEIO branches.
    source = re.sub(r"(?m)^(?:IFDEF|#ifdef) NETCHESSZX_NEXT_BANKING\n(?:(?!^(?:ENDIF|#endif)).)*^(?:ENDIF|#endif)\n",
                    "", source, flags=re.DOTALL)
    for source_line in source.splitlines():
        line = source_line.split(";", 1)[0].strip().lower()
        if line:
            lines.append(line)

    rst_indices = [index for index, line in enumerate(lines) if line == "rst 8"]
    if not rst_indices:
        print(f"[ERR] {label} has no esxDOS RST 8 sites")
        return False
    for index in rst_indices:
        previous_iy = next(
            (line for line in reversed(lines[:index])
             if line in ("push iy", "pop iy")),
            None,
        )
        if (previous_iy != "push iy" or index + 2 >= len(lines) or
                not lines[index + 1].startswith("defb 0x") or
                lines[index + 2] != "pop iy"):
            print(f"[ERR] {label} RST 8 at site {index} does not preserve IY")
            return False
    print(f"[OK] {label} preserves IY at {len(rst_indices)} RST 8 sites")
    return True


def check_overlay_loader_iy(root):
    return check_rst8_iy_file(
        root / "asm" / "esxdos" / "overlay_loader.asm",
        "Classic overlay loader",
    )


def check_about_overlay_iy(root):
    return check_rst8_iy_file(
        root / "asm" / "overlay" / "about" / "entry_about.asm",
        "Classic About overlay",
    )


def check_fileio_iy(root):
    return check_rst8_iy_file(
        root / "asm" / "esxdos" / "esx_fileio_spectalk.asm",
        "Classic FILEIO",
    )


def check_time_config_rtc_iy(root):
    source = normalize((root / "asm" / "overlay" / "time_config" /
                        "entry_time_config.asm").read_text(encoding="utf-8"))
    for label, opcode in (("tc_rtc_try_drvapi:", "defb m_drvapi"),
                          ("tc_rtc_try_getdate:", "defb m_getdate")):
        start = source.find(label)
        end = source.find(opcode, start)
        call = source[start:end]
        if (start < 0 or end < 0 or "push iy" not in call or
                "push ix" not in call or "ld iy, 0x5c3a" not in call or
                "rst 8" not in call):
            print(f"[ERR] TIME_CONFIG RTC {label} does not establish IY")
            return False
    if "tc_rtc_esx_epilogue:\npop ix\npop iy\nret c" not in source:
        print("[ERR] TIME_CONFIG RTC esxDOS epilogue is unbalanced")
        return False
    print("[OK] TIME_CONFIG RTC preserves IX/IY at 2 RST 8 sites")
    return True


def check_next_rtc_iy(root):
    return check_rst8_iy_file(
        root / "src" / "spectrum" / "overlay" / "mqtt_tx_ovl.c",
        "Next MQTT RTC overlay",
    )


def check_classic_rtc_contract(root):
    classic = (root / "asm" / "overlay" / "time_config" /
               "entry_time_config.asm").read_text(encoding="utf-8").lower()
    required = (
        "tc_rtc_try_drvapi:",
        "tc_rtc_try_getdate:",
        "tc_rtc_try_pcf8563:",
        "tc_rtc_pcf_validate:",
        "ld (ctx + 2), a",
        "ld (ctx + 1), a",
        "ld (ctx), a",
    )
    if any(token not in classic for token in required):
        print("[ERR] Classic RTC chain or validated HMS payload is incomplete")
        return False
    print("[OK] Classic RTC: DRVAPI, M_GETDATE and PCF8563 chain present")
    return True


def check_startup_clock_contract(root):
    overlay = (root / "src" / "spectrum" / "overlay" /
               "mqtt_tx_ovl.c").read_text(encoding="utf-8")
    app = (root / "src" / "spectrum" / "app" /
           "app.c").read_text(encoding="utf-8")
    start = overlay.find("uint8_t mqtt_tx_sync_time_ovl(void)")
    end = overlay.find("uint8_t mqtt_tx_send_text_ovl", start)
    sync = overlay[start:end]
    load = app.find("config_state = netchesszx_config_load_overlay();")
    preflight = app.find("connection_setup:", load)
    cold_boot = app[load:preflight]
    apply_start = app.find("static void session_setup_apply_time(void)")
    apply_end = app.find("static void session_setup_preview_piece_set", apply_start)
    apply_time = app[apply_start:apply_end]
    if (start < 0 or end < 0 or load < 0 or preflight < 0 or
            apply_start < 0 or apply_end < 0 or
            "netchesszx_timezone = NETCHESSZX_TIME_RTC" not in cold_boot or
            "netchesszx_timezone = timezone" not in sync or
            "clock_sync_run()" not in apply_time or
            "spectrum_overlay_exec_cached(spectrum_ovl_time_config" not in
            app.lower() or
            "session_setup_apply_time();" not in app):
        print("[ERR] startup clock does not expose the detected effective mode")
        return False
    print("[OK] startup/menu clock selects and applies the effective time mode")
    return True


def check_setup_latency_contract(root):
    app = normalize((root / "src" / "spectrum" / "app" /
                     "app.c").read_text(encoding="utf-8"))
    setup = normalize((root / "asm" / "overlay" / "setup" /
                       "entry_setup.asm").read_text(encoding="utf-8"))
    time_config = normalize((root / "asm" / "overlay" / "time_config" /
                             "entry_time_config.asm").read_text(encoding="utf-8"))
    setup_words = [line.split() for line in setup.splitlines()]
    time_words = [line.split() for line in time_config.splitlines()]
    save_start = app.find("static void session_setup_save(uint8_t key)")
    save_end = app.find("static uint8_t session_setup_step", save_start)
    step_end = app.find("static uint8_t session_setup_run", save_end)
    save = app[save_start:save_end]
    step = app[save_end:step_end]
    if (save_start < 0 or save_end < 0 or step_end < 0 or
            "uint16_t visible_mask = setup_visible_mask" not in save or
            "setup_visible_mask = visible_mask" not in save or
            "session_setup_render(" in save or
            "flags & (netchesszx_setup_flag_time_ui |" not in step or
            "netchesszx_setup_flag_action_ui" not in step or
            "!(flags & netchesszx_setup_flag_render)" not in step or
            ["flag_time_ui", "equ", "0x10"] not in setup_words or
            ["flag_action_ui", "equ", "0x20"] not in setup_words or
            ["flag_paint", "equ", "0x02"] not in time_words or
            ["flag_time_ui", "equ", "0x10"] not in time_words or
            ["flag_action_ui", "equ", "0x20"] not in time_words or
            "bit 4, a" not in time_config or "bit 5, a" not in time_config):
        print("[ERR] Setup navigation/save reintroduced redundant overlay loads")
        return False
    print("[OK] Setup navigation/save avoids unrelated TIME_CONFIG reloads")
    return True


def check_copy_token_exit(root):
    kernel = root / "asm" / "spectrum" / "shrink_kernels.asm"
    lines = []
    for source_line in kernel.read_text(encoding="utf-8").splitlines():
        line = source_line.split(";", 1)[0].strip().lower()
        if line:
            lines.append(line)
    branches = [
        index for index, line in enumerate(lines)
        if line == "djnz cpb_token_loop"
    ]
    if len(branches) != 1 or branches[0] + 1 >= len(lines):
        print("[ERR] copy-token loop branch not found exactly once")
        return False
    if lines[branches[0] + 1] != "jr cpb_done":
        print("[ERR] full copy-token buffer falls through into copy_digits")
        return False
    print("[OK] copy-token full-buffer exit reaches cpb_done")
    return True


def check_copy_token_runtime(root, probe_dir):
    z80asm = shutil.which("z80asm") or "z80asm"
    ticks = shutil.which("z88dk-ticks") or "z88dk-ticks"
    kernel = (root / "asm" / "spectrum" / "shrink_kernels.asm").resolve()
    source_path = probe_dir / "copy_token_probe.asm"
    binary_path = probe_dir / "copy_token_probe.bin"
    ram_path = probe_dir / "copy_token_probe.ram"

    source_path.write_text(COPY_TOKEN_PROBE_SOURCE, encoding="ascii")
    for old in probe_dir.glob("copy_token_probe*"):
        if old != source_path:
            old.unlink()

    result = run(
        [
            z80asm,
            "-b",
            "-r0x8000",
            "-O=.",
            "-o=copy_token_probe.bin",
            source_path.name,
            str(kernel),
        ],
        probe_dir,
    )
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        print("[ERR] copy-token runtime probe assembly failed")
        return False

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
            "100000",
            "-output",
            ram_path.name,
            binary_path.name,
        ],
        probe_dir,
    )
    if result.returncode != 0 or not ram_path.exists():
        sys.stdout.write(result.stdout)
        print("[ERR] copy-token runtime probe execution failed")
        return False

    ram = ram_path.read_bytes()
    expected = bytes((1,))
    if len(ram) < 65536 or ram[0x7000:0x7001] != expected:
        print("[ERR] copy-token or move-tail runtime contract failed")
        return False
    print("[OK] copy-token and move-tail runtime: cursor, grammar and SP")
    return True


def check_timer_tick_runtime(root, probe_dir):
    z80asm = shutil.which("z80asm") or "z80asm"
    ticks = shutil.which("z88dk-ticks") or "z88dk-ticks"
    kernel = (root / "asm" / "spectrum" / "shrink_kernels.asm").resolve()
    source_path = probe_dir / "timer_tick_probe.asm"
    binary_path = probe_dir / "timer_tick_probe.bin"
    ram_path = probe_dir / "timer_tick_probe.ram"

    source_path.write_text(TIMER_TICK_PROBE_SOURCE, encoding="ascii")
    for old in probe_dir.glob("timer_tick_probe*"):
        if old != source_path:
            old.unlink()

    result = run(
        [
            z80asm,
            "-b",
            "-r0x8000",
            "-O=.",
            "-o=timer_tick_probe.bin",
            source_path.name,
            str(kernel),
        ],
        probe_dir,
    )
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        print("[ERR] timer-tick runtime probe assembly failed")
        return False

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
            "100000",
            "-output",
            ram_path.name,
            binary_path.name,
        ],
        probe_dir,
    )
    if result.returncode != 0 or not ram_path.exists():
        sys.stdout.write(result.stdout)
        print("[ERR] timer-tick runtime probe execution failed")
        return False

    ram = ram_path.read_bytes()
    if len(ram) < 65536 or ram[0x7000:0x7001] != bytes((1,)):
        print("[ERR] timer-tick runtime failed progress, saturation, carry, or SP")
        return False
    print("[OK] timer-tick runtime: progress, saturation, carry, and SP")
    return True


def check_streq_runtime(root, probe_dir):
    z80asm = shutil.which("z80asm") or "z80asm"
    ticks = shutil.which("z88dk-ticks") or "z88dk-ticks"
    kernel = (root / "asm" / "spectrum" / "text.asm").resolve()
    source_path = probe_dir / "streq_probe.asm"
    binary_path = probe_dir / "streq_probe.bin"
    ram_path = probe_dir / "streq_probe.ram"

    source_path.write_text(STREQ_PROBE_SOURCE, encoding="ascii")
    for old in probe_dir.glob("streq_probe*"):
        if old != source_path:
            old.unlink()

    result = run(
        [
            z80asm,
            "-b",
            "-r0x8000",
            "-O=.",
            "-o=streq_probe.bin",
            source_path.name,
            str(kernel),
        ],
        probe_dir,
    )
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        print("[ERR] streq runtime probe assembly failed")
        return False

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
            "100000",
            "-output",
            ram_path.name,
            binary_path.name,
        ],
        probe_dir,
    )
    if result.returncode != 0 or not ram_path.exists():
        sys.stdout.write(result.stdout)
        print("[ERR] streq runtime probe execution failed")
        return False

    ram = ram_path.read_bytes()
    expected = bytes((1,))
    if len(ram) < 65536 or ram[0x7000:0x7001] != expected:
        print("[ERR] streq callee failed equal, prefix, empty, NUL, SP, or IY")
        return False
    print("[OK] streq runtime: equal, prefix, empty, NUL, callee SP, IY")
    return True


def check_outgoing_runtime(root, probe_dir, zcc):
    source_path = probe_dir / "outgoing_probe.c"
    asm_path = probe_dir / "outgoing_probe.asm"
    source_path.write_text(OUTGOING_C_PROBE_SOURCE, encoding="ascii")
    result = run(
        [
            zcc,
            "+z80",
            "-vn",
            "-clib=sdcc_iy",
            "-SO3",
            "-compiler=sdcc",
            "-Cs--no-reg-params",
            "--opt-code-size",
            "--fomit-frame-pointer",
            "-S",
            source_path.name,
            "-o",
            asm_path.name,
        ],
        probe_dir,
    )
    if result.returncode != 0 or not asm_path.exists():
        sys.stdout.write(result.stdout)
        print("[ERR] outgoing SDCC/IY caller probe compile failed")
        return False

    z80asm = shutil.which("z80asm") or "z80asm"
    ticks = shutil.which("z88dk-ticks") or "z88dk-ticks"
    kernel = (root / "asm" / "spectrum" / "shrink_kernels.asm").resolve()
    source_path = probe_dir / "outgoing_runtime_probe.asm"
    binary_path = probe_dir / "outgoing_runtime_probe.bin"
    ram_path = probe_dir / "outgoing_runtime_probe.ram"
    source_path.write_text(OUTGOING_RUNTIME_PROBE_SOURCE, encoding="ascii")
    for old in probe_dir.glob("outgoing_runtime_probe*"):
        if old != source_path and old.is_file():
            old.unlink()

    result = run(
        [
            z80asm,
            "-b",
            "-r0x8000",
            "-O=.",
            "-o=outgoing_runtime_probe.bin",
            source_path.name,
            asm_path.name,
            str(kernel),
        ],
        probe_dir,
    )
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        print("[ERR] outgoing runtime probe assembly failed")
        return False
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
            "100000",
            "-output",
            ram_path.name,
            binary_path.name,
        ],
        probe_dir,
    )
    if result.returncode != 0 or not ram_path.exists():
        sys.stdout.write(result.stdout)
        print("[ERR] outgoing runtime probe execution failed")
        return False
    ram = ram_path.read_bytes()
    if (len(ram) < 65536 or ram[0x7000:0x7001] != bytes((1,)) or
            ram[0x7001:0x7002] != bytes((2,))):
        print("[ERR] outgoing runtime probe failed text, return, or SP checks")
        return False
    print("[OK] outgoing runtime probe: generated callers, text, return, and SP")
    return True


def check_next_rtc_runtime(root, probe_dir, zcc):
    """Execute the production RTC wrapper/validator across firmware clobbers."""
    asm_path = probe_dir / "next_rtc.asm"
    result = run([
        zcc, "+z80", "-vn", "-clib=sdcc_iy", "-SO3", "-compiler=sdcc",
        "-Cs--no-reg-params", "--opt-code-size", "--fomit-frame-pointer",
        "-DNETCHESSZX_SDCC_IY", "-DNETCHESSZX_FIXED_LOW_RAM",
        "-DNETCHESSZX_NEXT", "-DNETCHESSZX_NEXT_BANKING",
        f"-I{root / 'src'}", "-S",
        str(root / "src/spectrum/overlay/mqtt_tx_ovl.c"), "-o", asm_path.name,
    ], probe_dir)
    if result.returncode:
        sys.stdout.write(result.stdout)
        return False
    generated = asm_path.read_text(encoding="utf-8")
    bodies = []
    for name in ("_next_rtc_time", "_capture_msdos_time_bytes_mode"):
        match = re.search(rf"(?m)^{name}:.*?(?=^;\t-)", generated, re.DOTALL)
        if not match:
            print(f"[ERR] RTC probe cannot find generated {name}")
            return False
        bodies.append(match[0])
    body = "\n".join(bodies)
    # Model only the firmware boundary; keep the emitted C and register saves.
    body = re.sub(r"(?im)^\s*DEFB\s+0xed,\s*0x91,\s*0x51,\s*(?:0xff|32)",
                  "\n    nop\n    nop\n    nop\n    nop", body)
    for opcode, stub in (("92", "rtc_drvapi"), ("8e", "rtc_getdate")):
        body = re.sub(rf"rst\s+8\s+defb\s+0x{opcode}", f"call {stub}",
                      body, flags=re.IGNORECASE)
    cases = [(timezone, firmware, date) for timezone in (2, 127)
             for firmware, date in ((0, 0x5d27), (1, 0x5d27),
                                    (2, 0x5d27), (0, 0))]
    harness = ["""SECTION code_user
defc _line_buf = 0x7100
defc _netchesszx_timezone = 0x7110
defc rtc_mode = 0x7111
defc rtc_date = 0x7112
defc clock_calls = 0x7114
defc stamp_calls = 0x7115
start:
    ld sp, 0x7ff0
    ld ix, 0x1234
    ld iy, 0x5c3a
"""]
    expected = bytearray()
    for index, (timezone, firmware, date) in enumerate(cases):
        valid = int(firmware != 2 and date != 0)
        applied = valid * int(timezone == 127)
        expected.extend((applied, applied, valid, 0xf0, 0x7f, 0x34, 0x12,
                         0x3a, 0x5c))
        address = 0x7000 + index * 9
        harness.append(f"""
    xor a
    ld (clock_calls), a
    ld (stamp_calls), a
    ld a, {timezone}
    ld (_netchesszx_timezone), a
    ld a, {firmware}
    ld (rtc_mode), a
    ld hl, {date}
    ld (rtc_date), hl
    ld l, {int(timezone == 127)}
    call _next_rtc_time
    ld a, l
    ld ({address + 2}), a
    ld a, (clock_calls)
    ld ({address}), a
    ld a, (stamp_calls)
    ld ({address + 1}), a
    ld ({address + 3}), sp
    ld ({address + 5}), ix
    ld ({address + 7}), iy
""")
    harness.append("""
    jp 0
_reset_line_buf:
    ld hl, _line_buf
    ld b, 6
    xor a
rtc_clear:
    ld (hl), a
    inc hl
    djnz rtc_clear
    ret
rtc_drvapi:
    ld a, (rtc_mode)
    or a
    jr z, rtc_success
    scf
    ret
rtc_getdate:
    ld a, (rtc_mode)
    cp 1
    jr z, rtc_success
    scf
    ret
rtc_success:
    ld ix, 0xabcd
    ld iy, 0xdcba
    ld bc, (rtc_date)
    ld de, 0x6000
    or a
    ret
_spectrum_net_runtime_set_clock:
    ld hl, clock_calls
    inc (hl)
    ret
_spectrum_net_runtime_set_fat_stamp:
    ld hl, stamp_calls
    inc (hl)
    ret
___sdcc_enter_ix:
    ex (sp), ix
    push ix
    ld ix, 2
    add ix, sp
    ret
""")
    (probe_dir / "next_rtc_probe.asm").write_text("".join(harness) + body,
                                                encoding="ascii")
    result = run([shutil.which("z80asm") or "z80asm", "-b", "-r0x8000",
                  "-O=.", "-o=next_rtc_probe.bin", "next_rtc_probe.asm"], probe_dir)
    if result.returncode:
        sys.stdout.write(result.stdout)
        return False
    result = run([shutil.which("z88dk-ticks") or "z88dk-ticks", "-mz80",
                  "-l", "0x8000", "-pc", "8000", "-end", "0",
                  "-counter", "1000000", "-output", "next_rtc_probe.ram",
                  "next_rtc_probe.bin"], probe_dir)
    if result.returncode:
        sys.stdout.write(result.stdout)
        return False
    ram = (probe_dir / "next_rtc_probe.ram").read_bytes()
    if ram[0x7000:0x7000 + len(expected)] != expected:
        print("[ERR] Next RTC runtime: mode, fallback, validation, SP or IX/IY")
        return False
    print("[OK] Next RTC runtime: 8 mode/firmware cases, validation, SP and IX/IY")
    return True


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--zcc", default="zcc")
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--root", default=".")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    if not check_overlay_loader_iy(root):
        return 1
    if not check_about_overlay_iy(root):
        return 1
    if not check_fileio_iy(root):
        return 1
    if not check_time_config_rtc_iy(root):
        return 1
    if not check_next_rtc_iy(root):
        return 1
    if not check_classic_rtc_contract(root):
        return 1
    if not check_startup_clock_contract(root):
        return 1
    if not check_setup_latency_contract(root):
        return 1
    if not check_copy_token_exit(root):
        return 1

    zcc = shutil.which(args.zcc) or args.zcc
    build_dir = Path(args.build_dir).resolve()
    probe_dir = build_dir / "abi_probe_sdcc_iy"
    probe_dir.mkdir(parents=True, exist_ok=True)

    source_path = probe_dir / "abi_probe.c"
    source_path.write_text(PROBE_SOURCE, encoding="ascii")

    for old in probe_dir.glob("abi_probe*"):
        if old != source_path and old.is_file():
            old.unlink()

    cmd = [
        zcc,
        "+z80",
        "-vn",
        "-clib=sdcc_iy",
        "-SO3",
        "-compiler=sdcc",
        "-Cs--no-reg-params",
        "--opt-code-size",
        "--fomit-frame-pointer",
        "-S",
        str(source_path.name),
        "-o",
        "abi_probe",
    ]
    result = run(cmd, probe_dir)
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        print("[ERR] SDCC/IY ABI probe compile failed")
        return result.returncode

    asm_files = sorted(probe_dir.glob("abi_probe*.asm"))
    if not asm_files and (probe_dir / "abi_probe").exists():
        asm_files = [probe_dir / "abi_probe"]
    if not asm_files:
        print("[ERR] SDCC/IY ABI probe did not emit asm")
        return 1

    asm_text = normalize(asm_files[0].read_text(encoding="utf-8", errors="replace"))
    packed_word_patterns = [
        ("ld\thl,0x3412", "push\thl"),
        ("ld\tde,0x3412", "push\tde"),
        ("ld\tbc,0x3412", "push\tbc"),
    ]
    call_index = asm_text.find("call\t_abi_pair")
    if call_index < 0:
        print("[ERR] SDCC/IY ABI changed: abi_pair call not found")
        return 1
    before_call = asm_text[:call_index]
    if not any(load in before_call and push in before_call for load, push in packed_word_patterns):
        print("[ERR] SDCC/IY ABI changed: expected uint8,uint8 packed word 0x3412 before abi_pair")
        return 1

    print("[OK] SDCC/IY ABI probe: uint8,uint8 packed stack call")
    if not check_outgoing_runtime(root, probe_dir, zcc):
        return 1
    if not check_copy_token_runtime(root, probe_dir):
        return 1
    if not check_timer_tick_runtime(root, probe_dir):
        return 1
    if not check_streq_runtime(root, probe_dir):
        return 1
    if not check_next_rtc_runtime(root, probe_dir, zcc):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
