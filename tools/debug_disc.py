#!/usr/bin/env python3
"""Build a workspace ISO with the debug DOL and a reserved writable game.ini."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import tempfile

from game_ini import update_scene
from inject_gamecube_payload import atomic_write
from make_validation_disc import GC_DISC_SIZE, GC_MAGIC, WORKSPACE_BUILD, read_dol


def ini_entry(stream) -> tuple[int, int, int]:
    """Return the on-disc FST entry position, data offset, and size."""
    stream.seek(0, 2)
    total = stream.tell()
    stream.seek(0)
    header = stream.read(0x440)
    if len(header) != 0x440 or header[:6] != b"GUTE52":
        raise ValueError("disc must be the supported GUTE52 revision")
    if struct.unpack_from(">I", header, 0x1C)[0] != GC_MAGIC:
        raise ValueError("invalid GameCube disc magic")
    fst, size = struct.unpack_from(">II", header, 0x424)
    if fst < 0x440 or size < 12 or size > 16 * 1024 * 1024 or fst + size > total:
        raise ValueError("disc FST is outside the image")
    stream.seek(fst)
    data = stream.read(size)
    root_type, parent, count = struct.unpack_from(">III", data)
    if root_type >> 24 != 1 or parent != 0 or count < 1 or count * 12 > size:
        raise ValueError("invalid root FST directory")
    strings = data[count * 12:]
    stack = [(count, "", 0)]
    found = []
    for index in range(1, count):
        while index >= stack[-1][0]:
            stack.pop()
        kind_name, offset, length = struct.unpack_from(">III", data, index * 12)
        kind, name_at = kind_name >> 24, kind_name & 0xFFFFFF
        end = strings.find(b"\0", name_at)
        if name_at >= len(strings) or end < 0 or kind not in (0, 1):
            raise ValueError("invalid FST name/type")
        name = strings[name_at:end].decode("ascii")
        if not name or name in (".", "..") or "/" in name or "\\" in name:
            raise ValueError("invalid FST filename")
        path = stack[-1][1] + name
        if kind:
            # Some repacked USM discs store a directory ordinal in the parent
            # field. Dolphin traverses the subtree end indices; use those for
            # paths while still rejecting forward/out-of-range parent data.
            if offset >= index or not index < length <= stack[-1][0]:
                raise ValueError("invalid FST directory hierarchy")
            stack.append((length, path + "/", index))
        else:
            if offset < 0x440 or offset + length > total:
                raise ValueError("FST file exceeds the disc image")
            if path.lower() == "game.ini":
                found.append((fst + index * 12, offset, length))
    if len(found) != 1:
        raise ValueError("disc must contain exactly one root game.ini")
    return found[0]


def make_debug_disc(source: Path, dol: Path, ini: Path, output: Path,
                    force: bool = False) -> dict:
    source, dol, ini, output = (p.resolve() for p in (source, dol, ini, output))
    sidecar = output.with_suffix(output.suffix + ".usm.json").resolve()
    if not output.is_relative_to(WORKSPACE_BUILD.resolve()):
        raise ValueError("debug disc output must be inside the workspace build directory")
    for artifact in (output, sidecar):
        if artifact in (source, dol, ini) or (artifact.exists() and any(
                os.path.samefile(artifact, p) for p in (source, dol, ini))):
            raise ValueError("output and manifest must not alias an input file")
    if (output.exists() or sidecar.exists()) and not force:
        raise ValueError("debug disc already exists; use --force to replace its copy")
    ini_data = ini.read_bytes()
    # Validate the format without changing the selected scene.
    update_scene(ini_data, "city")
    if not ini_data or len(ini_data) > 1024 * 1024:
        raise ValueError("invalid game.ini size")
    dol_data = read_dol(dol)
    with source.open("rb") as stream:
        entry, _, _ = ini_entry(stream)
    dol_at = (source.stat().st_size + 31) & ~31
    ini_at = (dol_at + len(dol_data) + 31) & ~31
    capacity = (max(len(ini_data) + 256, 8192) + 31) & ~31
    final_size = ini_at + capacity
    if final_size > GC_DISC_SIZE:
        raise ValueError("debug DOL and INI would exceed GameCube disc capacity")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix=output.name + ".", suffix=".tmp",
                                         dir=output.parent, delete=False) as stream:
            temporary = Path(stream.name)
            with source.open("rb") as original:
                shutil.copyfileobj(original, stream, 8 * 1024 * 1024)
            stream.write(b"\0" * (dol_at - stream.tell()))
            stream.write(dol_data)
            stream.write(b"\0" * (ini_at - stream.tell()))
            stream.write(ini_data.ljust(capacity, b"\0"))
            stream.seek(0x420)
            stream.write(struct.pack(">I", dol_at))
            stream.seek(entry + 4)
            stream.write(struct.pack(">II", ini_at, capacity))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, output)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    manifest = {"version": 1, "source": str(source), "iso": str(output),
                "game_ini": str(ini), "dol_offset": dol_at, "ini_entry": entry,
                "ini_offset": ini_at, "ini_capacity": capacity,
                "output_size": final_size}
    atomic_write(sidecar,
                 json.dumps(manifest, indent=2).encode() + b"\n")
    return manifest


def write_disc_ini(iso: Path, data: bytes) -> None:
    iso = iso.resolve()
    if not iso.is_relative_to(WORKSPACE_BUILD.resolve()):
        raise ValueError("only a workspace debug ISO may be updated")
    manifest = json.loads(iso.with_suffix(iso.suffix + ".usm.json").read_text())
    if manifest["version"] != 1 or Path(manifest["iso"]).resolve() != iso:
        raise ValueError("debug disc manifest does not match the ISO")
    if Path(manifest["source"]).resolve() == iso:
        raise ValueError("refusing to update the source disc")
    with iso.open("r+b") as stream:
        entry, offset, capacity = ini_entry(stream)
        if (entry, offset, capacity) != (manifest["ini_entry"], manifest["ini_offset"],
                                         manifest["ini_capacity"]):
            raise ValueError("debug ISO game.ini extent changed")
        stream.seek(0, 2)
        if stream.tell() != manifest["output_size"]:
            raise ValueError("debug ISO size changed")
        stream.seek(0x420)
        if struct.unpack(">I", stream.read(4))[0] != manifest["dol_offset"]:
            raise ValueError("debug ISO boot DOL changed")
        if len(data) > capacity:
            raise ValueError("game.ini exceeds its reserved disc extent")
        stream.seek(offset)
        stream.write(data.ljust(capacity, b"\0"))
        stream.flush()
        os.fsync(stream.fileno())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--dol", type=Path, required=True)
    parser.add_argument("--game-ini", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=WORKSPACE_BUILD / "debug-disc/usm-debug.iso")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    try:
        print(json.dumps(make_debug_disc(args.source, args.dol, args.game_ini, args.output,
                                         args.force), indent=2))
    except (OSError, ValueError, KeyError, struct.error) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    main()
