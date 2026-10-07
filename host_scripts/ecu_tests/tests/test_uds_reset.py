"""0x11 ECU reset"""
import time

import pytest

from ota_client import cli as ota

from ecu import (
    NRC_INCORRECT_LENGTH,
    NRC_SUBFUNCTION_NOT_SUPPORTED,
    SID_ECU_RESET,
    SUPPRESS_POS_RSP,
    Ecu,
    KeyAlgo,
    fmt,
)

HARD_RESET = 0x01

pytestmark = pytest.mark.slow


def reset_request(reset_type: int) -> bytes:
    return bytes([SID_ECU_RESET, reset_type])


def test_hard_reset_is_acknowledged_and_ecu_returns(ecu: Ecu, boot_timeout: float):
    response = ecu.request(reset_request(HARD_RESET))
    assert response == bytes([SID_ECU_RESET + 0x40, HARD_RESET]), f"unexpected reset response: {fmt(response)}"
    time.sleep(0.2)
    assert ecu.wait_alive(boot_timeout), f"ECU did not come back within {boot_timeout} s"
    assert ecu.session() == ota.SESSION_DEFAULT


def test_reset_from_programming_session_returns_to_default(programming: Ecu, boot_timeout: float):
    programming.hard_reset(boot_timeout)
    assert programming.session() == ota.SESSION_DEFAULT


def test_reset_locks_security(unlocked: Ecu, boot_timeout: float):
    assert unlocked.is_unlocked()
    unlocked.hard_reset(boot_timeout)
    unlocked.enter_programming()
    assert not unlocked.is_unlocked(), "security access survived a reset"


def test_unlock_works_after_reset(ecu: Ecu, key_algo: KeyAlgo, boot_timeout: float):
    ecu.hard_reset(boot_timeout)
    ecu.enter_programming()
    ecu.unlock(key_algo)
    assert ecu.is_unlocked()


def test_suppressed_reset_still_resets(programming: Ecu, boot_timeout: float):
    programming.silent(reset_request(HARD_RESET | SUPPRESS_POS_RSP))
    assert programming.wait_alive(boot_timeout), f"ECU did not come back within {boot_timeout} s"
    # only a reset gets from programming back to default this quickly without a response
    assert programming.session() == ota.SESSION_DEFAULT


def test_repeated_resets(ecu: Ecu, boot_timeout: float):
    for _ in range(3):
        ecu.hard_reset(boot_timeout)
        assert ecu.session() == ota.SESSION_DEFAULT


# 0x02 key off on, 0x03 soft reset, 0x04/0x05 rapid power shutdown
@pytest.mark.parametrize("reset_type", [0x00, 0x02, 0x03, 0x04, 0x05, 0x40, 0x7F])
def test_unsupported_reset_type_is_rejected(programming: Ecu, reset_type: int):
    programming.negative(reset_request(reset_type), NRC_SUBFUNCTION_NOT_SUPPORTED)
    # still in programming = it did not reset anyway
    assert programming.session() == ota.SESSION_PROGRAMMING


def test_unsupported_reset_type_is_not_suppressed(programming: Ecu):
    programming.negative(reset_request(0x03 | SUPPRESS_POS_RSP), NRC_SUBFUNCTION_NOT_SUPPORTED)
    assert programming.session() == ota.SESSION_PROGRAMMING


@pytest.mark.parametrize("payload", [
    pytest.param(bytes([SID_ECU_RESET]), id="no_subfunction"),
    pytest.param(reset_request(HARD_RESET) + b"\x00", id="one_extra_byte"),
    pytest.param(reset_request(HARD_RESET) + bytes(20), id="segmented"),
])
def test_reset_bad_length(programming: Ecu, payload: bytes):
    programming.negative(payload, NRC_INCORRECT_LENGTH)
    assert programming.session() == ota.SESSION_PROGRAMMING
