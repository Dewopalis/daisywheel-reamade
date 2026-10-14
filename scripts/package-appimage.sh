#!/usr/bin/env bash
set -euo pipefail
version=${1:?Usage: package-appimage.sh MAJOR.MINOR.PATCH [build-directory]}
build=${2:-build-ci}
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || exit 2
project=$(cd "$(dirname "$0")/.." && pwd)
cd "$project"
tools="$project/work/packaging-tools"
appdir="$project/work/AppDir"
[[ ! -e $appdir ]] || { echo 'Use a clean work/AppDir for packaging' >&2; exit 1; }
mkdir -p "$tools" "$appdir" dist
fetch() {
    local name=$1 tag=$2 repository=$3 checksum=$4
    if [[ ! -f "$tools/$name" ]]; then
        curl --fail --location --retry 3 "https://github.com/$repository/releases/download/$tag/$name" -o "$tools/$name"
    fi
    printf '%s  %s\n' "$checksum" "$tools/$name" | sha256sum --check -
    chmod +x "$tools/$name"
}
fetch linuxdeploy-x86_64.AppImage 1-alpha-20251107-1 linuxdeploy/linuxdeploy c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
fetch linuxdeploy-plugin-qt-x86_64.AppImage 1-alpha-20250213-1 linuxdeploy/linuxdeploy-plugin-qt 15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724
export APPIMAGE_EXTRACT_AND_RUN=1
export NO_STRIP=1 # linuxdeploy's old bundled strip cannot handle modern RELR sections.
export ARCH=x86_64
export QMAKE=${QMAKE:-/usr/bin/qmake6}
DESTDIR="$appdir" cmake --install "$build" --prefix /usr
mkdir -p "$appdir/usr/share/daisywheel-reamade"
cp scripts/register-appimage.py "$appdir/usr/share/daisywheel-reamade/"
plugin_root=$("$QMAKE" -query QT_INSTALL_PLUGINS)
platforms=(libqoffscreen.so)
for plugin in "$plugin_root"/platforms/libqwayland*.so; do
    [[ -f $plugin ]] && platforms+=("$(basename "$plugin")")
done
[[ ${#platforms[@]} -gt 1 ]] || { echo 'Missing Qt Wayland platform plugin' >&2; exit 1; }
EXTRA_PLATFORM_PLUGINS=$(IFS=';'; echo "${platforms[*]}")
export EXTRA_PLATFORM_PLUGINS
# Wayland and layer-shell plugins are loaded dynamically, not linked by the EXE.
libraries=()
for group in wayland-shell-integration wayland-graphics-integration-client; do
    [[ -d "$plugin_root/$group" ]] || { echo "Missing Qt plugin group: $group" >&2; exit 1; }
    mkdir -p "$appdir/usr/plugins/$group"
    cp -a "$plugin_root/$group/." "$appdir/usr/plugins/$group/"
    while IFS= read -r -d '' plugin; do libraries+=(--library "$plugin"); done < <(find "$appdir/usr/plugins/$group" -name '*.so' -print0)
done
[[ -f "$appdir/usr/plugins/wayland-shell-integration/liblayer-shell.so" ]]
# Preserve the bundled dependency notices supplied by Debian/Arch.
mkdir -p "$appdir/usr/share/doc/third-party"
for package in /usr/share/doc/libqt6* /usr/share/doc/libsdl2* /usr/share/doc/layer-shell-qt /usr/share/doc/liblayershell*; do
    if [[ -f "$package/copyright" ]]; then
        mkdir -p "$appdir/usr/share/doc/third-party/$(basename "$package")"
        cp "$package/copyright" "$appdir/usr/share/doc/third-party/$(basename "$package")/"
    fi
done
export OUTPUT="$project/dist/daisywheel-reamade-$version-linux-x86_64.AppImage"
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --desktop-file "$appdir/usr/share/applications/org.daisywheel.keyboard.desktop" \
    --icon-file "$project/packaging/daisywheel-reamade.svg" \
    --custom-apprun "$project/packaging/AppRun" \
    "${libraries[@]}" --plugin qt --output appimage
[[ -s $OUTPUT ]]
