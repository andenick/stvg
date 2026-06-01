# STVG Android Build Guide

## Architecture

```
React Frontend (same code) ←→ WASM Bindings (embind) ←→ C++ Engine (in-browser)
        └── Capacitor Android WebView shell (APK)
```

The C++ engine compiles to WebAssembly via Emscripten. No server needed on the device — the engine runs directly in the WebView's V8 WASM runtime. The React frontend calls engine functions via `embind` instead of REST/WebSocket.

## Prerequisites

1. **Emscripten SDK** (3.x+)
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk && ./emsdk install latest && ./emsdk activate latest
   source ./emsdk_env.sh
   ```

2. **Node.js** (20+) + npm

3. **Android Studio** (2024+) with:
   - Android SDK 35 (Android 16)
   - Build Tools 35.0.0
   - Platform Tools (includes ADB)

4. **Java JDK 17+** (for Gradle)

## Quick Build

```bash
# From StatisticalEngine/ directory:
chmod +x scripts/build-android.sh
./scripts/build-android.sh
```

## Manual Build Steps

### Step 1: Compile Engine to WASM

```bash
# Activate Emscripten
source /path/to/emsdk/emsdk_env.sh

# Configure
emcmake cmake -B build-wasm -S . -DCMAKE_BUILD_TYPE=Release

# Build
emmake cmake --build build-wasm --parallel 4

# Output: build-wasm/stvg_wasm.js, .wasm, .data
```

### Step 2: Copy WASM to Frontend

```bash
cp build-wasm/stvg_wasm.js frontend/src/wasm/stvg_wasm.js
cp build-wasm/stvg_wasm.wasm frontend/public/stvg_wasm.wasm
cp build-wasm/stvg_wasm.data frontend/public/stvg_wasm.data
```

### Step 3: Build Frontend

```bash
cd frontend
npm install
npm run build
cd ..
```

### Step 4: Capacitor Setup (first time only)

```bash
cd frontend
npm install @capacitor/core @capacitor/cli @capacitor/android
npx cap init "STVG" "com.stvg.banking" --web-dir ../static
npx cap add android
cd ..
```

### Step 5: Build APK

```bash
cd frontend
npx cap copy android
npx cap sync android

# Option A: Android Studio
npx cap open android
# Build > Build Bundle(s)/APK(s) > Build APK(s)

# Option B: Command line
cd android
./gradlew assembleDebug
# APK at: app/build/outputs/apk/debug/app-debug.apk
```

### Step 6: Install on Device

```bash
adb install frontend/android/app/build/outputs/apk/debug/app-debug.apk
```

## Target Device: Pixel 10 Pro

| Spec | Value |
|------|-------|
| Display | 6.3" LTPO OLED, 1080×2424, 120Hz |
| Android | 16 |
| Chipset | Tensor G5 |
| RAM | 16 GB |
| WASM Support | V8 with full WASM 4.0 (SIMD, threads, bulk memory) |

## Development Workflow

For iterating on the frontend without rebuilding WASM:

```bash
cd frontend
npm run dev    # Vite dev server with hot reload
# Test in Chrome DevTools mobile emulation
```

When engine changes are needed:
```bash
emmake cmake --build build-wasm --parallel 4
cp build-wasm/stvg_wasm.* frontend/src/wasm/  # .js
cp build-wasm/stvg_wasm.* frontend/public/    # .wasm + .data
npm run build && npx cap copy android
```

## Troubleshooting

### spdlog compilation fails
The WASM build disables spdlog threading (`SPDLOG_NO_THREAD_ID`, `SPDLOG_NO_TLS`). If you still get pthread errors, the `CMakeLists.wasm.txt` may need `SPDLOG_NO_ATOMIC_LEVELS` added.

### WASM binary too large
Expected size: 3-5MB for the engine WASM + 2MB for preloaded data. Total APK: ~15-20MB.
To reduce: use `-Os` instead of `-O2`, or strip debug info.

### WebView performance
The Pixel 10 Pro's Chrome WebView supports WebAssembly SIMD and runs at near-native speed. If charts lag, reduce the market chart history from 504 points to 252.

### JSON files not found
Ensure `--preload-file` in CMake points to the correct `data/` directory. The virtual filesystem mounts at `/data/`.
