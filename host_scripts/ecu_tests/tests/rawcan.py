"""Raw SocketCAN access plus a hand-rolled ISO-TP tester, so tests control every frame on the bus

Do not keep a kernel ISO-TP socket (ecu.Ecu) open on the same IDs while using this, the kernel
would answer the ECU's first frames with its own flow control
"""
import socket
import struct
import time
from dataclasses import dataclass

CAN_EFF_FLAG = 0x80000000
CAN_RTR_FLAG = 0x40000000
CAN_EFF_MASK = 0x1FFFFFFF
CAN_FRAME_FMT = "=IB3x8s"
CAN_FRAME_SIZE = struct.calcsize(CAN_FRAME_FMT)

# not exposed by every Python build, value is the same on all 64 bit Linux ports
SO_TIMESTAMPNS = getattr(socket, "SO_TIMESTAMPNS", 35)
TIMESPEC_FMT = "@ll"

PCI_SF = 0x0
PCI_FF = 0x1
PCI_CF = 0x2
PCI_FC = 0x3

FS_CTS = 0
FS_WAIT = 1
FS_OVERFLOW = 2

# ISO 15765-2 network layer timeouts
N_BS_S = 1.0 # sender waiting for a flow control
N_CR_S = 1.0 # receiver waiting for a consecutive frame

NRC_RESPONSE_PENDING = 0x78
P2_STAR_S = 20.0


@dataclass
class Frame:
    can_id: int
    data: bytes
    timestamp: float # seconds, from the kernel when available

    @property
    def pci(self) -> int:
        return self.data[0] >> 4 if self.data else -1

    def __str__(self) -> str:
        return f"{self.can_id:08X} [{len(self.data)}] {self.data.hex(' ')}"


@dataclass
class FlowControl:
    status: int
    block_size: int
    stmin_raw: int
    frame: Frame
    delay: float # seconds between sending the first frame and receiving this

    @property
    def stmin_s(self) -> float:
        return stmin_to_seconds(self.stmin_raw)


@dataclass
class Message:
    payload: bytes
    frames: list[Frame]


def stmin_to_seconds(raw: int) -> float:
    if raw <= 0x7F:
        return raw / 1000
    if 0xF1 <= raw <= 0xF9:
        return (raw - 0xF0) / 10000
    # reserved values are treated as the longest STmin
    return 0.127


def stmin_is_valid(raw: int) -> bool:
    return raw <= 0x7F or 0xF1 <= raw <= 0xF9


def single_frame(payload: bytes) -> bytes:
    assert 1 <= len(payload) <= 7
    return bytes([len(payload)]) + payload


def first_frame(payload: bytes) -> bytes:
    assert 8 <= len(payload) <= 4095
    return bytes([0x10 | (len(payload) >> 8), len(payload) & 0xFF]) + payload[:6]


def consecutive_frames(payload: bytes, first_sn: int = 1) -> list[bytes]:
    """The consecutive frames that follow first_frame(payload)"""
    frames = []
    sn = first_sn
    for offset in range(6, len(payload), 7):
        frames.append(bytes([0x20 | (sn & 0xF)]) + payload[offset:offset + 7])
        sn += 1
    return frames


def flow_control(status: int = FS_CTS, block_size: int = 0, stmin: int = 0) -> bytes:
    return bytes([0x30 | status, block_size, stmin])


def pad(frame: bytes, value: int | None) -> bytes:
    return frame if value is None else frame.ljust(8, bytes([value]))


