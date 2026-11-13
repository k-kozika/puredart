#!/usr/bin/env bash
set -euo pipefail

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root_dir=$(cd "$demo_dir/../../.." && pwd)
app_dir="$root_dir/hermes/example/app"
work_dir="$demo_dir/build"
payload_dir="$work_dir/payload"
meta_file="$work_dir/metadata/counter_meta.h"

for tool in bun node uv ninja java; do
    command -v "$tool" >/dev/null || { echo "purehermes demo: missing $tool" >&2; exit 1; }
done
export ANDROID_HOME
ANDROID_HOME=$(uv run --locked --project "$root_dir" python "$root_dir/tools/android.py" sdk)
android_ndk=$(uv run --locked --project "$root_dir" python "$root_dir/tools/android.py" --ndk-version 27.1.12297006 ndk)

(
    cd "$app_dir"
    bun install --frozen-lockfile
    CI=1 bunx expo prebuild --platform android --no-install
    ./android/gradlew -p android :app:assembleRelease \
        -PreactNativeArchitectures=arm64-v8a --console=plain
)
mkdir -p "$payload_dir" "$(dirname "$meta_file")"
install -m 0644 "$app_dir/android/app/build/outputs/apk/release/app-release.apk" \
    "$payload_dir/app-release.apk"
uv run --locked --project "$root_dir" python "$root_dir/tools/android.py" extract \
    "$payload_dir/app-release.apk" assets/index.android.bundle "$work_dir/index.android.bundle"
uv run --locked --project "$root_dir" --group build cmake -S "$demo_dir" -B "$work_dir/host" -G Ninja -DCMAKE_BUILD_TYPE=Release
uv run --locked --project "$root_dir" --group build cmake --build "$work_dir/host" --target purehermes-meta --parallel 4
"$work_dir/host/purehermes/purehermes-meta" "$work_dir/index.android.bundle" \
    --emit "$meta_file" --function-name counterStep --alias counter_updater \
    --string-value +1 --alias counter_title
uv run --locked --project "$root_dir" --group build cmake -S "$demo_dir" -B "$work_dir/android" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$android_ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
    -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384 \
    -DPUREHERMES_DEMO_META="$meta_file"
uv run --locked --project "$root_dir" --group build cmake --build "$work_dir/android" --target purehermes_demo --parallel 4
install -m 0644 "$work_dir/android/libpurehermes_demo.so" "$payload_dir/libpurehermes_demo.so"
echo "purehermes demo payload: $payload_dir"
