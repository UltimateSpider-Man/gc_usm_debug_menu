#!/usr/bin/env python3
"""Make a workspace-only GUTE52 boot disc with a supplied main DOL.

Direct executable boot does not run Dolphin's disc apploader and leaves this
game's FST uninitialized.  Preserve every source-disc byte, append the DOL at
a 32-byte boundary, and change only the boot-DOL offset in the copied header.
Never modify the supplied source ISO or DOL.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import struct
import tempfile


GC_DISC_SIZE = 1_459_978_240
GC_MAGIC = 0xC2339F3D
BOOT_DOL_OFFSET = 0x420
WORKSPACE_BUILD = Path(__file__).resolve().parents[1] / "build"


def read_dol(path: Path) -> bytes:
    data = path.read_bytes()
    if len(data) < 0x100:
        raise ValueError("DOL is shorter than its 0x100-byte header")
    offsets = struct.unpack_from(">18I", data, 0)
    addresses = struct.unpack_from(">18I", data, 0x48)
    sizes = struct.unpack_from(">18I", data, 0x90)
    for index, (offset, address, size) in enumerate(zip(offsets, addresses, sizes)):
        if not size:
            continue
        if offset < 0x100 or offset + size > len(data):
            raise ValueError(f"DOL section {index} exceeds the input file")
        if address < 0x80000000 or address + size > 0x81800000:
            raise ValueError(f"DOL section {index} exceeds GameCube MEM1")
    entrypoint = struct.unpack_from(">I", data, 0xE0)[0]
    if not any(size and address <= entrypoint < address + size
               for address, size in zip(addresses[:7], sizes[:7])):
        raise ValueError("DOL entrypoint is outside its text sections")
    return data


def make_disc(source: Path, dol: Path, output: Path, force: bool = False) -> dict:
    source, dol, output = source.resolve(), dol.resolve(), output.resolve()
    build = WORKSPACE_BUILD.resolve()
    if not output.is_relative_to(build):
        raise ValueError(f"validation output must be inside {build}")
    if output in (source, dol):
        raise ValueError("validation output must differ from both input paths")
    if output.exists() and (os.path.samefile(source, output) or os.path.samefile(dol, output)):
        raise ValueError("validation output aliases an input file")
    if output.exists() and not force:
        raise ValueError("output already exists; use --force to replace the validation copy")
    with source.open("rb") as stream:
        header = stream.read(0x440)
    if len(header) != 0x440 or header[:6] != b"GUTE52":
        raise ValueError("source disc must have the GUTE52 game ID")
    if struct.unpack_from(">I", header, 0x1C)[0] != GC_MAGIC:
        raise ValueError("source disc has an invalid GameCube magic word")
    source_size = source.stat().st_size
    dol_data = read_dol(dol)
    dol_offset = (source_size + 31) & ~31
    final_size = dol_offset + len(dol_data)
    if final_size > GC_DISC_SIZE:
        raise ValueError("appended DOL would exceed the GameCube disc capacity")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(prefix=output.name + ".", suffix=".tmp",
                                         dir=output.parent, delete=False) as destination:
            temporary = Path(destination.name)
            with source.open("rb") as original:
                shutil.copyfileobj(original, destination, 8 * 1024 * 1024)
            destination.write(b"\0" * (dol_offset - source_size))
            destination.write(dol_data)
            destination.seek(BOOT_DOL_OFFSET)
            destination.write(struct.pack(">I", dol_offset))
            destination.flush()
            os.fsync(destination.fileno())
        if temporary.stat().st_size != final_size:
            raise OSError("validation disc copy has an unexpected size")
        os.replace(temporary, output)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return {"output": str(output), "source_size": source_size,
            "dol_offset": dol_offset, "dol_size": len(dol_data),
            "dol_sha256": hashlib.sha256(dol_data).hexdigest(),
            "output_size": final_size}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="unmodified source GUTE52 ISO")
    parser.add_argument("--dol", type=Path, required=True, help="DOL to boot from the validation disc")
    parser.add_argument("--output", type=Path, default=WORKSPACE_BUILD / "dolphin-disc/validation.iso")
    parser.add_argument("--force", action="store_true", help="replace an existing workspace validation copy")
    args = parser.parse_args()
    try:
        result = make_disc(args.source, args.dol, args.output, args.force)
    except (OSError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")
    for key, value in result.items():
        print(f"{key}: {value:#x}" if isinstance(value, int) else f"{key}: {value}")


if __name__ == "__main__":
    main()
