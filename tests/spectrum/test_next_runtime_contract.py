#!/usr/bin/env python3
"""Check the renderer split without running the shared Z80 product build."""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def selected(source, defines):
    active = True
    stack = []
    lines = []
    for line in source.splitlines():
        directive = re.match(r"\s*(IFDEF|IFNDEF)\s+(\w+)", line)
        if directive:
            condition = (directive[2] in defines) == (directive[1] == "IFDEF")
            stack.append((active, condition))
            active = active and condition
        elif line.strip() == "ELSE":
            outer, condition = stack[-1]
            stack[-1] = (outer, not condition)
            active = outer and not condition
        elif line.strip() == "ENDIF":
            active = stack.pop()[0]
        elif active:
            lines.append(line)
    assert not stack, "unbalanced renderer partition"
    return "\n".join(lines)


def main():
    screen = (ROOT / "asm/spectrum/screen.asm").read_text(encoding="utf-8")
    extension = selected(screen, {"NETCHESSZX_NEXT", "NETCHESSZX_NEXT_EXTENSION"})
    resident = selected(screen, {"NETCHESSZX_NEXT", "NETCHESSZX_NEXT_RESIDENT"})
    classic = selected(screen, set())
    assert "SECTION bss_user" not in extension, "extension state must stay resident"
    assert "NETCHESSZX_OVERLAY_CONTEXT EQU 0x5ff8" in extension
    assert "piece_sprites_16x16 EQU 0x3b2b" in extension
    assert "piece_sprites_16x16 EQU 0x662b" in classic
    assert "NETCHESSZX_ASSET_BASE EQU 0x6000" in classic
    assert "NETCHESSZX_ASSET_BASE EQU 0x3500" in extension
    assert "piece_slot_squares" not in screen, "Reversi needs all 64 disc slots"

    top = (ROOT / "asm/next/screen_extension_stubs_top.asm").read_text()
    tail = (ROOT / "asm/next/screen_extension_stubs_tail.asm").read_text()
    queue = (ROOT / "asm/spectrum/input_queue.asm").read_text()
    for partition in (extension, resident + "\n" + top + tail + queue):
        labels = set(re.findall(r"(?m)^(\w+):", partition))
        externs = set(re.findall(r"(?m)^EXTERN (\w+)", partition))
        calls = set(re.findall(
            r"(?m)^\s*(?:call|jp|jr)\s+(?:(?:nz|z|nc|c|p|m|pe|po),\s*)?(\w+)",
            partition,
        ))
        # Fixed table addresses are supplied by extension_bank_layout.asm.
        missing = {name for name in calls - labels - externs
                   if not name.startswith("next_ext_")}
        assert not missing, f"unresolved cross-partition calls: {sorted(missing)}"

    graphics = (ROOT / "asm/next/graphics_bank_next.asm").read_text()
    assert "_overlay_code_slot" not in graphics, "graphics must preserve mapped overlays"
    bss = graphics.split("SECTION bss_user\n", 1)[1].split("SECTION code_user", 1)[0]
    assert len(re.findall(r"(?m)^ngb_\w+:\s+DEFS 1$", bss)) == 6
    loader = (ROOT / "asm/next/overlay_loader_next.asm").read_text()
    bss = loader.split("SECTION bss_user\n", 1)[1].split(
        "SECTION code_user", 1
    )[0]
    assert "ovl_call_active EQU next_expand_left" in bss
    entry = loader.split("_spectrum_overlay_exec:\n", 1)[1].split(
        "_spectrum_assets_load:", 1
    )[0]
    assert entry.count("jp nz, ovl_nested_fail") == 2
    assert entry.index("jp nz, ovl_nested_fail") < entry.index("ld (ovl_id), a")
    dispatch = loader.split("ovl_call_loaded:\n", 1)[1].split(
        "ovl_select_atlas_entry:", 1
    )[0]
    assert "ld (ovl_call_active), a" in dispatch
    assert "ovl_nested_fail:\n    ld hl, 0\n    ret" in dispatch
    returned = loader.split("ovl_return:\n", 1)[1].split("ovl_bad_entry:", 1)[0]
    assert returned.index("next_extension_page") < returned.index("    ei")
    assert returned.index("ld (ovl_call_active), a") < returned.index("    ei")
    assert "ld de, _spectrum_overlay_context" in loader
    print("Next runtime partition contract PASS")


if __name__ == "__main__":
    main()
