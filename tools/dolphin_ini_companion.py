#!/usr/bin/env python3
"""Save debug Level Select requests and export AI block snapshots in Dolphin.

Only exported mailbox response fields are written in the emulator. The source ISO is
never modified. Run with --source/--dolphin to build and launch, or --pid/--iso
to attach to an already running debug disc built by debug_disc.py.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import struct
import subprocess
import time

from debug_disc import make_debug_disc, write_disc_ini
from game_ini import scene_name, write_scene
from inject_gamecube_payload import DolImage, parse_symbol_file
from make_validation_disc import WORKSPACE_BUILD
from ai_export_companion import AI_HEADER_SIZE, AI_CAPACITY, AiExportMonitor, decode_ai_header

ROOT = Path(__file__).resolve().parents[1]
MAGIC, VERSION, MAILBOX_SIZE = 0x55534D53, 1, 60


def decode_mailbox(raw: bytes) -> dict:
    if len(raw) != MAILBOX_SIZE:
        raise ValueError("incomplete scene mailbox")
    magic, version, frame, host_frame, request, acknowledged, result = struct.unpack_from(">7I", raw)
    if (magic, version) != (MAGIC, VERSION):
        raise ValueError("scene mailbox version/signature mismatch")
    scene = raw[28:].split(b"\0", 1)[0].decode("ascii")
    if request:
        scene = scene_name(scene)
    return {"frame": frame, "host_frame": host_frame, "request": request,
            "acknowledged": acknowledged, "result": result, "scene": scene}


class DolphinMemory:
    """MEM1 discovery plus writes restricted to this payload's mailbox."""
    def __init__(self, pid: int, dol: Path, symbols: Path):
        if os.name != "nt":
            raise OSError("the Dolphin companion requires 64-bit Windows Python")
        if ctypes.sizeof(ctypes.c_void_p) != 8:
            raise OSError("use a 64-bit Python to inspect Dolphin")
        self.k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self.k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.k32.OpenProcess.restype = wintypes.HANDLE
        self.k32.CloseHandle.argtypes = [wintypes.HANDLE]
        self.k32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        self.k32.GetExitCodeProcess.restype = wintypes.BOOL
        self.k32.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                              ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        self.k32.ReadProcessMemory.restype = wintypes.BOOL
        self.k32.WriteProcessMemory.argtypes = self.k32.ReadProcessMemory.argtypes
        self.k32.WriteProcessMemory.restype = wintypes.BOOL
        self.k32.VirtualQueryEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                           ctypes.c_size_t]
        self.k32.VirtualQueryEx.restype = ctypes.c_size_t
        self.handle = self.k32.OpenProcess(0x438, False, pid)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        self.mem1 = None
        image = DolImage(dol.read_bytes())
        payload_offset, payload_start, payload_size = image.text_slot(3)
        exported = parse_symbol_file(symbols)
        self.mailbox = exported["usm_gc_scene_mailbox"]
        self.ai_mailbox = exported.get("usm_gc_ai_mailbox")
        if not payload_size or not payload_start <= self.mailbox <= payload_start + payload_size - MAILBOX_SIZE:
            self.close()
            raise ValueError("scene mailbox is outside the DOL payload")
        if self.ai_mailbox is not None and not payload_start <= self.ai_mailbox <= payload_start + payload_size - AI_HEADER_SIZE - AI_CAPACITY:
            self.close()
            raise ValueError("AI export mailbox is outside the DOL payload")
        first = next(s for s in image.sections if s.kind == "text" and s.slot == 0)
        self.signature = image.data[first.file_offset:first.file_offset + 64]
        self.first_address = first.address
        self.payload_start = payload_start
        self.payload_signature = image.data[payload_offset:payload_offset + 64]

    def close(self):
        if self.handle:
            self.k32.CloseHandle(self.handle)
            self.handle = None

    def alive(self) -> bool:
        code = wintypes.DWORD()
        return bool(self.k32.GetExitCodeProcess(self.handle, ctypes.byref(code)) and code.value == 259)

    def _read(self, address: int, size: int) -> bytes | None:
        buffer = ctypes.create_string_buffer(size)
        count = ctypes.c_size_t()
        ok = self.k32.ReadProcessMemory(self.handle, address, buffer, size, ctypes.byref(count))
        return buffer.raw if ok and count.value == size else None

    def discover(self) -> bool:
        class MBI(ctypes.Structure):
            _fields_ = [("BaseAddress", ctypes.c_void_p), ("AllocationBase", ctypes.c_void_p),
                        ("AllocationProtect", wintypes.DWORD), ("RegionSize", ctypes.c_size_t),
                        ("State", wintypes.DWORD), ("Protect", wintypes.DWORD), ("Type", wintypes.DWORD)]
        position = 0
        while position < 0x7FFFFFFFFFFF:
            info = MBI()
            if not self.k32.VirtualQueryEx(self.handle, position, ctypes.byref(info), ctypes.sizeof(info)):
                break
            base = info.BaseAddress or 0
            if info.State == 0x1000 and not info.Protect & 0x101 and info.RegionSize >= 0x1800000:
                if (self._read(base + self.first_address - 0x80000000, 64) == self.signature and
                        self._read(base + self.payload_start - 0x80000000, 64) == self.payload_signature):
                    raw = self._read(base + self.mailbox - 0x80000000, MAILBOX_SIZE)
                    if raw is not None:
                        try:
                            decode_mailbox(raw)
                        except (ValueError, UnicodeError):
                            pass
                        else:
                            self.mem1 = base
                            return True
            next_position = base + info.RegionSize
            if next_position <= position:
                break
            position = next_position
        return False

    def snapshot(self) -> dict:
        if self.mem1 is None:
            raise OSError("Dolphin MEM1 has not been discovered")
        raw = self._read(self.mem1 + self.mailbox - 0x80000000, MAILBOX_SIZE)
        if raw is None:
            self.mem1 = None
            raise OSError("Dolphin MEM1 is no longer readable")
        return decode_mailbox(raw)

    def write_word(self, offset: int, value: int) -> None:
        if self.mem1 is None or offset not in (12, 20, 24):
            raise ValueError("write is outside the scene mailbox response fields")
        data = struct.pack(">I", value)
        buffer = ctypes.create_string_buffer(data, 4)
        count = ctypes.c_size_t()
        if not self.k32.WriteProcessMemory(self.handle, self.mem1 + self.mailbox - 0x80000000 + offset,
                                           buffer, 4, ctypes.byref(count)) or count.value != 4:
            raise ctypes.WinError(ctypes.get_last_error())

    def ai_snapshot(self) -> dict:
        if self.mem1 is None or self.ai_mailbox is None:
            raise OSError("AI export MEM1 mailbox is not available")
        address = self.mem1 + self.ai_mailbox - 0x80000000
        raw = self._read(address, AI_HEADER_SIZE)
        if raw is None:
            raise OSError("AI export mailbox is no longer readable")
        state = decode_ai_header(raw)
        data = self._read(address + AI_HEADER_SIZE, state["length"]) if state["request"] else b""
        repeated = self._read(address, AI_HEADER_SIZE)
        if data is None or repeated is None:
            raise OSError("AI export snapshot is no longer readable")
        confirmed = decode_ai_header(repeated)
        if (state["request"], state["length"]) != (confirmed["request"], confirmed["length"]):
            raise ValueError("AI export changed while being read")
        state["data"] = data
        return state

    def write_ai_word(self, offset: int, value: int) -> None:
        if self.mem1 is None or self.ai_mailbox is None or offset not in (12, 20, 24):
            raise ValueError("write is outside the AI export mailbox response fields")
        data = struct.pack(">I", value)
        buffer = ctypes.create_string_buffer(data, 4)
        count = ctypes.c_size_t()
        address = self.mem1 + self.ai_mailbox - 0x80000000 + offset
        if not self.k32.WriteProcessMemory(self.handle, address, buffer, 4, ctypes.byref(count)) or count.value != 4:
            raise ctypes.WinError(ctypes.get_last_error())


