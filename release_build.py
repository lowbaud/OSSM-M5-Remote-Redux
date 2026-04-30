import json
import re
import shutil
import subprocess
import sys
from pathlib import Path


ENVS = [
    "m5stack-core2",
    "m5stack-cores3",
]

CHARGE_CURRENTS = [
    100,
    1000,
]

CONFIG_FILE = Path("src/config.h")

OUTPUT_DIR = Path("build/release")
FIRMWARE_DIR = Path("build/firmware")


def find_platformio():
    candidates = [
        shutil.which("pio"),
        shutil.which("pio.exe"),
        shutil.which("platformio"),
        shutil.which("platformio.exe"),
        str(Path.home() / ".platformio" / "penv" / "Scripts" / "pio.exe"),
        str(Path.home() / ".platformio" / "penv" / "Scripts" / "platformio.exe"),
    ]

    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate

    raise RuntimeError(
        "Could not find PlatformIO command.\n"
        "Checked PATH and ~/.platformio/penv/Scripts."
    )


import shutil

PIO_EXE = find_platformio()


def run_platformio(args):
    command = [PIO_EXE] + args

    print("")
    print("Running: " + " ".join(command))

    result = subprocess.run(command, shell=False)

    if result.returncode != 0:
        raise RuntimeError("Command failed: " + " ".join(command))


def patch_charge_current(value):
    text = CONFIG_FILE.read_text(encoding="utf-8")

    pattern = re.compile(
        r"^(\s*#\s*define\s+BATTERY_CHARGE_CURRENT\s+)(\d+)(.*)$",
        flags=re.MULTILINE,
    )

    new_text, count = pattern.subn(
        rf"\g<1>{value}\g<3>",
        text,
        count=1,
    )

    if count == 0:
        raise RuntimeError(
            "Could not find an active '#define BATTERY_CHARGE_CURRENT ...' in "
            + str(CONFIG_FILE)
        )

    CONFIG_FILE.write_text(new_text, encoding="utf-8")


def read_flag(flag):
    text = CONFIG_FILE.read_text(encoding="utf-8")

    pattern = re.compile(
        r"^\s*#\s*define\s+"
        + re.escape(flag)
        + r'\s+"?([^"\s]+)"?',
        flags=re.MULTILINE,
    )

    match = pattern.search(text)

    if match:
        return match.group(1)

    return None


def get_app_version_from_config():
    app_version = read_flag("APP_VERSION")

    if app_version:
        return app_version.replace("V", "").replace("v", "")

    return "2.2.0"


def get_app_name_from_config():
    app_name = read_flag("APP_NAME")

    if app_name:
        return app_name

    return "OSSM-M5-Remote"


def chip_family_for_env(env_name):
    if env_name == "m5stack-cores3":
        return "ESP32-S3"

    if env_name == "m5stack-core2":
        return "ESP32"

    raise RuntimeError("Unknown chip family for env: " + env_name)


def manifest_short_name_for_env(env_name):
    if env_name == "m5stack-cores3":
        return "cores3"

    if env_name == "m5stack-core2":
        return "core2"

    return env_name


def find_best_firmware_source(env_name):
    """
    Prefer release/renamed firmware from rename_fw.py.
    Fall back to merged-flash.bin from PlatformIO build dir.
    Last fallback is firmware.bin, but that is app-only and not ideal for webflasher at offset 0.
    """

    release_bins = sorted(FIRMWARE_DIR.glob(f"*_{env_name}_*.bin"))

    if release_bins:
        return release_bins[-1], True

    merged_bin = Path(".pio") / "build" / env_name / "merged-flash.bin"

    if merged_bin.exists():
        print("")
        print("WARNING: No renamed release .bin found in build/firmware.")
        print("Using merged image directly: " + str(merged_bin))
        return merged_bin, True

    app_bin = Path(".pio") / "build" / env_name / "firmware.bin"

    if app_bin.exists():
        print("")
        print("WARNING: merged-flash.bin not found.")
        print("Using app-only firmware.bin: " + str(app_bin))
        print("This is NOT suitable for ESP Web Flasher offset 0.")
        print("Fix mergeb_bin.py / extra_scripts if you want a full merged image.")
        return app_bin, False

    raise RuntimeError(
        "No firmware found for env "
        + env_name
        + "\nChecked:\n"
        + str(FIRMWARE_DIR / f'*_{env_name}_*.bin')
        + "\n"
        + str(merged_bin)
        + "\n"
        + str(app_bin)
    )


