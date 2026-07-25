#!/usr/bin/env python3
"""Serial smoke test for the STM32F446RE host-device control PoC."""

from __future__ import annotations

import argparse
import dataclasses
import struct
import sys
import time
from collections.abc import Iterable

SOF = b"\xA5\x5A"
PROTOCOL_VERSION = 0x01
FLAG_REQUEST = 0x01

PING_REQUEST = 0x01
GET_DEVICE_INFO_REQUEST = 0x02
START_STREAM_REQUEST = 0x0100
STOP_STREAM_REQUEST = 0x0102

PING_RESPONSE = 0x0081
DEVICE_INFO_RESPONSE = 0x0082
ERROR_RESPONSE = 0x00E0
START_STREAM_RESPONSE = 0x0101
STOP_STREAM_RESPONSE = 0x0103
TELEMETRY = 0x2000

MAX_PAYLOAD_LENGTH = 48
FRAME_OVERHEAD_LENGTH = 11


@dataclasses.dataclass(frozen=True)
class Frame:
    message_id: int
    flags: int
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


def encode_frame(message_id: int, flags: int, sequence: int, payload: bytes = b"") -> bytes:
    """Encode one protocol frame."""
    if not 0 <= message_id <= 0xFFFF:
        raise ValueError("message_id must fit in two bytes")
    if not 0 <= flags <= 0xFF:
        raise ValueError("flags must fit in one byte")
    if not 0 <= sequence <= 0xFFFF:
        raise ValueError("sequence must fit in two bytes")
    if len(payload) > MAX_PAYLOAD_LENGTH:
        raise ValueError("payload is too long")

    body = struct.pack(
        "<BHBBH",
        PROTOCOL_VERSION,
        message_id,
        flags,
        len(payload),
        sequence,
    ) + payload
    return SOF + body + struct.pack("<H", crc16_ccitt_false(body))


class FrameParser:
    """Incremental byte-stream parser tolerant of partial and combined reads."""

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

            if len(self._buffer) < 9:
                break

            version = self._buffer[2]
            payload_length = self._buffer[6]
            if version != PROTOCOL_VERSION or payload_length > MAX_PAYLOAD_LENGTH:
                self.format_error_count += 1
                del self._buffer[0]
                continue

            frame_length = FRAME_OVERHEAD_LENGTH + payload_length
            if len(self._buffer) < frame_length:
                break

            candidate = bytes(self._buffer[:frame_length])
            body_end = 9 + payload_length
            expected_crc = struct.unpack_from("<H", candidate, body_end)[0]
            actual_crc = crc16_ccitt_false(candidate[2:body_end])
            if expected_crc != actual_crc:
                self.crc_error_count += 1
                del self._buffer[0]
                continue

            message_id = struct.unpack_from("<H", candidate, 3)[0]
            flags = candidate[5]
            sequence = struct.unpack_from("<H", candidate, 7)[0]
            payload = candidate[9:body_end]
            frames.append(Frame(message_id, flags, sequence, payload))
            del self._buffer[:frame_length]

        return frames


def run_self_test() -> None:
    assert crc16_ccitt_false(b"123456789") == 0x29B1

    encoded = encode_frame(PING_REQUEST, FLAG_REQUEST, 0x1234)
    parser = FrameParser()
    frames: list[Frame] = []
    for chunk in (encoded[:1], encoded[1:4], encoded[4:9], encoded[9:]):
        frames.extend(parser.feed(chunk))

    assert frames == [Frame(PING_REQUEST, FLAG_REQUEST, 0x1234, b"")]

    combined = encode_frame(GET_DEVICE_INFO_REQUEST, FLAG_REQUEST, 2) + encode_frame(
        START_STREAM_REQUEST, FLAG_REQUEST, 3
    )
    parsed = parser.feed(combined)
    assert [frame.message_id for frame in parsed] == [
        GET_DEVICE_INFO_REQUEST,
        START_STREAM_REQUEST,
    ]
    print("serial smoke-test protocol self-test: PASS")


