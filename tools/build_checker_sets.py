#!/usr/bin/env python3
"""Build Classic checker masks and reflection streams from approved PNGs."""

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/spectrum/checker_pieces_16x16.asm"
REFLECTION_OUT = ROOT / "assets/spectrum/checker_reflections.bin"
CHECKER_DIR = ROOT / "assets/editable/checkers"

SETS = (
    ("BW-L", "bw_l"),
    ("BW-M", "bw_m"),
    ("BW-S", "bw_s"),
)
SPRITE_BYTES = 32
SET_BYTES = 64
REFLECTION_FRAME_COUNT = 7
REFLECTION_SET_BYTES = 32
REFLECTION_ROW_BITS = (5, 4, 4)
REFLECTION_X_BASE = (3, 6, 4)


def alpha_mask(path: Path) -> list[bool]:
    image = Image.open(path).convert("RGBA")
    if image.size != (16, 16):
        raise SystemExit(f"{path}: got {image.size}, expected 16x16")
    pixels = list(image.getchannel("A").getdata())
    if set(pixels) - {0, 255}:
        raise SystemExit(f"{path}: alpha must be hard 1-bpp (0 or 255)")
    mask = [pixel == 255 for pixel in pixels]
    if not any(mask):
        raise SystemExit(f"{path}: empty image")
    return mask


def sprite_bytes(mask: list[bool]) -> bytes:
    data = bytearray()
    for y in range(16):
        hi = 0
        lo = 0
        for x in range(8):
            if mask[y * 16 + x]:
                hi |= 0x80 >> x
        for x in range(8, 16):
            if mask[y * 16 + x]:
                lo |= 0x80 >> (x - 8)
        data.extend((hi, lo))
    return bytes(data)


def checker_set(label: str) -> bytes:
    face_a = alpha_mask(CHECKER_DIR / label / "reality-a.png")
    face_b = alpha_mask(CHECKER_DIR / label / "reality-b.png")
    data = sprite_bytes(face_a) + sprite_bytes(face_b)
    if len(data) != SET_BYTES:
        raise SystemExit(f"checker set got {len(data)} bytes, expected {SET_BYTES}")
    return data


def pack_bits(fields: list[tuple[int, int]]) -> bytes:
    data = bytearray()
    accumulator = 0
    bit_count = 0
    for value, width in fields:
        if value < 0 or value >= (1 << width):
            raise SystemExit(f"reflection field {value} does not fit {width} bits")
        accumulator |= value << bit_count
        bit_count += width
        while bit_count >= 8:
            data.append(accumulator & 0xff)
            accumulator >>= 8
            bit_count -= 8
    if bit_count:
        data.append(accumulator & 0xff)
    return bytes(data)


def unpack_reflection(data: bytes, set_index: int) -> list[set[tuple[int, int]]]:
    row_bits = REFLECTION_ROW_BITS[set_index]
    x_base = REFLECTION_X_BASE[set_index]
    bit_offset = 0

    def read(width: int) -> int:
        nonlocal bit_offset
        value = 0
        for bit in range(width):
            value |= ((data[bit_offset >> 3] >> (bit_offset & 7)) & 1) << bit
            bit_offset += 1
        return value

    frames = []
    for _frame in range(REFLECTION_FRAME_COUNT):
        y_start = read(3) + 3
        row_count = read(4) + 1
        delta: set[tuple[int, int]] = set()
        for y in range(y_start, y_start + row_count):
            code = read(row_bits)
            if row_bits == 5:
                if code == 0:
                    continue
                code -= 1
            x_start = x_base + (code >> 1)
            width = (code & 1) + 1
            delta.update((x, y) for x in range(x_start, x_start + width))
        frames.append(delta)
    return frames


