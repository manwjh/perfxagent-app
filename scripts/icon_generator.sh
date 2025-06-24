#!/bin/bash

# Icon Generator Script for PerfXAgent
# This script helps generate various icon sizes and formats from a source image

SOURCE_ICON="resources/icons/PerfxAgent-ASR.png"  # Your high-res source icon
OUTPUT_DIR="resources/icons"

# Create output directories
mkdir -p "$OUTPUT_DIR"

# Check if source icon exists
if [ ! -f "$SOURCE_ICON" ]; then
    echo "Error: Source icon not found at $SOURCE_ICON"
    echo "Please place your high-resolution source icon (1024x1024 recommended) at that location"
    exit 1
fi

echo "Generating icons from $SOURCE_ICON..."

# Generate PNG icons for different sizes
sizes=(16 24 32 48 64 128 256 512 1024)

for size in "${sizes[@]}"; do
    output_file="$OUTPUT_DIR/app_icon_${size}x${size}.png"
    echo "Generating $output_file..."
    
    # Using ImageMagick if available, otherwise using sips (macOS built-in)
    if command -v convert &> /dev/null; then
        convert "$SOURCE_ICON" -resize "${size}x${size}" "$output_file"
    elif command -v sips &> /dev/null; then
        sips -z "$size" "$size" "$SOURCE_ICON" --out "$output_file"
    else
        echo "Warning: Neither ImageMagick nor sips found. Please install ImageMagick for better icon generation."
        echo "You can install it with: brew install imagemagick"
    fi
done

# Generate ICO file (Windows)
echo "Generating Windows ICO file..."
if command -v convert &> /dev/null; then
    convert "$OUTPUT_DIR/app_icon_16x16.png" "$OUTPUT_DIR/app_icon_32x32.png" \
            "$OUTPUT_DIR/app_icon_48x48.png" "$OUTPUT_DIR/app_icon_64x64.png" \
            "$OUTPUT_DIR/app_icon_128x128.png" "$OUTPUT_DIR/app_icon_256x256.png" \
            "$OUTPUT_DIR/app_icon.ico"
else
    echo "Warning: ImageMagick not found. ICO generation skipped."
    echo "Install ImageMagick: brew install imagemagick"
fi

# Generate ICNS file (macOS)
echo "Generating macOS ICNS file..."
if command -v iconutil &> /dev/null; then
    # Create iconset directory
    iconset_dir="$OUTPUT_DIR/app_icon.iconset"
    mkdir -p "$iconset_dir"
    
    # Copy icons to iconset with proper naming
    cp "$OUTPUT_DIR/app_icon_16x16.png" "$iconset_dir/icon_16x16.png"
    cp "$OUTPUT_DIR/app_icon_32x32.png" "$iconset_dir/icon_16x16@2x.png"
    cp "$OUTPUT_DIR/app_icon_32x32.png" "$iconset_dir/icon_32x32.png"
    cp "$OUTPUT_DIR/app_icon_64x64.png" "$iconset_dir/icon_32x32@2x.png"
    cp "$OUTPUT_DIR/app_icon_128x128.png" "$iconset_dir/icon_128x128.png"
    cp "$OUTPUT_DIR/app_icon_256x256.png" "$iconset_dir/icon_128x128@2x.png"
    cp "$OUTPUT_DIR/app_icon_256x256.png" "$iconset_dir/icon_256x256.png"
    cp "$OUTPUT_DIR/app_icon_512x512.png" "$iconset_dir/icon_256x256@2x.png"
    cp "$OUTPUT_DIR/app_icon_512x512.png" "$iconset_dir/icon_512x512.png"
    cp "$OUTPUT_DIR/app_icon_1024x1024.png" "$iconset_dir/icon_512x512@2x.png"
    
    # Generate ICNS file
    iconutil -c icns "$iconset_dir" -o "$OUTPUT_DIR/app_icon.icns"
    
    # Clean up iconset directory
    rm -rf "$iconset_dir"
else
    echo "Warning: iconutil not found. ICNS generation skipped."
fi

echo "Icon generation complete!"
echo "Generated files in $OUTPUT_DIR:"
ls -la "$OUTPUT_DIR"/*.png "$OUTPUT_DIR"/*.ico "$OUTPUT_DIR"/*.icns 2>/dev/null || echo "No icon files found"

echo ""
echo "Next steps:"
echo "1. Place your high-resolution source icon at $SOURCE_ICON"
echo "2. Run this script again: ./scripts/icon_generator.sh"
echo "3. Use the generated icons in your application" 