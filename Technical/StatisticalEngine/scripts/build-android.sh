#!/bin/bash
# STVG Android Build Script
# Builds: WASM engine → frontend → Capacitor → APK
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENGINE_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ENGINE_DIR"

echo "═══════════════════════════════════════════════════"
echo "  STVG Android Build"
echo "═══════════════════════════════════════════════════"
echo ""

# ── Step 1: Build C++ Engine to WASM ─────────────────────
echo "[1/5] Building C++ engine → WebAssembly..."
if ! command -v emcmake &> /dev/null; then
    echo "  ERROR: Emscripten not found. Install via:"
    echo "    git clone https://github.com/emscripten-core/emsdk.git"
    echo "    cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    echo "    source ./emsdk_env.sh"
    exit 1
fi

emcmake cmake -B build-wasm -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" \
    -G "Unix Makefiles" \
    2>&1 | tail -5

emmake cmake --build build-wasm --parallel 4 2>&1 | tail -5
echo "  Done: build-wasm/stvg_wasm.js + stvg_wasm.wasm + stvg_wasm.data"

# ── Step 2: Copy WASM output to frontend ──────────────────
echo "[2/5] Copying WASM module to frontend..."
cp build-wasm/stvg_wasm.js frontend/src/wasm/stvg_wasm.js
cp build-wasm/stvg_wasm.wasm frontend/public/stvg_wasm.wasm
cp build-wasm/stvg_wasm.data frontend/public/stvg_wasm.data
echo "  Done"

# ── Step 3: Build frontend ────────────────────────────────
echo "[3/5] Building React frontend..."
cd frontend
npm install --silent 2>&1 | tail -3
npm run build 2>&1 | tail -3
cd ..
echo "  Done: static/ ($(du -sh static 2>/dev/null | cut -f1 || echo '?'))"

# ── Step 4: Capacitor sync ─────────────────────────────────
echo "[4/5] Syncing Capacitor..."
cd frontend
if [ ! -d "android" ]; then
    echo "  Initializing Capacitor Android project..."
    npx cap add android 2>&1 | tail -3
fi
npx cap copy android 2>&1 | tail -3
npx cap sync android 2>&1 | tail -3
cd ..
echo "  Done"

# ── Step 5: Build APK ──────────────────────────────────────
echo "[5/5] Building Android APK..."
cd frontend/android
if command -v ./gradlew &> /dev/null; then
    ./gradlew assembleDebug 2>&1 | tail -5
    APK_PATH="app/build/outputs/apk/debug/app-debug.apk"
    if [ -f "$APK_PATH" ]; then
        APK_SIZE=$(du -h "$APK_PATH" | cut -f1)
        echo "  APK: $APK_PATH ($APK_SIZE)"
    fi
else
    echo "  Gradle not found. Open in Android Studio:"
    echo "    npx cap open android"
fi
cd ../..

echo ""
echo "═══════════════════════════════════════════════════"
echo "  Build complete!"
echo ""
echo "  To install on Pixel 10 Pro:"
echo "    adb install frontend/android/app/build/outputs/apk/debug/app-debug.apk"
echo ""
echo "  To open in Android Studio:"
echo "    cd frontend && npx cap open android"
echo "═══════════════════════════════════════════════════"
