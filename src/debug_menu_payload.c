/*
 * Ultimate Spider-Man (GUTE52) GameCube debug-menu payload.
 *
 * Freestanding PowerPC code: no CRT, libogc, allocation, C++ runtime, r2, or
 * r13 small-data assumptions.  All callable addresses below are generated from
 * gc_address.txt provenance plus exact retail-DOL disassembly verification.
 */

#include "generated_data.h"

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;

#define ARRAY_COUNT(a) ((u32)(sizeof(a) / sizeof((a)[0])))
#define MENU_NONE 0xFFFFu
#define MAX_VISIBLE_ROWS 14u
#define MENU_TEXT_SCALE 1.05f
#define MAX_ENTITY_LIST_NODES 1024u
#define MAX_VARIANT_ENTITIES 1024u
#define MAX_MISSION_ROWS 256u
#define MAX_MISSION_GROUPS 9u

typedef struct __attribute__((packed)) PadStatus {
    u16 button;
    s8 stick_x;
    s8 stick_y;
    s8 substick_x;
    s8 substick_y;
    u8 trigger_left;
    u8 trigger_right;
    u8 analog_a;
    u8 analog_b;
    s8 error;
    u8 padding;
} PadStatus;

typedef struct NglQuad {
    u8 bytes[100];
} NglQuad;

typedef void (*NglListAddStringFn)(void *, float, float, float, u32, float, float,
                                   const char *, ...);
typedef void (*NglGetStringDimensionsFn)(void *, const char *, u32 *, u32 *, float, float);
typedef void (*NglQuadFn)(NglQuad *);
typedef void (*NglSetQuadRectFn)(NglQuad *, float, float, float, float);
typedef void (*NglSetQuadColorFn)(NglQuad *, u32);
typedef void (*NglSetQuadZFn)(NglQuad *, float);
typedef u32 (*PadReadFn)(PadStatus *);
typedef s32 (*GameGetCurStateFn)(void *);
typedef void (*GameEnablePhysicsFn)(void *, s32);
typedef s32 (*GeometryIsSceneAnalyzerEnabledFn)(void);
typedef void (*GeometryEnableSceneAnalyzerFn)(s32);
typedef void (*SetGodModeFn)(s32);
typedef void (*RumbleModeFn)(void *);
typedef void (*MissionPrepareUnloadScriptFn)(void *);
typedef void (*MissionForceMissionFn)(void *, s32, const char *, s32, const char *);
typedef s32 (*MissionConditionAppliesFn)(void *);
typedef void *(*MissionConditionGetInstanceFn)(void *, s32);
typedef const char *(*MissionInstanceGetScriptDataNameFn)(void *);
typedef s32 (*DVDConvertPathToEntrynumFn)(const char *);
typedef void *(*MStringFromCharFn)(void *, const char *);
typedef void (*MStringFinalizeFn)(void *, s32);
typedef void (*DevoptSetStringFn)(void *, s32, const void *);
typedef void (*TerrainSetDistrictVariantFn)(void *, s32, s32, s32);
typedef s32 (*RegionGetDistrictVariantFn)(void *);
typedef void (*EntityFindEntitiesFn)(s32);
typedef s32 (*ObjectPredicateFn)(void *);
typedef void *(*ObjectGetterFn)(void *);
typedef const char *(*StringHashToStringFn)(const void *);
typedef void (*VariantApplyFn)(void *, const void *);
typedef s32 (*DevoptGetFlagFn)(void *, s32);
typedef void (*DevoptSetFlagFn)(void *, s32, s32);
typedef s32 (*DevoptGetIntFn)(void *, s32);
typedef void (*DevoptSetIntFn)(void *, s32, s32);
typedef void *(*ArenaGetterFn)(void);

enum PadButton {
    PAD_LEFT = 0x0001,
    PAD_RIGHT = 0x0002,
    PAD_DOWN = 0x0004,
    PAD_UP = 0x0008,
    /* Synthetic: Dolphin maps DualSense Guide/PS to all four D-pad bits. */
    PAD_PS = 0x000F,
    PAD_Z = 0x0010,
    PAD_R = 0x0020,
    PAD_L = 0x0040,
    PAD_A = 0x0100,
    PAD_B = 0x0200,
    PAD_X = 0x0400,
    PAD_Y = 0x0800,
    PAD_START = 0x1000
};

enum ItemKind {
    ITEM_SUBMENU,
    ITEM_BOOL,
    ITEM_INT,
    ITEM_ENUM,
    ITEM_ACTION,
    ITEM_INFO,
    ITEM_PHYSICS,
    ITEM_PAUSE,
    ITEM_DEVFLAG,
    ITEM_DEVINT,
    ITEM_DEVINT_READONLY,
    ITEM_SLOW_MOTION,
    ITEM_CAMERA,
    ITEM_SHOW_DISTRICTS,
    ITEM_DISTRICT_VARIANT,
    ITEM_ENTITY_SUBMENU,
    ITEM_ENTITY_VARIANT,
    ITEM_MISSION,
    ITEM_MISSION_DISTRICT,
    ITEM_UNAVAILABLE
};

enum ActionId {
    ACTION_NONE,
    ACTION_SINGLE_STEP,
    ACTION_UNLOAD_DISTRICTS,
    ACTION_SAVE_GAME,
    ACTION_LOAD_GAME,
    ACTION_AUTO_LOAD,
    ACTION_HIRES_SCREENSHOT,
    ACTION_LORES_SCREENSHOT,
    ACTION_WARP_POI,
    ACTION_UNLOAD_MISSION,
    ACTION_REPLAY_START,
    ACTION_MEMTRACK_DUMP,
    ACTION_MEMTRACK_CHECKPOINT,
    ACTION_REBOOT,
    ACTION_SELECT_HERO,
    ACTION_NGL_SCREENSHOT
};

enum MenuId {
    MENU_ROOT,
    MENU_DVARS,
    MENU_WARP,
    MENU_GAME,
    MENU_SAVE_LOAD,
    MENU_SCREENSHOT,
    MENU_DEVOPTS,
    MENU_SAVED_SETTINGS,
    MENU_MISSIONS,
    MENU_MISSION_DISTRICT,
    MENU_DEBUG_RENDER,
    MENU_NGL_DEBUG,
    MENU_DISTRICT_VARIANTS,
    MENU_REPLAY,
    MENU_AI,
    MENU_MEMORY,
    MENU_SCRIPT_MEMTRACK,
    MENU_SLABS,
    MENU_FULL_SLABS,
    MENU_PARTIAL_SLABS,
    MENU_ALLOCATED_OBJECTS,
    MENU_FREE_OBJECTS,
    MENU_ENTITY_VARIANTS,
    MENU_ENTITY_VARIANT_VALUES,
    MENU_ENTITY_ANIMATIONS,
    MENU_LEVEL_SELECT,
    MENU_HERO_SELECT,
    MENU_SCRIPT,
    MENU_PROGRESSION,
    MENU_CHAR_SELECT,
    MENU_OPTIONS,
    MENU_COUNT
};

typedef struct MenuItem {
    const char *label;
    s32 value;
    s32 minimum;
    s32 maximum;
    u16 target;
    u8 kind;
    u8 action;
} MenuItem;

typedef struct MenuDef {
    const char *title;
    u16 parent;
    MenuItem *items;
    u16 item_count;
    const char *const *labels;
    const u8 *label_kinds;
    s32 *values;
    u16 label_count;
    u8 dynamic_kind;
    u8 generated_rows;
} MenuDef;

typedef struct Row {
    const char *label;
    s32 *value;
    s32 minimum;
    s32 maximum;
    u16 target;
    u8 kind;
    u8 action;
    void *context;
    u32 context_index;
    s32 context_id;
} Row;

typedef struct MissionRow {
    const char *name;
    const char *script_data;
    void *table;
    void *condition;
    u16 instance;
    s16 district;
} MissionRow;

typedef struct MissionGroup {
    const char *name;
    u16 start;
    u16 count;
    s16 district;
    u16 reserved;
} MissionGroup;

#define SUB(label_, target_) { (label_), 0, 0, 0, (target_), ITEM_SUBMENU, ACTION_NONE }
#define BOOL_ITEM(label_, initial_) { (label_), (initial_), 0, 1, MENU_NONE, ITEM_BOOL, ACTION_NONE }
#define INT_ITEM(label_, initial_, min_, max_) { (label_), (initial_), (min_), (max_), MENU_NONE, ITEM_INT, ACTION_NONE }
#define ENUM_ITEM(label_, initial_, max_) { (label_), (initial_), 0, (max_), MENU_NONE, ITEM_ENUM, ACTION_NONE }
#define ACTION_ITEM(label_, action_) { (label_), 0, 0, 0, MENU_NONE, ITEM_ACTION, (action_) }
#define INFO_ITEM(label_) { (label_), 0, 0, 0, MENU_NONE, ITEM_INFO, ACTION_NONE }
#define UNAVAILABLE_ITEM(label_) { (label_), 0, 0, 0, MENU_NONE, ITEM_UNAVAILABLE, ACTION_NONE }
#define DEVFLAG_ITEM(label_, id_) { (label_), (id_), 0, 1, MENU_NONE, ITEM_DEVFLAG, ACTION_NONE }
#define DEVINT_ITEM(label_, id_, min_, max_) { (label_), (id_), (min_), (max_), MENU_NONE, ITEM_DEVINT, ACTION_NONE }
#define SLOW_MOTION_ITEM(label_) { (label_), 0, 0, 1, MENU_NONE, ITEM_SLOW_MOTION, ACTION_NONE }
#define PHYSICS_ITEM(label_) { (label_), 1, 0, 1, MENU_NONE, ITEM_PHYSICS, ACTION_NONE }
#define PAUSE_ITEM(label_) { (label_), 1, 0, 1, MENU_NONE, ITEM_PAUSE, ACTION_NONE }
#define CAMERA_ITEM(label_) { (label_), 0, 0, 2, MENU_NONE, ITEM_CAMERA, ACTION_NONE }
#define SHOW_DISTRICTS_ITEM(label_) { (label_), 0, 0, 1, MENU_NONE, ITEM_SHOW_DISTRICTS, ACTION_NONE }

static MenuItem s_root_items[] = {
    SUB("Warp", MENU_WARP),
    SUB("Game", MENU_GAME),
    SUB("Missions", MENU_MISSIONS),
    SUB("Debug Render", MENU_DEBUG_RENDER),
    SUB("District variants", MENU_DISTRICT_VARIANTS),
    SUB("Replay", MENU_REPLAY),
    SUB("AI", MENU_AI),
    SUB("Memory", MENU_MEMORY),
    SUB("Entity Variants", MENU_ENTITY_VARIANTS),
    SUB("Level Select", MENU_LEVEL_SELECT),
    SUB("Script", MENU_SCRIPT),
    SUB("Progression", MENU_PROGRESSION),
    CAMERA_ITEM("Camera"),
    PAUSE_ITEM("Pause"),
    SUB("DVars", MENU_DVARS),
    SUB("Entity Animations", MENU_ENTITY_ANIMATIONS),
    SUB("Char Select", MENU_CHAR_SELECT),
    SUB("Options", MENU_OPTIONS)
};

