import logging
import socket
import sys
from pathlib import Path

import pytest

# NOTE: importing ota_client.main also runs its udsoncan.setup_logging() call
from ota_client.main import RXID, TXID, configure, get_auth_key

from ecu import Ecu, KeyAlgo, read_request
from ota_client import cli as ota
from rawcan import RawCan

OPT_IN_MARKERS = {"flash": "--flash", "lockout": "--lockout"}


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("ecu")
    group.addoption("--interface", default="can0", help="SocketCAN interface (default: can0)")
    group.addoption("--key-file", type=Path, default=None, help="Path to base64-encoded auth key file")
    group.addoption("--boot-timeout", type=float, default=15.0, help="Seconds to wait for the ECU to answer after a reset (default: 15)")
    group.addoption("--flash", action="store_true", help="Also run the tests that erase and write firmware slot B")
    group.addoption("--lockout", action="store_true", help="Also run the tests that trigger the 60 s security access lockout")


def pytest_configure(config: pytest.Config) -> None:
    # same as ota_client's cli without --verbose, every request and frame is too noisy
    logging.getLogger("UdsClient").setLevel(logging.WARNING)
    logging.getLogger("Connection").setLevel(logging.WARNING)


def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]) -> None:
    for marker, option in OPT_IN_MARKERS.items():
        if config.getoption(option):
            continue
        skip = pytest.mark.skip(reason=f"needs {option}")
        for item in items:
            if marker in item.keywords:
                item.add_marker(skip)


# =================================================================================================
# Session wide
# =================================================================================================
@pytest.fixture(scope="session")
def interface(pytestconfig: pytest.Config) -> str:
    name = pytestconfig.getoption("--interface")
    if not sys.platform.startswith("linux"):
        pytest.exit("These tests need SocketCAN, run them on the Raspberry Pi", returncode=2)
    try:
        socket.if_nametoindex(name)
    except OSError:
        pytest.exit(f"CAN interface {name} does not exist, bring it up first", returncode=2)
    return name


@pytest.fixture(scope="session")
def boot_timeout(pytestconfig: pytest.Config) -> float:
    return pytestconfig.getoption("--boot-timeout")


@pytest.fixture(scope="session", autouse=True)
def ecu_present(interface: str, boot_timeout: float) -> None:
    """Stops the whole run early instead of failing every test when nothing answers"""
    ecu = Ecu(interface)
    try:
        if not ecu.wait_alive(boot_timeout):
            pytest.exit(f"ECU does not answer tester present on {interface} (tx 0x{TXID:08X}, rx 0x{RXID:08X})", returncode=2)
    finally:
        ecu.close()


@pytest.fixture(scope="session")
def key_algo(pytestconfig: pytest.Config) -> KeyAlgo:
    """Computes the key for a seed with ota_client's security algorithm"""
    config = configure(get_auth_key(pytestconfig.getoption("--key-file")))
    algo = config["security_algo"]
    params = config.get("security_algo_params")
    # udsoncan passes whichever of these the algorithm declares
    accepted = algo.__code__.co_varnames[:algo.__code__.co_argcount]

    def compute(seed: bytes) -> bytes:
        available = {"level": ota.SECURITY_LEVEL, "seed": seed, "params": params}
        return algo(**{name: value for name, value in available.items() if name in accepted})

    return compute


@pytest.fixture(scope="session")
def long_read(interface: str, ecu_present: None) -> tuple[bytes, bytes]:
    """(request, expected response) for the longest read the server accepts

    Reading every DID at once needs a segmented request and response, repeating the list makes
    the response span more consecutive frames if the server allows it.
    """
    ecu = Ecu(interface)
    try:
        for repeats in (4, 3, 2, 1):
            request = read_request(*(list(ota.DID_NAMES) * repeats))
            response = ecu.request(request)
            if response is not None and response[0] == 0x62:
                assert len(response) > 7, "reading every DID should not fit in a single frame"
                return request, response
        pytest.fail("ECU rejected reading all of its DIDs in one request")
    finally:
        ecu.close()


# =================================================================================================
# Per test
# =================================================================================================
@pytest.fixture
def ecu(interface: str, ecu_present: None) -> Ecu:
    """UDS access, starting from the default session with security locked"""
    ecu = Ecu(interface)
    try:
        # a previous test may have left the ECU resetting or mid reception
        assert ecu.wait_alive(3.0), "ECU is not answering before the test"
        ecu.enter_default()
        yield ecu
        try:
            ecu.request(bytes([0x10, ota.SESSION_DEFAULT]))
        except OSError:
            pass
    finally:
        ecu.close()


@pytest.fixture
def programming(ecu: Ecu) -> Ecu:
    ecu.enter_programming()
    return ecu


@pytest.fixture
def unlocked(programming: Ecu, key_algo: KeyAlgo) -> Ecu:
    programming.unlock(key_algo)
    return programming


@pytest.fixture
def clear_bad_keys(ecu: Ecu, key_algo: KeyAlgo) -> None:
    """For tests that send bad keys: a good unlock afterwards so the attempts do not add up across tests"""
    yield
    ecu.wait_alive(3.0)
    ecu.enter_programming()
    ecu.unlock(key_algo)


@pytest.fixture
def raw(interface: str, long_read: tuple[bytes, bytes]) -> RawCan:
    """Frame level access, no kernel ISO-TP socket is open while this is in use"""
    # depends on long_read so its kernel socket has come and gone before any raw test starts
    can = RawCan(interface, TXID, RXID)
    try:
        # an abandoned transfer from the previous test needs N_Cr/N_Bs to time out
        assert can.wait_alive(3.0), "ECU is not answering before the test"
        yield can
    finally:
        can.close()
