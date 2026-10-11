from pathlib import Path
import json, struct, sys, time
assert sys.argv[1] == '31808'
source = Path(__file__).with_name('spawn_lifecycle_probe.py')
ns = {'__file__': str(source), '__name__': 'fast_pause_trace'}
exec(compile(source.read_text().split('\ntry:\n', 1)[0], str(source), 'exec'), ns)
try:
    state, write, symbol = (ns[key] for key in ('state','write','symbol'))
    history=[]
    start=time.monotonic()
    def capture():
        current=state()
        if not history or current != {key: value for key,value in history[-1].items() if key != 'seconds'}:
            history.append({'seconds':round(time.monotonic()-start,4),**current})
        return current
    assert capture()['state']==6
    for cycle in range(3):
        write(symbol('toggle_pending')[0], b'\1')
        deadline=time.monotonic()+3
        while time.monotonic()<deadline:
            current=capture()
            if current['state']==7 and current['visible'] and current['paused_by_menu'] and not current['pause_pending']:
                break
            time.sleep(.001)
        else: raise AssertionError('pause acquisition')
        write(symbol('toggle_pending')[0], b'\1')
        deadline=time.monotonic()+3
        while time.monotonic()<deadline:
            current=capture()
            if current['state']==6 and not current['visible'] and not current['paused']:
                break
            time.sleep(.001)
        else: raise AssertionError('pause release')
        time.sleep(.2)
    print(json.dumps({'cycles_passed':3,'changes':len(history)}))
finally:
    Path(__file__).with_name('dolphin-game-replay-final').joinpath('fast-pause-diagnostic.json').write_text(json.dumps(history,indent=2))
    ns['memory'].close()
