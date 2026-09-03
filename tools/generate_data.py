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
import struct
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
    "NGL_HIRES_SCREENSHOT_IN_PROGRESS": 0x802948E8,
    "NGL_BEGIN_HIRES_SCREENSHOT": 0x802948F0,
    "NGL_SAVE_HIRES_SCREENSHOT": 0x80294AD0,
    "NGL_ADJUST_VIEW_FOR_HIRES_SCREENSHOT": 0x80294B7C,
    "NGL_SCREENSHOT": 0x8029C774,
    "NGL_SCREEN_WIDTH": 0x80462704,
    "NGL_SCREEN_HEIGHT": 0x80462708,
    "NGL_DEBUG_FLAGS": 0x803D8D90,
    "PAD_READ": 0x80311F00,
    "GAME_PROCESS_CTOR": 0x801424BC,
    "GAME_PUSH_PROCESS": 0x8014252C,
    "GAME_GET_CUR_STATE": 0x80142620,
    "GAME_PAUSE": 0x8017E67C,
    "GAME_UNPAUSE": 0x8017E8C0,
    "GAME_LOAD_NEW_LEVEL": 0x8017EA14,
    "GAME_ENABLE_PHYSICS": 0x8017F0C8,
    "GEOMETRY_IS_SCENE_ANALYZER_ENABLED": 0x801803D8,
    "GEOMETRY_ENABLE_SCENE_ANALYZER": 0x801803E0,
    "SET_GOD_MODE": 0x800E1B70,
    "INPUT_SINGLETON_SLOT": 0x80462810,
    "RUMBLE_ENABLE": 0x801D736C,
    "RUMBLE_DISABLE": 0x801D7380,
    "SPIDER_MONKEY_START": 0x800D84D8,
    "SPIDER_MONKEY_ON_LEVEL_LOAD": 0x800D8734,
    "SPIDER_MONKEY_ON_LEVEL_UNLOAD": 0x800D8A50,
    "SPIDER_MONKEY_IS_RUNNING": 0x800D8AD0,
    "INPUT_SET_STATE_MONKEY_CALLBACK": 0x801E0458,
    "INPUT_SET_DELTA_MONKEY_CALLBACK": 0x801E0460,
    "SPIDER_MONKEY_RUNNING": 0x8045EF74,
    "SPIDER_MONKEY_OOK_TIMER": 0x8045EF78,
    "SPIDER_MONKEY_RUNTIME": 0x8045EF80,
    "SPIDER_MONKEY_RUNTIME_MONKEY_TEXT": 0x8045EF84,
    "SPIDER_MONKEY_RUNTIME_TEXT": 0x8045EF88,
    "WORLD_SLOT": 0x8045F180,
    "MISSION_MANAGER_SLOT": 0x8045F6EC,
    "MISSION_PREPARE_UNLOAD_SCRIPT": 0x801DB040,
    "MISSION_FORCE_MISSION": 0x801DB3C4,
    "MISSION_CONDITION_APPLIES_TO_CURRENT_HERO": 0x801DE1BC,
    "MISSION_CONDITION_GET_INSTANCE": 0x801DE6D4,
    "MISSION_INSTANCE_GET_SCRIPT_DATA_NAME": 0x801DCFD4,
    "MISSION_UPDATE_HERO_SWITCH": 0x801D9A70,
    "SCRIPT_MANAGER_FIND_FUNCTION_BY_NAME": 0x801B5FF0,
    "SCRIPT_INSTANCE_ADD_THREAD": 0x801B2E40,
    "SCRIPT_INSTANCE_ADD_THREAD_WITH_ARGS": 0x801B2EF0,
    "DVD_CONVERT_PATH_TO_ENTRYNUM": 0x802EEFD4,
    "MSTRING_FROM_CHAR": 0x800412C0,
    "MSTRING_FINALIZE": 0x800410E0,
    "DEVOPT_SET_STRING": 0x801D038C,
    "TERRAIN_UNLOAD_ALL_DISTRICTS_IMMEDIATE": 0x80178A20,
    "TERRAIN_UNLOCK_DISTRICT": 0x8017AE28,
    "TERRAIN_SET_DISTRICT_VARIANT": 0x80179438,
    "WORLD_MALOR_POINT": 0x80152B40,
    "FE_MANAGER": 0x803C8478,
    "ENTITY_TRACKER_GET_ARROW_TARGET_POS": 0x7F023E40,
    "RESOURCE_MANAGER_GET_PARTITION_POINTER": 0x8015D968,
    "RESOURCE_PACK_STREAMER_SET_ACTIVE": 0x8015DFB0,
    "RESOURCE_PACK_SLOT_GET_RESOURCE_DIRECTORY": 0x801961F4,
    "RESOURCE_DIRECTORY_GET_RESOURCE": 0x8015FF5C,
    "RESOURCE_DIRECTORY_TYPE_TO_VECTOR": 0x80160428,
    "RESOURCE_MANAGER_PUSH_CONTEXT": 0x8015CCE8,
    "RESOURCE_MANAGER_POP_CONTEXT": 0x8015CD78,
    "STRING_HASH_FROM_CHAR": 0x8015BF68,
    "ACTOR_ALLOCATE_ANIM_CONTROLLER": 0x8010523C,
    "ANIMATION_PLAY_BASE_LAYER": 0x800A6EA0,
    "NAL_GET_NEXT_ANIM": 0x802A3D74,
    "REGION_GET_DISTRICT_VARIANT": 0x80185F60,
    "FOUND_ENTITIES_SLOT": 0x8045F018,
    "ENTITY_FIND_ENTITIES": 0x80103244,
    "ACTOR_GET_AI_CORE": 0x80106A14,
    "AI_CORE_LIST_HIGH_SLOT": 0x8045F7B4,
    "ENTITY_IS_CONGLOMERATE": 0x801350F4,
    "ENTITY_IS_ACTOR": 0x80135124,
    "ACTOR_IS_ACTOR": 0x80135F88,
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
    "OS_REPORT": 0x802F5E2C,
    # slab_allocator::initialize stores these pointers through r13=0x80466A80.
    # The retail addresses and slab_t offsets are verified against GUTE52 at
    # 0x801C2270..0x801C29F4; they are not taken from the version-skewed map.
    "SLAB_PARTIAL_LISTS_SLOT": 0x804627C0,
    "SLAB_FULL_LISTS_SLOT": 0x804627C4,
    "SLAB_FREE_LIST_SLOT": 0x804627C8,
    "SLAB_INITIALIZED": 0x8045F644,
    # Live float values used by camera_target_info and ai::jump_state.
    "DVAR_CAMERA_MIN_DIST": 0x8033A944,
    "DVAR_CAMERA_MAX_DIST": 0x8033A948,
    "DVAR_CAMERA_SUPERMAX_DIST": 0x8033A94C,
    "DVAR_JUMP_CAP_VEL": 0x8045EC98,
    "DVAR_SNOW_BALLING": 0x8045EC9C,
    "DVAR_BASE_FACTOR": 0x8045ECA0,
    "MEM_GET_TOTAL_ALLOCED": 0x8018D448,
    "MEM_GET_BYTES_FREE": 0x8018D47C,
    # The fixed ARAM virtual image is directly callable through the retail
    # pager.  Its backing bytes live at virtual+0x01462000 in this DOL.
    "DEBUG_RENDER_GET_IVAL": 0x7F010CA0,
    "DEBUG_RENDER_SET_IVAL": 0x7F010CB4,
    "DEBUG_RENDER_GET_MIN": 0x7F010D4C,
    "DEBUG_RENDER_GET_MAX": 0x7F010D60,
    "GAME_SETTINGS_SAVE": 0x7F004AF0,
    "GAME_SETTINGS_LOAD_CARD": 0x7F004FB8,
    "GAME_SETTINGS_LOAD_GAME": 0x7F0050C8,
    "GAME_SETTINGS_LOAD_MOST_RECENT": 0x7F005394,
    "MEMORY_UNIT_OPERATION": 0x80460E48,
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
    "GAME_PROCESS_CTOR": "game_process::game_process(char *, game_state_e *, int)",
    "GAME_PUSH_PROCESS": "game::push_process(game_process &)",
    "GAME_GET_CUR_STATE": "game::get_cur_state(void) const",
    "GAME_PAUSE": "game::pause(void)",
    "GAME_UNPAUSE": "game::unpause(void)",
    "GAME_LOAD_NEW_LEVEL": "game::_load_new_level(mString &)",
    "GAME_ENABLE_PHYSICS": "game::enable_physics(bool)",
    "GEOMETRY_IS_SCENE_ANALYZER_ENABLED": "geometry_manager::is_scene_analyzer_enabled(void)",
    "GEOMETRY_ENABLE_SCENE_ANALYZER": "geometry_manager::enable_scene_analyzer(bool)",
    "SET_GOD_MODE": "set_god_mode(int)",
    "RUMBLE_ENABLE": "rumble_manager::enable_vibration(void)",
    "RUMBLE_DISABLE": "rumble_manager::disable_vibration(void)",
    "SPIDER_MONKEY_START": "spider_monkey::start(void)",
    "SPIDER_MONKEY_ON_LEVEL_LOAD": "spider_monkey::on_level_load(void)",
    "SPIDER_MONKEY_ON_LEVEL_UNLOAD": "spider_monkey::on_level_unload(void)",
    "SPIDER_MONKEY_IS_RUNNING": "spider_monkey::is_running(void)",
    "INPUT_SET_STATE_MONKEY_CALLBACK": "input_mgr::set_control_state_monkey_callback(float (*)(int))",
    "INPUT_SET_DELTA_MONKEY_CALLBACK": "input_mgr::set_control_delta_monkey_callback(float (*)(int))",
    "MISSION_PREPARE_UNLOAD_SCRIPT": "mission_manager::prepare_unload_script(void)",
    "MISSION_FORCE_MISSION": "mission_manager::force_mission(int, char *, int, char *)",
    "MISSION_CONDITION_APPLIES_TO_CURRENT_HERO": "mission_condition::applies_to_current_hero(void) const",
    "MISSION_CONDITION_GET_INSTANCE": "mission_condition::get_instance(int) const",
    "MISSION_INSTANCE_GET_SCRIPT_DATA_NAME": "mission_condition_instance::get_script_data_name(void) const",
    "MISSION_UPDATE_HERO_SWITCH": "mission_manager::update_hero_switch(void)",
    "SCRIPT_MANAGER_FIND_FUNCTION_BY_NAME": "script_manager::find_function_by_name(string_hash)",
    "SCRIPT_INSTANCE_ADD_THREAD": "script_instance::add_thread(vm_executable *)",
    "SCRIPT_INSTANCE_ADD_THREAD_WITH_ARGS": "script_instance::add_thread(vm_executable *, char *)",
    "DVD_CONVERT_PATH_TO_ENTRYNUM": "DVDConvertPathToEntrynum",
    "MSTRING_FROM_CHAR": "mString::mString(char *)",
    "MSTRING_FINALIZE": "mString::finalize(mash::allocation_scope)",
    "DEVOPT_SET_STRING": "os_developer_options::set_string(os_developer_options::strings_t, mString &)",
    "TERRAIN_UNLOCK_DISTRICT": "terrain::unlock_district(int)",
    "TERRAIN_SET_DISTRICT_VARIANT": "terrain::set_district_variant(int, int, bool)",
    "WORLD_MALOR_POINT": "world_dynamics_system::malor_point(vector3d &, int, bool)",
    "ENTITY_TRACKER_GET_ARROW_TARGET_POS": "entity_tracker_manager::get_the_arrow_target_pos(vector3d &)",
    "RESOURCE_MANAGER_GET_PARTITION_POINTER": "resource_manager::get_partition_pointer(resource_partition_enum)",
    "RESOURCE_PACK_STREAMER_SET_ACTIVE": "resource_pack_streamer::set_active(bool)",
    "RESOURCE_PACK_SLOT_GET_RESOURCE_DIRECTORY": "resource_pack_slot::get_resource_directory(void)",
    "RESOURCE_DIRECTORY_GET_RESOURCE": "resource_directory::get_resource(resource_key &, int *, resource_pack_slot **)",
    "RESOURCE_DIRECTORY_TYPE_TO_VECTOR": "resource_directory::tlresource_type_to_vector(tlresource_type)",
    "RESOURCE_MANAGER_PUSH_CONTEXT": "resource_manager::push_resource_context(resource_pack_slot *)",
    "RESOURCE_MANAGER_POP_CONTEXT": "resource_manager::pop_resource_context(void)",
    "STRING_HASH_FROM_CHAR": "string_hash::initialize(mash::allocation_scope, char *, unsigned int)",
    "ACTOR_ALLOCATE_ANIM_CONTROLLER": "actor::allocate_anim_controller(unsigned int, nalBaseSkeleton *)",
    "ANIMATION_PLAY_BASE_LAYER": "animation_controller::play_base_layer_anim(string_hash &, float, unsigned int, bool)",
    "NAL_GET_NEXT_ANIM": "nalGetNextAnimInFile(nalAnimClass<nalAnyPose> *)",
    "REGION_GET_DISTRICT_VARIANT": "region::get_district_variant(void)",
    "ENTITY_FIND_ENTITIES": "entity::find_entities(int)",
    "ACTOR_GET_AI_CORE": "actor::get_ai_core(void) const",
    "ENTITY_IS_CONGLOMERATE": "entity_base::is_a_conglomerate(void) const",
    "ENTITY_IS_ACTOR": "entity_base::is_an_actor(void) const",
    "ACTOR_IS_ACTOR": "actor::is_an_actor(void) const",
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
    "OS_REPORT": "OSReport",
    "MEM_GET_TOTAL_ALLOCED": "mem_get_total_alloced(os_heaptype_t)",
    "MEM_GET_BYTES_FREE": "mem_get_bytes_free(os_heaptype_t)",
    "DEBUG_RENDER_GET_IVAL": "debug_render_get_ival(debug_render_items_e)",
    "DEBUG_RENDER_SET_IVAL": "debug_render_set_ival(debug_render_items_e, int)",
    "DEBUG_RENDER_GET_MIN": "debug_render_get_min(debug_render_items_e)",
    "DEBUG_RENDER_GET_MAX": "debug_render_get_max(debug_render_items_e)",
    "GAME_SETTINGS_SAVE": "game_settings::save(int)",
    "GAME_SETTINGS_LOAD_CARD": "game_settings::load(void)",
    "GAME_SETTINGS_LOAD_GAME": "game_settings::load_game(int)",
    "GAME_SETTINGS_LOAD_MOST_RECENT": "game_settings::load_most_recent_game(void)",
}