static MenuItem s_game_items[] = {
    SUB("Save/Load", MENU_SAVE_LOAD),
    SUB("Screenshot", MENU_SCREENSHOT),
    SUB("Devopts", MENU_DEVOPTS),
    SUB("Saved Game Settings", MENU_SAVED_SETTINGS),
    UNAVAILABLE_ITEM("Report SLF Recall Timeouts"),
    PHYSICS_ITEM("Physics Enabled"),
    ACTION_ITEM("Single Step", ACTION_SINGLE_STEP),
    SLOW_MOTION_ITEM("Slow Motion Enabled"),
    UNAVAILABLE_ITEM("Monkey Enabled"),
    BOOL_ITEM("Rumble Enabled", 1),
    DEVINT_ITEM("God Mode", 29, 0, 5),
    SHOW_DISTRICTS_ITEM("Show Districts"),
    DEVFLAG_ITEM("Show Hero Position", 21),
    DEVFLAG_ITEM("Show FPS", 5),
    UNAVAILABLE_ITEM("User Camera on Controller 2"),
    ACTION_ITEM("Toggle Unload All Districts", ACTION_UNLOAD_DISTRICTS)
};

static MenuItem s_save_load_items[] = {
    ACTION_ITEM("Save Game", ACTION_SAVE_GAME),
    ACTION_ITEM("Load Game", ACTION_LOAD_GAME),
    ACTION_ITEM("Attemp Auto Load", ACTION_AUTO_LOAD)
};

static MenuItem s_screenshot_items[] = {
    ACTION_ITEM("Hires Screenshot", ACTION_HIRES_SCREENSHOT),
    ACTION_ITEM("Lores Screenshot", ACTION_LORES_SCREENSHOT)
};

static MenuItem s_dvars_items[] = {
    INFO_ITEM("No numeric retail DVars are registered")
};

static MenuItem s_warp_items[] = {
    ACTION_ITEM("--WARP TO POI--", ACTION_WARP_POI),
    INFO_ITEM("Region rows populate only when terrain exposes them")
};

static MenuItem s_missions_items[] = {
    ACTION_ITEM("UNLOAD CURRENT MISSION", ACTION_UNLOAD_MISSION)
};

static MenuItem s_debug_render_prefix[] = {
    SUB("NGL Debug", MENU_NGL_DEBUG)
};

static MenuItem s_ngl_items[] = {
    DEVFLAG_ITEM("ShowPerfInfo", 128),
    ACTION_ITEM("ScreenShot", ACTION_NGL_SCREENSHOT),
    UNAVAILABLE_ITEM("DisableQuads"),
    UNAVAILABLE_ITEM("DisableVSync"),
    UNAVAILABLE_ITEM("DisableScratch"),
    UNAVAILABLE_ITEM("DebugPrints"),
    UNAVAILABLE_ITEM("DumpFrameLog"),
    UNAVAILABLE_ITEM("DumpSceneFile"),
    UNAVAILABLE_ITEM("DumpTextures"),
    UNAVAILABLE_ITEM("DrawLightSpheres"),
    UNAVAILABLE_ITEM("DrawMeshSpheres"),
    UNAVAILABLE_ITEM("DisableDuplicateMaterialWarning"),
    UNAVAILABLE_ITEM("DisableMissingTextureWarning"),
    UNAVAILABLE_ITEM("DisableMipOpt"),
    UNAVAILABLE_ITEM("DisableFSAA"),
    UNAVAILABLE_ITEM("DrawToFrontBuffer"),
    UNAVAILABLE_ITEM("SyncRender"),
    UNAVAILABLE_ITEM("RenderSingleNode"),
    UNAVAILABLE_ITEM("ShowPerfGraph"),
    UNAVAILABLE_ITEM("RenderTargetShot")
};

static MenuItem s_replay_items[] = {
    ACTION_ITEM("Start", ACTION_REPLAY_START)
};

static MenuItem s_ai_items[] = {
    INFO_ITEM("Actor-ID submenus populate from live AI actors"),
    INFO_ITEM("-Core params"),
    INFO_ITEM("--None defined--"),
    UNAVAILABLE_ITEM("--Export this block--")
};

static MenuItem s_memory_items[] = {
    SUB("Script Memtrack", MENU_SCRIPT_MEMTRACK),
    ACTION_ITEM("Dump MemTrack Data Since Last Checkpoint", ACTION_MEMTRACK_DUMP),
    ACTION_ITEM("Set MemTrack Checkpoint", ACTION_MEMTRACK_CHECKPOINT),
    SUB("Slabs", MENU_SLABS)
};

static MenuItem s_script_memtrack_items[] = {
    ACTION_ITEM("Dump Memtrack Info", ACTION_MEMTRACK_DUMP),
    INFO_ITEM("Class-name and entity-ID rows are runtime supplied")
};

static MenuItem s_slabs_items[] = {
    INFO_ITEM("Total Slabs"),
    INFO_ITEM("Free Slabs"),
    SUB("Full Slabs", MENU_FULL_SLABS),
    SUB("Partial Slabs", MENU_PARTIAL_SLABS),
    SUB("Allocated Objects", MENU_ALLOCATED_OBJECTS),
    SUB("Free Objects", MENU_FREE_OBJECTS)
};

static MenuItem s_entity_animation_items[] = {
    INFO_ITEM("Actor-ID and animation-name rows are runtime supplied")
};

static MenuItem s_level_select_items[] = {
    INFO_ITEM("Available level display names are pack supplied"),
    ACTION_ITEM("-- REBOOT --", ACTION_REBOOT),
    SUB("Hero Select", MENU_HERO_SELECT)
};

static MenuItem s_hero_select_items[] = {
    ACTION_ITEM("ultimate_spiderman", ACTION_SELECT_HERO),
    ACTION_ITEM("arachno_man_costume", ACTION_SELECT_HERO),
    ACTION_ITEM("usm_wrestling_costume", ACTION_SELECT_HERO),
    ACTION_ITEM("usm_blacksuit_costume", ACTION_SELECT_HERO),
    ACTION_ITEM("peter_parker", ACTION_SELECT_HERO),
    ACTION_ITEM("peter_parker_costume", ACTION_SELECT_HERO),
    ACTION_ITEM("peter_hooded", ACTION_SELECT_HERO),
    ACTION_ITEM("peter_hooded_costume", ACTION_SELECT_HERO),
    ACTION_ITEM("venom", ACTION_SELECT_HERO),
    ACTION_ITEM("venom_spider", ACTION_SELECT_HERO)
};

static MenuItem s_script_items[] = {
    INFO_ITEM("Entries are created by the script debug-menu API")
};

static MenuItem s_progression_items[] = {
    INFO_ITEM("Progression handlers are supplied by loaded scripts")
};

static MenuItem s_options_items[] = {
    DEVFLAG_ITEM("Live in Glass House", 123)
};

static s32 s_devopt_values[DEVOPT_COUNT];

static MenuDef s_menus[MENU_COUNT] = {
    { "Debug Menu", MENU_NONE, s_root_items, ARRAY_COUNT(s_root_items), 0, 0, 0, 0, 0, 0 },
    { "DVars", MENU_ROOT, s_dvars_items, ARRAY_COUNT(s_dvars_items), 0, 0, 0, 0, 0, 0 },
    { "Warp", MENU_ROOT, s_warp_items, ARRAY_COUNT(s_warp_items), 0, 0, 0, 0, 0, 0 },
    { "Game", MENU_ROOT, s_game_items, ARRAY_COUNT(s_game_items), 0, 0, 0, 0, 0, 0 },
    { "Save/Load", MENU_GAME, s_save_load_items, ARRAY_COUNT(s_save_load_items), 0, 0, 0, 0, 0, 0 },
    { "Screenshot", MENU_GAME, s_screenshot_items, ARRAY_COUNT(s_screenshot_items), 0, 0, 0, 0, 0, 0 },
    { "Devopts", MENU_GAME, 0, 0, kDevoptLabels, kDevoptKinds, s_devopt_values, DEVOPT_COUNT, ITEM_BOOL, 0 },
    { "Saved Game Settings", MENU_GAME, 0, 0, kSavedSettingLabels, 0, 0, SAVED_SETTING_COUNT, ITEM_UNAVAILABLE, 0 },
    { "Missions", MENU_ROOT, s_missions_items, ARRAY_COUNT(s_missions_items), 0, 0, 0, 0, 0, 0 },
    { "District Missions", MENU_MISSIONS, 0, 0, 0, 0, 0, 0, ITEM_MISSION, 0 },
    { "Debug Render", MENU_ROOT, s_debug_render_prefix, ARRAY_COUNT(s_debug_render_prefix), kDebugRenderLabels, 0, 0, DEBUG_RENDER_COUNT, ITEM_UNAVAILABLE, 0 },
    { "NGL Debug", MENU_DEBUG_RENDER, s_ngl_items, ARRAY_COUNT(s_ngl_items), 0, 0, 0, 0, 0, 0 },
    { "District variants", MENU_ROOT, 0, 0, 0, 0, 0, 0, ITEM_DISTRICT_VARIANT, 0 },
    { "Replay", MENU_ROOT, s_replay_items, ARRAY_COUNT(s_replay_items), 0, 0, 0, 0, 0, 0 },
    { "AI", MENU_ROOT, s_ai_items, ARRAY_COUNT(s_ai_items), 0, 0, 0, 0, 0, 0 },
    { "Memory", MENU_ROOT, s_memory_items, ARRAY_COUNT(s_memory_items), 0, 0, 0, 0, 0, 0 },
    { "Script Memtrack", MENU_MEMORY, s_script_memtrack_items, ARRAY_COUNT(s_script_memtrack_items), 0, 0, 0, 0, 0, 0 },
    { "Slabs", MENU_MEMORY, s_slabs_items, ARRAY_COUNT(s_slabs_items), 0, 0, 0, 0, 0, 0 },
    { "Full Slabs", MENU_SLABS, 0, 0, 0, 0, 0, 44, ITEM_INFO, 1 },
    { "Partial Slabs", MENU_SLABS, 0, 0, 0, 0, 0, 44, ITEM_INFO, 1 },
    { "Allocated Objects", MENU_SLABS, 0, 0, 0, 0, 0, 44, ITEM_INFO, 1 },
    { "Free Objects", MENU_SLABS, 0, 0, 0, 0, 0, 44, ITEM_INFO, 1 },
    { "Entity Variants", MENU_ROOT, 0, 0, 0, 0, 0, 0, ITEM_ENTITY_SUBMENU, 0 },
    { "Entity Variant Values", MENU_ENTITY_VARIANTS, 0, 0, 0, 0, 0, 0, ITEM_ENTITY_VARIANT, 0 },
    { "Entity Animations", MENU_ROOT, s_entity_animation_items, ARRAY_COUNT(s_entity_animation_items), 0, 0, 0, 0, 0, 0 },
    { "Level Select", MENU_ROOT, s_level_select_items, ARRAY_COUNT(s_level_select_items), 0, 0, 0, 0, 0, 0 },
    { "Hero Select", MENU_LEVEL_SELECT, s_hero_select_items, ARRAY_COUNT(s_hero_select_items), 0, 0, 0, 0, 0, 0 },
    { "Script", MENU_ROOT, s_script_items, ARRAY_COUNT(s_script_items), 0, 0, 0, 0, 0, 0 },
    { "Progression", MENU_ROOT, s_progression_items, ARRAY_COUNT(s_progression_items), 0, 0, 0, 0, 0, 0 },
    { "Char Select", MENU_ROOT, s_hero_select_items, ARRAY_COUNT(s_hero_select_items), 0, 0, 0, 0, 0, 0 },
    { "Options", MENU_ROOT, s_options_items, ARRAY_COUNT(s_options_items), 0, 0, 0, 0, 0, 0 }
};

