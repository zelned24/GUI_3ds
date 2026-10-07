"""Prepare an isolated Old 3DS pixel-art profile; never edit the user's global config."""
import configparser
import os
import shutil
from pathlib import Path


def pixel_profile(source: str) -> str:
    original = configparser.ConfigParser(interpolation=None, strict=True)
    original.optionxform = str
    original.read_string(source)
    result = configparser.ConfigParser(interpolation=None)
    result.optionxform = str
    for section in ("Controls", "Renderer", "Layout", "Core", "System", "Audio"):
        result[section] = dict(original[section]) if original.has_section(section) else {}
    values = {
        "Renderer": {"texture_filter": "0", "texture_sampling": "1", "resolution_factor": "1", "use_integer_scaling": "true"},
        "Layout": {"filter_mode": "false"},
        "System": {"is_new_3ds": "false"},
        "Core": {"cpu_clock_percentage": "100"},
    }
    for section, settings in values.items():
        for key, value in settings.items():
            result[section][key] = value
            result[section][key + "\\default"] = "false"
    import io
    output = io.StringIO()
    result.write(output, space_around_delimiters=False)
    return output.getvalue()


def prepare_portable_preview(executable: Path, target: Path, profile: str):
    """Use Azahar's adjacent user directory; --user is ignored by this GUI."""
    executable, target = executable.resolve(), target.resolve()
    source = executable.parent
    if not executable.is_file() or executable.name.lower() != "azahar.exe":
        raise ValueError("Expected an installed azahar.exe")
    if target == source or source in target.parents or target in source.parents:
        raise ValueError("Portable preview must be separate from the installed emulator")
    target.mkdir(parents=True, exist_ok=True)
    for path in source.iterdir():
        if path == executable or path.suffix.lower() == ".dll":
            destination = target / path.name
            if not destination.exists() or destination.read_bytes() != path.read_bytes():
                shutil.copy2(path, destination)
    for name in ("plugins", "scripting"):
        if (source / name).is_dir():
            shutil.copytree(source / name, target / name, dirs_exist_ok=True)
    config = target / "user/config/qt-config.ini"
    config.parent.mkdir(parents=True, exist_ok=True)
    config.write_text(profile, encoding="utf-8", newline="\n")
    return target / executable.name


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--portable", action="store_true", help="Prepare a separate emulator with adjacent user/config")
    parser.add_argument("--azahar", type=Path, default=Path(os.environ["LOCALAPPDATA"]) / "Programs/Azahar/azahar.exe")
    args = parser.parse_args()
    source = Path(os.environ["APPDATA"]) / "Azahar/config/qt-config.ini"
    target = Path(__file__).resolve().parents[1] / "build/azahar-pixel-preview/config/qt-config.ini"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(pixel_profile(source.read_text(encoding="utf-8")), encoding="utf-8", newline="\n")
    print("Prepared isolated profile:", target)
    if args.portable:
        portable = target.parents[2] / "azahar-pixel-runtime"
        print("Portable emulator:", prepare_portable_preview(args.azahar, portable, target.read_text(encoding="utf-8")))
