import argparse
import base64
import os
import sys
import udsoncan
import isotp
from pathlib import Path
from typing import Any
from udsoncan.connections import IsoTPSocketConnection
from udsoncan.client import Client
from udsoncan.exceptions import *
from udsoncan.configs import default_client_config
from udsoncan.typing import ClientConfig
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

RXID = 0x1F001000
TXID = 0x1F000000

udsoncan.setup_logging()

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--key-file", type=Path, help="Path to base64-encoded auth key file")
    return parser.parse_args()

def get_auth_key(key_file: Path | None) -> Ed25519PrivateKey:
    if key_file is not None:
        key_base64 = key_file.read_text().strip()
    else:
        key_base64 = os.environ.get("PRIVATE_AUTH_KEY")
        if not key_base64:
            print("error: no auth key provided (use --key-file or set PRIVATE_AUTH_KEY)", file=sys.stderr)
            sys.exit(1)
    return Ed25519PrivateKey.from_private_bytes(base64.b64decode(key_base64))

def security_access_algorithm(level: int, seed: bytes, params: Any) -> bytes:        
    auth_key: Ed25519PrivateKey = params["key"]
    signature = auth_key.sign(seed)
    return signature

def configure(auth_key: Ed25519PrivateKey) -> ClientConfig:
    config = default_client_config.copy()
    config["security_algo"] = security_access_algorithm
    config["security_algo_params"] = {"key": auth_key}
    config["p2_timeout"] = 50 / 1000 # 50 ms
    config["p2_star_timeout"] = 5000 / 1000 # 5000 ms
    config["data_identifiers"] = {
        0xF180: ">BBB", # Bootloader version
        0xF181: ">BBB", # App version
        0xF186: ">B", # Current diagnostic session
        0xF191: ">H", # ECU ID (12 bits big endian) 
    }
    return config

def main():
    args = parse_args()
    auth_key = get_auth_key(args.key_file)

    try:
        conn = IsoTPSocketConnection("can0", isotp.Address(isotp.AddressingMode.Normal_29bits, rxid=RXID, txid=TXID))
    except Exception as e:
        print(f"Failed to open connection: {e}")
        sys.exit(1)

    config = configure(auth_key)
    with Client(conn, config) as client:
        conn.send(b'\x80')
        resp = conn.wait_frame(timeout=20)
        if resp is not None:
            print(resp.decode())
            

if __name__ == "__main__":
    main()