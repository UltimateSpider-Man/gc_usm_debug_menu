#pragma once

namespace usm::gc {

using u8 = unsigned char;
using s8 = signed char;
using u16 = unsigned short;
using s16 = signed short;
using u32 = unsigned int;
using s32 = signed int;
using uptr = unsigned int;

static_assert(sizeof(u8) == 1);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(void*) == 4, "The payload must be built for 32-bit PowerPC");

struct Vec3 {
    float x;
    float y;
    float z;
};

// Nintendo SDK PADStatus ABI. One PADRead call writes four of these records.
struct PadStatus {
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
};

static_assert(sizeof(PadStatus) == 12);

template <typename T>
inline T& memory(uptr address) {
    return *reinterpret_cast<T*>(address);
}

template <typename T>
inline volatile T& volatile_memory(uptr address) {
    return *reinterpret_cast<volatile T*>(address);
}

inline bool is_main_ram_pointer(const void* pointer) {
    const uptr value = reinterpret_cast<uptr>(pointer);
    return value >= 0x80000000u && value < 0x81800000u && (value & 3u) == 0u;
}

inline u32 align_up(u32 value, u32 alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

}  // namespace usm::gc
