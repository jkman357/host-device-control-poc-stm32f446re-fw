#!/usr/bin/env python3
"""Exercise every PC-to-MCU command and display live sine-wave telemetry.

The command sequence is:

    PING -> GET_DEVICE_INFO -> SET_STREAM_CONFIG -> START_STREAM

Telemetry is then received continuously. Press q or Q to send STOP_STREAM,
validate the ACK, print a summary, and close the serial port cleanly.
"""

from __future__ import annotations

import argparse
import dataclasses
import os
import struct
import sys
import time
from collections.abc import Iterable, Iterator
from typing import Protocol

SOF = b"\xA5\x5A"
PROTOCOL_VERSION = 0x01
MAX_PAYLOAD_LENGTH = 1024
FRAME_OVERHEAD_LENGTH = 10

PING = 0x01
GET_DEVICE_INFO = 0x02
SET_STREAM_CONFIG = 0x03
START_STREAM = 0x04
STOP_STREAM = 0x05

ACK = 0x80
NACK = 0x81
DEVICE_INFO = 0x82
DEVICE_STATUS = 0x83
TELEMETRY_SAMPLE = 0x90
ERROR_REPORT = 0x91

RESULT_OK = 0x00
STATE_IDLE = 0x00
STATE_STREAMING = 0x01

DEFAULT_BAUD_RATE = 115200
DEFAULT_INTERVAL_US = 5000
MIN_INTERVAL_US = 1000
MAX_INTERVAL_US = 60000
DEFAULT_TIMEOUT_S = 2.0
DEFAULT_DISPLAY_EVERY = 10

MESSAGE_NAMES = {
    PING: "PING",
    GET_DEVICE_INFO: "GET_DEVICE_INFO",
    SET_STREAM_CONFIG: "SET_STREAM_CONFIG",
    START_STREAM: "START_STREAM",
    STOP_STREAM: "STOP_STREAM",
    ACK: "ACK",
    NACK: "NACK",
    DEVICE_INFO: "DEVICE_INFO",
    DEVICE_STATUS: "DEVICE_STATUS",
    TELEMETRY_SAMPLE: "TELEMETRY_SAMPLE",
    ERROR_REPORT: "ERROR_REPORT",
}

RESULT_NAMES = {
    0x00: "OK",
    0x01: "INVALID_COMMAND",
    0x02: "INVALID_LENGTH",
    0x03: "INVALID_VALUE",
    0x04: "INVALID_STATE",
    0x05: "UNSUPPORTED_VERSION",
    0x06: "INTERNAL_ERROR",
}

NORMATIVE_VECTORS = (
    (PING, 1, b"", bytes.fromhex("A55A0101010000005597")),
    (
        SET_STREAM_CONFIG,
        0x1234,
        bytes.fromhex("8813"),
        bytes.fromhex("A55A0103341202008813909A"),
    ),
    (START_STREAM, 2, b"", bytes.fromhex("A55A010402000000DE2F")),
    (
        TELEMETRY_SAMPLE,
        1,
        bytes.fromhex("010000008813000091A8003D0000"),
        bytes.fromhex("A55A019001000E00010000008813000091A8003D00008DCF"),
    ),
)


@dataclasses.dataclass(frozen=True)
class Frame:
    """Decoded Protocol frame."""

    version: int
    message_id: int
    sequence: int
    payload: bytes


@dataclasses.dataclass(frozen=True)
class AckPayload:
    """Decoded ACK or NACK payload."""

    request_message_id: int
    result_code: int
    device_state: int


@dataclasses.dataclass(frozen=True)
class DeviceInfoPayload:
    """Decoded DEVICE_INFO payload."""

    device_type: int
    firmware_major: int
    firmware_minor: int
    firmware_patch: int
    maximum_stream_rate_hz: int
    device_name: str


@dataclasses.dataclass(frozen=True)
class TelemetryPayload:
    """Decoded TELEMETRY_SAMPLE payload."""

    sample_counter: int
    device_tick_us: int
    sine_value: float
    status_flags: int


@dataclasses.dataclass
class StreamStatistics:
    """Host-observed streaming statistics."""

    telemetry_frames: int = 0
    sample_gaps: int = 0
    first_sample_counter: int | None = None
    last_sample_counter: int | None = None
    minimum_sine: float = 1.0
    maximum_sine: float = -1.0
    nonzero_status_frames: int = 0


