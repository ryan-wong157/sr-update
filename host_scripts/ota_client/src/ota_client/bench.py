"""TEMP thesis benchmark: flashes the same fmt_tool image over and over and appends each run's timings to a CSV"""
import argparse
import csv
import logging
import time
from datetime import datetime
from pathlib import Path
from udsoncan import MemoryLocation
from udsoncan.connections import IsoTPSocketConnection
from udsoncan.client import Client
from udsoncan.exceptions import (
    InvalidResponseException,
    NegativeResponseException,
    TimeoutException,
    UnexpectedResponseException,
)

from .main import configure, get_auth_key
from .cli import (
    BENCH_BOOT_FIELDS,
    BENCH_TX_FIELDS,
    FW_MAX_IMAGE_SIZE_BYTES,
    IMAGE_HEADER_STRUCT,
    IMAGE_MAGIC,
    SECURITY_LEVEL,
    SESSION_PROGRAMMING,
    HandshakeSocket,
    TestCli,
    bench_ecu_id,
    handshake_ecu_id,
    make_connection,
    print_bench,
    prompt,
    prompt_int,
)

# the test app writes BOOT_HOLD_MAGIC and resets when it sees this frame, so the bootloader stays in its UDS server
ENTER_BOOTLOADER_ID = 0x1F000FFF
ENTER_BOOTLOADER_DATA = bytes([0x00, 0x01])
ENTER_BOOTLOADER_RETRY_S = 0.5
ENTER_BOOTLOADER_WAIT_S = 30.0

