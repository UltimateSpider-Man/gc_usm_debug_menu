// Executes the real menu/input implementation as a freestanding 32-bit host
// program. Only the fixed-address engine boundary is substituted.
#include "../../gamecube/debug_menu_core.h"
#include "../../gamecube/gc_engine.h"
#include "../../gamecube/gc_addresses.h"
#include "../../gamecube/menus/replay_gc.h"

using namespace usm::gc;
namespace {
s32 state = 6;
bool paused;
u32 failures;
u32 checks;
u32 calls;
bool lock_on_call;
bool close_on_call;
u32 observed_scalar;
PadStatus hardware[4];
bool replay_inject;
bool replay_focus;
PadStatus replay_observed[4];
u32 dismiss_pause_calls;
u32 unpause_calls;
bool native_dialog;

int system_call(u32 number, u32 arg1, u32 arg2 = 0u, u32 arg3 = 0u) {
    int result;
    asm volatile("int $0x80" : "=a"(result)
                 : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return result;
}
void print(const char* text) {
    system_call(4u, 1u, reinterpret_cast<uptr>(text), strlen(text));
}
void check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; print("FAIL: "); print(label); print("\n"); }
}
u32 fake_pad_read(PadStatus* pads) {
    memcpy(pads, hardware, sizeof(hardware));
    return 0x12345678u;
}
void disconnect_all() {
    memset(hardware, 0, sizeof(hardware));
    for (u32 i = 0u; i < 4u; ++i) { hardware[i].error = -1; }
}
input::Frame sample(bool focus = false) {
    PadStatus delivered[4];
    input::read_for_game(delivered, focus);
    return input::poll();
}
void action(menu::Entry&, menu::Command command) {
    if (command != menu::Command::activate) { return; }
    ++calls;
    if (lock_on_call) { menu::set_interaction_locked(true); }
    if (close_on_call) { menu::close(); }
}
void scalar_action(menu::Entry& entry, menu::Command command) {
    if (command == menu::Command::frame) { return; }
    ++calls;
    observed_scalar = entry.type == menu::EntryType::boolean8
                          ? *static_cast<u8*>(entry.target)
                          : *static_cast<u32*>(entry.target);
    if (lock_on_call) { menu::set_interaction_locked(true); }
    if (close_on_call) { menu::close(); }
}
menu::Menu* make_menu() {
    menu::reset(); state = 6; paused = false; calls = 0u;
    lock_on_call = false; close_on_call = false;
    auto* root = menu::create("Test", 2u);
    auto* entry = menu::add(*root, "Run", menu::EntryType::action);
    entry->handler = action;
    menu::set_root(*root);
    return root;
}
void open_menu() {
    input::Frame frame{}; frame.toggle_menu = true;
    menu::update(frame); menu::dispatch_deferred();
}
void accept() {
    input::Frame frame{}; frame.accept = true; menu::update(frame);
}
}

namespace usm::gc::replay {
void process_game_pads(PadStatus* pads, bool menu_has_focus) {
    replay_focus = menu_has_focus;
    memcpy(replay_observed, pads, sizeof(replay_observed));
    if (replay_inject) {
        pads[0].button = 0x110u;
        pads[0].stick_x = 55;
    }
}
}

