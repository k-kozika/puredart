#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>

#include "purehermes/bytecode.h"
int main(int argc, char** argv) {
    try {
        if (argc < 2)
            throw std::runtime_error(
                "usage: purehermes-meta bundle.hbc [--emit metadata.h "
                "(--function ID|--function-name NAME|--string ID|--string-value TEXT) --alias NAME "
                "...]");
        auto b = purehermes::Bundle::read_file(argv[1]);
        struct Selection {
            bool function;
            uint32_t id;
            std::string alias;
        };
        std::vector<Selection> selections;
        std::string output, pending_alias;
        for (int i = 2; i < argc; ++i) {
            std::string option = argv[i];
            if (i + 1 == argc) throw std::runtime_error("missing option value");
            std::string value = argv[++i];
            if (option == "--emit")
                output = value;
            else if (option == "--alias") {
                if (selections.empty())
                    pending_alias = value;
                else {
                    if (!selections.back().alias.empty())
                        throw std::runtime_error("duplicate alias for selector");
                    selections.back().alias = value;
                }
            } else if (option == "--function" || option == "--string" ||
                       option == "--function-name" || option == "--string-value") {
                uint32_t id = 0;
                if (option == "--function-name")
                    id = b.function(value).id;
                else if (option == "--string-value") {
                    size_t matches = 0;
                    for (uint32_t index = 0; index < b.strings().size(); ++index)
                        if (b.strings()[index] == value) {
                            id = index;
                            ++matches;
                        }
                    if (matches != 1)
                        throw std::runtime_error("missing/ambiguous string value; use an ID");
                } else {
                    size_t used = 0;
                    unsigned long parsed = std::stoul(value, &used);
                    if (used != value.size() || parsed > UINT32_MAX)
                        throw std::runtime_error("invalid selector ID");
                    id = uint32_t(parsed);
                }
                const bool function = option == "--function" || option == "--function-name";
                if (function)
                    b.function(id);
                else if (id >= b.strings().size())
                    throw std::runtime_error("invalid string ID");
                selections.push_back({function, id, std::move(pending_alias)});
                pending_alias.clear();
            } else
                throw std::runtime_error("unknown option");
        }
        if (!output.empty()) {
            if (selections.empty()) throw std::runtime_error("emit requires selectors");
            std::set<std::string> aliases{"bytes", "bundle"};
            for (const auto& selection : selections)
                if (!std::regex_match(selection.alias, std::regex("[A-Za-z_][A-Za-z0-9_]*")) ||
                    !aliases.insert(selection.alias).second)
                    throw std::runtime_error(
                        "each selector requires a unique C++ identifier alias");
            if (std::filesystem::weakly_canonical(output) ==
                std::filesystem::weakly_canonical(argv[1]))
                throw std::runtime_error("cannot overwrite input HBC");
            std::ofstream f(output);
            if (!f) throw std::runtime_error("cannot create metadata header");
            f << "// Generated metadata. The source HBC is unchanged.\n#pragma once\n#include "
                 "<purehermes/purehermes.h>\nnamespace purehermes_generated {\ninline constexpr "
                 "uint8_t bytes[] = {";
            for (size_t i = 0; i < b.bytes().size(); ++i) {
                if (i % 24 == 0) f << '\n';
                f << unsigned(b.bytes()[i]) << ',';
            }
            f << "\n};\ninline purehermes::Bundle bundle(){return "
                 "purehermes::Bundle::read(bytes,sizeof(bytes));}\n";
            for (const auto& selection : selections)
                f << "inline constexpr purehermes::"
                  << (selection.function ? "FunctionSelector" : "StringSelector") << ' '
                  << selection.alias << '{' << selection.id << "};\n";
            f << "}\n";
            if (!f) throw std::runtime_error("metadata write failed");
            return 0;
        }
        std::cout << "HBC " << b.version() << " functions=" << b.functions().size()
                  << " strings=" << b.strings().size() << '\n';
        for (const auto& f : b.functions()) {
            std::cout << f.id << '\t' << f.name << '\t' << f.offset << '\t' << f.size << "\tRet:";
            for (auto r : f.returns) std::cout << ' ' << r;
            std::cout << '\n';
        }
        for (const auto& use : b.string_uses())
            std::cout << "use " << use.string_id << " function=" << use.function_id
                      << " offset=" << use.instruction_offset << '\n';
        for (size_t i = 0; i < b.strings().size(); ++i)
            std::cout << "string " << i << '\t' << std::quoted(b.strings()[i]) << '\n';
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