class RawCan:
    def __init__(self, interface: str, tx_id: int, rx_id: int):
        self.tx_id = tx_id
        self.rx_id = rx_id
        self.sock = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        # only the ECU's response ID, exact match on the ID and the extended flag
        can_filter = struct.pack("=II", rx_id | CAN_EFF_FLAG, CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_EFF_MASK)
        self.sock.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FILTER, can_filter)
        try:
            self.sock.setsockopt(socket.SOL_SOCKET, SO_TIMESTAMPNS, 1)
        except OSError:
            pass
        self.sock.bind((interface,))

    def close(self) -> None:
        self.sock.close()

    # Frames ======================================================================================
    def send(self, data: bytes, can_id: int | None = None) -> None:
        """can_id is a raw SocketCAN ID, so it needs CAN_EFF_FLAG for a 29 bit ID"""
        if can_id is None:
            can_id = self.tx_id | CAN_EFF_FLAG
        self.sock.send(struct.pack(CAN_FRAME_FMT, can_id, len(data), data.ljust(8, b"\x00")))

    def recv(self, timeout: float) -> Frame | None:
        self.sock.settimeout(max(timeout, 0))
        try:
            raw, ancdata, _, _ = self.sock.recvmsg(CAN_FRAME_SIZE, 128)
        except (TimeoutError, BlockingIOError):
            return None
        timestamp = time.time()
        for level, kind, cmsg in ancdata:
            if level == socket.SOL_SOCKET and kind == SO_TIMESTAMPNS and len(cmsg) >= struct.calcsize(TIMESPEC_FMT):
                seconds, nanoseconds = struct.unpack_from(TIMESPEC_FMT, cmsg)
                timestamp = seconds + nanoseconds / 1e9
        can_id, dlc, data = struct.unpack(CAN_FRAME_FMT, raw)
        return Frame(can_id & CAN_EFF_MASK, data[:dlc], timestamp)

    def recv_all(self, duration: float) -> list[Frame]:
        """Everything the ECU sends in the next `duration` seconds"""
        frames = []
        deadline = time.monotonic() + duration
        while (remaining := deadline - time.monotonic()) > 0:
            frame = self.recv(remaining)
            if frame is not None:
                frames.append(frame)
        return frames

    def flush(self) -> None:
        while self.recv(0) is not None:
            pass

    def expect_silence(self, duration: float = 0.4) -> None:
        frames = self.recv_all(duration)
        assert not frames, "ECU should have stayed silent but sent: " + ", ".join(str(f) for f in frames)

    # ISO-TP tester ===============================================================================
    def expect_flow_control(self, sent_at: float, timeout: float = N_BS_S) -> FlowControl:
        for _ in range(20):
            frame = self.recv(timeout)
            assert frame is not None, f"no flow control within {timeout} s"
            assert frame.pci == PCI_FC, f"expected a flow control, got {frame}"
            assert len(frame.data) >= 3, f"flow control is shorter than 3 bytes: {frame}"
            fc = FlowControl(frame.data[0] & 0xF, frame.data[1], frame.data[2], frame, time.monotonic() - sent_at)
            if fc.status != FS_WAIT:
                return fc
        raise AssertionError("ECU kept sending flow control WAIT")

    def send_request(self, payload: bytes, pad_byte: int | None = None, allow_overflow: bool = False) -> FlowControl | None:
        """Sends a full message, honouring the ECU's block size and STmin

        Returns the first flow control, None for a single frame. With allow_overflow a flow
        control OVERFLOW is returned instead of failing, and nothing more is sent.
        """
        if len(payload) <= 7:
            self.send(pad(single_frame(payload), pad_byte))
            return None

        sent_at = time.monotonic()
        self.send(first_frame(payload))
        first_fc = fc = self.expect_flow_control(sent_at)
        if fc.status == FS_OVERFLOW and allow_overflow:
            return fc
        assert fc.status == FS_CTS, f"expected flow control CTS, got flow status {fc.status}: {fc.frame}"

        frames = consecutive_frames(payload)
        sent_in_block = 0
        for index, frame in enumerate(frames):
            self.send(pad(frame, pad_byte))
            sent_in_block += 1
            if index == len(frames) - 1:
                break
            if fc.block_size and sent_in_block == fc.block_size:
                fc = self.expect_flow_control(time.monotonic())
                assert fc.status == FS_CTS, f"expected flow control CTS, got flow status {fc.status}: {fc.frame}"
                sent_in_block = 0
            else:
                time.sleep(fc.stmin_s)
        return first_fc

    def recv_response(self, timeout: float = 2.0, block_size: int = 0, stmin: int = 0, fc_pad: int | None = None) -> Message:
        """Receives one full message, checking the ECU's framing along the way"""
        frame = self.recv(timeout)
        assert frame is not None, f"no response within {timeout} s"
        frames = [frame]

        if frame.pci == PCI_SF:
            length = frame.data[0] & 0xF
            assert 1 <= length <= 7, f"bad single frame length: {frame}"
            assert len(frame.data) >= length + 1, f"single frame is shorter than its length field: {frame}"
            return Message(frame.data[1:1 + length], frames)

        assert frame.pci == PCI_FF, f"expected a single or first frame, got {frame}"
        assert len(frame.data) == 8, f"first frame must fill the CAN frame: {frame}"
        total = ((frame.data[0] & 0xF) << 8) | frame.data[1]
        assert total >= 8, f"first frame used for a message that fits a single frame: {frame}"

        payload = frame.data[2:]
        fc = pad(flow_control(FS_CTS, block_size, stmin), fc_pad)
        self.send(fc)
        sn = 1
        received_in_block = 0
        while len(payload) < total:
            frame = self.recv(N_CR_S + stmin_to_seconds(stmin))
            assert frame is not None, f"consecutive frame {sn} never arrived, have {len(payload)}/{total} bytes"
            frames.append(frame)
            assert frame.pci == PCI_CF, f"expected a consecutive frame, got {frame}"
            assert frame.data[0] & 0xF == sn & 0xF, f"expected sequence number {sn & 0xF}, got {frame}"
            needed = min(7, total - len(payload))
            assert len(frame.data) >= 1 + needed, f"consecutive frame is missing data: {frame}"
            payload += frame.data[1:1 + needed]
            sn += 1
            received_in_block += 1
            if block_size and received_in_block == block_size and len(payload) < total:
                self.send(fc)
                received_in_block = 0
        return Message(payload, frames)

    def request(self, payload: bytes, timeout: float = 2.0, pad_byte: int | None = None) -> Message:
        """Full request/response exchange, waits out response pending NRCs"""
        self.send_request(payload, pad_byte)
        message = self.recv_response(timeout)
        while len(message.payload) == 3 and message.payload[0] == 0x7F and message.payload[2] == NRC_RESPONSE_PENDING:
            message = self.recv_response(P2_STAR_S)
        return message

    def ping(self, timeout: float = 1.0) -> bool:
        """Tester present as a single frame, the cheapest proof the ECU is still listening"""
        self.flush()
        try:
            self.send(single_frame(b"\x3E\x00"))
        except OSError:
            return False
        frame = self.recv(timeout)
        return frame is not None and frame.data[:3] == b"\x02\x7E\x00"

    def wait_alive(self, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while True:
            if self.ping(0.3):
                return True
            if time.monotonic() >= deadline:
                return False
