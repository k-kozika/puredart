#!/usr/bin/env bash
set -euo pipefail

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root_dir=$(cd "$demo_dir/../.." && pwd)
app_dir="$root_dir/example/app"
blutter_dir="$root_dir/third_party/blutter"
analysis_dir="$demo_dir/build/analysis"
meta_file="$analysis_dir/puredart_meta.cpp"
payload_dir="$demo_dir/build/payload"

for tool in python3 git cmake ninja; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "puredart demo: required tool not found: $tool" >&2
        exit 1
    fi
done

flutter_bin=$(command -v "${FLUTTER_BIN:-flutter}" || true)
if [[ -z "$flutter_bin" || ! -x "$flutter_bin" ]]; then
    echo "puredart demo: put Flutter 3.41.2 on PATH or set FLUTTER_BIN to its executable" >&2
    exit 1
fi
flutter_bin=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "$flutter_bin")

android_sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
if [[ -z "$android_sdk" && -f "$app_dir/android/local.properties" ]]; then
    android_sdk=$(sed -n 's/^sdk\.dir=//p' "$app_dir/android/local.properties" | head -n 1)
fi
if [[ -z "$android_sdk" || ! -d "$android_sdk" ]]; then
    echo "puredart demo: set ANDROID_HOME to an Android SDK containing platform 37" >&2
    exit 1
fi
export ANDROID_HOME="$android_sdk"

# Run from the app directory so PATH-based version managers use mise.toml.
(
    cd "$app_dir"
    "$flutter_bin" --version --machine | python3 -c '
import json, sys
version = json.load(sys.stdin)
if version.get("frameworkVersion") != "3.41.2" or version.get("dartSdkVersion", "").split(" ")[0] != "3.11.0":
    sys.exit("puredart demo: requires Flutter 3.41.2 / Dart 3.11.0; use example/app/mise.toml or FLUTTER_BIN")
'
    "$flutter_bin" build apk --release --target-platform android-arm64
)

"$root_dir/tools/blutter/setup-blutter.sh" "$blutter_dir"
mkdir -p "$analysis_dir"
python3 "$blutter_dir/blutter.py" \
    "$app_dir/build/app/outputs/flutter-apk/app-release.apk" \
    "$analysis_dir" --rebuild

if [[ ! -s "$meta_file" ]]; then
    echo "puredart demo: Blutter did not generate $meta_file" >&2
    exit 1
fi

"$app_dir/android/gradlew" -p "$demo_dir" stageRelease \
    "-PpuredartMeta=$meta_file"

install -m 0644 "$app_dir/build/app/outputs/flutter-apk/app-release.apk" \
    "$payload_dir/app-release.apk"
install -m 0644 "$demo_dir/load.js" "$payload_dir/load.js"

echo "puredart demo payload: $payload_dir"
