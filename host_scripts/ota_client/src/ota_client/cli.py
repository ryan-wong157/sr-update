import argparse
import logging
import os
import socket
import struct
import threading
import time
import isotp
from pathlib import Path
from typing import Callable
from udsoncan import MemoryLocation
from udsoncan.connections import IsoTPSocketConnection
from udsoncan.client import Client
from udsoncan.exceptions import (
    NegativeResponseException,
    InvalidResponseException,
    UnexpectedResponseException,
    TimeoutException,
)

# NOTE: importing main also runs its udsoncan.setup_logging() call
from .main import RXID, TXID, configure, get_auth_key

# Mirrors mcu/stm32g431cb/Core/Inc/config/flash_config.h and boot_core/Inc/image_format.h
FLASH_PAGE_SIZE_BYTES = 2048
FW_SLOT_A_SIZE_PAGES = 26
FW_SLOT_B_SIZE_PAGES = 27
FW_SLOT_SIZE_BYTES = FW_SLOT_B_SIZE_PAGES * FLASH_PAGE_SIZE_BYTES # slot B, where downloads go: 54 KB
# FW_ACTUAL_SIZE_BYTES: the trailer has a page of its own at the end of each slot (and slot B's
# first page is the swap offset), so the image gets every other page and nothing is taken off for the trailer
FW_MAX_IMAGE_SIZE_BYTES = (FW_SLOT_A_SIZE_PAGES - 1) * FLASH_PAGE_SIZE_BYTES # 50 KB

# Mirrors image_header_t in boot_core/Inc/image_format.h
IMAGE_MAGIC = 0xB0FE673D
IMAGE_HEADER_STRUCT = struct.Struct("<IHHIBBBx")

# Test boot and revert handshake, mirrors app_sdk/sr_app.c. Raw CAN frames, not ISO-TP
HANDSHAKE_TESTER_ID = 0x1F000FFD
HANDSHAKE_STATUS_WAITING = 0x00
HANDSHAKE_STATUS_CONFIRMED = 0x01
HANDSHAKE_STATUS_FAILED = 0x02
# sent by the bootloader instead of the app when it throws the update out, mirrors boot_core/Inc/common_config/boot_config.h
HANDSHAKE_STATUS_REJECTED_INVALID = 0x03
HANDSHAKE_STATUS_REJECTED_OLDER = 0x04
# covers a new image that never handshakes: the 20 s IWDG, the revert swap back, then the old app's revert handshake
HANDSHAKE_WAIT_S = 45.0
HANDSHAKE_RESULT_WAIT_S = 2.0

CAN_EFF_FLAG = 0x80000000
CAN_EFF_MASK = 0x1FFFFFFF
CAN_FRAME_FMT = "=IB3x8s"


def handshake_ecu_id(ecu_id: int) -> int:
    return 0x1F000FFD | (ecu_id << 12)


class HandshakeSocket:
    """Raw SocketCAN socket that only sees one ECU's handshake frames"""
    def __init__(self, interface: str, ecu_id: int):
        self.sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        can_filter = struct.pack("=II", handshake_ecu_id(ecu_id) | CAN_EFF_FLAG, CAN_EFF_FLAG | CAN_EFF_MASK)
        self.sock.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FILTER, can_filter)
        self.sock.bind((interface,))

    def close(self) -> None:
        self.sock.close()

    def send(self, can_id: int, data: bytes) -> None:
        self.sock.send(struct.pack(CAN_FRAME_FMT, can_id | CAN_EFF_FLAG, len(data), data.ljust(8, b"\x00")))

    def recv(self, timeout: float) -> bytes | None:
        """Data of the next handshake frame from the ECU, None on timeout"""
        self.sock.settimeout(max(timeout, 0))
        try:
            raw = self.sock.recv(struct.calcsize(CAN_FRAME_FMT))
        except (TimeoutError, BlockingIOError):
            return None
        _, dlc, data = struct.unpack(CAN_FRAME_FMT, raw)
        return data[:dlc]


FLASH_FILL_BYTE = 0xAA
FLASH_DEFAULT_SIZE_BYTES = 50000 # just under FW_MAX_IMAGE_SIZE_BYTES

# Mirrors boot_core/Inc/common_config/uds_config.h
S3_SERVER_MS = 5000
KEEPALIVE_PERIOD_S = 2.0

SESSION_DEFAULT = 0x01
SESSION_PROGRAMMING = 0x02
SECURITY_LEVEL = 0x01 # seed request = 0x01, send key = 0x02
SIGNATURE_LEN = 64

