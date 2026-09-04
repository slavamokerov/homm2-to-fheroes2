#!/usr/bin/env bash
# Builds the WASM core for the web converter (GitHub Pages).
# Requires the Emscripten SDK (emcc/em++) on PATH. Output: web/deploy/.
set -euo pipefail

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
OUT="$ROOT/web/deploy"

mkdir -p "$OUT"

em++ -O2 \
  -std=c++17 \
  -I"$ROOT/src" \
  --no-entry \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createH2convModule \
  -s USE_ZLIB=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s ENVIRONMENT=web \
  -s EXPORTED_RUNTIME_METHODS=[] \
  -s FILESYSTEM=0 \
  -s DISABLE_EXCEPTION_CATCHING=0 \
  --bind \
  -o "$OUT/h2conv.js" \
  "$ROOT/src/homm2_save.cpp" \
  "$ROOT/src/convert.cpp" \
  "$ROOT/src/fheroes2_save.cpp" \
  "$ROOT/web/wasm_api.cpp"

# Static site files (index.html, style.css, app.js, fonts) live next to the
# wasm output in web/deploy/.
cp "$ROOT/web/index.html" "$ROOT/web/style.css" "$ROOT/web/app.js" "$OUT/"
cp -R "$ROOT/web/fonts" "$OUT/fonts"
if [ -f "$ROOT/web/robots.txt" ]; then cp "$ROOT/web/robots.txt" "$OUT/"; fi
if [ -f "$ROOT/web/sitemap.xml" ]; then cp "$ROOT/web/sitemap.xml" "$OUT/"; fi
if [ -f "$ROOT/web/icon.png" ]; then cp "$ROOT/web/icon.png" "$OUT/icon.png"; fi
if [ -f "$ROOT/web/og-image.png" ]; then cp "$ROOT/web/og-image.png" "$OUT/"; fi
if [ -f "$ROOT/web/convert-logo.png" ]; then cp "$ROOT/web/convert-logo.png" "$OUT/"; fi

echo "Built. Deploy folder: $OUT"
ls -lh "$OUT"
