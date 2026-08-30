#!/usr/bin/env python3
"""Generate verified address and menu-label data for the GUTE52 payload.

gc_address.txt is a symbol/order guide from a nearby build, not a literal address
database for the retail DOL.  The header deliberately records both the listed
and the disassembly-verified address so a future port cannot confuse them.
"""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


EXPECTED_DOL_SHA256 = "3dd0ad5ede2ef9df27a1aba1be9bf9cd5dfd7a14caa4bb8da396f2574c89565c"


VERIFIED = {
    "PAYLOAD_BASE": 0x81780000,
    "HOOK_SITE": 0x8016E558,
    "HOOK_ORIGINAL_TARGET": 0x8017EBDC,
    "NGL_SYSFONT_SLOT": 0x80460C30,
    "NGL_LIST_ADD_STRING_SCALED": 0x8029AA08,
    "NGL_GET_STRING_DIMENSIONS_SCALED": 0x8029AD4C,
    "NGL_LIST_ADD_QUAD": 0x8029BD84,
    "NGL_INIT_QUAD": 0x8029BEE8,
    "NGL_SET_QUAD_COLOR": 0x8029BFB0,
    "NGL_SET_QUAD_RECT": 0x8029BFC4,
    "NGL_SET_QUAD_Z": 0x8029BFE8,
    "NGL_DEBUG_FLAGS": 0x803D8D90,
    "PAD_READ": 0x80311F00,
    "GAME_GET_CUR_STATE": 0x80142620,
    "GAME_ENABLE_PHYSICS": 0x8017F0C8,
    "GEOMETRY_IS_SCENE_ANALYZER_ENABLED": 0x801803D8,
    "GEOMETRY_ENABLE_SCENE_ANALYZER": 0x801803E0,
    "SET_GOD_MODE": 0x800E1B70,
    "INPUT_SINGLETON_SLOT": 0x80462810,
    "RUMBLE_ENABLE": 0x801D736C,
    "RUMBLE_DISABLE": 0x801D7380,
    "WORLD_SLOT": 0x8045F180,
    "MISSION_MANAGER_SLOT": 0x8045F6EC,
    "MISSION_PREPARE_UNLOAD_SCRIPT": 0x801DB040,
    "MISSION_FORCE_MISSION": 0x801DB3C4,
    "MISSION_CONDITION_APPLIES_TO_CURRENT_HERO": 0x801DE1BC,
    "MISSION_CONDITION_GET_INSTANCE": 0x801DE6D4,
    "MISSION_INSTANCE_GET_SCRIPT_DATA_NAME": 0x801DCFD4,
    "MISSION_UPDATE_HERO_SWITCH": 0x801D9A70,
    "DVD_CONVERT_PATH_TO_ENTRYNUM": 0x802EEFD4,
    "MSTRING_FROM_CHAR": 0x800412C0,
    "MSTRING_FINALIZE": 0x800410E0,
    "DEVOPT_SET_STRING": 0x801D038C,
    "TERRAIN_SET_DISTRICT_VARIANT": 0x80179438,
    "REGION_GET_DISTRICT_VARIANT": 0x80185F60,
    "FOUND_ENTITIES_SLOT": 0x8045F018,
    "ENTITY_FIND_ENTITIES": 0x80103244,
    "ENTITY_IS_CONGLOMERATE": 0x801350F4,
    "CONGLOMERATE_HAS_VARIANT_IFC": 0x80138D10,
    "CONGLOMERATE_VARIANT_IFC": 0x80138D28,
    "STRING_HASH_TO_STRING": 0x8015BFB8,
    "VARIANT_INTERFACE_APPLY_VARIANT": 0x800F7670,
    "DEVOPT_SINGLETON_SLOT": 0x804627F8,
    "DEVOPT_SET_FLAG": 0x801D02B8,
    "DEVOPT_GET_FLAG": 0x801D02C8,
    "DEVOPT_TOGGLE_FLAG": 0x801D0310,
    "DEVOPT_SET_INT": 0x801D0334,
    "DEVOPT_GET_INT": 0x801D0344,
    "DEVOPT_FLAG_NAME_TABLE": 0x80370518,
    "DEVOPT_FLAG_COUNT": 151,
    "DEVOPT_INT_NAME_TABLE": 0x80370A08,
    "DEVOPT_INT_COUNT": 78,
    "OS_GET_ARENA_HI": 0x802F4C40,
    "OS_GET_ARENA_LO": 0x802F4C48,
}


