/*
 * Native Windows/DInput DualShock 4 layout retained as bridge metadata.
 *
 * This table describes host indices used by the PC reference layout.  The
 * GameCube payload cannot read it at runtime: Dolphin independently translates
 * SDL controls into a GameCube PadStatus packet using GCPadNew_DS4.ini.
 */

typedef unsigned char u8;
typedef unsigned int u32;

#define ARRAY_COUNT(a) ((u32)(sizeof(a) / sizeof((a)[0])))

enum PadButton {
    PAD_UP = 0,
    PAD_DOWN,
    PAD_LEFT,
    PAD_RIGHT,
    PAD_START,
    PAD_SELECT,
    PAD_TOUCH_BUTTON,
    PAD_L3,
    PAD_R3,
    PAD_L1,
    PAD_R1,
    PAD_CROSS,
    PAD_CIRCLE,
    PAD_SQUARE,
    PAD_TRIANGLE,
    PAD_STICK_LEFT_X,
    PAD_STICK_LEFT_Y,
    PAD_STICK_RIGHT_X,
    PAD_STICK_RIGHT_Y,
    PAD_L2,
    PAD_R2,
    PAD_PS = 21,
    KEY_INVALID = 0xFF
};

enum GamePadMappingKind {
    MAPPING_INVALID = 0,
    BUTTON,
    AXIS,
    AXIS_10_00_00,
    AXIS_00_00_10,
    AXIS_00_05_10
};

typedef struct GamePadMappingEntry {
    u8 pad_button;
    u8 mapping_kind;
    u8 native_index;
} GamePadMappingEntry;

static const GamePadMappingEntry g_DInputDS4Mapping[]
    __attribute__((used, section(".ds4_mapping"), aligned(4))) = {
        { PAD_UP, AXIS_10_00_00, 5u },
        { PAD_DOWN, AXIS_00_00_10, 5u },
        { PAD_LEFT, AXIS_10_00_00, 4u },
        { PAD_RIGHT, AXIS_00_00_10, 4u },
        { PAD_START, BUTTON, 9u },
        { PAD_SELECT, BUTTON, 8u },
        { PAD_TOUCH_BUTTON, BUTTON, 13u },
        { PAD_L3, BUTTON, 10u },
        { PAD_R3, BUTTON, 11u },
        { PAD_L1, BUTTON, 4u },
        { PAD_R1, BUTTON, 5u },
        { PAD_CROSS, BUTTON, 1u },
        { PAD_CIRCLE, BUTTON, 2u },
        { PAD_SQUARE, BUTTON, 0u },
        { PAD_TRIANGLE, BUTTON, 3u },
        { PAD_STICK_LEFT_X, AXIS, 3u },
        { PAD_STICK_LEFT_Y, AXIS, 2u },
        { PAD_STICK_RIGHT_X, AXIS, 1u },
        { PAD_STICK_RIGHT_Y, AXIS, 0u },
        { PAD_L2, AXIS_00_05_10, 7u },
        { PAD_R2, AXIS_00_05_10, 6u },
        { PAD_PS, BUTTON, 12u },
        { KEY_INVALID, MAPPING_INVALID, 0u }
    };

_Static_assert(ARRAY_COUNT(g_DInputDS4Mapping) == 23u,
               "DS4 DInput mapping entry count changed");
_Static_assert(sizeof(GamePadMappingEntry) == 3u,
               "DS4 DInput mapping layout changed");
_Static_assert(PAD_PS == 21u, "DS4 PS metadata id changed");
