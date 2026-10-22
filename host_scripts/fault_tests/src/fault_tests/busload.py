"""Floods the bus with random traffic while an update runs in another terminal

Every frame gets a random ID that is never one of the OTA IDs. With --priority high the IDs are
all numerically lower than the bootloader's, so they win arbitration against it.

  fault-busload --load 50     about 50 % of a 1 Mbit/s bus
  fault-busload --load 100    as fast as the interface will take frames
"""
import argparse
import os
import random
import socket
import struct
import time

from ota_client import cli as ota

# every OTA frame is 0x1F000000 | ECU << 12 | 0xFFx, keep well clear of that block
OTA_ID_BASE = 0x1F000000
# rough bits per 8 byte frame with stuffing, standard and extended IDs averaged
BITS_PER_FRAME = 135


def random_id(priority: str) -> int:
    """Random extended ID, flagged for SocketCAN"""
    if priority == "high":
        can_id = random.randrange(0, OTA_ID_BASE)
    else:
        # above ECU 0x00F's OTA IDs so it loses arbitration to the update, skipping the 0xFFx
        # endings so it never lands on another ECU's OTA IDs either
        while (can_id := random.randrange(OTA_ID_BASE + 0x10000, ota.CAN_EFF_MASK + 1)) & 0xFF0 == 0xFF0:
            pass
    return can_id | ota.CAN_EFF_FLAG


def main():
    parser = argparse.ArgumentParser(description="Load the CAN bus with random frames")
    parser.add_argument("--interface", default="can0", help="SocketCAN interface (default: can0)")
    parser.add_argument("--load", type=float, default=50.0, help="Target bus load in %% (default: 50, 100 = flat out)")
    parser.add_argument("--bitrate", type=int, default=1_000_000, help="Bus bitrate in bit/s, used to turn --load into a frame rate (default: 1000000)")
    parser.add_argument("--priority", choices=["high", "low"], default="high", help="IDs below (high) or above (low) the OTA IDs (default: high)")
    parser.add_argument("--duration", type=float, default=0, help="Seconds to run, 0 = until Ctrl-C")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    sock.bind((args.interface,))
    # nothing to read, don't let received frames pile up
    sock.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FILTER, b"")

    flat_out = args.load >= 100
    period = 0 if flat_out else BITS_PER_FRAME / (args.bitrate * args.load / 100)
    print(f"Loading {args.interface}: {'flat out' if flat_out else f'{1 / period:.0f} frames/s'}, "
          f"{args.priority} priority IDs, Ctrl-C to stop")

    start = time.perf_counter()
    next_send = start
    sent = 0
    try:
        while args.duration == 0 or time.perf_counter() - start < args.duration:
            if not flat_out:
                # sleep only when far enough ahead, short sleeps overshoot and the rate falls behind
                ahead = next_send - time.perf_counter()
                if ahead > 0.002:
                    time.sleep(ahead)
                elif ahead > 0:
                    continue
                next_send += period
            frame = struct.pack(ota.CAN_FRAME_FMT, random_id(args.priority), 8, os.urandom(8))
            try:
                sock.send(frame)
                sent += 1
            except OSError:
                # tx queue full, the bus is saturated
                time.sleep(0.0005)
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
    elapsed = time.perf_counter() - start
    print(f"\nSent {sent} frames in {elapsed:.1f} s ({sent / elapsed:.0f} frames/s)")


if __name__ == "__main__":
    main()
