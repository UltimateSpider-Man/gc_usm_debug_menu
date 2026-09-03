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
#define MAX_ACTOR_ENTITIES 256u
#define MAX_MISSION_ROWS 1024u
#define MAX_MISSION_GROUPS 9u
#define MAX_LEVEL_ROWS 128u
#define MAX_ANIMATION_ROWS 512u
#define MAX_AI_CORES 256u
#define MAX_AI_INODES 128u
#define MAX_AI_PARAMS 512u
#define MAX_SCRIPT_BYTECODE_WORDS 0x8000u
#define SCRIPT_DISPATCH_REQUIRES_ENTRY (-2)
#define MISSION_ROW_CACHE_BASE 0x817A0000u
#define ANIMATION_NAME_CACHE_BASE 0x8179E000u
#define VARIANT_ENTITY_CACHE_BASE 0x817B0000u
#define ACTOR_ENTITY_CACHE_BASE 0x817B6000u
#define AUX_CACHE_BASE 0x81792000u
#define LEVEL_INDEX_CACHE_BASE (AUX_CACHE_BASE + 0x0000u)
#define SCRIPT_INDEX_CACHE_BASE (AUX_CACHE_BASE + 0x0200u)
#define SCRIPT_OWNER_CACHE_BASE (AUX_CACHE_BASE + 0x0400u)
#define SCRIPT_STATUS_CACHE_BASE (AUX_CACHE_BASE + 0x0800u)
#define MISSION_GROUP_CACHE_BASE (AUX_CACHE_BASE + 0x0900u)
#define MISSION_GROUP_NAME_CACHE_BASE (AUX_CACHE_BASE + 0x0A00u)
#define AI_TITLE_CACHE_BASE (AUX_CACHE_BASE + 0x0C00u)
#define AI_INODE_ORDER_CACHE_BASE (AI_TITLE_CACHE_BASE + 0x0080u)
#define AI_PARAM_ORDER_CACHE_BASE \
    (AI_INODE_ORDER_CACHE_BASE + (MAX_AI_INODES + 1u) * 2u)
#define AI_CORE_CACHE_BASE (ACTOR_ENTITY_CACHE_BASE + 0x0800u)

_Static_assert(ANIMATION_NAME_CACHE_BASE + MAX_ANIMATION_ROWS * 4u <=
                   MISSION_ROW_CACHE_BASE,
               "animation cache must not overlap the mission cache");
_Static_assert(MISSION_GROUP_NAME_CACHE_BASE + MAX_MISSION_GROUPS * 32u <=
                   AI_TITLE_CACHE_BASE,
               "auxiliary caches must not overlap the animation cache");
_Static_assert(AI_PARAM_ORDER_CACHE_BASE + (MAX_AI_PARAMS + 1u) * 2u <=
                   ANIMATION_NAME_CACHE_BASE,
               "AI title/order caches must not overlap the animation cache");
_Static_assert(AI_CORE_CACHE_BASE + MAX_AI_CORES * 4u <= 0x81800000u,
               "AI core cache must remain inside MEM1");

_Static_assert(DEVOPT_COUNT == ADDR_DEVOPT_FLAG_COUNT + ADDR_DEVOPT_INT_COUNT,
               "generated Devopts must cover both complete GUTE52 tables");
_Static_assert(ARRAY_COUNT(kDevoptLabels) == DEVOPT_COUNT,
               "Devopt label count mismatch");
_Static_assert(ARRAY_COUNT(kDevoptKinds) == DEVOPT_COUNT,
               "Devopt kind count mismatch");

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

typedef struct GameProcess {
    const char *name;
    const s32 *states;
    s32 index;
    s32 state_count;
    float state_time;
    u8 field_14;
    u8 padding[3];
} GameProcess;

_Static_assert(sizeof(GameProcess) == 0x18u,
               "retail game_process must remain a 24-byte copied value");

typedef struct ResourceKey {
    u32 name_hash;
    s32 type;
} ResourceKey;

typedef struct LevelDescriptor {
    char pack_name[32];
    char description[64];
    char menu_name[16];
    s32 fields_70[8];
} LevelDescriptor;

_Static_assert(sizeof(LevelDescriptor) == 0x90u,
               "retail level descriptors must remain 0x90 bytes");

typedef struct AnimationHandle {
    s32 mode;
    float phase;
    void *controller;
} AnimationHandle;

_Static_assert(sizeof(AnimationHandle) == 0x0Cu,
               "retail animation handles use a 12-byte aggregate return");

typedef struct AiVector {
    u32 field_0;
    s32 count;
    void **data;
    u32 capacity;
    u8 owning;
    u8 padding[3];
} AiVector;

typedef struct AiParamDataArray {
    AiVector values;
    void *cached;
} AiParamDataArray;

typedef struct AiParamBlock {
    s32 current_mode;
    AiParamDataArray *array;
    u8 changed;
    u8 padding[3];
} AiParamBlock;

typedef struct AiParamData {
    union {
        s32 integer;
        float floating;
        u32 hash;
        void *pointer;
    } value;
    s32 type;
    u32 name_hash;
} AiParamData;

_Static_assert(sizeof(AiVector) == 0x14u, "retail AI vectors are 0x14 bytes");
_Static_assert(sizeof(AiParamDataArray) == 0x18u,
               "retail AI parameter arrays are 0x18 bytes");
_Static_assert(sizeof(AiParamBlock) == 0x0Cu,
               "retail AI parameter blocks are 0x0C bytes");
_Static_assert(sizeof(AiParamData) == 0x0Cu,
               "retail AI parameters are 0x0C bytes");

typedef void (*NglListAddStringFn)(void *, float, float, float, u32, float, float,
                                   const char *, ...);
typedef void (*NglGetStringDimensionsFn)(void *, const char *, u32 *, u32 *, float, float);
typedef void (*NglQuadFn)(NglQuad *);
typedef void (*NglSetQuadRectFn)(NglQuad *, float, float, float, float);
typedef void (*NglSetQuadColorFn)(NglQuad *, u32);
typedef void (*NglSetQuadZFn)(NglQuad *, float);
typedef u32 (*PadReadFn)(PadStatus *);
typedef void (*GameProcessCtorFn)(GameProcess *, const char *, const s32 *, s32);
typedef void (*GamePushProcessFn)(void *, const GameProcess *);
typedef s32 (*GameGetCurStateFn)(void *);
typedef void (*GamePauseModeFn)(void *);
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
typedef void *(*ScriptManagerFindFunctionByNameFn)(const u32 *);
typedef void *(*ScriptInstanceAddThreadFn)(void *, void *);
typedef void *(*ScriptInstanceAddThreadWithArgsFn)(void *, void *, const void *);
typedef s32 (*DVDConvertPathToEntrynumFn)(const char *);
typedef void *(*MStringFromCharFn)(void *, const char *);
typedef void (*MStringFinalizeFn)(void *, s32);
typedef void (*DevoptSetStringFn)(void *, s32, const void *);
typedef void (*TerrainUnloadAllDistrictsFn)(void *);
typedef void (*TerrainUnlockDistrictFn)(void *, s32);
typedef void (*TerrainSetDistrictVariantFn)(void *, s32, s32, s32);
typedef void (*WorldMalorPointFn)(void *, const float *, s32, s32);
typedef s32 (*EntityTrackerArrowFn)(void *, float *);
typedef void *(*ResourceManagerGetPartitionFn)(s32);
typedef void (*ResourceStreamerSetActiveFn)(void *, s32);
typedef void *(*ResourcePackSlotGetDirectoryFn)(void *);
typedef void *(*ResourceDirectoryGetResourceFn)(void *, const ResourceKey *,
                                                s32 *, void **);
typedef void *(*ResourceDirectoryTypeVectorFn)(void *, s32);
typedef void *(*ResourcePushContextFn)(void *);
typedef void *(*ResourcePopContextFn)(void);
typedef void (*GameLoadNewLevelFn)(void *, const void *);
typedef void (*StringHashFromCharFn)(u32 *, const char *);
typedef void (*ActorAllocateAnimationFn)(void *, u32, void *);
typedef void (*AnimationPlayFn)(AnimationHandle *, void *, const u32 *, float,
                                u32, s32);
typedef void *(*NalGetNextAnimFn)(void *);
typedef s32 (*RegionGetDistrictVariantFn)(void *);
typedef void (*EntityFindEntitiesFn)(s32);
typedef s32 (*ObjectPredicateFn)(void *);
typedef void *(*ObjectGetterFn)(void *);
typedef void *(*ActorGetAICoreFn)(void *);
typedef const char *(*StringHashToStringFn)(const void *);
typedef void (*VariantApplyFn)(void *, const void *);
typedef s32 (*DevoptGetFlagFn)(void *, s32);
typedef void (*DevoptSetFlagFn)(void *, s32, s32);
typedef s32 (*DevoptGetIntFn)(void *, s32);
typedef void (*DevoptSetIntFn)(void *, s32, s32);
typedef void *(*ArenaGetterFn)(void);
typedef void (*VoidFn)(void);
typedef s32 (*BoolFn)(void);
typedef void (*InputMonkeyCallbackFn)(void *, void *);
typedef s32 (*DebugRenderGetFn)(s32);
typedef void (*DebugRenderSetFn)(s32, s32);
typedef void (*GameSettingsSlotFn)(void *, s32);
typedef void (*GameSettingsFn)(void *);
typedef u32 (*MemoryStatFn)(s32);
typedef void (*OSReportFn)(const char *, ...);

enum PadButton_GC {
    GC_PAD_LEFT = 0x0001,
    GC_PAD_RIGHT = 0x0002,
    GC_PAD_DOWN = 0x0004,
    GC_PAD_UP = 0x0008,
    /* Synthetic: Dolphin maps DS4 Share/PS and DualSense PS to this chord. */
    GC_PAD_PS = 0x000F,
    GC_PAD_Z = 0x0010,
    GC_PAD_R = 0x0020,
    GC_PAD_L = 0x0040,
    GC_PAD_A = 0x0100,
    GC_PAD_B = 0x0200,
    GC_PAD_X = 0x0400,
    GC_PAD_Y = 0x0800,
    GC_PAD_START = 0x1000
};

/*
 * DualShock 4 names for the logical GameCube bits emitted by Dolphin's
 * GCPadNew_DS4.ini profile. PADRead still returns a GameCube PadStatus packet;
 * these aliases express the physical DS4 controls without using native HID
 * report indices (which are not visible to the GameCube executable).
 */
enum DualShock4Button {
    DS4_DPAD_LEFT = GC_PAD_LEFT,
    DS4_DPAD_RIGHT = GC_PAD_RIGHT,
    DS4_DPAD_DOWN = GC_PAD_DOWN,
    DS4_DPAD_UP = GC_PAD_UP,
    DS4_SHARE = GC_PAD_PS,
    DS4_SELECT = GC_PAD_PS,
    DS4_PS = GC_PAD_PS,
    DS4_MENU_TOGGLE = GC_PAD_PS,
    DS4_R1 = GC_PAD_Z,
    DS4_R2 = GC_PAD_R,
    DS4_L2 = GC_PAD_L,
    DS4_CROSS = GC_PAD_A,
    DS4_CIRCLE = GC_PAD_B,
    DS4_SQUARE = GC_PAD_X,
    DS4_TRIANGLE = GC_PAD_Y,
    DS4_OPTIONS = GC_PAD_START
};

