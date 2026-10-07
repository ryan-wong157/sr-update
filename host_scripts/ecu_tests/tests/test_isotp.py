"""ISO 15765-2 tests against the ECU's transport layer, driven frame by frame over raw CAN

Requests that only need to prove a message was reassembled use an over-long tester present:
any complete message gets a response from the UDS layer, an incomplete one gets nothing.
"""
import time

import pytest

from ota_client import cli as ota
from ota_client.main import RXID, TXID

from ecu import read_request
from rawcan import (
    CAN_EFF_FLAG,
    FS_CTS,
    FS_OVERFLOW,
    FS_WAIT,
    N_BS_S,
    N_CR_S,
    PCI_CF,
    PCI_FC,
    PCI_FF,
    Frame,
    RawCan,
    consecutive_frames,
    first_frame,
    flow_control,
    pad,
    single_frame,
    stmin_is_valid,
)

PING = b"\x3E\x00"
PONG = b"\x7E\x00"
READ_ALL_DIDS = read_request(*ota.DID_NAMES)

# margin on top of the ECU's 1000 ms network layer timeouts
TIMEOUT_MARGIN_S = 0.5


def filler_request(length: int) -> bytes:
    """A tester present of the given total length, rejected by the UDS layer once fully received"""
    return (PING + bytes(i & 0xFF for i in range(length)))[:length]


def assert_reassembled(payload: bytes) -> None:
    assert payload[:2] == b"\x7F\x3E", f"expected a negative response to the over-long tester present, got {payload.hex(' ')}"


def assert_alive(raw: RawCan) -> None:
    assert raw.ping(), "ECU stopped answering single frame requests"


def start_long_response(raw: RawCan, long_read: tuple[bytes, bytes]) -> Frame:
    """Sends the long read and returns the first frame of the response, no flow control sent yet"""
    request, expected = long_read
    raw.send_request(request)
    frame = raw.recv(2.0)
    assert frame is not None, "no response to the long read"
    assert frame.pci == PCI_FF, f"expected a first frame, got {frame}"
    assert len(frame.data) == 8, f"first frame must fill the CAN frame: {frame}"
    total = ((frame.data[0] & 0xF) << 8) | frame.data[1]
    assert total == len(expected), f"first frame announces {total} bytes, the response is {len(expected)} bytes"
    return frame


def collect_consecutive(raw: RawCan, count: int, first_sn: int = 1, timeout: float = N_CR_S) -> list[Frame]:
    frames = []
    for sn in range(first_sn, first_sn + count):
        frame = raw.recv(timeout)
        assert frame is not None, f"consecutive frame {sn} never arrived"
        assert frame.pci == PCI_CF, f"expected a consecutive frame, got {frame}"
        assert frame.data[0] & 0xF == sn & 0xF, f"expected sequence number {sn & 0xF}, got {frame}"
        frames.append(frame)
    return frames


