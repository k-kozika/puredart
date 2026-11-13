# puredart

A small C++17 adapter for hooking Android ARM64 Dart AOT function entries.
The implementation has three layers:

- `native/`: Dart-independent ARM64 inline hooks (`purenative::core`).
- `dart/`: Dart register snapshots, argument/return changes, basic value reads,
  and Blutter metadata (`puredart::core`, backed by `purenative::core`).
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
add_subdirectory(path/to/puredart/dart puredart_build)
target_link_libraries(my_hook PRIVATE puredart::core)
puredart_add_meta(my_hook /absolute/path/to/blutter_meta.cpp)
```

Relative metadata paths resolve under `PUREDART_META_DIR` (default `dart/meta/`
in this checkout).
Missing metadata is a configure-time error. `PUREDART_ENABLE_RUNTIME_HOOKS=OFF`
builds only metadata lookup and basic value reads.

For a native-only consumer:

```cmake
add_subdirectory(path/to/puredart/native purenative_build)
target_link_libraries(my_native_hook PRIVATE purenative::core)
```

Its C++ interface is `<purenative/hook.h>` in namespace `purenative`.

To use both libraries explicitly:

```cmake
add_subdirectory(path/to/puredart/native purenative_build)
add_subdirectory(path/to/puredart/dart puredart_build)
target_link_libraries(my_hook PRIVATE purenative::core puredart::core)
puredart_add_meta(my_hook /absolute/path/to/blutter_meta.cpp)
```

Either subdirectory order works: the Dart adapter reuses `purenative::core`
if it exists, or adds it automatically when runtime hooks are enabled.
With runtime hooks disabled, adding only `dart/` does not build native hooks.
The repository root adds both libraries for combined builds.

Each library can also be built and tested on its own:

```sh
cmake -S native -B build/native-only
cmake --build build/native-only
ctest --test-dir build/native-only --output-on-failure
cmake -S dart -B build/dart-only
cmake --build build/dart-only
ctest --test-dir build/dart-only --output-on-failure
```

`PURENATIVE_BUILD_TESTS` and `PUREDART_BUILD_TESTS` control host tests
independently. Each defaults to on for its top-level project and off when
included by another project. The repository root defaults both to on.
Dart's Blutter dependency, metadata helper, and generation tools live under
`dart/`; native-only builds need none of them. The ARM64 patch implementation
and its calling-convention options live in `native/`; Dart ABI shims, callback
management, `libapp.so` discovery, and metadata/value handling live in `dart/`.

When updating a checkout from the previous layout, use fresh CMake build
directories and move any generated metadata from `meta/` to `dart/meta/` (or
set `PUREDART_META_DIR` explicitly). Rebuild cached Blutter executables from
the moved sources in a fresh build directory: they embed the absolute path
to their script templates.

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
per target. Original trampolines replay supported `stp`/`str` stack saves, native
`sub sp,sp,#imm` stack allocation, or native unsigned-immediate `ldr x` entries,
then branch directly to the original code. A leading BTI is retained;
unsupported prologues, PAC entry sequences, duplicate entries, and unavailable
nearby mappings are rejected before the batch is patched. This is an entry-hook
engine with a limited prologue contract, not a general instruction relocator.

Native hooks use `purenative::redirect_all()` and optional `Handle` outputs.
A native replacement may use the ABI scratch register x16. The Dart adapter
uses the engine's `preserve_ip` contract with x15 as its stack register: the
veneer saves x16/x17, and the shim or original trampoline consumes that frame. C++ callbacks run on an aligned
native stack, with both x15 (Dart SP) and CPU SP restored on exit. Its trampoline
pointer must not be called directly from C++.

`unhook(address)` / `purenative::remove(handle)` require the caller to ensure
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
is pinned to `46c64d36cd9ab493b253e87952d8d56989e6a182`:

```sh
dart/tools/blutter/setup-blutter.sh
```

A normal analysis emits `blutter_meta.cpp` and `blutter_meta.h` alongside the
existing Blutter outputs, containing:

- Function/stub names and RVAs, including `dart:*` functions.
- Class names/CIDs and canonical custom-object constants indexed by string fields.
- Basic-value VM layouts for 4-byte compressed-pointer builds.

Use repeatable `--metadata-class` options to emit only selected classes:

```sh
python3 dart/third_party/blutter/blutter.py path/to/app.apk out_dir --rebuild \
  --metadata-class MyHomePage \
  --metadata-class 'package:app/main.dart::_MyHomePageState'
```

Selectors match exact class names, or exact `library URL::class name` pairs.
A bare name selects every class with that name; each selector must match at
least one class. Selected metadata includes class IDs, methods declared by those
classes, and string-field constants from their instances (including inherited
fields). Parent/child classes, top-level functions, and VM stubs are excluded.
Other Blutter outputs and analysis are unchanged. Omit the options for full
metadata. The metadata ABI and runtime lookup names remain unchanged.

CLI regression tests run with `python3 -m unittest discover -s
dart/third_party/blutter/tests`. To check generated bundles against the release demo,
run `python3 dart/tests/check_metadata_filter.py --blutter <rebuilt-executable>
--libapp <demo-libapp.so>`. This compiles and validates the generated C++ and
checks class, symbol, and constant lookups.

Metadata ABI version 5 records string constant lengths, including embedded NUL
bytes. puredart uses the fork's metadata types and `blutter_bundle()` directly;
`puredart::meta::puredart_bundle()` forwards to it. General class/field layouts
and allocation data are omitted.
Regenerate older metadata; version mismatches are rejected. Generated sources
use a literal ABI version so rebuilding them with a newer header cannot silently
make stale data appear compatible.

## Runtime verification

The [injection demo](dart/example/demo/README.md) overrides an integer return using
puredart's own engine. Frida only injects the library.

To build the standalone Android ARM64 runtime tests:

```sh
cmake -S dart -B build-device \
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
retains its own [MIT license](dart/third_party/blutter/LICENSE).
