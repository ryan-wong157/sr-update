"""0x10 diagnostic session control and the S3 session timeout"""
import time

import pytest

from ota_client import cli as ota

from ecu import (
    NRC_INCORRECT_LENGTH,
    NRC_SUBFUNCTION_NOT_SUPPORTED,
    S3_SERVER_S,
    SID_SESSION_CONTROL,
    SID_TESTER_PRESENT,
    SUPPRESS_POS_RSP,
    Ecu,
    KeyAlgo,
    fmt,
)

DEFAULT = ota.SESSION_DEFAULT
PROGRAMMING = ota.SESSION_PROGRAMMING

# how far either side of S3 the timeout tests sample
S3_MARGIN_S = 1.0


def session_request(session: int) -> bytes:
    return bytes([SID_SESSION_CONTROL, session])


# =================================================================================================
# Session changes
# =================================================================================================
@pytest.mark.parametrize("session", [DEFAULT, PROGRAMMING])
def test_session_control_response_format(ecu: Ecu, session: int):
    response = ecu.positive(session_request(session))
    assert len(response) == 6, f"expected SID, session, P2 and P2*: {fmt(response)}"
    assert response[1] == session
    p2_ms = int.from_bytes(response[2:4], "big")
    p2_star_ms = int.from_bytes(response[4:6], "big") * 10
    assert p2_ms > 0
    assert p2_star_ms >= p2_ms, f"P2* ({p2_star_ms} ms) is shorter than P2 ({p2_ms} ms)"


def test_starts_in_default_session(ecu: Ecu):
    assert ecu.session() == DEFAULT


def test_enter_programming_session(ecu: Ecu):
    ecu.enter_programming()
    assert ecu.session() == PROGRAMMING


def test_return_to_default_session(programming: Ecu):
    programming.enter_default()
    assert programming.session() == DEFAULT


@pytest.mark.parametrize("session", [DEFAULT, PROGRAMMING])
def test_reentering_the_active_session(ecu: Ecu, session: int):
    ecu.change_session(session)
    ecu.change_session(session)
    assert ecu.session() == session


def test_session_cycling(ecu: Ecu):
    for _ in range(10):
        ecu.enter_programming()
        assert ecu.session() == PROGRAMMING
        ecu.enter_default()
        assert ecu.session() == DEFAULT


# 0x03 extended and 0x04 safety system are real ISO sessions this server does not have
@pytest.mark.parametrize("session", [0x00, 0x03, 0x04, 0x05, 0x40, 0x60, 0x7F])
@pytest.mark.parametrize("start", [DEFAULT, PROGRAMMING])
def test_unsupported_session_is_rejected(ecu: Ecu, start: int, session: int):
    ecu.change_session(start)
    ecu.negative(session_request(session), NRC_SUBFUNCTION_NOT_SUPPORTED)
    assert ecu.session() == start, "a rejected session change still changed the session"


@pytest.mark.parametrize("payload", [
    pytest.param(bytes([SID_SESSION_CONTROL]), id="no_subfunction"),
    pytest.param(session_request(PROGRAMMING) + b"\x00", id="one_extra_byte"),
    pytest.param(session_request(PROGRAMMING) + bytes(20), id="segmented"),
])
def test_session_control_bad_length(ecu: Ecu, payload: bytes):
    ecu.negative(payload, NRC_INCORRECT_LENGTH)
    assert ecu.session() == DEFAULT, "a rejected session change still changed the session"


# =================================================================================================
# Suppress positive response
# =================================================================================================
def test_suppressed_change_to_programming(ecu: Ecu):
    ecu.silent(session_request(PROGRAMMING | SUPPRESS_POS_RSP))
    assert ecu.session() == PROGRAMMING


def test_suppressed_change_to_default(programming: Ecu):
    programming.silent(session_request(DEFAULT | SUPPRESS_POS_RSP))
    assert programming.session() == DEFAULT


def test_unsupported_session_is_not_suppressed(ecu: Ecu):
    """The suppress bit only hides positive responses"""
    ecu.negative(session_request(0x03 | SUPPRESS_POS_RSP), NRC_SUBFUNCTION_NOT_SUPPORTED)


# =================================================================================================
# S3 timeout
# =================================================================================================
@pytest.mark.slow
def test_session_times_out_without_traffic(programming: Ecu):
    time.sleep(S3_SERVER_S + S3_MARGIN_S)
    assert programming.session() == DEFAULT


@pytest.mark.slow
def test_session_survives_until_s3(programming: Ecu):
    time.sleep(S3_SERVER_S - S3_MARGIN_S)
    assert programming.session() == PROGRAMMING


@pytest.mark.slow
@pytest.mark.parametrize("keepalive", [
    pytest.param(bytes([SID_TESTER_PRESENT, 0x00]), id="tester_present"),
    pytest.param(bytes([SID_TESTER_PRESENT, SUPPRESS_POS_RSP]), id="tester_present_suppressed"),
])
def test_tester_present_keeps_session_alive(programming: Ecu, keepalive: bytes):
    deadline = time.monotonic() + 2 * S3_SERVER_S + S3_MARGIN_S
    while time.monotonic() < deadline:
        programming.request(keepalive, timeout=0.2)
        time.sleep(ota.KEEPALIVE_PERIOD_S)
    assert programming.session() == PROGRAMMING


@pytest.mark.slow
def test_any_request_restarts_s3(programming: Ecu):
    """S3 runs from the last request, not from when the session was entered"""
    time.sleep(S3_SERVER_S - S3_MARGIN_S)
    assert programming.session() == PROGRAMMING
    time.sleep(S3_SERVER_S - S3_MARGIN_S)
    assert programming.session() == PROGRAMMING


@pytest.mark.slow
def test_rejected_request_restarts_s3(programming: Ecu):
    time.sleep(S3_SERVER_S - S3_MARGIN_S)
    programming.negative(session_request(0x03), NRC_SUBFUNCTION_NOT_SUPPORTED)
    time.sleep(S3_SERVER_S - S3_MARGIN_S)
    assert programming.session() == PROGRAMMING


@pytest.mark.slow
def test_default_session_does_not_time_out_into_anything(ecu: Ecu):
    time.sleep(S3_SERVER_S + S3_MARGIN_S)
    assert ecu.session() == DEFAULT


@pytest.mark.slow
def test_session_can_be_reentered_after_timeout(programming: Ecu):
    time.sleep(S3_SERVER_S + S3_MARGIN_S)
    assert programming.session() == DEFAULT
    programming.enter_programming()
    assert programming.session() == PROGRAMMING


# =================================================================================================
# Session changes lock security access again
# =================================================================================================
@pytest.mark.parametrize("session", [DEFAULT, PROGRAMMING])
def test_session_change_relocks_security(unlocked: Ecu, session: int):
    assert unlocked.is_unlocked()
    unlocked.change_session(session)
    unlocked.enter_programming()
    assert not unlocked.is_unlocked(), "security access survived a session change"


@pytest.mark.slow
def test_session_timeout_relocks_security(unlocked: Ecu, key_algo: KeyAlgo):
    assert unlocked.is_unlocked()
    time.sleep(S3_SERVER_S + S3_MARGIN_S)
    unlocked.enter_programming()
    assert not unlocked.is_unlocked(), "security access survived the session timeout"
