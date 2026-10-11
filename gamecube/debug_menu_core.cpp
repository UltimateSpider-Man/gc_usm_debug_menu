#include "debug_menu_core.h"

#include "gc_engine.h"

namespace usm::gc::menu {
namespace {

constexpr u32 maximum_menus = 64u;
constexpr u32 maximum_entries = 5120u;
constexpr u32 maximum_deferred_commands = 16u;
constexpr u16 visible_rows = 18u;
constexpr float menu_x = 25.0f;
constexpr float menu_y = 45.0f;
constexpr float menu_scale = 1.0f;
constexpr u32 maximum_line_width = 600u;
constexpr u32 fallback_character_width = 8u;
constexpr u32 fallback_line_height = 18u;
constexpr u32 render_line_capacity = 160u;
constexpr u32 panel_color = 0x141414C8u;   // GC NGL uses RRGGBBAA.
constexpr u32 title_color = 0xFFFF00FFu;
constexpr u32 selected_color = 0x00DC00FFu;
constexpr u32 normal_color = 0xDCDCDCFFu;
constexpr u32 arrow_color = 0xFF00FFFFu;
constexpr u32 status_color = 0xFF9090FFu;
constexpr const char* up_arrow = " ^ ^ ^ ";
constexpr const char* down_arrow = " v v v ";

struct DeferredCommand {
    Entry entry;
    Handler handler;
    Command command;
    s32 multiplier;
};

Menu menu_pool[maximum_menus];
Entry entry_pool[maximum_entries];
u32 used_menus;
u32 used_entries;
Menu* root_menu;
Menu* current_menu;
bool menu_visible;
bool paused_by_menu;
bool pause_pending;
bool unpause_pending;
bool interaction_locked;
char status_text[112];
u16 status_frames;
s32 command_multiplier = 1;
DeferredCommand deferred_commands[maximum_deferred_commands];
u8 deferred_head;
u8 deferred_count;
char rendered_title[render_line_capacity];
char rendered_status[render_line_capacity];
char rendered_entries[visible_rows][render_line_capacity];

u32 text_length(const char* value) {
    u32 length = 0u;
    if (value != nullptr) {
        while (value[length] != '\0') {
            ++length;
        }
    }
    return length;
}

void normalize(Menu& value) {
    if (value.count == 0u) {
        value.selected = 0u;
        value.scroll = 0u;
        return;
    }
    if (value.selected >= value.count) {
        value.selected = static_cast<u16>(value.count - 1u);
    }
    if (value.selected < value.scroll) {
        value.scroll = value.selected;
    }
    if (value.selected >= value.scroll + visible_rows) {
        value.scroll = static_cast<u16>(value.selected - visible_rows + 1u);
    }
}

void refresh(Menu& value) {
    if (value.refresh != nullptr) {
        value.refresh(value);
    }
    normalize(value);
}

void show_menu() {
    if (root_menu == nullptr) {
        return;
    }
    current_menu = root_menu;
    menu_visible = true;
    paused_by_menu = false;
    // Front-end frames have no gameplay pause to acquire. If gameplay starts
    // while the menu remains open, dispatch_deferred() arms the pause later.
    pause_pending = engine::game_state() == 6;
}

void queue_handler(const Entry& entry, Command command, s32 multiplier) {
    if (entry.handler == nullptr) {
        return;
    }
    if (deferred_count >= maximum_deferred_commands) {
        set_status("Deferred command queue is full");
        return;
    }
    const u32 slot =
        (static_cast<u32>(deferred_head) + deferred_count) % maximum_deferred_commands;
    DeferredCommand& deferred = deferred_commands[slot];
    deferred.entry = entry;
    deferred.handler = entry.handler;
    deferred.command = command;
    deferred.multiplier = multiplier;
    ++deferred_count;
}

void change_integer(Entry& entry, s32 direction, s32 multiplier) {
    if (entry.target == nullptr) {
        return;
    }
    auto* value = static_cast<volatile s32*>(entry.target);
    long long next = static_cast<long long>(*value) +
                     static_cast<long long>(direction) * entry.step * multiplier;
    if (next < entry.minimum) {
        next = entry.minimum;
    }
    if (next > entry.maximum) {
        next = entry.maximum;
    }
    *value = static_cast<s32>(next);
}

void change_float(Entry& entry, s32 direction, s32 multiplier) {
    if (entry.target == nullptr) {
        return;
    }
    auto* value = static_cast<volatile float*>(entry.target);
    float next = *value + static_cast<float>(direction * multiplier) * entry.float_step;
    if (next < entry.float_minimum) {
        next = entry.float_minimum;
    }
    if (next > entry.float_maximum) {
        next = entry.float_maximum;
    }
    *value = next;
}

void change_scalar(Entry& entry, Command command, s32 multiplier) {
    const s32 direction = command == Command::decrement ? -1 : 1;
    switch (entry.type) {
    case EntryType::boolean8:
        if (entry.target != nullptr) {
            auto* value = static_cast<volatile u8*>(entry.target);
            *value = *value == 0u ? 1u : 0u;
        }
        break;
    case EntryType::boolean32:
        if (entry.target != nullptr) {
            auto* value = static_cast<volatile u32*>(entry.target);
            *value = *value == 0u ? 1u : 0u;
        }
        break;
    case EntryType::integer32:
    case EntryType::choice:
        if (command != Command::activate) {
            change_integer(entry, direction, multiplier);
        }
        break;
    case EntryType::float32:
        if (command != Command::activate) {
            change_float(entry, direction, multiplier);
        }
        break;
    default:
        break;
    }
}

void execute(Entry& entry, Command command, bool fast) {
    const s32 multiplier = fast ? 10 : 1;
    command_multiplier = multiplier;
    switch (entry.type) {
    case EntryType::submenu:
        if (command == Command::activate && entry.submenu != nullptr) {
            entry.submenu->parent = current_menu;
            current_menu = entry.submenu;
        }
        break;
    case EntryType::action:
    case EntryType::custom:
        queue_handler(entry, command, multiplier);
        break;
    case EntryType::integer32:
    case EntryType::choice:
    case EntryType::float32:
        if (command == Command::activate) {
            break;
        }
        [[fallthrough]];
    case EntryType::boolean8:
    case EntryType::boolean32:
        if (entry.target != nullptr) {
            if (entry.handler != nullptr) {
                // Keep the value change and its engine callback together.
                // A close or transaction lock can cancel later commands
                // without leaving their scalar writes behind in the render.
                queue_handler(entry, command, multiplier);
            } else {
                change_scalar(entry, command, multiplier);
            }
        }
        break;
    case EntryType::read_only:
        break;
    }
    command_multiplier = 1;
}

void render_value(const Entry& entry, char* output, u32 capacity) {
    output[0] = '\0';
    if (entry.formatter != nullptr) {
        entry.formatter(entry, output, capacity);
        return;
    }
    switch (entry.type) {
    case EntryType::submenu:
        copy_text(output, capacity, ">");
        break;
    case EntryType::boolean8:
        copy_text(output, capacity,
                  entry.target != nullptr && *static_cast<volatile u8*>(entry.target) != 0u
                      ? "ON"
                      : "OFF");
        break;
    case EntryType::boolean32:
        copy_text(output, capacity,
                  entry.target != nullptr && *static_cast<volatile u32*>(entry.target) != 0u
                      ? "ON"
                      : "OFF");
        break;
    case EntryType::integer32:
    case EntryType::choice:
        if (entry.target != nullptr) {
            format_s32(output, capacity, *static_cast<volatile s32*>(entry.target));
        }
        break;
    case EntryType::float32:
        if (entry.target != nullptr) {
            format_float(output, capacity, *static_cast<volatile float*>(entry.target));
        }
        break;
    case EntryType::action:
    case EntryType::custom:
    case EntryType::read_only:
        break;
    }
}

void compose_entry_line(const Entry& entry, char* output, u32 capacity) {
    copy_text(output, capacity, entry.label != nullptr ? entry.label : "(unnamed)");
    char value[64];
    render_value(entry, value, sizeof(value));
    if (value[0] != '\0') {
        append_text(output, capacity, ": ");
        append_text(output, capacity, value);
    }
}

void dimensions_or_fallback(const char* text, u32& width, u32& height) {
    if (!engine::measure_text(text, menu_scale, width, height)) {
        width = text_length(text) * fallback_character_width;
        height = fallback_line_height;
    }
}

void fit_line(char* text, u32 maximum_width, u32& width, u32& height) {
    dimensions_or_fallback(text, width, height);
    u32 length = text_length(text);
    while (width > maximum_width && length > 3u) {
        --length;
        text[length - 3u] = '.';
        text[length - 2u] = '.';
        text[length - 1u] = '.';
        text[length] = '\0';
        dimensions_or_fallback(text, width, height);
    }
}

}  // namespace

void reset() {
    memset(menu_pool, 0, sizeof(menu_pool));
    memset(entry_pool, 0, sizeof(entry_pool));
    used_menus = 0u;
    used_entries = 0u;
    root_menu = nullptr;
    current_menu = nullptr;
    menu_visible = false;
    paused_by_menu = false;
    pause_pending = false;
    unpause_pending = false;
    interaction_locked = false;
    status_text[0] = '\0';
    status_frames = 0u;
    command_multiplier = 1;
    deferred_head = 0u;
    deferred_count = 0u;
    memset(deferred_commands, 0, sizeof(deferred_commands));
}

Menu* create(const char* title, u16 capacity, Refresh refresh_callback) {
    if (used_menus >= maximum_menus || used_entries + capacity > maximum_entries) {
        set_status("Menu pool exhausted");
        return nullptr;
    }
    Menu* value = &menu_pool[used_menus++];
    memset(value, 0, sizeof(*value));
    value->title = title;
    value->entries = &entry_pool[used_entries];
    value->capacity = capacity;
    value->refresh = refresh_callback;
    memset(value->entries, 0, sizeof(Entry) * capacity);
    used_entries += capacity;
    return value;
}

Entry* add(Menu& destination, const char* label, EntryType type) {
    if (destination.count >= destination.capacity) {
        set_status("Menu capacity exhausted");
        return nullptr;
    }
    Entry* entry = &destination.entries[destination.count++];
    memset(entry, 0, sizeof(*entry));
    entry->label = label;
    entry->type = type;
    entry->minimum = -1000;
    entry->maximum = 1000;
    entry->step = 1;
    entry->float_minimum = -1000.0f;
    entry->float_maximum = 1000.0f;
    entry->float_step = 0.1f;
    return entry;
}

Entry* add_submenu(Menu& destination, Menu& submenu) {
    Entry* entry = add(destination, submenu.title, EntryType::submenu);
    if (entry != nullptr) {
        entry->submenu = &submenu;
        submenu.parent = &destination;
    }
    return entry;
}

void set_root(Menu& root) {
    root_menu = &root;
    current_menu = &root;
}

Menu* current() {
    return current_menu;
}

bool visible() {
    return menu_visible;
}

bool owns_input() {
    return menu_visible || paused_by_menu || pause_pending || unpause_pending;
}

void set_interaction_locked(bool locked) {
    interaction_locked = locked;
    if (locked) {
        // A queued remove/add transaction owns the world from this point.
        // Later input from the same render batch must not mutate that world.
        deferred_head = 0u;
        deferred_count = 0u;
    }
}

bool interaction_is_locked() {
    return interaction_locked;
}

void close() {
    if (interaction_locked) {
        set_status("Operation in progress; wait for safe completion");
        return;
    }
    menu_visible = false;
    // Closing before dispatch cancels pending input. A handler which closes
    // after starting a load/warp must also cancel the rest of that batch.
    deferred_head = 0u;
    deferred_count = 0u;
    current_menu = root_menu;
    status_text[0] = '\0';
    status_frames = 0u;
    pause_pending = false;
    if (paused_by_menu) {
        unpause_pending = true;
    }
}

void update(const input::Frame& frame) {
    if (status_frames != 0u) {
        --status_frames;
    }
    if (interaction_locked) {
        return;
    }
    if (unpause_pending) {
        // close() can run from the update hook (notably after Hero Select).
        // Ignore the cached chord until that pending unpause has completed;
        // otherwise show_menu() could cancel pause ownership in the same frame.
        return;
    }
    if (pause_pending) {
        // A failed stock pause must not leave engine-mutating menu actions
        // available while gameplay is still advancing.  The opening chord or
        // B at the root can still dismiss the menu safely.
        if (frame.toggle_menu ||
            (frame.cancel && current_menu != nullptr && current_menu->parent == nullptr)) {
            close();
        }
        return;
    }
    if (frame.toggle_menu) {
        if (menu_visible) {
            close();
        } else {
            show_menu();
        }
        return;
    }
    if (!menu_visible || current_menu == nullptr) {
        return;
    }

    if (current_menu->count == 0u) {
        if (frame.cancel && current_menu->parent != nullptr) {
            current_menu = current_menu->parent;
        }
        return;
    }

    if (frame.up) {
        current_menu->selected = current_menu->selected == 0u
                                     ? static_cast<u16>(current_menu->count - 1u)
                                     : static_cast<u16>(current_menu->selected - 1u);
    }
    if (frame.down) {
        current_menu->selected = static_cast<u16>((current_menu->selected + 1u) % current_menu->count);
    }
    normalize(*current_menu);

    if (frame.cancel) {
        if (current_menu->parent != nullptr) {
            current_menu = current_menu->parent;
        } else {
            close();
        }
        return;
    }

    Entry& entry = current_menu->entries[current_menu->selected];
    if (frame.left) {
        execute(entry, Command::decrement, frame.fast);
    }
    if (frame.right) {
        execute(entry, Command::increment, frame.fast);
    }
    if (frame.accept) {
        execute(entry, Command::activate, frame.fast);
    }
    if (entry.handler != nullptr) {
        entry.handler(entry, Command::frame);
    }
}

void dispatch_deferred() {
    if (menu_visible && !paused_by_menu && !pause_pending &&
        engine::game_state() == 6) {
        pause_pending = true;
    }

    if (pause_pending) {
        if (!menu_visible) {
            pause_pending = false;
        } else if (engine::game_state() == 6) {
            if (!engine::game_paused()) {
                engine::pause_game();
            }
            if (engine::game_paused() && engine::game_state() == 7) {
                pause_pending = false;
                paused_by_menu = true;
            } else {
                set_status("Waiting for the game to accept pause...");
            }
        } else {
            // Front-end/loading states do not need a gameplay pause.  If the
            // same visible menu later enters RUNNING, the check above rearms.
            pause_pending = false;
        }
    }

    if (pause_pending) {
        return;
    }

    while (deferred_count != 0u) {
        DeferredCommand deferred = deferred_commands[deferred_head];
        deferred_head = static_cast<u8>(
            (static_cast<u32>(deferred_head) + 1u) % maximum_deferred_commands);
        --deferred_count;
        if (deferred.handler != nullptr) {
            command_multiplier = deferred.multiplier;
            change_scalar(deferred.entry, deferred.command, deferred.multiplier);
            deferred.handler(deferred.entry, deferred.command);
            command_multiplier = 1;
        }
    }

    if (unpause_pending) {
        // The stock pause UI can activate a dialog while our pause is held.
        // Its native Deactivate path restores UI state and permits unpause,
        // matching the PC debug menu. Never dismiss a pause owned elsewhere.
        if (paused_by_menu) engine::dismiss_debug_pause_menu();
        if (engine::game_paused()) engine::unpause_game();
        if (!engine::game_paused()) {
            unpause_pending = false;
            paused_by_menu = false;
        } else {
            // A stock IGO/dialog can reject unpause.  The world is still
            // paused and the Hero transaction is already safe here, so hand
            // control back to the native pause UI instead of trapping input.
            unpause_pending = false;
            paused_by_menu = false;
            menu_visible = false;
            current_menu = root_menu;
            set_status("Game refused unpause; input returned to pause UI");
        }
    }

    if (current_menu != nullptr) {
        refresh(*current_menu);
    }
}

void render() {
    if (!menu_visible || current_menu == nullptr) {
        return;
    }
    const u16 end = current_menu->count < current_menu->scroll + visible_rows
                        ? current_menu->count
                        : static_cast<u16>(current_menu->scroll + visible_rows);
    const bool show_up_arrow = current_menu->scroll != 0u;
    const bool show_down_arrow = current_menu->scroll + visible_rows < current_menu->count;
    const bool show_status = status_frames != 0u && status_text[0] != '\0';

    u32 maximum_width = 0u;
    u32 line_height = 0u;
    u32 line_count = 1u + (show_up_arrow ? 1u : 0u) +
                     static_cast<u32>(end - current_menu->scroll) +
                     (show_down_arrow ? 1u : 0u) + (show_status ? 1u : 0u);
    u32 width = 0u;
    u32 height = 0u;

    copy_text(rendered_title, sizeof(rendered_title),
              current_menu->title != nullptr ? current_menu->title : "Menu");
    fit_line(rendered_title, maximum_line_width, width, height);
    maximum_width = width;
    line_height = height;
    if (show_up_arrow) {
        dimensions_or_fallback(up_arrow, width, height);
        if (width > maximum_width) {
            maximum_width = width;
        }
        if (height > line_height) {
            line_height = height;
        }
    }
    for (u16 index = current_menu->scroll; index < end; ++index) {
        const Entry& entry = current_menu->entries[index];
        char* line = rendered_entries[index - current_menu->scroll];
        compose_entry_line(entry, line, render_line_capacity);
        fit_line(line, maximum_line_width, width, height);
        if (width > maximum_width) {
            maximum_width = width;
        }
        if (height > line_height) {
            line_height = height;
        }
    }
    if (show_down_arrow) {
        dimensions_or_fallback(down_arrow, width, height);
        if (width > maximum_width) {
            maximum_width = width;
        }
        if (height > line_height) {
            line_height = height;
        }
    }
    if (show_status) {
        copy_text(rendered_status, sizeof(rendered_status), status_text);
        fit_line(rendered_status, maximum_line_width, width, height);
        if (width > maximum_width) {
            maximum_width = width;
        }
        if (height > line_height) {
            line_height = height;
        }
    }
    if (line_height == 0u) {
        line_height = fallback_line_height;
    }

    float panel_right = menu_x + static_cast<float>(maximum_width) + 10.0f;
    if (panel_right > 635.0f) {
        panel_right = 635.0f;
    }
    float panel_bottom = menu_y + static_cast<float>(line_height * line_count) + 10.0f;
    if (panel_bottom > 475.0f) {
        panel_bottom = 475.0f;
    }
    engine::draw_panel(menu_x - 5.0f, menu_y - 5.0f, panel_right, panel_bottom,
                       panel_color, 0.5f);

    float y = menu_y;
    engine::draw_text(rendered_title, menu_x, y, title_color, menu_scale);
    y += static_cast<float>(line_height);

    if (show_up_arrow) {
        engine::draw_text(up_arrow, menu_x, y, arrow_color, menu_scale);
        y += static_cast<float>(line_height);
    }
    for (u16 index = current_menu->scroll; index < end; ++index) {
        const char* line = rendered_entries[index - current_menu->scroll];
        engine::draw_text(line, menu_x, y,
                          index == current_menu->selected ? selected_color : normal_color,
                          menu_scale);
        y += static_cast<float>(line_height);
    }
    if (show_down_arrow) {
        engine::draw_text(down_arrow, menu_x, y, arrow_color, menu_scale);
        y += static_cast<float>(line_height);
    }
    if (show_status) {
        engine::draw_text(rendered_status, menu_x, y, status_color, menu_scale);
    }
}

void clear_entries(Menu& value) {
    value.count = 0u;
    value.selected = 0u;
    value.scroll = 0u;
    memset(value.entries, 0, sizeof(Entry) * value.capacity);
}

void set_status(const char* text, u16 frames) {
    copy_text(status_text, sizeof(status_text), text);
    status_frames = frames;
}

s32 change_multiplier() {
    return command_multiplier;
}

void copy_text(char* output, u32 capacity, const char* value) {
    if (output == nullptr || capacity == 0u) {
        return;
    }
    u32 index = 0u;
    if (value != nullptr) {
        while (index + 1u < capacity && value[index] != '\0') {
            output[index] = value[index];
            ++index;
        }
    }
    output[index] = '\0';
}

void append_text(char* output, u32 capacity, const char* value) {
    if (output == nullptr || capacity == 0u || value == nullptr) {
        return;
    }
    u32 index = text_length(output);
    u32 source = 0u;
    while (index + 1u < capacity && value[source] != '\0') {
        output[index++] = value[source++];
    }
    output[index] = '\0';
}

void format_s32(char* output, u32 capacity, s32 value) {
    if (output == nullptr || capacity == 0u) {
        return;
    }
    char reversed[16];
    u32 count = 0u;
    const bool negative = value < 0;
    u32 magnitude = negative ? static_cast<u32>(-(value + 1)) + 1u : static_cast<u32>(value);
    do {
        reversed[count++] = static_cast<char>('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0u && count < sizeof(reversed));

    u32 index = 0u;
    if (negative && index + 1u < capacity) {
        output[index++] = '-';
    }
    while (count != 0u && index + 1u < capacity) {
        output[index++] = reversed[--count];
    }
    output[index] = '\0';
}

void format_float(char* output, u32 capacity, float value) {
    u32 bits;
    memcpy(&bits, &value, sizeof(bits));
    if ((bits & 0x7F800000u) == 0x7F800000u) {
        copy_text(output, capacity, (bits & 0x007FFFFFu) != 0u
                                       ? "NaN" : (bits >> 31u) != 0u ? "-INF" : "INF");
        return;
    }
    const bool negative = value < 0.0f;
    if (negative) {
        value = -value;
    }
    s32 exponent = 0;
    if (value >= 100000000.0f) {
        do {
            value /= 10.0f;
            ++exponent;
        } while (value >= 10.0f);
    }
    const s32 scaled = static_cast<s32>(value * 10.0f + 0.5f);
    const s32 whole = scaled / 10;
    const s32 fraction = scaled % 10;
    char number[20];
    format_s32(number, sizeof(number), whole);
    copy_text(output, capacity, negative ? "-" : "");
    append_text(output, capacity, number);
    append_text(output, capacity, ".");
    number[0] = static_cast<char>('0' + fraction);
    number[1] = '\0';
    append_text(output, capacity, number);
    if (exponent != 0) {
        append_text(output, capacity, "e");
        format_s32(number, sizeof(number), exponent);
        append_text(output, capacity, number);
    }
}

}  // namespace usm::gc::menu
