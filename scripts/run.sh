#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
preset="${AWC_PRESET:-dev}"
case "$preset" in
    dev) build_dir="build-debug" ;;
    release) build_dir="build-release" ;;
    asan) build_dir="build-asan" ;;
    *)
        echo "Unknown preset: $preset" >&2
        exit 2
        ;;
esac

"$repo_root/scripts/build.sh" "$preset"
if [[ "$preset" == "dev" || "$preset" == "asan" ]]; then
    exec "$repo_root/$build_dir/adventures-with-chickens-remastered" \
        --width 1280 --height 720 "$@"
fi

exec "$repo_root/$build_dir/adventures-with-chickens-remastered" "$@"
