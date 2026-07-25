#!/usr/bin/env python3
"""Serial smoke test for the shared PC/MCU Protocol on NUCLEO-F446RE."""

from __future__ import annotations

import argparse
import dataclasses
import struct
import sys
import time
from collections.abc import Iterable

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
DEFAULT_INTERVAL_US = 5000

NORMATIVE_VECTORS = (
    (PING, 1, b"", bytes.fromhex("A55A0101010000005597")),
    (ACK, 1, bytes.fromhex("010000"), bytes.fromhex("A55A018001000300010000536F")),
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
    version: int
    message_id: int
    sequence: int
    payload: bytes


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
    """Encode one authoritative Protocol frame."""
    if not 0 <= message_id <= 0xFF:
        raise ValueError("message_id must fit in one byte")
    if not 0 <= sequence <= 0xFFFF:
        raise ValueError("sequence must fit in two bytes")
    if len(payload) > MAX_PAYLOAD_LENGTH:
        raise ValueError("payload is too long")

    body = struct.pack("<BBHH", PROTOCOL_VERSION, message_id, sequence, len(payload)) + payload
    return SOF + body + struct.pack("<H", crc16_ccitt_false(body))


class FrameParser:
    """Incremental parser tolerant of partial reads, combined reads, and noise."""

    def __init__(self) -> None:
        self._buffer = bytearray()
        self.crc_error_count = 0
        self.format_error_count = 0

    def feed(self, data: bytes) -> list[Frame]:
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


def run_self_test() -> None:
    """Verify all normative vectors and controlled CRC corruption."""
    assert crc16_ccitt_false(b"123456789") == 0x29B1

    parser = FrameParser()
    for message_id, sequence, payload, expected in NORMATIVE_VECTORS:
        encoded = encode_frame(message_id, sequence, payload)
        assert encoded == expected

        frames: list[Frame] = []
        split_a = min(1, len(expected))
        split_b = min(5, len(expected))
        for chunk in (expected[:split_a], expected[split_a:split_b], expected[split_b:]):
            frames.extend(parser.feed(chunk))
        assert frames == [Frame(PROTOCOL_VERSION, message_id, sequence, payload)]

    corrupted = bytearray(NORMATIVE_VECTORS[-1][3])
    corrupted[8] ^= 0x01
    assert parser.feed(bytes(corrupted)) == []
    assert parser.crc_error_count == 1
    print("serial smoke-test shared Protocol self-test: PASS")


def describe_frame(frame: Frame) -> str:
    """Return a readable description for a decoded frame."""
    if frame.message_id in (ACK, NACK) and len(frame.payload) == 3:
        request_id, result, state = struct.unpack("<BBB", frame.payload)
        name = "ACK" if frame.message_id == ACK else "NACK"
        return (
            f"{name} seq={frame.sequence} request=0x{request_id:02X} "
            f"result={result} state={state}"
        )

    if frame.message_id == DEVICE_INFO and len(frame.payload) >= 8:
        device_type, fw_major, fw_minor, fw_patch, max_rate, name_length = struct.unpack_from(
            "<HBBBHB", frame.payload, 0
        )
        expected_length = 8 + name_length
        if len(frame.payload) == expected_length:
            device_name = frame.payload[8:].decode("utf-8", errors="replace")
            return (
                f"DEVICE_INFO seq={frame.sequence} type=0x{device_type:04X} "
                f"fw={fw_major}.{fw_minor}.{fw_patch} max_rate={max_rate} Hz "
                f"name={device_name}"
            )

    if frame.message_id == DEVICE_STATUS and len(frame.payload) == 3:
        state, status_flags = struct.unpack("<BH", frame.payload)
        return f"DEVICE_STATUS seq={frame.sequence} state={state} status=0x{status_flags:04X}"

    if frame.message_id == TELEMETRY_SAMPLE and len(frame.payload) == 14:
        sample_counter, device_tick_us, sine_value, status_flags = struct.unpack(
            "<IIfH", frame.payload
        )
        return (
            f"TELEMETRY_SAMPLE seq={frame.sequence} sample={sample_counter} "
            f"tick={device_tick_us} us sine={sine_value:.6f} "
            f"status=0x{status_flags:04X}"
        )

    if frame.message_id == ERROR_REPORT and len(frame.payload) == 6:
        error_code, detail = struct.unpack("<HI", frame.payload)
        return f"ERROR_REPORT seq={frame.sequence} code={error_code} detail={detail}"

    return (
        f"FRAME version={frame.version} id=0x{frame.message_id:02X} "
        f"seq={frame.sequence} payload={frame.payload.hex(' ')}"
    )


def read_until(serial_port: object, parser: FrameParser, deadline: float) -> list[Frame]:
    """Read until at least one frame is available or the deadline expires."""
    frames: list[Frame] = []
    while time.monotonic() < deadline:
        waiting = getattr(serial_port, "in_waiting", 0)
        data = serial_port.read(max(1, waiting))
        if data:
            frames.extend(parser.feed(data))
            if frames:
                break
    return frames


def send_request(
    serial_port: object,
    message_id: int,
    sequence: int,
    payload: bytes = b"",
) -> None:
    """Encode and send one PC-to-MCU command."""
    serial_port.write(encode_frame(message_id, sequence, payload))
    serial_port.flush()


def response_matches(frame: Frame, expected_id: int, request_id: int, sequence: int) -> bool:
    """Return whether a direct response matches the pending command."""
    if frame.message_id != expected_id or frame.sequence != sequence:
        return False
    if expected_id == ACK:
        if len(frame.payload) != 3:
            return False
        return frame.payload[0] == request_id and frame.payload[1] == RESULT_OK
    if expected_id == DEVICE_INFO:
        return len(frame.payload) >= 8
    return True


def wait_for_response(
    serial_port: object,
    parser: FrameParser,
    expected_id: int,
    request_id: int,
    sequence: int,
    timeout_s: float,
) -> Frame:
    """Wait for one matching direct response."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        for frame in read_until(serial_port, parser, deadline):
            print(describe_frame(frame))
            if response_matches(frame, expected_id, request_id, sequence):
                return frame
            if frame.message_id == NACK and frame.sequence == sequence:
                raise RuntimeError(describe_frame(frame))
    raise TimeoutError(
        f"timeout waiting for 0x{expected_id:02X} to request 0x{request_id:02X}"
    )


def run_hardware_test(port: str, telemetry_count: int, timeout_s: float) -> None:
    """Execute command, configuration, streaming, and stop bring-up checks."""
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as exc:
        raise RuntimeError("pyserial is required: python3 -m pip install pyserial") from exc

    parser = FrameParser()
    sequence = 1

    with serial.Serial(port=port, baudrate=115200, timeout=0.05, write_timeout=1.0) as serial_port:
        serial_port.reset_input_buffer()
        serial_port.reset_output_buffer()

        send_request(serial_port, PING, sequence)
        wait_for_response(serial_port, parser, ACK, PING, sequence, timeout_s)
        sequence += 1

        send_request(serial_port, GET_DEVICE_INFO, sequence)
        wait_for_response(serial_port, parser, DEVICE_INFO, GET_DEVICE_INFO, sequence, timeout_s)
        sequence += 1

        config_payload = struct.pack("<H", DEFAULT_INTERVAL_US)
        send_request(serial_port, SET_STREAM_CONFIG, sequence, config_payload)
        wait_for_response(serial_port, parser, ACK, SET_STREAM_CONFIG, sequence, timeout_s)
        sequence += 1

        send_request(serial_port, START_STREAM, sequence)
        wait_for_response(serial_port, parser, ACK, START_STREAM, sequence, timeout_s)
        sequence += 1

        received_telemetry = 0
        previous_sample_counter: int | None = None
        telemetry_deadline = time.monotonic() + max(timeout_s, telemetry_count * 0.02)
        while received_telemetry < telemetry_count and time.monotonic() < telemetry_deadline:
            for frame in read_until(serial_port, parser, telemetry_deadline):
                if frame.message_id != TELEMETRY_SAMPLE or len(frame.payload) != 14:
                    print(describe_frame(frame))
                    continue

                sample_counter = struct.unpack_from("<I", frame.payload, 0)[0]
                if previous_sample_counter is not None:
                    expected_counter = (previous_sample_counter + 1) & 0xFFFFFFFF
                    if sample_counter != expected_counter:
                        print(
                            f"WARNING: sample counter gap {previous_sample_counter} -> {sample_counter}",
                            file=sys.stderr,
                        )
                previous_sample_counter = sample_counter
                print(describe_frame(frame))
                received_telemetry += 1

        send_request(serial_port, STOP_STREAM, sequence)
        wait_for_response(serial_port, parser, ACK, STOP_STREAM, sequence, timeout_s)

        if received_telemetry < telemetry_count:
            raise TimeoutError(f"received {received_telemetry}/{telemetry_count} telemetry frames")

    print(
        "hardware smoke test: PASS "
        f"({received_telemetry} telemetry frames, parser CRC errors={parser.crc_error_count})"
    )


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", nargs="?", help="serial port, for example COM5 or /dev/ttyACM0")
    parser.add_argument("--telemetry-count", type=int, default=20)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def main() -> int:
    """Run the self-test or hardware smoke test."""
    args = parse_args()

    if args.self_test:
        run_self_test()
        return 0

    if not args.port:
        print("error: serial port is required unless --self-test is used", file=sys.stderr)
        return 2
    if args.telemetry_count <= 0:
        print("error: --telemetry-count must be positive", file=sys.stderr)
        return 2
    if args.timeout <= 0:
        print("error: --timeout must be positive", file=sys.stderr)
        return 2

    try:
        run_hardware_test(args.port, args.telemetry_count, args.timeout)
    except (OSError, RuntimeError, TimeoutError, ValueError) as exc:
        print(f"hardware smoke test: FAIL: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
