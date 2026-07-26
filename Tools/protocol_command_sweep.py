#!/usr/bin/env python3
"""Interactive command sweep for the STM32F446RE PoC."""

from __future__ import annotations

import argparse
import collections
import os
import struct
import sys
import time
from typing import Deque, Iterable, List, Optional, Tuple

SOF = b"\xA5\x5A"
VERSION = 1
FRAME_OVERHEAD = 10
MAX_PAYLOAD = 1024

PING = 0x01
GET_DEVICE_INFO = 0x02
SET_STREAM_CONFIG = 0x03
START_STREAM = 0x04
STOP_STREAM = 0x05
ACK = 0x80
NACK = 0x81
DEVICE_INFO = 0x82
TELEMETRY_SAMPLE = 0x90

Frame = Tuple[int, int, int, bytes]


class Parser:
    """Bounded stream parser used by the hardware test tool."""

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.crc_error_count = 0
        self.format_error_count = 0

    def feed(self, data: bytes) -> List[Frame]:
        self.buffer.extend(data)
        frames: List[Frame] = []

        while True:
            sof_index = self.buffer.find(SOF)
            if sof_index < 0:
                self.buffer[:] = self.buffer[-1:] if self.buffer[-1:] == SOF[:1] else b""
                break
            if sof_index > 0:
                del self.buffer[:sof_index]
            if len(self.buffer) < FRAME_OVERHEAD:
                break

            payload_length = struct.unpack_from("<H", self.buffer, 6)[0]
            if payload_length > MAX_PAYLOAD:
                self.format_error_count += 1
                del self.buffer[0]
                continue

            total_length = FRAME_OVERHEAD + payload_length
            if len(self.buffer) < total_length:
                break

            raw = bytes(self.buffer[:total_length])
            del self.buffer[:total_length]
            received_crc = struct.unpack_from("<H", raw, total_length - 2)[0]
            if crc16(raw[2:-2]) != received_crc:
                self.crc_error_count += 1
                continue

            frames.append(
                (
                    raw[2],
                    raw[3],
                    struct.unpack_from("<H", raw, 4)[0],
                    raw[8:-2],
                )
            )

        return frames


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for data_byte in data:
        crc ^= data_byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def encode(message_id: int, sequence: int, payload: bytes = b"") -> bytes:
    body = struct.pack("<BBHH", VERSION, message_id, sequence, len(payload)) + payload
    return SOF + body + struct.pack("<H", crc16(body))


def key_pressed() -> bool:
    if os.name == "nt":
        import msvcrt

        if msvcrt.kbhit():
            return msvcrt.getwch().lower() == "q"
        return False

    import select

    ready, _, _ = select.select([sys.stdin], [], [], 0)
    return bool(ready) and sys.stdin.read(1).lower() == "q"


def wait_for_direct_response(
    port: object,
    parser: Parser,
    pending_frames: Deque[Frame],
    request_id: int,
    sequence: int,
    timeout: float = 2.0,
) -> Frame:
    """Return the matching response while preserving every unrelated frame."""

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        pending_count = len(pending_frames)
        for _ in range(pending_count):
            frame = pending_frames.popleft()
            version, message_id, frame_sequence, payload = frame
            sequence_matches = frame_sequence == sequence
            command_response_matches = (
                message_id in (ACK, NACK)
                and len(payload) >= 1
                and payload[0] == request_id
            )
            device_info_matches = (
                request_id == GET_DEVICE_INFO and message_id == DEVICE_INFO
            )

            if (
                version == VERSION
                and sequence_matches
                and (command_response_matches or device_info_matches)
            ):
                return frame
            pending_frames.append(frame)

        data = port.read(4096)
        if data:
            pending_frames.extend(parser.feed(data))

    raise TimeoutError(
        f"no direct response for request 0x{request_id:02X}, sequence {sequence}"
    )


def expect_ack(frame: Frame, request_id: int, expected_state: int) -> None:
    version, message_id, _, payload = frame
    expected_payload = bytes((request_id, 0, expected_state))
    if version != VERSION or message_id != ACK or payload != expected_payload:
        raise RuntimeError(f"unexpected ACK: {frame}")


def strict_result(
    *,
    sample_count: int,
    first_sample_counter: Optional[int],
    sample_gaps: int,
    nonzero_status_frames: int,
    parser: Parser,
) -> bool:
    return (
        sample_count > 0
        and first_sample_counter == 1
        and sample_gaps == 0
        and nonzero_status_frames == 0
        and parser.crc_error_count == 0
        and parser.format_error_count == 0
    )


