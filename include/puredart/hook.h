#pragma once

#if defined(__clang__)
#if __has_feature(shadow_call_stack)
#error "puredart callbacks cannot use ShadowCallStack because Dart AOT does not preserve x18"
#endif
#endif

#include <cstddef>
#include <cstdint>
#include <functional>

#include "puredart/context.h"

namespace puredart {

using HookCallback = std::function<void(HookContext&)>;

// Installs a pre-call hook. If the callback calls set_return() or
// set_fp_return(), the Dart function is skipped. Otherwise its original
// trampoline is tail-called.
// The optional original pointer is for the Dart shim continuation only: it
// consumes the veneer IP frame. Do not invoke it directly from C++.
bool hook(uintptr_t address, HookCallback callback, void** original = nullptr) noexcept;

// Removes a previously installed hook and releases its slot. The target must
// not be executing (including the proxy/trampoline) while its slot is reclaimed;
// prevent new calls during removal. This function must not be
// called from that hook's callback.
bool unhook(uintptr_t address) noexcept;

size_t hook_capacity() noexcept;
size_t hook_count() noexcept;

namespace detail {
void dispatch_hook(size_t slot, HookContext& context) noexcept;
#if defined(PUREDART_TESTING)
bool publish_hook_callback_for_testing(size_t slot, HookCallback callback) noexcept;
void clear_hook_callback_for_testing(size_t slot) noexcept;
#endif
}

}  // namespace puredart

#define PUREDART_CONCAT_INNER(a, b) a##b
#define PUREDART_CONCAT(a, b) PUREDART_CONCAT_INNER(a, b)
#define PUREDART_HOOK(address, body)                                                   \
    ::puredart::hook((address),                                                       \
                     [=](::puredart::HookContext& PUREDART_CONCAT(_puredart_ctx_,     \
                                                                  __LINE__)) body)
