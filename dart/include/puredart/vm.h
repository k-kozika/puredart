#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "puredart/meta.h"

namespace puredart::vm {

constexpr uintptr_t kHeapObjectTag = 1;

uint64_t heap_from_register(uintptr_t heap_register) noexcept;
uintptr_t decompress(uint32_t compressed, uint64_t heap) noexcept;
uintptr_t untag(uintptr_t tagged) noexcept;
bool is_heap_object(uintptr_t value) noexcept;
int64_t smi_value(uintptr_t value) noexcept;

class MemoryMap {
public:
    struct Range {
        uintptr_t start;
        uintptr_t end;
    };

    static MemoryMap read();
    bool mapped(uintptr_t address, size_t length = 1) const noexcept;

private:
    std::vector<Range> ranges_;
};

uint32_t class_id(uintptr_t untagged, const MemoryMap& map) noexcept;

class Object {
public:
    Object() = default;
    // Constructs a value captured from a CPU register. Register values are
    // already decompressed in supported Dart AOT builds.
    Object(uintptr_t tagged, uint64_t heap, const MemoryMap* map = nullptr) noexcept;
    static Object from_compressed(uint32_t tagged, uint64_t heap,
                                  const MemoryMap* map = nullptr) noexcept;

    [[nodiscard]] uintptr_t tagged() const noexcept { return tagged_; }
    [[nodiscard]] uintptr_t address() const noexcept { return untag(tagged_); }
    [[nodiscard]] uint64_t heap() const noexcept { return heap_; }
    [[nodiscard]] bool is_smi() const noexcept { return !is_heap_object(tagged_); }
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] int64_t as_smi() const noexcept { return smi_value(tagged_); }
    [[nodiscard]] uint32_t cid() const noexcept;
    [[nodiscard]] const meta::ClassInfo* class_info() const noexcept;

    [[nodiscard]] std::optional<int64_t> integer() const noexcept;
    [[nodiscard]] std::optional<bool> boolean() const noexcept;
    [[nodiscard]] std::optional<double> number() const noexcept;
    [[nodiscard]] std::optional<std::string> string() const;

private:
    const MemoryMap& memory() const noexcept;

    [[nodiscard]] std::optional<uintptr_t> raw_field(uint32_t offset) const noexcept;

    uintptr_t tagged_{};
    uint64_t heap_{};
    const MemoryMap* map_{};
    bool present_{};
};

}  // namespace puredart::vm