DEBUG_RENDER_LABELS = [
    "ACTIVITY_INFO", "AI_COVER_MARKERS", "ALS", "ANCHORS", "ANIM_INFO",
    "BIPED_COLL_VOLUMES", "BOX_TRIGGERS", "BRAINS", "BRAINS_ENABLED",
    "CAPSULE_HISTORY", "COLLIDE_INFO", "COLLISIONS", "CYLINDERS", "DECALS",
    "DGRAPH", "ENTITIES", "ENTITY_TRIGGERS", "GLASS_HOUSE",
    "INTERACTABLE_TRIGGERS", "LADDERS", "LEGOS", "LIGHTS", "LIMBO_GLOW",
    "LINES", "LINE_INFO", "LOW_LODS", "MARKERS", "MINI_GAME",
    "MISSION_MARKERS", "OBBS", "OCCLUSION", "PARKING_MARKERS", "PATHS",
    "PATROLS", "PAUSE_TIMERS", "PEDS", "POINT_TRIGGERS", "REGION_MESHES",
    "RENDER_INFO", "SCENE_ANIM_INFO", "SKELETONS", "SOUND_STREAM_USAGE",
    "SPHERES", "SUBDIVISION", "TARGETING", "TRAFFIC", "TRAFFIC_PATHS",
    "VIS_SPHERES", "VOICE", "WATER_EXCLUSION_TRIGGERS", "WATER_EXIT_MARKERS",
]