DID_NAMES = {
    0xF180: "Bootloader version",
    0xF181: "App version",
    0xF186: "Current diagnostic session",
    0xF191: "ECU ID",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Interactive UDS test client for the bootloader")
    parser.add_argument("--key-file", type=Path, help="Path to base64-encoded auth key file")
    parser.add_argument("--interface", default="can0", help="SocketCAN interface (default: can0)")
    parser.add_argument("--no-keepalive", action="store_true", help="Start with the tester present keepalive off")
    parser.add_argument("--verbose", action="store_true", help="Show udsoncan request/response logs")
    return parser.parse_args()


# =================================================================================================
# Input helpers
# =================================================================================================
def prompt(text: str, default: str = "") -> str:
    suffix = f" [{default}]" if default else ""
    answer = input(f"{text}{suffix}: ").strip()
    return answer if answer else default


def prompt_int(text: str, default: int, as_hex: bool = False) -> int:
    """Accepts decimal or 0x-prefixed hex"""
    default_str = f"0x{default:X}" if as_hex else str(default)
    while True:
        answer = prompt(text, default_str)
        try:
            return int(answer, 0)
        except ValueError:
            print(f"  '{answer}' is not a number (use decimal or 0x hex)")


def prompt_yes_no(text: str, default: bool) -> bool:
    answer = prompt(f"{text} (y/n)", "y" if default else "n").lower()
    return answer.startswith("y")


# =================================================================================================
# Test client
# =================================================================================================
class TestCli:
    def __init__(self, client: Client, conn: IsoTPSocketConnection, keepalive: bool, interface: str = "can0"):
        self.client = client
        self.conn = conn
        self.interface = interface
        # udsoncan's client is not thread safe, the keepalive thread shares it with the menu
        self.lock = threading.Lock()
        self.keepalive_enabled = keepalive
        self.stop_event = threading.Event()
        self.keepalive_thread = threading.Thread(target=self.keepalive_loop, daemon=True)

    # Keepalive ===================================================================================
    def keepalive_loop(self) -> None:
        while not self.stop_event.wait(KEEPALIVE_PERIOD_S):
            if not self.keepalive_enabled:
                continue
            try:
                with self.lock:
                    with self.client.suppress_positive_response:
                        self.client.tester_present()
            except Exception:
                # bus might be down or ECU mid-reset, the menu actions will report it properly
                pass

    def toggle_keepalive(self) -> None:
        self.keepalive_enabled = not self.keepalive_enabled
        print(f"Keepalive is now {'ON' if self.keepalive_enabled else 'OFF'}")
        if not self.keepalive_enabled:
            print(f"The server drops back to the default session after {S3_SERVER_MS} ms of silence")

    # 0x80 test service ===========================================================================
    def test_service(self) -> None:
        self.send_raw(b"\x80", as_text=True)

    # 0x10 session control ========================================================================
    def change_session(self) -> None:
        print(f"  0x{SESSION_DEFAULT:02X} = default, 0x{SESSION_PROGRAMMING:02X} = programming, anything else should be rejected")
        session = prompt_int("Session", SESSION_PROGRAMMING, as_hex=True)
        suppress = prompt_yes_no("Suppress positive response", False)
        with self.lock:
            if suppress:
                with self.client.suppress_positive_response(wait_nrc=True):
                    self.client.change_session(session)
                print("Sent, no positive response expected")
                return
            response = self.client.change_session(session)
        data = response.service_data
        print(f"Now in session 0x{data.session_echo:02X}, P2 = {data.p2_server_max * 1000:.0f} ms, P2* = {data.p2_star_server_max * 1000:.0f} ms")

    # 0x11 ECU reset ==============================================================================
    def ecu_reset(self) -> None:
        print("  0x01 = hard reset, anything else should be rejected")
        reset_type = prompt_int("Reset type", 0x01, as_hex=True)
        suppress = prompt_yes_no("Suppress positive response", False)
        with self.lock:
            if suppress:
                with self.client.suppress_positive_response(wait_nrc=True):
                    self.client.ecu_reset(reset_type)
                print("Sent, no positive response expected")
            else:
                self.client.ecu_reset(reset_type)
                print("ECU acknowledged the reset")
        print("Session and security access are back to default/locked after a reset")

    # 0x22 read data by identifier ================================================================
    def read_data(self) -> None:
        for did, name in DID_NAMES.items():
            print(f"  0x{did:04X} = {name}")
        answer = prompt("DIDs to read, comma separated (blank = all)")
        if answer:
            try:
                dids = [int(part, 16) for part in answer.replace("0x", "").split(",")]
            except ValueError:
                print("Could not parse that as a list of hex DIDs")
                return
        else:
            dids = list(DID_NAMES)

        unknown = [did for did in dids if did not in DID_NAMES]
        if unknown:
            # udsoncan refuses to encode DIDs it has no codec for
            print(f"No codec for {', '.join(f'0x{did:04X}' for did in unknown)}, use the raw request option to send these")
            return

        with self.lock:
            response = self.client.read_data_by_identifier(dids)
        for did, value in response.service_data.values.items():
            print(f"  0x{did:04X} {DID_NAMES[did]}: {self.format_did(did, value)}")

    @staticmethod
    def format_did(did: int, value: tuple) -> str:
        if did in (0xF180, 0xF181):
            return ".".join(str(part) for part in value)
        if did == 0xF186:
            names = {SESSION_DEFAULT: "default", SESSION_PROGRAMMING: "programming"}
            return f"0x{value[0]:02X} ({names.get(value[0], 'unknown')})"
        return f"0x{value[0]:03X}"

    # 0x27 security access ========================================================================
    def unlock(self) -> None:
        with self.lock:
            self.client.unlock_security_access(SECURITY_LEVEL)
        print("Security access unlocked")

    def request_seed(self) -> None:
        with self.lock:
            response = self.client.request_seed(SECURITY_LEVEL)
        print(f"Seed: {response.service_data.seed.hex()}")
        print("The server now expects a key, a new seed request replaces this one")

    def send_bad_key(self) -> None:
        print("3 bad keys in a row locks security access out for 60 s")
        request_seed_first = prompt_yes_no("Request a seed first (n = test key without seed)", True)
        with self.lock:
            if request_seed_first:
                self.client.request_seed(SECURITY_LEVEL)
            self.client.send_key(SECURITY_LEVEL + 1, os.urandom(SIGNATURE_LEN))
        print("Server ACCEPTED a random key, this should never happen")

    # 0x3E tester present =========================================================================
    def tester_present(self) -> None:
        suppress = prompt_yes_no("Suppress positive response", False)
        with self.lock:
            if suppress:
                with self.client.suppress_positive_response(wait_nrc=True):
                    self.client.tester_present()
                print("Sent, no positive response expected")
            else:
                self.client.tester_present()
                print("ECU responded to tester present")

    # 0x34 / 0x36 / 0x37 download =================================================================
    def flash(self) -> None:
        print(f"Writes 0x{FLASH_FILL_BYTE:02X} bytes into slot B, max image size is {FW_MAX_IMAGE_SIZE_BYTES} bytes")
        size = prompt_int("Number of bytes", FLASH_DEFAULT_SIZE_BYTES)
        self.download(bytes([FLASH_FILL_BYTE]) * size)

    def flash_file(self) -> None:
        path = Path(prompt("Path to fmt_tool image (e.g. app-img.bin)"))
        if not path.is_file():
            print(f"{path.resolve()} not found")
            return
        image = path.read_bytes()

        if len(image) < IMAGE_HEADER_STRUCT.size:
            print("File is too small to be an image")
            return
        magic, hdr_size, ecu_id, img_size, major, minor, revision = IMAGE_HEADER_STRUCT.unpack_from(image)
        if magic != IMAGE_MAGIC:
            print(f"Bad header magic 0x{magic:08X}, was this made by fmt_tool?")
            return
        if len(image) > FW_MAX_IMAGE_SIZE_BYTES:
            print(f"Image is {len(image)} bytes, max is {FW_MAX_IMAGE_SIZE_BYTES}")
            return
        new_version = f"{major}.{minor}.{revision}"
        print(f"ECU ID 0x{ecu_id:03X}, version {new_version}, {img_size} byte binary, {len(image)} bytes total")

        self.download(image)

        # what's in slot A now is what the ECU falls back to if the update doesn't stick
        with self.lock:
            old = self.client.read_data_by_identifier_first(0xF181)
        old_version = ".".join(str(part) for part in old)
        old_text = "no previous image" if old_version == "0.0.0" else f"version {old_version}"

        # open before the reset so the first handshake frame can't be missed
        handshake = HandshakeSocket(self.interface, ecu_id)
        try:
            with self.lock:
                self.client.ecu_reset(0x01)
            print(f"ECU is resetting to install the update, waiting up to {HANDSHAKE_WAIT_S:.0f} s for the handshake")
            self.confirm_update(handshake, ecu_id, new_version, old_text)
        finally:
            handshake.close()

    def confirm_update(self, handshake: HandshakeSocket, ecu_id: int, new_version: str, old_text: str) -> None:
        """Blocks on the test boot (or revert) handshake and reports whether the update stuck"""
        deadline = time.monotonic() + HANDSHAKE_WAIT_S
        data = None
        while time.monotonic() < deadline:
            data = handshake.recv(deadline - time.monotonic())
            if data is None or len(data) < 4:
                data = None
                continue
            if data[0] in (HANDSHAKE_STATUS_REJECTED_INVALID, HANDSHAKE_STATUS_REJECTED_OLDER):
                reason = "failed verification" if data[0] == HANDSHAKE_STATUS_REJECTED_INVALID else "is older than the running image"
                running = ".".join(str(part) for part in data[1:4])
                print(f"Update rejected by the bootloader: {new_version} {reason}, ECU is still running version {running}")
                return
            if data[0] == HANDSHAKE_STATUS_WAITING:
                break
            data = None
        if data is None:
            print(f"No handshake: the update was rejected or failed to boot, ECU should still be on {old_text}")
            return

        # a reverted app handshakes too, the only tell is that the version didn't change to the new one
        running = ".".join(str(part) for part in data[1:4])
        reverted = running != new_version
        if reverted:
            print(f"ECU reverted to version {running} instead of booting {new_version}, confirming the revert")
        else:
            print(f"ECU is test booting {running}, confirming")

        # resend the confirm until the ECU answers with a result, a frame can get lost on the bus
        deadline = time.monotonic() + HANDSHAKE_RESULT_WAIT_S
        confirm = bytes([(ecu_id >> 8) & 0xFF, ecu_id & 0xFF])
        while time.monotonic() < deadline:
            handshake.send(HANDSHAKE_TESTER_ID, confirm)
            data = handshake.recv(0.2)
            if data is None or len(data) < 4 or data[0] == HANDSHAKE_STATUS_WAITING:
                continue
            if reverted:
                print(f"Update failed, ECU is running version {running}")
            elif data[0] == HANDSHAKE_STATUS_CONFIRMED:
                print(f"Handshake successful, ECU confirmed and is now running version {running}")
            else:
                print(f"ECU failed to confirm the image, it has rolled back to {old_text}")
            return
        print(f"ECU never answered the confirm, it rolls back to {old_text} when its watchdog fires")

    def download(self, image: bytes) -> None:
        """0x34 / 0x36 / 0x37 of image into slot B"""
        size = len(image)
        if prompt_yes_no("Enter programming session and unlock first", True):
            with self.lock:
                self.client.change_session(SESSION_PROGRAMMING)
                self.client.unlock_security_access(SECURITY_LEVEL)
            print("In programming session, security access unlocked")

        with self.lock:
            # address is ignored by the server, it always writes to slot B
            location = MemoryLocation(address=0, memorysize=size, address_format=32, memorysize_format=32)
            # TEMP benchmark: transport time is 0x34 + (first 0x36 .. 0x37), the block size prompt is excluded
            x34_start = time.perf_counter()
            response = self.client.request_download(location)
            x34_s = time.perf_counter() - x34_start
        max_length = response.service_data.max_length
        print(f"Download accepted, server max block length = {max_length}")

        # ISO 14229 counts the SID and sequence counter bytes in the max block length
        block_size = prompt_int("Data bytes per block", max_length - 2)
        if block_size <= 0:
            print("Block size must be at least 1")
            return

        num_blocks = (size + block_size - 1) // block_size
        start = time.perf_counter()
        seq_counter = 1
        for block_num, offset in enumerate(range(0, size, block_size), start=1):
            with self.lock:
                self.client.transfer_data(seq_counter, image[offset:offset + block_size])
            seq_counter = (seq_counter + 1) & 0xFF
            sent = min(offset + block_size, size)
            print(f"\r  block {block_num}/{num_blocks}, {sent}/{size} bytes", end="", flush=True)
        print()

        with self.lock:
            self.client.request_transfer_exit()
        transport_s = x34_s + time.perf_counter() - start
        print(f"Transfer complete, {size} bytes in {transport_s:.1f} s")
        print(f"BENCH transport_us={transport_s * 1e6:.0f} bytes={size} block_size={block_size}")

    def request_download(self) -> None:
        size = prompt_int("Number of bytes", FLASH_DEFAULT_SIZE_BYTES)
        address = prompt_int("Address (ignored by the server)", 0, as_hex=True)
        with self.lock:
            location = MemoryLocation(address=address, memorysize=size, address_format=32, memorysize_format=32)
            response = self.client.request_download(location)
        print(f"Download accepted, server max block length = {response.service_data.max_length}")

    def transfer_block(self) -> None:
        seq_counter = prompt_int("Sequence counter", 1)
        length = prompt_int("Number of data bytes", 254)
        with self.lock:
            self.client.transfer_data(seq_counter & 0xFF, bytes([FLASH_FILL_BYTE]) * length)
        print(f"Block {seq_counter} accepted")

    def transfer_exit(self) -> None:
        with self.lock:
            self.client.request_transfer_exit()
        print("Transfer exit accepted")

    # Raw request =================================================================================
    def raw_request(self) -> None:
        answer = prompt("Request bytes in hex (e.g. 22 F1 80)")
        try:
            payload = bytes.fromhex(answer.replace("0x", "").replace(",", " "))
        except ValueError:
            print("Could not parse that as hex")
            return
        if not payload:
            return
        self.send_raw(payload)

    def send_raw(self, payload: bytes, as_text: bool = False) -> None:
        """Bypasses udsoncan's request building and response checking"""
        with self.lock:
            self.conn.empty_rxqueue()
            self.conn.send(payload)
            response = self.conn.wait_frame(timeout=2)
            while response is not None and len(response) == 3 and response[0] == 0x7F and response[2] == 0x78:
                print("  response pending...")
                response = self.conn.wait_frame(timeout=20)

        if response is None:
            print("No response")
        elif as_text:
            print(f"Response: {response.decode(errors='replace')} ({response.hex(' ')})")
        elif len(response) == 3 and response[0] == 0x7F:
            print(f"Negative response: {response.hex(' ')} (NRC 0x{response[2]:02X})")
        else:
            print(f"Response: {response.hex(' ')}")

    # Menu ========================================================================================
    def run(self) -> None:
        actions: list[tuple[str, str, Callable[[], None]]] = [
            ("1", "Test service (0x80)", self.test_service),
            ("2", "Change session (0x10)", self.change_session),
            ("3", "ECU reset (0x11)", self.ecu_reset),
            ("4", "Read data by identifier (0x22)", self.read_data),
            ("5", "Security access: unlock (0x27)", self.unlock),
            ("6", "Security access: request seed only (0x27)", self.request_seed),
            ("7", "Security access: send a bad key (0x27)", self.send_bad_key),
            ("8", "Tester present (0x3E)", self.tester_present),
            ("9", f"Flash 0x{FLASH_FILL_BYTE:02X} image: full sequence (0x34, 0x36, 0x37)", self.flash),
            ("f", "Flash a fmt_tool image file and confirm it (0x34, 0x36, 0x37, reset, handshake)", self.flash_file),
            ("10", "Request download only (0x34)", self.request_download),
            ("11", "Transfer a single block (0x36)", self.transfer_block),
            ("12", "Request transfer exit only (0x37)", self.transfer_exit),
            ("r", "Raw hex request", self.raw_request),
            ("k", "Toggle tester present keepalive", self.toggle_keepalive),
        ]
        handlers = {key: handler for key, _, handler in actions}

        self.keepalive_thread.start()
        try:
            while True:
                print()
                for key, label, _ in actions:
                    print(f"  {key:>2}) {label}")
                print("   q) Quit")
                print(f"  keepalive: {'ON' if self.keepalive_enabled else 'OFF'}")

                choice = prompt("> ").lower()
                if choice == "q":
                    return
                handler = handlers.get(choice)
                if handler is None:
                    print("Unknown option")
                    continue

                try:
                    handler()
                except NegativeResponseException as e:
                    print(f"\nNegative response: {e.response.code_name} (0x{e.response.code:02X})")
                except (InvalidResponseException, UnexpectedResponseException) as e:
                    print(f"\nBad response: {e}")
                except TimeoutException as e:
                    print(f"\nTimed out: {e}")
        except (KeyboardInterrupt, EOFError):
            print()
        finally:
            self.stop_event.set()
            self.keepalive_thread.join()


def main():
    args = parse_args()
    auth_key = get_auth_key(args.key_file)

    if not args.verbose:
        # main.py's setup_logging() logs every request and frame, too noisy for a menu
        logging.getLogger("UdsClient").setLevel(logging.WARNING)
        logging.getLogger("Connection").setLevel(logging.WARNING)

    conn = IsoTPSocketConnection(args.interface, isotp.Address(isotp.AddressingMode.Normal_29bits, rxid=RXID, txid=TXID))
    config = configure(auth_key)
    # udsoncan caps every request at 5 s total by default, which cuts P2* short
    config["request_timeout"] = None
    with Client(conn, config) as client:
        TestCli(client, conn, keepalive=not args.no_keepalive, interface=args.interface).run()


if __name__ == "__main__":
    main()
