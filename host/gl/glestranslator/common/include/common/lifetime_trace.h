// Optional GLES translator lifetime counters. No resource ownership is changed.
#pragma once
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace aemu_lifetime_trace {
constexpr int kNamedTypes = 10;
constexpr int kShareGroup = 10;
constexpr int kNameSpace = 11;
constexpr int kEglContext = 12;
constexpr int kKinds = 13;

inline bool enabled() {
    static const bool value = [] {
        const char* setting = std::getenv("AEMU_RESOURCE_TRACE");
        return setting && std::strcmp(setting, "1") == 0;
    }();
    return value;
}

struct Counters {
    std::atomic<long long> live[kKinds];
    std::atomic<unsigned long long> created[kKinds];
    std::atomic<unsigned long long> destroyed[kKinds];
    std::atomic<unsigned> emitted{0};
    Counters() {
        for (int i = 0; i < kKinds; ++i) {
            live[i].store(0); created[i].store(0); destroyed[i].store(0);
        }
    }
};
inline Counters& counters() { static Counters value; return value; }
// One shared budget for identity events. The marker invalidates complete pairing claims.
inline bool identityEventAllowed() {
    if (!enabled()) return false;
    static std::atomic<unsigned> emitted{0};
    const unsigned number = emitted.fetch_add(1, std::memory_order_relaxed);
    if (number == 8192) {
        std::fputs("AEMU_RESOURCE identity_trace_limit limit=8192\n", stderr);
        std::fflush(stderr);
    }
    return number < 8192;
}
inline void created(int kind) {
    if (!enabled() || kind < 0 || kind >= kKinds) return;
    counters().created[kind].fetch_add(1, std::memory_order_relaxed);
    counters().live[kind].fetch_add(1, std::memory_order_relaxed);
}
inline void destroyed(int kind) {
    if (!enabled() || kind < 0 || kind >= kKinds) return;
    counters().live[kind].fetch_sub(1, std::memory_order_relaxed);
    counters().destroyed[kind].fetch_add(1, std::memory_order_relaxed);
}

inline void snapshot(const char* event, const void* owner) {
    if (!enabled()) return;
    auto& values = counters();
    const unsigned number = values.emitted.fetch_add(1, std::memory_order_relaxed);
    if (number >= 2048) return;
    static const char* labels[kKinds] = {"null", "buffer", "texture", "renderbuffer",
        "framebuffer", "shader_program", "sampler", "query", "vao", "transform_feedback",
        "share_group", "namespace", "egl_context"};
    char text[3072];
    int used = std::snprintf(text, sizeof(text), "AEMU_RESOURCE lifetime event=%s owner=%p seq=%u", event, owner, number);
    for (int i = 0; i < kKinds && used > 0 && used < int(sizeof(text)) - 1; ++i) {
        const int added = std::snprintf(text + used, sizeof(text) - used,
            " %s_live=%lld %s_created=%llu %s_destroyed=%llu", labels[i],
            values.live[i].load(std::memory_order_relaxed), labels[i],
            values.created[i].load(std::memory_order_relaxed), labels[i],
            values.destroyed[i].load(std::memory_order_relaxed));
        if (added < 0) return;
        used += added;
    }
    if (used >= int(sizeof(text)) - 2) return;
    text[used++] = '\n'; text[used] = '\0';
    std::fputs(text, stderr); std::fflush(stderr);
    if (number == 2047) {
        std::fputs("AEMU_RESOURCE lifetime_trace_limit limit=2048\n", stderr);
        std::fflush(stderr);
    }
}
}  // namespace aemu_lifetime_trace
