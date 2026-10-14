"""Desktop entry paths must stay one literal executable, even with field codes."""
from pathlib import Path
import runpy
import subprocess
import tempfile
import unittest

entry = runpy.run_path(str(Path(__file__).parents[1] / 'scripts/register-appimage.py'))['desktop_entry']


class RegistrationTests(unittest.TestCase):
    def test_literal_path_and_system_argument(self):
        output = entry(Path('/tmp/My keyboard.AppImage'))
        self.assertIn('Exec="/tmp/My keyboard.AppImage" --kde-input-method\n', output)
        self.assertIn('X-KDE-Wayland-VirtualKeyboard=true\n', output)

    def test_special_characters_validate(self):
        output = entry(Path('/tmp/My "wheel" $name%file.AppImage'))
        self.assertIn('%%file', output)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'test.desktop'
            target.write_text(output)
            subprocess.run(['desktop-file-validate', str(target)], check=True)

    def test_no_injected_desktop_lines(self):
        with self.assertRaises(ValueError):
            entry(Path('/tmp/wheel\nExec=other.AppImage'))


if __name__ == '__main__':
    unittest.main()
