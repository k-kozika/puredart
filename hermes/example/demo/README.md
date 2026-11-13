# Native Hermes hook injection demo

This demo builds [hookme](../app/README.md), selects `counterStep` and its `+1`
literal from the exact release HBC, and builds `libpurehermes_demo.so`.
The generated header embeds the unchanged bundle once and exports both named
selectors. No function or string IDs are hardcoded in the payload.

The native callback adds nine to the updater's original `count + 1` return.
`map_literal` supplies `+10` through the VM's string map. React receives the
updated number through its normal state-update path.

## Build

Requires Bun, Node, uv, Java, Ninja, Android SDK, and NDK
27.1.12297006 (or `ANDROID_NDK`). Python, CMake and Frida versions come from
the root `pyproject.toml` and `uv.lock`. Frida is needed only for injection.

```sh
./hermes/example/demo/build-demo.sh
```

The script installs the locked app dependencies, runs Expo Prebuild, builds an
ARM64 release APK, extracts its original HBC and generates metadata with
`purehermes-meta`. It then builds the native payload with 16KB ELF alignment.
Outputs are under `hermes/example/demo/build/payload/`.

## Inject

Use a rooted ARM64 Android device with a matching Frida 17.2.17 server.
The default uses local ADB and a USB-connected Frida device:

```sh
uv run --locked --group injection python hermes/example/demo/run-demo.py
```

`--serial` or `ANDROID_SERIAL` selects the same device for ADB and Frida.
`--adb` selects an ADB executable. A forwarded or network Frida server is opt-in
with `--remote ADDRESS` or `FRIDA_REMOTE`. ADB commands run through the local
ADB executable.

The runner installs `com.example.hermes.hookme`, stops it before replacing the
extra library, copies the payload with its `apk_data_file` label, then starts
the loader. Both root adbd and `su` are supported.

The loader reads the checked-in `load.js` and passes the native directory as
RPC data. It preloads the payload, installs native hooks as the VM loads, and
detaches. The hooks remain in the app process. Stop that process and launch
normally to restore the original `+1` behavior. See logcat's `PurehermesDemo`
tag if installation is rejected.

The runtime needs an exact verified VM Build ID; rebuilding for another Hermes
version may require a new profile. Callback and payload lifetimes follow the
[core contract](../../README.md#callback-and-value-contract).
