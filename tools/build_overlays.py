#!/usr/bin/env python3
"""Build Mirror Shift overlays incrementally with target-specific code limits."""

from __future__ import annotations

import os
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

sys.dont_write_bytecode = True

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
BUILDER_PATH = Path(__file__).resolve()
sys.path.insert(0, str(TOOLS))

from gen_overlay_atlas import BLOCK_SIZE, replace_if_changed  # noqa: E402
from gen_overlay_defs import parse_map  # noqa: E402


@dataclass(frozen=True)
class OverlaySpec:
    name: str
    asm: tuple[str, ...]
    c_sources: tuple[tuple[str, tuple[str, ...]], ...] = ()
    extra_inputs: tuple[str, ...] = ()
    local_symbols: tuple[str, ...] = ()


SPXN_XFS_LOCAL_SYMBOLS = (
    "_esx_fopen", "_esx_fread", "_esx_fclose", "_spxn_xfs_fseek",
    "_spxn_xfs_dir_scratch", "_spxn_xfs_state_end", "_esx_handle",
    "_esx_buf", "_esx_count", "_esx_result",
    "_spxn_xfs_scratch_preserve_base", "_spxn_xfs_scratch_preserve_size",
    "_spxn_xfs_scratch_preserve_backup",
)


def env_value(env: dict[str, str], name: str) -> str:
    return env.get(name, "").strip()


def env_path(env: dict[str, str], name: str) -> tuple[str, ...]:
    value = env_value(env, name)
    return (value,) if value else ()


def split_args(value: str) -> list[str]:
    if not value.strip():
        return []
    return shlex.split(value)


