#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <Metadata.h>

#define PUREDART_META_VERSION ::blutter::meta::kAbiVersion

namespace puredart::meta {

using Symbol = ::blutter::meta::Symbol;
using LookupIndex = ::blutter::meta::LookupIndex;
using ClassInfo = ::blutter::meta::ClassInfo;
using Constant = ::blutter::meta::Constant;
using VmLayout = ::blutter::meta::Layout;
using Bundle = ::blutter::meta::Bundle;

// blutter_bundle() is defined by the generated blutter_meta.cpp linked into
// the final consumer shared library.
inline const Bundle& puredart_bundle() {
    return ::blutter::meta::blutter_bundle();
}

uint32_t hash_name(std::string_view name) noexcept;
uintptr_t resolve(std::string_view qualified_name) noexcept;
uintptr_t constant(std::string_view cls, std::string_view field,
                   std::string_view value) noexcept;
const ClassInfo* class_of(std::string_view name) noexcept;
const ClassInfo* class_by_id(uint32_t cid) noexcept;
std::string_view string_at(uint32_t offset) noexcept;
bool validate(const Bundle& bundle) noexcept;

}  // namespace puredart::meta
