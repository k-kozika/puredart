#include "puredart/meta.h"

#include <algorithm>
#include <cstring>
#include <limits>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace puredart::meta {
namespace {

bool valid_string_offset(const Bundle& bundle, uint32_t offset) noexcept {
    if (bundle.strings == nullptr || offset >= bundle.strings_size) return false;
    return std::memchr(bundle.strings + offset, '\0', bundle.strings_size - offset) != nullptr;
}

bool equal_at(const Bundle& bundle, uint32_t offset, std::string_view value) noexcept {
    if (!valid_string_offset(bundle, offset)) return false;
    const char* text = bundle.strings + offset;
    return std::string_view(text) == value;
}

std::string_view view_at(const Bundle& bundle, uint32_t offset) noexcept {
    return valid_string_offset(bundle, offset) ? std::string_view(bundle.strings + offset)
                                                : std::string_view{};
}

bool valid_constant_value(const Bundle& bundle, const Constant& item) noexcept {
    return bundle.strings != nullptr && item.value < bundle.strings_size &&
        item.value_length < bundle.strings_size - item.value &&
        bundle.strings[item.value + item.value_length] == '\0';
}

uint32_t hash_constant_key(std::string_view cls, std::string_view field,
                           std::string_view value) noexcept {
    uint32_t hash = 2166136261u;
    const auto append = [&hash](std::string_view part) {
        for (const unsigned char ch : part) {
            hash ^= ch;
            hash *= 16777619u;
        }
        hash ^= 0;
        hash *= 16777619u;
    };
    append(cls);
    append(field);
    append(value);
    return hash;
}

bool sorted_indexes(const LookupIndex* indexes, uint32_t count) noexcept {
    if (count != 0 && indexes == nullptr) return false;
    for (uint32_t i = 1; i < count; ++i) {
        if (indexes[i].hash < indexes[i - 1].hash) return false;
    }
    return true;
}

bool validation_failure(const char* reason) noexcept {
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_ERROR, "Puredart",
                        "metadata validation failed: %s", reason);
#else
    (void)reason;
#endif
    return false;
}

}  // namespace

uint32_t hash_name(std::string_view name) noexcept {
    uint32_t hash = 2166136261u;
    for (const unsigned char ch : name) {
        hash ^= ch;
        hash *= 16777619u;
    }
    return hash;
}

std::string_view string_at(uint32_t offset) noexcept {
    const Bundle& bundle = puredart_bundle();
    if (!valid_string_offset(bundle, offset)) return {};
    return bundle.strings + offset;
}

uintptr_t resolve(std::string_view name) noexcept {
    const Bundle& bundle = puredart_bundle();
    if (bundle.symbol_count == 0 || bundle.symbols == nullptr) return 0;
    const uint32_t wanted = hash_name(name);
    const Symbol* first = bundle.symbols;
    const Symbol* last = first + bundle.symbol_count;
    const Symbol* match = std::lower_bound(first, last, wanted,
        [](const Symbol& symbol, uint32_t hash) { return symbol.hash < hash; });
    for (; match != last && match->hash == wanted; ++match) {
        if (equal_at(bundle, match->name, name)) return match->rva;
    }
    return 0;
}

uintptr_t constant(std::string_view cls, std::string_view field,
                   std::string_view value) noexcept {
    const Bundle& bundle = puredart_bundle();
    if (bundle.constants == nullptr || bundle.constants_by_key == nullptr) return 0;
    const uint32_t wanted = hash_constant_key(cls, field, value);
    const LookupIndex* first = bundle.constants_by_key;
    const LookupIndex* last = first + bundle.constant_count;
    const LookupIndex* match = std::lower_bound(first, last, wanted,
        [](const LookupIndex& index, uint32_t hash) { return index.hash < hash; });
    for (; match != last && match->hash == wanted; ++match) {
        if (match->index >= bundle.constant_count) continue;
        const Constant& item = bundle.constants[match->index];
        if (equal_at(bundle, item.cls, cls) &&
            equal_at(bundle, item.field, field) &&
            valid_constant_value(bundle, item) &&
            ::blutter::meta::constant_value(bundle, item) == value) {
            return item.heap;
        }
    }
    return 0;
}

