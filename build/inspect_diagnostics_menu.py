from pathlib import Path
import json, struct, sys, time
source = Path(__file__).with_name('live_gc_smoke.py')
ns = {'__file__': str(source), '__name__': 'inspect_helpers'}
exec(compile(source.read_text().split('\ntry:\n', 1)[0], str(source), 'exec'), ns)
try:
    read, word, symbol, write, menu = (ns[x] for x in ('read', 'word', 'symbol', 'write', 'menu'))
    pool = symbol('menu_pool')[0]
    for i in range(word(symbol('used_menus')[0])):
        item = menu(pool + i * 24)
        if item['title'] not in ('AI', 'Entity Animations'):
            continue
        write(symbol('current_menu')[0], struct.pack('>I', item['address']))
        time.sleep(.3)
        item = menu(item['address'])
        item['rows'] = [{'label':item['labels'][j], 'data':read(item['entries'] + j*52,52).hex()}
                        for j in range(item['count'])]
        print(json.dumps(item, indent=2))
finally:
    ns['memory'].close()
