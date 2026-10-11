#!/usr/bin/env python3
"""Validate and inject the GameCube debug-menu payload into a DOL."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple


TARGET_LOAD_ADDRESS = 0x80636040
MEM1_END = 0x81800000
DOL_TEXT_SLOT = 3
DOL_HEADER_SIZE = 0x100
EXPECTED_DOL_SHA256 = (
    "3dd0ad5ede2ef9df27a1aba1be9bf9cd5dfd7a14caa4bb8da396f2574c89565c"
)

DEFAULT_HOOK_SYMBOL = "usm_debug_menu_hook"
DEFAULT_UPDATE_HOOK_SYMBOL = "usm_debug_menu_update_hook"
DEFAULT_PAD_HOOK_SYMBOL = "usm_debug_menu_pad_read_hook"
DEFAULT_ARENA_SYMBOL = "usm_gc_arena_lo_wrapper"
DEFAULT_END_SYMBOL = "__payload_end"


class InjectionError(RuntimeError):
    pass


@dataclass(frozen=True)
class DolSection:
    kind: str
    slot: int
    file_offset: int
    address: int
    size: int

    @property
    def file_end(self) -> int:
        return self.file_offset + self.size

    @property
    def address_end(self) -> int:
        return self.address + self.size


@dataclass(frozen=True)
class LoadSegment:
    address: int
    file_data: bytes
    memory_size: int

    @property
    def address_end(self) -> int:
        return self.address + self.memory_size


@dataclass(frozen=True)
class PreparedPayload:
    image: bytes
    load_address: int
    end_address: int
    symbols: Mapping[str, int]


@dataclass(frozen=True)
class PatchSpec:
    name: str
    address: int
    expected_opcode: int
    target_symbol: str
    kind: str = "branch"


def progression_patches() -> Tuple[PatchSpec, ...]:
    manifest = Path(__file__).with_name("progression_bindings.json")
    try:
        records = json.loads(manifest.read_text(encoding="utf-8"))
        return tuple(PatchSpec(item["symbol"], int(item["address"], 0),
                               int(item["expected"], 0), item["symbol"], item["kind"])
                     for item in records)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        raise InjectionError(f"invalid progression binding manifest: {exc}") from exc


def read_u32_be(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise InjectionError(f"32-bit read outside file at 0x{offset:X}")
    return struct.unpack_from(">I", data, offset)[0]


def write_u32_be(data: bytearray, offset: int, value: int) -> None:
    if offset < 0 or offset + 4 > len(data):
        raise InjectionError(f"32-bit write outside file at 0x{offset:X}")
    if not 0 <= value <= 0xFFFFFFFF:
        raise InjectionError(f"value does not fit u32: 0x{value:X}")
    struct.pack_into(">I", data, offset, value)


def checked_range(offset: int, size: int, total: int, description: str) -> None:
    if offset < 0 or size < 0 or offset > total or size > total - offset:
        raise InjectionError(
            f"{description} range 0x{offset:X}+0x{size:X} exceeds 0x{total:X}"
        )


def ranges_overlap(start_a: int, end_a: int, start_b: int, end_b: int) -> bool:
    return start_a < end_b and start_b < end_a


def align_up(value: int, alignment: int) -> int:
    if alignment <= 0 or alignment & (alignment - 1):
        raise InjectionError(f"invalid alignment: {alignment}")
    return (value + alignment - 1) & ~(alignment - 1)


class DolImage:
    def __init__(self, data: bytes):
        if len(data) < DOL_HEADER_SIZE:
            raise InjectionError("DOL is smaller than its 0x100-byte header")
        self.data = data
        self.sections = self._parse_sections()
        self.bss_address = read_u32_be(data, 0xD8)
        self.bss_size = read_u32_be(data, 0xDC)
        self._validate_layout()

    def _parse_sections(self) -> List[DolSection]:
        sections: List[DolSection] = []
        layouts = (("text", 7, 0x00, 0x48, 0x90),
                   ("data", 11, 0x1C, 0x64, 0xAC))
        for kind, count, offsets_at, addresses_at, sizes_at in layouts:
            for slot in range(count):
                file_offset = read_u32_be(self.data, offsets_at + slot * 4)
                address = read_u32_be(self.data, addresses_at + slot * 4)
                size = read_u32_be(self.data, sizes_at + slot * 4)
                fields = (file_offset, address, size)
                if fields == (0, 0, 0):
                    continue
                if 0 in fields:
                    raise InjectionError(
                        f"partially populated DOL {kind} slot {slot}: {fields!r}"
                    )
                sections.append(DolSection(kind, slot, file_offset, address, size))
        return sections

    def _validate_layout(self) -> None:
        for section in self.sections:
            if section.file_offset < DOL_HEADER_SIZE:
                raise InjectionError(
                    f"{section.kind}{section.slot} overlaps the DOL header"
                )
            checked_range(
                section.file_offset,
                section.size,
                len(self.data),
                f"DOL {section.kind}{section.slot}",
            )
            if section.address_end > 0x100000000:
                raise InjectionError(
                    f"{section.kind}{section.slot} wraps the 32-bit address space"
                )

        for index, left in enumerate(self.sections):
            for right in self.sections[index + 1:]:
                if ranges_overlap(
                    left.file_offset, left.file_end,
                    right.file_offset, right.file_end,
                ):
                    raise InjectionError(
                        f"DOL file ranges overlap: {left.kind}{left.slot} and "
                        f"{right.kind}{right.slot}"
                    )
                if ranges_overlap(
                    left.address, left.address_end,
                    right.address, right.address_end,
                ):
                    raise InjectionError(
                        f"DOL load ranges overlap: {left.kind}{left.slot} and "
                        f"{right.kind}{right.slot}"
                    )

        if self.bss_size and self.bss_address + self.bss_size > 0x100000000:
            raise InjectionError("DOL BSS wraps the 32-bit address space")

    def text_slot(self, slot: int) -> Tuple[int, int, int]:
        if not 0 <= slot < 7:
            raise InjectionError(f"invalid DOL text slot {slot}")
        return (
            read_u32_be(self.data, 0x00 + slot * 4),
            read_u32_be(self.data, 0x48 + slot * 4),
            read_u32_be(self.data, 0x90 + slot * 4),
        )

    def address_to_file_offset(self, address: int, size: int = 1) -> int:
        if size <= 0 or address < 0 or address + size > 0x100000000:
            raise InjectionError(f"invalid DOL address range 0x{address:X}+0x{size:X}")
        matches = [
            section for section in self.sections
            if address >= section.address and address + size <= section.address_end
        ]
        if len(matches) != 1:
            raise InjectionError(
                f"DOL address 0x{address:08X}+0x{size:X} maps to "
                f"{len(matches)} sections"
            )
        section = matches[0]
        return section.file_offset + address - section.address


def parse_elf_symbols(data: bytes, section_headers: Sequence[Tuple[int, ...]]) -> Dict[str, int]:
    symbols: Dict[str, int] = {}
    for section_index, section in enumerate(section_headers):
        (_, section_type, _, _, offset, size, link, _, _, entry_size) = section
        if section_type not in (2, 11):  # SHT_SYMTAB, SHT_DYNSYM
            continue
        if entry_size < 16 or size % entry_size:
            raise InjectionError(f"malformed ELF symbol table section {section_index}")
        if link >= len(section_headers):
            raise InjectionError(f"ELF symbol table {section_index} has invalid sh_link")
        linked = section_headers[link]
        if linked[1] != 3:  # SHT_STRTAB
            raise InjectionError(f"ELF symbol table {section_index} is not linked to STRTAB")
        string_offset, string_size = linked[4], linked[5]
        checked_range(offset, size, len(data), f"ELF symbol table {section_index}")
        checked_range(string_offset, string_size, len(data), "ELF symbol strings")
        strings = data[string_offset:string_offset + string_size]

        for cursor in range(offset, offset + size, entry_size):
            name_offset, value, _, _, _, shndx = struct.unpack_from(">IIIBBH", data, cursor)
            if shndx == 0 or name_offset == 0:  # SHN_UNDEF or anonymous
                continue
            if name_offset >= len(strings):
                raise InjectionError("ELF symbol name offset exceeds its string table")
            terminator = strings.find(b"\0", name_offset)
            if terminator < 0:
                raise InjectionError("unterminated name in ELF symbol string table")
            name = strings[name_offset:terminator].decode("utf-8", errors="strict")
            previous = symbols.get(name)
            if previous is not None and previous != value:
                raise InjectionError(
                    f"ELF defines symbol {name!r} at both 0x{previous:X} and 0x{value:X}"
                )
            symbols[name] = value
    return symbols


def parse_elf32_be(data: bytes) -> Tuple[List[LoadSegment], Dict[str, int]]:
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise InjectionError("payload is not an ELF file")
    if data[4] != 1:
        raise InjectionError("payload ELF is not ELF32")
    if data[5] != 2:
        raise InjectionError("payload ELF is not big-endian")
    if data[6] != 1:
        raise InjectionError("payload ELF uses an unsupported ELF version")

    header = struct.unpack_from(">HHIIIIIHHHHHH", data, 16)
    (elf_type, machine, version, _, phoff, shoff, _, ehsize,
     phentsize, phnum, shentsize, shnum, _) = header
    if elf_type != 2:  # ET_EXEC
        raise InjectionError(f"payload ELF type is {elf_type}, expected ET_EXEC")
    if machine != 20:  # EM_PPC
        raise InjectionError(f"payload ELF machine is {machine}, expected PowerPC (20)")
    if version != 1 or ehsize < 52:
        raise InjectionError("malformed ELF32 header")
    if not phnum or phnum == 0xFFFF or phentsize < 32:
        raise InjectionError("ELF has no usable program-header table")
    checked_range(phoff, phentsize * phnum, len(data), "ELF program headers")

    segments: List[LoadSegment] = []
    for index in range(phnum):
        cursor = phoff + index * phentsize
        (segment_type, offset, vaddr, paddr, file_size, memory_size,
         _, alignment) = struct.unpack_from(">IIIIIIII", data, cursor)
        if segment_type != 1:  # PT_LOAD
            continue
        if file_size > memory_size:
            raise InjectionError(f"PT_LOAD {index} has p_filesz greater than p_memsz")
        checked_range(offset, file_size, len(data), f"ELF PT_LOAD {index}")
        if paddr not in (0, vaddr):
            raise InjectionError(
                f"PT_LOAD {index} has different p_vaddr/p_paddr; load address is ambiguous"
            )
        if vaddr + memory_size > 0x100000000:
            raise InjectionError(f"PT_LOAD {index} wraps the 32-bit address space")
        if alignment > 1:
            if alignment & (alignment - 1):
                raise InjectionError(f"PT_LOAD {index} has non-power-of-two alignment")
            if (vaddr - offset) % alignment:
                raise InjectionError(f"PT_LOAD {index} violates its alignment")
        if memory_size:
            segments.append(
                LoadSegment(vaddr, data[offset:offset + file_size], memory_size)
            )

    if not segments:
        raise InjectionError("ELF contains no non-empty PT_LOAD segments")
    segments.sort(key=lambda segment: segment.address)
    for left, right in zip(segments, segments[1:]):
        if left.address_end > right.address:
            raise InjectionError("ELF PT_LOAD memory ranges overlap")

    section_headers: List[Tuple[int, ...]] = []
    if shnum:
        if shnum == 0xFFFF or shentsize < 40:
            raise InjectionError("ELF has an unsupported section-header table")
        checked_range(shoff, shentsize * shnum, len(data), "ELF section headers")
        for index in range(shnum):
            section_headers.append(
                struct.unpack_from(">IIIIIIIIII", data, shoff + index * shentsize)
            )
    symbols = parse_elf_symbols(data, section_headers)
    return segments, symbols


_SYMBOL_NAME = re.compile(r"^[A-Za-z_.$][A-Za-z0-9_.$@]*$")
_HEX_VALUE = re.compile(r"^(?:0x)?[0-9A-Fa-f]{1,16}$")


def parse_symbol_file(path: Path) -> Dict[str, int]:
    try:
        text = path.read_text(encoding="utf-8", errors="strict")
    except (OSError, UnicodeError) as exc:
        raise InjectionError(f"cannot read symbol file {path}: {exc}") from exc

    symbols: Dict[str, int] = {}
    assignment = re.compile(
        r"^\s*([A-Za-z_.$][A-Za-z0-9_.$@]*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;?\s*$"
    )
    readelf = re.compile(
        r"^\s*\d+:\s+([0-9A-Fa-f]+)\s+\d+\s+\S+\s+\S+\s+\S+\s+\S+\s+(\S+)"
    )

    for line in text.splitlines():
        name: Optional[str] = None
        value: Optional[int] = None
        match = assignment.match(line)
        if match:
            name, raw_value = match.groups()
            value = int(raw_value, 16)
        else:
            match = readelf.match(line)
            if match:
                raw_value, name = match.groups()
                value = int(raw_value, 16)
            else:
                parts = line.strip().split()
                # GNU nm: ADDRESS TYPE NAME. GNU objdump: ADDRESS ... NAME.
                if len(parts) >= 3 and _HEX_VALUE.match(parts[0]):
                    candidate = parts[-1]
                    if _SYMBOL_NAME.match(candidate) and candidate != "*UND*":
                        name = candidate
                        value = int(parts[0], 16)
                elif len(parts) == 2 and _HEX_VALUE.match(parts[0]):
                    if _SYMBOL_NAME.match(parts[1]):
                        value = int(parts[0], 16)
                        name = parts[1]

        if name is None or value is None:
            continue
        if not 0 <= value <= 0xFFFFFFFF:
            raise InjectionError(f"symbol {name!r} is outside the 32-bit address space")
        previous = symbols.get(name)
        if previous is not None and previous != value:
            raise InjectionError(
                f"symbol file defines {name!r} at two different addresses"
            )
        symbols[name] = value

    if not symbols:
        raise InjectionError(f"no symbols recognized in {path}")
    return symbols


def merge_symbols(primary: Dict[str, int], extra: Mapping[str, int]) -> Dict[str, int]:
    merged = dict(primary)
    for name, value in extra.items():
        previous = merged.get(name)
        if previous is not None and previous != value:
            raise InjectionError(
                f"symbol {name!r} disagrees between ELF and external symbol file"
            )
        merged[name] = value
    return merged


def prepare_payload(
    payload_path: Path,
    payload_format: str,
    symbol_path: Optional[Path],
    load_address: int,
    required_symbols: Iterable[str],
    end_symbol: str,
) -> PreparedPayload:
    try:
        payload_data = payload_path.read_bytes()
    except OSError as exc:
        raise InjectionError(f"cannot read payload {payload_path}: {exc}") from exc
    if not payload_data:
        raise InjectionError("payload file is empty")

    is_elf = payload_data.startswith(b"\x7fELF")
    if payload_format == "elf" and not is_elf:
        raise InjectionError("--payload-format elf was selected for a non-ELF file")
    if payload_format == "binary" and is_elf:
        raise InjectionError("--payload-format binary was selected for an ELF file")

    symbols: Dict[str, int] = {}
    segments: Optional[List[LoadSegment]] = None
    if is_elf and payload_format != "binary":
        segments, symbols = parse_elf32_be(payload_data)
    if symbol_path is not None:
        symbols = merge_symbols(symbols, parse_symbol_file(symbol_path))

    missing = [name for name in required_symbols if name not in symbols]
    if end_symbol not in symbols:
        missing.append(end_symbol)
    if missing:
        raise InjectionError("missing payload symbols: " + ", ".join(sorted(set(missing))))

    end_address = symbols[end_symbol]
    if end_address <= load_address:
        raise InjectionError(
            f"{end_symbol} (0x{end_address:08X}) is not above payload base "
            f"0x{load_address:08X}"
        )
    if end_address > MEM1_END:
        raise InjectionError(
            f"payload ends at 0x{end_address:08X}, beyond MEM1 end 0x{MEM1_END:08X}"
        )

    image_size = end_address - load_address
    image = bytearray(image_size)
    if segments is None:
        if len(payload_data) > image_size:
            raise InjectionError("raw payload extends beyond __payload_end")
        image[:len(payload_data)] = payload_data
    else:
        if segments[0].address != load_address:
            raise InjectionError(
                f"first PT_LOAD starts at 0x{segments[0].address:08X}; "
                f"expected 0x{load_address:08X}"
            )
        for segment in segments:
            if segment.address < load_address or segment.address_end > end_address:
                raise InjectionError("ELF PT_LOAD lies outside payload start/__payload_end")
            offset = segment.address - load_address
            image[offset:offset + len(segment.file_data)] = segment.file_data

    for name in required_symbols:
        value = symbols[name]
        if value & 3:
            raise InjectionError(f"payload symbol {name!r} is not word-aligned")
        if not load_address <= value < end_address:
            raise InjectionError(f"payload symbol {name!r} lies outside the payload")

    return PreparedPayload(bytes(image), load_address, end_address, symbols)


def encode_relative_branch(source: int, target: int, link: bool = True) -> int:
    if source & 3 or target & 3:
        raise InjectionError("PowerPC branch source and target must be word-aligned")
    displacement = target - source
    if displacement < -0x02000000 or displacement > 0x01FFFFFC:
        raise InjectionError(
            f"branch 0x{source:08X} -> 0x{target:08X} is outside the +/-32 MiB range"
        )
    return 0x48000000 | (displacement & 0x03FFFFFC) | (1 if link else 0)


def normalize_sha256(value: str) -> str:
    normalized = value.strip().lower()
    if not re.fullmatch(r"[0-9a-f]{64}", normalized):
        raise argparse.ArgumentTypeError("SHA-256 must contain exactly 64 hexadecimal digits")
    return normalized


def integer_argument(value: str) -> int:
    try:
        parsed = int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid integer: {value!r}") from exc
    if not 0 <= parsed <= 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("integer must fit an unsigned 32-bit value")
    return parsed


def build_patched_dol(
    original: bytes,
    payload: PreparedPayload,
    patch_specs: Sequence[PatchSpec],
) -> Tuple[bytes, List[Tuple[PatchSpec, int]]]:
    dol = DolImage(original)
    if dol.text_slot(DOL_TEXT_SLOT) != (0, 0, 0):
        raise InjectionError(f"DOL text slot {DOL_TEXT_SLOT} is not empty")

    payload_start = payload.load_address
    payload_end = payload.end_address
    for section in dol.sections:
        if ranges_overlap(
            payload_start, payload_end, section.address, section.address_end
        ):
            raise InjectionError(
                f"payload overlaps DOL {section.kind}{section.slot} at "
                f"0x{section.address:08X}-0x{section.address_end:08X}"
            )
    if dol.bss_size and ranges_overlap(
        payload_start,
        payload_end,
        dol.bss_address,
        dol.bss_address + dol.bss_size,
    ):
        raise InjectionError("payload overlaps the DOL BSS")

    patch_offsets: List[Tuple[PatchSpec, int]] = []
    seen_addresses = set()
    for spec in patch_specs:
        if spec.address in seen_addresses or spec.address & 3:
            raise InjectionError(f"duplicate or unaligned patch address: 0x{spec.address:08X}")
        seen_addresses.add(spec.address)
        if spec.kind not in ("branch", "absolute"):
            raise InjectionError(f"invalid patch kind: {spec.kind!r}")
        target = payload.symbols.get(spec.target_symbol)
        if target is None or target & 3 or not payload_start <= target < payload_end:
            raise InjectionError(f"missing or invalid payload target: {spec.target_symbol}")
        file_offset = dol.address_to_file_offset(spec.address, 4)
        opcode = read_u32_be(original, file_offset)
        if opcode != spec.expected_opcode:
            raise InjectionError(
                f"{spec.name} opcode mismatch at 0x{spec.address:08X}: "
                f"expected 0x{spec.expected_opcode:08X}, found 0x{opcode:08X}"
            )
        patch_offsets.append((spec, file_offset))

    payload_file_offset = align_up(len(original), 0x20)
    if payload_file_offset > 0xFFFFFFFF or len(payload.image) > 0xFFFFFFFF:
        raise InjectionError("payload file offset/size does not fit the DOL header")

    patched = bytearray(original)
    patched.extend(b"\0" * (payload_file_offset - len(patched)))
    patched.extend(payload.image)

    write_u32_be(patched, 0x00 + DOL_TEXT_SLOT * 4, payload_file_offset)
    write_u32_be(patched, 0x48 + DOL_TEXT_SLOT * 4, payload.load_address)
    write_u32_be(patched, 0x90 + DOL_TEXT_SLOT * 4, len(payload.image))

    applied: List[Tuple[PatchSpec, int]] = []
    for spec, file_offset in patch_offsets:
        target = payload.symbols[spec.target_symbol]
        instruction = (target if spec.kind == "absolute" else
                       encode_relative_branch(spec.address, target, link=True))
        write_u32_be(patched, file_offset, instruction)
        applied.append((spec, instruction))

    verification = DolImage(bytes(patched))
    expected_slot = (payload_file_offset, payload.load_address, len(payload.image))
    if verification.text_slot(DOL_TEXT_SLOT) != expected_slot:
        raise InjectionError("internal verification of DOL text slot failed")
    if patched[payload_file_offset:payload_file_offset + len(payload.image)] != payload.image:
        raise InjectionError("internal verification of appended payload failed")
    for (spec, file_offset), (_, instruction) in zip(patch_offsets, applied):
        if read_u32_be(patched, file_offset) != instruction:
            raise InjectionError(f"internal verification of {spec.name} failed")

    return bytes(patched), applied


def ensure_distinct_paths(*paths: Path) -> None:
    resolved = [path.resolve() for path in paths]
    if len(set(resolved)) != len(resolved):
        raise InjectionError("input DOL, output DOL, and backup paths must be distinct")


def create_backup(source: Path, backup: Path, expected_hash: str) -> None:
    backup.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists():
        existing_hash = hashlib.sha256(backup.read_bytes()).hexdigest()
        if existing_hash != expected_hash:
            raise InjectionError(
                f"refusing to overwrite non-matching backup {backup} "
                f"(SHA-256 {existing_hash})"
            )
        return
    shutil.copy2(source, backup)
    copied_hash = hashlib.sha256(backup.read_bytes()).hexdigest()
    if copied_hash != expected_hash:
        raise InjectionError("backup verification failed")


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=path.name + ".", suffix=".tmp", dir=str(path.parent)
    )
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_name, path)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except OSError:
            pass
        raise


def make_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dol", required=True, type=Path, help="unmodified input DOL")
    parser.add_argument("--payload", required=True, type=Path, help="ELF32-BE or raw payload")
    parser.add_argument("--output", required=True, type=Path, help="patched output DOL")
    parser.add_argument("--backup", required=True, type=Path, help="verified original backup")
    parser.add_argument(
        "--symbols", type=Path,
        help="optional GNU nm/readelf/objdump symbol file; required for raw payloads",
    )
    parser.add_argument(
        "--payload-format", choices=("auto", "elf", "binary"), default="auto"
    )
    parser.add_argument(
        "--expected-sha256", type=normalize_sha256, default=EXPECTED_DOL_SHA256,
        help="required input revision hash (default: repository main.dol)",
    )
    parser.add_argument(
        "--load-address", type=integer_argument, default=TARGET_LOAD_ADDRESS
    )
    parser.add_argument("--hook-symbol", default=DEFAULT_HOOK_SYMBOL)
    parser.add_argument("--update-hook-symbol", default=DEFAULT_UPDATE_HOOK_SYMBOL)
    parser.add_argument("--pad-hook-symbol", default=DEFAULT_PAD_HOOK_SYMBOL)
    parser.add_argument("--arena-symbol", default=DEFAULT_ARENA_SYMBOL)
    parser.add_argument("--end-symbol", default=DEFAULT_END_SYMBOL)
    parser.add_argument("--dry-run", action="store_true", help="validate without writing files")
    return parser


def run(arguments: argparse.Namespace) -> None:
    ensure_distinct_paths(arguments.dol, arguments.output, arguments.backup)
    try:
        original = arguments.dol.read_bytes()
    except OSError as exc:
        raise InjectionError(f"cannot read input DOL {arguments.dol}: {exc}") from exc

    actual_hash = hashlib.sha256(original).hexdigest()
    if actual_hash != arguments.expected_sha256:
        raise InjectionError(
            "input DOL SHA-256 mismatch:\n"
            f"  expected: {arguments.expected_sha256}\n"
            f"  actual:   {actual_hash}"
        )

    progression = progression_patches()
    required_symbols = (
        arguments.hook_symbol,
        arguments.update_hook_symbol,
        arguments.pad_hook_symbol,
        arguments.arena_symbol,
    ) + tuple(spec.target_symbol for spec in progression)
    payload = prepare_payload(
        arguments.payload,
        arguments.payload_format,
        arguments.symbols,
        arguments.load_address,
        required_symbols,
        arguments.end_symbol,
    )
    if payload.symbols[arguments.arena_symbol] != arguments.load_address:
        raise InjectionError(
            f"{arguments.arena_symbol} must be first at 0x{arguments.load_address:08X}"
        )

    patches = (
        PatchSpec("debug-menu hook", 0x8016E55C, 0x4812636D, arguments.hook_symbol),
        PatchSpec(
            "deferred update hook",
            0x8017BEE0,
            0x4BFC7909,
            arguments.update_hook_symbol,
        ),
        PatchSpec(
            "exclusive-input PADRead hook",
            0x801ACCC8,
            0x48165239,
            arguments.pad_hook_symbol,
        ),
        PatchSpec("OSSetArenaLo call 1", 0x802F39B8, 0x480012A1, arguments.arena_symbol),
        PatchSpec("OSSetArenaLo call 2", 0x802F39F4, 0x48001265, arguments.arena_symbol),
    )
    patched, applied = build_patched_dol(original, payload, patches + progression)

    print(f"Input SHA-256: {actual_hash}")
    print(
        f"Payload: 0x{payload.load_address:08X}-0x{payload.end_address:08X} "
        f"({len(payload.image)} bytes)"
    )
    for spec, instruction in applied:
        target = payload.symbols[spec.target_symbol]
        print(
            f"Patch {spec.name}: 0x{spec.address:08X} -> 0x{target:08X} "
            f"(0x{instruction:08X})"
        )

    if arguments.dry_run:
        print("Dry run complete; no files written.")
        return

    create_backup(arguments.dol, arguments.backup, actual_hash)
    atomic_write(arguments.output, patched)
    output_hash = hashlib.sha256(patched).hexdigest()
    print(f"Backup: {arguments.backup}")
    print(f"Patched DOL: {arguments.output}")
    print(f"Output SHA-256: {output_hash}")


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = make_argument_parser()
    arguments = parser.parse_args(argv)
    try:
        run(arguments)
    except (InjectionError, OSError, struct.error, UnicodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
