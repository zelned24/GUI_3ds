"""Prepare an isolated Old 3DS pixel-art profile; never edit the user's global config."""
import configparser
import os
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


if __name__ == "__main__":
    source = Path(os.environ["APPDATA"]) / "Azahar/config/qt-config.ini"
    target = Path(__file__).resolve().parents[1] / "build/azahar-pixel-preview/config/qt-config.ini"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(pixel_profile(source.read_text(encoding="utf-8")), encoding="utf-8", newline="\n")
    print("Prepared isolated profile:", target)
