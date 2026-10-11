#include "gc_input.h"

#include "gc_addresses.h"
#include "menus/replay_gc.h"

namespace usm::gc::input {
namespace {

constexpr u16 button_left = 0x0001u;
constexpr u16 button_right = 0x0002u;
constexpr u16 button_down = 0x0004u;
constexpr u16 button_up = 0x0008u;
constexpr u16 button_z = 0x0010u;
constexpr u16 button_r = 0x0020u;
constexpr u16 button_l = 0x0040u;
constexpr u16 button_a = 0x0100u;
constexpr u16 button_b = 0x0200u;
constexpr u16 button_x = 0x0400u;
constexpr u32 pad_count = 4u;
constexpr u32 no_channel = pad_count;
constexpr s8 pad_error_none = 0;
constexpr s8 pad_error_no_controller = -1;
constexpr s8 stick_press_threshold = 48;
constexpr s8 stick_release_threshold = 32;
constexpr u16 edge_mask =
    button_left | button_right | button_down | button_up | button_a | button_b;

u16 previous_buttons[pad_count];
u16 observed_buttons[pad_count];
u16 latest_buttons[pad_count];
u16 pending_pressed[pad_count];
u16 stick_directions[pad_count];
bool previous_toggle[pad_count];
bool pending_fast[pad_count];
bool latest_valid[pad_count];
u8 repeat_up;
u8 repeat_down;
u8 repeat_left;
u8 repeat_right;
PadStatus latest_pads[pad_count];
bool sample_pending;
bool toggle_pending;
u32 active_channel = no_channel;
u32 repeat_channel = no_channel;

bool toggle_down(u16 buttons) {
    return (buttons & button_z) != 0u;
}

void reset_repeat() {
    repeat_up = 0u;
    repeat_down = 0u;
    repeat_left = 0u;
    repeat_right = 0u;
}

u16 normalized_buttons(u32 channel, const PadStatus& pad) {
    u16 directions = stick_directions[channel];
    if (pad.stick_x <= -stick_press_threshold) {
        directions = static_cast<u16>((directions | button_left) & ~button_right);
    } else if (pad.stick_x >= stick_press_threshold) {
        directions = static_cast<u16>((directions | button_right) & ~button_left);
    } else if (pad.stick_x >= -stick_release_threshold &&
               pad.stick_x <= stick_release_threshold) {
        directions = static_cast<u16>(directions & ~(button_left | button_right));
    }
    if (pad.stick_y >= stick_press_threshold) {
        directions = static_cast<u16>((directions | button_up) & ~button_down);
    } else if (pad.stick_y <= -stick_press_threshold) {
        directions = static_cast<u16>((directions | button_down) & ~button_up);
    } else if (pad.stick_y >= -stick_release_threshold &&
               pad.stick_y <= stick_release_threshold) {
        directions = static_cast<u16>(directions & ~(button_up | button_down));
    }
    stick_directions[channel] = directions;
    return static_cast<u16>(pad.button | directions);
}

void observe_pads(const PadStatus* pads) {
    if (pads == nullptr) {
        return;
    }
    for (u32 channel = 0u; channel < pad_count; ++channel) {
        if (pads[channel].error == pad_error_none) {
            const u16 buttons = normalized_buttons(channel, pads[channel]);
            const u16 pressed = static_cast<u16>(
                buttons & static_cast<u16>(~observed_buttons[channel]));
            const bool toggle = toggle_down(pads[channel].button);
            pending_pressed[channel] = static_cast<u16>(
                pending_pressed[channel] | (pressed & edge_mask));
            if ((pressed & edge_mask) != 0u && (buttons & button_x) != 0u) {
                pending_fast[channel] = true;
            }
            if (toggle && !previous_toggle[channel] && !toggle_pending) {
                toggle_pending = true;
                active_channel = channel;
            }
            previous_toggle[channel] = toggle;
            observed_buttons[channel] = buttons;
            latest_buttons[channel] = buttons;
            latest_pads[channel] = pads[channel];
            latest_valid[channel] = true;
        } else if (pads[channel].error == pad_error_no_controller) {
            previous_toggle[channel] = false;
            observed_buttons[channel] = 0u;
            latest_buttons[channel] = 0u;
            pending_pressed[channel] = 0u;
            pending_fast[channel] = false;
            stick_directions[channel] = 0u;
            latest_pads[channel] = pads[channel];
            latest_valid[channel] = true;
        }
        // NOT_READY/TRANSFER are transient. Preserve edge state and active
        // ownership until the SDK reports a real disconnect or valid sample.
    }
    sample_pending = true;
}

bool repeated(u16 buttons, u16 pressed, u16 mask, u8& counter) {
    if ((pressed & mask) != 0u) {
        counter = 1u;
        return true;
    }
    if ((buttons & mask) == 0u) {
        counter = 0u;
        return false;
    }
    if (counter == 0u) {
        counter = 1u;
        return true;
    }
    if (counter < 21u) {
        ++counter;
    } else {
        counter = 18u;
    }
    return counter >= 18u && ((counter - 18u) & 3u) == 0u;
}

}  // namespace

u32 read_for_game(PadStatus* pads, bool menu_has_focus) {
    using PadRead = u32 (*)(PadStatus*);
    const u32 reset_mask = reinterpret_cast<PadRead>(address::pad_read)(pads);
    if (pads == nullptr) {
        return reset_mask;
    }

    observe_pads(pads);
    replay::process_game_pads(pads, menu_has_focus);

    for (u32 channel = 0u; channel < pad_count; ++channel) {
        PadStatus& pad = pads[channel];
        if (menu_has_focus) {
            const s8 error = pad.error;
            pad = PadStatus{};
            pad.error = error;
        } else {
            // Z belongs to the debug menu. START and both trigger buttons
            // retain their normal behavior while the menu is closed.
            pad.button = static_cast<u16>(
                pad.button & static_cast<u16>(~button_z));
        }
    }
    return reset_mask;
}

Frame poll() {
    if (!sample_pending) {
        PadStatus pads[pad_count]{};
        using PadRead = u32 (*)(PadStatus*);
        reinterpret_cast<PadRead>(address::pad_read)(pads);
        observe_pads(pads);
    }
    sample_pending = false;

    if (active_channel >= pad_count ||
        (latest_valid[active_channel] &&
         latest_pads[active_channel].error == pad_error_no_controller)) {
        active_channel = no_channel;
        for (u32 channel = 0u; channel < pad_count; ++channel) {
            if (latest_valid[channel] &&
                latest_pads[channel].error == pad_error_none) {
                active_channel = channel;
                break;
            }
        }
    }

    if (repeat_channel != active_channel) {
        reset_repeat();
        repeat_channel = active_channel;
    }

    const bool connected = active_channel < pad_count && latest_valid[active_channel] &&
                           latest_pads[active_channel].error == pad_error_none;
    u16 buttons = connected ? latest_buttons[active_channel] : 0u;
    const u16 old_buttons = connected ? previous_buttons[active_channel] : 0u;
    const u16 pressed = connected
                            ? static_cast<u16>(
                                  (buttons & static_cast<u16>(~old_buttons)) |
                                  pending_pressed[active_channel])
                            : 0u;
    if (connected && pending_fast[active_channel]) {
        buttons = static_cast<u16>(buttons | button_x);
    }
    const bool toggle = toggle_down(buttons);

    Frame frame{};
    frame.toggle_menu = toggle_pending;
    toggle_pending = false;
    frame.fast = (buttons & button_x) != 0u;
    frame.connected = connected;

    if (!toggle && !frame.toggle_menu) {
        frame.up = repeated(buttons, pressed, button_up, repeat_up);
        frame.down = repeated(buttons, pressed, button_down, repeat_down);
        frame.left = repeated(buttons, pressed, button_left, repeat_left);
        frame.right = repeated(buttons, pressed, button_right, repeat_right);
        frame.accept = (pressed & button_a) != 0u;
        frame.cancel = (pressed & button_b) != 0u;
    } else {
        reset_repeat();
    }

    for (u32 channel = 0u; channel < pad_count; ++channel) {
        if (latest_valid[channel] && latest_pads[channel].error == pad_error_none) {
            previous_buttons[channel] = latest_buttons[channel];
        } else if (latest_valid[channel] &&
                   latest_pads[channel].error == pad_error_no_controller) {
            previous_buttons[channel] = 0u;
        }
        pending_pressed[channel] = 0u;
        pending_fast[channel] = false;
    }
    if (!connected) {
        reset_repeat();
    }
    return frame;
}

bool connected(u32 channel) {
    return channel < pad_count && latest_valid[channel] &&
           latest_pads[channel].error == pad_error_none;
}

}  // namespace usm::gc::input
