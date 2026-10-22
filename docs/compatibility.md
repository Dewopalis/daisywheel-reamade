# Compatibility

Compatibility depends on the operating system, desktop session, controller,
and target application. The matrix distinguishes local hardware testing from
CI-only checks.

| Operating system / desktop | Controller | Application or scenario | Result |
| --- | --- | --- | --- |
| Windows 11 | Xbox Series X controller | Native and Qt text controls; controller typing in another app | Tested on the development PC |
| CachyOS / Arch Linux, KDE Plasma 6 on Wayland | Xbox Series X controller | KDE text-focus popup; typing into KWrite; controller settings and mouse mode | Tested on the development PC |
| macOS 15 (GitHub Actions runner) | Not tested | Build, package and app-launch smoke test | CI-tested only; physical Mac testing needed |
| Steam Deck, ROG Ally and other handhelds | Not tested | Not yet tested on device | Intended targets, no device results yet |

These results do not establish compatibility with every application or
controller. In particular, browser and XWayland field detection on Linux,
Steam Deck Gaming Mode, and GNOME or X11 system-keyboard integration remain
unverified or unsupported. The Windows test coverage includes native and Qt
controls, but broader browser and handheld testing is still needed.

## Submit a test report

Please [open a compatibility report](https://github.com/Dewopalis/daisywheel-reamade/issues/new?template=compatibility_report.md)
with the Daisywheel version, OS and desktop version, Wayland/X11 session,
controller and connection type, target application, and what you tried. The
template includes a checklist to make results easier to compare. Please do not
include private text entered into other applications or unrelated logs.

The `compatibility` issue label is applied automatically to these reports.

## Package and platform notes

- KDE Plasma on Wayland is the supported Linux system-keyboard target.
- Windows uses UI Automation for automatic field detection. Targets must expose
  suitable accessibility metadata. Elevated apps, secure desktops and login
  screens are unsupported.
- The macOS app bundle is experimental and not notarized. CI verifies building,
  packaging and launch, but not controller input or desktop integration on a
  physical Mac.
- Touchscreen behavior, reboot/startup behavior on target devices, controller
  reconnects, multiple displays and handheld layouts need more community
  testing.

See [troubleshooting](troubleshooting.md) for known setup and input issues.
