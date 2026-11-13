# purehermes

C++17 Android ARM64 hooks for the Hermes interpreter. It shares
`purenative::core` with the Dart adapter and has no dependency on Dart, Blutter,
Frida, Rust, or React Native at runtime.

The supported HBC readers are **96, 97, 98, 99**. Native hook support is selected
by an **exact ELF Build ID**, not by the HBC number. `profiles/` records four
optimized source builds in each of two ELF alignment configurations and the
original HBC99 pwnme Maven runtime. Other builds require their own reviewed and
device-tested profile.
No HBC translation, source changes, APK repacking, or bytecode writes are used.

## Build and use

```cmake
add_subdirectory(path/to/puredart/hermes purehermes_build)
target_link_libraries(my_payload PRIVATE purehermes::core)
```

Adding `native/`, `dart/`, and `hermes/` in either native-first or adapter-first
order reuses `purenative::core`. `PUREHERMES_ENABLE_RUNTIME_HOOKS=OFF` builds only
the reader and configuration interface. Unsupported platforms return
`InstallState::unsupported`. `PUREHERMES_BUILD_TESTS` and
`PUREHERMES_BUILD_TOOLS` default on for a standalone host project.

Inspect an unchanged bundle and export a named function selector:

```sh
cmake -S hermes -B build/hermes -G Ninja
cmake --build build/hermes
build/hermes/purehermes-meta original.hbc
build/hermes/purehermes-meta original.hbc --emit counter_meta.h \
  --function 8245 --alias counter_updater
```

The generated header owns a copy of the original HBC and exports
`purehermes_generated::bundle()` and `purehermes_generated::counter_updater`.
Use `--string ID --alias NAME` instead to export a `StringSelector`.
`--function-name NAME` and `--string-value TEXT` select unique exact matches.
Repeat selectors with their own `--alias` to emit them together in one header,
sharing a single unchanged HBC copy. Inspection
lists function IDs, body offsets, all normal return positions, strings, and
string references in instructions and object/array literals. Duplicate function
names are rejected by `Bundle::function(name)`; use the explicit ID in that case.

```cpp
#include <purehermes/purehermes.h>
#include "counter_meta.h"

purehermes::HookPlan plan(purehermes_generated::bundle());
plan.on_return(purehermes_generated::counter_updater,
    [](purehermes::ReturnContext& context) {
        context.set_number(context.number() + 9);
    });
// The original updater adds 1; its normal numeric result is adjusted by 9.
// Optionally map the demo's literal string ID, including all shared uses:
plan.map_literal({1389}, "+10");
auto result = plan.install(purehermes::module_at(observed_hermes_base));
```

The loader must call `install` **before the target bundle executes**. Module
polling cannot guarantee catching its first execution. The demo loader preloads
the payload outside the loader notification and installs the native hooks when
the VM appears. Injecting at an already-running bundle is unsupported; there is
no retroactive cache or UI rewrite.

After successful installation, the plan, callbacks, bundle copy, and replacement
strings are owned until process exit. The payload's shared library must also
remain loaded. Destroying the local `HookPlan` does not remove hooks. Callback
captures must own their state; do not capture local references that will expire.

`InstallResult` reports `installed`, `unsupported`, `rejected`, or
`protection_error`. Inspect `hooks_active` before unloading a payload: a
protection restoration failure can leave live, tracked hooks even though the
result's boolean conversion is false. Use process termination for removal;
live unhook is deliberately not provided.

## Callback and value contract

- Callbacks run on the VM thread, once per matching normal numeric return.
  They can read/replace an HV64 JavaScript Number, including infinity and NaN.
  Other return types are left untouched and do not invoke the callback.
  Multiple Runtime instances can invoke the same callback concurrently; protect
  any shared mutable captures yourself.
- VM exceptions propagate normally and do not invoke a normal-return callback.
  A C++ callback exception is contained; the original VM result is retained.
- Keep callbacks bounded. Do not re-enter the VM, allocate in its GC heap, or
  keep VM pointers across callbacks. Native recursion into dispatch is guarded.
- Matching uses both the return IP and actual CodeBlock identity. Different
  functions can share identical bytecode. Old VM profiles decode the runtime's
  XOR-protected CodeBlock pointer; newer ones use plain pointers.
- RuntimeModule destruction invalidates its bindings. Runtime recreation is
  tested; lazy source compilation, debugger bytecode changes, hot reload,
  generators/async completion hooks, native functions, JIT execution and
  before-call/skip-call hooks are outside this version's contract.
- At most eight VM modules can be installed per process, with one immutable
  plan per module. Configure all desired selectors in that plan before installing.
- `map_literal` supports ASCII non-identifier string IDs before first use.
  It changes **every use of that ID in the matched module**, not a single JSX
  property. Identifier IDs are rejected because the VM imports them earlier.
  Static payload-owned storage satisfies lazy identifier lifetime requirements.
