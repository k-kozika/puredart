#include <nativehook/hook.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

namespace {
void expect(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
uint32_t read(uintptr_t address) { return *reinterpret_cast<const uint32_t*>(address); }
bool mapped(uintptr_t address, size_t size) {
    std::vector<unsigned char> residency(size / sysconf(_SC_PAGESIZE));
    return mincore(reinterpret_cast<void*>(address & ~(size - 1)), size, residency.data()) == 0;
}
}
void run(size_t size) {
    using namespace nativehook;
    testing::page_size(size);
    void* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    auto* code = reinterpret_cast<uint32_t*>((reinterpret_cast<uintptr_t>(mapping) + size - 1) & ~(size - 1));
    expect(mapping != MAP_FAILED, "allocate test code");
    code[0] = 0xa9bf7bfd; // stp fp,lr,[sp,#-16]!
    code[1] = kRet;
    code[8] = kBtiC;
    code[9] = 0xf81f0fe0; // str x0,[sp,#-16]!
    code[10] = kRet;
    code[16] = kBtiC; // replacement
    code[17] = kRet;
    code[24] = 0x10000000; // adr (unsupported relocation)
    code[32] = 0xd503233f; // paciasp
    code[40] = 0xa9bf79fd; // stp fp,lr,[x15,#-16]!
    expect(mprotect(code, size, PROT_READ | PROT_EXEC) == 0, "protect test code");
    const uintptr_t base = reinterpret_cast<uintptr_t>(code);
    const uintptr_t replacement = base + 64;
    uintptr_t original = 0;
    std::vector<Handle> handles;
    expect(!redirect_all({{0, replacement}}, &handles), "reject null target");
    expect(!redirect_all({{base + 1, replacement}}, &handles), "reject unaligned target");
    expect(!redirect_all({{base, 4}}, &handles), "reject unmapped replacement");
    expect(!redirect_all({{base, replacement}, {base, replacement}}, &handles), "reject duplicate batch");
    expect(!redirect_all({{base, replacement}, {base + 96, replacement, &original}}, &handles),
           "reject unsupported prologue before modifying batch");
    expect(read(base) == 0xa9bf7bfd && original == 0, "failed batch unchanged");
    expect(!redirect_all({{base + 128, replacement}}, &handles), "reject unsupported PAC");
    expect(!redirect_all({{base + 160, replacement, &original}}, &handles), "reject mismatched stack register");
    expect(!redirect_all({{base, replacement, nullptr, true, 32}}, &handles), "reject invalid stack register");
    testing::fail_next(testing::Failure::allocation);
    expect(!redirect_all({{base, replacement, &original}}, &handles), "allocation failure");
    testing::fail_next(testing::Failure::writable);
    expect(!redirect_all({{base, replacement, &original}}, &handles), "protection failure");
    expect(read(base) == 0xa9bf7bfd && original == 0 && handles.empty(), "failures leave no patch/output");
    uintptr_t dart_original = 0;
    expect(redirect_all({{base, replacement, &original},
                         {base + 32, replacement, &dart_original, true}}, &handles), "batch install");
    expect(handles.size() == 2 && read(base + 32) == kBtiC, "retain original BTI");
    const uintptr_t veneer = branch_target(base, read(base));
    expect(original == veneer + 32 && read(original) == kBtiC, "original trampoline landing");
    expect(read(original + 8) == 0xa9bf7bfd &&
           branch_target(original + 12, read(original + 12)) == base + 4,
           "native trampoline replays prologue and branches directly");
    expect(read(dart_original + 4) == 0xa8c147f0 && read(dart_original + 8) == 0xf81f0fe0 &&
           branch_target(dart_original + 12, read(dart_original + 12)) == base + 40,
           "Dart trampoline restores IP frame and resumes after BTI/prologue");
    expect(read(dart_original - 32) == 0xa9bf47f0, "Dart veneer saves IP registers");
    uintptr_t unchanged = 17;
    expect(!redirect_all({{base, replacement, &unchanged}}) && unchanged == 17, "reject existing patch");
    expect(!nativehook::remove({0}), "reject invalid handle");
    testing::fail_next(testing::Failure::writable);
    expect(!nativehook::remove(handles[0]) && read(base) != 0xa9bf7bfd, "failed removal leaves hook intact");
    expect(nativehook::remove(handles[0]) && read(base) == 0xa9bf7bfd, "restore original instruction");
    expect(mapped(veneer, size), "shared veneer remains for second hook");
    expect(!nativehook::remove(handles[0]), "reject stale handle");
    expect(nativehook::remove(handles[1]) && read(base + 36) == 0xf81f0fe0, "restore BTI-adjacent instruction");
    expect(!mapped(veneer, size), "last removal releases veneer");
    handles.clear();
    expect(redirect_all({{base, replacement, &original}}, &handles), "reinstall removed entry");
    expect(nativehook::remove(handles[0]), "remove reinstalled hook");
    handles.clear();
    expect(redirect_all({{base + 160, replacement, &dart_original, true, 15}}, &handles),
           "install alternate-stack hook");
    expect(read(dart_original - 32) == 0xa9bf45f0 && read(dart_original + 4) == 0xa8c145f0 &&
           read(dart_original + 8) == 0xa9bf79fd,
           "alternate-stack veneer and trampoline preserve IP registers through x15");
    expect(nativehook::remove(handles[0]) && read(base + 160) == 0xa9bf79fd,
           "remove alternate-stack hook");
    munmap(mapping, size * 2);
}

int main() { run(4096); run(16384); std::puts("nativehook 4 KiB/16 KiB tests passed"); }
