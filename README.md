# Daisywheel REAmade

**Controller-first typing for handheld computers and desktop PCs.**

Inspired by Steam's original Daisywheel, REAmade lets you select a petal with the left stick and enter its characters with the four face buttons. Open it with your controller on any display, or let KDE's automatic-opening policy decide when it appears.

The goal is a keyboard for Windows handhelds such as the ROG Ally and Linux systems running KDE Plasma, including CachyOS and Steam Deck Desktop Mode.

![Daisywheel REAmade typing Hello World into KWrite using an Xbox controller](docs/media/hello-world.gif)

*Controller typing into KWrite on KDE Plasma / Wayland.*

**Current status: Linux/KDE Wayland and Windows prototypes.** Controller typing into another application has been confirmed on both development PCs. The v0.1.0 Windows and Linux packages are published on GitHub Releases. Windows sign-in startup, broader application compatibility, handheld hardware and Steam Deck Gaming Mode still need validation.

## Open source and contributing

Daisywheel REAmade is developed in the open. The project license and its terms
are in [LICENSE](LICENSE) ([GPL-3.0-only](https://spdx.org/licenses/GPL-3.0-only.html)).
In plain language, people may use, change and redistribute the project, including
commercially. If they distribute a modified version covered by this license,
they must keep it under GPL-3.0 and provide its corresponding source under the
license terms. Private changes do not need to be published. The full license
text controls. Contributions are welcome; see
[CONTRIBUTING.md](CONTRIBUTING.md) for how to build, report issues and propose
changes. You can also use the GitHub issue and pull request templates.

## Download builds

The [Build packages workflow](https://github.com/Dewopalis/daisywheel-reamade/actions/workflows/packages.yml)
prepares a Windows setup EXE and portable ZIP, a Debian DEB, a Fedora RPM, and a
Linux AppImage.

To download a recent development build, sign in to GitHub, open the workflow
link above, select a successful **Build packages** run, then download the bundle
you need under **Artifacts** at the bottom of the run summary:

- `windows-x64`: setup `.exe` and portable `.zip`
- `linux-debian-appimage`: `.deb` and `.AppImage`
- `linux-fedora-rpm`: `.rpm`

Extract the downloaded artifact ZIP to get the installer or package file inside.
Artifacts are tied to that workflow run and expire after 14 days. They are useful
for trying recent changes; they have not necessarily been manually validated on
handheld hardware.

For reviewed, persistent downloads, visit [GitHub Releases](https://github.com/Dewopalis/daisywheel-reamade/releases).
The first release, [v0.1.0](https://github.com/Dewopalis/daisywheel-reamade/releases/tag/v0.1.0),
is published with the Windows installer and portable ZIP, Debian DEB, Fedora
RPM, Linux AppImage and SHA-256 checksums. Future version tags build packages
and prepare a draft release for review before publication.

See [installation and release packaging](docs/releases/packaging.md) for supported
platform baselines, installing downloads and creating a release. The source-build
instructions below remain available for developers.

## Platform support

| Platform | Status |
| --- | --- |
| KDE Plasma 6 on Linux / Wayland | Working prototype; primary development target |
| CachyOS / Arch-based KDE distributions | Installation instructions below; more device testing needed |
| Steam Deck Desktop Mode | Intended target; not yet tested on the device |
| Steam Deck Gaming Mode | Separate integration planned for investigation |
| Windows 11 | Controller typing confirmed; UI Automation field detection tested with native and Qt controls on `windows-integration` |
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

## Run on Windows

### Launch the keyboard

From the repository folder in PowerShell:

```powershell
Start-Process -FilePath '.\build-windows\daisywheel.exe' -ArgumentList '--windows-input'
```

For the current development checkout, you can also run this from any folder:

```powershell
Start-Process -FilePath 'C:\Development\daisywheel-reamade\build-windows\daisywheel.exe' -ArgumentList '--windows-input'
```

Keep the entire `build-windows` directory together: the executable needs its DLLs and plugin folders. Double-clicking the EXE without arguments opens **local test mode**, which types into its own field. For an Explorer/desktop shortcut, use this Target (adjust the path for your installation):

```text
"C:\Development\daisywheel-reamade\build-windows\daisywheel.exe" --windows-input
```

System mode initially runs in the notification area near the clock. Look under the hidden-icons arrow if needed. Press Home/Guide or Back/View, or right-click the Daisywheel tray icon and choose **Show / hide keyboard**. Focus a text field in another app and type with your controller. A chosen shortcut takes precedence over that button's typing action. Only one Windows system-mode instance can run at a time.

### Open automatically when selecting a text field

1. Launch Daisywheel in Windows system mode.
2. Open the wheel and enable **Open automatically when a text field is selected**. You can also enable **Open automatically for text fields** from the tray menu.
3. Click, tap, or tab into an editable text field in another application. The overlay appears without deliberately taking keyboard focus.
4. Move focus to a non-text control or read-only field to hide an automatically opened overlay.

The option is saved between launches. Detection uses Windows UI Automation and direct support for standard native text controls. Native text boxes, Qt text boxes/document editors and password fields have been tested. Browsers and other custom controls can work when they expose suitable accessibility information, but browser compatibility has not yet been validated. Detection checks field metadata and editability; it does not read field contents.

Hiding the overlay suppresses automatic reopening for the same field. Selecting a different field or leaving and returning to text entry restores automatic opening. Opening manually keeps the overlay open until you dismiss it. Controller and tray opening remain available if automatic detection fails. The overlay opens at the bottom right of the foreground application's display, with a pointer-display fallback.

Daisywheel runs as a separate app. It does not register as a replacement touch-keyboard option in Windows Settings or disable Microsoft's keyboard. If both appear, adjust the built-in touch keyboard's automatic-opening preference separately.

### Start automatically when signing in

1. Launch with `--windows-input` and open the wheel.
2. Enable **Start Daisywheel when I sign in to Windows**.
3. Enable the automatic text-field option too if you want the wheel to appear as you select text fields after sign-in.
4. Keep the executable and its companion DLLs at the same location.

This registers the current executable with `--windows-input` for **your Windows account**. Administrator rights are not required. The saved automatic-opening preference is restored on launch. Windows may delay startup slightly; this is sign-in startup, not login-screen keyboard support. A full sign-out/reboot test is still outstanding.

If Windows has disabled the app, check **Settings → Apps → Startup** or **Task Manager → Startup apps** for Daisywheel/DaisywheelREAmade and enable it. See [Microsoft's startup-app instructions](https://support.microsoft.com/en-gb/windows/experience/startup-boot/configure-startup-applications-in-windows).

To remove startup registration, uncheck **Start Daisywheel when I sign in to Windows**. Choosing **Quit** stops the current instance but keeps startup registration and preferences. Turning off automatic opening keeps manual opening available. If you move the app, launch it from the new location and toggle the startup checkbox off and on to update its path.

### Update or remove

Choose **Quit** from the tray before replacing the executable; a running instance locks the EXE on Windows. For installation from a package, use the [Windows setup installer](docs/releases/packaging.md#install-downloaded-packages). Before removing the application folder, uncheck its startup option, then quit. For updating a source build, rebuild and launch again with `--windows-input`.

### Build from source

Use a matching **Qt 6 / MinGW 64-bit** pair, CMake 3.20+, Ninja and the SDL2 MinGW 64-bit development package. The branch includes `scripts/build-windows.ps1`; it configures, builds, runs the shared wheel tests and deploys runtime DLLs alongside the executables. Its defaults use dependencies in `work/`; supply `-QtRoot`, `-CompilerRoot`, `-SDLRoot`, `-CMake` and `-Ninja` for other installations.

```powershell
& .\scripts\build-windows.ps1
Start-Process -FilePath '.\build-windows\daisywheel.exe' -ArgumentList '--controllers' -Wait -RedirectStandardOutput '.\build-windows\controllers.txt'
Get-Content '.\build-windows\controllers.txt'
Start-Process -FilePath '.\build-windows\daisywheel.exe' -ArgumentList '--windows-input'
```

Characters, Space, Backspace and Enter go to the current foreground application. Release physical Shift/Ctrl/Alt/Windows keys before typing. Windows can block input to applications running at a higher privilege level. Secure desktops and login screens are unsupported.

Run the interactive tests on an unlocked Windows desktop:

```powershell
.\build-windows\windows-input-tests.exe
.\build-windows\windows-focus-tests.exe
```

The input test uses a separate native Windows text control to check Unicode including a surrogate pair, Backspace, Enter, self-process rejection and overlay focus preservation. The focus test checks native/Qt editable fields, same-window field identities, read-only fields, buttons, document editors and password fields. Both temporarily focus their own verified fixture windows, including a synthetic mouse click; they do not read user text. Reconnects, browsers, elevated applications and handheld displays require additional testing. A Qt text-field probe reordered a mixed Unicode/emoji string injected as one burst; this remains a compatibility issue before adding multi-character Unicode layouts or prediction.

Validated on the Windows development PC: Release build, shared wheel/visibility tests, isolated native input and UI Automation focus tests, local/system-mode smoke tests, and detection of an accessible mapped Xbox controller. The user also confirmed controller-driven typing into another application. See [Windows development notes](docs/windows-development.md).

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
| Back / View | Also show or hide in Windows/KDE system mode |

The shortcut dropdown offers Home/Guide, Back/View and Start/Menu. A chosen shortcut takes precedence over that button's typing action. Steam or desktop bindings may also respond to Guide; select another shortcut if needed. Daisywheel does not suppress those other bindings.

Enable **Minimal mode (wheel only)** to hide the settings and help panel and shrink the window for handheld screens. A small **≡** button in the upper-right restores the full controls. You can also hold the selected controller shortcut for about a second while the wheel is visible.

## Opening and appearance

In KDE mode, automatic opening follows KDE's **Show Virtual Keyboard** policy through KWin's `willShowOnActive` API. The overlay starts hidden and picks up applied policy changes without restarting. Failed policy queries disable automatic opening. Windows uses its saved Daisywheel automatic-opening option described above.

Controller opening works regardless of the automatic policy and keeps the overlay open until dismissed. Hiding it suppresses automatic reopening in the same input context; leaving that context restores automatic behavior. KDE only commits into an active input context. Windows sends keystrokes to the foreground application; focus a text field first.

| Setting | Current behavior |
| --- | --- |
| Opacity | 30–100%, affects panel and characters together; saved between launches |
| Controller shortcut | Saved between launches |
| Minimal mode | Shows the wheel at a compact size; use the upper-right **≡** button or hold the selected controller shortcut to restore the controls |
| KDE system-mode position | Drag the header or corner and release to apply; position and size are saved between launches |
| Drag handle / pin toggle | Drag the header to move and enable **Pin position** to lock it |
| Hide window | Keeps controller handling running |
| Quit | Stops the process; controller reopening requires relaunch |

KWin's `VirtualKeyboard.visible` property describes its native input panel, not this independent overlay; it is not a reliable visibility check for Daisywheel.

Unpin the wheel, drag the header to move it, then pin it if you want to lock the
position. Drag the small diagonal grip in the top-left corner to resize. KDE
applies layer-shell movement and resizing when the drag is released so the
compositor does not fight intermediate updates. Windows and KDE system mode save
position and size between launches. Windows keeps the
position on its monitor and clamps it to the available desktop area after size
or display changes.

## Updating and removing on KDE

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
- Resizable window with a corner drag grip and a smaller default wheel footprint.
- Registration in KDE Virtual Keyboard settings and launch through KWin.
- Automated checks for stick selection, character slots and automatic/manual visibility rules.
- v0.1.0 Windows setup/portable packages, Debian DEB, Fedora RPM and Linux AppImage built by CI; package installation or startup checks passed in the release workflow.

Implemented, with more live validation needed:

- KDE-controlled automatic opening, especially on real touch/tablet hardware.
- Backspace and Enter across applications and toolkits.
- Reboot startup, controller reconnection and multiple-display behavior.

Current limits include a provisional English layout, compatibility dependent on a usable application input context, and controller events potentially reaching the target app as well as the wheel. There is no prediction, repeat-on-hold, configurable deadzone or full controller remapping yet. Windows automatic-opening compatibility still needs testing across more applications; Steam Deck Gaming Mode remains a separate investigation.

## Roadmap

- [x] Move, pin and resize the KDE system overlay with saved position and size.
- [x] Resize the application by dragging its corner grip; compact handheld tuning remains planned.
- [x] Wheel-only Minimal mode with a smaller window footprint.
- [ ] Further compact-layout tuning for handheld screens.
- [ ] Validate touch/tablet opening and all KDE opening policies on hardware.
- [ ] Validate reboot startup, reconnects and multiple displays.
- [ ] Test browsers, editors and XWayland applications.
- [ ] Configurable controller mappings, deadzones and repeat behavior.
- [ ] Additional languages, layouts and controller-specific button labels.
- [ ] Validate the Windows input backend across more applications and controller hardware; text/control delivery and focus preservation are implemented.
- [ ] Validate Windows UI Automation automatic opening in browsers and more applications; native/Qt detection, manual activation and opt-in startup are implemented.
- [x] Automated Linux and Windows package builds, package checks and draft-release creation.
- [x] Review and publish the v0.1.0 release.
- [ ] Test the published packages on supported handheld hardware, including Steam Deck Desktop Mode and ROG Ally.
- [ ] Investigate Steam Deck Gaming Mode separately.

Windows uses its own activation and input backend. Microsoft's IME API does not provide a general independent replacement touch keyboard; see [Microsoft's IME documentation](https://learn.microsoft.com/en-gb/windows/apps/design/input/input-method-editors).

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
