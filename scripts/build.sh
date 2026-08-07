#!/usr/bin/env bash
set -euo pipefail

preset="${1:-dev}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$repo_root"
cmake --preset "$preset"
cmake --build --preset "$preset"