# Prefixes are used because the demangler's whitespace is not stable.
LISTED_PREFIXES = {
    "NGL_LIST_ADD_STRING_SCALED": "nglListAddString(nglFont *, float, float, float, unsigned long, float, float, char *,...)",
    "NGL_GET_STRING_DIMENSIONS_SCALED": "nglGetStringDimensions(nglFont *, char *, unsigned int *, unsigned int *, float, float)",
    "NGL_LIST_ADD_QUAD": "nglListAddQuad(nglQuad *)",
    "NGL_INIT_QUAD": "nglInitQuad(nglQuad *)",
    "NGL_SET_QUAD_COLOR": "nglSetQuadColor(nglQuad *, unsigned long)",
    "NGL_SET_QUAD_RECT": "nglSetQuadRect(nglQuad *, float, float, float, float)",
    "NGL_SET_QUAD_Z": "nglSetQuadZ(nglQuad *, float)",
    "PAD_READ": "PADRead",
    "GAME_GET_CUR_STATE": "game::get_cur_state(void) const",
    "GAME_ENABLE_PHYSICS": "game::enable_physics(bool)",
    "GEOMETRY_IS_SCENE_ANALYZER_ENABLED": "geometry_manager::is_scene_analyzer_enabled(void)",
    "GEOMETRY_ENABLE_SCENE_ANALYZER": "geometry_manager::enable_scene_analyzer(bool)",
    "SET_GOD_MODE": "set_god_mode(int)",
    "RUMBLE_ENABLE": "rumble_manager::enable_vibration(void)",
    "RUMBLE_DISABLE": "rumble_manager::disable_vibration(void)",
    "MISSION_PREPARE_UNLOAD_SCRIPT": "mission_manager::prepare_unload_script(void)",
    "MISSION_FORCE_MISSION": "mission_manager::force_mission(int, char *, int, char *)",
    "MISSION_CONDITION_APPLIES_TO_CURRENT_HERO": "mission_condition::applies_to_current_hero(void) const",
    "MISSION_CONDITION_GET_INSTANCE": "mission_condition::get_instance(int) const",
    "MISSION_INSTANCE_GET_SCRIPT_DATA_NAME": "mission_condition_instance::get_script_data_name(void) const",
    "MISSION_UPDATE_HERO_SWITCH": "mission_manager::update_hero_switch(void)",
    "DVD_CONVERT_PATH_TO_ENTRYNUM": "DVDConvertPathToEntrynum",
    "MSTRING_FROM_CHAR": "mString::mString(char *)",
    "MSTRING_FINALIZE": "mString::finalize(mash::allocation_scope)",
    "DEVOPT_SET_STRING": "os_developer_options::set_string(os_developer_options::strings_t, mString &)",
    "TERRAIN_SET_DISTRICT_VARIANT": "terrain::set_district_variant(int, int, bool)",
    "REGION_GET_DISTRICT_VARIANT": "region::get_district_variant(void)",
    "ENTITY_FIND_ENTITIES": "entity::find_entities(int)",
    "ENTITY_IS_CONGLOMERATE": "entity_base::is_a_conglomerate(void) const",
    "CONGLOMERATE_HAS_VARIANT_IFC": "conglomerate::has_variant_ifc(void) const",
    "CONGLOMERATE_VARIANT_IFC": "conglomerate::variant_ifc(void) const",
    "STRING_HASH_TO_STRING": "string_hash::to_string(void) const",
    "VARIANT_INTERFACE_APPLY_VARIANT": "variant_interface::apply_variant(string_hash)",
    "DEVOPT_SET_FLAG": "os_developer_options::set_flag(os_developer_options::flags_t, bool)",
    "DEVOPT_GET_FLAG": "os_developer_options::get_flag(os_developer_options::flags_t) const",
    "DEVOPT_TOGGLE_FLAG": "os_developer_options::toggle_flag(os_developer_options::flags_t)",
    "DEVOPT_SET_INT": "os_developer_options::set_int(os_developer_options::ints_t, int)",
    "DEVOPT_GET_INT": "os_developer_options::get_int(os_developer_options::ints_t) const",
    "OS_GET_ARENA_HI": "OSGetArenaHi",
    "OS_GET_ARENA_LO": "OSGetArenaLo",
}


DEBUG_RENDER_LABELS = [
    "CAPSULE_HISTORY", "LIGHTS", "BOX_TRIGGERS", "WATER_EXCLUSION_TRIGGERS",
    "POINT_TRIGGERS", "ENTITY_TRIGGERS", "INTERACTABLE_TRIGGERS", "OCCLUSION",
    "LEGOS", "REGION_MESHES", "ENTITIES", "LOW_LODS", "ACTIVITY_INFO",
    "RENDER_INFO", "COLLIDE_INFO", "MARKERS", "PARKING_MARKERS",
    "WATER_EXIT_MARKERS", "MISSION_MARKERS", "PATHS", "GLASS_HOUSE", "OBBS",
    "TRAFFIC_PATHS", "MINI_GAME", "BRAINS", "VOICE", "PATROLS", "PAUSE_TIMERS",
    "ANIM_INFO", "SCENE_ANIM_INFO", "TARGETING", "VIS_SPHERES", "LADDERS",
    "COLLISIONS", "BRAINS_ENABLED", "ANCHORS", "LINE_INFO", "SUBDIVISION",
    "SKELETONS", "SOUND_STREAM_USAGE", "SPHERES", "LINES", "CYLINDERS", "DGRAPH",
    "PEDS", "TRAFFIC", "ALS", "AI_COVER_MARKERS", "LIMBO_GLOW",
    "BIPED_COLL_VOLUMES", "DECALS",
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_symbol_map(path: Path) -> dict[str, tuple[int, int]]:
    symbols: dict[str, tuple[int, int]] = {}
    pattern = re.compile(r"^Name=(.*?) Address=(0x[0-9a-fA-F]+) Size=(\d+)$")
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line)
        if match:
            symbols[match.group(1)] = (int(match.group(2), 16), int(match.group(3)))
    return symbols


