import can
import isotp

def main():
    # ---------------- Configuration ----------------

    CAN_INTERFACE = "can0"      # e.g. can0, vcan0, PCAN_USBBUS1, etc.
    TX_ID = 0x200               # ECU receive ID
    RX_ID = 0x100               # ECU transmit ID

    # ------------------------------------------------

    # Open CAN interface
    bus = can.interface.Bus(
        interface="socketcan",
        channel=CAN_INTERFACE,
    )

    # Create ISO-TP stack
    addr = isotp.Address(
        addressing_mode=isotp.AddressingMode.Normal_11bits,
        txid=TX_ID,
        rxid=RX_ID,
    )

    stack = isotp.CanStack(bus=bus, address=addr)

    # Device expects exactly 20 bytes on RX_ID (see main.c: bytes_received != 20 check)
    payload = bytes(range(20))

    print(f"Sending {len(payload)} bytes...")
    stack.send(payload)

    # Wait until transmission is complete
    while stack.transmitting():
        stack.process()

    print("Waiting for response...")

    while True:
        stack.process()

        if stack.available():
            response = stack.recv()

            print("Received:", response)

            # Device always replies with this fixed 15-byte payload (see main.c)
            if response == b"brochachowacho":
                print("Received expected response!")
            else:
                print("Unexpected response.")

            break


if __name__ == "__main__":
    main()
