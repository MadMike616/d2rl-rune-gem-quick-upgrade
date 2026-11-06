# Hook Inventory

Release: v0.1.75  
Release date: 2026-11-07  
Plugin binary: `d2rl-rune-gem-quick-upgrade.dll`  
File version: `0.1.75.0`

## Native inline hooks

All addresses below are RVAs from the D2R main image. The plugin validates expected bytes before installing each hook and skips the affected path when a fingerprint does not match.

| RVA | Target | Purpose |
| --- | --- | --- |
| `0x2CF760` | Advanced Stash slot input handler | Detect Shift/Alt right-clicks on dedicated material slots and run the selected recipe. |
| `0x2A98B0` | Item widget input dispatcher | Track the modifier gesture; Ctrl suppresses recipe handling so Ctrl+Shift remains available for the game's move-to-Cube action. |
| `0x843FC0` | `UI_ConsumeCurrentInput` | Handle the selected stash-slot action after the widget hit test. |
| `0x2A72D0` | Inventory item renderer | Hide temporary ingredient and output sprites during crafting. |
| `0x2C3390` | Inventory item widget draw | Suppress temporary item-cell flashes during crafting. |
| `0x2A7170` | Inventory slot widget draw | Suppress temporary slot-cell flashes during crafting. |

## Vtable validation targets

The plugin reads these vtable entries to validate the UI structures. It does not write or replace vtable entries.

| Vtable RVA | Entry offset | Expected target | Purpose |
| --- | --- | --- | --- |
| `0x1CF4678` | `+0xC8` | `0x2CE900` | Advanced Stash slot counter getter. |
| `0x1CF4678` | `+0x100` | `0x2CF760` | Advanced Stash slot input handler. |
| `0x1CF3800` | `+0xF0` | `0x2C3390` | Inventory item widget draw method. |
| `0x1CF3F20` | `+0xF0` | `0x2A7170` | Inventory slot widget draw method. |

## Input listener

Modifier state is tracked through D2RLoader's SDK input key-listener service for Shift, Alt, and Control. If that service cannot register, the plugin falls back to Windows keyboard-state reads. The plugin does not install a separate OS-level keyboard hook.

## Release change and collision note

Compared with v0.1.74, v0.1.75 changes the item-input dispatch guard to ignore recipe gestures while Control is held. This preserves Ctrl+Shift item movement and Alt/Shift recipe gestures. No inline hook RVA, patch site, or vtable target changed in this release.

