#!/usr/bin/env python3
"""Sanity-check a firmware image built for the UV-K1 / UV-K5(8) v3.

The check is intentionally simple and does not need the radio:

* the first word of the image is the initial stack pointer,
* the second word is the reset handler address (with the Thumb bit set),
* both point into the memory ranges of the PY32F071xB,
* the image fits in the 118 KiB application flash area.

Usage:
    python3 scripts/verify_image.py build/uv-k1-k5v3/cfw_uv-k1-k5v3.bin
"""

from __future__ import annotations

import pathlib
import struct
import sys

APP_BASE = 0x08002800
APP_END = 0x08020000          # 128 KiB flash
RAM_BASE = 0x20000000
RAM_END = 0x20004000          # 16 KiB RAM
MAX_IMAGE_SIZE = APP_END - APP_BASE


def fail(message: str) -> None:
    print(f"verify: FAIL: {message}")
    raise SystemExit(1)


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: verify_image.py <firmware.bin>")

    image = pathlib.Path(sys.argv[1])
    if not image.is_file():
        fail(f"{image} does not exist")

    data = image.read_bytes()
    if len(data) < 8:
        fail(f"{image} is too small to contain a vector table")

    stack_pointer, reset_handler = struct.unpack_from("<II", data)

    if not (RAM_BASE < stack_pointer <= RAM_END):
        fail(f"initial stack pointer 0x{stack_pointer:08x} is outside RAM")
    if not (APP_BASE <= reset_handler < APP_END):
        fail(f"reset handler 0x{reset_handler:08x} is outside application flash")
    if not (reset_handler & 1):
        fail(f"reset handler 0x{reset_handler:08x} does not have the Thumb bit set")
    if len(data) > MAX_IMAGE_SIZE:
        fail(f"image is {len(data)} bytes, maximum is {MAX_IMAGE_SIZE} bytes")

    print(f"verify: OK: {image}")
    print(f"  size         : {len(data)} bytes ({MAX_IMAGE_SIZE - len(data)} bytes free)")
    print(f"  stack pointer: 0x{stack_pointer:08x}")
    print(f"  reset handler: 0x{reset_handler:08x}")


if __name__ == "__main__":
    main()
