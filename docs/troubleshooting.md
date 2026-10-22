# Troubleshooting

## Linux and KDE Plasma

**No controller:** run `./build/daisywheel --controllers`. Check Linux device
detection and session access. Sandboxes may hide input devices even when the
host detects them.

**Text only appears in the test field:** quit local test mode, select Daisywheel
REAmade under **System Settings → Keyboard → Virtual Keyboard**, and click
**Apply** to enable system input.

**Does not open automatically:** check the KDE **Show Virtual Keyboard**
policy, confirm the session is Wayland, and focus a text field. Touch/tablet
policy should not open the keyboard for ordinary mouse input. The configured
controller shortcut remains available for manual opening.

**Guide also opens Steam:** choose another controller shortcut in Settings.
Steam or desktop bindings may still respond to Guide; Daisywheel does not
suppress them.

**Stopped responding after Quit:** select **None** and click **Apply**, then
select Daisywheel REAmade again and apply so KWin launches it.

**Missing from Virtual Keyboard settings:** verify the desktop entry's
executable path and `X-KDE-Wayland-VirtualKeyboard=true`, then run
`kbuildsycoca6 --noincremental`.

**Pointer does not move:** hold LT while the wheel is visible. On KDE, grant
RemoteDesktop pointer permission when prompted. If permission was denied, check
the desktop portal settings and try again.

## Windows

**Does not start on sign-in:** check **Settings → Apps → Startup** or **Task
Manager → Startup apps** and enable Daisywheel. Keep the registered executable
path and companion DLLs together. See [Microsoft's startup-app instructions](https://support.microsoft.com/en-gb/windows/experience/startup-boot/configure-startup-applications-in-windows).

**Text is not entered in another app:** focus an editable text field and launch
Daisywheel in system mode. Input may be blocked when the target application
runs at a higher privilege level. Secure desktops and login screens are not
supported.

**Automatic opening does not detect a field:** Windows UI Automation requires
the target app to expose suitable accessibility information. Open the wheel
manually as a fallback and [report the application and field type](https://github.com/Dewopalis/daisywheel-reamade/issues/new?template=compatibility_report.md).

## More help

- [Installation](installation.md)
- [Controller controls and settings](controls.md)
- [Compatibility matrix](compatibility.md)
- [Windows build and testing](windows-build-and-testing.md)
