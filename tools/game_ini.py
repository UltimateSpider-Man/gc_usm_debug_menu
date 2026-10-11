#!/usr/bin/env python3
"""Edit only the active [Strings] SCENE_NAME in a GameCube game.ini."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re

from inject_gamecube_payload import atomic_write

SCENES = {"city": "city_arena", "characterb": "characterb_arena"}


def scene_name(value: str) -> str:
    result = SCENES.get(value.lower(), value.lower())
    if result not in SCENES.values():
        raise ValueError("scene must be city, characterb, city_arena, or characterb_arena")
    return result


def update_scene(raw: bytes, scene: str) -> bytes:
    """Preserve comments, unrelated keys, encoding, and newline convention."""
    value = scene_name(scene).encode("ascii")
    content, nul, padding = raw.partition(b"\0")
    if nul and padding.strip(b"\0\r\n \t"):
        raise ValueError("game.ini contains non-padding bytes after its first NUL")
    bom = b"\xef\xbb\xbf" if content.startswith(b"\xef\xbb\xbf") else b""
    content = content[len(bom):]
    newline = b"\r\n" if b"\r\n" in content else b"\n"
    lines = content.splitlines(keepends=True)
    output: list[bytes] = []
    in_strings = False
    saw_strings = False
    inserted = False
    for line in lines:
        body = line.rstrip(b"\r\n")
        ending = line[len(body):]
        section = re.fullmatch(rb"\s*\[([^\]]+)\]\s*(?:[;#].*)?", body)
        if section:
            if in_strings and not inserted:
                if output and not output[-1].endswith((b"\r", b"\n")):
                    output[-1] += newline
                output.append(b"SCENE_NAME=" + value + newline)
                inserted = True
            in_strings = section[1].strip().lower() == b"strings"
            saw_strings |= in_strings
        match = re.fullmatch(rb"(\s*SCENE_NAME\s*=\s*)(.*)", body, re.IGNORECASE) if in_strings else None
        if match:
            # Duplicate active keys have parser-dependent precedence. Keep a
            # single active assignment; commented examples stay byte-exact.
            if inserted:
                continue
            tail = match[2]
            comment = re.search(rb"\s+[;#].*", tail)
            suffix = comment[0] if comment else b""
            output.append(match[1] + value + suffix + ending)
            inserted = True
        else:
            output.append(line)
    if not inserted:
        if output and not output[-1].endswith((b"\r", b"\n")):
            output[-1] += newline
        if not saw_strings:
            output.append(b"[Strings]" + newline)
        output.append(b"SCENE_NAME=" + value + newline)
    result = bom + b"".join(output)
    if nul:
        # Reuse existing zero padding when the longer arena name fits it.
        result += b"\0" * max(1, len(raw) - len(result))
    return result


def write_scene(path: Path, scene: str, *, backup: bool = True) -> bytes:
    original = path.read_bytes()
    replacement = update_scene(original, scene)
    if replacement != original:
        if backup:
            saved = path.with_name(path.name + ".usm-debug-original")
            if not saved.exists():
                # Exclusive creation never replaces a user's prior backup.
                with saved.open("xb") as stream:
                    stream.write(original)
                    stream.flush()
                    os.fsync(stream.fileno())
        atomic_write(path, replacement)
    return replacement


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ini", type=Path, required=True)
    parser.add_argument("--scene", required=True)
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--output", type=Path)
    destination.add_argument("--in-place", action="store_true")
    args = parser.parse_args()
    try:
        if args.in_place:
            write_scene(args.ini, args.scene)
            output = args.ini
        else:
            if args.output.resolve() == args.ini.resolve():
                raise ValueError("use --in-place to replace the supplied INI with a backup")
            atomic_write(args.output, update_scene(args.ini.read_bytes(), args.scene))
            output = args.output
    except (OSError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")
    print(f"{output}: SCENE_NAME={scene_name(args.scene)}")


if __name__ == "__main__":
    main()
