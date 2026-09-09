#!/usr/bin/env python3
"""Judge both memory contracts and reject paged writable state/short stacks."""

import contextlib
import io
from pathlib import Path
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from check_lowmem_layout import main as check_layout, parse_const, select_source


def run_map(symbols, target):
    with tempfile.TemporaryDirectory(prefix="mirrorshift-lowmem-") as folder:
        path = Path(folder) / "fixture.map"
        path.write_text("\n".join(f"{key} = ${value:04X}" for key, value in symbols.items()))
        with contextlib.redirect_stdout(io.StringIO()):
            return check_layout(["--map", str(path), "--target", target])


def main():
    conditional = """#ifndef HEADER
#define HEADER
#ifdef NETCHESSZX_NEXT
#define VALUE 0x3500
#else
#define VALUE 0x6000
#endif
#endif
IFDEF NETCHESSZX_NEXT
stage EQU 0x3b2b
ELSE
stage EQU 0x662b
ENDIF
"""
    assert "VALUE 0x3500" in select_source(conditional, "next")
    assert "VALUE 0x6000" not in select_source(conditional, "next")
    assert "stage EQU 0x662b" in select_source(conditional, "classic")
    header = ROOT / "src/spectrum/lowram_map.h"
    assert parse_const(header, "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR", "next") == 0x3C2B
    assert parse_const(header, "NETCHESSZX_LOWRAM_OVERLAY_SCRATCH_ADDR", "classic") == 0x672B
    for ambiguous in ("#if UNKNOWN\n#define VALUE 42\n#endif", "#if UNKNOWN\n#else\n#define VALUE 42\n#endif"):
        try:
            select_source(ambiguous, "next")
        except SystemExit:
            pass
        else:
            raise AssertionError("expression-gated constant accepted without evaluation")

    classic = {
        "expand_2x": 0x6000, "piece_sprites_16x16": 0x662B,
        "_mqtt_stream": 0x633C, "_overlay_scratch_base": 0x672B,
        "_overlay_code_slot": 0x6800, "_spectrum_overlay_context": 0x5FF8,
        "__BSS_END_tail": 0xFD58, "__register_sp": 0xFF58,
    }
    assert run_map(classic, "classic") == 0
    next_map = dict(classic, asset_load_addr=0x3500, piece_sprites_16x16=0x3B2B,
                    _mqtt_stream=0x383C, _overlay_scratch_base=0x3C2B,
                    _overlay_code_slot=0x6000, next_sprite_stage=0x3B2B,
                    next_palette_stage=0x3CCB, __DATA_head=0x9000,
                    __DATA_END_tail=0x9100, __BSS_head=0x9100,
                    __BSS_END_tail=0xA000, __register_sp=0xAC00)
    assert run_map(next_map, "next") == 0
    for changed in ({"_mqtt_stream": 0x383B}, {"next_sprite_stage": 0x662B},
                    {"next_palette_stage": 0x3CCC}, {"_spectrum_overlay_context": 0x5FE0},
                    {"__DATA_head": 0x7FFF}, {"__BSS_head": 0x7FFF},
                    {"__register_sp": 0xABFF}):
        try:
            run_map(dict(next_map, **changed), "next")
        except SystemExit:
            pass
        else:
            raise AssertionError(f"invalid Next layout accepted: {changed}")
    print("Classic/Next lowmem guard PASS (including 7 rejected layouts)")


if __name__ == "__main__":
    main()
