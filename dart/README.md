# puredart

C++17 hooks for Dart AOT functions on Android ARM64. Callbacks can inspect
registers, change arguments, override integer or floating-point returns, and
read basic Dart values. Generated Blutter metadata supplies function addresses,
class IDs, and constants from the target app.

To see a complete example, follow the [Dart counter demo](example/demo/README.md).
It changes each tap's increment from one to ten.

## Requirements

- Android ARM64 for runtime hooks, a C++17 compiler, CMake 3.22 or newer, and the
  Android NDK.
- Git, uv, and the [Blutter host build dependencies](third_party/blutter/README.md#environment-setup)
  to generate metadata.
- A target APK whose Dart version is supported by the bundled Blutter. The demo
  uses Flutter 3.41.2 / Dart 3.11.0; the bundled Blutter does not support Dart 3.13
  snapshots.
- A supported function entry and a callback using the target's actual Dart
  calling convention. The adapter does not infer function signatures or convert
  arbitrary C++ values to Dart objects.

## Use in your project

The workflow is: generate metadata from your APK, build it into your hook
library, then load that library into the app and install your callbacks.

### 1. Generate metadata

Clone the repository with submodules as shown in the [root README](../README.md#try-a-demo).
Run these commands from the repository root:

```sh
dart/tools/blutter/setup-blutter.sh
uv sync --locked
uv run --locked --group blutter python dart/third_party/blutter/blutter.py \
  path/to/app.apk build/analysis --rebuild
```

Use the bundled Blutter so that its generated metadata matches the adapter.
The first analysis may fetch and build the matching Dart tools. Analysis emits
`build/analysis/blutter_meta.cpp` and `blutter_meta.h`, alongside assembly and
other Blutter outputs.

Generate metadata from the exact APK you intend to hook. Inspect the generated
assembly to identify the target's lookup name, argument registers, and return
representation. Regenerate metadata when the target app changes or a metadata
ABI mismatch is reported.

### 2. Build your hook library

In your Android CMake project, create a shared-library target containing your
callbacks and link the adapter:

```cmake
add_library(my_hook SHARED hook.cpp)
add_subdirectory(path/to/puredart/dart puredart_build)
target_link_libraries(my_hook PRIVATE puredart::core)
puredart_add_meta(my_hook /absolute/path/to/build/analysis/blutter_meta.cpp)
```

The adapter adds `purenative::core` automatically. Relative metadata paths resolve
under `PUREDART_META_DIR`, which defaults to `dart/meta/` in this checkout.
Missing metadata is a configure-time error.

### 3. Load the library and install a callback

Your app or loader must load the shared library and invoke your installation
code. The [demo loader](example/demo/README.md) shows this with Frida; Frida is
used only to load the native payload and call its entry point.

This example targets the demo's `counterStep()`, which returns an unboxed integer
in x0 in that release build. Adapt the symbol and ABI representation to your own
target:

```cpp
#include <puredart/puredart.h>

extern "C" __attribute__((visibility("default"))) void start_hooks() {
    if (!puredart::install()) return; // Validate generated metadata.
    puredart::bootstrap([](uintptr_t base) {
        const auto rva = puredart::meta::resolve("hookme$main$counterStep");
        if (!rva) return;
        puredart::hook(base + rva, [](puredart::HookContext& context) {
            context.set_return(10);
        });
    });
}
```

`bootstrap()` polls for `libapp.so` every 10 ms for up to two minutes. It does
not interpose the Android loader or guarantee catching the first invocation
during startup. Install directly at a consumer-controlled load point when that
timing is required. Check the return values of `bootstrap()` and `hook()` in
your installation code; the [demo payload](example/demo/demo/src/main/cpp/demo.cpp)
also logs whether the hook was installed.

## Callback behavior

The callback runs before the Dart function. `set_arg()` changes x0..x7.
`set_return()` or `set_fp_return()` skips the function; otherwise execution
continues through the original trampoline. There are 16 reusable hook slots.

Dart calling conventions vary by target. Inspect the generated assembly to
choose registers and distinguish boxed values, such as Smi, from unboxed
integers. Supported entry instructions are described in the
[native hook contract](../native/README.md#supported-function-entries).

Keep callbacks bounded and non-blocking. They run synchronously on the Dart
thread; avoid I/O, contended locks, and VM re-entry. C++ callback code must be
compiled without ShadowCallStack because Dart does not maintain x18.
`puredart::core` propagates `-fno-sanitize=shadow-call-stack`; apply it separately
to callbacks compiled in other targets.

`unhook(address)` requires the caller to ensure no thread is executing the
target, shim, callback, or trampoline, and to prevent new calls until removal
completes. Never unhook from a callback. The adapter's trampoline pointer must
not be called directly from C++.

## Reading Dart values

`vm::Object` reads Smi, boxed integers, bool, double, and one/two-byte strings.
It also exposes CID and the tagged address. `from_compressed()` explicitly
interprets compressed fields/constants; register objects are already expanded.
An absent object is invalid, while Smi zero is a valid value.

Heap values and memory-map snapshots are callback-scoped. Do not retain them
across GC/isolate transitions. Memory-map checks are bounds checks, not GC
synchronization. The adapter offers no general fields, collection traversal,
heap writes, allocation, or Dart runtime invocation.

## Metadata options

Full metadata includes:

- Function/stub names and RVAs, including `dart:*` functions.
- Class names/CIDs and canonical custom-object constants indexed by string fields.
- Basic-value VM layouts for 4-byte compressed-pointer builds.

To emit only selected classes, repeat `--metadata-class`:

```sh
uv run --locked --group blutter python dart/third_party/blutter/blutter.py \
  path/to/app.apk build/analysis --rebuild \
  --metadata-class MyHomePage \
  --metadata-class 'package:app/main.dart::_MyHomePageState'
```

Selectors match exact class names, or exact `library URL::class name` pairs.
A bare name selects every class with that name; each selector must match at
least one class. Selected metadata includes class IDs, methods declared by those
classes, and string-field constants from their instances, including inherited
fields. Parent/child classes, top-level functions, and VM stubs are excluded.
Other Blutter outputs and analysis are unchanged. Omit the options for full
metadata. The metadata ABI and runtime lookup names remain unchanged.

## Build options and reference

`PUREDART_ENABLE_RUNTIME_HOOKS=OFF` builds only metadata lookup and basic value
reads. In this mode, adding only `dart/` does not build native hooks.

- [Public API headers](include/puredart/).
- [Build, test, and maintenance](../docs/development.md): host and device tests,
  metadata ABI, dependency details, and adapter implementation.
- [Shared host tools](../tools/README.md): SDK/ADB and ELF helpers.