def reflection_set(label: str, set_index: int) -> bytes:
    face_b = alpha_mask(CHECKER_DIR / label / "reality-b.png")
    row_bits = REFLECTION_ROW_BITS[set_index]
    x_base = REFLECTION_X_BASE[set_index]
    fields: list[tuple[int, int]] = []
    deltas: list[set[tuple[int, int]]] = []

    for frame_index in range(1, REFLECTION_FRAME_COUNT + 1):
        path = (
            CHECKER_DIR
            / label
            / "reflection"
            / f"reflection-{frame_index:02}.png"
        )
        frame = alpha_mask(path)
        if any(pixel and not base for pixel, base in zip(frame, face_b)):
            raise SystemExit(f"{path}: reflection adds pixels outside reality-b")
        delta = [base and not pixel for pixel, base in zip(frame, face_b)]
        deltas.append(
            {(index & 15, index >> 4) for index, changed in enumerate(delta) if changed}
        )
        changed_rows = [
            y for y in range(16) if any(delta[y * 16 : (y + 1) * 16])
        ]
        if not changed_rows:
            raise SystemExit(f"{path}: reflection frame has no changed pixels")
        y_start = changed_rows[0]
        row_count = changed_rows[-1] - y_start + 1
        if not 3 <= y_start <= 10 or not 1 <= row_count <= 16:
            raise SystemExit(f"{path}: reflection row span cannot be encoded")
        fields.extend(((y_start - 3, 3), (row_count - 1, 4)))

        for y in range(y_start, y_start + row_count):
            xs = [x for x in range(16) if delta[y * 16 + x]]
            if not xs:
                if row_bits != 5:
                    raise SystemExit(f"{path}: empty interior row cannot be encoded")
                fields.append((0, row_bits))
                continue
            width = len(xs)
            if width > 2 or xs != list(range(xs[0], xs[0] + width)):
                raise SystemExit(f"{path}: reflection row is not a 1-2 pixel run")
            code = 2 * (xs[0] - x_base) + (width - 1)
            if row_bits == 5:
                code += 1
            if code < 0 or code >= (1 << row_bits):
                raise SystemExit(f"{path}: reflection row cannot be encoded")
            fields.append((code, row_bits))

    packed = pack_bits(fields)
    if len(packed) > REFLECTION_SET_BYTES:
        raise SystemExit(
            f"{label}: reflection needs {len(packed)} bytes, "
            f"limit is {REFLECTION_SET_BYTES}"
        )
    padded = packed.ljust(REFLECTION_SET_BYTES, b"\0")
    if unpack_reflection(padded, set_index) != deltas:
        raise SystemExit(f"{label}: reflection round-trip mismatch")
    return padded


def emit_defb(lines: list[str], data: bytes) -> None:
    if len(data) != SPRITE_BYTES:
        raise SystemExit(f"sprite got {len(data)} bytes, expected {SPRITE_BYTES}")
    for index in range(0, SPRITE_BYTES, 2):
        lines.append(f"    DEFB 0x{data[index]:02x}, 0x{data[index + 1]:02x}")


def emit_set(
    lines: list[str], label: str, identifier: str, data: bytes, public_labels: bool
) -> None:
    lines.extend(("", f"netchesszx_piece_set_{identifier}:"))
    lines.extend(("", f"; {label}: Reality A and Reality B"))
    emit_defb(lines, data[:SPRITE_BYTES])
    emit_defb(lines, data[SPRITE_BYTES:])


def main() -> int:
    lines = [
        "; Mirror Shift 16x16 checker sprite sets.",
        "; Generated by tools/build_checker_sets.py from approved editable PNG alpha.",
        "; Each set is one physical A/B pair.",
        "",
        "PIECE_SPRITE_WIDTH       EQU 16",
        "PIECE_SPRITE_HEIGHT      EQU 16",
        "PIECE_SPRITE_ROW_BYTES   EQU 2",
        "PIECE_SPRITE_BYTES       EQU 32",
        "PIECE_SPRITE_COUNT       EQU 6",
        "PIECE_SPRITE_SET_BYTES   EQU 64",
        "PIECE_SPRITE_SET_COUNT   EQU 3",
        "",
        "SECTION rodata_user",
        "",
        "PUBLIC netchesszx_piece_sprites_16x16",
    ]
    lines.extend(("", "netchesszx_piece_sprites_16x16:"))

    total = 0
    reflections = bytearray()
    for index, (label, identifier) in enumerate(SETS):
        data = checker_set(label)
        emit_set(lines, label, identifier, data, index == 0)
        total += len(data)
        reflections.extend(reflection_set(label, index))

    OUT.write_text("\n".join(lines) + "\n", encoding="ascii")
    REFLECTION_OUT.write_bytes(reflections)
    print(f"[OK] {OUT}: {total} bytes")
    print(f"[OK] {REFLECTION_OUT}: {len(reflections)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
