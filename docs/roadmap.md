# Roadmap

This is a working plan, not a release schedule. Community compatibility reports
help determine what should be tested next.

## Completed

- Controller typing, manual show/hide shortcuts and text delivery to other applications on Windows and KDE Plasma / Wayland.
- KDE Virtual Keyboard integration, including KDE-controlled automatic opening.
- Windows UI Automation text-field detection and optional sign-in startup.
- Overlay movement, pinning, resizing and adjustable transparency.
- Handheld and living-room controller profiles, pointer speed, mouse controls, repeat Backspace and haptic feedback (v0.3.0).
- Windows and Linux packages, including the guided Linux AppImage installer (v0.3.0).
- Timeouts and cancellation for KDE pointer requests, coalesced pointer movement, and held mouse-button cleanup (v0.3.1).
- Consolidated settings, configurable controller shortcut and inverted-scroll option (v0.4.0).
- Optional MIDI-synthesized petal and button sounds with volume and previews (v0.4.0).
- Controller-navigable settings, living-room default profile, configurable stick deadzones, cursor editing with selection and word navigation, and compact settings layout (v0.5.0).

## Planned and under investigation

- Validate touch/tablet opening and KDE keyboard policies on supported hardware.
- Test sign-in/reboot startup, controller reconnection and multiple displays on target systems.
- Broaden application and controller compatibility on Windows and Linux, including browsers and XWayland applications.
- Validate compact layouts and controller labels on handheld screens.
- Test controller-only interaction with KDE's external portal permission prompts.
- Improve mouse-mode recovery from sleep/resume and portal failures without requiring an LT cycle.
- Test published packages on Steam Deck Desktop Mode and ROG Ally.
- Investigate Steam Deck Gaming Mode separately.
- Add more languages, layouts and controller-specific button labels.

Windows uses its own activation and input backend. Microsoft's IME API does
not provide a general independent replacement touch keyboard; see
[Microsoft's IME documentation](https://learn.microsoft.com/en-gb/windows/apps/design/input/input-method-editors).

## Suggest a priority

Use [GitHub Discussions or issues](https://github.com/Dewopalis/daisywheel-reamade/issues)
to share use cases and compatibility results before starting larger features.
