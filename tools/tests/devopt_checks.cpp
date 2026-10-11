// Real menu entries, GC-width mapped objects and the production engine bridge.
// Replace only native PPC call boundaries; handlers and 32-bit writes are real.
#include "../../gamecube/gc_engine.cpp"
#include "../../gamecube/menus/devopt_features_gc.cpp"

using namespace usm::gc;
namespace {
constexpr uptr game_object = 0x81000000u, world_object = 0x81001000u;
constexpr uptr options_object = 0x81002000u, other_options = 0x81003000u;
constexpr uptr hero_object = 0x81004000u, pose_object = 0x81005000u;
constexpr uptr terrain_object = 0x81006000u, region_object = 0x81007000u;
constexpr uptr region_vector = 0x81008000u, input_manager = 0x81009000u;
constexpr uptr mouse_controller = 0x8100A000u, controller2 = 0x8100B000u;
u32 checks, failures, physics_calls, god_calls, camera_calls, pose_calls, region_calls;
s32 state = 6;
bool second_connected;
u32 tick;
char draws[8][192];
u32 draw_count;

int syscall(u32 number, u32 arg1, u32 arg2 = 0u, u32 arg3 = 0u) {
    int result;
    asm volatile("int $0x80" : "=a"(result)
                 : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return result;
}
void print(const char* text) {
    u32 length = 0u;
    while (text[length] != '\0') ++length;
    syscall(4u, 1u, reinterpret_cast<uptr>(text), length);
}
void check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; print("FAIL: "); print(label); print("\n"); }
}
bool equal(const char* first, const char* second) {
    for (u32 i = 0u;; ++i) {
        if (first[i] != second[i]) return false;
        if (first[i] == '\0') return true;
    }
}
bool starts(const char* first, const char* prefix) {
    for (u32 i = 0u; prefix[i] != '\0'; ++i) if (first[i] != prefix[i]) return false;
    return true;
}
u32& word(uptr location) { return *reinterpret_cast<u32*>(location); }
void boundary(uptr location, uptr target) {
    *reinterpret_cast<u8*>(location) = 0xE9u;
    word(location + 1u) = target - location - 5u;
}
s32 native_state(void*) { return state; }
void native_physics(void* owner, bool enabled) {
    ++physics_calls;
    word(reinterpret_cast<uptr>(owner) + 0x1A8u) = enabled ? 0u : 1u;
}
void native_god(s32 mode) {
    ++god_calls;
    word(address::god_mode_cheat) = mode >= 1 && mode <= 3 ? 1u : 0u;
    word(address::ultra_god_mode_cheat) = mode == 2 || mode == 4 ? 1u : 0u;
    word(address::mega_god_mode_cheat) = mode == 3 || mode == 5 ? 1u : 0u;
}
void native_camera(void*, s32 mode) { ++camera_calls; word(options_object + 0x348u) = mode; }
bool native_analyzer() { return false; }
void native_enable_analyzer(bool) {}
void* native_pose(void* actor) {
    ++pose_calls;
    return actor == reinterpret_cast<void*>(hero_object) ? reinterpret_cast<void*>(pose_object) : nullptr;
}
void* native_region(void* actor) {
    ++region_calls;
    return actor == reinterpret_cast<void*>(hero_object) ? reinterpret_cast<void*>(region_object) : nullptr;
}
u32 native_tick() { tick += 675000u; return tick; }
void native_text(void*, const char* text, float, float, float, u32, float, float) {
    if (draw_count < 8u) menu::copy_text(draws[draw_count++], sizeof(draws[0]), text);
}
void put_name(uptr slot, u32 index, uptr base, const char* name) {
    const uptr location = base + index * 80u;
    menu::copy_text(reinterpret_cast<char*>(location), 80u, name);
    word(slot + index * 4u) = location;
}
void prepare_names() {
    for (u32 i = 0u; i != 151u; ++i)
        put_name(address::developer_flag_names, i, 0x81200000u, "TEST_FLAG");
    for (u32 i = 0u; i != 78u; ++i)
        put_name(address::developer_int_names, i, 0x81210000u, "TEST_INT");
    put_name(address::developer_flag_names, 5u, 0x81200000u, "SHOW_FPS");
    put_name(address::developer_flag_names, 14u, 0x81200000u, "ALLOC_ARAM");
    put_name(address::developer_flag_names, 15u, 0x81200000u, "RENDER_LOWLODS");
    put_name(address::developer_flag_names, 21u, 0x81200000u, "SHOW_DEBUG_INFO");
    put_name(address::developer_flag_names, 88u, 0x81200000u, "SHOW_DEBUG_TEXT");
    put_name(address::developer_flag_names, 91u, 0x81200000u, "USERCAM_ON_CONTROLLER2");
    put_name(address::developer_flag_names, 150u, 0x81200000u, "UNLOCK_ALL_UNLOCKABLES");
    put_name(address::developer_int_names, 2u, 0x81210000u, "CAMERA_STATE");
    put_name(address::developer_int_names, 13u, 0x81210000u, "FRAME_LOCK");
    put_name(address::developer_int_names, 29u, 0x81210000u, "GOD_MODE");
    put_name(address::developer_int_names, 30u, 0x81210000u, "LISTBUFFER");
    put_name(address::developer_int_names, 77u, 0x81210000u, "MAX_AEPS_PARTICLES");
}
void reset_fixture() {
    for (uptr p = game_object; p != controller2 + 0x100u; p += 4u) word(p) = 0u;
    for (u32 i = 0u; i != 0x478u; i += 4u) word(options_object + i) = 0xA5A5A5A5u;
    word(address::game_pointer) = game_object;
    word(address::world_pointer) = world_object;
    word(address::developer_options_pointer) = options_object;
    word(game_object + 0xD8u) = 0x8100C000u;
    word(game_object + 0x68u) = world_object;
    word(game_object + 0x1A0u) = 1u;
    word(game_object + 0x5Cu) = 1u;
    word(world_object + 0x21Cu) = hero_object;
    word(world_object + 0x198u) = terrain_object;
    word(terrain_object) = region_vector;
    word(terrain_object + 12u) = 1u;
    word(region_vector) = region_object;
    word(region_object + 0xC0u) = 7u;
    word(region_object + 0xCCu) = 99u;
    word(address::input_manager_pointer) = input_manager;
    word(address::mouselook_controller_pointer) = mouse_controller;
    word(input_manager + 0x3Cu) = controller2;
    word(controller2) = 0xF4241u;
    word(game_object + 0x78u) = 0x8100D000u;
    word(game_object + 0x80u) = 0x8100E000u;
    word(game_object + 0x74u) = 0x8100D000u;
    word(world_object + 0x58u) = 0x8100F000u;
    for (u32 p = 0x24u; p <= 0x40u; p += 4u)
        word(world_object + 0x28u + p) = 0x81010000u + p * 16u;
    word(options_object + 0x348u) = 0u;
    word(options_object + 0x374u) = 30u;
    word(options_object + 0x3B4u) = 0u;
    native_god(0);
    physics_calls = 0u; god_calls = 0u; camera_calls = 0u;
    pose_calls = 0u; region_calls = 0u; draw_count = 0u;
    state = 6; second_connected = false;
}
void display(menu::Entry& entry, char* output) { entry.formatter(entry, output, 192u); }
}

