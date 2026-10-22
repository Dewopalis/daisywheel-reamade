#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build-ci}"
version="${2:?Usage: package-macos.sh [build-dir] version}"
app="$build_dir/daisywheel.app"
executable="$app/Contents/MacOS/daisywheel"
qt_prefix="$(brew --prefix qt@6)"
sdl2_prefix="$(brew --prefix sdl2)"
qtbase_prefix="$(brew --prefix qtbase)"

if [[ ! -d "$app" || ! -x "$executable" ]]; then
  echo "macOS app bundle not found: $app" >&2
  exit 1
fi

smoke_test_bundle() {
  local stage="$1"
  echo "Smoke-testing packaged app bundle ($stage)"
  python3 - "$executable" "$stage" <<'PY'
import os
import subprocess
import sys
import tempfile

environment = os.environ.copy()
environment.pop("DYLD_LIBRARY_PATH", None)
environment["SDL_AUDIODRIVER"] = "dummy"
environment["QT_DEBUG_PLUGINS"] = "1"
stage = sys.argv[2]
process = subprocess.Popen(
    [sys.argv[1], "--smoke-test"],
    env=environment,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
)
try:
    stdout, stderr = process.communicate(timeout=20)
except subprocess.TimeoutExpired:
    print(f"Packaged macOS app smoke test timed out ({stage}); sampling process {process.pid}", file=sys.stderr)
    try:
        with tempfile.NamedTemporaryFile() as sample_file:
            sample = subprocess.run(
                ["/usr/bin/sample", str(process.pid), "3", "-file", sample_file.name],
                capture_output=True,
                text=True,
                timeout=8,
            )
            sample_file.seek(0)
            sample_output = sample_file.read().decode(errors="replace")
        if sample_output:
            print("--- process sample (last 12000 characters) ---", file=sys.stderr)
            print(sample_output[-12000:], file=sys.stderr)
        if sample.stderr:
            print(sample.stderr, end="", file=sys.stderr)
    except (OSError, subprocess.TimeoutExpired) as sample_error:
        print(f"Could not capture process sample: {sample_error}", file=sys.stderr)
    process.kill()
    stdout, stderr = process.communicate()
    for stream_name, stream in (("stdout", stdout), ("stderr", stderr)):
        if stream:
            print(f"--- captured {stream_name} (last 12000 characters) ---", file=sys.stderr)
            print(stream[-12000:], file=sys.stderr)
    raise SystemExit(1)
if stdout:
    print(stdout, end="")
if stderr:
    print(stderr, end="", file=sys.stderr)
if process.returncode:
    raise SystemExit(f"Packaged app smoke test exited with status {process.returncode} ({stage})")
PY
}

qmake="$qt_prefix/bin/qmake"
if [[ ! -x "$qmake" ]]; then
  qmake="$qtbase_prefix/bin/qmake"
fi
plugin_root="$("$qmake" -query QT_INSTALL_PLUGINS)"
platform_plugin_source="$plugin_root/platforms/libqcocoa.dylib"
platform_plugin="$app/Contents/PlugIns/platforms/libqcocoa.dylib"
if [[ ! -f "$platform_plugin_source" ]]; then
  echo "Qt Cocoa platform plugin not found: $platform_plugin_source" >&2
  exit 1
fi

# Homebrew's plugin directory contains optional plugins whose dependencies are
# not installed with qt@6. Deploy the app's linked Qt frameworks and only the
# Cocoa platform plugin, avoiding unrelated plugin dependency scans.
mkdir -p "$(dirname "$platform_plugin")"
cp "$platform_plugin_source" "$platform_plugin"
"$qt_prefix/bin/macdeployqt" "$app" -always-overwrite -no-plugins -no-codesign \
  -executable="$platform_plugin" \
  -libpath="$qt_prefix/lib" -libpath="$qtbase_prefix/lib" \
  -libpath="$sdl2_prefix/lib"

# Check Qt deployment before applying the app-specific SDL library changes.
smoke_test_bundle "after Qt deployment"

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

# This launch separates SDL bundling from signing.
smoke_test_bundle "after SDL bundling, before signing"

# The bundle is unsigned for distribution and will need manual approval on
# first launch until the project has an Apple Developer ID and notarization.
echo "Ad-hoc signing the completed app bundle"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
smoke_test_bundle "after signing"

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
