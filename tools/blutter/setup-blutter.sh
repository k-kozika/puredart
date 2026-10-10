#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
puredart_dir=$(cd "$script_dir/../.." && pwd)
blutter_dir=${1:-"$puredart_dir/third_party/blutter"}
expected_commit=4a60ac648bf448c5a7596437243bcd0b9376fdf0

if [[ ! -e "$blutter_dir/.git" ]]; then
    if [[ "$blutter_dir" == "$puredart_dir/third_party/blutter" && -e "$puredart_dir/.git" ]]; then
        echo "puredart: initializing pinned blutter submodule" >&2
        git -C "$puredart_dir" submodule update --init --recursive third_party/blutter
    else
        echo "puredart: standalone checkout; cloning pinned blutter" >&2
        git clone https://github.com/worawit/blutter.git "$blutter_dir"
        git -C "$blutter_dir" checkout "$expected_commit"
    fi
fi

actual_commit=$(git -C "$blutter_dir" rev-parse HEAD)
if [[ "$actual_commit" != "$expected_commit" ]]; then
    echo "puredart: expected blutter $expected_commit, found $actual_commit" >&2
    exit 1
fi

install -m 0644 "$script_dir/PuredartWriter.h" "$blutter_dir/blutter/src/PuredartWriter.h"
install -m 0644 "$script_dir/PuredartWriter.cpp" "$blutter_dir/blutter/src/PuredartWriter.cpp"
install -m 0644 "$script_dir/PuredartNaming.h" "$blutter_dir/blutter/src/PuredartNaming.h"

if git -C "$blutter_dir" apply --check "$script_dir/blutter.patch" 2>/dev/null; then
    git -C "$blutter_dir" apply "$script_dir/blutter.patch"
elif git -C "$blutter_dir" apply --reverse --check "$script_dir/blutter.patch" 2>/dev/null; then
    echo "puredart: blutter patch already applied" >&2
else
    echo "puredart: pinned blutter tree does not match blutter.patch" >&2
    exit 1
fi

echo "puredart: blutter writer installed in $blutter_dir" >&2
