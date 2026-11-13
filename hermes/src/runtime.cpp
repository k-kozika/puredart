#include <algorithm>
#include <limits>
#include <stdexcept>

#include "purehermes/purehermes.h"
namespace purehermes {
#include "profiles.inc"
std::vector<RuntimeProfile> supported_profiles() { return builtin_profiles(); }
InstallResult HookPlan::install(HermesModule module) noexcept {
    try {
        for (const auto& profile : builtin_profiles())
            if (profile.build_id == module.build_id) return install(module, profile);
        return {InstallState::unsupported, "no verified profile for this VM build ID"};
    } catch (...) {
        return {InstallState::rejected, "profile lookup failed"};
    }
}

struct HookPlan::Impl {
    Bundle bundle;
    struct Hook {
        uint32_t id;
        std::function<void(ReturnContext&)> callback;
    };
    std::vector<Hook> hooks;
    std::vector<std::pair<uint32_t, std::string>> strings;
    bool sealed = false;
    explicit Impl(Bundle b) : bundle(std::move(b)) {}
};
HookPlan::HookPlan(Bundle b) : impl_(std::make_shared<Impl>(std::move(b))) {}
HookPlan& HookPlan::on_return(FunctionSelector s, std::function<void(ReturnContext&)> fn) {
    if (impl_->sealed) throw std::logic_error("plan already installed");
    const auto& f = impl_->bundle.function(s.id);
    if (f.kind != 0)
        throw std::invalid_argument("only ordinary interpreted functions are supported");
    if (!fn || f.returns.empty()) throw std::invalid_argument("callback or return sites missing");
    for (auto& h : impl_->hooks)
        if (h.id == s.id) throw std::invalid_argument("duplicate function hook");
    impl_->hooks.push_back({s.id, std::move(fn)});
    return *this;
}
HookPlan& HookPlan::map_literal(StringSelector s, std::string text) {
    if (impl_->sealed) throw std::logic_error("plan already installed");
    if (s.id >= impl_->bundle.strings().size()) throw std::out_of_range("string ID");
    if (impl_->bundle.is_identifier(s.id))
        throw std::invalid_argument("identifier mappings require an earlier VM load hook");
    for (unsigned char c : impl_->bundle.strings()[s.id])
        if (c >= 128) throw std::invalid_argument("original literal must be ASCII");
    for (unsigned char c : text)
        if (c >= 128) throw std::invalid_argument("replacement must be ASCII");
    for (auto& v : impl_->strings)
        if (v.first == s.id) throw std::invalid_argument("duplicate string mapping");
    impl_->strings.emplace_back(s.id, std::move(text));
    return *this;
}
#ifndef PUREHERMES_ANDROID_RUNTIME
InstallResult HookPlan::install(HermesModule, const RuntimeProfile&) noexcept {
    return {InstallState::unsupported, "runtime hooks require Android ARM64"};
}
#endif
}  // namespace purehermes
#ifdef PUREHERMES_ANDROID_RUNTIME
#include <elf.h>
#include <purenative/hook.h>
#include <sys/mman.h>
#include <unistd.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
namespace purehermes {
namespace {
struct AsciiRef {
    const char* data;
    size_t size;
};
// A nontrivial one-pointer return uses the same AAPCS64 x8 sret lowering as
// the VM factory's unique_ptr<CodeBlock>. A trivial struct would use x0 instead.
struct Owner {
    void* pointer{};
    ~Owner() {}
};
thread_local const std::string* replacement = nullptr;
thread_local bool in_callback = false;
struct ReplacementScope {
    const std::string* previous = replacement;
    ReplacementScope() { replacement = nullptr; }
    ~ReplacementScope() { replacement = previous; }
};
bool mapped(uintptr_t p, size_t n, int* protection = nullptr) {
    p &= 0x00ffffffffffffffULL;  // Android tagged malloc pointers (TBI).
    FILE* f = fopen("/proc/self/maps", "re");
    if (!f) return false;
    char line[1024], perms[8];
    unsigned long long lo, hi;
    bool found = false;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "%llx-%llx %7s", &lo, &hi, perms) == 3 && p >= lo && p <= hi &&
            n <= hi - p && perms[0] == 'r') {
            if (protection)
                *protection = PROT_READ | (perms[1] == 'w' ? PROT_WRITE : 0) |
                              (perms[2] == 'x' ? PROT_EXEC : 0);
            found = true;
            break;
        }
    fclose(f);
    return found;
}
uint32_t hash(AsciiRef s) {
    uint32_t h = 0;
    for (size_t i = 0; i < s.size; ++i) {
        h += uint8_t(s.data[i]);
        h += h << 10;
        h ^= h >> 6;
    }
    return h;
}
void substitute(AsciiRef& s, uint32_t& h) {
    if (replacement) {
        s = {replacement->data(), replacement->size()};
        h = hash(s);
    }
}
}  // namespace
HermesModule module_at(uintptr_t base) {
    if (!mapped(base, sizeof(Elf64_Ehdr))) throw std::runtime_error("unmapped ELF header");
    const auto* e = reinterpret_cast<const Elf64_Ehdr*>(base);
    if (memcmp(e->e_ident, ELFMAG, SELFMAG) != 0 || e->e_machine != EM_AARCH64 ||
        e->e_phentsize != sizeof(Elf64_Phdr))
        throw std::runtime_error("not an ARM64 ELF");
    if (!mapped(base + e->e_phoff, size_t(e->e_phnum) * sizeof(Elf64_Phdr)))
        throw std::runtime_error("unmapped ELF program headers");
    const auto* ph = reinterpret_cast<const Elf64_Phdr*>(base + e->e_phoff);
    for (size_t i = 0; i < e->e_phnum; ++i)
        if (ph[i].p_type == PT_NOTE && mapped(base + ph[i].p_vaddr, ph[i].p_memsz)) {
            size_t p = 0;
            const auto* data = reinterpret_cast<const uint8_t*>(base + ph[i].p_vaddr);
            while (p + 12 <= ph[i].p_memsz) {
                Elf64_Nhdr n;
                memcpy(&n, data + p, 12);
                p += 12;
                const size_t names = (size_t(n.n_namesz) + 3) & ~size_t(3),
                             desc = (size_t(n.n_descsz) + 3) & ~size_t(3);
                if (names > ph[i].p_memsz - p || desc > ph[i].p_memsz - p - names) break;
                if (n.n_type == NT_GNU_BUILD_ID && n.n_namesz == 4 && !memcmp(data + p, "GNU", 4)) {
                    static const char hex[] = "0123456789abcdef";
                    std::string id;
                    for (size_t j = 0; j < n.n_descsz; ++j) {
                        auto c = data[p + names + j];
                        id += hex[c >> 4];
                        id += hex[c & 15];
                    }
                    return {base, id};
                }
                p += names + desc;
            }
        }
    throw std::runtime_error("ELF GNU build ID missing");
}
extern "C" {
void ph_ret_0();
void ph_ret_1();
void ph_ret_2();
void ph_ret_3();
void ph_ret_4();
void ph_ret_5();
void ph_ret_6();
void ph_ret_7();
}
struct RuntimeAccess {
    struct Binding {
        void* module;
        void* codeblock;
        uintptr_t ip;
        size_t hook;
    };
    struct Slot {
        std::shared_ptr<HookPlan::Impl> plan;
        RuntimeProfile profile;
        uintptr_t base{}, factory{}, symbol{}, destructor{}, lazy{}, handle{};
        std::vector<Binding> bindings;
        std::vector<void*> modules;
    };
    static std::array<Slot, 8>& slots() {
        static auto* a = new std::array<Slot, 8>;
        return *a;
    }
    static std::mutex& mutex() {
        static auto* m = new std::mutex;
        return *m;
    }
    static void bind(size_t index, void* module, void* cb, const uint8_t* bc,
                     uint32_t id) noexcept {
        try {
            auto& s = slots()[index];
            bool wanted = (id == 0 && !s.plan->strings.empty());
            for (const auto& hook : s.plan->hooks) wanted = wanted || hook.id == id;
            if (!wanted) return;
            if (id >= s.plan->bundle.functions().size() || !bc) return;
            const auto& f = s.plan->bundle.function(id);
            const auto& bytes = s.plan->bundle.bytes();
            uintptr_t base = reinterpret_cast<uintptr_t>(bc) - f.offset;
            // Full buffer identity, not just the source hash in the header.
            if (!mapped(base, bytes.size()) ||
                memcmp(reinterpret_cast<void*>(base), bytes.data(), bytes.size()))
                return;
            std::lock_guard<std::mutex> lock(mutex());
            if (std::find(s.modules.begin(), s.modules.end(), module) == s.modules.end())
                s.modules.push_back(module);
            for (size_t h = 0; h < s.plan->hooks.size(); ++h)
                if (s.plan->hooks[h].id == id)
                    for (auto offset : f.returns)
                        s.bindings.push_back(
                            {module, cb, reinterpret_cast<uintptr_t>(bc) + offset, h});
        } catch (...) {
        }  // Never unwind through the VM's exception-disabled code.
    }
    template <size_t I>
    static void* factory_raw(void* module, uintptr_t header, const uint8_t* bc, uint32_t id) {
        auto f = reinterpret_cast<void* (*)(void*, uintptr_t, const uint8_t*, uint32_t)>(
            slots()[I].factory);
        void* cb = f(module, header, bc, id);
        bind(I, module, cb, bc, id);
        return cb;
    }
    template <size_t I>
    static Owner factory_owner(void* module, uintptr_t header, const uint8_t* bc, uint32_t id) {
        auto f = reinterpret_cast<Owner (*)(void*, uintptr_t, const uint8_t*, uint32_t)>(
            slots()[I].factory);
        Owner cb = f(module, header, bc, id);
        bind(I, module, cb.pointer, bc, id);
        return cb;
    }
    template <size_t I>
    static uint32_t symbol(void* module, uint32_t id, const void* entry, uint64_t opt_hash) {
        ReplacementScope scope;
        auto& s = slots()[I];
        {
            std::lock_guard<std::mutex> lock(mutex());
            if (std::find(s.modules.begin(), s.modules.end(), module) != s.modules.end())
                for (const auto& v : s.plan->strings)
                    if (v.first == id) replacement = &v.second;
        }
        auto f = reinterpret_cast<uint32_t (*)(void*, uint32_t, const void*, uint64_t)>(s.symbol);
        return f(module, id, entry, opt_hash);
    }
    template <size_t I>
    static uintptr_t handle(void* self, void* rt, AsciiRef str, uint32_t h) {
        substitute(str, h);
        return reinterpret_cast<uintptr_t (*)(void*, void*, AsciiRef, uint32_t)>(slots()[I].handle)(
            self, rt, str, h);
    }
    template <size_t I>
    static uint32_t lazy_new(void* self, void* rt, AsciiRef str, uint32_t h) {
        substitute(str, h);
        return reinterpret_cast<uint32_t (*)(void*, void*, AsciiRef, uint32_t)>(slots()[I].lazy)(
            self, rt, str, h);
    }
    template <size_t I>
    static uint32_t lazy_old(void* self, AsciiRef str, uint32_t h) {
        substitute(str, h);
        return reinterpret_cast<uint32_t (*)(void*, AsciiRef, uint32_t)>(slots()[I].lazy)(self, str,
                                                                                          h);
    }
    template <size_t I>
    static void destroy(void* module) {
        auto& s = slots()[I];
        {
            std::lock_guard<std::mutex> lock(mutex());
            s.bindings.erase(std::remove_if(s.bindings.begin(), s.bindings.end(),
                                            [&](auto& b) { return b.module == module; }),
                             s.bindings.end());
            s.modules.erase(std::remove(s.modules.begin(), s.modules.end(), module),
                            s.modules.end());
        }
        reinterpret_cast<void (*)(void*)>(s.destructor)(module);
    }
    static uintptr_t dispatch(uint64_t* regs, size_t index) noexcept {
        auto& s = slots()[index];
        if (in_callback) return s.base + s.profile.ret_handler_rva;
        try {
            const uintptr_t ip = regs[s.profile.ip_register],
                            frame = regs[s.profile.frame_register];
            std::unique_lock<std::mutex> lock(mutex());
            for (const auto& b : s.bindings)
                if (b.ip == ip) {
                    // A shared instruction buffer does not imply shared function identity.
                    uint64_t closure =
                        *reinterpret_cast<const uint64_t*>(frame + s.profile.closure_offset);
                    uintptr_t cb = closure;
                    if ((closure >> 48) == 0xffff)
                        cb = *reinterpret_cast<const uintptr_t*>(
                            (closure & 0x0000ffffffffffffULL) +
                            s.profile.function_codeblock_offset);
                    if ((closure >> 48) == 0xffff && s.profile.pointer_key_offset) {
                        auto rt = *reinterpret_cast<const uintptr_t*>(
                            reinterpret_cast<uintptr_t>(b.module) +
                            s.profile.module_runtime_offset);
                        cb ^=
                            *reinterpret_cast<const uintptr_t*>(rt + s.profile.pointer_key_offset);
                    }
                    if (cb != reinterpret_cast<uintptr_t>(b.codeblock)) continue;
                    auto* value = reinterpret_cast<uint64_t*>(
                        frame + size_t(*reinterpret_cast<const uint8_t*>(ip + 1)) * 8);
                    if (*value >= 0xfff9000000000000ULL) continue;  // HV64 numeric tag contract.
                    ReturnContext context;
                    memcpy(&context.result_, value, 8);
                    const auto* callback = &s.plan->hooks[b.hook].callback;
                    lock.unlock();
                    in_callback = true;
                    try {
                        (*callback)(context);
                    } catch (...) {
                        context.changed_ = false;
                    }
                    in_callback = false;
                    if (context.changed_) {
                        if (std::isnan(context.result_))
                            context.result_ = std::numeric_limits<double>::quiet_NaN();
                        memcpy(value, &context.result_, 8);
                    }
                    break;
                }
        } catch (...) {
            in_callback = false;
        }
        return s.base + s.profile.ret_handler_rva;
    }
    template <size_t... I>
    static auto factories(bool owner, std::index_sequence<I...>) {
        return std::array<uintptr_t, sizeof...(I)>{
            (owner ? reinterpret_cast<uintptr_t>(&factory_owner<I>)
                   : reinterpret_cast<uintptr_t>(&factory_raw<I>))...};
    }
    template <size_t... I>
    static auto symbols(std::index_sequence<I...>) {
        return std::array<uintptr_t, sizeof...(I)>{reinterpret_cast<uintptr_t>(&symbol<I>)...};
    }
    template <size_t... I>
    static auto destructors(std::index_sequence<I...>) {
        return std::array<uintptr_t, sizeof...(I)>{reinterpret_cast<uintptr_t>(&destroy<I>)...};
    }
    template <size_t... I>
    static auto handles(std::index_sequence<I...>) {
        return std::array<uintptr_t, sizeof...(I)>{reinterpret_cast<uintptr_t>(&handle<I>)...};
    }
    template <size_t... I>
    static auto lazies(bool rt, std::index_sequence<I...>) {
        return std::array<uintptr_t, sizeof...(I)>{
            (rt ? reinterpret_cast<uintptr_t>(&lazy_new<I>)
                : reinterpret_cast<uintptr_t>(&lazy_old<I>))...};
    }
};
extern "C" __attribute__((visibility("hidden"))) uintptr_t ph_dispatch(uint64_t* regs,
                                                                       size_t index) noexcept {
    return RuntimeAccess::dispatch(regs, index);
}
InstallResult HookPlan::install(HermesModule module, const RuntimeProfile& p) noexcept {
    try {
        if (impl_->sealed) return {InstallState::rejected, "plan already installed"};
        if (p.hbc_version != impl_->bundle.version() || p.ip_register > 30 ||
            p.frame_register > 30 || p.build_id.empty() ||
            module_at(module.base).build_id != p.build_id || module.build_id != p.build_id)
            return {InstallState::rejected, "VM build ID, ABI or HBC mismatch"};
        if (impl_->hooks.empty() && impl_->strings.empty())
            return {InstallState::rejected, "empty hook plan"};
        std::lock_guard<std::mutex> lock(RuntimeAccess::mutex());
        auto& slots = RuntimeAccess::slots();
        size_t i = 0;
        for (; i < slots.size(); ++i) {
            if (slots[i].base == module.base) return {InstallState::rejected, "VM already hooked"};
            if (!slots[i].plan) break;
        }
        if (i == slots.size()) return {InstallState::rejected, "VM hook capacity exhausted"};
        const std::array<std::pair<uintptr_t, uint32_t>, 5> entries = {
            {{p.factory_rva, p.factory_instruction},
             {p.map_string_rva, p.map_instruction},
             {p.destructor_rva, p.destructor_instruction},
             {p.lazy_string_rva, p.lazy_instruction},
             {p.symbol_handle_rva, p.symbol_handle_instruction}}};
        for (auto e : entries) {
            if (!e.first || !mapped(module.base + e.first, 4) ||
                *reinterpret_cast<const uint32_t*>(module.base + e.first) != e.second)
                return {InstallState::rejected, "entry instruction mismatch"};
        }
        auto& s = slots[i];
        auto sequence = std::make_index_sequence<8>{};
        std::array<uintptr_t, 8> stubs = {
            reinterpret_cast<uintptr_t>(&ph_ret_0), reinterpret_cast<uintptr_t>(&ph_ret_1),
            reinterpret_cast<uintptr_t>(&ph_ret_2), reinterpret_cast<uintptr_t>(&ph_ret_3),
            reinterpret_cast<uintptr_t>(&ph_ret_4), reinterpret_cast<uintptr_t>(&ph_ret_5),
            reinterpret_cast<uintptr_t>(&ph_ret_6), reinterpret_cast<uintptr_t>(&ph_ret_7)};
        uint8_t opcode =
            impl_->bundle.bytes()
                [impl_->bundle.function(impl_->hooks.empty() ? 0 : impl_->hooks.front().id).offset];
        // Locate Ret by semantic name in the decoded bundle, never a fixed number.
        bool found = false;
        for (const auto& f : impl_->bundle.functions())
            for (const auto& ins : f.instructions)
                if (ins.name == "Ret") {
                    opcode = ins.opcode;
                    found = true;
                    break;
                }
        if (!found) {
            return {InstallState::rejected, "Ret opcode absent"};
        }
        auto* slot =
            reinterpret_cast<uintptr_t*>(module.base + p.dispatch_table_rva + size_t(opcode) * 8);
        int protection = 0;
        if (!mapped(reinterpret_cast<uintptr_t>(slot), 8, &protection) ||
            *slot != module.base + p.ret_handler_rva) {
            return {InstallState::rejected, "dispatch table mismatch"};
        }
        const long system_page_size = sysconf(_SC_PAGESIZE);
        if (system_page_size <= 0 || (system_page_size & (system_page_size - 1)))
            return {InstallState::rejected, "invalid system page size"};
        const size_t page_size = size_t(system_page_size);
        uintptr_t page = reinterpret_cast<uintptr_t>(slot) & ~(page_size - 1);
        // Finish every allocation before reserving a slot or changing page permissions.
        // From publication through permission restoration, only noexcept operations run.
        RuntimeAccess::Slot prepared;
        prepared.plan = impl_;
        prepared.profile = p;
        prepared.base = module.base;
        std::vector<purenative::Handle> handles;
        const std::vector<purenative::Redirect> redirects = {
            {module.base + p.factory_rva, RuntimeAccess::factories(p.factory_sret, sequence)[i],
             &s.factory},
            {module.base + p.map_string_rva, RuntimeAccess::symbols(sequence)[i], &s.symbol},
            {module.base + p.destructor_rva, RuntimeAccess::destructors(sequence)[i],
             &s.destructor},
            {module.base + p.lazy_string_rva,
             RuntimeAccess::lazies(p.lazy_has_runtime, sequence)[i], &s.lazy},
            {module.base + p.symbol_handle_rva, RuntimeAccess::handles(sequence)[i], &s.handle}};
        InstallResult success{InstallState::installed, "installed; attach before bundle execution",
                              true};
        InstallResult active_error{InstallState::protection_error,
                                   "hooks active but page protection restoration failed", true};
        InstallResult failure{InstallState::rejected, "native entry installation failed"};
        InstallResult write_failure{InstallState::rejected, "dispatch page not writable"};
        static_assert(noexcept(s = std::move(prepared)));
        s = std::move(prepared);
        if (mprotect(reinterpret_cast<void*>(page), page_size, protection | PROT_WRITE)) {
            s = {};
            return write_failure;
        }
        bool ok = purenative::redirect_all(redirects, &handles);
        if (ok) {
            impl_->sealed = true;
            __atomic_store_n(slot, stubs[i], __ATOMIC_RELEASE);
        }
        bool restored = mprotect(reinterpret_cast<void*>(page), page_size, protection) == 0;
        if (!ok) {
            s = {};
            if (!restored) failure.state = InstallState::protection_error;
            return failure;
        }
        return restored ? std::move(success) : std::move(active_error);
    } catch (const std::exception& e) {
        return {InstallState::rejected, e.what()};
    }
}
}  // namespace purehermes
#else
namespace purehermes {
HermesModule module_at(uintptr_t) { throw std::runtime_error("requires Android ARM64"); }
}  // namespace purehermes
#endif
