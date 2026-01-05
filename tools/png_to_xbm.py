#!/usr/bin/env python3
"""Convert PNG icons to XBM format for Adafruit GFX library."""

import os
from PIL import Image

def png_to_xbm(png_path, xbm_path, var_name):
    """Convert PNG to XBM format."""
    # Open image
    img = Image.open(png_path)
    width, height = img.size

    # Handle transparency - create white background, paste with alpha
    if img.mode == 'RGBA':
        # Create a white background
        background = Image.new('RGB', img.size, (255, 255, 255))
        # Paste the image using alpha channel as mask
        background.paste(img, mask=img.split()[3])
        img = background

    # Convert to 1-bit black and white
    img = img.convert('1')

    # Get pixel data
    pixels = list(img.getdata())

    # Convert to XBM byte array
    bytes_per_line = (width + 7) // 8
    xbm_data = []

    for y in range(height):
        for byte_x in range(bytes_per_line):
            byte_val = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < width:
                    pixel_idx = y * width + x
                    # XBM format: 1 = white, 0 = black (inverted from typical)
                    # We want black pixels (0) to be drawn, so invert
                    if pixels[pixel_idx] == 0:  # Black pixel
                        byte_val |= (1 << bit)
            xbm_data.append(byte_val)

    # Write XBM file
    with open(xbm_path, 'w') as f:
        f.write(f"#define {var_name}_width {width}\n")
        f.write(f"#define {var_name}_height {height}\n")
        f.write(f"static const unsigned char {var_name}_bits[] PROGMEM = {{\n")

        for i, byte in enumerate(xbm_data):
            if i % 12 == 0:
                f.write("  ")
            f.write(f"0x{byte:02x}")
            if i < len(xbm_data) - 1:
                f.write(", ")
            if (i + 1) % 12 == 0 and i < len(xbm_data) - 1:
                f.write("\n")

        f.write("\n};\n")

def main():
    assets_dir = "/Users/davidb/Documents/Work/sechoir/dryer/assets"
    icons_dir = os.path.join(assets_dir, "icons")

    icons = [
        ("electric.png", "icon_electric"),
        ("fan.png", "icon_fan"),
        ("recycling.png", "icon_recycling"),
        ("water.png", "icon_water")
    ]

    for png_file, var_name in icons:
        png_path = os.path.join(icons_dir, png_file)
        xbm_path = os.path.join(icons_dir, png_file.replace('.png', '.xbm'))

        if os.path.exists(png_path):
            png_to_xbm(png_path, xbm_path, var_name)
            print(f"Converted {png_file} -> {os.path.basename(xbm_path)}")
        else:
            print(f"Warning: {png_path} not found")

if __name__ == "__main__":
    main()
