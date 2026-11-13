#include "puredart/vm.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace puredart::vm {
namespace {

const MemoryMap& fallback_map() {
    static const MemoryMap map = MemoryMap::read();
    return map;
}

void append_utf8(std::string& result, uint32_t code_point) {
    if (code_point <= 0x7f) {
        result.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7ff) {
        result.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
        result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    } else if (code_point <= 0xffff) {
        result.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
        result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    } else {
        result.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
        result.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    }
}

bool range_access(const std::vector<MemoryMap::Range>& ranges, uintptr_t address,
                  size_t length) noexcept {
    if (length == 0) return true;
    const uintptr_t end = address + length;
    if (end < address) return false;
    for (const MemoryMap::Range& range : ranges) {
        if (address >= range.start && end <= range.end) {
            return true;
        }
    }
    return false;
}

}  // namespace

uint64_t heap_from_register(uintptr_t heap_register) noexcept {
    return static_cast<uint64_t>(heap_register) << 32;
}

uintptr_t decompress(uint32_t compressed, uint64_t heap) noexcept {
    return static_cast<uintptr_t>(heap + static_cast<int32_t>(compressed));
}

uintptr_t untag(uintptr_t tagged) noexcept {
    return tagged & ~kHeapObjectTag;
}

bool is_heap_object(uintptr_t value) noexcept {
    return (value & kHeapObjectTag) != 0;
}

int64_t smi_value(uintptr_t value) noexcept {
    return static_cast<intptr_t>(value) >> 1;
}

MemoryMap MemoryMap::read() {
    MemoryMap map;
#if defined(__linux__) || defined(__ANDROID__)
    FILE* file = std::fopen("/proc/self/maps", "re");
    if (file == nullptr) return map;
    char line[512];
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        unsigned long long start = 0;
        unsigned long long end = 0;
        char permissions[8] = {};
        if (std::sscanf(line, "%llx-%llx %7s", &start, &end, permissions) != 3 ||
            permissions[0] != 'r') continue;
        map.ranges_.push_back(
            {static_cast<uintptr_t>(start), static_cast<uintptr_t>(end)});
    }
    std::fclose(file);
#endif
    return map;
}

bool MemoryMap::mapped(uintptr_t address, size_t length) const noexcept {
    return range_access(ranges_, address, length);
}

uint32_t class_id(uintptr_t address, const MemoryMap& map) noexcept {
    if (!map.mapped(address, sizeof(uint32_t))) return 0;
    const auto& layout = meta::puredart_bundle().layout;
    const uint32_t tags = *reinterpret_cast<const uint32_t*>(address);
    return (tags >> layout.class_id_tag_pos) & layout.class_id_tag_mask;
}

Object::Object(uintptr_t tagged, uint64_t heap, const MemoryMap* map) noexcept
    : tagged_(tagged), heap_(heap), map_(map), present_(true) {}

Object Object::from_compressed(uint32_t tagged, uint64_t heap,
                               const MemoryMap* map) noexcept {
    Object result;
    result.present_ = true;
    result.heap_ = heap;
    result.map_ = map;
    if (is_heap_object(tagged)) {
        result.tagged_ = heap == 0 ? tagged : decompress(tagged, heap);
    } else {
        result.tagged_ = static_cast<uintptr_t>(
            static_cast<intptr_t>(static_cast<int32_t>(tagged)));
    }
    return result;
}

const MemoryMap& Object::memory() const noexcept {
    return map_ != nullptr ? *map_ : fallback_map();
}

bool Object::valid() const noexcept {
    if (!present_) return false;
    return is_smi() || memory().mapped(address(), sizeof(uint32_t));
}

uint32_t Object::cid() const noexcept {
    return is_smi() ? meta::puredart_bundle().layout.smi_cid : class_id(address(), memory());
}

const meta::ClassInfo* Object::class_info() const noexcept {
    return meta::class_by_id(cid());
}

std::optional<uintptr_t> Object::raw_field(uint32_t offset) const noexcept {
    const uintptr_t location = address() + offset;
    if (is_smi() || !memory().mapped(location, sizeof(uint32_t))) return std::nullopt;
    return *reinterpret_cast<const uint32_t*>(location);
}

std::optional<int64_t> Object::integer() const noexcept {
    if (!valid()) return std::nullopt;
    if (is_smi()) return as_smi();
    const auto& layout = meta::puredart_bundle().layout;
    if (cid() != layout.mint_cid ||
        !memory().mapped(address() + layout.instance_size, sizeof(int64_t))) {
        return std::nullopt;
    }
    return *reinterpret_cast<const int64_t*>(address() + layout.instance_size);
}

std::optional<bool> Object::boolean() const noexcept {
    const auto& layout = meta::puredart_bundle().layout;
    if (!valid() || cid() != layout.bool_cid ||
        !memory().mapped(address() + layout.instance_size, sizeof(uint8_t))) {
        return std::nullopt;
    }
    return *reinterpret_cast<const uint8_t*>(address() + layout.instance_size) != 0;
}

std::optional<double> Object::number() const noexcept {
    if (const auto integer_value = integer()) return static_cast<double>(*integer_value);
    const auto& layout = meta::puredart_bundle().layout;
    if (!valid() || cid() != layout.double_cid ||
        !memory().mapped(address() + layout.instance_size, sizeof(double))) {
        return std::nullopt;
    }
    return *reinterpret_cast<const double*>(address() + layout.instance_size);
}

std::optional<std::string> Object::string() const {
    if (!valid()) return std::nullopt;
    const auto& layout = meta::puredart_bundle().layout;
    const bool one_byte = cid() == layout.one_byte_string_cid;
    if (!one_byte && cid() != layout.two_byte_string_cid) return std::nullopt;
    const auto length_value = raw_field(layout.string_length_offset);
    if (!length_value || is_heap_object(*length_value)) return std::nullopt;
    const int64_t signed_length = smi_value(*length_value);
    if (signed_length < 0 || signed_length > (1 << 26)) return std::nullopt;
    const size_t length = static_cast<size_t>(signed_length);
    const uint32_t data_offset = one_byte ? layout.one_byte_string_data_offset
                                          : layout.two_byte_string_data_offset;
    const size_t byte_length = length * (one_byte ? 1u : 2u);
    if (!memory().mapped(address() + data_offset, byte_length)) return std::nullopt;
    std::string result;
    result.reserve(length);
    if (one_byte) {
        const auto* data = reinterpret_cast<const uint8_t*>(address() + data_offset);
        for (size_t i = 0; i < length; ++i) append_utf8(result, data[i]);
        return result;
    }
    const auto* data = reinterpret_cast<const uint16_t*>(address() + data_offset);
    for (size_t i = 0; i < length; ++i) {
        uint32_t code_point = data[i];
        if (code_point >= 0xd800 && code_point <= 0xdbff) {
            if (i + 1 < length && data[i + 1] >= 0xdc00 && data[i + 1] <= 0xdfff) {
                code_point = 0x10000 + ((code_point - 0xd800) << 10) +
                    (static_cast<uint32_t>(data[++i]) - 0xdc00);
            } else {
                code_point = 0xfffd;
            }
        } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
            code_point = 0xfffd;
        }
        append_utf8(result, code_point);
    }
    return result;
}

}  // namespace puredart::vm
