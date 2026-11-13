# hookme

An Expo SDK 58 / React Native 0.88 counter app for the
[Hermes native-hook demo](../demo/README.md). It increments by one without
injection. `src/counter.ts` exports the ordinary `counterStep` updater, allowing
the demo to select it from the exact release HBC by name.

```sh
bun install --frozen-lockfile
bun run lint
bun run typecheck
bun run start
```

Android app configuration uses `hookme` for the name, slug and scheme, and
`com.example.hermes.hookme` for its package. Expo generates the ignored
`android/` and `ios/` projects from `app.json` through
[Prebuild](https://docs.expo.dev/workflow/continuous-native-generation).
See [the SDK 58 app-config reference](https://docs.expo.dev/versions/v58.0.0/config/app/)
when changing native identity or configuration.

For a release APK and matching injection payload, run `../demo/build-demo.sh`.
Generated native projects, dependencies and build outputs stay out of Git.
The app sources retain their [original MIT license](LICENSE).
