#pragma once
#include <functional>
#include <memory>

#include "purehermes/bytecode.h"

namespace purehermes {
struct FunctionSelector {
    uint32_t id;
};
struct StringSelector {
    uint32_t id;
};
class ReturnContext {
   public:
    double number() const noexcept { return result_; }
    void set_number(double value) noexcept {
        result_ = value;
        changed_ = true;
    }

   private:
    friend struct RuntimeAccess;
    double result_{};
    bool changed_{};
};
// ELF build-specific data, generated/verified offline; not selected by HBC alone.
struct RuntimeProfile {
    std::string build_id;
    uint32_t hbc_version{};
    uintptr_t factory_rva{}, map_string_rva{}, destructor_rva{};
    uintptr_t dispatch_table_rva{}, ret_handler_rva{};
    uint32_t factory_instruction{}, map_instruction{}, destructor_instruction{};
    uint8_t ip_register{}, frame_register{};
    bool factory_sret{};
    uintptr_t lazy_string_rva{}, symbol_handle_rva{};
    uint32_t lazy_instruction{}, symbol_handle_instruction{};
    bool lazy_has_runtime{};
    int32_t closure_offset{};
    uint32_t function_codeblock_offset{};
    uint32_t module_runtime_offset{}, pointer_key_offset{};
};
enum class InstallState { installed, unsupported, rejected, protection_error };
struct InstallResult {
    InstallState state{InstallState::rejected};
    std::string message;
    bool hooks_active{};
    explicit operator bool() const noexcept { return state == InstallState::installed; }
};
struct HermesModule {
    uintptr_t base{};
    std::string build_id;
};
HermesModule module_at(uintptr_t base);
std::vector<RuntimeProfile> supported_profiles();
class HookPlan {
   public:
    // Configure before bundle execution. Copies are owned until process exit
    // after successful installation; destroying HookPlan never live-unhooks.
    explicit HookPlan(Bundle bundle);
    HookPlan& on_return(FunctionSelector, std::function<void(ReturnContext&)>);
    // Entire module string ID, including every shared use. UTF-8 ASCII initially.
    HookPlan& map_literal(StringSelector, std::string replacement);
    InstallResult install(HermesModule, const RuntimeProfile&) noexcept;
    InstallResult install(HermesModule) noexcept;

   private:
    friend struct RuntimeAccess;
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
}  // namespace purehermes
