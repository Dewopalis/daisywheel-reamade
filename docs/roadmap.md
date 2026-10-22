# Roadmap

Daisywheel's path to 1.0 is about dependable behavior when input or the desktop
is interrupted. The milestones below are planning targets, not a promise to
publish every version. Compatibility reports and real-device testing will shape
the work.

## v1.0 readiness

The release is ready when people can install Daisywheel on a documented target,
use its advertised features, and recover from common failures without leaving
their desktop in a broken input state.

### Input reliability and recovery — critical

- No stuck keys or mouse buttons, lost focus, or unintended text entry after
  release, focus changes, hiding the keyboard, quitting, or controller
  disconnect.
- Recover cleanly when a controller disconnects and reconnects, the computer
  sleeps and resumes, the target application closes, or KDE RemoteDesktop
  permission is denied or revoked.
- Keep typing available when pointer permission fails, and clearly explain that
  mouse mode is unavailable.
- Late portal responses must not revive an obsolete session; pending work must
  be cancelled or bounded when a session ends.

### State-transition regression tests — critical

Expand the existing test setup to cover important sequences, including
typing → editing → mouse mode → settings → hidden → disconnected. Prefer
scripted controller-event sequences that assert the final input and session
state. Add tests for each reproduced failure so fixes remain covered.

### Installation lifecycle — high

- Verify clean install, update, startup, and uninstall for each published
  package, including an upgrade from the previous stable release.
- Confirm preferences survive updates and are handled predictably on removal.
- On Windows, test updating while an older version is running and document the
  expected behavior if files are locked.
- Keep package builds reproducible and smoke tests useful across supported
  platforms.

### Privacy, diagnostics, and failure feedback — high

- Do not retain entered text or include it in diagnostic output.
- Explain permissions when they are requested and report permission failures in
  clear, short status messages.
- Indicate controller disconnects and recover automatically when possible.
- Document known limitations and provide enough diagnostic context to report a
  problem without collecting private text.

## Proposed v1.0 platform policy

These are support targets to validate, not claims that every environment
already works. The [compatibility matrix](compatibility.md) remains the source
of current test results.

| Environment | Proposed v1.0 status | Evidence needed |
| --- | --- | --- |
| Windows 11 | Supported target | Repeatable install/update/uninstall and input recovery tests across representative apps and controllers |
| KDE Plasma 6 on Wayland | Supported target | Validate keyboard, controller, portal permission and suspend/reconnect flows |
| Debian, Fedora and Arch-family KDE systems | Supported where validated | State tested package baseline and desktop version; do not infer support for every derivative |
| Steam Deck Desktop Mode | Candidate target | Test the released package and lifecycle on physical hardware |
| Steam Deck Gaming Mode | Experimental / not a 1.0 requirement | Separate integration work and device validation |
| macOS | Experimental | Physical Mac input and integration testing; CI packaging alone is insufficient |
| GNOME and other desktops | Not officially supported for 1.0 | Reconsider only with an appropriate system-keyboard integration and validation |

## Suggested milestones

- **v0.6 — Reliability:** interrupted-input cleanup, reconnection and
  permission recovery, plus state-transition regression tests.
- **v0.7 — Installation and polish:** install/update/uninstall validation,
  preference persistence, clearer failure feedback, and a repeatable release
  checklist.
- **v0.8–0.9 — Release candidates:** external testing on target environments,
  fixes for critical issues, a feature freeze, and final support-policy review.
- **v1.0 — Stable:** no known critical input or installation bugs on supported
  targets, published limitations, and a repeatable release process.

These version numbers are optional milestones; reaching 1.0 does not require
releasing every minor version. New languages, text prediction, extensive
controller-specific artwork, GNOME integration, macOS support, and Steam Deck
Gaming Mode can remain post-1.0 work.

## Existing work and other investigations

Completed foundations include KDE pointer-request timeouts and cancellation,
coalesced pointer movement, held mouse-button cleanup, controller
disconnect/reconnect handling, settings navigation, and package smoke tests.
The readiness work should verify these behaviors under interruption and close
remaining gaps.

Other areas to explore when they support validated use cases:

- Validate touch/tablet opening and KDE keyboard policies on supported hardware.
- Broaden application and controller compatibility on Windows and Linux,
  including browsers and XWayland applications.
- Validate compact layouts and controller labels on handheld screens.
- Add more languages, layouts, and controller-specific button labels.

Windows uses its own activation and input backend. Microsoft's IME API does
not provide a general independent replacement touch keyboard; see
[Microsoft's IME documentation](https://learn.microsoft.com/en-gb/windows/apps/design/input/input-method-editors).

## Suggest a priority

Use [GitHub Discussions or issues](https://github.com/Dewopalis/daisywheel-reamade/issues)
to share use cases and compatibility results before starting larger features.
