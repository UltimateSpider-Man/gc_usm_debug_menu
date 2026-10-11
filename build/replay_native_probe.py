"""Exercise Replay through real deferred handlers in the isolated test game."""
import json
from pathlib import Path
import struct
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
assert int(sys.argv[1]) != 9084
smoke = ROOT / 'build/live_gc_smoke.py'
ns = {'__file__': str(smoke), '__name__': 'replay_native_helpers'}
exec(compile(smoke.read_text().split('\ntry:\n', 1)[0], str(smoke), 'exec'), ns)
read, word, symbol, write = (ns[name] for name in ('read', 'word', 'symbol', 'write'))

def wait(predicate, seconds=6):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.002)
    raise AssertionError({'mode': value('mode'), 'stored': value('stored_samples'),
                          'cursor': value('playback_cursor'), 'status': ns['text'](symbol('status_text')[0])})

def value(name):
    return word(symbol(name, 'replay')[0])

def reopen():
    if not read(symbol('menu_visible')[0], 1)[0]:
        write(symbol('toggle_pending')[0], b'\1')
    wait(lambda: read(symbol('menu_visible')[0], 1)[0] != 0)
    time.sleep(0.2)

def action(label):
    reopen()
    write(symbol('current_menu')[0], struct.pack('>I', replay['address']))
    ns['queue'](replay['entries'] + replay['labels'].index(label) * 52)
    wait(lambda: read(symbol('deferred_count')[0], 1) == b'\0')

try:
    pool = symbol('menu_pool')[0]
    menus = {ns['menu'](pool + i * 24)['title']: ns['menu'](pool + i * 24)
             for i in range(word(symbol('used_menus')[0]))}
    replay = menus['Replay']
    physical = symbol('latest_pads')[0]
    # Allow the disposable boot/dialog A pulse55s to finish before a Replay
    # menu can receive it. The launch timestamp belongs to this owned process.
    launched = float((ROOT / 'build/dolphin-game-replay-final/launched-unix-seconds.txt').read_text())
    while time.time() < launched + 65:
        time.sleep(.05)
    wait(lambda: 0 < read(physical + 2, 1)[0] < 128 and read(physical, 2) == b'\0\0', 50)
    action('Record')
    wait(lambda: value('mode') == 2 and value('stored_samples') >= 20)
    action('Stop')
    wait(lambda: value('mode') == 0)
    count = value('stored_samples')
    duration = struct.unpack('>f', read(symbol('duration_seconds', 'replay')[0], 4))[0]
    assert count >= 20 and duration > 0
    # The disposable controller expression alternates positive/neutral stick.
    # Observe physical input before playback, instead of writing any input RAM.
    wait(lambda: read(physical, 4) == b'\0\0\0\0', 50)
    wait(lambda: read(0x8043DD90, 4) == b'\0\0\0\0', 5)
    time.sleep(0.1)
    assert read(0x8043DD90, 4) == b'\0\0\0\0'
    action('Play')
    wait(lambda: value('mode') == 4)
    delivery = []
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline and value('mode') == 4:
        raw = read(physical, 12)
        native = read(0x8043DD90, 12)
        if raw[2:4] == b'\0\0' and 0 < native[2] < 128:
            delivery.append({'cursor': value('playback_cursor'),
                             'physical_pad0': raw.hex(), 'native_clamped_pad0': native.hex()})
        time.sleep(0.002)
    Path(sys.argv[2]).write_text(json.dumps({'pid':int(sys.argv[1]), 'recorded_samples':count,
        'native_delivery_while_physical_neutral':delivery, 'completion_verified':False}, indent=2))
    wait(lambda: value('mode') == 0)
    assert delivery, 'Recorded stick input never reached native pad buffer while physical stick was neutral'
    assert value('playback_cursor') == count
    action('Play')
    wait(lambda: value('mode') == 4)
    action('Stop')
    wait(lambda: value('mode') == 0)
    interrupted = value('playback_cursor')
    assert interrupted < count and value('stored_samples') == count
    action('Clear')
    wait(lambda: value('stored_samples') == 0)
    result = {'pid': int(sys.argv[1]), 'labels': replay['labels'], 'recorded_samples': count,
              'recorded_seconds': duration, 'native_delivery_while_physical_neutral': delivery,
              'playback_completed_cursor': count, 'interrupted_cursor': interrupted,
              'after_clear_samples': value('stored_samples'),
              'scope': 'Controller inputs in current world and position; world state is not rewound; RAM session only',
              'setup': 'An existing script dialog was dismissed through the payload native pause helper before testing; Record, Play, Stop and Clear use actual Replay menu handlers',
              'visual_rendering_verified': False}
    Path(sys.argv[2]).write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
finally:
    ns['memory'].close()
