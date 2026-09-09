import json
import unittest
from pathlib import Path

from PIL import Image
from tools import build_checker_sets


ROOT = Path(__file__).resolve().parents[2]
EDITABLE_CHECKERS = ROOT / "assets/editable/checkers"
CURSOR = ROOT / "assets/editable/cursor.png"
CLASSIC = ROOT / "assets/spectrum/checker_pieces_16x16.asm"
CLASSIC_REFLECTIONS = ROOT / "assets/spectrum/checker_reflections.bin"
NEXT = ROOT / "assets/next/checker_piece_sprites.bin"
NEXT_META = ROOT / "assets/next/checker_piece_sprites.json"
NEXT_ABOUT = ROOT / "assets/next/about_screen.nxi"
NEXT_EDITABLE = ROOT / "assets/editable/next"
MAKEFILE = ROOT / "Makefile"
QT_RENDERER = ROOT / "src/pc/client/piece_renderer.cpp"
QT_BUILD = ROOT / "client/CMakeLists.txt"
QT_PACKAGE = ROOT / "client/build-msvc.ps1"
QT_BANNER = ROOT / "src/pc/client/app_banner.cpp"
QT_BANNER_HEADER = ROOT / "src/pc/client/app_banner.h"
QT_WORDMARK = ROOT / "assets/pc-client/mirrorshift-wordmark.png"
QT_ABOUT = ROOT / "assets/pc-client/about/mirrorshift-about.png"
QT_WINDOW = ROOT / "src/pc/client/main_window.cpp"
ZX_SCREEN = ROOT / "asm/spectrum/screen.asm"
ZX_BOARD_OVERLAY = ROOT / "asm/overlay/board/entry_board.asm"
ZX_OVERLAY_LOADER = ROOT / "asm/esxdos/overlay_loader.asm"
ZX_UI = ROOT / "assets/spectrum/ui_runtime_assets.asm"
ZX_GUI = ROOT / "src/spectrum/ui/gui.c"
ZX_RENDER_H = ROOT / "src/spectrum/ui/render.h"
ZX_APP = ROOT / "src/spectrum/app/app.c"


def classic_bytes() -> bytes:
    text = CLASSIC.read_text(encoding="ascii")
    body = text.split("netchesszx_piece_sprites_16x16:", 1)[1]
    data = bytearray()
    for line in body.splitlines():
        line = line.split(";", 1)[0].strip()
        if not line.upper().startswith("DEFB"):
            continue
        data.extend(int(token.strip(), 0) for token in line[4:].split(","))
    return bytes(data)


def alpha_bytes(path: Path) -> bytes:
    with Image.open(path) as image:
        rgba = image.convert("RGBA")
        assert rgba.size == (16, 16)
        alpha = list(rgba.getchannel("A").getdata())
    assert set(alpha) <= {0, 255}
    out = bytearray()
    for y in range(16):
        word = sum((1 << (15 - x)) for x in range(16) if alpha[y * 16 + x])
        out.extend(word.to_bytes(2, "big"))
    return bytes(out)


def next_visible(pattern: bytes) -> bytes:
    out = bytearray()
    for y in range(16):
        word = sum(
            (1 << (15 - x))
            for x in range(16)
            if pattern[y * 16 + x] != 0xE3
        )
        out.extend(word.to_bytes(2, "big"))
    return bytes(out)


def rgb333_palette(raw: bytes):
    palette = []
    for first, second in zip(raw[0::2], raw[1::2]):
        values = (first >> 5, (first >> 2) & 7, ((first & 3) << 1) | (second & 1))
        palette.append(tuple(round(value * 255 / 7) for value in values))
    return palette


def decoded_sprite(pattern: bytes, palette):
    pixels = [
        (0, 0, 0, 0) if index == 0xE3 else palette[index] + (255,)
        for index in pattern
    ]
    image = Image.new("RGBA", (16, 16))
    image.putdata(pixels)
    return image


