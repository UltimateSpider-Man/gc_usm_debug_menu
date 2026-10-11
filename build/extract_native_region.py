import struct
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
data = (root / 'main.original.dol').read_bytes()
offsets = struct.unpack_from('>18I', data, 0)
addresses = struct.unpack_from('>18I', data, 0x48)
sizes = struct.unpack_from('>18I', data, 0x90)
address, size = int(sys.argv[1], 16), int(sys.argv[2], 16)
for offset, base, extent in zip(offsets, addresses, sizes):
    virtual = 0x7F004000 if base == 0x80466000 else base
    if extent and virtual <= address and address + size <= virtual + extent:
        start = offset + address - virtual
        Path(sys.argv[3]).write_bytes(data[start:start + size])
        break
else:
    raise ValueError('Address outside original DOL')