SLAB_LAYOUT_WORDS = {
    # Startup establishes r13, then slab_allocator::initialize publishes the
    # partial/full/free lists, slab storage, metadata, and initialized flag.
    0x80003110: 0x3DA08046,
    0x80003114: 0x61AD6A80,
    0x801C22B4: 0x906DBD40,
    0x801C22EC: 0x906DBD44,
    0x801C2314: 0x906DBD48,
    0x801C233C: 0x906D8BC8,
    0x801C2350: 0x906D8BCC,
    0x801C23B8: 0x900D8BC4,
}


DVAR_LAYOUT_WORDS = {
    0x8033A944: 0x40100000,  # camera_min_dist = 2.25f
    0x8033A948: 0x40900000,  # camera_max_dist = 4.50f
    0x8033A94C: 0x41000000,  # camera_supermax_dist = 8.00f
    0x8045EC98: 0x42040000,  # jump_cap_vel = 33.00f
    0x8045EC9C: 0x40200000,  # snow_balling = 2.50f
    0x8045ECA0: 0x41B80000,  # base_factor = 23.00f
}


MONKEY_LAYOUT_WORDS = {
    0x800D84D8: 0x9421FFE8,  # spider_monkey::start prologue
    0x800D84F8: 0x48107F61,  # set state callback
    0x800D8508: 0x48107F59,  # set delta callback
    0x800D8518: 0x900D84F4,  # m_running = true
    0x800D8734: 0x9421FFD8,  # on_level_load prologue
    0x800D8A50: 0x9421FFF0,  # on_level_unload prologue
    0x800D8AD0: 0x806D84F4,  # is_running: lwz r3,-0x7b0c(r13)
    0x800D8AD4: 0x4E800020,  # is_running: blr
    0x801E0458: 0x90830030,  # input_mgr state callback slot
    0x801E045C: 0x4E800020,
    0x801E0460: 0x90830034,  # input_mgr delta callback slot
    0x801E0464: 0x4E800020,
}


