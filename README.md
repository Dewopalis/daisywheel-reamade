# Daisywheel REAmade

Controller-first text entry for Windows handhelds and KDE Plasma/Wayland desktops,
including CachyOS. Initial native prototype: Qt 6 Widgets and SDL2, C++17.

## Build

Install Qt 6 development files, SDL2 development files, CMake and a C++ compiler.
On Windows, provide Qt and SDL2 CMake package paths through CMAKE_PREFIX_PATH.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/daisywheel
```

Default mode types into its own test field. Linux also provides an experimental
KDE system-keyboard mode using input-method-v1.
Eight petals hold four characters each. Left stick selects a petal; physical
south/east/west/north buttons select the matching colored character. SDL normalizes
controller mappings; displayed colors currently follow Xbox conventions.

LB toggles uppercase, RB toggles symbols, left stick click inserts space,
right stick click deletes, Start inserts a newline, Back hides/reopens the wheel.
Home/Guide toggles the entire window while the process is running, including when
another app has focus. Select Back/View or Start/Menu as a fallback if Steam or
the desktop intercepts Guide. A selected shortcut takes precedence over typing
actions on that button. The shortcut does not launch a stopped process or suppress
the button in other apps. Quit exits; Hide keeps controller polling active.

Drag the top handle to move the frameless overlay. Pin position locks that handle;
the compositor may still move the window during monitor or workspace changes.
The opacity slider affects the panel and text together (30–100%). Pin, opacity
and shortcut preferences persist in Qt's user settings. Exact screen coordinates
are not restored on Wayland; placement is owned by the compositor.
Touching the local test field reopens the wheel; this requires hardware validation.
There is no repeat, prediction or physical-keyboard global shortcut.
Layout is provisional, not verified against Steam.

## KDE system keyboard (experimental)

Linux builds need Qt Wayland Client and Qt DBus development files, LayerShellQt,
Wayland development files and the Qt Wayland protocol scanner. No private Qt
headers are required.

A user-local `org.daisywheel.keyboard.desktop` entry registers the build in KDE's
Virtual Keyboard settings with `X-KDE-Wayland-VirtualKeyboard=true` and
`Exec=/absolute/path/to/daisywheel --kde-input-method`.
Select Daisywheel there to let KWin launch it as the input method.

Text is committed to the active input-method context, never cached for later
delivery. No active context means no insertion. A LayerShellQt overlay displays
the wheel without acquiring keyboard focus. The initial input-panel display path
remained hidden, so it was replaced. Both overlay rendering and text commit to a
separate Qt application passed live checks.
Controller delivery into other apps, control keys, touch activation and broader
application coverage still need live testing.
The overlay currently stays at bottom right on the active output; dragging/pinning
are disabled in this mode pending configurable anchors and margins. Opacity
remains available. Home/Guide and Back toggle the overlay even without a text
field. Showing it requests KWin's `forceActivate` method to enable typing into
a mouse-focused field. KWin's VirtualKeyboard.visible property describes its
input panel and does not represent this independently displayed overlay.

Automatic opening follows KDE's **Show Virtual Keyboard** setting, queried live
through KWin's `willShowOnActive` API. Touch/tablet mode only opens automatically
when KWin permits it; automatic opening is disabled by KDE's Never mode. The
overlay starts hidden. Changes in System Settings are reflected without restarting
Daisywheel once applied. Policy query failures disable automatic opening.
Home/Back remains a manual override and keeps the wheel open until dismissed,
including when automatic opening is disabled. Manual dismissal prevents automatic
reopening in the same input context; ending that context restores automatic mode.

Close standalone test instances before using system mode, to avoid duplicate
controller handling. Change Virtual Keyboard back to None or your prior keyboard
to stop the experiment. System mode is Linux-only; Windows backend remains planned.

## Integration milestones

1. Verify controller ergonomics, deadzone, disconnect behavior and character layout.
2. KDE backend: implement compositor-supported input-method protocol and keyboard
   surface lifecycle, with a desktop entry for KDE's virtual-keyboard selector.
   Touch activation should be driven by compositor text-input activation; verify
   the target KWin protocol and application compatibility before choosing bindings.
3. Windows backend: nonactivating overlay, editable-focus detection through UI
   Automation and touch-trigger detection, Unicode and control-key delivery via
   SendInput. Check insertion results and privilege limitations. Windows does not
   expose a general third-party replacement touch keyboard through its IME API.
4. Package and test on CachyOS/KDE Wayland and Windows 11 handheld hardware.
5. Investigate Steam Deck Gaming Mode separately from Plasma Desktop Mode.

The shared selection engine must not know about native windows or input injection.
Backends should expose activation/deactivation, commit text and send control keys.
Do not store entered text in logs. Keep global controller handling scoped to the
active wheel; assess whether the target application also receives controller events.

## References

- https://github.com/likethemammal/daisywheeljs — MIT web implementation.
- https://github.com/EvilJagaGenius/AnyWheel — historical Python controller keyboard.
- https://docs.kde.org/stable_kf6/en/kwin/kcontrol/kwinvirtualkeyboard/index.html
- https://learn.microsoft.com/en-gb/windows/apps/design/input/input-method-editors
- https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-sendinput

This prototype is an original implementation; reference source was not copied.
