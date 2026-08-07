#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
sdk_root="${AWC_EMSDK_DIR:-$repo_root/../native-gb/native-gb-web/.cache/emsdk}"
build_root="$repo_root/build-web"
output_root="$repo_root/site/runtime"
gubsy_root="${AWC_GUBSY_DIR:-$repo_root/../Splonks/gubsy}"

if [[ ! -f "$sdk_root/emsdk_env.sh" ]]; then
    printf 'Emscripten SDK not found: %s\n' "$sdk_root" >&2
    exit 1
fi

# shellcheck disable=SC1091
source "$sdk_root/emsdk_env.sh" >/dev/null
emcmake cmake -S "$repo_root" -B "$build_root" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DAWC_GUBSY_DIR="$gubsy_root"
cmake --build "$build_root" --target adventures-with-chickens-remastered

cmake -E remove_directory "$output_root"
mkdir -p "$output_root"

wasm_hash="$(sha256sum "$build_root/adventures-with-chickens.wasm" | cut -c1-16)"
data_hash="$(sha256sum "$build_root/adventures-with-chickens.data" | cut -c1-16)"
wasm_name="adventures-with-chickens.$wasm_hash.wasm"
data_name="adventures-with-chickens.$data_hash.data"
cp "$build_root/adventures-with-chickens.wasm" "$output_root/$wasm_name"
cp "$build_root/adventures-with-chickens.data" "$output_root/$data_name"

temporary_js="$output_root/adventures-with-chickens.js"
sed \
    -e "s/adventures-with-chickens\\.wasm/$wasm_name/g" \
    -e "s/adventures-with-chickens\\.data/$data_name/g" \
    "$build_root/adventures-with-chickens.js" > "$temporary_js"
js_hash="$(sha256sum "$temporary_js" | cut -c1-16)"
js_name="adventures-with-chickens.$js_hash.js"
mv "$temporary_js" "$output_root/$js_name"

source_commit="$(git -C "$repo_root" rev-parse HEAD)"
printf '{\n  "schema": 1,\n  "source_commit": "%s",\n  "module": "%s",\n  "wasm": "%s",\n  "data": "%s"\n}\n' \
    "$source_commit" "$js_name" "$wasm_name" "$data_name" > "$output_root/manifest.json"

"$repo_root/scripts/build-web-audio.sh"
printf 'Built %s, %s, and %s\n' "$js_name" "$wasm_name" "$data_name"
