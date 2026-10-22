"""Connection plus one helper per step of an update, the baseline every fault test builds on

Errors are left to propagate (udsoncan's NegativeResponseException, TimeoutException, ...) so a
test can tell exactly which step failed and how.
"""
import logging
import time
from udsoncan import MemoryLocation
from udsoncan.client import Client
from udsoncan.exceptions import (
    InvalidResponseException,
    NegativeResponseException,
    TimeoutException,
    UnexpectedResponseException,
)

# NOTE: importing ota_client.main also runs its udsoncan.setup_logging() call
from ota_client.main import configure, get_auth_key
from ota_client import cli as ota

# the test app writes BOOT_HOLD_MAGIC and resets on this frame, so the bootloader stays in its UDS server
ENTER_BOOTLOADER_ID = 0x1F000FFF
ENTER_BOOTLOADER_DATA = bytes([0x00, 0x01])
ENTER_BOOTLOADER_RETRY_S = 0.5
ENTER_BOOTLOADER_WAIT_S = 30.0
# ota_client's 45 s is too short once the fault pauses (1.8 s each) are in the swap
HANDSHAKE_WAIT_S = 180.0

UDS_ERRORS = (NegativeResponseException, TimeoutException, InvalidResponseException, UnexpectedResponseException)

# Handshake outcomes returned by wait_result()
CONFIRMED = "confirmed"
REVERTED = "reverted"
REJECTED_INVALID = "rejected_invalid"
REJECTED_OLDER = "rejected_older"
NO_HANDSHAKE = "no_handshake"
CONFIRM_FAILED = "confirm_failed"
NO_CONFIRM_ANSWER = "no_confirm_answer"
TEST_BOOTED = "test_booted" # wait_result(confirm=False): the new image is up and waiting, left unconfirmed


def quiet_logs() -> None:
    """udsoncan logs every request and frame, too noisy for a test"""
    logging.getLogger("UdsClient").setLevel(logging.WARNING)
    logging.getLogger("Connection").setLevel(logging.WARNING)


# Connection ======================================================================================
def connect(interface: str = "can0", key_file=None) -> Client:
    """Opened UDS client, close it with client.close()"""
    config = configure(get_auth_key(key_file))
    # udsoncan caps every request at 5 s total by default, which cuts P2* short
    config["request_timeout"] = None
    client = Client(ota.make_connection(interface), config)
    client.open()
    return client


def open_handshake(interface: str, ecu_id: int) -> ota.HandshakeSocket:
    """Raw socket for the test boot handshake, open it before the 0x11 reset so no frame is missed"""
    return ota.HandshakeSocket(interface, ecu_id)


# Image ===========================================================================================
def parse_header(image: bytes) -> tuple[int, str] | None:
    """(ecu_id, version) from an fmt_tool header, None if the image has no valid header"""
    if len(image) < ota.IMAGE_HEADER_STRUCT.size:
        return None
    magic, _, ecu_id, _, major, minor, revision = ota.IMAGE_HEADER_STRUCT.unpack_from(image)
    if magic != ota.IMAGE_MAGIC:
        return None
    return ecu_id, f"{major}.{minor}.{revision}"


# Commands ========================================================================================
def enter_bootloader(client: Client, handshake: ota.HandshakeSocket) -> None:
    """Kicks the app into the bootloader and waits until the bootloader takes the programming session"""
    deadline = time.monotonic() + ENTER_BOOTLOADER_WAIT_S
    while time.monotonic() < deadline:
        # harmless if the bootloader is already up, its CAN filter drops this ID
        handshake.send(ENTER_BOOTLOADER_ID, ENTER_BOOTLOADER_DATA)
        time.sleep(ENTER_BOOTLOADER_RETRY_S)
        client.conn.empty_rxqueue()
        try:
            programming_session(client)
            return
        except UDS_ERRORS:
            continue
    raise TimeoutError(f"bootloader did not answer within {ENTER_BOOTLOADER_WAIT_S:.0f} s")


def programming_session(client: Client) -> None:
    client.change_session(ota.SESSION_PROGRAMMING)


def default_session(client: Client) -> None:
    client.change_session(ota.SESSION_DEFAULT)


def request_seed(client: Client) -> bytes:
    return client.request_seed(ota.SECURITY_LEVEL).service_data.seed


def send_key(client: Client, key: bytes) -> None:
    client.send_key(ota.SECURITY_LEVEL + 1, key)


def unlock(client: Client) -> None:
    """Seed request then the key signed with the auth key, the normal 0x27 unlock"""
    client.unlock_security_access(ota.SECURITY_LEVEL)


def request_download(client: Client, size: int) -> int:
    """0x34, returns the data bytes per 0x36 block"""
    location = MemoryLocation(address=0, memorysize=size, address_format=32, memorysize_format=32)
    response = client.request_download(location)
    # ISO 14229 counts the SID and sequence counter bytes in the max block length
    return response.service_data.max_length - 2


def transfer_data(client: Client, image: bytes, block_size: int, before_block=None) -> None:
    """0x36 for the whole image. before_block(block_num, num_blocks) runs before each block (1 based),
    which is where a test pauses or dies mid-transfer"""
    num_blocks = (len(image) + block_size - 1) // block_size
    seq_counter = 1
    for block_num, offset in enumerate(range(0, len(image), block_size), start=1):
        if before_block is not None:
            before_block(block_num, num_blocks)
        client.transfer_data(seq_counter, image[offset:offset + block_size])
        seq_counter = (seq_counter + 1) & 0xFF


def transfer_exit(client: Client) -> None:
    client.request_transfer_exit()


def reset(client: Client) -> None:
    client.ecu_reset(0x01)


def wait_result(handshake: ota.HandshakeSocket, ecu_id: int, new_version: str | None, confirm: bool = True) -> tuple[str, str | None]:
    """Follows the test boot after the reset, returns (outcome, version the ECU is running)

    The old app's revert handshake looks the same as the new app's, the only tell is the version, so
    the new image needs a different version from the one installed for a revert to show up.
    """
    deadline = time.monotonic() + HANDSHAKE_WAIT_S
    data = None
    while time.monotonic() < deadline:
        data = handshake.recv(deadline - time.monotonic())
        if data is None or len(data) < 4:
            data = None
            continue
        running = ".".join(str(part) for part in data[1:4])
        if data[0] == ota.HANDSHAKE_STATUS_REJECTED_INVALID:
            return REJECTED_INVALID, running
        if data[0] == ota.HANDSHAKE_STATUS_REJECTED_OLDER:
            return REJECTED_OLDER, running
        if data[0] == ota.HANDSHAKE_STATUS_WAITING:
            break
        data = None
    if data is None:
        return NO_HANDSHAKE, None

    reverted = running != new_version
    if not confirm:
        return (REVERTED if reverted else TEST_BOOTED), running

    # resend the confirm until the ECU answers, a frame can get lost on the bus
    deadline = time.monotonic() + ota.HANDSHAKE_RESULT_WAIT_S
    confirm_frame = bytes([(ecu_id >> 8) & 0xFF, ecu_id & 0xFF])
    while time.monotonic() < deadline:
        handshake.send(ota.HANDSHAKE_TESTER_ID, confirm_frame)
        data = handshake.recv(0.2)
        if data is None or len(data) < 4 or data[0] == ota.HANDSHAKE_STATUS_WAITING:
            continue
        if reverted:
            return REVERTED, running
        return (CONFIRMED if data[0] == ota.HANDSHAKE_STATUS_CONFIRMED else CONFIRM_FAILED), running
    return NO_CONFIRM_ANSWER, running