namespace usm::gc::engine {
s32 game_state() { return state; }
bool game_paused() { return paused; }
void pause_game() { paused = true; state = 7; }
void unpause_game() { ++unpause_calls; paused = false; state = 6; }
bool dismiss_debug_pause_menu() {
    ++dismiss_pause_calls;
    if (native_dialog) { native_dialog = false; paused = false; state = 6; }
    return true;
}
void draw_panel(float, float, float, float, u32, float) {}
void draw_text(const char*, float, float, u32, float) {}
bool measure_text(const char*, float, u32&, u32&) { return false; }
}
extern "C" void* memcpy(void* d, const void* s, unsigned int n) {
    auto* out = static_cast<u8*>(d); const auto* in = static_cast<const u8*>(s);
    for (u32 i = 0; i < n; ++i) out[i] = in[i]; return d;
}
extern "C" void* memset(void* d, int v, unsigned int n) {
    auto* out = static_cast<u8*>(d);
    for (u32 i = 0; i < n; ++i) out[i] = static_cast<u8>(v); return d;
}
extern "C" int memcmp(const void* a, const void* b, unsigned int n) {
    const auto* x = static_cast<const u8*>(a); const auto* y = static_cast<const u8*>(b);
    for (u32 i = 0; i < n; ++i) if (x[i] != y[i]) return x[i] - y[i]; return 0;
}
extern "C" unsigned int strlen(const char* s) {
    u32 size = 0; while (s[size]) ++size; return size;
}

