// Exercise the actual engine bridge with GC-sized pointers and mapped MEM1.
// Only native calls are replaced; readiness checks and object writes are real.
#include "../../gamecube/gc_engine.cpp"

using namespace usm::gc;
namespace {
constexpr uptr game_object = 0x81000000u;
constexpr uptr settings_object = 0x81001000u;
constexpr uptr world_object = 0x81002000u;
constexpr uptr pause_manager = 0x81003000u;
constexpr uptr comicpanel_object = 0x81004000u;
constexpr u32 game_size = 0x340u;
u8 before[game_size];
u32 checks, failures, state_calls, physics_calls, deactivate_calls;
s32 native_state = 6;
uptr last_state_owner, last_physics_owner, last_deactivate_owner;
s32 incoming_pause_index;

int system_call(u32 number, u32 first, u32 second = 0u, u32 third = 0u) {
    int result;
    asm volatile("int $0x80" : "=a"(result)
                 : "a"(number), "b"(first), "c"(second), "d"(third) : "memory");
    return result;
}
void print(const char* text) {
    u32 size = 0u;
    while (text[size] != '\0') ++size;
    system_call(4u, 1u, reinterpret_cast<uptr>(text), size);
}
void print_count(u32 value) {
    char text[11];
    u32 index = sizeof(text) - 1u;
    text[index] = '\0';
    do {
        text[--index] = static_cast<char>('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    print(text + index);
}
void check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; print("FAIL: "); print(label); print("\n"); }
}
u32& word(uptr address) { return *reinterpret_cast<u32*>(address); }
s32 get_state(void* owner) {
    ++state_calls;
    last_state_owner = reinterpret_cast<uptr>(owner);
    return native_state;
}
void enable_physics(void* owner, bool enabled) {
    ++physics_calls;
    last_physics_owner = reinterpret_cast<uptr>(owner);
    word(last_physics_owner + 0x1A8u) = enabled ? 0u : 1u;
}
void deactivate_pause(void* owner) {
    ++deactivate_calls;
    last_deactivate_owner = reinterpret_cast<uptr>(owner);
    incoming_pause_index = static_cast<s32>(word(last_deactivate_owner + 8u));
    // The native boundary owns these effects; the engine bridge must not
    // replace them with a raw pause index or game paused-word write.
    word(last_deactivate_owner + 8u) = 0xFFFFFFFFu;
    word(game_object + 0x1B0u) = 0u;
    word(word(address::comicpanel_system_pointer) + 0x74u) =
        word(last_deactivate_owner + 0x3Cu);
}
void boundary(uptr address, uptr function) {
    *reinterpret_cast<u8*>(address) = 0xE9u;
    word(address + 1u) = function - address - 5u;
}
void snapshot() {
    for (u32 index = 0u; index != game_size; ++index)
        before[index] = *reinterpret_cast<u8*>(game_object + index);
}
bool unchanged_except(u32 first = game_size, u32 size = 0u) {
    for (u32 index = 0u; index != game_size; ++index)
        if ((index < first || index >= first + size) &&
            before[index] != *reinterpret_cast<u8*>(game_object + index)) return false;
    return true;
}
void reset_game() {
    for (u32 index = 0u; index != game_size; ++index)
        *reinterpret_cast<u8*>(game_object + index) = 0xA5u;
    word(address::game_pointer) = game_object;
    word(game_object + 0xD8u) = settings_object;
    word(game_object + 0x1A4u) = 0u;
    word(game_object + 0x1A8u) = 1u;
    native_state = 6;
    state_calls = 0u;
    physics_calls = 0u;
    snapshot();
}
void reset_pause() {
    reset_game();
    word(address::world_pointer) = world_object;
    word(game_object + 0x68u) = world_object;
    word(game_object + 0x5Cu) = 1u;
    word(game_object + 0x1A0u) = 1u;
    word(game_object + 0x1B0u) = 1u;
    word(address::pause_menu_system_pointer) = pause_manager;
    word(address::comicpanel_system_pointer) = comicpanel_object;
    word(pause_manager + 8u) = 0u;
    word(pause_manager + 0x3Cu) = 0x12345678u;
    word(comicpanel_object + 0x74u) = 0xABCDEF01u;
    native_state = 7;
    deactivate_calls = 0u;
    incoming_pause_index = -1;
    snapshot();
}
bool pause_unchanged() {
    return deactivate_calls == 0u && unchanged_except() &&
           word(pause_manager + 8u) == 0u &&
           word(comicpanel_object + 0x74u) == 0xABCDEF01u;
}
}

