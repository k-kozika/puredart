#include <dlfcn.h>
#include <jni.h>
#include <jsi/jsi.h>
#include <purenative/hook.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include "hermes/hermes.h"
#include "profile.h"
#include "purehermes/purehermes.h"
using namespace facebook;
static void check(bool value, const char* text) {
    if (!value) throw std::runtime_error(text);
}
extern "C" long ph_stack_target(long);
extern "C" long ph_load_target(const long*);
static uintptr_t stack_original = 0, load_original = 0;
static long stack_replace(long x) {
    return reinterpret_cast<long (*)(long)>(stack_original)(x) + 9;
}
static long load_replace(const long* x) {
    return reinterpret_cast<long (*)(const long*)>(load_original)(x) + 9;
}
static void native_entry_tests() {
    long zero = 0;
    check(ph_stack_target(0) == 1 && ph_load_target(&zero) == 1, "native baseline");
    std::vector<purenative::Handle> handles;
    check(purenative::redirect_all({{reinterpret_cast<uintptr_t>(&ph_stack_target),
                                     reinterpret_cast<uintptr_t>(&stack_replace), &stack_original},
                                    {reinterpret_cast<uintptr_t>(&ph_load_target),
                                     reinterpret_cast<uintptr_t>(&load_replace), &load_original}},
                                   &handles),
          "native SUB/LDR install");
    check(ph_stack_target(0) == 10 && ph_load_target(&zero) == 10,
          "native SUB/LDR trampoline execution");
    for (auto h : handles) check(purenative::remove(h), "native entry removal");
    check(ph_stack_target(0) == 1 && ph_load_target(&zero) == 1, "native entry restoration");
}
static std::string run(const char* path) {
    native_entry_tests();
    auto b = purehermes::Bundle::read_file(path);
    auto load = [&](jsi::Runtime& rt) {
        std::string code(reinterpret_cast<const char*>(b.bytes().data()), b.bytes().size());
        auto buffer = std::make_shared<jsi::StringBuffer>(code);
        rt.evaluateJavaScript(buffer, "fixture.hbc");
        return buffer;
    };
    auto call = [](jsi::Runtime& rt, const char* name, double arg) {
        return rt.global().getPropertyAsFunction(rt, name).call(rt, arg).asNumber();
    };
    auto benchmark = [&](jsi::Runtime& rt) {
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 10000; ++i)
            check(call(rt, "untouched", i) == i + 1, "benchmark result");
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now() - start)
                   .count() /
               10000;
    };
    long long baseline = 0, hooked = 0;
    {
        auto rt = facebook::hermes::makeHermesRuntime();
        load(*rt);
        check(call(*rt, "update", 0) == 1, "baseline update");
        check(rt->global().getProperty(*rt, "title").asString(*rt).utf8(*rt) == "+1",
              "baseline title");
        baseline = benchmark(*rt);
    }
    auto hits = std::make_shared<std::atomic<size_t>>(0);
    purehermes::HookPlan plan(b);
    plan.on_return({b.function("update").id}, [hits](purehermes::ReturnContext& c) {
        ++*hits;
        c.set_number(c.number() + 9);
    });
    plan.on_return({b.function("branches").id}, [](purehermes::ReturnContext& c) {
        if (c.number() < 0) throw std::runtime_error("callback failure");
        c.set_number(c.number() * 2);
    });
    auto nonnumeric_hits = std::make_shared<std::atomic<size_t>>(0);
    plan.on_return({b.function("text").id},
                   [nonnumeric_hits](purehermes::ReturnContext&) { ++*nonnumeric_hits; });
    uint32_t string_id = 0;
    bool found = false;
    for (uint32_t i = 0; i < b.strings().size(); ++i)
        if (b.strings()[i] == "+1") {
            string_id = i;
            found = true;
        }
    check(found, "title string missing");
    plan.map_literal({string_id}, "+10");
    Dl_info info{};
    check(dladdr(reinterpret_cast<void*>(&facebook::hermes::makeHermesRuntime), &info) != 0,
          "VM address");
    auto module = purehermes::module_at(reinterpret_cast<uintptr_t>(info.dli_fbase));
    auto wrong = test_profile();
    wrong.build_id = "mismatched";
    check(!plan.install(module, wrong), "mismatch must be rejected");
    // Reach native backend preflight after the dispatch page has become writable.
    // A Ret handler begins with LDRB, which cannot be relocated as an entry.
    wrong = test_profile();
    wrong.lazy_string_rva = wrong.ret_handler_rva;
    wrong.lazy_instruction =
        *reinterpret_cast<const uint32_t*>(module.base + wrong.ret_handler_rva);
    const auto& updater = b.function("update");
    const uint8_t ret = b.bytes()[updater.offset + updater.returns.front()];
    auto* dispatch = reinterpret_cast<const uintptr_t*>(module.base + wrong.dispatch_table_rva +
                                                        size_t(ret) * 8);
    const uintptr_t original_ret = *dispatch;
    auto rejected = plan.install(module, wrong);
    check(!rejected && !rejected.hooks_active && *dispatch == original_ret,
          "native preflight failure leaves dispatch unchanged");
    auto installed = plan.install(module, test_profile());
    check(bool(installed), installed.message.c_str());
    auto start = std::chrono::steady_clock::now();
    for (int instance = 0; instance < 2; ++instance) {
        auto rt = facebook::hermes::makeHermesRuntime();
        auto buffer = load(*rt);
        check(rt->global().getProperty(*rt, "title").asString(*rt).utf8(*rt) == "+10",
              "mapped title");
        for (int n = 0; n < 17; ++n) {
            auto result = rt->global().getPropertyAsFunction(*rt, "click").call(*rt).asNumber();
            check(result == (n + 1) * 10, "React-style functional counter state");
        }
        check(rt->global().getPropertyAsFunction(*rt, "text").call(*rt).asString(*rt).utf8(*rt) ==
                  "unchanged",
              "nonnumeric return preserved");
        check(call(*rt, "untouched", 0) == 1, "deduplicated bytecode must remain untouched");
        check(call(*rt, "branches", 2) == 8, "positive return branch");
        check(call(*rt, "branches", -2) == -1, "callback exception retains original value");
        check(call(*rt, "recurse", 12) == 12, "unrelated recursion");
        bool thrown = false;
        try {
            rt->global().getPropertyAsFunction(*rt, "throws").call(*rt);
        } catch (const jsi::JSError&) {
            thrown = true;
        }
        check(thrown, "VM exception must propagate");
        check(std::isinf(call(*rt, "update", INFINITY)), "infinity");
        check(std::isnan(call(*rt, "update", NAN)), "NaN");
        for (int n = 0; n < 1000; ++n)
            check(call(*rt, "untouched", n) == n + 1, "unmatched stress");
        hooked = benchmark(*rt);
        check(buffer->size() == b.bytes().size() &&
                  !memcmp(buffer->data(), b.bytes().data(), buffer->size()),
              "VM HBC remains byte-identical");
    }
    check(nonnumeric_hits->load() == 0, "nonnumeric callbacks skipped");
    check(hits->load() == 38, "one callback per matched return");
    std::ostringstream out;
    out << "PASS HBC " << b.version() << " hits=" << hits->load()
        << " runtimes=2 hbc_unchanged=true page_size=" << sysconf(_SC_PAGESIZE)
        << " baseline_ns=" << baseline << " hooked_ns=" << hooked << " elapsed_ms="
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 start)
               .count();
    return out.str();
}
extern "C" JNIEXPORT jstring JNICALL Java_org_purehermes_test_MainActivity_run(JNIEnv* env, jclass,
                                                                               jstring path) {
    const char* p = env->GetStringUTFChars(path, nullptr);
    std::string result;
    try {
        result = run(p);
    } catch (const std::exception& e) {
        result = std::string("FAIL ") + e.what();
    }
    env->ReleaseStringUTFChars(path, p);
    return env->NewStringUTF(result.c_str());
}
