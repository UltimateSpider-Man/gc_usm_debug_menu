#!/usr/bin/env python3
"""Inject the staged GameCube debug-menu image into the verified GUTE52 DOL.

The source DOL is treated as immutable.  A bootstrap plus final-address-linked
payload is appended as text section 3 below the retail startup stack.  The DOL
entry runs the bootstrap, which copies the payload to a reserved top-of-MEM1
region.  OSInit's existing 0x81780000 ArenaHi fallback is forced so its cold
arena clear and heap cannot overwrite that copy.  The inert render call is
then replaced with a branch-and-link to the reserved payload.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from dataclasses import dataclass
from pathlib import Path
import struct
import sys
import tempfile


EXPECTED_INPUT_SHA256 = (
    "3DD0AD5EDE2EF9DF27A1ABA1BE9BF9CD5DFD7A14CAA4BB8DA396F2574C89565C"
)

DOL_HEADER_SIZE = 0x100
SECTION_COUNT = 18
TEXT_SECTION_COUNT = 7
TEXT_SLOT_INDEX = 3
SECTION_FILE_OFFSETS_OFFSET = 0x00
SECTION_ADDRESSES_OFFSET = 0x48
SECTION_SIZES_OFFSET = 0x90
BSS_ADDRESS_OFFSET = 0xD8
BSS_SIZE_OFFSET = 0xDC
ENTRY_POINT_OFFSET = 0xE0

HOOK_SIGNATURE_ADDRESS = 0x8016E554
HOOK_SIGNATURE = bytes.fromhex("7F E3 FB 78 48 01 06 85 48 12 63 6D")
HOOK_ADDRESS = 0x8016E558
EXPECTED_HOOK_WORD = 0x48010685

ARENA_HI_BRANCH_ADDRESS = 0x802F3A04
EXPECTED_ARENA_HI_BRANCH_WORD = 0x40820010
PATCHED_ARENA_HI_BRANCH_WORD = 0x60000000

STAGING_ADDRESS = 0x80626000
STAGING_LIMIT = 0x80636000
PAYLOAD_ADDRESS = 0x81780000
EXPECTED_ENTRY_POINT = 0x80003100
MEM1_END = 0x81800000
MAX_STAGED_SIZE = STAGING_LIMIT - STAGING_ADDRESS
FILE_ALIGNMENT = 0x20


class InjectionError(RuntimeError):
    """Raised when an input invariant or post-write check fails."""


@dataclass(frozen=True)
class DolSection:
    index: int
    file_offset: int
    address: int
    size: int

    @property
    def kind(self) -> str:
        return "text" if self.index < TEXT_SECTION_COUNT else "data"

    @property
    def kind_index(self) -> int:
        if self.index < TEXT_SECTION_COUNT:
            return self.index
        return self.index - TEXT_SECTION_COUNT

    @property
    def name(self) -> str:
        return f"{self.kind}{self.kind_index}"

    @property
    def file_end(self) -> int:
        return self.file_offset + self.size

    @property
    def address_end(self) -> int:
        return self.address + self.size


@dataclass(frozen=True)
class DolImage:
    data: bytes
    sections: tuple[DolSection, ...]
    bss_address: int
    bss_size: int

    def section(self, index: int) -> DolSection:
        return self.sections[index]

    def runtime_to_file(self, address: int, size: int = 1) -> int:
        if size < 0:
            raise InjectionError(f"negative mapping size: {size}")
        matches = [
            section
            for section in self.sections
            if section.size
            and section.address <= address
            and address + size <= section.address_end
        ]
        if len(matches) != 1:
            names = ", ".join(section.name for section in matches) or "none"
            raise InjectionError(
                f"runtime range 0x{address:08X}..0x{address + size:08X} "
                f"maps to {len(matches)} sections ({names})"
            )
        section = matches[0]
        return section.file_offset + (address - section.address)

    def read_runtime(self, address: int, size: int) -> bytes:
        file_offset = self.runtime_to_file(address, size)
        return self.data[file_offset : file_offset + size]


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def align_up(value: int, alignment: int) -> int:
    if alignment <= 0 or alignment & (alignment - 1):
        raise InjectionError(f"alignment must be a positive power of two: {alignment}")
    return (value + alignment - 1) & -alignment


def ranges_overlap(start_a: int, end_a: int, start_b: int, end_b: int) -> bool:
    return start_a < end_b and start_b < end_a


def parse_dol(data: bytes) -> DolImage:
    if len(data) < DOL_HEADER_SIZE:
        raise InjectionError(
            f"DOL is only 0x{len(data):X} bytes; header requires 0x{DOL_HEADER_SIZE:X}"
        )

    file_offsets = struct.unpack_from(f">{SECTION_COUNT}I", data, SECTION_FILE_OFFSETS_OFFSET)
    addresses = struct.unpack_from(f">{SECTION_COUNT}I", data, SECTION_ADDRESSES_OFFSET)
    sizes = struct.unpack_from(f">{SECTION_COUNT}I", data, SECTION_SIZES_OFFSET)
    sections = tuple(
        DolSection(index, file_offset, address, size)
        for index, (file_offset, address, size) in enumerate(
            zip(file_offsets, addresses, sizes)
        )
    )

    for section in sections:
        fields = (section.file_offset, section.address, section.size)
        if section.size == 0:
            if section.file_offset != 0 or section.address != 0:
                raise InjectionError(
                    f"empty {section.name} has nonzero header fields: "
                    f"offset=0x{section.file_offset:X}, address=0x{section.address:X}"
                )
            continue
        if section.file_offset < DOL_HEADER_SIZE:
            raise InjectionError(
                f"{section.name} starts inside the DOL header at 0x{section.file_offset:X}"
            )
        if section.file_end > len(data):
            raise InjectionError(
                f"{section.name} extends past EOF: 0x{section.file_end:X} > 0x{len(data):X}"
            )
        if section.address_end > 0x1_0000_0000:
            raise InjectionError(f"{section.name} wraps the 32-bit address space")
        if not any(fields):
            raise InjectionError(f"invalid zeroed nonempty section {section.name}")

    populated = [section for section in sections if section.size]
    for left_index, left in enumerate(populated):
        for right in populated[left_index + 1 :]:
            if ranges_overlap(
                left.file_offset, left.file_end, right.file_offset, right.file_end
            ):
                raise InjectionError(
                    f"file ranges for {left.name} and {right.name} overlap"
                )
            if ranges_overlap(
                left.address, left.address_end, right.address, right.address_end
            ):
                raise InjectionError(
                    f"runtime ranges for {left.name} and {right.name} overlap"
                )

    bss_address, bss_size = struct.unpack_from(">2I", data, BSS_ADDRESS_OFFSET)
    if bss_address + bss_size > 0x1_0000_0000:
        raise InjectionError("BSS wraps the 32-bit address space")

    return DolImage(data, sections, bss_address, bss_size)


def encode_relative_bl(source_address: int, target_address: int) -> int:
    if source_address & 3 or target_address & 3:
        raise InjectionError(
            f"branch endpoints must be four-byte aligned: "
            f"0x{source_address:08X} -> 0x{target_address:08X}"
        )
    displacement = target_address - source_address
    if displacement < -0x02000000 or displacement > 0x01FFFFFC:
        raise InjectionError(
            f"branch displacement {displacement:+#x} is outside PPC relative range"
        )
    return 0x48000001 | (displacement & 0x03FFFFFC)


def decode_relative_bl(source_address: int, instruction: int) -> int:
    if instruction & 0xFC000003 != 0x48000001:
        raise InjectionError(f"0x{instruction:08X} is not a relative PPC BL instruction")
    displacement = instruction & 0x03FFFFFC
    if displacement & 0x02000000:
        displacement -= 0x04000000
    return (source_address + displacement) & 0xFFFFFFFF


def validate_source(source_data: bytes) -> DolImage:
    actual_hash = sha256(source_data)
    if actual_hash != EXPECTED_INPUT_SHA256:
        raise InjectionError(
            "unsupported input DOL: "
            f"SHA-256 is {actual_hash}, expected {EXPECTED_INPUT_SHA256} (GUTE52)"
        )

    dol = parse_dol(source_data)
    text3 = dol.section(TEXT_SLOT_INDEX)
    if (text3.file_offset, text3.address, text3.size) != (0, 0, 0):
        raise InjectionError(
            "DOL text slot 3 is already occupied: "
            f"offset=0x{text3.file_offset:X}, address=0x{text3.address:08X}, "
            f"size=0x{text3.size:X}"
        )

    actual_signature = dol.read_runtime(
        HOOK_SIGNATURE_ADDRESS, len(HOOK_SIGNATURE)
    )
    if actual_signature != HOOK_SIGNATURE:
        raise InjectionError(
            f"hook signature mismatch at 0x{HOOK_SIGNATURE_ADDRESS:08X}: "
            f"found {actual_signature.hex(' ').upper()}, "
            f"expected {HOOK_SIGNATURE.hex(' ').upper()}"
        )

    hook_word = struct.unpack(">I", dol.read_runtime(HOOK_ADDRESS, 4))[0]
    if hook_word != EXPECTED_HOOK_WORD:
        raise InjectionError(
            f"hook word mismatch at 0x{HOOK_ADDRESS:08X}: "
            f"found 0x{hook_word:08X}, expected 0x{EXPECTED_HOOK_WORD:08X}"
        )

    arena_hi_branch_word = struct.unpack(
        ">I", dol.read_runtime(ARENA_HI_BRANCH_ADDRESS, 4)
    )[0]
    if arena_hi_branch_word != EXPECTED_ARENA_HI_BRANCH_WORD:
        raise InjectionError(
            f"ArenaHi branch mismatch at 0x{ARENA_HI_BRANCH_ADDRESS:08X}: "
            f"found 0x{arena_hi_branch_word:08X}, "
            f"expected 0x{EXPECTED_ARENA_HI_BRANCH_WORD:08X}"
        )
    entry_point = struct.unpack_from(">I", source_data, ENTRY_POINT_OFFSET)[0]
    if entry_point != EXPECTED_ENTRY_POINT:
        raise InjectionError(
            f"entry-point mismatch: found 0x{entry_point:08X}, "
            f"expected 0x{EXPECTED_ENTRY_POINT:08X}"
        )
    return dol


def validate_payload(payload: bytes, dol: DolImage) -> None:
    if not payload:
        raise InjectionError("payload is empty")
    if len(payload) > MAX_STAGED_SIZE:
        raise InjectionError(
            f"staged image is 0x{len(payload):X} bytes; "
            f"maximum is 0x{MAX_STAGED_SIZE:X}"
        )
    staging_end = STAGING_ADDRESS + len(payload)
    if staging_end > STAGING_LIMIT:
        raise InjectionError(
            f"staged image ends at 0x{staging_end:08X}, beyond the retail "
            f"startup stack boundary 0x{STAGING_LIMIT:08X}"
        )

    for section in dol.sections:
        if section.size and ranges_overlap(
            STAGING_ADDRESS, staging_end, section.address, section.address_end
        ):
            raise InjectionError(
                f"staged image range overlaps existing {section.name}"
            )
    if dol.bss_size and ranges_overlap(
        STAGING_ADDRESS,
        staging_end,
        dol.bss_address,
        dol.bss_address + dol.bss_size,
    ):
        raise InjectionError("staged image range overlaps the DOL BSS")

    # The staged image includes the bootstrap, so this is a conservative upper
    # bound for the slightly smaller runtime copy at PAYLOAD_ADDRESS.
    runtime_end_bound = PAYLOAD_ADDRESS + len(payload)
    if runtime_end_bound > MEM1_END:
        raise InjectionError(
            f"runtime payload bound ends at 0x{runtime_end_bound:08X}, "
            f"beyond MEM1 end 0x{MEM1_END:08X}"
        )


def build_patched_dol(source_data: bytes, payload: bytes) -> tuple[bytes, int, int]:
    source_dol = validate_source(source_data)
    validate_payload(payload, source_dol)

    payload_file_offset = align_up(len(source_data), FILE_ALIGNMENT)
    patched = bytearray(source_data)
    patched.extend(b"\0" * (payload_file_offset - len(patched)))
    patched.extend(payload)

    struct.pack_into(
        ">I",
        patched,
        SECTION_FILE_OFFSETS_OFFSET + TEXT_SLOT_INDEX * 4,
        payload_file_offset,
    )
    struct.pack_into(
        ">I",
        patched,
        SECTION_ADDRESSES_OFFSET + TEXT_SLOT_INDEX * 4,
        STAGING_ADDRESS,
    )
    struct.pack_into(
        ">I",
        patched,
        SECTION_SIZES_OFFSET + TEXT_SLOT_INDEX * 4,
        len(payload),
    )

    hook_file_offset = source_dol.runtime_to_file(HOOK_ADDRESS, 4)
    hook_instruction = encode_relative_bl(HOOK_ADDRESS, PAYLOAD_ADDRESS)
    struct.pack_into(">I", patched, hook_file_offset, hook_instruction)
    arena_hi_branch_offset = source_dol.runtime_to_file(ARENA_HI_BRANCH_ADDRESS, 4)
    struct.pack_into(
        ">I", patched, arena_hi_branch_offset, PATCHED_ARENA_HI_BRANCH_WORD
    )
    struct.pack_into(">I", patched, ENTRY_POINT_OFFSET, STAGING_ADDRESS)
    return bytes(patched), payload_file_offset, hook_instruction


def verify_patched_dol(
    written_data: bytes,
    source_data: bytes,
    payload: bytes,
    expected_payload_offset: int,
    expected_hook_instruction: int,
) -> None:
    expected_size = expected_payload_offset + len(payload)
    if len(written_data) != expected_size:
        raise InjectionError(
            f"post-write size mismatch: 0x{len(written_data):X} != 0x{expected_size:X}"
        )

    written_dol = parse_dol(written_data)
    text3 = written_dol.section(TEXT_SLOT_INDEX)
    expected_section = (expected_payload_offset, STAGING_ADDRESS, len(payload))
    actual_section = (text3.file_offset, text3.address, text3.size)
    if actual_section != expected_section:
        raise InjectionError(
            f"post-write text3 mismatch: {actual_section!r} != {expected_section!r}"
        )
    if written_data[text3.file_offset : text3.file_end] != payload:
        raise InjectionError("post-write text3 bytes do not match the input payload")

    entry_point = struct.unpack_from(">I", written_data, ENTRY_POINT_OFFSET)[0]
    if entry_point != STAGING_ADDRESS:
        raise InjectionError(
            f"post-write entry point is 0x{entry_point:08X}, "
            f"not staging address 0x{STAGING_ADDRESS:08X}"
        )

    arena_hi_branch_word = struct.unpack(
        ">I", written_dol.read_runtime(ARENA_HI_BRANCH_ADDRESS, 4)
    )[0]
    if arena_hi_branch_word != PATCHED_ARENA_HI_BRANCH_WORD:
        raise InjectionError(
            f"post-write ArenaHi clamp mismatch: "
            f"0x{arena_hi_branch_word:08X} != "
            f"0x{PATCHED_ARENA_HI_BRANCH_WORD:08X}"
        )

    hook_word = struct.unpack(">I", written_dol.read_runtime(HOOK_ADDRESS, 4))[0]
    if hook_word != expected_hook_instruction:
        raise InjectionError(
            f"post-write hook mismatch: 0x{hook_word:08X} != "
            f"0x{expected_hook_instruction:08X}"
        )
    branch_target = decode_relative_bl(HOOK_ADDRESS, hook_word)
    if branch_target != PAYLOAD_ADDRESS:
        raise InjectionError(
            f"post-write hook targets 0x{branch_target:08X}, "
            f"not 0x{PAYLOAD_ADDRESS:08X}"
        )

    prefix_size = HOOK_ADDRESS - HOOK_SIGNATURE_ADDRESS
    suffix_address = HOOK_ADDRESS + 4
    suffix_offset = prefix_size + 4
    if written_dol.read_runtime(HOOK_SIGNATURE_ADDRESS, prefix_size) != (
        HOOK_SIGNATURE[:prefix_size]
    ):
        raise InjectionError("instructions before the hook changed unexpectedly")
    expected_suffix = HOOK_SIGNATURE[suffix_offset:]
    if written_dol.read_runtime(suffix_address, len(expected_suffix)) != expected_suffix:
        raise InjectionError("instructions after the hook changed unexpectedly")

    source_hook_offset = parse_dol(source_data).runtime_to_file(HOOK_ADDRESS, 4)
    source_arena_offset = parse_dol(source_data).runtime_to_file(
        ARENA_HI_BRANCH_ADDRESS, 4
    )
    differing_source = bytearray(source_data)
    struct.pack_into(">I", differing_source, source_hook_offset, expected_hook_instruction)
    struct.pack_into(
        ">I", differing_source, source_arena_offset, PATCHED_ARENA_HI_BRANCH_WORD
    )
    for header_offset in (
        SECTION_FILE_OFFSETS_OFFSET + TEXT_SLOT_INDEX * 4,
        SECTION_ADDRESSES_OFFSET + TEXT_SLOT_INDEX * 4,
        SECTION_SIZES_OFFSET + TEXT_SLOT_INDEX * 4,
        ENTRY_POINT_OFFSET,
    ):
        differing_source[header_offset : header_offset + 4] = written_data[
            header_offset : header_offset + 4
        ]
    if written_data[: len(source_data)] != bytes(differing_source):
        raise InjectionError(
            "bytes outside text3 header fields, entry, render hook, and ArenaHi "
            "clamp changed unexpectedly"
        )


def write_atomically(output_path: Path, data: bytes) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            prefix=f".{output_path.name}.",
            suffix=".tmp",
            dir=output_path.parent,
            delete=False,
        ) as temporary:
            temporary_path = Path(temporary.name)
            temporary.write(data)
            temporary.flush()
            os.fsync(temporary.fileno())
        os.replace(temporary_path, output_path)
        temporary_path = None
    finally:
        if temporary_path is not None:
            try:
                temporary_path.unlink()
            except FileNotFoundError:
                pass


def paths_refer_to_same_file(input_path: Path, output_path: Path) -> bool:
    if input_path.resolve() == output_path.resolve():
        return True
    if output_path.exists():
        try:
            return input_path.samefile(output_path)
        except OSError:
            return False
    return False


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Append a staged debug-menu image to the verified GUTE52 main.dol, "
            "reserve its runtime arena, patch entry/UI hooks, and keep the "
            "input immutable."
        )
    )
    parser.add_argument("input", type=Path, help="unmodified GUTE52 main.dol")
    parser.add_argument("payload", type=Path, help="raw payload binary")
    parser.add_argument("output", type=Path, help="separate path for patched main.dol")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    input_path: Path = args.input
    payload_path: Path = args.payload
    output_path: Path = args.output

    try:
        if not input_path.is_file():
            raise InjectionError(f"input DOL does not exist: {input_path}")
        if not payload_path.is_file():
            raise InjectionError(f"payload does not exist: {payload_path}")
        if paths_refer_to_same_file(input_path, output_path):
            raise InjectionError("output must be a separate file; refusing to modify input")

        source_data = input_path.read_bytes()
        payload = payload_path.read_bytes()
        patched, payload_offset, hook_instruction = build_patched_dol(
            source_data, payload
        )
        write_atomically(output_path, patched)

        written_data = output_path.read_bytes()
        verify_patched_dol(
            written_data,
            source_data,
            payload,
            payload_offset,
            hook_instruction,
        )
        source_hash_after = sha256(input_path.read_bytes())
        if source_hash_after != EXPECTED_INPUT_SHA256:
            raise InjectionError(
                "source DOL changed during injection; expected immutable input hash "
                f"{EXPECTED_INPUT_SHA256}, found {source_hash_after}"
            )

        print(f"Input:   {input_path}  SHA-256 {EXPECTED_INPUT_SHA256}")
        print(
            f"Payload: {payload_path}  0x{len(payload):X} bytes, "
            f"SHA-256 {sha256(payload)}"
        )
        print(
            f"Text3:   file 0x{payload_offset:X}, staging memory "
            f"0x{STAGING_ADDRESS:08X}..0x{STAGING_ADDRESS + len(payload):08X}"
        )
        print(
            f"Entry:   0x{EXPECTED_ENTRY_POINT:08X} -> 0x{STAGING_ADDRESS:08X}; "
            f"runtime payload copy -> 0x{PAYLOAD_ADDRESS:08X}"
        )
        print(
            f"ArenaHi: 0x{ARENA_HI_BRANCH_ADDRESS:08X}: "
            f"0x{EXPECTED_ARENA_HI_BRANCH_WORD:08X} -> "
            f"0x{PATCHED_ARENA_HI_BRANCH_WORD:08X} "
            f"(force 0x{PAYLOAD_ADDRESS:08X} exclusive bound)"
        )
        print(
            f"Hook:    0x{HOOK_ADDRESS:08X}: 0x{EXPECTED_HOOK_WORD:08X} -> "
            f"0x{hook_instruction:08X} (BL 0x{PAYLOAD_ADDRESS:08X})"
        )
        print(f"Output:  {output_path}  SHA-256 {sha256(written_data)}")
        print("Verification: passed; source DOL unchanged")
        return 0
    except (InjectionError, OSError) as error:
        print(f"inject_dol.py: error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
