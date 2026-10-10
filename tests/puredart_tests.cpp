#include "puredart/puredart.h"
#include "PuredartNaming.h"

#include <array>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

template <size_t Size, typename T>
void store(std::array<unsigned char, Size>& bytes, size_t offset, T value) {
    expect(offset + sizeof(value) <= bytes.size(), "fixture write is in bounds");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

template <size_t Size>
puredart::vm::Object object_from(std::array<unsigned char, Size>& bytes, uint32_t cid,
                                 const puredart::vm::MemoryMap& map) {
    const auto& layout = puredart::meta::puredart_bundle().vm;
    store(bytes, 0, cid << layout.class_id_shift);
    const uintptr_t address = reinterpret_cast<uintptr_t>(bytes.data());
    expect((address & puredart::vm::kHeapObjectTag) == 0, "fixture object is aligned");
    return puredart::vm::Object(address | puredart::vm::kHeapObjectTag, 0, &map);
}

}  // namespace

int main() {
    using namespace puredart;
    expect(meta::validate(meta::puredart_bundle()), "fixture validates");
    meta::Bundle unsupported_layout = meta::puredart_bundle();
    unsupported_layout.vm.compressed_word_size = 8;
    expect(!meta::validate(unsupported_layout), "non-compressed-pointer layout is rejected");
    std::array<meta::LookupIndex, 3> invalid_class_index{};
    std::memcpy(invalid_class_index.data(), meta::puredart_bundle().classes_by_name,
                sizeof(invalid_class_index));
    invalid_class_index[0].index = meta::puredart_bundle().class_count;
    meta::Bundle invalid_index_bundle = meta::puredart_bundle();
    invalid_index_bundle.classes_by_name = invalid_class_index.data();
    expect(!meta::validate(invalid_index_bundle), "invalid class lookup index is rejected");
#if defined(PUREDART_TESTING)
    expect(install(), "host install validates metadata");
    expect(installed(), "installed state is retained");
#endif
    expect(meta::resolve("BillingType::fromCode") == 0x5e11ac, "symbol lookup");
    expect(meta::resolve("dart:core::_StringBase::length") == 0x1234,
           "dart symbol lookup");
    expect(meta::resolve("missing") == 0, "missing symbol");
    expect(meta::constant("BillingType", "name", "premium") == 0xb8e1f1,
           "constant lookup");
    expect(meta::constant("BillingType", "name", "free") == 0,
           "missing constant");

    const meta::ClassInfo* cls = meta::class_of("BillingType");
    expect(cls != nullptr && cls->cid == 6735, "class lookup");
    expect(meta::class_by_id(6735) == cls && meta::class_by_id(999999) == nullptr,
           "binary class ID lookup");
    HookContext context{};
    context.x[0] = 1;
    context.x[28] = 0x12345678;
    expect(context.arg(0) == 1 && context.heap_register() == 0x12345678,
           "hook context reads registers");
    expect(context.set_arg(2, 99) && context.arg(2) == 99 &&
               !context.set_arg(8, 100),
           "hook context updates Dart argument registers");
    context.set_return(42);
    expect(context.has_return_override() && context.x[0] == 42,
           "hook context overrides return");
    HookContext fp_context{};
    fp_context.set_fp_return(3.25);
    double fp_return = 0;
    std::memcpy(&fp_return, fp_context.q[0], sizeof(fp_return));
    expect(fp_context.has_return_override() && fp_return == 3.25,
           "hook context overrides floating-point return");

    expect(vm::smi_value(84) == 42, "Smi decode");
    expect(vm::untag(0x1001) == 0x1000, "heap tag removal");
    expect(vm::decompress(0xb8e1f1, 0x700000000000ULL) == 0x700000b8e1f1ULL,
           "compressed pointer decode");
    expect(vm::Object::from_compressed(0xb8e1f1, 0x700000000000ULL).tagged() ==
               0x700000b8e1f1ULL,
           "compressed object construction is explicit");
    expect(vm::Object(0xb8e1f1, 0x700000000000ULL).tagged() == 0xb8e1f1,
           "register object is not heuristically decompressed");
#if defined(PUREDART_TESTING)
    expect(hook_capacity() >= 1 && hook_count() == 0, "hook slots available");
    expect(!unhook(0x1234), "host unhook is unavailable");
#endif

    const vm::MemoryMap map = vm::MemoryMap::read();
    const auto& layout = meta::puredart_bundle().vm;

    expect(!vm::Object{}.valid(), "absent object is invalid");
    expect(vm::Object(0, 0).integer() == std::optional<int64_t>(0), "zero Smi is valid");
    expect(vm::Object::from_compressed(0xfffffffeu, 0).integer() ==
               std::optional<int64_t>(-1), "negative compressed Smi");
    meta::Bundle old_bundle = meta::puredart_bundle();
    old_bundle.meta_version = 3;
    expect(!meta::validate(old_bundle), "old metadata version is rejected");
    alignas(16) std::array<unsigned char, 64> number_bytes{};
    auto mint = object_from(number_bytes, layout.cid_mint, map);
    store(number_bytes, layout.instance_data_offset, int64_t{-42});
    expect(mint.integer() == std::optional<int64_t>(-42), "boxed integer");
    auto real = object_from(number_bytes, layout.cid_double, map);
    store(number_bytes, layout.instance_data_offset, 3.25);
    expect(real.number() == std::optional<double>(3.25), "boxed double");
    auto boolean = object_from(number_bytes, layout.cid_bool, map);
    store(number_bytes, layout.instance_data_offset, uint8_t{1});
    expect(boolean.boolean() == std::optional<bool>(true), "boxed bool");

    alignas(16) std::array<unsigned char, 64> latin1_bytes{};
    vm::Object latin1 = object_from(latin1_bytes, layout.cid_string, map);
    store(latin1_bytes, layout.string_length_offset, uint32_t{4});  // Smi(2)
    latin1_bytes[layout.one_byte_string_data_offset] = 'A';
    latin1_bytes[layout.one_byte_string_data_offset + 1] = 0xe9;
    expect(latin1.string() == std::optional<std::string>("A\xc3\xa9"),
           "OneByteString Latin-1 is converted to UTF-8");

    alignas(16) std::array<unsigned char, 64> utf16_bytes{};
    vm::Object utf16 = object_from(utf16_bytes, layout.cid_two_byte_string, map);
    store(utf16_bytes, layout.string_length_offset, uint32_t{6});  // Smi(3)
    store(utf16_bytes, layout.two_byte_string_data_offset, uint16_t{'A'});
    store(utf16_bytes, layout.two_byte_string_data_offset + 2, uint16_t{0xd83d});
    store(utf16_bytes, layout.two_byte_string_data_offset + 4, uint16_t{0xde00});
    expect(utf16.string() == std::optional<std::string>("A\xf0\x9f\x98\x80"),
           "TwoByteString surrogate pair is converted to UTF-8");

    alignas(16) std::array<unsigned char, 64> invalid_utf16_bytes{};
    vm::Object invalid_utf16 =
        object_from(invalid_utf16_bytes, layout.cid_two_byte_string, map);
    store(invalid_utf16_bytes, layout.string_length_offset, uint32_t{2});  // Smi(1)
    store(invalid_utf16_bytes, layout.two_byte_string_data_offset, uint16_t{0xd83d});
    expect(invalid_utf16.string() == std::optional<std::string>("\xef\xbf\xbd"),
           "unpaired surrogate is replaced");

    expect(blutter::function_name("app$main", "Widget", "build", false) ==
               "Widget::build",
           "short method name");
    expect(blutter::function_name("app$main", "Widget", "build", true) ==
               "app$main$Widget::build",
           "qualified method name");
    expect(blutter::function_name("app$main", "", "main", false) == "main",
           "short top-level function name");
    expect(blutter::function_name("app$main", "", "main", true) == "app$main$main",
           "qualified top-level function name");

#if defined(PUREDART_TESTING)
    constexpr size_t kTestSlot = 0;
    std::atomic<size_t> callback_calls{0};
    std::atomic<size_t> ready_dispatchers{0};
    std::atomic<bool> stop_dispatchers{false};
    std::vector<std::thread> dispatchers;
    for (size_t i = 0; i < 4; ++i) {
        dispatchers.emplace_back([&] {
            HookContext concurrent_context{};
            ready_dispatchers.fetch_add(1, std::memory_order_release);
            while (!stop_dispatchers.load(std::memory_order_acquire)) {
                detail::dispatch_hook(kTestSlot, concurrent_context);
            }
        });
    }
    while (ready_dispatchers.load(std::memory_order_acquire) != dispatchers.size()) {
        std::this_thread::yield();
    }
    expect(detail::publish_hook_callback_for_testing(
               kTestSlot, [&](HookContext&) { callback_calls.fetch_add(1); }),
           "hook callback publication");
    for (size_t attempt = 0; attempt < 100000 && callback_calls.load() < 1000; ++attempt) {
        std::this_thread::yield();
    }
    expect(callback_calls.load() != 0, "published callback becomes visible to dispatchers");
    stop_dispatchers.store(true, std::memory_order_release);
    for (std::thread& dispatcher : dispatchers) dispatcher.join();
    const size_t calls_before_clear = callback_calls.load();
    detail::clear_hook_callback_for_testing(kTestSlot);
    HookContext cleared_context{};
    detail::dispatch_hook(kTestSlot, cleared_context);
    expect(callback_calls.load() == calls_before_clear, "cleared callback is not dispatched");

    std::atomic<bool> callback_entered{false};
    std::atomic<bool> release_callback{false};
    std::atomic<bool> clear_started{false};
    std::atomic<bool> clear_finished{false};
    expect(detail::publish_hook_callback_for_testing(kTestSlot, [&](HookContext&) {
               callback_entered.store(true, std::memory_order_release);
               while (!release_callback.load(std::memory_order_acquire)) {
                   std::this_thread::yield();
               }
           }),
           "in-flight callback publication");
    std::thread in_flight_dispatcher([&] {
        HookContext in_flight_context{};
        detail::dispatch_hook(kTestSlot, in_flight_context);
    });
    while (!callback_entered.load(std::memory_order_acquire)) std::this_thread::yield();
    std::thread clearer([&] {
        clear_started.store(true, std::memory_order_release);
        detail::clear_hook_callback_for_testing(kTestSlot);
        clear_finished.store(true, std::memory_order_release);
    });
    while (!clear_started.load(std::memory_order_acquire)) std::this_thread::yield();
    for (size_t attempt = 0; attempt < 1000; ++attempt) std::this_thread::yield();
    expect(!clear_finished.load(std::memory_order_acquire),
           "callback storage remains alive while dispatch is active");
    release_callback.store(true, std::memory_order_release);
    in_flight_dispatcher.join();
    clearer.join();
    expect(clear_finished.load(std::memory_order_acquire),
           "callback storage is released after dispatch finishes");

    expect(detail::publish_hook_callback_for_testing(
               kTestSlot, [&](HookContext&) { callback_calls.fetch_add(1); }),
           "released hook slot is reusable");
    detail::dispatch_hook(kTestSlot, cleared_context);
    expect(callback_calls.load() == calls_before_clear + 1, "reused hook slot dispatches");
    detail::clear_hook_callback_for_testing(kTestSlot);
#endif

    std::cout << "puredart tests passed\n";
    return 0;
}