- The reader bounds-checks tables, overflow headers, instructions and literal
  buffers, and never guesses the nearest HBC format. This initial reader accepts
  complete buffers whose length matches the declared HBC length (no epilogue).

## Version data and profiles

`tools/generate_formats.py` uses a checked-in preprocessor adapter on the pinned
official `BytecodeList.def`, and extracts packed fields from the matching format
header.
The generated tables and `formats.lock.json` contain no hermes-dec code or
Python opcode modules. Meta's MIT notice is in `third_party/HERMES-LICENSE`.
The normal build is offline and uses the checked-in C++ tables.

The four official commits are locked only in `src/formats.lock.json`, together
with the repository and Ret opcode. No per-file checksums are stored. Sources,
compilers and VMs are cached under `build/hermes-toolchains/<HBC>/<commit>/`.
Version differences include function field widths,
overflow-header offsets, frame layouts, byte-string versus undefined literal
encoding, native ABI signatures and XOR-protected pointers; they are not one
`version >= 97` branch. The interface remains the same across all four.

Extract a candidate profile from an unstripped Android ARM64 ELF:

```sh
uv run --locked python hermes/tools/profile.py libhermesvm-unstripped.so 99 build/profile.json \
  --llvm /path/to/ndk/toolchains/llvm/prebuilt/HOST_TAG/bin
```

This requires `pyelftools`. Shared ELF/DWARF operations are provided by
[`../tools/hooktools`](../tools/README.md). The extractor obtains symbols, GNU Build ID, entry
instructions, dispatch relocations, Ret register operands and member offsets.
It rejects ambiguous/missing information. Only move a candidate into `profiles/`
after inspecting its disassembly and passing device tests. Same-HBC custom builds
are not automatically trusted. Explicit profiles can be supplied to
`install(module, profile)` for testing. Regenerate embedded profiles with:

```sh
uv run --locked python hermes/tools/generate_profiles.py hermes/profiles hermes/src/profiles.inc
```

## Reproduce the Android suite

Prerequisites: uv, Java, Clang, Ninja, Android SDK platform 35 and build tools
37.0.0, and NDK 27.1.12297006. Python 3.13 is selected by `.python-version`;
`pyproject.toml` and `uv.lock` declare all Python dependencies. The `build` group
provides CMake 3.31.10 because older Hermes does not support CMake 4 policies.
The optional `injection` group pins Frida 17.2.17 and its compatible CLI.

```sh
uv sync --locked
uv run --locked --group build python hermes/tools/build_toolchains.py
uv run --locked --group build python hermes/tools/device_suite.py
uv run --locked python hermes/tools/run_device.py
uv run --locked python hermes/tools/run_device.py \
  --serial "$DEVICE_SERIAL" --reports /path/to/reports/16k
```

Use `build_toolchains.py --fetch-only` to fetch sources without compiling.
The full commit is part of the cache path, so changing a pin selects a fresh
source and build directory. The prior version-only cache is not reused.

The first step downloads pinned official sources and fbjni test dependencies;
all builds stay under `build/`. The Android harness and fbjni CMake configuration
are checked-in files; their paths are supplied through CMake variables. Hermes
source is left unchanged. Each APK uses one real VM and its matching compiler,
an unchanged HBC asset, Release native
harness code and a build-specific profile. `device_suite.py` creates a local
throwaway test signing key. Rebuilding can change Build IDs; the harness tests
its freshly extracted explicit profile without automatically promoting it.
VMs and harnesses link with 16KB ELF alignment by default. For historical 4KB
VM builds, pass `--link-page-size 4096` to `build_toolchains.py`; this changes
the VM's Build ID and requires rebuilding the harness with its new profile.

`run_device.py` uses local ADB by default, installing the test package
`org.purehermes.test` on the selected ARM64 Android device. Choose a device with
`--serial` or `ANDROID_SERIAL`; otherwise ADB performs its normal selection.
`--adb PATH` selects a local ADB executable; otherwise SDK discovery or PATH
is used.
The runner requires PASS for every requested version and saves per-process logs
and a matrix under `build/hermes-reports/`. Use `--reports PATH` to choose a
directory outside this checkout. Select a separately booted 16KB ARM64 AVD for
the second run.
Both the device and the harness log their actual page size. The same 16KB-aligned
builds passed on 4KB and 16KB kernels. Android's compatibility loading of older
4KB-aligned VMs is not guaranteed; the failed initial attempts are retained.

Host tests include 40,008-byte functions that force overflow headers, shared
bytecode bodies, UTF-16 surrogate strings, array/object literal references,
malformed inputs, mixed consumer builds and ARM64 CFI/BTI/FP register preservation.
The intentionally unrolled `tests/fixtures/large.js` is also checked in.
Regenerate goldens explicitly with
`uv run --locked python hermes/tools/generate_fixtures.py --output ...`;
ordinary tests never regenerate tracked fixtures.
