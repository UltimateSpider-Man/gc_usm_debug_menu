from pathlib import Path
import sys, struct, json
source = Path(__file__).with_name('live_gc_smoke.py')
ns = {'__file__': str(source), '__name__': 'snapshot_helpers'}
exec(compile(source.read_text().split('\ntry:\n', 1)[0], str(source), 'exec'), ns)
try:
    word, read, symbol = (ns[x] for x in ('word', 'read', 'symbol'))
    game = word(0x8045F694)
    end = word(game + 0x88)
    states, index, count = struct.unpack('>3I', read(end - 0x14, 12))
    print(json.dumps({'game': hex(game), 'state': word(states + index * 4),
      'loaded': word(game + 0x1A0), 'frontend': word(0x803B8494),
      'black': word(game + 0x194), 'paused': word(game + 0x1B0),
      'menu': read(symbol('menu_visible')[0], 1).hex(),
      'physical_pad0': read(symbol('latest_pads')[0], 12).hex(),
      'native_pad0': read(0x8043DD90, 12).hex(),
      'status':ns['text'](symbol('status_text')[0]),
      'deferred_count':read(symbol('deferred_count')[0],1)[0],
      'hero':hex(word(word(0x8045F180)+0x21C)),
      'selected_animation_actor':hex(word(symbol('selected_animation_actor')[0])),
      'actor_context':hex(word(word(symbol('selected_animation_actor')[0])+0xBC)) if word(symbol('selected_animation_actor')[0]) else None,
      'current_menu':ns['menu'](word(symbol('current_menu')[0]))}))
finally:
    ns['memory'].close()
