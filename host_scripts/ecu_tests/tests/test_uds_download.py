"""0x34 request download, 0x36 transfer data, 0x37 request transfer exit

Tests marked flash get a download accepted, which erases and writes firmware slot B. They only
run with --flash. Everything else only sends requests a correct server refuses before touching
flash.
"""
import time

import pytest

from ota_client import cli as ota

from ecu import (
    NRC_CONDITIONS_NOT_CORRECT,
    NRC_INCORRECT_LENGTH,
    NRC_REQUEST_OUT_OF_RANGE,
    NRC_REQUEST_SEQUENCE_ERROR,
    NRC_SECURITY_ACCESS_DENIED,
    NRC_SERVICE_NOT_SUPPORTED_IN_SESSION,
    NRC_TRANSFER_DATA_SUSPENDED,
    NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED,
    NRC_WRONG_BLOCK_SEQUENCE_COUNTER,
    S3_SERVER_S,
    SID_REQUEST_DOWNLOAD,
    SID_TRANSFER_DATA,
    SID_TRANSFER_EXIT,
    Ecu,
    KeyAlgo,
    download_request,
    fmt,
    nrc_of,
)

MAX_IMAGE_SIZE = ota.FW_MAX_IMAGE_SIZE_BYTES
SMALL_IMAGE_SIZE = 1000

TRANSFER_EXIT = bytes([SID_TRANSFER_EXIT])
TRANSFER_EXIT_OK = bytes([SID_TRANSFER_EXIT + 0x40])

# which of "wrong session" and "locked" a server reports first is its own choice
NOT_ALLOWED_IN_DEFAULT = (NRC_SERVICE_NOT_SUPPORTED_IN_SESSION, NRC_SECURITY_ACCESS_DENIED)
# a size the slot cannot hold
BAD_SIZE = (NRC_REQUEST_OUT_OF_RANGE, NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED)


def image(size: int) -> bytes:
    # same fill as cli.py's flash option
    return bytes([ota.FLASH_FILL_BYTE]) * size


def transfer_request(seq_counter: int, data: bytes) -> bytes:
    return bytes([SID_TRANSFER_DATA, seq_counter]) + data


def block_ok(seq_counter: int) -> bytes:
    return bytes([SID_TRANSFER_DATA + 0x40, seq_counter])


# =================================================================================================
# Access control
# =================================================================================================
def test_download_rejected_in_default_session(ecu: Ecu):
    ecu.negative(download_request(SMALL_IMAGE_SIZE), *NOT_ALLOWED_IN_DEFAULT)


def test_transfer_data_rejected_in_default_session(ecu: Ecu):
    ecu.negative(transfer_request(1, image(16)), *NOT_ALLOWED_IN_DEFAULT, NRC_REQUEST_SEQUENCE_ERROR)


def test_transfer_exit_rejected_in_default_session(ecu: Ecu):
    ecu.negative(TRANSFER_EXIT, *NOT_ALLOWED_IN_DEFAULT, NRC_REQUEST_SEQUENCE_ERROR)


def test_download_rejected_while_locked(programming: Ecu):
    programming.negative(download_request(SMALL_IMAGE_SIZE), NRC_SECURITY_ACCESS_DENIED)


def test_transfer_data_rejected_while_locked(programming: Ecu):
    programming.negative(transfer_request(1, image(16)), NRC_SECURITY_ACCESS_DENIED, NRC_REQUEST_SEQUENCE_ERROR)


def test_transfer_exit_rejected_while_locked(programming: Ecu):
    programming.negative(TRANSFER_EXIT, NRC_SECURITY_ACCESS_DENIED, NRC_REQUEST_SEQUENCE_ERROR)


def test_download_rejected_with_seed_pending(programming: Ecu):
    """Having asked for a seed is not being unlocked"""
    programming.request_seed()
    programming.negative(download_request(SMALL_IMAGE_SIZE), NRC_SECURITY_ACCESS_DENIED)


def test_download_rejected_after_relock(unlocked: Ecu):
    unlocked.enter_programming()
    unlocked.negative(download_request(SMALL_IMAGE_SIZE), NRC_SECURITY_ACCESS_DENIED)


def test_download_rejected_in_default_session_after_unlock(unlocked: Ecu):
    unlocked.enter_default()
    unlocked.negative(download_request(SMALL_IMAGE_SIZE), *NOT_ALLOWED_IN_DEFAULT)


# =================================================================================================
# Sequence errors and malformed requests, nothing here starts a download
# =================================================================================================
def test_transfer_data_without_download(unlocked: Ecu):
    unlocked.negative(transfer_request(1, image(16)), NRC_REQUEST_SEQUENCE_ERROR)


