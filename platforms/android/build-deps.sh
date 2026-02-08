#!/usr/bin/env bash
# Build/download native dependencies for the Android libtorrent+WebRTC build.
# Run once before ./gradlew assembleDebug.
#
# What it does:
#   1. Downloads Boost headers (header-only, no compilation needed)
#   2. Cross-compiles OpenSSL static libs for each Android ABI
#
# Prerequisites:
#   - ANDROID_NDK_HOME or NDK at ~/Android/Sdk/ndk/<version>
#   - perl, make, curl
#
# Output:
#   prebuilt/boost/include/boost/...
#   prebuilt/openssl/<abi>/{lib,include}/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PREBUILT_DIR="${SCRIPT_DIR}/prebuilt"
BUILD_DIR="${SCRIPT_DIR}/.deps-build"
mkdir -p "$BUILD_DIR"

# ========== Boost headers ==========
BOOST_VERSION="1.83.0"
BOOST_VERSION_UNDERSCORE="${BOOST_VERSION//./_}"
BOOST_TARBALL="boost_${BOOST_VERSION_UNDERSCORE}.tar.gz"
BOOST_URL="https://archives.boost.io/release/${BOOST_VERSION}/source/${BOOST_TARBALL}"
BOOST_PREFIX="${PREBUILT_DIR}/boost"

if [ -f "${BOOST_PREFIX}/include/boost/version.hpp" ]; then
    echo "Boost ${BOOST_VERSION} headers already present, skipping"
else
    echo "===== Downloading Boost ${BOOST_VERSION} headers ====="
    BOOST_DL="${BUILD_DIR}/${BOOST_TARBALL}"
    if [ ! -f "$BOOST_DL" ]; then
        curl -fSL "$BOOST_URL" -o "$BOOST_DL"
    fi
    BOOST_SRC="${BUILD_DIR}/boost_${BOOST_VERSION_UNDERSCORE}"
    rm -rf "$BOOST_SRC"
    tar xzf "$BOOST_DL" -C "$BUILD_DIR"
    # Copy only the headers (no compiled libraries needed)
    mkdir -p "${BOOST_PREFIX}/include"
    cp -r "${BOOST_SRC}/boost" "${BOOST_PREFIX}/include/"
    rm -rf "$BOOST_SRC"
    echo "===== Boost headers installed to ${BOOST_PREFIX}/include ====="
fi

# ========== OpenSSL ==========
OPENSSL_VERSION="openssl-3.3.2"
OPENSSL_URL="https://github.com/openssl/openssl/releases/download/${OPENSSL_VERSION}/${OPENSSL_VERSION}.tar.gz"
ANDROID_API=26  # minSdk from build.gradle.kts
OPENSSL_PREBUILT="${PREBUILT_DIR}/openssl"

# Find NDK
if [ -z "${ANDROID_NDK_HOME:-}" ]; then
    for d in ~/Android/Sdk/ndk/*/; do
        ANDROID_NDK_HOME="${d%/}"
    done
fi
if [ -z "${ANDROID_NDK_HOME:-}" ] || [ ! -d "$ANDROID_NDK_HOME" ]; then
    echo "ERROR: ANDROID_NDK_HOME not set and NDK not found" >&2
    exit 1
fi
echo "Using NDK: $ANDROID_NDK_HOME"

declare -A ABI_TARGET=(
    [arm64-v8a]=android-arm64
    [armeabi-v7a]=android-arm
    [x86_64]=android-x86_64
)

OPENSSL_TARBALL="${BUILD_DIR}/${OPENSSL_VERSION}.tar.gz"
if [ ! -f "$OPENSSL_TARBALL" ]; then
    echo "Downloading ${OPENSSL_VERSION}..."
    curl -fSL "$OPENSSL_URL" -o "$OPENSSL_TARBALL"
fi

for ABI in "${!ABI_TARGET[@]}"; do
    TARGET="${ABI_TARGET[$ABI]}"
    PREFIX="${OPENSSL_PREBUILT}/${ABI}"
    SRC="${BUILD_DIR}/${OPENSSL_VERSION}-${ABI}"

    if [ -f "${PREFIX}/lib/libssl.a" ] && [ -f "${PREFIX}/lib/libcrypto.a" ]; then
        echo "OpenSSL already built for ${ABI}, skipping"
        continue
    fi

    echo "===== Building OpenSSL for ${ABI} (${TARGET}) ====="

    rm -rf "$SRC"
    mkdir -p "$SRC"
    tar xzf "$OPENSSL_TARBALL" -C "$SRC" --strip-components=1

    pushd "$SRC" >/dev/null

    export ANDROID_NDK_ROOT="$ANDROID_NDK_HOME"
    export PATH="${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH"

    # -fPIC is required because the static libs are linked into liblevin.so.
    # Without it, ARMv7 assembly relocations fail with R_ARM_REL32 errors.
    ./Configure "$TARGET" \
        -D__ANDROID_API__=$ANDROID_API \
        --prefix="$PREFIX" \
        --openssldir="$PREFIX/ssl" \
        -fPIC \
        no-shared \
        no-tests \
        no-ui-console \
        no-comp \
        no-engine \
        no-docs

    make -j"$(nproc)"
    make install_sw

    popd >/dev/null
    rm -rf "$SRC"

    echo "===== OpenSSL built for ${ABI} -> ${PREFIX} ====="
done

echo ""
echo "Done. Dependencies at: ${PREBUILT_DIR}/"
echo "  Boost:   $(ls "${BOOST_PREFIX}/include/boost/version.hpp" 2>/dev/null || echo 'not found')"
ls -la "${OPENSSL_PREBUILT}"/*/lib/libssl.a 2>/dev/null || true
