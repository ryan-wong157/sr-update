"""0x27 security access

Every test that sends a bad key uses clear_bad_keys, which unlocks properly afterwards so the
failed attempts of one test never add up with the next and trip the lockout by accident.
"""
import os
import time

import pytest

from ota_client import cli as ota

from ecu import (
    NRC_EXCEEDED_ATTEMPTS,
    NRC_INCORRECT_LENGTH,
    NRC_INVALID_KEY,
    NRC_REQUEST_SEQUENCE_ERROR,
    NRC_SUBFUNCTION_NOT_SUPPORTED,
    NRC_TIME_DELAY_NOT_EXPIRED,
    SECURITY_LOCKOUT_S,
    SECURITY_MAX_ATTEMPTS,
    SEED_REQUEST,
    SEND_KEY,
    SID_SECURITY_ACCESS,
    Ecu,
    KeyAlgo,
    fmt,
    nrc_of,
)

KEY_ACCEPTED = bytes([SID_SECURITY_ACCESS + 0x40, SEND_KEY])


def random_key() -> bytes:
    return os.urandom(ota.SIGNATURE_LEN)


def send_key_request(key: bytes) -> bytes:
    return bytes([SID_SECURITY_ACCESS, SEND_KEY]) + key


# =================================================================================================
# Seeds
# =================================================================================================
def test_seed_request(programming: Ecu):
    seed = programming.request_seed()
    assert any(seed), "locked server handed out an all zero seed"


def test_seeds_are_not_repeated(programming: Ecu):
    seeds = [programming.request_seed() for _ in range(10)]
    assert len(set(seeds)) == len(seeds), "the same seed was handed out twice"
    assert len({len(seed) for seed in seeds}) == 1, "seed length changes between requests"


def test_seed_is_different_after_session_change(programming: Ecu):
    first = programming.request_seed()
    programming.enter_default()
    programming.enter_programming()
    assert programming.request_seed() != first


@pytest.mark.slow
def test_seed_is_different_after_reset(programming: Ecu, boot_timeout: float):
    """A seed that repeats after a reset lets a recorded key be replayed"""
    seeds = set()
    for _ in range(3):
        programming.enter_programming()
        seeds.add(programming.request_seed())
        programming.hard_reset(boot_timeout)
    assert len(seeds) == 3, "the same seed came back after a reset"


# =================================================================================================
# Unlocking
# =================================================================================================
def test_valid_key_unlocks(programming: Ecu, key_algo: KeyAlgo):
    seed = programming.request_seed()
    assert programming.send_key(key_algo(seed)) == KEY_ACCEPTED


def test_seed_is_all_zero_once_unlocked(unlocked: Ecu):
    """ISO 14229: asking for a seed on an already unlocked level returns zeros"""
    seed = unlocked.request_seed()
    assert not any(seed), f"expected an all zero seed, got {seed.hex()}"
    # and it stays unlocked
    assert unlocked.is_unlocked()


def test_unlock_after_replaced_seed(programming: Ecu, key_algo: KeyAlgo):
    """A new seed request replaces the pending one, the key for the newest seed is the valid one"""
    programming.request_seed()
    programming.request_seed()
    seed = programming.request_seed()
    assert programming.send_key(key_algo(seed)) == KEY_ACCEPTED


def test_unlock_repeatedly(ecu: Ecu, key_algo: KeyAlgo):
    for _ in range(5):
        ecu.enter_programming()
        assert not ecu.is_unlocked()
        ecu.unlock(key_algo)
        assert ecu.is_unlocked()


def test_tester_present_does_not_disturb_pending_seed(programming: Ecu, key_algo: KeyAlgo):
    """The keepalive runs between seed and key in real use"""
    seed = programming.request_seed()
    programming.positive(b"\x3E\x00")
    programming.positive(b"\x22\xF1\x86")
    assert programming.send_key(key_algo(seed)) == KEY_ACCEPTED


# =================================================================================================
# Bad keys
# =================================================================================================
def test_key_without_seed_is_rejected(programming: Ecu, clear_bad_keys: None):
    programming.negative(send_key_request(random_key()), NRC_REQUEST_SEQUENCE_ERROR)
    assert not programming.is_unlocked()


def test_valid_looking_key_without_seed_is_rejected(programming: Ecu, key_algo: KeyAlgo, clear_bad_keys: None):
    """A key for a seed from an earlier session, with nothing pending in this one"""
    seed = programming.request_seed()
    key = key_algo(seed)
    programming.enter_default()
    programming.enter_programming()
    programming.negative(send_key_request(key), NRC_REQUEST_SEQUENCE_ERROR)
    assert not programming.is_unlocked()