const ClassInfo* class_of(std::string_view name) noexcept {
    const Bundle& bundle = puredart_bundle();
    if (bundle.classes == nullptr || bundle.classes_by_name == nullptr) return nullptr;
    const uint32_t wanted = hash_name(name);
    const LookupIndex* first = bundle.classes_by_name;
    const LookupIndex* last = first + bundle.class_count;
    const LookupIndex* match = std::lower_bound(first, last, wanted,
        [](const LookupIndex& index, uint32_t hash) { return index.hash < hash; });
    for (; match != last && match->hash == wanted; ++match) {
        if (match->index < bundle.class_count &&
            equal_at(bundle, bundle.classes[match->index].name, name)) {
            return &bundle.classes[match->index];
        }
    }
    return nullptr;
}

const ClassInfo* class_by_id(uint32_t cid) noexcept {
    const Bundle& bundle = puredart_bundle();
    if (bundle.classes == nullptr) return nullptr;
    const ClassInfo* first = bundle.classes;
    const ClassInfo* last = first + bundle.class_count;
    const ClassInfo* match = std::lower_bound(first, last, cid,
        [](const ClassInfo& cls, uint32_t wanted) { return cls.cid < wanted; });
    return match != last && match->cid == cid ? match : nullptr;
}

bool validate(const Bundle& b) noexcept {
    if (b.abi_version != PUREDART_META_VERSION || b.strings == nullptr ||
        b.strings_size == 0 || b.strings[b.strings_size - 1] != '\0') {
        return validation_failure("header or string pool");
    }
    if ((b.symbol_count != 0 && b.symbols == nullptr) ||
        (b.class_count != 0 && (b.classes == nullptr || b.classes_by_name == nullptr)) ||
        (b.constant_count != 0 &&
         (b.constants == nullptr || b.constants_by_key == nullptr))) {
        return validation_failure("missing table");
    }
    uint32_t previous_hash = 0;
    for (uint32_t i = 0; i < b.symbol_count; ++i) {
        const Symbol& symbol = b.symbols[i];
        if (i != 0 && symbol.hash < previous_hash)
            return validation_failure("symbol ordering");
        if (!valid_string_offset(b, symbol.name))
            return validation_failure("symbol name");
        previous_hash = symbol.hash;
    }
    for (uint32_t i = 0; i < b.class_count; ++i) {
        const ClassInfo& cls = b.classes[i];
        if (i != 0 && cls.cid <= b.classes[i - 1].cid)
            return validation_failure("class ordering");
        if (!valid_string_offset(b, cls.name))
            return validation_failure("class record");
    }
    if (!sorted_indexes(b.classes_by_name, b.class_count))
        return validation_failure("class index ordering");
    for (uint32_t i = 0; i < b.class_count; ++i) {
        const LookupIndex& index = b.classes_by_name[i];
        if (index.index >= b.class_count ||
            index.hash != hash_name(view_at(b, b.classes[index.index].name)))
            return validation_failure("class index");
    }
    for (uint32_t i = 0; i < b.constant_count; ++i) {
        const Constant& item = b.constants[i];
        if (!valid_string_offset(b, item.cls) ||
            !valid_string_offset(b, item.field) ||
            !valid_constant_value(b, item) || (item.heap & 1u) == 0) {
            return validation_failure("constant record");
        }
    }
    if (!sorted_indexes(b.constants_by_key, b.constant_count))
        return validation_failure("constant index ordering");
    for (uint32_t i = 0; i < b.constant_count; ++i) {
        const LookupIndex& index = b.constants_by_key[i];
        if (index.index >= b.constant_count)
            return validation_failure("constant index range");
        const Constant& item = b.constants[index.index];
        if (index.hash != hash_constant_key(view_at(b, item.cls),
                                            view_at(b, item.field),
                                            ::blutter::meta::constant_value(b, item)))
            return validation_failure("constant index hash");
    }
    if (b.layout.compressed_word_size != sizeof(uint32_t) || b.layout.class_id_tag_mask == 0 ||
        b.layout.class_id_tag_pos >= 32 ||
        b.layout.string_length_offset >= b.layout.one_byte_string_data_offset ||
        b.layout.string_length_offset >= b.layout.two_byte_string_data_offset) {
        return validation_failure("VM layout");
    }
    return true;
}

}  // namespace puredart::meta