class SerialLike(Protocol):
    """Minimal serial interface used by the command sweep."""

    @property
    def in_waiting(self) -> int: ...

    def read(self, size: int = 1) -> bytes: ...

    def write(self, data: bytes) -> int: ...

    def flush(self) -> None: ...


class FrameParser:
    """Incremental parser for partial reads, combined reads, and line noise."""

    def __init__(self) -> None:
        self._buffer = bytearray()
        self.crc_error_count = 0
        self.format_error_count = 0

    def feed(self, data: bytes) -> list[Frame]:
        """Consume bytes and return every complete CRC-valid frame."""
        self._buffer.extend(data)
        frames: list[Frame] = []

        while True:
            sof_index = self._buffer.find(SOF)
            if sof_index < 0:
                if self._buffer[-1:] == SOF[:1]:
                    del self._buffer[:-1]
                else:
                    self._buffer.clear()
                break

            if sof_index > 0:
                del self._buffer[:sof_index]

            if len(self._buffer) < 8:
                break

            payload_length = struct.unpack_from("<H", self._buffer, 6)[0]
            if payload_length > MAX_PAYLOAD_LENGTH:
                self.format_error_count += 1
                del self._buffer[0]
                continue

            frame_length = FRAME_OVERHEAD_LENGTH + payload_length
            if len(self._buffer) < frame_length:
                break

            candidate = bytes(self._buffer[:frame_length])
            body_end = 8 + payload_length
            expected_crc = struct.unpack_from("<H", candidate, body_end)[0]
            actual_crc = crc16_ccitt_false(candidate[2:body_end])
            if expected_crc != actual_crc:
                self.crc_error_count += 1
                del self._buffer[0]
                continue

            version, message_id, sequence, _ = struct.unpack_from("<BBHH", candidate, 2)
            payload = candidate[8:body_end]
            frames.append(Frame(version, message_id, sequence, payload))
            del self._buffer[:frame_length]

        return frames


class KeyPoller:
    """Nonblocking q/Q key detector for Windows and POSIX terminals."""

    def __init__(self) -> None:
        self._is_windows = os.name == "nt"
        self._msvcrt = None
        self._termios = None
        self._old_terminal_settings = None
        self._stdin_fd: int | None = None

    def __enter__(self) -> "KeyPoller":
        if self._is_windows:
            import msvcrt

            self._msvcrt = msvcrt
            return self

        if sys.stdin.isatty():
            import termios
            import tty

            self._termios = termios
            self._stdin_fd = sys.stdin.fileno()
            self._old_terminal_settings = termios.tcgetattr(self._stdin_fd)
            tty.setcbreak(self._stdin_fd)
        return self

    def __exit__(self, exc_type: object, exc_value: object, traceback: object) -> None:
        if (
            self._termios is not None
            and self._stdin_fd is not None
            and self._old_terminal_settings is not None
        ):
            self._termios.tcsetattr(
                self._stdin_fd,
                self._termios.TCSADRAIN,
                self._old_terminal_settings,
            )

    def quit_requested(self) -> bool:
        """Return true after q or Q is pressed without requiring Enter."""
        if self._is_windows:
            assert self._msvcrt is not None
            while self._msvcrt.kbhit():
                key = self._msvcrt.getwch()
                if key in ("q", "Q"):
                    return True
            return False

        if not sys.stdin.isatty():
            return False

        import select

        readable, _, _ = select.select([sys.stdin], [], [], 0.0)
        if readable:
            key = sys.stdin.read(1)
            return key in ("q", "Q")
        return False


def crc16_ccitt_false(data: Iterable[int]) -> int:
    """Return CRC-16/CCITT-FALSE for the supplied bytes."""
    crc = 0xFFFF
    for value in data:
        crc ^= (value & 0xFF) << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def encode_frame(message_id: int, sequence: int, payload: bytes = b"") -> bytes:
    """Encode one Protocol frame."""
    if not 0 <= message_id <= 0xFF:
        raise ValueError("message_id must fit in one byte")
    if not 1 <= sequence <= 0xFFFF:
        raise ValueError("host command sequence must be 1 through 65535")
    if len(payload) > MAX_PAYLOAD_LENGTH:
        raise ValueError("payload is too long")

    body = struct.pack("<BBHH", PROTOCOL_VERSION, message_id, sequence, len(payload)) + payload
    return SOF + body + struct.pack("<H", crc16_ccitt_false(body))


