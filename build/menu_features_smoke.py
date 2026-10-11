"""Run Game/Devopts/Replay handlers in an isolated Dolphin test instance."""
import json
from pathlib import Path
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
smoke = ROOT / 'build/live_gc_smoke.py'
assert int(sys.argv[1]) != 9084, 'Never mutate the user instance'
namespace = {'__file__': str(smoke), '__name__': 'menu_features_helpers'}
exec(compile(smoke.read_text().split('\ntry:\n', 1)[0], str(smoke), 'exec'), namespace)
memory = namespace['memory']
symbol, read, word = (namespace[name] for name in ('symbol', 'read', 'word'))
menu, write, queue = (namespace[name] for name in ('menu', 'write', 'queue'))

def reopen():
    if read(symbol('menu_visible')[0], 1) == b'\0':
        write(symbol('toggle_pending')[0], b'\1')
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline and read(symbol('menu_visible')[0], 1) != b'\1':
            time.sleep(0.01)
    assert read(symbol('menu_visible')[0], 1) == b'\1'

def select(view, index):
    reopen()
    write(view['address'] + 20, struct.pack('>2H', index, index))
    write(symbol('current_menu')[0], struct.pack('>I', view['address']))
    time.sleep(0.15)

def command(view, index, direction=0):
    select(view, index)
    queue(view['entries'] + index * 52, direction)
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline and read(symbol('deferred_count')[0], 1) != b'\0':
        time.sleep(0.001)
    time.sleep(0.2)

def rendered():
    active = word(symbol('current_menu')[0])
    selected, scroll = struct.unpack('>2H', read(active + 20, 4))
    return namespace['text'](symbol('rendered_entries')[0] + (selected - scroll) * 160, 160)

def god_mode():
    god, ultra, mega = (word(address) != 0 for address in (0x8045EFAC, 0x8045EFB0, 0x8045EFB4))
    return (3 if mega else 2 if ultra else 1) if god else (5 if mega else 4 if ultra else 0)

try:
    pool = symbol('menu_pool')[0]
    menus = {menu(pool + i * 24)['title']: menu(pool + i * 24)
             for i in range(word(symbol('used_menus')[0]))}
    root, game, devopts, replay = (menus[name] for name in ('Debug Menu', 'Game', 'Devopts', 'Replay'))
    assert root['labels'].index('Replay') == root['labels'].index('District variants') + 1
    assert {'Record', 'Play', 'Stop', 'Clear'}.issubset(replay['labels']) and devopts['count'] == 229
    options, game_object = word(0x804627F8), word(0x8045F694)
    result = {'pid': int(sys.argv[1]), 'method': 'payload-only navigation and existing deferred native handlers',
              'root_labels': root['labels'], 'devopts_count': devopts['count'], 'checks': []}
    for index in (3, 99):
        address = options + 4 + index * 4
        before = word(address)
        neighbors = read(address - 4, 12)
        command(devopts, index)
        after = word(address)
        visible = rendered()
        assert after == (0 if before else 1)
        assert read(address - 4, 4) == neighbors[:4] and read(address + 4, 4) == neighbors[8:]
        command(devopts, index)
        assert word(address) == (1 if before else 0)
        result['checks'].append({'flag': devopts['labels'][index], 'before': before, 'changed': after, 'visible': visible})
    last_small_int = next(index for index in range(77, 30, -1)
                          if -999 <= struct.unpack('>i', read(options + 0x340 + index * 4, 4))[0] <= 999)
    for index in (3, last_small_int):
        row, address = 151 + index, options + 0x340 + index * 4
        before = struct.unpack('>i', read(address, 4))[0]
        direction = 2 if before < 1000 else 1
        command(devopts, row, direction)
        changed = struct.unpack('>i', read(address, 4))[0]
        assert changed == before + (1 if direction == 2 else -1)
        visible = rendered()
        command(devopts, row, 1 if direction == 2 else 2)
        assert struct.unpack('>i', read(address, 4))[0] == before
        result['checks'].append({'int': devopts['labels'][row], 'before': before, 'changed': changed, 'visible': visible})
    before = god_mode()
    direction = 2 if before < 5 else 1
    row = devopts['labels'].index('GOD_MODE')
    command(devopts, row, direction)
    changed = god_mode()
    assert changed == before + (1 if direction == 2 else -1)
    assert word(options + 0x340 + 29 * 4) == changed
    select(game, game['labels'].index('God Mode'))
    assert rendered().endswith(str(changed))
    command(devopts, row, 1 if direction == 2 else 2)
    assert god_mode() == before
    result['checks'].append({'god_mode_before': before, 'god_mode_changed': changed, 'restored': god_mode()})
    physics = game['labels'].index('Physics Enabled')
    before = word(game_object + 0x1A8)
    command(game, physics)
    assert word(game_object + 0x1A8) == (0 if before else 1)
    physics_visible = rendered()
    command(game, physics)
    assert word(game_object + 0x1A8) == before
    result['checks'].append({'physics_before_disabled_word': before, 'changed_visible': physics_visible})
    flags = [('Show FPS', 5), ('Show Hero Position', 21), ('Show Districts', 88)]
    for label, index in flags:
        before = word(options + 4 + index * 4)
        select(game, game['labels'].index(label))
        if not before:
            command(game, game['labels'].index(label))
        assert word(options + 4 + index * 4) != 0
        result['checks'].append({'game_alias': label, 'native_flag': index, 'visible': rendered(), 'original': before})
    time.sleep(1)
    fps_address = symbol('measured_fps', 'devopt_features')[0]
    measured_fps = struct.unpack('>f', read(fps_address, 4))[0]
    assert measured_fps > 0
    result['measured_fps'] = measured_fps
    result['overlay_visual_rendering_verified'] = False
    for label, index in flags:
        original = next(item['original'] for item in result['checks'] if item.get('game_alias') == label)
        if not original:
            command(game, game['labels'].index(label))
        assert word(options + 4 + index * 4) == original
    frame_lock = word(options + 0x340 + 13 * 4)
    assert frame_lock != 120
    slow = game['labels'].index('Slow Motion Enabled')
    command(game, slow)
    assert word(options + 0x340 + 13 * 4) == 120
    command(game, slow)
    assert word(options + 0x340 + 13 * 4) == frame_lock
    result['checks'].append({'slow_motion_frame_lock_before': frame_lock, 'enabled': 120, 'restored': frame_lock})
    result['replay_rows'] = replay['labels']
    Path(sys.argv[2]).write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
finally:
    memory.close()
