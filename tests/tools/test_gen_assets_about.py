import io
import json
import re
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from tools import asm_data, gen_assets, make_about_board


ROOT = Path(__file__).resolve().parents[2]
UI = ROOT / "assets/spectrum/ui_runtime_assets.asm"
PIECES = ROOT / "assets/spectrum/checker_pieces_16x16.asm"
SCREEN = ROOT / "asm/spectrum/screen.asm"
ABOUT = ROOT / "assets/spectrum/about_board.bin"
ABOUT_SRC = ROOT / "assets/spectrum/about_classic.scr"
ZX_LOADER = ROOT / "asm/esxdos/overlay_loader.asm"
NEXT_LOADER = ROOT / "asm/next/overlay_loader_next.asm"
ABOUT_ENTRY = ROOT / "asm/overlay/about/entry_about.asm"
MENU_CONFIG_ENTRY = ROOT / "asm/overlay/menu_config/entry_menu_config.asm"
APP = ROOT / "src/spectrum/app/app.c"
GUI = ROOT / "src/spectrum/ui/gui.c"
GUI_H = ROOT / "src/spectrum/ui/gui.h"
RENDER_H = ROOT / "src/spectrum/ui/render.h"


def asm_equ(path, name):
    text = path.read_text(encoding="ascii")
    match = re.search(rf"(?m)^{name}\s+EQU\s+([0-9]+)\s*$", text)
    if not match:
        raise AssertionError(f"{name} not found in {path}")
    return int(match.group(1), 10)


def run_generator(loader, output, *, is_next, version=None):
    if version is None:
        version = (ROOT / "VERSION").read_text(encoding="ascii").strip()
    argv = [
        "gen_assets.py",
        str(UI),
        str(PIECES),
        str(SCREEN),
        str(loader),
        str(ABOUT),
        str(output),
        "--version",
        version,
    ]
    if is_next:
        argv.append("--next")
    with redirect_stdout(io.StringIO()):
        gen_assets.main(argv)


class AboutStructuralTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = ABOUT.read_bytes()
        cls.payload = gen_assets.build_about_payload(cls.raw)
        cls.ui, cls.ui_offsets = gen_assets.parse_defb(
            UI.read_text(encoding="ascii")
        )
        pieces_text = PIECES.read_text(encoding="ascii")
        cls.pieces, _ = gen_assets.parse_defb(
            gen_assets.block_from_label(pieces_text, "netchesszx_piece_sprites_16x16")
        )
        cls.piece_sets = gen_assets.runtime_piece_sets(cls.pieces, str(PIECES))

    def test_payload_is_full_width_ula_band(self):
        self.assertEqual(len(self.raw), 5184)
        self.assertEqual(self.payload, self.raw)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_row_bytes"), 32)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_chunk_bytes"), 448)
        self.assertEqual(
            asm_equ(ABOUT_ENTRY, "about_pixel_chunks")
            * asm_equ(ABOUT_ENTRY, "about_chunk_rows")
            + asm_equ(ABOUT_ENTRY, "about_tail_rows"),
            144,
        )

    def test_shared_defb_parser_preserves_data_offsets_and_errors(self):
        source = (
            "before:\n"
            "    DEFB 1\n"
            "start:\n"
            '    DEFB 0x02, "AB" ; comment\n'
            "middle:\n"
            "    defb 260\n"
            "end:\n"
            "    DEFB 9\n"
        )
        data, offsets = asm_data.parse_defb_with_offsets(source)
        self.assertEqual(data, b"\x01\x02AB\x04\x09")
        self.assertEqual(offsets, {"before": 0, "start": 1, "middle": 4, "end": 5})
        self.assertEqual(
            asm_data.parse_defb_block(source, "start", "end"),
            b"\x02AB\x04",
        )
        with self.assertRaisesRegex(SystemExit, "label not found: missing"):
            asm_data.label_block(source, "missing")
        with self.assertRaisesRegex(SystemExit, "end label not found: missing"):
            asm_data.label_block(source, "start", "missing")

    def test_committed_payload_matches_source_generator(self):
        self.assertEqual(make_about_board.build_board(ABOUT_SRC, UI), self.raw)

    def test_credit_layout_fits_and_keeps_color_rows_separate(self):
        self.assertEqual(
            make_about_board.TEXT_LINES,
            [
                ("MIRRORSHIFT", make_about_board.ATTR_WHITE, 110),
                ("(C) 2026 M.I. MONGE GARCIA", make_about_board.ATTR_WHITE, 116),
                ("GITHUB.COM/IGNACIOMONGE/MIRRORSHIFT", make_about_board.ATTR_YELLOW, 122),
                ("LICENSE: GNU GPL V2.0", make_about_board.ATTR_YELLOW, 128),
            ],
        )
        for text, _color, y in make_about_board.TEXT_LINES:
            self.assertLessEqual(
                make_about_board.text_origin_x(text) + len(text) * 4, 256
            )
            self.assertLessEqual(y + 4, make_about_board.BAND_H)

        attrs = self.raw[make_about_board.SCREEN_W_BYTES * make_about_board.BAND_H :]
        self.assertFalse(any(attr & 0x80 for attr in attrs))
        pixels = self.raw[: make_about_board.SCREEN_W_BYTES * make_about_board.BAND_H]
        for y in range(make_about_board.TEXT_TOP, make_about_board.TEXT_LINES[0][2]):
            start = y * make_about_board.SCREEN_W_BYTES
            self.assertFalse(any(pixels[start : start + make_about_board.SCREEN_W_BYTES]))

    def test_classic_about_restores_its_full_width_footprint(self):
        app = APP.read_text(encoding="ascii")
        gui = GUI.read_text(encoding="ascii")
        screen = SCREEN.read_text(encoding="ascii")
        gui_h = GUI_H.read_text(encoding="ascii")
        render_h = RENDER_H.read_text(encoding="ascii")

        self.assertIn("spectrum_gui_restore_game_center();", app)
        self.assertIn("void spectrum_gui_restore_game_center(void)", gui)
        self.assertIn("spectrum_restore_game_center(gui_live_board);", gui)
        self.assertIn("void spectrum_gui_restore_game_center(void);", gui_h)
        self.assertIn("void spectrum_restore_game_center(const char *board)", render_h)
        self.assertIn("PUBLIC _spectrum_restore_game_center", screen)
        self.assertIn("call restore_game_center_canvas", screen)

    def test_classic_about_blocks_the_blinking_move_marker(self):
        gui = GUI.read_text(encoding="ascii")
        marker = gui[gui.index("static void move_marker_render") :]
        marker = marker[: marker.index("static void move_marker_clear")]

        self.assertIn("if (about_visible == 1u)", marker)
        self.assertLess(marker.index("return;"), marker.index("spec[0]"))

    def test_runtime_constants_match_payload(self):
        menu_config = MENU_CONFIG_ENTRY.read_text(encoding="ascii")
        self.assertEqual(asm_equ(ZX_LOADER, "asset_ui_size"), len(self.ui))
        self.assertEqual(asm_equ(ZX_LOADER, "asset_piece_offset"), len(self.ui))
        self.assertEqual(asm_equ(NEXT_LOADER, "asset_ui_size"), len(self.ui))
        expected_light_attrs = gen_assets.EXPECTED_UI_OFFSETS[
            "board_theme_light_attrs"
        ]
        self.assertIn(
            f"menu_config_dat_light_attrs EQU 0x6000 + {expected_light_attrs}",
            menu_config,
        )
        self.assertEqual(
            asm_equ(ABOUT_ENTRY, "about_payload_offset_classic"), 908
        )
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_payload_offset_next"), 876)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_payload_size"), 5184)
        self.assertEqual(asm_equ(ABOUT_ENTRY, "about_input_size"), 448)
        self.assertEqual(gen_assets.ZX_EXTRA_PIECE_OFFSET, 6092)
        self.assertEqual(gen_assets.EXPECTED_ZX_DAT_BYTES, 6284)
        self.assertEqual(gen_assets.NEXT_EXTRA_PIECE_OFFSET, 876)
        self.assertEqual(gen_assets.EXPECTED_NEXT_DAT_BYTES, 1004)
        loader = ZX_LOADER.read_text(encoding="ascii")
        self.assertEqual(gen_assets.parse_loader_asset_size(loader), 908)
        offset = self.ui_offsets["self_silence_msg"]
        self.assertEqual(offset, 651)
        self.assertEqual(self.ui[offset : offset + 13], b"SELF SILENCE\0")
        offset = self.ui_offsets["echo_silence_msg"]
        self.assertEqual(offset, 664)
        self.assertEqual(self.ui[offset : offset + 13], b"ECHO SILENCE\0")
        self.assertEqual(
            gen_assets.parse_loader_about_size(ZX_LOADER.read_text(encoding="ascii")),
            5184,
        )
        self.assertEqual(
            gen_assets.parse_loader_about_size(NEXT_LOADER.read_text(encoding="ascii")),
            0,
        )

    def test_classic_file_reads_use_chunked_uart_pump(self):
        loader = ZX_LOADER.read_text(encoding="ascii")
        about = ABOUT_ENTRY.read_text(encoding="ascii")

        self.assertEqual(loader.lower().count("defb 0x9d"), 1)
        self.assertIn("call _spectrum_uart_background_pump", loader)
        self.assertIn("EXTERN ovl_read_chunked", about)
        self.assertNotIn("defb 0x9d", about.lower())

    def test_turn_label_and_move_marker_use_independent_frames(self):
        app = APP.read_text(encoding="ascii")
        gui = GUI.read_text(encoding="ascii")

        self.assertIn("white_to_move ? SPECTRUM_GUI_TURN_MARKER_WHITE : 0u", app)
        self.assertIn("spectrum_render_turn_label(label_mode)", gui)
        self.assertIn(
            "move_marker_place((uint8_t)(mode & SPECTRUM_GUI_TURN_MARKER_WHITE))",
            gui,
        )
        self.assertIn("if (white_to_move)", gui)

    def test_turn_label_clear_restores_normal_attributes(self):
        screen = (ROOT / "asm" / "spectrum" / "screen.asm").read_text(
            encoding="utf-8"
        ).lower()
        start = screen.index("clear_turn_strip:")
        end = screen.index("_spectrum_render_notice:", start)
        clear = screen[start:end]
        self.assertIn("ld hl, netchesszx_top_turn_attr_addr", clear)
        self.assertIn("ld b, netchesszx_top_turn_width_bytes", clear)
        self.assertIn("ld (hl), attr_timer", clear)

    def test_next_rejects_classic_loader_and_keeps_nex_dat_layout(self):
        with tempfile.TemporaryDirectory() as temp:
            zx_out = Path(temp) / "zx.dat"
            next_out = Path(temp) / "next.dat"
            with self.assertRaisesRegex(SystemExit, "Next requires the NEX loader"):
                run_generator(ZX_LOADER, zx_out, is_next=True)
            self.assertFalse(zx_out.exists())
            run_generator(NEXT_LOADER, next_out, is_next=True)
            next_dat = next_out.read_bytes()

        next_extras = self.piece_sets[gen_assets.PIECE_SET_BYTES :]
        self.assertEqual(len(next_dat), gen_assets.EXPECTED_NEXT_DAT_BYTES)
        self.assertEqual(
            next_dat[gen_assets.NEXT_EXTRA_PIECE_OFFSET :], next_extras
        )

    def test_classic_dat_interleaves_masks_and_reflections_per_set(self):
        reflections = (
            ROOT / "assets" / "spectrum" / "checker_reflections.bin"
        ).read_bytes()
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "classic.dat"
            run_generator(ZX_LOADER, output, is_next=False)
            data = output.read_bytes()

        offsets = (
            gen_assets.EXPECTED_UI_BYTES,
            gen_assets.ZX_EXTRA_PIECE_OFFSET,
            gen_assets.ZX_EXTRA_PIECE_OFFSET
            + gen_assets.CLASSIC_RUNTIME_PIECE_BYTES,
        )
        for index, offset in enumerate(offsets):
            expected = (
                self.piece_sets[
                    index * gen_assets.PIECE_SET_BYTES :
                    (index + 1) * gen_assets.PIECE_SET_BYTES
                ]
                + reflections[
                    index * gen_assets.REFLECTION_SET_BYTES :
                    (index + 1) * gen_assets.REFLECTION_SET_BYTES
                ]
            )
            self.assertEqual(
                data[offset : offset + gen_assets.CLASSIC_RUNTIME_PIECE_BYTES],
                expected,
            )

    def test_version_is_consistent_across_products(self):
        version = (ROOT / "VERSION").read_text(encoding="ascii").strip()
        match = re.fullmatch(
            r"([0-9]+)\.([0-9]+)(?:\.([0-9]+))?(-dev(?:[0-9]{3}|ESP))?",
            version,
        )
        self.assertIsNotNone(match)
        descriptor_path = ROOT / "zxespemu-launchers.json"
        if descriptor_path.exists():
            launcher_version = (
                f"{match[1]}.{match[2]}.{match[3] or '0'}{match[4] or ''}"
            )
            descriptor = json.loads(descriptor_path.read_text(encoding="utf-8"))
            descriptor_base = re.split(
                r"[-+]", descriptor["version"], maxsplit=1
            )[0]
            self.assertEqual(descriptor_base, launcher_version)

        with tempfile.TemporaryDirectory() as temp:
            classic_out = Path(temp) / "classic.dat"
            next_out = Path(temp) / "next.dat"
            run_generator(ZX_LOADER, classic_out, is_next=False)
            run_generator(NEXT_LOADER, next_out, is_next=True)
            start = gen_assets.EXPECTED_UI_OFFSETS["version_banner_msg"]
            end = start + gen_assets.VERSION_SLOT
            classic_banner = classic_out.read_bytes()[start:end].rstrip(b"\0")
            next_banner = next_out.read_bytes()[start:end].rstrip(b"\0")

        expected_banner = (
            version if "-dev" in version else f"VERSION {version}."
        ).encode("ascii")
        self.assertEqual(classic_banner, expected_banner)
        self.assertEqual(next_banner, expected_banner)

        source = (ROOT / "src/pc/client/main_window.cpp").read_text(encoding="utf-8")
        self.assertRegex(
            source,
            r'setWindowTitle\(QStringLiteral\("Mirror Shift %1"\)\.arg\(\s*'
            r"QString::fromLatin1\(kAppVersion\)\)\);",
        )
        self.assertNotRegex(source, r'setWindowTitle\([^;]*"Mirror Shift [0-9]')

    def test_development_version_keeps_suffix_in_banner(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "dev.dat"
            run_generator(ZX_LOADER, output, is_next=False, version="1.1.0-dev001")
            start = gen_assets.EXPECTED_UI_OFFSETS["version_banner_msg"]
            banner = output.read_bytes()[start : start + gen_assets.VERSION_SLOT]

        self.assertEqual(banner.rstrip(b"\0"), b"1.1.0-dev001")

    def test_esp_development_version_keeps_suffix_in_banner(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "dev-esp.dat"
            run_generator(ZX_LOADER, output, is_next=False, version="1.1.0-devESP")
            start = gen_assets.EXPECTED_UI_OFFSETS["version_banner_msg"]
            banner = output.read_bytes()[start : start + gen_assets.VERSION_SLOT]

        self.assertEqual(banner.rstrip(b"\0"), b"1.1.0-devESP")

    def test_invalid_product_version_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "invalid.dat"
            with self.assertRaisesRegex(SystemExit, "version must contain"):
                run_generator(ZX_LOADER, output, is_next=False, version="build-dev001")

    def test_unknown_about_layout_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            loader = Path(temp) / "loader.asm"
            output = Path(temp) / "bad.dat"
            loader.write_text(
                "asset_ui_size EQU 812\n"
                "asset_load_size EQU 908\n"
                "about_board_size EQU 1361\n",
                encoding="ascii",
            )
            with self.assertRaisesRegex(SystemExit, "expected 5184 or 0"):
                run_generator(loader, output, is_next=False)


if __name__ == "__main__":
    unittest.main()
