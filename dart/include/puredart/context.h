#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace puredart {

// Register snapshot taken before a Dart function runs. A callback may inspect
// x0..x7 through arg() and short-circuit the function with set_return() or
// set_fp_return(). If no return value is set, the original function is
// tail-called with its original register state.
struct alignas(16) HookContext {
    // Fixed ARM64 layout also lets non-ARM64 consumers build disabled stubs.
    uint64_t x[31]{};
    uint64_t sp{};
    uint32_t return_overridden{};
    uint32_t reserved{};
    alignas(16) unsigned char q[8][16]{};

    [[nodiscard]] uintptr_t arg(size_t index) const noexcept {
        return index < 8 ? x[index] : 0;
    }

    [[nodiscard]] uintptr_t reg(size_t index) const noexcept {
        return index < 31 ? x[index] : 0;
    }

    bool set_arg(size_t index, uintptr_t value) noexcept {
        if (index >= 8) return false;
        x[index] = value;
        return true;
    }

    void set_return(uintptr_t value) noexcept {
        x[0] = value;
        return_overridden = 1;
    }

    void set_fp_return(float value) noexcept {
        std::memset(q[0], 0, sizeof(q[0]));
        std::memcpy(q[0], &value, sizeof(value));
        return_overridden = 1;
    }

    void set_fp_return(double value) noexcept {
        std::memset(q[0], 0, sizeof(q[0]));
        std::memcpy(q[0], &value, sizeof(value));
        return_overridden = 1;
    }

    [[nodiscard]] bool has_return_override() const noexcept {
        return return_overridden != 0;
    }

    [[nodiscard]] uintptr_t heap_register() const noexcept { return x[28]; }
    [[nodiscard]] uintptr_t thread_register() const noexcept { return x[26]; }
    [[nodiscard]] uintptr_t pool_register() const noexcept { return x[27]; }
};

static_assert(offsetof(HookContext, sp) == 31 * sizeof(uint64_t));
static_assert(offsetof(HookContext, return_overridden) == 32 * sizeof(uint64_t));
static_assert(offsetof(HookContext, q) == 272);
static_assert(sizeof(HookContext) == 400);

}  // namespace puredart
