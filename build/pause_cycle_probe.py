from pathlib import Path
import sys, struct, json, time
source = Path(__file__).with_name('live_gc_smoke.py')
ns = {'__file__': str(source), '__name__': 'pause_helpers'}
exec(compile(source.read_text().split('\ntry:\n', 1)[0], str(source), 'exec'), ns)
assert int(sys.argv[1]) != 9084
try:
    word, read, symbol, write = (ns[x] for x in ('word', 'read', 'symbol', 'write'))
    game = word(0x8045F694)
    result=[]
    if '--unpause' in sys.argv:
        ns['allowed'].append(symbol('unpause_pending'))
        write(symbol('unpause_pending')[0], b'\1')
    else:
        write(symbol('toggle_pending')[0], b'\1')
    start=time.monotonic()
    while time.monotonic()-start<2:
        end=word(game+0x88)
        states,index,count=struct.unpack('>3I',read(end-0x14,12))
        item={'state':word(states+index*4),'paused':word(game+0x1B0),
              **{name:read(symbol(name)[0],1)[0] for name in ('menu_visible','paused_by_menu','pause_pending','unpause_pending')},
              'physical_buttons':read(symbol('latest_pads')[0],2).hex()}
        item['native_pause_menu_index']=struct.unpack('>i',read(word(0x803B8490)+8,4))[0]
        item['status']=ns['text'](symbol('status_text')[0])
        if not result or item!={k:v for k,v in result[-1].items() if k!='seconds'}:
            result.append({'seconds':round(time.monotonic()-start,4),**item})
        time.sleep(.001)
    print(json.dumps(result,indent=2))
finally:
    ns['memory'].close()
