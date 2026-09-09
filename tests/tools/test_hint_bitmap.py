from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
RULES_SOURCE = ROOT / "asm/overlay/rules/rules_stub.asm"
SCREEN_SOURCE = ROOT / "asm/spectrum/screen.asm"
APP_SOURCE = ROOT / "src/spectrum/app/app.c"
GUI_SOURCE = ROOT / "src/spectrum/ui/gui.c"


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

    square_draw = screen.split("draw_one_board_square:", 1)[1].split(
        "clear_square_pixels_2x2:", 1
    )[0]
    assert "ld a, (render_skip_clear)" in square_draw
    assert "ld a, (mark_mode)" not in square_draw

    app = APP_SOURCE.read_text(encoding="utf-8")
    for start, end in (
        ("static void handle_opponent_disconnected_with", "static void handle_opponent_disconnected"),
        ("static void mqtt_peer_reset_wait_state", "static void mqtt_peer_disconnected_wait"),
    ):
        teardown = app.rsplit(start, 1)[1].split(end, 1)[0]
        assert teardown.index("reset_board_moves_chat();") < teardown.index(
            "spectrum_gui_hide_board_pieces();"
        )

    gui = GUI_SOURCE.read_text(encoding="utf-8")
    redraw_all = gui.split("void spectrum_gui_redraw_board_squares", 1)[1].split(
        "static void spectrum_gui_redraw_board_flip_squares", 1
    )[0]
    assert "gui_board_cell(row, col) != '.'" not in redraw_all

    print("Hint and disconnect redraw modes match the NetChessZX ownership model")


if __name__ == "__main__":
    main()
