#include "gc_engine.h"

#include "gc_addresses.h"
#include "gc_input.h"
#include "resource_memory_gc.h"

namespace usm::gc::engine {
namespace {

template <typename Function>
inline Function function_at(uptr location) {
    return reinterpret_cast<Function>(location);
}

template <typename T>
inline T* object_from_slot(uptr location) {
    T* value = memory<T*>(location);
    return is_main_ram_pointer(value) ? value : nullptr;
}

inline u8* bytes(void* value) {
    return reinterpret_cast<u8*>(value);
}

inline const u8* bytes(const void* value) {
    return reinterpret_cast<const u8*>(value);
}

bool rel_context_ready() {
    // The main game REL and gameplay-owned objects are treated as callable
    // only in RUNNING (6) or PAUSED (7).  Transitional states 5 and 8..14 can
    // retain stale-looking pointers while tearing a level down or rebuilding
    // it, so a non-null game/settings pair alone is not sufficient evidence.
    if (!game_ready() || settings() == nullptr) {
        return false;
    }
    const s32 state = game_state();
    return state == 6 || state == 7;
}

bool camera_object_ready(const void* object, u32 size) {
    const uptr location = reinterpret_cast<uptr>(object);
    return is_main_ram_pointer(object) && size <= 0x81800000u - location;
}

}  // namespace

void* game() {
    return object_from_slot<void>(address::game_pointer);
}

void* world() {
    return object_from_slot<void>(address::world_pointer);
}

void* developer_options() {
    return object_from_slot<void>(address::developer_options_pointer);
}

void* settings() {
    void* game_object = game();
    if (game_object == nullptr) {
        return nullptr;
    }
    void* value = *reinterpret_cast<void**>(bytes(game_object) + 0xD8u);
    return is_main_ram_pointer(value) ? value : nullptr;
}

void* terrain() {
    void* world_object = world();
    if (world_object == nullptr) {
        return nullptr;
    }
    void* value = *reinterpret_cast<void**>(bytes(world_object) + 0x198u);
    return is_main_ram_pointer(value) ? value : nullptr;
}

bool game_ready() {
    return game() != nullptr;
}

s32 game_state() {
    void* game_object = game();
    if (game_object == nullptr) {
        return -1;
    }
    using Function = s32 (*)(void*);
    return function_at<Function>(address::game_get_state)(game_object);
}

bool game_paused() {
    void* game_object = game();
    return game_object != nullptr &&
           *reinterpret_cast<volatile u32*>(bytes(game_object) + 0x1B0u) != 0u;
}

bool gameplay_rel_ready() {
    return rel_context_ready();
}

bool interactive_world_ready() {
    if (!rel_context_ready() || world() == nullptr) {
        return false;
    }
    const s32 state = game_state();
    return state == 6 || state == 7;
}

void pause_game() {
    if (void* object = game()) {
        function_at<void (*)(void*)>(address::game_pause)(object);
    }
}

void unpause_game() {
    if (void* object = game()) {
        function_at<void (*)(void*)>(address::game_unpause)(object);
    }
}

bool dismiss_debug_pause_menu() {
    void* object = game();
    void* world_object = world();
    // Deactivate calls game::unpause, which accesses through game+0x314.
    // Validate the whole native span before settings() or game_state() read it.
    if (!camera_object_ready(object, 0x318u) ||
        !camera_object_ready(world_object, 0x228u) ||
        !camera_object_ready(settings(), 4u) ||
        *reinterpret_cast<void**>(bytes(object) + 0x68u) != world_object ||
        *reinterpret_cast<volatile u32*>(bytes(object) + 0x5Cu) == 0u ||
        *reinterpret_cast<volatile u32*>(bytes(object) + 0x1A0u) == 0u ||
        *reinterpret_cast<volatile u32*>(bytes(object) + 0x1B0u) == 0u ||
        game_state() != 7) {
        return false;
    }
    void* manager = object_from_slot<void>(address::pause_menu_system_pointer);
    void* comicpanel = memory<void*>(address::comicpanel_system_pointer);
    const uptr comicpanel_location = reinterpret_cast<uptr>(comicpanel);
    // The current native comicpanel system is mutable UI state allocated in
    // the ARAM pager (observed F035A4C8), not a READY packed resource. The native
    // page-fault handler at801AEF50 accepts the F segment. Trust only this
    // engine-owned global in the loaded/paused context checked above, with
    // alignment and the complete +74 word inside MEM1 or the pager segment.
    const bool comicpanel_ready = camera_object_ready(comicpanel, 0x78u) ||
        ((comicpanel_location & 3u) == 0u && comicpanel_location >= 0xF0000000u &&
         0x77u <= 0xFFFFFFFFu - comicpanel_location);
    if (!camera_object_ready(manager, 0x40u) ||
        !comicpanel_ready ||
        *reinterpret_cast<volatile s32*>(bytes(manager) + 8u) < 0) {
        return false;
    }
    // Native 7F025700 invokes MakeActive(-1), unpauses, and restores the saved
    // comicpanel state at manager+3C. A raw index write skips those side effects.
    function_at<void (*)(void*)>(address::pause_menu_deactivate)(manager);
    return true;
}

void set_camera(s32 camera) {
    if (!camera_available(camera)) {
        return;
    }
    void* object = game();
    // Native handle_cameras8017DD8C..DDD8 reconciles CAMERA_STATE against
    // this complete word every frame. Changing only the Devopt reverts User.
    *reinterpret_cast<volatile u32*>(bytes(object) + 0x1C8u) = camera == 1 ? 1u : 0u;
    // The native setter stores the CAMERA Devopt and switches chase/user
    // cameras, but its case 2 intentionally does no scene-analyzer work.
    // The analyzer's own frame advance checks this separate geometry flag.
    function_at<void (*)(bool)>(address::enable_scene_analyzer)(false);
    if (camera == 2) {
        function_at<void (*)(void*, s32)>(address::game_set_camera)(object, 0);
    }
    function_at<void (*)(void*, s32)>(address::game_set_camera)(object, camera);
    function_at<void (*)(bool)>(address::enable_scene_analyzer)(camera == 2);
}

s32 get_camera() {
    if (function_at<bool (*)()>(address::scene_analyzer_enabled)()) {
        return 2;
    }
    void* object = game();
    return camera_object_ready(object, 0x1CCu) &&
                   *reinterpret_cast<volatile u32*>(bytes(object) + 0x1C8u) != 0u ? 1 : 0;
}

bool camera_available(s32 camera) {
    if (camera < 0 || camera > 2) {
        return false;
    }
    void* object = game();
    void* world_object = world();
    if (!camera_object_ready(object, 0x1CCu) ||
        !camera_object_ready(world_object, 0x70u) ||
        !camera_object_ready(developer_options(), 0x34Cu) || !rel_context_ready() ||
        *reinterpret_cast<volatile u32*>(bytes(object) + 0x1A0u) == 0u ||
        *reinterpret_cast<volatile u32*>(bytes(object) + 0x5Cu) == 0u ||
        *reinterpret_cast<void**>(bytes(object) + 0x68u) != world_object ||
        !camera_object_ready(*reinterpret_cast<void**>(bytes(object) + 0x78u), 0x130u) ||
        !camera_object_ready(*reinterpret_cast<void**>(bytes(object) + 0x80u), 4u)) {
        return false;
    }
    if (camera != 0) {
        // wds_camera_manager is embedded at world+28. Native User advance
        // 8015868C and Analyzer advance8015873C each require four objects.
        u8* manager = bytes(world_object) + 0x28u;
        const u32 first = camera == 1 ? 0x24u : 0x34u;
        for (u32 offset = first; offset <= first + 0xCu; offset += 4u) {
            if (!camera_object_ready(*reinterpret_cast<void**>(manager + offset),
                                     offset == first + 0xCu ? 0xCCu : 8u)) {
                return false;
            }
        }
    }
    return true;
}

bool controller2_camera_enabled() {
    void* controller = object_from_slot<void>(address::mouselook_controller_pointer);
    return get_developer_flag(91u) && controller != nullptr &&
           *reinterpret_cast<volatile s32*>(bytes(controller) + 0x14u) == 0xF4241;
}

bool set_controller2_camera(bool enabled) {
    if (!interactive_world_ready() || developer_options() == nullptr) {
        return false;
    }
    void* controller = object_from_slot<void>(address::mouselook_controller_pointer);
    void* manager = object_from_slot<void>(address::input_manager_pointer);
    if (controller == nullptr || manager == nullptr) {
        return false;
    }
    s32 device_id = -1;
    if (enabled) {
        // input_mgr::insert_device (0x801DFBB8) fills joypad slots at
        // +0x38; gc_joypad_device stores its id in word zero. Its retail
        // is_connected method always returns true, so use sampled PAD state.
        void* device = *reinterpret_cast<void**>(bytes(manager) + 0x3Cu);
        if (!input::connected(1u) || !is_main_ram_pointer(device)) {
            return false;
        }
        device_id = *reinterpret_cast<volatile s32*>(device);
        if (device_id != 0xF4241) {
            return false;
        }
    }
    *reinterpret_cast<volatile s32*>(bytes(controller) + 0x14u) = device_id;
    set_developer_flag(91u, enabled);
    if (get_camera() == 1) {
        void* hero = *reinterpret_cast<void**>(bytes(world()) + 0x21Cu);
        if (is_main_ram_pointer(hero)) {
            function_at<void (*)(void*, bool)>(enabled ? address::actor_unsuspend
                                                      : address::actor_suspend)(hero, true);
        }
    }
    return true;
}

void reboot_game() {
    set_rumble(false);
    // OS_RESET_RESTART restarts the current disc, retaining SDK shutdown
    // callbacks instead of jumping past audio/DVD teardown.
    function_at<void (*)(s32, u32, bool)>(address::os_reset_system)(0, 0u, false);
}

void set_physics(bool enabled) {
    if (void* object = game(); object != nullptr && rel_context_ready()) {
        function_at<void (*)(void*, bool)>(address::game_enable_physics)(object, enabled);
    }
}

void request_single_step() {
    if (void* object = game(); object != nullptr && rel_context_ready()) {
        // The native producer at 0x8017DEC4 sets this word. Game advance
        // consumes it alongside physics-disabled +0x1A8 and clears it at
        // 0x80142D9C after advancing one frame.
        *reinterpret_cast<volatile u32*>(bytes(object) + 0x1A4u) = 1u;
    }
}

void set_god_mode(s32 mode) {
    if (rel_context_ready()) {
        function_at<void (*)(s32)>(address::set_god_mode)(mode);
    }
}

s32 get_god_mode() {
    const bool god = volatile_memory<u32>(address::god_mode_cheat) != 0u;
    const bool ultra = volatile_memory<u32>(address::ultra_god_mode_cheat) != 0u;
    const bool mega = volatile_memory<u32>(address::mega_god_mode_cheat) != 0u;
    if (god) {
        return mega ? 3 : ultra ? 2 : 1;
    }
    return mega ? 5 : ultra ? 4 : 0;
}

bool monkey_running() {
    return volatile_memory<u32>(address::spider_monkey_running) != 0u;
}

void set_monkey_running(bool enabled) {
    if (!rel_context_ready()) {
        return;
    }
    if (enabled) {
        function_at<void (*)()>(address::spider_monkey_start)();
        if (game_ready()) {
            function_at<void (*)()>(address::spider_monkey_on_level_load)();
        }
        set_rumble(false);
    } else {
        if (monkey_running() && game_ready()) {
            function_at<void (*)()>(address::spider_monkey_on_level_unload)();
        }
        volatile_memory<u32>(address::spider_monkey_running) = 0u;
        set_developer_int(9u, 0);  // MONKEY_MODE
    }
}

void set_rumble(bool enabled) {
    void* manager = memory<void*>(address::input_manager_pointer);
    if (is_main_ram_pointer(manager)) {
        void* rumble = *reinterpret_cast<void**>(bytes(manager) + 4u);
        if (is_main_ram_pointer(rumble)) {
            function_at<void (*)(void*)>(enabled ? address::rumble_enable
                                                : address::rumble_disable)(rumble);
        }
    }
    if (!enabled) {
        using Function = void (*)(s32, u32);
        for (s32 channel = 0; channel != 4; ++channel) {
            function_at<Function>(address::pad_control_motor)(channel, 0u);
        }
    }
}

const char* developer_flag_name(u32 index) {
    if (index >= address::developer_flag_count) {
        return nullptr;
    }
    const char* value =
        reinterpret_cast<const char* const*>(address::developer_flag_names)[index];
    return readable_c_string(value) ? value : nullptr;
}

const char* developer_int_name(u32 index) {
    if (index >= address::developer_int_count) {
        return nullptr;
    }
    const char* value =
        reinterpret_cast<const char* const*>(address::developer_int_names)[index];
    return readable_c_string(value) ? value : nullptr;
}

bool get_developer_flag(u32 index) {
    void* options = developer_options();
    if (options == nullptr || index >= address::developer_flag_count) {
        return false;
    }
    return *reinterpret_cast<volatile u32*>(bytes(options) + 4u + index * 4u) != 0u;
}

void set_developer_flag(u32 index, bool value) {
    void* options = developer_options();
    if (options != nullptr && index < address::developer_flag_count) {
        *reinterpret_cast<volatile u32*>(bytes(options) + 4u + index * 4u) = value ? 1u : 0u;
    }
}

s32 get_developer_int(u32 index) {
    void* options = developer_options();
    if (options == nullptr || index >= address::developer_int_count) {
        return 0;
    }
    return *reinterpret_cast<volatile s32*>(bytes(options) + 0x340u + index * 4u);
}

void set_developer_int(u32 index, s32 value) {
    void* options = developer_options();
    if (options != nullptr && index < address::developer_int_count) {
        *reinterpret_cast<volatile s32*>(bytes(options) + 0x340u + index * 4u) = value;
    }
}

const char* debug_render_name(u32 index) {
    if (index >= 51u || !game_ready()) {
        return nullptr;
    }
    const auto* names = reinterpret_cast<const MString*>(address::debug_render_names);
    return readable_c_string(names[index].guts) ? names[index].guts : nullptr;
}

s32 debug_render_get(u32 index) {
    if (!rel_context_ready() || index >= 51u) {
        return 0;
    }
    return function_at<s32 (*)(u32)>(address::debug_render_get_int)(index);
}

void debug_render_set(u32 index, s32 value) {
    if (rel_context_ready() && index < 51u) {
        function_at<void (*)(u32, s32)>(address::debug_render_set_int)(index, value);
    }
}

s32 debug_render_min(u32 index) {
    if (!rel_context_ready() || index >= 51u) {
        return 0;
    }
    return function_at<s32 (*)(u32)>(address::debug_render_get_min)(index);
}

s32 debug_render_max(u32 index) {
    if (!rel_context_ready() || index >= 51u) {
        return 1;
    }
    return function_at<s32 (*)(u32)>(address::debug_render_get_max)(index);
}

u32 ngl_debug_get(u32 native_index) {
    return native_index < address::ngl_debug_flag_count
               ? volatile_memory<u8>(address::ngl_debug_flags + native_index)
               : 0u;
}

void ngl_debug_set(u32 native_index, u32 value) {
    if (native_index >= address::ngl_debug_flag_count) {
        return;
    }
    volatile_memory<u8>(address::ngl_debug_flags + native_index) =
        static_cast<u8>(value & 0xFFu);
    // The retail renderer snapshots all 19 bytes into nglSyncDebug at its own
    // frame boundary.  Let that native copy preserve the renderer's ordering.
}

void save_game(s32 slot) {
    if (void* object = settings(); object != nullptr && rel_context_ready()) {
        function_at<void (*)(void*, bool)>(address::game_settings_reset_container)(object, false);
        function_at<void (*)(void*, s32)>(address::game_settings_save)(object, slot);
    }
}

void load_game(s32 slot) {
    if (void* object = settings(); object != nullptr && rel_context_ready()) {
        function_at<void (*)(void*, s32)>(address::game_settings_load_game)(object, slot);
    }
}

void auto_load_game() {
    if (void* object = settings(); object != nullptr && rel_context_ready()) {
        function_at<void (*)(void*)>(address::game_settings_load_most_recent)(object);
    }
}

void hires_screenshot() {
    s32 width = get_developer_int(66u);
    s32 height = get_developer_int(67u);
    if (width <= 0) {
        width = 1280;
    }
    if (height <= 0) {
        height = 896;
    }
    function_at<void (*)(s32, s32)>(address::ngl_begin_hires_screenshot)(width, height);
}

void lores_screenshot() {
    static char filename[] = "usm_debug.tga";
    function_at<void (*)(char*)>(address::ngl_screenshot)(filename);
}

u32 terrain_region_count() {
    if (!interactive_world_ready()) {
        return 0u;
    }
    void* object = terrain();
    if (object == nullptr) {
        return 0u;
    }
    const s32 count = *reinterpret_cast<volatile s32*>(bytes(object) + 0xCu);
    return count > 0 && count < 1024 ? static_cast<u32>(count) : 0u;
}

void* terrain_region(u32 index) {
    void* object = terrain();
    if (object == nullptr || index >= terrain_region_count()) {
        return nullptr;
    }
    void** regions = *reinterpret_cast<void***>(bytes(object));
    if (!is_main_ram_pointer(regions)) {
        return nullptr;
    }
    void* region = regions[index];
    return is_main_ram_pointer(region) ? region : nullptr;
}

const char* region_name(void* region) {
    if (!is_main_ram_pointer(region) || reinterpret_cast<uptr>(region) > 0x817FFFD0u) {
        return nullptr;
    }
    void* mash = *reinterpret_cast<void**>(bytes(region) + 0x2Cu);
    const char* name = reinterpret_cast<const char*>(mash);
    return readable_c_string(name) ? name : nullptr;
}

bool region_is_warp_target(void* region) {
    return is_main_ram_pointer(region) &&
           (*reinterpret_cast<volatile const u32*>(bytes(region) + 0x50u) & 0x4000u) != 0u;
}

s32 region_variant(void* region) {
    if (!is_main_ram_pointer(region)) {
        return 0;
    }
    return function_at<s32 (*)(void*)>(address::region_get_variant)(region);
}

s32 region_variant_count(void* region) {
    if (!is_main_ram_pointer(region)) {
        return 0;
    }
    const s32 count = *reinterpret_cast<volatile s32*>(bytes(region) + 0xC8u);
    return count >= 0 && count < 128 ? count : 0;
}

bool warp_to_point_of_interest() {
    if (!interactive_world_ready()) {
        return false;
    }
    void* igo = object_from_slot<void>(address::fe_igo_pointer);
    if (igo == nullptr) {
        return false;
    }
    void* tracker_slot = bytes(igo) + 0x54u;
    if (!is_main_ram_pointer(tracker_slot)) {
        return false;
    }
    void* tracker = *reinterpret_cast<void**>(tracker_slot);
    if (!is_main_ram_pointer(tracker)) {
        return false;
    }

    Vec3 destination{};
    const bool has_target =
        function_at<bool (*)(void*, Vec3*)>(address::entity_tracker_get_arrow_target)(
            tracker, &destination);
    if (!has_target) {
        return false;
    }
    void* world_object = world();
    if (world_object == nullptr) {
        return false;
    }
    function_at<void (*)(void*, Vec3*, s32, bool)>(address::world_malor_point)(
        world_object, &destination, 0, false);
    return true;
}

void warp_to_region(u32 index) {
    if (!interactive_world_ready()) {
        return;
    }
    void* region = terrain_region(index);
    void* world_object = world();
    if (region == nullptr || world_object == nullptr) {
        return;
    }
    const s32 flags = *reinterpret_cast<volatile s32*>(bytes(region) + 0x50u);
    if ((flags & 1) != 0) {
        const s32 district = *reinterpret_cast<volatile s32*>(bytes(region) + 0xC0u);
        function_at<void (*)(void*, s32)>(address::terrain_unlock_district)(terrain(), district);
    }
    auto* destination = reinterpret_cast<Vec3*>(bytes(region) + 0xA4u);
    function_at<void (*)(void*, Vec3*, s32, bool)>(address::world_malor_point)(
        world_object, destination, 0, false);
}

void set_region_variant(u32 index, s32 variant) {
    if (!interactive_world_ready()) {
        return;
    }
    void* region = terrain_region(index);
    void* terrain_object = terrain();
    if (region == nullptr || terrain_object == nullptr) {
        return;
    }
    const s32 district = *reinterpret_cast<volatile s32*>(bytes(region) + 0xC0u);
    function_at<void (*)(void*, s32, s32, bool)>(address::terrain_set_district_variant)(
        terrain_object, district, variant, true);
}

void unload_all_districts() {
    if (interactive_world_ready()) {
        void* object = terrain();
        if (object == nullptr) {
            return;
        }
        function_at<void (*)(void*)>(address::terrain_unload_all_districts)(object);
    }
}

bool set_district_streaming_active(bool active) {
    if (!interactive_world_ready()) {
        return false;
    }
    // The resource manager owns an array of partition pointers in its first
    // word. Validate both levels before asking for the district partition.
    void* manager = object_from_slot<void>(address::resource_manager_pointer);
    if (manager == nullptr ||
        !is_main_ram_pointer(*reinterpret_cast<void**>(bytes(manager)))) {
        return false;
    }

    void* partition =
        function_at<void* (*)(s32)>(address::resource_manager_get_partition)(6);
    if (!is_main_ram_pointer(partition)) {
        return false;
    }

    // resource_partition embeds its resource_pack_streamer at offset 0x18.
    void* streamer = bytes(partition) + 0x18u;
    if (!is_main_ram_pointer(streamer)) {
        return false;
    }
    function_at<void (*)(void*, bool)>(address::resource_pack_streamer_set_active)(
        streamer, active);
    return true;
}

void draw_panel(float left, float top, float right, float bottom, u32 color, float z) {
    alignas(32) u8 quad[100];
    function_at<void (*)(void*)>(address::ngl_init_quad)(quad);
    function_at<void (*)(void*, u32)>(address::ngl_set_quad_color)(quad, color);
    function_at<void (*)(void*, float, float, float, float)>(address::ngl_set_quad_rect)(
        quad, left, top, right, bottom);
    function_at<void (*)(void*, float)>(address::ngl_set_quad_z)(quad, z);
    function_at<void (*)(void*)>(address::ngl_add_quad)(quad);
}

void draw_text(const char* text, float x, float y, u32 color, float scale) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    void* font = memory<void*>(address::ngl_system_font_pointer);
    if (!is_main_ram_pointer(font)) {
        return;
    }
    using Function = void (*)(void*, const char*, float, float, float, u32, float, float);
    function_at<Function>(address::ngl_add_string)(font, text, x, y, 0.0f, color, scale, scale);
}

