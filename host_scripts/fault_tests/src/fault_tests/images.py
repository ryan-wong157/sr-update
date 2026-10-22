"""Generates every bad image scenario from one good fmt_tool image

Re-signed variants use the signing key the same way fmt_tool does (--key-path, else the
PRIVATE_SIGNING_KEY env var), so the only thing wrong with each one is the thing being tested.
"""
import argparse
import base64
import csv
import hashlib
import os
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

from fmt_tool.main import (
    HEADER_FORMAT,
    HEADER_MAGIC,
    HEADER_PADDED_SIZE_BYTES,
    TL_FORMAT,
    TLV_ED25519_SIG_ID,
    TLV_HEADER_FORMAT,
    TLV_HEADER_MAGIC,
    TLV_SHA512_ID,
)
from ota_client import cli as ota

# version given to every bad image that could otherwise get past the version check. Never older
# than what is installed, and never equal to it, so a revert can be told apart from a confirm
BAD_VERSION = (255, 255, 255)


@dataclass
class Header:
    ecu_id: int
    img_size: int
    version: tuple[int, int, int]
    magic: int = HEADER_MAGIC
    hdr_size: int = HEADER_PADDED_SIZE_BYTES

    def pack(self) -> bytes:
        return struct.pack(HEADER_FORMAT, self.magic, self.hdr_size, self.ecu_id, self.img_size, *self.version) \
            .ljust(HEADER_PADDED_SIZE_BYTES, b"\x00")


def tlv(entries: list[tuple[int, bytes]], magic: int = TLV_HEADER_MAGIC, total_size: int | None = None) -> bytes:
    body = b"".join(struct.pack(TL_FORMAT, tlv_type, len(value)) + value for tlv_type, value in entries)
    if total_size is None:
        total_size = struct.calcsize(TLV_HEADER_FORMAT) + len(body)
    return struct.pack(TLV_HEADER_FORMAT, magic, total_size) + body


def sign(header: bytes, payload: bytes, key: Ed25519PrivateKey) -> tuple[bytes, bytes]:
    """(sha512, signature) the way fmt_tool makes them"""
    sha = hashlib.sha512(header + payload).digest()
    return sha, key.sign(sha)


def signed_image(header: Header, payload: bytes, key: Ed25519PrivateKey) -> bytes:
    packed = header.pack()
    sha, sig = sign(packed, payload, key)
    return packed + payload + tlv([(TLV_SHA512_ID, sha), (TLV_ED25519_SIG_ID, sig)])


def flip(data: bytes, index: int) -> bytes:
    return data[:index] + bytes([data[index] ^ 0xFF]) + data[index + 1:]


def older(version: tuple[int, int, int]) -> tuple[int, int, int] | None:
    major, minor, revision = version
    if revision:
        return major, minor, revision - 1
    if minor:
        return major, minor - 1, 255
    if major:
        return major - 1, 255, 255
    return None


def load_key(key_path: Path | None) -> Ed25519PrivateKey:
    if key_path is not None:
        key_base64 = key_path.read_text().strip()
    else:
        key_base64 = os.environ.get("PRIVATE_SIGNING_KEY")
        if not key_base64:
            sys.exit("error: no signing key (use --key-path or set PRIVATE_SIGNING_KEY)")
    return Ed25519PrivateKey.from_private_bytes(base64.b64decode(key_base64))


