# Rune Gem Quick Upgrade

Rune Gem Quick Upgrade adds upgrade and downgrade shortcuts for supported materials in Diablo II: Resurrected Advanced Stash material slots.

- Hold **Shift** and right-click a recipe's primary input slot to upgrade it.
- Hold **Alt** and right-click a recipe's output slot to reverse that recipe.

The plugin handles materials in dedicated Advanced Stash slots. It does not act on items in the Personal or Shared tabs.

## Supported recipes

The plugin reads the active mod's `cubemain.txt` and accepts enabled recipes that match these forms:

- Three identical gems into one gem of the next rank.
- Three identical runes into one rune of the next rank.
- A next-rank rune recipe that also requires one specified gem.
- Three Rejuvenation Potions (`rvs`) into one Full Rejuvenation Potion (`rvl`).
- Additional mod recipes listed in `included-recipes` in the plugin's TOML config.

For an upgrade, click the recipe's primary input slot. Secondary ingredients, such as a gem required for a rune recipe, are withdrawn automatically. For a downgrade, click the recipe's output slot. The plugin recreates all recipe inputs at their specified quantities, including a gem cost when present. If more than one enabled one-output recipe produces that item, it refuses to choose between them.

Custom recipes must be enabled in `cubemain.txt`, use simple item-code inputs, and produce exactly one item. Every input and output must exist in `misc.txt`, `armor.txt`, or `weapons.txt` with `AdvancedStashStackable` set to `1`; each item's type must exist in `itemtypes.txt`. The plugin reads these tables from the active mod and uses D2RLoader's compiled tables for missing files. Recipes with unsupported conditions are ignored. The potion recipe is included only when the active tables define it.

When the local player is ready, the plugin marks Normal-quality stackable materials in the Advanced Stash as identified. This keeps their tooltips correct on the first hover after entering a game.

## Requirements and compatibility

- Windows, Diablo II: Resurrected, and D2RLoader with PluginSDK ABI 4 support.
- An Advanced Stash mod with dedicated material slots and the required inventory, item, and thread services.
- The plugin does not check or restrict the D2R game version. Its native hooks verify expected function bytes and UI vtables, and skip those paths when the structures do not match.
- When available, `bankexpansionlayouthd.json` supplies the item codes for dedicated material slots. If it is missing, the plugin uses its built-in item-code list. Slot recognition does not depend on screen coordinates.
- Works in a local game or as the authoritative TCP/IP host. Controller input and remote TCP/IP clients are not supported.

## Install

1. Copy `d2rl-rune-gem-quick-upgrade.dll` from the release into the active mod's `d2rloader/plugins/` directory.
2. Launch the game through D2RLoader. The plugin creates `d2rloader/config/rune-gem-quick-upgrade.toml` on first run.
3. Open an Advanced Stash material tab and use **Shift + right-click** on a recipe input to upgrade or **Alt + right-click** on its output to downgrade.

By default, the plugin requires and uses the Horadric Cube. Set `allow_without_cube = true` to allow Inventory staging when the character has no Cube. If the character has a Cube, it remains the preferred staging container.

The TOML config has separate switches for each action. Set `enable_upgrading = false` to turn off **Shift + right-click** upgrades, or `enable_downgrading = false` to turn off **Alt + right-click** downgrades. Both default to `true`; either action can remain enabled while the other is disabled.

To enable a mod-specific recipe, add its exact `description` text from `cubemain.txt` to the `included-recipes` array. Matching ignores letter case and surrounding spaces. For example:

```toml
included-recipes = ["2 Corrupted Orb -> Exalted Orb", "2 Exalted Orbs -> Divine Orb"]
```

Every ingredient and output must be marked `AdvancedStashStackable = 1` in the active item tables. These strings only select recipes already defined by the mod; they do not create or alter CubeMain recipes.

## Build from source

Use CMake 3.29 or newer and a Visual Studio C++ toolchain:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

CMake fetches the pinned D2RLoader PluginSDK revision. To build without fetching it, set `RUNE_GEM_QUICK_UPGRADE_LOCAL_SDK_DIR` to a local PluginSDK checkout. The DLL is written to the build directory.

## License

MIT. See [LICENSE](LICENSE).