static u8 s_open;
static u8 s_physics_user_changed;
static u8 s_saved_physics_valid;
static u8 s_single_step_pending;
static u8 s_initialized;
static u16 s_current_menu;
static u16 s_selected[MENU_COUNT];
static u16 s_window_start[MENU_COUNT];
static u16 s_previous_buttons;
static u16 s_hold_frames[4];
static s16 s_devopt_ids[DEVOPT_COUNT];
static u32 s_saved_physics_disabled;
static void *s_saved_physics_game;
static s32 s_saved_frame_lock;
static void *s_game;
static const char *s_message;
static u8 s_devopt_ids_ready;
static u8 s_slow_motion_owned;
static u8 s_entity_list_ready;
static u8 s_variant_entity_selected;
#ifdef DEBUG_MENU_AUTO_OPEN
static u8 s_auto_open_pending = 1;
static u16 s_auto_open_ready_frames;
#endif
static u16 s_variant_entity_count;
static u32 s_variant_entity_hash;
static void *s_variant_entities[MAX_VARIANT_ENTITIES];
static void *s_variant_interfaces[MAX_VARIANT_ENTITIES];
static void *s_variant_data[MAX_VARIANT_ENTITIES];
static u32 s_variant_hashes[MAX_VARIANT_ENTITIES];
static u16 s_variant_counts[MAX_VARIANT_ENTITIES];
static MissionRow s_mission_rows[MAX_MISSION_ROWS];
static MissionGroup s_mission_groups[MAX_MISSION_GROUPS];
static char s_mission_group_names[MAX_MISSION_GROUPS][32];
static u16 s_mission_row_count;
static u8 s_mission_group_count;
static u8 s_active_mission_group;
static void *s_mission_world;
static void *s_mission_manager;
static u8 s_hero_swap_pending;
static u8 s_hero_swap_physics_valid;
static u8 s_hero_swap_request_replaced;
static u32 s_hero_swap_saved_physics_disabled;
static const char *s_hero_swap_name;
static const char *s_active_hero_name;
static void *s_hero_swap_game;
static void *s_hero_swap_world;
static void *s_hero_swap_manager;

static const char *const s_camera_names[] = { "Chase", "User", "SceneAnalyzer" };

static void menu_tick_and_render(void *game);
static void close_menu(void);
static void set_message(const char *message);

static void relinquish_slow_motion_ownership(void)
{
    u32 index;
    s_slow_motion_owned = 0;
    for (index = 0; index < ARRAY_COUNT(s_game_items); ++index) {
        if (s_game_items[index].kind == ITEM_SLOW_MOTION) {
            s_game_items[index].value = 0;
            return;
        }
    }
}

/* This section is placed first by linker.ld.  The patched BL lands here. */
__attribute__((section(".hook"), used, noinline))
void debug_menu_hook(void *game)
{
    menu_tick_and_render(game);
}

static u32 string_length(const char *text)
{
    u32 length = 0;
    if (!text)
        return 0;
    while (text[length])
        ++length;
    return length;
}

static s32 string_equal(const char *left, const char *right)
{
    if (!left || !right)
        return 0;
    while (*left && *right) {
        if (*left != *right)
            return 0;
        ++left;
        ++right;
    }
    return *left == *right;
}

static void string_copy(char *out, u32 capacity, const char *text)
{
    u32 i = 0;
    if (!capacity)
        return;
    if (text) {
        while (i + 1 < capacity && text[i]) {
            out[i] = text[i];
            ++i;
        }
    }
    out[i] = 0;
}

static s32 mem1_range(const void *pointer, u32 size)
{
    u32 address = (u32)pointer;
    if (!pointer || address < 0x80000000u || address >= 0x81800000u)
        return 0;
    return size <= 0x81800000u - address;
}

static s32 mem1_word_range(const void *pointer, u32 size)
{
    return (((u32)pointer & 3u) == 0u) && mem1_range(pointer, size);
}

static const char *copy_mem1_text(char *out, u32 capacity, const char *text)
{
    u32 index = 0;
    if (!capacity)
        return out;
    out[0] = 0;
    if (!mem1_range(text, 1u)) {
        string_copy(out, capacity, "<invalid name>");
        return out;
    }
    while (index + 1u < capacity && mem1_range(text + index, 1u)) {
        char character = text[index];
        out[index] = character;
        if (!character)
            return out;
        ++index;
    }
    out[index] = 0;
    return out;
}

static void string_append(char *out, u32 capacity, const char *text)
{
    u32 length = string_length(out);
    u32 i = 0;
    if (!text || length >= capacity)
        return;
    while (length + i + 1 < capacity && text[i]) {
        out[length + i] = text[i];
        ++i;
    }
    out[length + i] = 0;
}

static void append_unsigned(char *out, u32 capacity, u32 value)
{
    char digits[11];
    u32 count = 0;
    if (!value) {
        string_append(out, capacity, "0");
        return;
    }
    while (value && count < ARRAY_COUNT(digits)) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (count) {
        char one[2];
        one[0] = digits[--count];
        one[1] = 0;
        string_append(out, capacity, one);
    }
}

static void append_signed(char *out, u32 capacity, s32 value)
{
    if (value < 0) {
        string_append(out, capacity, "-");
        append_unsigned(out, capacity, (u32)(-(value + 1)) + 1u);
    } else {
        append_unsigned(out, capacity, (u32)value);
    }
}

static s32 safe_mem1_string_length(const char *text, u32 maximum)
{
    u32 length;
    if (!text)
        return -1;
    for (length = 0; length <= maximum; ++length) {
        if (!mem1_range(text + length, 1u))
            return -1;
        if (!text[length])
            return (s32)length;
    }
    return -1;
}

static s32 pack_exists(const char *prefix, const char *name)
{
    char path[128];
    s32 name_length = safe_mem1_string_length(name, 95u);
    if (name_length <= 0)
        return 0;
    path[0] = 0;
    string_append(path, sizeof(path), prefix);
    string_append(path, sizeof(path), name);
    string_append(path, sizeof(path), ".GCPACK");
    if (string_length(path) + 1u >= sizeof(path))
        return 0;
    return ((DVDConvertPathToEntrynumFn)ADDR_DVD_CONVERT_PATH_TO_ENTRYNUM)(path) >= 0;
}

static void reset_mission_cache(void)
{
    s_mission_row_count = 0;
    s_mission_group_count = 0;
    s_active_mission_group = 0;
    s_mission_world = 0;
    s_mission_manager = 0;
}

static s32 live_hero_ready(void *world)
{
    void *hero;
    void *controller;
    if (!mem1_word_range(world, 0x228u) ||
        *(volatile s32 *)((u8 *)world + 0x224u) <= 0)
        return 0;
    hero = *(void *volatile *)((u8 *)world + 0x21Cu);
    if (!mem1_word_range(hero, 0x90u))
        return 0;
    controller = *(void *volatile *)((u8 *)hero + 0x8Cu);
    return mem1_word_range(controller, 0x424u);
}

static u16 append_mission_table(void *table, s16 district)
{
    u8 *conditions;
    u16 condition_count;
    u16 start = s_mission_row_count;
    u32 condition_index;
    if (!mem1_word_range(table, 0x48u))
        return 0;
    conditions = *(u8 *volatile *)((u8 *)table + 0x38u);
    condition_count = *(volatile u16 *)((u8 *)table + 0x3Cu);
    if (!condition_count || condition_count > 1024u ||
        !mem1_word_range(conditions, (u32)condition_count * 0x34u))
        return 0;

    for (condition_index = 0;
         condition_index < condition_count && s_mission_row_count < MAX_MISSION_ROWS;
         ++condition_index) {
        u8 *condition = conditions + condition_index * 0x34u;
        const char *name;
        u8 *instances;
        u16 instance_count;
        u32 instance_index;
        if (!((MissionConditionAppliesFn)
                  ADDR_MISSION_CONDITION_APPLIES_TO_CURRENT_HERO)(condition))
            continue;
        name = *(const char *volatile *)(condition + 0x18u);
        if (safe_mem1_string_length(name, 31u) <= 0 ||
            !pack_exists("/packs/gc/PK_", name))
            continue;
        instances = *(u8 *volatile *)(condition + 0x08u);
        instance_count = *(volatile u16 *)(condition + 0x0Cu);
        if (!instance_count || instance_count > 256u ||
            !mem1_word_range(instances, (u32)instance_count * 0x68u))
            continue;
        for (instance_index = 0;
             instance_index < instance_count && s_mission_row_count < MAX_MISSION_ROWS;
             ++instance_index) {
            void *instance = ((MissionConditionGetInstanceFn)
                ADDR_MISSION_CONDITION_GET_INSTANCE)(condition, (s32)instance_index);
            const char *script_data;
            MissionRow *row;
            if (!mem1_word_range(instance, 0x68u) ||
                instance != instances + instance_index * 0x68u)
                continue;
            script_data = ((MissionInstanceGetScriptDataNameFn)
                ADDR_MISSION_INSTANCE_GET_SCRIPT_DATA_NAME)(instance);
            if (script_data && safe_mem1_string_length(script_data, 31u) < 0)
                continue;
            row = &s_mission_rows[s_mission_row_count++];
            row->name = name;
            row->script_data = script_data;
            row->table = table;
            row->condition = condition;
            row->instance = (u16)instance_index;
            row->district = district;
        }
    }
    return (u16)(s_mission_row_count - start);
}

