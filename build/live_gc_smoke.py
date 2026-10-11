"""Disposable Dolphin test: inspect dynamic menus and queue existing handlers.

Requires an isolated game instance paused by the real Z hook. Writes only
payload menu/deferred state, never code, native objects, saves, or user inputs.
The frontend blackscreen bypass used to reach this test is recorded separately.
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
memory = DolphinMemory(pid, ROOT / "main.dol", ROOT / "build/gamecube/usm_debug_menu.symbols.txt")
assert memory.discover()
mapping = (ROOT / "build/gamecube/usm_debug_menu.map").read_text()


def symbol(name, namespace=""):
    match = re.search(r"\.(?:bss|data)\.[^\n ]*" + namespace + r"[^\n ]*L\d+" + name +
                      r"E\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)", mapping)
    assert match, name
    return int(match[1], 16), int(match[2], 16)


def read(address, size):
    if 0x80000000 <= address <= 0x81800000 - size:
        data = memory._read(memory.mem1 + address - 0x80000000, size)
        assert data is not None
        return data
    assert 0xF0000000 <= address <= 0x100000000 - size
    # The native ARAM pager uses SR15 VSID0x20 and a128KB hashed page table.
    # Translate only already resident pages; never fault pages in or write RAM.
    chunks = []
    while size:
        amount = min(size, 0x1000 - (address & 0xFFF))
        page = address & ~0xFFF
        data = None
        for _ in range(3):
            table = word(0x8045F524)
            table = 0x80000000 | (table & 0x3FFFFFFF)
            table_bytes = read(table, 0x20000)
            for offset in range(0, len(table_bytes), 8):
                pte0, pte1 = struct.unpack_from(">II", table_bytes, offset)
                if not pte0 & 0x80000000 or (pte0 >> 7) & 0x7FFFFF != 0x20:
                    continue
                group = offset // 64
                if pte0 & 0x40:
                    group = (~group) & 0x7FF
                vpn = ((pte0 & 0x3F) << 10) | ((group ^ 0x20) & 0x3FF)
                if vpn & 0x400 != (group ^ 0x20) & 0x400:
                    continue
                if (0xF0000000 | (vpn << 12)) != page:
                    continue
                physical = pte1 & 0xFFFFF000
                assert physical <= 0x1800000 - 0x1000
                candidate = read(0x80000000 + physical + (address & 0xFFF), amount)
                if read(table + offset, 8) == table_bytes[offset:offset + 8]:
                    data = candidate
                break
            if data is not None:
                break
        assert data is not None, f"ARAM page {page:#x} is not stably resident"
        chunks.append(data)
        address += amount
        size -= amount
    return b"".join(chunks)


def word(address):
    return struct.unpack(">I", read(address, 4))[0]


def text(address, limit=112):
    return read(address, limit).split(b"\0", 1)[0].decode("ascii", "replace")


allowed = [symbol(name) for name in ("current_menu", "menu_pool", "deferred_commands", "deferred_count", "deferred_head", "toggle_pending")]


def write(address, data):
    assert any(start <= address <= start + size - len(data) for start, size in allowed)
    buffer = ctypes.create_string_buffer(data, len(data))
    count = ctypes.c_size_t()
    assert memory.k32.WriteProcessMemory(memory.handle, memory.mem1 + address - 0x80000000,
                                         buffer, len(data), ctypes.byref(count)) and count.value == len(data)


def menu(address):
    # Dynamic menu refresh clears and rebuilds entries during a native update.
    # Take a coherent snapshot rather than reading individual words mid-build.
    for _ in range(100):
        header = read(address, 24)
        title, entries, parent, refresh, count, capacity, selected, scroll = struct.unpack(">4I4H", header)
        blob = read(entries, count * 52)
        pointers = [struct.unpack_from('>I', blob, i * 52)[0] for i in range(count)]
        if not all(0x80000000 <= pointer < 0x81800000 for pointer in pointers):
            time.sleep(.002)
            continue
        labels = [text(pointer) for pointer in pointers]
        if read(address, 24) == header and read(entries, count * 52) == blob:
            return {"title": text(title), "address": address, "entries": entries, "count": count,
                    "labels": labels}
        time.sleep(.002)
    raise AssertionError('Dynamic menu did not yield a coherent snapshot')


def queue(entry_address, input_command=0):
    assert read(symbol("interaction_locked")[0], 1) == b"\0"
    assert read(symbol("deferred_count")[0], 1) == b"\0"
    for _ in range(100):
        entry = read(entry_address, 52)
        if entry == read(entry_address, 52) and struct.unpack_from('>I', entry, 12)[0]:
            break
        time.sleep(.002)
    else:
        raise AssertionError('Entry did not yield a coherent handler snapshot')
    handler = struct.unpack_from(">I", entry, 12)[0]
    assert 0x80636040 <= handler < 0x81800000
    index = read(symbol("deferred_head")[0], 1)[0]
    # Entry52 + handler4 + command1/padding3 + multiplier4 =64 bytes.
    command = entry + struct.pack(">I", handler) + bytes([input_command]) + b"\0" * 3 + struct.pack(">I", 1)
    write(symbol("deferred_commands")[0] + index * 64, command)
    write(symbol("deferred_count")[0], b"\1")


def enter_submenu(parent, index):
    parent_view = menu(parent)
    entry = parent_view["entries"] + index * 52
    child = word(entry + 4)
    assert child and read(entry + 48, 1) == b"\0"
    write(parent + 20, struct.pack(">H", index))
    write(child + 8, struct.pack(">I", parent))
    write(symbol("current_menu")[0], struct.pack(">I", child))
    time.sleep(0.3)
    return menu(child)


try:
    if "--reopen" in sys.argv and read(symbol("menu_visible")[0], 1) == b"\0":
        write(symbol("toggle_pending")[0], b"\1")
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline and read(symbol("menu_visible")[0], 1) != b"\1":
            time.sleep(0.01)
    assert read(symbol("menu_visible")[0], 1) == b"\1"
    pool, _ = symbol("menu_pool")
    count = word(symbol("used_menus")[0])
    menus = {menu(pool + i * 24)["title"]: pool + i * 24 for i in range(count)}
    result = {"pid": pid, "method": "synthetic deferred commands from existing entries; real engine calls",
              "dynamic_menus": []}
    for name in ("Missions", "Level Select", "Progression"):
        write(symbol("current_menu")[0], struct.pack(">I", menus[name]))
        time.sleep(0.2)
        item = menu(menus[name])
        result["dynamic_menus"].append(item)
    result["mission_records"] = word(symbol("mission_record_count", "mission_level")[0])
    result["mission_catalogue_truncated"] = bool(read(symbol("mission_catalogue_truncated", "mission_level")[0], 1)[0])
    labels_address, _ = symbol("mission_labels", "mission_level")
    result["mission_labels"] = [text(labels_address + i * 96, 96) for i in range(result["mission_records"])]
    if "--diagnostics" in sys.argv:
        result["diagnostics"] = {}
        for name in ("AI", "Entity Animations"):
            write(symbol("current_menu")[0], struct.pack(">I", menus[name]))
            deadline = time.monotonic() + 5
            while True:
                time.sleep(0.1)
                root_view = menu(menus[name])
                children = [i for i in range(root_view["count"])
                            if read(root_view["entries"] + i * 52 + 48, 1) == b"\0"]
                if children or time.monotonic() >= deadline:
                    break
            result["diagnostics"][name] = {"root": root_view}
            if name == "Entity Animations":
                assert children, root_view
            if not children:
                continue
            selected_child = children[0]
            if name == "Entity Animations":
                hero = word(word(0x8045F180) + 0x21C)
                page_index = next(i for i, label in enumerate(root_view["labels"])
                                  if label.startswith("Page ("))
                page_entry = root_view["entries"] + page_index * 52
                page_target = word(page_entry + 8)
                maximum_page = word(page_entry + 24)
                assert maximum_page <= 4096 // 31

                def actor_rows(view):
                    return [i for i in range(view["count"])
                            if read(view["entries"] + i * 52 + 48, 1) == b"\0"]

                def change_actor_page(view, wanted):
                    assert 0 <= wanted <= maximum_page
                    for _ in range(maximum_page + 1):
                        current_page = word(page_target)
                        if current_page == wanted:
                            return view
                        rows = actor_rows(view)
                        previous_first = word(view["entries"] + rows[0] * 52 + 8) if rows else 0
                        direction = 2 if current_page < wanted else 1
                        expected_page = current_page + (1 if direction == 2 else -1)
                        queue(page_entry, direction)
                        deadline = time.monotonic() + 3
                        while time.monotonic() < deadline:
                            candidate = menu(menus[name])
                            rows = actor_rows(candidate)
                            first = word(candidate["entries"] + rows[0] * 52 + 8) if rows else 0
                            if word(page_target) == expected_page and rows and first != previous_first:
                                view = candidate
                                break
                            time.sleep(0.005)
                        else:
                            raise AssertionError(("actor page did not refresh", expected_page, candidate))
                    raise AssertionError(("actor page traversal exceeded bound", wanted))

                root_view = change_actor_page(root_view, 0)
                selected_actor = None
                fallback = None
                selected_page = 0
                for page in range(maximum_page + 1):
                    if page:
                        root_view = change_actor_page(root_view, page)
                    for index in actor_rows(root_view):
                        actor = word(root_view["entries"] + index * 52 + 8)
                        if fallback is None and word(actor + 0x74):
                            fallback = (page, actor)
                        if actor == hero:
                            selected_page, selected_actor = page, actor
                            break
                    if selected_actor is not None:
                        break
                if selected_actor is None:
                    assert fallback is not None, "No animation actor has an existing controller"
                    selected_page, selected_actor = fallback
                    root_view = change_actor_page(root_view, selected_page)
                selected_child = next(index for index in actor_rows(root_view)
                                      if word(root_view["entries"] + index * 52 + 8) == selected_actor)
                result["diagnostics"][name]["selection"] = {
                    "hero": hex(hero), "actor": hex(selected_actor), "root_page": selected_page,
                    "used_existing_controller_fallback": selected_actor != hero}
                result["diagnostics"][name]["selected_root"] = root_view
            detail = enter_submenu(menus[name], selected_child)
            result["diagnostics"][name]["detail"] = detail
            if name == "AI":
                parameters = enter_submenu(detail["address"], 0)
                result["diagnostics"][name]["parameters"] = parameters
                editable = []
                for i in range(parameters["count"]):
                    if parameters["labels"][i].startswith("Page (") or "Export" in parameters["labels"][i]:
                        continue
                    entry = parameters["entries"] + i * 52
                    target = word(entry + 8)
                    if target and word(target + 4) in (0, 1):
                        editable.append((word(target + 4), i, target))
                if editable:
                    kind, i, target = sorted(editable, reverse=True)[0]
                    before = word(target)
                    queue(parameters["entries"] + i * 52, 2)
                    time.sleep(0.15)
                    incremented = word(target)
                    queue(parameters["entries"] + i * 52, 1)
                    time.sleep(0.15)
                    result["diagnostics"][name]["edit_round_trip"] = {
                        "type": kind, "before": before, "incremented": incremented,
                        "restored": word(target), "label": parameters["labels"][i]}
                if "--export-ai" in sys.argv:
                    export_index = next(i for i, label in enumerate(parameters["labels"]) if "Export" in label)
                    queue(parameters["entries"] + export_index * 52)
                    deadline = time.monotonic() + 8
                    while time.monotonic() < deadline:
                        try:
                            exported = memory.ai_snapshot()
                        except ValueError:
                            # The payload/companion publishes a coherent snapshot
                            # across multiple writes; retry its explicit guard.
                            time.sleep(.002)
                            continue
                        if exported["request"] and exported["acknowledged"] == exported["request"]:
                            break
                        time.sleep(0.01)
                    result["diagnostics"][name]["export"] = {key: value for key, value in exported.items() if key != "data"}
                    import hashlib
                    result["diagnostics"][name]["export"]["data_sha256"] = hashlib.sha256(exported["data"]).hexdigest()
                    assert exported["request"] and exported["acknowledged"] == exported["request"] and exported["result"] == 1
            elif "--play-animation" in sys.argv and detail["count"] > 1:
                entry = detail["entries"] + 52
                actor = word(entry + 8)
                expected_hash = word(entry + 44)
                print(json.dumps({'animation_queued_entry': read(entry,52).hex(), 'detail':detail['address'],
                                  'selected':result['diagnostics'][name]['selection']}), flush=True)
                queue(entry)
                playback = {
                    "label": detail["labels"][1], "actor": hex(actor),
                    "expected_hash": hex(expected_hash)}
                deadline = time.monotonic() + 3
                matched = False
                while time.monotonic() < deadline:
                    controller = word(actor + 0x74)
                    status = text(symbol("status_text")[0])
                    closed = read(symbol("menu_visible")[0], 1) == b"\0"
                    playback.update(controller=hex(controller), menu_closed=closed, status=status)
                    if controller and closed and status == "Animation started at normal speed":
                        vtable = word(controller + 0x10)
                        adjustment = struct.unpack(">h", read(vtable + 0xA8, 2))[0]
                        adjusted = controller + adjustment
                        state = word(adjusted + 0x28)
                        instance = word(state) if state else 0
                        clip = word(instance + 0x0C) if instance else 0
                        actual_hash = word(clip + 8) if clip else 0
                        speed = struct.unpack(">f", read(state + 4, 4))[0] if state else None
                        playback.update(vtable=hex(vtable), this_adjustment=adjustment,
                            adjusted_controller=hex(adjusted), base_state=hex(state),
                            instance=hex(instance), clip=hex(clip),
                            actual_hash=hex(actual_hash), speed=speed)
                        if actual_hash == expected_hash and speed == 1.0:
                            matched = True
                            break
                    time.sleep(0.001)
                playback["native_clip_and_speed_match"] = matched
                result["diagnostics"][name]["playback"] = playback
                if not matched:
                    Path(__file__).with_name('animation-failure.json').write_text(json.dumps(result, indent=2))
                assert matched and playback["menu_closed"] and playback["status"] == "Animation started at normal speed", playback
    if "--cameras" in sys.argv:
        camera = menu(menus["Debug Menu"])
        camera_index = next(i for i, label in enumerate(camera["labels"]) if label.rstrip() == "Camera")
        result["cameras"] = []
        for input_command, name in ((0, "user"), (0, "sceneanalyzer"), (0, "chase"), (1, "sceneanalyzer"), (2, "chase")):
            write(symbol("current_menu")[0], struct.pack(">I", menus["Debug Menu"]))
            time.sleep(0.1)
            queue(camera["entries"] + camera_index * 52, input_command)
            time.sleep(0.3)
            game, options, world = word(0x8045F694), word(0x804627F8), word(0x8045F180)
            visible_row = text(symbol("rendered_entries")[0] + camera_index * 160, 160)
            assert visible_row == "Camera : " + name, visible_row
            expected_mode = ("chase", "user", "sceneanalyzer").index(name)
            assert word(game + 0x1C8) == (expected_mode == 1)
            assert word(options + 0x348) == expected_mode
            assert word(0x804626F8) == (expected_mode == 2)
            result["cameras"].append({"selected": name, "input_command": input_command,
                "visible_row": visible_row, "user_enabled": word(game + 0x1C8),
                "camera_state": word(options + 0x348), "analyzer_enabled": word(0x804626F8),
                "active": hex(word(game + 0x74)), "chase": hex(word(game + 0x78)),
                "user": hex(word(world + 0x58)), "game_paused": word(game + 0x1B0),
                "menu_visible": read(symbol("menu_visible")[0], 1)[0]})
    if "--spawn-pop" in sys.argv:
        script = menu(menus["Script"])
        index = script["labels"].index("alex_ohirn")
        queue(script["entries"] + index * 52)
        deadline = time.monotonic() + 10
        actor_count = symbol("actor_count", "character_spawn")[0]
        while time.monotonic() < deadline and word(actor_count) == 0:
            time.sleep(0.1)
        result["spawned_actors"] = word(actor_count)
        result["spawn_status"] = text(symbol("status_text")[0])
        if result["spawned_actors"]:
            queue(script["entries"] + script["labels"].index("Pop All Characters") * 52)
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and word(actor_count):
                time.sleep(0.1)
            result["actors_after_pop_all"] = word(actor_count)
            result["pop_status"] = text(symbol("status_text")[0])
    if "--progression" in sys.argv:
        progression = menu(menus["Progression"])
        if progression["labels"] == ["Build Progression entries"]:
            queue(progression["entries"])
            time.sleep(1)
        result["progression_after_setup"] = menu(menus["Progression"])
        result["setup_status"] = text(symbol("status_text")[0])
        if read(symbol("menu_visible")[0], 1) == b"\0":
            write(symbol("toggle_pending")[0], b"\1")
            time.sleep(0.2)
        write(symbol("current_menu")[0], struct.pack(">I", menus["Progression"]))
        time.sleep(0.2)
        result["progression_after_refresh"] = menu(menus["Progression"])
        if "--progression-action" in sys.argv:
            refreshed = result["progression_after_refresh"]
            index = refreshed["labels"].index("toggle progression off")
            queue(refreshed["entries"] + index * 52)
            time.sleep(0.3)
            result["progression_activation"] = {
                "label": "toggle progression off",
                "menu_closed": read(symbol("menu_visible")[0], 1) == b"\0",
                "status": text(symbol("status_text")[0]),
                "scope": "existing entry handler reached native add_thread; full script side effects not asserted",
            }
    if "--levels" in sys.argv:
        level = menu(menus["Level Select"])
        write(symbol("current_menu")[0], struct.pack(">I", menus["Level Select"]))
        time.sleep(0.2)
        queue(level["entries"] + level["labels"].index("characterb") * 52)
        time.sleep(0.2)
        result["characterb_status"] = text(symbol("status_text")[0])
        result["characterb_mailbox"] = memory.snapshot()
        before_reboot = memory.snapshot()
        queue(level["entries"] + level["labels"].index("city") * 52)
        if "--auto-reboot" in sys.argv:
            deadline = time.monotonic() + 40
            history = []
            saw_request = False
            while time.monotonic() < deadline:
                try:
                    if memory.mem1 is None and not memory.discover():
                        time.sleep(0.01)
                        continue
                    state = memory.snapshot()
                except (OSError, ValueError, UnicodeError):
                    memory.mem1 = None
                    time.sleep(0.01)
                    continue
                key = (state["request"], state["acknowledged"], state["result"])
                if not history or key != tuple(history[-1][k] for k in ("request", "acknowledged", "result")):
                    history.append(state)
                saw_request |= state["request"] != 0
                if saw_request and state["request"] == 0 and state["frame"] < before_reboot["frame"]:
                    break
                time.sleep(0.01)
            result["city_save_and_reboot"] = {"mailbox_history": history,
                "reboot_observed": saw_request and state["request"] == 0 and state["frame"] < before_reboot["frame"]}
        else:
            time.sleep(1)
            result["city_status"] = text(symbol("status_text")[0])
            result["city_mailbox"] = memory.snapshot()
    output_directory = ROOT / "build/dolphin-progression-final"
    for argument in sys.argv:
        if argument.startswith("--output-dir="):
            output_directory = Path(argument.split("=", 1)[1])
    destination = output_directory / ("native-level-smoke.json" if "--levels" in sys.argv else "native-progression-smoke.json" if "--progression" in sys.argv else "native-camera-smoke.json" if "--cameras" in sys.argv else "native-diagnostics-smoke.json" if "--diagnostics" in sys.argv else "native-menu-smoke.json")
    destination.write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
finally:
    memory.close()
