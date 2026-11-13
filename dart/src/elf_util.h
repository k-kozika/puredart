#pragma once

#if defined(__linux__) || defined(__ANDROID__)
#include <elf.h>
#include <link.h>
#endif

#include <cstdint>
#include <cstring>

namespace puredart::elf {

#if defined(__linux__) || defined(__ANDROID__)

inline bool ends_with(const char* value, const char* suffix) {
    const size_t length = std::strlen(value);
    const size_t suffix_length = std::strlen(suffix);
    return length >= suffix_length &&
           std::memcmp(value + length - suffix_length, suffix, suffix_length) == 0;
}

inline uintptr_t to_runtime(uintptr_t base, ElfW(Addr) value) {
    const uintptr_t address = static_cast<uintptr_t>(value);
    return address < base ? base + address : address;
}

struct Module {
    uintptr_t base{};
    uintptr_t text_start{};
    uintptr_t text_end{};
    const ElfW(Dyn)* dynamic{};
    char path[512]{};
};

struct FindContext {
    const char* suffix;
    Module* module;
};

inline int find_module_callback(dl_phdr_info* info, size_t, void* data) {
    auto* context = static_cast<FindContext*>(data);
    if (info->dlpi_name == nullptr || !ends_with(info->dlpi_name, context->suffix)) return 0;
    Module& module = *context->module;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)& phdr = info->dlpi_phdr[i];
        if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X) != 0) {
            module.text_start = info->dlpi_addr + phdr.p_vaddr;
            module.text_end = module.text_start + phdr.p_memsz;
        } else if (phdr.p_type == PT_DYNAMIC) {
            module.dynamic = reinterpret_cast<const ElfW(Dyn)*>(info->dlpi_addr + phdr.p_vaddr);
        }
    }
    if (module.dynamic == nullptr) return 0;
    module.base = static_cast<uintptr_t>(info->dlpi_addr);
    std::strncpy(module.path, info->dlpi_name, sizeof(module.path) - 1);
    return 1;
}

inline bool find_module(const char* suffix, Module* result) {
    if (result == nullptr) return false;
    *result = {};
    FindContext context{suffix, result};
    dl_iterate_phdr(find_module_callback, &context);
    return result->base != 0;
}

inline const ElfW(Dyn)* dynamic_entry(const Module& module, ElfW(Sword) tag) {
    for (const ElfW(Dyn)* item = module.dynamic;
         item != nullptr && item->d_tag != DT_NULL; ++item) {
        if (item->d_tag == tag) return item;
    }
    return nullptr;
}

inline uintptr_t dynamic_pointer(const Module& module, ElfW(Sword) tag) {
    const ElfW(Dyn)* item = dynamic_entry(module, tag);
    return item == nullptr ? 0 : to_runtime(module.base, item->d_un.d_ptr);
}

inline uintptr_t lookup_plt_slot(const Module& module, const char* name) {
    const auto* symbols = reinterpret_cast<const ElfW(Sym)*>(dynamic_pointer(module, DT_SYMTAB));
    const char* strings = reinterpret_cast<const char*>(dynamic_pointer(module, DT_STRTAB));
    const auto* relocations = reinterpret_cast<const ElfW(Rela)*>(
        dynamic_pointer(module, DT_JMPREL));
    const ElfW(Dyn)* size = dynamic_entry(module, DT_PLTRELSZ);
    if (symbols == nullptr || strings == nullptr || relocations == nullptr || size == nullptr) {
        return 0;
    }
    const size_t count = size->d_un.d_val / sizeof(ElfW(Rela));
    for (size_t i = 0; i < count; ++i) {
#if defined(__LP64__)
        const uint32_t symbol_index = ELF64_R_SYM(relocations[i].r_info);
#else
        const uint32_t symbol_index = ELF32_R_SYM(relocations[i].r_info);
#endif
        if (std::strcmp(strings + symbols[symbol_index].st_name, name) == 0) {
            return to_runtime(module.base, relocations[i].r_offset);
        }
    }
    return 0;
}

#else

struct Module { uintptr_t base{}; };
inline bool find_module(const char*, Module*) { return false; }
inline uintptr_t lookup_plt_slot(const Module&, const char*) { return 0; }

#endif

}  // namespace puredart::elf