def decode_ack(frame: Frame) -> AckPayload:
    """Decode and validate an ACK or NACK payload."""
    if frame.message_id not in (ACK, NACK):
        raise ValueError("frame is not ACK or NACK")
    if len(frame.payload) != 3:
        raise ValueError(f"ACK/NACK payload length is {len(frame.payload)}, expected 3")
    request_message_id, result_code, device_state = struct.unpack("<BBB", frame.payload)
    return AckPayload(request_message_id, result_code, device_state)


def decode_device_info(frame: Frame) -> DeviceInfoPayload:
    """Decode and validate a DEVICE_INFO payload."""
    if frame.message_id != DEVICE_INFO:
        raise ValueError("frame is not DEVICE_INFO")
    if len(frame.payload) < 8:
        raise ValueError("DEVICE_INFO payload is shorter than its fixed fields")

    device_type, fw_major, fw_minor, fw_patch, maximum_rate, name_length = struct.unpack_from(
        "<HBBBHB", frame.payload, 0
    )
    expected_length = 8 + name_length
    if len(frame.payload) != expected_length:
        raise ValueError(
            f"DEVICE_INFO payload length is {len(frame.payload)}, expected {expected_length}"
        )
    device_name = frame.payload[8:].decode("utf-8", errors="strict")
    return DeviceInfoPayload(
        device_type,
        fw_major,
        fw_minor,
        fw_patch,
        maximum_rate,
        device_name,
    )


def decode_telemetry(frame: Frame) -> TelemetryPayload:
    """Decode and validate a TELEMETRY_SAMPLE payload."""
    if frame.message_id != TELEMETRY_SAMPLE:
        raise ValueError("frame is not TELEMETRY_SAMPLE")
    if len(frame.payload) != 14:
        raise ValueError(f"telemetry payload length is {len(frame.payload)}, expected 14")

    sample_counter, device_tick_us, sine_value, status_flags = struct.unpack(
        "<IIfH", frame.payload
    )
    if not -1.0001 <= sine_value <= 1.0001:
        raise ValueError(f"sine value is outside the expected range: {sine_value}")
    return TelemetryPayload(sample_counter, device_tick_us, sine_value, status_flags)


def next_sequence(sequence: int) -> int:
    """Advance a host command sequence and wrap 65535 to 1."""
    return 1 if sequence >= 0xFFFF else sequence + 1


def read_frames(serial_port: SerialLike, parser: FrameParser, timeout_s: float) -> list[Frame]:
    """Read available serial bytes for up to timeout_s."""
    deadline = time.monotonic() + timeout_s
    frames: list[Frame] = []
    while time.monotonic() < deadline:
        waiting = serial_port.in_waiting
        data = serial_port.read(max(1, waiting))
        if data:
            frames.extend(parser.feed(data))
            if frames:
                return frames
    return frames


def send_command(
    serial_port: SerialLike,
    message_id: int,
    sequence: int,
    payload: bytes = b"",
) -> None:
    """Encode, print, and send one PC-to-MCU command."""
    encoded = encode_frame(message_id, sequence, payload)
    name = MESSAGE_NAMES.get(message_id, f"0x{message_id:02X}")
    print(f"TX {name:<17} seq={sequence:<5} {encoded.hex(' ').upper()}")
    written = serial_port.write(encoded)
    if written != len(encoded):
        raise OSError(f"short serial write: {written}/{len(encoded)} bytes")
    serial_port.flush()


