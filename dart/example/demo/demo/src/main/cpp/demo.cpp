#include <puredart/puredart.h>
#include <android/log.h>
#include <atomic>

namespace {
std::atomic<int> state{0};
void on_libapp(uintptr_t base) {
    const uintptr_t rva = puredart::meta::resolve("hookme$main$counterStep");
    const bool ok = rva != 0 && puredart::hook(base + rva, [](puredart::HookContext& context) {
        // counterStep returns an unboxed integer in x0 in this release build.
        context.set_return(10);
    });
    state.store(ok ? 1 : -1, std::memory_order_release);
    __android_log_print(ok ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "PuredartDemo",
                        "counterStep hook %s", ok ? "installed" : "failed");
}
}

extern "C" __attribute__((visibility("default"))) int puredart_demo_start() {
    if (!puredart::install() || !puredart::bootstrap(on_libapp)) return -1;
    return state.load(std::memory_order_acquire);
}
