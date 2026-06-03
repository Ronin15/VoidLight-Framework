#!/usr/bin/env python3
"""
Atlas Tool for VoidLight-Framework

Coherent workflow for managing sprite atlases:

1. EXTRACT  - Extract sprites from atlas.png → res/sprites/
2. EDIT     - Add/modify sprites in res/sprites/ (manual step)
3. MAP      - Assign texture IDs to sprites (visual tool)
4. PACK     - Pack sprites into atlas.png + update atlas.json

Usage:
    python3 tools/atlas_tool.py extract    # Extract sprites from atlas.png
    python3 tools/atlas_tool.py map        # Open visual mapper to assign IDs
    python3 tools/atlas_tool.py pack       # Pack sprites and update atlas.json
    python3 tools/atlas_tool.py list       # List current sprites

Requires: pip install pillow
"""

import argparse
import hashlib
import json
import os
import sys
import webbrowser
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Required: pip install pillow")
    sys.exit(1)


def get_paths():
    """Get standard project paths."""
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    return {
        'project_root': project_root,
        'sprites_dir': project_root / "res" / "sprites",
        'atlas_png': project_root / "res" / "img" / "atlas.png",
        'atlas_json': project_root / "res" / "data" / "atlas.json",
        'mapper_session': script_dir / "atlas_mapper_session.html",
    }


def get_image_hash(img: Image.Image) -> str:
    """Get hash of image pixel data for duplicate detection."""
    return hashlib.md5(img.tobytes()).hexdigest()


# =============================================================================
# HTTP Server for Mapper UI
# =============================================================================

class MapperRequestHandler(SimpleHTTPRequestHandler):
    """HTTP handler for the sprite mapper with save endpoint."""

    sprites_dir = None  # Set by cmd_map
    paths = None        # Set by cmd_map

    def do_POST(self):
        if self.path == '/save-mappings':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            mappings = json.loads(post_data.decode('utf-8'))

            sprites_dir = MapperRequestHandler.sprites_dir
            try:
                renamed = apply_sprite_renames(sprites_dir, mappings)
            except ValueError as exc:
                self.send_response(400)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                response = json.dumps({'status': 'error', 'error': str(exc)})
                self.wfile.write(response.encode('utf-8'))
                return

            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            response = json.dumps({'status': 'saved', 'renamed': renamed})
            self.wfile.write(response.encode('utf-8'))

            if renamed:
                print(f"\nRenamed {len(renamed)} sprites.")
            else:
                print("\nNo renames needed - all mappings already match filenames.")
        else:
            self.send_response(404)
            self.end_headers()

    def do_GET(self):
        if self.path == '/mapper-state':
            if MapperRequestHandler.paths is None:
                self.send_response(500)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                response = json.dumps({'status': 'error', 'error': 'Mapper paths not configured'})
                self.wfile.write(response.encode('utf-8'))
                return

            state = build_mapper_state(MapperRequestHandler.paths)
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(state).encode('utf-8'))
            return

        super().do_GET()

    def log_message(self, format, *args):
        pass  # Suppress request logging


# =============================================================================
# EXTRACT - Pull sprites from atlas.png
# =============================================================================

def detect_sprite_regions(atlas: Image.Image) -> list:
    """Auto-detect sprite regions using simple flood fill."""
    width, height = atlas.size
    pixels = atlas.load()
    visited = set()
    regions = []

    def is_transparent(x, y):
        if x < 0 or x >= width or y < 0 or y >= height:
            return True
        return pixels[x, y][3] == 0

    def flood_fill(start_x, start_y):
        stack = [(start_x, start_y)]
        min_x, min_y = start_x, start_y
        max_x, max_y = start_x, start_y

        while stack:
            x, y = stack.pop()
            if (x, y) in visited or is_transparent(x, y):
                continue
            visited.add((x, y))
            min_x, min_y = min(min_x, x), min(min_y, y)
            max_x, max_y = max(max_x, x), max(max_y, y)
            for dx, dy in [(0, 1), (0, -1), (1, 0), (-1, 0)]:
                nx, ny = x + dx, y + dy
                if (nx, ny) not in visited:
                    stack.append((nx, ny))

        return min_x, min_y, max_x, max_y

    for y in range(height):
        for x in range(width):
            if (x, y) not in visited and not is_transparent(x, y):
                min_x, min_y, max_x, max_y = flood_fill(x, y)
                w = max_x - min_x + 1
                h = max_y - min_y + 1
                # Filter out tiny sprites (shadows, noise) - minimum 1x1
                if w >= 1 and h >= 1:
                    regions.append({'x': min_x, 'y': min_y, 'w': w, 'h': h})

    regions.sort(key=lambda r: (r['y'], r['x']))
    return regions


def split_by_gaps(region: dict, atlas: Image.Image, max_size: int = 64) -> list:
    """Split a region using transparency gaps, or grid-based splitting as fallback."""
    if region['w'] <= max_size and region['h'] <= max_size:
        return [region]

    # Extract sub-image
    sub_img = atlas.crop((region['x'], region['y'],
                          region['x'] + region['w'], region['y'] + region['h']))
    pixels = sub_img.load()

    # Find horizontal gaps (rows of full transparency)
    h_splits = [0]
    for y in range(sub_img.height):
        if all(pixels[x, y][3] == 0 for x in range(sub_img.width)):
            if h_splits[-1] != y:
                h_splits.append(y)
    h_splits.append(sub_img.height)

    # Find vertical gaps (columns of full transparency)
    v_splits = [0]
    for x in range(sub_img.width):
        if all(pixels[x, y][3] == 0 for y in range(sub_img.height)):
            if v_splits[-1] != x:
                v_splits.append(x)
    v_splits.append(sub_img.width)

    # If no gaps found, fall back to grid-based splitting
    if len(h_splits) <= 2 and len(v_splits) <= 2:
        return split_by_grid(region, atlas, grid_size=32)

    # Split by gaps and process each cell
    result = []
    for i in range(len(h_splits) - 1):
        for j in range(len(v_splits) - 1):
            y1, y2 = h_splits[i], h_splits[i + 1]
            x1, x2 = v_splits[j], v_splits[j + 1]

            if x2 - x1 < 2 or y2 - y1 < 2:
                continue

            cell = sub_img.crop((x1, y1, x2, y2))
            cell_regions = detect_sprite_regions(cell)

            for cr in cell_regions:
                cr['x'] += region['x'] + x1
                cr['y'] += region['y'] + y1
                result.append(cr)

    return result if result else [region]


def split_by_grid(region: dict, atlas: Image.Image, grid_size: int = 32) -> list:
    """Force split a region into grid cells, then find tight bounds in each."""
    result = []

    for gy in range(0, region['h'], grid_size):
        for gx in range(0, region['w'], grid_size):
            cell_x = region['x'] + gx
            cell_y = region['y'] + gy
            cell_w = min(grid_size, region['w'] - gx)
            cell_h = min(grid_size, region['h'] - gy)

            # Extract cell and run flood fill to find sprites within
            cell = atlas.crop((cell_x, cell_y, cell_x + cell_w, cell_y + cell_h))
            cell_regions = detect_sprite_regions(cell)

            for cr in cell_regions:
                cr['x'] += cell_x
                cr['y'] += cell_y
                result.append(cr)

    # Merge sprites that were split at grid boundaries
    if result:
        result = merge_adjacent_sprites(result, atlas)

    return result if result else [region]