def test_random_key_is_rejected(programming: Ecu, clear_bad_keys: None):
    programming.request_seed()
    programming.negative(send_key_request(random_key()), NRC_INVALID_KEY)
    assert not programming.is_unlocked()


def test_corrupted_key_is_rejected(programming: Ecu, key_algo: KeyAlgo, clear_bad_keys: None):
    """One flipped bit in an otherwise valid key"""
    seed = programming.request_seed()
    key = bytearray(key_algo(seed))
    key[-1] ^= 0x01
    programming.negative(send_key_request(bytes(key)), NRC_INVALID_KEY)
    assert not programming.is_unlocked()


def test_key_for_replaced_seed_is_rejected(programming: Ecu, key_algo: KeyAlgo, clear_bad_keys: None):
    old_seed = programming.request_seed()
    programming.request_seed()
    programming.negative(send_key_request(key_algo(old_seed)), NRC_INVALID_KEY)
    assert not programming.is_unlocked()


def test_seed_is_consumed_by_a_bad_key(programming: Ecu, key_algo: KeyAlgo, clear_bad_keys: None):
    """One guess per seed: after a bad key even the right key for that seed must not work"""
    seed = programming.request_seed()
    programming.negative(send_key_request(random_key()), NRC_INVALID_KEY)
    response = programming.send_key(key_algo(seed))
    assert response != KEY_ACCEPTED, "the correct key was accepted after a failed guess on the same seed"
    assert nrc_of(response, SID_SECURITY_ACCESS) in (NRC_REQUEST_SEQUENCE_ERROR, NRC_INVALID_KEY), f"unexpected response: {fmt(response)}"
    assert not programming.is_unlocked()


def test_key_cannot_be_replayed(programming: Ecu, key_algo: KeyAlgo, clear_bad_keys: None):
    """A key captured from one unlock is useless for the next seed"""
    seed = programming.request_seed()
    key = key_algo(seed)
    assert programming.send_key(key) == KEY_ACCEPTED
    programming.enter_programming()
    assert programming.request_seed() != seed
    programming.negative(send_key_request(key), NRC_INVALID_KEY)
    assert not programming.is_unlocked()


def test_key_while_unlocked_is_rejected(unlocked: Ecu, clear_bad_keys: None):
    """No seed is pending once unlocked, so a stray key is a sequence error"""
    unlocked.negative(send_key_request(random_key()), NRC_REQUEST_SEQUENCE_ERROR, NRC_INVALID_KEY)


@pytest.mark.parametrize("length", [1, ota.SIGNATURE_LEN - 1, ota.SIGNATURE_LEN + 1, 2 * ota.SIGNATURE_LEN])
def test_key_with_wrong_length_is_rejected(programming: Ecu, clear_bad_keys: None, length: int):
    programming.request_seed()
    programming.negative(send_key_request(os.urandom(length)), NRC_INCORRECT_LENGTH, NRC_INVALID_KEY)
    assert not programming.is_unlocked()


def test_truncated_valid_key_is_rejected(programming: Ecu, key_algo: KeyAlgo, clear_bad_keys: None):
    seed = programming.request_seed()
    programming.negative(send_key_request(key_algo(seed)[:-1]), NRC_INCORRECT_LENGTH, NRC_INVALID_KEY)
    assert not programming.is_unlocked()


def test_empty_key_is_rejected(programming: Ecu, clear_bad_keys: None):
    programming.request_seed()
    programming.negative(send_key_request(b""), NRC_INCORRECT_LENGTH)
    assert not programming.is_unlocked()


def test_unlock_works_after_a_bad_key(programming: Ecu, key_algo: KeyAlgo):
    programming.request_seed()
    programming.negative(send_key_request(random_key()), NRC_INVALID_KEY)
    programming.unlock(key_algo)
    assert programming.is_unlocked()


# =================================================================================================
# Malformed requests
# =================================================================================================
# odd = seed request, even = send key, only level 0x01/0x02 exists
@pytest.mark.parametrize("subfunction", [0x00, 0x03, 0x05, 0x11, 0x41, 0x43, 0x7F])
def test_unsupported_seed_level_is_rejected(programming: Ecu, subfunction: int):
    programming.negative(bytes([SID_SECURITY_ACCESS, subfunction]), NRC_SUBFUNCTION_NOT_SUPPORTED)


@pytest.mark.parametrize("subfunction", [0x04, 0x06, 0x12, 0x42, 0x7E])
def test_unsupported_key_level_is_rejected(programming: Ecu, clear_bad_keys: None, subfunction: int):
    programming.request_seed()
    programming.negative(bytes([SID_SECURITY_ACCESS, subfunction]) + random_key(), NRC_SUBFUNCTION_NOT_SUPPORTED)
    assert not programming.is_unlocked()


