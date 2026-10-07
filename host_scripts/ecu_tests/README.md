# ecu-tests

Black-box tests for the bootloader's ISO-TP and UDS implementation. They run on the Raspberry Pi
against the real ECU over SocketCAN, and only know what `ota_client`'s `cli.py` says the server
implements plus what ISO 15765-2 and ISO 14229 require.

## Running

Bring the CAN interface up the same way as for `ota_client`, then:

```bash
uv run pytest                      # everything that does not write flash or trigger the lockout
uv run pytest --flash              # also the download tests, these erase and write slot B
uv run pytest --lockout            # also the 60 s security lockout tests
uv run pytest -m "not slow"        # skip resets, S3 timeouts and full image transfers
uv run pytest tests/test_isotp.py  # one layer only
```

Options:

| Option | Default | |
| --- | --- | --- |
| `--interface` | `can0` | SocketCAN interface |
| `--key-file` | none | Auth key file, passed to `ota_client`'s `get_auth_key` like the cli does |
| `--boot-timeout` | `15` | Seconds to wait for the ECU to answer again after a reset |
| `--flash` | off | Run the tests marked `flash` |
| `--lockout` | off | Run the tests marked `lockout` |

Nothing else may be talking to the ECU while the tests run (close the `ota-client` cli, its
keepalive would keep sessions alive and answer the ECU's first frames).

## Layout

| File | Covers |
| --- | --- |
| `tests/test_isotp.py` | Single frames, segmented reception and transmission, flow control, timeouts, driven frame by frame over raw CAN |
| `tests/test_uds_general.py` | Unsupported services, tester present (0x3E), test service (0x80), P2 timing |
| `tests/test_uds_session.py` | Session control (0x10), S3 timeout, relocking on session change |
| `tests/test_uds_reset.py` | ECU reset (0x11) |
| `tests/test_uds_read_did.py` | Read data by identifier (0x22) |
| `tests/test_uds_security.py` | Security access (0x27), bad keys, lockout |
| `tests/test_uds_download.py` | Request download, transfer data, transfer exit (0x34, 0x36, 0x37) |
| `tests/rawcan.py` | Raw CAN socket and a hand-rolled ISO-TP tester |
| `tests/ecu.py` | Raw UDS requests over a kernel ISO-TP socket |

CAN IDs, sizes, timeouts and the key algorithm are imported from `ota_client`, so they stay in
step with the client.

## Reading failures

Where the standards leave the server a choice (which NRC wins when a request is wrong in two
ways, whether a repeated block is acknowledged), the tests accept every allowed answer. Where the
server's behaviour could not be known from `cli.py`, they assert what ISO 14229 / ISO 15765-2
specify, so a failure there is either a bug or a deliberate deviation worth writing down.