def merge_adjacent_sprites(regions: list, atlas: Image.Image, min_overlap_ratio: float = 0.5) -> list:
    """Merge sprites that share a significant boundary (were split by grid)."""
    if not regions:
        return regions

    # Build adjacency: check which sprites actually touch in the atlas
    pixels = atlas.load()
    width, height = atlas.size

    def count_shared_pixels(r1: dict, r2: dict) -> tuple:
        """Count pixels where two sprites touch and return (count, edge_length)."""
        shared = 0
        edge_len = 0

        # Horizontal adjacency (r1 left of r2)
        if r1['x'] + r1['w'] == r2['x']:
            y_start = max(r1['y'], r2['y'])
            y_end = min(r1['y'] + r1['h'], r2['y'] + r2['h'])
            edge_len = y_end - y_start
            for y in range(y_start, y_end):
                x1 = r1['x'] + r1['w'] - 1
                x2 = r2['x']
                if (0 <= x1 < width and 0 <= x2 < width and
                    pixels[x1, y][3] > 0 and pixels[x2, y][3] > 0):
                    shared += 1
            if shared > 0:
                return shared, edge_len

        # Horizontal adjacency (r2 left of r1)
        if r2['x'] + r2['w'] == r1['x']:
            y_start = max(r1['y'], r2['y'])
            y_end = min(r1['y'] + r1['h'], r2['y'] + r2['h'])
            edge_len = y_end - y_start
            for y in range(y_start, y_end):
                x1 = r1['x']
                x2 = r2['x'] + r2['w'] - 1
                if (0 <= x1 < width and 0 <= x2 < width and
                    pixels[x1, y][3] > 0 and pixels[x2, y][3] > 0):
                    shared += 1
            if shared > 0:
                return shared, edge_len

        # Vertical adjacency (r1 above r2)
        if r1['y'] + r1['h'] == r2['y']:
            x_start = max(r1['x'], r2['x'])
            x_end = min(r1['x'] + r1['w'], r2['x'] + r2['w'])
            edge_len = x_end - x_start
            for x in range(x_start, x_end):
                y1 = r1['y'] + r1['h'] - 1
                y2 = r2['y']
                if (0 <= y1 < height and 0 <= y2 < height and
                    pixels[x, y1][3] > 0 and pixels[x, y2][3] > 0):
                    shared += 1
            if shared > 0:
                return shared, edge_len

        # Vertical adjacency (r2 above r1)
        if r2['y'] + r2['h'] == r1['y']:
            x_start = max(r1['x'], r2['x'])
            x_end = min(r1['x'] + r1['w'], r2['x'] + r2['w'])
            edge_len = x_end - x_start
            for x in range(x_start, x_end):
                y1 = r1['y']
                y2 = r2['y'] + r2['h'] - 1
                if (0 <= y1 < height and 0 <= y2 < height and
                    pixels[x, y1][3] > 0 and pixels[x, y2][3] > 0):
                    shared += 1
            if shared > 0:
                return shared, edge_len

        return 0, 0

    def should_merge(r1: dict, r2: dict) -> bool:
        """Merge only if sprites share significant boundary (>50% of edge)."""
        # Quick bounding box check
        if (r1['x'] + r1['w'] < r2['x'] - 1 or r2['x'] + r2['w'] < r1['x'] - 1 or
            r1['y'] + r1['h'] < r2['y'] - 1 or r2['y'] + r2['h'] < r1['y'] - 1):
            return False

        shared, edge_len = count_shared_pixels(r1, r2)
        if edge_len == 0:
            return False

        # Require significant overlap (>50% of shared edge has content on both sides)
        ratio = shared / edge_len
        return ratio >= min_overlap_ratio

    # Union-Find to group touching sprites
    parent = list(range(len(regions)))

    def find(x):
        if parent[x] != x:
            parent[x] = find(parent[x])
        return parent[x]

    def union(x, y):
        px, py = find(x), find(y)
        if px != py:
            parent[px] = py

    # Find all pairs that should merge (significant boundary overlap)
    for i in range(len(regions)):
        for j in range(i + 1, len(regions)):
            if should_merge(regions[i], regions[j]):
                union(i, j)

    # Group regions by their root
    groups = {}
    for i, r in enumerate(regions):
        root = find(i)
        if root not in groups:
            groups[root] = []
        groups[root].append(r)

    # Merge each group into a single bounding box (but limit max merged size)
    max_merged_size = 96  # Don't merge if result would be larger than this
    result = []
    for group in groups.values():
        if len(group) == 1:
            result.append(group[0])
        else:
            # Calculate merged bounding box
            min_x = min(r['x'] for r in group)
            min_y = min(r['y'] for r in group)
            max_x = max(r['x'] + r['w'] for r in group)
            max_y = max(r['y'] + r['h'] for r in group)
            merged_w = max_x - min_x
            merged_h = max_y - min_y

            # If merged would be too large, keep sprites separate
            if merged_w > max_merged_size or merged_h > max_merged_size:
                result.extend(group)
            else:
                result.append({
                    'x': min_x,
                    'y': min_y,
                    'w': merged_w,
                    'h': merged_h
                })

    return result


def split_oversized_regions(regions: list, atlas: Image.Image, max_size: int = 64) -> list:
    """Process all regions, splitting oversized ones."""
    result = []
    for region in regions:
        if region['w'] > max_size or region['h'] > max_size:
            result.extend(split_by_gaps(region, atlas, max_size))
        else:
            result.append(region)
    return result


def cmd_extract(paths: dict, max_size: int = 64):
    """Extract sprites from atlas.png to res/sprites/."""
    atlas_path = paths['atlas_png']
    sprites_dir = paths['sprites_dir']
    atlas_json_path = paths['atlas_json']

    if not atlas_path.exists():
        print(f"Error: Atlas not found: {atlas_path}")
        return False

    atlas = Image.open(atlas_path).convert('RGBA')
    print(f"Loaded atlas: {atlas.width}x{atlas.height}")

    regions = []

    # If atlas.json exists, use its coordinates directly
    if atlas_json_path.exists():
        try:
            with open(atlas_json_path, 'r') as f:
                data = json.load(f)
            for name, coords in data.get('regions', {}).items():
                regions.append({
                    'name': name,
                    'x': coords['x'],
                    'y': coords['y'],
                    'w': coords['w'],
                    'h': coords['h']
                })
            print(f"Using atlas.json: {len(regions)} regions")
        except Exception as e:
            print(f"Warning: Could not load atlas.json: {e}")

    # Fall back to flood fill detection if no atlas.json
    if not regions:
        print("Detecting sprite regions (flood fill)...")
        regions = detect_sprite_regions(atlas)
        print(f"Found {len(regions)} regions")

        # Split oversized regions using gap detection
        oversized = sum(1 for r in regions if r['w'] > max_size or r['h'] > max_size)
        if oversized > 0:
            print(f"Splitting {oversized} oversized regions (max_size={max_size})...")
            regions = split_oversized_regions(regions, atlas, max_size=max_size)
            print(f"After splitting: {len(regions)} regions")

        # Assign names after splitting
        for i, r in enumerate(regions):
            r['name'] = f"sprite_{i+1:03d}"

    regions.sort(key=lambda r: (r['y'], r['x']))

    # Create sprites directory
    sprites_dir.mkdir(parents=True, exist_ok=True)

    # Clear existing sprites
    for old_file in sprites_dir.glob("*.png"):
        old_file.unlink()

    # Extract sprites
    extracted = []
    for r in regions:
        name = r['name']
        sprite = atlas.crop((r['x'], r['y'], r['x'] + r['w'], r['y'] + r['h']))
        output_path = sprites_dir / f"{name}.png"
        sprite.save(output_path)

        extracted.append({
            'name': name,
            'x': r['x'], 'y': r['y'], 'w': r['w'], 'h': r['h']
        })

    # Save extraction manifest
    manifest = {'sprites': extracted}
    manifest_path = sprites_dir / "_manifest.json"
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)

    print(f"\nExtracted {len(extracted)} sprites to {sprites_dir}")
    print(f"\nNext step: Run 'python3 tools/atlas_tool.py map' to assign texture IDs")

    return True


