"""Execute the production two-event Spectrum input queue in z88dk-ticks."""

import argparse
import shutil
import subprocess
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--build-dir", default="build")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    work = Path(args.build_dir).resolve() / "input-queue"
    work.mkdir(parents=True, exist_ok=True)
    binary = work / "input_queue.bin"
    ram = work / "input_queue.ram"
    binary.unlink(missing_ok=True)
    ram.unlink(missing_ok=True)

    result = subprocess.run(
        [shutil.which("z80asm") or "z80asm", "-b", "-r0x8000", "-O=.",
         "-o=" + binary.name,
         str(root / "tests/spectrum/test_input_queue_vector.asm"),
         str(root / "asm/spectrum/input_queue.asm")],
        cwd=work, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode:
        print(result.stdout, end="")
        print("[ERR] input queue vector assembly failed")
        return 1
    result = subprocess.run(
        [shutil.which("z88dk-ticks") or "z88dk-ticks", "-mz80", "-l", "0x8000",
         "-pc", "8000", "-end", "0", "-counter", "100000", "-output",
         ram.name, binary.name],
        cwd=work, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode or not ram.exists():
        print(result.stdout, end="")
        print("[ERR] input queue vector execution failed")
        return 1
    image = ram.read_bytes()
    if len(image) < 65536 or image[0x7000] != 0:
        checkpoint = image[0x7000] if len(image) >= 0x7001 else -1
        print(f"[ERR] input queue ordering/full/clear/BREAK/MENU failed ({checkpoint})")
        return 1
    print("[OK] input queue: ordering, full, clear, BREAK, MENU")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
