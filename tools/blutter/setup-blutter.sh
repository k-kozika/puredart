#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
puredart_dir=$(cd "$script_dir/../.." && pwd)
blutter_dir=${1:-"$puredart_dir/third_party/blutter"}
expected_commit=46c64d36cd9ab493b253e87952d8d56989e6a182

if [[ ! -e "$blutter_dir/.git" ]]; then
    if [[ "$blutter_dir" == "$puredart_dir/third_party/blutter" && -e "$puredart_dir/.git" ]]; then
        echo "puredart: initializing pinned blutter submodule" >&2
        git -C "$puredart_dir" submodule update --init --recursive third_party/blutter
    else
        echo "puredart: standalone checkout; cloning pinned blutter" >&2
        git clone --branch feat/metadata https://github.com/k-kozika/blutter.git "$blutter_dir"
        git -C "$blutter_dir" checkout "$expected_commit"
    fi
fi

actual_commit=$(git -C "$blutter_dir" rev-parse HEAD)
if [[ "$actual_commit" != "$expected_commit" ]]; then
    echo "puredart: expected blutter $expected_commit, found $actual_commit" >&2
    exit 1
fi

echo "puredart: metadata-enabled blutter ready in $blutter_dir" >&2
