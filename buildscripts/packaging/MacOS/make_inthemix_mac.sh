#!/bin/bash
#
# InTheMix by Anonymous Tracks: the Mac app in a DMG (Apple Silicon), signed
# ad-hoc (free, no Apple account: enough for Apple Silicon to run it; users
# open it once with System Settings > Privacy & Security > Open Anyway).
# Based on GlideMix's make_glidemix_alpha.sh. Used by .github/workflows/macos.yml.
#
#   buildscripts/packaging/MacOS/make_inthemix_mac.sh <build-dir> <stage-dir> <out-dir>
#
# The build must already be installed into <stage-dir> (cmake --install).

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
BUILD="$1"
STAGE="$2"
OUT="$3"
FLAVOR="${4:-arm64}"   # arm64 | universal
v() { grep -E "set\(MUSE_APP_VERSION_$1 " "$ROOT/version.cmake" | grep -oE '"[0-9]+"' | tr -d '"'; }
VERSION="$(v MAJOR).$(v MINOR).$(v PATCH)"
LABEL="$(v MAJOR).$(v MINOR)"
[ "$(v PATCH)" != "0" ] && LABEL="$VERSION"

APP="$(find "$STAGE" -maxdepth 1 -name "*.app" | head -1)"
[ -d "$APP" ] || { echo "no app in $STAGE"; ls -la "$STAGE"; exit 1; }
if [ "$(basename "$APP")" != "InTheMix.app" ]; then
    mv "$APP" "$STAGE/InTheMix.app"
    APP="$STAGE/InTheMix.app"
fi
echo "== App: $APP"

echo "== Analysis models"
# Beat This! (the beat grid), found by mixHouseModelPath(). The GlideMix phrase
# model is not shipped (no licence stated): the built-in phrase detector is used.
MODELS="$APP/Contents/Resources/models"
mkdir -p "$MODELS"
cp "$ROOT/mixhouse_deps/beat_this/beat_this.onnx" "$MODELS/"
cp "$ROOT/src/mixhouse/models/beat_this_small.onnx" "$MODELS/"
ls -la "$MODELS"

if [ -d "$OUT/ffmpeg" ]; then
    echo "== FFmpeg (LGPL)"
    mkdir -p "$APP/Contents/Frameworks"
    rm -rf "$APP/Contents/Frameworks/ffmpeg"
    cp -R "$OUT/ffmpeg" "$APP/Contents/Frameworks/ffmpeg"
fi
FFMPEG_DIR="$APP/Contents/Frameworks/ffmpeg"

echo "== Copy in libraries Qt's deploy step missed"
FW="$APP/Contents/Frameworks"
mkdir -p "$FW"
for _pass in 1 2 3 4 5; do
    added=0
    while IFS= read -r lib; do
        [ -e "$FW/$lib" ] && continue
        found=$(find "$BUILD" "$ROOT/mixhouse_deps" -name "$lib" \( -type f -o -type l \) 2>/dev/null | head -1)
        if [ -z "$found" ]; then
            echo "Cannot find $lib"
            exit 1
        fi
        echo "  + $lib"
        cp -L "$found" "$FW/$lib"
        chmod u+w "$FW/$lib"
        install_name_tool -id "@rpath/$lib" "$FW/$lib" 2>/dev/null || true
        added=1
    done < <(find "$APP/Contents" -path "$FFMPEG_DIR" -prune -o -type f \( -name "*.dylib" -o -perm +111 \) -print0 \
        | xargs -0 otool -L 2>/dev/null | grep -oE "@rpath/[^ /]+\.dylib" | sed 's|@rpath/||' | sort -u)
    [ "$added" = 0 ] && break
done

echo "== Check nothing points back to the build machine"
LEAKS=$(find "$APP/Contents" -type f \( -name "*.dylib" -o -perm +111 \) -print0 \
    | xargs -0 otool -L 2>/dev/null | grep -E "^\s+/(Users|opt|usr/local)/" | sort -u || true)
if [ -n "$LEAKS" ]; then
    echo "These libraries are not inside the app:"
    echo "$LEAKS"
    exit 1
fi

echo "== Licences"
mkdir -p "$APP/Contents/Resources/licenses"
cp "$ROOT/LICENSE.txt" "$APP/Contents/Resources/licenses/InTheMix-LICENSE.txt"
cp "$ROOT/BUILDING-INTHEMIX.md" "$APP/Contents/Resources/licenses/THIRD-PARTY.md"

echo "== Remove debug symbols"
find "$APP/Contents" -type d -name "*.dSYM" -prune -exec rm -rf {} +

echo "== Sign ad-hoc"
find "$APP/Contents" -type f \( -name "*.dylib" -o -name "*.so" \) -print0 \
    | xargs -0 -P 4 -I {} codesign --force --sign - '{}'
find "$APP/Contents/Frameworks" -maxdepth 1 -name "*.framework" -print0 2>/dev/null \
    | xargs -0 -I {} codesign --force --sign - '{}'
codesign --force --deep --options runtime \
    --entitlements "$HERE/macosx_entitlements.plist" \
    --sign - "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

echo "== DMG"
DMG_DIR="$OUT/dmg"
rm -rf "$DMG_DIR"
mkdir -p "$DMG_DIR"
cp -R "$APP" "$DMG_DIR/"
ln -s /Applications "$DMG_DIR/Applications"
cat > "$DMG_DIR/How to open InTheMix.txt" <<'TXT'
InTheMix by Anonymous Tracks -- how to open it the first time

InTheMix is not signed with an Apple Developer ID, so macOS asks you to
confirm once:

1. Drag InTheMix into Applications.
2. Double-click InTheMix. macOS says it cannot verify it: click Done.
3. Open System Settings > Privacy & Security, scroll down to
   "InTheMix was blocked..." and click Open Anyway.
4. Confirm with your password. From then on it opens normally.

macOS may also ask whether InTheMix may use your Documents or Music
folders (where your songs and mixes are): allow the folders your music is in.

InTheMix is open source (GPL v3): https://github.com/anonymoustracks/InTheMix-
Website and support: https://anonymoustracks.com
TXT
DMG="$OUT/InTheMix-Beta-v$LABEL-mac-$FLAVOR.dmg"
rm -f "$DMG"
hdiutil create -volname "InTheMix Beta v$LABEL" -srcfolder "$DMG_DIR" -ov -format UDZO "$DMG" >/dev/null
rm -rf "$DMG_DIR"
echo "== Done: $DMG ($(du -h "$DMG" | cut -f1))"
