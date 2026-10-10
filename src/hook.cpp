#include "puredart/hook.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <new>
#include <nativehook/hook.h>
#include <thread>
#include <utility>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#include "puredart/puredart.h"

namespace {

constexpr size_t kHookSlots = 16;

struct Slot {
    // The callback object is immutable after publication. The atomic pointer
    // establishes a happens-before edge from construction to dispatch without
    // putting a lock or a shared_ptr reference-count update on every hook call.
    std::unique_ptr<puredart::HookCallback> callback;
    std::atomic<puredart::HookCallback*> published_callback{nullptr};
    std::atomic<size_t> dispatchers{0};
    uintptr_t address{};
    nativehook::Handle handle{};
};

std::array<Slot, kHookSlots> slots;
std::mutex slots_mutex;
std::atomic<size_t> active_slots{0};
thread_local uint32_t dispatching_slots = 0;

bool publish_callback(Slot& slot, puredart::HookCallback callback) noexcept {
    auto* value = new (std::nothrow) puredart::HookCallback(std::move(callback));
    if (value == nullptr) return false;
    slot.callback.reset(value);
    slot.published_callback.store(value, std::memory_order_release);
    return true;
}

void clear_callback(Slot& slot) noexcept {
    slot.published_callback.store(nullptr, std::memory_order_release);
    while (slot.dispatchers.load(std::memory_order_acquire) != 0) {
        std::this_thread::yield();
    }
    slot.callback.reset();
}

}  // namespace

extern "C" {
__attribute__((visibility("hidden"))) uintptr_t puredart_hook_originals[kHookSlots]{};

#if defined(__ANDROID__) && defined(__aarch64__)
void puredart_hook_proxy_0();
void puredart_hook_proxy_1();
void puredart_hook_proxy_2();
void puredart_hook_proxy_3();
void puredart_hook_proxy_4();
void puredart_hook_proxy_5();
void puredart_hook_proxy_6();
void puredart_hook_proxy_7();
void puredart_hook_proxy_8();
void puredart_hook_proxy_9();
void puredart_hook_proxy_10();
void puredart_hook_proxy_11();
void puredart_hook_proxy_12();
void puredart_hook_proxy_13();
void puredart_hook_proxy_14();
void puredart_hook_proxy_15();
#endif
}

