"""Pack a transparent application PNG into a multi-size Windows ICO.

On macOS this uses the built-in ``sips`` resampler. On other systems it uses
Pillow when available; the resulting ICO itself has no runtime dependencies.
"""

from __future__ import annotations

import argparse
import io
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path


SIZES = (16, 24, 32, 48, 64, 128, 256)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def resized_png(source: Path, size: int, temporary: Path) -> bytes:
    try:
        from PIL import Image
    except ImportError:
        if not shutil.which("sips"):
            raise SystemExit("Install Pillow or run this script on macOS with sips")
        destination = temporary / f"icon-{size}.png"
        subprocess.run(
            ["sips", "-z", str(size), str(size), str(source), "--out", str(destination)],
            check=True,
            capture_output=True,
            text=True,
        )
        data = destination.read_bytes()
    else:
        with Image.open(source) as image:
            image = image.convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)
            output = io.BytesIO()
            image.save(output, format="PNG")
            data = output.getvalue()

    if not data.startswith(PNG_SIGNATURE):
        raise ValueError(f"{size}px result is not PNG")
    width, height = struct.unpack_from(">II", data, 16)
    if (width, height) != (size, size):
        raise ValueError(f"{size}px result has size {width}x{height}")
    if data[25] not in (4, 6):
        raise ValueError(f"{size}px result lost its alpha channel")
    return data


def build_icon(source: Path, output: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="stellarvoyager-icon-") as temp:
        images = [resized_png(source, size, Path(temp)) for size in SIZES]

    directory = bytearray(struct.pack("<HHH", 0, 1, len(images)))
    offset = 6 + 16 * len(images)
    for size, data in zip(SIZES, images):
        encoded_size = 0 if size == 256 else size
        directory.extend(
            struct.pack("<BBBBHHII", encoded_size, encoded_size, 0, 0, 1, 32, len(data), offset)
        )
        offset += len(data)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(directory + b"".join(images))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="transparent master PNG")
    parser.add_argument("output", type=Path, help="Windows ICO destination")
    args = parser.parse_args()
    build_icon(args.source, args.output)


if __name__ == "__main__":
    main()