def find_md5_for_source(source_bin):
    md5_file = source_bin.with_suffix(".md5")

    if md5_file.exists():
        return md5_file

    return None


def create_md5_file(bin_file):
    import hashlib

    md5_file = bin_file.with_suffix(".md5")

    with open(bin_file, "rb") as f:
        md5 = hashlib.md5(f.read()).hexdigest()

    with open(md5_file, "w", encoding="utf-8") as f:
        f.write(md5)

    print("Writing MD5: " + str(md5_file))
    return md5_file


def create_webflasher_manifest(env_name, charge_current, firmware_filename, app_version):
    manifest = {
        "name": f"M5-Remote-{charge_current}mAh",
        "version": f"V{app_version}",
        "funding_url": "",
        "builds": [
            {
                "chipFamily": chip_family_for_env(env_name),
                "parts": [
                    {
                        "path": firmware_filename,
                        "offset": 0,
                    }
                ],
            }
        ],
    }

    short_env = manifest_short_name_for_env(env_name)
    manifest_file = OUTPUT_DIR / f"manifest_{short_env}_{charge_current}mah.json"

    print("Writing webflasher manifest: " + str(manifest_file))

    with open(manifest_file, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=4)

    return manifest_file


def copy_release_files(env_name, charge_current):
    source_bin, is_merged_image = find_best_firmware_source(env_name)

    app_name = get_app_name_from_config()
    app_version = get_app_version_from_config()
    version_for_filename = app_version.replace(".", "-")

    target_bin = OUTPUT_DIR / (
        f"{app_name}_{env_name}_{version_for_filename}_charge-{charge_current}mA.bin"
    )

    print("Copying " + str(source_bin) + " -> " + str(target_bin))
    shutil.copy(source_bin, target_bin)

    source_md5 = find_md5_for_source(source_bin)

    if source_md5:
        target_md5 = target_bin.with_suffix(".md5")
        print("Copying " + str(source_md5) + " -> " + str(target_md5))
        shutil.copy(source_md5, target_md5)
    else:
        create_md5_file(target_bin)

    if not is_merged_image:
        print("")
        print("WARNING: Manifest was created for offset 0, but source was app-only firmware.bin.")
        print("This will probably flash but not boot.")
        print("You need merged-flash.bin for ESP Web Flasher offset 0.")

    create_webflasher_manifest(
        env_name=env_name,
        charge_current=charge_current,
        firmware_filename=target_bin.name,
        app_version=app_version,
    )


def main():
    print("Using PlatformIO: " + PIO_EXE)

    if not CONFIG_FILE.exists():
        raise RuntimeError("Missing config file: " + str(CONFIG_FILE))

    original_config = CONFIG_FILE.read_text(encoding="utf-8")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    try:
        for env_name in ENVS:
            for charge_current in CHARGE_CURRENTS:
                print("")
                print("========================================")
                print(
                    f"Building {env_name} "
                    f"with BATTERY_CHARGE_CURRENT {charge_current}"
                )
                print("========================================")

                patch_charge_current(charge_current)

                run_platformio(["run", "-e", env_name, "-t", "clean"])
                run_platformio(["run", "-e", env_name])

                copy_release_files(env_name, charge_current)

    finally:
        print("")
        print("Restoring original config.h")
        CONFIG_FILE.write_text(original_config, encoding="utf-8")

    print("")
    print("Release build completed.")
    print("Output directory: " + str(OUTPUT_DIR))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print("")
        print("ERROR: " + str(exc))
        sys.exit(1)