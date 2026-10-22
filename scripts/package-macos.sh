#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build-ci}"
version="${2:?Usage: package-macos.sh [build-dir] version}"
app="$build_dir/daisywheel.app"
executable="$app/Contents/MacOS/daisywheel"
qt_prefix="$(brew --prefix qt@6)"
sdl2_prefix="$(brew --prefix sdl2)"

if [[ ! -d "$app" || ! -x "$executable" ]]; then
  echo "macOS app bundle not found: $app" >&2
  exit 1
fi

"$qt_prefix/bin/macdeployqt" "$app" -always-overwrite -no-codesign \
  -libpath="$qt_prefix/lib" -libpath="$(brew --prefix qtbase)/lib" \
  -libpath="$sdl2_prefix/lib"

sdl2_load_path="$(otool -L "$executable" | awk '$1 ~ /libSDL2[^ ]*\.dylib$/ { print $1; exit }')"
if [[ -z "$sdl2_load_path" ]]; then
  echo "Could not find the SDL2 dependency in $executable" >&2
  exit 1
fi

sdl2_source="$sdl2_prefix/lib/libSDL2.dylib"
if [[ ! -f "$sdl2_source" ]]; then
  echo "SDL2 library not found: $sdl2_source" >&2
  exit 1
fi

frameworks="$app/Contents/Frameworks"
mkdir -p "$frameworks"
sdl2_bundled="$frameworks/libSDL2.dylib"
sdl2_bundle_reference='@executable_path/../Frameworks/libSDL2.dylib'
cp -L "$sdl2_source" "$sdl2_bundled"
install_name_tool -id "$sdl2_bundle_reference" "$sdl2_bundled"
install_name_tool -change "$sdl2_load_path" "$sdl2_bundle_reference" "$executable"

if otool -L "$executable" | grep -E '/opt/homebrew/|/usr/local/opt/'; then
  echo "The app still depends on a Homebrew library outside its bundle" >&2
  exit 1
fi

# The bundle is unsigned for distribution and will need manual approval on
# first launch until the project has an Apple Developer ID and notarization.
echo "Ad-hoc signing the completed app bundle"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
echo "Smoke-testing the packaged app bundle"
python3 - "$executable" <<'PY'
import os
import subprocess
import sys

environment = os.environ.copy()
environment.pop("DYLD_LIBRARY_PATH", None)
environment["SDL_AUDIODRIVER"] = "dummy"
try:
    subprocess.run([sys.argv[1], "--smoke-test"], env=environment, check=True, timeout=30)
except subprocess.TimeoutExpired as error:
    raise SystemExit(f"Packaged macOS app smoke test timed out after {error.timeout} seconds")
PY

case "$(uname -m)" in
  arm64|aarch64) architecture=arm64 ;;
  x86_64) architecture=x86_64 ;;
  *) echo "Unsupported macOS runner architecture: $(uname -m)" >&2; exit 1 ;;
esac

mkdir -p dist
zip_path="$PWD/dist/daisywheel-reamade-${version}-macos-${architecture}.zip"
dmg_path="$PWD/dist/daisywheel-reamade-${version}-macos-${architecture}.dmg"
staging="$(mktemp -d)"
trap 'rm -rf "$staging"' EXIT

app_copy="$staging/Daisywheel REAmade.app"
ditto "$app" "$app_copy"
ditto -c -k --sequesterRsrc --keepParent "$app_copy" "$zip_path"
ln -s /Applications "$staging/Applications"
hdiutil create -volname "Daisywheel REAmade ${version}" \
  -srcfolder "$staging" -ov -format UDZO "$dmg_path"
