"""Service dispatch, tester present (0x3E), the 0x80 test service and response timing"""
import pytest

from ecu import (
    NRC_INCORRECT_LENGTH,
    NRC_SERVICE_NOT_SUPPORTED,
    NRC_SUBFUNCTION_NOT_SUPPORTED,
    SID_TEST_SERVICE,
    SID_TESTER_PRESENT,
    SUPPRESS_POS_RSP,
    Ecu,
    fmt,
    nrc_of,
    read_request,
)

TESTER_PRESENT = bytes([SID_TESTER_PRESENT, 0x00])
TESTER_PRESENT_OK = bytes([SID_TESTER_PRESENT + 0x40, 0x00])

# allowance for the Pi's scheduling on top of the server's advertised P2
P2_HOST_MARGIN_S = 0.05


# =================================================================================================
# Service dispatch
# =================================================================================================
# ISO 14229 services cli.py does not list as implemented
@pytest.mark.parametrize("sid", [0x14, 0x19, 0x23, 0x24, 0x28, 0x2A, 0x2C, 0x2E, 0x2F, 0x31, 0x35, 0x38, 0x3D, 0x83, 0x84, 0x85, 0x86, 0x87])
def test_unsupported_service_is_rejected(ecu: Ecu, sid: int):
    ecu.negative(bytes([sid, 0x01]), NRC_SERVICE_NOT_SUPPORTED)


@pytest.mark.parametrize("sid", [0x00, 0x01, 0x0F, 0x12, 0x21, 0x3F, 0xBA, 0xBF])
def test_unassigned_service_id_is_rejected(ecu: Ecu, sid: int):
    ecu.negative(bytes([sid]), NRC_SERVICE_NOT_SUPPORTED)


def test_unsupported_service_is_rejected_in_programming_session(programming: Ecu):
    programming.negative(b"\x31\x01\xFF\x00", NRC_SERVICE_NOT_SUPPORTED)


@pytest.mark.parametrize("sid", [0x50, 0x62, 0x67, 0x7E, 0x7F, 0xC0, 0xFF])
def test_response_service_id_as_request(ecu: Ecu, sid: int):
    """SIDs from the response ranges are not requests, a server may ignore them or reject them"""
    response = ecu.request(bytes([sid, 0x00]), timeout=0.5)
    assert response is None or nrc_of(response, sid) == NRC_SERVICE_NOT_SUPPORTED, f"unexpected response: {fmt(response)}"
    assert ecu.request(TESTER_PRESENT) == TESTER_PRESENT_OK


# =================================================================================================
# 0x3E tester present
# =================================================================================================
def test_tester_present(ecu: Ecu):
    assert ecu.request(TESTER_PRESENT) == TESTER_PRESENT_OK


def test_tester_present_in_programming_session(programming: Ecu):
    assert programming.request(TESTER_PRESENT) == TESTER_PRESENT_OK


def test_tester_present_suppressed(ecu: Ecu):
    ecu.silent(bytes([SID_TESTER_PRESENT, SUPPRESS_POS_RSP]))
    assert ecu.request(TESTER_PRESENT) == TESTER_PRESENT_OK


@pytest.mark.parametrize("subfunction", [0x01, 0x02, 0x40, 0x7F])
def test_tester_present_bad_subfunction(ecu: Ecu, subfunction: int):
    ecu.negative(bytes([SID_TESTER_PRESENT, subfunction]), NRC_SUBFUNCTION_NOT_SUPPORTED)


def test_tester_present_bad_subfunction_is_not_suppressed(ecu: Ecu):
    """The suppress bit only hides positive responses"""
    ecu.negative(bytes([SID_TESTER_PRESENT, SUPPRESS_POS_RSP | 0x01]), NRC_SUBFUNCTION_NOT_SUPPORTED)


@pytest.mark.parametrize("payload", [
    pytest.param(bytes([SID_TESTER_PRESENT]), id="no_subfunction"),
    pytest.param(TESTER_PRESENT + b"\x00", id="one_extra_byte"),
    pytest.param(TESTER_PRESENT + bytes(5), id="full_single_frame"),
    pytest.param(TESTER_PRESENT + bytes(30), id="segmented"),
])
def test_tester_present_bad_length(ecu: Ecu, payload: bytes):
    ecu.negative(payload, NRC_INCORRECT_LENGTH)


def test_tester_present_bad_length_is_not_suppressed(ecu: Ecu):
    ecu.negative(bytes([SID_TESTER_PRESENT, SUPPRESS_POS_RSP, 0x00]), NRC_INCORRECT_LENGTH)


def test_repeated_tester_present(ecu: Ecu):
    for _ in range(100):
        assert ecu.request(TESTER_PRESENT) == TESTER_PRESENT_OK


# =================================================================================================
# 0x80 test service
# =================================================================================================
def test_test_service_responds(ecu: Ecu):
    response = ecu.request(bytes([SID_TEST_SERVICE]))
    assert response is not None, "no response from the test service"
    assert nrc_of(response, SID_TEST_SERVICE) is None, f"test service was rejected: {fmt(response)}"


def test_test_service_in_programming_session(programming: Ecu):
    response = programming.request(bytes([SID_TEST_SERVICE]))
    assert response is not None, "no response from the test service"
    assert nrc_of(response, SID_TEST_SERVICE) is None, f"test service was rejected: {fmt(response)}"


# =================================================================================================
# Timing
# =================================================================================================
@pytest.mark.parametrize("payload", [
    pytest.param(TESTER_PRESENT, id="tester_present"),
    pytest.param(read_request(0xF186), id="read_session"),
    pytest.param(b"\x10\x01", id="session_control"),
    pytest.param(b"\x31\x01\xFF\x00", id="unsupported_service"),
])
def test_response_within_p2(ecu: Ecu, payload: bytes):
    """The first response has to arrive within the P2 the server advertises"""
    session_response = ecu.enter_default()
    p2_s = int.from_bytes(session_response[2:4], "big") / 1000
    worst = 0.0
    for _ in range(10):
        assert ecu.request(payload) is not None
        worst = max(worst, ecu.last_elapsed)
    assert worst <= p2_s + P2_HOST_MARGIN_S, f"slowest response took {worst * 1000:.1f} ms, P2 is {p2_s * 1000:.0f} ms"
