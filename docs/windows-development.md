# Windows development — 14 October 2026

Branch: `windows-integration`.

The README roadmap guides this branch: Windows text/control-key delivery, a nonactivating overlay, activation and startup configuration. Packaging, broader application validation and handheld testing remain separate work.

## Implemented

- Explicit `--windows-input` mode; default local typing and KDE mode remain available.
- UTF-16 Unicode input through `SendInput`, plus Space, Backspace and Enter.
- Foreground-process checks, modifier checks and failure reporting without recording user text.
- Nonactivating topmost overlay, including mouse activation handling.
- Controller shortcut and Back/View opening, compact wheel, tray show/hide and Quit.
- Saved opt-in automatic opening through Windows UI Automation, with direct support for standard native EDIT controls; current-user Windows startup registration.
- COM queries on a dedicated MTA worker with provider timeouts, cached metadata and approximately 200 ms polling; no text-content queries.
- Separate field identities for dismissal/reopening, a tray automatic-opening toggle and placement on the foreground application's display.
- Single Windows system-mode instance.
- Project-local build script and runtime deployment.

## Validation

Toolchain: Qt 6.8.3 MinGW 64-bit, MinGW 13.1.0, SDL2 2.32.10, CMake 4.4.4 and Ninja 1.13.2. Dependencies are in ignored `work/`; deployable build output is in ignored `build-windows/`.

Passed:

- Release compilation and linking.
- CTest wheel-selection, character slots and shared visibility rules.
- Separate-process native Windows input test: Unicode, a surrogate pair, Backspace, Enter, self-process rejection, show/click activation policy and preserved foreground.
- `--windows-input --smoke-test` and local `--smoke-test`.
- SDL detected one mapped, accessible Xbox One Controller.
- User confirmed controller-driven typing into another application on Windows.
- Separate-process focus tests: native EDIT, Qt QLineEdit, two fields in the same window, read-only QLineEdit, a non-text button, editable/read-only QPlainTextEdit and a password field.
- Dismissal remains suppressed for the current field and resets when selecting a different field.

The first compact Windows wheel used fixed-size petals, causing overlap. Petal circles, character positions and font sizes now scale together with the available wheel area. `--render-wheel-preview <png-path>` renders the compact wheel for visual inspection without starting controller handling.

The automated checks verify synthesized input, field classification, focus identity and launch behavior; the user also confirmed the controller-to-app typing flow. Full automatic overlay interactions, startup registration and tray controls still need live user testing. Browser fields and reboot startup have not been tested. Linux was not rebuilt on this Windows host. Interactive tests click only their own verified fixture windows to obtain foreground focus.

## Known compatibility finding

A separate Qt `QPlainTextEdit` target reordered a single burst containing ASCII, an accented character and an emoji: the emoji appeared before the preceding text. The same sequence passed against the native Windows EDIT control. This is unresolved. Current wheel character actions send one ASCII character per button press; multi-character Unicode features need explicit application compatibility tests.

## Next validation

1. Focus Notepad, open with the controller and type; check uppercase, symbols, Space, Backspace, Enter and hide/reopen.
2. Exercise tray controls, controller reconnects, duplicate launches and Windows sign-in startup.
3. Check complete automatic overlay opening/hiding and tray settings, including dismissal and switching fields. Test browser forms, contenteditable regions and other accessibility providers; manual opening remains a fallback.
4. Test monitor switching, scaling and ROG Ally/Steam Deck-sized screens. Both automatic and manual placement now use the foreground application's monitor, with a pointer-display fallback.
5. Investigate the Qt Unicode burst issue and validate slow/unavailable accessibility providers.

UI Automation uses only process, focus, control-type and editability metadata plus opaque field identities. It does not read field values or names. The worker follows [Microsoft's UI Automation threading guidance](https://learn.microsoft.com/windows/win32/winauto/uiauto-threading).

`SendInput` is subject to Windows integrity restrictions; elevated targets may block delivery. See [Microsoft SendInput documentation](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-sendinput). Mouse activation behavior follows [WM_MOUSEACTIVATE](https://learn.microsoft.com/windows/win32/inputdev/wm-mouseactivate).
