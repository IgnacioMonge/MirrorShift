#!/usr/bin/env python3
"""ROM calls restore slot 1 and keep their I/O buffers outside that slot."""

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from check_lowmem_layout import select_source


def check_calls(source, expected):
    lines = [line.split(";", 1)[0].strip().lower() for line in source.splitlines()]
    lines = [line for line in lines if line]
    sites = [i for i, line in enumerate(lines) if line == "rst 8"]
    assert len(sites) == expected
    for i in sites:
        assert lines[i - 1] == "defb 0xed, 0x91, 0x51, 0xff"
        assert lines[i + 1].startswith("defb ")
        assert lines[i + 2] in ("defb 0xed, 0x91, 0x51, next_extension_page",
                                "defb 0xed, 0x91, 0x51, 32")


def check_handles():
    """Run unchanged wrapper instructions with simulated RST 8/MMU hardware."""
    work = ROOT / "build/esxdos-handles/runtime"
    work.mkdir(parents=True, exist_ok=True)
    failures = []
    total = 0
    for fileui in (False, True):
        # symbol, opcode, mode, encoded input, firmware A/carry/BC,
        # expected handle/result/return L. None means unspecified by the ABI.
        cases = []
        opens = [("opendir", 0xa3, 0)] if fileui else [
            ("fopen", 0x9a, 1), ("fcreate", 0x9a, 14),
            ("fcreate_new", 0x9a, 6)]
        for symbol, opcode, mode in opens:
            for raw, carry in ((0, 0), (7, 0), (2, 1), (5, 1)):
                cases.append((symbol, opcode, mode, 0x55, raw, carry, 0,
                              0 if carry else raw + 1,
                              0xbeef if fileui else raw if carry else 0, None))
        for raw in (0, 7):
            if fileui:
                for value, carry in ((1, 0), (0, 0), (5, 1)):
                    cases.append(("readdir", 0xa4, None, raw + 1,
                                  value, carry, 0x9876, raw + 1,
                                  0 if carry else value, None))
            else:
                for symbol, opcode in (("fread", 0x9d), ("fwrite", 0x9e)):
                    for carry in (0, 1):
                        cases.append((symbol, opcode, None, raw + 1,
                                      5, carry, 3, raw + 1,
                                      0 if carry else 3, None))
            for carry in (0, 1):
                cases.append(("fclose", 0x9b, None, raw + 1,
                              5, carry, 0, raw + 1, 0xbeef,
                              0xff if carry else 0))

        for banking in (False, True):
            name = f"{'next' if banking else 'classic'}_{'dir' if fileui else 'file'}"
            harness = ["""SECTION code_user
start:
    ld a, 0xc3
    ld (8), a
    ld hl, firmware
    ld (9), hl
"""]
            checks = []
            for index, case in enumerate(cases):
                symbol, opcode, mode, handle, value, carry, count, stored, result, ret = case
                address = 0x6000 + index * 21
                harness.append(f"""
    ld sp, 0x7ff0
    ld ix, 0x1357
    ld iy, 0x5c3a
    ld a, {handle}
    ld (_esx_handle), a
    ld hl, 0xbeef
    ld (_esx_result), hl
    ld hl, 0x6800
    ld (_esx_buf), hl
    ld hl, 0x1234
    ld (_esx_count), hl
    ld a, {value}
    ld (return_a), a
    ld a, {carry}
    ld (return_carry), a
    ld hl, {count}
    ld (return_bc), hl
    ld a, 32
    ld (mmu1), a
    xor a
    ld (page_calls), a
    ld hl, 0x6900
    call _esx_{symbol}
    ld a, l
    ld ({address + 3}), a
    ld ({address + 4}), sp
    ld ({address + 6}), ix
    ld ({address + 8}), iy
    ld a, (_esx_handle)
    ld ({address}), a
    ld hl, (_esx_result)
    ld ({address + 1}), hl
    ld hl, trace
    ld de, {address + 10}
    ld bc, 9
    ldir
    ld a, (mmu1)
    ld ({address + 19}), a
    ld a, (page_calls)
    ld ({address + 20}), a
""")
                expected = [(0, 1, stored), (1, 2, result), (4, 2, 0x7ff0),
                            (6, 2, 0x1357), (8, 2, 0x5c3a),
                            (10, 1, ord('*') if mode is not None else handle - 1),
                            (17, 1, 0xff if banking else 32), (18, 1, opcode),
                            (19, 1, 32), (20, 1, 2 if banking else 0)]
                if mode is not None:
                    expected += [(12, 1, mode), (13, 2, 0x6900), (15, 2, 0x6900)]
                elif symbol != "fclose":
                    expected.append((13, 2, 0x6800))
                    if not fileui:
                        expected.append((11, 2, 0x1234))
                if ret is not None:
                    expected.append((3, 1, ret))
                checks.append((address, case, expected))
            harness.append("""
    jp 0
firmware:
    ld (trace), a
    ld (trace + 1), bc
    ld (trace + 3), ix
    ld (trace + 5), hl
    pop hl
    ld a, (hl)
    ld (trace + 8), a
    inc hl
    push hl
    ld a, (mmu1)
    ld (trace + 7), a
    ld ix, 0xabcd
    ld iy, 0xdcba
    ld bc, (return_bc)
    ld a, (return_carry)
    rrca
    ld a, (return_a)
    ret
page_rom:
    push af
    ld a, 0xff
    jr page_set
page_extension:
    push af
    ld a, 32
page_set:
    ld (mmu1), a
    ld a, (page_calls)
    inc a
    ld (page_calls), a
    pop af
    ret
trace: defs 9
return_a: defs 1
return_carry: defs 1
return_bc: defs 2
mmu1: defs 1
page_calls: defs 1
""")
            wrapper = "esx_fileui.asm" if fileui else "esx_saveload.asm"
            harness.append(f'INCLUDE "asm/esxdos/{wrapper}"\n')
            (work / f"{name}.asm").write_text("".join(harness), encoding="ascii")
            command = [shutil.which("z80asm") or "z80asm", "-b", "-m",
                       "-r0x8000", "-O=.", f"-I={ROOT.as_posix()}",
                       f"-o={name}.bin"]
            if banking:
                command.append("-DNETCHESSZX_NEXT_BANKING")
            subprocess.run([*command, f"{name}.asm"], cwd=work, check=True)
            binary = bytearray((work / f"{name}.bin").read_bytes())
            symbols = dict((key, int(value, 16)) for key, value in re.findall(
                r"(?m)^(\w+)\s*=\s*\$([0-9a-fA-F]+)",
                (work / f"{name}.map").read_text()))
            # ticks has no Next MMU. Replace only each assembled NEXTREG with
            # a same-size CALL/NOP recorder preserving AF and all other regs.
            for page, stub in ((0xff, "page_rom"), (32, "page_extension")):
                instruction = bytes((0xed, 0x91, 0x51, page))
                assert binary.count(instruction) == ((3 if fileui else 6) if banking else 0)
                address = symbols[stub]
                binary = binary.replace(instruction,
                                        bytes((0xcd, address & 255, address >> 8, 0)))
            (work / f"{name}_sim.bin").write_bytes(binary)
            subprocess.run([shutil.which("z88dk-ticks") or "z88dk-ticks", "-mz80",
                            "-l", "0x8000", "-pc", "8000", "-end", "0",
                            "-counter", "500000", "-output", f"{name}.ram",
                            f"{name}_sim.bin"], cwd=work, check=True,
                           stdout=subprocess.DEVNULL)
            ram = (work / f"{name}.ram").read_bytes()
            assert len(ram) >= 65536, "Incomplete Z80 memory image"
            for address, case, expected in checks:
                total += 1
                for offset, width, value in expected:
                    actual = int.from_bytes(ram[address + offset:address + offset + width], "little")
                    if actual != value:
                        failures.append(f"{name} {case}: field {offset}, {actual:#x} != {value:#x}")
                        break
    assert not failures, "\n".join(failures[:8]) + f"\n{len(failures)}/{total} failed"
    print(f"esxDOS handles: {total} Z80 cases PASS; errors, SP/IX/IY and MMU1")