def overlay_specs(env: dict[str, str]) -> list[OverlaySpec]:
    edit_buf = env_value(env, "EDIT_BUF_OVL") or "asm/overlay/edit/edit_buf.asm"
    esx_common = env_path(env, "ESX_COMMON_ASM")
    esx_fileui = env_path(env, "ESX_FILEUI_ASM")
    esx_saveload = env_path(env, "ESX_SAVELOAD_ASM")
    atomic = env_path(env, "SPXN_ATOMIC_SRC")
    resolver = tuple((source, ()) for source in env_path(env, "SPXN_RESOLVE_C"))
    xfs = env_path(env, "SPXN_XFS_OVERLAY_ASM")
    time_sources = env_path(env, "SPXN_UDP_SRC") + env_path(env, "SPXN_TIME_SRC")
    mach = env_value(env, "GAME_PROTOCOL_MACH_SRC") or "src/common/protocol/game_protocol_mach.c"
    mqtt_flags = tuple(split_args(env_value(env, "MQTT_CONNECT_OVL_CFLAGS")))
    time_flags = tuple(split_args(env_value(env, "SPXN_TIME_OVL_CFLAGS")))

    specs = [
        OverlaySpec("RULES", ("asm/overlay/rules/entry_rules.asm", "asm/overlay/rules/rules_stub.asm"), (("src/spectrum/overlay/rules_restore_ovl.c", ()),), ("src/common/reversi/reversi_restore.inc",)),
        OverlaySpec("BOARD", ("asm/overlay/board/entry_board.asm",)),
        OverlaySpec("GUI_LOG", ("asm/overlay/gui_log/entry_gui_log.asm",), (("src/spectrum/overlay/gui_log_ovl.c", ()),)),
        OverlaySpec("MQTT_CONNECT", ("asm/overlay/mqtt_connect/entry_mqtt_connect.asm",), (("src/spectrum/overlay/mqtt_connect_ovl.c", mqtt_flags),)),
        OverlaySpec("MQTT_TX", ("asm/overlay/mqtt_tx/entry_mqtt_tx.asm",), (("src/spectrum/overlay/mqtt_tx_ovl.c", ()),)),
        OverlaySpec("DIRECT", ("asm/overlay/direct/entry_direct.asm",), (("src/spectrum/overlay/direct_ovl.c", ()),)),
        OverlaySpec("MENU_CONFIG", ("asm/overlay/menu_config/entry_menu_config.asm",)),
        OverlaySpec("MENU_LOGIC", ("asm/overlay/menu_logic/entry_menu_logic.asm",), (("src/spectrum/overlay/status_ovl.c", ()),)),
        OverlaySpec("SETUP", ("asm/overlay/setup/entry_setup.asm", edit_buf)),
        OverlaySpec("INPUT_EDIT", (
            "asm/overlay/input_edit/entry_input_edit.asm",
            "asm/overlay/input_edit/setup_edit_line.asm",
        ), (("src/spectrum/overlay/input_edit_ovl.c", ()),)),
        OverlaySpec("SAVELOAD", ("asm/overlay/saveload/entry_saveload.asm", *esx_saveload), (("src/spectrum/overlay/saveload_ovl.c", ()), *((source, ()) for source in atomic)), esx_common),
        OverlaySpec("RESTORE", ("asm/overlay/restore/entry_restore.asm",), (("src/spectrum/overlay/restore_ovl.c", ()),)),
        OverlaySpec("ABOUT", ("asm/overlay/about/entry_about.asm",)),
        OverlaySpec("FILEUI", ("asm/overlay/fileui/entry_fileui.asm", *esx_fileui), (("src/spectrum/overlay/fileui_ovl.c", ()),), esx_common),
        OverlaySpec("CONTROL", ("asm/overlay/control/entry_control.asm",), (("src/spectrum/overlay/control_ovl.c", ()), (mach, ()))),
        OverlaySpec("CONFIG", ("asm/overlay/config/entry_config.asm", *esx_saveload), (("src/spectrum/overlay/config_ovl.c", ()), *((source, ()) for source in atomic)), esx_common),
        OverlaySpec("TIME_CONFIG", ("asm/overlay/time_config/entry_time_config.asm", edit_buf)),
    ]
    if env_value(env, "NET_BACKEND") == "spectranext":
        specs.append(OverlaySpec("TIME", ("asm/overlay/time/entry_time.asm",), (("src/spectrum/overlay/time_ovl.c", time_flags), *((source, time_flags) for source in time_sources))))
        specs = [OverlaySpec(
            spec.name,
            spec.asm + (xfs if spec.name in {"SAVELOAD", "CONFIG", "FILEUI"} else ()),
            spec.c_sources + (resolver if spec.name in {"MQTT_CONNECT", "DIRECT", "TIME"} else ()),
            spec.extra_inputs,
            SPXN_XFS_LOCAL_SYMBOLS if xfs and spec.name in {"SAVELOAD", "CONFIG", "FILEUI"} else (),
        ) for spec in specs]
    specs.append(OverlaySpec("NOTICES", ("asm/overlay/notices/entry_notices.asm",), (("src/spectrum/overlay/notices_ovl.c", ()),)))
    return specs


def overlay_output(build_dir: Path, zx_name: str, spec: OverlaySpec) -> Path:
    return build_dir / f"{zx_name}_{spec.name}.OVL"


def spectrum_headers() -> list[Path]:
    return sorted(path for tree in (ROOT / "src/common", ROOT / "src/spectrum")
                  if tree.is_dir() for path in tree.rglob("*.h"))


def spec_inputs(spec: OverlaySpec, map_path: Path, defs: Path, stamp: Path | None,
                headers: list[Path], builder: Path = BUILDER_PATH) -> list[Path]:
    inputs = [map_path, defs, builder]
    if stamp is not None:
        inputs.append(stamp)
    inputs.extend(Path(path) for path in spec.asm)
    inputs.extend(Path(path) for path, _ in spec.c_sources)
    inputs.extend(Path(path) for path in spec.extra_inputs)
    if spec.c_sources:
        inputs.extend(headers)
    return inputs


def is_stale(output: Path, inputs: list[Path]) -> bool:
    if not output.exists():
        return True
    try:
        output_time = output.stat().st_mtime
        return any(path.stat().st_mtime > output_time for path in inputs)
    except OSError:
        return True


def config_stamp(build_dir: Path) -> Path | None:
    for name in ("spectrum_config.json", "nex_config.json"):
        path = build_dir / name
        if path.exists():
            return path
    return None


