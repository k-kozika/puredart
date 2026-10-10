#include "puredart/puredart.h"

#include <atomic>
#include <mutex>

namespace puredart {
namespace {

std::once_flag install_once;
std::atomic<bool> install_result{false};

}  // namespace

bool install() noexcept {
    std::call_once(install_once, [] {
        const bool ok = meta::validate(meta::puredart_bundle());
        install_result.store(ok, std::memory_order_release);
    });
    return install_result.load(std::memory_order_acquire);
}

bool installed() noexcept {
    return install_result.load(std::memory_order_acquire);
}

}  // namespace puredart
