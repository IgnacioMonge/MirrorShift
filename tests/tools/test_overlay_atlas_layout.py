#!/usr/bin/env python3
"""Contract checks for Classic compact and Next paged overlay atlases."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import os
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from gen_overlay_atlas import (  # noqa: E402
    HEADER_LEN, MAGIC, ORDER, VERSION, replace_if_changed,
)


def run(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(ROOT / "tools/gen_overlay_atlas.py"), *args],
        text=True,
        capture_output=True,
        check=False,
    )


def args(root: Path, out: Path, table: Path) -> list[str]:
    return [
        "--build-dir", str(root), "--name", "MIRSHIFT", "--out", str(out),
        "--asm-out", str(table),
    ]


def write_overlays(root: Path, first_size: int = 1) -> list[bytes]:
    payloads = []
    for index, name in enumerate(ORDER):
        data = bytes([index]) * (first_size if index == 0 else index + 1)
        (root / f"MIRSHIFT_{name}.OVL").write_bytes(data)
        payloads.append(data)
    return payloads


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="overlay_atlas_layout_") as temp_name:
        root = Path(temp_name)
        payloads = write_overlays(root)
        classic_out = root / "classic.ovl"
        classic_table = root / "classic.asm"
        result = run(root, *args(root, classic_out, classic_table))
        assert result.returncode == 0, result.stderr

        offset = HEADER_LEN
        entries = bytearray()
        for data in payloads:
            entries += offset.to_bytes(2, "little") + len(data).to_bytes(2, "little")
            offset += len(data)
        expected = (
            MAGIC + bytes((VERSION, len(ORDER))) + HEADER_LEN.to_bytes(2, "little")
            + bytes(entries) + bytes(HEADER_LEN - 8 - len(entries)) + b"".join(payloads)
        )
        assert classic_out.read_bytes() == expected
        assert classic_table.read_text(encoding="ascii").count("DW ") == len(ORDER) + 1
        unchanged_bytes = classic_out.read_bytes()
        unchanged_ns = 1_700_000_000_000_000_000
        os.utime(classic_out, ns=(unchanged_ns, unchanged_ns))
        with mock.patch("gen_overlay_atlas.os.replace") as replace:
            assert not replace_if_changed(classic_out, unchanged_bytes)
        replace.assert_not_called()
        assert classic_out.stat().st_mtime_ns == unchanged_ns

        temp_prefix = f".{classic_out.name}."
        temp_files_before = {
            path.name for path in root.iterdir()
            if path.name.startswith(temp_prefix)
        }
        with mock.patch(
            "gen_overlay_atlas.os.replace",
            side_effect=PermissionError("locked"),
        ) as replace:
            try:
                replace_if_changed(classic_out, unchanged_bytes + b"changed")
            except PermissionError:
                pass
            else:
                raise AssertionError("failed replace must propagate")
        replace.assert_called_once()
        assert classic_out.read_bytes() == unchanged_bytes
        assert classic_out.stat().st_mtime_ns == unchanged_ns
        temp_files_after = {
            path.name for path in root.iterdir()
            if path.name.startswith(temp_prefix)
        }
        assert temp_files_after == temp_files_before

        cart_out, cart_table = root / "cart.ovl", root / "cart.asm"
        source = root / "source.c"
        source.write_text("first", encoding="ascii")
        cart_args = args(root, cart_out, cart_table) + ["--block-size", "4096",
                     "--binding-path", str(source), "--binding-value", "flags=normal"]
        assert run(root, *cart_args).returncode == 0
        first = cart_out.read_bytes()
        assert first[:96] == expected[:96] and first[100:] == expected[100:]
        fingerprint = first[96:100]
        table_text = cart_table.read_text()
        for i, value in enumerate(fingerprint):
            assert f"ovl_atlas_fingerprint_{i} EQU {value}" in table_text
        source.write_text("other", encoding="ascii")
        assert run(root, *cart_args).returncode == 0
        assert cart_out.read_bytes()[96:100] != fingerprint
        fingerprint = cart_out.read_bytes()[96:100]
        assert run(root, *cart_args[:-1], "flags=changed").returncode == 0
        assert cart_out.read_bytes()[96:100] != fingerprint

        next_out = root / "next.ovl"
        next_table = root / "next.asm"
        result = run(
            root, *args(root, next_out, next_table), "--block-size", "8192",
            "--size-limit", "4096",
        )
        assert result.returncode == 0, result.stderr
        paged = next_out.read_bytes()
        assert len(paged) == len(ORDER) * 8192
        for index, data in enumerate(payloads):
            page = paged[index * 8192 : (index + 1) * 8192]
            assert page[: len(data)] == data and not any(page[len(data) :])
        assert next_table.read_text(encoding="ascii").count("DW ") == len(ORDER)

        (root / "MIRSHIFT_RULES.OVL").write_bytes(bytes(4097))
        result = run(
            root, *args(root, next_out, next_table), "--block-size", "8192",
            "--size-limit", "4096",
        )
        assert result.returncode != 0 and "too large" in result.stderr
        result = run(
            root, *args(root, next_out, next_table), "--block-size", "8192",
            "--size-limit", "0",
        )
        assert result.returncode != 0 and "size limit" in result.stderr

        bootstrap = root / "bootstrap.asm"
        command = args(root, next_out, bootstrap) + [
            "--block-size", "8192", "--size-limit", "4096", "--bootstrap-asm",
        ]
        assert run(root, *command).returncode == 0
        first = bootstrap.read_bytes()
        assert run(root, *command).returncode == 0
        assert bootstrap.read_bytes() == first
    print("overlay atlas Classic/Next layouts ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