namespace usm::gc::input {
bool connected(u32 channel) { return channel == 1u && second_connected; }
}

extern "C" int run_tests() {
    u32 mapping[] = {0x80000000u, 0x1800000u, 7u, 0x32u, ~0u, 0u};
    if (static_cast<uptr>(syscall(90u, reinterpret_cast<uptr>(mapping))) != mapping[0]) return 2;
    boundary(address::game_get_state, reinterpret_cast<uptr>(native_state));
    boundary(address::game_enable_physics, reinterpret_cast<uptr>(native_physics));
    boundary(address::set_god_mode, reinterpret_cast<uptr>(native_god));
    boundary(address::game_set_camera, reinterpret_cast<uptr>(native_camera));
    boundary(address::scene_analyzer_enabled, reinterpret_cast<uptr>(native_analyzer));
    boundary(address::enable_scene_analyzer, reinterpret_cast<uptr>(native_enable_analyzer));
    boundary(0x801354C4u, reinterpret_cast<uptr>(native_pose));
    boundary(0x80100D94u, reinterpret_cast<uptr>(native_region));
    boundary(0x802FA778u, reinterpret_cast<uptr>(native_tick));
    boundary(address::ngl_add_string, reinterpret_cast<uptr>(native_text));
    word(address::ngl_system_font_pointer) = 0x81011000u;
    word(0x800000F8u) = 162000000u;
    prepare_names(); reset_fixture(); menu::reset();
    menu::Menu* devopts = menu::create("Devopts", 229u);
    devopt_features::configure_devopts(*devopts);
    check(devopts->count == 229u, "all 151 GC flags and 78 GC integers are present");
    check(equal(devopts->entries[14u].label, "ALLOC_ARAM") &&
          equal(devopts->entries[15u].label, "RENDER_LOWLODS"),
          "GC ALLOC_ARAM does not shift later options onto PC ordinals");
    check(equal(devopts->entries[150u].label, "UNLOCK_ALL_UNLOCKABLES") &&
          equal(devopts->entries[181u].label, "LISTBUFFER") &&
          equal(devopts->entries[228u].label, "MAX_AEPS_PARTICLES"),
          "last flag, GC allocation integer and last integer retain native names");
    bool all_flags = true;
    for (u32 i = 0u; i != 151u; ++i) {
        if (i == 91u) continue;
        auto& entry = devopts->entries[i];
        const uptr location = options_object + 4u + i * 4u;
        const u32 before = word(location - 4u), after = word(location + 4u);
        word(location) = 0xA5A5A5A5u;
        entry.handler(entry, menu::Command::activate);
        all_flags = all_flags && word(location) == 0u &&
                    word(location - 4u) == before && word(location + 4u) == after;
        entry.handler(entry, menu::Command::increment);
        all_flags = all_flags && word(location) == 1u;
    }
    check(all_flags, "every generic flag edits exactly its complete native four-byte word");
    bool all_ints = true;
    for (u32 i = 0u; i != 78u; ++i) {
        if (i == 2u || i == 29u) continue;
        auto& entry = devopts->entries[151u + i];
        const uptr location = options_object + 0x340u + i * 4u;
        const u32 before = word(location - 4u), after = word(location + 4u);
        word(location) = 123u;
        entry.handler(entry, menu::Command::increment);
        all_ints = all_ints && word(location) == 124u &&
                   word(location - 4u) == before && word(location + 4u) == after;
        entry.handler(entry, menu::Command::decrement);
        all_ints = all_ints && word(location) == 123u;
    }
    check(all_ints, "every generic integer modifies its native scalar and preserves neighbours");
    auto& last = devopts->entries[228u];
    word(options_object + 0x474u) = 0x7FFFFFFFu;
    last.handler(last, menu::Command::increment);
    check(word(options_object + 0x474u) == 1000u, "INT_MAX saturates safely at the PC upper bound");
    word(options_object + 0x474u) = 0x80000000u;
    last.handler(last, menu::Command::decrement);
    check(word(options_object + 0x474u) == static_cast<u32>(-1000),
          "INT_MIN saturates safely at the PC lower bound");
    word(options_object + 0x474u) = 10u;
    last.handler(last, menu::Command::activate); last.handler(last, menu::Command::frame);
    check(word(options_object + 0x474u) == 10u, "integer A and frame callbacks never change values");
    reset_fixture();
    auto& god = devopts->entries[180u];
    god.handler(god, menu::Command::increment);
    check(god_calls == 1u && engine::get_god_mode() == 1 && word(options_object + 0x3B4u) == 1u,
          "Devopts GOD_MODE updates the live native cheats and stored integer together");
    native_god(5); word(options_object + 0x3B4u) = 0u;
    char text[192]; display(god, text);
    check(equal(text, "5"), "Devopts GOD_MODE displays live native cheat state");
    god_calls = 0u; god.handler(god, menu::Command::increment);
    check(god_calls == 1u && engine::get_god_mode() == 5 && word(options_object + 0x3B4u) == 5u,
          "GOD_MODE respects its native six-mode range");
    state = 8; god_calls = 0u; word(options_object + 0x3B4u) = 2u;
    god.handler(god, menu::Command::decrement);
    check(god_calls == 0u && word(options_object + 0x3B4u) == 1u,
          "outside gameplay GOD_MODE updates the startup preference without calling unloaded code");
    reset_fixture();
    auto& camera = devopts->entries[153u]; camera.handler(camera, menu::Command::increment);
    check(camera_calls == 1u && word(game_object + 0x1C8u) == 1u &&
          word(options_object + 0x348u) == 1u,
          "Devopts CAMERA_STATE invokes the native camera transition and persistent enable word");
    state = 8; camera_calls = 0u; camera.handler(camera, menu::Command::decrement);
    check(camera_calls == 0u && word(options_object + 0x348u) == 1u,
          "CAMERA_STATE rejects a world transition without editing its option");
    reset_fixture(); word(options_object + 4u + 91u * 4u) = 0u;
    auto& second = devopts->entries[91u]; second.handler(second, menu::Command::activate);
    check(word(options_object + 4u + 91u * 4u) == 0u,
          "controller 2 camera flag cannot activate on a disconnected PAD channel");
    second_connected = true; second.handler(second, menu::Command::activate);
    check(engine::controller2_camera_enabled() && word(mouse_controller + 0x14u) == 0xF4241u,
          "controller 2 camera connects the native controller and flag together");
    second.handler(second, menu::Command::activate);
    check(!engine::controller2_camera_enabled() && word(mouse_controller + 0x14u) == ~0u,
          "controller 2 camera disables the native controller and flag together");

    menu::Menu* game_menu = menu::create("Game", 6u);
    const char* labels[] = {"Physics Enabled", "Slow Motion Enabled", "God Mode",
                            "Show Districts", "Show Hero Position", "Show FPS"};
    for (const char* label : labels) menu::add(*game_menu, label, menu::EntryType::boolean8);
    devopt_features::configure_game_extras(*game_menu);
    bool live_rows = true;
    for (u32 i = 0u; i != 6u; ++i)
        live_rows = live_rows && game_menu->entries[i].type == menu::EntryType::custom &&
                    game_menu->entries[i].target == nullptr && game_menu->entries[i].formatter != nullptr;
    check(live_rows, "Game controls have live formatters and no stale payload boolean targets");
    check(game_menu->entries[3u].id == 88u && game_menu->entries[4u].id == 21u &&
          game_menu->entries[5u].id == 5u, "Game display aliases resolve the GC native names");
    reset_fixture(); auto& physics = game_menu->entries[0u];
    word(game_object + 0x1A8u) = 1u; display(physics, text);
    check(equal(text, "False"), "Physics Enabled displays native disabled state set by another menu");
    physics.handler(physics, menu::Command::activate); display(physics, text);
    check(physics_calls == 1u && word(game_object + 0x1A8u) == 0u && equal(text, "True"),
          "Physics Enabled restores the actual native physics gate");
    word(game_object + 0x1A0u) = 0u; physics.handler(physics, menu::Command::activate); display(physics, text);
    check(physics_calls == 1u && equal(text, "N/A"), "unloaded level blocks physics calls and shows unavailable");
    reset_fixture(); auto& slow = game_menu->entries[1u];
    slow.handler(slow, menu::Command::activate); display(slow, text);
    check(word(options_object + 0x374u) == 120u && equal(text, "True"),
          "Slow Motion writes the native per-frame FRAME_LOCK consumer");
    slow.handler(slow, menu::Command::activate);
    check(word(options_object + 0x374u) == 30u, "Slow Motion restores the prior FRAME_LOCK exactly");
    word(options_object + 0x374u) = 60u; slow.handler(slow, menu::Command::activate);
    word(address::developer_options_pointer) = other_options; word(other_options + 0x374u) = 120u;
    slow.handler(slow, menu::Command::activate);
    check(word(other_options + 0x374u) == 0u, "replacement Devopts object never inherits another owner's restore value");
    word(address::developer_options_pointer) = options_object;
    word(options_object + 0x374u) = 45u; display(slow, text);
    check(equal(text, "False"), "Slow Motion formatter follows external FRAME_LOCK changes");
    auto& game_god = game_menu->entries[2u]; native_god(3); god_calls = 0u;
    game_god.handler(game_god, menu::Command::decrement); display(game_god, text);
    check(god_calls == 1u && engine::get_god_mode() == 2 && equal(text, "2") &&
          word(options_object + 0x3B4u) == 2u, "Game God Mode synchronizes its live state with Devopts GOD_MODE");

    reset_fixture();
    for (u32 i = 0u; i != 151u; ++i) word(options_object + 4u + i * 4u) = 0u;
    draw_count = 0u; devopt_features::render();
    check(draw_count == 0u && pose_calls == 0u && region_calls == 0u,
          "disabled display flags produce no HUD or native entity queries");
    game_menu->entries[5u].handler(game_menu->entries[5u], menu::Command::activate);
    tick = 0xFFF00000u;
    for (u32 i = 0u; i != 17u; ++i) { draw_count = 0u; devopt_features::render(); }
    check(draw_count == 1u && starts(draws[0u], "FPS: 60."),
          "Show FPS renders measured frames per SDK timebase second across tick rollover");
    word(options_object + 4u + 5u * 4u) = 0u;
    word(options_object + 4u + 21u * 4u) = 1u;
    *reinterpret_cast<float*>(pose_object + 0x30u) = 12.5f;
    *reinterpret_cast<float*>(pose_object + 0x34u) = -3.0f;
    *reinterpret_cast<float*>(pose_object + 0x38u) = 44.0f;
    draw_count = 0u; devopt_features::render();
    check(draw_count == 1u && starts(draws[0u], "Hero X: 12.5 Y: -3.0 Z: 44.0") && pose_calls == 1u,
          "Show Hero Position renders native absolute pose translation");
    word(options_object + 4u + 21u * 4u) = 0u;
    word(options_object + 4u + 88u * 4u) = 1u;
    draw_count = 0u; devopt_features::render();
    check(draw_count == 2u && equal(draws[0u], "DISTRICTS") && equal(draws[1u], "7"),
          "Show Districts renders native region+C0 district id rather than region+CC");
    word(region_vector) = 0x81007500u; draw_count = 0u; devopt_features::render();
    check(draw_count == 2u && equal(draws[1u], "--"),
          "a stale primary region outside current terrain cannot supply a district id");
    word(options_object + 4u + 88u * 4u) = 0u;
    word(options_object + 4u + 99u * 4u) = 1u; word(region_vector) = region_object;
    draw_count = 0u; devopt_features::render();
    check(draw_count == 2u && equal(draws[1u], "7"),
          "Devopts SHOW_DISTRICTS shares the functional district HUD");
    word(options_object + 4u + 99u * 4u) = 0u;
    word(options_object + 4u + 6u * 4u) = 1u; draw_count = 0u; devopt_features::render();
    check(draw_count == 1u && equal(draws[0u], "Regions: 1"),
          "SHOW_STREAMER_INFO renders the live native terrain catalogue count");
    word(terrain_object) = 0x817FFFFCu; word(terrain_object + 12u) = 2u;
    draw_count = 0u; devopt_features::render();
    check(draw_count == 1u && equal(draws[0u], "Regions: 0"),
          "partial terrain vector cannot be traversed beyond the MEM1 extent");
    state = 8; draw_count = 0u; pose_calls = 0u; region_calls = 0u; devopt_features::render();
    check(draw_count == 0u && pose_calls == 0u && region_calls == 0u,
          "world transition suppresses the HUD and native entity queries");
    reset_fixture(); word(address::developer_options_pointer) = 0x817FFFFCu;
    display(last, text); last.handler(last, menu::Command::increment);
    check(equal(text, "N/A"), "partial Devopts allocation at MEM1 end is rejected before any scalar read");
    word(address::developer_options_pointer) = 0u; display(devopts->entries[0u], text);
    check(equal(text, "N/A"), "missing Devopts shows unavailable rather than an invented boolean");
    char number[16]; menu::format_s32(number, sizeof(number), static_cast<s32>(checks));
    print(failures == 0u ? "PASS: " : "FAILED: "); print(number); print(" Game and Devopts checks\n");
    return failures == 0u ? 0 : 1;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