enum ItemKind {
    ITEM_SUBMENU,
    ITEM_BOOL,
    ITEM_INT,
    ITEM_ENUM,
    ITEM_ACTION,
    ITEM_INFO,
    ITEM_PHYSICS,
    ITEM_DEVFLAG,
    ITEM_NAMED_DEVFLAG,
    ITEM_DEVINT,
    ITEM_DEVINT_READONLY,
    ITEM_SLOW_MOTION,
    ITEM_CAMERA,
    ITEM_SHOW_DISTRICTS,
    ITEM_WARP_REGION,
    ITEM_LEVEL,
    ITEM_DISTRICT_VARIANT,
    ITEM_ENTITY_SUBMENU,
    ITEM_ENTITY_VARIANT,
    ITEM_MISSION,
    ITEM_MISSION_DISTRICT,
    ITEM_SLAB_TOTAL,
    ITEM_SLAB_FREE,
    ITEM_SLAB_BUCKET,
    ITEM_DVAR_FLOAT,
    ITEM_MONKEY,
    ITEM_SAVED_BOOL,
    ITEM_SAVED_INT,
    ITEM_SAVED_FLOAT,
    ITEM_AI_ENTITY,
    ITEM_AI_PARAM_BLOCK,
    ITEM_AI_PARAM,
    ITEM_ANIMATION_ENTITY,
    ITEM_ANIMATION,
    ITEM_SCRIPT_HANDLER,
    ITEM_SCRIPT_REQUIRES_ENTRY,
    ITEM_SCRIPT_UNVERIFIED,
    ITEM_PROGRESSION,
    ITEM_NGL_PERF_INFO,
    ITEM_NGL_FLAG,
    ITEM_DEBUG_RENDER,
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
    ACTION_AI_EXPORT,
    ACTION_REBOOT,
    ACTION_SELECT_HERO,
    ACTION_NGL_SCREENSHOT,
    ACTION_PROGRESSION_ON,
    ACTION_PROGRESSION_OFF,
    ACTION_PROGRESSION_ALL_TOKENS
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
    MENU_AI_ENTITY,
    MENU_AI_BLOCK,
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
    MENU_ENTITY_ANIMATION_VALUES,
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

_Static_assert(sizeof(MissionRow) == 0x14u,
               "MissionRow layout must match the fixed MEM1 cache size");

typedef struct MissionGroup {
    const char *name;
    u16 start;
    u16 count;
    s16 district;
    u16 reserved;
} MissionGroup;

typedef struct ProgressionEntry {
    const char *handler_signature;
} ProgressionEntry;

typedef struct ScriptEntry {
    const char *label;
    const char *handler_signature;
} ScriptEntry;

enum ScriptRowStatus {
    SCRIPT_ROW_DISPATCHABLE,
    SCRIPT_ROW_REQUIRES_ENTRY,
    SCRIPT_ROW_UNVERIFIED
};

#define SUB(label_, target_) { (label_), 0, 0, 0, (target_), ITEM_SUBMENU, ACTION_NONE }
#define BOOL_ITEM(label_, initial_) { (label_), (initial_), 0, 1, MENU_NONE, ITEM_BOOL, ACTION_NONE }
#define INT_ITEM(label_, initial_, min_, max_) { (label_), (initial_), (min_), (max_), MENU_NONE, ITEM_INT, ACTION_NONE }
#define ENUM_ITEM(label_, initial_, max_) { (label_), (initial_), 0, (max_), MENU_NONE, ITEM_ENUM, ACTION_NONE }
#define ACTION_ITEM(label_, action_) { (label_), 0, 0, 0, MENU_NONE, ITEM_ACTION, (action_) }
#define INFO_ITEM(label_) { (label_), 0, 0, 0, MENU_NONE, ITEM_INFO, ACTION_NONE }
#define UNAVAILABLE_ITEM(label_) { (label_), 0, 0, 0, MENU_NONE, ITEM_UNAVAILABLE, ACTION_NONE }
#define DEVFLAG_ITEM(label_, id_) { (label_), (id_), 0, 1, MENU_NONE, ITEM_DEVFLAG, ACTION_NONE }
#define NAMED_DEVFLAG_ITEM(label_) { (label_), 0, 0, 1, MENU_NONE, ITEM_NAMED_DEVFLAG, ACTION_NONE }
#define DEVINT_ITEM(label_, id_, min_, max_) { (label_), (id_), (min_), (max_), MENU_NONE, ITEM_DEVINT, ACTION_NONE }
#define SLOW_MOTION_ITEM(label_) { (label_), 0, 0, 1, MENU_NONE, ITEM_SLOW_MOTION, ACTION_NONE }
#define PHYSICS_ITEM(label_) { (label_), 1, 0, 1, MENU_NONE, ITEM_PHYSICS, ACTION_NONE }
#define CAMERA_ITEM(label_) { (label_), 0, 0, 2, MENU_NONE, ITEM_CAMERA, ACTION_NONE }
#define SHOW_DISTRICTS_ITEM(label_) { (label_), 0, 0, 1, MENU_NONE, ITEM_SHOW_DISTRICTS, ACTION_NONE }
#define SLAB_TOTAL_ITEM(label_) { (label_), 0, 0, 0, MENU_NONE, ITEM_SLAB_TOTAL, ACTION_NONE }
#define SLAB_FREE_ITEM(label_) { (label_), 0, 0, 0, MENU_NONE, ITEM_SLAB_FREE, ACTION_NONE }
#define DVAR_FLOAT_ITEM(label_, address_, min_, max_) \
    { (label_), (s32)(address_), (min_), (max_), MENU_NONE, ITEM_DVAR_FLOAT, ACTION_NONE }
#define MONKEY_ITEM(label_) \
    { (label_), 0, 0, 1, MENU_NONE, ITEM_MONKEY, ACTION_NONE }
#define PROGRESSION_ITEM(label_, index_) \
    { (label_), (index_), 0, 0, MENU_NONE, ITEM_PROGRESSION, ACTION_NONE }
#define NGL_FLAG_ITEM(label_, offset_) \
    { (label_), (offset_), 0, 1, MENU_NONE, ITEM_NGL_FLAG, ACTION_NONE }

static MenuItem s_root_items[] = {
    SUB("DVars", MENU_DVARS),
    SUB("Warp", MENU_WARP),
    SUB("Game", MENU_GAME),
    SUB("Missions", MENU_MISSIONS),
    SUB("Debug Render", MENU_DEBUG_RENDER),
    SUB("District variants", MENU_DISTRICT_VARIANTS),
    SUB("AI", MENU_AI),
    SUB("Memory", MENU_MEMORY),
    SUB("Entity Variants", MENU_ENTITY_VARIANTS),
    SUB("Entity Animations", MENU_ENTITY_ANIMATIONS),
    SUB("Level Select", MENU_LEVEL_SELECT),
    SUB("Script", MENU_SCRIPT),
    SUB("Progression", MENU_PROGRESSION),
    CAMERA_ITEM("Camera")
};

_Static_assert(ARRAY_COUNT(s_root_items) == 14u,
               "the recorded root menu must remain exactly 14 rows");

static MenuItem s_game_items[] = {
    PHYSICS_ITEM("Physics Enabled"),
    ACTION_ITEM("Single Step", ACTION_SINGLE_STEP),
    SLOW_MOTION_ITEM("Slow Motion Enabled"),
    MONKEY_ITEM("Monkey Enabled"),
    BOOL_ITEM("Rumble Enabled", 1),
    DEVINT_ITEM("God Mode", 29, 0, 5),
    SHOW_DISTRICTS_ITEM("Show Districts"),
    DEVFLAG_ITEM("Show Hero Position", 21),
    DEVFLAG_ITEM("Show FPS", 5),
    NAMED_DEVFLAG_ITEM("User Camera on Controller 2"),
    ACTION_ITEM("Toggle Unload All Districts", ACTION_UNLOAD_DISTRICTS),
    SUB("Save/Load", MENU_SAVE_LOAD),
    SUB("Screenshot", MENU_SCREENSHOT),
    SUB("Devopts", MENU_DEVOPTS),
    SUB("Saved Game Settings", MENU_SAVED_SETTINGS)
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
    DVAR_FLOAT_ITEM("base_factor", ADDR_DVAR_BASE_FACTOR, 0, 100),
    DVAR_FLOAT_ITEM("camera_min_dist", ADDR_DVAR_CAMERA_MIN_DIST, 0, 100),
    DVAR_FLOAT_ITEM("camera_max_dist", ADDR_DVAR_CAMERA_MAX_DIST, 0, 100),
    DVAR_FLOAT_ITEM("camera_supermax_dist", ADDR_DVAR_CAMERA_SUPERMAX_DIST, 0, 100),
    DVAR_FLOAT_ITEM("jump_cap_vel", ADDR_DVAR_JUMP_CAP_VEL, 0, 100),
    DVAR_FLOAT_ITEM("snow_balling", ADDR_DVAR_SNOW_BALLING, 0, 20)
};

static MenuItem s_warp_items[] = {
    ACTION_ITEM("--WARP TO POI--", ACTION_WARP_POI)
};

static MenuItem s_missions_items[] = {
    ACTION_ITEM("UNLOAD CURRENT MISSION", ACTION_UNLOAD_MISSION)
};

static MenuItem s_debug_render_prefix[] = {
    SUB("NGL Debug", MENU_NGL_DEBUG)
};

static MenuItem s_ngl_items[] = {
    UNAVAILABLE_ITEM("ShowPerfBar"),
    { "ShowPerfInfo", 0, 0, 2, MENU_NONE, ITEM_NGL_PERF_INFO, ACTION_NONE },
    ACTION_ITEM("ScreenShot", ACTION_NGL_SCREENSHOT),
    NGL_FLAG_ITEM("DisableQuads", 6),
    NGL_FLAG_ITEM("DisableVSync", 9),
    NGL_FLAG_ITEM("DisableScratch", 10),
    NGL_FLAG_ITEM("DebugPrints", 11),
    NGL_FLAG_ITEM("DumpFrameLog", 12),
    NGL_FLAG_ITEM("DumpSceneFile", 13),
    NGL_FLAG_ITEM("DumpTextures", 14),
    NGL_FLAG_ITEM("DrawLightSpheres", 15),
    NGL_FLAG_ITEM("DrawMeshSpheres", 16),
    NGL_FLAG_ITEM("DisableDuplicateMaterialWarning", 17),
    NGL_FLAG_ITEM("DisableMissingTextureWarning", 18),
    UNAVAILABLE_ITEM("RenderSingleNode")
};

static MenuItem s_replay_items[] = {
    ACTION_ITEM("Start", ACTION_REPLAY_START)
};

static MenuItem s_memory_items[] = {
    SUB("Script Memtrack", MENU_SCRIPT_MEMTRACK),
    ACTION_ITEM("Dump MemTrack Data Since Last Checkpoint", ACTION_MEMTRACK_DUMP),
    ACTION_ITEM("Set MemTrack Checkpoint", ACTION_MEMTRACK_CHECKPOINT),
    SUB("Slabs", MENU_SLABS)
};

static MenuItem s_script_memtrack_items[] = {
    ACTION_ITEM("Dump Memtrack Info", ACTION_MEMTRACK_DUMP),
    UNAVAILABLE_ITEM("Class-name/entity-ID histories were stripped")
};

static MenuItem s_slabs_items[] = {
    SLAB_TOTAL_ITEM("Total Slabs"),
    SLAB_FREE_ITEM("Free Slabs"),
    SUB("Full Slabs", MENU_FULL_SLABS),
    SUB("Partial Slabs", MENU_PARTIAL_SLABS),
    SUB("Allocated Objects", MENU_ALLOCATED_OBJECTS),
    SUB("Free Objects", MENU_FREE_OBJECTS)
};

static const char *const s_unknown_animation_names[] = {
    "No animations in this actor resource pack"
};

static MenuItem s_level_select_items[] = {
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

/* The PC recreation also exposes these script-created utility menus.  Keep
   the recorded 14-row GameCube root unchanged and attach them to Script. */
static MenuItem s_script_items[] = {
    SUB("Char Select", MENU_CHAR_SELECT),
    SUB("Options", MENU_OPTIONS)
};

#include "script_debug_catalog.inc"

_Static_assert(ARRAY_COUNT(s_script_entries) == 245u,
               "Script catalog must contain 22 probes and 223 visible rows");

static MenuItem s_progression_items[] = {
    ACTION_ITEM("toggle progression on", ACTION_PROGRESSION_ON),
    ACTION_ITEM("toggle progression off", ACTION_PROGRESSION_OFF),
    ACTION_ITEM("turnon all tokens", ACTION_PROGRESSION_ALL_TOKENS),
    PROGRESSION_ITEM("start progression before S01_FATHERS_PRIDE", 0),
    PROGRESSION_ITEM("start progression before S02_WORKOUT", 1),
    PROGRESSION_ITEM("start progression before JOHNNY_STORM", 2),
    PROGRESSION_ITEM("start progression before COMBAT_TOUR", 3),
    PROGRESSION_ITEM("start progression before S03_RHINO_CHASE", 4),
    PROGRESSION_ITEM("start progression before S04_RHINO_BEATDOWN", 5),
    PROGRESSION_ITEM("start progression before S05_SKY_CHASE", 6),
    PROGRESSION_ITEM("start progression before S06_SKYSCRAPER_BATTLE", 7),
    PROGRESSION_ITEM("start progression before S07_GREEN_GOBLIN", 8),
    PROGRESSION_ITEM("start progression before S08_CLASS_TRIP", 9),
    PROGRESSION_ITEM("start progression before S09_IGC1", 10),
    PROGRESSION_ITEM("start progression before S10_SPIDEY_VS_SILVER_SABLE", 11),
    PROGRESSION_ITEM("start progression before S11_DEFEND_SABLE", 12),
    PROGRESSION_ITEM("start progression before S13_FINAL_BATTLE", 13),
    PROGRESSION_ITEM("start progression game finished", 14),
    PROGRESSION_ITEM("start progression before V01_FEEDING_TIME", 15),
    PROGRESSION_ITEM("start progression before V02_WOLVERINE", 16),
    PROGRESSION_ITEM("start progression before V03_THROW_DOWN", 17),
    PROGRESSION_ITEM("start progression before V07_ELECTRO_CHASE", 18),
    PROGRESSION_ITEM("start progression before V08_ELECTRO_FINALE", 19),
    PROGRESSION_ITEM("start progression before V09_THE_GREAT_ESCAPE", 20),
    PROGRESSION_ITEM("start progression before V10_BEETLE_BATTLE", 21),
    PROGRESSION_ITEM("start progression before V12_VENOM_VS_CARNAGE", 22)
};

_Static_assert(ARRAY_COUNT(s_progression_items) == 26u,
               "Progression must contain three actions and 23 checkpoints");

/* Exact script signatures registered by CITY_ARENA.  The retail function map
   hashes the name before '(', while the suffix documents and validates the VM
   stack contract used by debug_menu_entry::script_handler_helper. */
static const char s_progression_toggle_on[] =
    "toggle_progression_on(debug_menu_entry)";
static const char s_progression_toggle_off[] =
    "toggle_progression_off(debug_menu_entry)";
static const char s_progression_all_tokens[] =
    "global_turnon_tokens(debug_menu_entry)";
static const char s_progression_start_to_level_s01[] =
    "start_to_level_S01(debug_menu_entry)";
static const char s_progression_start_to_level_s02[] =
    "start_to_level_S02(debug_menu_entry)";
static const char s_progression_start_to_johnny_storm[] =
    "start_to_johnny_storm(debug_menu_entry)";
static const char s_progression_start_to_combat_tour[] =
    "start_to_combat_tour(debug_menu_entry)";
static const char s_progression_start_to_level_s03[] =
    "start_to_level_S03(debug_menu_entry)";
static const char s_progression_start_to_level_s04[] =
    "start_to_level_S04(debug_menu_entry)";
static const char s_progression_start_to_level_s05[] =
    "start_to_level_S05(debug_menu_entry)";
static const char s_progression_start_to_level_s06[] =
    "start_to_level_S06(debug_menu_entry)";
static const char s_progression_start_to_level_s07[] =
    "start_to_level_S07(debug_menu_entry)";
static const char s_progression_start_to_level_s08[] =
    "start_to_level_S08(debug_menu_entry)";
static const char s_progression_start_to_level_s09[] =
    "start_to_level_S09(debug_menu_entry)";
static const char s_progression_start_to_level_s10[] =
    "start_to_level_S10(debug_menu_entry)";
static const char s_progression_start_to_level_s11[] =
    "start_to_level_S11(debug_menu_entry)";
static const char s_progression_start_to_level_s13[] =
    "start_to_level_S13(debug_menu_entry)";
static const char s_progression_game_finished[] =
    "game_finished(debug_menu_entry)";
static const char s_progression_start_to_level_v01[] =
    "start_to_level_V01(debug_menu_entry)";
static const char s_progression_start_to_level_v02[] =
    "start_to_level_V02(debug_menu_entry)";
static const char s_progression_start_to_level_v03[] =
    "start_to_level_V03(debug_menu_entry)";
static const char s_progression_start_to_level_v07[] =
    "start_to_level_V07(debug_menu_entry)";
static const char s_progression_start_to_level_v08[] =
    "start_to_level_V08(debug_menu_entry)";
static const char s_progression_start_to_level_v09[] =
    "start_to_level_V09(debug_menu_entry)";
static const char s_progression_start_to_level_v10[] =
    "start_to_level_V10(debug_menu_entry)";
static const char s_progression_start_to_level_v12[] =
    "start_to_level_V12(debug_menu_entry)";

static const ProgressionEntry s_progression_entries[] = {
    { s_progression_start_to_level_s01 },
    { s_progression_start_to_level_s02 },
    { s_progression_start_to_johnny_storm },
    { s_progression_start_to_combat_tour },
    { s_progression_start_to_level_s03 },
    { s_progression_start_to_level_s04 },
    { s_progression_start_to_level_s05 },
    { s_progression_start_to_level_s06 },
    { s_progression_start_to_level_s07 },
    { s_progression_start_to_level_s08 },
    { s_progression_start_to_level_s09 },
    { s_progression_start_to_level_s10 },
    { s_progression_start_to_level_s11 },
    { s_progression_start_to_level_s13 },
    { s_progression_game_finished },
    { s_progression_start_to_level_v01 },
    { s_progression_start_to_level_v02 },
    { s_progression_start_to_level_v03 },
    { s_progression_start_to_level_v07 },
    { s_progression_start_to_level_v08 },
    { s_progression_start_to_level_v09 },
    { s_progression_start_to_level_v10 },
    { s_progression_start_to_level_v12 }
};

_Static_assert(ARRAY_COUNT(s_progression_entries) == 23u,
               "Progression callback table must cover every checkpoint");

static MenuItem s_options_items[] = {
    DEVFLAG_ITEM("Live in Glass House", 123)
};

static MenuDef s_menus[MENU_COUNT] = {
    { "Debug Menu", MENU_NONE, s_root_items, ARRAY_COUNT(s_root_items), 0, 0, 0, 0, 0, 0 },
    { "DVars", MENU_ROOT, s_dvars_items, ARRAY_COUNT(s_dvars_items), 0, 0, 0, 0, 0, 0 },
    { "Warp", MENU_ROOT, s_warp_items, ARRAY_COUNT(s_warp_items), 0, 0, 0, 0, 0, 0 },
    { "Game", MENU_ROOT, s_game_items, ARRAY_COUNT(s_game_items), 0, 0, 0, 0, 0, 0 },
    { "Save/Load", MENU_GAME, s_save_load_items, ARRAY_COUNT(s_save_load_items), 0, 0, 0, 0, 0, 0 },
    { "Screenshot", MENU_GAME, s_screenshot_items, ARRAY_COUNT(s_screenshot_items), 0, 0, 0, 0, 0, 0 },
    { "Devopts", MENU_GAME, 0, 0, kDevoptLabels, kDevoptKinds, 0, DEVOPT_COUNT, ITEM_BOOL, 0 },
    { "Saved Game Settings", MENU_GAME, 0, 0, kSavedSettingLabels, 0, 0, SAVED_SETTING_COUNT, ITEM_SAVED_INT, 0 },
    { "Missions", MENU_ROOT, s_missions_items, ARRAY_COUNT(s_missions_items), 0, 0, 0, 0, 0, 0 },
    { "District Missions", MENU_MISSIONS, 0, 0, 0, 0, 0, 0, ITEM_MISSION, 0 },
    { "Debug Render", MENU_ROOT, s_debug_render_prefix, ARRAY_COUNT(s_debug_render_prefix), kDebugRenderLabels, 0, 0, DEBUG_RENDER_COUNT, ITEM_DEBUG_RENDER, 0 },
    { "NGL Debug", MENU_DEBUG_RENDER, s_ngl_items, ARRAY_COUNT(s_ngl_items), 0, 0, 0, 0, 0, 0 },
    { "District variants", MENU_ROOT, 0, 0, 0, 0, 0, 0, ITEM_DISTRICT_VARIANT, 0 },
    { "Replay", MENU_ROOT, s_replay_items, ARRAY_COUNT(s_replay_items), 0, 0, 0, 0, 0, 0 },
    { "AI", MENU_ROOT, 0, 0, 0, 0, 0, 0, ITEM_AI_ENTITY, 0 },
    { "AI Entity", MENU_AI, 0, 0, 0, 0, 0, 0, ITEM_AI_PARAM_BLOCK, 0 },
    { "AI Block", MENU_AI_ENTITY, 0, 0, 0, 0, 0, 0, ITEM_AI_PARAM, 0 },
    { "Memory", MENU_ROOT, s_memory_items, ARRAY_COUNT(s_memory_items), 0, 0, 0, 0, 0, 0 },
    { "Script Memtrack", MENU_MEMORY, s_script_memtrack_items, ARRAY_COUNT(s_script_memtrack_items), 0, 0, 0, 0, 0, 0 },
    { "Slabs", MENU_MEMORY, s_slabs_items, ARRAY_COUNT(s_slabs_items), 0, 0, 0, 0, 0, 0 },
    { "Full Slabs", MENU_SLABS, 0, 0, 0, 0, 0, 44, ITEM_SLAB_BUCKET, 1 },
    { "Partial Slabs", MENU_SLABS, 0, 0, 0, 0, 0, 44, ITEM_SLAB_BUCKET, 1 },
    { "Allocated Objects", MENU_SLABS, 0, 0, 0, 0, 0, 44, ITEM_SLAB_BUCKET, 1 },
    { "Free Objects", MENU_SLABS, 0, 0, 0, 0, 0, 44, ITEM_SLAB_BUCKET, 1 },
    { "Entity Variants", MENU_ROOT, 0, 0, 0, 0, 0, 0, ITEM_ENTITY_SUBMENU, 0 },
    { "Entity Variant Values", MENU_ENTITY_VARIANTS, 0, 0, 0, 0, 0, 0, ITEM_ENTITY_VARIANT, 0 },
    { "Entity Animations", MENU_ROOT, 0, 0, 0, 0, 0, 0, ITEM_ANIMATION_ENTITY, 0 },
    { "Entity Animation Values", MENU_ENTITY_ANIMATIONS, 0, 0, s_unknown_animation_names, 0, 0, ARRAY_COUNT(s_unknown_animation_names), ITEM_INFO, 0 },
    { "Level Select", MENU_ROOT, s_level_select_items, ARRAY_COUNT(s_level_select_items), 0, 0, 0, 0, 0, 0 },
    { "Hero Select", MENU_LEVEL_SELECT, s_hero_select_items, ARRAY_COUNT(s_hero_select_items), 0, 0, 0, 0, 0, 0 },
    { "Script", MENU_ROOT, s_script_items, ARRAY_COUNT(s_script_items), 0, 0, 0, 0, ITEM_SCRIPT_HANDLER, 0 },
    { "Progression", MENU_ROOT, s_progression_items, ARRAY_COUNT(s_progression_items), 0, 0, 0, 0, 0, 0 },
    { "Char Select", MENU_SCRIPT, s_hero_select_items, ARRAY_COUNT(s_hero_select_items), 0, 0, 0, 0, 0, 0 },
    { "Options", MENU_SCRIPT, s_options_items, ARRAY_COUNT(s_options_items), 0, 0, 0, 0, 0, 0 }
};

static u8 s_open;
static u8 s_initialized;
static u16 s_current_menu;
static u16 s_selected[MENU_COUNT];
static u16 s_window_start[MENU_COUNT];
static u16 s_previous_buttons;
static u16 s_hold_frames[4];
static s32 s_saved_frame_lock;
static void *s_game;
static const char *s_message;
static char s_memtrack_message[96];
static u32 s_memtrack_checkpoint_alloced;
static u32 s_memtrack_checkpoint_free;
static u8 s_memtrack_checkpoint_valid;
static GameProcess s_hires_process;
static const s32 s_hires_states[] = { 8, 9, 14 };
static void *s_menu_pause_game;
static u8 s_menu_pause_owned;
static u8 s_slow_motion_owned;
static u8 s_entity_list_ready;
static u8 s_actor_list_ready;
static u8 s_ai_list_ready;
static u8 s_variant_entity_selected;
static u8 s_ai_entity_selected;
static u8 s_animation_entity_selected;
static u8 s_animation_cache_ready;
static u8 s_animation_cache_truncated;
static u16 s_animation_count;
static u8 s_level_cache_ready;
static u8 s_level_cache_truncated;
static u16 s_level_count;
#define s_level_indices ((u16 *)LEVEL_INDEX_CACHE_BASE)
static LevelDescriptor *s_level_descriptors;
static u32 s_level_descriptor_count;
static u8 s_script_cache_ready;
static u8 s_script_cache_layout_error;
static u16 s_script_entry_count;
#define s_script_entry_indices ((u16 *)SCRIPT_INDEX_CACHE_BASE)
#define s_script_entry_owners ((void **)SCRIPT_OWNER_CACHE_BASE)
#define s_script_entry_status ((u8 *)SCRIPT_STATUS_CACHE_BASE)
#ifdef DEBUG_MENU_AUTO_OPEN
static u8 s_auto_open_pending = 1;
static u16 s_auto_open_ready_frames;
#endif
static u16 s_variant_entity_count;
static u32 s_variant_entity_hash;
#define s_variant_entities ((void **)VARIANT_ENTITY_CACHE_BASE)
#define s_variant_interfaces ((void **)(VARIANT_ENTITY_CACHE_BASE + 0x1000u))
#define s_variant_data ((void **)(VARIANT_ENTITY_CACHE_BASE + 0x2000u))
#define s_variant_hashes ((u32 *)(VARIANT_ENTITY_CACHE_BASE + 0x3000u))
#define s_variant_counts ((u16 *)(VARIANT_ENTITY_CACHE_BASE + 0x4000u))
#define s_actor_entities ((void **)ACTOR_ENTITY_CACHE_BASE)
#define s_actor_hashes ((u32 *)(ACTOR_ENTITY_CACHE_BASE + 0x0400u))
static u16 s_actor_entity_count;
static u32 s_ai_entity_hash;
static void *s_ai_entity_core;
static u16 s_ai_core_count;
static u16 s_ai_block_index;
static u32 s_ai_block_hash;
static void *s_ai_inode_order_core;
static void **s_ai_inode_order_data;
static u16 s_ai_inode_order_count;
static AiParamBlock *s_ai_param_order_block;
static void **s_ai_param_order_data;
static u16 s_ai_param_order_count;
static u32 s_animation_entity_hash;
#define s_ai_cores ((void **)AI_CORE_CACHE_BASE)
#define s_ai_entity_title ((char *)AI_TITLE_CACHE_BASE)
#define s_ai_block_title ((char *)(AI_TITLE_CACHE_BASE + 64u))
#define s_ai_inode_order ((u16 *)AI_INODE_ORDER_CACHE_BASE)
#define s_ai_param_order ((u16 *)AI_PARAM_ORDER_CACHE_BASE)
#define s_animation_names ((const char **)ANIMATION_NAME_CACHE_BASE)
#define s_mission_rows ((MissionRow *)MISSION_ROW_CACHE_BASE)
#define s_mission_groups ((MissionGroup *)MISSION_GROUP_CACHE_BASE)
#define s_mission_group_names ((char (*)[32])MISSION_GROUP_NAME_CACHE_BASE)
static u16 s_mission_row_count;
static u8 s_mission_group_count;
static u8 s_active_mission_group;
static u8 s_mission_cache_truncated;
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
static void export_ai_block_to_report(void);
static s32 get_devflag(s32 id);
static void *get_terrain(void);
static s32 get_district_regions(void *terrain, void ***regions_out,
                                u32 *count_out);

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

static void append_hex32(char *out, u32 capacity, u32 value)
{
    static const char digits[] = "0123456789ABCDEF";
    s32 shift;
    string_append(out, capacity, "0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        char one[2];
        one[0] = digits[(value >> (u32)shift) & 0xFu];
        one[1] = 0;
        string_append(out, capacity, one);
    }
}

static void append_fixed_2(char *out, u32 capacity, float value)
{
    s32 scaled;
    u32 fraction;
    if (value >= 0.0f)
        scaled = (s32)(value * 100.0f + 0.5f);
    else
        scaled = (s32)(value * 100.0f - 0.5f);
    append_signed(out, capacity, scaled / 100);
    string_append(out, capacity, ".");
    fraction = scaled < 0
        ? (u32)(-(scaled % 100)) : (u32)(scaled % 100);
    if (fraction < 10u)
        string_append(out, capacity, "0");
    append_unsigned(out, capacity, fraction);
}

static void append_ai_float(char *out, u32 capacity, float value)
{
    union {
        float floating;
        u32 bits;
    } raw;
    raw.floating = value;
    if ((raw.bits & 0x7F800000u) == 0x7F800000u ||
        value > 10000000.0f || value < -10000000.0f) {
        append_hex32(out, capacity, raw.bits);
        return;
    }
    append_fixed_2(out, capacity, value);
}

static volatile float *dvar_float_pointer(const Row *row)
{
    volatile float *value;
    if (!row->value)
        return 0;
    value = (volatile float *)(u32)*row->value;
    return mem1_word_range((const void *)value, sizeof(float)) ? value : 0;
}

static void *game_settings_object(void)
{
    void *settings;
    if (!mem1_word_range(s_game, 0xDCu))
        return 0;
    settings = *(void *volatile *)((u8 *)s_game + 0xD8u);
    if (!mem1_word_range(settings, 0x550u))
        return 0;
    return settings;
}

static volatile u32 *saved_setting_pointer(u32 index)
{
    void *settings = game_settings_object();
    u32 offset;
    if (!settings || index >= SAVED_SETTING_COUNT)
        return 0;
    offset = (u32)kSavedSettingOffsets[index];
    if (offset > 0x180u)
        return 0;
    return (volatile u32 *)((u8 *)settings + 0x358u + offset);
}

static void saved_setting_limits(u32 index, s32 *minimum, s32 *maximum)
{
    *minimum = 0;
    *maximum = 1000;
    if (index == 15u || index == 16u || index == 34u) {
        *maximum = 1;
    } else if (index == 10u) {
        *maximum = 3;
    } else if (index == 14u) {
        *maximum = 2;
    } else if (index == 17u || index == 33u || index == 40u) {
        *maximum = 3;
    } else if (index == 39u) {
        *maximum = 1;
    } else if (index == 61u) {
        *maximum = 100;
    } else if ((index >= 7u && index <= 9u) || index == 11u) {
        *maximum = 10000;
    }
}

static float saved_float_step(u32 index)
{
    if (index == 15u || index == 16u || index == 34u)
        return 0.05f;
    if (index == 38u)
        return 10.0f;
    return 1.0f;
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

static u32 target_string_hash(const char *text)
{
    u32 hash = 0;
    while (text && *text) {
        u8 character = (u8)*text++;
        if (character >= (u8)'A' && character <= (u8)'Z')
            character = (u8)(character + ((u8)'a' - (u8)'A'));
        hash = hash * 33u + (u32)character;
    }
    return hash;
}

static void refresh_level_cache(void)
{
    ResourceKey key;
    void *partition;
    void **slots;
    void *slot;
    void *directory;
    void *resource;
    s32 resource_bytes = 0;
    u32 descriptor_index;

    s_level_cache_ready = 1;
    s_level_cache_truncated = 0;
    s_level_count = 0;
    s_level_descriptors = 0;
    s_level_descriptor_count = 0;

    partition = ((ResourceManagerGetPartitionFn)
        ADDR_RESOURCE_MANAGER_GET_PARTITION_POINTER)(0);
    if (!mem1_word_range(partition, 0x0Cu))
        return;
    slots = *(void **volatile *)((u8 *)partition + 0x08u);
    if (!mem1_word_range(slots, 4u))
        return;
    slot = slots[0];
    if (!mem1_word_range(slot, 0x1Cu))
        return;
    directory = ((ResourcePackSlotGetDirectoryFn)
        ADDR_RESOURCE_PACK_SLOT_GET_RESOURCE_DIRECTORY)(slot);
    if (!mem1_word_range(directory, 4u))
        return;

    key.name_hash = target_string_hash("level");
    key.type = 9;
    resource = ((ResourceDirectoryGetResourceFn)
        ADDR_RESOURCE_DIRECTORY_GET_RESOURCE)(directory, &key,
                                               &resource_bytes, 0);
    if (!resource || resource_bytes <= 0 ||
        (resource_bytes % (s32)sizeof(LevelDescriptor)) != 0 ||
        !mem1_range(resource, (u32)resource_bytes))
        return;

    s_level_descriptors = (LevelDescriptor *)resource;
    s_level_descriptor_count = (u32)resource_bytes / sizeof(LevelDescriptor);
    if (s_level_descriptor_count > 1024u) {
        s_level_descriptors = 0;
        s_level_descriptor_count = 0;
        return;
    }
    for (descriptor_index = 0;
         descriptor_index < s_level_descriptor_count;
         ++descriptor_index) {
        LevelDescriptor *descriptor = &s_level_descriptors[descriptor_index];
        if (safe_mem1_string_length(descriptor->pack_name, 31u) <= 0 ||
            safe_mem1_string_length(descriptor->menu_name, 15u) <= 0 ||
            !pack_exists("/packs/gc/", descriptor->pack_name))
            continue;
        if (s_level_count >= MAX_LEVEL_ROWS) {
            s_level_cache_truncated = 1;
            break;
        }
        s_level_indices[s_level_count++] = (u16)descriptor_index;
    }
}

static LevelDescriptor *level_descriptor_at(u32 row)
{
    u32 descriptor_index;
    if (!s_level_cache_ready)
        refresh_level_cache();
    if (row >= s_level_count || !s_level_descriptors)
        return 0;
    descriptor_index = s_level_indices[row];
    if (descriptor_index >= s_level_descriptor_count ||
        !mem1_range(&s_level_descriptors[descriptor_index],
                    sizeof(LevelDescriptor)))
        return 0;
    return &s_level_descriptors[descriptor_index];
}

static void reset_mission_cache(void)
{
    s_mission_row_count = 0;
    s_mission_group_count = 0;
    s_active_mission_group = 0;
    s_mission_cache_truncated = 0;
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

    for (condition_index = 0; condition_index < condition_count; ++condition_index) {
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
        for (instance_index = 0; instance_index < instance_count; ++instance_index) {
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
            if (s_mission_row_count >= MAX_MISSION_ROWS) {
                s_mission_cache_truncated = 1;
                return (u16)(s_mission_row_count - start);
            }
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
    s32 game_state;
    u8 *desired_name;
    u32 index;
    u32 temporary_string[4];
    if (!s_game) {
        set_message("Hero swap requires active gameplay");
        return 0;
    }
    game_state = ((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game);
    if (game_state != 6 &&
        !(game_state == 7 && s_menu_pause_owned && s_menu_pause_game == s_game)) {
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

    close_menu();
    if (((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game) != 6) {
        set_message("Hero swap could not leave the pause process");
        return 0;
    }

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
     * disabled. Temporarily enable it for the whole retail state machine;
     * update_hero_swap_status restores the exact prior raw state.
     */
    ((GameEnablePhysicsFn)ADDR_GAME_ENABLE_PHYSICS)(s_game, 1);
    /* Publish the request last; retail update_hero_switch owns it from here. */
    *(volatile s32 *)((u8 *)manager + 0xF0u) = 0;
    return 1;
}

/* Chuck's string_hash is a case-insensitive 33x hash.  CITY_ARENA stores the
   full typed signature, but the fixed-DOL function map takes the bare-name hash
   as a four-byte object passed by address. */
static s32 script_handler_hash(const char *signature,
                               u32 *out_hash,
                               u32 *out_parameter_bytes)
{
    u32 hash = 0;
    u32 parameter_bytes;
    const char *cursor = signature;
    if (!cursor || !out_hash || !out_parameter_bytes || !*cursor)
        return 0;
    while (*cursor && *cursor != '(') {
        u8 character = (u8)*cursor++;
        if (character >= (u8)'A' && character <= (u8)'Z')
            character = (u8)(character + ((u8)'a' - (u8)'A'));
        hash = hash * 33u + (u32)character;
    }
    if (string_equal(cursor, "(debug_menu_entry)"))
        parameter_bytes = 4u;
    else if (string_equal(cursor, "()"))
        parameter_bytes = 0u;
    else
        return 0;
    *out_hash = hash;
    *out_parameter_bytes = parameter_bytes;
    return 1;
}

/* Resolve a typed debug-menu callback and the first live instance owned by its
   executable.  expected_owner keeps duplicate bare-name hashes in the mission
   group that exposed the row.  Return 1 when both are live, 0 when the callback
   is absent/from another group, and -1 for an unexpected ABI or invalid retail
   object. */
static s32 resolve_script_handler(const char *signature,
                                  void *expected_owner,
                                  void **executable_out,
                                  void **instance_out,
                                  void **owner_out,
                                  u32 *parameter_bytes_out)
{
    u32 hash;
    u32 expected_parameter_bytes;
    u32 parameter_bytes;
    void *executable;
    void *owner;
    void *instances;
    void *instance;

    if (!script_handler_hash(signature, &hash, &expected_parameter_bytes))
        return -1;
    executable = ((ScriptManagerFindFunctionByNameFn)
        ADDR_SCRIPT_MANAGER_FIND_FUNCTION_BY_NAME)(&hash);
    if (!executable)
        return 0;
    if (!mem1_word_range(executable, 0x18u))
        return -1;

    owner = *(void *volatile *)executable;
    if (!mem1_word_range(owner, 0x30u))
        return -1;
    if (expected_owner && owner != expected_owner)
        return 0;
    instances = *(void *volatile *)((u8 *)owner + 0x2Cu);
    if (!mem1_word_range(instances, 4u))
        return -1;
    instance = *(void *volatile *)instances;
    if (!mem1_word_range(instance, 0x30u) ||
        *(void *volatile *)((u8 *)instance + 0x2Cu) != owner)
        return -1;

    parameter_bytes = *(volatile u32 *)((u8 *)executable + 0x0Cu);
    if (parameter_bytes != expected_parameter_bytes)
        return -1;

    if (executable_out)
        *executable_out = executable;
    if (instance_out)
        *instance_out = instance;
    if (owner_out)
        *owner_out = owner;
    if (parameter_bytes_out)
        *parameter_bytes_out = parameter_bytes;
    return 1;
}

/* Validate the linked VM instruction stream before supplying the four-byte
   debug_menu_entry argument.  SPR offsets are relative to the VM's moving
   data-stack pointer, so an FFFC operand is not proof of formal-parameter use
   (it commonly duplicates the newest temporary). */
static s32 script_executable_layout_valid(const void *executable)
{
    const volatile u16 *buffer;
    u32 word_count;
    u32 position = 0;

    if (!mem1_word_range(executable, 0x18u))
        return 0;
    buffer = *(volatile u16 *volatile *)((u8 *)executable + 0x10u);
    word_count = *(volatile u32 *)((u8 *)executable + 0x14u);
    if (!buffer || ((u32)buffer & 1u) != 0u ||
        !mem1_range((const void *)buffer, 2u) || word_count == 0u ||
        word_count > MAX_SCRIPT_BYTECODE_WORDS ||
        word_count > (0x81800000u - (u32)buffer) / 2u)
        return 0;

    while (position < word_count) {
        u16 opword = buffer[position++];
        u32 argument_type = (u32)opword & 0x7Fu;
        u32 operand_words;

        switch (argument_type) {
        case 0u:
            operand_words = 0u;
            break;
        case 4u:
        case 5u:
        case 6u:
        case 7u:
            operand_words = 1u;
            break;
        case 1u:
        case 2u:
        case 3u:
        case 8u:
        case 9u:
        case 10u:
        case 11u:
        case 15u:
        case 16u:
        case 17u:
            operand_words = 2u;
            break;
        default:
            return 0;
        }

        /* DSIZE occupies one word between the opword and its operand. */
        if ((opword & 0x0080u) != 0u) {
            if (position >= word_count)
                return 0;
            ++position;
        }
        if (operand_words > word_count - position)
            return 0;
        position += operand_words;
    }
    return position == word_count ? 1 : 0;
}

/* Queue a handler on the first live instance owned by its executable.  The
   retail GameCube debug_menu_entry SLF accessors are no-dereference stubs and
   return default values, so the ABI-correct four-byte argument is a null entry
   value rather than a fabricated host-side object. */
static s32 dispatch_script_handler(const char *signature, void *expected_owner)
{
    u32 entry_argument = 0u;
    u32 parameter_bytes;
    void *executable;
    void *instance;
    void *thread;
    s32 result = resolve_script_handler(signature, expected_owner,
                                        &executable, &instance, 0,
                                        &parameter_bytes);

    if (result <= 0)
        return result;
    if (parameter_bytes == 0u) {
        thread = ((ScriptInstanceAddThreadFn)
            ADDR_SCRIPT_INSTANCE_ADD_THREAD)(instance, executable);
    } else if (parameter_bytes == 4u) {
        if (!script_executable_layout_valid(executable))
            return -1;
        thread = ((ScriptInstanceAddThreadWithArgsFn)
            ADDR_SCRIPT_INSTANCE_ADD_THREAD_WITH_ARGS)(
                instance, executable, &entry_argument);
    } else {
        return -1;
    }
    return mem1_word_range(thread, 4u) ? 1 : -1;
}

/* Every original Progression row is installed by CITY_ARENA on one script
   instance.  Resolve a known member first, then require the selected callback
   to belong to that exact owner so a duplicate bare-name hash cannot cross
   script groups. */
static s32 dispatch_progression_handler(const char *signature)
{
    void *city_arena_owner = 0;
    s32 result = resolve_script_handler(s_progression_toggle_on, 0,
                                        0, 0, &city_arena_owner, 0);
    if (result <= 0)
        return result;
    return dispatch_script_handler(signature, city_arena_owner);
}

/* Build a live Script menu from the evidence-derived catalog.  A catalog row
   with a null label is a unique mission-group probe; its following display
   rows are exposed only while that mission's script is resident. */
static void refresh_script_cache(void)
{
    u32 index;
    s32 group_available = 0;
    void *group_owner = 0;

    s_script_entry_count = 0;
    s_script_cache_layout_error = 0;
    for (index = 0; index < ARRAY_COUNT(s_script_entries); ++index) {
        const ScriptEntry *entry = &s_script_entries[index];
        s32 result;

        if (!entry->label) {
            void *executable = 0;
            void *owner = 0;
            result = resolve_script_handler(entry->handler_signature, 0,
                                            &executable, 0, &owner, 0);
            if (result > 0) {
                group_available = 1;
                group_owner = owner;
            } else {
                group_available = 0;
                group_owner = 0;
            }
            if (result < 0)
                s_script_cache_layout_error = 1;
            continue;
        }
        if (!group_available)
            continue;
        {
            void *executable = 0;
            void *owner = 0;
            u32 parameter_bytes = 0;
            u8 row_status = SCRIPT_ROW_DISPATCHABLE;
            result = resolve_script_handler(entry->handler_signature,
                                            group_owner, &executable, 0,
                                            &owner, &parameter_bytes);
            if (result > 0) {
                if (parameter_bytes == 4u) {
                    if (!script_executable_layout_valid(executable)) {
                        row_status = SCRIPT_ROW_UNVERIFIED;
                        s_script_cache_layout_error = 1;
                    }
                }
                if (s_script_entry_count < ARRAY_COUNT(s_script_entries)) {
                    u32 cache_index = s_script_entry_count++;
                    s_script_entry_indices[cache_index] = (u16)index;
                    s_script_entry_owners[cache_index] = owner;
                    s_script_entry_status[cache_index] = row_status;
                }
            } else if (result < 0) {
                s_script_cache_layout_error = 1;
            }
        }
    }
    s_script_cache_ready = 1;
}

static void start_progression_entry(u32 index)
{
    const ProgressionEntry *entry;
    s32 result;
    if (index >= ARRAY_COUNT(s_progression_entries)) {
        set_message("Progression checkpoint is out of range");
        return;
    }
    entry = &s_progression_entries[index];
    result = dispatch_progression_handler(entry->handler_signature);
    if (result > 0) {
        set_message("Retail progression callback queued");
        close_menu();
        return;
    }
    if (result == 0) {
        set_message("CITY_ARENA progression handler is not loaded");
    } else if (result == SCRIPT_DISPATCH_REQUIRES_ENTRY) {
        set_message("Progression callback needs its native debug-menu entry");
    } else {
        set_message("Progression script has an unexpected retail layout");
    }
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
        /* Keep the retail hero-switch state machine advancing. */
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

static void render_district_overlay(void)
{
    static const u32 COLOR_HEADER = 0x70E0FFFFu;
    static const u32 COLOR_ACTIVE = 0xFFFFFFFFu;
    static const u32 COLOR_LOCKED = 0xFF8080FFu;
    void *terrain = get_terrain();
    void **regions;
    u32 region_count;
    u32 region_index;
    u32 shown = 0;
    float y = 18.0f;
    char line[96];

    if (!get_district_regions(terrain, &regions, &region_count))
        return;
    add_text(350.0f, y, COLOR_HEADER, "District streamer");
    y += (float)get_line_height();
    for (region_index = 0; region_index < region_count && shown < 12u;
         ++region_index) {
        void *region = regions[region_index];
        const char *name;
        u32 flags;
        if (!mem1_word_range(region, 0xCCu))
            continue;
        flags = *(volatile u32 *)((u8 *)region + 0x50u);
        if ((flags & 0x4000u) == 0u)
            continue;
        name = *(const char *volatile *)((u8 *)region + 0x2Cu);
        if (safe_mem1_string_length(name, 47u) <= 0)
            continue;
        string_copy(line, sizeof(line), name);
        string_append(line, sizeof(line), " d=");
        append_signed(line, sizeof(line),
                      *(volatile s32 *)((u8 *)region + 0xC0u));
        string_append(line, sizeof(line), " v=");
        append_signed(line, sizeof(line),
                      ((RegionGetDistrictVariantFn)
                          ADDR_REGION_GET_DISTRICT_VARIANT)(region));
        string_append(line, sizeof(line), (flags & 1u) ? " locked" : " active");
        add_text(350.0f, y, (flags & 1u) ? COLOR_LOCKED : COLOR_ACTIVE, line);
        y += (float)get_line_height();
        ++shown;
    }
    if (shown < region_count && shown == 12u)
        add_text(350.0f, y, COLOR_HEADER, "... more regions ...");
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

static s32 devopt_id_for_row(u32 row)
{
    if (row >= DEVOPT_COUNT)
        return -1;
    if (row < ADDR_DEVOPT_FLAG_COUNT)
        return kDevoptKinds[row] == ITEM_BOOL ? (s32)row : -1;
    return kDevoptKinds[row] == ITEM_INT
        ? (s32)(row - ADDR_DEVOPT_FLAG_COUNT)
        : -1;
}

static s32 devopt_id_for_label(const char *label)
{
    u32 row;
    /* Preserve the stock menu caption while resolving the exact GUTE52 name. */
    if (string_equal(label, "User Camera on Controller 2"))
        label = "USERCAM_ON_CONTROLLER2";
    for (row = 0; row < DEVOPT_COUNT; ++row) {
        if (string_equal(kDevoptLabels[row], label))
            return devopt_id_for_row(row);
    }
    return -1;
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

static void *warp_region_at(u32 wanted_row, u32 *index_out)
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
        const char *name;
        if (!mem1_word_range(region, 0xC4u) ||
            (*(volatile u32 *)((u8 *)region + 0x50u) & 0x4000u) == 0u)
            continue;
        name = *(const char *volatile *)((u8 *)region + 0x2Cu);
        if (safe_mem1_string_length(name, 63u) <= 0)
            continue;
        if (row == wanted_row) {
            *index_out = region_index;
            return region;
        }
        ++row;
    }
    return 0;
}

static u32 warp_region_row_count(void)
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
        const char *name;
        if (!mem1_word_range(region, 0xC4u) ||
            (*(volatile u32 *)((u8 *)region + 0x50u) & 0x4000u) == 0u)
            continue;
        name = *(const char *volatile *)((u8 *)region + 0x2Cu);
        if (safe_mem1_string_length(name, 63u) > 0)
            ++count;
    }
    return count;
}

static s32 warp_to_region_row(u32 wanted_row)
{
    void *world = *(void *volatile *)ADDR_WORLD_SLOT;
    void *terrain = get_terrain();
    void *region;
    u32 region_index;
    u32 flags;
    s32 district;

    if (!mem1_word_range(world, 0x220u) || !live_hero_ready(world) || !terrain ||
        *(void *volatile *)((u8 *)world + 0x198u) != terrain)
        return 0;
    region = warp_region_at(wanted_row, &region_index);
    if (!mem1_word_range(region, 0xC4u) ||
        *(void *volatile *)((u8 *)world + 0x198u) != terrain)
        return 0;

    flags = *(volatile u32 *)((u8 *)region + 0x50u);
    district = *(volatile s32 *)((u8 *)region + 0xC0u);
    if (flags & 1u)
        ((TerrainUnlockDistrictFn)ADDR_TERRAIN_UNLOCK_DISTRICT)(terrain, district);
    close_menu();
    if (*(void *volatile *)ADDR_WORLD_SLOT != world ||
        ((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game) != 6)
        return 0;
    ((WorldMalorPointFn)ADDR_WORLD_MALOR_POINT)(
        world, (const float *)((u8 *)region + 0xA4u), 0, 0);
    return 1;
}

static s32 warp_to_current_poi(void)
{
    void *world = *(void *volatile *)ADDR_WORLD_SLOT;
    void *igo;
    void *tracker;
    float position[3] = { 0.0f, 0.0f, 0.0f };

    if (!mem1_word_range(world, 0x220u) || !live_hero_ready(world) ||
        !mem1_word_range((const void *)ADDR_FE_MANAGER, 0x18u))
        return 0;
    igo = *(void *volatile *)(ADDR_FE_MANAGER + 0x14u);
    if (!mem1_word_range(igo, 0x58u))
        return 0;
    tracker = *(void *volatile *)((u8 *)igo + 0x54u);
    if (!mem1_word_range(tracker, 0x14u) ||
        !mem1_word_range(*(void *volatile *)((u8 *)tracker + 4u), 0x0Cu) ||
        !((EntityTrackerArrowFn)ADDR_ENTITY_TRACKER_GET_ARROW_TARGET_POS)(
            tracker, position))
        return 0;
    close_menu();
    if (*(void *volatile *)ADDR_WORLD_SLOT != world ||
        ((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game) != 6)
        return 0;
    ((WorldMalorPointFn)ADDR_WORLD_MALOR_POINT)(world, position, 0, 0);
    return 1;
}

static s32 load_level_row(u32 wanted_row)
{
    LevelDescriptor *descriptor = level_descriptor_at(wanted_row);
    u32 temporary_string[4];
    s32 state;

    if (!descriptor ||
        safe_mem1_string_length(descriptor->pack_name, 31u) <= 0 ||
        !pack_exists("/packs/gc/", descriptor->pack_name) || !s_game)
        return 0;
    state = ((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game);
    if (state != 6 &&
        !(state == 7 && s_menu_pause_owned && s_menu_pause_game == s_game))
        return 0;

    ((MStringFromCharFn)ADDR_MSTRING_FROM_CHAR)(temporary_string,
                                                descriptor->pack_name);
    close_menu();
    if (((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game) != 6) {
        ((MStringFinalizeFn)ADDR_MSTRING_FINALIZE)(temporary_string, 0);
        return 0;
    }
    ((GameLoadNewLevelFn)ADDR_GAME_LOAD_NEW_LEVEL)(s_game, temporary_string);
    ((MStringFinalizeFn)ADDR_MSTRING_FINALIZE)(temporary_string, 0);
    s_level_cache_ready = 0;
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

static s32 entity_is_verified_actor(void *entity)
{
    u8 *vtable;
    void *predicate;
    if (!mem1_word_range(entity, 0x14u))
        return 0;
    vtable = *(u8 *volatile *)entity;
    if (!mem1_word_range(vtable, 0xD8u))
        return 0;
    predicate = *(void *volatile *)(vtable + 0xD4u);
    return (u32)predicate == ADDR_ACTOR_IS_ACTOR;
}

static void clear_actor_entity_cache(void)
{
    s_actor_entity_count = 0;
}

static void refresh_actor_entity_cache(void)
{
    void *world;
    void *list;
    void *sentinel;
    void *previous;
    void *node;
    u32 steps = 0;
    if (s_actor_list_ready)
        return;
    s_actor_list_ready = 1;
    clear_actor_entity_cache();

    world = *(void *volatile *)ADDR_WORLD_SLOT;
    if (!mem1_word_range(world, 0x78u))
        return;
    ((EntityFindEntitiesFn)ADDR_ENTITY_FIND_ENTITIES)(1);
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
        const u32 *hash;
        const char *name;
        if (node == sentinel) {
            if (*(void *volatile *)((u8 *)sentinel + 4u) != previous)
                clear_actor_entity_cache();
            return;
        }
        if (!mem1_word_range(node, 12u) ||
            *(void *volatile *)((u8 *)node + 4u) != previous) {
            clear_actor_entity_cache();
            return;
        }
        next = *(void *volatile *)node;
        if (next == node || !mem1_word_range(next, 12u) ||
            *(void *volatile *)((u8 *)next + 4u) != node) {
            clear_actor_entity_cache();
            return;
        }
        entity = *(void *volatile *)((u8 *)node + 8u);
        if (entity_is_verified_actor(entity) &&
            s_actor_entity_count < MAX_ACTOR_ENTITIES) {
            hash = (const u32 *)((u8 *)entity + 0x10u);
            name = ((StringHashToStringFn)ADDR_STRING_HASH_TO_STRING)(hash);
            if (safe_mem1_string_length(name, 63u) > 0) {
                u32 slot = s_actor_entity_count++;
                s_actor_entities[slot] = entity;
                s_actor_hashes[slot] = *hash;
            }
        }
        previous = node;
        node = next;
        ++steps;
    }
    clear_actor_entity_cache();
}

static s32 selected_actor_index(u32 wanted_hash)
{
    u32 index;
    refresh_actor_entity_cache();
    for (index = 0; index < s_actor_entity_count; ++index) {
        if (s_actor_hashes[index] == wanted_hash)
            return (s32)index;
    }
    return -1;
}

static const char *actor_name_at(u32 index)
{
    void *entity;
    const u32 *hash;
    const char *name;
    if (index >= s_actor_entity_count)
        return 0;
    entity = s_actor_entities[index];
    if (!entity_is_verified_actor(entity))
        return 0;
    hash = (const u32 *)((u8 *)entity + 0x10u);
    name = ((StringHashToStringFn)ADDR_STRING_HASH_TO_STRING)(hash);
    return safe_mem1_string_length(name, 63u) > 0 ? name : 0;
}

static void clear_ai_core_cache(void)
{
    s_ai_core_count = 0;
}

/*
 * The GameCube AI root owns a target-native intrusive list.  It is separate
 * from the generic entity search used by Animations: the PC menu enumerates
 * only high-priority live AI cores and skips actors carrying flag 0x800.
 */
static void refresh_ai_core_cache(void)
{
    void *list;
    void *sentinel;
    void *previous;
    void *node;
    u32 steps = 0;

    if (s_ai_list_ready)
        return;
    s_ai_list_ready = 1;
    clear_ai_core_cache();

    list = *(void *volatile *)ADDR_AI_CORE_LIST_HIGH_SLOT;
    if (!mem1_word_range(list, 8u))
        return;
    sentinel = *(void *volatile *)((u8 *)list + 4u);
    if (!mem1_word_range(sentinel, 12u))
        return;

    previous = sentinel;
    node = *(void *volatile *)sentinel;
    while (steps < MAX_ENTITY_LIST_NODES) {
        void *next;
        void *core;
        void *actor;
        const u32 *hash;
        const char *name;
        if (node == sentinel) {
            if (*(void *volatile *)((u8 *)sentinel + 4u) != previous)
                clear_ai_core_cache();
            return;
        }
        if (!mem1_word_range(node, 12u) ||
            *(void *volatile *)((u8 *)node + 4u) != previous) {
            clear_ai_core_cache();
            return;
        }
        next = *(void *volatile *)node;
        if (next == node || !mem1_word_range(next, 12u) ||
            *(void *volatile *)((u8 *)next + 4u) != node) {
            clear_ai_core_cache();
            return;
        }
        core = *(void *volatile *)((u8 *)node + 8u);
        if (mem1_word_range(core, 0x5Cu)) {
            actor = *(void *volatile *)((u8 *)core + 0x58u);
            if (entity_is_verified_actor(actor) &&
                (*(volatile u32 *)((u8 *)actor + 4u) & 0x800u) == 0u) {
                hash = (const u32 *)((u8 *)actor + 0x10u);
                name = ((StringHashToStringFn)ADDR_STRING_HASH_TO_STRING)(hash);
                if (safe_mem1_string_length(name, 63u) > 0 &&
                    s_ai_core_count < MAX_AI_CORES) {
                    u32 slot = s_ai_core_count++;
                    s_actor_entities[slot] = actor;
                    s_actor_hashes[slot] = *hash;
                    s_ai_cores[slot] = core;
                }
            }
        }
        previous = node;
        node = next;
        ++steps;
    }
    clear_ai_core_cache();
}

static s32 selected_ai_core_index(void)
{
    u32 index;
    if (!s_ai_entity_selected || !s_ai_entity_core)
        return -1;
    refresh_ai_core_cache();
    for (index = 0; index < s_ai_core_count; ++index) {
        if (s_ai_cores[index] == s_ai_entity_core &&
            s_actor_hashes[index] == s_ai_entity_hash)
            return (s32)index;
    }
    return -1;
}

static s32 ai_inode_vector(void *core, void ***data_out, u32 *count_out)
{
    AiVector *vector;
    s32 count;
    void **data;
    *data_out = 0;
    *count_out = 0;
    if (!mem1_word_range(core, 0x5Cu))
        return 0;
    vector = *(AiVector *volatile *)((u8 *)core + 0x54u);
    if (!vector)
        return 1;
    if (!mem1_word_range(vector, sizeof(*vector)))
        return 0;
    count = vector->count;
    data = vector->data;
    if (count < 0 || count > (s32)MAX_AI_INODES ||
        (count && !mem1_word_range(data, (u32)count * 4u)))
        return 0;
    *data_out = data;
    *count_out = (u32)count;
    return 1;
}

static AiParamBlock *ai_block_at(void *core, void *actor, u32 block_index,
                                 u32 expected_inode_hash)
{
    void **inodes;
    u32 inode_count;
    void *inode;
    if (!mem1_word_range(core, 0x5Cu) ||
        *(void *volatile *)((u8 *)core + 0x58u) != actor)
        return 0;
    if (block_index == 0u)
        return (AiParamBlock *)((u8 *)core + 0x44u);
    if (!ai_inode_vector(core, &inodes, &inode_count) ||
        block_index - 1u >= inode_count)
        return 0;
    inode = inodes[block_index - 1u];
    if (!mem1_word_range(inode, 0x1Cu) ||
        *(void *volatile *)((u8 *)inode + 8u) != core ||
        *(void *volatile *)((u8 *)inode + 0x0Cu) != actor ||
        *(volatile u32 *)((u8 *)inode + 4u) != expected_inode_hash)
        return 0;
    return (AiParamBlock *)((u8 *)inode + 0x10u);
}

static AiParamBlock *selected_ai_block(void)
{
    s32 index = selected_ai_core_index();
    if (index < 0)
        return 0;
    return ai_block_at(s_ai_cores[(u32)index], s_actor_entities[(u32)index],
                       s_ai_block_index, s_ai_block_hash);
}

static s32 ai_parameter_vector(AiParamBlock *block, void ***data_out,
                               u32 *count_out, s32 *has_array_out)
{
    AiParamDataArray *array;
    s32 count;
    void **data;
    *data_out = 0;
    *count_out = 0;
    *has_array_out = 0;
    if (!mem1_word_range(block, sizeof(*block)))
        return 0;
    array = block->array;
    if (!array)
        return 1;
    if (!mem1_word_range(array, sizeof(*array)))
        return 0;
    *has_array_out = 1;
    count = array->values.count;
    data = array->values.data;
    if (count < 0 || count > (s32)MAX_AI_PARAMS ||
        (count && !mem1_word_range(data, (u32)count * 4u)))
        return 0;
    *data_out = data;
    *count_out = (u32)count;
    return 1;
}

/*
 * The PC debug menu sorts AI child rows lexically.  Keep an index-only view in
 * reserved MEM1 so the retail core/inode/parameter vectors remain untouched.
 * Source index zero represents the synthetic Core/Export row.
 */
static void make_ai_order_label(char *out, u16 source_index, void **values,
                                u32 hash_offset, const char *synthetic_label)
{
    void *value;
    const char *name;
    if (!source_index) {
        string_copy(out, 64u, synthetic_label);
        return;
    }
    value = values[source_index - 1u];
    if (!mem1_word_range(value, hash_offset + 4u)) {
        out[0] = 0;
        return;
    }
    name = ((StringHashToStringFn)ADDR_STRING_HASH_TO_STRING)(
        (u8 *)value + hash_offset);
    copy_mem1_text(out, 64u, name);
    if (hash_offset == 4u)
        string_append(out, 64u, " inode params");
}

static s32 ai_order_before(u16 left_index, u16 right_index, void **values,
                           u32 hash_offset, const char *synthetic_label)
{
    char left[64];
    char right[64];
    u32 index = 0;
    make_ai_order_label(left, left_index, values, hash_offset, synthetic_label);
    make_ai_order_label(right, right_index, values, hash_offset, synthetic_label);
    while (left[index] && left[index] == right[index])
        ++index;
    if ((u8)left[index] != (u8)right[index])
        return (u8)left[index] < (u8)right[index];
    return left_index < right_index;
}

static void build_ai_order(u16 *order, u32 row_count, void **values,
                           u32 hash_offset, const char *synthetic_label)
{
    u32 gap;
    u32 index;
    for (index = 0; index < row_count; ++index)
        order[index] = (u16)index;
    for (gap = row_count / 2u; gap; gap /= 2u) {
        for (index = gap; index < row_count; ++index) {
            u16 value = order[index];
            u32 destination = index;
            while (destination >= gap &&
                   ai_order_before(value, order[destination - gap], values,
                                   hash_offset, synthetic_label)) {
                order[destination] = order[destination - gap];
                destination -= gap;
            }
            order[destination] = value;
        }
    }
}

static void prepare_ai_inode_order(void *core, void **inodes, u32 inode_count)
{
    u16 row_count = (u16)(inode_count + 1u);
    if (s_ai_inode_order_core == core && s_ai_inode_order_data == inodes &&
        s_ai_inode_order_count == row_count)
        return;
    build_ai_order(s_ai_inode_order, row_count, inodes, 4u, "-Core params");
    s_ai_inode_order_core = core;
    s_ai_inode_order_data = inodes;
    s_ai_inode_order_count = row_count;
}

static void prepare_ai_param_order(AiParamBlock *block, void **parameters,
                                   u32 parameter_count)
{
    u16 row_count = (u16)(parameter_count + 1u);
    if (s_ai_param_order_block == block &&
        s_ai_param_order_data == parameters &&
        s_ai_param_order_count == row_count)
        return;
    build_ai_order(s_ai_param_order, row_count, parameters, 8u,
                   "--Export this block--");
    s_ai_param_order_block = block;
    s_ai_param_order_data = parameters;
    s_ai_param_order_count = row_count;
}

static AiParamData *ai_parameter_at(AiParamBlock *block, u32 parameter_index)
{
    void **parameters;
    u32 count;
    s32 has_array;
    AiParamData *parameter;
    if (!ai_parameter_vector(block, &parameters, &count, &has_array) ||
        !has_array || parameter_index >= count)
        return 0;
    parameter = (AiParamData *)parameters[parameter_index];
    if (!mem1_word_range(parameter, sizeof(*parameter)) ||
        parameter->type < 0 || parameter->type > 7)
        return 0;
    return parameter;
}

static AiParamData *resolve_ai_parameter(const Row *row)
{
    AiParamData *parameter = ai_parameter_at(selected_ai_block(),
                                              row->context_index);
    if (!parameter || parameter->name_hash != (u32)row->context_id ||
        parameter->type != row->minimum)
        return 0;
    return parameter;
}

static void refresh_animation_inventory(void)
{
    MenuDef *menu = &s_menus[MENU_ENTITY_ANIMATION_VALUES];
    s32 actor_index;
    void *actor;
    void *slot;
    void *directory;
    void *vector;
    void *locations;
    u32 file_count;
    u32 file_index;

    s_animation_count = 0;
    s_animation_cache_truncated = 0;
    s_animation_cache_ready = 1;
    menu->labels = s_unknown_animation_names;
    menu->label_count = (u16)ARRAY_COUNT(s_unknown_animation_names);
    menu->dynamic_kind = ITEM_INFO;

    if (!s_animation_entity_selected)
        return;
    actor_index = selected_actor_index(s_animation_entity_hash);
    if (actor_index < 0)
        return;
    actor = s_actor_entities[(u32)actor_index];
    if (!entity_is_verified_actor(actor) || !mem1_word_range(actor, 0xC0u))
        return;
    slot = *(void *volatile *)((u8 *)actor + 0xBCu);
    if (!mem1_word_range(slot, 0x1Cu))
        return;
    directory = ((ResourcePackSlotGetDirectoryFn)
        ADDR_RESOURCE_PACK_SLOT_GET_RESOURCE_DIRECTORY)(slot);
    if (!mem1_word_range(directory, 0x50u))
        return;
    vector = ((ResourceDirectoryTypeVectorFn)
        ADDR_RESOURCE_DIRECTORY_TYPE_TO_VECTOR)(directory, 8);
    if (!mem1_word_range(vector, 8u))
        return;
    locations = *(void *volatile *)vector;
    file_count = *(volatile u16 *)((u8 *)vector + 4u);
    if (file_count > 1024u ||
        (file_count && !mem1_range(locations, file_count * 12u)))
        return;

    for (file_index = 0; file_index < file_count; ++file_index) {
        void *location = (u8 *)locations + file_index * 12u;
        void *file = *(void *volatile *)((u8 *)location + 8u);
        void *animation;
        u32 chain_count = 0;
        if (!mem1_word_range(file, 0x38u) ||
            *(volatile u32 *)file != 0x00010101u)
            continue;
        animation = *(void *volatile *)((u8 *)file + 0x34u);
        while (animation && chain_count++ < 4096u) {
            const char *name;
            void *next;
            if (!mem1_word_range(animation, 0x10u)) {
                s_animation_count = 0;
                return;
            }
            name = (const char *)animation + 0x0Cu;
            if (safe_mem1_string_length(name, 63u) > 0) {
                if (s_animation_count >= MAX_ANIMATION_ROWS) {
                    s_animation_cache_truncated = 1;
                    break;
                }
                s_animation_names[s_animation_count++] = name;
            }
            next = ((NalGetNextAnimFn)ADDR_NAL_GET_NEXT_ANIM)(animation);
            if (next == animation) {
                s_animation_count = 0;
                return;
            }
            animation = next;
        }
        if (chain_count >= 4096u && animation) {
            s_animation_count = 0;
            return;
        }
        if (s_animation_cache_truncated)
            break;
    }

    if (s_animation_count) {
        menu->labels = (const char *const *)s_animation_names;
        menu->label_count = s_animation_count;
        menu->dynamic_kind = ITEM_ANIMATION;
    }
}

static s32 play_selected_animation(const char *name)
{
    s32 actor_index;
    void *actor;
    void *slot;
    void *controller;
    u32 hash = 0;
    AnimationHandle handle = { 0, 0.0f, 0 };

    if (safe_mem1_string_length(name, 63u) <= 0)
        return 0;
    actor_index = selected_actor_index(s_animation_entity_hash);
    if (actor_index < 0)
        return 0;
    actor = s_actor_entities[(u32)actor_index];
    if (!entity_is_verified_actor(actor) || !mem1_word_range(actor, 0xC0u))
        return 0;
    slot = *(void *volatile *)((u8 *)actor + 0xBCu);
    if (!mem1_word_range(slot, 0x70u) ||
        *(volatile u32 *)((u8 *)slot + 8u) < 2u ||
        *(volatile u32 *)((u8 *)slot + 8u) > 4u ||
        !mem1_word_range(*(void *volatile *)((u8 *)slot + 0x18u), 4u))
        return 0;

    ((StringHashFromCharFn)ADDR_STRING_HASH_FROM_CHAR)(&hash, name);
    ((ResourcePushContextFn)ADDR_RESOURCE_MANAGER_PUSH_CONTEXT)(slot);
    controller = *(void *volatile *)((u8 *)actor + 0x74u);
    if (!mem1_word_range(controller, 4u)) {
        void *skeleton = *(void *volatile *)((u8 *)actor + 0x70u);
        ((ActorAllocateAnimationFn)ADDR_ACTOR_ALLOCATE_ANIM_CONTROLLER)(
            actor, 0u, skeleton);
        controller = *(void *volatile *)((u8 *)actor + 0x74u);
    }
    if (mem1_word_range(controller, 0x14u))
        ((AnimationPlayFn)ADDR_ANIMATION_PLAY_BASE_LAYER)(
            &handle, controller, &hash, 0.0f, 32u, 1);
    ((ResourcePopContextFn)ADDR_RESOURCE_MANAGER_POP_CONTEXT)();
    return mem1_word_range(controller, 0x14u) && handle.controller != 0;
}

static u32 menu_row_count(u16 menu_id)
{
    MenuDef *menu = &s_menus[menu_id];
    if (menu_id == MENU_LEVEL_SELECT) {
        if (!s_level_cache_ready)
            refresh_level_cache();
        return (u32)s_level_count + (u32)menu->item_count;
    }
    if (menu_id == MENU_ENTITY_ANIMATION_VALUES && !s_animation_cache_ready)
        refresh_animation_inventory();
    if (menu_id == MENU_SCRIPT) {
        if (!s_script_cache_ready)
            refresh_script_cache();
        return (u32)menu->item_count +
            (s_script_entry_count ? s_script_entry_count : 1u);
    }
    if (menu_id == MENU_WARP)
        return (u32)menu->item_count + warp_region_row_count();
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
        return s_mission_groups[s_active_mission_group].count
            ? s_mission_groups[s_active_mission_group].count : 1u;
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
    if (menu_id == MENU_AI) {
        refresh_ai_core_cache();
        return s_ai_core_count ? s_ai_core_count : 1u;
    }
    if (menu_id == MENU_AI_ENTITY) {
        s32 index = selected_ai_core_index();
        void **inodes;
        u32 inode_count;
        if (index < 0 ||
            !ai_inode_vector(s_ai_cores[(u32)index], &inodes, &inode_count))
            return 1u;
        prepare_ai_inode_order(s_ai_cores[(u32)index], inodes, inode_count);
        return s_ai_inode_order_count;
    }
    if (menu_id == MENU_AI_BLOCK) {
        AiParamBlock *block = selected_ai_block();
        void **parameters;
        u32 parameter_count;
        s32 has_array;
        if (!ai_parameter_vector(block, &parameters, &parameter_count, &has_array) ||
            !has_array)
            return 1u;
        prepare_ai_param_order(block, parameters, parameter_count);
        return s_ai_param_order_count;
    }
    if (menu_id == MENU_ENTITY_ANIMATIONS) {
        refresh_actor_entity_cache();
        return s_actor_entity_count ? s_actor_entity_count : 1u;
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

    if (menu_id == MENU_LEVEL_SELECT) {
        if (!s_level_cache_ready)
            refresh_level_cache();
        if (row_index < s_level_count) {
            LevelDescriptor *descriptor = level_descriptor_at(row_index);
            if (!descriptor)
                return row;
            row.label = copy_mem1_text(generated, generated_capacity,
                                       descriptor->menu_name);
            row.kind = ITEM_LEVEL;
            row.context = descriptor;
            row.context_index = row_index;
            return row;
        }
        row_index -= s_level_count;
        if (row_index < menu->item_count) {
            MenuItem *item = &menu->items[row_index];
            row.label = item->label;
            row.value = &item->value;
            row.minimum = item->minimum;
            row.maximum = item->maximum;
            row.target = item->target;
            row.kind = item->kind;
            row.action = item->action;
        }
        return row;
    }

    if (menu_id == MENU_SCRIPT) {
        u32 catalog_index;
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
        if (!s_script_cache_ready)
            refresh_script_cache();
        if (!s_script_entry_count) {
            row.label = s_script_cache_layout_error
                ? "Loaded script handlers have an unexpected ABI"
                : "No script debug handlers are loaded";
            return row;
        }
        if (row_index >= s_script_entry_count)
            return row;
        catalog_index = s_script_entry_indices[row_index];
        if (catalog_index >= ARRAY_COUNT(s_script_entries))
            return row;
        row.label = s_script_entries[catalog_index].label;
        if (s_script_entry_status[row_index] == SCRIPT_ROW_DISPATCHABLE)
            row.kind = ITEM_SCRIPT_HANDLER;
        else if (s_script_entry_status[row_index] ==
                 SCRIPT_ROW_REQUIRES_ENTRY)
            row.kind = ITEM_SCRIPT_REQUIRES_ENTRY;
        else
            row.kind = ITEM_SCRIPT_UNVERIFIED;
        row.context = s_script_entry_owners[row_index];
        row.context_index = catalog_index;
        return row;
    }

    if (menu_id == MENU_WARP && row_index >= menu->item_count) {
        u32 region_index;
        void *region = warp_region_at(row_index - menu->item_count,
                                      &region_index);
        const char *name;
        if (!region)
            return row;
        name = *(const char *volatile *)((u8 *)region + 0x2Cu);
        row.label = copy_mem1_text(generated, generated_capacity, name);
        row.kind = ITEM_WARP_REGION;
        row.context = region;
        row.context_index = region_index;
        return row;
    }

    if (menu_id == MENU_SAVED_SETTINGS) {
        s32 minimum;
        s32 maximum;
        if (row_index >= SAVED_SETTING_COUNT)
            return row;
        row.label = kSavedSettingLabels[row_index];
        row.context_index = row_index;
        saved_setting_limits(row_index, &minimum, &maximum);
        row.minimum = minimum;
        row.maximum = maximum;
        if (kSavedSettingKinds[row_index] == 1u)
            row.kind = ITEM_SAVED_BOOL;
        else if (kSavedSettingKinds[row_index] == 3u)
            row.kind = ITEM_SAVED_FLOAT;
        else
            row.kind = ITEM_SAVED_INT;
        return row;
    }

    if (menu_id == MENU_AI) {
        const char *name;
        void *actor;
        refresh_ai_core_cache();
        if (!s_ai_core_count) {
            row.label = "No live high-priority AI cores";
            return row;
        }
        if (row_index >= s_ai_core_count)
            return row;
        actor = s_actor_entities[row_index];
        if (!entity_is_verified_actor(actor))
            return row;
        name = ((StringHashToStringFn)ADDR_STRING_HASH_TO_STRING)(
            (u8 *)actor + 0x10u);
        row.label = copy_mem1_text(generated, generated_capacity, name);
        row.context = s_ai_cores[row_index];
        row.context_index = row_index;
        row.context_id = (s32)s_actor_hashes[row_index];
        row.target = MENU_AI_ENTITY;
        row.kind = ITEM_AI_ENTITY;
        return row;
    }

    if (menu_id == MENU_AI_ENTITY) {
        s32 index = selected_ai_core_index();
        void *core;
        void *actor;
        void **inodes;
        u32 inode_count;
        u32 inode_index;
        u16 source_index;
        void *inode;
        const u32 *hash;
        const char *name;
        if (index < 0) {
            row.label = "Selected AI core is no longer live";
            return row;
        }
        core = s_ai_cores[(u32)index];
        actor = s_actor_entities[(u32)index];
        if (!ai_inode_vector(core, &inodes, &inode_count))
            return row;
        prepare_ai_inode_order(core, inodes, inode_count);
        if (row_index >= s_ai_inode_order_count)
            return row;
        source_index = s_ai_inode_order[row_index];
        if (source_index == 0u) {
            row.label = "-Core params";
            row.target = MENU_AI_BLOCK;
            row.kind = ITEM_AI_PARAM_BLOCK;
            row.context = (u8 *)core + 0x44u;
            return row;
        }
        inode_index = (u32)source_index - 1u;
        if (inode_index >= inode_count)
            return row;
        inode = inodes[inode_index];
        if (!mem1_word_range(inode, 0x1Cu) ||
            *(void *volatile *)((u8 *)inode + 8u) != core ||
            *(void *volatile *)((u8 *)inode + 0x0Cu) != actor)
            return row;
        hash = (const u32 *)((u8 *)inode + 4u);
        name = ((StringHashToStringFn)ADDR_STRING_HASH_TO_STRING)(hash);
        copy_mem1_text(generated, generated_capacity, name);
        string_append(generated, generated_capacity, " inode params");
        row.label = generated;
        row.target = MENU_AI_BLOCK;
        row.kind = ITEM_AI_PARAM_BLOCK;
        row.context = (u8 *)inode + 0x10u;
        row.context_index = source_index;
        row.context_id = (s32)*hash;
        return row;
    }

    if (menu_id == MENU_AI_BLOCK) {
        AiParamBlock *block = selected_ai_block();
        void **parameters;
        u32 parameter_count;
        u32 parameter_index;
        s32 has_array;
        u16 source_index;
        AiParamData *parameter;
        const char *name;
        if (!ai_parameter_vector(block, &parameters, &parameter_count,
                                 &has_array)) {
            row.label = "AI parameter block changed; reopen it";
            return row;
        }
        if (!has_array) {
            row.label = "--None defined--";
            return row;
        }
        prepare_ai_param_order(block, parameters, parameter_count);
        if (row_index >= s_ai_param_order_count)
            return row;
        source_index = s_ai_param_order[row_index];
        if (source_index == 0u) {
            row.label = "--Export this block--";
            row.kind = ITEM_ACTION;
            row.action = ACTION_AI_EXPORT;
            return row;
        }
        parameter_index = (u32)source_index - 1u;
        if (parameter_index >= parameter_count)
            return row;
        parameter = (AiParamData *)parameters[parameter_index];
        if (!mem1_word_range(parameter, sizeof(*parameter)) ||
            parameter->type < 0 || parameter->type > 7)
            return row;
        name = ((StringHashToStringFn)ADDR_STRING_HASH_TO_STRING)(
            &parameter->name_hash);
        row.label = copy_mem1_text(generated, generated_capacity, name);
        row.kind = ITEM_AI_PARAM;
        row.context = parameter;
        row.context_index = parameter_index;
        row.context_id = (s32)parameter->name_hash;
        row.minimum = parameter->type;
        return row;
    }

    if (menu_id == MENU_ENTITY_ANIMATIONS) {
        const char *name;
        refresh_actor_entity_cache();
        if (!s_actor_entity_count) {
            row.label = "No live actors found";
            return row;
        }
        name = actor_name_at(row_index);
        if (!name)
            return row;
        row.label = copy_mem1_text(generated, generated_capacity, name);
        row.context = s_actor_entities[row_index];
        row.context_index = row_index;
        row.context_id = (s32)s_actor_hashes[row_index];
        row.target = MENU_ENTITY_ANIMATION_VALUES;
        row.kind = ITEM_ANIMATION_ENTITY;
        return row;
    }

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

    if (menu_id == MENU_MISSIONS) {
        u32 global_count = s_mission_group_count ? s_mission_groups[0].count : 0u;
        u32 district_groups = s_mission_group_count > 1u
            ? (u32)s_mission_group_count - 1u : 0u;
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
        if (row_index < global_count) {
            u32 mission_index = (u32)s_mission_groups[0].start + row_index;
            row.label = make_mission_label(&s_mission_rows[mission_index], generated,
                                           generated_capacity);
            row.kind = ITEM_MISSION;
            row.context = s_mission_rows[mission_index].condition;
            row.context_index = mission_index;
            row.context_id = s_mission_rows[mission_index].district;
            return row;
        }
        row_index -= global_count;
        if (row_index < district_groups) {
            u32 group_index = row_index + 1u;
            row.label = s_mission_groups[group_index].name;
            row.target = MENU_MISSION_DISTRICT;
            row.kind = ITEM_MISSION_DISTRICT;
            row.context_index = group_index;
            row.context_id = s_mission_groups[group_index].district;
            return row;
        }
        if (!global_count && !district_groups) {
            row.label = s_mission_world
                ? "No compatible mission packs found"
                : "Missions require a loaded hero";
            return row;
        }
        return row;
    }

    if (menu_id == MENU_MISSION_DISTRICT) {
        MissionGroup *group;
        u32 mission_index;
        if (s_active_mission_group >= s_mission_group_count)
            return row;
        group = &s_mission_groups[s_active_mission_group];
        if (!group->count) {
            row.label = s_mission_cache_truncated
                ? "Mission cache full; reopen after unloading districts"
                : "No compatible missions for the current hero";
            return row;
        }
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
        if (item->kind == ITEM_PROGRESSION)
            row.context_index = (u32)item->value;
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
    row.minimum = -1000000;
    row.maximum = 1000000;
    row.kind = menu->label_kinds ? menu->label_kinds[row_index] : menu->dynamic_kind;
    if (menu_id == MENU_DEBUG_RENDER) {
        row.kind = ITEM_DEBUG_RENDER;
        row.context_index = row_index;
        row.minimum = ((DebugRenderGetFn)ADDR_DEBUG_RENDER_GET_MIN)(
            (s32)row_index);
        row.maximum = ((DebugRenderGetFn)ADDR_DEBUG_RENDER_GET_MAX)(
            (s32)row_index);
    }
    if (menu_id == MENU_DEVOPTS && row.kind == ITEM_INT) {
        if (string_equal(row.label, "GOD_MODE")) {
            row.minimum = 0;
            row.maximum = 5;
        } else if (string_equal(row.label, "FRAME_LOCK")) {
            row.minimum = 0;
            row.maximum = 120;
        }
        row.kind = ITEM_DEVINT;
    }
    return row;
}

static s32 physics_enabled(void)
{
    if (!s_game)
        return 0;
    return *(volatile u32 *)((u8 *)s_game + 0x1A8u) == 0u;
}

/*
 * Retail slab_allocator layout, verified from GUTE52's allocate/deallocate
 * paths. A simple_list header is { first, last, count }; slab_t links through
 * +0x14 and stores object size/capacity/allocated at +0x0A/+0x0C/+0x0E.
 */
static s32 slab_list_count(const void *list)
{
    u32 count;
    if (!mem1_word_range(list, 12u))
        return 0;
    count = *(volatile u32 *)((const u8 *)list + 8u);
    return count <= 4096u ? (s32)count : 0;
}

static void *slab_bucket_list(u32 slot_address, u32 bucket)
{
    void *lists;
    if (bucket >= 44u)
        return 0;
    lists = *(void *volatile *)slot_address;
    if (!mem1_word_range(lists, 44u * 12u))
        return 0;
    return (u8 *)lists + bucket * 12u;
}

static s32 slab_object_count_in_list(const void *list, u32 bucket, s32 free_objects)
{
    void *node;
    u32 expected_size = (bucket + 1u) * 4u;
    u32 steps = 0;
    u32 total = 0;
    u32 declared;
    if (!mem1_word_range(list, 12u))
        return 0;
    declared = *(volatile u32 *)((const u8 *)list + 8u);
    if (declared > 4096u)
        return 0;
    node = *(void *volatile *)list;
    while (node && steps < declared) {
        s32 object_size;
        s32 capacity;
        s32 allocated;
        void *next;
        if (!mem1_word_range(node, 0x24u))
            return 0;
        object_size = *(volatile s16 *)((u8 *)node + 0x0Au);
        capacity = *(volatile s16 *)((u8 *)node + 0x0Cu);
        allocated = *(volatile s16 *)((u8 *)node + 0x0Eu);
        if (object_size != (s32)expected_size || capacity < 0 ||
            allocated < 0 || allocated > capacity || capacity > 480)
            return 0;
        total += (u32)(free_objects ? capacity - allocated : allocated);
        if (total > 0x7FFFFFFFu)
            return 0;
        next = *(void *volatile *)((u8 *)node + 0x14u);
        if (next == node)
            return 0;
        node = next;
        ++steps;
    }
    return steps == declared && !node ? (s32)total : 0;
}

static s32 slab_bucket_value(u16 menu_id, u32 bucket)
{
    void *partial = slab_bucket_list(ADDR_SLAB_PARTIAL_LISTS_SLOT, bucket);
    void *full = slab_bucket_list(ADDR_SLAB_FULL_LISTS_SLOT, bucket);
    if (*(volatile u32 *)ADDR_SLAB_INITIALIZED != 1u)
        return 0;
    if (menu_id == MENU_FULL_SLABS)
        return slab_list_count(full);
    if (menu_id == MENU_PARTIAL_SLABS)
        return slab_list_count(partial);
    if (menu_id == MENU_ALLOCATED_OBJECTS)
        return slab_object_count_in_list(partial, bucket, 0) +
               slab_object_count_in_list(full, bucket, 0);
    if (menu_id == MENU_FREE_OBJECTS)
        return slab_object_count_in_list(partial, bucket, 1) +
               slab_object_count_in_list(full, bucket, 1);
    return 0;
}

static s32 slab_total_count(void)
{
    u32 bucket;
    s32 total = 0;
    void *free_list;
    if (*(volatile u32 *)ADDR_SLAB_INITIALIZED != 1u)
        return 0;
    free_list = *(void *volatile *)ADDR_SLAB_FREE_LIST_SLOT;
    total += slab_list_count(free_list);
    for (bucket = 0; bucket < 44u; ++bucket) {
        total += slab_list_count(slab_bucket_list(ADDR_SLAB_PARTIAL_LISTS_SLOT, bucket));
        total += slab_list_count(slab_bucket_list(ADDR_SLAB_FULL_LISTS_SLOT, bucket));
    }
    return total;
}

static s32 ngl_debug_flag(u32 offset)
{
    if (offset >= 19u)
        return 0;
    return *(volatile u8 *)(ADDR_NGL_DEBUG_FLAGS + offset) != 0u;
}

static void set_ngl_debug_flag(u32 offset, s32 enabled)
{
    u8 value;
    if (offset >= 19u)
        return;
    value = enabled ? 1u : 0u;
    *(volatile u8 *)(ADDR_NGL_DEBUG_FLAGS + offset) = value;
    /* nglSyncDebug immediately follows the 19-byte nglDebug object. */
    *(volatile u8 *)(ADDR_NGL_DEBUG_FLAGS + 19u + offset) = value;
}

static s32 row_value(u16 menu_id, u32 row_index, const Row *row)
{
    s32 id;
    /* Dynamic Devopts use their generated target-native table order as IDs. */
    if (menu_id == MENU_DEVOPTS && row_index < DEVOPT_COUNT) {
        id = devopt_id_for_row(row_index);
        if (id >= 0 && kDevoptKinds[row_index] == ITEM_BOOL)
            return get_devflag(id);
        if (id >= 0 && kDevoptKinds[row_index] == ITEM_INT)
            return get_devint(id);
        return 0;
    }
    if (row->kind == ITEM_PHYSICS)
        return physics_enabled();
    if (row->kind == ITEM_DEVFLAG)
        return get_devflag(row->value ? *row->value : 0);
    if (row->kind == ITEM_NAMED_DEVFLAG) {
        id = devopt_id_for_label(row->label);
        return id >= 0 ? get_devflag(id) : 0;
    }
    if (row->kind == ITEM_DEVINT)
        return get_devint(row->value ? *row->value : 0);
    if (row->kind == ITEM_SLOW_MOTION)
        return row->value ? *row->value : 0;
    if (row->kind == ITEM_SLAB_TOTAL)
        return slab_total_count();
    if (row->kind == ITEM_SLAB_FREE) {
        void *free_list = *(void *volatile *)ADDR_SLAB_FREE_LIST_SLOT;
        return *(volatile u32 *)ADDR_SLAB_INITIALIZED == 1u
            ? slab_list_count(free_list) : 0;
    }
    if (row->kind == ITEM_SLAB_BUCKET)
        return slab_bucket_value(menu_id, row_index);
    if (row->kind == ITEM_DVAR_FLOAT) {
        volatile float *value = dvar_float_pointer(row);
        return value ? (s32)*value : 0;
    }
    if (row->kind == ITEM_MONKEY)
        return ((BoolFn)ADDR_SPIDER_MONKEY_IS_RUNNING)() != 0;
    if (row->kind == ITEM_SAVED_BOOL || row->kind == ITEM_SAVED_INT) {
        volatile u32 *saved = saved_setting_pointer(row->context_index);
        return saved ? (s32)*saved : 0;
    }
    if (row->kind == ITEM_SAVED_FLOAT) {
        volatile float *saved = (volatile float *)saved_setting_pointer(row->context_index);
        return saved ? (s32)*saved : 0;
    }
    if (row->kind == ITEM_NGL_PERF_INFO)
        return *(volatile u8 *)(ADDR_NGL_DEBUG_FLAGS + 4u);
    if (row->kind == ITEM_NGL_FLAG)
        return ngl_debug_flag(row->value ? (u32)*row->value : 0u);
    if (row->kind == ITEM_DEBUG_RENDER && row->context_index < DEBUG_RENDER_COUNT)
        return ((DebugRenderGetFn)ADDR_DEBUG_RENDER_GET_IVAL)(
            (s32)row->context_index);
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

/*
 * Retail GC does not expose spider_monkey::stop as a standalone symbol.  This
 * mirrors that routine with the exact live input-callback slots and globals
 * used by spider_monkey::start/is_running in GUTE52.
 */
static void set_monkey_enabled(s32 enabled)
{
    s32 monkey_mode_id = devopt_id_for_label("MONKEY_MODE");
    void *input = *(void *volatile *)ADDR_INPUT_SINGLETON_SLOT;
    s32 running = ((BoolFn)ADDR_SPIDER_MONKEY_IS_RUNNING)() != 0;

    if (enabled) {
        if (!running) {
            ((VoidFn)ADDR_SPIDER_MONKEY_START)();
            ((VoidFn)ADDR_SPIDER_MONKEY_ON_LEVEL_LOAD)();
        }
        apply_rumble_enabled(0);
        return;
    }

    if (running)
        ((VoidFn)ADDR_SPIDER_MONKEY_ON_LEVEL_UNLOAD)();
    if (mem1_word_range(input, 0x38u)) {
        ((InputMonkeyCallbackFn)ADDR_INPUT_SET_STATE_MONKEY_CALLBACK)(input, 0);
        ((InputMonkeyCallbackFn)ADDR_INPUT_SET_DELTA_MONKEY_CALLBACK)(input, 0);
    }
    *(volatile u32 *)ADDR_SPIDER_MONKEY_RUNNING = 0u;
    *(volatile float *)ADDR_SPIDER_MONKEY_OOK_TIMER = 0.0f;
    *(volatile float *)ADDR_SPIDER_MONKEY_RUNTIME = 0.0f;
    *(void *volatile *)ADDR_SPIDER_MONKEY_RUNTIME_TEXT = 0;
    *(void *volatile *)ADDR_SPIDER_MONKEY_RUNTIME_MONKEY_TEXT = 0;
    if (monkey_mode_id >= 0)
        set_devint(monkey_mode_id, 0);
}

static void run_action(u8 action, const char *label)
{
    s32 result;
    switch (action) {
    case ACTION_SINGLE_STEP:
        if (mem1_word_range(s_game, 0x1A8u)) {
            *(volatile u32 *)((u8 *)s_game + 0x1A4u) = 1u;
            set_message("Single-step armed; close the menu to advance one frame");
        }
        break;
    case ACTION_NGL_SCREENSHOT:
        set_ngl_debug_flag(5u, 1);
        set_message("NGL screenshot requested");
        break;
    case ACTION_SAVE_GAME: {
        void *settings = game_settings_object();
        s32 slot;
        if (!settings) {
            set_message("No live game_settings object");
            break;
        }
        slot = *(volatile s32 *)((u8 *)settings + 0x53Cu);
        if (slot < 0 || slot > 2) {
            set_message("Current save slot is invalid");
            break;
        }
        if (*(volatile u32 *)ADDR_MEMORY_UNIT_OPERATION != 0u) {
            set_message("The memory card is busy");
            break;
        }
        if (*(volatile u8 *)((u8 *)settings + 0x546u)) {
            set_message("A load-game transition is already active");
            break;
        }
        close_menu();
        ((GameSettingsSlotFn)ADDR_GAME_SETTINGS_SAVE)(settings, slot);
        break;
    }
    case ACTION_LOAD_GAME: {
        void *settings = game_settings_object();
        s32 slot;
        if (!settings) {
            set_message("No live game_settings object");
            break;
        }
        slot = *(volatile s32 *)((u8 *)settings + 0x53Cu);
        if (slot < 0 || slot > 2) {
            set_message("Current save slot is invalid");
            break;
        }
        if (*(volatile u32 *)ADDR_MEMORY_UNIT_OPERATION != 0u) {
            set_message("The memory card is busy");
            break;
        }
        if (*(volatile u8 *)((u8 *)settings + 0x546u)) {
            set_message("A load-game transition is already active");
            break;
        }
        if (!*(volatile u8 *)((u8 *)settings + 0x540u + (u32)slot)) {
            set_message("The current save slot has no loaded save");
            break;
        }
        close_menu();
        ((GameSettingsSlotFn)ADDR_GAME_SETTINGS_LOAD_GAME)(settings, slot);
        break;
    }
    case ACTION_AUTO_LOAD: {
        void *settings = game_settings_object();
        if (!settings) {
            set_message("No live game_settings object");
            break;
        }
        if (*(volatile u32 *)ADDR_MEMORY_UNIT_OPERATION != 0u) {
            set_message("The memory card is busy");
            break;
        }
        if (*(volatile u8 *)((u8 *)settings + 0x546u)) {
            set_message("A load-game transition is already active");
            break;
        }
        if (!*(volatile u8 *)((u8 *)settings + 0x540u) &&
            !*(volatile u8 *)((u8 *)settings + 0x541u) &&
            !*(volatile u8 *)((u8 *)settings + 0x542u)) {
            set_message("No valid save is loaded from the memory card");
            break;
        }
        close_menu();
        ((GameSettingsFn)ADDR_GAME_SETTINGS_LOAD_MOST_RECENT)(settings);
        break;
    }
    case ACTION_HIRES_SCREENSHOT: {
        s32 width_id = devopt_id_for_label("HIRES_SCREENSHOT_X");
        s32 height_id = devopt_id_for_label("HIRES_SCREENSHOT_Y");
        s32 state;
        s32 width;
        s32 height;
        if (!s_game) {
            set_message("Hires capture requires active gameplay");
            break;
        }
        state = ((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game);
        if (state != 6 &&
            !(state == 7 && s_menu_pause_owned && s_menu_pause_game == s_game)) {
            set_message("Hires capture requires active gameplay");
            break;
        }
        if (((BoolFn)ADDR_NGL_HIRES_SCREENSHOT_IN_PROGRESS)()) {
            set_message("A hires capture is already in progress");
            break;
        }
        if (width_id < 0 || height_id < 0) {
            set_message("Hires dimensions are absent from the retail Devopts");
            break;
        }
        width = get_devint(width_id);
        height = get_devint(height_id);
        if (width < 640 || height < 480 || width > 8192 || height > 8192) {
            set_message("Hires dimensions must be 640x480 through 8192x8192");
            break;
        }
        *(volatile s32 *)ADDR_NGL_SCREEN_WIDTH = width;
        *(volatile s32 *)ADDR_NGL_SCREEN_HEIGHT = height;
        close_menu();
        if (((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game) != 6)
            break;
        ((GameProcessCtorFn)ADDR_GAME_PROCESS_CTOR)(
            &s_hires_process, "hires screenshot", s_hires_states,
            (s32)ARRAY_COUNT(s_hires_states));
        ((GamePushProcessFn)ADDR_GAME_PUSH_PROCESS)(s_game, &s_hires_process);
        break;
    }
    case ACTION_LORES_SCREENSHOT:
        set_ngl_debug_flag(5u, 1);
        set_message("Lores screenshot requested through retail GC NGL");
        break;
    case ACTION_WARP_POI:
        if (warp_to_current_poi())
            close_menu();
        else
            set_message("No live mission arrow target is available");
        break;
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
    case ACTION_MEMTRACK_DUMP: {
        u32 alloced = ((MemoryStatFn)ADDR_MEM_GET_TOTAL_ALLOCED)(0);
        u32 free_bytes = ((MemoryStatFn)ADDR_MEM_GET_BYTES_FREE)(0);
        if (!s_memtrack_checkpoint_valid) {
            s_memtrack_checkpoint_alloced = alloced;
            s_memtrack_checkpoint_free = free_bytes;
            s_memtrack_checkpoint_valid = 1;
            set_message("Memory checkpoint initialized; run Dump again for deltas");
            break;
        }
        s_memtrack_message[0] = 0;
        string_append(s_memtrack_message, sizeof(s_memtrack_message), "Allocated delta: ");
        append_signed(s_memtrack_message, sizeof(s_memtrack_message),
                      (s32)(alloced - s_memtrack_checkpoint_alloced));
        string_append(s_memtrack_message, sizeof(s_memtrack_message),
                      " bytes; free delta: ");
        append_signed(s_memtrack_message, sizeof(s_memtrack_message),
                      (s32)(free_bytes - s_memtrack_checkpoint_free));
        set_message(s_memtrack_message);
        break;
    }
    case ACTION_MEMTRACK_CHECKPOINT:
        s_memtrack_checkpoint_alloced =
            ((MemoryStatFn)ADDR_MEM_GET_TOTAL_ALLOCED)(0);
        s_memtrack_checkpoint_free =
            ((MemoryStatFn)ADDR_MEM_GET_BYTES_FREE)(0);
        s_memtrack_checkpoint_valid = 1;
        set_message("Payload memory checkpoint captured");
        break;
    case ACTION_AI_EXPORT:
        export_ai_block_to_report();
        break;
    case ACTION_REBOOT: set_message("Xbox XBE reboot has no GameCube equivalent"); break;
    case ACTION_SELECT_HERO:
        queue_hero_swap(label);
        break;
    case ACTION_PROGRESSION_ON:
        result = dispatch_progression_handler(s_progression_toggle_on);
        if (result > 0) {
            set_message("Retail progression-on callback queued");
            close_menu();
        } else if (result == 0) {
            set_message("CITY_ARENA progression handler is not loaded");
        } else if (result == SCRIPT_DISPATCH_REQUIRES_ENTRY) {
            set_message("Progression callback needs its native debug-menu entry");
        } else {
            set_message("Progression script has an unexpected retail layout");
        }
        break;
    case ACTION_PROGRESSION_OFF:
        result = dispatch_progression_handler(s_progression_toggle_off);
        if (result > 0) {
            set_message("Retail progression-off callback queued");
            close_menu();
        } else if (result == 0) {
            set_message("CITY_ARENA progression handler is not loaded");
        } else if (result == SCRIPT_DISPATCH_REQUIRES_ENTRY) {
            set_message("Progression callback needs its native debug-menu entry");
        } else {
            set_message("Progression script has an unexpected retail layout");
        }
        break;
    case ACTION_PROGRESSION_ALL_TOKENS:
        result = dispatch_progression_handler(s_progression_all_tokens);
        if (result > 0) {
            set_message("Retail all-tokens callback queued");
            close_menu();
        } else if (result == 0) {
            set_message("CITY_ARENA progression handler is not loaded");
        } else if (result == SCRIPT_DISPATCH_REQUIRES_ENTRY) {
            set_message("All tokens needs its native debug-menu entry");
        } else {
            set_message("Progression script has an unexpected retail layout");
        }
        break;
    case ACTION_UNLOAD_DISTRICTS: {
        void *terrain = get_terrain();
        void *partition6 = ((ResourceManagerGetPartitionFn)
            ADDR_RESOURCE_MANAGER_GET_PARTITION_POINTER)(6);
        void *partition5 = ((ResourceManagerGetPartitionFn)
            ADDR_RESOURCE_MANAGER_GET_PARTITION_POINTER)(5);
        void *streamer6;
        void *streamer5;
        s32 active;
        if (!terrain || !mem1_word_range(partition6, 0x1Cu) ||
            !mem1_word_range(partition5, 0x1Cu)) {
            set_message("Terrain streaming partitions are not loaded");
            break;
        }
        streamer6 = (u8 *)partition6 + 0x18u;
        streamer5 = (u8 *)partition5 + 0x18u;
        active = *(volatile u32 *)streamer6 != 0u ||
                 *(volatile u32 *)streamer5 != 0u;
        if (active) {
            ((TerrainUnloadAllDistrictsFn)
                ADDR_TERRAIN_UNLOAD_ALL_DISTRICTS_IMMEDIATE)(terrain);
            ((ResourceStreamerSetActiveFn)ADDR_RESOURCE_PACK_STREAMER_SET_ACTIVE)(
                streamer6, 0);
            ((ResourceStreamerSetActiveFn)ADDR_RESOURCE_PACK_STREAMER_SET_ACTIVE)(
                streamer5, 0);
        } else {
            ((ResourceStreamerSetActiveFn)ADDR_RESOURCE_PACK_STREAMER_SET_ACTIVE)(
                streamer6, 1);
            ((ResourceStreamerSetActiveFn)ADDR_RESOURCE_PACK_STREAMER_SET_ACTIVE)(
                streamer5, 1);
        }
        close_menu();
        break;
    }
    default: set_message("Action acknowledged"); break;
    }
}

static void open_menu(void)
{
    s32 state;
    if (!mem1_word_range(s_game, 0x1B4u))
        return;
    state = ((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game);
    if (state != 6 && state != 7)
        return;
    s_menu_pause_owned = 0;
    s_menu_pause_game = 0;
    if (state == 6) {
        ((GamePauseModeFn)ADDR_GAME_PAUSE)(s_game);
        if (((GameGetCurStateFn)ADDR_GAME_GET_CUR_STATE)(s_game) != 7)
            return;
        s_menu_pause_owned = 1;
        s_menu_pause_game = s_game;
    }
    s_open = 1;
    s_current_menu = MENU_ROOT;
    s_script_cache_ready = 0;
    set_message(0);
}

static void close_menu(void)
{
    void *paused_game = s_menu_pause_game;
    u8 release_pause = s_menu_pause_owned;
    s_open = 0;
    s_script_cache_ready = 0;
    s_menu_pause_owned = 0;
    s_menu_pause_game = 0;
    if (release_pause && paused_game == s_game &&
        mem1_word_range(paused_game, 0x1B4u) &&
        *(volatile u32 *)((u8 *)paused_game + 0x1B0u) != 0u)
        ((GamePauseModeFn)ADDR_GAME_UNPAUSE)(paused_game);
}

static void change_row(s32 direction, s32 activate)
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
    if (row.kind == ITEM_AI_ENTITY) {
        if (direction > 0) {
            s32 actor_index;
            s_ai_entity_hash = (u32)row.context_id;
            s_ai_entity_core = row.context;
            s_ai_entity_selected = 1;
            s_ai_inode_order_core = 0;
            s_ai_param_order_block = 0;
            actor_index = selected_ai_core_index();
            if (actor_index >= 0) {
                const char *name = ((StringHashToStringFn)
                    ADDR_STRING_HASH_TO_STRING)(
                        (u8 *)s_actor_entities[(u32)actor_index] + 0x10u);
                copy_mem1_text(s_ai_entity_title, 64u, name);
                s_menus[MENU_AI_ENTITY].title = s_ai_entity_title;
                s_ai_block_index = 0;
                s_ai_block_hash = 0;
                s_selected[MENU_AI_ENTITY] = 0;
                s_window_start[MENU_AI_ENTITY] = 0;
                s_current_menu = MENU_AI_ENTITY;
                set_message("Live high-priority AI core resolved");
            } else {
                s_ai_entity_selected = 0;
                s_ai_entity_core = 0;
                set_message("AI core changed; reopen AI");
            }
        }
        return;
    }
    if (row.kind == ITEM_AI_PARAM_BLOCK) {
        if (direction > 0) {
            AiParamBlock *block;
            s_ai_block_index = (u16)row.context_index;
            s_ai_block_hash = (u32)row.context_id;
            s_ai_param_order_block = 0;
            block = selected_ai_block();
            if (block) {
                string_copy(s_ai_block_title, 64u, row.label);
                s_menus[MENU_AI_BLOCK].title = s_ai_block_title;
                s_selected[MENU_AI_BLOCK] = 0;
                s_window_start[MENU_AI_BLOCK] = 0;
                s_current_menu = MENU_AI_BLOCK;
                set_message(0);
            } else {
                set_message("AI parameter block changed; reopen AI");
            }
        }
        return;
    }
    if (row.kind == ITEM_AI_PARAM) {
        AiParamData *parameter;
        if (activate)
            return;
        parameter = resolve_ai_parameter(&row);
        if (!parameter) {
            set_message("AI parameter changed; reopen this block");
            return;
        }
        if (parameter->type == 0) {
            const float maximum = 3.402823466e+38F;
            float current = parameter->value.floating;
            float delta = direction > 0 ? 9.0f : -0.30000001f;
            if (direction > 0 && current > maximum - delta)
                parameter->value.floating = maximum;
            else if (direction < 0 && current < -maximum - delta)
                parameter->value.floating = -maximum;
            else
                parameter->value.floating = current + delta;
        } else if (parameter->type == 1) {
            s32 current = parameter->value.integer;
            if (direction > 0) {
                if (current < 0x7FFFFFFF)
                    parameter->value.integer = current + 1;
            } else if (current > (-0x7FFFFFFF - 1)) {
                parameter->value.integer = current - 1;
            }
        }
        return;
    }
    if (row.kind == ITEM_ANIMATION_ENTITY) {
        if (direction > 0) {
            s32 actor_index = selected_actor_index((u32)row.context_id);
            if (actor_index >= 0) {
                const char *name = actor_name_at((u32)actor_index);
                s_animation_entity_hash = (u32)row.context_id;
                s_animation_entity_selected = 1;
                s_animation_cache_ready = 0;
                refresh_animation_inventory();
                if (name)
                    s_menus[MENU_ENTITY_ANIMATION_VALUES].title = name;
                s_selected[MENU_ENTITY_ANIMATION_VALUES] = 0;
                s_window_start[MENU_ENTITY_ANIMATION_VALUES] = 0;
                s_current_menu = MENU_ENTITY_ANIMATION_VALUES;
                set_message(s_animation_cache_truncated
                    ? "Animation list capped at 512 entries"
                    : (s_animation_count ? 0 : "Actor has no animation resources"));
            }
        }
        return;
    }
    if (row.kind == ITEM_ANIMATION) {
        if (direction > 0) {
            if (play_selected_animation(row.label))
                close_menu();
            else
                set_message("Actor or animation resources changed; reopen Animations");
        }
        return;
    }
    if (row.kind == ITEM_LEVEL) {
        if (direction > 0 && !load_level_row(row.context_index))
            set_message("Level resources changed; reopen Level Select");
        return;
    }
    if (row.kind == ITEM_WARP_REGION) {
        if (direction > 0) {
            u32 dynamic_row = selected - s_menus[MENU_WARP].item_count;
            if (warp_to_region_row(dynamic_row))
                close_menu();
            else
                set_message("Region or live player changed; reopen Warp");
        }
        return;
    }
    if (row.kind == ITEM_SCRIPT_HANDLER ||
        row.kind == ITEM_SCRIPT_REQUIRES_ENTRY ||
        row.kind == ITEM_SCRIPT_UNVERIFIED) {
        if (direction > 0) {
            s32 result;
            if (row.context_index >= ARRAY_COUNT(s_script_entries)) {
                set_message("Script handler is out of range");
                return;
            }
            if (row.kind == ITEM_SCRIPT_REQUIRES_ENTRY) {
                set_message("Handler needs its native debug-menu entry");
                return;
            }
            if (row.kind == ITEM_SCRIPT_UNVERIFIED) {
                set_message("Handler bytecode is not verified; call blocked");
                return;
            }
            result = dispatch_script_handler(
                s_script_entries[row.context_index].handler_signature,
                row.context);
            if (result > 0) {
                set_message("Retail script callback queued");
                close_menu();
            } else if (result == 0) {
                s_script_cache_ready = 0;
                set_message("Script handler is no longer loaded");
            } else if (result == SCRIPT_DISPATCH_REQUIRES_ENTRY) {
                s_script_cache_ready = 0;
                set_message("Handler now needs its native debug-menu entry");
            } else {
                set_message("Script handler has an unexpected retail layout");
            }
        }
        return;
    }
    if (row.kind == ITEM_PROGRESSION) {
        if (direction > 0)
            start_progression_entry(row.context_index);
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
                s_selected[row.target] = 0;
                s_window_start[row.target] = 0;
            }
            if (row.target == MENU_AI) {
                s_ai_list_ready = 0;
                s_ai_entity_selected = 0;
                s_ai_entity_core = 0;
                s_selected[row.target] = 0;
                s_window_start[row.target] = 0;
            }
            if (row.target == MENU_ENTITY_ANIMATIONS) {
                s_actor_list_ready = 0;
                s_animation_entity_selected = 0;
                s_animation_cache_ready = 0;
                s_selected[row.target] = 0;
                s_window_start[row.target] = 0;
            }
            if (row.target == MENU_SCRIPT) {
                s_script_cache_ready = 0;
                refresh_script_cache();
                s_selected[row.target] = 0;
                s_window_start[row.target] = 0;
            }
            if (row.target == MENU_LEVEL_SELECT) {
                s_level_cache_ready = 0;
                s_selected[row.target] = 0;
                s_window_start[row.target] = 0;
            }
            s_menus[row.target].parent = menu_id;
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
    if (row.kind == ITEM_DVAR_FLOAT) {
        volatile float *target = dvar_float_pointer(&row);
        float next;
        if (!target)
            return;
        next = *target + (direction > 0 ? 0.25f : -0.25f);
        if (next < (float)row.minimum)
            next = (float)row.minimum;
        if (next > (float)row.maximum)
            next = (float)row.maximum;
        *target = next;
        return;
    }
    if (row.kind == ITEM_MONKEY) {
        set_monkey_enabled(!value);
        close_menu();
        return;
    }
    if (row.kind == ITEM_SAVED_BOOL) {
        volatile u32 *target = saved_setting_pointer(row.context_index);
        if (target)
            *target = *target ? 0u : 1u;
        else
            set_message("No live game_settings object");
        return;
    }
    if (row.kind == ITEM_SAVED_INT) {
        volatile u32 *target = saved_setting_pointer(row.context_index);
        s32 next;
        if (!target) {
            set_message("No live game_settings object");
            return;
        }
        next = (s32)*target + direction;
        if (next < row.minimum)
            next = row.minimum;
        if (next > row.maximum)
            next = row.maximum;
        *target = (u32)next;
        return;
    }
    if (row.kind == ITEM_SAVED_FLOAT) {
        volatile float *target = (volatile float *)saved_setting_pointer(row.context_index);
        float next;
        float step;
        if (!target) {
            set_message("No live game_settings object");
            return;
        }
        step = saved_float_step(row.context_index);
        next = *target + (direction > 0 ? step : -step);
        if (next < (float)row.minimum)
            next = (float)row.minimum;
        if (next > (float)row.maximum)
            next = (float)row.maximum;
        *target = next;
        return;
    }
    if (row.kind == ITEM_NGL_PERF_INFO) {
        value += direction;
        if (value < 0)
            value = 2;
        if (value > 2)
            value = 0;
        *(volatile u8 *)(ADDR_NGL_DEBUG_FLAGS + 4u) = (u8)value;
        *(volatile u8 *)(ADDR_NGL_DEBUG_FLAGS + 19u + 4u) = (u8)value;
        return;
    }
    if (row.kind == ITEM_NGL_FLAG) {
        set_ngl_debug_flag(row.value ? (u32)*row.value : 0u, !value);
        return;
    }
    if (row.kind == ITEM_DEBUG_RENDER) {
        if (row.context_index < DEBUG_RENDER_COUNT)
            ((DebugRenderSetFn)ADDR_DEBUG_RENDER_SET_IVAL)(
                (s32)row.context_index, value + direction);
        return;
    }
    if (row.kind == ITEM_PHYSICS) {
        value = !value;
        if (s_game)
            ((GameEnablePhysicsFn)ADDR_GAME_ENABLE_PHYSICS)(s_game, value);
        return;
    }
    if (row.kind == ITEM_DEVFLAG) {
        set_devflag(row.value ? *row.value : 0, !value);
        return;
    }
    if (row.kind == ITEM_NAMED_DEVFLAG) {
        s32 id = devopt_id_for_label(row.label);
        if (id >= 0)
            set_devflag(id, !value);
        else
            set_message("Devflag is absent from the retail table");
        return;
    }
    if (row.kind == ITEM_DEVINT &&
        !(menu_id == MENU_DEVOPTS &&
          selected >= s_menus[menu_id].item_count)) {
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
        set_message("Payload district overlay changed");
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

static s32 button_chord_pressed(u16 buttons, u16 pressed,
                                u16 first, u16 second)
{
    return (((buttons & first) != 0u && (pressed & second) != 0u) ||
            ((buttons & second) != 0u && (pressed & first) != 0u));
}

static void handle_input(u16 buttons)
{
    u16 pressed = (u16)(buttons & (u16)~s_previous_buttons);
    s_previous_buttons = buttons;

    if (s_hero_swap_pending)
        return;

    if ((buttons & DS4_MENU_TOGGLE) == DS4_MENU_TOGGLE) {
        if (pressed & DS4_MENU_TOGGLE) {
            if (s_open)
                close_menu();
            else
                open_menu();
        }
        return;
    }
    if (button_chord_pressed(buttons, pressed, DS4_L2, DS4_R2) ||
        button_chord_pressed(buttons, pressed, DS4_R1, DS4_OPTIONS)) {
        if (s_open)
            close_menu();
        else
            open_menu();
        return;
    }
    if (!s_open)
        return;

    if (pressed & DS4_CIRCLE) {
        u16 parent = s_menus[s_current_menu].parent;
        if (parent == MENU_NONE)
            close_menu();
        else
            s_current_menu = parent;
        return;
    }
    if (pressed & DS4_CROSS) {
        change_row(1, 1);
        return;
    }
    if (repeated_key(buttons, pressed, DS4_DPAD_UP, 0))
        move_selection(-1);
    if (repeated_key(buttons, pressed, DS4_DPAD_DOWN, 1))
        move_selection(1);
    if (repeated_key(buttons, pressed, DS4_DPAD_LEFT, 2))
        change_row(-1, 0);
    if (repeated_key(buttons, pressed, DS4_DPAD_RIGHT, 3))
        change_row(1, 0);
    if (pressed & DS4_L2)
        page_selection(-1);
    if (pressed & DS4_R2)
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
    if (row->kind == ITEM_SCRIPT_REQUIRES_ENTRY) {
        string_append(out, capacity, ": [needs native entry]");
        return;
    }
    if (row->kind == ITEM_SCRIPT_UNVERIFIED) {
        string_append(out, capacity, ": [unverified bytecode]");
        return;
    }
    if (row->kind == ITEM_WARP_REGION) {
        return;
    }
    if ((row->kind == ITEM_SAVED_BOOL || row->kind == ITEM_SAVED_INT ||
         row->kind == ITEM_SAVED_FLOAT) &&
        !saved_setting_pointer(row->context_index)) {
        string_append(out, capacity, ": [no game settings]");
        return;
    }
    if (row->kind == ITEM_ACTION && row->action == ACTION_SELECT_HERO) {
        if (s_hero_swap_pending && string_equal(row->label, s_hero_swap_name))
            string_append(out, capacity, ": Switching");
        else if (s_active_hero_name && string_equal(row->label, s_active_hero_name))
            string_append(out, capacity, ": Active");
        return;
    }
    if (row->kind == ITEM_AI_PARAM) {
        AiParamData *parameter = resolve_ai_parameter(row);
        string_append(out, capacity, ": ");
        if (!parameter) {
            string_append(out, capacity, "[changed; reopen block]");
            return;
        }
        switch (parameter->type) {
        case 0:
            append_ai_float(out, capacity, parameter->value.floating);
            break;
        case 1:
            append_signed(out, capacity, parameter->value.integer);
            break;
        case 2: {
            const char *value = ((StringHashToStringFn)
                ADDR_STRING_HASH_TO_STRING)(&parameter->value.hash);
            if (safe_mem1_string_length(value, 63u) >= 0)
                string_append(out, capacity, value);
            else
                append_unsigned(out, capacity, parameter->value.hash);
            string_append(out, capacity, " (hash) [read-only]");
            break;
        }
        case 3: {
            const char *value = (const char *)parameter->value.pointer;
            if (safe_mem1_string_length(value, 63u) >= 0)
                string_append(out, capacity, value);
            else
                string_append(out, capacity, "<invalid>");
            string_append(out, capacity, " (fixedstring) [read-only]");
            break;
        }
        case 4: {
            const float *value = (const float *)parameter->value.pointer;
            if (!mem1_word_range(value, 12u)) {
                string_append(out, capacity, "<invalid vector3d>");
                break;
            }
            string_append(out, capacity, "(");
            append_ai_float(out, capacity, value[0]);
            string_append(out, capacity, " ");
            append_ai_float(out, capacity, value[1]);
            string_append(out, capacity, " ");
            append_ai_float(out, capacity, value[2]);
            string_append(out, capacity, ") (vector3d) [read-only]");
            break;
        }
        case 5: {
            const float *value = (const float *)parameter->value.pointer;
            if (!mem1_word_range(value, 8u)) {
                string_append(out, capacity, "<invalid float variance>");
                break;
            }
            string_append(out, capacity, "(b");
            append_ai_float(out, capacity, value[0]);
            string_append(out, capacity, " v");
            append_ai_float(out, capacity, value[1]);
            string_append(out, capacity, ") (float variance) [read-only]");
            break;
        }
        case 6:
            append_hex32(out, capacity, (u32)parameter->value.pointer);
            string_append(out, capacity, " (entity) [read-only]");
            break;
        default:
            append_hex32(out, capacity, (u32)parameter->value.pointer);
            string_append(out, capacity, " (pointer) [read-only]");
            break;
        }
        return;
    }
    if (menu_id == MENU_NGL_DEBUG) {
        string_append(out, capacity, ": ");
        if (row->kind == ITEM_NGL_PERF_INFO)
            append_signed(out, capacity, value);
        else if (row->kind == ITEM_NGL_FLAG)
            string_append(out, capacity, value ? "True" : "False");
        else if (row->kind == ITEM_ACTION &&
                 row->action == ACTION_NGL_SCREENSHOT)
            append_signed(out, capacity,
                          *(volatile u8 *)(ADDR_NGL_DEBUG_FLAGS + 5u));
        else
            string_append(out, capacity, "[unavailable in retail GC]");
        return;
    }
    if (row->kind == ITEM_DEBUG_RENDER) {
        string_append(out, capacity, ": ");
        if (row->minimum == 0 && row->maximum == 1)
            string_append(out, capacity, value ? "True" : "False");
        else
            append_signed(out, capacity, value);
        return;
    }
    if (row->kind == ITEM_SUBMENU || row->kind == ITEM_ENTITY_SUBMENU ||
        row->kind == ITEM_MISSION_DISTRICT || row->kind == ITEM_AI_ENTITY ||
        row->kind == ITEM_AI_PARAM_BLOCK || row->kind == ITEM_ANIMATION_ENTITY) {
        string_append(out, capacity, ": >");
    } else if (row->kind == ITEM_BOOL || row->kind == ITEM_PHYSICS ||
               row->kind == ITEM_DEVFLAG || row->kind == ITEM_SLOW_MOTION ||
               row->kind == ITEM_SHOW_DISTRICTS || row->kind == ITEM_MONKEY ||
               row->kind == ITEM_NAMED_DEVFLAG || row->kind == ITEM_SAVED_BOOL) {
        string_append(out, capacity, ": ");
        string_append(out, capacity, value ? "True" : "False");
    } else if (row->kind == ITEM_INT || row->kind == ITEM_DEVINT ||
               row->kind == ITEM_SAVED_INT || row->kind == ITEM_NGL_PERF_INFO ||
               row->kind == ITEM_DISTRICT_VARIANT ||
               row->kind == ITEM_SLAB_TOTAL || row->kind == ITEM_SLAB_FREE ||
               row->kind == ITEM_SLAB_BUCKET) {
        string_append(out, capacity, ": ");
        append_signed(out, capacity, value);
    } else if (row->kind == ITEM_DEVINT_READONLY) {
        string_append(out, capacity, ": ");
        append_signed(out, capacity, value);
        string_append(out, capacity, " [read-only]");
    } else if (row->kind == ITEM_DVAR_FLOAT || row->kind == ITEM_SAVED_FLOAT) {
        volatile float *dvar = row->kind == ITEM_DVAR_FLOAT
            ? dvar_float_pointer(row)
            : (volatile float *)saved_setting_pointer(row->context_index);
        string_append(out, capacity, ": ");
        if (dvar)
            append_fixed_2(out, capacity, *dvar);
        else
            string_append(out, capacity, row->kind == ITEM_SAVED_FLOAT
                ? "[no game settings]" : "[unavailable]");
    } else if (row->kind == ITEM_CAMERA) {
        string_append(out, capacity, ": ");
        string_append(out, capacity, s_camera_names[(u32)value % ARRAY_COUNT(s_camera_names)]);
    } else if (row->kind == ITEM_ENUM) {
        string_append(out, capacity, ": ");
        append_signed(out, capacity, value);
    }
}

/*
 * os_file::write is a one-instruction no-op in retail GUTE52.  Keep the PC
 * export row useful by sending a lossless raw value plus the typed on-screen
 * rendering to the SDK report channel (visible in Dolphin's OSReport log).
 */
static void export_ai_block_to_report(void)
{
    AiParamBlock *block = selected_ai_block();
    void **parameters;
    u32 count;
    u32 index;
    s32 has_array;
    if (!ai_parameter_vector(block, &parameters, &count, &has_array)) {
        set_message("AI parameter block changed; reopen it");
        return;
    }
    ((OSReportFn)ADDR_OS_REPORT)("// AI parameters for %s / %s\n",
                                s_ai_entity_title, s_ai_block_title);
    if (!has_array) {
        ((OSReportFn)ADDR_OS_REPORT)("// no parameters defined in this block.\n");
    } else {
        prepare_ai_param_order(block, parameters, count);
        for (index = 0; index < count; ++index) {
            char generated[96];
            char line[160];
            u32 display_index;
            Row row;
            for (display_index = 0; display_index < s_ai_param_order_count;
                 ++display_index) {
                if (s_ai_param_order[display_index] == (u16)(index + 1u))
                    break;
            }
            if (display_index >= s_ai_param_order_count)
                continue;
            row = get_row(MENU_AI_BLOCK, display_index, generated,
                              sizeof(generated));
            AiParamData *parameter = resolve_ai_parameter(&row);
            if (!parameter)
                continue;
            format_row(MENU_AI_BLOCK, display_index, &row, line, sizeof(line));
            ((OSReportFn)ADDR_OS_REPORT)("type%d raw=%08x %s\n",
                                        parameter->type,
                                        *(volatile u32 *)parameter, line);
        }
    }
    close_menu();
}

static void render_menu(void)
{
    static const u32 COLOR_TITLE = 0x00DC00FFu;
    static const u32 COLOR_SELECTED = 0xFFFF00FFu;
    static const u32 COLOR_NORMAL = 0xDCDCDCFFu;
    static const u32 COLOR_SCROLL = 0xE000E0FFu;
    u16 menu_id = s_current_menu;
    MenuDef *menu = &s_menus[menu_id];
    u32 count = menu_row_count(menu_id);
    u32 start;
    u32 visible;
    u32 line_height = get_line_height();
    u32 widest;
    u32 width;
    u32 has_message;
    u32 has_top_scroll;
    u32 has_bottom_scroll;
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
    has_top_scroll = start != 0u;
    has_bottom_scroll = start + visible < count;
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
    panel_right = x + (float)widest + 10.0f;
    if (panel_right < 236.0f)
        panel_right = 236.0f;
    if (panel_right > 626.0f)
        panel_right = 626.0f;
    panel_bottom = y + (float)((visible + 1u + has_top_scroll +
                                has_bottom_scroll + has_message) * line_height + 14u);
    if (panel_bottom > 470.0f)
        panel_bottom = 470.0f;

    ((NglQuadFn)ADDR_NGL_INIT_QUAD)(&quad);
    ((NglSetQuadRectFn)ADDR_NGL_SET_QUAD_RECT)(&quad, 17.0f, 24.0f, panel_right, panel_bottom);
    ((NglSetQuadColorFn)ADDR_NGL_SET_QUAD_COLOR)(&quad, 0x141414C8u);
    ((NglSetQuadZFn)ADDR_NGL_SET_QUAD_Z)(&quad, 0.5f);
    ((NglQuadFn)ADDR_NGL_LIST_ADD_QUAD)(&quad);

    add_text(x, y, COLOR_TITLE, menu->title);
    y += (float)line_height;
    if (has_top_scroll) {
        add_text(x + 4.0f, y, COLOR_SCROLL, "^ ^ ^");
        y += (float)line_height;
    }

    for (i = 0; i < visible; ++i) {
        u32 absolute = start + i;
        Row row = get_row(menu_id, absolute, generated, sizeof(generated));
        format_row(menu_id, absolute, &row, line, sizeof(line));
        add_text(x, y, absolute == s_selected[menu_id] ? COLOR_SELECTED : COLOR_NORMAL, line);
        y += (float)line_height;
    }
    if (has_bottom_scroll) {
        add_text(x + 4.0f, y, COLOR_SCROLL, "v v v");
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
    s_actor_list_ready = 0;
    s_ai_list_ready = 0;

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
#if defined(DEBUG_MENU_AUTO_TARGET_MISSIONS)
        refresh_mission_cache();
        s_current_menu = MENU_MISSIONS;
#elif defined(DEBUG_MENU_AUTO_TARGET_WARP)
        s_current_menu = MENU_WARP;
#elif defined(DEBUG_MENU_AUTO_TARGET_GAME)
        s_current_menu = MENU_GAME;
#elif defined(DEBUG_MENU_AUTO_TARGET_DEBUG_RENDER)
        s_current_menu = MENU_DEBUG_RENDER;
#elif defined(DEBUG_MENU_AUTO_TARGET_NGL_DEBUG)
        s_current_menu = MENU_NGL_DEBUG;
#elif defined(DEBUG_MENU_AUTO_TARGET_AI)
        s_ai_list_ready = 0;
        s_current_menu = MENU_AI;
#elif defined(DEBUG_MENU_AUTO_TARGET_ENTITY_ANIMATIONS)
        s_actor_list_ready = 0;
        s_current_menu = MENU_ENTITY_ANIMATIONS;
#elif defined(DEBUG_MENU_AUTO_TARGET_SCRIPT)
        refresh_script_cache();
        s_current_menu = MENU_SCRIPT;
#elif defined(DEBUG_MENU_AUTO_TARGET_PROGRESSION)
        s_current_menu = MENU_PROGRESSION;
#elif defined(DEBUG_MENU_AUTO_TARGET_SAVED_SETTINGS)
        s_current_menu = MENU_SAVED_SETTINGS;
#elif defined(DEBUG_MENU_AUTO_TARGET_DEVOPTS)
        s_current_menu = MENU_DEVOPTS;
#elif defined(DEBUG_MENU_AUTO_TARGET_CHAR_SELECT)
        s_current_menu = MENU_CHAR_SELECT;
#elif defined(DEBUG_MENU_AUTO_TARGET_OPTIONS)
        s_current_menu = MENU_OPTIONS;
#elif defined(DEBUG_MENU_AUTO_TARGET_LEVEL_SELECT)
        s_level_cache_ready = 0;
        s_current_menu = MENU_LEVEL_SELECT;
#elif defined(DEBUG_MENU_AUTO_TARGET_MEMORY)
        s_current_menu = MENU_MEMORY;
#elif defined(DEBUG_MENU_AUTO_TARGET_SAVE_LOAD)
        s_current_menu = MENU_SAVE_LOAD;
#elif defined(DEBUG_MENU_AUTO_TARGET_SCREENSHOT)
        s_current_menu = MENU_SCREENSHOT;
#elif defined(DEBUG_MENU_AUTO_TARGET_DISTRICT_VARIANTS)
        s_current_menu = MENU_DISTRICT_VARIANTS;
#endif
    }
#endif
    update_hero_swap_status();

    ((PadReadFn)ADDR_PAD_READ)(pads);
    if (pads[0].error == 0)
        buttons = pads[0].button;
    handle_input(buttons);
    if (get_devflag(6))
        render_district_overlay();
    if (s_open)
        render_menu();
}