def save_request(game_ini: Path, iso: Path, scene: str) -> None:
    replacement = write_scene(game_ini, scene)
    write_disc_ini(iso, replacement)


def scene_request_still_current(memory, state):
    latest = memory.snapshot()
    return latest["frame"] >= state["frame"] and all(
        latest[key] == state[key] for key in ("request", "scene"))


def monitor(memory: DolphinMemory, game_ini: Path, iso: Path, ai_directory: Path | None = None) -> None:
    last_success = 0
    next_retry = 0.0
    announced = False
    previous_frame = 0
    ai_monitor = AiExportMonitor(ai_directory) if ai_directory is not None and memory.ai_mailbox is not None else None
    while memory.alive():
        try:
            if memory.mem1 is None and not memory.discover():
                time.sleep(0.25)
                continue
            state = memory.snapshot()
            memory.write_word(12, state["frame"])
        except (OSError, ValueError, UnicodeError):
            memory.mem1 = None
            announced = False
            time.sleep(0.25)
            continue
        if state["request"] == 0 or state["frame"] < previous_frame:
            last_success = 0
            next_retry = 0.0
        previous_frame = state["frame"]
        if not announced:
            print("Connected. City/Characterb selections will save game.ini and the debug ISO.", flush=True)
            announced = True
        request = state["request"]
        if request and request != last_success and time.monotonic() >= next_retry:
            # The game publishes request last. Re-read to avoid a mixed pair
            # when two selections arrive during this polling interval.
            try:
                confirmed = memory.snapshot()
            except (OSError, ValueError, UnicodeError):
                memory.mem1 = None
                time.sleep(0.25)
                continue
            if (confirmed["request"], confirmed["scene"]) == (request, state["scene"]):
                try:
                    save_request(game_ini, iso, state["scene"])
                except (OSError, ValueError, KeyError) as error:
                    print(f"Save failed: {error}", flush=True)
                    try:
                        if scene_request_still_current(memory, state):
                            memory.write_word(24, 2)
                            memory.write_word(20, request)
                    except (OSError, ValueError, UnicodeError):
                        memory.mem1 = None
                    next_retry = time.monotonic() + 2.0
                else:
                    try:
                        current = scene_request_still_current(memory, state)
                        if current:
                            memory.write_word(24, 1)
                            memory.write_word(20, request)
                    except (OSError, ValueError, UnicodeError):
                        memory.mem1 = None
                    else:
                        if current:
                            last_success = request
                    next_retry = 0.0
                    print(f"Saved SCENE_NAME={state['scene']}", flush=True)
        if ai_monitor is not None:
            try:
                ai_monitor.poll(memory)
            except (OSError, ValueError):
                # SDK restart temporarily clears the mailbox. Rediscovery of
                # the scene payload already handles the shared MEM1 lifetime.
                pass
        time.sleep(0.1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--source", type=Path, help="source ISO; build a debug copy and launch Dolphin")
    mode.add_argument("--pid", type=int, help="attach to this running Dolphin process")
    parser.add_argument("--game-ini", type=Path, required=True)
    parser.add_argument("--iso", type=Path, default=WORKSPACE_BUILD / "debug-disc/usm-debug-game-replay.iso")
    parser.add_argument("--dolphin", type=Path)
    parser.add_argument("--user", type=Path, help="optional isolated Dolphin user directory")
    parser.add_argument("--dol", type=Path, default=ROOT / "main.dol")
    parser.add_argument("--symbols", type=Path, default=ROOT / "build/gamecube/usm_debug_menu.symbols.txt")
    parser.add_argument("--ai-export-dir", type=Path, default=WORKSPACE_BUILD / "ai-exports")
    args = parser.parse_args()
    memory = None
    try:
        if os.name != "nt":
            raise OSError("the Dolphin companion requires Windows")
        if args.source:
            if not args.dolphin or not args.dolphin.is_file():
                raise ValueError("--dolphin must name Dolphin.exe when launching")
            make_debug_disc(args.source, args.dol, args.game_ini, args.iso, force=True)
            command = [str(args.dolphin), "--exec", str(args.iso)]
            if args.user:
                command.extend(["--user", str(args.user)])
            process = subprocess.Popen(command)
            args.pid = process.pid
        elif not args.iso.with_suffix(args.iso.suffix + ".usm.json").is_file():
            raise ValueError("--iso must be a debug disc made by debug_disc.py")
        memory = DolphinMemory(args.pid, args.dol, args.symbols)
        print("Waiting for the debug payload...", flush=True)
        monitor(memory, args.game_ini, args.iso, args.ai_export_dir)
    except KeyboardInterrupt:
        print("Companion stopped; Dolphin remains open.")
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"error: {error}\n")
    finally:
        if memory:
            memory.close()


if __name__ == "__main__":
    main()