AI_LAYOUT_WORDS = {
    0x80106A14: 0x8123007C,  # actor::get_ai_core: actor + 0x7c
    0x80106A24: 0x80690014,  # ai interface + 0x14 -> ai_core
    0x80135124: 0x38600000,  # entity_base::is_an_actor = false
    0x80135F88: 0x38600001,  # actor::is_an_actor = true
    # Fixed-DOL AI-core construction/access proves the GC-shifted fields.
    0x8021B43C: 0x3B9F0044,  # inline core param block
    0x8021B47C: 0x93DF0058,  # core actor pointer
    0x8021B53C: 0x3BDF0054,  # core inode-vector pointer field
    0x8021B544: 0x907F0054,
    0x8021B6FC: 0x81490008,  # mVector data pointer
    0x8021B728: 0x80090004,  # mVector count
    # Pageable AI image backing (virtual+0x01462000) proves array/param shape.
    0x804E06A0: 0x807F0014,  # param array cached pointer
    0x804E06AC: 0x80030008,
    0x804E0708: 0x809F0008,
    0x804E070C: 0x80BF0004,
    0x804E1168: 0x81230008,  # param name hash
    0x804E1178: 0x81230004,  # param type
    0x804E13D8: 0xC0230000,  # float value
    0x804E13E0: 0x80630000,  # integer value
    0x804E162C: 0x90830008,  # inode core
    0x804E1630: 0x80040058,  # inode actor from core
    0x804E1634: 0x9003000C,
    0x804E1A80: 0x387E0004,  # inode name hash
    0x804E1A8C: 0x387E0010,  # inode inline param block
    0x804E1954: 0x98090008,  # param block changed byte
    0x804E1958: 0x90090004,  # param array pointer
    0x804E195C: 0x90090000,  # current mode
    # Global high-core list and its 12-byte intrusive-list nodes.
    0x8021B5F8: 0x800D8D34,
    0x8021B618: 0x93CD8D34,
    0x8021B61C: 0x806D8D34,
    0x8021B63C: 0x81690004,  # list + 4 -> allocated sentinel
    0x8021B640: 0x800B0000,  # sentinel -> next
    0x8025A3A4: 0x901F0004,
    0x8025A3D0: 0x91290000,
    0x8025A3D8: 0x91290004,
    0x8025A3DC: 0x913F0004,  # publish sentinel at list + 4
    0x8025A404: 0x80030004,
    0x8025A43C: 0x801F0000,
    0x8025A440: 0x90030008,
    0x8025A44C: 0x91230000,
    0x8025A450: 0x91630004,
}


TERRAIN_LAYOUT_WORDS = {
    0x80178A20: 0x9421FFE0,  # terrain::unload_all_districts_immediate prologue
    0x80178A24: 0x7C0802A6,  # mflr r0
    0x80178AF4: 0x4E800020,  # blr
    # terrain::unlock_district scans region pointers, matches region+0xc0,
    # clears the locked bit at +0x50, and refreshes the terrain state.
    0x8017AE28: 0x9421FFF8,
    0x8017AE48: 0x81230000,
    0x8017AE58: 0x800900C0,
    0x8017AE64: 0x80090050,
    0x8017AE68: 0x5400003C,
    0x8017AE6C: 0x90090050,
    # The two terrain-streaming partitions own inline streamers at +0x18.
    0x8015DFB0: 0x90830000,
    0x8015DFB4: 0x4E800020,
    0x80178548: 0x38600006,
    0x8017854C: 0x4BFE541D,
    0x80178550: 0x3AA30018,
    0x80178558: 0x38800001,
    0x8017855C: 0x4BFE5A55,
    0x80178560: 0x38600005,
    0x80178564: 0x4BFE5405,
    0x80178568: 0x3B630018,
    0x80178570: 0x38800001,
    0x80178574: 0x4BFE5A3D,
    0x8015E830: 0x801D0000,
    0x8015E838: 0x2C000000,
    0x8015E83C: 0x41820130,
}


