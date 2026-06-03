#!/bin/bash
# Copyright (c) 2025 Hammer Forged Games
# All rights reserved.
# Licensed under the MIT License - see LICENSE file for details

# Generate seasonal texture variants using ImageMagick
# Creates spring_, summer_, fall_, winter_ prefixed versions of tile textures
#
# Usage: extract sprites first with atlas_tool.py, run this script, then re-pack.
# See tools/README.md for the full workflow.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SPRITES_DIR="$PROJECT_ROOT/res/sprites"

echo "Generating seasonal texture variants..."
echo "Source directory: $SPRITES_DIR"

# Textures that get seasonal variants
TILE_TEXTURES=(
    "biome_celestial"
    "biome_default"
    "biome_desert"
    "biome_forest"
    "biome_haunted"
    "biome_mountain"
    "biome_ocean"
    "biome_plains"
    "biome_swamp"
    "building_cityhall"
    "building_house"
    "building_hut"
    "building_large"
    "bush"
    "obstacle_grass"
    "obstacle_tree"
)

# Track generated files
generated=0
skipped=0

for texture in "${TILE_TEXTURES[@]}"; do
    src="$SPRITES_DIR/${texture}.png"

    if [[ ! -f "$src" ]]; then
        echo "WARNING: Source texture not found: $src"
        skipped=$((skipped + 1))
        continue
    fi

    echo "Processing: $texture"

    # Spring - vibrant, slight green boost, bright
    spring_out="$SPRITES_DIR/spring_${texture}.png"
    convert "$src" -modulate 105,115,100 -level 0%,100%,1.02 "$spring_out"
    generated=$((generated + 1))

    # Summer - warm golden tones, high saturation
    summer_out="$SPRITES_DIR/summer_${texture}.png"
    convert "$src" -modulate 100,125,100 -colorize 3,3,0 "$summer_out"
    generated=$((generated + 1))

    # Fall - orange/brown hue shift (hue rotate toward warm colors)
    fall_out="$SPRITES_DIR/fall_${texture}.png"
    convert "$src" -modulate 95,90,115 "$fall_out"
    generated=$((generated + 1))

    # Winter - desaturated, blue tint, snow overlay effect.
    # Keep the base plains tile pixel-local so its opposite edges remain tileable.
    winter_out="$SPRITES_DIR/winter_${texture}.png"
    if [[ "$texture" == "biome_plains" ]]; then
        convert "$src" -modulate 90,20,100 -colorspace Gray -colorize 0,0,8 "$winter_out"
    else
        convert "$src" -modulate 90,50,100 -colorize 0,0,12 \
            \( +clone -threshold 65% -blur 0x0.5 -modulate 100,0,100 \) \
            -compose screen -composite "$winter_out"
    fi
    generated=$((generated + 1))
done

echo ""
echo "Seasonal texture generation complete!"
echo "Generated: $generated textures"
echo "Skipped: $skipped textures"
echo ""
echo "Textures created in: $SPRITES_DIR"
ls -la "$SPRITES_DIR" | grep -cE "^-.*(spring|summer|fall|winter)_" | xargs -I {} echo "Seasonal texture files: {}"
