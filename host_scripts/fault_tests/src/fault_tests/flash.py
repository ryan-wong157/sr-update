"""Runs one full update and prints how it ended, with hooks to pause or die mid-transfer

  fault-flash img.bin                        normal update, prints the outcome
  fault-flash img.bin --pause-at-block 10    stops before block 10 so the CAN cable can be pulled
  fault-flash img.bin --die-at-block 10      exits abruptly before block 10, like a killed client
  fault-flash img.bin --no-confirm           leaves the test boot unconfirmed (pull power during it)
"""
import argparse
import os
from pathlib import Path

from fault_tests import ecu
from fault_tests.ecu import UDS_ERRORS


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run one update against the ECU and print the outcome")
    parser.add_argument("image", type=Path, help="Image to flash")
    parser.add_argument("--interface", default="can0", help="SocketCAN interface (default: can0)")
    parser.add_argument("--key-file", type=Path, help="Auth key file (default: PRIVATE_AUTH_KEY env var)")
    # the ECU's own ID, not the image's, so the rejection of a wrong ECU or headerless image is still heard
    parser.add_argument("--ecu-id", type=lambda s: int(s, 16), default=0x001, help="Target ECU ID as hex (default: 0x001)")
    parser.add_argument("--pause-at-block", type=int, metavar="N", help="Wait for Enter before 0x36 block N")
    parser.add_argument("--die-at-block", type=int, metavar="N", help="Exit without cleaning up before 0x36 block N")
    parser.add_argument("--no-confirm", action="store_true", help="Don't confirm the test boot, the IWDG reverts it")
    parser.add_argument("--no-enter", action="store_true", help="ECU is already sitting in the bootloader, skip the app reset frame")
    parser.add_argument("--verbose", action="store_true", help="Show udsoncan request/response logs")
    return parser.parse_args()


def main():
    args = parse_args()
    if not args.verbose:
        ecu.quiet_logs()

    image = args.image.read_bytes()
    header = ecu.parse_header(image)
    new_version = header[1] if header else None
    ecu_id = args.ecu_id
    print(f"{args.image.name}: {len(image)} B, version {new_version or '?'}, ECU 0x{ecu_id:03X}")

    stage = {"name": "connect"}

    def before_block(block_num: int, num_blocks: int) -> None:
        stage["name"] = f"0x36 block {block_num}/{num_blocks}"
        if block_num == args.die_at_block:
            print(f"Dying before block {block_num}/{num_blocks}")
            os._exit(1)
        if block_num == args.pause_at_block:
            input(f"Paused before block {block_num}/{num_blocks}, pull the cable then press Enter to carry on ")

    client = ecu.connect(args.interface, args.key_file)
    # opened before the reset so no handshake frame can be missed
    handshake = ecu.open_handshake(args.interface, ecu_id)
    try:
        if not args.no_enter:
            stage["name"] = "enter bootloader"
            ecu.enter_bootloader(client, handshake)
        else:
            stage["name"] = "0x10 programming session"
            ecu.programming_session(client)
        stage["name"] = "0x27 unlock"
        ecu.unlock(client)
        stage["name"] = "0x34"
        block_size = ecu.request_download(client, len(image))
        ecu.transfer_data(client, image, block_size, before_block)
        stage["name"] = "0x37"
        ecu.transfer_exit(client)
        stage["name"] = "0x11 reset"
        ecu.reset(client)
        stage["name"] = "handshake"
        outcome, running = ecu.wait_result(handshake, ecu_id, new_version, confirm=not args.no_confirm)
        print(f"Outcome: {outcome}" + (f", ECU running {running}" if running else ""))
    except (*UDS_ERRORS, TimeoutError, OSError) as e:
        print(f"Outcome: failed during {stage['name']}: {e}")
    finally:
        handshake.close()
        client.close()


if __name__ == "__main__":
    main()
