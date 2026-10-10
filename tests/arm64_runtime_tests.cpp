#include <puredart/puredart.h>
#include <nativehook/hook.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

namespace {
void expect(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
struct Output {
    uintptr_t result, x16, x17, x18, thr, pp, heap, after_sp, before_sp;
    double fp;
    uintptr_t dart_sp;
};
extern "C" void test_dart_target();
extern "C" void test_call_dart(Output*, uintptr_t, uintptr_t, double);
void registers(const Output& output) {
    expect(output.x16 == 0x1616 && output.x17 == 0x1717 && output.x18 == 0x1818,
           "IP/platform registers restored");
    expect(output.thr == 0x2626 && output.pp == 0x2727 && output.heap == 0x2828,
           "Dart reserved registers restored");
    expect(output.after_sp == output.before_sp, "CPU stack restored");
    expect(output.dart_sp == output.before_sp - 8, "Dart stack restored");
}
}
int main() {
    using namespace puredart;
    Output output{};
    const uintptr_t target = reinterpret_cast<uintptr_t>(&test_dart_target);
    test_call_dart(&output, 2, 3, 1.5);
    expect(output.result == 5, "unhooked target");
    expect(hook(target, [](HookContext& c) {
        expect(c.reg(16) == 0x1616 && c.reg(17) == 0x1717, "snapshot includes input IP values");
        expect(c.set_arg(0, 10), "set argument");
        // Stress C++ calls and allocation while the shim holds Dart registers.
        std::string scratch(128, 'x');
        expect(scratch.size() == 128, "callback runs C++");
    }), "install continuing hook");
    expect(!hook(target, [](HookContext&) {}), "reject duplicate hook");
    test_call_dart(&output, 2, 3, 1.5);
    expect(output.result == 13 && output.fp == 1.5, "modified arguments and FP input continue");
    registers(output);
    expect(unhook(target) && hook_count() == 0, "remove continuing hook");
    expect(hook(target, [](HookContext& c) { c.set_return(42); }), "reinstall overriding hook");
    test_call_dart(&output, 2, 3, 1.5);
    expect(output.result == 42, "integer return override");
    registers(output);
    expect(unhook(target), "remove override");
    expect(hook(target, [](HookContext& c) { c.set_fp_return(3.25); }), "install FP override");
    test_call_dart(&output, 2, 3, 1.5);
    expect(output.fp == 3.25, "FP return override");
    registers(output);
    expect(unhook(target), "remove FP override");
    expect(hook(target, [target](HookContext&) { expect(!unhook(target), "reject removal from callback"); }),
           "install callback removal test");
    test_call_dart(&output, 2, 3, 1.5);
    expect(unhook(target), "remove outside callback");

    const size_t size = sysconf(_SC_PAGESIZE);
    auto* code = static_cast<unsigned char*>(mmap(nullptr, size, PROT_READ | PROT_WRITE,
                                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    expect(code != MAP_FAILED, "allocate slot targets");
    const uint32_t instructions[] = {nativehook::kBtiC, 0xa9bf79fd, 0xd2800020, 0xa8c179fd, nativehook::kRet};
    for (size_t i = 0; i < hook_capacity() + 1; ++i) std::memcpy(code + i * 32, instructions, sizeof(instructions));
    __builtin___clear_cache(reinterpret_cast<char*>(code), reinterpret_cast<char*>(code + size));
    expect(mprotect(code, size, PROT_READ | PROT_EXEC) == 0, "protect slot targets");
    for (size_t i = 0; i < hook_capacity(); ++i)
        expect(hook(reinterpret_cast<uintptr_t>(code + i * 32), [](HookContext&) {}), "fill slots");
    expect(!hook(reinterpret_cast<uintptr_t>(code + hook_capacity() * 32), [](HookContext&) {}), "reject overflow");
    for (size_t i = 0; i < hook_capacity(); ++i) expect(unhook(reinterpret_cast<uintptr_t>(code + i * 32)), "release slots");
    test_call_dart(&output, 2, 3, 1.5);
    expect(output.result == 5, "original behavior restored");
    munmap(code, size);
    std::puts("puredart ARM64 runtime tests passed");
}
