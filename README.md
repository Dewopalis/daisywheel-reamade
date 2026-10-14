# Daisywheel REAmade

**Controller-first typing for handheld computers and desktop PCs.**

Inspired by Steam's original Daisywheel, REAmade lets you select a petal with the left stick and enter its characters with the four face buttons. Open it with your controller on any display, or let KDE's automatic-opening policy decide when it appears.

The goal is a keyboard for Windows handhelds such as the ROG Ally and Linux systems running KDE Plasma, including CachyOS and Steam Deck Desktop Mode.

**Current status: early Linux/KDE Wayland prototype.** Controller typing into another application has been confirmed on the development PC. Windows system input and Steam Deck Gaming Mode are not implemented. There are no packaged releases yet; build from source using the instructions below.

## Platform support

| Platform | Status |
| --- | --- |
| KDE Plasma 6 on Linux / Wayland | Working prototype; primary development target |
| CachyOS / Arch-based KDE distributions | Installation instructions below; more device testing needed |
| Steam Deck Desktop Mode | Intended target; not yet tested on the device |
| Steam Deck Gaming Mode | Separate integration planned for investigation |
| Windows 11 | System-input backend planned; shared UI not yet validated on Windows |
| GNOME, other Wayland desktops, X11 | System-keyboard integration not supported yet |

## Install on CachyOS / Arch Linux with KDE

Use a **Plasma Wayland** session. Check with `echo "$XDG_SESSION_TYPE"`; it should print `wayland`.

### 1. Install dependencies

```bash
sudo pacman -Syu --needed base-devel git cmake pkgconf qt6-base qt6-wayland layer-shell-qt wayland sdl2-compat
```

Requirements: C++17, CMake 3.20+, Qt 6.8+ Widgets/Wayland Client/DBus, SDL2, Wayland protocol scanner, and LayerShellQt. No private Qt headers are required. Qt 6.8+ is needed for the protocol generator's `PRIVATE_CODE` option; see [Qt documentation](https://doc.qt.io/qt-6.8/qt-generate-wayland-protocol-client-sources.html). Arch supplies the overlay library through [layer-shell-qt](https://archlinux.org/packages/extra/x86_64/layer-shell-qt/).

Other distributions need equivalent development packages; their installation commands have not been verified yet.

### 2. Clone and build

