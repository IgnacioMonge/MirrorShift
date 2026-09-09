#!/usr/bin/env python3
"""Generate Mirror Shift's original 16x16 RGB333 Next board tiles."""

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/editable/next/boards"
LEVELS = (0, 36, 73, 109, 146, 182, 219, 255)

PALETTES = {
    "black-and-white": (
        ((182, 219, 219), (219, 255, 255), (146, 182, 182), (182, 182, 219)),
        ((146, 146, 182), (182, 182, 219), (73, 73, 109), (109, 109, 146)),
    ),
    "blue3": (
        ((219, 255, 255), (255, 255, 255), (146, 182, 219), (182, 219, 255)),
        ((146, 182, 219), (182, 219, 219), (73, 109, 146), (109, 146, 182)),
    ),
    "green": (
        ((219, 255, 219), (255, 255, 255), (146, 219, 182), (182, 219, 219)),
        ((109, 146, 146), (146, 182, 182), (36, 73, 73), (73, 109, 109)),
    ),
    "brown": (
        ((219, 219, 255), (255, 255, 255), (182, 182, 219), (219, 182, 255)),
        ((146, 109, 182), (182, 146, 219), (73, 36, 109), (109, 73, 146)),
    ),
    "wood": (
        ((255, 219, 219), (255, 255, 255), (219, 182, 146), (255, 219, 182)),
        ((146, 109, 109), (182, 146, 146), (73, 36, 36), (109, 73, 73)),
    ),
}


def tile(colours: tuple[tuple[int, int, int], ...]) -> Image.Image:
    base, light, dark, accent = colours
    pixels = []
    for y in range(16):
        for x in range(16):
            colour = base
            if x + y <= 4 or (y == 0 and x < 10) or (x == 0 and y < 10):
                colour = light
            elif x + y >= 26 or (y == 15 and x > 5) or (x == 15 and y > 5):
                colour = dark
            elif (x, y) in ((11, 2), (12, 2), (11, 3), (3, 11), (3, 12), (2, 12)):
                colour = accent
            pixels.append((*colour, 255))
    image = Image.new("RGBA", (16, 16))
    image.putdata(pixels)
    return image


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    for theme, pair in PALETTES.items():
        for face, colours in zip(("light", "dark"), pair):
            if any(component not in LEVELS for colour in colours for component in colour):
                raise SystemExit(f"{theme}-{face}: non-RGB333 component")
            tile(colours).save(OUT / f"{theme}-{face}.png", optimize=True)
    print(f"[OK] wrote {len(PALETTES) * 2} original board tiles")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
