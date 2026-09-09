from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
RULES_SOURCE = ROOT / "asm/overlay/rules/rules_stub.asm"
SCREEN_SOURCE = ROOT / "asm/spectrum/screen.asm"


def main() -> None:
    source = RULES_SOURCE.read_text(encoding="utf-8")
    start = source.index("rh_draw_to:")
    end = source.index("rh_rot_loop:", start)
    seed = source[start:end]

    assert seed.index("xor a") < seed.index("scf")

    for col in range(8):
        value = 0
        carry = 1
        for _ in range(col + 1):
            next_carry = (value >> 7) & 1
            value = ((value << 1) | carry) & 0xFF
            carry = next_carry
        assert value == 1 << col

    screen = SCREEN_SOURCE.read_text(encoding="ascii")
    redraw = screen.split("_spectrum_render_square_with_hint:", 1)[1].split(
        "render_hint_common:", 1
    )[0]
    assert redraw.index("ld (mark_mode), a") < redraw.index(
        "call _spectrum_render_square"
    )
    marked = screen.split("render_hint_from_mark_spec:", 1)[1].split(
        "ENDIF", 1
    )[0]
    assert marked.index("ld (mark_mode), a") < marked.index(
        "jp render_hint_common"
    )

    print("Hint bitmap seeds columns and clears cursor mode before redraw")


if __name__ == "__main__":
    main()
