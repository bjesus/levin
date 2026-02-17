#!/bin/bash
# Build Levin.app for macOS
# Usage: ./build.sh [--stub]
#
# Requires: cmake, swiftc, Xcode Command Line Tools
# For real session (default): also requires boost, openssl (brew install boost openssl)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-macos"
APP_DIR="$SCRIPT_DIR/Levin.app"
SRC_DIR="$SCRIPT_DIR/Levin/Sources"
RES_DIR="$SCRIPT_DIR/Levin/Resources"

USE_STUB=OFF
BUILD_TYPE=Release

for arg in "$@"; do
    case "$arg" in
        --stub) USE_STUB=ON ;;
        --debug) BUILD_TYPE=Debug ;;
    esac
done

echo "==> Building liblevin (stub=$USE_STUB, type=$BUILD_TYPE)"

CMAKE_ARGS=(
    -S "$ROOT_DIR" -B "$BUILD_DIR"
    -DLEVIN_USE_STUB_SESSION="$USE_STUB"
    -DLEVIN_BUILD_DAEMON=OFF
    -DLEVIN_BUILD_TESTS=OFF
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
)

# Homebrew OpenSSL is keg-only — pass the root dir so CMake can find it
if [ "$USE_STUB" = "OFF" ]; then
    OPENSSL_PREFIX="$(brew --prefix openssl 2>/dev/null || true)"
    if [ -n "$OPENSSL_PREFIX" ]; then
        CMAKE_ARGS+=(-DOPENSSL_ROOT_DIR="$OPENSSL_PREFIX")
    fi
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu)"

# Collect all static libraries that need to be linked
LIBS=("-L$BUILD_DIR/liblevin" "-llevin")
LIBS+=("-lcurl")

if [ "$USE_STUB" = "OFF" ]; then
    # Real libtorrent session — link all the static deps.
    # libdatachannel is fetched as a libtorrent subdep, so its build
    # artifacts live inside libtorrent-build/deps/, not at top-level _deps/.
    LT_BUILD="$BUILD_DIR/_deps/libtorrent-build"

    LIBS+=("-L$LT_BUILD" "-ltorrent-rasterbar")

    # Find all static libraries from libdatachannel and its transitive deps
    for lib in datachannel-static juice-static srtp2 usrsctp; do
        found=$(find "$BUILD_DIR" -name "lib${lib}.a" -print -quit 2>/dev/null)
        if [ -n "$found" ]; then
            LIBS+=("$found")
        fi
    done

    # OpenSSL from Homebrew
    if [ -d "/opt/homebrew/opt/openssl" ]; then
        LIBS+=("-L/opt/homebrew/opt/openssl/lib")
    elif [ -d "/usr/local/opt/openssl" ]; then
        LIBS+=("-L/usr/local/opt/openssl/lib")
    fi
    LIBS+=("-lssl" "-lcrypto")
fi

echo "==> Libraries to link: ${LIBS[*]}"
echo "==> Compiling Swift sources"
SWIFT_FILES=("$SRC_DIR"/*.swift)

# swiftc needs linker flags passed via -Xlinker
LINKER_FLAGS=()
for lib in "${LIBS[@]}"; do
    LINKER_FLAGS+=(-Xlinker "$lib")
done
for fw in AppKit SwiftUI IOKit CoreFoundation CoreServices Security SystemConfiguration Network; do
    LINKER_FLAGS+=(-Xlinker -framework -Xlinker "$fw")
done
LINKER_FLAGS+=(-Xlinker -lc++)

SWIFT_FLAGS=(
    -import-objc-header "$SRC_DIR/Levin-Bridging-Header.h"
    -I "$ROOT_DIR/liblevin/include"
    "${LINKER_FLAGS[@]}"
)

if [ "$BUILD_TYPE" = "Release" ]; then
    SWIFT_FLAGS+=(-O)
else
    SWIFT_FLAGS+=(-g -Onone)
fi

# Build for the native architecture
BINARY="$SCRIPT_DIR/levin-bin"
swiftc \
    -target "$(uname -m)-apple-macos14.0" \
    "${SWIFT_FLAGS[@]}" \
    "${SWIFT_FILES[@]}" \
    -o "$BINARY"

echo "==> Creating Levin.app bundle"
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/Contents/MacOS"
mkdir -p "$APP_DIR/Contents/Resources"

cp "$BINARY" "$APP_DIR/Contents/MacOS/Levin"
cp "$RES_DIR/Info.plist" "$APP_DIR/Contents/"

# Generate .icns from AppIcon.png using iconutil (macOS only)
if [ -f "$RES_DIR/AppIcon.png" ] && command -v iconutil &>/dev/null && command -v sips &>/dev/null; then
    ICONSET="$SCRIPT_DIR/AppIcon.iconset"
    mkdir -p "$ICONSET"
    for size in 16 32 64 128 256 512; do
        sips -z "$size" "$size" "$RES_DIR/AppIcon.png" --out "$ICONSET/icon_${size}x${size}.png" &>/dev/null
    done
    for size in 16 32 128 256; do
        doubled=$((size * 2))
        cp "$ICONSET/icon_${doubled}x${doubled}.png" "$ICONSET/icon_${size}x${size}@2x.png" 2>/dev/null || true
    done
    cp "$ICONSET/icon_512x512.png" "$ICONSET/icon_256x256@2x.png" 2>/dev/null || true
    iconutil -c icns "$ICONSET" -o "$APP_DIR/Contents/Resources/AppIcon.icns"
    rm -rf "$ICONSET"
    echo "  Generated AppIcon.icns"
elif [ -f "$RES_DIR/AppIcon.icns" ]; then
    cp "$RES_DIR/AppIcon.icns" "$APP_DIR/Contents/Resources/"
fi

# Copy menu bar icon (template image)
[ -f "$RES_DIR/MenuBarIcon.png" ] && cp "$RES_DIR/MenuBarIcon.png" "$APP_DIR/Contents/Resources/"
[ -f "$RES_DIR/MenuBarIcon@2x.png" ] && cp "$RES_DIR/MenuBarIcon@2x.png" "$APP_DIR/Contents/Resources/"

# Ad-hoc code sign (required on Apple Silicon)
echo "==> Code signing (ad-hoc)"
codesign --force --deep -s - "$APP_DIR"

# Clean up intermediate binary
rm -f "$BINARY"

echo "==> Done: $APP_DIR"