def main():
    fileio = (ROOT / "asm/esxdos/esx_fileio_spectalk.asm").read_text()
    check_calls(select_source(fileio, "next"), 6)
    check_calls(select_source("#define ESX_FILEUI\n" + fileio, "next"), 3)
    assert "defb 0xaa" in select_source(fileio, "next").lower()
    assert "0xed, 0x91" not in select_source(fileio, "classic")
    rtc = (ROOT / "asm/overlay/time_config/entry_time_config.asm").read_text()
    check_calls(select_source(rtc, "next"), 2)
    mqtt = (ROOT / "src/spectrum/overlay/mqtt_tx_ovl.c").read_text()
    rtc = next(block for block in re.findall(r"#asm(.*?)#endasm", mqtt, re.S)
               if "rst 8" in block)
    check_calls(select_source(rtc, "next"), 2)
    config = (ROOT / "src/spectrum/overlay/config_ovl.c").read_text()
    assert ("#if defined(NETCHESSZX_CONFIG_PACK_APPLY_TEST) || "
            "defined(NETCHESSZX_NEXT_BANKING)") in config
    assert "#define CONFIG_WIRE setup_config_record" in config
    check_handles()
    print("Next esxDOS/RTC slot-1 mapping PASS")


if __name__ == "__main__":
    main()
