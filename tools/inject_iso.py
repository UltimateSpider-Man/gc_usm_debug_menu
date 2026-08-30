#!/usr/bin/env python3
"""Replace main.dol in the compact GUTE52 test ISO and relocate/repair its FST."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import struct
import tempfile


ISO_SIZE = 0x4BCD0000
EXPECTED_GAME_ID = b"GUTE52"
EXPECTED_DOL_OFFSET = 0x202A0
EXPECTED_FST_SIZE = 0x6AB8
EXPECTED_FST_MAX_SIZE = 0x6AB8
EXPECTED_ENTRY_COUNT = 861
EXPECTED_DIRECTORY_COUNT = 9
EXPECTED_FILE_COUNT = 852
EXPECTED_MIN_FILE_OFFSET = 0x590000
EXPECTED_MAX_FILE_END = 0x4BCCBF60
EXPECTED_DOL_ENTRY = 0x80626000
LEGACY_FST_SHA256 = "B76500342E866E65F9B609FFE252460C2BD08D1C96FFF8CECC4CFF098AA9F316"
REPAIRED_FST_SHA256 = "063BD221F64C293AA6CB6A4F52605AC985518B53A8419F94DC5967D31A0AF4D2"

DIRECTORY_ASSERTIONS = {
    0: ("", 0, 861),
    1: ("movies", 0, 13),
    2: ("gc", 1, 13),
    13: ("packs", 0, 639),
    14: ("gc", 13, 635),
    639: ("sound", 0, 856),
    640: ("gc", 639, 856),
    641: ("city_arena", 640, 810),
    810: ("streams", 640, 856),
}

LEGACY_PARENTS = {14: 1, 640: 1, 641: 2, 810: 2}
REPAIRED_PARENTS = {14: 13, 640: 639, 641: 640, 810: 640}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def be32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def dol_extent(dol: bytes) -> tuple[int, int]:
    if len(dol) < 0x100:
        raise ValueError("DOL is shorter than its 0x100-byte header")
    sections: list[tuple[int, int]] = []
    for index in range(7):
        sections.append((be32(dol, index * 4), be32(dol, 0x90 + index * 4)))
    for index in range(11):
        sections.append((be32(dol, 0x1C + index * 4), be32(dol, 0xAC + index * 4)))
    extent = 0x100
    for file_offset, size in sections:
        if not size:
            continue
        if file_offset < 0x100 or file_offset + size > len(dol):
            raise ValueError(
                f"DOL section 0x{file_offset:X}+0x{size:X} is outside the file"
            )
        extent = max(extent, file_offset + size)
    entry = be32(dol, 0xE0)
    if extent != len(dol):
        raise ValueError(
            f"DOL length 0x{len(dol):X} does not equal declared extent 0x{extent:X}"
        )
    if entry != EXPECTED_DOL_ENTRY:
        raise ValueError(f"unexpected DOL entry 0x{entry:08X}")
    return extent, entry


def fst_name(blob: bytes | bytearray, entry_count: int, name_offset: int) -> str:
    strings_offset = entry_count * 12
    start = strings_offset + name_offset
    if start < strings_offset or start >= len(blob):
        raise ValueError(f"FST name offset 0x{name_offset:X} is outside the string table")
    end = blob.find(b"\0", start)
    if end < 0:
        raise ValueError("unterminated FST name")
    return bytes(blob[start:end]).decode("ascii")


def parse_fst(blob: bytes | bytearray) -> list[dict[str, int | str | bool]]:
    if len(blob) != EXPECTED_FST_SIZE:
        raise ValueError(f"unexpected FST size 0x{len(blob):X}")
    root_word = be32(blob, 0)
    entry_count = be32(blob, 8)
    if (root_word >> 24) != 1 or entry_count != EXPECTED_ENTRY_COUNT:
        raise ValueError("unexpected FST root entry")
    if entry_count * 12 >= len(blob):
        raise ValueError("FST entry table overlaps or consumes the string table")
    entries: list[dict[str, int | str | bool]] = []
    for index in range(entry_count):
        offset = index * 12
        word = be32(blob, offset)
        is_dir = (word >> 24) != 0
        entries.append(
            {
                "index": index,
                "directory": is_dir,
                "name": "" if index == 0 else fst_name(blob, entry_count, word & 0x00FFFFFF),
                "second": be32(blob, offset + 4),
                "third": be32(blob, offset + 8),
            }
        )
    return entries


def assert_directory_layout(entries: list[dict[str, int | str | bool]], repaired: bool) -> None:
    expected_parents = REPAIRED_PARENTS if repaired else LEGACY_PARENTS
    directories = [entry for entry in entries if entry["directory"]]
    if len(directories) != EXPECTED_DIRECTORY_COUNT:
        raise ValueError(f"unexpected directory count {len(directories)}")
    for index, (name, repaired_parent, next_index) in DIRECTORY_ASSERTIONS.items():
        entry = entries[index]
        expected_parent = expected_parents.get(index, repaired_parent)
        actual = (entry["name"], entry["second"], entry["third"])
        expected = (name, expected_parent, next_index)
        if not entry["directory"] or actual != expected:
            raise ValueError(f"unexpected directory entry {index}: {actual!r}")


def validate_repaired_fst(blob: bytes | bytearray) -> tuple[int, int]:
    entries = parse_fst(blob)
    assert_directory_layout(entries, repaired=True)
    stack: list[tuple[int, int]] = [(0, EXPECTED_ENTRY_COUNT)]
    files = 0
    min_file_offset = ISO_SIZE
    max_file_end = 0
    for entry in entries[1:]:
        index = int(entry["index"])
        while stack and index >= stack[-1][1]:
            stack.pop()
        if not stack:
            raise ValueError(f"FST entry {index} is outside the root directory")
        if entry["directory"]:
            parent = int(entry["second"])
            next_index = int(entry["third"])
            if parent != stack[-1][0]:
                raise ValueError(
                    f"directory {index} parent {parent} does not match {stack[-1][0]}"
                )
            if not (index < next_index <= stack[-1][1]):
                raise ValueError(f"directory {index} has invalid next index {next_index}")
            stack.append((index, next_index))
        else:
            files += 1
            file_offset = int(entry["second"])
            file_end = file_offset + int(entry["third"])
            if file_end > ISO_SIZE:
                raise ValueError(f"file entry {index} extends beyond the ISO")
            min_file_offset = min(min_file_offset, file_offset)
            max_file_end = max(max_file_end, file_end)
    if files != EXPECTED_FILE_COUNT:
        raise ValueError(f"unexpected file count {files}")
    if min_file_offset != EXPECTED_MIN_FILE_OFFSET:
        raise ValueError(f"unexpected first file offset 0x{min_file_offset:X}")
    if max_file_end != EXPECTED_MAX_FILE_END:
        raise ValueError(f"unexpected maximum file end 0x{max_file_end:X}")
    return min_file_offset, max_file_end


def repaired_fst(source_fst: bytes) -> bytes:
    source_hash = sha256(source_fst)
    if source_hash not in (LEGACY_FST_SHA256, REPAIRED_FST_SHA256):
        raise ValueError(f"unsupported source FST SHA-256 {source_hash}")
    entries = parse_fst(source_fst)
    assert_directory_layout(entries, repaired=source_hash == REPAIRED_FST_SHA256)
    result = bytearray(source_fst)
    for index, parent in REPAIRED_PARENTS.items():
        struct.pack_into(">I", result, index * 12 + 4, parent)
    result_hash = sha256(result)
    if result_hash != REPAIRED_FST_SHA256:
        raise ValueError(f"repaired FST has unexpected SHA-256 {result_hash}")
    validate_repaired_fst(result)
    return bytes(result)


def read_iso_layout(path: Path) -> tuple[int, int, int, int, bytes]:
    if path.stat().st_size != ISO_SIZE:
        raise ValueError(f"unexpected ISO length 0x{path.stat().st_size:X}")
    with path.open("rb") as stream:
        header = stream.read(0x430)
        if header[:6] != EXPECTED_GAME_ID:
            raise ValueError(f"unexpected game ID {header[:6]!r}")
        dol_offset = be32(header, 0x420)
        fst_offset = be32(header, 0x424)
        fst_size = be32(header, 0x428)
        fst_max_size = be32(header, 0x42C)
        if dol_offset != EXPECTED_DOL_OFFSET:
            raise ValueError(f"unexpected DOL offset 0x{dol_offset:X}")
        if fst_size != EXPECTED_FST_SIZE or fst_max_size != EXPECTED_FST_MAX_SIZE:
            raise ValueError("unexpected FST size fields")
        if fst_offset + fst_size > ISO_SIZE:
            raise ValueError("source FST is outside the ISO")
        stream.seek(fst_offset)
        fst = stream.read(fst_size)
    if len(fst) != fst_size:
        raise ValueError("could not read the complete source FST")
    return dol_offset, fst_offset, fst_size, fst_max_size, fst


def validate_output(path: Path, dol: bytes, expected_fst_offset: int) -> None:
    dol_offset, fst_offset, fst_size, fst_max_size, fst = read_iso_layout(path)
    if fst_offset != expected_fst_offset:
        raise ValueError(f"output FST offset is 0x{fst_offset:X}, expected 0x{expected_fst_offset:X}")
    if fst_size != fst_max_size:
        raise ValueError("output FST size fields differ")
    if sha256(fst) != REPAIRED_FST_SHA256:
        raise ValueError("output FST hash does not match the repaired table")
    first_file, _ = validate_repaired_fst(fst)
    with path.open("rb") as stream:
        stream.seek(dol_offset)
        embedded_dol = stream.read(len(dol))
    if sha256(embedded_dol) != sha256(dol):
        raise ValueError("embedded DOL hash does not match the standalone DOL")
    extent, _ = dol_extent(embedded_dol)
    dol_end = dol_offset + extent
    fst_end = fst_offset + fst_size
    if not (dol_end <= fst_offset and fst_end <= first_file):
        raise ValueError("DOL, FST, or first game file overlaps")


def inject(source_iso: Path, dol_path: Path, output_iso: Path) -> None:
    source_iso = source_iso.resolve()
    dol_path = dol_path.resolve()
    output_iso = output_iso.resolve()
    if not source_iso.is_file() or not dol_path.is_file():
        raise ValueError("source ISO and DOL must both exist")
    dol = dol_path.read_bytes()
    extent, entry = dol_extent(dol)
    dol_offset, old_fst_offset, fst_size, _, source_fst = read_iso_layout(source_iso)
    fst = repaired_fst(source_fst)
    first_file, _ = validate_repaired_fst(fst)
    new_fst_offset = align_up(dol_offset + extent, 0x20)
    if new_fst_offset + fst_size > first_file:
        raise ValueError("new DOL and relocated FST do not fit before the first game file")

    output_iso.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{output_iso.name}.", suffix=".tmp", dir=output_iso.parent
    )
    os.close(descriptor)
    temporary_path = Path(temporary_name)
    try:
        shutil.copyfile(source_iso, temporary_path)
        with temporary_path.open("r+b") as stream:
            stream.seek(dol_offset)
            stream.write(dol)
            padding = new_fst_offset - (dol_offset + len(dol))
            if padding:
                stream.write(b"\0" * padding)
            stream.seek(new_fst_offset)
            stream.write(fst)
            stream.seek(0x424)
            stream.write(struct.pack(">I", new_fst_offset))
            stream.flush()
            os.fsync(stream.fileno())
        validate_output(temporary_path, dol, new_fst_offset)
        os.replace(temporary_path, output_iso)
    finally:
        if temporary_path.exists():
            temporary_path.unlink()

    print(f"Input ISO:  {source_iso}")
    print(f"DOL:        {dol_path}  0x{len(dol):X} bytes  SHA-256 {sha256(dol)}")
    print(f"DOL range:  0x{dol_offset:X}..0x{dol_offset + extent:X}; entry 0x{entry:08X}")
    print(f"FST move:   0x{old_fst_offset:X} -> 0x{new_fst_offset:X}; repaired SHA-256 {sha256(fst)}")
    print(f"Output ISO: {output_iso}  SHA-256 {hash_file(output_iso)}")
    print("Verification: passed; DOL/FST/files do not overlap")


def hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_iso", type=Path)
    parser.add_argument("dol", type=Path)
    parser.add_argument("output_iso", type=Path)
    args = parser.parse_args()
    inject(args.source_iso, args.dol, args.output_iso)


if __name__ == "__main__":
    main()