```bash
git clone https://github.com/Dewopalis/daisywheel-reamade.git
cd daisywheel-reamade
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### 3. Test your controller locally

```bash
./build/daisywheel
```

This mode types **only into its own test field**. With an Xbox controller, hold the left stick upward and press A/B/X/Y: the field should receive `a/b/c/d`.

You can inspect controller detection with `./build/daisywheel --controllers`.
**Quit the test instance before enabling system mode**, so two instances do not process the same controller input.

### 4. Install and register with KDE

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

The installed executable is named `daisywheel-reamade`; the build output remains `daisywheel`. This stable install path means moving your source checkout will not break KDE's launcher. Runtime libraries must remain installed.

### 5. Enable and configure

1. Open **System Settings → Keyboard → Virtual Keyboard**, or search settings for “Virtual Keyboard.”
2. Select **Daisywheel REAmade** and click **Apply**.
3. Choose your preferred **Show Virtual Keyboard** policy: never automatically, touch/tablet input, or any input. Labels may vary by Plasma version.
4. Focus a text field in another application, press **Home/Guide**, and type with your controller.

KWin launches the selected keyboard. **No separate Autostart entry is needed.** The selection persists across sessions, so launch after logging into Plasma is expected; a full reboot test remains outstanding. This setup does not install a keyboard for the login screen or establish lock-screen support.

## Controls

Characters sit at the physical south/east/west/north face-button positions. SDL supplies controller mappings; colors currently follow Xbox conventions.

| Control | Action |
| --- | --- |
| Left stick | Select a petal; neutral does not enter a character |
| A / B / X / Y | Enter the bottom / right / left / top character in the selected petal |
| Left bumper | Toggle uppercase |
| Right bumper | Toggle letters and symbols |
| Left stick click | Space |
| Right stick click | Backspace |
| Start / Menu | Enter |
| Home / Guide | Show or hide the keyboard by default |
| Back / View | Also show or hide in KDE system mode |

The shortcut dropdown offers Home/Guide, Back/View and Start/Menu. A chosen shortcut takes precedence over that button's typing action. Steam or desktop bindings may also respond to Guide; select another shortcut if needed. Daisywheel does not suppress those other bindings.

## Opening and appearance

Automatic opening follows KDE's **Show Virtual Keyboard** policy through KWin's `willShowOnActive` API. The overlay starts hidden and picks up applied policy changes without restarting. Failed policy queries disable automatic opening.

Controller opening works regardless of the automatic policy and keeps the overlay open until dismissed. Hiding it suppresses automatic reopening in the same input context; leaving that context restores automatic behavior. Opening without a text field shows the wheel, but no text is inserted until an input context is available.

| Setting | Current behavior |
| --- | --- |
| Opacity | 30–100%, affects panel and characters together; saved between launches |
| Controller shortcut | Saved between launches |
| KDE system-mode position | Bottom right of the active display; free placement not implemented |
| Drag handle / pin toggle | Available in local test mode; disabled in KDE system mode |
| Hide window | Keeps controller handling running |
| Quit | Stops the process; controller reopening requires relaunch |

KWin's `VirtualKeyboard.visible` property describes its native input panel, not this independent overlay; it is not a reliable visibility check for Daisywheel.

## Updating and removing

First select **None** in KDE Virtual Keyboard settings and apply to stop the keyboard. From the source checkout:

```bash
git pull --ff-only
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
install -Dm755 build/daisywheel "$HOME/.local/bin/daisywheel-reamade"
```

Select Daisywheel REAmade again and apply. Rebuild after dependency updates if the binary stops starting.

To disable it, select None or your previous keyboard and apply. To remove the user installation after disabling:

```bash
rm -- "$HOME/.local/share/applications/org.daisywheel.keyboard.desktop"
rm -- "$HOME/.local/bin/daisywheel-reamade"
kbuildsycoca6 --noincremental
```

## Where we are now

Confirmed on the development PC:

- Xbox Series X controller detected and mapped through SDL.
- Petal selection and typing in the local test field.
- Controller shortcuts showing and hiding the keyboard.
- Controller-driven typing into another application on KDE Wayland.
- Overlay rendering without acquiring keyboard focus; opacity control implemented.
- Registration in KDE Virtual Keyboard settings and launch through KWin.
- Automated checks for stick selection, character slots and automatic/manual visibility rules.

Implemented, with more live validation needed:

- KDE-controlled automatic opening, especially on real touch/tablet hardware.
- Backspace and Enter across applications and toolkits.
- Reboot startup, controller reconnection and multiple-display behavior.

Current limits include a provisional English layout, compatibility dependent on a usable application input context, and controller events potentially reaching the target app as well as the wheel. There is no prediction, repeat-on-hold, configurable deadzone or full controller remapping yet. Windows system input, installers and Gaming Mode support remain planned.

## Roadmap

- [ ] Configurable pinned placement in KDE system mode, with saved anchors and offsets.
- [ ] Adjustable size and a compact handheld layout.
- [ ] Validate touch/tablet opening and all KDE opening policies on hardware.
- [ ] Validate reboot startup, reconnects and multiple displays.
- [ ] Test browsers, editors and XWayland applications.
- [ ] Configurable controller mappings, deadzones and repeat behavior.
- [ ] Additional languages, layouts and controller-specific button labels.
- [ ] Windows 11 input backend that preserves focus and delivers text/control keys.
- [ ] Windows automatic/manual activation and startup configuration.
- [ ] Linux and Windows packages, installers and reproducible builds.
- [ ] Testing on CachyOS handhelds, Steam Deck Desktop Mode and ROG Ally.
- [ ] Investigate Steam Deck Gaming Mode separately.

Windows needs its own activation and input backend. Microsoft's IME API does not provide a general independent replacement touch keyboard; see [Microsoft's IME documentation](https://learn.microsoft.com/en-gb/windows/apps/design/input/input-method-editors).

## Troubleshooting

**No controller:** run `./build/daisywheel --controllers`. Check Linux device detection and session access. Sandboxes may hide input devices even when the host detects them.

**Only types in the test field:** quit local test mode and select the keyboard in KDE settings for system input.

**Doesn't open automatically:** apply your KDE policy, verify a Wayland session, and try Home/Guide with a text field focused. Touch/tablet mode should not open for ordinary mouse input.

**Guide also opens Steam:** choose another shortcut.

**Stopped responding after Quit:** select None and apply, then select Daisywheel REAmade and apply again so KWin launches it.

**Missing from settings:** verify the desktop entry's executable path and `X-KDE-Wayland-VirtualKeyboard=true`, then run `kbuildsycoca6 --noincremental`.

## Implementation and references

Qt Widgets renders the UI and SDL2 handles controllers. KDE system mode uses input-method-v1 for text delivery and a LayerShellQt overlay for display without taking keyboard focus. Normal operation does not retain entered text or put it in diagnostic logs.

This is an original implementation inspired by [DaisyWheelJS](https://github.com/likethemammal/daisywheeljs), [AnyWheel](https://github.com/EvilJagaGenius/AnyWheel) and [KDE Plasma Keyboard](https://github.com/KDE/plasma-keyboard).

Related documentation: [KDE Virtual Keyboard](https://docs.kde.org/stable_kf6/en/kwin/kcontrol/kwinvirtualkeyboard/index.html) and [Windows SendInput](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-sendinput).