class CheckerAssetTests(unittest.TestCase):
    def test_classic_slots_are_checker_pairs(self):
        data = classic_bytes()
        self.assertEqual(len(data), 3 * 64)
        sets = [data[offset : offset + 64] for offset in range(0, len(data), 64)]
        for set_name, piece_set in zip(("BW-L", "BW-M", "BW-S"), sets):
            sprites = [
                piece_set[offset : offset + 32]
                for offset in range(0, 64, 32)
            ]
            self.assertNotEqual(sprites[0], sprites[1])
            self.assertEqual(sprites[0], alpha_bytes(EDITABLE_CHECKERS / set_name / "reality-a.png"))
            self.assertEqual(sprites[1], alpha_bytes(EDITABLE_CHECKERS / set_name / "reality-b.png"))

    def test_classic_reflections_round_trip_exactly(self):
        packed = CLASSIC_REFLECTIONS.read_bytes()
        self.assertEqual(len(packed), 3 * build_checker_sets.REFLECTION_SET_BYTES)
        for set_index, set_name in enumerate(("BW-L", "BW-M", "BW-S")):
            block = packed[
                set_index * build_checker_sets.REFLECTION_SET_BYTES :
                (set_index + 1) * build_checker_sets.REFLECTION_SET_BYTES
            ]
            decoded = build_checker_sets.unpack_reflection(block, set_index)
            base_b = build_checker_sets.alpha_mask(
                EDITABLE_CHECKERS / set_name / "reality-b.png"
            )
            for frame_index, delta in enumerate(decoded, 1):
                frame = build_checker_sets.alpha_mask(
                    EDITABLE_CHECKERS
                    / set_name
                    / "reflection"
                    / f"reflection-{frame_index:02}.png"
                )
                rebuilt = [
                    base and (index & 15, index >> 4) not in delta
                    for index, base in enumerate(base_b)
                ]
                self.assertEqual(rebuilt, frame)

    def test_classic_reflection_scheduler_is_single_and_non_nested(self):
        gui = ZX_GUI.read_text(encoding="ascii")
        render_h = ZX_RENDER_H.read_text(encoding="ascii")
        screen = ZX_SCREEN.read_text(encoding="ascii")
        board = ZX_BOARD_OVERLAY.read_text(encoding="ascii")
        loader = ZX_OVERLAY_LOADER.read_text(encoding="ascii")
        app = ZX_APP.read_text(encoding="ascii")

        self.assertIn(
            "#if !defined(NETCHESSZX_NEXT) && !defined(NETCHESSZX_HOST_TEST)",
            gui,
        )
        self.assertIn("spectrum_piece_reflection_tick();", gui)
        self.assertIn("add a, 15", screen)
        self.assertNotIn("piece_reflection_target", screen)
        self.assertIn("ld (piece_reflection_wait), a", screen)
        self.assertIn("EXTERN _spectrum_gui_active_coord_row", board)
        self.assertIn("add a, 13", board)
        self.assertIn("ld b, 64", board)
        self.assertIn("MIRRORSHIFT_REFLECT_FRAME", board)
        self.assertIn("call boa_wait_frame", board)
        self.assertNotIn("boa_reflect_skip_frame", board)
        self.assertEqual(board.count("ld hl, MIRRORSHIFT_PIECE_REFLECTION"), 1)
        self.assertIn("ovl_call_active: DEFS 1", loader)
        self.assertGreaterEqual(loader.count("jp nz, ovl_nested_fail"), 2)
        self.assertIn("ld (ovl_call_active), a", loader)
        self.assertIn("#ifndef NETCHESSZX_NEXT", render_h)
        self.assertIn("spectrum_piece_reflection_dispatch_pending", render_h)
        dispatch = screen.split(
            "_spectrum_piece_reflection_dispatch_pending:", 1
        )[1].split("ENDIF", 1)[0]
        self.assertIn("ld a, (piece_reflection_wait)", dispatch)
        self.assertIn("ret nz", dispatch)
        self.assertIn("jr piece_reflection_ready", dispatch)
        poll = app.split("poll_status = netchesszx_session_poll", 1)[1]
        idle = poll.split(
            "if (poll_status == NETCHESSZX_SESSION_POLL_NONE)", 1
        )[1].split("#endif", 1)[0]
        self.assertIn("spectrum_piece_reflection_dispatch_pending();", idle)

    def test_next_piece_slots_include_derived_flash_outlines(self):
        raw = NEXT.read_bytes()
        palette = rgb333_palette((ROOT / "assets/next/checker_sprite_palette.bin").read_bytes()[:320])
        self.assertEqual(len(raw), 12800)
        patterns = [raw[offset : offset + 256] for offset in range(0, 36 * 256, 256)]
        for set_index, set_name in enumerate(("BW-L", "BW-M", "BW-S")):
            piece_set = patterns[set_index * 12 : (set_index + 1) * 12]
            # Preserve the bank size; duplicated pairs 1/2 now carry markers.
            self.assertNotEqual(piece_set[0], piece_set[1])
            source_a = Image.open(
                NEXT_EDITABLE / "pieces" / set_name / "reality-a.png"
            ).convert("RGBA")
            source_b = Image.open(
                NEXT_EDITABLE / "pieces" / set_name / "reality-b.png"
            ).convert("RGBA")
            for theme in range(5):
                for source, offset in ((source_a, 0), (source_b, 1)):
                    expected = source
                    if theme in (1, 2):
                        marker = Image.open(NEXT_EDITABLE / "markers" /
                            ("cursor.png" if theme == 1 else "selected.png")).convert("RGBA")
                        expected = Image.alpha_composite(source, marker)
                    self.assertEqual(
                        list(decoded_sprite(piece_set[theme * 2 + offset], palette).getdata()),
                        list(expected.getdata()),
                    )
            for source, outline in ((source_a, piece_set[10]), (source_b, piece_set[11])):
                source_pixels = list(source.getdata())
                outline_pixels = list(decoded_sprite(outline, palette).getdata())
                for y in range(16):
                    for x in range(16):
                        index = y * 16 + x
                        if source_pixels[index][3]:
                            self.assertEqual(outline_pixels[index], source_pixels[index])
                            continue
                        neighbours = (
                            source_pixels[ny * 16 + nx][3]
                            for ny in range(max(0, y - 1), min(16, y + 2))
                            for nx in range(max(0, x - 1), min(16, x + 2))
                            if nx != x or ny != y
                        )
                        expected = (0, 255, 255, 255) if any(neighbours) else (0, 0, 0, 0)
                        self.assertEqual(outline_pixels[index], expected)
        meta = json.loads(NEXT_META.read_text(encoding="utf-8"))
        self.assertEqual(meta["material"], "editable-rgb333")
        self.assertEqual(meta["sets"], ["BW-L", "BW-M", "BW-S"])
        self.assertEqual(meta["pipeline"], 13)
        self.assertEqual(
            meta["pattern_layout"],
            "A,B,cursorA,cursorB,selectedA,selectedB,A,B,A,B,outlineA,outlineB",
        )
        self.assertEqual(meta["editable_piece_sources"], 6)
        self.assertEqual(meta["flash_palette_index"], 159)
        self.assertEqual(meta["sprite_palette_entries"], 256)
        self.assertEqual(meta["shimmer_mode"], "per-sprite-palette-offset")
        self.assertEqual(meta["shimmer_entry_sample"], [6, 3])
        self.assertEqual(meta["shimmer_exit_sample"], [10, 9])
        self.assertEqual(meta["shimmer_palette_offsets"], {"entry": 4, "exit": 12})
        self.assertEqual(
            meta["marker_pattern_layout"],
            "hint,cursor,hint+cursor,selected",
        )

    def test_next_white_discs_have_no_interior_alpha_holes(self):
        for set_name in ("BW-L", "BW-M", "BW-S"):
            white = Image.open(
                NEXT_EDITABLE / "pieces" / set_name / "reality-a.png"
            ).convert("RGBA")
            white_alpha = list(white.getchannel("A").getdata())
            for row in range(16):
                white_x = [
                    col for col in range(16) if white_alpha[row * 16 + col]
                ]
                if not white_x:
                    continue
                self.assertEqual(
                    white_x,
                    list(range(min(white_x), max(white_x) + 1)),
                    f"{set_name} white disc row {row} contains an alpha hole",
                )

    def test_next_board_tiles_do_not_reserve_a_solid_grid_pixel(self):
        for path in sorted((NEXT_EDITABLE / "boards").glob("*.png")):
            with Image.open(path) as source:
                tile = source.convert("RGBA")
            top = [tile.getpixel((x, 0)) for x in range(16)]
            left = [tile.getpixel((0, y)) for y in range(16)]
            self.assertGreater(len(set(top)), 1, f"{path.name} reserves its top row")
            self.assertGreater(len(set(left)), 1, f"{path.name} reserves its left col")

    def test_next_shimmer_palettes_only_lift_the_sampled_colour_bands(self):
        patterns = NEXT.read_bytes()
        palette = rgb333_palette(
            (ROOT / "assets/next/checker_sprite_palette.bin").read_bytes()[:512]
        )
        meta = json.loads(NEXT_META.read_text(encoding="utf-8"))
        palette_entries = meta["palette_entries"]
        entry_indices = set()
        exit_indices = set()

        for set_index in range(3):
            for side in range(2):
                pattern_index = set_index * 12 + side
                pattern = patterns[pattern_index * 256 : (pattern_index + 1) * 256]
                entry_indices.add(pattern[3 * 16 + 6])
                exit_indices.add(pattern[9 * 16 + 10])

        self.assertTrue(entry_indices.isdisjoint(exit_indices))

        def lifted(rgb):
            levels = [round(component * 7 / 255) for component in rgb]
            if levels == [7, 7, 7]:
                levels = [6, 7, 7]
            else:
                levels = [min(7, level + 1) for level in levels]
            return tuple(round(level * 255 / 7) for level in levels)

        for index in range(palette_entries):
            base = palette[index]
            self.assertEqual(
                palette[64 + index],
                lifted(base) if index in entry_indices else base,
            )
            self.assertEqual(
                palette[192 + index],
                lifted(base) if index in exit_indices else base,
            )

    def test_classic_and_next_cursors_follow_their_editable_sources(self):
        expected_classic = alpha_bytes(CURSOR)

        screen = ZX_SCREEN.read_text(encoding="ascii")
        body = screen.split("board_cursor_mask:", 1)[1].split("ENDIF", 1)[0]
        classic = bytearray()
        for line in body.splitlines():
            line = line.split(";", 1)[0].strip()
            if line.upper().startswith("DEFB"):
                classic.extend(int(token.strip(), 0) for token in line[4:].split(","))
        self.assertEqual(bytes(classic), expected_classic[:6] + expected_classic[26:])

        raw = NEXT.read_bytes()
        cursor_pattern = raw[47 * 256 : 48 * 256]
        combined_pattern = raw[48 * 256 : 49 * 256]
        expected_next = alpha_bytes(NEXT_EDITABLE / "markers/cursor.png")
        self.assertEqual(next_visible(cursor_pattern), expected_next)

        palette = rgb333_palette(
            (ROOT / "assets/next/checker_sprite_palette.bin").read_bytes()[:320]
        )
        hint = Image.open(NEXT_EDITABLE / "markers/hint.png").convert("RGBA")
        cursor = Image.open(NEXT_EDITABLE / "markers/cursor.png").convert("RGBA")
        expected_combined = Image.alpha_composite(cursor, hint)
        self.assertEqual(
            list(decoded_sprite(combined_pattern, palette).getdata()),
            list(expected_combined.getdata()),
        )

        makefile = MAKEFILE.read_text(encoding="utf-8")
        self.assertIn("NEXT_SPRITE_SOURCE_PNGS", makefile)

    def test_qt_uses_procedural_glass_tokens_without_packaged_checkers(self):
        renderer = QT_RENDERER.read_text(encoding="utf-8")
        build = QT_BUILD.read_text(encoding="utf-8")
        self.assertIn("static const GlassTheme kGlassThemes[]", renderer)
        self.assertIn("drawFragmentFace", renderer)
        self.assertNotIn("MIRRORSHIFT_CHECKER_ASSET_SOURCE", build)
        self.assertNotIn("MIRRORSHIFT_CHECKER_ASSET_DEST", build)

    def test_qt_wordmark_contract(self):
        banner = QT_BANNER.read_text(encoding="utf-8")
        build = QT_BUILD.read_text(encoding="utf-8")
        package = QT_PACKAGE.read_text(encoding="utf-8")
        header = QT_BANNER_HEADER.read_text(encoding="utf-8")
        with Image.open(QT_WORDMARK) as image:
            self.assertEqual(image.size, (1734, 252))
            self.assertEqual(image.mode, "RGBA")
            self.assertEqual(image.getchannel("A").getextrema(), (0, 255))
        self.assertIn(
            'assets/pc-client/mirrorshift-wordmark.png', banner
        )
        self.assertIn("painter.drawImage(logoRect, logoImage_)", banner)
        self.assertIn("Qt::KeepAspectRatio", banner)
        self.assertIn("QPainter::CompositionMode_Screen", banner)
        self.assertIn("QImage logoImage_", header)
        self.assertIn("MIRRORSHIFT_DESKTOP_WORDMARK", build)
        self.assertIn("pc-client-wordmark", build)
        self.assertIn("mirrorshift-wordmark.png", package)
        self.assertIn('QStringLiteral("Two sides. One reality")', banner)
        self.assertIn("Qt::AlignRight | Qt::AlignTop", banner)
        self.assertIn("Qt::AlignLeft | Qt::AlignVCenter", banner)
        self.assertNotIn("versionText_", banner)
        self.assertNotIn("VERSION %1", banner)
        self.assertIn("scheduleNextShine", banner)
        self.assertIn("18000", banner)
        self.assertIn("bounded(18001u)", banner)
        self.assertIn("QBasicTimer shineTimer_", header)
        self.assertIn("mirrorPalette", banner)
        self.assertIn("QColor(0, 0, 0, 244)", banner)
        self.assertIn("QColor(10, 148, 211, 220)", banner)
        self.assertIn("QColor(91, 211, 244, 206)", banner)
        self.assertIn("fragment.setAlpha", banner)
        self.assertIn("extraMirrorMosaic", banner)
        self.assertIn("mosaicLeft", banner)
        self.assertIn("for (int column = 0", banner)
        self.assertIn("painter.fillRect(pixel.x, pixel.y", banner)
        self.assertIn("painter.drawLine", banner)

    def test_qt_about_art_contract(self):
        window = QT_WINDOW.read_text(encoding="utf-8")
        build = QT_BUILD.read_text(encoding="utf-8")
        package = QT_PACKAGE.read_text(encoding="utf-8")
        with Image.open(QT_ABOUT) as image:
            self.assertEqual(image.size, (696, 525))
            self.assertEqual(image.mode, "RGBA")
            self.assertEqual(image.getchannel("A").getextrema(), (255, 255))
        self.assertIn(
            'assets/pc-client/about/mirrorshift-about.png', window
        )
        self.assertIn("scaledToWidth", window)
        self.assertIn("MIRRORSHIFT_DESKTOP_ABOUT_ART", build)
        self.assertIn("pc-client-about-art", build)
        self.assertIn('"about"', package)
        self.assertIn(
            'href=\\\"https://github.com/IgnacioMonge/MirrorShift\\\"', window
        )
        self.assertIn("github.com/IgnacioMonge/MirrorShift</a>", window)

    def test_next_about_round_trips_editable_png(self):
        raw = NEXT_ABOUT.read_bytes()
        makefile = MAKEFILE.read_text(encoding="utf-8")
        self.assertEqual(len(raw), 512 + 256 * 192)
        self.assertGreater(len(set(raw[512:])), 32)
        palette = rgb333_palette(raw[:512])
        decoded = [palette[index] + (255,) for index in raw[512:]]
        source = Image.open(NEXT_EDITABLE / "about.png").convert("RGBA")
        self.assertEqual(source.size, (256, 192))
        self.assertEqual(decoded, list(source.getdata()))
        self.assertIn(
            "NEXT_ABOUT_NXI := assets/next/about_screen.nxi", makefile
        )
        self.assertIn("$(NEXT_ABOUT_NXI): tools/build_next_about.py $(NEXT_ABOUT_PNG)", makefile)

    def test_spectrum_mirrored_brand_and_echoes_contract(self):
        screen = ZX_SCREEN.read_text(encoding="utf-8")
        ui = ZX_UI.read_text(encoding="ascii")
        banner = screen.split("draw_banner:", 1)[1].split(
            "draw_banner_info:", 1
        )[0]
        self.assertIn("call draw_scaled_text", banner)
        self.assertNotIn("IFDEF NETCHESSZX_NEXT", banner)
        self.assertIn("cp 'r'", screen)
        self.assertIn("dsg_mirror_nibble:", screen)
        mirror_block = screen.split("dsg_mirror_nibble:", 1)[1].split(
            "dsg_nibble_ready:", 1
        )[0]
        self.assertIn("add a, a", mirror_block)
        banner_info = screen.split("draw_banner_info:", 1)[1].split(
            "draw_menu:", 1
        )[0]
        self.assertIn("ld b, 9", banner_info)
        self.assertIn("NETCHESSZX_BANNER_INFO_COL EQU 22", screen)
        self.assertIn("banner_shine_tick:", screen)
        self.assertIn("ld a, r", screen)
        self.assertIn("add a, 4", screen)
        self.assertIn('DEFB "MIrRORSHIFT",0,0', ui)
        self.assertIn('DEFB "ECHOES",0', ui)


if __name__ == "__main__":
    unittest.main()
