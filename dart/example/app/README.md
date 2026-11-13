# hookme

A Flutter counter app for the [Dart native-hook demo](../demo/README.md).
It calls the non-inlined `counterStep()` helper and normally increments by one.
The separately injected payload overrides that helper's return with ten.

Use the pinned Flutter 3.41.2 / Dart 3.11.0 SDK in `mise.toml`:

```sh
flutter pub get
flutter analyze
flutter test
```

The Android package is `com.example.hookme`. Build the release APK, Blutter
metadata and injection payload with `../demo/build-demo.sh`.
