"""Bind paged cartridge artifacts to their linked geometry and XFS ABI."""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from gen_overlay_defs import parse_map


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--atlas", type=Path, required=True)
    args = parser.parse_args()
    symbols = parse_map(args.build_dir / "MIRSHIFT.map")
    assert symbols["_overlay_code_slot"] == 0x2000
    assert symbols["_spectrum_overlay_context"] == 0x5FF8
    assert symbols["_spxn_overlay_page_table"] == 0x6800
    assert symbols["_spxn_overlay_loader_start"] == 0x6E00
    assert symbols["_spxn_overlay_loader_end"] <= 0x7000
    assert symbols["__bss_user_head"] == 0x5B00
    assert symbols["__bss_user_tail"] <= 0x5B60
    assert symbols["__BSS_UNINITIALIZED_tail"] <= 0x5B60
    assert symbols["__BSS_END_tail"] <= 0x5B60
    assert symbols["_esx_handle"] == 0x5B60
    assert symbols["_spxn_xfs_state_end"] == 0x5B74
    assert symbols["_spxn_rom_held"] < 0x5B60
    atlas = args.atlas.read_bytes()
    assert atlas[:6] == b"NZOA\x01\x13"
    assert int.from_bytes(atlas[6:8], "little") == 100
    sizes = json.loads((args.build_dir / "overlay_sizes.json").read_text())
    offset = 100
    names = list(sizes)[:atlas[5]]
    for index, name in enumerate(names):
        start = int.from_bytes(atlas[8 + 4 * index:10 + 4 * index], "little")
        size = int.from_bytes(atlas[10 + 4 * index:12 + 4 * index], "little")
        payload = (args.build_dir / f"MIRSHIFT_{name}.OVL").read_bytes()
        assert start == offset and size == len(payload) == sizes[name]
        assert 0 < size <= 4096 and atlas[start:start + size] == payload
        count = payload[0]
        assert 0 < count and 1 + count * 2 <= size
        for entry in range(count):
            address = int.from_bytes(payload[1 + entry * 2:3 + entry * 2], "little")
            assert 0x2000 <= address < 0x2000 + size
        offset += size
    assert offset == len(atlas)
    table = (args.build_dir / "overlay_atlas_table.asm").read_text()
    for index, byte in enumerate(atlas[96:100]):
        assert re.search(rf"ovl_atlas_fingerprint_{index} EQU {byte}$", table, re.M)
        assert symbols[f"ovl_atlas_fingerprint_{index}"] == byte
    for name in ("CONFIG", "SAVELOAD", "FILEUI"):
        maps = list((args.build_dir / "ovl" / name).rglob(f"MIRSHIFT_{name}.map"))
        assert len(maps) == 1, (name, maps)
        overlay = parse_map(maps[0])
        for symbol, expected in (("_esx_handle", 0x5B60),
                                 ("_spxn_xfs_scratch_preserve_base", 0x662B),
                                 ("_spxn_xfs_scratch_preserve_size", 96),
                                 ("_spxn_xfs_scratch_preserve_backup", 0x676B)):
            assert overlay[symbol] == expected, (name, symbol)
        assert 0x2000 <= overlay["_esx_fopen"] < 0x3000
        assert overlay["_spxn_rom_hlcall"] == symbols["_spxn_rom_hlcall"]
    assert "_spxn_resolve" not in symbols
    print("[OK] cartridge linked layout, all entry tables, atlas binding and cold XFS preservation")


if __name__ == "__main__":
    main()
