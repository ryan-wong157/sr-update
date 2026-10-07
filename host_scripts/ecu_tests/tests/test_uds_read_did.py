"""0x22 read data by identifier"""
import pytest

from ota_client import cli as ota

from ecu import (
    NRC_INCORRECT_LENGTH,
    NRC_REQUEST_OUT_OF_RANGE,
    SID_READ_DATA,
    Ecu,
    fmt,
    nrc_of,
    read_request,
)

DIDS = list(ota.DID_NAMES)
DID_SESSION = 0xF186
# none of these are in cli.py's DID list
UNKNOWN_DIDS = [0x0000, 0x0001, 0x1234, 0xF100, 0xF17F, 0xF182, 0xF185, 0xF187, 0xF190, 0xF192, 0xF1FF, 0xFFFF]


def read_one(ecu: Ecu, did: int) -> bytes:
    """Data record of a single DID"""
    response = ecu.positive(read_request(did))
    assert response[1:3] == did.to_bytes(2, "big"), f"response does not echo DID 0x{did:04X}: {fmt(response)}"
    assert len(response) > 3, f"DID 0x{did:04X} came back with no data"
    return response[3:]


# =================================================================================================
# Single DIDs
# =================================================================================================
@pytest.mark.parametrize("did", DIDS, ids=lambda did: f"0x{did:04X}")
def test_read_did(ecu: Ecu, did: int):
    read_one(ecu, did)


@pytest.mark.parametrize("did", DIDS, ids=lambda did: f"0x{did:04X}")
def test_read_did_in_programming_session(programming: Ecu, did: int):
    read_one(programming, did)


@pytest.mark.parametrize("did", [did for did in DIDS if did != DID_SESSION], ids=lambda did: f"0x{did:04X}")
def test_did_value_is_stable(ecu: Ecu, did: int):
    """Versions and the ECU ID are constants: same on every read and in every session"""
    value = read_one(ecu, did)
    assert read_one(ecu, did) == value
    ecu.enter_programming()
    assert read_one(ecu, did) == value


def test_session_did_tracks_the_session(ecu: Ecu):
    assert read_one(ecu, DID_SESSION) == bytes([ota.SESSION_DEFAULT])
    ecu.enter_programming()
    assert read_one(ecu, DID_SESSION) == bytes([ota.SESSION_PROGRAMMING])
    ecu.enter_default()
    assert read_one(ecu, DID_SESSION) == bytes([ota.SESSION_DEFAULT])


# =================================================================================================
# Multiple DIDs
# =================================================================================================
def expected_records(ecu: Ecu, dids: list[int]) -> bytes:
    return bytes([SID_READ_DATA + 0x40]) + b"".join(did.to_bytes(2, "big") + read_one(ecu, did) for did in dids)


@pytest.mark.parametrize("dids", [
    pytest.param(DIDS[:2], id="first_two"),
    pytest.param(DIDS[:3], id="first_three"),
    pytest.param(DIDS, id="all"),
    pytest.param(DIDS[::-1], id="all_reversed"),
    pytest.param(DIDS[1::2] + DIDS[::2], id="all_shuffled"),
])
def test_read_multiple_dids(ecu: Ecu, dids: list[int]):
    """Records come back in request order, each identical to reading that DID alone"""
    expected = expected_records(ecu, dids)
    assert ecu.positive(read_request(*dids)) == expected


def test_read_all_dids_in_programming_session(programming: Ecu):
    expected = expected_records(programming, DIDS)
    assert programming.positive(read_request(*DIDS)) == expected


def test_same_did_twice(ecu: Ecu):
    """Either answered twice or rejected, but never half answered"""
    response = ecu.request(read_request(DID_SESSION, DID_SESSION))
    if nrc_of(response, SID_READ_DATA) is None:
        assert response == expected_records(ecu, [DID_SESSION, DID_SESSION]), f"unexpected response: {fmt(response)}"


def test_too_many_dids(ecu: Ecu):
    """A request for more DIDs than the server can answer gets a response, positive or not"""
    # kept under 256 bytes so the request itself fits the server's receive buffer
    dids = DIDS * 15
    response = ecu.request(read_request(*dids))
    assert response is not None, f"no response to a {len(dids)} DID read"
    nrc = nrc_of(response, SID_READ_DATA)
    if nrc is None:
        assert response == expected_records(ecu, dids)
    else:
        # 0x14 = response too long
        assert nrc in (NRC_INCORRECT_LENGTH, 0x14, NRC_REQUEST_OUT_OF_RANGE), f"unexpected NRC 0x{nrc:02X}"


# =================================================================================================
# Rejections
# =================================================================================================
@pytest.mark.parametrize("did", UNKNOWN_DIDS, ids=lambda did: f"0x{did:04X}")
def test_unknown_did_is_rejected(ecu: Ecu, did: int):
    ecu.negative(read_request(did), NRC_REQUEST_OUT_OF_RANGE)


def test_only_unknown_dids_is_rejected(ecu: Ecu):
    ecu.negative(read_request(*UNKNOWN_DIDS[:3]), NRC_REQUEST_OUT_OF_RANGE)


@pytest.mark.parametrize("dids", [
    pytest.param([DID_SESSION, 0x1234], id="known_then_unknown"),
    pytest.param([0x1234, DID_SESSION], id="unknown_then_known"),
])
def test_known_and_unknown_dids_mixed(ecu: Ecu, dids: list[int]):
    """ISO 14229 answers with the supported DIDs only, rejecting the whole request is the common shortcut"""
    response = ecu.request(read_request(*dids))
    if nrc_of(response, SID_READ_DATA) is None:
        assert response == expected_records(ecu, [DID_SESSION]), f"unexpected response: {fmt(response)}"
    else:
        assert nrc_of(response, SID_READ_DATA) == NRC_REQUEST_OUT_OF_RANGE, f"unexpected response: {fmt(response)}"


@pytest.mark.parametrize("payload", [
    pytest.param(bytes([SID_READ_DATA]), id="no_did"),
    pytest.param(bytes([SID_READ_DATA, 0xF1]), id="half_a_did"),
    pytest.param(read_request(DID_SESSION) + b"\xF1", id="one_and_a_half_dids"),
    pytest.param(read_request(*DIDS) + b"\xF1", id="segmented_odd_length"),
])
def test_read_bad_length(ecu: Ecu, payload: bytes):
    ecu.negative(payload, NRC_INCORRECT_LENGTH)