extern "C" int run_tests() {
    u32 mmap_arguments[] = {0x80000000u, 0x1800000u, 7u, 0x32u, 0xFFFFFFFFu, 0u};
    if (static_cast<uptr>(system_call(90u, reinterpret_cast<uptr>(mmap_arguments))) != 0x80000000u) {
        print("Cannot map MEM1 engine fixture\n"); return 2;
    }
    boundary(address::game_get_state, reinterpret_cast<uptr>(get_state));
    boundary(address::game_enable_physics, reinterpret_cast<uptr>(enable_physics));
    u32 rel_mapping[] = {address::pause_menu_deactivate & ~0xFFFu,
                        0x1000u, 7u, 0x32u, 0xFFFFFFFFu, 0u};
    if (static_cast<uptr>(system_call(90u, reinterpret_cast<uptr>(rel_mapping))) != rel_mapping[0]) {
        print("Cannot map native pause REL fixture\n"); return 2;
    }
    boundary(address::pause_menu_deactivate, reinterpret_cast<uptr>(deactivate_pause));

    reset_game();
    engine::request_single_step();
    check(word(game_object + 0x1A4u) == 1u, "Single Step publishes the native complete word at game+1A4");
    check(unchanged_except(0x1A4u, 4u), "Single Step changes only its native four-byte flag");
    check(*reinterpret_cast<u8*>(game_object + 0x175u) == 0xA5u,
          "Single Step preserves the unrelated level transition byte175");
    check(word(game_object + 0x1A0u) == 0xA5A5A5A5u && word(game_object + 0x1A8u) == 1u,
          "Single Step preserves adjacent gameplay and physics-disabled words");
    check(state_calls == 1u && last_state_owner == game_object,
          "Single Step validates readiness against the actual game instance");
    snapshot(); engine::request_single_step();
    check(unchanged_except(), "repeated Single Step requests remain a single pending frame");

    reset_game(); native_state = 7;
    word(game_object + 0x1A4u) = 0xA5A5A5A5u;
    engine::request_single_step();
    check(word(game_object + 0x1A4u) == 1u, "PAUSED state replaces every byte of a previous flag value");
    reset_game(); word(game_object + 0x1A8u) = 0u; engine::request_single_step();
    check(word(game_object + 0x1A4u) == 1u && word(game_object + 0x1A8u) == 0u,
          "Single Step leaves enabled physics unchanged");

    bool transitions_unchanged = true;
    for (s32 state = 0; state != 15; ++state) {
        if (state == 6 || state == 7) continue;
        reset_game(); native_state = state; engine::request_single_step();
        transitions_unchanged = transitions_unchanged && unchanged_except();
    }
    check(transitions_unchanged, "every transitional game state blocks Single Step writes");
    reset_game(); word(address::game_pointer) = 0u; engine::request_single_step();
    check(unchanged_except() && state_calls == 0u, "missing game blocks Single Step without a native call");
    reset_game(); word(address::game_pointer) = game_object + 1u; engine::request_single_step();
    check(unchanged_except() && state_calls == 0u, "unaligned game pointer blocks Single Step");
    reset_game(); word(game_object + 0xD8u) = 0u; snapshot(); engine::request_single_step();
    check(unchanged_except() && state_calls == 0u, "missing settings block Single Step before native state lookup");
    reset_game(); word(game_object + 0xD8u) = 0x81900000u; snapshot(); engine::request_single_step();
    check(unchanged_except() && state_calls == 0u, "out-of-MEM1 settings block Single Step");

    reset_game(); word(game_object + 0x1A8u) = 0u; snapshot(); engine::set_physics(false);
    check(physics_calls == 1u && last_physics_owner == game_object && word(game_object + 0x1A8u) == 1u,
          "Disable Physics calls the native boolean setter for game+1A8");
    check(unchanged_except(0x1A8u, 4u), "Disable Physics preserves the single-step flag and other fields");
    engine::set_physics(true);
    check(physics_calls == 2u && word(game_object + 0x1A8u) == 0u,
          "Enable Physics clears the native disabled flag");
    native_state = 8; snapshot(); engine::set_physics(false);
    check(physics_calls == 2u && unchanged_except(), "transition state blocks native physics changes");

    reset_pause();
    check(engine::dismiss_debug_pause_menu(), "debug-owned paused gameplay dismisses an active native pause menu");
    check(deactivate_calls == 1u && last_deactivate_owner == pause_manager,
          "pause dismissal calls the verified REL Deactivate entry with the manager instance");
    check(incoming_pause_index == 0,
          "pause dismissal preserves the active index until native Deactivate receives it");
    check(word(pause_manager + 8u) == 0xFFFFFFFFu && word(game_object + 0x1B0u) == 0u &&
              word(comicpanel_object + 0x74u) == 0x12345678u,
          "native dismissal retains menu, unpause and comicpanel restoration side effects");
    check(unchanged_except(0x1B0u, 4u), "pause dismissal changes no unrelated game fields");
    snapshot();
    check(!engine::dismiss_debug_pause_menu() && deactivate_calls == 1u && unchanged_except(),
          "an already unpaused game never receives a second native dismissal");

    reset_pause(); word(pause_manager + 8u) = 0xFFFFFFFFu;
    check(!engine::dismiss_debug_pause_menu() && deactivate_calls == 0u && unchanged_except() &&
              word(game_object + 0x1B0u) == 1u,
          "inactive native pause menu does not dismiss or unpause through this API");
    reset_pause(); word(pause_manager + 8u) = 5u;
    check(engine::dismiss_debug_pause_menu() && incoming_pause_index == 5,
          "other active native pause pages use the same Deactivate lifecycle");
    bool pause_transitions_unchanged = true;
    for (s32 state = 0; state != 15; ++state) {
        if (state == 7) continue;
        reset_pause(); native_state = state;
        pause_transitions_unchanged = pause_transitions_unchanged &&
                                     !engine::dismiss_debug_pause_menu() && pause_unchanged();
    }
    check(pause_transitions_unchanged, "only native PAUSED state permits pause UI dismissal");
    reset_pause(); word(address::game_pointer) = 0u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged() && state_calls == 0u,
          "missing game blocks dismissal before native state lookup");
    reset_pause(); word(address::game_pointer) = game_object + 1u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged() && state_calls == 0u,
          "unaligned game blocks pause dismissal");
    reset_pause(); word(address::game_pointer) = 0x817FFD00u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged() && state_calls == 0u,
          "truncated game cannot reach native unpause fields through +314");
    reset_pause(); word(address::world_pointer) = 0u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "missing world blocks pause dismissal");
    reset_pause(); word(address::world_pointer) = world_object + 1u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "unaligned world blocks pause dismissal");
    reset_pause(); word(address::world_pointer) = 0x817FFF00u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "truncated world blocks pause dismissal");
    reset_pause(); word(game_object + 0x68u) = world_object + 4u; snapshot();
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(),
          "stale world not owned by the current game cannot dismiss a pause menu");
    reset_pause(); word(game_object + 0x1A0u) = 0u; snapshot();
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "unloaded level blocks pause dismissal");
    reset_pause(); word(game_object + 0x5Cu) = 0u; snapshot();
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "unready loader blocks pause dismissal");
    reset_pause(); word(game_object + 0x1B0u) = 0u; snapshot();
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "external running state is never dismissed");
    reset_pause(); word(game_object + 0xD8u) = 0u; snapshot();
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged() && state_calls == 0u,
          "missing settings block native REL dismissal before state lookup");
    reset_pause(); word(game_object + 0xD8u) = settings_object + 1u; snapshot();
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "unaligned settings block pause dismissal");
    reset_pause(); word(game_object + 0xD8u) = 0x81900000u; snapshot();
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "settings outside MEM1 block pause dismissal");
    reset_pause(); word(address::pause_menu_system_pointer) = 0u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "missing native manager blocks pause dismissal");
    reset_pause(); word(address::pause_menu_system_pointer) = pause_manager + 1u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "unaligned native manager blocks pause dismissal");
    reset_pause(); word(address::pause_menu_system_pointer) = 0x817FFFC4u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(),
          "native manager must include its saved comicpanel state through +3C");
    reset_pause(); word(address::comicpanel_system_pointer) = 0u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "missing comicpanel owner blocks native dismissal");
    reset_pause(); word(address::comicpanel_system_pointer) = comicpanel_object + 1u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(), "unaligned comicpanel owner blocks native dismissal");
    reset_pause(); word(address::comicpanel_system_pointer) = 0x817FFF8Cu;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(),
          "comicpanel owner must include native state field +74");
    reset_pause(); word(address::pause_menu_system_pointer) = 0x817FFFC0u;
    word(0x817FFFC8u) = 0u; word(0x817FFFFCu) = 0x5A5A5A5Au;
    check(engine::dismiss_debug_pause_menu() && last_deactivate_owner == 0x817FFFC0u &&
              word(comicpanel_object + 0x74u) == 0x5A5A5A5Au,
          "complete native manager exactly at MEM1 end remains valid");

    constexpr uptr native_paged_comicpanel = 0xF035A4C8u;
    u32 native_ui_mapping[] = {0xF035A000u, 0x1000u, 7u, 0x32u, 0xFFFFFFFFu, 0u};
    if (static_cast<uptr>(system_call(90u, reinterpret_cast<uptr>(native_ui_mapping))) != native_ui_mapping[0]) {
        print("Cannot map native paged comicpanel fixture\n"); return 2;
    }
    reset_pause();
    word(address::comicpanel_system_pointer) = native_paged_comicpanel;
    word(native_paged_comicpanel + 0x74u) = 0xCAFEBABEu;
    check(engine::dismiss_debug_pause_menu() && deactivate_calls == 1u &&
              word(native_paged_comicpanel + 0x74u) == 0x12345678u &&
              word(game_object + 0x1B0u) == 0u,
          "actual native F035A4C8 comicpanel address receives Deactivate restoration and unpause");
    reset_pause();
    word(address::comicpanel_system_pointer) = native_paged_comicpanel;
    word(address::resource_manager_pointer) = 0u;
    check(engine::dismiss_debug_pause_menu() && deactivate_calls == 1u,
          "engine-owned mutable paged UI does not require a READY read-only resource slot");
    reset_pause(); word(address::comicpanel_system_pointer) = native_paged_comicpanel + 1u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(),
          "unaligned native paged comicpanel pointer cannot enter Deactivate");
    reset_pause(); word(address::comicpanel_system_pointer) = 0xE035A4C8u;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(),
          "non-pager virtual segment cannot replace the native comicpanel owner");
    reset_pause(); word(address::comicpanel_system_pointer) = 0xFFFFFF8Cu;
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(),
          "native paged comicpanel's complete state word cannot wrap the address space");
    reset_pause(); word(address::comicpanel_system_pointer) = native_paged_comicpanel;
    word(game_object + 0x1A0u) = 0u; snapshot();
    check(!engine::dismiss_debug_pause_menu() && pause_unchanged(),
          "valid native paged UI still requires the current level to be loaded");

    u32 paged_mapping[] = {0xF0200000u, 0x20000u, 7u, 0x32u, 0xFFFFFFFFu, 0u};
    if (static_cast<uptr>(system_call(90u, reinterpret_cast<uptr>(paged_mapping))) != paged_mapping[0]) {
        print("Cannot map paged engine fixture\n"); return 2;
    }
    constexpr uptr manager = 0x81100000u, partitions = 0x81100100u, partition = 0x81100200u;
    constexpr uptr slots = 0x81100300u, slot = 0x81100400u, directory = 0x81100500u;
    constexpr uptr region = 0x81100600u, paged_name = 0xF0200101u;
    word(0x8045F214u) = manager; word(manager) = partitions; word(partitions) = partition;
    word(partition + 8u) = slots; word(partition + 12u) = slots + 4u; word(slots) = slot;
    word(slot + 8u) = 4u; word(slot + 0x18u) = directory; word(directory + 0x78u) = slot;
    word(directory + 0x80u) = paged_mapping[0]; word(directory + 0x88u) = 0x10000u;
    word(region + 0x2Cu) = paged_name;
    const char label[] = "Queens";
    for (u32 i = 0u; i < sizeof(label); ++i) *reinterpret_cast<char*>(paged_name + i) = label[i];
    check(engine::region_name(reinterpret_cast<void*>(region)) == reinterpret_cast<const char*>(paged_name),
          "Warp and District Variant accept owned paged region names, including unaligned strings");
    word(slot + 8u) = 3u;
    check(engine::region_name(reinterpret_cast<void*>(region)) == nullptr,
          "region name becomes unavailable when its cached pack owner unloads");
    word(slot + 8u) = 4u; word(region + 0x2Cu) = 0xF0210000u;
    check(engine::region_name(reinterpret_cast<void*>(region)) == nullptr,
          "mapped but unowned paged region name is rejected");
    check(engine::region_name(reinterpret_cast<void*>(paged_mapping[0])) == nullptr,
          "readable paged data never authorizes a mutable region object");
    word(region + 0x2Cu) = 0xF020FFFFu; *reinterpret_cast<char*>(0xF020FFFFu) = 'Q';
    check(engine::region_name(reinterpret_cast<void*>(region)) == nullptr,
          "region name cannot cross its owned resource extent");
    check(engine::region_name(reinterpret_cast<void*>(0x817FFFF0u)) == nullptr,
          "region mash field cannot be read beyond MEM1");

    if (failures == 0u) {
        print("PASS: "); print_count(checks); print(" engine bridge checks\n");
    } else {
        print("FAILED engine bridge checks\n");
    }
    return failures == 0u ? 0 : 1;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
