#!/usr/bin/env bash
set -euo pipefail

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root_dir=$(cd "$demo_dir/../../.." && pwd)
payload_dir="$demo_dir/build/payload"
device_dir=/data/local/tmp/puredart-demo
package=com.example.hookme

adb=(uv run --locked --project "$root_dir" python "$root_dir/tools/android.py" adb)
if [[ -n "${FRIDA_BIN:-}" ]]; then
    frida=("$FRIDA_BIN")
else
    frida=(uv run --locked --project "$root_dir" --group injection frida)
fi

for file in app-release.apk libpuredart_demo.so load.js; do
    if [[ ! -f "$payload_dir/$file" ]]; then
        echo "puredart demo: missing $payload_dir/$file; run ./build-demo.sh first" >&2
        exit 1
    fi
done

"${adb[@]}" install -r "$payload_dir/app-release.apk"
"${adb[@]}" shell am force-stop "$package"
"${adb[@]}" shell mkdir -p "$device_dir"
"${adb[@]}" push "$payload_dir/libpuredart_demo.so" "$device_dir/libpuredart_demo.so.upload"

apk_path=$("${adb[@]}" shell pm path "$package" | tr -d '\r' | sed -n 's/^package://p' | head -n 1)
if [[ -z "$apk_path" || "$apk_path" != */base.apk ]]; then
    echo "puredart demo: cannot resolve installed APK path for $package" >&2
    exit 1
fi
native_dir=${apk_path%/base.apk}/lib/arm64
copy_script="mkdir -p '$native_dir'; \
cp '$device_dir/libpuredart_demo.so.upload' '$native_dir/libpuredart_demo.so'; \
chown system:system '$native_dir/libpuredart_demo.so'; \
chmod 755 '$native_dir/libpuredart_demo.so'; \
chcon u:object_r:apk_data_file:s0 '$native_dir/libpuredart_demo.so'; \
rm '$device_dir/libpuredart_demo.so.upload'"
if [[ $("${adb[@]}" shell id -u | tr -d '\r') == 0 ]]; then
    "${adb[@]}" shell "$copy_script"
else
    "${adb[@]}" shell su -c "$copy_script"
fi

parameters=$(printf '{"nativeDirectory":"%s"}' "$native_dir")
if [[ -n "${FRIDA_REMOTE:-}" ]]; then
    frida_device=(-H "$FRIDA_REMOTE")
elif [[ -n "${ANDROID_SERIAL:-}" ]]; then
    frida_device=(-D "$ANDROID_SERIAL")
else
    frida_device=(-U)
fi
exec "${frida[@]}" "${frida_device[@]}" -f "$package" -l "$payload_dir/load.js" -P "$parameters"
