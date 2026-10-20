"""TEMP thesis benchmark helper: writes a raw binary of an exact size for fmt_tool to sign.

A pure filler binary isn't a runnable app, so it never handshakes and the ECU reverts after the 20 s IWDG.
Pass --base with a real app .bin to pad that out instead, so the image still boots and confirms.
"""
import argparse
from pathlib import Path

# 50 KB image slot - 1024 byte header - 140 bytes of TLVs (4 header + 2 x (4 + 64)), mirrors fmt_tool/ota_client
MAX_RAW_SIZE_BYTES = 25 * 2048 - 1024 - 140


def main():
    parser = argparse.ArgumentParser(description="Write a binary of an exact size, filled with one byte value")
    parser.add_argument("size", type=lambda s: int(s, 0), help=f"Output size in bytes (max {MAX_RAW_SIZE_BYTES} to fit the slot once signed)")
    parser.add_argument("-o", "--output", type=Path, help="Output path (default: fill_<size>.bin)")
    parser.add_argument("--fill", type=lambda s: int(s, 0), default=0xAA, help="Fill byte, decimal or 0x hex (default: 0xAA)")
    parser.add_argument("--base", type=Path, help="Real app binary to pad out to size instead of pure filler")
    args = parser.parse_args()

    if not 0 <= args.fill <= 0xFF:
        parser.error("--fill must be a single byte (0-255)")
    if args.size > MAX_RAW_SIZE_BYTES:
        print(f"warning: {args.size} bytes won't fit in the slot once signed, max is {MAX_RAW_SIZE_BYTES}")

    data = args.base.read_bytes() if args.base else b""
    if len(data) > args.size:
        parser.error(f"--base is already {len(data)} bytes, bigger than {args.size}")
    data += bytes([args.fill]) * (args.size - len(data))

    output = args.output or Path(f"fill_{args.size}.bin")
    output.write_bytes(data)
    print(f"Wrote {len(data)} bytes to {output}")


if __name__ == "__main__":
    main()