def cmd_extract_from(paths: dict, source: str, output_dir: str = None,
                     prefix: str = None, max_size: int = 64):
    """Extract sprites from any source image into sprites directory."""
    source_path = Path(source)
    if not source_path.exists():
        print(f"Error: Source image not found: {source_path}")
        return False

    # Output directory: custom or default res/sprites/
    sprites_dir = Path(output_dir) if output_dir else paths['sprites_dir']
    sprites_dir.mkdir(parents=True, exist_ok=True)

    # Auto-prefix from source filename (e.g., "items_sheet.png" -> "items_sheet_")
    auto_prefix = prefix if prefix else f"{source_path.stem}_"

    # Load and detect sprites
    img = Image.open(source_path).convert('RGBA')
    print(f"Loaded: {source_path} ({img.width}x{img.height})")

    regions = detect_sprite_regions(img)
    print(f"Found {len(regions)} regions")

    # Split oversized regions
    oversized = sum(1 for r in regions if r['w'] > max_size or r['h'] > max_size)
    if oversized > 0:
        print(f"Splitting {oversized} oversized regions...")
        regions = split_oversized_regions(regions, img, max_size)
        print(f"After splitting: {len(regions)} regions")

    regions.sort(key=lambda r: (r['y'], r['x']))

    # Extract sprites (append to existing - no clear)
    extracted = 0
    for i, r in enumerate(regions):
        name = f"{auto_prefix}{i+1:03d}"
        sprite = img.crop((r['x'], r['y'], r['x'] + r['w'], r['y'] + r['h']))
        output_path = sprites_dir / f"{name}.png"
        sprite.save(output_path)
        extracted += 1

    print(f"\nExtracted {extracted} sprites to {sprites_dir}")
    print(f"Naming: {auto_prefix}001.png, {auto_prefix}002.png, ...")
    print(f"\nNext: Run 'python3 tools/atlas_tool.py pack' to rebuild atlas")
    return True


# =============================================================================
# MAP - Visual tool to assign texture IDs to sprites
# =============================================================================

def guess_category(width: int, height: int, atlas_y: int) -> str:
    """Guess sprite category based on size and atlas position."""
    # 32x32 in top area = likely biome tiles
    if width == 32 and height == 32 and atlas_y < 100:
        return 'Biomes'

    # Tall sprites (trees, buildings)
    if height > 50:
        if width > 40:
            return 'Buildings'
        return 'Obstacles'  # Trees

    # NPC region - sprite sheets are typically wide (multiple frames)
    if 600 < atlas_y < 900 and width >= 32:
        return 'NPCs'

    # Lower atlas region = items and materials
    if atlas_y > 3500:
        if width <= 15 and height <= 15:
            return 'Materials & Currency'
        if width <= 30 and height <= 30:
            return 'Items'

    # Small square-ish sprites in upper regions = decorations
    if width <= 20 and height <= 20:
        return 'Decorations'

    # Small-medium = likely decorations (flowers, mushrooms, etc)
    if width <= 32 and height <= 32:
        return 'Decorations'

    return 'Unknown'


def build_mapper_state(paths: dict) -> dict:
    """Collect current sprite mapper state from disk."""
    import base64

    sprites_dir = paths['sprites_dir']

    # Load manifest for position info
    manifest_path = sprites_dir / "_manifest.json"
    manifest_sprites = {}
    if manifest_path.exists():
        with open(manifest_path, 'r') as f:
            mdata = json.load(f)
        for s in mdata.get('sprites', []):
            manifest_sprites[s['name']] = s

    sprite_data = []
    for png_file in sorted(sprites_dir.glob("*.png")):
        if png_file.name.startswith('_'):
            continue

        with Image.open(png_file) as img:
            width = img.width
            height = img.height

        with open(png_file, 'rb') as f:
            b64 = base64.b64encode(f.read()).decode('utf-8')

        minfo = manifest_sprites.get(png_file.stem, {})
        atlas_y = minfo.get('y', 0)
        suggested_category = guess_category(width, height, atlas_y)

        sprite_data.append({
            'filename': png_file.stem,
            'dataUrl': f'data:image/png;base64,{b64}',
            'width': width,
            'height': height,
            'atlasY': atlas_y,
            'mapped': not png_file.stem.startswith('sprite_'),
            'suggestedCategory': suggested_category
        })

    expected_ids = collect_expected_texture_ids(paths)
    sprite_names = {s['filename'] for s in sprite_data}
    missing_by_category = {}

    for category, ids in expected_ids.items():
        missing = []
        for item in ids:
            if item['id'] not in sprite_names:
                missing.append(item)
        if missing:
            missing_by_category[category] = missing

    return {
        'sprites': sprite_data,
        'expectedIds': expected_ids,
        'missingIds': missing_by_category,
    }


def apply_sprite_renames(sprites_dir: Path, mappings: dict) -> list:
    """Apply mapper renames using temp files so swaps and cycles are correct."""
    renames = {old: new for old, new in mappings.items() if old != new}
    if not renames:
        return []

    targets = list(renames.values())
    duplicate_targets = sorted({name for name in targets if targets.count(name) > 1})
    if duplicate_targets:
        raise ValueError(f"Multiple sprites map to the same target: {', '.join(duplicate_targets)}")

    source_names = set(renames.keys())
    existing_names = {
        path.stem for path in sprites_dir.glob("*.png")
        if not path.name.startswith('_')
    }
    conflicts = sorted(
        new_name for new_name in targets
        if new_name in existing_names and new_name not in source_names
    )
    if conflicts:
        raise ValueError(f"Target sprite already exists: {', '.join(conflicts)}")

    temp_files = {}
    pid = os.getpid()

    for index, old_name in enumerate(renames.keys()):
        src = sprites_dir / f"{old_name}.png"
        if not src.exists():
            continue

        suffix = 0
        while True:
            tmp_name = f"__atlas_tmp_{pid}_{index}_{suffix}_{old_name}.png"
            tmp = sprites_dir / tmp_name
            if not tmp.exists():
                break
            suffix += 1

        src.rename(tmp)
        temp_files[old_name] = tmp

    renamed = []
    for old_name, new_name in renames.items():
        tmp = temp_files.get(old_name)
        if tmp is None:
            continue

        dst = sprites_dir / f"{new_name}.png"
        tmp.rename(dst)
        renamed.append({'old': old_name, 'new': new_name})
        print(f"  Renamed: {old_name}.png -> {new_name}.png")

    return renamed


