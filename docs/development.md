# Build, test, and maintenance

This guide covers developing and verifying the libraries. For app integration,
start with the [native](../native/README.md), [Dart](../dart/README.md), or
[Hermes](../hermes/README.md) guide. To try hooks in an app, use the
[injection demos](../README.md#try-a-demo).

## Host build and tests

Requires Git, a C++17 compiler, and CMake 3.22 or newer. Install uv to include
the shared Python helper suite. Clone the repository, then build and test from
its root:

```sh
git clone --recurse-submodules https://github.com/k-kozika/puredart.git
cd puredart
uv sync --locked
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Host tests inspect ARM64 patches in mapped instruction buffers without executing
ARM64 code, including simulated 4 KiB and 16 KiB page sizes. They also check
metadata, basic values, callback publication, CFI, BTI landings, and the
ShadowCallStack guard. Runtime execution requires an Android ARM64 device.

Each library can also be built and tested on its own:

```sh
cmake -S native -B build/native-only
cmake --build build/native-only
ctest --test-dir build/native-only --output-on-failure

cmake -S dart -B build/dart-only
cmake --build build/dart-only
ctest --test-dir build/dart-only --output-on-failure

cmake -S hermes -B build/hermes-only
cmake --build build/hermes-only
ctest --test-dir build/hermes-only --output-on-failure
```

`PURENATIVE_BUILD_TESTS`, `PUREDART_BUILD_TESTS`, and `PUREHERMES_BUILD_TESTS`
control host tests independently. Each defaults to on for its top-level project
and off when included by another project. The repository root defaults all three
to on. `HOOKTOOLS_BUILD_TESTS` controls the shared Python helper suite when uv is
available; the root project defaults it to on.

Python entry points use the root `pyproject.toml` and `uv.lock`. Workflow groups
and additional host dependencies are described in the [tools guide](../tools/README.md).

## Combined CMake builds

The repository root adds all three libraries. To use native hooks and the Dart
adapter explicitly in a consumer project:

```cmake
add_subdirectory(path/to/puredart/native purenative_build)
add_subdirectory(path/to/puredart/dart puredart_build)
target_link_libraries(my_hook PRIVATE purenative::core puredart::core)
puredart_add_meta(my_hook /absolute/path/to/blutter_meta.cpp)
```

Either subdirectory order works: the Dart adapter reuses `purenative::core`
if it exists, or adds it automatically when runtime hooks are enabled. Hermes
also reuses the shared target. With Dart runtime hooks disabled, adding only
`dart/` does not build native hooks.

## Android runtime tests

Set `ANDROID_NDK` to the NDK directory. To build and run the standalone Dart
ARM64 runtime tests:

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
rejection, and removal/reinstallation. Run on both 4 KiB and 16 KiB page-size
configurations when devices are available.

For Hermes, follow [Reproduce the Android suite](../hermes/README.md#reproduce-the-android-suite).

## Blutter dependency and metadata ABI

Dart uses the metadata-enabled Blutter fork tracked by the submodule. The
[setup script](../dart/tools/blutter/setup-blutter.sh) checks the expected commit;
[.gitmodules](../.gitmodules) records the fork and branch. Use these as the source
of truth for the dependency revision.

Metadata ABI version 5 records string constant lengths, including embedded NUL
bytes. puredart uses the fork's metadata types and `blutter_bundle()` directly;
`puredart::meta::puredart_bundle()` forwards to it. General class/field layouts
and allocation data are omitted.

Metadata ABI mismatches are rejected. Generated sources
use a literal ABI version so rebuilding them with a newer header cannot silently
make stale data appear compatible.

Run the Blutter CLI regression tests from the repository root:

```sh
uv run --locked --group blutter python -m unittest discover \
  -s dart/third_party/blutter/tests
```

To check generated bundles against the release demo, supply a rebuilt Blutter
executable and the demo's `libapp.so`:

```sh
uv run --locked --group blutter python dart/tests/check_metadata_filter.py \
  --blutter /path/to/rebuilt-blutter --libapp /path/to/demo-libapp.so
```

This compiles and validates the generated C++ and checks class, symbol, and
constant lookups.

## Hook engine and Dart adapter implementation

The ARM64 patch implementation and calling-convention options live in `native/`.
Dart ABI shims, callback management, `libapp.so` discovery, and metadata/value
handling live in `dart/`. Blutter and metadata generation tools also live under
`dart/`; native-only builds need none of them.

The engine allocates nearby veneers and atomically writes one B instruction per
target. See the [native hook contract](../native/README.md#supported-function-entries)
for supported prologues and rejection behavior. Protection-restoration failures
are logged; published hooks remain tracked for removal rather than reporting
a failed installation with untracked live code.

The Dart adapter uses the engine's `preserve_ip` contract with x15 as its stack
register: the veneer saves x16/x17, and the shim or original trampoline consumes
that frame. C++ callbacks run on an aligned native stack, with both x15 (Dart SP)
and CPU SP restored on exit. The trampoline pointer must not be called directly
from C++.

The shim maintains CFI and BTI landing pads and restores the Dart reserved
registers x26/x27/x28. Callback compilation must follow the
[ShadowCallStack restriction](../dart/README.md#callback-behavior).

