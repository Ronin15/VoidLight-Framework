# VoidLight-Framework Tools

Tools for managing sprite atlases and texture mappings.

## Atlas Tool - Coherent Workflow

The atlas tool manages the complete sprite atlas lifecycle:

```
atlas.png → EXTRACT → res/sprites/ → MAP → rename → PACK → atlas.png + atlas.json
```

### Quick Start

```bash
# 1. Extract sprites from atlas
python3 tools/atlas_tool.py extract

# 2. Map sprites to texture IDs (opens browser with local server)
python3 tools/atlas_tool.py map
# → Click sprite, click texture ID, repeat
# → Click "Save Mappings" → renames sprite files immediately
# → Press Ctrl+C to stop server

# 3. Pack sprites and update atlas
python3 tools/atlas_tool.py pack
# → Packs sprites into atlas.png
# → Updates atlas.json (single source of truth for coordinates)
# → Cleans up sprite files from res/sprites/
```

### Commands

| Command | Description |
|---------|-------------|
| `extract` | Extract sprites from atlas.png → res/sprites/ |
| `extract-from` | Extract sprites from any source image → res/sprites/ |
| `map` | Visual tool to assign texture IDs to sprites |
| `pack` | Pack sprites into atlas.png + update atlas.json |
| `list` | Show current sprites status |

### Workflow Details

**1. EXTRACT** - Pull sprites from atlas.png
```bash
python3 tools/atlas_tool.py extract
```
- Clears res/sprites/ and extracts fresh from atlas.png
- Auto-detects sprite regions in atlas.png
- Uses existing atlas.json names if available
- Unnamed sprites get `sprite_001.png`, `sprite_002.png`, etc.

**1b. EXTRACT-FROM** - Import sprites from any source image
```bash
# Extract from external image (auto-names: mysheet_001.png, mysheet_002.png, ...)
python3 tools/atlas_tool.py extract-from mysheet.png

# Custom prefix
python3 tools/atlas_tool.py extract-from items.png --prefix sword_

# Custom output directory
python3 tools/atlas_tool.py extract-from source.png -o ./temp/

# Allow larger sprites without splitting
python3 tools/atlas_tool.py extract-from source.png --max-size 128
```
- Extract sprites from any PNG image into res/sprites/
- Auto-prefix from source filename (e.g., `items.png` → `items_001.png`, `items_002.png`)
- Appends to existing sprites (does not clear directory)
- Useful for importing sprites from external sprite sheets
- Optional `--prefix` to override auto-naming
- Optional `--output` for custom output directory

**2. MAP** - Assign texture IDs
```bash
python3 tools/atlas_tool.py map
```
- Starts local HTTP server on localhost:8000
- Opens visual mapper in browser
- Left panel: all sprites (click to select)
- Right panel: expected texture IDs from JSON files
- Click sprite → click ID to assign
- Arrow keys to navigate sprites
- "Save Mappings" applies renames immediately (two-phase rename for circular deps)
- Press Ctrl+C in terminal to stop server when done

**3. PACK** - Build atlas and update atlas.json
```bash
python3 tools/atlas_tool.py pack
```
- Packs all sprites into new atlas.png
- Creates atlas.json with all region coordinates
- Deduplicates sprites (named sprites take priority over unnamed)
- Cleans up sprite files from res/sprites/ after successful pack

### File Locations

- `res/img/atlas.png` - Sprite atlas image
- `res/sprites/` - Individual sprite files (temporary working directory)
- `res/data/atlas.json` - Atlas region coordinates (single source of truth)

Data files (`items.json`, `weapons.json`, `equipment.json`, `materials.json`,
`currency.json`, `races.json`, etc.) define `textureId`. C++ looks up
coordinates from `atlas.json` at runtime using the `textureId`.

### Adding New Sprites

`res/sprites/` is a working directory, not the canonical sprite store. `pack`
rebuilds `atlas.png` and `atlas.json` from whatever PNGs are currently in that
directory, then removes those PNGs. To update the current atlas, start by
extracting it so existing sprites are present before you add or modify files.

**Option A: Add or modify a sprite in the current atlas**
1. Run `python3 tools/atlas_tool.py extract`
2. Add or replace a PNG in `res/sprites/` with the texture ID as filename
   - e.g., `res/sprites/new_item_world.png`
3. Run `python3 tools/atlas_tool.py pack`
4. Atlas rebuilt, atlas.json updated automatically, and `res/sprites/` cleaned

**Option B: From external sprite sheet**
1. Extract sprites from source image:
   ```bash
   python3 tools/atlas_tool.py extract-from ~/Downloads/new_sprites.png
   ```
2. Rename extracted sprites to their texture IDs, or use `map` command
3. Run `python3 tools/atlas_tool.py pack`

### Modifying Existing Sprites

1. Edit the sprite in `res/sprites/`
2. Run `python3 tools/atlas_tool.py pack`
3. Atlas rebuilt with updated sprite

### Requirements

```bash
# Install pillow for image processing
pip install pillow
# or on Ubuntu/Debian:
sudo apt-get install python3-pil
```

---

## Seasonal Texture Generator

`generate_seasonal_textures.sh` generates spring/summer/fall/winter color variants
of tile textures using ImageMagick.

```bash
# 1. Extract sprites from atlas
python3 tools/atlas_tool.py extract

# 2. Run seasonal generator on extracted sprites
bash tools/generate_seasonal_textures.sh

# 3. Re-pack atlas with seasonal variants
python3 tools/atlas_tool.py pack
```

**Requirements:** ImageMagick (`convert` command)

The generator reads base PNGs from `res/sprites/`; missing sources are skipped with a warning. Run `extract` first when generating variants from the current atlas.

**Textures affected:** `biome_celestial`, `biome_default`, `biome_desert`, `biome_forest`, `biome_haunted`, `biome_mountain`, `biome_ocean`, `biome_plains`, `biome_swamp`, `building_cityhall`, `building_house`, `building_hut`, `building_large`, `bush`, `obstacle_grass`, and `obstacle_tree`.

Each base texture produces `spring_`, `summer_`, `fall_`, and `winter_` variants. Data files keep their `textureId`; runtime code resolves seasonal atlas IDs from `atlas.json`.
