#include "scene_config_gc.h"

#include "debug_menu_core.h"
#include "gc_engine.h"

// The desktop companion reads this exported, big-endian mailbox from MEM1.
// Publish the request sequence last and acknowledge only after both requested
// files have been flushed. The payload remains usable without the companion.
extern "C" {
struct UsmSceneMailbox {
    usm::gc::u32 magic;
    usm::gc::u32 version;
    volatile usm::gc::u32 frame;
    volatile usm::gc::u32 host_frame;
    volatile usm::gc::u32 request;
    volatile usm::gc::u32 acknowledged;
    volatile usm::gc::u32 result;
    char scene[32];
};
static_assert(sizeof(UsmSceneMailbox) == 60u);
__attribute__((used)) UsmSceneMailbox usm_gc_scene_mailbox = {
    0x55534D53u, 1u, 1u, 0u, 0u, 0u, 0u, {0}
};
}

namespace usm::gc::scene_config {
namespace {
constexpr uptr set_developer_string = 0x801D038Cu;
constexpr u32 scene_name_index = 1u;
u32 reported_request;
u32 reported_result;
u32 reboot_request;
u32 reboot_wait;
constexpr u32 reboot_timeout_frames = 600u;

bool equal(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    for (u32 i = 0u; i != 32u; ++i) {
        if (lhs[i] != rhs[i]) return false;
        if (lhs[i] == '\0') return true;
    }
    return false;
}

bool host_connected() {
    const u32 seen = usm_gc_scene_mailbox.host_frame;
    return seen != 0u && usm_gc_scene_mailbox.frame - seen < 300u;
}

void publish_barrier() {
#if defined(__powerpc__)
    asm volatile("sync" ::: "memory");
#else
    asm volatile("" ::: "memory");
#endif
}
}  // namespace

bool select_scene(const char* internal_name) {
    if (!equal(internal_name, "city_arena") &&
        !equal(internal_name, "characterb_arena")) {
        menu::set_status("Unsupported SCENE_NAME");
        return false;
    }
    void* options = engine::developer_options();
    const uptr options_address = reinterpret_cast<uptr>(options);
    if ((options_address & 3u) != 0u || options_address < 0x80000000u ||
        options_address > 0x81800000u - 0x280u) {
        menu::set_status("Developer options are not ready");
        return false;
    }
    engine::MString view{};
    view.guts = internal_name;
    view.size = static_cast<s32>(strlen(internal_name));
    // Verified native set_string: strings start at +0x260, stride16, and
    // mString::operator= copies the source bytes into game-owned storage.
    using SetString = void (*)(void*, u32, const engine::MString*);
    reinterpret_cast<SetString>(set_developer_string)(options, scene_name_index, &view);
    const auto* stored = reinterpret_cast<const engine::MString*>(
        static_cast<const u8*>(options) + 0x270u);
    const uptr string_address = reinterpret_cast<uptr>(stored->guts);
    if (stored->size != view.size || string_address < 0x80000000u ||
        string_address > 0x81800000u - static_cast<u32>(view.size) - 1u ||
        !equal(stored->guts, internal_name)) {
        menu::set_status("Native SCENE_NAME update failed");
        return false;
    }
    menu::copy_text(usm_gc_scene_mailbox.scene, sizeof(usm_gc_scene_mailbox.scene), internal_name);
    usm_gc_scene_mailbox.result = 0u;
    publish_barrier();
    u32 next = usm_gc_scene_mailbox.request + 1u;
    if (next == 0u) next = 1u;
    usm_gc_scene_mailbox.request = next;
    return true;
}

bool select_scene_and_reboot(const char* internal_name) {
    if (reboot_request != 0u || menu::interaction_is_locked()) {
        menu::set_status("Wait for the current operation before rebooting");
        return false;
    }
    if (!host_connected()) {
        menu::set_status("Run the INI companion before City save and reboot");
        return false;
    }
    if (!select_scene(internal_name)) return false;
    reboot_request = usm_gc_scene_mailbox.request;
    reboot_wait = 0u;
    menu::set_interaction_locked(true);
    menu::set_status("Saving game.ini; reboot after save confirmation", 600u);
    return true;
}

void frame_advance() {
    ++usm_gc_scene_mailbox.frame;
    if (usm_gc_scene_mailbox.frame == 0u) ++usm_gc_scene_mailbox.frame;
    const u32 request = usm_gc_scene_mailbox.request;
    if (request != 0u && (request != reported_request || usm_gc_scene_mailbox.result != reported_result) &&
        request == usm_gc_scene_mailbox.acknowledged) {
        publish_barrier();
        reported_request = request;
        reported_result = usm_gc_scene_mailbox.result;
        menu::set_status(usm_gc_scene_mailbox.result == 1u
                             ? "game.ini SCENE_NAME saved"
                             : "game.ini write failed; scene changed for this session");
    }
    if (reboot_request == 0u) return;
    const bool confirmed = request == reboot_request &&
                           usm_gc_scene_mailbox.acknowledged == reboot_request;
    if (confirmed && usm_gc_scene_mailbox.result == 1u) {
        // Consume before calling the SDK: a test/native return cannot reset
        // twice, and the companion has already flushed both requested files.
        reboot_request = 0u;
        reboot_wait = 0u;
        menu::set_interaction_locked(false);
        menu::set_status("game.ini saved; rebooting...");
        engine::reboot_game();
        return;
    }
    if (request != reboot_request ||
        (confirmed && usm_gc_scene_mailbox.result != 0u) ||
        !host_connected() || ++reboot_wait >= reboot_timeout_frames) {
        reboot_request = 0u;
        reboot_wait = 0u;
        menu::set_interaction_locked(false);
        menu::set_status("game.ini save not confirmed; reboot canceled");
    }
}

void format_persistence(char* output, u32 capacity) {
    const u32 request = usm_gc_scene_mailbox.request;
    const char* value;
    if (request != 0u && request == usm_gc_scene_mailbox.acknowledged) {
        value = usm_gc_scene_mailbox.result == 1u ? "Saved" : "Write failed";
    } else if (!host_connected()) {
        value = "Disc read-only; run INI companion";
    } else {
        value = request == 0u ? "Companion connected" : "Saving...";
    }
    menu::copy_text(output, capacity, value);
}

}  // namespace usm::gc::scene_config
