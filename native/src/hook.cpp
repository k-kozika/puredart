#include "nativehook/hook.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

#if defined(__ANDROID__)
#include <android/log.h>
#endif
#if defined(__ANDROID__) || defined(NATIVEHOOK_TESTING)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace nativehook {
#if (defined(__ANDROID__) && defined(__aarch64__)) || defined(NATIVEHOOK_TESTING)
namespace {
constexpr size_t kStride = 64;
struct Batch {
    void* page = MAP_FAILED;
    size_t size{};
    ~Batch() { if (page != MAP_FAILED) munmap(page, size); }
};
struct Patch {
    uintptr_t entry;
    uintptr_t site;
    uint32_t saved;
    uint32_t branch;
    int protection;
    std::shared_ptr<Batch> batch;
};
std::mutex mutex;
std::unordered_map<uint64_t, Patch> patches;
uint64_t next_id = 1;
#if defined(NATIVEHOOK_TESTING)
testing::Failure failure{};
size_t test_page_size{};
#endif

void log_error(const char* message) {
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_ERROR, "NativeHook", "%s", message);
#else
    std::fprintf(stderr, "nativehook: %s\n", message);
#endif
}

bool code_range(uintptr_t address, size_t length, int* protection) {
    if (address == 0 || address + length < address) return false;
    // smaps exposes BTI via VmFlags ("bt"); maps only exposes r/w/x. Keep
    // PROT_BTI when changing permissions, because mprotect clears it otherwise.
    // https://github.com/torvalds/linux/blob/master/mm/mprotect.c
    FILE* file = std::fopen("/proc/self/smaps", "re");
    if (!file) return false;
    char line[512];
    bool in_range = false;
    bool found = false;
    while (std::fgets(line, sizeof(line), file)) {
        unsigned long long start, end;
        char access[8]{};
        if (std::sscanf(line, "%llx-%llx %7s", &start, &end, access) == 3) {
            in_range = address >= start && address + length <= end &&
                       access[0] == 'r' && access[2] == 'x';
            if (in_range)
                *protection = PROT_READ | PROT_EXEC | (access[1] == 'w' ? PROT_WRITE : 0);
        } else if (in_range && std::strncmp(line, "VmFlags:", 8) == 0) {
#if defined(__aarch64__)
            // Linux's ARM64 UAPI value; also builds with older NDK headers.
            constexpr int bti_protection = 0x10;
            if (std::strstr(line, " bt ")) *protection |= bti_protection;
#endif
            found = true;
            break;
        }
    }
    std::fclose(file);
    return found;
}

bool reachable(uintptr_t from, uintptr_t to) {
    const int64_t delta = static_cast<int64_t>(to) - static_cast<int64_t>(from);
    return (delta & 3) == 0 && delta >= -0x08000000 && delta < 0x08000000;
}
uint32_t branch(uintptr_t from, uintptr_t to) {
    const int64_t delta = static_cast<int64_t>(to) - static_cast<int64_t>(from);
    return 0x14000000 | (static_cast<uint32_t>(delta / 4) & 0x03ffffff);
}
void word(char* buffer, size_t offset, uint32_t value) {
    std::memcpy(buffer + offset, &value, 4);
}
void flush(uintptr_t start, size_t length) {
    __builtin___clear_cache(reinterpret_cast<char*>(start), reinterpret_cast<char*>(start + length));
}

bool stack_save(uint32_t instruction, uint8_t stack_register) {
    const uint32_t base = static_cast<uint32_t>(stack_register) << 5;
    return (instruction & 0xffc003e0) == (0xa9800000 | base) ||
           (instruction & 0xffe00fe0) == (0xf8000c00 | base);
}
bool pac(uint32_t instruction) {
    return instruction == 0xd503233f || instruction == 0xd503237f; // paciasp/pacibsp
}
struct Page { uintptr_t address; int protection; };
bool make_writable(const std::vector<Page>& pages, size_t size) {
    size_t writable = 0;
    for (const auto& page : pages) {
        bool failed = false;
#if defined(NATIVEHOOK_TESTING)
        if (failure == testing::Failure::writable) { failure = {}; failed = true; }
#endif
        if (failed || mprotect(reinterpret_cast<void*>(page.address), size,
                                page.protection | PROT_WRITE) != 0) {
            for (size_t i = 0; i < writable; ++i)
                mprotect(reinterpret_cast<void*>(pages[i].address), size, pages[i].protection);
            return false;
        }
        ++writable;
    }
    return true;
}
void restore_permissions(const std::vector<Page>& pages, size_t size) {
    for (const auto& page : pages) {
        if (mprotect(reinterpret_cast<void*>(page.address), size, page.protection) != 0)
            log_error("could not restore code page permissions");
    }
}
}  // namespace

