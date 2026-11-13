# pure

C++17 hooks for Android ARM64 apps. Redirect native functions, inspect or change
Dart AOT arguments and returns, or change Hermes interpreter numeric returns and
string literals. All three libraries share an inline hook engine with no external
hook-engine dependency.

## Supported environments

- Runtime hooks require Android ARM64. Building a hook library requires a C++17
  compiler, CMake 3.22 or newer, and the Android NDK.
- Dart hooks require metadata generated from the target APK with the bundled
  Blutter. Dart version, calling convention, and function entry instructions
  affect compatibility; see the [Dart guide](dart/README.md#requirements).
- Hermes bytecode readers support HBC 96–99. Runtime hooks require an exact
  supported VM ELF Build ID; an HBC version alone does not establish runtime
  compatibility. See the [Hermes guide](hermes/README.md).
- Native and Dart entry hooks support a limited set of function prologues.
  Unsupported entries are rejected; see the [native hook contract](native/README.md#supported-function-entries).

## Try a demo

The `hookme` demos show a counter changing from increments of one to increments
of ten after loading a native hook library.

```sh
git clone --recurse-submodules https://github.com/k-kozika/puredart.git
cd puredart
```

Choose a demo and follow its requirements and build/run steps:

| App runtime | Demo | Expected result |
| --- | --- | --- |
| Flutter / Dart AOT | [Dart counter demo](dart/example/demo/README.md) | Each tap adds ten instead of one. |
| React Native / Hermes | [Hermes counter demo](hermes/example/demo/README.md) | Each tap adds ten, and the button label changes to `+10`. |

Both injection demos require a rooted ARM64 Android device and a matching Frida
server. Frida loads the hook library; the hooks run inside pure. Each demo guide
lists its required SDK and tool versions.

## Use in your project

Choose the library for your target:

| Target | Library | Integration guide |
| --- | --- | --- |
| Native ARM64 functions | `purenative::core` | [Native hooks](native/README.md) |
| Dart AOT functions and basic values | `puredart::core` | [Dart hooks and metadata generation](dart/README.md) |
| Hermes numeric returns and string literals | `purehermes::core` | [Hermes hooks and bundle selectors](hermes/README.md#build-and-use) |

For Dart or Hermes, first generate metadata from the target app. Then build an
Android shared library containing your callbacks and the selected library, load
it into the app, and install the hooks. The integration guides describe the API
and loading requirements; the demos provide complete examples. Native-only
consumers can use `native/` independently.

## Development and tools

- [Build, test, and maintenance](docs/development.md): host and Android runtime
  tests, combined builds, and dependency details.
- [Shared host tools](tools/README.md): Android SDK/ADB discovery and ELF/DWARF
  inspection.

## License

pure is licensed under [MPL-2.0](LICENSE). The Blutter submodule retains its own
[MIT license](dart/third_party/blutter/LICENSE). Generated Hermes format
definitions retain Meta's [MIT notice](hermes/third_party/HERMES-LICENSE).