GAME_PROCESS_LAYOUT_WORDS = {
    # game_process is a 24-byte value copied into game+0x84's process stack.
    0x801424BC: 0x3D608035,
    0x801424CC: 0x90890000,
    0x801424D0: 0x90A90004,
    0x801424D4: 0x90C9000C,
    0x801424E0: 0x91690008,
    0x8014252C: 0x9421FFE8,
    0x80142544: 0x387E0084,
    0x80142560: 0x80050000,
    0x80142598: 0x91230004,
    # States 8 and 9 are the begin/save hires capture state pair.
    0x80142740: 0x806DBC84,
    0x80142744: 0x808DBC88,
    0x80142748: 0x481521A9,
    0x80142750: 0x48152381,
    # Running state: +0x1a8 is physics-disabled and +0x1a4 is one-step.
    0x80142D60: 0x801E01A8,
    0x80142D6C: 0x801E01A4,
    0x80142D9C: 0x93FE01A4,
    # Pause/unpause own game+0x1b0 and push/pop the retail pause process.
    0x8017E67C: 0x9421FFD8,
    0x8017E6B0: 0x801E01B0,
    0x8017E860: 0x4BFC3CCD,
    0x8017E864: 0x93BE01B0,
    0x8017E8C0: 0x9421FFC8,
    0x8017E8D4: 0x801F01B0,
}


WARP_LAYOUT_WORDS = {
    # world_dynamics_system::malor_point teleports player r5 to vector r4.
    0x80152B40: 0x9421FFE0,
    0x80152B54: 0x7C9F2378,
    0x80152B58: 0x54BC103A,
    0x80152B64: 0x7C7DE02E,
    0x80152B68: 0x4BFA9371,
    0x80152BB0: 0x4E800020,
}


LEVEL_LAYOUT_WORDS = {
    0x8015D968: 0x812D8794,  # resource_manager partition table
    0x8015D96C: 0x5463103A,
    0x8015D974: 0x7C6B182E,
    0x801961F4: 0x80630018,  # GC resource_pack_slot directory at +0x18
    0x801961F8: 0x4E800020,
    0x8015FF5C: 0x9421FFE0,  # resource_directory::get_resource
    0x8015FF7C: 0x38A10008,
    0x8015FFA0: 0x8009000C,
    0x8017EA14: 0x9421FFF0,  # game::_load_new_level(mString &)
    0x8017EA24: 0x80040004,
    0x8017EA4C: 0x913F019C,
    # Retail lookup proves partition+8, resource key type 9, and 0x90 rows.
    0x8014302C: 0x81230008,
    0x80143068: 0x38A00009,
    0x80143128: 0x480530CD,
    0x80143138: 0x4801CE25,
    0x80143148: 0x38000090,
    0x801431C0: 0x38E70090,
}


NGL_HIRES_LAYOUT_WORDS = {
    0x8027F0B4: 0x38600280,  # retail screen width = 640
    0x8027F0B8: 0x4E800020,
    0x8027F0BC: 0x386001E0,  # retail screen height = 480
    0x8027F0C0: 0x4E800020,
    0x802948E8: 0x806DA18C,  # nglHiresScreenShotInProgress
    0x802948EC: 0x4E800020,
    0x802948F0: 0x9421FFB8,  # nglBeginHiresScreenShot prologue
    0x80294AD0: 0x9421FFB8,  # nglSaveHiresScreenshot prologue
    0x80294B7C: 0x9421FFF8,  # nglAdjustViewForHiresScreenshot prologue
    0x8029C774: 0x9421FFF0,  # nglScreenShot prologue
}


NGL_DEBUG_LAYOUT_WORDS = {
    # nglDebugInit proves nglDebug and its adjacent sync copy are 19 bytes.
    0x8029DE80: 0x38A00013,
    0x8029DE84: 0x387E8D90,
    0x8029DE94: 0x8BA90012,
    0x8029DEB4: 0xA0090010,
    0x8029DEC4: 0x9BAB0012,
    # The render handoff copies the same packed 19-byte object.
    0x80282240: 0x392B8D90,
    0x80282244: 0x88890012,
    0x80282248: 0x396A8DA3,
    # Retail consumers identify ShowPerfInfo at +4 and ScreenShot at +5.
    0x80285C58: 0x88098DA7,
    0x80282994: 0x881E0005,
    0x802829E8: 0x981E0005,
}


DEBUG_RENDER_LAYOUT_WORDS = {
    # The ARAM virtual image begins at 0x7f004000 and is stored in this DOL at
    # 0x80466000.  Verify the backing instructions for all four callable APIs.
    0x80472CA0: 0x3D207F1C,  # get_ival: lis r9, debug_render_items@ha
    0x80472CAC: 0x7C69182E,  # indexed load
    0x80472CB4: 0x9421FFF0,  # set_ival prologue
    0x80472CCC: 0x48000095,  # clamp against get_max
    0x80472D0C: 0x7FE9012E,  # indexed store
    0x80472D4C: 0x3D207F1C,  # get_min
    0x80472D58: 0x7C69182E,
    0x80472D60: 0x3D207F1C,  # get_max
    0x80472D6C: 0x7C69182E,
}


