"""Raw UDS request/response access to the ECU over a kernel ISO-TP socket

Requests are sent as raw bytes rather than through udsoncan's client, so tests can send malformed
requests and see exactly what comes back
"""
import time
from typing import Callable

import isotp
from udsoncan.connections import IsoTPSocketConnection

from ota_client import cli as ota
from ota_client.main import RXID, TXID

SID_SESSION_CONTROL = 0x10
SID_ECU_RESET = 0x11
SID_READ_DATA = 0x22
SID_SECURITY_ACCESS = 0x27
SID_REQUEST_DOWNLOAD = 0x34
SID_TRANSFER_DATA = 0x36
SID_TRANSFER_EXIT = 0x37
SID_TESTER_PRESENT = 0x3E
SID_TEST_SERVICE = 0x80
SID_NEGATIVE = 0x7F

SUPPRESS_POS_RSP = 0x80
SEED_REQUEST = ota.SECURITY_LEVEL
SEND_KEY = ota.SECURITY_LEVEL + 1

NRC_SERVICE_NOT_SUPPORTED = 0x11
NRC_SUBFUNCTION_NOT_SUPPORTED = 0x12
NRC_INCORRECT_LENGTH = 0x13
NRC_CONDITIONS_NOT_CORRECT = 0x22
NRC_REQUEST_SEQUENCE_ERROR = 0x24
NRC_REQUEST_OUT_OF_RANGE = 0x31
NRC_SECURITY_ACCESS_DENIED = 0x33
NRC_INVALID_KEY = 0x35
NRC_EXCEEDED_ATTEMPTS = 0x36
NRC_TIME_DELAY_NOT_EXPIRED = 0x37
NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED = 0x70
NRC_TRANSFER_DATA_SUSPENDED = 0x71
NRC_WRONG_BLOCK_SEQUENCE_COUNTER = 0x73
NRC_RESPONSE_PENDING = 0x78
NRC_SERVICE_NOT_SUPPORTED_IN_SESSION = 0x7F

S3_SERVER_S = ota.S3_SERVER_MS / 1000
# cli.py: 3 bad keys in a row locks security access out for 60 s
SECURITY_MAX_ATTEMPTS = 3
SECURITY_LOCKOUT_S = 60.0

P2_STAR_S = 20.0

KeyAlgo = Callable[[bytes], bytes]


def fmt(response: bytes | None) -> str:
    return "no response" if response is None else response.hex(" ")


def nrc_of(response: bytes | None, sid: int) -> int | None:
    """The NRC if this is a negative response to sid"""
    if response is not None and len(response) == 3 and response[0] == SID_NEGATIVE and response[1] == sid:
        return response[2]
    return None


def read_request(*dids: int) -> bytes:
    return bytes([SID_READ_DATA]) + b"".join(did.to_bytes(2, "big") for did in dids)


def download_request(size: int, address: int = 0) -> bytes:
    # same encoding as cli.py: no compression/encryption, 4 byte address and 4 byte size
    return bytes([SID_REQUEST_DOWNLOAD, 0x00, 0x44]) + address.to_bytes(4, "big") + size.to_bytes(4, "big")


