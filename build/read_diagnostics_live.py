import json, struct, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from dolphin_ini_companion import DolphinMemory
memory=DolphinMemory(int(sys.argv[1]),ROOT/'main.dol',ROOT/'build/gamecube/usm_debug_menu.symbols.txt')
assert memory.discover()
def words(address, count=12):
    if not 0x80000000<=address<=0x81800000-count*4:return None
    raw=memory._read(memory.mem1+address-0x80000000,count*4)
    return [hex(x) for x in struct.unpack('>'+str(count)+'I',raw)] if raw else None
def word(address):return int(words(address,1)[0],16)
try:
    result={}
    for label,slot in [('ai_high',0x8045F7B0),('ai_low',0x8045F7B4),('found_entities',0x8045F018)]:
        pointer=word(slot); item={'slot':hex(slot),'pointer':hex(pointer),'object':words(pointer)}
        if words(pointer,2):
            sentinel=word(pointer+4);item['sentinel']=hex(sentinel);item['sentinel_data']=words(sentinel)
            node=word(sentinel) if words(sentinel,1) else 0;nodes=[]
            for _ in range(12):
                if node==sentinel or not words(node,3):break
                obj=word(node+8);nodes.append({'address':hex(node),'data':words(node,3),'object':words(obj)})
                node=word(node)
            item['nodes']=nodes
            node=word(sentinel) if words(sentinel,1) else 0
            previous=sentinel
            for visited in range(4097):
                if node==sentinel:
                    item['validation']={'visited':visited,'tail_matches':word(sentinel+4)==previous};break
                if not words(node,3):
                    item['validation']={'visited':visited,'bad_node':hex(node)};break
                obj=word(node+8);nxt=word(node);prev=word(node+4)
                if prev!=previous or not words(obj,5):
                    item['validation']={'visited':visited,'bad_entity_or_prev':hex(node),'previous':hex(previous),'data':words(node,3)};break
                previous=node;node=nxt
        result[label]=item
    world=word(0x8045F180);hero=word(world+0x21C)
    result['hero']={'world':hex(world),'actor':hex(hero),'data':words(hero,52),'controller':words(word(hero+0x74)), 'context':words(word(hero+0xBC))}
    result['native_pause_dialog']={'system':hex(word(0x803B8490)), 'index':word(word(0x803B8490)+8)}
    destination=Path(sys.argv[2]);destination.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:memory.close()
