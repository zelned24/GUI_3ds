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

def write_index(root, report):
    prefix = "images/pokemon/icons/"
    keys = [row["sourcePath"].removeprefix(prefix) for row in report["files"]]
    if keys != sorted(set(keys)) or any(not row["sourcePath"].startswith(prefix) for row in report["files"]):
        raise ValueError("Appearance icon source keys must be unique, ordered and within the pinned root")
    header = "// Generated physical icon keys from pinned assets; actor identity mapping is separate.\n#pragma once\n#include <cstdint>\n#include <cstddef>\n#include <cstring>\nnamespace Pokerogue3DS {\n"
    header += "struct AppearanceIconFrame {const char* sourceKey;uint16_t page,x,y,width,height;};\ninline constexpr AppearanceIconFrame kAppearanceIconFrames[]={\n"
    for key, row in zip(keys, report["files"]):
        header += "    {" + json.dumps(key) + "," + ",".join(str(row[field]) for field in ("page","x","y","width","height")) + "},\n"
    header += "};\ninline constexpr const char* kAppearanceIconPages[]={\n"
    header += "".join(json.dumps(row["runtimePath"])+",\n" for row in report["pages"])
    header += "};\ninline constexpr const char* kCompactAppearanceIconPages[]={\n"
    header += "".join(json.dumps(row["compactRuntimePath"])+",\n" for row in report["pages"])
    header += "};\ninline constexpr std::size_t kAppearanceIconCount=sizeof(kAppearanceIconFrames)/sizeof(kAppearanceIconFrames[0]);\n"
    header += """inline const AppearanceIconFrame* findAppearanceIcon(const char* sourceKey) {
    if(!sourceKey || !*sourceKey) return nullptr;
    std::size_t first=0,last=kAppearanceIconCount;
    while(first<last) {
        const auto middle=first+(last-first)/2;
        if(std::strcmp(kAppearanceIconFrames[middle].sourceKey,sourceKey)<0) first=middle+1;
        else last=middle;
    }
    return first<kAppearanceIconCount && !std::strcmp(kAppearanceIconFrames[first].sourceKey,sourceKey)
        ? &kAppearanceIconFrames[first] : nullptr;
}
}
"""
    target=root / "project/generated/include/content/AppearanceIcons.hpp"
    target.write_bytes(header.encode("utf-8"))

def prepare_compact(root, report):
    for page in report["pages"]:
        staged=root / "build/native-presentation/appearance-icons" / f"appearance-icons-{page['page']}.png"
        png=staged.with_name(f"appearance-icons-compact-{page['page']}.png")
        with Image.open(staged) as image:
            image.convert("RGBA").resize((256,256),Image.Resampling.NEAREST).save(png)
        target=root / "build/romfs/presentation/icons" / f"appearance-icons-compact-{page['page']}.t3x"
        subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(png)],check=True,stdout=subprocess.DEVNULL)
        page.update({"compactRuntimePath":f"romfs:/presentation/icons/appearance-icons-compact-{page['page']}.t3x",
            "compactStagedSHA256":hashlib.sha256(png.read_bytes()).hexdigest(),
            "compactConvertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest(),
            "compactEstimatedResidentBytes":256*256*4,"compactAdaptation":"NEAREST_1_2_NATIVE_20_15"})
    return report

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
    prepare_compact(root, report)
    (root / "docs/generated/APPEARANCE_ICON_CONVERSION_REPORT.json").write_bytes(
        (json.dumps(report, sort_keys=True, indent=2)+"\n").encode("utf-8"))
    write_index(root, report)
    print(f"Packed {len(records)} pinned appearance icons in {len(pages)} native pages")
    return report

if __name__ == "__main__": prepare()
