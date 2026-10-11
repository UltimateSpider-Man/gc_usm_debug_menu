#include "ngl_features_gc.h"

#include "../debug_menu_core.h"
#include "../gc_engine.h"

namespace usm::gc::ngl_features {
namespace {

constexpr u32 history_capacity = 120u;
constexpr uptr native_cpu_timing = 0x803D8E6Cu;
float history[history_capacity];
u32 history_count;
u32 history_next;
s32 info_mode;
s32 graph_enabled;

void append_timing(char* output, u32 capacity, const char* label, float value) {
    menu::append_text(output, capacity, label);
    char number[24];
    menu::format_float(number, sizeof(number), value);
    menu::append_text(output, capacity, number);
    menu::append_text(output, capacity, " ms");
}

}  // namespace

bool handles(u32 catalog_index) {
    return catalog_index == 1u || catalog_index == 19u;
}

s32 get(u32 catalog_index) {
    return catalog_index == 1u ? info_mode : catalog_index == 19u ? graph_enabled : 0;
}

void set(u32 catalog_index, s32 value) {
    if (catalog_index == 1u) {
        info_mode = value < 0 ? 0 : value > 2 ? 2 : value;
    } else if (catalog_index == 19u) {
        graph_enabled = value != 0 ? 1 : 0;
    }
}

void render() {
    if (info_mode == 0 && graph_enabled == 0) {
        return;
    }
    // nglListSend's 0x802823BC..0x802823F8 calculates elapsed ticks,
    // converts to milliseconds, then stores at nglPerfInfo+0xB4. Unlike
    // guessed PC struct offsets, this float has a verified native producer.
    const float timing = volatile_memory<float>(native_cpu_timing);
    if (!(timing >= 0.0f && timing <= 1000.0f)) {
        return;
    }
    history[history_next] = timing;
    history_next = (history_next + 1u) % history_capacity;
    if (history_count < history_capacity) {
        ++history_count;
    }

    float total = 0.0f;
    float peak = 0.0f;
    for (u32 index = 0u; index < history_count; ++index) {
        const float value = history[index];
        total += value;
        if (value > peak) {
            peak = value;
        }
    }
    constexpr float left = 358.0f;
    constexpr float right = 614.0f;
    float graph_top = 42.0f;
    if (info_mode != 0) {
        const float height = info_mode == 2 ? 43.0f : 25.0f;
        engine::draw_panel(left, 12.0f, right, 12.0f + height, 0x101010D0u);
        char text[100];
        menu::copy_text(text, sizeof(text), "");
        append_timing(text, sizeof(text), "NGL CPU: ", timing);
        engine::draw_text(text, left + 6.0f, 16.0f, 0xFFFFFFFFu, 0.60f);
        if (info_mode == 2) {
            menu::copy_text(text, sizeof(text), "");
            append_timing(text, sizeof(text), "Avg ", total / static_cast<float>(history_count));
            append_timing(text, sizeof(text), "  Peak ", peak);
            engine::draw_text(text, left + 6.0f, 32.0f, 0xFFFF00FFu, 0.52f);
        }
        graph_top = 16.0f + height;
    }
    if (graph_enabled == 0) {
        return;
    }
    constexpr float graph_height = 80.0f;
    constexpr float milliseconds_ceiling = 50.0f;
    const float baseline = graph_top + graph_height;
    engine::draw_panel(left, graph_top, right, baseline + 20.0f, 0x101010D0u);
    const float target_line = baseline - graph_height * (16.667f / milliseconds_ceiling);
    engine::draw_panel(left + 6.0f, target_line, right - 6.0f, target_line + 1.0f,
                       0x50505080u);
    for (u32 index = 0u; index < history_count; ++index) {
        const u32 oldest = history_count == history_capacity ? history_next : 0u;
        const float value = history[(oldest + index) % history_capacity];
        const float clipped = value < milliseconds_ceiling ? value : milliseconds_ceiling;
        const float x = left + 6.0f + static_cast<float>(index) * 2.0f;
        const float top = baseline - clipped * (graph_height / milliseconds_ceiling);
        const u32 color = value <= 16.667f ? 0x40E040FFu
                            : value <= 33.333f ? 0xFFFF00FFu : 0xFF4040FFu;
        engine::draw_panel(x, top, x + 1.5f, baseline, color);
    }
    engine::draw_text("NGL CPU: 120 frames / 50 ms", left + 6.0f, baseline + 3.0f,
                      0xFFFFFFFFu, 0.50f);
}

}  // namespace usm::gc::ngl_features
