#!/usr/bin/env python3
"""Focused contracts for the incremental 2 KiB overlay builder."""

from __future__ import annotations

import os
import sys
import tempfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from build_overlays import (  # noqa: E402
    BLOCK_SIZE,
    OverlaySpec,
    env_path,
    is_stale,
    maybe_rebuild_resident,
    overlay_specs,
    require_current_map,
    split_args,
    spec_inputs,
    build_one,
)
from gen_overlay_atlas import ORDER  # noqa: E402


CLASSIC_ENV = {
    "EDIT_BUF_OVL": "asm/overlay/edit/edit_buf.asm",
    "ESX_COMMON_ASM": "asm/esxdos/esx_fileio_spectalk.asm",
    "ESX_FILEUI_ASM": "asm/esxdos/esx_fileui.asm",
    "ESX_SAVELOAD_ASM": "asm/esxdos/esx_saveload.asm",
    "GAME_PROTOCOL_MACH_SRC": "src/common/protocol/game_protocol_mach.c",
}


def test_specs_preserve_mirror_atlas_contract() -> None:
    specs = overlay_specs(CLASSIC_ENV)
    assert [spec.name for spec in specs] == ORDER + ["NOTICES"]
    board = next(spec for spec in specs if spec.name == "BOARD")
    assert board.asm == ("asm/overlay/board/entry_board.asm",)
    assert board.c_sources == ()
    assert next(spec for spec in specs if spec.name == "INPUT_EDIT")


def test_spectranext_uses_current_time_inputs() -> None:
    env = dict(CLASSIC_ENV)
    env.update({
        "NET_BACKEND": "spectranext",
        "SPXN_UDP_SRC": "/driver/spxudp.c",
        "SPXN_TIME_SRC": "/driver/spxtime.c",
        "SPXN_TIME_OVL_CFLAGS": "--opt-code-size -DTIME=host",
    })
    time = next(spec for spec in overlay_specs(env) if spec.name == "TIME")
    assert [source for source, _flags in time.c_sources] == [
        "src/spectrum/overlay/time_ovl.c", "/driver/spxudp.c", "/driver/spxtime.c"
    ]
    assert time.c_sources[1][1] == ("--opt-code-size", "-DTIME=host")


def test_windows_paths_and_flags_stay_one_argument() -> None:
    assert env_path({"SPXN_TIME_SRC": r"C:\Driver Files\spxtime.c"}, "SPXN_TIME_SRC") == (
        r"C:\Driver Files\spxtime.c",
    )
    assert split_args('-I"C:/Driver Files" -DNAME=1') == ["-IC:/Driver Files", "-DNAME=1"]


def test_inputs_invalidate_for_map_asm_extra_headers_and_stamp() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        output = root / "GUI_LOG.OVL"
        map_path = root / "MIRSHIFT.map"
        defs = root / "overlay_defs.asm"
        header = root / "event.h"
        source = root / "gui_log_ovl.c"
        asm = root / "entry_gui_log.asm"
        extra = root / "driver.asm"
        stamp = root / "spectrum_config.json"
        builder = root / "build_overlays.py"
        for path in (output, map_path, defs, header, source, asm, extra, stamp, builder):
            path.write_text("x", encoding="utf-8")
            os.utime(path, (1_700_000_000, 1_700_000_000))
        spec = OverlaySpec("GUI_LOG", (str(asm),), ((str(source), ()),), (str(extra),))
        inputs = spec_inputs(spec, map_path, defs, stamp, [header], builder)
        assert not is_stale(output, inputs)
        os.utime(map_path, (1_700_000_001, 1_700_000_001))
        assert is_stale(output, inputs)
        os.utime(map_path, (1_700_000_000, 1_700_000_000))
        os.utime(header, (1_700_000_001, 1_700_000_001))
        assert is_stale(output, inputs)
        os.utime(header, (1_700_000_000, 1_700_000_000))
        os.utime(asm, (1_700_000_001, 1_700_000_001))
        assert is_stale(output, inputs)
        os.utime(asm, (1_700_000_000, 1_700_000_000))
        os.utime(extra, (1_700_000_001, 1_700_000_001))
        assert is_stale(output, inputs)
        os.utime(extra, (1_700_000_000, 1_700_000_000))
        os.utime(stamp, (1_700_000_001, 1_700_000_001))
        assert is_stale(output, inputs)


def test_map_must_match_config_stamp() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        map_path = root / "MIRSHIFT.map"
        stamp = root / "spectrum_config.json"
        map_path.write_text("x", encoding="utf-8")
        stamp.write_text("{}", encoding="utf-8")
        os.utime(map_path, (1_700_000_000, 1_700_000_000))
        os.utime(stamp, (1_700_000_001, 1_700_000_001))
        try:
            require_current_map(map_path, stamp)
        except SystemExit as exc:
            assert "newer" in str(exc)
        else:
            raise AssertionError("newer config stamp accepted")


def test_atlas_rebuild_keeps_make_target_and_2k_cap() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        build_dir = Path(tmp)
        (build_dir / "overlay_atlas_table.changed").write_text("1\n", encoding="ascii")
        with patch("build_overlays.run") as run:
            maybe_rebuild_resident({"MAKE": "make", "ZX_OVL": "release/MIRSHIFT.OVL"}, build_dir)
        assert run.call_args.args[0] == ["make", "ATLAS_FINAL=1", "release/MIRSHIFT.OVL"]
    assert BLOCK_SIZE == 2048


