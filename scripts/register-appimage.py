#!/usr/bin/env python3
"""Register a fixed AppImage path in KDE without changing the preferred keyboard."""
import os
from pathlib import Path
import subprocess
import sys


def desktop_entry(path: Path) -> str:
    value = str(path)
    if any(c in value for c in '\r\n\x00'):
        raise ValueError('AppImage path must not contain line breaks')
    # Desktop Exec uses its own quoting rules and treats percent as field codes.
    value = value.replace('\\', '\\\\\\\\').replace('"', '\\\\"')
    value = value.replace('`', '\\\\`').replace('$', '\\\\$').replace('%', '%%')
    return f'''[Desktop Entry]
Type=Application
Name=Daisywheel REAmade
Exec="{value}" --kde-input-method
Icon=input-keyboard-virtual
Terminal=false
NoDisplay=true
X-KDE-Wayland-VirtualKeyboard=true
'''


if __name__ == '__main__':
    appimage = Path(sys.argv[1]).resolve(strict=True)
    if not appimage.is_file() or not os.access(appimage, os.X_OK):
        raise SystemExit('AppImage must be an executable file')
    data = Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share')))
    destination = data / 'applications/org.daisywheel.keyboard.desktop'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(desktop_entry(appimage), encoding='utf-8')
    subprocess.run(['kbuildsycoca6', '--noincremental'], check=True)
    print(f'Registered {destination}. Select Daisywheel REAmade in KDE Virtual Keyboard settings and Apply.')