def cmd_map(paths: dict):
    """Open visual mapper to assign texture IDs to sprites."""
    sprites_dir = paths['sprites_dir']
    output_html = paths['mapper_session']

    if not sprites_dir.exists():
        print(f"Error: No sprites directory. Run 'extract' first.")
        return False

    # Load sprites
    sprite_files = sorted(sprites_dir.glob("*.png"))
    if not sprite_files:
        print(f"Error: No sprites found in {sprites_dir}")
        return False

    mapper_state = build_mapper_state(paths)
    sprite_data = mapper_state['sprites']
    expected_ids = mapper_state['expectedIds']
    missing_by_category = mapper_state['missingIds']
    sprite_names = {s['filename'] for s in sprite_data}
    mapped_count = sum(
        1
        for ids in expected_ids.values()
        for item in ids
        if item['id'] in sprite_names
    )

    total_ids = sum(len(v) for v in expected_ids.values())
    total_missing = sum(len(v) for v in missing_by_category.values())

    print(f"\nLoaded {len(sprite_data)} sprites")
    print(f"Found {total_ids} expected texture IDs from JSON files")
    print(f"  Mapped: {mapped_count} | Missing sprites: {total_missing}")

    if missing_by_category:
        print(f"\n⚠️  TEXTURES NEEDED ({total_missing} total):")
        for category, missing in missing_by_category.items():
            print(f"\n  {category}:")
            for item in missing:
                print(f"    - {item['id']} ({item['name']})")

    # Generate the mapper HTML
    html = generate_mapper_html(sprite_data, expected_ids, paths, missing_by_category)

    with open(output_html, 'w', encoding='utf-8') as f:
        f.write(html)

    print(f"\nStarting mapper server on http://localhost:8000")
    print("\nWorkflow:")
    print("  1. Click a sprite on the left")
    print("  2. Click a texture ID on the right to assign")
    print("  3. Repeat for all unmapped sprites")
    print("  4. Click 'Save Mappings'")
    print("\nPress Ctrl+C to stop server when done")

    # Set up HTTP server
    MapperRequestHandler.sprites_dir = sprites_dir
    MapperRequestHandler.paths = paths
    original_dir = os.getcwd()
    os.chdir(output_html.parent)

    server = HTTPServer(('localhost', 8000), MapperRequestHandler)
    webbrowser.open(f"http://localhost:8000/{output_html.name}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped")
    finally:
        os.chdir(original_dir)

    return True


def collect_expected_texture_ids(paths: dict) -> dict:
    """Collect all expected texture IDs by scanning JSON files in res/data/."""
    categories = {}
    data_dir = paths['project_root'] / "res" / "data"

    # Process resource catalogs (items, weapons, equipment, materials, currency)
    resource_catalogs = (
        "items.json",
        "weapons.json",
        "equipment.json",
        "materials.json",
        "currency.json",
    )
    items_found = []
    materials_found = []
    currency_found = []
    seen_tex_ids = set()
    for catalog_name in resource_catalogs:
        resources_path = data_dir / catalog_name
        if not resources_path.exists():
            continue
        try:
            with open(resources_path, 'r') as f:
                data = json.load(f)
            # Separate into categories based on the 'category' field
            for resource in data.get('resources', []):
                for texture_key in ('textureId', 'worldTextureId', 'iconTextureId'):
                    tex_id = resource.get(texture_key)
                    if tex_id and tex_id not in seen_tex_ids:
                        seen_tex_ids.add(tex_id)
                        entry = {
                            'id': tex_id,
                            'name': resource.get('name', tex_id),
                            'source': catalog_name
                        }
                        cat = resource.get('category', '')
                        if cat == 'Item':
                            items_found.append(entry)
                        elif cat == 'Material':
                            materials_found.append(entry)
                        elif cat == 'Currency':
                            currency_found.append(entry)
                        else:
                            items_found.append(entry)
        except Exception:
            pass

    if items_found:
        categories['Items'] = items_found
    if materials_found:
        categories['Materials'] = materials_found
    if currency_found:
        categories['Currency'] = currency_found

    # Process races.json (NPCs)
    races_path = data_dir / "races.json"
    if races_path.exists():
        try:
            with open(races_path, 'r') as f:
                data = json.load(f)
            found = scan_for_texture_ids(data, 'races.json')
            if found:
                categories['Races'] = found
        except Exception:
            pass

    # Process monster_types.json
    monster_path = data_dir / "monster_types.json"
    if monster_path.exists():
        try:
            with open(monster_path, 'r') as f:
                data = json.load(f)
            found = scan_for_texture_ids(data, 'monster_types.json')
            if found:
                categories['Monsters'] = found
        except Exception:
            pass

    # Process species.json (Animals)
    species_path = data_dir / "species.json"
    if species_path.exists():
        try:
            with open(species_path, 'r') as f:
                data = json.load(f)
            found = scan_for_texture_ids(data, 'species.json')
            if found:
                categories['Animals'] = found
        except Exception:
            pass

    # Process world_objects.json - break into subcategories
    world_path = data_dir / "world_objects.json"
    seasons = ['spring', 'summer', 'fall', 'winter']
    if world_path.exists():
        try:
            with open(world_path, 'r') as f:
                data = json.load(f)

            # Extract each subcategory separately
            for subcat in ['biomes', 'obstacles', 'decorations', 'buildings']:
                if subcat in data:
                    found = []
                    seen_tex_ids = set()
                    for key, obj in data[subcat].items():
                        tex_id = obj.get('textureId')
                        if tex_id and tex_id not in seen_tex_ids:
                            seen_tex_ids.add(tex_id)
                            is_seasonal = obj.get('seasonal', False)
                            # Always include base texture ID
                            found.append({
                                'id': tex_id,
                                'name': obj.get('name', key),
                                'source': 'world_objects.json'
                            })
                            if is_seasonal:
                                # Also generate all 4 seasonal variants
                                for season in seasons:
                                    found.append({
                                        'id': f"{season}_{tex_id}",
                                        'name': f"{obj.get('name', key)} ({season.title()})",
                                        'source': 'world_objects.json',
                                        'seasonal': True,
                                        'baseName': obj.get('name', key)
                                    })
                    if found:
                        categories[subcat.title()] = found
        except Exception:
            pass

    return categories


def scan_for_texture_ids(data, source: str, parent_name: str = None,
                         _seen: set = None) -> list:
    """Recursively scan data structure for unique texture ID fields."""
    if _seen is None:
        _seen = set()
    found = []
    texture_keys = ('textureId', 'worldTextureId', 'iconTextureId')

    if isinstance(data, dict):
        for key in texture_keys:
            if key in data and data[key] and data[key] not in _seen:
                _seen.add(data[key])
                name = data.get('name') or data.get('id') or parent_name or data[key]
                found.append({
                    'id': data[key],
                    'name': name,
                    'source': source
                })

        for k, v in data.items():
            found.extend(scan_for_texture_ids(v, source, k, _seen))

    elif isinstance(data, list):
        for item in data:
            found.extend(scan_for_texture_ids(item, source, parent_name, _seen))

    return found


