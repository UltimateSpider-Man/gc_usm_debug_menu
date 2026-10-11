"""Isolated diagnostic: call the payload pause bridge and trace native results."""
from pathlib import Path
import json, re, struct, sys, time
assert int(sys.argv[1]) != 9084
root = Path(__file__).resolve().parents[1]
source = root / 'build/live_gc_smoke.py'
ns = {'__file__': str(source), '__name__': 'pause_diagnostic'}
exec(compile(source.read_text().split('\ntry:\n', 1)[0], str(source), 'exec'), ns)
try:
    read, word, symbol, write = (ns[key] for key in ('read', 'word', 'symbol', 'write'))
    helper = int(re.search(r'^([0-9a-f]+)\s+T\s+_ZN3usm2gc6engine24dismiss_debug_pause_menuEv$',
                          (root / 'build/gamecube/usm_debug_menu.symbols.txt').read_text(), re.M)[1], 16)
    assert read(symbol('deferred_count')[0], 1) == b'\0'
    def snapshot():
        game = word(0x8045F694)
        end = word(game + 0x88)
        states, index, count = struct.unpack('>3I', read(end - 0x14, 12))
        manager = word(0x803B8490)
        return {'state': word(states + index * 4), 'paused': word(game + 0x1B0),
                'physical_pad0': read(symbol('latest_pads')[0], 12).hex(),
                'native_pause_menu_index': struct.unpack('>i', read(manager + 8, 4))[0],
                'status': ns['text'](symbol('status_text')[0]),
                **{name: read(symbol(name)[0], 1)[0] for name in ('menu_visible','paused_by_menu','pause_pending','unpause_pending','deferred_count')}}
    history = [{'seconds': 0, **snapshot()}]
    index = read(symbol('deferred_head')[0], 1)[0]
    command = b'\0' * 52 + struct.pack('>I', helper) + b'\0' * 4 + struct.pack('>I', 1)
    write(symbol('deferred_commands')[0] + index * 64, command)
    write(symbol('deferred_count')[0], b'\1')
    start = time.monotonic()
    while time.monotonic() - start < 1:
        current = snapshot()
        if current != {key: value for key, value in history[-1].items() if key != 'seconds'}:
            history.append({'seconds': round(time.monotonic() - start, 4), **current})
        time.sleep(.001)
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else root / 'build/dolphin-game-replay-final/pause-helper-diagnostic.json'
    output.write_text(json.dumps(history, indent=2))
    significant=[]
    for item in history:
        current={key:value for key,value in item.items() if key not in ('seconds','physical_pad0')}
        if not significant or current != {key:value for key,value in significant[-1].items() if key not in ('seconds','physical_pad0')}:
            significant.append(item)
    print(json.dumps(significant, indent=2))
finally:
    ns['memory'].close()
