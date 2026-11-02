#!/usr/bin/env bash
# Android build (ADR 007, release signing in ADR 010). Runs inside the Android Docker image; on Windows use:
#   .\tools\build_android.ps1 [-Release]
# Needs the recompiler output first (tools/recomp.sh). Submodules stay untouched: lib/ is copied to the
# volume at /build, where the patches in tools/android/patches/ are applied.
# Steps: all (default), native, apk.
set -euo pipefail

STEP=${1:-all}
ROOT=/work
TREE=/build/tree
OUT=/build/native
NATIVE_LIBS=/build/jniLibs/arm64-v8a

if grep -q $'\r' "$0"; then
    echo "error: $0 has CRLF line endings; check out with LF (see .gitattributes)" >&2
    exit 1
fi

for input in RecompiledFuncs RecompiledPatches/patches.c rsp/aspMain.cpp; do
    [ -e "$ROOT/$input" ] || { echo "error: $input missing; run tools/recomp.sh first" >&2; exit 1; }
done

sync_tree() {
    mkdir -p "$TREE"
    # Patched files must stay patched between builds, so lib/ is only recopied when a patch or a pin changed.
    local stamp
    stamp=$( { cat "$ROOT"/tools/android/patches/*/*.patch; \
               cat "$ROOT"/.git/modules/lib/*/HEAD 2>/dev/null || true; } | sha256sum | cut -d' ' -f1)
    if [ "$(cat "$TREE/.lib_stamp" 2>/dev/null || true)" != "$stamp" ]; then
        echo "== copy lib/ and apply patches"
        rsync -a --delete --exclude .git "$ROOT/lib/" "$TREE/lib/"
        apply_patches N64ModernRuntime lib/N64ModernRuntime
        apply_patches rt64 lib/rt64
        apply_patches plume lib/rt64/src/contrib/plume
        apply_patches RecompFrontend lib/RecompFrontend
        echo "$stamp" > "$TREE/.lib_stamp"
    fi
    local item
    for item in CMakeLists.txt recompcontrollerdb.txt assets icons include src rsp RecompiledFuncs RecompiledPatches patches mods; do
        if [ -e "$ROOT/$item" ]; then rsync -a --delete "$ROOT/$item" "$TREE/"; fi
    done
}

# The Windows checkout has CRLF text files and the patches are LF, hence --ignore-whitespace.
apply_patches() {
    local name=$1 dir=$2 patch
    for patch in "$ROOT/tools/android/patches/$name"/*.patch; do
        [ -e "$patch" ] || continue
        echo "   $name: $(basename "$patch")"
        (cd "$TREE/$dir" && git apply --ignore-whitespace -p1 "$patch")
    done
}

build_native() {
    sync_tree

    echo "== host tools"
    mkdir -p /build/host
    clang++ -std=c++17 -O2 "$TREE/lib/rt64/src/tools/file_to_c/file_to_c.cpp" -o /build/host/file_to_c

    echo "== configure"
    cmake -S "$TREE" -B "$OUT" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DROBOTRON_FILE_TO_C=/build/host/file_to_c \
        -DRT64_FILE_TO_C=/build/host/file_to_c

    echo "== build"
    cmake --build "$OUT" --target Robotron64Recompiled -j "$(nproc)" -- -k 0

    mkdir -p "$NATIVE_LIBS"
    cp "$OUT/libRobotron64.so" "$NATIVE_LIBS/"
    cp "$(find "$OUT" -name 'libSDL2.so' | head -1)" "$NATIVE_LIBS/"
    cp "$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" "$NATIVE_LIBS/"
    file "$NATIVE_LIBS/libRobotron64.so"
}

build_apk() {
    sync_tree
    echo "== apk"
    rsync -a --delete "$ROOT/android/" /build/android/
    local version
    version=$(sed -n 's/^project(Robotron64Recompiled VERSION \([0-9.]*\).*/\1/p' "$TREE/CMakeLists.txt")
    [ -n "$version" ] || { echo "error: no version found in CMakeLists.txt" >&2; exit 1; }
    export GRADLE_USER_HOME=/build/gradle
    # Keeps the debug signing key in the volume. A new key per container makes Android refuse updates.
    export ANDROID_USER_HOME=/build/android-user
    python3 "$ROOT/tools/collect_licenses.py" /build/licenses
    cp "$ROOT/CREDITS.md" /build/licenses/
    local gradle_args=(
        -PappVersion="$version"
        -PnativeLibs=/build/jniLibs
        -PsdlJava="$OUT/_deps/sdl2-src/android-project/app/src/main/java"
        -PgameAssets="$TREE/assets"
        -PcontrollerDb="$TREE/recompcontrollerdb.txt"
        -Plicenses=/build/licenses
    )
    mkdir -p "$ROOT/build-android"
    if [ "${ROBOTRON_RELEASE:-}" = 1 ]; then
        # The native build keeps debug info for the debug APK; copies without it go into the release.
        rm -rf /build/jniLibs-release
        cp -r /build/jniLibs /build/jniLibs-release
        "$ANDROID_NDK"/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip --strip-debug /build/jniLibs-release/arm64-v8a/*.so
        gradle_args+=(-PnativeLibs=/build/jniLibs-release)
        (cd /build/android && gradle --no-daemon -q assembleRelease "${gradle_args[@]}" \
            -PreleaseKeystore=/signing/release.jks -PreleasePasswordFile=/signing/keystore-password.txt)
        local apk="$ROOT/build-android/Robotron64-$version.apk"
        cp /build/android/app/build/outputs/apk/release/app-release.apk "$apk"
        local tools
        tools=$(ls -d "$ANDROID_HOME"/build-tools/* | sort -V | tail -1)
        "$tools/apksigner" verify --print-certs "$apk"
        if "$tools/aapt" dump badging "$apk" | grep -q application-debuggable; then
            echo "error: the release APK is debuggable" >&2
            exit 1
        fi
    else
        (cd /build/android && gradle --no-daemon -q assembleDebug "${gradle_args[@]}")
        local apk="$ROOT/build-android/Robotron64-debug.apk"
        cp /build/android/app/build/outputs/apk/debug/app-debug.apk "$apk"
    fi
    ls -la "$apk"
}

case "$STEP" in
    native) build_native ;;
    apk) build_apk ;;
    all) build_native; build_apk ;;
    *) echo "usage: $0 [all|native|apk]" >&2; exit 1 ;;
esac