static void refresh_mission_cache(void)
{
    void *world = *(void *volatile *)ADDR_WORLD_SLOT;
    u8 *manager = *(u8 *volatile *)ADDR_MISSION_MANAGER_SLOT;
    void *global_table;
    s32 district_count;
    s32 district_index;
    MissionGroup *global;
    reset_mission_cache();
    if (!mem1_word_range(world, 0x38Cu) || !live_hero_ready(world) ||
        !mem1_word_range(manager, 0x3Cu) ||
        manager != (u8 *)world + 0x298u)
        return;

    global_table = *(void *volatile *)(manager + 0x14u);
    district_count = *(volatile s32 *)(manager + 0x38u);
    if (district_count < 0 || district_count > 8)
        return;

    s_mission_world = world;
    s_mission_manager = manager;
    global = &s_mission_groups[0];
    string_copy(s_mission_group_names[0], sizeof(s_mission_group_names[0]), "global");
    global->name = s_mission_group_names[0];
    global->start = 0;
    global->district = 0;
    global->reserved = 0;
    global->count = append_mission_table(global_table, 0);
    s_mission_group_count = 1;

    for (district_index = 0;
         district_index < district_count && s_mission_group_count < MAX_MISSION_GROUPS;
         ++district_index) {
        void *table = *(void *volatile *)(manager + 0x18u + (u32)district_index * 4u);
        void *region;
        const char *region_name;
        s32 district;
        u16 start;
        u16 count;
        MissionGroup *group;
        if (!mem1_word_range(table, 0x48u))
            continue;
        region = *(void *volatile *)((u8 *)table + 0x44u);
        if (!mem1_word_range(region, 0xC4u))
            continue;
        district = *(volatile s32 *)((u8 *)region + 0xC0u);
        if (district < -32768 || district > 32767)
            continue;
        start = s_mission_row_count;
        count = append_mission_table(table, (s16)district);
        if (!count)
            continue;
        group = &s_mission_groups[s_mission_group_count];
        region_name = *(const char *volatile *)((u8 *)region + 0x2Cu);
        if (safe_mem1_string_length(region_name, 31u) >= 0)
            copy_mem1_text(s_mission_group_names[s_mission_group_count],
                           sizeof(s_mission_group_names[0]), region_name);
        else
            string_copy(s_mission_group_names[s_mission_group_count],
                        sizeof(s_mission_group_names[0]), "District");
        group->name = s_mission_group_names[s_mission_group_count];
        group->start = start;
        group->count = count;
        group->district = (s16)district;
        group->reserved = 0;
        ++s_mission_group_count;
    }
}

static const char *make_mission_label(const MissionRow *row, char *buffer,
                                      u32 capacity)
{
    char safe_name[32];
    char safe_script_data[32];
    buffer[0] = 0;
    copy_mem1_text(safe_name, sizeof(safe_name), row->name);
    string_append(buffer, capacity, safe_name);
    string_append(buffer, capacity, " (");
    if (row->script_data) {
        copy_mem1_text(safe_script_data, sizeof(safe_script_data), row->script_data);
        string_append(buffer, capacity, safe_script_data);
    } else {
        append_unsigned(buffer, capacity, row->instance);
    }
    string_append(buffer, capacity, ")");
    return buffer;
}

static s32 mission_table_is_registered(void *manager, void *table, s16 district)
{
    s32 district_count;
    s32 index;
    if (!mem1_word_range(manager, 0x3Cu) || !mem1_word_range(table, 0x48u))
        return 0;
    if (*(void *volatile *)((u8 *)manager + 0x14u) == table)
        return district == 0;
    district_count = *(volatile s32 *)((u8 *)manager + 0x38u);
    if (district_count < 0 || district_count > 8)
        return 0;
    for (index = 0; index < district_count; ++index) {
        if (*(void *volatile *)((u8 *)manager + 0x18u + (u32)index * 4u) == table) {
            void *region = *(void *volatile *)((u8 *)table + 0x44u);
            return mem1_word_range(region, 0xC4u) &&
                   *(volatile s32 *)((u8 *)region + 0xC0u) == district;
        }
    }
    return 0;
}

static s32 select_cached_mission(u32 row_index)
{
    void *world = *(void *volatile *)ADDR_WORLD_SLOT;
    void *manager = *(void *volatile *)ADDR_MISSION_MANAGER_SLOT;
    MissionRow *row;
    u8 *condition;
    u8 *conditions;
    u8 *instances;
    u16 condition_count;
    u16 instance_count;
    u32 condition_offset;
    void *instance;
    const char *script_data;
    if (row_index >= s_mission_row_count || world != s_mission_world ||
        manager != s_mission_manager || !mem1_word_range(world, 0x38Cu) ||
        !mem1_word_range(manager, 0x3Cu) ||
        manager != (u8 *)world + 0x298u)
        return 0;
    row = &s_mission_rows[row_index];
    condition = (u8 *)row->condition;
    if (!mission_table_is_registered(manager, row->table, row->district))
        return 0;
    conditions = *(u8 *volatile *)((u8 *)row->table + 0x38u);
    condition_count = *(volatile u16 *)((u8 *)row->table + 0x3Cu);
    if (!condition_count || condition_count > 1024u ||
        !mem1_word_range(conditions, (u32)condition_count * 0x34u) ||
        (u32)condition < (u32)conditions)
        return 0;
    condition_offset = (u32)condition - (u32)conditions;
    if (condition_offset % 0x34u || condition_offset / 0x34u >= condition_count ||
        !mem1_word_range(condition, 0x34u) ||
        *(const char *volatile *)(condition + 0x18u) != row->name ||
        safe_mem1_string_length(row->name, 31u) <= 0 ||
        !live_hero_ready(world) ||
        !((MissionConditionAppliesFn)
              ADDR_MISSION_CONDITION_APPLIES_TO_CURRENT_HERO)(condition))
        return 0;
    instances = *(u8 *volatile *)(condition + 0x08u);
    instance_count = *(volatile u16 *)(condition + 0x0Cu);
    if (row->instance >= instance_count ||
        !mem1_word_range(instances, (u32)instance_count * 0x68u))
        return 0;
    instance = ((MissionConditionGetInstanceFn)
        ADDR_MISSION_CONDITION_GET_INSTANCE)(condition, row->instance);
    if (instance != instances + (u32)row->instance * 0x68u)
        return 0;
    script_data = ((MissionInstanceGetScriptDataNameFn)
        ADDR_MISSION_INSTANCE_GET_SCRIPT_DATA_NAME)(instance);
    if (script_data != row->script_data ||
        (script_data && safe_mem1_string_length(script_data, 31u) < 0) ||
        !pack_exists("/packs/gc/PK_", row->name))
        return 0;
    ((MissionForceMissionFn)ADDR_MISSION_FORCE_MISSION)(
        manager, row->district, row->name, row->instance, row->script_data);
    return 1;
}

static void restore_hero_swap_physics(void)
{
    if (s_hero_swap_physics_valid && s_game == s_hero_swap_game &&
        mem1_word_range(s_game, 0x1ACu))
        *(volatile u32 *)((u8 *)s_game + 0x1A8u) =
            s_hero_swap_saved_physics_disabled;
    s_hero_swap_physics_valid = 0;
    s_hero_swap_game = 0;
}

