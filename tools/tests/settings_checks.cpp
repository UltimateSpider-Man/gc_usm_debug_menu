// Runs the actual Saved Settings handlers against a bounded fake game object.
// Build freestanding with -m32 -ffunction-sections -fdata-sections and
// -Wl,--gc-sections; unused registry code is discarded at link time.
#include "../../gamecube/menu_registry.cpp"

using namespace usm::gc;
namespace {
alignas(4) u8 settings_data[0x600];
u32 failures;
u32 checks;
u32 rumble_calls;
bool rumble_state;
s32 debug_value;

int system_call(u32 number, u32 arg1, u32 arg2 = 0u, u32 arg3 = 0u) {
    int result;
    asm volatile("int $0x80" : "=a"(result)
                 : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return result;
}
void print(const char* text) {
    u32 length = 0u;
    while (text[length] != '\0') { ++length; }
    system_call(4u, 1u, reinterpret_cast<uptr>(text), length);
}
void check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; print("FAIL: "); print(label); print("\n"); }
}
void reset_data() {
    for (u32 i = 0u; i < sizeof(settings_data); ++i) { settings_data[i] = 0xA5u; }
    rumble_calls = 0u;
    rumble_state = false;
}
u32& word(u32 offset) { return *reinterpret_cast<u32*>(settings_data + offset); }
}

namespace usm::gc::engine {
void* settings() { return settings_data; }
bool gameplay_rel_ready() { return true; }
void set_rumble(bool enabled) { ++rumble_calls; rumble_state = enabled; }
s32 debug_render_get(u32) { return debug_value; }
s32 debug_render_min(u32) { return 0; }
s32 debug_render_max(u32) { return 1; }
void debug_render_set(u32, s32 value) { debug_value = value; }
}
namespace usm::gc::menu {
s32 change_multiplier() { return 10; }
void set_status(const char*, u16) {}
}

extern "C" int run_tests() {
    using namespace usm::gc::registry;
    menu::Entry entry{};
    reset_data();
    word(0x38Cu) = 0u;
    entry.id = 13u;
    game_value_handler(entry, menu::Command::activate);
    check(word(0x38Cu) == 1u, "GC rumble boolean writes a complete four-byte word");
    check(word(0x388u) == 0xA5A5A5A5u, "rumble preserves the preceding scalar");
    check(word(0x390u) == 0xA5A5A5A5u, "rumble preserves the following scalar");
    check(rumble_calls == 1u && rumble_state, "Saved Rumble applies native enable gate");
    game_value_handler(entry, menu::Command::activate);
    check(word(0x38Cu) == 0u && rumble_calls == 2u && !rumble_state,
          "Saved Rumble applies native disable gate");

    reset_data();
    word(0x400u) = 0u;
    entry.id = 42u;
    game_value_handler(entry, menu::Command::activate);
    check(word(0x400u) == 1u, "Invert Camera uses GC scalar 42 at settings+0x400");
    check(word(0x3FCu) == 0xA5A5A5A5u, "Invert Camera preserves preceding scalar");
    check(word(0x404u) == 0xA5A5A5A5u, "Invert Camera preserves following scalar");

    reset_data();
    entry.id = 96u;
    word(0x4D8u) = 0x7FFFFFFFu;
    game_value_handler(entry, menu::Command::increment);
    check(word(0x4D8u) == 1000u, "last saved integer saturates INT_MAX safely");
    word(0x4D8u) = 0x80000000u;
    game_value_handler(entry, menu::Command::decrement);
    check(word(0x4D8u) == 0u, "last saved integer saturates INT_MIN safely");
    check(word(0x4DCu) == 0xA5A5A5A5u, "last saved scalar preserves adjacent object data");
    check(bounded_adjustment(0x7FFFFFFF, 10, -1000, 1000) == 1000,
          "developer INT_MAX addition is bounded before arithmetic");
    check(bounded_adjustment(static_cast<s32>(0x80000000u), -10, -1000, 1000) == -1000,
          "developer INT_MIN subtraction is bounded before arithmetic");
    check(bounded_adjustment(995, 10, 0, 1000) == 1000, "upper adjustment saturates");
    check(bounded_adjustment(5, -10, 0, 1000) == 0, "lower adjustment saturates");

    entry.id = 15u;
    auto* floating = reinterpret_cast<float*>(settings_data + 0x394u);
    word(0x394u) = 0x7FC00000u;
    game_value_handler(entry, menu::Command::increment);
    check(*floating == 1.0f, "NaN saved float recovers before arithmetic");
    word(0x394u) = 0x7F800000u;
    game_value_handler(entry, menu::Command::decrement);
    check(*floating == 999.0f, "positive infinity saved float is bounded");
    word(0x394u) = 0xFF800000u;
    game_value_handler(entry, menu::Command::increment);
    check(*floating == 1.0f, "negative infinity saved float is bounded");
    *floating = 1000.0f;
    game_value_handler(entry, menu::Command::increment);
    check(*floating == 1000.0f, "saved float upper adjustment saturates");

    entry.id = 0u;
    debug_value = 0x7FFFFFFF;
    debug_render_handler(entry, menu::Command::increment);
    check(debug_value == 1, "Debug Render saturates native INT_MAX before addition");
    debug_value = static_cast<s32>(0x80000000u);
    debug_render_handler(entry, menu::Command::decrement);
    check(debug_value == 0, "Debug Render saturates native INT_MIN before subtraction");

    print(failures == 0u ? "PASS: Saved Settings ABI, native rumble, and overflow checks\n"
                         : "FAILED: Saved Settings checks\n");
    return failures == 0u && checks == 21u ? 0 : 1;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
