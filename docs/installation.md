# Installation

For recent tested packages, choose a file from the [latest GitHub release](https://github.com/Dewopalis/daisywheel-reamade/releases/latest).
Development builds are available through [GitHub Actions](development.md#development-builds).

## Windows

Run the setup `.exe` and follow the installer. Launch Daisywheel REAmade from
the Start menu. To update, run a newer setup package. To uninstall, use
**Settings → Apps → Installed apps**.

The portable `.zip` does not use an installer. Extract the whole archive and
keep its files together. Launch the included executable. To update, replace
the extracted folder; to remove, delete it.

The installed app runs in the notification area. Look under the hidden-icons
arrow if needed. Use the configured controller shortcut or choose
**Show / hide keyboard** from the tray menu. Focus a text field in another app
before typing. Only one system-mode instance can run at a time.

### Open automatically on text focus

1. Launch Daisywheel in Windows system mode.
2. Open the wheel and enable **Open automatically when a text field is selected**,
   or enable **Open automatically for text fields** from the tray menu.
3. Click, tap or tab into an editable field to show the overlay. Move focus to a
   non-text control or read-only field to hide it.

The setting is saved between launches. Detection uses Windows UI Automation and
direct support for standard native text controls. Native and Qt text controls,
document editors and password fields have been tested. Browsers and other
custom controls need to expose suitable accessibility information and have not
been fully validated. Daisywheel checks field metadata and editability; it does
not read field contents. Hiding the overlay suppresses automatic reopening for
the same field. Selecting another field or leaving and returning to text entry
restores automatic opening. Manual controller and tray opening remain
available if detection fails.

Daisywheel runs as a separate app. It does not register as a replacement
touch-keyboard option in Windows Settings or disable Microsoft's keyboard. If
both appear, adjust the built-in touch keyboard's automatic-opening preference
separately.

### Start with Windows and remove the app

In the wheel settings, enable **Start Daisywheel when I sign in to Windows**.
This registers the current executable with `--windows-input` for your account;
administrator rights are not required. Keep the executable and companion DLLs
at the same location. Windows may delay startup slightly. This is sign-in
startup, not login-screen support. A full reboot test is still outstanding.

To stop automatic startup, uncheck **Start Daisywheel when I sign in to
Windows**. Choosing **Quit** stops the current instance but keeps startup
registration and preferences. If you move the app, launch it from the new
location and toggle the startup checkbox off and on to update its path.

For the setup package, run a newer installer to update or uninstall Daisywheel
from **Settings → Apps → Installed apps**. For the portable ZIP, replace the
whole extracted folder to update or delete it to remove the app. Before
uninstalling either version, disable sign-in startup and quit the running
instance. A running Windows process locks the executable while updating.

## Debian, Ubuntu and Fedora

Open the downloaded `.deb` or `.rpm` in your desktop's software installer and
choose **Install**. KDE users must then open **System Settings → Keyboard →
Virtual Keyboard**, select **Daisywheel REAmade**, and click **Apply**. Choose
the **Show Virtual Keyboard** policy that suits your setup.

## Arch, CachyOS and other Linux distributions

Run the `.AppImage`. On first launch, choose **Install for this user** to copy
it to a stable location and register it with KDE. If it will not launch,
right-click the file, open **Properties → Permissions**, and enable **Is
executable**. After installation, select Daisywheel REAmade in KDE's Virtual
Keyboard settings and click **Apply**.

The system keyboard integration currently targets KDE Plasma on Wayland. See
[compatibility](compatibility.md) for other desktop environments and sessions.

## macOS

The experimental `.dmg` and `.zip` are available from the [latest release](https://github.com/Dewopalis/daisywheel-reamade/releases/latest).
Open the `.dmg` and drag Daisywheel REAmade to Applications. The app is not
notarized, so macOS may ask you to approve it on first launch. It has CI build
and launch testing but still needs testing on a physical Mac.

## Update, remove or change keyboard settings on KDE

Update DEB and RPM installations through the system's package or application
manager. To remove one, use that same manager. The AppImage's **Install for this
user** option registers a stable copy; see the [packaging guide](releases/packaging.md#install-downloaded-packages)
for details and removal steps.

To stop Daisywheel from opening as the KDE virtual keyboard, select **None**
or another keyboard in **System Settings → Keyboard → Virtual Keyboard** and
click **Apply**. This does not uninstall the application.

## More detail

See the [release packaging guide](releases/packaging.md) for package
requirements, supported baselines, checksums and removal instructions.
