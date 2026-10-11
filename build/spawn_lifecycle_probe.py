"""Trace a spawn in an owned disposable Dolphin using payload navigation only."""
import json
import hashlib
import re
from pathlib import Path
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
smoke = ROOT / 'build/live_gc_smoke.py'
assert int(sys.argv[1]) != 9084, 'Never mutate the user instance'
namespace = {'__file__': str(smoke), '__name__': 'spawn_lifecycle_helpers'}
exec(compile(smoke.read_text().split('\ntry:\n', 1)[0], str(smoke), 'exec'), namespace)
memory = namespace['memory']
symbol, read, word = (namespace[name] for name in ('symbol', 'read', 'word'))
menu, write, queue = (namespace[name] for name in ('menu', 'write', 'queue'))
mailbox = int(re.search(r'^([0-9a-f]+)\s+\w\s+usm_gc_scene_mailbox$',
                       (ROOT / 'build/gamecube/usm_debug_menu.symbols.txt').read_text(), re.M)[1], 16)

def snapshot():
    game = word(0x8045F694)
    end = word(game + 0x88)
    states, index, count = struct.unpack('>3I', read(end - 0x14, 12))
    native_state = word(states + index * 4) if index < count <= 64 else -1
    world = word(0x8045F180)
    item = {'state': native_state, 'world': hex(world), 'terrain': hex(word(world + 0x198)),
            'visible': read(symbol('menu_visible')[0], 1)[0],
            'unpause': read(symbol('unpause_pending')[0], 1)[0],
            'paused': word(game + 0x1B0)}
    for name in ('actor_count', 'queue_size', 'queue_head', 'owning_world', 'owning_terrain'):
        item[name] = word(symbol(name, 'character_spawn')[0])
    item['results0'] = read(symbol('results', 'character_spawn')[0], 1)[0]
    item['worker_active'] = read(symbol('worker_active', 'character_spawn')[0], 1)[0]
    item['status'] = namespace['text'](symbol('status_text')[0])
    item['paused_by_menu'] = read(symbol('paused_by_menu')[0], 1)[0]
    item['pause_pending'] = read(symbol('pause_pending')[0], 1)[0]
    manager = word(0x803B8490)
    item['native_pause_menu_index'] = struct.unpack('>i', read(manager + 8, 4))[0]
    hero = word(world + 0x21C)
    damage = word(hero + 0x68)
    item['hero_health'] = struct.unpack('>f', read(damage + 0x200, 4))[0]
    item['god_mode_enabled'] = word(0x8045EFAC) != 0
    item['actor0'] = read(symbol('actors', 'character_spawn')[0], 12).hex()
    item['packs_sha256'] = hashlib.sha256(read(*symbol('packs', 'character_spawn'))).hexdigest()
    return item

def state():
    for attempt in range(10):
        before = word(mailbox + 8)
        item = snapshot()
        after = word(mailbox + 8)
        if before == after:
            item['stable_frame'] = True
            return item
    item['stable_frame'] = False
    return item

