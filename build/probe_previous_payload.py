"""Read-only snapshot of this task's Dolphin MEM1 and payload state."""
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import re
import struct
import sys

root = Path(__file__).resolve().parents[1]
pid = int(sys.argv[1])
k32 = ctypes.WinDLL("kernel32", use_last_error=True)

class MBI(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p), ("AllocationBase", ctypes.c_void_p),
                ("AllocationProtect", wintypes.DWORD), ("RegionSize", ctypes.c_size_t),
                ("State", wintypes.DWORD), ("Protect", wintypes.DWORD), ("Type", wintypes.DWORD)]

k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
k32.OpenProcess.restype = wintypes.HANDLE
k32.VirtualQueryEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.POINTER(MBI), ctypes.c_size_t]
k32.VirtualQueryEx.restype = ctypes.c_size_t
k32.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
k32.ReadProcessMemory.restype = wintypes.BOOL
k32.CloseHandle.argtypes = [wintypes.HANDLE]
handle = k32.OpenProcess(0x410, False, pid)
if not handle:
    raise ctypes.WinError(ctypes.get_last_error())

def read(address, size):
    buffer = ctypes.create_string_buffer(size)
    count = ctypes.c_size_t()
    if not k32.ReadProcessMemory(handle, address, buffer, size, ctypes.byref(count)) or count.value != size:
        return None
    return buffer.raw

try:
    original = (root / "main.original.dol").read_bytes()
    first_offset = struct.unpack_from(">I", original)[0]
    signature = original[first_offset:first_offset + 64]
    position = 0
    mem1 = None
    while position < 0x7FFFFFFFFFFF:
        info = MBI()
        if not k32.VirtualQueryEx(handle, position, ctypes.byref(info), ctypes.sizeof(info)):
            break
        base = info.BaseAddress or 0
        if info.State == 0x1000 and not info.Protect & 0x101 and info.RegionSize >= 0x1800000:
            if read(base + 0x3100, len(signature)) == signature:
                mem1 = base
                break
        position = base + info.RegionSize
    if mem1 is None:
        raise RuntimeError("No MEM1 mapping matched the original DOL's first 64 instruction bytes")

    def gc_read(address, size):
        if not 0x80000000 <= address < address + size <= 0x81800000:
            return None
        return read(mem1 + address - 0x80000000, size)

    def word(address):
        data = gc_read(address, 4)
        return struct.unpack(">I", data)[0] if data is not None else None

    state = {"pid": pid, "mem1_mapping": hex(mem1), "render_hook_instruction": hex(word(0x8016E55C))}
    game = word(0x8045F694)
    state["game_pointer"] = hex(game) if game is not None else None
    state["empty_render_flag"] = word(0x804607C4)
    front_end = word(0x803B8494)
    state["front_end_pointer"] = hex(front_end) if front_end else None
    state["front_end_state_38"] = word(front_end + 0x38) if front_end else None
    options = word(0x804627F8)
    state["no_load_screen"] = word(options + 4 + 67 * 4) if options else None
    if game and (process_end := word(game + 0x88)):
        state["level_loaded"] = word(game + 0x1A0)
        state["render_gate_194"] = word(game + 0x194)
        state["loader_ready_5c"] = word(game + 0x5C)
        state["active_camera_74"] = word(game + 0x74)
        state["single_step_1a4"] = word(game + 0x1A4)
        state["physics_disabled_1a8"] = word(game + 0x1A8)
        process = process_end - 0x18
        states, index, count = word(process + 4), word(process + 8), word(process + 12)
        if (name := word(process)) and (name_data := gc_read(name, 64)):
            state["game_process_name"] = name_data.split(b"\0", 1)[0].decode("ascii", "replace")
        if states and index is not None and count is not None and index < count <= 64:
            state["game_state"] = word(states + index * 4)
    if "--baseline" in sys.argv:
        print(json.dumps(state, indent=2))
        sys.exit(0)
    mapping = (root / "build/gamecube/usm_debug_menu.map").read_text()
    names = ["menu_visible", "pause_pending", "unpause_pending", "paused_by_menu", "interaction_locked",
             "current_menu", "root_menu", "used_entries", "used_menus", "is_initialized", "runtime_initialized",
             "latest_pads", "latest_buttons", "previous_toggle", "active_channel", "toggle_pending", "sample_pending"]
    for name in names:
        match = re.search(r"\.(?:bss|data)\.[^\n ]*L\d+" + name + r"E\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)", mapping)
        if not match:
            continue
        address, size = int(match[1], 16) - 0x300, int(match[2], 16)
        data = gc_read(address, size)
        state[name] = int.from_bytes(data, "big") if size <= 4 else data.hex()
    current = state.get("current_menu", 0)
    if current and (header := gc_read(current, 24)):
        title, entries, parent, refresh, count, capacity, selected, scroll = struct.unpack(">4I4H", header)
        state["current_menu_header"] = {"address": hex(current), "count": count, "capacity": capacity,
                                          "selected": selected, "scroll": scroll}
        if data := gc_read(title, 80):
            state["current_menu_header"]["title"] = data.split(b"\0", 1)[0].decode("ascii", "replace")
        labels = []
        for i in range(min(count, 64)):
            label = word(entries + i * 52)
            data = gc_read(label, 80) if label else None
            labels.append(data.split(b"\0", 1)[0].decode("ascii", "replace") if data else None)
        state["current_menu_labels"] = labels
    print(json.dumps(state, indent=2))
finally:
    k32.CloseHandle(handle)

