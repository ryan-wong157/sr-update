"""
STM32 Binary image formatter tool. For OTA firmware updates
Written by Ryan Wong    
"""
import sys
import os
import base64
import argparse
import struct
from pathlib import Path
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

HEADER_FORMAT = "<IHHIBBBx" # x is the reserved byte in the version
# Header is padded out to this so the binary's vector table is aligned, must match IMAGE_HEADER_SIZE
HEADER_PADDED_SIZE_BYTES = 1024
TLV_HEADER_FORMAT = "<HH"
TL_FORMAT = "<HH"

TLV_HEADER_SIZE_BYTES = 4
TL_SIZE_BYTES = 4

HEADER_MAGIC = 0xb0fe673d
TLV_HEADER_MAGIC = 0xcd34
TLV_SHA512_ID = 0x01
TLV_ED25519_SIG_ID = 0x02
gh_private_key_base64 = str(os.environ.get("PRIVATE_SIGNING_KEY"))

def prep_header(ecu_id: int, image_size: int, version: tuple[int, int, int]) -> bytes:
    major, minor, revision = version
    header = struct.pack(
        HEADER_FORMAT,
        HEADER_MAGIC,
        HEADER_PADDED_SIZE_BYTES,
        ecu_id,
        image_size,
        major,
        minor,
        revision
    )
    return header.ljust(HEADER_PADDED_SIZE_BYTES, b"\x00")

def prep_sha512(path: Path, header: bytes) -> bytes:
    digest = hashes.Hash(hashes.SHA512())
    digest.update(header)
    with open(path, mode="rb") as file:
        while True:
            chunk = file.read(4096)
            if not chunk:
                break
            digest.update(chunk)
    return digest.finalize()

def prep_ed25519(sha512: bytes, key: Ed25519PrivateKey) -> bytes:
    return key.sign(sha512)

def validate_ecu_id(id: str) -> int:
    try:
        val = int(id, 16) 
    except ValueError:
        raise argparse.ArgumentTypeError(f"must be a 12-bit hex value (e.g. 0x123), got '{id}'")
    if val < 0 or val > 0xFFF:
        raise argparse.ArgumentTypeError(f"0x{val:X} out of 12-bit range (0x000–0xFFF)")
    return val

def validate_version(version: str) -> tuple[int, int, int]:
    # strip so a version read straight out of a file with a trailing newline still works
    parts = version.strip().split(".")
    try:
        if len(parts) != 3:
            raise ValueError
        values = tuple(int(part) for part in parts)
    except ValueError:
        raise argparse.ArgumentTypeError(f"must be major.minor.revision (e.g. 1.2.3), got '{version}'")
    if any(value < 0 or value > 0xFF for value in values):
        raise argparse.ArgumentTypeError(f"each part of the version must be 0-255, got '{version}'")
    return values

def main():
    parser = argparse.ArgumentParser(description="SR-update image format tool")
    parser.add_argument("--ecu-id", type=validate_ecu_id, required=True, help="12 bit ECU ID as hex (e.g. 0x111)")
    parser.add_argument("--image-version", type=validate_version, required=True, help="Firmware version as major.minor.revision, each 0-255 (e.g. 1.2.3)")
    parser.add_argument("--key-path", default=None, help="Relative path to base64 ed25519 private key file if exists")
    parser.add_argument("target", help="Relative path to binary")
    args = parser.parse_args()
        
    bin_path = Path(args.target).resolve()
    if not bin_path.is_file():
        print(f"Path: {bin_path} not found")
        sys.exit(1)
    
    try:
        if args.key_path and Path(args.key_path).exists():
            raw_bytes = base64.b64decode(Path(args.key_path).read_text())
            private_key = Ed25519PrivateKey.from_private_bytes(raw_bytes)    
        else:
            raw_bytes = base64.b64decode(gh_private_key_base64)
            private_key = Ed25519PrivateKey.from_private_bytes(raw_bytes)
    except Exception as e:
        print(f"Reading private key failed: {e}")
        sys.exit(1)

    bin_name = bin_path.stem
    bin_size = bin_path.stat().st_size
    new_file = bin_path.parent / f"{bin_name}-img.bin"
    
    with open(bin_path, mode="rb") as old_bin, open(new_file, mode="wb") as new_bin:
        header = prep_header(args.ecu_id, bin_size, args.image_version)
        new_bin.write(header)
        while True:
            chunk = old_bin.read(4096)
            if not chunk:
                break
            new_bin.write(chunk)
        
        sha = prep_sha512(bin_path, header)
        sig = prep_ed25519(sha, private_key)
        
        # size = tlv_header + TLV x 2 
        tlv_total_size = len(sha) + len(sig) + 2 * TL_SIZE_BYTES + TLV_HEADER_SIZE_BYTES
        tlv_header = struct.pack(TLV_HEADER_FORMAT, TLV_HEADER_MAGIC, tlv_total_size)
        sha_tl = struct.pack(TL_FORMAT, TLV_SHA512_ID, len(sha))
        sig_tl = struct.pack(TL_FORMAT, TLV_ED25519_SIG_ID, len(sig))
        new_bin.write(tlv_header)
        new_bin.write(sha_tl)
        new_bin.write(sha)
        new_bin.write(sig_tl)        
        new_bin.write(sig)
    
if __name__ == "__main__":
    main()