def require_current_map(map_path: Path, stamp: Path | None) -> None:
    if stamp is None:
        raise SystemExit(f"[ERR] overlay build needs {map_path.parent / 'spectrum_config.json'}")
    if stamp.stat().st_mtime > map_path.stat().st_mtime:
        raise SystemExit(f"[ERR] overlay config {stamp} is newer than {map_path}; relink first")


def run(command: list[str], cwd: Path | None = None) -> None:
    if subprocess.run(command, cwd=cwd, check=False).returncode:
        raise SystemExit(1)


def object_path(work_dir: Path, source: Path) -> Path:
    for candidate in (work_dir / source.with_suffix(".o"),
                      work_dir / f"{source.stem}.o",
                      source.with_suffix(".o")):
        if candidate.exists():
            return candidate
    raise SystemExit(f"[ERR] assembler produced no object for {source}")


def assemble(z80asm: list[str], work_dir: Path, source: Path) -> Path:
    run([*z80asm, f"-O={work_dir.as_posix()}", source.as_posix()])
    return object_path(work_dir, source)


def compile_c(zcc: list[str], cflags: list[str], extra: tuple[str, ...],
              build_dir: Path, build_dir_up: str, source: Path, output: Path) -> None:
    source_arg = source.as_posix() if source.is_absolute() else (Path(build_dir_up) / source).as_posix()
    try:
        output_arg = output.relative_to(build_dir).as_posix()
    except ValueError:
        output_arg = output.as_posix()
    run([*zcc, "+z80", *cflags, *extra, "-c", source_arg, "-o", output_arg], build_dir)


def write_defs(python: str, map_path: Path, defs: Path) -> None:
    result = subprocess.run([python, str(TOOLS / "gen_overlay_defs.py"), str(map_path)],
                            check=False, capture_output=True, text=True)
    if result.stderr:
        sys.stderr.write(result.stderr)
    if result.returncode:
        raise SystemExit(result.returncode)
    replace_if_changed(defs, result.stdout.encode())


def filter_local_symbols(source: Path, output: Path, symbols: tuple[str, ...]) -> Path:
    if not symbols:
        return source
    lines = []
    for line in source.read_text(encoding="utf-8").splitlines(keepends=True):
        fields = line.split()
        if len(fields) >= 2 and fields[0] in ("PUBLIC", "DEFC") and fields[1] in symbols:
            continue
        lines.append(line)
    replace_if_changed(output, "".join(lines).encode())
    return output


def build_one(spec: OverlaySpec, env: dict[str, str], build_dir: Path,
              zx_name: str, map_path: Path, defs: Path, slot: int,
              headers: list[Path], force: bool) -> Path:
    size_limit = int(env_value(env, "OVERLAY_SIZE_LIMIT") or BLOCK_SIZE)
    output = overlay_output(build_dir, zx_name, spec)
    if not force and not is_stale(output, spec_inputs(spec, map_path, defs, config_stamp(build_dir), headers)):
        if output.stat().st_size <= size_limit:
            print(f"  skip {spec.name} ({output.stat().st_size} bytes)")
            return output
    work_dir = build_dir / "ovl" / spec.name
    work_dir.mkdir(parents=True, exist_ok=True)
    z80asm = split_args(env_value(env, "ZX_Z80ASM") or "z80asm")
    zcc = split_args(env_value(env, "ZCC") or "zcc")
    cflags = split_args(env_value(env, "ZX_OVL_CFLAGS"))
    objects = [assemble(z80asm, work_dir, Path(source)) for source in spec.asm]
    for index, (source, extra) in enumerate(spec.c_sources):
        obj = work_dir / f"{Path(source).stem}_{index}.o"
        compile_c(zcc, cflags, extra, build_dir, env_value(env, "BUILD_DIR_UP") or "../", Path(source), obj)
        objects.append(obj)
    for pattern in ("overlay_defs.o", "overlay_defs.o~"):
        for path in work_dir.rglob(pattern):
            path.unlink()
    local_defs = filter_local_symbols(defs, work_dir / "overlay_defs.asm", spec.local_symbols)
    run([*z80asm, f"-O={work_dir.as_posix()}", "-b", "-m", f"-r0x{slot:04X}",
         f"-o={output.name}", *(path.as_posix() for path in objects), local_defs.as_posix()])
    produced = work_dir / output.name
    if not produced.exists():
        produced = work_dir / "build" / output.name
    if not produced.exists():
        raise SystemExit(f"[ERR] assembler did not write {produced}")
    size = produced.stat().st_size
    if size > size_limit:
        raise SystemExit(f"[ERR] {spec.name} is {size} bytes (max {size_limit})")
    output.write_bytes(produced.read_bytes())
    print(f"  built {spec.name}: {size}/{size_limit} bytes")
    return output


