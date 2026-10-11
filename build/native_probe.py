import sys, struct
from pathlib import Path
sys.path.insert(0, '/tmp/usm-capstone')
from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN
p=Path('/mnt/c/Users/usmde/Desktop/USM2/usm_debug_menu_gamecube/main.original.dol')
data=p.read_bytes()
offsets=struct.unpack_from('>18I',data,0)
addresses=struct.unpack_from('>18I',data,0x48)
sizes=struct.unpack_from('>18I',data,0x90)
dis=Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
for arg in sys.argv[1:]:
    a,_,n=arg.partition(':'); address=int(a,16); count=int(n,16) if n else 0x80
    for off,base,size in zip(offsets,addresses,sizes):
        virtual=0x7F004000 if base==0x80466000 else base
        if size and virtual <= address and address+count <= virtual+size:
            start=off+address-virtual
            print(f'--- {address:08x} ---')
            for i in dis.disasm(data[start:start+count],address):
                print(f'{i.address:08x} {i.bytes.hex()} {i.mnemonic} {i.op_str}')
            break
    else: raise ValueError(f'Address outside DOL: {address:x}')
