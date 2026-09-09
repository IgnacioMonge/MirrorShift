#!/usr/bin/env python3
"""Execute the production Spectrum MQTT CONNECT builder in z88dk-ticks."""

import argparse
import shutil
import subprocess
from pathlib import Path


PROBE = r"""
SECTION code_user

PUBLIC _mqtt_connect_start_ovl
PUBLIC _mqtt_activate_side_ovl
PUBLIC _net_preflight_ovl
PUBLIC _mqtt_probe_seat_ovl
PUBLIC _netchesszx_mqtt_code
PUBLIC _netchesszx_local_color
PUBLIC _netchesszx_session_role
PUBLIC _netchesszx_mqtt_session_id

EXTERN _mqtt_connect_packet_ovl

PACKET      EQU 0x672b
TEST_STATUS EQU 0x7000
TEST_SP     EQU 0x7ff0

test_start:
    ld sp, TEST_SP
    xor a
    ld (TEST_STATUS), a
    ld (_netchesszx_local_color), a
    ld (_netchesszx_session_role), a
    ld hl, 42
    ld (_netchesszx_mqtt_session_id), hl
    call _mqtt_connect_packet_ovl
    ld a, h
    or a
    jp nz, test_done
    ld a, l
    cp host_expected_end - host_expected
    jp nz, test_done
    ld hl, host_expected
    ld de, PACKET
    ld b, host_expected_end - host_expected
    call compare_packet
    or a
    jp z, test_done
    ld a, 1
    ld (TEST_STATUS), a

    ld a, 1
    ld (_netchesszx_local_color), a
    ld (_netchesszx_session_role), a
    xor a
    ld (_netchesszx_mqtt_session_id), a
    ld (_netchesszx_mqtt_session_id + 1), a
    call _mqtt_connect_packet_ovl
    ld a, h
    or a
    jp nz, test_done
    ld a, l
    cp guest_expected_end - guest_expected
    jp nz, test_done
    ld hl, guest_expected
    ld de, PACKET
    ld b, guest_expected_end - guest_expected
    call compare_packet
    or a
    jp z, test_done
    ld hl, 0
    add hl, sp
    ld de, TEST_SP
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 2
    ld (TEST_STATUS), a
test_done:
    jp 0

compare_packet:
    ld a, (hl)
    cp '?'
    jr nz, compare_exact
    ld a, (de)
    cp '0'
    jr c, compare_fail
    cp ':'
    jr c, compare_next
    cp 'A'
    jr c, compare_fail
    cp 'G'
    jr nc, compare_fail
    jr compare_next
compare_exact:
    ld a, (de)
    cp (hl)
    jr nz, compare_fail
compare_next:
    inc hl
    inc de
    djnz compare_packet
    ld a, 1
    ret
compare_fail:
    xor a
    ret

host_expected:
    DEFB 0x10, 54, 0, 4, "MQTT", 4, 6, 0, 20, 0, 9
    DEFM "ZXABH????"
    DEFB 0, 23
    DEFM "netchesszx/v1/AB/pres_w"
    DEFB 0, 6
    DEFM "F W 42"
host_expected_end:
guest_expected:
    DEFB 0x10, 21, 0, 4, "MQTT", 4, 2, 0, 20, 0, 9
    DEFM "ZXABJ????"
guest_expected_end:

_mqtt_connect_start_ovl:
_mqtt_activate_side_ovl:
_net_preflight_ovl:
_mqtt_probe_seat_ovl:
    ret
_netchesszx_mqtt_code:
    DEFM "AB"
    DEFB 0
_netchesszx_local_color:
    DEFB 0
_netchesszx_session_role:
    DEFB 0
_netchesszx_mqtt_session_id:
    DEFW 0
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--build-dir", default="build")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    work = Path(args.build_dir).resolve() / "mqtt_connect_probe"
    work.mkdir(parents=True, exist_ok=True)
    source = work / "mqtt_connect_probe.asm"
    binary = work / "mqtt_connect_probe.bin"
    ram = work / "mqtt_connect_probe.ram"
    source.write_text(PROBE, encoding="ascii")
    binary.unlink(missing_ok=True)
    ram.unlink(missing_ok=True)

    result = subprocess.run(
        [shutil.which("z80asm") or "z80asm", "-b", "-r0x8000", "-O=.",
         "-o=" + binary.name, source.name,
         str(root / "asm/overlay/mqtt_connect/entry_mqtt_connect.asm"),
         str(root / "asm/spectrum/text.asm")],
        cwd=work, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    if result.returncode:
        print(result.stdout, end="")
        print("[ERR] MQTT CONNECT probe assembly failed")
        return 1
    result = subprocess.run(
        [shutil.which("z88dk-ticks") or "z88dk-ticks", "-mz80", "-l", "0x8000",
         "-pc", "8000", "-end", "0", "-counter", "100000", "-output",
         ram.name, binary.name],
        cwd=work, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    if result.returncode or not ram.exists():
        print(result.stdout, end="")
        print("[ERR] MQTT CONNECT probe execution failed")
        return 1
    image = ram.read_bytes()
    checkpoint = image[0x7000] if len(image) >= 65536 else 0
    if checkpoint != 2:
        print(f"[ERR] MQTT CONNECT runtime failed after checkpoint {checkpoint}")
        return 1
    print("[OK] MQTT CONNECT: host correlated will; guest no will; lengths and SP")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
