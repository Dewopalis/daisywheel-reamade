# Controls and settings

## Typing and editing

Characters sit at the physical south/east/west/north face-button positions.
SDL supplies controller mappings; colors currently follow Xbox conventions.

| Control | Action |
| --- | --- |
| Left stick | Select a petal; neutral does not enter a character |
| A / B / X / Y | Enter the bottom / right / left / top character in the selected petal |
| Left bumper | Toggle uppercase |
| Right bumper | Switch character pages: letters and symbols; with Extended special characters, also accents and punctuation |
| Left stick click | Space |
| Right stick click / hold | Backspace once / repeat while held |
| Start / Menu | Enter |
| Configured shortcut | Show or hide the keyboard |

With the keyboard open, hold **RT** and move the left stick to move the text
cursor. Hold **LB** while moving to select text; hold **RB** to move by words,
or both to select by words.

## Controller mouse mode

Enable mouse mode in Settings. Hold **LT** while the keyboard is open, then
move the left stick to move the pointer. **R1** left-clicks and **R2**
right-clicks. **X / Y** scroll up/down; D-pad up/down scroll faster. Mouse mode
is inactive while the keyboard is hidden, so it does not interfere with games.

On KDE, Daisywheel asks the desktop's RemoteDesktop portal for pointer access
the first time mouse mode is used. Grant permission when prompted.

## Settings

The wheel shows L2/L1 and R1/R2 at the top and L3/R3 at the lower sides. A
marker lights while a control is held, and a face-button press briefly
highlights its matching character. Open **Settings** from the title bar for the
Help tutorial and configuration.

| Setting | Behavior |
| --- | --- |
| Controller shortcut | Choose Home/Guide, Back/View or Start/Menu; selected button no longer performs its normal wheel action |
| Controller profile | Living room / desktop enables mouse mode; Handheld disables it; Custom allows individual settings |
| Character layout | Standard or Extended special characters; imported layouts also include an extended page |
| Layout file | Import or export a plain-text layout to edit and share it |
| Stick deadzone | Shared by wheel selection, pointer movement and cursor editing; adjustable from 5–45% |
| Pointer speed | Adjusts controller pointer movement |
| Haptic feedback | Optional button feedback with adjustable strength; stick movement does not trigger rumble |
| Sound effects | Optional petal and button sounds with volume control and previews |
| Opacity | 30–100%, saved between launches |
| Pin position | Lock or unlock dragging |
| Hide keyboard | Hides the wheel while controller handling remains active |
| Quit Daisywheel | Stops the process; reopening requires launching it again |

Settings are controller navigable. Use the D-pad to move between controls,
left/right to change a value, **A/Start** to activate, **LB/RB** to switch tabs,
and **B** to go back. The Help screen explains the controls.

## Visibility and position

On KDE, automatic opening follows the **Show Virtual Keyboard** policy in
System Settings. Manual controller opening works regardless of that policy.
Windows has a separate automatic text-field setting. See
[installation](installation.md) for setup.

Hiding the overlay suppresses automatic reopening in the same input context;
leaving that context restores automatic behavior. KDE can only commit text
into an active input context. On Windows, focus a text field in the foreground
application first.

Drag the header to move the wheel and the diagonal grip to resize it. A
translucent outline previews the result during KDE dragging. Position, size and
opacity are saved between launches. Pin position in Settings to lock dragging.

## Related guides

- [Installation and KDE keyboard selection](installation.md)
- [Compatibility and known limitations](compatibility.md)
- [Troubleshooting](troubleshooting.md)