bool redirect_all(const std::vector<Redirect>& redirects, std::vector<Handle>* handles) noexcept {
    try {
        std::lock_guard lock(mutex);
        if (handles && !handles->empty()) return false;
        if (redirects.empty()) return true;
        const long system_size = sysconf(_SC_PAGESIZE);
        if (system_size <= 0) return false;
        size_t size = static_cast<size_t>(system_size);
#if defined(NATIVEHOOK_TESTING)
        if (test_page_size) size = test_page_size;
#endif
        if ((size & (size - 1)) != 0 || redirects.size() > size / kStride) return false;
        std::vector<Patch> prepared;
        std::vector<Page> pages;
        std::vector<Handle> ids;
        prepared.reserve(redirects.size());
        ids.reserve(redirects.size());
        for (const auto& redirect : redirects) {
            int protection = 0, replacement_protection = 0;
            if (redirect.stack_register > 31 || (redirect.entry & 3) || (redirect.replacement & 3) ||
                !code_range(redirect.entry, 4, &protection) ||
                !code_range(redirect.replacement, 4, &replacement_protection)) return false;
            uintptr_t site = redirect.entry;
            uint32_t first = *reinterpret_cast<const uint32_t*>(site);
            if (first == kBtiC || first == 0xd503249f || first == 0xd50324df) {
                site += 4;
                if (!code_range(site, 4, &protection)) return false;
                first = *reinterpret_cast<const uint32_t*>(site);
            }
            if (pac(first) || (redirect.original && !stack_save(first, redirect.stack_register))) return false;
            if (std::any_of(prepared.begin(), prepared.end(), [site](const Patch& p) { return p.site == site; }) ||
                std::any_of(patches.begin(), patches.end(), [site](const auto& p) { return p.second.site == site; }))
                return false;
            prepared.push_back({redirect.entry, site, first, 0, protection, {}});
            const uintptr_t page = site & ~(size - 1);
            if (std::none_of(pages.begin(), pages.end(), [page](const Page& p) { return p.address == page; }))
                pages.push_back({page, protection});
        }
        auto batch = std::make_shared<Batch>();
        batch->size = size;
#if defined(NATIVEHOOK_TESTING)
        if (failure == testing::Failure::allocation) { failure = {}; return false; }
#endif
        const uintptr_t origin = prepared.front().site & ~(size - 1);
        constexpr int fixed_no_replace = 0x100000;
        for (int64_t distance = 0x10000; distance < 0x08000000 && batch->page == MAP_FAILED; distance += 0x10000) {
            for (int direction : {-1, 1}) {
                const int64_t hint = static_cast<int64_t>(origin) + direction * distance;
                if (hint <= 0) continue;
                void* candidate = mmap(reinterpret_cast<void*>(hint), size, PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE | MAP_ANONYMOUS | fixed_no_replace, -1, 0);
                if (candidate == MAP_FAILED) continue;
                // Older kernels may ignore MAP_FIXED_NOREPLACE. Verify the
                // returned mapping and both branch directions explicitly.
                const uintptr_t address = reinterpret_cast<uintptr_t>(candidate);
                bool valid = address == static_cast<uintptr_t>(hint);
                for (size_t i = 0; i < prepared.size(); ++i) {
                    valid = valid && reachable(prepared[i].site, address + i * kStride) &&
                        reachable(address + i * kStride + 44, prepared[i].site + 4);
                }
                if (valid) { batch->page = candidate; break; }
                munmap(candidate, size);
            }
        }
        if (batch->page == MAP_FAILED) return false;
        for (size_t i = 0; i < prepared.size(); ++i) {
            char* veneer = static_cast<char*>(batch->page) + i * kStride;
            // A native replacement uses the ABI scratch x16. A Dart adapter
            // consumes the saved IP frame instead, preserving its input values.
            const uint32_t stack_base = static_cast<uint32_t>(redirects[i].stack_register) << 5;
            word(veneer, 0, redirects[i].preserve_ip ? (0xa9bf4410 | stack_base) : kNop);
            word(veneer, 4, 0x58000070); // ldr x16, #12 (literal at +16)
            word(veneer, 8, 0xd61f0200); // br x16 (BTI call-compatible)
            word(veneer, 12, kNop);
            std::memcpy(veneer + 16, &redirects[i].replacement, sizeof(uintptr_t));
            word(veneer, 32, kBtiC);
            word(veneer, 36, redirects[i].preserve_ip ? (0xa8c14410 | stack_base) : kNop);
            word(veneer, 40, prepared[i].saved);
            word(veneer, 44, branch(reinterpret_cast<uintptr_t>(veneer + 44), prepared[i].site + 4));
            prepared[i].branch = branch(prepared[i].site, reinterpret_cast<uintptr_t>(veneer));
            prepared[i].batch = batch;
        }
        flush(reinterpret_cast<uintptr_t>(batch->page), redirects.size() * kStride);
        if (mprotect(batch->page, size, PROT_READ | PROT_EXEC) != 0) return false;
        // Allocate registry nodes and outputs before touching executable code.
        try {
            for (const Patch& patch : prepared) {
                const uint64_t id = next_id++;
                patches.emplace(id, patch);
                ids.push_back({id});
            }
        } catch (...) {
            for (Handle id : ids) patches.erase(id.id);
            throw;
        }
        if (!make_writable(pages, size)) {
            for (Handle id : ids) patches.erase(id.id);
            return false;
        }
        // All original pointers are published before any entry becomes callable.
        for (size_t i = 0; i < redirects.size(); ++i) {
            if (redirects[i].original)
                *redirects[i].original = reinterpret_cast<uintptr_t>(batch->page) + i * kStride + 32;
        }
        for (const Patch& patch : prepared) {
            __atomic_store_n(reinterpret_cast<uint32_t*>(patch.site), patch.branch, __ATOMIC_RELEASE);
            flush(patch.site, 4);
        }
        restore_permissions(pages, size);
        if (handles) handles->swap(ids);
        return true;
    } catch (...) { return false; }
}

