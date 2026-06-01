#!/bin/bash
# STVG Distribution Packaging Script
# Builds release C++ and frontend, packages into dist/
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENGINE_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ENGINE_DIR"

echo "=== STVG Distribution Build ==="
echo ""

# 1. Build C++ in Release mode
echo "[1/5] Building C++ (Release)..."
cmake -B build-release -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release -Wno-dev 2>&1 | tail -3
cmake --build build-release --config Release --parallel 4 2>&1 | tail -3
echo "  Done: build-release/Release/stvg_server.exe"

# 2. Build frontend
echo "[2/5] Building frontend..."
cd frontend
npm run build 2>&1 | tail -3
cd ..
echo "  Done: static/ ($(du -sh static | cut -f1))"

# 3. Create dist directory
echo "[3/5] Creating dist/..."
rm -rf dist
mkdir -p dist
cp build-release/Release/stvg_server.exe dist/
cp -r static dist/
cp -r data dist/
cp run.bat dist/

# 4. Create zip
echo "[4/5] Packaging..."
cd dist
zip -r ../stvg-v1.0-win64.zip . -x "*.git*" 2>/dev/null || \
  7z a ../stvg-v1.0-win64.zip . 2>/dev/null || \
  echo "  (No zip/7z available — dist/ is ready for manual zipping)"
cd ..

# 5. Summary
echo ""
echo "[5/5] Package contents:"
echo "  dist/"
ls -lh dist/ | tail -n +2
echo ""
if [ -f stvg-v1.0-win64.zip ]; then
  echo "  Archive: stvg-v1.0-win64.zip ($(du -h stvg-v1.0-win64.zip | cut -f1))"
fi
echo ""
echo "=== Done ==="
