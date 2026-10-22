# Architecture overview

Daisywheel separates its controller-driven wheel from the platform-specific
work of displaying an overlay and entering text into another application.

## Shared interface and input

Qt Widgets renders the settings and wheel interface. SDL2 discovers controllers
and provides mapped buttons, sticks, triggers and rumble. Shared wheel logic
turns stick direction and face-button presses into characters and actions.

The local test mode sends text only to its own visible test field. System mode
uses a platform backend so text goes to the active input context or foreground
application.

## Platform integration

On KDE Plasma Wayland, Daisywheel registers as a virtual keyboard. KWin's input
method protocol supplies the active text context, while LayerShellQt displays
the overlay without taking keyboard focus. KDE's virtual-keyboard policy
controls automatic opening. Controller mouse mode requests pointer access
through the desktop RemoteDesktop portal.

On Windows, system mode sends key input with `SendInput`, uses a nonactivating
overlay, and can use UI Automation metadata to detect editable fields. It does
not read field contents. See [Windows input and focus implementation notes](windows-implementation-notes.md)
for implementation details and the [Windows development guide](windows-build-and-testing.md)
for build and test instructions.

The macOS bundle is experimental. The project can build and smoke-test it in
CI, but physical Mac testing is still required.

## Data and privacy

Daisywheel does not retain entered text or include it in diagnostic logs.
Platform integrations inspect focus and editability metadata only where needed
to deliver input or detect a text field.

## Prior art and references

This is an original implementation informed by [DaisyWheelJS](https://github.com/likethemammal/daisywheeljs),
[AnyWheel](https://github.com/EvilJagaGenius/AnyWheel) and
[KDE Plasma Keyboard](https://github.com/KDE/plasma-keyboard).

Related documentation: [KDE Virtual Keyboard](https://docs.kde.org/stable_kf6/en/kwin/kcontrol/kwinvirtualkeyboard/index.html),
[Microsoft SendInput](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-sendinput),
and [Microsoft UI Automation threading](https://learn.microsoft.com/windows/win32/winauto/uiauto-threading).