def scenarios(image: bytes, key: Ed25519PrivateKey) -> list[tuple[str, bytes, str]]:
    """[(name, image, expected outcome)] built from one good signed image"""
    magic, hdr_size, ecu_id, img_size, major, minor, revision = struct.unpack_from(HEADER_FORMAT, image)
    if magic != HEADER_MAGIC or hdr_size != HEADER_PADDED_SIZE_BYTES:
        sys.exit("error: input is not an fmt_tool image (bad header magic or size)")
    version = (major, minor, revision)
    payload = image[hdr_size:hdr_size + img_size]
    good_tlv = image[hdr_size + img_size:]

    def bad_header(**overrides) -> Header:
        fields = dict(ecu_id=ecu_id, img_size=len(payload), version=BAD_VERSION) | overrides
        return Header(**fields)

    bad = bad_header()
    bad_packed = bad.pack()
    sha, sig = sign(bad_packed, payload, key)
    good_entries = [(TLV_SHA512_ID, sha), (TLV_ED25519_SIG_ID, sig)]
    signed_bad = signed_image(bad, payload, key)
    tlv_offset = len(bad_packed) + len(payload)
    # largest image 0x34 accepts, the header claims a payload that runs past the slot
    max_payload = ota.FW_MAX_IMAGE_SIZE_BYTES - HEADER_PADDED_SIZE_BYTES

    out = [
        # 1.2 header
        ("no_header", payload + good_tlv, "rejected_invalid"),
        ("bad_header_magic", signed_image(bad_header(magic=HEADER_MAGIC ^ 0xFFFFFFFF), payload, key), "rejected_invalid"),
        ("bad_header_size", signed_image(bad_header(hdr_size=512), payload, key), "rejected_invalid"),
        ("header_size_field_too_big", signed_image(bad_header(img_size=len(payload) + 2048), payload, key), "rejected_invalid"),
        ("header_size_field_too_small", signed_image(bad_header(img_size=max(len(payload) - 2048, 1)), payload, key), "rejected_invalid"),
        ("header_size_field_past_slot", signed_image(bad_header(img_size=max_payload + 2048), payload, key), "rejected_invalid"),

        # 1.3 TLV
        ("no_tlv", bad_packed + payload, "rejected_invalid"),
        ("bad_tlv_magic", bad_packed + payload + tlv(good_entries, magic=TLV_HEADER_MAGIC ^ 0xFFFF), "rejected_invalid"),
        ("tlv_no_hash", bad_packed + payload + tlv([(TLV_ED25519_SIG_ID, sig)]), "rejected_invalid"),
        ("tlv_no_signature", bad_packed + payload + tlv([(TLV_SHA512_ID, sha)]), "rejected_invalid"),
        ("tlv_total_size_too_big", bad_packed + payload + tlv(good_entries, total_size=0xFFFF), "rejected_invalid"),
        ("tlv_entry_length_wrong", bad_packed + payload + tlv([(TLV_SHA512_ID, sha[:32]), (TLV_ED25519_SIG_ID, sig)]), "rejected_invalid"),

        # 1.4 too big, the download is refused before anything is written
        ("too_big", signed_image(bad_header(img_size=max_payload + 1), payload.ljust(max_payload + 1, b"\xFF"), key), "0x34 NRC 0x31"),

        # 1.5 empty
        ("zero_length_payload", signed_image(bad_header(img_size=0), b"", key), "rejected_invalid"),
        ("header_only", bad_packed, "rejected_invalid"),

        # 1.6 truncated
        ("truncated_mid_payload", signed_bad[:tlv_offset // 2], "rejected_invalid"),
        ("truncated_mid_tlv", signed_bad[:-32], "rejected_invalid"),

        # 1.7 garbage that is correctly formatted and signed, boots, crashes, IWDG reverts it
        ("signed_garbage", signed_image(bad, os.urandom(len(payload)), key), "reverted (after the 20 s IWDG)"),

        # 1.8 changed after signing
        ("payload_changed_after_signing", flip(signed_bad, len(bad_packed) + len(payload) // 2), "rejected_invalid"),
        ("header_changed_after_signing", bad_header(version=(254, 254, 254)).pack() + signed_bad[HEADER_PADDED_SIZE_BYTES:], "rejected_invalid"),

        # 1.9 / 1.10 signature
        ("bad_hash", flip(signed_bad, tlv_offset + 4 + 4), "rejected_invalid"),
        ("bad_signature", flip(signed_bad, len(signed_bad) - 1), "rejected_invalid"),
        ("wrong_key", signed_image(bad, payload, Ed25519PrivateKey.generate()), "rejected_invalid"),

        # 1.11 another ECU's image, flash with --ecu-id of the real ECU to hear the rejection
        ("wrong_ecu", signed_image(bad_header(ecu_id=ecu_id ^ 0x800), payload, key), "rejected_invalid"),
    ]

    # 1.12 / 1.13 versions, relative to the good image so install that first
    older_version = older(version)
    if older_version is not None:
        out.append(("older_version", signed_image(Header(ecu_id, len(payload), older_version), payload, key), "rejected_older"))
    # the input image itself, equal versions are allowed. Before newer_version, which installs a
    # higher version and would make this one older
    out.append(("same_version", image, "confirmed"))
    newer = (major, minor, revision + 1) if revision < 255 else (major, minor + 1, 0)
    out.append(("newer_version", signed_image(Header(ecu_id, len(payload), newer), payload, key), "confirmed"))
    return out


def main():
    parser = argparse.ArgumentParser(description="Write every bad image scenario from one good fmt_tool image")
    parser.add_argument("image", type=Path, help="Good signed image made by fmt_tool (e.g. app-img.bin)")
    parser.add_argument("-o", "--out", type=Path, help="Output folder (default: <image name>-faults next to the image)")
    parser.add_argument("--key-path", type=Path, help="Base64 ed25519 signing key file (default: PRIVATE_SIGNING_KEY env var)")
    args = parser.parse_args()

    image = args.image.read_bytes()
    out_dir = args.out or args.image.with_name(f"{args.image.stem}-faults")
    out_dir.mkdir(parents=True, exist_ok=True)

    rows = []
    for index, (name, data, expected) in enumerate(scenarios(image, load_key(args.key_path)), start=1):
        path = out_dir / f"{index:02d}_{name}.bin"
        path.write_bytes(data)
        rows.append({"file": path.name, "bytes": len(data), "expected": expected})
        print(f"{path.name:<40}{len(data):>7} B  expect {expected}")

    with (out_dir / "manifest.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["file", "bytes", "expected"])
        writer.writeheader()
        writer.writerows(rows)
    print(f"\n{len(rows)} images in {out_dir.resolve()}")


if __name__ == "__main__":
    main()
