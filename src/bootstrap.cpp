#include "puredart/bootstrap.h"

#include <atomic>

#if defined(__ANDROID__)
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include "elf_util.h"
#endif

namespace puredart {
#if defined(__ANDROID__)
namespace {
std::atomic<LibAppCallback> callback_{nullptr};

void* watch(void*) {
    // Do not interpose dlopen: forwarding it from another shared library changes
    // Android's caller-based linker namespace and can prevent libapp loading.
    for (int attempt = 0; attempt < 12000; ++attempt) {
        elf::Module app;
        if (elf::find_module("libapp.so", &app)) {
            __android_log_print(ANDROID_LOG_INFO, "Puredart", "libapp.so mapped @ %p",
                                reinterpret_cast<void*>(app.base));
            if (auto callback = callback_.load(std::memory_order_acquire)) callback(app.base);
            return nullptr;
        }
        usleep(10000);
    }
    __android_log_print(ANDROID_LOG_ERROR, "Puredart", "libapp.so was not observed");
    return nullptr;
}
}

bool bootstrap(LibAppCallback callback) noexcept {
    if (!callback) return false;
    LibAppCallback expected = nullptr;
    if (!callback_.compare_exchange_strong(expected, callback, std::memory_order_acq_rel))
        return expected == callback;
    pthread_t thread;
    if (pthread_create(&thread, nullptr, watch, nullptr) != 0) {
        callback_.store(nullptr, std::memory_order_release);
        return false;
    }
    pthread_detach(thread);
    return true;
}
#else
bool bootstrap(LibAppCallback) noexcept { return false; }
#endif
}  // namespace puredart
