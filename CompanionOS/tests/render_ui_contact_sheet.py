"""Optional PPM-to-PNG conversion and contact sheet; requires Pillow only."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="A resolution directory from run_ui_host.ps1 -Render")
    args = parser.parse_args()
    paths = sorted(args.directory.glob("*.ppm"))
    if not paths:
        parser.error("No PPM images found")
    cells = []
    for path in paths:
        with Image.open(path) as source:
            image = source.convert("RGB")
        if len(image.getcolors(image.width * image.height)) < 3:
            raise ValueError(f"Nearly blank page: {path}")
        image.save(path.with_suffix(".png"))
        scale = 2 if image.width <= 200 else 1
        image = image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)
        cell = Image.new("RGB", (340, 284), "#242424")
        ImageDraw.Draw(cell).text((10, 6), path.stem, fill="white")
        cell.paste(image, (10, 24))
        cells.append(cell)
    columns = 3
    sheet = Image.new("RGB", (340 * columns, 284 * ((len(cells) + columns - 1) // columns)), "#242424")
    for index, cell in enumerate(cells):
        sheet.paste(cell, ((index % columns) * 340, (index // columns) * 284))
    output = args.directory / "contact-sheet.png"
    sheet.save(output)
    print(f"Converted {len(paths)} nonblank pages; {output}")


if __name__ == "__main__":
    main()
