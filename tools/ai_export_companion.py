"""Bounded AI debug snapshots saved by the Windows Dolphin companion."""
from __future__ import annotations

from pathlib import Path
import struct
import time
import uuid

from game_ini import atomic_write

AI_MAGIC, AI_VERSION, AI_HEADER_SIZE, AI_CAPACITY = 0x55534D41, 1, 32, 65536


def decode_ai_header(raw: bytes) -> dict:
    if len(raw) != AI_HEADER_SIZE:
        raise ValueError("incomplete AI export mailbox")
    magic, version, frame, host_frame, request, acknowledged, result, length = struct.unpack(">8I", raw)
    if (magic, version) != (AI_MAGIC, AI_VERSION):
        raise ValueError("AI export mailbox signature/version mismatch")
    if length > AI_CAPACITY or (request and not length) or result > 2:
        raise ValueError("invalid AI export mailbox fields")
    return {"frame": frame, "host_frame": host_frame, "request": request,
            "acknowledged": acknowledged, "result": result, "length": length}


def save_ai_export(directory: Path, data: bytes) -> Path:
    if not data or len(data) > AI_CAPACITY or b"\0" in data:
        raise ValueError("invalid AI text snapshot")
    directory.mkdir(parents=True, exist_ok=True)
    destination = directory / f"ai-block-{time.time_ns()}-{uuid.uuid4().hex}.txt"
    atomic_write(destination, data)
    return destination


class AiExportMonitor:
    def __init__(self, directory: Path):
        self.directory = directory
        self.last_success = 0
        self.previous_frame = 0
        self.next_retry = 0.0
        self.saved_request = 0
        self.saved_data = b""
        self.saved_path = None

    @staticmethod
    def still_current(memory, state):
        latest = memory.ai_snapshot()
        return latest["frame"] >= state["frame"] and all(
            latest[key] == state[key] for key in ("request", "length", "data"))

    def poll(self, memory) -> None:
        state = memory.ai_snapshot()
        memory.write_ai_word(12, state["frame"])
        if state["request"] == 0 or state["frame"] < self.previous_frame:
            self.last_success = 0
            self.next_retry = 0.0
            self.saved_request, self.saved_data, self.saved_path = 0, b"", None
        self.previous_frame = state["frame"]
        request = state["request"]
        if not request or request == self.last_success or time.monotonic() < self.next_retry:
            return
        confirmed = memory.ai_snapshot()
        if any(confirmed[key] != state[key] for key in ("request", "length", "data")):
            return
        try:
            if (self.saved_request, self.saved_data) == (request, state["data"]):
                destination = self.saved_path
            else:
                destination = save_ai_export(self.directory, state["data"])
                self.saved_request, self.saved_data, self.saved_path = request, state["data"], destination
        except (OSError, ValueError) as error:
            print(f"AI export failed: {error}", flush=True)
            if not self.still_current(memory, state):
                return
            memory.write_ai_word(24, 2)
            memory.write_ai_word(20, request)
            self.next_retry = time.monotonic() + 2.0
        else:
            # SDK restart can happen while a slow filesystem flush is in
            # progress. Never acknowledge a different or rewound session.
            if not self.still_current(memory, state):
                return
            memory.write_ai_word(24, 1)
            memory.write_ai_word(20, request)
            self.last_success = request
            self.next_retry = 0.0
            print(f"Saved AI block: {destination}", flush=True)
