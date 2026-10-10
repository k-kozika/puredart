# Native Dart hook injection demo

The Flutter counter calls `counterStep()` to obtain its increment. Without
injection it returns one. A native puredart callback overrides its return with
ten, so each tap adds ten. The helper is marked `vm:never-inline`; its parsing
call gives it a stack-save prologue supported by the inline engine.

Frida only loads the native library and calls `puredart_demo_start()`.
All module detection, register handling, and function replacement run inside
puredart. The demo does not allocate or write Dart heap objects.

## Requirements

- Flutter 3.41.2 / Dart 3.11.0 (the pinned Blutter does not support Dart 3.13 snapshots).
- Android SDK 37 and NDK 28.2.13676358.
- ARM64 Android device with root, `su`, and a matching Frida server.
- JDK 17 or newer, Python 3, Git, CMake, Ninja, and the [Blutter host dependencies](../../third_party/blutter/README.md).
- `adb` and `frida` to run the injection demo.

## Build and run

```sh
git clone --recurse-submodules https://github.com/k-kozika/puredart.git
cd puredart/example/demo
export ANDROID_HOME=/path/to/android-sdk
export FLUTTER_BIN=/path/to/flutter-3.41.2/bin/flutter
./build-demo.sh
./run-demo.sh
```

`FLUTTER_BIN` is optional when the pinned Flutter is on PATH. With mise, run
`mise install` in `example/app`, then `mise exec -- ../demo/build-demo.sh`.
The build checks Flutter/Dart versions before compiling; the app also limits
its Dart SDK to 3.11.x. Gradle wrapper scripts and their JAR are included in
this repository. SDK paths, signing keys, and generated build outputs are
machine-local and ignored by Git.

`ANDROID_HOME` may be replaced by `ANDROID_SDK_ROOT` or an existing app
`android/local.properties`. No local configuration file is required for the
first build when `ANDROID_HOME` is set. Set `ADB_BIN` / `FRIDA_BIN` if those
executables are not on PATH. Use `ANDROID_SERIAL` for multiple devices.

The build generates metadata from the exact release APK. Verify the helper's
return representation in `build/analysis/asm/hookme/main.dart` when changing
Flutter versions; the callback uses the ABI observed in this build.

The native library logs `counterStep hook installed` under `PuredartDemo`.
Stop the injected process and launch normally to restore increments of one.

`run-demo.sh` uses root to copy the generated library into the installed
package's native library directory and apply its `apk_data_file` label, so the
app can executable-map it. The APK is not repackaged for injection.
