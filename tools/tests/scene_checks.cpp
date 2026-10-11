// Exercise the real scene setter and mailbox at the native call boundary.
#include "../../gamecube/scene_config_gc.cpp"
using namespace usm::gc;
namespace {
u32 checks, failures, calls, statuses;
u32 reboots;
bool interaction_locked;
void* options = reinterpret_cast<void*>(0x81000000u);
bool native_fails;
char last_status[100];
int syscall(u32 n, u32 a, u32 b = 0, u32 c = 0) {
    int r; asm volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory"); return r;
}
void print(const char* s) { syscall(4, 1, reinterpret_cast<uptr>(s), strlen(s)); }
void check(bool value, const char* label) {
    ++checks; if (!value) { ++failures; print("FAIL: "); print(label); print("\n"); }
}
bool same(const char* left, const char* right) {
    for (u32 i = 0;; ++i) {
        if (left[i] != right[i]) return false;
        if (!left[i]) return true;
    }
}
void native_set(void* owner, u32 index, const engine::MString* source) {
    ++calls;
    if (owner != options || index != 1 || source->size != static_cast<s32>(strlen(source->guts))) ++failures;
    auto* target = reinterpret_cast<engine::MString*>(0x81000270u);
    *target = *source;
    target->guts = native_fails ? reinterpret_cast<char*>(0x81900000u) : reinterpret_cast<char*>(0x81001000u);
    if (!native_fails) menu::copy_text(const_cast<char*>(target->guts), 32, source->guts);
}
}
extern "C" unsigned int strlen(const char* s) { u32 n = 0; while (s[n]) ++n; return n; }
namespace usm::gc::engine {
void* developer_options() { return options; }
void reboot_game() { ++reboots; }
}
namespace usm::gc::menu {
bool interaction_is_locked() { return interaction_locked; }
void set_interaction_locked(bool value) { interaction_locked = value; }
void copy_text(char* out, u32 size, const char* text) {
    if (!size) return;
    u32 i = 0;
    for (; i + 1 < size && text[i]; ++i) out[i] = text[i];
    out[i] = '\0';
}
void set_status(const char* value, u16) { ++statuses; copy_text(last_status, sizeof(last_status), value); }
}
extern "C" int run_tests() {
    u32 mmap_args[] = {0x80000000u, 0x1800000u, 7u, 0x32u, 0xFFFFFFFFu, 0u};
    if (static_cast<u32>(syscall(90, reinterpret_cast<uptr>(mmap_args))) != 0x80000000u) return 2;
    auto* entry = reinterpret_cast<u8*>(0x801D038Cu);
    entry[0] = 0xE9; *reinterpret_cast<u32*>(entry + 1) = reinterpret_cast<uptr>(native_set) - 0x801D0391u;
    using namespace scene_config;
    char display[100];
    format_persistence(display, sizeof(display));
    check(same(display, "Disc read-only; run INI companion"), "offline persistence is explicit");
    check(!select_scene("missing_arena") && calls == 0, "unsupported scene never calls native setter");
    options = nullptr;
    check(!select_scene("city_arena") && calls == 0, "missing options cannot publish a request");
    options = reinterpret_cast<void*>(0x817FFFFCu);
    check(!select_scene("city_arena") && calls == 0, "out of bounds options rejected");
    options = reinterpret_cast<void*>(0x81000000u);
    check(select_scene("city_arena") && calls == 1, "City calls verified native copy setter");
    check(usm_gc_scene_mailbox.request == 1 && equal(usm_gc_scene_mailbox.scene, "city_arena"),
          "City request publishes canonical scene");
    check(select_scene("characterb_arena") && calls == 2, "Characterb calls native setter");
    check(usm_gc_scene_mailbox.request == 2 && equal(usm_gc_scene_mailbox.scene, "characterb_arena"),
          "Characterb request publishes canonical scene");
    native_fails = true;
    check(!select_scene("city_arena") && usm_gc_scene_mailbox.request == 2,
          "invalid native copy pointer cannot be dereferenced or acknowledged");
    native_fails = false;
    usm_gc_scene_mailbox.host_frame = usm_gc_scene_mailbox.frame;
    format_persistence(display, sizeof(display));
    check(equal(display, "Saving..."), "connected pending request shows saving");
    usm_gc_scene_mailbox.result = 1; usm_gc_scene_mailbox.acknowledged = 2;
    const u32 before_status = statuses; frame_advance();
    check(statuses == before_status + 1 && equal(last_status, "game.ini SCENE_NAME saved"),
          "success is reported only after companion acknowledgment");
    frame_advance(); check(statuses == before_status + 1, "acknowledgment status is reported once");
    format_persistence(display, sizeof(display)); check(equal(display, "Saved"), "saved formatter");
    select_scene("city_arena"); usm_gc_scene_mailbox.result = 2; usm_gc_scene_mailbox.acknowledged = 3;
    frame_advance(); format_persistence(display, sizeof(display));
    check(equal(display, "Write failed"), "failed write is distinct from saved");
    usm_gc_scene_mailbox.result = 1; frame_advance();
    check(same(last_status, "game.ini SCENE_NAME saved"), "successful retry updates failed request status");
    usm_gc_scene_mailbox.frame = 400; usm_gc_scene_mailbox.host_frame = 1;
    select_scene("city_arena"); format_persistence(display, sizeof(display));
    check(same(display, "Disc read-only; run INI companion"), "disconnected companion expires");
    usm_gc_scene_mailbox.request = 0xFFFFFFFFu; select_scene("city_arena");
    check(usm_gc_scene_mailbox.request == 1, "request sequence wrap skips zero");
    usm_gc_scene_mailbox.frame = 0xFFFFFFFFu; frame_advance();
    check(usm_gc_scene_mailbox.frame == 1, "frame sequence wrap skips zero");
    const u32 calls_before_reboot = calls;
    usm_gc_scene_mailbox.host_frame = 0u;
    check(!select_scene_and_reboot("city_arena") && calls == calls_before_reboot && reboots == 0u,
          "offline City refuses to reboot or publish an unsavable selection");
    usm_gc_scene_mailbox.host_frame = usm_gc_scene_mailbox.frame;
    interaction_locked = true;
    check(!select_scene_and_reboot("city_arena") && calls == calls_before_reboot,
          "another transaction blocks City save and reboot");
    interaction_locked = false;
    check(select_scene_and_reboot("city_arena") && reboot_request == usm_gc_scene_mailbox.request && interaction_locked,
          "City publishes a save request and locks further engine mutations");
    const u32 pending = reboot_request;
    frame_advance();
    check(reboots == 0u && reboot_request == pending, "City does not reboot before acknowledgment");
    usm_gc_scene_mailbox.acknowledged = pending - 1u; usm_gc_scene_mailbox.result = 1u; frame_advance();
    check(reboots == 0u && reboot_request == pending, "stale successful acknowledgment cannot reboot City");
    usm_gc_scene_mailbox.acknowledged = pending; usm_gc_scene_mailbox.result = 0u; frame_advance();
    check(reboots == 0u && reboot_request == pending, "acknowledgment without a successful save result cannot reboot");
    usm_gc_scene_mailbox.result = 1u; frame_advance();
    check(reboots == 1u && reboot_request == 0u && !interaction_locked,
          "successful flush acknowledgment triggers exactly one native reboot");
    frame_advance(); check(reboots == 1u, "consumed success cannot trigger another reboot");
    usm_gc_scene_mailbox.host_frame = usm_gc_scene_mailbox.frame;
    select_scene_and_reboot("city_arena"); usm_gc_scene_mailbox.acknowledged = reboot_request;
    usm_gc_scene_mailbox.result = 2u; frame_advance();
    check(reboots == 1u && reboot_request == 0u && !interaction_locked,
          "failed INI write cancels reboot and restores menu input");
    usm_gc_scene_mailbox.result = 1u; frame_advance();
    check(reboots == 1u, "late successful retry after cancellation never reboots unexpectedly");
    usm_gc_scene_mailbox.host_frame = usm_gc_scene_mailbox.frame;
    select_scene_and_reboot("city_arena"); usm_gc_scene_mailbox.host_frame = 0u; frame_advance();
    check(reboots == 1u && reboot_request == 0u && !interaction_locked,
          "companion disconnect cancels a pending reboot");
    usm_gc_scene_mailbox.host_frame = usm_gc_scene_mailbox.frame;
    select_scene_and_reboot("city_arena");
    for (u32 i = 0u; i < reboot_timeout_frames; ++i) {
        usm_gc_scene_mailbox.host_frame = usm_gc_scene_mailbox.frame;
        frame_advance();
    }
    check(reboots == 1u && reboot_request == 0u && !interaction_locked,
          "connected companion without confirmation times out without rebooting");
    usm_gc_scene_mailbox.host_frame = usm_gc_scene_mailbox.frame;
    select_scene_and_reboot("city_arena"); ++usm_gc_scene_mailbox.request; frame_advance();
    check(reboots == 1u && reboot_request == 0u && !interaction_locked,
          "replacement scene request cancels the obsolete reboot transaction");
    native_fails = true; usm_gc_scene_mailbox.host_frame = usm_gc_scene_mailbox.frame;
    check(!select_scene_and_reboot("city_arena") && !interaction_locked && reboot_request == 0u,
          "native SCENE_NAME copy failure never arms a reboot");
    print(failures ? "FAILED scene checks\n" : "PASS: 32 scene configuration checks\n");
    return failures ? 1 : 0;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
