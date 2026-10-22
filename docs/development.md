# Development

This guide is for contributors building Daisywheel from source. For normal
installation, see [installation](installation.md). Start with
[CONTRIBUTING.md](../CONTRIBUTING.md) before larger changes.

## Linux development build

The current development instructions target CachyOS / Arch Linux in a KDE
Plasma Wayland session. Check the session with `echo "$XDG_SESSION_TYPE"`; it
should print `wayland`.

Install dependencies:

```bash
sudo pacman -Syu --needed base-devel git cmake pkgconf qt6-base qt6-wayland layer-shell-qt wayland sdl2-compat
```

Requirements include C++17, CMake 3.20+, Qt 6.8+ Widgets/Wayland Client/DBus,
SDL2, the Wayland protocol scanner and LayerShellQt. Qt 6.8+ is needed for the
protocol generator's `PRIVATE_CODE` option. Other distributions need equivalent
packages; their setup commands have not been verified.

Clone and build:

```bash
git clone https://github.com/Dewopalis/daisywheel-reamade.git
cd daisywheel-reamade
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run local test mode with `./build/daisywheel`. It types only into its own test
field. To check controller detection, run `./build/daisywheel --controllers`.
Quit local test mode before enabling system mode so two instances do not
process the same controller input.

## Register a Linux source build with KDE

Run from the source directory. This installs for your user only:

```bash
install -Dm755 build/daisywheel "$HOME/.local/bin/daisywheel-reamade"
mkdir -p "$HOME/.local/share/applications"
cat > "$HOME/.local/share/applications/org.daisywheel.keyboard.desktop" <<EOF_DESKTOP
[Desktop Entry]
Type=Application
Name=Daisywheel REAmade
Comment=Controller-first keyboard for KDE Wayland
Exec="$HOME/.local/bin/daisywheel-reamade" --kde-input-method
Icon=input-keyboard-virtual
Terminal=false
NoDisplay=true
X-KDE-Wayland-VirtualKeyboard=true
EOF_DESKTOP
kbuildsycoca6 --noincremental
```

Select Daisywheel REAmade under **System Settings → Keyboard → Virtual
Keyboard**, click **Apply**, and choose the **Show Virtual Keyboard** policy.
KWin launches the selected keyboard; no separate Autostart entry is needed.
The selection persists across sessions. This does not install a keyboard for
the login screen or establish lock-screen support.

To update a source build, rebuild it and reinstall the executable. To remove
the user installation, first select **None** or another keyboard in KDE and
click **Apply**, then run:

```bash
rm -- "$HOME/.local/share/applications/org.daisywheel.keyboard.desktop"
rm -- "$HOME/.local/bin/daisywheel-reamade"
kbuildsycoca6 --noincremental
```

Avoid mixing source and package installations. A user-local KDE desktop entry
from a source build can take precedence over the package entry.

## Windows build and tests

Windows contributors should use the [Windows build and testing guide](windows-build-and-testing.md)
for the Qt/MinGW toolchain, build script, interactive input and focus tests,
and validation limits. The tests need an unlocked interactive Windows
desktop.

## Development builds

The [Build packages workflow](https://github.com/Dewopalis/daisywheel-reamade/actions/workflows/packages.yml)
builds Windows, Linux and experimental macOS packages. To try a recent build,
sign in to GitHub, open a successful workflow run, and download the bundle you
need from **Artifacts**:

- `windows-x64`: setup `.exe` and portable `.zip`
- `linux-debian-appimage`: `.deb` and `.AppImage`
- `linux-fedora-rpm`: `.rpm`
- `macos-<architecture>`: `.dmg` and `.zip`

Artifacts expire after 14 days and may not have been manually validated on
handheld hardware. For persistent downloads and package requirements, see
[release packaging](releases/packaging.md).