def generate_mapper_html(sprites: list, expected_ids: dict, paths: dict, missing_ids: dict = None) -> str:
    """Generate the visual mapper HTML."""
    sprites_json = json.dumps(sprites)
    ids_json = json.dumps(expected_ids)
    missing_json = json.dumps(missing_ids or {})

    return f'''<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Atlas Mapper - VoidLight-Framework</title>
    <style>
        * {{ box-sizing: border-box; margin: 0; padding: 0; }}
        body {{ font-family: 'Segoe UI', sans-serif; background: #1a1a2e; color: #eee; height: 100vh; overflow: hidden; }}

        .header {{ background: #16213e; padding: 12px 20px; display: flex; align-items: center; gap: 20px; border-bottom: 2px solid #0f3460; }}
        .header h1 {{ font-size: 1.3em; color: #0f9; }}
        .header .stats {{ color: #888; font-size: 0.9em; }}
        .header .stats span {{ color: #0f9; font-weight: bold; }}

        .btn {{ padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; }}
        .btn-primary {{ background: #0f9; color: #000; }}
        .btn-primary:hover {{ background: #0da; }}
        .btn-danger {{ background: #e94560; color: #fff; }}

        .container {{ display: flex; height: calc(100vh - 56px); }}

        .sprites-panel {{ flex: 1; overflow-y: auto; padding: 15px; background: #0f0f1a; display: flex; flex-direction: column; }}
        .sprites-filter {{ padding: 8px; margin-bottom: 8px; display: flex; gap: 8px; flex-shrink: 0; }}
        .sprites-filter input {{ flex: 1; padding: 8px; background: #1a1a2e; border: 1px solid #333; color: #eee; border-radius: 4px; }}
        .sprites-filter input:focus {{ border-color: #0f9; outline: none; }}
        .sprites-filter select {{ padding: 8px; background: #1a1a2e; border: 1px solid #333; color: #eee; border-radius: 4px; }}
        .sprites-grid-wrapper {{ flex: 1; overflow-y: auto; }}
        .sprites-grid {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(100px, 1fr)); gap: 8px; }}

        .sprite-card {{ background: #1a1a2e; border: 2px solid #333; border-radius: 6px; padding: 6px; cursor: pointer; text-align: center; }}
        .sprite-card:hover {{ border-color: #0f9; }}
        .sprite-card.selected {{ border-color: #0f9; box-shadow: 0 0 10px rgba(0,255,136,0.4); }}
        .sprite-card.mapped {{ border-color: #4ecdc4; background: #1a2a2e; }}
        .sprite-card img {{ max-width: 80px; max-height: 60px; image-rendering: pixelated; }}
        .sprite-card .name {{ font-size: 10px; color: #888; margin-top: 4px; word-break: break-all; }}
        .sprite-card.mapped .name {{ color: #4ecdc4; }}
        .sprite-card .guess {{ font-size: 9px; color: #666; background: #252540; padding: 2px 5px; border-radius: 3px; margin-top: 3px; display: inline-block; }}

        .ids-panel {{ width: 320px; background: #16213e; border-left: 2px solid #0f3460; display: flex; flex-direction: column; }}
        .ids-panel h3 {{ padding: 10px; background: #0f3460; font-size: 0.9em; }}

        .id-search {{ padding: 10px; border-bottom: 1px solid #333; }}
        .id-search input {{ width: 100%; padding: 8px; background: #1a1a2e; border: 1px solid #333; color: #eee; border-radius: 4px; }}
        .id-search input:focus {{ border-color: #0f9; outline: none; }}

        .id-list {{ flex: 1; overflow-y: auto; }}
        .id-category {{ border-bottom: 1px solid #333; }}
        .id-category-header {{ padding: 8px 10px; background: #0f3460; font-size: 0.8em; color: #888; cursor: pointer; }}
        .id-category-header:hover {{ background: #1a4a7a; }}
        .id-item {{ padding: 8px 15px; cursor: pointer; font-size: 0.85em; border-left: 3px solid transparent; }}
        .id-item:hover {{ background: #252540; border-left-color: #0f9; }}
        .id-item.used {{ opacity: 0.4; text-decoration: line-through; }}
        .id-item.missing {{ background: #2a1a1a; border-left-color: #e94560; }}
        .id-item.missing:hover {{ background: #3a2525; }}
        .id-item .id-name {{ color: #0f9; }}
        .id-item .id-desc {{ color: #666; font-size: 0.8em; }}

        .selected-info {{ padding: 15px; background: #0a0a15; border-bottom: 1px solid #333; }}
        .selected-info img {{ max-width: 100%; max-height: 100px; image-rendering: pixelated; }}
        .selected-info .label {{ color: #888; font-size: 0.8em; margin-top: 8px; }}
        .selected-info .value {{ color: #0f9; font-weight: bold; }}

        .missing-section {{ background: #3a1a1a; border-bottom: 2px solid #e94560; }}
        .missing-header {{ padding: 10px; background: #4a1a1a; color: #e94560; font-weight: bold; cursor: pointer; display: flex; justify-content: space-between; align-items: center; }}
        .missing-header:hover {{ background: #5a2a2a; }}
        .missing-content {{ max-height: 200px; overflow-y: auto; }}
        .missing-content.collapsed {{ display: none; }}
        .missing-category {{ padding: 5px 10px; background: #2a1515; color: #f90; font-size: 0.8em; }}
        .missing-item {{ padding: 6px 15px; font-size: 0.85em; border-left: 3px solid #e94560; cursor: pointer; }}
        .missing-item:hover {{ background: #4a2020; }}
        .missing-item .id-name {{ color: #e94560; }}
        .missing-item .id-desc {{ color: #888; font-size: 0.8em; }}
        .copy-btn {{ cursor: pointer; opacity: 0.6; margin-left: 5px; }}
        .copy-btn:hover {{ opacity: 1; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>Atlas Mapper</h1>
        <div class="stats">
            Mapped: <span id="mappedCount">0</span> / <span id="totalCount">0</span>
        </div>
        <button class="btn btn-primary" onclick="saveMappings()">Save Mappings</button>
        <button class="btn btn-danger" onclick="clearMappings()">Clear All</button>
    </div>

    <div class="container">
        <div class="sprites-panel">
            <div class="sprites-filter">
                <input type="text" id="spriteSearch" placeholder="Search sprites..." oninput="render()">
                <select id="spriteFilter" onchange="render()">
                    <option value="all">All</option>
                    <option value="unmapped">Unmapped</option>
                    <option value="mapped">Mapped</option>
                </select>
            </div>
            <div class="sprites-grid-wrapper">
                <div class="sprites-grid" id="spritesGrid"></div>
            </div>
        </div>

        <div class="ids-panel">
            <div class="selected-info" id="selectedInfo">
                <div style="color: #666; text-align: center;">Select a sprite</div>
            </div>

            <div class="id-search">
                <input type="text" id="idSearch" placeholder="Filter texture IDs..." oninput="render()">
            </div>

            <div class="missing-section" id="missingSection"></div>
            <div class="id-list" id="idList"></div>
        </div>
    </div>

    <script>
        let sprites = {sprites_json};
        let expectedIds = {ids_json};
        let missingIds = {missing_json};

        let selectedSprite = null;
        let mappings = {{}};  // filename -> textureId

        function rebuildMappingsFromSprites() {{
            mappings = {{}};
            sprites.forEach(s => {{
                if (s.mapped) mappings[s.filename] = s.filename;
            }});
        }}

        rebuildMappingsFromSprites();

        function render() {{
            // Save scroll positions before re-rendering
            const spritesPanel = document.querySelector('.sprites-panel');
            const idsPanel = document.querySelector('.ids-panel');
            const missingContent = document.getElementById('missingContent');
            const spritesScroll = spritesPanel ? spritesPanel.scrollTop : 0;
            const idsScroll = idsPanel ? idsPanel.scrollTop : 0;
            const missingScroll = missingContent ? missingContent.scrollTop : 0;

            renderMissing();
            renderSprites();
            renderIdList();
            updateStats();

            // Restore scroll positions after re-rendering
            if (spritesPanel) spritesPanel.scrollTop = spritesScroll;
            if (idsPanel) idsPanel.scrollTop = idsScroll;
            const newMissingContent = document.getElementById('missingContent');
            if (newMissingContent) newMissingContent.scrollTop = missingScroll;
        }}

        function getMissingIds() {{
            // Dynamically calculate missing IDs based on current mappings
            const mappedTextureIds = new Set(Object.values(mappings));
            const spriteFilenames = new Set(sprites.map(s => s.filename));

            const missing = {{}};
            Object.entries(expectedIds).forEach(([category, items]) => {{
                const categoryMissing = items.filter(item => {{
                    // Not missing if: sprite exists with this name OR it's been mapped to
                    return !spriteFilenames.has(item.id) && !mappedTextureIds.has(item.id);
                }});
                if (categoryMissing.length > 0) {{
                    missing[category] = categoryMissing;
                }}
            }});
            return missing;
        }}

        function renderMissing() {{
            const section = document.getElementById('missingSection');
            const currentMissing = getMissingIds();
            const totalMissing = Object.values(currentMissing).flat().length;

            if (totalMissing === 0) {{
                section.style.display = 'none';
                return;
            }}

            section.style.display = 'block';
            let html = `
                <div class="missing-header" onclick="toggleMissing()">
                    <span>⚠️ NEEDS SPRITES (${{totalMissing}})</span>
                    <span id="missingToggle">▼</span>
                </div>
                <div class="missing-content" id="missingContent">
            `;

            Object.entries(currentMissing).forEach(([category, items]) => {{
                if (items.length === 0) return;
                html += `<div class="missing-category">${{category}} (${{items.length}})</div>`;
                items.forEach(item => {{
                    html += `
                        <div class="missing-item" onclick="assignId('${{item.id}}')" title="Click to assign '${{item.id}}' to selected sprite">
                            <div class="id-name">${{item.id}} <span class="copy-btn" onclick="event.stopPropagation(); copyTextureId('${{item.id}}')" title="Copy ID">📋</span></div>
                            <div class="id-desc">${{item.name}} (from ${{item.source}})</div>
                        </div>
                    `;
                }});
            }});

            html += '</div>';
            section.innerHTML = html;
        }}

        function toggleMissing() {{
            const content = document.getElementById('missingContent');
            const toggle = document.getElementById('missingToggle');
            content.classList.toggle('collapsed');
            toggle.textContent = content.classList.contains('collapsed') ? '▶' : '▼';
        }}

        function copyTextureId(id) {{
            navigator.clipboard.writeText(id).then(() => {{
                alert(`Copied "${{id}}" to clipboard.\\n\\nCreate a sprite file named: ${{id}}.png`);
            }});
        }}

        function renderSprites() {{
            const grid = document.getElementById('spritesGrid');
            grid.innerHTML = '';

            const search = (document.getElementById('spriteSearch')?.value || '').toLowerCase();
            const filter = document.getElementById('spriteFilter')?.value || 'all';

            // Sort: mapped sprites first, then unmapped
            const sorted = [...sprites].sort((a, b) => {{
                const aM = mappings[a.filename] ? 0 : 1;
                const bM = mappings[b.filename] ? 0 : 1;
                return aM - bM;
            }});

            sorted.forEach(sprite => {{
                const isMapped = !!mappings[sprite.filename];
                if (filter === 'unmapped' && isMapped) return;
                if (filter === 'mapped' && !isMapped) return;

                const displayName = mappings[sprite.filename] || sprite.filename;
                if (search && !displayName.toLowerCase().includes(search) &&
                    !sprite.filename.toLowerCase().includes(search)) return;

                const card = document.createElement('div');
                card.className = 'sprite-card' +
                    (selectedSprite === sprite.filename ? ' selected' : '') +
                    (isMapped ? ' mapped' : '');

                const guessHtml = !isMapped && sprite.suggestedCategory ?
                    `<div class="guess">${{sprite.suggestedCategory}}</div>` : '';
                card.innerHTML = `
                    <img src="${{sprite.dataUrl}}" alt="${{sprite.filename}}">
                    <div class="name">${{displayName}}</div>
                    ${{guessHtml}}
                `;
                card.onclick = () => selectSprite(sprite.filename);
                grid.appendChild(card);
            }});
        }}

        function renderIdList() {{
            const list = document.getElementById('idList');
            list.innerHTML = '';

            const usedIds = new Set(Object.values(mappings));
            const idSearch = (document.getElementById('idSearch')?.value || '').toLowerCase();

            // Build set of missing IDs dynamically
            const currentMissing = getMissingIds();
            const missingIdSet = new Set(Object.values(currentMissing).flat().map(x => x.id));

            // Get suggested category for selected sprite
            const selectedSpriteData = sprites.find(s => s.filename === selectedSprite);
            const suggestedCat = selectedSpriteData?.suggestedCategory;

            // Categories from data files
            const allCategories = {{}};
            Object.entries(expectedIds).forEach(([cat, ids]) => {{
                allCategories[cat] = [...ids];
            }});

            // Helper: check if item matches ID search
            const matchesSearch = (item) => !idSearch ||
                item.id.toLowerCase().includes(idSearch) ||
                (item.name && item.name.toLowerCase().includes(idSearch));

            // Show suggested matches first if we have a suggestion
            if (suggestedCat && suggestedCat !== 'Unknown' && allCategories[suggestedCat]) {{
                const suggestedIds = allCategories[suggestedCat].filter(x => !usedIds.has(x.id) && matchesSearch(x));
                if (suggestedIds.length > 0) {{
                    const suggestCat = document.createElement('div');
                    suggestCat.className = 'id-category';
                    suggestCat.innerHTML = `<div class="id-category-header" style="background:#4a3a00;color:#f90;">Suggested: ${{suggestedCat}} (${{suggestedIds.length}} available)</div>`;

                    suggestedIds.forEach(item => {{
                        const div = document.createElement('div');
                        const isMissing = missingIdSet.has(item.id);
                        div.className = 'id-item' + (isMissing ? ' missing' : '');
                        div.style.borderLeftColor = '#f90';
                        const missingBadge = isMissing ? ' <span style="color:#fff;background:#e94560;padding:1px 4px;border-radius:3px;font-size:9px;">NO SPRITE</span>' : '';
                        const hasSpriteBadge = !isMissing ? ' <span style="color:#fff;background:#2a6a2a;padding:1px 4px;border-radius:3px;font-size:9px;">✓</span>' : '';
                        div.innerHTML = `
                            <div class="id-name">${{item.id}}${{missingBadge}}${{hasSpriteBadge}}</div>
                            <div class="id-desc">${{item.name}}</div>
                        `;
                        div.onclick = () => assignId(item.id);
                        suggestCat.appendChild(div);
                    }});

                    list.appendChild(suggestCat);
                }}
            }}

            const categoryOrder = ['Items', 'Materials', 'Currency', 'Races', 'Monsters', 'Animals', 'Biomes', 'Obstacles', 'Decorations', 'Buildings'];

            categoryOrder.forEach(categoryName => {{
                const ids = allCategories[categoryName];
                if (!ids || ids.length === 0) return;

                const filteredIds = idSearch ? ids.filter(matchesSearch) : ids;
                if (filteredIds.length === 0) return;

                const cat = document.createElement('div');
                cat.className = 'id-category';
                const missingCount = filteredIds.filter(x => missingIdSet.has(x.id)).length;
                const missingIndicator = missingCount > 0 ? ` <span style="color:#e94560;">⚠️ ${{missingCount}} need sprites</span>` : '';
                cat.innerHTML = `<div class="id-category-header">${{categoryName}} (${{filteredIds.length}})${{missingIndicator}}</div>`;

                filteredIds.forEach(item => {{
                    const div = document.createElement('div');
                    const isMissing = missingIdSet.has(item.id);
                    div.className = 'id-item' + (usedIds.has(item.id) ? ' used' : '') + (isMissing ? ' missing' : '');
                    const missingBadge = isMissing ? ' <span style="color:#fff;background:#e94560;padding:1px 4px;border-radius:3px;font-size:9px;">NO SPRITE</span>' : '';
                    const hasSpriteBadge = !isMissing ? ' <span style="color:#fff;background:#2a6a2a;padding:1px 4px;border-radius:3px;font-size:9px;">✓</span>' : '';
                    div.innerHTML = `
                        <div class="id-name">${{item.id}}${{missingBadge}}${{hasSpriteBadge}}</div>
                        <div class="id-desc">${{item.name}}</div>
                    `;
                    div.onclick = () => assignId(item.id);
                    cat.appendChild(div);
                }});

                list.appendChild(cat);
            }});
        }}

        function selectSprite(filename) {{
            selectedSprite = filename;
            const sprite = sprites.find(s => s.filename === filename);

            const guessLine = sprite.suggestedCategory ?
                `<div class="label">Suggested: <span style="color:#f90">${{sprite.suggestedCategory}}</span></div>` : '';

            document.getElementById('selectedInfo').innerHTML = `
                <img src="${{sprite.dataUrl}}">
                <div class="label">File: ${{sprite.filename}}.png</div>
                <div class="label">Size: ${{sprite.width}}x${{sprite.height}}</div>
                ${{guessLine}}
                <div class="label">Mapped to:</div>
                <div class="value">${{mappings[filename] || '(unmapped)'}}</div>
            `;

            render();
        }}

        function assignId(textureId) {{
            if (!selectedSprite) {{
                alert('Select a sprite first');
                return;
            }}

            // Remove this ID from any other sprite
            Object.keys(mappings).forEach(k => {{
                if (mappings[k] === textureId) delete mappings[k];
            }});

            mappings[selectedSprite] = textureId;

            // Auto-advance to next unmapped sprite
            const currentIdx = sprites.findIndex(s => s.filename === selectedSprite);
            for (let i = 1; i < sprites.length; i++) {{
                const nextIdx = (currentIdx + i) % sprites.length;
                if (!mappings[sprites[nextIdx].filename]) {{
                    selectSprite(sprites[nextIdx].filename);
                    return;
                }}
            }}

            render();
        }}

        function updateStats() {{
            const total = sprites.length;
            const mapped = Object.keys(mappings).length;
            document.getElementById('totalCount').textContent = total;
            document.getElementById('mappedCount').textContent = mapped;
        }}

        function clearMappings() {{
            if (confirm('Clear all mappings?')) {{
                mappings = {{}};
                render();
            }}
        }}

        function loadMapperState(previousSelection) {{
            return fetch('/mapper-state', {{cache: 'no-store'}})
                .then(response => {{
                    if (!response.ok) {{
                        return response.json().then(data => {{
                            throw new Error(data.error || 'Failed to load mapper state');
                        }});
                    }}
                    return response.json();
                }})
                .then(state => {{
                    sprites = state.sprites || [];
                    expectedIds = state.expectedIds || {{}};
                    missingIds = state.missingIds || {{}};
                    rebuildMappingsFromSprites();

                    selectedSprite = sprites.some(s => s.filename === previousSelection)
                        ? previousSelection
                        : null;

                    if (selectedSprite) {{
                        selectSprite(selectedSprite);
                    }} else {{
                        document.getElementById('selectedInfo').innerHTML =
                            '<div style="color: #666; text-align: center;">Select a sprite</div>';
                        render();
                    }}
                }});
        }}

        function saveMappings() {{
            const renames = Object.entries(mappings).filter(([old, newN]) => old !== newN);

            if (renames.length === 0) {{
                alert('No renames needed - all sprites already have correct names.');
                return;
            }}

            if (!confirm(`Rename ${{renames.length}} sprite(s)?\\n\\n${{renames.map(([o,n]) => o + ' → ' + n).join('\\n')}}`)) {{
                return;
            }}

            fetch('/save-mappings', {{
                method: 'POST',
                headers: {{'Content-Type': 'application/json'}},
                body: JSON.stringify(mappings, null, 2)
            }})
            .then(response => {{
                if (!response.ok) {{
                    return response.json().then(data => {{
                        throw new Error(data.error || 'Save failed');
                    }});
                }}
                return response.json();
            }})
            .then(data => {{
                if (data.renamed && data.renamed.length > 0) {{
                    const previousSelection = selectedSprite;
                    return loadMapperState(previousSelection).then(() => {{
                        alert(`Renamed ${{data.renamed.length}} sprites. Ready for next batch or pack.`);
                    }});
                }} else {{
                    alert('No renames were applied.');
                }}
            }})
            .catch(err => alert('Error: ' + err));
        }}

        // Keyboard navigation (skip when typing in inputs)
        document.addEventListener('keydown', e => {{
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
            if (!selectedSprite) return;
            const idx = sprites.findIndex(s => s.filename === selectedSprite);

            if (e.key === 'ArrowRight' || e.key === 'ArrowDown') {{
                e.preventDefault();
                selectSprite(sprites[(idx + 1) % sprites.length].filename);
            }} else if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') {{
                e.preventDefault();
                selectSprite(sprites[(idx - 1 + sprites.length) % sprites.length].filename);
            }}
        }});

        render();
    </script>
</body>
</html>'''