CSV_FIELDS = [
    "timestamp", "run", "image", "bytes", "block_size", "transport_us", "total_us",
    *BENCH_TX_FIELDS.values(), *BENCH_BOOT_FIELDS.values(), "result",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Repeatedly flash one image and log the benchmark timings to a CSV")
    parser.add_argument("--key-file", type=Path, help="Path to base64-encoded auth key file")
    parser.add_argument("--interface", default="can0", help="SocketCAN interface (default: can0)")
    parser.add_argument("--verbose", action="store_true", help="Show udsoncan request/response logs")
    return parser.parse_args()


def enter_bootloader(client: Client, conn: IsoTPSocketConnection, trigger: HandshakeSocket) -> float:
    """Kicks the app into the bootloader and waits until the bootloader's UDS server takes the programming session,
    returns the perf_counter() from just before the 0x10 request that succeeded"""
    deadline = time.monotonic() + ENTER_BOOTLOADER_WAIT_S
    while time.monotonic() < deadline:
        # harmless if the bootloader is already up, its CAN filter drops this ID
        trigger.send(ENTER_BOOTLOADER_ID, ENTER_BOOTLOADER_DATA)
        time.sleep(ENTER_BOOTLOADER_RETRY_S)
        conn.empty_rxqueue()
        start = time.perf_counter()
        try:
            client.change_session(SESSION_PROGRAMMING)
            return start
        except (TimeoutException, NegativeResponseException, InvalidResponseException, UnexpectedResponseException):
            continue
    raise TimeoutError(f"bootloader did not answer within {ENTER_BOOTLOADER_WAIT_S:.0f} s")


def download(client: Client, image: bytes, status: dict) -> tuple[float, int]:
    """0x34 / 0x36 / 0x37 at the server's max block size, returns (transport time in us, data bytes per block)"""
    location = MemoryLocation(address=0, memorysize=len(image), address_format=32, memorysize_format=32)
    start = time.perf_counter()
    status["stage"] = "0x34"
    response = client.request_download(location)
    # ISO 14229 counts the SID and sequence counter bytes in the max block length
    block_size = response.service_data.max_length - 2
    num_blocks = (len(image) + block_size - 1) // block_size
    seq_counter = 1
    for block_num, offset in enumerate(range(0, len(image), block_size), start=1):
        status["stage"] = f"0x36 block {block_num}/{num_blocks}"
        client.transfer_data(seq_counter, image[offset:offset + block_size])
        seq_counter = (seq_counter + 1) & 0xFF
    status["stage"] = "0x37"
    client.request_transfer_exit()
    return (time.perf_counter() - start) * 1e6, block_size


def run_once(tester: TestCli, trigger: HandshakeSocket, image: bytes, ecu_id: int, version: str, status: dict) -> dict:
    """status["stage"] tracks the step in progress so a failure can say where it happened"""
    client = tester.client
    status["stage"] = "enter bootloader"
    # total time runs from the 0x10 that got answered to the ECU's answer to the handshake confirm
    session_start = enter_bootloader(client, tester.conn, trigger)
    status["stage"] = "0x27 unlock"
    client.unlock_security_access(SECURITY_LEVEL)

    transport_us, block_size = download(client, image, status)
    print(f"BENCH transport_us={transport_us:.0f} bytes={len(image)} block_size={block_size}")

    # open before the reset so no frame can be missed
    handshake = HandshakeSocket(tester.interface, handshake_ecu_id(ecu_id))
    bench = HandshakeSocket(tester.interface, bench_ecu_id(ecu_id))
    try:
        status["stage"] = "0x11 reset"
        client.ecu_reset(0x01)
        status["stage"] = "handshake"
        result = tester.confirm_update(handshake, ecu_id, version, "the previous image")
        total_us = (time.perf_counter() - session_start) * 1e6
        print(f"BENCH total_us={total_us:.0f}")
        fields = print_bench(bench)
    finally:
        handshake.close()
        bench.close()

    return {"bytes": len(image), "block_size": block_size, "transport_us": round(transport_us),
            "total_us": round(total_us), **fields, "result": result}


def main():
    args = parse_args()
    auth_key = get_auth_key(args.key_file)

    if not args.verbose:
        logging.getLogger("UdsClient").setLevel(logging.WARNING)
        logging.getLogger("Connection").setLevel(logging.WARNING)

    image_path = Path(prompt("Path to fmt_tool image (e.g. app-img.bin)"))
    if not image_path.is_file():
        print(f"{image_path.resolve()} not found")
        return
    image = image_path.read_bytes()
    if len(image) < IMAGE_HEADER_STRUCT.size or len(image) > FW_MAX_IMAGE_SIZE_BYTES:
        print(f"Image is {len(image)} bytes, must be a fmt_tool image of at most {FW_MAX_IMAGE_SIZE_BYTES} bytes")
        return
    magic, _, ecu_id, _, major, minor, revision = IMAGE_HEADER_STRUCT.unpack_from(image)
    if magic != IMAGE_MAGIC:
        print(f"Bad header magic 0x{magic:08X}, was this made by fmt_tool?")
        return
    version = f"{major}.{minor}.{revision}"

    runs = prompt_int("Number of runs (0 = until Ctrl-C)", 10)
    csv_path = Path(prompt("CSV output", "bench_results.csv"))
    write_header = not csv_path.exists() or csv_path.stat().st_size == 0
    if not write_header:
        with csv_path.open(newline="") as f:
            if next(csv.reader(f), []) != CSV_FIELDS:
                print(f"{csv_path.resolve()} has different columns (older bench version?), pick a new CSV")
                return

    conn = make_connection(args.interface)
    config = configure(auth_key)
    # udsoncan caps every request at 5 s total by default, which cuts P2* short
    config["request_timeout"] = None

    trigger = HandshakeSocket(args.interface, ENTER_BOOTLOADER_ID)
    try:
        with Client(conn, config) as client, csv_path.open("a", newline="") as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=CSV_FIELDS)
            if write_header:
                writer.writeheader()
            tester = TestCli(client, conn, keepalive=False, interface=args.interface)

            run = 1
            while runs == 0 or run <= runs:
                print(f"\n=== Run {run}{f'/{runs}' if runs else ''}, ECU 0x{ecu_id:03X}, version {version} ===")
                row = {"timestamp": datetime.now().isoformat(timespec="seconds"), "run": run, "image": image_path.name}
                status = {"stage": ""}
                try:
                    row.update(run_once(tester, trigger, image, ecu_id, version, status))
                except (TimeoutError, TimeoutException, NegativeResponseException,
                        InvalidResponseException, UnexpectedResponseException) as e:
                    print(f"Run {run} failed during {status['stage']}: {e}")
                    row["result"] = f"error: {type(e).__name__} during {status['stage']}"
                writer.writerow(row)
                csv_file.flush()
                run += 1
    except KeyboardInterrupt:
        print()
    finally:
        trigger.close()
    print(f"Results in {csv_path.resolve()}")


if __name__ == "__main__":
    main()
