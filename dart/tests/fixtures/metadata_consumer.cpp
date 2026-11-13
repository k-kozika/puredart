#include "puredart/meta.h"
#include <cassert>
#include <iostream>
#include <string_view>
void text(std::string_view value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::cout << '\t';
    for (unsigned char c : value) std::cout << digits[c >> 4] << digits[c & 15];
}
int main() {
    using namespace puredart::meta;
    const auto& b = puredart_bundle();
    assert(validate(b));
    assert(class_of("MissingMetadataClass") == nullptr);
    assert(resolve("MissingMetadataFunction") == 0);
    for (uint32_t i = 0; i < b.class_count; ++i) {
        const auto& c = b.classes[i];
        assert(class_by_id(c.cid) == &c);
        assert(class_of(string_at(c.name)) != nullptr);
        std::cout << "C\t" << c.cid; text(string_at(c.name)); std::cout << '\n';
    }
    for (uint32_t i = 0; i < b.symbol_count; ++i) {
        const auto& s = b.symbols[i];
        assert(resolve(string_at(s.name)) != 0);
        std::cout << "S\t" << s.rva; text(string_at(s.name)); std::cout << '\n';
    }
    for (uint32_t i = 0; i < b.constant_count; ++i) {
        const auto& c = b.constants[i];
        const auto value = blutter::meta::constant_value(b, c);
        assert(constant(string_at(c.cls), string_at(c.field), value) != 0);
        std::cout << "K\t" << c.heap;
        text(string_at(c.cls)); text(string_at(c.field)); text(value); std::cout << '\n';
    }
}
