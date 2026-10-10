#include "puredart/meta.h"

#include <algorithm>
#include <iterator>

namespace puredart::meta {
namespace {

constexpr char kStrings[] =
    "\0"
    "BillingType::fromCode\0"
    "dart:core::_StringBase::length\0"
    "Object\0"
    "_Enum\0"
    "BillingType\0"
    "index\0"
    "name\0"
    "code\0"
    "premium\0";

constexpr uint32_t kFromCode = 1;
constexpr uint32_t kDartLength = 23;
constexpr uint32_t kObject = 54;
constexpr uint32_t kEnum = 61;
constexpr uint32_t kBillingType = 67;
constexpr uint32_t kIndex = 79;
constexpr uint32_t kName = 85;
constexpr uint32_t kCode = 90;
constexpr uint32_t kPremium = 95;

constexpr Symbol unsorted_symbols[] = {
    {0, 0x5e11ac, kFromCode},
    {0, 0x1234, kDartLength},
};

const Symbol* symbols() {
    static Symbol values[] = {
        {hash_name("BillingType::fromCode"), 0x5e11ac, kFromCode},
        {hash_name("dart:core::_StringBase::length"), 0x1234, kDartLength},
    };
    static bool sorted = false;
    if (!sorted) {
        if (values[1].hash < values[0].hash) {
            Symbol tmp = values[0]; values[0] = values[1]; values[1] = tmp;
        }
        sorted = true;
    }
    return values;
}

uint32_t constant_hash(std::string_view cls, std::string_view field,
                       std::string_view value) {
    uint32_t hash = 2166136261u;
    for (std::string_view part : {cls, field, value}) {
        for (const unsigned char ch : part) {
            hash ^= ch;
            hash *= 16777619u;
        }
        hash ^= 0;
        hash *= 16777619u;
    }
    return hash;
}

constexpr ClassInfo kClasses[] = {
    {44, kObject}, {120, kEnum}, {6735, kBillingType},
};

const LookupIndex* classes_by_name() {
    static LookupIndex values[] = {
        {hash_name("Object"), 0},
        {hash_name("_Enum"), 1},
        {hash_name("BillingType"), 2},
    };
    static const bool sorted = [] {
        std::sort(std::begin(values), std::end(values),
                  [](const LookupIndex& left, const LookupIndex& right) {
                      return left.hash < right.hash;
                  });
        return true;
    }();
    (void)sorted;
    return values;
}

constexpr Constant kConstants[] = {
    {kBillingType, kName, kPremium, 0xb8e1f1},
};

const LookupIndex* constants_by_key() {
    static const LookupIndex values[] = {
        {constant_hash("BillingType", "name", "premium"), 0},
    };
    return values;
}

}  // namespace

const Bundle& puredart_bundle() {
    static const Bundle bundle = {
        4u,
        2, symbols(),
        3, kClasses, classes_by_name(),
        1, kConstants, constants_by_key(),
        kStrings, sizeof(kStrings),
        {4, 12, 0xfffff, 8, 8, 16, 16, 60, 61, 62, 63, 94, 95},
    };
    return bundle;
}

}  // namespace puredart::meta