def consecutive_count(expected: bytes) -> int:
    # the first frame carries 6 bytes, each consecutive frame up to 7
    return -(-(len(expected) - 6) // 7)


def reassemble(first: Frame, consecutive: list[Frame], total: int) -> bytes:
    return (first.data[2:] + b"".join(frame.data[1:] for frame in consecutive))[:total]


# =================================================================================================
# Single frames
# =================================================================================================
def test_single_frame_request_and_response(raw: RawCan):
    raw.send(single_frame(PING))
    frame = raw.recv(1.0)
    assert frame is not None, "no response to a single frame tester present"
    assert frame.can_id == RXID
    assert frame.data[0] == 0x02, f"expected a single frame with 2 data bytes, got {frame}"
    assert frame.data[1:3] == PONG
    raw.expect_silence(0.2)


@pytest.mark.parametrize("pad_byte", [0x00, 0x55, 0xAA, 0xCC, 0xFF])
def test_padded_single_frame_is_accepted(raw: RawCan, pad_byte: int):
    raw.send(pad(single_frame(PING), pad_byte))
    assert raw.recv_response(1.0).payload == PONG


def test_seven_byte_single_frame_is_accepted(raw: RawCan):
    # largest single frame: 3 DIDs
    dids = list(ota.DID_NAMES)[:3]
    message = raw.request(read_request(*dids))
    assert message.payload[0] == 0x62, f"expected a positive read response, got {message.payload.hex(' ')}"


def test_back_to_back_single_frames(raw: RawCan):
    for _ in range(50):
        raw.send(single_frame(PING))
        assert raw.recv_response(1.0).payload == PONG


@pytest.mark.parametrize("frame", [
    pytest.param(b"", id="empty_can_frame"),
    pytest.param(b"\x00\x3E\x00", id="sf_dl_0"),
    pytest.param(b"\x00", id="sf_dl_0_pci_only"),
    pytest.param(b"\x08\x3E\x00\x00\x00\x00\x00\x00", id="sf_dl_8"),
    pytest.param(b"\x0F\x3E\x00\x00\x00\x00\x00\x00", id="sf_dl_15"),
    pytest.param(b"\x02\x3E", id="sf_dl_2_with_1_byte"),
    pytest.param(b"\x07\x3E\x00", id="sf_dl_7_with_2_bytes"),
    pytest.param(b"\x02", id="sf_dl_2_with_0_bytes"),
    pytest.param(b"\x42\x3E\x00", id="reserved_pci_4"),
    pytest.param(b"\x82\x3E\x00", id="reserved_pci_8"),
    pytest.param(b"\xF2\x3E\x00", id="reserved_pci_f"),
])
def test_invalid_frame_is_ignored(raw: RawCan, frame: bytes):
    raw.send(frame)
    raw.expect_silence()
    assert_alive(raw)


@pytest.mark.parametrize("can_id", [
    pytest.param((TXID ^ 0x1) | CAN_EFF_FLAG, id="neighbouring_id"),
    pytest.param(RXID | CAN_EFF_FLAG, id="ecus_own_tx_id"),
    pytest.param(TXID & 0x7FF, id="11_bit_id"),
])
def test_request_on_other_can_id_is_ignored(raw: RawCan, can_id: int):
    raw.send(single_frame(PING), can_id=can_id)
    raw.expect_silence()
    assert_alive(raw)


def test_unexpected_consecutive_frame_is_ignored(raw: RawCan):
    raw.send(b"\x21" + PING + bytes(5))
    raw.expect_silence()
    assert_alive(raw)


@pytest.mark.parametrize("status", [FS_CTS, FS_WAIT, FS_OVERFLOW])
def test_unexpected_flow_control_is_ignored(raw: RawCan, status: int):
    raw.send(flow_control(status))
    raw.expect_silence()
    assert_alive(raw)


# =================================================================================================
# Segmented reception (tester -> ECU)
# =================================================================================================
def test_first_frame_is_answered_with_flow_control(raw: RawCan):
    fc = raw.send_request(filler_request(20))
    assert fc.frame.can_id == RXID
    assert fc.status == FS_CTS
    assert stmin_is_valid(fc.stmin_raw), f"STmin 0x{fc.stmin_raw:02X} is a reserved value"
    assert fc.delay < N_BS_S, f"flow control took {fc.delay * 1000:.0f} ms"
    assert_reassembled(raw.recv_response().payload)


# 8 = smallest segmented message, 13/20/27 end exactly on a frame boundary,
# 118 and up make the sequence number wrap from 15 back to 0
@pytest.mark.parametrize("length", [8, 9, 12, 13, 14, 19, 20, 21, 27, 28, 62, 63, 64, 111, 112, 118, 119, 150, 200, 255, 256])
def test_segmented_request_is_reassembled(raw: RawCan, length: int):
    assert_reassembled(raw.request(filler_request(length)).payload)


def test_segmented_request_content_is_intact(raw: RawCan):
    # the DIDs only come back in this order if every byte landed in the right place
    message = raw.request(READ_ALL_DIDS)
    assert message.payload[0] == 0x62, f"expected a positive read response, got {message.payload.hex(' ')}"
    position = 1
    for did in ota.DID_NAMES:
        position = message.payload.find(did.to_bytes(2, "big"), position)
        assert position != -1, f"DID 0x{did:04X} missing or out of order in {message.payload.hex(' ')}"
        position += 2


@pytest.mark.parametrize("pad_byte", [0x00, 0xAA, 0xCC])
def test_padding_on_last_consecutive_frame_is_not_data(raw: RawCan, pad_byte: int):
    # 9 byte request: the last consecutive frame carries 3 bytes and 4 of padding
    message = raw.request(READ_ALL_DIDS, pad_byte=pad_byte)
    assert message.payload[0] == 0x62, f"padding changed the request the server saw: {message.payload.hex(' ')}"


def test_back_to_back_segmented_requests(raw: RawCan):
    for _ in range(10):
        assert_reassembled(raw.request(filler_request(64)).payload)


@pytest.mark.parametrize("length", [1, 6, 7])
def test_first_frame_with_single_frame_length_is_ignored(raw: RawCan, length: int):
    raw.send(bytes([0x10, length]) + filler_request(6))
    raw.expect_silence()
    assert_alive(raw)


def test_first_frame_shorter_than_8_bytes_is_ignored(raw: RawCan):
    raw.send(bytes([0x10, 20]) + filler_request(3))
    raw.expect_silence()
    assert_alive(raw)


def test_oversized_first_frame(raw: RawCan):
    """4095 bytes either overflows the receive buffer or has to be received in full"""
    payload = filler_request(4095)
    fc = raw.send_request(payload, allow_overflow=True)
    if fc.status == FS_OVERFLOW:
        raw.expect_silence()
        # an overflowed reception is dropped, these must not be taken as part of anything
        for frame in consecutive_frames(payload)[:3]:
            raw.send(frame)
        raw.expect_silence()
    else:
        assert_reassembled(raw.recv_response().payload)
    assert_alive(raw)


def test_escape_sequence_first_frame(raw: RawCan):
    """FF_DL = 0 with a 32 bit length (messages over 4095 bytes)"""
    raw.send(b"\x10\x00" + (1_000_000).to_bytes(4, "big") + PING)
    frames = raw.recv_all(0.5)
    assert len(frames) <= 1, "more than one frame in response to a first frame"
    if frames:
        # not an error to ignore it, but the only thing worth saying to 1 MB is overflow
        assert frames[0].pci == PCI_FC and frames[0].data[0] & 0xF == FS_OVERFLOW, f"expected overflow or silence, got {frames[0]}"
    time.sleep(N_CR_S + TIMEOUT_MARGIN_S)
    assert_alive(raw)


@pytest.mark.parametrize("sequence", [
    pytest.param([2, 3, 4], id="starts_at_2"),
    pytest.param([0, 1, 2], id="starts_at_0"),
    pytest.param([1, 1, 2], id="repeated"),
    pytest.param([1, 3, 4], id="skipped"),
    pytest.param([1, 2, 2], id="last_repeated"),
])
def test_wrong_sequence_number_aborts_reception(raw: RawCan, sequence: list[int]):
    # 27 bytes = first frame + exactly 3 consecutive frames
    payload = filler_request(27)
    raw.send(first_frame(payload))
    fc = raw.expect_flow_control(time.monotonic())
    assert fc.status == FS_CTS
    for sn, frame in zip(sequence, consecutive_frames(payload)):
        raw.send(bytes([0x20 | sn]) + frame[1:])
        time.sleep(max(fc.stmin_s, 0.002))
    raw.expect_silence(N_CR_S + TIMEOUT_MARGIN_S)
    assert_alive(raw)


def test_missing_consecutive_frames_time_out(raw: RawCan):
    payload = filler_request(20)
    raw.send(first_frame(payload))
    assert raw.expect_flow_control(time.monotonic()).status == FS_CTS
    # N_Cr expires, the reception is dropped without any error frame
    raw.expect_silence(N_CR_S + TIMEOUT_MARGIN_S)
    # too late, these no longer belong to anything
    for frame in consecutive_frames(payload):
        raw.send(frame)
        time.sleep(0.005)
    raw.expect_silence()
    assert_alive(raw)


def test_reception_resumes_within_n_cr(raw: RawCan):
    """A gap shorter than N_Cr between consecutive frames is not a timeout"""
    payload = filler_request(20)
    raw.send(first_frame(payload))
    assert raw.expect_flow_control(time.monotonic()).status == FS_CTS
    for frame in consecutive_frames(payload):
        time.sleep(N_CR_S / 2)
        raw.send(frame)
    assert_reassembled(raw.recv_response().payload)


def test_single_frame_interrupts_segmented_reception(raw: RawCan):
    """ISO 15765-2: a new single frame terminates the reception in progress and is processed"""
    payload = filler_request(20)
    raw.send(first_frame(payload))
    assert raw.expect_flow_control(time.monotonic()).status == FS_CTS
    raw.send(single_frame(PING))
    assert raw.recv_response(1.0).payload == PONG
    for frame in consecutive_frames(payload):
        raw.send(frame)
        time.sleep(0.005)
    raw.expect_silence()
    assert_alive(raw)


def test_first_frame_interrupts_segmented_reception(raw: RawCan):
    """ISO 15765-2: a new first frame terminates the reception in progress and starts a new one"""
    raw.send(first_frame(filler_request(27)))
    assert raw.expect_flow_control(time.monotonic()).status == FS_CTS
    # 9 bytes of valid read, would be rejected if bytes of the abandoned message leaked in
    message = raw.request(READ_ALL_DIDS)
    assert message.payload[0] == 0x62, f"expected a positive read response, got {message.payload.hex(' ')}"
    assert_alive(raw)


# =================================================================================================
# Segmented transmission (ECU -> tester)
# =================================================================================================
def test_segmented_response_is_complete(raw: RawCan, long_read: tuple[bytes, bytes]):
    request, expected = long_read
    message = raw.request(request)
    assert message.payload == expected
    assert all(frame.can_id == RXID for frame in message.frames)
    raw.expect_silence(0.2)


def test_segmented_response_waits_for_flow_control(raw: RawCan, long_read: tuple[bytes, bytes]):
    first = start_long_response(raw, long_read)
    raw.expect_silence(N_BS_S / 2)
    raw.send(flow_control())
    _, expected = long_read
    consecutive = collect_consecutive(raw, consecutive_count(expected))
    assert reassemble(first, consecutive, len(expected)) == expected


def test_missing_flow_control_aborts_transmission(raw: RawCan, long_read: tuple[bytes, bytes]):
    start_long_response(raw, long_read)
    # N_Bs expires, the ECU gives up on the response
    raw.expect_silence(N_BS_S + TIMEOUT_MARGIN_S)
    raw.send(flow_control())
    raw.expect_silence()
    assert_alive(raw)


@pytest.mark.parametrize("block_size", [1, 2, 3])
def test_ecu_honours_block_size(raw: RawCan, long_read: tuple[bytes, bytes], block_size: int):
    _, expected = long_read
    total_cfs = consecutive_count(expected)
    if total_cfs <= block_size:
        pytest.skip(f"longest response is only {total_cfs} consecutive frames")

    first = start_long_response(raw, long_read)
    consecutive = []
    while len(consecutive) < total_cfs:
        raw.send(flow_control(block_size=block_size))
        count = min(block_size, total_cfs - len(consecutive))
        consecutive += collect_consecutive(raw, count, first_sn=len(consecutive) + 1)
        # nothing more until the next flow control
        raw.expect_silence(0.2)
    assert reassemble(first, consecutive, len(expected)) == expected


@pytest.mark.parametrize("stmin_ms", [10, 50, 127])
def test_ecu_honours_stmin(raw: RawCan, long_read: tuple[bytes, bytes], stmin_ms: int):
    _, expected = long_read
    total_cfs = consecutive_count(expected)
    if total_cfs < 2:
        pytest.skip("longest response is a single consecutive frame")

    first = start_long_response(raw, long_read)
    raw.send(flow_control(stmin=stmin_ms))
    consecutive = collect_consecutive(raw, total_cfs)
    assert reassemble(first, consecutive, len(expected)) == expected
    gaps = [later.timestamp - earlier.timestamp for earlier, later in zip(consecutive, consecutive[1:])]
    # 1 ms of slack for timestamp jitter
    assert min(gaps) >= stmin_ms / 1000 - 0.001, f"consecutive frames only {min(gaps) * 1000:.2f} ms apart with STmin = {stmin_ms} ms"


@pytest.mark.parametrize("stmin", [0xF1, 0xF5, 0xF9])
def test_ecu_accepts_microsecond_stmin(raw: RawCan, long_read: tuple[bytes, bytes], stmin: int):
    _, expected = long_read
    first = start_long_response(raw, long_read)
    raw.send(flow_control(stmin=stmin))
    consecutive = collect_consecutive(raw, consecutive_count(expected))
    assert reassemble(first, consecutive, len(expected)) == expected


@pytest.mark.parametrize("stmin", [0x80, 0xF0, 0xFA, 0xFF])
def test_ecu_survives_reserved_stmin(raw: RawCan, long_read: tuple[bytes, bytes], stmin: int):
    """ISO 15765-2 says to use 127 ms for reserved values, here only completion is required"""
    _, expected = long_read
    first = start_long_response(raw, long_read)
    raw.send(flow_control(stmin=stmin))
    consecutive = collect_consecutive(raw, consecutive_count(expected))
    assert reassemble(first, consecutive, len(expected)) == expected


@pytest.mark.parametrize("pad_byte", [0x00, 0xAA, 0xCC])
def test_padded_flow_control_is_accepted(raw: RawCan, long_read: tuple[bytes, bytes], pad_byte: int):
    _, expected = long_read
    first = start_long_response(raw, long_read)
    raw.send(pad(flow_control(), pad_byte))
    consecutive = collect_consecutive(raw, consecutive_count(expected))
    assert reassemble(first, consecutive, len(expected)) == expected


def test_flow_control_wait_holds_transmission(raw: RawCan, long_read: tuple[bytes, bytes]):
    _, expected = long_read
    first = start_long_response(raw, long_read)
    # each WAIT restarts N_Bs, so the total hold can exceed it
    for _ in range(3):
        raw.send(flow_control(FS_WAIT))
        raw.expect_silence(N_BS_S / 2)
    raw.send(flow_control())
    consecutive = collect_consecutive(raw, consecutive_count(expected))
    assert reassemble(first, consecutive, len(expected)) == expected


def test_flow_control_overflow_aborts_transmission(raw: RawCan, long_read: tuple[bytes, bytes]):
    start_long_response(raw, long_read)
    raw.send(flow_control(FS_OVERFLOW))
    raw.expect_silence()
    raw.send(flow_control())
    raw.expect_silence()
    assert_alive(raw)


@pytest.mark.parametrize("status", [0x3, 0x7, 0xF])
def test_flow_control_with_reserved_status_aborts_transmission(raw: RawCan, long_read: tuple[bytes, bytes], status: int):
    start_long_response(raw, long_read)
    raw.send(flow_control(status))
    raw.expect_silence()
    time.sleep(N_BS_S + TIMEOUT_MARGIN_S)
    assert_alive(raw)


def test_truncated_flow_control_does_not_start_transmission(raw: RawCan, long_read: tuple[bytes, bytes]):
    start_long_response(raw, long_read)
    # no block size or STmin bytes
    raw.send(b"\x30")
    raw.expect_silence()
    time.sleep(N_BS_S + TIMEOUT_MARGIN_S)
    assert_alive(raw)


def test_ecu_recovers_after_aborted_transmission(raw: RawCan, long_read: tuple[bytes, bytes]):
    request, expected = long_read
    start_long_response(raw, long_read)
    raw.expect_silence(N_BS_S + TIMEOUT_MARGIN_S)
    assert raw.request(request).payload == expected