def test_oversize_output_is_never_cached() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        build_dir = Path(tmp)
        map_path = build_dir / "MIRSHIFT.map"
        defs = build_dir / "overlay_defs.asm"
        for path in (map_path, defs):
            path.write_text("x", encoding="utf-8")
        spec = OverlaySpec("TOO_BIG", ())
        output = build_dir / "MIRSHIFT_TOO_BIG.OVL"

        def write_oversize(command: list[str], _cwd: Path | None = None) -> None:
            work_dir = Path(next(arg[3:] for arg in command if arg.startswith("-O=")))
            name = next(arg[3:] for arg in command if arg.startswith("-o="))
            (work_dir / name).parent.mkdir(parents=True, exist_ok=True)
            (work_dir / name).write_bytes(b"x" * (BLOCK_SIZE + 1))

        with patch("build_overlays.run", side_effect=write_oversize):
            try:
                build_one(spec, {}, build_dir, "MIRSHIFT", map_path, defs, 0x6800, [], False)
            except SystemExit as exc:
                assert "max 2048" in str(exc)
            else:
                raise AssertionError("oversize overlay accepted")
        assert not output.exists()

        output.write_bytes(b"x" * (BLOCK_SIZE + 1))
        calls: list[list[str]] = []

        def write_valid(command: list[str], _cwd: Path | None = None) -> None:
            calls.append(command)
            work_dir = Path(next(arg[3:] for arg in command if arg.startswith("-O=")))
            name = next(arg[3:] for arg in command if arg.startswith("-o="))
            (work_dir / name).write_bytes(b"ok")

        with patch("build_overlays.run", side_effect=write_valid):
            build_one(spec, {}, build_dir, "MIRSHIFT", map_path, defs, 0x6800, [], False)
        assert calls and output.read_bytes() == b"ok"


def test_failed_output_is_not_cached() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        build_dir = Path(tmp)
        map_path = build_dir / "MIRSHIFT.map"
        defs = build_dir / "overlay_defs.asm"
        for path in (map_path, defs):
            path.write_text("x", encoding="utf-8")
        spec = OverlaySpec("RETRY", ())
        output = build_dir / "MIRSHIFT_RETRY.OVL"

        def fail_after_writing(command: list[str], _cwd: Path | None = None) -> None:
            work_dir = Path(next(arg[3:] for arg in command if arg.startswith("-O=")))
            name = next(arg[3:] for arg in command if arg.startswith("-o="))
            (work_dir / name).parent.mkdir(parents=True, exist_ok=True)
            (work_dir / name).write_bytes(b"partial")
            raise SystemExit(1)

        with patch("build_overlays.run", side_effect=fail_after_writing):
            try:
                build_one(spec, {}, build_dir, "MIRSHIFT", map_path, defs, 0x6800, [], False)
            except SystemExit:
                pass
            else:
                raise AssertionError("failed overlay accepted")
        assert not output.exists()

        def write_valid(command: list[str], _cwd: Path | None = None) -> None:
            work_dir = Path(next(arg[3:] for arg in command if arg.startswith("-O=")))
            name = next(arg[3:] for arg in command if arg.startswith("-o="))
            (work_dir / name).write_bytes(b"ok")

        with patch("build_overlays.run", side_effect=write_valid) as run:
            build_one(spec, {}, build_dir, "MIRSHIFT", map_path, defs, 0x6800, [], False)
        assert run.called and output.read_bytes() == b"ok"


def main() -> int:
    from build_overlays import filter_local_symbols
    specs = {spec.name: spec for spec in overlay_specs({
        "NET_BACKEND": "spectranext", "SPXN_RESOLVE_C": "driver/spxresolve.c",
        "SPXN_XFS_OVERLAY_ASM": "build/xfs_compat.asm"})}
    for name in ("DIRECT", "MQTT_CONNECT", "TIME"):
        assert ("driver/spxresolve.c", ()) in specs[name].c_sources
    assert specs["ABOUT"].asm == ("asm/overlay/about/entry_about.asm",)
    for name in ("SAVELOAD", "CONFIG", "FILEUI"):
        assert "build/xfs_compat.asm" in specs[name].asm
        assert "_esx_fopen" in specs[name].local_symbols
    with tempfile.TemporaryDirectory() as temp:
        defs = Path(temp) / "defs.asm"
        defs.write_text("PUBLIC _esx_fopen\nDEFC _esx_fopen = 123\nPUBLIC _spxn_rom_held\n")
        result = filter_local_symbols(defs, Path(temp) / "local.asm", specs["CONFIG"].local_symbols)
        assert result.read_text() == "PUBLIC _spxn_rom_held\n"
    for test in (
        test_specs_preserve_mirror_atlas_contract,
        test_spectranext_uses_current_time_inputs,
        test_windows_paths_and_flags_stay_one_argument,
        test_inputs_invalidate_for_map_asm_extra_headers_and_stamp,
        test_map_must_match_config_stamp,
        test_atlas_rebuild_keeps_make_target_and_2k_cap,
        test_oversize_output_is_never_cached,
        test_failed_output_is_not_cached,
    ):
        test()
        print(f"[OK] {test.__name__}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
