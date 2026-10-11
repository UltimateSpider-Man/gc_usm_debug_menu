"""Reach the debug hook in a disposable, isolated Null-backend smoke instance.

Records an explicit one-word startup black-screen bypass. This is not a normal
boot or interactive rendering test, and must never target the user's instance.
"""
import ctypes
import json
from pathlib import Path
import re
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from dolphin_ini_companion import DolphinMemory

pid = int(sys.argv[1])
assert pid not in (31716, 9084), "The user's Dolphin instance is excluded"
destination = Path(sys.argv[2])
memory = DolphinMemory(pid, ROOT / "main.dol", ROOT / "build/gamecube/usm_debug_menu.symbols.txt")
mapping = (ROOT / "build/gamecube/usm_debug_menu.map").read_text()


def word(address):
    assert 0x80000000 <= address <= 0x817FFFFC
    raw = memory._read(memory.mem1 + address - 0x80000000, 4)
    assert raw is not None
    return struct.unpack(">I", raw)[0]


def byte_symbol(name):
    match = re.search(r"\.(?:bss|data)\.[^\n ]*L\d+" + name + r"E\s+0x([0-9a-f]+)\s+0x1\b", mapping)
    assert match
    address = int(match[1], 16)
    return memory._read(memory.mem1 + address - 0x80000000, 1)[0]


try:
    deadline = time.monotonic() + 45
    allow_bypass = "--no-bypass" not in sys.argv
    evidence = {"pid": pid, "backend": "Null", "normal_boot_verified": False,
                "startup_bypass": "none" if not allow_bypass else "Only game+0x194 black-screen WORD cleared once after RUNNING+level_loaded+frontend null"}
    while time.monotonic() < deadline:
        if memory.mem1 is None and not memory.discover():
            time.sleep(0.1)
            continue
        game = word(0x8045F694)
        if not game or not 0x80000000 <= game <= 0x817FF000:
            time.sleep(0.1)
            continue
        process_end = word(game + 0x88)
        if process_end < 0x80000018:
            time.sleep(0.1)
            continue
        states, index, count = (word(process_end - 0x18 + i) for i in (4, 8, 12))
        state = word(states + index * 4) if 0 <= index < count <= 64 and states >= 0x80000000 else -1
        if state in (6, 7) and word(game + 0x1A0) == 1 and word(0x803B8494) == 0:
            before = word(game + 0x194)
            assert before in (0, 1)
            if before and not allow_bypass:
                time.sleep(0.1)
                continue
            if before:
                raw = ctypes.create_string_buffer(b"\0" * 4, 4)
                written = ctypes.c_size_t()
                assert memory.k32.WriteProcessMemory(memory.handle, memory.mem1 + game + 0x194 - 0x80000000,
                                                     raw, 4, ctypes.byref(written)) and written.value == 4
            evidence.update(game=hex(game), game_state_before=state, black_screen_before=before)
            break
        time.sleep(0.1)
    else:
        raise RuntimeError("Disposable test did not reach a loaded native world")
    deadline = time.monotonic() + 35
    while time.monotonic() < deadline:
        if byte_symbol("menu_visible") and byte_symbol("paused_by_menu"):
            break
        time.sleep(0.05)
    evidence.update(normal_boot_verified=not allow_bypass, menu_visible=byte_symbol("menu_visible"), paused_by_menu=byte_symbol("paused_by_menu"),
                    game_paused=word(game + 0x1B0), black_screen_after=word(game + 0x194))
    destination.write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps(evidence, indent=2))
finally:
    memory.close()
