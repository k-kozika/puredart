# puredart

A small C++17 adapter for hooking Android ARM64 Dart AOT function entries.
The implementation has three layers:

- `native/`: Dart-independent ARM64 inline hooks (`nativehook::core`).
- `puredart::core`: Dart register snapshots, argument/return changes, and basic value reads.
- Consumer: app-specific hook callbacks and generated Blutter metadata.

There is no external hook-engine dependency. Native consumers can add only
`native/` to their CMake project; they do not need Dart metadata or the adapter.

## Build

```sh
git clone --recurse-submodules https://github.com/k-kozika/puredart.git
cd puredart
cmake -S . -B build -DPUREDART_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Host tests inspect ARM64 patches in mapped instruction buffers without executing
ARM64 code, including simulated 4 KiB and 16 KiB page sizes. They also check
metadata, basic values, callback publication, CFI, BTI landings, and the
ShadowCallStack guard. Runtime hooks require Android ARM64.

Android integration:

```cmake
add_subdirectory(path/to/puredart puredart_build)
target_link_libraries(my_hook PRIVATE puredart::core)
puredart_add_meta(my_hook /absolute/path/to/blutter_meta.cpp)
```

Relative metadata paths resolve under `PUREDART_META_DIR` (default `meta/`).
Missing metadata is a configure-time error. `PUREDART_ENABLE_RUNTIME_HOOKS=OFF`
builds only metadata lookup and basic value reads.

For a native-only consumer:

```cmake
add_subdirectory(path/to/puredart/native nativehook_build)
target_link_libraries(my_native_hook PRIVATE nativehook::core)
```

## Hook behavior

```cpp
#include <puredart/puredart.h>

if (!puredart::install()) return; // Validate generated metadata.
puredart::bootstrap([](uintptr_t base) {
    const auto rva = puredart::meta::resolve("MyClass::method");
    if (!rva) return;
    puredart::hook(base + rva, [](puredart::HookContext& context) {
        context.set_return(42); // Supply the target's actual ABI representation.
    });
});
```

`bootstrap()` polls for `libapp.so` every 10 ms for up to two minutes. It does
not interpose the Android loader and does not guarantee catching the first
invocation during startup; install directly at a consumer-controlled load point
when that timing is required.

The callback runs before the Dart function. `set_arg()` changes x0..x7.
`set_return()` or `set_fp_return()` skips the function; otherwise the shim
continues through the original trampoline. There are 16 reusable hook slots.
Dart calling conventions vary by target: inspect the generated assembly to
choose registers and distinguish boxed values (for example Smi) from unboxed
integers. The adapter does not infer a Dart function signature.

The engine allocates nearby veneers and atomically writes one B instruction
per target. Original trampolines replay only supported `stp`/`str` stack-save
prologues, then branch directly to the original code. A leading BTI is retained;
unsupported prologues, PAC entry sequences, duplicate entries, and unavailable
nearby mappings are rejected before the batch is patched. This is an entry-hook
engine with a limited prologue contract, not a general instruction relocator.

Native hooks use `nativehook::redirect_all()` and optional `Handle` outputs.
A native replacement may use the ABI scratch register x16. The Dart adapter
uses the engine's `preserve_ip` contract with x15 as its stack register: the
veneer saves x16/x17, and the shim or original trampoline consumes that frame. C++ callbacks run on an aligned
native stack, with both x15 (Dart SP) and CPU SP restored on exit. Its trampoline
pointer must not be called directly from C++.

`unhook(address)` / `nativehook::remove(handle)` require the caller to ensure
that no thread is executing the target, replacement, callback, or trampoline,
and to prevent new calls until removal completes. Never unhook from a callback.
Removal restores the original instruction and frees a shared veneer page only
after its final handle is removed. Hooks installed without handle outputs live
until process exit. Protection-restoration failures are logged; once branches
are published they remain registered for removal, rather than reporting a
failed installation with untracked live code.

Keep callbacks bounded and non-blocking. They run synchronously on the Dart
thread; avoid I/O, contended locks, and VM re-entry. C++ callback code must be
compiled without ShadowCallStack because Dart does not maintain x18.
`puredart::core` propagates `-fno-sanitize=shadow-call-stack`; apply it separately
to callbacks compiled in other targets. The shim maintains CFI and BTI landing
pads and restores the Dart reserved registers x26/x27/x28.

## Values and generated metadata

`vm::Object` reads Smi, boxed integers, bool, double, and one/two-byte strings.
It also exposes CID and the tagged address. `from_compressed()` explicitly
interprets compressed fields/constants; register objects are already expanded.
An absent object is invalid, while Smi zero is a valid value.

Heap values and memory-map snapshots are callback-scoped. Do not retain them
across GC/isolate transitions. Memory-map checks are bounds checks, not GC
synchronization. The adapter offers no general fields, collection traversal,
heap writes, allocation, or Dart runtime invocation.

The [Blutter fork's `feat/metadata` branch](https://github.com/k-kozika/blutter/tree/feat/metadata)
is pinned to `bda5ec4a41aa3066a3982faa6fd0b8853d6d5f24`:

```sh
tools/blutter/setup-blutter.sh
```

A normal analysis emits `blutter_meta.cpp` and `blutter_meta.h` alongside the
existing Blutter outputs, containing:

- Function/stub names and RVAs, including `dart:*` functions.
- Class names/CIDs and canonical custom-object constants indexed by string fields.
- Basic-value VM layouts for 4-byte compressed-pointer builds.

Metadata ABI version 5 records string constant lengths, including embedded NUL
bytes. puredart uses the fork's metadata types and `blutter_bundle()` directly;
`puredart::meta::puredart_bundle()` forwards to it. General class/field layouts
and allocation data are omitted.
Regenerate older metadata; version mismatches are rejected. Generated sources
use a literal ABI version so rebuilding them with a newer header cannot silently
make stale data appear compatible.

## Runtime verification

The [injection demo](example/demo/README.md) overrides an integer return using
puredart's own engine. Frida only injects the library.

To build the standalone Android ARM64 runtime tests:

```sh
cmake -S . -B build-device \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DANDROID_STL=c++_static -DPUREDART_BUILD_DEVICE_TESTS=ON
cmake --build build-device
adb push build-device/puredart_arm64_runtime_tests /data/local/tmp/
adb shell /data/local/tmp/puredart_arm64_runtime_tests
```

These tests execute synthetic Dart-convention functions to check argument
changes, integer/FP returns, register/SP restoration, capacity, duplicate
rejection, and removal/reinstallation. Run on both page-size configurations
when devices are available.

## License

puredart is licensed under [MPL-2.0](LICENSE). The pinned Blutter submodule
retains its own [MIT license](third_party/blutter/LICENSE).