bool measure_text(const char* text, float scale, u32& width, u32& height) {
    width = 0u;
    height = 0u;
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    void* font = memory<void*>(address::ngl_system_font_pointer);
    if (!is_main_ram_pointer(font)) {
        return false;
    }
    using Function = void (*)(void*, const char*, u32*, u32*, float, float);
    function_at<Function>(address::ngl_get_string_dimensions)(
        font, text, &width, &height, scale, scale);
    if (width > 4096u || height == 0u || height > 256u) {
        width = 0u;
        height = 0u;
        return false;
    }
    return true;
}

void finish_scene() {
    function_at<void (*)()>(address::ngl_list_end_scene)();
}

void update_game_button(void* button, float time_inc) {
    function_at<void (*)(void*, float)>(address::game_button_update)(button, time_inc);
}

bool readable_c_string(const char* text) {
    const uptr pointer = reinterpret_cast<uptr>(text);
    // Authored strings can reside in a live ARAM-paged resource. Resolve its
    // owner once; this function never permits mutation of that resource.
    uptr end = 0u;
    if (!resource_memory::readable_extent(pointer, end)) {
        return false;
    }
    constexpr u32 maximum_length = 127u;
    const u32 available = end - pointer;
    const u32 limit = available < maximum_length + 1u ? available : maximum_length + 1u;
    for (u32 index = 0u; index < limit; ++index) {
        const u8 value = *reinterpret_cast<const volatile u8*>(text + index);
        if (value == 0u) {
            return index != 0u;
        }
        if (value < 0x20u || value >= 0x7Fu) {
            return false;
        }
    }
    return false;
}

}  // namespace usm::gc::engine

extern "C" void* memcpy(void* destination, const void* source, unsigned int size) {
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    for (unsigned int index = 0; index < size; ++index) {
        out[index] = in[index];
    }
    return destination;
}

extern "C" void* memset(void* destination, int value, unsigned int size) {
    auto* out = static_cast<unsigned char*>(destination);
    for (unsigned int index = 0; index < size; ++index) {
        out[index] = static_cast<unsigned char>(value);
    }
    return destination;
}

extern "C" int memcmp(const void* lhs, const void* rhs, unsigned int size) {
    const auto* left = static_cast<const unsigned char*>(lhs);
    const auto* right = static_cast<const unsigned char*>(rhs);
    for (unsigned int index = 0; index < size; ++index) {
        if (left[index] != right[index]) {
            return static_cast<int>(left[index]) - static_cast<int>(right[index]);
        }
    }
    return 0;
}

extern "C" unsigned int strlen(const char* value) {
    unsigned int size = 0;
    while (value != nullptr && value[size] != '\0') {
        ++size;
    }
    return size;
}
