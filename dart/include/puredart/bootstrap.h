#pragma once

#include <cstdint>

namespace puredart {

using LibAppCallback = void (*)(uintptr_t libapp_base);

// Watches for libapp.so and invokes callback exactly once. It is safe to call
// after libapp.so is already mapped. Polls every 10 ms for up to two minutes;
// installation is asynchronous and does not guarantee observing the first call.
bool bootstrap(LibAppCallback callback) noexcept;

}  // namespace puredart
