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
            size_t regexps = 0;
            for (const auto& function : b.functions())
                for (const auto& in : function.instructions) {
                    if (in.name != "CreateRegExp") continue;
                    ++regexps;
                    check(b.strings().at(in.operands.at(1)) == "hello" &&
                              b.strings().at(in.operands.at(2)) == "g",
                          "regexp string operands");
                    for (size_t operand = 1; operand <= 2; ++operand) {
                        check(std::count_if(b.string_uses().begin(), b.string_uses().end(),
                                            [&](const auto& use) {
                                                return use.string_id == in.operands[operand] &&
                                                       use.function_id == function.id &&
                                                       use.instruction_offset == in.offset;
                                            }) == 1,
                              "both regexp string references");
                        auto invalid = b.bytes();
                        // CreateRegExp has a Reg8 followed by three UInt32 operands.
                        set(invalid, function.offset + in.offset + 2 + 4 * (operand - 1),
                            uint32_t(b.strings().size()));
                        rejects([&] { purehermes::Bundle::read(invalid.data(), invalid.size()); });
                    }
                }
            check(regexps == 1, "regexp fixture");
            size_t generators = 0;
            for (const auto& generator : b.functions()) {
                if (std::none_of(generator.instructions.begin(), generator.instructions.end(),
                                 [](const auto& in) { return in.name == "CreateGenerator"; }))
                    continue;
                ++generators;
                const bool wide = generator.name == "generator128";
                check(generator.parameters == (wide ? 129 : 36), "generator parameters");
                const size_t header_size = version == 96 ? 16 : 12;
                const uint8_t flags = b.bytes().at(128 + (generator.id + 1) * header_size - 1);
                check(bool(flags & 32) == (wide || version >= 98),
                      "generator overflow fixture");
                // HBC96 does not encode a function kind in its flags.
                check(generator.kind == (version == 96 ? 0 : 1), "generator function kind");
                check(!generator.returns.empty(), "generator has return sites");
                if (version != 96) {
                    purehermes::HookPlan generator_plan(b);
                    rejects([&] { generator_plan.on_return({generator.id}, [](auto&) {}); });
                }
            }
            check(generators == 2, "generator fixtures");
            auto large = purehermes::Bundle::read_file(std::string(FIXTURES) + "/large" +
                                                       std::to_string(version) + ".hbc");
            check(large.function("large").size == 40008 &&
                      large.function("large").returns == std::vector<uint32_t>{40006} &&
                      large.function("large").kind == 0,
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
