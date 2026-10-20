# Binary builds and releases

The `Build packages` workflow builds x86-64 downloads using GitHub Actions.
PRs, pushes to `main`, and manual workflow runs upload test artifacts. Tags of
the form `vMAJOR.MINOR.PATCH` additionally create a **draft** GitHub release after
every build succeeds. No personal access token or repository secret is needed;
the release job uses GitHub's scoped automatic token.

The project is licensed under GPL-3.0-only. Native packages include the license
text with their documentation; the RPM metadata declares the SPDX license ID.

For a recent build, open the [Build packages workflow](https://github.com/Dewopalis/daisywheel-reamade/actions/workflows/packages.yml),
choose a successful run, then download the required bundle in its **Artifacts**
section. GitHub requires sign-in and repository read access for workflow
artifacts. They expire after 14 days. For stable downloads, use
[GitHub Releases](https://github.com/Dewopalis/daisywheel-reamade/releases):
tagged builds create drafts, which maintainers test before publishing.

Version [v0.4.0](https://github.com/Dewopalis/daisywheel-reamade/releases/tag/v0.4.0)
is the current release. Its workflow passed the Windows, DEB/AppImage and RPM
jobs. CI installed the DEB and RPM packages and checked startup, verified the
extracted AppImage, and smoke-tested the staged Windows package. Testing across
more target devices and applications remains ongoing.

## Downloads produced

| Download | Build environment | Runtime requirement |
| --- | --- | --- |
| Windows setup `.exe` | Windows 2022 runner, MSYS2 UCRT64 Qt/SDL2, Inno Setup | Windows 11 x64; runtime DLLs included |
| Windows portable `.zip` | Same as installer | Extract the whole folder, use `Start Daisywheel.cmd` |
| `.deb` | Debian 13, system Qt 6.8 / LayerShellQt 6.3 | Debian 13 KDE/Wayland; package manager installs runtime dependencies |
| `.rpm` | Fedora 43, system Qt/LayerShellQt | Fedora 43 KDE/Wayland; dependency-compatible later releases need validation |
| `.AppImage` | Debian 13; Qt, SDL2 and LayerShellQt bundled | KDE/Wayland, x86-64, glibc 2.41+; older distributions are not supported by this build |

AppImage is the intended download for current CachyOS/Arch users. It does not
make system-input support available on other desktops or Steam Deck Gaming Mode.
The hosted v0.3.0 package workflow passed its CI installation and startup checks;
manual testing on target systems is still needed before promising broader device
or application compatibility. Windows and Linux packages are currently unsigned.

## Create a release

1. Merge packaging changes and check all `Build packages` jobs on `main`.
2. Update `project(Daisywheel VERSION ...)` in `CMakeLists.txt`, commit, and merge.
3. Tag the release commit with the same version, for example:

   ```bash
   git switch main
   git pull --ff-only origin main
   git tag v0.4.0
   git push origin v0.4.0
   ```

   Use the same `vMAJOR.MINOR.PATCH` version as `CMakeLists.txt`. The current
   development version is `0.4.0`; prerelease suffixes are not supported.
4. Wait for Windows, DEB/AppImage and RPM jobs to succeed. The tag must match
   the CMake version.
5. Open the draft under GitHub Releases. Download and test the actual installer,
   portable ZIP, native packages and AppImage; check controller typing, focus,
   activation policies and restart startup on the intended machines.
6. Edit the release notes and publish the reviewed draft. `SHA256SUMS.txt`
   contains checksums for all five downloads.

Build jobs have read-only repository permission. Only the tag-triggered release
job gets `contents: write`. Pull requests cannot publish releases. Artifacts
are retained for 14 days; published releases provide persistent downloads.

## Install downloaded packages

**Windows:** run the setup EXE. It installs for the current user, creates a Start
Menu entry that starts system mode, and can create a desktop shortcut. Sign-in
startup remains opt-in inside the app. The portable ZIP needs its entire folder;
`bin/daisywheel.exe` alone is not a portable application.

**Debian / Ubuntu:** double-click the downloaded `.deb` to open KDE Discover (or
your installed package installer), then choose **Install**. You can also install
from a terminal with `sudo apt install ./daisywheel-reamade_*.deb`.

**Fedora:** double-click the downloaded `.rpm` to open KDE Discover (or your
installed package installer), then choose **Install**. The terminal option is
`sudo dnf install ./daisywheel-reamade-*.rpm`.

For either Linux native package, select Daisywheel REAmade in KDE's Virtual
Keyboard settings and Apply. Do not add a second autostart entry. An old user-local
desktop entry from the source-install instructions can shadow the packaged one;
remove or update it before selecting the package's keyboard.

**CachyOS / Arch AppImage:** open the downloaded AppImage. If it will not open,
right-click it, choose **Properties → Permissions**, and enable **Is executable**.
On first launch, choose **Install for this user**. The installer copies it to a
stable location and registers it with KDE without changing your preferred
keyboard. To finish, select Daisywheel REAmade in **System Settings → Keyboard →
Virtual Keyboard** and click **Apply**. The installer offers to open that
settings page for you. Reinstalling a newer AppImage from its download location
replaces the installed copy and updates the KDE registration.

If the system lacks AppImage FUSE support, set `APPIMAGE_EXTRACT_AND_RUN=1` for
one-off runs. For KWin startup on those systems, extract to a stable directory and
register its `AppRun --kde-input-method` path manually rather than depending on
temporary extraction paths.

## Local packaging

Linux CMake installation is staged rather than writing to your machine:

```bash
cmake -S . -B build-ci -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-ci --parallel
ctest --test-dir build-ci --output-on-failure
DESTDIR="$PWD/work/staging" cmake --install build-ci
cpack --config build-ci/CPackConfig.cmake -G DEB -B dist  # Debian build host
cpack --config build-ci/CPackConfig.cmake -G RPM -B dist  # Fedora build host
bash scripts/package-appimage.sh 0.4.0 build-ci
```

Native packages must be built in their corresponding distro environment;
converting an Arch build into a DEB/RPM does not produce compatible dependencies.
The AppImage packager downloads version-pinned linuxdeploy and Qt-plugin tools
and verifies their SHA-256 digests before executing them.

Windows CI uses `scripts/package-windows.ps1` after a UCRT64 build. It stages the
application, runs `windeployqt`, collects transitive non-system DLLs, tests staged
startup and creates ZIP/installer artifacts. The existing `build-windows.ps1`
remains the separate developer build script for the earlier MinGW setup.

CI runs shared selection/visibility/placement tests and headless startup checks.
DEB and RPM jobs install their generated package before checking startup. The
AppImage job tests the extracted bundled runtime and presence of Wayland plugins.
Interactive Windows focus tests and real KDE touch/controller flows remain manual;
hosted runners are not handheld desktops.
