#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

node --check "$repo_root/site/launcher.js"
node --check "$repo_root/site/runtime.js"
python3 - "$repo_root/site" <<'PY'
from pathlib import Path
import json
import sys

root = Path(sys.argv[1])
required = [
    "index.html",
    "styles.css",
    "launcher.js",
    "runtime.js",
    "_headers",
    "runtime/manifest.json",
    "audio/music/Rock2.ogg",
    "audio/sounds/Intro.wav",
]
missing = [path for path in required if not (root / path).is_file()]
if missing:
    raise SystemExit("missing web output: " + ", ".join(missing))

manifest = json.loads((root / "runtime/manifest.json").read_text())
for field in ("module", "wasm", "data"):
    if not (root / "runtime" / manifest[field]).is_file():
        raise SystemExit(f"manifest points to missing {field}: {manifest[field]}")
print("web bundle: pass")
PY
