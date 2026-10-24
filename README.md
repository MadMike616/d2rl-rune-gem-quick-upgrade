# Rune Gem Quick Upgrade

Rune Gem Quick Upgrade adds a shortcut for upgrading supported materials in Diablo II: Resurrected Advanced Stash material slots. Hold **Shift** and right-click a material slot to run a matching Horadric Cube recipe. Ingredients are staged temporarily, consumed through an item transaction, and the output is deposited back into its dedicated stash slot.

## Supported recipes

The plugin reads the active mod's `cubemain.txt` and accepts enabled recipes that match these forms:

- Three identical gems into one gem of the next rank.
- Three identical runes into one rune of the next rank.
- The mod's next-rank rune recipe that also requires one specified gem.
- Three Rejuvenation Potions (`rvs`) into one Full Rejuvenation Potion (`rvl`).
- Additional mod recipes listed in `included-recipes` in the plugin's TOML config.

Additional recipes must be enabled in `cubemain.txt`, use simple item-code inputs, and produce exactly one item. Every input and the output must exist in `misc.txt`, `armor.txt`, or `weapons.txt` with `AdvancedStashStackable` set to `1`, and its item type must exist in `itemtypes.txt`. The plugin reads these tables from the active mod and uses D2RLoader's compiled tables for missing files. Other custom recipes and recipes with unsupported conditions are ignored. The potion recipe is included only when the active recipe tables define it.

## Requirements and compatibility

- Windows, Diablo II: Resurrected, and D2RLoader with PluginSDK ABI 4 support. The plugin does not gate on a specific D2R version.
- An Advanced Stash mod with dedicated material slots and the required inventory, item, and thread services.
- The plugin's native Advanced Stash input path checks function fingerprints and expected UI vtables, and skips the native path when those layouts do not match. It has been validated on D2R build **93847**.
- The plugin reads loose Excel tables from the active mod when available and falls back to D2RLoader's compiled Excel data. It reads `bankexpansionlayouthd.json` for material-slot coordinates when present; otherwise it uses built-in coordinates.
- Works in a local game or as the authoritative TCP/IP host. Controller input and remote TCP/IP clients are not supported.

## Install

1. Copy `d2rl-rune-gem-quick-upgrade.dll` from the release into the active mod's `d2rloader/plugins/` directory.
2. Launch the game through D2RLoader. The plugin creates `d2rloader/config/rune-gem-quick-upgrade.toml` on first run.
3. Open an Advanced Stash material tab, hold **Shift**, and right-click a slot with enough ingredients for a supported recipe.

The default setting stages ingredients in the Horadric Cube and requires the character to carry it. Set `allow_without_cube = true` in the plugin's TOML file to allow Inventory staging when the character has no Cube. If the character does carry the Cube, the plugin continues to use it.

To enable a mod-specific recipe, add its exact `description` text from `cubemain.txt` to the `included-recipes` array. Matching ignores letter case and surrounding spaces. For example, if the mod's `description` fields use these strings:

```toml
included-recipes = ["2 Corrupted Orb -> Exalted Orb", "2 Exalted Orbs -> Divine Orb"]
```

Every ingredient and output item must be marked `AdvancedStashStackable = 1` in the active item tables. The strings only select recipes already defined by the mod; they do not create or alter CubeMain recipes.

## Build from source

Use CMake 3.29 or newer and a Visual Studio C++ toolchain:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

CMake fetches the pinned D2RLoader PluginSDK revision. To build without fetching it, set `RUNE_GEM_QUICK_UPGRADE_LOCAL_SDK_DIR` to a local PluginSDK checkout. The DLL is written to the build directory.

## License

MIT. See [LICENSE](LICENSE).