def wait_for(predicate, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        item = state()
        if item['stable_frame'] and predicate(item):
            return item
        time.sleep(0.002)
    raise AssertionError(item)

try:
    if '--reopen' in sys.argv and read(symbol('menu_visible')[0], 1) == b'\0':
        write(symbol('toggle_pending')[0], b'\1')
        time.sleep(0.5)
    assert read(symbol('menu_visible')[0], 1) == b'\1'
    # Let the disposable boot expression finish before closing gameplay pause.
    # Otherwise its late START pulse would open the stock pause UI mid-test.
    deadline = time.monotonic() + 55
    while time.monotonic() < deadline and read(symbol('latest_pads')[0], 2) != b'\0\0':
        time.sleep(0.02)
    assert read(symbol('latest_pads')[0], 2) == b'\0\0'
    assert read(symbol('paused_by_menu')[0], 1) == b'\1'
    pool = symbol('menu_pool')[0]
    menus = {menu(pool + i * 24)['title']: pool + i * 24 for i in range(word(symbol('used_menus')[0]))}
    # Use the actual live God Mode handler to protect this disposable hero.
    # The retail startup loader resets the INI God Mode value to zero.
    game_menu = menu(menus['Game'])
    god_row = game_menu['labels'].index('God Mode')
    original_god_mode = word(word(0x804627F8) + 0x340 + 29 * 4)
    assert original_god_mode in (0, 3)
    for _ in range(3 - original_god_mode):
        queue(game_menu['entries'] + god_row * 52, 2)
        deadline = time.monotonic() + 3
        while read(symbol('deferred_count')[0], 1) != b'\0' and time.monotonic() < deadline:
            time.sleep(.002)
        time.sleep(.05)
    assert word(word(0x804627F8) + 0x340 + 29 * 4) == 3 and word(0x8045EFAC)
    progression = menu(menus['Progression'])
    write(symbol('current_menu')[0], struct.pack('>I', progression['address']))
    time.sleep(.2)
    progression = menu(menus['Progression'])
    if progression['labels'] == ['Build Progression entries']:
        queue(progression['entries'])
        time.sleep(1)
        if read(symbol('menu_visible')[0], 1) == b'\0':
            write(symbol('toggle_pending')[0], b'\1')
            time.sleep(.2)
        write(symbol('current_menu')[0], struct.pack('>I', progression['address']))
        time.sleep(.2)
        progression = menu(menus['Progression'])
    off_row = progression['labels'].index('toggle progression off')
    queue(progression['entries'] + off_row * 52)
    # Let the actual VM handler disable progression before testing repeated
    # unpauses. Any already queued stock prompt receives the real A pulse55s.
    deadline = time.monotonic() + 20
    running_since = None
    while time.monotonic() < deadline:
        item = state()
        if item['state'] == 6 and not item['visible'] and not item['paused']:
            if running_since is None: running_since = time.monotonic()
            if time.monotonic() - running_since >= .5: break
        else: running_since = None
        time.sleep(.002)
    else: raise AssertionError({'progression_setup': item})
    write(symbol('toggle_pending')[0], b'\1')
    wait_for(lambda item: item['state'] == 7 and item['visible'] and item['paused_by_menu'] and not item['pause_pending'])
    script = menu(menus['Script'])
    write(symbol('current_menu')[0], struct.pack('>I', script['address']))
    time.sleep(0.2)
    history = [{'seconds': 0, **state()}]
    assert history[0]['actor_count'] == 0 and history[0]['queue_size'] == 0
    selected = script['labels'].index('alex_ohirn')
    queue(script['entries'] + selected * 52)
    start = time.monotonic()
    while time.monotonic() - start < 10:
        current = state()
        previous = {key: value for key, value in history[-1].items() if key != 'seconds'}
        if current != previous:
            history.append({'seconds': round(time.monotonic() - start, 4), **current})
        if current['actor_count']:
            break
        time.sleep(0.002)
    assert current['actor_count'] == 1, history
    Path(sys.argv[2]).write_text(json.dumps({'first_attempt': history}, indent=2) + '\n')
    created = wait_for(lambda item: item['actor_count'] == 1 and item['state'] == 6 and not item['paused'])
    write(symbol('toggle_pending')[0], b'\1')
    paused = wait_for(lambda item: item['visible'] and item['state'] == 7 and item['paused_by_menu'] and not item['pause_pending'])
    write(symbol('toggle_pending')[0], b'\1')
    retention = []
    start = time.monotonic()
    while time.monotonic() - start < 1.0:
        item = state()
        if not retention or item != retention[-1]:
            retention.append(item)
        time.sleep(0.002)
    # A script may open its own dialog after the world resumes. The disposable
    # controller profile acknowledges it with a real A pulse at55s.
    running = wait_for(lambda item: item['state'] == 6 and not item['visible'], 20)
    assert running['actor_count'] == 1 and running['actor0'] == created['actor0']
    assert running['packs_sha256'] == created['packs_sha256']
    write(symbol('toggle_pending')[0], b'\1')
    wait_for(lambda item: item['state'] == 7 and item['visible'])
    queue(script['entries'] + script['labels'].index('Pop All Characters') * 52)
    popped = wait_for(lambda item: item['actor_count'] == 0 and item['queue_size'] == 0)
    result = {'first_attempt': history, 'created': created, 'paused': paused,
              'test_god_mode': 3, 'original_god_mode': original_god_mode,
              'test_progression': 'toggle progression off through the real native VM handler',
              'pause_return_retention': retention, 'running_same_actor_and_packs': running,
              'after_pop_all': popped}
    destination = Path(sys.argv[2])
    destination.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
finally:
    memory.close()