def frame_description(frame: Frame) -> str:
    """Return a concise readable frame description."""
    name = MESSAGE_NAMES.get(frame.message_id, f"0x{frame.message_id:02X}")
    if frame.message_id in (ACK, NACK):
        ack = decode_ack(frame)
        result_name = RESULT_NAMES.get(ack.result_code, str(ack.result_code))
        return (
            f"{name} seq={frame.sequence} request="
            f"{MESSAGE_NAMES.get(ack.request_message_id, f'0x{ack.request_message_id:02X}')} "
            f"result={result_name} state={ack.device_state}"
        )
    if frame.message_id == DEVICE_INFO:
        info = decode_device_info(frame)
        return (
            f"DEVICE_INFO seq={frame.sequence} type=0x{info.device_type:04X} "
            f"fw={info.firmware_major}.{info.firmware_minor}.{info.firmware_patch} "
            f"max_rate={info.maximum_stream_rate_hz}Hz name={info.device_name}"
        )
    if frame.message_id == DEVICE_STATUS and len(frame.payload) == 3:
        state, status_flags = struct.unpack("<BH", frame.payload)
        return f"DEVICE_STATUS seq={frame.sequence} state={state} flags=0x{status_flags:04X}"
    if frame.message_id == ERROR_REPORT and len(frame.payload) == 6:
        error_code, detail = struct.unpack("<HI", frame.payload)
        return f"ERROR_REPORT seq={frame.sequence} code={error_code} detail={detail}"
    return (
        f"{name} version={frame.version} seq={frame.sequence} "
        f"payload={frame.payload.hex(' ').upper()}"
    )