def describe_frame(frame: Frame) -> str:
    if frame.message_id == PING_RESPONSE and len(frame.payload) == 6:
        uptime_ms, state, version = struct.unpack("<IBB", frame.payload)
        return f"PING_RESPONSE uptime={uptime_ms} ms state={state} protocol={version}"

    if frame.message_id == DEVICE_INFO_RESPONSE and len(frame.payload) == 12:
        version, fw_major, fw_minor, fw_patch, board_id, transport_id, period_us, max_payload, capabilities, _ = struct.unpack(
            "<BBBBBBHBBH", frame.payload
        )
        return (
            "DEVICE_INFO_RESPONSE "
            f"protocol={version} fw={fw_major}.{fw_minor}.{fw_patch} "
            f"board={board_id} transport={transport_id} period={period_us} us "
            f"max_payload={max_payload} capabilities=0x{capabilities:02X}"
        )

    if frame.message_id == ERROR_RESPONSE and len(frame.payload) == 4:
        request_id, result, state = struct.unpack("<HBB", frame.payload)
        return f"ERROR_RESPONSE request=0x{request_id:04X} result={result} state={state}"

    if frame.message_id in (START_STREAM_RESPONSE, STOP_STREAM_RESPONSE) and len(frame.payload) == 2:
        result, state = struct.unpack("<BB", frame.payload)
        name = "START_STREAM_RESPONSE" if frame.message_id == START_STREAM_RESPONSE else "STOP_STREAM_RESPONSE"
        return f"{name} result={result} state={state}"

    if frame.message_id == TELEMETRY and len(frame.payload) == 14:
        uptime_ms, sample, state, status, event_overflow, rx_overflow, tx_overflow = struct.unpack(
            "<IhBBHHH", frame.payload
        )
        return (
            f"TELEMETRY seq={frame.sequence} uptime={uptime_ms} ms sample={sample} "
            f"state={state} status=0x{status:02X} overflow(event/rx/tx)="
            f"{event_overflow}/{rx_overflow}/{tx_overflow}"
        )

    return (
        f"FRAME id=0x{frame.message_id:04X} flags=0x{frame.flags:02X} "
        f"seq={frame.sequence} payload={frame.payload.hex(' ')}"
    )


def read_until(serial_port: object, parser: FrameParser, deadline: float) -> list[Frame]:
    frames: list[Frame] = []
    while time.monotonic() < deadline:
        waiting = getattr(serial_port, "in_waiting", 0)
        data = serial_port.read(max(1, waiting))
        if data:
            frames.extend(parser.feed(data))
            if frames:
                break
    return frames


def send_request(serial_port: object, message_id: int, sequence: int) -> None:
    frame = encode_frame(message_id, FLAG_REQUEST, sequence)
    serial_port.write(frame)
    serial_port.flush()


def run_hardware_test(port: str, telemetry_count: int, timeout_s: float) -> None:
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as exc:
        raise RuntimeError("pyserial is required: python3 -m pip install pyserial") from exc

    parser = FrameParser()
    sequence = 1

    with serial.Serial(port=port, baudrate=115200, timeout=0.05, write_timeout=1.0) as serial_port:
        serial_port.reset_input_buffer()
        serial_port.reset_output_buffer()

        for request_id, expected_id in (
            (PING_REQUEST, PING_RESPONSE),
            (GET_DEVICE_INFO_REQUEST, DEVICE_INFO_RESPONSE),
            (START_STREAM_REQUEST, START_STREAM_RESPONSE),
        ):
            send_request(serial_port, request_id, sequence)
            deadline = time.monotonic() + timeout_s
            matched = False
            while time.monotonic() < deadline and not matched:
                for frame in read_until(serial_port, parser, deadline):
                    print(describe_frame(frame))
                    if frame.message_id == expected_id and frame.sequence == sequence:
                        matched = True
            if not matched:
                raise TimeoutError(
                    f"timeout waiting for response 0x{expected_id:04X} to request 0x{request_id:04X}"
                )
            sequence += 1

        received_telemetry = 0
        previous_sequence: int | None = None
        telemetry_deadline = time.monotonic() + max(timeout_s, telemetry_count * 0.02)
        while received_telemetry < telemetry_count and time.monotonic() < telemetry_deadline:
            for frame in read_until(serial_port, parser, telemetry_deadline):
                if frame.message_id != TELEMETRY:
                    print(describe_frame(frame))
                    continue

                if previous_sequence is not None and frame.sequence != ((previous_sequence + 1) & 0xFFFF):
                    print(
                        f"WARNING: telemetry sequence gap {previous_sequence} -> {frame.sequence}",
                        file=sys.stderr,
                    )
                previous_sequence = frame.sequence
                print(describe_frame(frame))
                received_telemetry += 1

        send_request(serial_port, STOP_STREAM_REQUEST, sequence)
        stop_deadline = time.monotonic() + timeout_s
        stopped = False
        while time.monotonic() < stop_deadline and not stopped:
            for frame in read_until(serial_port, parser, stop_deadline):
                if frame.message_id == STOP_STREAM_RESPONSE and frame.sequence == sequence:
                    print(describe_frame(frame))
                    stopped = True
                elif frame.message_id != TELEMETRY:
                    print(describe_frame(frame))

        if received_telemetry < telemetry_count:
            raise TimeoutError(
                f"received {received_telemetry}/{telemetry_count} telemetry frames"
            )
        if not stopped:
            raise TimeoutError("timeout waiting for STOP_STREAM result")

    print(
        "hardware smoke test: PASS "
        f"({received_telemetry} telemetry frames, parser CRC errors={parser.crc_error_count})"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", nargs="?", help="serial port, for example COM5 or /dev/ttyACM0")
    parser.add_argument("--telemetry-count", type=int, default=20)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def main() -> int:
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