def pack_atlas(env: dict[str, str], python: str, build_dir: Path, zx_name: str) -> None:
    run([python, str(TOOLS / "gen_overlay_atlas.py"), "--build-dir", str(build_dir),
         "--name", zx_name, "--out", env_value(env, "ZX_OVL"), "--asm-out",
         env_value(env, "OVL_ATLAS_TABLE"), "--changed-stamp",
         str(build_dir / "overlay_atlas_table.changed"), "--sizes-out",
         str(build_dir / "overlay_sizes.json"),
         "--block-size", env_value(env, "OVERLAY_BLOCK_SIZE") or "2048",
         "--size-limit", env_value(env, "OVERLAY_SIZE_LIMIT") or "2048",
         *split_args(env_value(env, "ATLAS_BINDING_ARGS")),
         *split_args(env_value(env, "OVERLAY_ATLAS_FLAGS"))])


def maybe_rebuild_resident(env: dict[str, str], build_dir: Path) -> None:
    changed = build_dir / "overlay_atlas_table.changed"
    if changed.read_text(encoding="ascii").strip() != "1":
        return
    if env_value(env, "ATLAS_FINAL") == "1":
        raise SystemExit("[ERR] overlay atlas changed during final pass")
    print("[INFO] overlay atlas table changed; rebuilding resident")
    run([*split_args(env_value(env, "MAKE") or "make"), "ATLAS_FINAL=1", env_value(env, "ZX_OVL")])


def main(argv: list[str] | None = None, env: dict[str, str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    only = args[args.index("--only") + 1].upper().replace("-", "_") if "--only" in args else ""
    force = "--force" in args
    environ = dict(os.environ) if env is None else env
    build_dir = Path(env_value(environ, "BUILD_DIR") or "build")
    zx_name = env_value(environ, "ZX_NAME") or "MIRSHIFT"
    map_path = build_dir / f"{zx_name}.map"
    if not map_path.exists():
        raise SystemExit(f"[ERR] overlay build needs {map_path}; run make tap first")
    stamp = config_stamp(build_dir)
    if "--size-check" in args:
        require_current_map(map_path, stamp)
    defs = Path(env_value(environ, "OVL_DEFS") or str(build_dir / "overlay_defs.asm"))
    write_defs(env_value(environ, "PYTHON") or sys.executable, map_path, defs)
    slot = parse_map(map_path).get("_overlay_code_slot")
    block_size = int(env_value(environ, "OVERLAY_BLOCK_SIZE") or BLOCK_SIZE)
    size_limit = int(env_value(environ, "OVERLAY_SIZE_LIMIT") or BLOCK_SIZE)
    expected = {2048: (0x6800, 2048), 4096: (0x2000, 4096),
                8192: (0x6000, 4096)}.get(block_size)
    if (slot, size_limit) != expected:
        raise SystemExit(f"[ERR] overlay slot/limit {(slot, size_limit)!r} must be {expected!r}")
    specs = overlay_specs(environ)
    selected = [spec for spec in specs if not only or spec.name == only]
    if not selected:
        raise SystemExit(f"[ERR] unknown overlay {only!r}")
    headers = spectrum_headers()
    resolver = env_value(environ, "SPXN_RESOLVE_C")
    if resolver:
        headers.extend(sorted(Path(resolver).parent.glob("*.h")))
    for spec in selected:
        build_one(spec, environ, build_dir, zx_name, map_path, defs, slot, headers, force)
    if only:
        return 0
    pack_atlas(environ, env_value(environ, "PYTHON") or sys.executable, build_dir, zx_name)
    maybe_rebuild_resident(environ, build_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
