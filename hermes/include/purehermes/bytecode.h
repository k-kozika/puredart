#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace purehermes {
struct Instruction {
    uint32_t offset{};
    uint8_t opcode{};
    std::string name;
    std::vector<int64_t> operands;
};
struct Function {
    uint32_t id{}, offset{}, size{}, parameters{}, registers{};
    std::string name;
    uint8_t kind{};  // 0=ordinary; generator/async returns require different semantics.
    std::vector<Instruction> instructions;
    std::vector<uint32_t> returns;
};
struct StringUse {
    uint32_t string_id{}, function_id{}, instruction_offset{};
};
// Owns the unchanged bytecode and decoded metadata. No borrowed input pointers.
class Bundle {
   public:
    static Bundle read(const uint8_t* bytes, size_t size);
    static Bundle read_file(const std::string& path);
    uint32_t version() const noexcept { return version_; }
    const std::vector<uint8_t>& bytes() const noexcept { return bytes_; }
    const std::vector<Function>& functions() const noexcept { return functions_; }
    const std::vector<std::string>& strings() const noexcept { return strings_; }
    const std::vector<StringUse>& string_uses() const noexcept { return uses_; }
    bool is_identifier(uint32_t id) const { return identifiers_.at(id) != 0; }
    const Function& function(uint32_t id) const;
    // Requires a unique exact name. Anonymous/duplicate names require an ID.
    const Function& function(const std::string& name) const;

   private:
    uint32_t version_{};
    std::vector<uint8_t> bytes_;
    std::vector<Function> functions_;
    std::vector<std::string> strings_;
    std::vector<StringUse> uses_;
    std::vector<uint8_t> identifiers_;
};
std::vector<uint32_t> supported_versions();
}  // namespace purehermes
