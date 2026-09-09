#!/usr/bin/env python3
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCREEN = ROOT / "asm" / "spectrum" / "screen.asm"
LOADER = ROOT / "asm" / "next" / "overlay_loader_next.asm"
GRAPHICS_BANK = ROOT / "asm" / "next" / "graphics_bank_next.asm"
GRAPHICS_LAYOUT = ROOT / "asm" / "next" / "extension_bank_layout.asm"
GEN_ASSETS = ROOT / "tools" / "gen_assets.py"
BUILD_SPRITES = ROOT / "tools" / "build_next_checker_sprites.py"
SPRITE_PALETTE_BIN = ROOT / "assets" / "next" / "checker_sprite_palette.bin"
MENU_CONFIG = ROOT / "asm" / "overlay" / "menu_config" / "entry_menu_config.asm"
SETUP = ROOT / "asm" / "overlay" / "setup" / "entry_setup.asm"


def block(text: str, start: str, end: str) -> str:
    return text[text.index(start) : text.index(end, text.index(start))]


class NextBoardThemeRgb333Tests(unittest.TestCase):
    def setUp(self) -> None:
        self.screen = SCREEN.read_text(encoding="utf-8")
        self.loader = LOADER.read_text(encoding="utf-8")
        self.graphics_bank = GRAPHICS_BANK.read_text(encoding="utf-8")
        self.graphics_layout = GRAPHICS_LAYOUT.read_text(encoding="utf-8")
        self.gen_assets = GEN_ASSETS.read_text(encoding="utf-8")
        self.build_sprites = BUILD_SPRITES.read_text(encoding="utf-8")
        self.sprite_palette_bin = SPRITE_PALETTE_BIN.read_bytes()
        self.menu_config = MENU_CONFIG.read_text(encoding="utf-8")
        self.setup = SETUP.read_text(encoding="utf-8")

    def test_exact_rgb333_pairs_and_private_attributes(self) -> None:
        self.assertIn("NEXT_BOARD_COORD_LINE_ATTR EQU 0x81", self.screen)
        self.assertIn("NEXT_BOARD_COORD_SELECTED_ATTR EQU 0x8a", self.screen)
        table = block(
            self.screen,
            "next_board_coord_rgb333:",
            "next_board_coord_rgb333_end:",
        )
        values = [
            int(value, 16)
            for value in re.findall(r"0x([0-9a-fA-F]{2})", table)
        ]
        self.assertEqual(
            values,
            [
                0x7F, 0x01, 0x4E, 0x00,
                0x5F, 0x00, 0x29, 0x00,
                0xD7, 0x01, 0x45, 0x01,
                0xF6, 0x00, 0x44, 0x01,
            ],
        )

        frame = block(
            self.screen,
            "restore_board_frame_attrs:",
            "draw_one_board_square:",
        )
        self.assertIn("call board_light_line_attr", frame)
        self.assertIn("ld a, NEXT_BOARD_COORD_LINE_ATTR", frame)

    def test_palette_is_live_before_private_attributes_are_painted(self) -> None:
        initial = block(
            self.screen,
            "_spectrum_render_board:",
            "_spectrum_render_board_area:",
        )
        self.assertLess(
            initial.index("call next_board_coord_palette_sync"),
            initial.index("call draw_board_coords"),
        )
        apply_theme = block(
            self.screen,
            "_netchesszx_board_theme_apply:",
            "compute_screen_base:",
        )
        self.assertLess(
            apply_theme.index("call next_board_coord_palette_sync"),
            apply_theme.index("call restore_board_frame_attrs"),
        )
        self.assertNotIn("NEXTREG_ULA_CONTROL", apply_theme)

    def test_standard_ula_groups_and_menu_tables_stay_on_main_contract(self) -> None:
        self.assertIn(
            "NEXT_BOARD_LIGHT_ATTRS = [0x78, 0x6F, 0x66, 0x77, 0x37]",
            self.gen_assets,
        )
        self.assertIn(
            "NEXT_BOARD_DARK_ATTRS = [0x07, 0x4D, 0x20, 0x56, 0x52]",
            self.gen_assets,
        )
        self.assertIn('EDITABLE_ROOT = ROOT / "assets/editable/next"', self.build_sprites)
        self.assertNotIn("OBSIDIAN_FACE_A", self.build_sprites)
        table = block(
            self.build_sprites,
            "STANDARD_ULA_PALETTE = bytes((",
            "))",
        )
        values = [
            int(value, 16)
            for value in re.findall(r"0x([0-9a-fA-F]{2})", table)
        ]
        self.assertEqual(
            values,
            [
                0x00,0x00, 0x02,0x01, 0xA0,0x00, 0xA2,0x01,
                0x14,0x00, 0x16,0x01, 0xB4,0x00, 0xB6,0x01,
                0x00,0x00, 0x02,0x01, 0xA0,0x00, 0xA2,0x01,
                0x14,0x00, 0x16,0x01, 0xB4,0x00, 0xB6,0x01,
                0x00,0x00, 0x03,0x01, 0xE0,0x00, 0xE3,0x01,
                0x1C,0x00, 0x1F,0x01, 0xFC,0x00, 0xFF,0x01,
                0x00,0x00, 0x03,0x01, 0xE0,0x00, 0xE3,0x01,
                0x1C,0x00, 0x1F,0x01, 0xFC,0x00, 0xFF,0x01,
                *([0x00, 0x00] * 3), 0xDF,0x01,
                0xFF,0x01, 0xFF,0x01, 0xFF,0x01, 0xFF,0x01,
                *([0x00, 0x00] * 3), 0xB7,0x00,
                0xBB,0x01, 0x96,0x01, 0xB3,0x00, 0xB2,0x00,
                0xBB,0x00, 0xDF,0x01, 0xDF,0x00, 0xDB,0x01,
                0xFB,0x00, *([0x00, 0x00] * 3),
                0x92,0x01, 0x97,0x00, 0x72,0x00, 0x8E,0x01,
                0x8D,0x01, *([0x00, 0x00] * 3),
            ],
        )
        self.assertEqual(len(self.sprite_palette_bin), 256 * 2 + len(values))
        self.assertEqual(self.sprite_palette_bin[318:320], bytes((0x1F, 0x01)))
        self.assertEqual(self.sprite_palette_bin[-len(values) :], bytes(values))
        self.assertIn("ld e, 192", self.graphics_bank)
        init = block(
            self.graphics_bank,
            "ngb_sprite_system_init:",
            "ngb_palette_upload_pairs:",
        )
        self.assertLess(
            init.index("call ngb_palette_upload_pairs"),
            init.index("ld a, nextreg_ula_control"),
        )
        self.assertIn("or 0x08", init)
        self.assertIn("nextreg_ula_control", self.graphics_layout)
        self.assertRegex(
            self.graphics_layout,
            r"(?m)^next_ula_standard_palette_size\s+EQU 128$",
        )
        self.assertIn("PUBLIC nextreg_read", self.loader)
        self.assertIn("PUBLIC nextreg_write", self.loader)
        self.assertIn("next_sprite_flash_palette_index    EQU 159", self.graphics_layout)
        self.assertIn("call next_piece_flash_palette_sync", self.screen)

    def test_next_menu_swatches_preserve_ula_palette_groups(self) -> None:
        swatch = block(
            self.menu_config,
            "menu_config_board_swatch:",
            "menu_config_board_swatch_store:",
        )
        self.assertRegex(
            swatch,
            r"IFDEF NETCHESSZX_NEXT\s+jr menu_config_board_swatch_store\s+ELSE[\s\S]*?or 0x40\s+ENDIF",
        )
        self.assertIn(
            "menu_config_next_board_swatch_attrs:\n"
            "    DEFB 0xc0,0xc9,0xd2,0xdb,0xe4",
            self.menu_config,
        )
        self.assertIn("ld a, b\n    add a, 3", swatch)
        self.assertIn("or d\n    or 0x80", swatch)
        self.assertIn(
            "menu_config_next_board_swatch_pattern:\n"
            "    DEFB 0xfe,0xfc,0xf8,0xf0,0xe0,0xc0,0x80,0x00",
            self.menu_config,
        )
        self.assertIn("call menu_config_next_board_swatch_pixels", self.menu_config)
        self.assertNotIn("ld a, 0x68", self.menu_config)
        self.assertIn(
            'DEFB 15, "BOARD  ", 127, "   ", 127, "   ", 127, "   ", '
            '127, "   ", 127, 0',
            self.menu_config,
        )

    def test_next_board_focus_previews_theme_through_existing_action(self) -> None:
        focus = block(self.setup, "su_focus_board:", "su_focus_set:")
        self.assertIn("call su_cycle_choice", focus)
        self.assertIn("set 3, (hl)", focus)
        self.assertNotIn("IFDEF NETCHESSZX_NEXT", focus)

if __name__ == "__main__":
    unittest.main()
