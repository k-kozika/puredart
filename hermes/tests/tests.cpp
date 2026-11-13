#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "purehermes/purehermes.h"
#ifndef FIXTURES
#define FIXTURES "."
#endif
static void check(bool value, const char* text) {
    if (!value) throw std::runtime_error(text);
}
template <class F>
static void rejects(F f) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception&) {
        caught = true;
    }
    check(caught, "expected rejection");
}
static void set(std::vector<uint8_t>& b, size_t offset, uint32_t v) {
    for (size_t i = 0; i < 4; ++i) b.at(offset + i) = uint8_t(v >> (8 * i));
}
int main() {
    try {
        check(purehermes::supported_versions() == std::vector<uint32_t>{96, 97, 98, 99},
              "version list");
        for (size_t n = 0; n < 128; ++n) {
            std::vector<uint8_t> b(n);
            rejects([&] { purehermes::Bundle::read(b.data(), n); });
        }
        for (auto version : purehermes::supported_versions()) {
            auto b = purehermes::Bundle::read_file(std::string(FIXTURES) + "/v" +
                                                   std::to_string(version) + ".hbc");
            check(b.version() == version, "format version");
            const auto& f = b.function("update");
            check(f.size == 12 && f.returns == std::vector<uint32_t>{10}, "return decode");
            check(b.function("untouched").offset == f.offset, "shared bytecode fixture");
            check(b.function("branches").returns.size() == 2, "multiple returns");
            check(b.function("throws").returns.empty(), "throw has no normal return");
            check(b.bytes().at(f.offset + 10) == (version == 96   ? 92
                                                  : version == 97 ? 104
                                                                  : 118),
                  "version-specific Ret");
            uint32_t title = 0;
            for (uint32_t i = 0; i < b.strings().size(); ++i)
                if (b.strings()[i] == "+1") title = i;
            size_t uses = 0;
            for (const auto& use : b.string_uses())
                if (use.string_id == title) ++uses;
            check(uses >= 3, "direct, array and object string references");
            check(std::find(b.strings().begin(), b.strings().end(), "🐘日本") != b.strings().end(),
                  "UTF16 surrogate decoding");
            auto large = purehermes::Bundle::read_file(std::string(FIXTURES) + "/large" +
                                                       std::to_string(version) + ".hbc");
            check(large.function("large").size == 40008 &&
                      large.function("large").returns == std::vector<uint32_t>{40006},
                  "overflow function header");
            purehermes::HookPlan plan(b);
            plan.on_return({f.id}, [](auto& c) { c.set_number(c.number() + 9); });
            rejects([&] { plan.on_return({f.id}, [](auto&) {}); });
            rejects([&] { plan.on_return({b.function("throws").id}, [](auto&) {}); });
            rejects([&] { plan.map_literal({uint32_t(b.strings().size())}, "bad"); });
            check(plan.install({}, {}).state == purehermes::InstallState::unsupported,
                  "host runtime disabled");
            auto bytes = b.bytes();
            set(bytes, 8, 100);
            rejects([&] { purehermes::Bundle::read(bytes.data(), bytes.size()); });
            bytes = b.bytes();
            set(bytes, 32, 1);
            rejects([&] { purehermes::Bundle::read(bytes.data(), bytes.size()); });
            bytes = b.bytes();
            set(bytes, 40, 0xffffffff);
            rejects([&] { purehermes::Bundle::read(bytes.data(), bytes.size()); });
            bytes = b.bytes();
            bytes[f.offset + 10] = 255;
            rejects([&] { purehermes::Bundle::read(bytes.data(), bytes.size()); });
            bytes = b.bytes();
            bytes.pop_back();
            rejects([&] { purehermes::Bundle::read(bytes.data(), bytes.size()); });
        }
        std::cout << "PASS formats 96,97,98,99: reader, selectors, bounds, shared bodies, typed "
                     "callback registration\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