def test_transfer_exit_without_download(unlocked: Ecu):
    unlocked.negative(TRANSFER_EXIT, NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.parametrize("payload", [
    pytest.param(bytes([SID_REQUEST_DOWNLOAD]), id="sid_only"),
    pytest.param(bytes([SID_REQUEST_DOWNLOAD, 0x00]), id="no_format_identifier"),
    pytest.param(bytes([SID_REQUEST_DOWNLOAD, 0x00, 0x44]), id="no_address_or_size"),
    pytest.param(download_request(SMALL_IMAGE_SIZE)[:7], id="no_size"),
    pytest.param(download_request(SMALL_IMAGE_SIZE)[:-1], id="size_one_byte_short"),
    pytest.param(download_request(SMALL_IMAGE_SIZE) + b"\x00", id="one_extra_byte"),
])
def test_download_bad_length(unlocked: Ecu, payload: bytes):
    unlocked.negative(payload, NRC_INCORRECT_LENGTH)
    # and it did not start a download
    unlocked.negative(transfer_request(1, image(16)), NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.parametrize("size", [0, MAX_IMAGE_SIZE + 1, ota.FW_SLOT_SIZE_BYTES, ota.FW_SLOT_SIZE_BYTES + 1, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF])
def test_download_bad_size(unlocked: Ecu, size: int):
    unlocked.negative(download_request(size), *BAD_SIZE)
    unlocked.negative(transfer_request(1, image(16)), NRC_REQUEST_SEQUENCE_ERROR)


def test_transfer_data_bad_length_without_download(unlocked: Ecu):
    unlocked.negative(bytes([SID_TRANSFER_DATA]), NRC_INCORRECT_LENGTH, NRC_REQUEST_SEQUENCE_ERROR)


# =================================================================================================
# Downloads
# =================================================================================================
@pytest.mark.flash
def test_download_response_format(unlocked: Ecu):
    response = unlocked.positive(download_request(SMALL_IMAGE_SIZE))
    length_bytes = response[1] >> 4
    assert response[1] & 0xF == 0, f"low nibble of the length format identifier is reserved: {fmt(response)}"
    assert 1 <= length_bytes <= 4 and len(response) == 2 + length_bytes, f"malformed response: {fmt(response)}"
    max_length = int.from_bytes(response[2:], "big")
    # SID + sequence counter + at least one data byte, and it has to fit an ISO-TP message
    assert 3 <= max_length <= 4095, f"max block length of {max_length} is not usable"


@pytest.mark.flash
def test_download_ignores_address(unlocked: Ecu):
    """cli.py: the address is ignored, the server always writes to slot B"""
    unlocked.positive(download_request(SMALL_IMAGE_SIZE, address=0xDEADBEEF))


@pytest.mark.flash
@pytest.mark.parametrize("size", [1, 2, 7, 8, 255, 256, 257, SMALL_IMAGE_SIZE, 2047, 2048, 2049, 4096])
def test_small_download(unlocked: Ecu, size: int):
    max_length = unlocked.request_download(size)
    unlocked.transfer_image(image(size), max_length - 2)
    assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK


@pytest.mark.flash
@pytest.mark.parametrize("block_size", [1, 5, 6, 7, 100])
def test_download_with_small_blocks(unlocked: Ecu, block_size: int):
    """Blocks smaller than the server's maximum, including ones that fit a single frame"""
    size = 40 * block_size
    max_length = unlocked.request_download(size)
    assert block_size <= max_length - 2
    unlocked.transfer_image(image(size), block_size)
    assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK


@pytest.mark.flash
def test_download_with_mixed_block_sizes(unlocked: Ecu):
    max_length = unlocked.request_download(SMALL_IMAGE_SIZE)
    sizes = [1, max_length - 2, 3, 64, 7]
    sent = 0
    seq_counter = 1
    while sent < SMALL_IMAGE_SIZE:
        length = min(sizes[seq_counter % len(sizes)], SMALL_IMAGE_SIZE - sent)
        assert unlocked.transfer_data(seq_counter, image(length)) == block_ok(seq_counter)
        sent += length
        seq_counter += 1
    assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK


@pytest.mark.flash
@pytest.mark.slow
def test_full_size_download(unlocked: Ecu):
    """Largest image the slot takes, with enough blocks for the sequence counter to wrap 255 -> 0"""
    max_length = unlocked.request_download(MAX_IMAGE_SIZE)
    block_size = min(max_length - 2, 128)
    assert MAX_IMAGE_SIZE // block_size > 256, "not enough blocks to wrap the sequence counter"
    unlocked.transfer_image(image(MAX_IMAGE_SIZE), block_size)
    assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK


@pytest.mark.flash
@pytest.mark.slow
def test_full_size_download_with_max_blocks(unlocked: Ecu):
    """The same as cli.py's flash option does by default"""
    max_length = unlocked.request_download(MAX_IMAGE_SIZE)
    unlocked.transfer_image(image(MAX_IMAGE_SIZE), max_length - 2)
    assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK


@pytest.mark.flash
def test_consecutive_downloads(unlocked: Ecu):
    """A finished download leaves the server ready for the next one"""
    for _ in range(3):
        max_length = unlocked.request_download(SMALL_IMAGE_SIZE)
        unlocked.transfer_image(image(SMALL_IMAGE_SIZE), max_length - 2)
        assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK


@pytest.mark.flash
def test_download_with_non_uniform_data(unlocked: Ecu):
    data = bytes((i * 7 + (i >> 8)) & 0xFF for i in range(SMALL_IMAGE_SIZE))
    max_length = unlocked.request_download(len(data))
    unlocked.transfer_image(data, max_length - 2)
    assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK


# =================================================================================================
# Errors during a download
# =================================================================================================
@pytest.mark.flash
@pytest.mark.parametrize("seq_counter", [0, 2, 3, 0xFF])
def test_first_block_with_wrong_sequence_counter(unlocked: Ecu, seq_counter: int):
    unlocked.request_download(SMALL_IMAGE_SIZE)
    unlocked.negative(transfer_request(seq_counter, image(100)), NRC_WRONG_BLOCK_SEQUENCE_COUNTER)


@pytest.mark.flash
def test_skipped_sequence_counter(unlocked: Ecu):
    unlocked.request_download(SMALL_IMAGE_SIZE)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    assert unlocked.transfer_data(2, image(100)) == block_ok(2)
    unlocked.negative(transfer_request(4, image(100)), NRC_WRONG_BLOCK_SEQUENCE_COUNTER)


@pytest.mark.flash
def test_repeated_sequence_counter(unlocked: Ecu):
    """ISO 14229 lets a server acknowledge a repeated block without writing it again, or it may
    refuse it. Either way the repeat must not count as new data."""
    size = 300
    unlocked.request_download(size)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    response = unlocked.transfer_data(1, image(100))
    assert response == block_ok(1) or nrc_of(response, SID_TRANSFER_DATA) == NRC_WRONG_BLOCK_SEQUENCE_COUNTER, \
        f"unexpected response to a repeated block: {fmt(response)}"
    if response == block_ok(1):
        # 100 bytes written so far, so the transfer is not complete yet
        assert unlocked.transfer_data(2, image(100)) == block_ok(2)
        unlocked.negative(TRANSFER_EXIT, NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.flash
def test_transfer_data_without_data(unlocked: Ecu):
    unlocked.request_download(SMALL_IMAGE_SIZE)
    unlocked.negative(bytes([SID_TRANSFER_DATA, 0x01]), NRC_INCORRECT_LENGTH)
    unlocked.negative(bytes([SID_TRANSFER_DATA]), NRC_INCORRECT_LENGTH)


@pytest.mark.flash
def test_block_longer_than_max_block_length(unlocked: Ecu):
    max_length = unlocked.request_download(MAX_IMAGE_SIZE)
    try:
        # one data byte too many
        response = unlocked.transfer_data(1, image(max_length - 1))
    except OSError:
        # refused by the transport layer already
        return
    assert response is None or nrc_of(response, SID_TRANSFER_DATA) in (NRC_INCORRECT_LENGTH, NRC_REQUEST_OUT_OF_RANGE, NRC_TRANSFER_DATA_SUSPENDED), \
        f"oversized block was not refused: {fmt(response)}"


@pytest.mark.flash
def test_block_longer_than_the_image(unlocked: Ecu):
    unlocked.request_download(100)
    unlocked.negative(transfer_request(1, image(101)), NRC_INCORRECT_LENGTH, NRC_REQUEST_OUT_OF_RANGE, NRC_TRANSFER_DATA_SUSPENDED)


@pytest.mark.flash
def test_more_data_than_announced(unlocked: Ecu):
    unlocked.request_download(200)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    assert unlocked.transfer_data(2, image(100)) == block_ok(2)
    unlocked.negative(
        transfer_request(3, image(1)),
        NRC_REQUEST_SEQUENCE_ERROR, NRC_INCORRECT_LENGTH, NRC_REQUEST_OUT_OF_RANGE, NRC_TRANSFER_DATA_SUSPENDED,
    )


@pytest.mark.flash
def test_last_block_overshoots_the_image(unlocked: Ecu):
    unlocked.request_download(150)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    unlocked.negative(transfer_request(2, image(100)), NRC_INCORRECT_LENGTH, NRC_REQUEST_OUT_OF_RANGE, NRC_TRANSFER_DATA_SUSPENDED)


@pytest.mark.flash
def test_transfer_exit_before_any_data(unlocked: Ecu):
    unlocked.request_download(SMALL_IMAGE_SIZE)
    unlocked.negative(TRANSFER_EXIT, NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.flash
def test_transfer_exit_before_all_data(unlocked: Ecu):
    unlocked.request_download(SMALL_IMAGE_SIZE)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    unlocked.negative(TRANSFER_EXIT, NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.flash
def test_transfer_data_after_exit(unlocked: Ecu):
    max_length = unlocked.request_download(SMALL_IMAGE_SIZE)
    unlocked.transfer_image(image(SMALL_IMAGE_SIZE), max_length - 2)
    assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK
    unlocked.negative(transfer_request(1, image(16)), NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.flash
def test_second_transfer_exit(unlocked: Ecu):
    max_length = unlocked.request_download(SMALL_IMAGE_SIZE)
    unlocked.transfer_image(image(SMALL_IMAGE_SIZE), max_length - 2)
    assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK
    unlocked.negative(TRANSFER_EXIT, NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.flash
def test_download_during_download(unlocked: Ecu):
    """A second request download either restarts the transfer or is refused, never a mix"""
    unlocked.request_download(SMALL_IMAGE_SIZE)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    response = unlocked.request(download_request(200))
    if response is not None and response[0] == SID_REQUEST_DOWNLOAD + 0x40:
        # restarted: sequence counter and byte count start over for the new size
        assert unlocked.transfer_data(1, image(200)) == block_ok(1)
        assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK
    else:
        assert nrc_of(response, SID_REQUEST_DOWNLOAD) in (NRC_CONDITIONS_NOT_CORRECT, NRC_REQUEST_SEQUENCE_ERROR, NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED), \
            f"unexpected response: {fmt(response)}"
        # refused: the original transfer carries on
        assert unlocked.transfer_data(2, image(100)) == block_ok(2)


@pytest.mark.flash
def test_rejected_download_after_finished_download(unlocked: Ecu):
    """A refused request download after a finished one leaves no transfer open"""
    max_length = unlocked.request_download(SMALL_IMAGE_SIZE)
    unlocked.transfer_image(image(SMALL_IMAGE_SIZE), max_length - 2)
    assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK
    unlocked.negative(download_request(MAX_IMAGE_SIZE + 1), *BAD_SIZE)
    unlocked.negative(transfer_request(1, image(16)), NRC_REQUEST_SEQUENCE_ERROR)


# =================================================================================================
# A download does not outlive the session or the unlock
# =================================================================================================
@pytest.mark.flash
def test_session_change_aborts_download(unlocked: Ecu, key_algo: KeyAlgo):
    unlocked.request_download(SMALL_IMAGE_SIZE)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    unlocked.enter_default()
    unlocked.enter_programming()
    unlocked.unlock(key_algo)
    unlocked.negative(transfer_request(2, image(100)), NRC_REQUEST_SEQUENCE_ERROR)
    unlocked.negative(TRANSFER_EXIT, NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.flash
def test_transfer_rejected_after_relock(unlocked: Ecu):
    unlocked.request_download(SMALL_IMAGE_SIZE)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    unlocked.enter_programming()
    unlocked.negative(transfer_request(2, image(100)), NRC_SECURITY_ACCESS_DENIED, NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.flash
@pytest.mark.slow
def test_session_timeout_aborts_download(unlocked: Ecu, key_algo: KeyAlgo):
    unlocked.request_download(SMALL_IMAGE_SIZE)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    time.sleep(S3_SERVER_S + 1.0)
    unlocked.negative(transfer_request(2, image(100)), *NOT_ALLOWED_IN_DEFAULT, NRC_REQUEST_SEQUENCE_ERROR)
    unlocked.enter_programming()
    unlocked.unlock(key_algo)
    unlocked.negative(transfer_request(2, image(100)), NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.flash
@pytest.mark.slow
def test_reset_aborts_download(unlocked: Ecu, key_algo: KeyAlgo, boot_timeout: float):
    unlocked.request_download(SMALL_IMAGE_SIZE)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    unlocked.hard_reset(boot_timeout)
    unlocked.enter_programming()
    unlocked.unlock(key_algo)
    unlocked.negative(transfer_request(2, image(100)), NRC_REQUEST_SEQUENCE_ERROR)


@pytest.mark.flash
@pytest.mark.slow
def test_download_works_after_aborted_download(unlocked: Ecu, key_algo: KeyAlgo):
    """A half written slot from an abandoned transfer does not block the next attempt"""
    unlocked.request_download(MAX_IMAGE_SIZE)
    assert unlocked.transfer_data(1, image(100)) == block_ok(1)
    unlocked.enter_default()
    unlocked.enter_programming()
    unlocked.unlock(key_algo)
    max_length = unlocked.request_download(SMALL_IMAGE_SIZE)
    unlocked.transfer_image(image(SMALL_IMAGE_SIZE), max_length - 2)
    assert unlocked.request(TRANSFER_EXIT) == TRANSFER_EXIT_OK