def test_security_access_without_subfunction(programming: Ecu):
    programming.negative(bytes([SID_SECURITY_ACCESS]), NRC_INCORRECT_LENGTH)


# =================================================================================================
# Lockout
# =================================================================================================
def fail_until_locked_out(ecu: Ecu) -> None:
    for attempt in range(1, SECURITY_MAX_ATTEMPTS + 1):
        ecu.request_seed(wait_out_delay=False)
        expected = NRC_EXCEEDED_ATTEMPTS if attempt == SECURITY_MAX_ATTEMPTS else NRC_INVALID_KEY
        ecu.negative(send_key_request(random_key()), expected)


def wait_out_lockout(ecu: Ecu, already_waited: float = 0.0) -> None:
    """Sleeps to just past the end of the lockout, keeping the programming session alive"""
    deadline = time.monotonic() + SECURITY_LOCKOUT_S + 2 - already_waited
    while time.monotonic() < deadline:
        ecu.positive(b"\x3E\x00")
        time.sleep(min(ota.KEEPALIVE_PERIOD_S, max(deadline - time.monotonic(), 0)))


@pytest.mark.lockout
@pytest.mark.slow
def test_bad_keys_lock_out_security_access(programming: Ecu, key_algo: KeyAlgo):
    fail_until_locked_out(programming)
    locked_at = time.monotonic()

    # no seeds, and the valid key for an old seed is not a way in either
    programming.negative(bytes([SID_SECURITY_ACCESS, SEED_REQUEST]), NRC_TIME_DELAY_NOT_EXPIRED)
    response = programming.send_key(random_key())
    assert nrc_of(response, SID_SECURITY_ACCESS) in (NRC_TIME_DELAY_NOT_EXPIRED, NRC_REQUEST_SEQUENCE_ERROR, NRC_EXCEEDED_ATTEMPTS), \
        f"unexpected response to a key during lockout: {fmt(response)}"

    # still locked out most of the way through the delay
    hold = SECURITY_LOCKOUT_S * 0.75
    while time.monotonic() - locked_at < hold:
        programming.positive(b"\x3E\x00")
        time.sleep(ota.KEEPALIVE_PERIOD_S)
    programming.negative(bytes([SID_SECURITY_ACCESS, SEED_REQUEST]), NRC_TIME_DELAY_NOT_EXPIRED)

    wait_out_lockout(programming, already_waited=time.monotonic() - locked_at)
    programming.unlock(key_algo)
    assert programming.is_unlocked()


@pytest.mark.lockout
@pytest.mark.slow
def test_lockout_survives_session_changes(programming: Ecu, key_algo: KeyAlgo):
    fail_until_locked_out(programming)
    locked_at = time.monotonic()
    for _ in range(3):
        programming.enter_default()
        programming.enter_programming()
        programming.negative(bytes([SID_SECURITY_ACCESS, SEED_REQUEST]), NRC_TIME_DELAY_NOT_EXPIRED)

    wait_out_lockout(programming, already_waited=time.monotonic() - locked_at)
    programming.unlock(key_algo)


@pytest.mark.lockout
@pytest.mark.slow
def test_successful_unlock_resets_the_attempt_counter(programming: Ecu, key_algo: KeyAlgo):
    """Many bad keys spread out between good unlocks never reach the limit"""
    try:
        for _ in range(SECURITY_MAX_ATTEMPTS + 1):
            for _ in range(SECURITY_MAX_ATTEMPTS - 1):
                programming.request_seed(wait_out_delay=False)
                programming.negative(send_key_request(random_key()), NRC_INVALID_KEY)
            programming.unlock(key_algo)
            programming.enter_programming()
    finally:
        # if the counter was not reset this leaves the server locked out, unlock() waits that out
        programming.wait_alive(3.0)
        programming.enter_programming()
        programming.unlock(key_algo)


@pytest.mark.lockout
@pytest.mark.slow
def test_attempt_counter_survives_session_changes(programming: Ecu, key_algo: KeyAlgo):
    """Bouncing through the default session between guesses must not give unlimited guesses"""
    for attempt in range(1, SECURITY_MAX_ATTEMPTS + 1):
        programming.request_seed(wait_out_delay=False)
        expected = NRC_EXCEEDED_ATTEMPTS if attempt == SECURITY_MAX_ATTEMPTS else NRC_INVALID_KEY
        programming.negative(send_key_request(random_key()), expected)
        programming.enter_default()
        programming.enter_programming()
    locked_at = time.monotonic()
    programming.negative(bytes([SID_SECURITY_ACCESS, SEED_REQUEST]), NRC_TIME_DELAY_NOT_EXPIRED)

    wait_out_lockout(programming, already_waited=time.monotonic() - locked_at)
    programming.unlock(key_algo)
