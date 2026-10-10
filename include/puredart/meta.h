#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#define PUREDART_META_VERSION 4u

namespace puredart::meta {

struct Symbol {
    uint32_t hash;
    uint32_t rva;
    uint32_t name_off;
};

struct LookupIndex {
    uint32_t hash;
    uint32_t index;
};

struct ClassInfo {
    uint32_t cid;
    uint32_t name_off;
};

struct Constant {
    uint32_t class_off;
    uint32_t field_off;
    uint32_t value_off;
    uint32_t heap_offset;
};

struct VmLayout {
    uint32_t compressed_word_size;
    uint32_t class_id_shift;
    uint32_t class_id_mask;
    uint32_t instance_data_offset;
    uint32_t string_length_offset;
    uint32_t one_byte_string_data_offset;
    uint32_t two_byte_string_data_offset;
    uint32_t cid_smi;
    uint32_t cid_mint;
    uint32_t cid_double;
    uint32_t cid_bool;
    uint32_t cid_string;
    uint32_t cid_two_byte_string;
};

struct Bundle {
    uint32_t meta_version;
    uint32_t symbol_count;
    const Symbol* symbols;
    uint32_t class_count;
    const ClassInfo* classes;
    const LookupIndex* classes_by_name;
    uint32_t constant_count;
    const Constant* constants;
    const LookupIndex* constants_by_key;
    const char* strings;
    uint32_t strings_size;
    VmLayout vm;
};

// Defined exactly once by the generated puredart_meta.cpp linked into the
// final consumer shared library.
const Bundle& puredart_bundle();

uint32_t hash_name(std::string_view name) noexcept;
uintptr_t resolve(std::string_view qualified_name) noexcept;
uintptr_t constant(std::string_view cls, std::string_view field,
                   std::string_view value) noexcept;
const ClassInfo* class_of(std::string_view name) noexcept;
const ClassInfo* class_by_id(uint32_t cid) noexcept;
std::string_view string_at(uint32_t offset) noexcept;
bool validate(const Bundle& bundle) noexcept;

}  // namespace puredart::meta