static s32 queue_hero_swap(const char *name)
{
    void *world = *(void *volatile *)ADDR_WORLD_SLOT;
    void *manager = *(void *volatile *)ADDR_MISSION_MANAGER_SLOT;
    void *devopts = *(void *volatile *)ADDR_DEVOPT_SINGLETON_SLOT;
    s32 name_length = safe_mem1_string_length(name, 31u);
    u8 *desired_name;
    u32 index;
    u32 temporary_string[4];
    if (!s_game || ((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game) != 6) {
        set_message("Hero swap requires active gameplay");
        return 0;
    }
    if (name_length <= 0 || !pack_exists("/packs/gc/", name)) {
        set_message("Selected hero pack is not on this disc");
        return 0;
    }
    if (!mem1_word_range(world, 0x38Cu) ||
        !mem1_word_range(manager, 0xF4u) ||
        manager != (u8 *)world + 0x298u ||
        *(volatile s32 *)((u8 *)world + 0x224u) != 1) {
        set_message("Hero swap requires exactly one live player");
        return 0;
    }
    if (*(volatile s32 *)((u8 *)manager + 0xF0u) != -1) {
        set_message("The engine is already switching heroes");
        return 0;
    }

    if (mem1_word_range(devopts, 0x290u)) {
        ((MStringFromCharFn)ADDR_MSTRING_FROM_CHAR)(temporary_string, name);
        ((DevoptSetStringFn)ADDR_DEVOPT_SET_STRING)(devopts, 2, temporary_string);
        ((MStringFinalizeFn)ADDR_MSTRING_FINALIZE)(temporary_string, 0);
    }

    desired_name = (u8 *)manager + 0xD0u;
    for (index = 0; index < 32u; ++index)
        desired_name[index] = 0;
    for (index = 0; index < (u32)name_length; ++index)
        desired_name[index] = (u8)name[index];

    s_hero_swap_name = name;
    s_hero_swap_game = s_game;
    s_hero_swap_world = world;
    s_hero_swap_manager = manager;
    s_hero_swap_saved_physics_disabled =
        *(volatile u32 *)((u8 *)s_game + 0x1A8u);
    s_hero_swap_physics_valid = 1;
    s_hero_swap_request_replaced = 0;
    s_hero_swap_pending = 1;
    set_message("Switching hero...");

    /*
     * mission_manager::frame_advance skips update_hero_switch while physics is
     * paused. Temporarily release the menu pause for the whole retail state
     * machine; update_hero_swap_status restores the exact prior raw state.
     */
    ((GameEnablePhysicsFn)ADDR_GAME_ENABLE_PHYSICS)(s_game, 1);
    /* Publish the request last; retail update_hero_switch owns it from here. */
    *(volatile s32 *)((u8 *)manager + 0xF0u) = 0;
    return 1;
}

static void update_hero_swap_status(void)
{
    void *world;
    void *manager;
    s32 frame;
    if (!s_hero_swap_pending)
        return;
    world = *(void *volatile *)ADDR_WORLD_SLOT;
    manager = *(void *volatile *)ADDR_MISSION_MANAGER_SLOT;
    if (world != s_hero_swap_world || manager != s_hero_swap_manager ||
        s_game != s_hero_swap_game ||
        !mem1_word_range(world, 0x38Cu) ||
        !mem1_word_range(manager, 0xF4u) ||
        manager != (u8 *)world + 0x298u) {
        s_hero_swap_pending = 0;
        restore_hero_swap_physics();
        set_message("Hero swap stopped because the world changed");
        return;
    }
    frame = *(volatile s32 *)((u8 *)manager + 0xF0u);
    if (frame >= 0 && !s_hero_swap_request_replaced &&
        (safe_mem1_string_length((const char *)manager + 0xD0u, 31u) < 0 ||
         !string_equal((const char *)manager + 0xD0u, s_hero_swap_name))) {
        s_hero_swap_request_replaced = 1;
        set_message("The engine replaced the hero swap request");
    }
    if (frame == -1) {
        if (*(volatile s32 *)((u8 *)world + 0x224u) == 1) {
            s_hero_swap_pending = 0;
            restore_hero_swap_physics();
            if (!s_hero_swap_request_replaced)
                s_active_hero_name = s_hero_swap_name;
            close_menu();
        } else {
            s_hero_swap_pending = 0;
            restore_hero_swap_physics();
            set_message("Hero swap failed; reload a mission");
        }
    } else if (frame < 0 || frame > 3) {
        if (*(volatile s32 *)((u8 *)world + 0x224u) == 1) {
            s_hero_swap_pending = 0;
            restore_hero_swap_physics();
            set_message("Hero swap state was replaced by the engine");
        } else {
            s_hero_swap_pending = 0;
            restore_hero_swap_physics();
            set_message("Hero swap failed; reload a mission");
        }
    } else {
        /* The open menu normally owns a pause, so guard every pending frame. */
        ((GameEnablePhysicsFn)ADDR_GAME_ENABLE_PHYSICS)(s_game, 1);
    }
}

static void *get_font(void)
{
    return *(void *volatile *)ADDR_NGL_SYSFONT_SLOT;
}

static void add_text(float x, float y, u32 color, const char *text)
{
    void *font = get_font();
    if (!font || !text)
        return;
    ((NglListAddStringFn)ADDR_NGL_LIST_ADD_STRING_SCALED)(
        font, x + 1.0f, y + 1.0f, 0.16f, 0x000000D0u,
        MENU_TEXT_SCALE, MENU_TEXT_SCALE, "%s", text);
    ((NglListAddStringFn)ADDR_NGL_LIST_ADD_STRING_SCALED)(
        font, x, y, 0.15f, color, MENU_TEXT_SCALE, MENU_TEXT_SCALE, "%s", text);
}

static u32 get_text_width(const char *text)
{
    u32 width = 0;
    u32 height = 0;
    void *font = get_font();
    if (font && text) {
        ((NglGetStringDimensionsFn)ADDR_NGL_GET_STRING_DIMENSIONS_SCALED)(
            font, text, &width, &height, MENU_TEXT_SCALE, MENU_TEXT_SCALE);
    }
    return width;
}

static u32 get_line_height(void)
{
    u32 width = 0;
    u32 height = 0;
    void *font = get_font();
    if (font) {
        ((NglGetStringDimensionsFn)ADDR_NGL_GET_STRING_DIMENSIONS_SCALED)(
            font, "Ag", &width, &height, MENU_TEXT_SCALE, MENU_TEXT_SCALE);
    }
    if (height < 10u || height > 24u)
        height = 15u;
    return height;
}

static void *get_devopts(void)
{
    return *(void *volatile *)ADDR_DEVOPT_SINGLETON_SLOT;
}

static s32 get_devflag(s32 id)
{
    void *devopts = get_devopts();
    if (!devopts)
        return 0;
    return ((DevoptGetFlagFn)ADDR_DEVOPT_GET_FLAG)(devopts, id) != 0;
}

static void set_devflag(s32 id, s32 enabled)
{
    void *devopts = get_devopts();
    if (devopts)
        ((DevoptSetFlagFn)ADDR_DEVOPT_SET_FLAG)(devopts, id, enabled != 0);
}

static s32 get_devint(s32 id)
{
    void *devopts = get_devopts();
    if (!devopts)
        return 0;
    return ((DevoptGetIntFn)ADDR_DEVOPT_GET_INT)(devopts, id);
}

static void set_devint(s32 id, s32 value)
{
    void *devopts = get_devopts();
    if (devopts)
        ((DevoptSetIntFn)ADDR_DEVOPT_SET_INT)(devopts, id, value);
}

static s32 find_name_id(u32 table_address, u32 count, const char *wanted)
{
    const char *const volatile *names = (const char *const volatile *)table_address;
    u32 index;
    for (index = 0; index < count; ++index) {
        if (string_equal(names[index], wanted))
            return (s32)index;
    }
    return -1;
}

static void initialize_devopt_ids(void)
{
    u32 row;
    for (row = 0; row < DEVOPT_COUNT; ++row) {
        if (kDevoptKinds[row] == ITEM_BOOL) {
            s_devopt_ids[row] = (s16)find_name_id(
                ADDR_DEVOPT_FLAG_NAME_TABLE, ADDR_DEVOPT_FLAG_COUNT,
                kDevoptLabels[row]);
        } else if (kDevoptKinds[row] == ITEM_INT) {
            s_devopt_ids[row] = (s16)find_name_id(
                ADDR_DEVOPT_INT_NAME_TABLE, ADDR_DEVOPT_INT_COUNT,
                kDevoptLabels[row]);
        } else {
            s_devopt_ids[row] = -1;
        }
    }
    s_devopt_ids_ready = 1;
}

static s32 devopt_id_for_row(u32 row)
{
    if (!s_devopt_ids_ready)
        initialize_devopt_ids();
    if (row >= DEVOPT_COUNT)
        return -1;
    return s_devopt_ids[row];
}

static void *get_terrain(void)
{
    void *world = *(void *volatile *)ADDR_WORLD_SLOT;
    void *terrain;
    if (!mem1_word_range(world, 0x19Cu))
        return 0;
    terrain = *(void *volatile *)((u8 *)world + 0x198u);
    return mem1_word_range(terrain, 0x10u) ? terrain : 0;
}

static s32 get_district_regions(void *terrain, void ***regions_out, u32 *count_out)
{
    void **regions;
    s32 region_count;
    if (!mem1_word_range(terrain, 0x10u))
        return 0;
    regions = *(void **volatile *)terrain;
    region_count = *(volatile s32 *)((u8 *)terrain + 0x0Cu);
    if (region_count < 0 || (u32)region_count > 1024u)
        return 0;
    if (region_count && !mem1_word_range(regions, (u32)region_count * 4u))
        return 0;
    *regions_out = regions;
    *count_out = (u32)region_count;
    return 1;
}

static void *district_variant_region_at(u32 wanted_row, u32 *index_out,
                                        s32 *district_out)
{
    void *terrain = get_terrain();
    void **regions;
    u32 region_count;
    u32 region_index;
    u32 row = 0;
    if (!get_district_regions(terrain, &regions, &region_count))
        return 0;
    for (region_index = 0; region_index < region_count; ++region_index) {
        void *region = regions[region_index];
        s32 variant_count;
        s32 current;
        if (!mem1_word_range(region, 0xCCu))
            continue;
        variant_count = *(volatile s32 *)((u8 *)region + 0xC8u);
        if (variant_count < 2 || variant_count > 256)
            continue;
        current = ((RegionGetDistrictVariantFn)ADDR_REGION_GET_DISTRICT_VARIANT)(region);
        if (current < 0 || current >= variant_count)
            continue;
        if (row == wanted_row) {
            *index_out = region_index;
            *district_out = *(volatile s32 *)((u8 *)region + 0xC0u);
            return region;
        }
        ++row;
    }
    return 0;
}

static u32 district_variant_row_count(void)
{
    void *terrain = get_terrain();
    void **regions;
    u32 region_count;
    u32 region_index;
    u32 count = 0;
    if (!get_district_regions(terrain, &regions, &region_count))
        return 0;
    for (region_index = 0; region_index < region_count; ++region_index) {
        void *region = regions[region_index];
        s32 variant_count;
        s32 current;
        if (!mem1_word_range(region, 0xCCu))
            continue;
        variant_count = *(volatile s32 *)((u8 *)region + 0xC8u);
        if (variant_count < 2 || variant_count > 256)
            continue;
        current = ((RegionGetDistrictVariantFn)ADDR_REGION_GET_DISTRICT_VARIANT)(region);
        if (current >= 0 && current < variant_count)
            ++count;
    }
    return count;
}

static s32 get_entity_variant_info(void *entity, void **interface_out,
                                   void **data_out, u16 *count_out)
{
    void *interface;
    void *data;
    u16 count;
    if (!mem1_word_range(entity, 0x130u))
        return 0;
    if (!((ObjectPredicateFn)ADDR_ENTITY_IS_CONGLOMERATE)(entity) ||
        !((ObjectPredicateFn)ADDR_CONGLOMERATE_HAS_VARIANT_IFC)(entity))
        return 0;
    interface = ((ObjectGetterFn)ADDR_CONGLOMERATE_VARIANT_IFC)(entity);
    if (!mem1_word_range(interface, 0x12u))
        return 0;
    data = *(void *volatile *)((u8 *)interface + 0x0Cu);
    count = *(volatile u16 *)((u8 *)interface + 0x10u);
    if (!count || count > 1024u || !mem1_word_range(data, (u32)count * 0x10u))
        return 0;
    *interface_out = interface;
    *data_out = data;
    *count_out = count;
    return 1;
}

static void clear_variant_entity_cache(void)
{
    s_variant_entity_count = 0;
}

static void refresh_variant_entity_cache(void)
{
    void *world;
    void *list;
    void *sentinel;
    void *previous;
    void *node;
    u32 steps = 0;
    if (s_entity_list_ready)
        return;
    s_entity_list_ready = 1;
    clear_variant_entity_cache();

    world = *(void *volatile *)ADDR_WORLD_SLOT;
    if (!mem1_word_range(world, 0x78u))
        return;
    ((EntityFindEntitiesFn)ADDR_ENTITY_FIND_ENTITIES)(0x100);
    list = *(void *volatile *)ADDR_FOUND_ENTITIES_SLOT;
    if (!mem1_word_range(list, 8u))
        return;
    sentinel = *(void *volatile *)((u8 *)list + 4u);
    if (!mem1_word_range(sentinel, 12u))
        return;

    previous = sentinel;
    node = *(void *volatile *)sentinel;
    while (steps < MAX_ENTITY_LIST_NODES) {
        void *next;
        void *entity;
        void *interface;
        void *data;
        u16 variant_count;
        if (node == sentinel) {
            if (*(void *volatile *)((u8 *)sentinel + 4u) != previous)
                clear_variant_entity_cache();
            return;
        }
        if (!mem1_word_range(node, 12u) ||
            *(void *volatile *)((u8 *)node + 4u) != previous) {
            clear_variant_entity_cache();
            return;
        }
        next = *(void *volatile *)node;
        if (next == node || !mem1_word_range(next, 12u) ||
            *(void *volatile *)((u8 *)next + 4u) != node) {
            clear_variant_entity_cache();
            return;
        }
        entity = *(void *volatile *)((u8 *)node + 8u);
        if (get_entity_variant_info(entity, &interface, &data, &variant_count) &&
            s_variant_entity_count < MAX_VARIANT_ENTITIES) {
            u32 slot = s_variant_entity_count++;
            s_variant_entities[slot] = entity;
            s_variant_interfaces[slot] = interface;
            s_variant_data[slot] = data;
            s_variant_hashes[slot] = *(volatile u32 *)((u8 *)entity + 0x10u);
            s_variant_counts[slot] = variant_count;
        }
        previous = node;
        node = next;
        ++steps;
    }
    clear_variant_entity_cache();
}

static s32 selected_variant_entity_index(void)
{
    u32 index;
    if (!s_variant_entity_selected)
        return -1;
    refresh_variant_entity_cache();
    for (index = 0; index < s_variant_entity_count; ++index) {
        if (s_variant_hashes[index] == s_variant_entity_hash)
            return (s32)index;
    }
    return -1;
}

static s32 apply_entity_variant(u32 wanted_hash)
{
    s32 selected_index;
    void *interface;
    u8 *data;
    u16 count;
    u32 index;

    /* find_entities rebuilds its list, so re-resolve every pointer by entity ID. */
    s_entity_list_ready = 0;
    refresh_variant_entity_cache();
    selected_index = selected_variant_entity_index();
    if (selected_index < 0)
        return 0;
    interface = s_variant_interfaces[(u32)selected_index];
    data = (u8 *)s_variant_data[(u32)selected_index];
    count = s_variant_counts[(u32)selected_index];
    if (!mem1_word_range(interface, 0x12u) ||
        !mem1_word_range(data, (u32)count * 0x10u))
        return 0;
    for (index = 0; index < count; ++index) {
        u32 *hash = (u32 *)(data + index * 0x10u);
        if (*(volatile u32 *)hash == wanted_hash) {
            ((VariantApplyFn)ADDR_VARIANT_INTERFACE_APPLY_VARIANT)(interface, hash);
            return 1;
        }
    }
    return 0;
}

static u32 menu_row_count(u16 menu_id)
{
    MenuDef *menu = &s_menus[menu_id];
    if (menu_id == MENU_MISSIONS) {
        u32 global_count = s_mission_group_count ? s_mission_groups[0].count : 0u;
        u32 district_groups = s_mission_group_count > 1u
            ? (u32)s_mission_group_count - 1u : 0u;
        u32 dynamic_count = global_count + district_groups;
        return (u32)menu->item_count + (dynamic_count ? dynamic_count : 1u);
    }
    if (menu_id == MENU_MISSION_DISTRICT) {
        if (s_active_mission_group >= s_mission_group_count)
            return 0;
        return s_mission_groups[s_active_mission_group].count;
    }
    if (menu_id == MENU_DISTRICT_VARIANTS)
        return district_variant_row_count();
    if (menu_id == MENU_ENTITY_VARIANTS) {
        refresh_variant_entity_cache();
        return s_variant_entity_count;
    }
    if (menu_id == MENU_ENTITY_VARIANT_VALUES) {
        s32 index = selected_variant_entity_index();
        return index >= 0 ? s_variant_counts[(u32)index] : 0u;
    }
    return (u32)menu->item_count + (u32)menu->label_count;
}

static const char *make_slab_label(u32 row, char *buffer, u32 capacity)
{
    u32 bytes = (row + 1u) * 4u;
    buffer[0] = 0;
    if (bytes < 100u)
        string_append(buffer, capacity, " ");
    if (bytes < 10u)
        string_append(buffer, capacity, " ");
    append_unsigned(buffer, capacity, bytes);
    string_append(buffer, capacity, " byte");
    return buffer;
}

static Row get_row(u16 menu_id, u32 row_index, char *generated, u32 generated_capacity)
{
    MenuDef *menu = &s_menus[menu_id];
    Row row;
    row.label = "";
    row.value = 0;
    row.minimum = 0;
    row.maximum = 0;
    row.target = MENU_NONE;
    row.kind = ITEM_INFO;
    row.action = ACTION_NONE;
    row.context = 0;
    row.context_index = 0;
    row.context_id = 0;

    if (menu_id == MENU_DISTRICT_VARIANTS) {
        u32 region_index;
        s32 district;
        void *region = district_variant_region_at(row_index, &region_index, &district);
        const char *name;
        if (!region)
            return row;
        name = *(const char *volatile *)((u8 *)region + 0x2Cu);
        row.label = copy_mem1_text(generated, generated_capacity, name);
        row.minimum = 0;
        row.maximum = *(volatile s32 *)((u8 *)region + 0xC8u) - 1;
        row.kind = ITEM_DISTRICT_VARIANT;
        row.context = region;
        row.context_index = region_index;
        row.context_id = district;
        return row;
    }

    if (menu_id == MENU_ENTITY_VARIANTS) {
        void *entity;
        const u32 *hash;
        const char *name;
        refresh_variant_entity_cache();
        if (row_index >= s_variant_entity_count)
            return row;
        entity = s_variant_entities[row_index];
        if (!mem1_word_range(entity, 0x14u))
            return row;
        hash = (const u32 *)((u8 *)entity + 0x10u);
        name = ((StringHashToStringFn)ADDR_STRING_HASH_TO_STRING)(hash);
        row.label = copy_mem1_text(generated, generated_capacity, name);
        row.target = MENU_ENTITY_VARIANT_VALUES;
        row.kind = ITEM_ENTITY_SUBMENU;
        row.context_id = (s32)*hash;
        return row;
    }

    if (menu_id == MENU_ENTITY_VARIANT_VALUES) {
        s32 entity_index = selected_variant_entity_index();
        u8 *data;
        u32 *hash;
        const char *name;
        if (entity_index < 0 || row_index >= s_variant_counts[(u32)entity_index])
            return row;
        data = (u8 *)s_variant_data[(u32)entity_index];
        hash = (u32 *)(data + row_index * 0x10u);
        if (!mem1_word_range(hash, 4u))
            return row;
        name = ((StringHashToStringFn)ADDR_STRING_HASH_TO_STRING)(hash);
        row.label = copy_mem1_text(generated, generated_capacity, name);
        row.kind = ITEM_ENTITY_VARIANT;
        row.context_id = (s32)*hash;
        return row;
    }

    if (menu_id == MENU_MISSIONS && row_index >= menu->item_count) {
        u32 dynamic_index = row_index - menu->item_count;
        u32 global_count = s_mission_group_count ? s_mission_groups[0].count : 0u;
        u32 district_groups = s_mission_group_count > 1u
            ? (u32)s_mission_group_count - 1u : 0u;
        if (!global_count && !district_groups) {
            row.label = s_mission_world
                ? "No compatible mission packs found"
                : "Missions require a loaded hero";
            return row;
        }
        if (dynamic_index < global_count) {
            u32 mission_index = (u32)s_mission_groups[0].start + dynamic_index;
            row.label = make_mission_label(&s_mission_rows[mission_index], generated,
                                           generated_capacity);
            row.kind = ITEM_MISSION;
            row.context = s_mission_rows[mission_index].condition;
            row.context_index = mission_index;
            row.context_id = s_mission_rows[mission_index].district;
            return row;
        }
        dynamic_index -= global_count;
        if (dynamic_index < district_groups) {
            u32 group_index = dynamic_index + 1u;
            row.label = s_mission_groups[group_index].name;
            row.target = MENU_MISSION_DISTRICT;
            row.kind = ITEM_MISSION_DISTRICT;
            row.context_index = group_index;
            row.context_id = s_mission_groups[group_index].district;
        }
        return row;
    }

    if (menu_id == MENU_MISSION_DISTRICT) {
        MissionGroup *group;
        u32 mission_index;
        if (s_active_mission_group >= s_mission_group_count)
            return row;
        group = &s_mission_groups[s_active_mission_group];
        if (row_index >= group->count)
            return row;
        mission_index = (u32)group->start + row_index;
        row.label = make_mission_label(&s_mission_rows[mission_index], generated,
                                       generated_capacity);
        row.kind = ITEM_MISSION;
        row.context = s_mission_rows[mission_index].condition;
        row.context_index = mission_index;
        row.context_id = s_mission_rows[mission_index].district;
        return row;
    }

    if (row_index < menu->item_count) {
        MenuItem *item = &menu->items[row_index];
        row.label = item->label;
        row.value = &item->value;
        row.minimum = item->minimum;
        row.maximum = item->maximum;
        row.target = item->target;
        row.kind = item->kind;
        row.action = item->action;
        return row;
    }

    row_index -= menu->item_count;
    if (row_index >= menu->label_count)
        return row;
    if (menu->generated_rows)
        row.label = make_slab_label(row_index, generated, generated_capacity);
    else
        row.label = menu->labels[row_index];
    row.value = menu->values ? &menu->values[row_index] : 0;
    row.minimum = -1000;
    row.maximum = 1000;
    row.kind = menu->label_kinds ? menu->label_kinds[row_index] : menu->dynamic_kind;
    if (menu_id == MENU_DEVOPTS && row.kind == ITEM_INT) {
        if (string_equal(row.label, "GOD_MODE")) {
            row.minimum = 0;
            row.maximum = 5;
        } else if (string_equal(row.label, "FRAME_LOCK")) {
            row.minimum = 0;
            row.maximum = 120;
        } else {
            row.kind = ITEM_DEVINT_READONLY;
        }
    }
    return row;
}

static s32 physics_enabled(void)
{
    if (!s_game)
        return 0;
    return *(volatile u32 *)((u8 *)s_game + 0x1A8u) == 0u;
}

static s32 row_value(u16 menu_id, u32 row_index, const Row *row)
{
    s32 id;
    if (row->kind == ITEM_PHYSICS)
        return physics_enabled();
    if (row->kind == ITEM_PAUSE)
        return s_game ? !physics_enabled() : 0;
    if (row->kind == ITEM_DEVFLAG)
        return get_devflag(row->value ? *row->value : 0);
    if (row->kind == ITEM_DEVINT)
        return get_devint(row->value ? *row->value : 0);
    if (row->kind == ITEM_SLOW_MOTION)
        return row->value ? *row->value : 0;
    if (row->kind == ITEM_CAMERA) {
        if (((GeometryIsSceneAnalyzerEnabledFn)ADDR_GEOMETRY_IS_SCENE_ANALYZER_ENABLED)())
            return 2;
        return s_game && *(volatile u32 *)((u8 *)s_game + 0x1C8u) ? 1 : 0;
    }
    if (row->kind == ITEM_SHOW_DISTRICTS)
        return get_devflag(6);
    if (row->kind == ITEM_DISTRICT_VARIANT &&
        mem1_word_range(row->context, 0xCCu))
        return ((RegionGetDistrictVariantFn)ADDR_REGION_GET_DISTRICT_VARIANT)(row->context);
    if (row->kind == ITEM_BOOL && row->label &&
        string_equal(row->label, "Rumble Enabled")) {
        void *input = *(void *volatile *)ADDR_INPUT_SINGLETON_SLOT;
        void *rumble;
        if (!mem1_word_range(input, 0x2Cu))
            return row->value ? *row->value : 0;
        rumble = *(void *volatile *)((u8 *)input + 4u);
        if (mem1_word_range(rumble, 4u))
            return (*(volatile u32 *)((u8 *)input + 0x28u) & 2u) == 0u;
    }
    if (menu_id == MENU_DEVOPTS && row_index < DEVOPT_COUNT) {
        id = devopt_id_for_row(row_index);
        if (id >= 0 && kDevoptKinds[row_index] == ITEM_BOOL)
            return get_devflag(id);
        if (id >= 0 && kDevoptKinds[row_index] == ITEM_INT)
            return get_devint(id);
        return 0;
    }
    return row->value ? *row->value : 0;
}

static void set_message(const char *message)
{
    s_message = message;
}

static void apply_rumble_enabled(s32 enabled)
{
    void *input = *(void *volatile *)ADDR_INPUT_SINGLETON_SLOT;
    void *rumble;
    if (!mem1_word_range(input, 8u))
        return;
    rumble = *(void **)((u8 *)input + 4u);
    if (!mem1_word_range(rumble, 4u))
        return;
    if (enabled)
        ((RumbleModeFn)ADDR_RUMBLE_ENABLE)(rumble);
    else
        ((RumbleModeFn)ADDR_RUMBLE_DISABLE)(rumble);
}

static void run_action(u8 action, const char *label)
{
    switch (action) {
    case ACTION_SINGLE_STEP:
        if (s_game) {
            ((GameEnablePhysicsFn)ADDR_GAME_ENABLE_PHYSICS)(s_game, 1);
            s_single_step_pending = 1;
            set_message("Single-step armed for the next game frame");
        }
        break;
    case ACTION_NGL_SCREENSHOT:
        *(volatile u8 *)(ADDR_NGL_DEBUG_FLAGS + 5u) = 1u;
        set_message("NGL screenshot requested");
        break;
    case ACTION_SAVE_GAME: set_message("Save Game is Xbox-only in this safe GC port"); break;
    case ACTION_LOAD_GAME: set_message("Load Game is Xbox-only in this safe GC port"); break;
    case ACTION_AUTO_LOAD: set_message("Auto Load is Xbox-only in this safe GC port"); break;
    case ACTION_HIRES_SCREENSHOT: set_message("Hires Screenshot is not exposed by retail GC NGL"); break;
    case ACTION_LORES_SCREENSHOT:
        *(volatile u8 *)(ADDR_NGL_DEBUG_FLAGS + 5u) = 1u;
        set_message("Lores screenshot requested through retail GC NGL");
        break;
    case ACTION_WARP_POI: set_message("POI warp needs a live mission arrow target"); break;
    case ACTION_UNLOAD_MISSION: {
        void *world = *(void *volatile *)ADDR_WORLD_SLOT;
        void *manager = *(void *volatile *)ADDR_MISSION_MANAGER_SLOT;
        if (mem1_word_range(world, 0x38Cu) &&
            mem1_word_range(manager, 0x54u) &&
            manager == (u8 *)world + 0x298u) {
            ((MissionPrepareUnloadScriptFn)ADDR_MISSION_PREPARE_UNLOAD_SCRIPT)(
                manager);
            close_menu();
        } else {
            set_message("No mission manager is loaded");
        }
        break;
    }
    case ACTION_REPLAY_START: set_message("Replay playback was linker-removed from retail GC"); break;
    case ACTION_MEMTRACK_DUMP: set_message("MemTrack code is stubbed in the retail GameCube build"); break;
    case ACTION_MEMTRACK_CHECKPOINT: set_message("MemTrack code is stubbed in the retail GameCube build"); break;
    case ACTION_REBOOT: set_message("Xbox XBE reboot has no GameCube equivalent"); break;
    case ACTION_SELECT_HERO:
        queue_hero_swap(label);
        break;
    case ACTION_UNLOAD_DISTRICTS: set_message("District unload request is not bound to an unsafe REL address"); break;
    default: set_message("Action acknowledged"); break;
    }
}

static void open_menu(void)
{
    s_open = 1;
    s_current_menu = MENU_ROOT;
    s_physics_user_changed = 0;
    s_saved_physics_valid = 0;
    s_saved_physics_game = 0;
    if (s_game) {
        s_saved_physics_disabled = *(volatile u32 *)((u8 *)s_game + 0x1A8u);
        s_saved_physics_game = s_game;
        s_saved_physics_valid = 1;
        ((GameEnablePhysicsFn)ADDR_GAME_ENABLE_PHYSICS)(s_game, 0);
    }
    set_message(0);
}

static void close_menu(void)
{
    if (s_saved_physics_valid && s_game == s_saved_physics_game &&
        !s_physics_user_changed)
        *(volatile u32 *)((u8 *)s_game + 0x1A8u) = s_saved_physics_disabled;
    s_saved_physics_valid = 0;
    s_saved_physics_game = 0;
    s_open = 0;
}

static void change_row(s32 direction)
{
    char generated[96];
    u16 menu_id = s_current_menu;
    u32 selected = s_selected[menu_id];
    Row row = get_row(menu_id, selected, generated, sizeof(generated));
    s32 value = row_value(menu_id, selected - (selected >= s_menus[menu_id].item_count
                                                ? s_menus[menu_id].item_count : 0u), &row);

    if (row.kind == ITEM_MISSION) {
        if (direction > 0) {
            if (select_cached_mission(row.context_index))
                close_menu();
            else
                set_message("Mission table changed; reopen Missions");
        }
        return;
    }
    if (row.kind == ITEM_MISSION_DISTRICT) {
        if (direction > 0 && row.context_index < s_mission_group_count) {
            s_active_mission_group = (u8)row.context_index;
            s_menus[MENU_MISSION_DISTRICT].title =
                s_mission_groups[s_active_mission_group].name;
            s_selected[MENU_MISSION_DISTRICT] = 0;
            s_window_start[MENU_MISSION_DISTRICT] = 0;
            s_current_menu = MENU_MISSION_DISTRICT;
            set_message(0);
        }
        return;
    }
    if (row.kind == ITEM_ENTITY_SUBMENU) {
        if (direction > 0) {
            s_variant_entity_hash = (u32)row.context_id;
            s_variant_entity_selected = 1;
            s_selected[MENU_ENTITY_VARIANT_VALUES] = 0;
            s_window_start[MENU_ENTITY_VARIANT_VALUES] = 0;
            s_current_menu = MENU_ENTITY_VARIANT_VALUES;
            set_message("Live variants resolved for the selected entity");
        }
        return;
    }
    if (row.kind == ITEM_ENTITY_VARIANT) {
        if (direction > 0) {
            if (apply_entity_variant((u32)row.context_id))
                set_message("Entity variant applied");
            else
                set_message("Entity changed before the variant could be applied");
        }
        return;
    }
    if (row.kind == ITEM_SUBMENU) {
        if (direction > 0 && row.target < MENU_COUNT) {
            if (row.target == MENU_MISSIONS) {
                refresh_mission_cache();
                s_selected[MENU_MISSIONS] = 0;
                s_window_start[MENU_MISSIONS] = 0;
            }
            s_current_menu = row.target;
            set_message(0);
        }
        return;
    }
    if (row.kind == ITEM_ACTION) {
        if (direction > 0)
            run_action(row.action, row.label);
        return;
    }
    if (row.kind == ITEM_INFO || row.kind == ITEM_UNAVAILABLE ||
        row.kind == ITEM_DEVINT_READONLY)
        return;
    if (row.kind == ITEM_PHYSICS) {
        value = !value;
        if (s_game) {
            ((GameEnablePhysicsFn)ADDR_GAME_ENABLE_PHYSICS)(s_game, value);
            s_physics_user_changed = 1;
        }
        return;
    }
    if (row.kind == ITEM_PAUSE) {
        value = !value;
        if (s_game) {
            ((GameEnablePhysicsFn)ADDR_GAME_ENABLE_PHYSICS)(s_game, value ? 0 : 1);
            s_physics_user_changed = 1;
            set_message(value ? "Gameplay paused" : "Gameplay unpaused");
        }
        return;
    }
    if (row.kind == ITEM_DEVFLAG) {
        set_devflag(row.value ? *row.value : 0, !value);
        return;
    }
    if (row.kind == ITEM_DEVINT) {
        value += direction;
        if (value > row.maximum)
            value = row.maximum;
        if (value < row.minimum)
            value = row.minimum;
        set_devint(row.value ? *row.value : 0, value);
        if (row.value && *row.value == 29)
            ((SetGodModeFn)ADDR_SET_GOD_MODE)(value);
        return;
    }
    if (row.kind == ITEM_SLOW_MOTION) {
        if (value) {
            if (s_slow_motion_owned)
                set_devint(13, s_saved_frame_lock);
            relinquish_slow_motion_ownership();
        } else {
            s_saved_frame_lock = get_devint(13);
            set_devint(13, 120);
            s_slow_motion_owned = 1;
            if (row.value)
                *row.value = 1;
        }
        close_menu();
        return;
    }
    if (row.kind == ITEM_CAMERA) {
        value += direction;
        if (value > 2)
            value = 0;
        if (value < 0)
            value = 2;
        if (!s_game)
            return;
        if (value == 2) {
            *(volatile u32 *)((u8 *)s_game + 0x1C8u) = 0u;
            ((GeometryEnableSceneAnalyzerFn)ADDR_GEOMETRY_ENABLE_SCENE_ANALYZER)(1);
        } else {
            if (((GeometryIsSceneAnalyzerEnabledFn)ADDR_GEOMETRY_IS_SCENE_ANALYZER_ENABLED)())
                ((GeometryEnableSceneAnalyzerFn)ADDR_GEOMETRY_ENABLE_SCENE_ANALYZER)(0);
            *(volatile u32 *)((u8 *)s_game + 0x1C8u) = value == 1 ? 1u : 0u;
        }
        return;
    }
    if (row.kind == ITEM_SHOW_DISTRICTS) {
        value = !value;
        set_devflag(6, value);
        if (value)
            set_devflag(88, 1);
        set_message("Streamer district overlay changed; refresh hook is unavailable");
        return;
    }
    if (row.kind == ITEM_DISTRICT_VARIANT) {
        void *terrain = get_terrain();
        void **regions;
        u32 region_count;
        void *region;
        s32 district;
        s32 variant_count;
        if (!get_district_regions(terrain, &regions, &region_count) ||
            row.context_index >= region_count)
            return;
        region = regions[row.context_index];
        if (!mem1_word_range(region, 0xCCu))
            return;
        district = *(volatile s32 *)((u8 *)region + 0xC0u);
        variant_count = *(volatile s32 *)((u8 *)region + 0xC8u);
        if (district != row.context_id || variant_count < 2 || variant_count > 256)
            return;
        value = ((RegionGetDistrictVariantFn)ADDR_REGION_GET_DISTRICT_VARIANT)(region);
        if (value < 0 || value >= variant_count)
            return;
        value += direction;
        if (value >= variant_count)
            value = variant_count - 1;
        if (value < 0)
            value = 0;
        ((TerrainSetDistrictVariantFn)ADDR_TERRAIN_SET_DISTRICT_VARIANT)(
            terrain, district, value, 1);
        set_message("District variant applied");
        return;
    }
    if (menu_id == MENU_DEVOPTS && selected >= s_menus[menu_id].item_count) {
        u32 dynamic_row = selected - s_menus[menu_id].item_count;
        s32 id = devopt_id_for_row(dynamic_row);
        if (id < 0) {
            set_message("Devopt is not present in the retail GameCube table");
            return;
        }
        if (kDevoptKinds[dynamic_row] == ITEM_BOOL) {
            set_devflag(id, !value);
            return;
        }
        if (kDevoptKinds[dynamic_row] == ITEM_INT) {
            value += direction;
            if (value > row.maximum)
                value = row.maximum;
            if (value < row.minimum)
                value = row.minimum;
            if (string_equal(kDevoptLabels[dynamic_row], "FRAME_LOCK"))
                relinquish_slow_motion_ownership();
            set_devint(id, value);
            if (string_equal(kDevoptLabels[dynamic_row], "GOD_MODE"))
                ((SetGodModeFn)ADDR_SET_GOD_MODE)(value);
            return;
        }
    }
    if (!row.value)
        return;
    if (row.kind == ITEM_BOOL) {
        *row.value = !value;
        if (row.label && string_equal(row.label, "Rumble Enabled"))
            apply_rumble_enabled(*row.value);
    } else {
        value += direction;
        if (row.kind == ITEM_ENUM) {
            if (value > row.maximum)
                value = row.minimum;
            if (value < row.minimum)
                value = row.maximum;
        } else {
            if (value > row.maximum)
                value = row.maximum;
            if (value < row.minimum)
                value = row.minimum;
        }
        *row.value = value;
    }
}

static void update_window(void)
{
    u16 menu_id = s_current_menu;
    u32 selected = s_selected[menu_id];
    u32 start = s_window_start[menu_id];
    if (selected < start)
        start = selected;
    if (selected >= start + MAX_VISIBLE_ROWS)
        start = selected - MAX_VISIBLE_ROWS + 1u;
    s_window_start[menu_id] = (u16)start;
}

static void move_selection(s32 direction)
{
    u16 menu_id = s_current_menu;
    u32 count = menu_row_count(menu_id);
    s32 selected;
    if (!count)
        return;
    selected = (s32)s_selected[menu_id] + direction;
    if (selected < 0)
        selected = (s32)count - 1;
    if ((u32)selected >= count)
        selected = 0;
    s_selected[menu_id] = (u16)selected;
    update_window();
}

static void page_selection(s32 direction)
{
    s32 step = (s32)MAX_VISIBLE_ROWS * direction;
    u16 menu_id = s_current_menu;
    u32 count = menu_row_count(menu_id);
    s32 selected;
    if (!count)
        return;
    selected = (s32)s_selected[menu_id] + step;
    if (selected < 0)
        selected = 0;
    if ((u32)selected >= count)
        selected = (s32)count - 1;
    s_selected[menu_id] = (u16)selected;
    update_window();
}

static s32 repeated_key(u16 buttons, u16 pressed, u16 mask, u32 slot)
{
    if (pressed & mask) {
        s_hold_frames[slot] = 1;
        return 1;
    }
    if (!(buttons & mask)) {
        s_hold_frames[slot] = 0;
        return 0;
    }
    if (s_hold_frames[slot] < 0xFFFFu)
        ++s_hold_frames[slot];
    if (s_hold_frames[slot] >= 15u && ((s_hold_frames[slot] - 15u) % 3u) == 0u)
        return 1;
    return 0;
}

static void handle_input(u16 buttons)
{
    u16 pressed = (u16)(buttons & (u16)~s_previous_buttons);
    s_previous_buttons = buttons;

    if (s_hero_swap_pending)
        return;

    if ((buttons & PAD_PS) == PAD_PS) {
        if (pressed & PAD_PS) {
            if (s_open)
                close_menu();
            else
                open_menu();
        }
        return;
    }
    if (((buttons & PAD_Z) && (pressed & PAD_START)) ||
        ((buttons & PAD_START) && (pressed & PAD_Z))) {
        if (s_open)
            close_menu();
        else
            open_menu();
        return;
    }
    if (!s_open)
        return;

    if (pressed & PAD_B) {
        u16 parent = s_menus[s_current_menu].parent;
        if (parent == MENU_NONE)
            close_menu();
        else
            s_current_menu = parent;
        return;
    }
    if (pressed & PAD_A) {
        change_row(1);
        return;
    }
    if (repeated_key(buttons, pressed, PAD_UP, 0))
        move_selection(-1);
    if (repeated_key(buttons, pressed, PAD_DOWN, 1))
        move_selection(1);
    if (repeated_key(buttons, pressed, PAD_LEFT, 2))
        change_row(-1);
    if (repeated_key(buttons, pressed, PAD_RIGHT, 3))
        change_row(1);
    if (pressed & PAD_L)
        page_selection(-1);
    if (pressed & PAD_R)
        page_selection(1);
}

static void format_row(u16 menu_id, u32 absolute_row, const Row *row,
                       char *out, u32 capacity)
{
    s32 value = row_value(menu_id,
                          absolute_row >= s_menus[menu_id].item_count
                              ? absolute_row - s_menus[menu_id].item_count
                              : absolute_row,
                          row);
    string_copy(out, capacity, row->label);
    if (menu_id == MENU_DEVOPTS &&
        absolute_row >= s_menus[menu_id].item_count &&
        devopt_id_for_row(absolute_row - s_menus[menu_id].item_count) < 0) {
        string_append(out, capacity, ": [not in GC]");
        return;
    }
    if (row->kind == ITEM_UNAVAILABLE) {
        string_append(out, capacity, ": [unavailable in retail GC]");
        return;
    }
    if (row->kind == ITEM_ACTION && row->action == ACTION_SELECT_HERO) {
        if (s_hero_swap_pending && string_equal(row->label, s_hero_swap_name))
            string_append(out, capacity, ": Switching");
        else if (s_active_hero_name && string_equal(row->label, s_active_hero_name))
            string_append(out, capacity, ": Active");
        return;
    }
    if (row->kind == ITEM_SUBMENU || row->kind == ITEM_ENTITY_SUBMENU ||
        row->kind == ITEM_MISSION_DISTRICT) {
        string_append(out, capacity, ": >");
    } else if (row->kind == ITEM_PAUSE) {
        string_append(out, capacity, ": ");
        string_append(out, capacity, value ? "Paused" : "Unpaused");
    } else if (row->kind == ITEM_BOOL || row->kind == ITEM_PHYSICS ||
               row->kind == ITEM_DEVFLAG || row->kind == ITEM_SLOW_MOTION ||
               row->kind == ITEM_SHOW_DISTRICTS) {
        string_append(out, capacity, ": ");
        string_append(out, capacity, value ? "True" : "False");
    } else if (row->kind == ITEM_INT || row->kind == ITEM_DEVINT ||
               row->kind == ITEM_DISTRICT_VARIANT) {
        string_append(out, capacity, ": ");
        append_signed(out, capacity, value);
    } else if (row->kind == ITEM_DEVINT_READONLY) {
        string_append(out, capacity, ": ");
        append_signed(out, capacity, value);
        string_append(out, capacity, " [read-only]");
    } else if (row->kind == ITEM_CAMERA) {
        string_append(out, capacity, ": ");
        string_append(out, capacity, s_camera_names[(u32)value % ARRAY_COUNT(s_camera_names)]);
    } else if (row->kind == ITEM_ENUM) {
        string_append(out, capacity, ": ");
        append_signed(out, capacity, value);
    }
}

static void render_menu(void)
{
    static const u32 COLOR_TITLE = 0xFFFF00FFu;
    static const u32 COLOR_SELECTED = 0x00DC00FFu;
    static const u32 COLOR_NORMAL = 0xDCDCDCFFu;
    u16 menu_id = s_current_menu;
    MenuDef *menu = &s_menus[menu_id];
    u32 count = menu_row_count(menu_id);
    u32 start;
    u32 visible;
    u32 line_height = get_line_height();
    u32 widest;
    u32 width;
    u32 has_message;
    u32 has_scroll;
    u32 i;
    float x = 28.0f;
    float y = 28.0f;
    float panel_right;
    float panel_bottom;
    NglQuad quad;
    char line[160];
    char generated[96];

    if (!get_font())
        return;
    if (!count) {
        s_selected[menu_id] = 0;
        s_window_start[menu_id] = 0;
    } else if (s_selected[menu_id] >= count) {
        s_selected[menu_id] = (u16)(count - 1u);
    }
    update_window();
    start = s_window_start[menu_id];
    visible = count > start ? count - start : 0;
    if (visible > MAX_VISIBLE_ROWS)
        visible = MAX_VISIBLE_ROWS;
    has_message = s_message && s_message[0] ? 1u : 0u;
    has_scroll = start || start + visible < count;
    widest = get_text_width(menu->title);
    for (i = 0; i < visible; ++i) {
        u32 absolute = start + i;
        Row row = get_row(menu_id, absolute, generated, sizeof(generated));
        format_row(menu_id, absolute, &row, line, sizeof(line));
        width = get_text_width(line);
        if (width > widest)
            widest = width;
    }
    if (has_message) {
        width = get_text_width(s_message);
        if (width > widest)
            widest = width;
    }
    panel_right = x + (float)widest + 10.0f + (has_scroll ? 20.0f : 0.0f);
    if (panel_right < 236.0f)
        panel_right = 236.0f;
    if (panel_right > 626.0f)
        panel_right = 626.0f;
    panel_bottom = y + (float)((visible + 1u + has_message) * line_height + 14u);
    if (panel_bottom > 470.0f)
        panel_bottom = 470.0f;

    ((NglQuadFn)ADDR_NGL_INIT_QUAD)(&quad);
    ((NglSetQuadRectFn)ADDR_NGL_SET_QUAD_RECT)(&quad, 17.0f, 24.0f, panel_right, panel_bottom);
    ((NglSetQuadColorFn)ADDR_NGL_SET_QUAD_COLOR)(&quad, 0x141414C8u);
    ((NglSetQuadZFn)ADDR_NGL_SET_QUAD_Z)(&quad, 0.5f);
    ((NglQuadFn)ADDR_NGL_LIST_ADD_QUAD)(&quad);

    add_text(x, y, COLOR_TITLE, menu->title);
    if (start)
        add_text(panel_right - 18.0f, y, COLOR_NORMAL, "^");
    y += (float)line_height;

    for (i = 0; i < visible; ++i) {
        u32 absolute = start + i;
        Row row = get_row(menu_id, absolute, generated, sizeof(generated));
        format_row(menu_id, absolute, &row, line, sizeof(line));
        add_text(x, y, absolute == s_selected[menu_id] ? COLOR_SELECTED : COLOR_NORMAL, line);
        if (i + 1u == visible && start + visible < count)
            add_text(panel_right - 18.0f, y, COLOR_NORMAL, "v");
        y += (float)line_height;
    }
    if (s_message)
        add_text(x, y, COLOR_NORMAL, s_message);
}

static void menu_tick_and_render(void *game)
{
    PadStatus pads[4];
    u16 buttons = 0;
    s_game = game;
    s_entity_list_ready = 0;

    if (!s_initialized) {
        s_current_menu = MENU_ROOT;
        s_message = 0;
        s_initialized = 1;
    }
#ifdef DEBUG_MENU_AUTO_OPEN
    if (s_auto_open_pending && s_game &&
        ((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game) == 6) {
        void *auto_world = *(void *volatile *)ADDR_WORLD_SLOT;
        if (!mem1_word_range(auto_world, 0x38Cu) ||
            *(volatile s32 *)((u8 *)auto_world + 0x224u) != 1) {
            s_auto_open_ready_frames = 0;
        } else if (s_auto_open_ready_frames < 300u) {
            ++s_auto_open_ready_frames;
        }
    } else {
        s_auto_open_ready_frames = 0;
    }
    if (s_auto_open_pending && s_auto_open_ready_frames >= 300u) {
        s_auto_open_pending = 0;
        open_menu();
#if defined(DEBUG_MENU_AUTO_TARGET_PAUSE)
        {
            u32 index;
            for (index = 0; index < ARRAY_COUNT(s_root_items); ++index) {
                if (s_root_items[index].kind == ITEM_PAUSE) {
                    s_selected[MENU_ROOT] = (u16)index;
                    break;
                }
            }
        }
#elif defined(DEBUG_MENU_AUTO_TARGET_MISSIONS)
        refresh_mission_cache();
        s_current_menu = MENU_MISSIONS;
#elif defined(DEBUG_MENU_AUTO_TARGET_CHAR_SELECT)
        s_current_menu = MENU_CHAR_SELECT;
#endif
    }
#endif
    if (s_single_step_pending) {
        ((GameEnablePhysicsFn)ADDR_GAME_ENABLE_PHYSICS)(s_game, 0);
        s_single_step_pending = 0;
    }

    update_hero_swap_status();

    ((PadReadFn)ADDR_PAD_READ)(pads);
    if (pads[0].error == 0)
        buttons = pads[0].button;
    handle_input(buttons);
    if (s_open)
        render_menu();
}