bool remove(Handle handle) noexcept {
    try {
        std::lock_guard lock(mutex);
        auto found = patches.find(handle.id);
        if (found == patches.end()) return false;
        const Patch& patch = found->second;
        int protection = 0;
        if (!code_range(patch.site, 4, &protection)) return false;
        if (*reinterpret_cast<const uint32_t*>(patch.site) != patch.branch) return false;
        const size_t size = patch.batch->size;
        const std::vector<Page> pages{{patch.site & ~(size - 1), patch.protection}};
        if (!make_writable(pages, size)) return false;
        __atomic_store_n(reinterpret_cast<uint32_t*>(patch.site), patch.saved, __ATOMIC_RELEASE);
        flush(patch.site, 4);
        restore_permissions(pages, size);
        patches.erase(found); // Last handle releases the shared veneer page.
        return true;
    } catch (...) { return false; }
}
#if defined(NATIVEHOOK_TESTING)
void testing::fail_next(Failure value) noexcept { std::lock_guard lock(mutex); failure = value; }
void testing::page_size(size_t value) noexcept { std::lock_guard lock(mutex); test_page_size = value; }
#endif
#else
bool redirect_all(const std::vector<Redirect>& redirects, std::vector<Handle>* handles) noexcept {
    if (handles && !handles->empty()) return false;
    return redirects.empty();
}
bool remove(Handle) noexcept { return false; }
#endif
}  // namespace nativehook
