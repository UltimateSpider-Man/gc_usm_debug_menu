#include "ai_export_gc.h"

#include "debug_menu_core.h"

extern "C" {
struct UsmAiMailbox {
    usm::gc::u32 magic;
    usm::gc::u32 version;
    volatile usm::gc::u32 frame;
    volatile usm::gc::u32 host_frame;
    volatile usm::gc::u32 request;
    volatile usm::gc::u32 acknowledged;
    volatile usm::gc::u32 result;
    volatile usm::gc::u32 length;
    char data[65536];
};
static_assert(sizeof(UsmAiMailbox) == 65568u);
static_assert(__builtin_offsetof(UsmAiMailbox, frame) == 8u);
static_assert(__builtin_offsetof(UsmAiMailbox, host_frame) == 12u);
static_assert(__builtin_offsetof(UsmAiMailbox, request) == 16u);
static_assert(__builtin_offsetof(UsmAiMailbox, acknowledged) == 20u);
static_assert(__builtin_offsetof(UsmAiMailbox, result) == 24u);
static_assert(__builtin_offsetof(UsmAiMailbox, length) == 28u);
static_assert(__builtin_offsetof(UsmAiMailbox, data) == 32u);

// Native PowerPC stores these words big-endian. The host owns heartbeat/ack
// and the completion result; request publication resets result to pending.
__attribute__((used)) UsmAiMailbox usm_gc_ai_mailbox = {
    0x55534D41u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, {0}
};
}

namespace usm::gc::ai_export {
namespace {
constexpr u32 heartbeat_frames = 300u;
constexpr u32 timeout_frames = 600u;
u32 request_sequence;
u32 pending_request;
u32 pending_frames;
bool serializing;

void barrier() {
#if defined(__powerpc__)
    asm volatile("sync" ::: "memory");
#else
    asm volatile("" ::: "memory");
#endif
}

bool host_connected() {
    const u32 seen = usm_gc_ai_mailbox.host_frame;
    return seen != 0u && usm_gc_ai_mailbox.frame - seen < heartbeat_frames;
}

void complete(const char* status, bool cancel) {
    pending_request = 0u;
    pending_frames = 0u;
    if (cancel) {
        // Retire an unacknowledged snapshot without reusing its sequence.
        // A host already saving its copied bytes may still finish that file.
        barrier();
        usm_gc_ai_mailbox.request = 0u;
    }
    menu::set_status(status);
}
}  // namespace

bool request_export(Serializer serializer) {
    if (pending_request != 0u || serializing) {
        menu::set_status("AI export is already waiting for the companion");
        return false;
    }
    if (!host_connected()) {
        menu::set_status("Run the Dolphin companion before exporting AI");
        return false;
    }
    if (serializer == nullptr) {
        menu::set_status("AI export has no serializer");
        return false;
    }
    u32 length = 0u;
    serializing = true;
    // The host can retry a completed request. Retire its header before any
    // serializer writes, so its two snapshot reads cannot accept mixed data.
    usm_gc_ai_mailbox.request = 0u;
    barrier();
    const bool serialized = serializer(usm_gc_ai_mailbox.data,
                                       sizeof(usm_gc_ai_mailbox.data), length);
    serializing = false;
    if (!serialized) {
        menu::set_status("AI export could not serialize this block");
        return false;
    }
    if (length == 0u || length > sizeof(usm_gc_ai_mailbox.data)) {
        menu::set_status("AI export data is empty or exceeds 65536 bytes");
        return false;
    }
    ++request_sequence;
    if (request_sequence == 0u) ++request_sequence;
    pending_request = request_sequence;
    pending_frames = 0u;
    usm_gc_ai_mailbox.result = 0u;
    usm_gc_ai_mailbox.length = length;
    // The host cannot observe a new request until all snapshot bytes and
    // metadata are published. No pending path below writes snapshot data.
    barrier();
    usm_gc_ai_mailbox.request = pending_request;
    menu::set_status("Saving AI block...", 600u);
    return true;
}

void frame_advance() {
    ++usm_gc_ai_mailbox.frame;
    if (usm_gc_ai_mailbox.frame == 0u) ++usm_gc_ai_mailbox.frame;
    if (pending_request == 0u) return;

    if (usm_gc_ai_mailbox.acknowledged == pending_request) {
        barrier();
        const u32 result = usm_gc_ai_mailbox.result;
        if (result == 1u) {
            complete("AI export saved", false);
            return;
        }
        if (result == 2u) {
            complete("AI export failed; companion could not save file", false);
            return;
        }
    }
    if (!host_connected()) {
        complete("AI export canceled; companion disconnected", true);
    } else if (++pending_frames >= timeout_frames) {
        complete("AI export timed out; snapshot canceled", true);
    }
}

}  // namespace usm::gc::ai_export
