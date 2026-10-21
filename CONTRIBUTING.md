# Contributing to Daisywheel REAmade

Thanks for helping improve Daisywheel REAmade. The project is in active
development, so focused bug reports, documentation fixes, platform testing and
small, reviewable changes are all useful.

## Before starting

- For a bug, check existing issues and include your operating system, desktop
  session (for Linux, KDE Plasma on Wayland), controller, and the steps that
  reproduce it. Say whether the issue occurs in local test mode or system mode.
- For a larger feature, open an issue first so we can agree on the behavior and
  platform scope before you invest time implementing it.
- Please do not include private text typed with the keyboard, access tokens,
  personal data, or unrelated system logs in an issue.

## Build and check

See the platform setup in the [README](README.md). On Linux with the required
Qt 6, LayerShellQt, Wayland and SDL2 development packages installed:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run `./build/daisywheel` to exercise the wheel in its local test field. System
input needs a KDE Plasma Wayland session and a focused text field. Avoid running
two system-mode instances at the same time because both can read controller
input.

Windows contributors can follow the build and interactive test instructions in
the README's [Windows instructions](README.md#run-on-windows). The native
Windows input and focus tests need an unlocked interactive Windows desktop, so
report clearly which checks you could run.

## Pull requests

- Keep each pull request focused and explain the user-visible change and why it
  is needed.
- Include steps to reproduce a bug fix and note which operating systems or
  sessions you checked.
- Run the relevant build and tests when available. If a check requires
  hardware, Windows, or a live KDE session you do not have, say so in the PR.
- Update user or packaging documentation when behavior, controls, setup, or
  supported platforms change.
- Do not commit build output, packaged binaries, personal configuration, or
  test recordings unless the change specifically needs an asset.
- Be open to review feedback; maintainers may ask for a smaller change or more
  validation before merging.

By submitting a contribution, you agree that it may be distributed under the
project license in [LICENSE](LICENSE), currently GPL-3.0-only. You retain the
copyright to your own work; no separate contributor agreement is currently
required.

## Project scope

Daisywheel REAmade is a controller-first keyboard for handheld computers and
desktop systems. Linux system input currently targets KDE Plasma on Wayland.
Please discuss proposals for other desktops, touch-first behavior, or Steam Deck
Gaming Mode in an issue before building a large change around them.