extern "C" int run_tests() {
    // Install a jump to fake PADRead at the exact retail API address. This
    // exercises read_for_game/poll without adding a production test switch.
    u32 mapping[6] = {address::pad_read & ~4095u, 4096u, 7u, 0x32u, ~0u, 0u};
    if (static_cast<uptr>(system_call(90u, reinterpret_cast<uptr>(mapping))) != mapping[0]) {
        print("Cannot map PADRead test boundary\n"); return 2;
    }
    auto* jump = reinterpret_cast<u8*>(address::pad_read);
    jump[0] = 0xB8u; *reinterpret_cast<uptr*>(jump + 1) = reinterpret_cast<uptr>(&fake_pad_read);
    jump[5] = 0xFFu; jump[6] = 0xE0u;
    disconnect_all(); sample(); hardware[0].error = 0;
    check(!sample().toggle_menu, "idle input does not open menu");
    hardware[0].button = 0x1000u;
    check(!sample().toggle_menu, "START alone does not open menu");
    hardware[0].button = 0x60u;
    check(!sample().toggle_menu, "L+R do not open menu");
    hardware[0].button = 0x10u;
    PadStatus delivered[4];
    check(input::read_for_game(delivered, false) == 0x12345678u, "PAD reset mask preserved");
    check(delivered[0].button == 0u, "Z removed from gameplay input");
    check(input::poll().toggle_menu, "Z alone opens menu");
    check(!sample().toggle_menu, "held Z does not repeat toggle");
    hardware[0].button = 0u; sample();
    hardware[0].button = 0x10u;
    input::read_for_game(delivered, false);
    hardware[0].button = 0u;
    input::read_for_game(delivered, false);
    check(input::poll().toggle_menu, "short Z press latched before render");
    hardware[0].button = 0x1060u; hardware[0].trigger_left = 77u; hardware[0].trigger_right = 99u;
    input::read_for_game(delivered, false); input::poll();
    check(delivered[0].button == 0x1060u && delivered[0].trigger_left == 77u &&
          delivered[0].trigger_right == 99u, "START and triggers preserved with menu closed");
    hardware[0].button = 0x100u; hardware[0].stick_x = 70;
    input::read_for_game(delivered, true);
    check(delivered[0].button == 0u && delivered[0].stick_x == 0 && delivered[0].error == 0,
          "menu neutralizes gameplay input preserving controller status");
    input::poll();
    hardware[0].button = 0u; hardware[0].stick_x = 0; sample();
    hardware[0].button = 0x108u;
    auto frame = sample();
    check(frame.up && frame.accept, "D-Pad and A remain navigation controls");
    hardware[0].button = 0u; sample();
    hardware[2].error = 0; hardware[2].button = 0x10u;
    check(sample().toggle_menu, "Z on controller 3 opens menu");
    hardware[2].button = 0u; sample(); hardware[0].button = 0x100u;
    check(!sample().accept, "opening controller owns navigation focus");
    hardware[2].button = 0x200u;
    check(sample().cancel, "focused controller B navigates back");
    check(input::connected(0u) && input::connected(2u) && !input::connected(4u),
          "controller connection query is bounded");
    hardware[2].error = -1; hardware[0].button = 0u; sample(); hardware[0].button = 0x100u;
    check(sample().accept, "disconnect transfers navigation to connected controller");

    hardware[0].button = 0u; sample();
    replay_inject = true;
    hardware[0].button = 0x10u;
    input::read_for_game(delivered, false);
    check(replay_observed[0].button == 0x10u && !replay_focus,
          "Replay hook receives physical Z before gameplay filtering");
    check(delivered[0].button == 0x100u && delivered[0].stick_x == 55,
          "Replay output reaches gameplay with Z stripped");
    frame = input::poll();
    check(frame.toggle_menu && !frame.accept,
          "physical Z keeps menu control while injected A cannot navigate");
    hardware[0].button = 0u; input::read_for_game(delivered, true);
    check(replay_focus && delivered[0].button == 0u && delivered[0].stick_x == 0,
          "menu focus neutralizes injected Replay input");
    input::poll(); replay_inject = false;
    hardware[0].button = 0x100u;
    check(sample().accept, "physical input resumes after Replay stops");
    hardware[0].button = 0u; sample();

    make_menu(); input::Frame toggle{}; toggle.toggle_menu = true;
    menu::update(toggle); accept();
    check(calls == 0u, "actions blocked while acquiring game pause");
    menu::dispatch_deferred();
    check(menu::visible() && paused && menu::owns_input(), "opening menu acquires game pause");
    accept(); check(calls == 0u, "actions deferred outside render");
    menu::dispatch_deferred(); check(calls == 1u, "deferred action executes once");
    menu::close();
    check(!menu::visible() && menu::owns_input(), "closing retains input until unpause");
    menu::dispatch_deferred();
    check(!paused && !menu::owns_input(), "deferred unpause releases input");

    make_menu(); open_menu(); native_dialog = true;
    const u32 owned_dismissals = dismiss_pause_calls;
    const u32 owned_unpause_calls = unpause_calls;
    menu::close(); menu::dispatch_deferred();
    check(!paused && !native_dialog && dismiss_pause_calls == owned_dismissals + 1u,
          "closing debug-owned pause deactivates its native dialog UI");
    check(unpause_calls == owned_unpause_calls,
          "native UI deactivation does not trigger a duplicate unpause");
    make_menu(); state = 7; paused = true; native_dialog = true;
    const u32 external_dismissals = dismiss_pause_calls;
    menu::update(toggle); menu::dispatch_deferred(); menu::close(); menu::dispatch_deferred();
    check(paused && native_dialog && dismiss_pause_calls == external_dismissals,
          "closing over a preexisting pause preserves native dialogs");
    native_dialog = false;

    make_menu(); open_menu(); accept(); menu::close(); menu::dispatch_deferred();
    check(calls == 0u, "closing cancels queued action");
    make_menu(); open_menu(); lock_on_call = true; accept(); accept(); menu::dispatch_deferred();
    check(calls == 1u, "transaction lock cancels remaining actions");
    menu::close(); check(menu::visible(), "transaction lock prevents close");
    menu::set_interaction_locked(false); menu::close(); menu::dispatch_deferred();
    check(calls == 1u, "cancelled transaction action never runs later");
    make_menu(); open_menu(); close_on_call = true; accept(); accept(); menu::dispatch_deferred();
    check(calls == 1u, "closing handler cancels subsequent queued actions");

    auto* scalar_root = make_menu(); u8 boolean8 = 0u;
    auto* scalar = menu::add(*scalar_root, "Toggle", menu::EntryType::boolean8);
    scalar->target = &boolean8; scalar->handler = scalar_action;
    open_menu(); scalar_root->selected = 1u; close_on_call = true;
    accept(); accept();
    check(boolean8 == 0u, "handler-backed boolean changes are deferred outside render");
    menu::dispatch_deferred();
    check(calls == 1u && observed_scalar == 1u,
          "first queued boolean handler observes its own toggle");
    check(boolean8 == 1u, "closing cancels subsequent scalar mutation");

    scalar_root = make_menu(); u32 boolean32 = 0u;
    scalar = menu::add(*scalar_root, "Toggle32", menu::EntryType::boolean32);
    scalar->target = &boolean32; scalar->handler = scalar_action;
    open_menu(); scalar_root->selected = 1u; lock_on_call = true;
    accept(); accept(); menu::dispatch_deferred();
    check(calls == 1u && observed_scalar == 1u && boolean32 == 1u,
          "transaction lock cancels subsequent word boolean mutation");

    scalar_root = make_menu(); s32 deferred_integer = 5;
    scalar = menu::add(*scalar_root, "Integer", menu::EntryType::integer32);
    scalar->target = &deferred_integer; scalar->handler = scalar_action;
    open_menu(); scalar_root->selected = 1u;
    input::Frame scalar_adjust{}; scalar_adjust.right = true; scalar_adjust.fast = true;
    menu::update(scalar_adjust);
    check(deferred_integer == 5, "handler-backed integer mutation is deferred");
    menu::dispatch_deferred();
    check(deferred_integer == 15 && observed_scalar == 15u,
          "deferred integer retains the command multiplier");

    scalar_root = make_menu(); float deferred_float = 1.0f;
    scalar = menu::add(*scalar_root, "Float", menu::EntryType::float32);
    scalar->target = &deferred_float; scalar->handler = scalar_action; scalar->float_step = 0.5f;
    open_menu(); scalar_root->selected = 1u; menu::update(scalar_adjust);
    check(deferred_float == 1.0f, "handler-backed float mutation is deferred");
    menu::dispatch_deferred();
    check(deferred_float == 6.0f, "deferred float retains the command multiplier");

    char number[16]; menu::format_s32(number, sizeof(number), static_cast<s32>(0x80000000u));
    check(memcmp(number, "-2147483648", 12u) == 0, "integer formatting handles INT_MIN");
    u32 float_bits = 0x7FC00000u; float special;
    memcpy(&special, &float_bits, 4u); menu::format_float(number, sizeof(number), special);
    check(memcmp(number, "NaN", 4u) == 0, "NaN formatting avoids integer conversion");
    float_bits = 0x7F800000u; memcpy(&special, &float_bits, 4u);
    menu::format_float(number, sizeof(number), special);
    check(memcmp(number, "INF", 4u) == 0, "infinite float formatting");
    float_bits = 0xFF800000u; memcpy(&special, &float_bits, 4u);
    menu::format_float(number, sizeof(number), special);
    check(memcmp(number, "-INF", 5u) == 0, "negative infinite float formatting");
    menu::format_float(number, sizeof(number), 1.0e20f);
    check(memcmp(number, "1.0e20", 7u) == 0, "large float formatting avoids integer overflow");
    auto* root = make_menu(); s32 integer = 2147483647;
    auto* value = menu::add(*root, "Integer", menu::EntryType::integer32);
    value->target = &integer; value->minimum = (-2147483647 - 1); value->maximum = 2147483647;
    open_menu(); root->selected = 1u; input::Frame adjust{}; adjust.right = true; adjust.fast = true;
    menu::update(adjust);
    check(integer == 2147483647, "integer maximum saturates without signed overflow");
    integer = (-2147483647 - 1); adjust.right = false; adjust.left = true;
    menu::update(adjust);
    check(integer == (-2147483647 - 1), "integer minimum saturates without signed underflow");
    print(failures == 0u ? "PASS: " : "FAILED: ");
    menu::format_s32(number, sizeof(number), static_cast<s32>(checks)); print(number); print(" runtime checks\n");
    return failures == 0u ? 0 : 1;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