def wait_for_direct_response(
    serial_port: SerialLike,
    parser: FrameParser,
    expected_message_id: int,
    request_message_id: int,
    sequence: int,
    timeout_s: float,
    statistics: StreamStatistics | None = None,
) -> Frame:
    """Wait for the response matching a command while tolerating telemetry."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        remaining = max(0.001, deadline - time.monotonic())
        for frame in read_frames(serial_port, parser, min(0.05, remaining)):
            if frame.version != PROTOCOL_VERSION:
                raise RuntimeError(f"unexpected response wire version: {frame.version}")

            if frame.message_id == TELEMETRY_SAMPLE:
                telemetry = decode_telemetry(frame)
                if statistics is not None:
                    update_stream_statistics(statistics, telemetry)
                continue

            print(f"RX {frame_description(frame)}")
            if frame.sequence != sequence:
                continue

            if frame.message_id == NACK:
                nack = decode_ack(frame)
                if nack.request_message_id == request_message_id:
                    raise RuntimeError(frame_description(frame))
                continue

            if frame.message_id != expected_message_id:
                continue

            if expected_message_id == ACK:
                ack = decode_ack(frame)
                if ack.request_message_id != request_message_id:
                    continue
                if ack.result_code != RESULT_OK:
                    raise RuntimeError(frame_description(frame))
            return frame

    expected_name = MESSAGE_NAMES.get(expected_message_id, f"0x{expected_message_id:02X}")
    request_name = MESSAGE_NAMES.get(request_message_id, f"0x{request_message_id:02X}")
    raise TimeoutError(f"timeout waiting for {expected_name} to {request_name} seq={sequence}")


def update_stream_statistics(
    statistics: StreamStatistics,
    telemetry: TelemetryPayload,
) -> None:
    """Update continuity, range, and status statistics."""
    if statistics.first_sample_counter is None:
        statistics.first_sample_counter = telemetry.sample_counter

    if statistics.last_sample_counter is not None:
        expected = (statistics.last_sample_counter + 1) & 0xFFFFFFFF
        if telemetry.sample_counter != expected:
            statistics.sample_gaps += (telemetry.sample_counter - expected) & 0xFFFFFFFF

    statistics.last_sample_counter = telemetry.sample_counter
    statistics.telemetry_frames += 1
    statistics.minimum_sine = min(statistics.minimum_sine, telemetry.sine_value)
    statistics.maximum_sine = max(statistics.maximum_sine, telemetry.sine_value)
    if telemetry.status_flags != 0:
        statistics.nonzero_status_frames += 1


def render_sine_line(telemetry: TelemetryPayload, width: int = 61) -> str:
    """Render one horizontal ASCII oscilloscope sample."""
    if width < 9 or width % 2 == 0:
        raise ValueError("waveform width must be an odd integer of at least 9")

    center = width // 2
    position = round((telemetry.sine_value + 1.0) * (width - 1) / 2.0)
    position = max(0, min(width - 1, position))
    cells = [" "] * width
    cells[center] = "|"
    cells[position] = "*"
    return (
        f"sample={telemetry.sample_counter:10d} "
        f"tick={telemetry.device_tick_us:10d}us "
        f"sine={telemetry.sine_value:+.6f} [{''.join(cells)}] "
        f"flags=0x{telemetry.status_flags:04X}"
    )


def run_self_test() -> None:
    """Run deterministic framing, parser, payload, and rendering tests."""
    if crc16_ccitt_false(b"123456789") != 0x29B1:
        raise AssertionError("CRC check vector failed")

    parser = FrameParser()
    for message_id, sequence, payload, expected in NORMATIVE_VECTORS:
        encoded = encode_frame(message_id, sequence, payload)
        if encoded != expected:
            raise AssertionError(
                f"vector mismatch for 0x{message_id:02X}: "
                f"{encoded.hex().upper()} != {expected.hex().upper()}"
            )
        parsed: list[Frame] = []
        for chunk in (expected[:1], expected[1:5], expected[5:]):
            parsed.extend(parser.feed(chunk))
        if parsed != [Frame(PROTOCOL_VERSION, message_id, sequence, payload)]:
            raise AssertionError(f"parser mismatch for message 0x{message_id:02X}")

    telemetry_frame = Frame(
        PROTOCOL_VERSION,
        TELEMETRY_SAMPLE,
        1,
        bytes.fromhex("010000008813000091A8003D0000"),
    )
    telemetry = decode_telemetry(telemetry_frame)
    if telemetry.sample_counter != 1 or telemetry.device_tick_us != 5000:
        raise AssertionError("telemetry integer decoding failed")
    if abs(telemetry.sine_value - 0.031410757) > 0.000001:
        raise AssertionError("telemetry float decoding failed")
    if "*" not in render_sine_line(telemetry):
        raise AssertionError("waveform rendering failed")

    if next_sequence(0xFFFF) != 1 or next_sequence(1) != 2:
        raise AssertionError("sequence wrap rule failed")

    print("protocol command sweep self-test: PASS")


def run_hardware_sweep(
    port: str,
    interval_us: int,
    timeout_s: float,
    display_every: int,
) -> None:
    """Exercise all commands, stream sine telemetry, and stop on q/Q."""
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as exc:
        raise RuntimeError("pyserial is required: py -m pip install pyserial") from exc

    parser = FrameParser()
    statistics = StreamStatistics()
    sequence = 1
    stream_started = False
    stop_completed = False

    print(f"Opening {port} at {DEFAULT_BAUD_RATE} bps")
    with serial.Serial(
        port=port,
        baudrate=DEFAULT_BAUD_RATE,
        bytesize=8,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.02,
        write_timeout=1.0,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False,
    ) as serial_port:
        serial_port.reset_input_buffer()
        serial_port.reset_output_buffer()
        time.sleep(0.05)

        send_command(serial_port, PING, sequence)
        ping_ack = wait_for_direct_response(
            serial_port, parser, ACK, PING, sequence, timeout_s
        )
        if decode_ack(ping_ack).device_state != STATE_IDLE:
            raise RuntimeError("PING reports that the device is not idle before the sweep")
        print("PASS PING")
        sequence = next_sequence(sequence)

        send_command(serial_port, GET_DEVICE_INFO, sequence)
        info_frame = wait_for_direct_response(
            serial_port, parser, DEVICE_INFO, GET_DEVICE_INFO, sequence, timeout_s
        )
        info = decode_device_info(info_frame)
        print(
            "PASS GET_DEVICE_INFO: "
            f"{info.device_name}, firmware "
            f"{info.firmware_major}.{info.firmware_minor}.{info.firmware_patch}, "
            f"maximum {info.maximum_stream_rate_hz} Hz"
        )
        sequence = next_sequence(sequence)

        config_payload = struct.pack("<H", interval_us)
        send_command(serial_port, SET_STREAM_CONFIG, sequence, config_payload)
        config_ack = wait_for_direct_response(
            serial_port, parser, ACK, SET_STREAM_CONFIG, sequence, timeout_s
        )
        if decode_ack(config_ack).device_state != STATE_IDLE:
            raise RuntimeError("SET_STREAM_CONFIG did not leave the device idle")
        print(f"PASS SET_STREAM_CONFIG: interval={interval_us} us")
        sequence = next_sequence(sequence)

        send_command(serial_port, START_STREAM, sequence)
        start_ack = wait_for_direct_response(
            serial_port, parser, ACK, START_STREAM, sequence, timeout_s
        )
        if decode_ack(start_ack).device_state != STATE_STREAMING:
            raise RuntimeError("START_STREAM ACK did not report STREAMING")
        stream_started = True
        print("PASS START_STREAM")
        sequence = next_sequence(sequence)

        print()
        print("Streaming sine telemetry. Press q or Q to stop.")
        print("The asterisk moves from -1 at the left to +1 at the right.")
        print()

        try:
            with KeyPoller() as key_poller:
                while True:
                    if key_poller.quit_requested():
                        print("q/Q received; stopping stream...")
                        break

                    frames = read_frames(serial_port, parser, 0.05)
                    for frame in frames:
                        if frame.version != PROTOCOL_VERSION:
                            raise RuntimeError(
                                f"unexpected telemetry wire version: {frame.version}"
                            )
                        if frame.message_id == TELEMETRY_SAMPLE:
                            telemetry = decode_telemetry(frame)
                            update_stream_statistics(statistics, telemetry)
                            if statistics.telemetry_frames % display_every == 0:
                                print(render_sine_line(telemetry))
                        elif frame.message_id == ERROR_REPORT:
                            print(f"RX {frame_description(frame)}", file=sys.stderr)
                        else:
                            print(f"RX unexpected during stream: {frame_description(frame)}")
        except KeyboardInterrupt:
            print("Ctrl+C received; stopping stream...")
        finally:
            if stream_started:
                send_command(serial_port, STOP_STREAM, sequence)
                stop_ack = wait_for_direct_response(
                    serial_port,
                    parser,
                    ACK,
                    STOP_STREAM,
                    sequence,
                    max(timeout_s, 1.5),
                    statistics,
                )
                if decode_ack(stop_ack).device_state != STATE_IDLE:
                    raise RuntimeError("STOP_STREAM ACK did not report IDLE")
                stop_completed = True
                print("PASS STOP_STREAM")

    if not stop_completed:
        raise RuntimeError("stream ended without a confirmed STOP_STREAM ACK")
    if statistics.telemetry_frames == 0:
        raise RuntimeError("no TELEMETRY_SAMPLE frame was received")

    print()
    print("Protocol command sweep: PASS")
    print(f"  telemetry frames:       {statistics.telemetry_frames}")
    print(f"  sample gaps:            {statistics.sample_gaps}")
    print(f"  sine minimum/maximum:   {statistics.minimum_sine:+.6f} / {statistics.maximum_sine:+.6f}")
    print(f"  nonzero status frames:  {statistics.nonzero_status_frames}")
    print(f"  parser CRC errors:      {parser.crc_error_count}")
    print(f"  parser format errors:   {parser.format_error_count}")


def parse_args() -> argparse.Namespace:
    """Parse command-line options."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port_positional", nargs="?", help="serial port, such as COM3")
    parser.add_argument("--port", dest="port_option", help="serial port, such as COM3")
    parser.add_argument(
        "--interval-us",
        type=int,
        default=DEFAULT_INTERVAL_US,
        help=f"stream interval in microseconds (default: {DEFAULT_INTERVAL_US})",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT_S,
        help=f"command timeout in seconds (default: {DEFAULT_TIMEOUT_S})",
    )
    parser.add_argument(
        "--display-every",
        type=int,
        default=DEFAULT_DISPLAY_EVERY,
        help=(
            "print one waveform row for every N telemetry frames "
            f"(default: {DEFAULT_DISPLAY_EVERY})"
        ),
    )
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def main() -> int:
    """Run the offline self-test or the interactive hardware sweep."""
    args = parse_args()

    try:
        if args.self_test:
            run_self_test()
            return 0

        port = args.port_option or args.port_positional
        if not port:
            raise ValueError("serial port is required; use --port COM3")
        if not MIN_INTERVAL_US <= args.interval_us <= MAX_INTERVAL_US:
            raise ValueError(
                f"--interval-us must be {MIN_INTERVAL_US} through {MAX_INTERVAL_US}"
            )
        if args.timeout <= 0.0:
            raise ValueError("--timeout must be positive")
        if args.display_every <= 0:
            raise ValueError("--display-every must be positive")

        run_hardware_sweep(
            port=port,
            interval_us=args.interval_us,
            timeout_s=args.timeout,
            display_every=args.display_every,
        )
    except (OSError, RuntimeError, TimeoutError, UnicodeError, ValueError) as exc:
        print(f"Protocol command sweep: FAIL: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