def run(args: argparse.Namespace) -> None:
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError("pyserial is required: py -m pip install pyserial") from exc

    parser = Parser()
    pending_frames: Deque[Frame] = collections.deque()
    sequence = 1

    with serial.Serial(args.port, args.baud, timeout=0.02) as port:
        port.reset_input_buffer()
        port.reset_output_buffer()

        port.write(encode(PING, sequence))
        expect_ack(
            wait_for_direct_response(
                port, parser, pending_frames, PING, sequence
            ),
            PING,
            0,
        )
        sequence += 1

        port.write(encode(GET_DEVICE_INFO, sequence))
        device_info = wait_for_direct_response(
            port, parser, pending_frames, GET_DEVICE_INFO, sequence
        )
        if device_info[1] != DEVICE_INFO:
            raise RuntimeError("GET_DEVICE_INFO did not return DEVICE_INFO")
        sequence += 1

        port.write(
            encode(
                SET_STREAM_CONFIG,
                sequence,
                struct.pack("<H", args.interval_us),
            )
        )
        expect_ack(
            wait_for_direct_response(
                port, parser, pending_frames, SET_STREAM_CONFIG, sequence
            ),
            SET_STREAM_CONFIG,
            0,
        )
        sequence += 1

        port.write(encode(START_STREAM, sequence))
        expect_ack(
            wait_for_direct_response(
                port, parser, pending_frames, START_STREAM, sequence
            ),
            START_STREAM,
            1,
        )
        sequence += 1

        first_sample_counter: Optional[int] = None
        last_sample_counter: Optional[int] = None
        sample_gaps = 0
        nonzero_status_frames = 0
        sample_count = 0

        print("Streaming; press q or Q to stop.")
        while not key_pressed():
            data = port.read(4096)
            if data:
                pending_frames.extend(parser.feed(data))

            while pending_frames:
                version, message_id, _, payload = pending_frames.popleft()
                if message_id != TELEMETRY_SAMPLE:
                    continue
                if version != VERSION or len(payload) != 14:
                    parser.format_error_count += 1
                    continue

                counter, device_tick, value, status = struct.unpack(
                    "<IIfH", payload
                )
                if first_sample_counter is None:
                    first_sample_counter = counter
                if (
                    last_sample_counter is not None
                    and counter != ((last_sample_counter + 1) & 0xFFFFFFFF)
                ):
                    sample_gaps += 1
                last_sample_counter = counter
                sample_count += 1
                if status != 0:
                    nonzero_status_frames += 1

                if sample_count % args.display_every == 0:
                    print(
                        f"{counter:10d} {device_tick:10d} "
                        f"{value:+.6f} status=0x{status:04X}"
                    )

        port.write(encode(STOP_STREAM, sequence))
        expect_ack(
            wait_for_direct_response(
                port, parser, pending_frames, STOP_STREAM, sequence
            ),
            STOP_STREAM,
            0,
        )

    passed = strict_result(
        sample_count=sample_count,
        first_sample_counter=first_sample_counter,
        sample_gaps=sample_gaps,
        nonzero_status_frames=nonzero_status_frames,
        parser=parser,
    )

    print(
        f"samples={sample_count} first={first_sample_counter} "
        f"gaps={sample_gaps} status_frames={nonzero_status_frames} "
        f"crc_errors={parser.crc_error_count} "
        f"format_errors={parser.format_error_count}"
    )

    if args.statistics_only:
        strict_label = "PASS" if passed else "FAIL"
        print(
            "Protocol command sweep: STATISTICS ONLY "
            f"(strict criteria: {strict_label})"
        )
        return

    if not passed:
        raise RuntimeError("strict sweep criteria failed")

    print("Protocol command sweep: PASS")


class _FakePort:
    def __init__(self, reads: Iterable[bytes]) -> None:
        self._reads = collections.deque(reads)

    def read(self, _: int) -> bytes:
        if self._reads:
            return self._reads.popleft()
        return b""


def self_test() -> None:
    parser = Parser()
    ping = encode(PING, 1)
    unrelated_ack = encode(ACK, 1, bytes((START_STREAM, 0, 1)))
    ping_ack = encode(ACK, 1, bytes((PING, 0, 0)))
    telemetry = encode(
        TELEMETRY_SAMPLE,
        1,
        struct.pack("<IIfH", 1, 5000, 0.0, 0),
    )

    pending_frames: Deque[Frame] = collections.deque()
    fake_port = _FakePort([unrelated_ack + ping_ack + telemetry])
    response = wait_for_direct_response(
        fake_port,
        parser,
        pending_frames,
        PING,
        1,
        timeout=0.1,
    )
    assert response[1] == ACK
    assert len(pending_frames) == 2
    assert pending_frames[0][1] == TELEMETRY_SAMPLE
    assert pending_frames[1][1] == ACK
    assert pending_frames[1][3][0] == START_STREAM

    clean_parser = Parser()
    assert strict_result(
        sample_count=10,
        first_sample_counter=1,
        sample_gaps=0,
        nonzero_status_frames=0,
        parser=clean_parser,
    )
    assert not strict_result(
        sample_count=10,
        first_sample_counter=2,
        sample_gaps=0,
        nonzero_status_frames=0,
        parser=clean_parser,
    )
    assert not strict_result(
        sample_count=10,
        first_sample_counter=1,
        sample_gaps=1,
        nonzero_status_frames=0,
        parser=clean_parser,
    )
    assert not strict_result(
        sample_count=10,
        first_sample_counter=1,
        sample_gaps=0,
        nonzero_status_frames=1,
        parser=clean_parser,
    )

    bad_frame = bytearray(ping)
    bad_frame[-1] ^= 1
    error_parser = Parser()
    assert error_parser.feed(bytes(bad_frame)) == []
    assert error_parser.crc_error_count == 1
    assert not strict_result(
        sample_count=10,
        first_sample_counter=1,
        sample_gaps=0,
        nonzero_status_frames=0,
        parser=error_parser,
    )

    print("protocol_command_sweep self-test: PASS")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port")
    parser.add_argument("--baud", type=int, default=460800)
    parser.add_argument("--interval-us", type=int, default=5000)
    parser.add_argument("--display-every", type=int, default=10)
    parser.add_argument("--statistics-only", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if not 1000 <= args.interval_us <= 60000:
        parser.error("--interval-us must be between 1000 and 60000")
    if args.display_every <= 0:
        parser.error("--display-every must be greater than zero")
    if not args.self_test and not args.port:
        parser.error("--port is required unless --self-test is used")
    return args


def main() -> None:
    args = parse_args()
    if args.self_test:
        self_test()
    else:
        run(args)


if __name__ == "__main__":
    main()
