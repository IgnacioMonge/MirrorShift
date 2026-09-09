#!/usr/bin/env python3
"""Focused contract for Next 4K overlays plus mirrored resident code."""

from __future__ import annotations

import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from gen_next_nex import (  # noqa: E402
    LOADER_RAW_BANK_COUNT,
    MAIN_BANKS,
    OVERLAY_EXEC_SIZE,
    OVERLAY_PAGE_COUNT,
    PAGE_SIZE,
    RESIDENT_MIRROR_BASE,
    RESIDENT_MIRROR_END,
    render_overlay_pages,
    resident_length,
    validate_mirror_installer,
    validate_resident_mirror,
)
from gen_overlay_atlas import MAGIC, ORDER as BASE_ORDER  # noqa: E402

ORDER = BASE_ORDER + ["NOTICES"]


def source_int(text: str, name: str, operator: str) -> int:
    match = re.search(
        rf"(?m)^{re.escape(name)}\s*{operator}\s*(0x[0-9a-fA-F]+|[0-9]+)$",
        text,
    )
    assert match, name
    return int(match.group(1), 0)


def run_atlas(
    root: Path, block_size: int | None, size_limit: int | None = None
) -> subprocess.CompletedProcess[str]:
    cmd = [
        sys.executable,
        str(ROOT / "tools/gen_overlay_atlas.py"),
        "--build-dir",
        str(root),
        "--name",
        "MIRSHIFT",
        "--extra",
        "NOTICES",
        "--out",
        str(root / "MIRSHIFT.OVL"),
        "--asm-out",
        str(root / "overlay_atlas_table.asm"),
    ]
    if block_size is not None:
        cmd += ["--block-size", str(block_size)]
    if size_limit is not None:
        cmd += ["--size-limit", str(size_limit)]
    return subprocess.run(cmd, text=True, capture_output=True, check=False)


def write_overlays(root: Path, first_size: int) -> None:
    for index, name in enumerate(ORDER):
        size = first_size if index == 0 else index + 1
        (root / f"MIRSHIFT_{name}.OVL").write_bytes(bytes([index]) * size)


