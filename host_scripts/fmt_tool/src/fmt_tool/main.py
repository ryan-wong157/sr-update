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

HEADER_FORMAT = "<IHHIBBH"
TLV_HEADER_FORMAT = "<HH"
TL_FORMAT = "<HH"

TLV_HEADER_SIZE_BYTES = 4
TL_SIZE_BYTES = 4

HEADER_MAGIC = 0xb0fe673d
TLV_HEADER_MAGIC = 0x6907
TLV_SHA256_ID = 0x01
TLV_ED25519_SIG_ID = 0x02
gh_private_key_base64 = str(os.environ.get("PRIVATE_SIGNING_KEY"))

def prep_header(ecu_id: int, image_size: int) -> bytes:
    header = struct.pack(
        HEADER_FORMAT, 
        HEADER_MAGIC, 
        struct.calcsize(HEADER_FORMAT), 
        ecu_id,
        image_size,
        1, # major
        0, # minor
        0 # version
    )
    return header

def prep_sha256(path: Path, header: bytes) -> bytes:
    digest = hashes.Hash(hashes.SHA256())
    digest.update(header)
    with open(path, mode="rb") as file:
        while True:
            chunk = file.read(4096)
            if not chunk:
                break
            digest.update(chunk)
    return digest.finalize()

def prep_ed25519(sha256: bytes, key: Ed25519PrivateKey) -> bytes:
    return key.sign(sha256)

def validate_ecu_id(id: str) -> int:
    try:
        val = int(id, 16) 
    except ValueError:
        raise argparse.ArgumentTypeError(f"must be a 12-bit hex value (e.g. 0x123), got '{id}'")
    if val < 0 or val > 0xFFF:
        raise argparse.ArgumentTypeError(f"0x{val:X} out of 12-bit range (0x000–0xFFF)")
    return val

def main():
    parser = argparse.ArgumentParser(description="SR-update image format tool")
    parser.add_argument("--ecu-id", type=validate_ecu_id, required=True, help="12 bit ECU ID as hex (e.g. 0x111)")
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
        header = prep_header(args.ecu_id, bin_size)
        new_bin.write(header)
        while True:
            chunk = old_bin.read(4096)
            if not chunk:
                break
            new_bin.write(chunk)
        
        sha = prep_sha256(bin_path, header)
        sig = prep_ed25519(sha, private_key)
        
        # size = tlv_header + TLV x 2 
        tlv_total_size = len(sha) + len(sig) + 2 * TL_SIZE_BYTES + TLV_HEADER_SIZE_BYTES
        tlv_header = struct.pack(TLV_HEADER_FORMAT, TLV_HEADER_MAGIC, tlv_total_size)
        sha_tl = struct.pack(TL_FORMAT, TLV_SHA256_ID, len(sha))
        sig_tl = struct.pack(TL_FORMAT, TLV_ED25519_SIG_ID, len(sig))
        new_bin.write(tlv_header)
        new_bin.write(sha_tl)
        new_bin.write(sha)
        new_bin.write(sig_tl)        
        new_bin.write(sig)
    
if __name__ == "__main__":
    main()