class Ecu:
    def __init__(self, interface: str):
        self.interface = interface
        self.conn: IsoTPSocketConnection | None = None
        self.last_elapsed = 0.0 # seconds from sending the last request to its first response
        self.last_pending_count = 0 # response pending NRCs seen before the last final response
        self.connect()

    def connect(self) -> None:
        address = isotp.Address(isotp.AddressingMode.Normal_29bits, rxid=RXID, txid=TXID)
        self.conn = IsoTPSocketConnection(self.interface, address)
        self.conn.open()

    def close(self) -> None:
        if self.conn is not None:
            self.conn.close()
            self.conn = None

    # Requests ====================================================================================
    def request(self, payload: bytes, timeout: float = 2.0) -> bytes | None:
        """Sends a request and returns the final response, None if the ECU stays silent"""
        # udsoncan's receive thread quits silently on a socket error, which would look like a dead ECU
        if self.conn.exit_requested:
            self.close()
            self.connect()
        self.conn.empty_rxqueue()
        self.last_pending_count = 0
        start = time.monotonic()
        self.conn.send(payload)
        response = self.conn.wait_frame(timeout=timeout, exception=False)
        self.last_elapsed = time.monotonic() - start
        while nrc_of(response, payload[0]) == NRC_RESPONSE_PENDING:
            self.last_pending_count += 1
            response = self.conn.wait_frame(timeout=P2_STAR_S, exception=False)
        return response

    def positive(self, payload: bytes, timeout: float = 2.0) -> bytes:
        response = self.request(payload, timeout)
        assert response is not None, f"no response to {fmt(payload[:16])}"
        assert response[0] == payload[0] + 0x40, f"expected a positive response to {fmt(payload[:16])}, got {fmt(response)}"
        return response

    def negative(self, payload: bytes, *nrcs: int, timeout: float = 2.0) -> int:
        """Expects a negative response carrying one of nrcs, returns the one received"""
        response = self.request(payload, timeout)
        nrc = nrc_of(response, payload[0])
        expected = "/".join(f"0x{code:02X}" for code in nrcs)
        assert nrc in nrcs, f"expected NRC {expected} to {fmt(payload[:16])}, got {fmt(response)}"
        return nrc

    def silent(self, payload: bytes, window: float = 0.5) -> None:
        response = self.request(payload, timeout=window)
        assert response is None, f"expected no response to {fmt(payload[:16])}, got {fmt(response)}"

    # Liveness ====================================================================================
    def wait_alive(self, timeout: float) -> bool:
        """Polls tester present until the ECU answers"""
        deadline = time.monotonic() + timeout
        while True:
            try:
                if self.request(bytes([SID_TESTER_PRESENT, 0x00]), timeout=0.3) == bytes([SID_TESTER_PRESENT + 0x40, 0x00]):
                    # unanswered polls can sit in the CAN tx queue and all get answered at once
                    time.sleep(0.2)
                    self.conn.empty_rxqueue()
                    return True
            except OSError:
                # tx queue is full because nothing is acking, the ECU is still down
                time.sleep(0.3)
            if time.monotonic() >= deadline:
                return False

    def hard_reset(self, boot_timeout: float) -> None:
        response = self.request(bytes([SID_ECU_RESET, 0x01]))
        assert response == bytes([SID_ECU_RESET + 0x40, 0x01]), f"ECU reset was not acknowledged: {fmt(response)}"
        # let the reset actually happen so wait_alive is not answered by the old instance
        time.sleep(0.2)
        assert self.wait_alive(boot_timeout), f"ECU did not come back within {boot_timeout} s of a reset"

    # Sessions ====================================================================================
    def change_session(self, session: int) -> bytes:
        response = self.positive(bytes([SID_SESSION_CONTROL, session]))
        assert response[1] == session, f"session control echoed the wrong session: {fmt(response)}"
        return response

    def enter_default(self) -> bytes:
        return self.change_session(ota.SESSION_DEFAULT)

    def enter_programming(self) -> bytes:
        return self.change_session(ota.SESSION_PROGRAMMING)

    def session(self) -> int:
        """Active session as reported by DID 0xF186"""
        response = self.positive(read_request(0xF186))
        assert response[1:3] == b"\xF1\x86" and len(response) == 4, f"bad response to reading DID 0xF186: {fmt(response)}"
        return response[3]

    # Security access =============================================================================
    def request_seed(self, wait_out_delay: bool = True) -> bytes:
        """wait_out_delay rides out a lockout left behind by an earlier test"""
        deadline = time.monotonic() + SECURITY_LOCKOUT_S + 10
        while True:
            response = self.request(bytes([SID_SECURITY_ACCESS, SEED_REQUEST]))
            delayed = nrc_of(response, SID_SECURITY_ACCESS) == NRC_TIME_DELAY_NOT_EXPIRED
            if not (delayed and wait_out_delay and time.monotonic() < deadline):
                break
            # short enough that the session does not time out while waiting
            time.sleep(2)
        assert response is not None and response[:2] == bytes([SID_SECURITY_ACCESS + 0x40, SEED_REQUEST]), \
            f"seed request failed: {fmt(response)}"
        seed = response[2:]
        assert seed, "seed response carries no seed"
        return seed

    def send_key(self, key: bytes) -> bytes | None:
        return self.request(bytes([SID_SECURITY_ACCESS, SEND_KEY]) + key)

    def unlock(self, key_algo: KeyAlgo) -> None:
        seed = self.request_seed()
        if not any(seed):
            return # all zero seed = already unlocked
        response = self.send_key(key_algo(seed))
        assert response == bytes([SID_SECURITY_ACCESS + 0x40, SEND_KEY]), f"valid key was not accepted: {fmt(response)}"

    def is_unlocked(self) -> bool:
        """ISO 14229: a seed request on an unlocked level answers with an all zero seed

        Leaves a seed pending on the server if it was locked.
        """
        return not any(self.request_seed())

    # Download ====================================================================================
    def request_download(self, size: int) -> int:
        """Returns the server's max block length (SID and sequence counter included)"""
        response = self.positive(download_request(size))
        length_bytes = response[1] >> 4
        assert length_bytes >= 1 and len(response) == 2 + length_bytes, f"malformed request download response: {fmt(response)}"
        return int.from_bytes(response[2:], "big")

    def transfer_data(self, seq_counter: int, data: bytes) -> bytes | None:
        return self.request(bytes([SID_TRANSFER_DATA, seq_counter & 0xFF]) + data)

    def transfer_image(self, image: bytes, block_size: int) -> None:
        seq_counter = 1
        for offset in range(0, len(image), block_size):
            response = self.transfer_data(seq_counter, image[offset:offset + block_size])
            assert response == bytes([SID_TRANSFER_DATA + 0x40, seq_counter]), \
                f"block with sequence counter {seq_counter} at offset {offset} was rejected: {fmt(response)}"
            seq_counter = (seq_counter + 1) & 0xFF
