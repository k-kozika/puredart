# hookme for Hermes

- [app/](app/README.md): the Expo / React Native counter app, named `hookme`.
- [demo/](demo/README.md): build and inject the native `purehermes::core` payload.

The normal release APK increments by one. The separately loaded native payload
changes the selected updater's numeric return to add ten and maps its button
literal to `+10`. The source and APK are not patched to obtain that behavior.

The Android package is `com.example.hermes.hookme`, so it can coexist with the
[Dart example](../../dart/example/README.md), `com.example.hookme`.
The original external pwnme project and its historical verification artifacts
remain outside this checkout.
