#!/usr/bin/env bash
set -euo pipefail

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
payload_dir="$demo_dir/build/payload"
device_dir=/data/local/tmp/puredart-demo
package=com.example.hookme

if [[ -n "${ADB_BIN:-}" ]]; then
    adb_bin="$ADB_BIN"
elif [[ -n "${ANDROID_HOME:-}" && -x "$ANDROID_HOME/platform-tools/adb" ]]; then
    adb_bin="$ANDROID_HOME/platform-tools/adb"
else
    adb_bin=$(command -v adb || true)
fi
frida_bin=${FRIDA_BIN:-$(command -v frida || true)}
if [[ -z "$adb_bin" || ! -x "$adb_bin" ]]; then
    echo "puredart demo: adb not found; set ANDROID_HOME or ADB_BIN" >&2
    exit 1
fi
if [[ -z "$frida_bin" || ! -x "$frida_bin" ]]; then
    echo "puredart demo: frida not found; set FRIDA_BIN" >&2
    exit 1
fi

for file in app-release.apk libpuredart_demo.so load.js; do
    if [[ ! -f "$payload_dir/$file" ]]; then
        echo "puredart demo: missing $payload_dir/$file; run ./build-demo.sh first" >&2
        exit 1
    fi
done

"$adb_bin" install -r "$payload_dir/app-release.apk"
"$adb_bin" shell mkdir -p "$device_dir"
"$adb_bin" push "$payload_dir/libpuredart_demo.so" "$device_dir/libpuredart_demo.so.upload"

apk_path=$("$adb_bin" shell pm path "$package" | tr -d '\r' | sed -n 's/^package://p' | head -n 1)
if [[ -z "$apk_path" || "$apk_path" != */base.apk ]]; then
    echo "puredart demo: cannot resolve installed APK path for $package" >&2
    exit 1
fi
native_dir=${apk_path%/base.apk}/lib/arm64
"$adb_bin" shell su -c \
    "cp '$device_dir/libpuredart_demo.so.upload' '$native_dir/libpuredart_demo.so'; \
chown system:system '$native_dir/libpuredart_demo.so'; \
chmod 755 '$native_dir/libpuredart_demo.so'; \
chcon u:object_r:apk_data_file:s0 '$native_dir/libpuredart_demo.so'; \
rm '$device_dir/libpuredart_demo.so.upload'"

runtime_loader="$payload_dir/load-runtime.js"
sed "s|@PUREDART_NATIVE_DIR@|$native_dir|g" "$payload_dir/load.js" > "$runtime_loader"
exec "$frida_bin" -U -f "$package" -l "$runtime_loader"
