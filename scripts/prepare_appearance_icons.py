"""Pack all pinned raw Pokemon icon keys without altering source pixels."""
import hashlib
import io
import json
import subprocess
import zipfile
from pathlib import Path
from PIL import Image
from inventory_pokemon_icons import inventory

ROOT = Path(__file__).resolve().parents[1]

def prepare(root=ROOT):
    source = inventory(root)
    archive = subprocess.check_output(["git", "-C", str(root / "build/upstream/pokerogue-assets"),
        "archive", "--format=zip", source["revision"], source["sourceRoot"]])
    staged = root / "build/native-presentation/appearance-icons"
    output = root / "build/romfs/presentation/icons"
    staged.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    records, pages = [], []
    canvas = None
    def save_page(index, image):
        png = staged / f"appearance-icons-{index}.png"
        texture = output / f"appearance-icons-{index}.t3x"
        image.save(png)
        subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe", "-f", "rgba8", "-o", str(texture), str(png)],
            check=True, stdout=subprocess.DEVNULL)
        pages.append({"page": index, "runtimePath": f"romfs:/presentation/icons/appearance-icons-{index}.t3x",
            "stagedSHA256": hashlib.sha256(png.read_bytes()).hexdigest(),
            "convertedSHA256": hashlib.sha256(texture.read_bytes()).hexdigest(),
            "width": 512, "height": 512, "estimatedResidentBytes": 512*512*4})
    with zipfile.ZipFile(io.BytesIO(archive)) as files:
        for index, row in enumerate(source["files"]):
            if (row["width"], row["height"]) != (40, 30):
                raise ValueError("Unsupported original icon canvas: " + row["sourcePath"])
            page, cell = divmod(index, 192)
            if cell == 0:
                if canvas is not None: save_page(page-1, canvas)
                canvas = Image.new("RGBA", (512, 512))
            x, y = (cell%12)*40, (cell//12)*32
            raw = files.read(row["sourcePath"])
            if hashlib.sha256(raw).hexdigest() != row["sourceSHA256"]:
                raise ValueError("Pinned icon hash mismatch")
            with Image.open(io.BytesIO(raw)) as image:
                canvas.paste(image.convert("RGBA"), (x, y))
            records.append({"sourcePath": row["sourcePath"], "sourceSHA256": row["sourceSHA256"],
                "page": page, "x": x, "y": y, "width": 40, "height": 30})
    if canvas is not None: save_page(len(pages), canvas)
    report = {"schemaVersion": 1, "repository": source["repository"], "revision": source["revision"],
        "sourceInventoryHash": source["contentSHA256"], "adaptation": "ORIGINAL_RGBA_PIXELS_1_TO_1",
        "runtimeValidation": "NOT_EXECUTED", "identityMapping": "RAW_SOURCE_PATH_ONLY",
        "pages": pages, "files": records}
    (root / "docs/generated/APPEARANCE_ICON_CONVERSION_REPORT.json").write_bytes(
        (json.dumps(report, sort_keys=True, indent=2)+"\n").encode("utf-8"))
    print(f"Packed {len(records)} pinned appearance icons in {len(pages)} native pages")
    return report

if __name__ == "__main__": prepare()
