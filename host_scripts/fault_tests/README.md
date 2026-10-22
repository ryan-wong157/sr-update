# fault-tests

Tooling for the fault tolerance scenarios. Runs on the Raspberry Pi over SocketCAN, apart from
`fault-images` which runs anywhere.

| Command | Does |
| --- | --- |
| `fault-images IMG` | Writes every bad image scenario from one good fmt_tool image, plus `manifest.csv` with the expected outcome of each |
| `fault-flash IMG` | One full update (enter bootloader, 0x27, 0x34/36/37, 0x11, handshake) and prints how it ended |
| `fault-busload` | Floods the bus with random IDs while an update runs in another terminal |

`src/fault_tests/ecu.py` is the baseline for anything new: `connect()` plus one helper per step
(`unlock`, `request_seed`, `send_key`, `request_download`, `transfer_data`, `transfer_exit`,
`reset`, `wait_result`, ...).

Keys are the same as everywhere else: the auth key from `PRIVATE_AUTH_KEY` (or `--key-file`), the
signing key from `PRIVATE_SIGNING_KEY` (or `--key-path`).

## Bad images

```bash
uv run fault-images ../app-img.bin          # -> ../app-img-faults/NN_<scenario>.bin + manifest.csv
uv run fault-flash ../app-img-faults/03_bad_header_magic.bin
```

Install the input image first, the version scenarios are relative to it. Every bad image that could
get past the version check is versioned 255.255.255, so it is never rejected as older and a revert
can be told apart from a confirm (the old app's handshake carries the old version).

## Power loss

The bootloader on this branch stops at every trailer write and swap step with the LED on for
1.5 s, then off for 0.3 s (`CFG_FAULT_PAUSE_*` in `boot_core/Inc/fault_pause.h`). Pull the plug
while it is lit. Count the lit pauses to know where you are. N is the swap size in pages, the
bigger of the two images.

Test swap, straight after the 0x11 reset:

| Pause | State in flash when you pull |
| --- | --- |
| 1 | slot A trailer erased |
| 2 | swap type TEST written |
| 3 | swap size written |
| 4 | slot A trailer magic written (swap now committed) |
| 5 | slot B trailer erased |
| 6 + 4i | page i: slot A page copied to slot B, not recorded yet |
| 7 + 4i | page i: first half recorded |
| 8 + 4i | page i: slot B page copied into slot A, not recorded yet |
| 9 + 4i | page i: recorded done |

After power comes back the swap resumes straight from the last recorded step, with no setup
pauses and two pauses fewer for a half done page, so pull again to test a resume of a resume.

Revert (test boot left unconfirmed with `fault-flash --no-confirm`, then the 20 s IWDG): 3 pauses
(slot B trailer erased, slot B copy done, slot B trailer magic), then the same 5 setup pauses with
swap type REVERT, then 4 per page counting down from the last page.

After the swap, before the confirm: `fault-flash IMG --no-confirm` and pull the plug while the
new app is waiting for its confirm.

## Interrupted transfer

```bash
uv run fault-flash IMG --pause-at-block 10    # pull the CAN cable, plug back in, press Enter
uv run fault-flash IMG --die-at-block 10      # client exits mid-transfer
uv run fault-flash IMG                        # then a normal update to show it recovered
```

## Bus load

```bash
uv run fault-busload --load 50                # terminal 1, Ctrl-C to stop
uv run fault-flash IMG                        # terminal 2
```

`--priority high` (default) uses IDs below the OTA ones so they win arbitration, `low` uses IDs
above them. `--load 100` sends as fast as the interface takes frames.