def find_listed(symbols: dict[str, tuple[int, int]], prefix: str) -> tuple[int, int]:
    matches = [(name, value) for name, value in symbols.items() if name.startswith(prefix)]
    if len(matches) != 1:
        names = ", ".join(name for name, _ in matches) or "none"
        raise RuntimeError(f"expected one gc_address symbol starting with {prefix!r}; got {names}")
    return matches[0][1]


def parse_xbox_tables(path: Path) -> tuple[list[tuple[str, int]], list[str]]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()

    # game_option_t stru_14F2CB8[230] in the supplied Xbox decompile.
    devopt_block = "\n".join(lines[128557:128802])
    options = [
        (name, int(kind))
        for name, kind in re.findall(
            r'\{\s*"([^"]+)"\s*,.*?,\s*([1-4])\s*\}', devopt_block, re.S
        )
        if kind in ("1", "2", "3")
    ]

    # populate_gamefile_menu emits these names through mString::from_char.
    saved_block = "\n".join(lines[703230:706084])
    saved = re.findall(
        r'j_mString::from_char\([^\n]*"([A-Z][A-Z0-9_]+)"\)', saved_block
    )

    if len(options) != 225:
        raise RuntimeError(f"Xbox Devopts extraction returned {len(options)} rows, expected 225")
    if len(saved) != 97 or len(set(saved)) != 97:
        raise RuntimeError(f"Xbox saved-game extraction returned {len(saved)} rows, expected 97 unique")
    if len(DEBUG_RENDER_LABELS) != 51:
        raise RuntimeError("internal Debug Render inventory is incomplete")
    return options, saved


def c_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit_string_array(name: str, values: list[str]) -> list[str]:
    out = [f"static const char *const {name}[] = {{"]
    out.extend(f"    {c_string(value)}," for value in values)
    out.append("};")
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--gc-address", type=Path, required=True)
    parser.add_argument("--xbox-decompile", type=Path, required=True)
    parser.add_argument("--dol", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    dol_hash = sha256(args.dol)
    if dol_hash != EXPECTED_DOL_SHA256:
        raise RuntimeError(
            f"unsupported main.dol SHA-256 {dol_hash}; expected {EXPECTED_DOL_SHA256}"
        )

    symbols = parse_symbol_map(args.gc_address)
    listed = {key: find_listed(symbols, prefix) for key, prefix in LISTED_PREFIXES.items()}
    devopts, saved = parse_xbox_tables(args.xbox_decompile)

    out = [
        "/* Generated by tools/generate_data.py; do not hand-edit. */",
        "#ifndef USM_GC_GENERATED_DATA_H",
        "#define USM_GC_GENERATED_DATA_H",
        "",
        f"#define TARGET_DOL_SHA256 {c_string(dol_hash)}",
    ]
    for key, value in VERIFIED.items():
        out.append(f"#define ADDR_{key} 0x{value:08X}u")
    out.append("")
    for key in sorted(listed):
        address, size = listed[key]
        out.append(f"#define GC_LISTED_{key} 0x{address:08X}u /* size {size} */")

    out.extend(["", f"#define DEVOPT_COUNT {len(devopts)}u"])
    out.extend(emit_string_array("kDevoptLabels", [name for name, _ in devopts]))
    out.append("static const unsigned char kDevoptKinds[] = {")
    for offset in range(0, len(devopts), 24):
        out.append("    " + ", ".join(str(kind) for _, kind in devopts[offset:offset + 24]) + ",")
    out.append("};")

    out.extend(["", f"#define SAVED_SETTING_COUNT {len(saved)}u"])
    out.extend(emit_string_array("kSavedSettingLabels", saved))
    out.extend(["", f"#define DEBUG_RENDER_COUNT {len(DEBUG_RENDER_LABELS)}u"])
    out.extend(emit_string_array("kDebugRenderLabels", DEBUG_RENDER_LABELS))
    out.extend(["", "#endif /* USM_GC_GENERATED_DATA_H */", ""])

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(out), encoding="utf-8", newline="\n")

    print(f"verified DOL: {dol_hash}")
    for key in sorted(listed):
        print(f"{key}: gc_address=0x{listed[key][0]:08X}, retail=0x{VERIFIED[key]:08X}")
    print(f"generated {len(devopts)} Devopts, {len(saved)} saved settings, 51 Debug Render rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