POI_WARP_LAYOUT_WORDS = {
    # entity_tracker_manager::get_the_arrow_target_pos is virtual 0x7f023e40;
    # these are its verified DOL backing instructions at virtual+0x01462000.
    0x80485E40: 0x9421FFD8,
    0x80485E50: 0x7C7F1B78,
    0x80485E54: 0x7C9E2378,
    0x80485EE4: 0xC0090030,
    0x80485EEC: 0xD01E0000,
    0x80485EF4: 0xD01E0004,
    0x80485EFC: 0xD01E0008,
    0x80485F34: 0x38600000,
    0x80485F48: 0x4E800020,
}


ANIMATION_LAYOUT_WORDS = {
    # Actor target layout: skeleton +0x70, controller +0x74, pack slot +0xbc.
    0x8010523C: 0x9421FFD0,
    0x8010524C: 0x7C7F1B78,
    0x80105254: 0x801F0074,
    0x80105278: 0x801F0070,
    0x8010529C: 0x907F0070,
    # EABI aggregate-return play call preserves hidden result/controller/hash.
    0x800A6EA0: 0x9421FFA8,
    0x800A6EB4: 0x7C9F2378,
    0x800A6EB8: 0x7CBE2B78,
    0x800A6EBC: 0x7C7A1B78,
    0x800A6EC8: 0xFFE00890,
    0x800A6ECC: 0x7CDD3378,
    0x800A6ED0: 0x7CF93B78,
    # Resource-context stack and animation-file traversal.
    0x8015CCE8: 0x9421FFE8,
    0x8015CCF8: 0x90610008,
    0x8015CD78: 0x9421FFF0,
    0x8015CD98: 0x812B0004,
    0x80160428: 0x2C040007,
    0x80160464: 0x2C04000B,
    0x801604D8: 0x38630048,
    0x801604DC: 0x4E800020,
    0x8015BF68: 0x9421FFE8,
    0x8015BF7C: 0x7C842379,
    0x802A3D74: 0x7C631B79,
    0x802A3D80: 0x80030004,
    0x802A3D88: 0x4E800020,
}


SAVE_LOAD_LAYOUT_WORDS = {
    # Pageable game_settings code; backing addresses are virtual+0x01462000.
    0x80466AF0: 0x9421FFB0,  # save(int)
    0x80466B0C: 0x1F99003C,  # slot indexes three 60-byte records
    0x80466BD4: 0x99480540,  # mark valid[slot]
    0x80466D38: 0x933F053C,  # publish current slot
    0x80466FB8: 0x9421FFF0,  # load card metadata/data
    0x80466FC8: 0x7C7E1B78,
    0x804670C8: 0x9421FFE8,  # load_game(int)
    0x804670DC: 0x7C9C2378,
    0x804670E4: 0x881F0546,
    0x8046710C: 0x981F0546,
    0x80467110: 0x939F0548,
    0x80467284: 0x939F053C,
    0x80467394: 0x9421FFF0,  # load_most_recent_game()
    0x804673A4: 0x7C7F1B78,
    0x804673A8: 0x4BFFFF09,
    0x802DD018: 0x806DA3C8,  # MemoryUnitManager::GetCurrentOperation
    0x802DD01C: 0x4E800020,
}


MEMORY_STATS_LAYOUT_WORDS = {
    0x8018D448: 0x9421FFD0,  # mem_get_total_alloced(heap)
    0x8018D454: 0x7C641B78,
    0x8018D460: 0x80610024,
    0x8018D470: 0x4E800020,
    0x8018D47C: 0x9421FFC0,  # mem_get_bytes_free(heap)
    0x8018D48C: 0x7C7E1B78,
    0x8018D4A4: 0x80610024,
    0x8018D4A8: 0x7C63E850,
    0x8018D4BC: 0x4E800020,
    # Native checkpoint/stats entry points are deliberately inert in retail.
    0x8018D154: 0x38600000,
    0x8018D158: 0x4E800020,
}


OS_REPORT_LAYOUT_WORDS = {
    0x801AC0C0: 0x4E800020,  # retail os_file::write is a no-op
    0x802F5E2C: 0x7C0802A6,  # OSReport variadic prologue
    0x802F5E30: 0x90010004,
    0x802F5E34: 0x9421FF88,
    0x802F5E38: 0x40860024,
}