# =============================================================================
# PACK - Pack sprites into atlas and update atlas.json
# =============================================================================

def cmd_pack(paths: dict):
    """Pack sprites into atlas.png and update atlas.json."""
    sprites_dir = paths['sprites_dir']
    atlas_png = paths['atlas_png']
    atlas_json_path = paths['atlas_json']

    if not sprites_dir.exists():
        print(f"Error: Sprites directory not found: {sprites_dir}")
        return False

    # Collect all PNG files (excluding manifest and temp files)
    sprite_files = [f for f in sorted(sprites_dir.glob("*.png")) if not f.name.startswith('_')]

    if not sprite_files:
        print("No sprites found")
        return False

    existing_region_count = 0
    if atlas_json_path.exists():
        try:
            with atlas_json_path.open('r', encoding='utf-8') as f:
                atlas_data = json.load(f)
            existing_region_count = len(atlas_data.get('regions', {}))
        except (OSError, json.JSONDecodeError) as ex:
            print(f"Warning: Could not read existing atlas.json for pack sanity check: {ex}")

    if existing_region_count > 0 and len(sprite_files) < max(1, existing_region_count // 2):
        print(
            "Warning: res/sprites contains far fewer sprites than the current atlas "
            f"({len(sprite_files)} PNGs vs {existing_region_count} atlas regions). "
            "If you intend to update the existing atlas, run extract first; pack will "
            "replace atlas.png and atlas.json with only the sprites currently present."
        )

    print(f"Loading {len(sprite_files)} sprites...")

    # Load sprites
    sprites = []
    for png_file in sprite_files:
        img = Image.open(png_file).convert('RGBA')
        sprites.append({
            'id': png_file.stem,
            'image': img,
            'width': img.width,
            'height': img.height
        })

    # =========================================================================
    # Smart duplicate detection - prioritize named sprites over unnamed
    # =========================================================================
    hash_to_sprite = {}
    duplicates = []

    # First pass: add named sprites (priority)
    for sprite in sprites:
        if not sprite['id'].startswith('sprite_'):
            img_hash = get_image_hash(sprite['image'])
            hash_to_sprite[img_hash] = sprite

    # Second pass: add unnamed only if unique
    for sprite in sprites:
        if sprite['id'].startswith('sprite_'):
            img_hash = get_image_hash(sprite['image'])
            if img_hash in hash_to_sprite:
                # Duplicate of a named sprite - skip it
                duplicates.append((sprite['id'], hash_to_sprite[img_hash]['id']))
            else:
                hash_to_sprite[img_hash] = sprite

    # Use deduplicated sprites for packing
    sprites = list(hash_to_sprite.values())

    if duplicates:
        # Separate duplicates into two categories
        named_dupes = [(u, k) for u, k in duplicates if not k.startswith('sprite_')]
        unnamed_dupes = [(u, k) for u, k in duplicates if k.startswith('sprite_')]

        print(f"\nDuplicate detection: found {len(duplicates)} duplicates")

        if named_dupes:
            print(f"\n  {len(named_dupes)} unnamed sprites match named sprites (keeping named):")
            for unnamed, named in named_dupes[:10]:
                print(f"    {unnamed} → {named}")
            if len(named_dupes) > 10:
                print(f"    ... and {len(named_dupes) - 10} more")

        if unnamed_dupes:
            print(f"\n  {len(unnamed_dupes)} duplicate unnamed sprites (keeping first):")
            for dup, kept in unnamed_dupes[:5]:
                print(f"    {dup} → {kept}")
            if len(unnamed_dupes) > 5:
                print(f"    ... and {len(unnamed_dupes) - 5} more")

        print(f"\nPacking {len(sprites)} unique sprites (removed {len(duplicates)} duplicates)")
    else:
        print(f"No duplicates found, packing {len(sprites)} sprites")

    # Sort by height for efficient packing
    sprites.sort(key=lambda s: s['height'], reverse=True)

    # Pack into rows
    max_width = 512
    padding = 1
    rows = []
    current_row = {'sprites': [], 'width': 0, 'height': 0}

    for sprite in sprites:
        if current_row['width'] + sprite['width'] + padding > max_width:
            if current_row['sprites']:
                rows.append(current_row)
            current_row = {'sprites': [], 'width': 0, 'height': 0}

        current_row['sprites'].append(sprite)
        current_row['width'] += sprite['width'] + padding
        current_row['height'] = max(current_row['height'], sprite['height'])

    if current_row['sprites']:
        rows.append(current_row)

    # Calculate atlas size
    atlas_width = max_width
    atlas_height = sum(row['height'] + padding for row in rows)

    def next_power_of_2(n):
        p = 1
        while p < n:
            p *= 2
        return p

    atlas_height = next_power_of_2(atlas_height)

    # Create atlas
    atlas = Image.new('RGBA', (atlas_width, atlas_height), (0, 0, 0, 0))
    regions = {}
    y_offset = 0

    for row in rows:
        x_offset = 0
        for sprite in row['sprites']:
            atlas.paste(sprite['image'], (x_offset, y_offset))
            regions[sprite['id']] = {
                'x': x_offset,
                'y': y_offset,
                'w': sprite['width'],
                'h': sprite['height']
            }
            x_offset += sprite['width'] + padding
        y_offset += row['height'] + padding

    # Save atlas.png
    atlas.save(atlas_png)
    print(f"Saved: {atlas_png} ({atlas_width}x{atlas_height})")

    # Save atlas.json
    atlas_data = {'atlasId': 'atlas', 'regions': regions}
    with open(atlas_json_path, 'w') as f:
        json.dump(atlas_data, f, indent=2)
    print(f"Saved: {atlas_json_path} ({len(regions)} regions)")

    print("\n  Atlas coordinates stored in atlas.json only (data files unchanged)")

    # Clean up sprite files after successful pack
    current_sprites = [f for f in sprites_dir.glob("*.png") if not f.name.startswith('_')]
    if current_sprites:
        print(f"\nCleaning up {len(current_sprites)} sprite files from {sprites_dir}...")
        for sprite_file in current_sprites:
            sprite_file.unlink()
        print("  Sprites removed (now in atlas)")

    return True


# =============================================================================
# LIST - Show current sprites
# =============================================================================

def cmd_list(paths: dict):
    """List sprites in res/sprites/."""
    sprites_dir = paths['sprites_dir']

    if not sprites_dir.exists():
        print(f"Sprites directory not found: {sprites_dir}")
        print("Run 'extract' first")
        return

    sprite_files = sorted([f for f in sprites_dir.glob("*.png") if not f.name.startswith('_')])

    named = [f for f in sprite_files if not f.stem.startswith('sprite_')]
    unnamed = [f for f in sprite_files if f.stem.startswith('sprite_')]

    print(f"\nSprites in {sprites_dir}:")
    print(f"  Total: {len(sprite_files)}")
    print(f"  Named: {len(named)}")
    print(f"  Unnamed: {len(unnamed)}")

    if named:
        print(f"\nNamed sprites:")
        for f in named[:20]:
            img = Image.open(f)
            print(f"  {f.stem} ({img.width}x{img.height})")
        if len(named) > 20:
            print(f"  ... and {len(named) - 20} more")

    if unnamed:
        print(f"\nUnnamed sprites (need mapping):")
        for f in unnamed[:10]:
            img = Image.open(f)
            print(f"  {f.stem} ({img.width}x{img.height})")
        if len(unnamed) > 10:
            print(f"  ... and {len(unnamed) - 10} more")


# =============================================================================
# MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Atlas Tool for VoidLight-Framework',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Workflow:
  1. extract  - Extract sprites from atlas.png
  2. map      - Assign texture IDs to sprites (visual tool)
  3. pack     - Pack sprites and update atlas.json

Example:
  python3 tools/atlas_tool.py extract
  python3 tools/atlas_tool.py map
  # (assign IDs in browser, save to apply renames)
  python3 tools/atlas_tool.py pack
''')

    subparsers = parser.add_subparsers(dest='command', help='Commands')
    extract_parser = subparsers.add_parser('extract', help='Extract sprites from atlas.png')
    extract_parser.add_argument('--max-size', type=int, default=64,
                                help='Split regions larger than this (default: 64)')
    extract_from_parser = subparsers.add_parser('extract-from',
        help='Extract sprites from any source image into res/sprites/')
    extract_from_parser.add_argument('source', help='Source image to extract from')
    extract_from_parser.add_argument('--output', '-o',
        help='Output directory (default: res/sprites/)')
    extract_from_parser.add_argument('--prefix', '-p',
        help='Override auto-prefix (default: source filename)')
    extract_from_parser.add_argument('--max-size', type=int, default=64,
        help='Split regions larger than this (default: 64)')
    subparsers.add_parser('map', help='Visual tool to assign texture IDs')
    subparsers.add_parser('pack', help='Pack sprites into atlas.png and update atlas.json')
    subparsers.add_parser('list', help='List current sprites')

    args = parser.parse_args()
    paths = get_paths()

    if args.command == 'extract':
        cmd_extract(paths, max_size=args.max_size)
    elif args.command == 'extract-from':
        cmd_extract_from(paths, args.source, args.output, args.prefix, args.max_size)
    elif args.command == 'map':
        cmd_map(paths)
    elif args.command == 'pack':
        cmd_pack(paths)
    elif args.command == 'list':
        cmd_list(paths)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