def main() -> int:
    from test_next_paging import main as check_paging

    check_paging()
    assert len(ORDER) == OVERLAY_PAGE_COUNT == 18
    assert resident_length(
        {"__data_compiler_tail": 0xF010, "__DATA_END_tail": 0xF011}, 0xF000
    ) == 0x11
    with tempfile.TemporaryDirectory(prefix="next_overlay_pages_") as temp_name:
        root = Path(temp_name)
        write_overlays(root, OVERLAY_EXEC_SIZE)
        result = run_atlas(root, PAGE_SIZE, OVERLAY_EXEC_SIZE)
        assert result.returncode == 0, result.stderr
        paged = (root / "MIRSHIFT.OVL").read_bytes()
        assert len(paged) == OVERLAY_PAGE_COUNT * PAGE_SIZE
        for index in range(OVERLAY_PAGE_COUNT):
            split = index * PAGE_SIZE + OVERLAY_EXEC_SIZE
            assert not any(paged[split : (index + 1) * PAGE_SIZE])
        mirror = bytes(range(256)) * (OVERLAY_EXEC_SIZE // 256)
        mirrored = render_overlay_pages(paged, mirror)
        for index in range(OVERLAY_PAGE_COUNT):
            start = index * PAGE_SIZE
            split = start + OVERLAY_EXEC_SIZE
            end = start + PAGE_SIZE
            assert mirrored[start:split] == paged[start:split]
            assert mirrored[split:end] == mirror
        table = (root / "overlay_atlas_table.asm").read_text(encoding="ascii")
        assert "ovl_atlas_count EQU 18" in table
        assert "DW 4096" in table

        contaminated = bytearray(paged)
        contaminated[OVERLAY_EXEC_SIZE] = 1
        try:
            render_overlay_pages(bytes(contaminated), mirror)
        except SystemExit as exc:
            assert "mirror half" in str(exc)
        else:
            raise AssertionError("non-empty resident mirror half accepted")

        (root / "MIRSHIFT_RULES.OVL").write_bytes(bytes(OVERLAY_EXEC_SIZE + 1))
        result = run_atlas(root, PAGE_SIZE, OVERLAY_EXEC_SIZE)
        assert result.returncode != 0 and "too large" in result.stderr

        (root / "MIRSHIFT_RULES.OVL").unlink()
        result = run_atlas(root, PAGE_SIZE, OVERLAY_EXEC_SIZE)
        assert result.returncode != 0

        result = run_atlas(root, OVERLAY_EXEC_SIZE, PAGE_SIZE)
        assert result.returncode != 0 and "size limit" in result.stderr
        result = run_atlas(root, PAGE_SIZE, 0)
        assert result.returncode != 0 and "size limit" in result.stderr

        write_overlays(root, 2049)
        result = run_atlas(root, None)
        assert result.returncode != 0 and "too large" in result.stderr
        write_overlays(root, 2048)
        result = run_atlas(root, None)
        assert result.returncode == 0, result.stderr
        classic = (root / "MIRSHIFT.OVL").read_bytes()
        assert classic.startswith(MAGIC)
        assert len(classic) < OVERLAY_PAGE_COUNT * PAGE_SIZE

    writable = {
        "__data_compiler_head": 0xF000,
        "__data_compiler_tail": 0xF010,
        "__data_user_head": 0xF010,
        "__data_user_tail": 0xF011,
        "__bss_compiler_head": 0xF100,
        "__bss_compiler_tail": 0xF200,
        "__bss_user_head": 0xF200,
        "__bss_user_tail": 0xF210,
    }
    validate_resident_mirror(writable)
    validate_resident_mirror(
        {
            name: address
            for name, address in writable.items()
            if "data_user" not in name
        }
    )
    writable["__data_compiler_head"] = RESIDENT_MIRROR_BASE
    writable["__data_compiler_tail"] = RESIDENT_MIRROR_END
    try:
        validate_resident_mirror(writable)
    except SystemExit as exc:
        assert "writable section" in str(exc)
    else:
        raise AssertionError("writable resident mirror accepted")

    try:
        validate_resident_mirror({"__data_user_head": 0xF000})
    except SystemExit as exc:
        assert "incomplete writable section" in str(exc)
    else:
        raise AssertionError("incomplete writable section accepted")

    installer = {name: 0xE000 for name in (
        "next_install_overlay_mirrors", "next_map_slot2",
        "next_map_slot3", "nextreg_write",
    )}
    validate_mirror_installer(installer)
    installer["next_map_slot3"] = RESIDENT_MIRROR_BASE
    try:
        validate_mirror_installer(installer)
    except SystemExit as exc:
        assert "remapped MMU window" in str(exc)
    else:
        raise AssertionError("self-overwriting mirror installer accepted")

    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    overlay_offset = source_int(makefile, "NEXT_OVERLAY_OFFSET", r":=")
    assert overlay_offset % PAGE_SIZE == 0
    assert source_int(makefile, "ZX_ORG", r":=") == RESIDENT_MIRROR_BASE
    assert MAIN_BANKS[0] == (5, 0x4000)
    assert "OVERLAY_BLOCK_SIZE=8192" in makefile
    assert "OVERLAY_SIZE_LIMIT=4096" in makefile
    assert "--org $(ZX_ORG)" in makefile
    assert "--overlay-offset $(NEXT_OVERLAY_OFFSET)" in makefile
    assert source_int(makefile, "NEXT_RAW_BANK_BASE", r":=") == 16
    assert source_int(makefile, "NEXT_MAX_BUNDLE_BANKS", r":=") == 4
    assert overlay_offset + OVERLAY_PAGE_COUNT * PAGE_SIZE <= (
        LOADER_RAW_BANK_COUNT * 2 * PAGE_SIZE
    )
    print("Next mirrored direct-MMU overlay layout ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
