# purenative

C++17 inline hooks for native Android ARM64 functions. Use `purenative::core`
to redirect function entries to your replacements, with optional original
trampolines and removal handles. The library has no external hook-engine
dependency and can be used independently of Dart, Hermes, and their metadata.

## Build and use

Build your Android shared-library target with a C++17 compiler, CMake 3.22 or
newer, and the Android NDK targeting `arm64-v8a`. Add the library to your CMake
project:

```cmake
add_subdirectory(path/to/puredart/native purenative_build)
target_link_libraries(my_native_hook PRIVATE purenative::core)
```

Include `<purenative/hook.h>` and use namespace `purenative`. The
[public header](include/purenative/hook.h) defines `Redirect`, `Handle`,
`redirect_all()`, and `remove()`.

- `redirect_all()` prepares a batch before publishing any branch. Each
  `Redirect` supplies the target entry and replacement addresses; its optional
  `original` output receives a trampoline for continuing through the original.
- Native replacements must follow the target's ABI and may use the ABI scratch
  register x16. The default `Redirect` uses the CPU stack pointer and
  `preserve_ip = false`.
- Request `Handle` outputs if you need removal. Without handles, installed hooks
  live until process exit.

Your app or loader must load the shared library and call your installation code
at the required point in the app's lifetime. Runtime installation is supported
on Android ARM64; unsupported hosts return `false`.

## Supported function entries

This entry-hook engine supports a limited set of prologues. Original trampolines
replay supported `stp`/`str` stack saves, native `sub sp,sp,#imm` stack allocation,
or native unsigned-immediate `ldr x` entries, then branch directly to the
original code. A leading BTI is retained.

Unsupported prologues, PAC entry sequences, duplicate entries, and unavailable
nearby mappings are rejected before the batch is patched. Inspect the target's
entry instructions when choosing hook points.

## Removal and lifetime

Before calling `remove(handle)`, ensure no thread is executing the target,
replacement, or trampoline, and prevent new calls until removal completes.
Never remove a hook from its callback.

Removal restores the original instruction and frees a shared veneer page only
after its final handle is removed. Protection-restoration failures are logged;
once branches are published they remain registered for removal.

## Related documentation

- [Dart adapter](../dart/README.md) and [Hermes adapter](../hermes/README.md).
- [Build, test, and maintenance](../docs/development.md), including the adapter
  calling convention and combined CMake builds.
