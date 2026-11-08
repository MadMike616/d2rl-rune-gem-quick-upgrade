# Hook Inventory — Rune Gem Quick Upgrade

## Plugin metadata

| Field | Value |
|---|---|
| Plugin name | Rune Gem Quick Upgrade |
| Plugin ID | rune-gem-quick-upgrade |
| Release version | 0.1.76 |
| Inventory updated | 2026-11-08 |
| Source revision | Tag v0.1.76 |
| D2R build(s) audited | Not validated |
| D2RLoader SDK / Plugin ABI | SDK 0.4.0 / Plugin ABI 4 |
| Audit status | Complete |

## Scope and method

Inspected `src/plugin.cpp`, `src/plugin.rc`, `CMakeLists.txt`, and `README.md` in the v0.1.76 release source. Searched for every `InstallInlineHook` call and reviewed its target, expected-byte guard, and related widget checks. The plugin installs six inline hooks. The inventory is source-level; no live game build was validated. No hook-site changes were made from v0.1.75.

## Hook sites

| Site ID | Type | Target | Address / identifier | Operation | Guard / validation | Source reference | Notes |
|---|---|---|---|---|---|---|---|
| H-01 | Inline | Advanced Stash slot input handler | RVA 0x2CF760 | Installs a handler for recipe gestures on material slots | Expected function bytes and Advanced Stash vtable entries are validated | `src/plugin.cpp` — `InstallAdvancedStashSlotInputHook` | Hook is installed only when the validated Advanced Stash path is available. |
| H-02 | Inline | Item widget input dispatcher | RVA 0x2A98B0 | Tracks modifier state and filters recipe input | Expected function bytes are checked before installation | `src/plugin.cpp` — `InstallItemWidgetInputBridge` | Ctrl suppresses crafting so Ctrl+Shift remains available to the game's move-to-Cube gesture. |
| H-03 | Inline | UI_ConsumeCurrentInput | RVA 0x843FC0 | Handles the selected stash-slot action after hit testing | Expected function bytes are checked before installation | `src/plugin.cpp` — `InstallConsumeCurrentInputHook` | Input dispatch path. |
| H-04 | Inline | Inventory item renderer | RVA 0x2A72D0 | Hides temporary ingredient and output sprites during crafting | Expected function bytes are checked before installation | `src/plugin.cpp` — `InstallTemporarySpriteRendererHook` | Temporary sprite suppression. |
| H-05 | Inline | Inventory item widget draw | RVA 0x2C3390 | Suppresses temporary item-cell flashes | Expected function bytes and the widget vtable entry are validated | `src/plugin.cpp` — `InstallTemporaryInventoryWidgetDrawHook` | Vtable entry is checked, not replaced. |
| H-06 | Inline | Inventory slot widget draw | RVA 0x2A7170 | Suppresses temporary slot-cell flashes | Expected function bytes and the widget vtable entry are validated | `src/plugin.cpp` — `InstallTemporaryInventorySlotWidgetDrawHook` | Vtable entry is checked, not replaced. |

## Known collision overlaps

None identified in the current inventory; this is not a compatibility guarantee.

| Other plugin | Shared target | Relationship | Evidence / notes |
|---|---|---|---|

## Audit notes

The plugin reads Advanced Stash and inventory vtable entries to confirm widget types but does not replace those entries. Modifier listeners use D2RLoader's input service with a keyboard-state fallback; no operating-system input hook is installed. No game build was validated. The v0.1.76 Release rebuild changes optimization and file-version metadata only; native hook sites are unchanged from v0.1.75.

