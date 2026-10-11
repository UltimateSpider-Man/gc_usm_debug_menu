#pragma once

#include "gc_input.h"
#include "gc_types.h"

namespace usm::gc::menu {

struct Entry;
struct Menu;

enum class Command : u8 {
    activate,
    decrement,
    increment,
    frame,
};

enum class EntryType : u8 {
    submenu,
    action,
    boolean8,
    boolean32,
    integer32,
    float32,
    choice,
    custom,
    read_only,
};

using Handler = void (*)(Entry&, Command);
using Formatter = void (*)(const Entry&, char*, u32);
using Refresh = void (*)(Menu&);

struct Entry {
    const char* label;
    Menu* submenu;
    void* target;
    Handler handler;
    Formatter formatter;
    s32 minimum;
    s32 maximum;
    s32 step;
    float float_minimum;
    float float_maximum;
    float float_step;
    u32 id;
    EntryType type;
    u8 flags;
    u16 reserved;
};

struct Menu {
    const char* title;
    Entry* entries;
    Menu* parent;
    Refresh refresh;
    u16 count;
    u16 capacity;
    u16 selected;
    u16 scroll;
};

void reset();
Menu* create(const char* title, u16 capacity, Refresh refresh = nullptr);
Entry* add(Menu& destination, const char* label, EntryType type);
Entry* add_submenu(Menu& destination, Menu& submenu);

void set_root(Menu& root);
Menu* current();
bool visible();
// True while game-facing PAD input must remain neutralized.  This extends
// beyond the final visible frame until the deferred pause/unpause transition
// has completed in the game-update hook.
bool owns_input();
// Prevent navigation, execution and closing while an engine transaction would
// leave the paused world in an unsafe intermediate state.  Intended for short,
// update-hook-owned flows.
void set_interaction_locked(bool locked);
bool interaction_is_locked();
void close();
void update(const input::Frame& input);
void render();

// Run queued engine-facing handlers and refresh the active dynamic menu from
// the game update hook.  Input/navigation and NGL drawing stay in render_ui.
void dispatch_deferred();

void clear_entries(Menu& menu);
void set_status(const char* text, u16 frames = 180u);
s32 change_multiplier();

void format_s32(char* output, u32 capacity, s32 value);
void format_float(char* output, u32 capacity, float value);
void copy_text(char* output, u32 capacity, const char* value);
void append_text(char* output, u32 capacity, const char* value);

}  // namespace usm::gc::menu
