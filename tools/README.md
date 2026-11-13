# Shared host tools

`hooktools` contains host-side operations used by native, Dart, and Hermes
workflows. Android helpers use the project's Python 3.13 standard library.
ELF inspection requires `pyelftools`; disassembly and member lookup use LLVM.
None of these Python modules is linked into a runtime payload.
`tools/android.py extract APK ENTRY OUTPUT` copies an APK entry unchanged,
without an inline Python program in build scripts.

Python entry points use the root `pyproject.toml` and `uv.lock`:

```sh
uv sync --locked
uv run --locked python tools/android.py sdk
uv run --locked python tools/android.py --ndk-version 27.1.12297006 ndk
uv run --locked python tools/android.py adb shell getconf PAGESIZE
uv run --locked python tools/android.py --serial "$DEVICE_SERIAL" \
  adb install -r build/test.apk

uv run --locked python tools/inspect_elf.py libapp.so
uv run --locked python tools/inspect_elf.py libhermesvm-unstripped.so \
  --symbol createCodeBlockE --member codeBlock_ --llvm /path/to/llvm/bin
```

- `hooktools/sdk.py` resolves explicit paths, `ANDROID_HOME` /
  `ANDROID_SDK_ROOT`, `ANDROID_NDK`, `ADB_BIN`, and standard Linux/macOS SDK
  locations. NDK versions are selected by the caller rather than hidden in the
  common helper. `dart/example/demo/build-demo.sh` and the Hermes build tools use it.
  NDK LLVM paths are discovered from the installed toolchain directory, without
  a fixed Linux/macOS host tag.
- `hooktools/adb.py` runs local ADB. `ANDROID_SERIAL` is used unless an explicit
  serial is passed. APK installation and single-file `push` / `pull` use the
  selected ADB executable directly. Structured shell arguments are quoted;
  a single shell-script argument is passed intact.
  Commands are noninteractive and do not consume the caller's standard input.
  `dart/example/demo/run-demo.sh` and the Hermes device runner use this interface.
- `hooktools/elf.py` reads GNU Build IDs, unique symbols, file-backed words and
  relative pointer relocations, and invokes LLVM for disassembly and unique
  DWARF member offsets. Both `inspect_elf.py` and the Hermes profile extractor
  use it. Ambiguous symbols/members and unsupported pointer relocations fail.

ADB is selected through `--adb`, `ADB_BIN`, SDK discovery or PATH.
SDK discovery checks explicit arguments, standard environment variables and
conventional locations relative to the current user's
home. No user-specific SDK path is stored in the repository.

Dependency groups are selected only for the workflows that need them:

- `--group build`: CMake 3.31.10 for native/compiler builds.
- `--group blutter`: Requests for Blutter's SDK fetching.
- `--group injection`: the matching Frida library and CLI.
- `dev` (included by default): Ruff; run `uv run --locked ruff check ...`.

`uv sync --locked --all-groups` installs every workflow. Child Python processes
use `sys.executable`, so they inherit the selected environment. No separate
manually managed virtual environment or `uvx` invocation is required.

The HBC definitions, Ret decoding, Hermes ABI selection and profile generation
remain in `hermes/tools/`. Blutter setup remains in `dart/tools/`.

Run the shared helper tests with:

```sh
uv run --locked python -m unittest discover -s tools/tests -v
```

ELF tests use the locked `pyelftools` dependency and require a Linux C compiler
and LLVM; unavailable optional dependencies are reported as skipped. The root
CMake project registers this suite through `uv run --locked` when uv is
available and `HOOKTOOLS_BUILD_TESTS=ON` (the default).

Fixed C/Python test programs live in `tools/tests/fixtures/`; the tests copy
those files into their temporary environment instead of assembling source code.

The Hermes runner writes device logs under the ignored `build/hermes-reports/`
by default. Use `--reports /path/to/reports` to store them outside the checkout.
