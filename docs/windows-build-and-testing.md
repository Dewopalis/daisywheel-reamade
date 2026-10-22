# Windows build and testing

For normal use, see [Windows installation](installation.md#windows). This guide
is for developers who want to build the project or run its Windows input and
focus tests.

## Requirements

Use a matching **Qt 6 / MinGW 64-bit** pair, CMake 3.20+, Ninja and the SDL2
MinGW 64-bit development package. The build script defaults to dependencies in
`work/`; pass `-QtRoot`, `-CompilerRoot`, `-SDLRoot`, `-CMake` and `-Ninja` to
use other installations.

## Build and launch

From PowerShell in the repository directory:

```powershell
& .\scripts\build-windows.ps1
Start-Process -FilePath '.\build-windows\daisywheel.exe' -ArgumentList '--controllers' -Wait -RedirectStandardOutput '.\build-windows\controllers.txt'
Get-Content '.\build-windows\controllers.txt'
Start-Process -FilePath '.\build-windows\daisywheel.exe' -ArgumentList '--windows-input'
```

The script configures and builds the app, runs the shared wheel tests, and
deploys runtime DLLs alongside the executables. Keep the whole
`build-windows` directory together. Daisywheel sends characters, Space,
Backspace and Enter to the current foreground application. Release physical
Shift/Ctrl/Alt/Windows keys before typing. Windows may block input to apps
running at a higher privilege level. Secure desktops and login screens are not
supported.

## Interactive input and focus tests

Run these on an unlocked Windows desktop:

```powershell
.\build-windows\windows-input-tests.exe
.\build-windows\windows-focus-tests.exe
```

The input test uses its own native text control to check Unicode, a surrogate
pair, Backspace, Enter, self-process rejection and overlay focus preservation.
The focus test checks native and Qt editable fields, fields in the same window,
read-only controls, buttons, document editors and password fields. Both tests
focus their own verified fixture windows, including a synthetic mouse click;
they do not read user text.

## Current validation and limits

The Windows development PC has passed a Release build, shared wheel and
visibility tests, isolated native input and UI Automation focus tests,
local/system-mode smoke tests, and controller detection. Controller-driven
typing into another application has also been confirmed.

Reconnects, browsers, elevated applications and handheld displays still need
more testing. A Qt text-field probe reordered a mixed Unicode and emoji string
injected as one burst, while the same sequence passed against a native Windows
EDIT control. Current wheel actions send one ASCII character per button press;
this remains a compatibility concern for future multi-character Unicode
layouts or prediction.

For startup and input problems, see [troubleshooting](troubleshooting.md#windows).
For implementation details, see [Windows implementation notes](windows-implementation-notes.md).
