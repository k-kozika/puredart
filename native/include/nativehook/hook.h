#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace nativehook {

constexpr uint32_t kRet = 0xd65f03c0;
constexpr uint32_t kNop = 0xd503201f;
constexpr uint32_t kBtiC = 0xd503245f;

inline uintptr_t branch_target(uintptr_t pc, uint32_t instruction) {
    int64_t displacement = instruction & 0x03ffffff;
    if (displacement & 0x02000000) displacement -= 0x04000000;
    return pc + displacement * 4;
}

struct Handle { uint64_t id{}; };

struct Redirect {
    uintptr_t entry;
    uintptr_t replacement;
    uintptr_t* original = nullptr;
    // Adapter contract: the veneer pushes x16/x17 (16 bytes) before jumping.
    // The replacement must consume this frame on its return path, or leave it
    // for the original trampoline to consume on its continuation path.
    bool preserve_ip = false;
    // Register used as the target convention's stack pointer (31 = CPU SP).
    uint8_t stack_register = 31;
};

// Prepare a batch before publishing any branch. A null handles output retains
// the hooks until process exit. Successful trampolines remain valid until remove.
// Supports Android ARM64; unsupported hosts return false.
bool redirect_all(const std::vector<Redirect>& redirects,
                  std::vector<Handle>* handles = nullptr) noexcept;

// Caller must ensure no thread is in the target, replacement or trampoline,
// and prevent new calls until removal completes. Never remove from a callback.
bool remove(Handle handle) noexcept;

#if defined(NATIVEHOOK_TESTING)
namespace testing {
enum class Failure { none, allocation, writable };
void fail_next(Failure failure) noexcept;
void page_size(size_t size) noexcept;
}
#endif

}  // namespace nativehook