namespace puredart {

namespace detail {

void dispatch_hook(size_t slot, HookContext& context) noexcept {
    if (slot >= kHookSlots) return;
    Slot& state = slots[slot];
    state.dispatchers.fetch_add(1, std::memory_order_acq_rel);
    HookCallback* callback = state.published_callback.load(std::memory_order_acquire);
    if (callback == nullptr) {
        state.dispatchers.fetch_sub(1, std::memory_order_release);
        return;
    }
    const uint32_t previous_slots = dispatching_slots;
    dispatching_slots |= uint32_t{1} << slot;
    try {
        (*callback)(context);
    } catch (...) {
#if defined(__ANDROID__)
        __android_log_print(ANDROID_LOG_ERROR, "Puredart",
                            "exception escaped hook callback %zu", slot);
#endif
    }
    dispatching_slots = previous_slots;
    state.dispatchers.fetch_sub(1, std::memory_order_release);
}

}  // namespace detail

extern "C" __attribute__((visibility("hidden"))) void
puredart_dispatch_hook(size_t slot, HookContext* context) noexcept {
    if (context != nullptr) detail::dispatch_hook(slot, *context);
}

bool hook(uintptr_t address, HookCallback callback, void** original) noexcept {
#if defined(__ANDROID__) && defined(__aarch64__)
    if (original) *original = nullptr;
    if (address == 0 || !callback || !install()) return false;
    static void* const proxies[kHookSlots] = {
        reinterpret_cast<void*>(&puredart_hook_proxy_0),
        reinterpret_cast<void*>(&puredart_hook_proxy_1),
        reinterpret_cast<void*>(&puredart_hook_proxy_2),
        reinterpret_cast<void*>(&puredart_hook_proxy_3),
        reinterpret_cast<void*>(&puredart_hook_proxy_4),
        reinterpret_cast<void*>(&puredart_hook_proxy_5),
        reinterpret_cast<void*>(&puredart_hook_proxy_6),
        reinterpret_cast<void*>(&puredart_hook_proxy_7),
        reinterpret_cast<void*>(&puredart_hook_proxy_8),
        reinterpret_cast<void*>(&puredart_hook_proxy_9),
        reinterpret_cast<void*>(&puredart_hook_proxy_10),
        reinterpret_cast<void*>(&puredart_hook_proxy_11),
        reinterpret_cast<void*>(&puredart_hook_proxy_12),
        reinterpret_cast<void*>(&puredart_hook_proxy_13),
        reinterpret_cast<void*>(&puredart_hook_proxy_14),
        reinterpret_cast<void*>(&puredart_hook_proxy_15),
    };

    std::lock_guard lock(slots_mutex);
    for (const auto& existing : slots) {
        if (existing.address == address) return false;
    }
    size_t slot = kHookSlots;
    for (size_t i = 0; i < kHookSlots; ++i) {
        if (!slots[i].callback) {
            slot = i;
            break;
        }
    }
    if (slot == kHookSlots) return false;
    if (!publish_callback(slots[slot], std::move(callback))) return false;
    std::vector<nativehook::Handle> handles;
    bool redirected = false;
    try {
        redirected = nativehook::redirect_all(
            {{address, reinterpret_cast<uintptr_t>(proxies[slot]),
              &puredart_hook_originals[slot], true, 15}}, &handles);
    } catch (...) {
        // Vector preparation can fail before entering the noexcept engine.
    }
    if (!redirected) {
        clear_callback(slots[slot]);
        puredart_hook_originals[slot] = 0;
        return false;
    }
    slots[slot].address = address;
    slots[slot].handle = handles.front();
    active_slots.fetch_add(1, std::memory_order_release);
    if (original != nullptr) *original = reinterpret_cast<void*>(puredart_hook_originals[slot]);
    return true;
#else
    (void)address;
    (void)callback;
    if (original != nullptr) *original = nullptr;
    return false;
#endif
}

bool unhook(uintptr_t address) noexcept {
#if defined(__ANDROID__) && defined(__aarch64__)
    if (address == 0) return false;
    std::lock_guard lock(slots_mutex);
    size_t slot = kHookSlots;
    for (size_t i = 0; i < kHookSlots; ++i) {
        if (slots[i].address == address && slots[i].handle.id != 0) {
            slot = i;
            break;
        }
    }
    if (slot == kHookSlots || (dispatching_slots & (uint32_t{1} << slot)) != 0) return false;
    if (!nativehook::remove(slots[slot].handle)) return false;

    clear_callback(slots[slot]);
    slots[slot].address = 0;
    slots[slot].handle = {};
    puredart_hook_originals[slot] = 0;
    active_slots.fetch_sub(1, std::memory_order_release);
    return true;
#else
    (void)address;
    return false;
#endif
}

size_t hook_capacity() noexcept { return kHookSlots; }
size_t hook_count() noexcept { return active_slots.load(std::memory_order_acquire); }

#if defined(PUREDART_TESTING)
namespace detail {

bool publish_hook_callback_for_testing(size_t slot, HookCallback callback) noexcept {
    if (slot >= kHookSlots || !callback) return false;
    std::lock_guard lock(slots_mutex);
    if (slots[slot].callback) return false;
    return publish_callback(slots[slot], std::move(callback));
}

void clear_hook_callback_for_testing(size_t slot) noexcept {
    if (slot >= kHookSlots) return;
    if ((dispatching_slots & (uint32_t{1} << slot)) != 0) return;
    std::lock_guard lock(slots_mutex);
    clear_callback(slots[slot]);
}

}  // namespace detail
#endif

}  // namespace puredart