SCRIPT_DISPATCH_LAYOUT_WORDS = {
    0x801B5FF0: 0x9421FFE0,  # script_manager::find_function_by_name prologue
    0x801B2E40: 0x9421FFE0,  # script_instance::add_thread(executable)
    0x801B2E54: 0x7C7E1B78,  # preserve instance argument
    0x801B2E5C: 0x7C9D2378,  # preserve executable argument
    0x801B2EA0: 0x7FA5EB78,  # executable passed to thread constructor
    0x801B2EA4: 0x7FC4F378,  # instance passed to thread constructor
    0x801B2EA8: 0x48004449,  # construct the VM thread
    0x801B2ED8: 0x7FE3FB78,  # return the new thread
    0x801B2EF0: 0x9421FFE0,  # script_instance::add_thread(executable, args)
    0x801B2F08: 0x4BFFFF39,  # reuse the no-argument overload
    0x801B2F10: 0x2C1B0000,  # skip the parameter copy when args is null
    0x801B2F14: 0x4182002C,
    0x801B2F18: 0x83BC000C,  # executable+0x0c parameter byte count
    0x801B2F30: 0x4816618D,  # copy exactly that many argument bytes
    # vm_executable::_link_un_mash proves the bytecode buffer/word-count ABI
    # used by the payload's debug_menu_entry access guard.
    0x801B6F50: 0x801C0014,  # lwz r0, +0x14 (u32 count of u16 words)
    0x801B6F58: 0x83FC0010,  # lwz r31, executable+0x10 (u16 buffer)
    0x801B6F5C: 0x7C000214,  # byte count = word count * two
    0x801B6F64: 0x7C1F0214,  # end = buffer + byte count
    0x801B6F78: 0xA01F0000,  # lhz r0, 0(buffer)
    0x801B6F7C: 0x3BFF0002,  # consume the opword
    0x801B6F80: 0x5409067E,  # mask opword to its low seven-bit arg type
    0x801B6F84: 0x700B0080,  # test the optional DSIZE word flag
    0x801B6F88: 0x41820008,
    0x801B6F8C: 0x3BFF0002,  # consume the optional DSIZE word
    # Fixed-DOL caller proves executable->owner, owner->instances, first instance.
    0x800E33F4: 0x81230000,
    0x800E33F8: 0x8169002C,
    0x800E33FC: 0x83EB0000,
    # script_object::_add publishes the owner back-pointer at instance+0x2c.
    0x801B462C: 0x9065002C,
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def dol_sections(data: bytes) -> list[tuple[int, int, int]]:
    sections: list[tuple[int, int, int]] = []
    for offset_base, address_base, size_base, count in (
        (0x00, 0x48, 0x90, 7),
        (0x1C, 0x64, 0xAC, 11),
    ):
        for index in range(count):
            file_offset = struct.unpack_from(">I", data, offset_base + index * 4)[0]
            section_address = struct.unpack_from(">I", data, address_base + index * 4)[0]
            section_size = struct.unpack_from(">I", data, size_base + index * 4)[0]
            if file_offset and section_size:
                sections.append((file_offset, section_address, section_size))
    return sections


def dol_file_span(data: bytes, address: int, size: int = 1) -> tuple[int, int]:
    for file_offset, section_address, section_size in dol_sections(data):
        if section_address <= address and address + size <= section_address + section_size:
            offset = file_offset + address - section_address
            return offset, file_offset + section_size
    raise RuntimeError(
        f"address range 0x{address:08X}..0x{address + size:08X} "
        "is not backed by a DOL section"
    )


def read_dol_word(data: bytes, address: int) -> int:
    file_offset, _ = dol_file_span(data, address, 4)
    return struct.unpack_from(">I", data, file_offset)[0]


def read_dol_c_string(data: bytes, address: int, maximum: int = 128) -> str:
    file_offset, section_end = dol_file_span(data, address)
    search_end = min(section_end, file_offset + maximum)
    string_end = data.find(b"\0", file_offset, search_end)
    if string_end < 0:
        raise RuntimeError(f"unterminated DOL string at 0x{address:08X}")
    try:
        value = data[file_offset:string_end].decode("ascii")
    except UnicodeDecodeError as error:
        raise RuntimeError(f"non-ASCII DOL string at 0x{address:08X}") from error
    if not value:
        raise RuntimeError(f"empty DOL string at 0x{address:08X}")
    return value


def verify_slab_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in SLAB_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"slab layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_dvar_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in DVAR_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"DVar layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_monkey_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in MONKEY_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"spider-monkey layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_ai_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in AI_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"AI layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_terrain_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in TERRAIN_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"terrain layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_game_process_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in GAME_PROCESS_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"game process layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_warp_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in WARP_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"warp layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_level_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in LEVEL_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"level-select layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_ngl_hires_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in NGL_HIRES_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"NGL hires layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_ngl_debug_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in NGL_DEBUG_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"NGL debug layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_debug_render_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in DEBUG_RENDER_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"Debug Render ARAM-image check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_poi_warp_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in POI_WARP_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"POI-warp ARAM-image check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_animation_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in ANIMATION_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"animation layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_save_load_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in SAVE_LOAD_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"save/load ARAM-image check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_memory_stats_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in MEMORY_STATS_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"memory-stat layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_os_report_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in OS_REPORT_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"OSReport layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def verify_script_dispatch_layout(path: Path) -> None:
    data = path.read_bytes()
    for address, expected in SCRIPT_DISPATCH_LAYOUT_WORDS.items():
        actual = read_dol_word(data, address)
        if actual != expected:
            raise RuntimeError(
                f"script dispatch layout check failed at 0x{address:08X}: "
                f"found 0x{actual:08X}, expected 0x{expected:08X}"
            )


def saved_setting_layout(count: int) -> tuple[list[int], list[int]]:
    """Return exact GUTE52 offsets/kinds for the 97 debug-menu fields.

    Kinds deliberately reuse the payload's numeric convention: 1=word bool,
    2=s32, 3=float.  The GameCube port expands every Xbox byte/boolx field to
    a 32-bit word; these offsets are relative to game_settings+0x358.
    """
    if count != 97:
        raise RuntimeError(f"saved-setting layout expects 97 rows, got {count}")
    offsets = [0] * count
    kinds = [2] * count

    for index in range(0, 12):
        offsets[index] = index * 4
    offsets[12:18] = [0x30, 0x34, 0x38, 0x3C, 0x40, 0x44]
    for index in range(18, 33):
        offsets[index] = 0x48 + (index - 18) * 4
    offsets[33:41] = [0x84, 0x88, 0x8C, 0x90, 0x94, 0x98, 0x9C, 0xA0]
    for index in range(41, 60):
        offsets[index] = 0xA4 + (index - 41) * 4
    offsets[60:65] = [0xF0, 0xF4, 0xF8, 0xFC, 0x100]
    offsets[65:70] = [0x104, 0x108, 0x10C, 0x110, 0x114]
    offsets[70:75] = [0x118, 0x11C, 0x120, 0x124, 0x128]
    offsets[75:78] = [0x12C, 0x130, 0x134]
    for index in range(78, 97):
        offsets[index] = 0x138 + (index - 78) * 4

    bool_indices = {12, 13} | set(range(18, 33)) | set(range(35, 38)) | set(range(41, 60))
    float_indices = {15, 16, 34, 38} | set(range(65, 70)) | set(range(75, 78))
    for index in bool_indices:
        kinds[index] = 1
    for index in float_indices:
        kinds[index] = 3
    return offsets, kinds


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


def parse_gc_devopts(path: Path) -> list[tuple[str, int]]:
    """Extract the complete target-native GUTE52 flag/int inventories.

    Each verified table is an array of pointers to NUL-terminated names.  The
    row order is also the exact enum ID consumed by the retail get/set calls.
    Item kind values deliberately match the payload: 1=bool, 2=s32.
    """
    data = path.read_bytes()
    rows: list[tuple[str, int]] = []
    inventories = (
        (VERIFIED["DEVOPT_FLAG_NAME_TABLE"], VERIFIED["DEVOPT_FLAG_COUNT"], 1),
        (VERIFIED["DEVOPT_INT_NAME_TABLE"], VERIFIED["DEVOPT_INT_COUNT"], 2),
    )
    for table, count, kind in inventories:
        for index in range(count):
            name_address = read_dol_word(data, table + index * 4)
            name = read_dol_c_string(data, name_address)
            if not re.fullmatch(r"[A-Z][A-Z0-9_]*", name):
                raise RuntimeError(
                    f"unexpected GUTE52 Devopt name {name!r} at index {index}"
                )
            rows.append((name, kind))

    expected_count = VERIFIED["DEVOPT_FLAG_COUNT"] + VERIFIED["DEVOPT_INT_COUNT"]
    names = [name for name, _ in rows]
    if len(rows) != expected_count or len(set(names)) != expected_count:
        raise RuntimeError(
            f"GUTE52 Devopt extraction returned {len(rows)} rows, "
            f"expected {expected_count} unique"
        )
    return rows


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
    verify_slab_layout(args.dol)
    verify_dvar_layout(args.dol)
    verify_monkey_layout(args.dol)
    verify_ai_layout(args.dol)
    verify_terrain_layout(args.dol)
    verify_game_process_layout(args.dol)
    verify_warp_layout(args.dol)
    verify_level_layout(args.dol)
    verify_ngl_hires_layout(args.dol)
    verify_ngl_debug_layout(args.dol)
    verify_debug_render_layout(args.dol)
    verify_poi_warp_layout(args.dol)
    verify_animation_layout(args.dol)
    verify_save_load_layout(args.dol)
    verify_memory_stats_layout(args.dol)
    verify_os_report_layout(args.dol)
    verify_script_dispatch_layout(args.dol)

    symbols = parse_symbol_map(args.gc_address)
    listed = {key: find_listed(symbols, prefix) for key, prefix in LISTED_PREFIXES.items()}
    _xbox_devopts, saved = parse_xbox_tables(args.xbox_decompile)
    devopts = parse_gc_devopts(args.dol)

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
    saved_offsets, saved_kinds = saved_setting_layout(len(saved))
    out.append("static const unsigned short kSavedSettingOffsets[] = {")
    for offset in range(0, len(saved_offsets), 16):
        out.append(
            "    " + ", ".join(
                f"0x{value:03X}u" for value in saved_offsets[offset:offset + 16]
            ) + ","
        )
    out.append("};")
    out.append("static const unsigned char kSavedSettingKinds[] = {")
    for offset in range(0, len(saved_kinds), 24):
        out.append(
            "    " + ", ".join(str(value) for value in saved_kinds[offset:offset + 24]) + ","
        )
    out.append("};")
    out.extend(["", f"#define DEBUG_RENDER_COUNT {len(DEBUG_RENDER_LABELS)}u"])
    out.extend(emit_string_array("kDebugRenderLabels", DEBUG_RENDER_LABELS))
    out.extend(["", "#endif /* USM_GC_GENERATED_DATA_H */", ""])

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(out), encoding="utf-8", newline="\n")

    print(f"verified DOL: {dol_hash}")
    for key in sorted(listed):
        print(f"{key}: gc_address=0x{listed[key][0]:08X}, retail=0x{VERIFIED[key]:08X}")
    print(
        f"generated {len(devopts)} target-native Devopts, "
        f"{len(saved)} saved settings, 51 Debug Render rows"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
