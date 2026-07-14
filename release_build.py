"""Build full-install firmware artifacts for every supported Redux variant.

The generated web-flasher manifests install merged images at offset zero. The
merged images overwrite stored settings even when the optional full-device
erase is not selected.
"""

import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

from scripts.project_metadata import firmware_stem, load_project_metadata


ENVS = [
    "m5stack-core2",
    "m5stack-cores3",
]

CHARGE_CURRENTS = [
    500,
    1000,
]

PROJECT_DIR = Path(__file__).resolve().parent
PROJECT_CONFIG = PROJECT_DIR / "platformio.ini"
OUTPUT_DIR = PROJECT_DIR / "build" / "release"
FIRMWARE_DIR = PROJECT_DIR / "build" / "firmware"
CHARGE_CURRENT_ENV = "OSSM_CHARGE_CURRENT_MA"


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


def run_platformio(pio_exe, args, charge_current):
    command = [pio_exe] + args
    child_env = os.environ.copy()
    child_env[CHARGE_CURRENT_ENV] = str(charge_current)

    print("")
    print("Running: " + " ".join(command))
    print(f"{CHARGE_CURRENT_ENV}={charge_current}")

    result = subprocess.run(
        command,
        shell=False,
        env=child_env,
        cwd=PROJECT_DIR,
    )

    if result.returncode != 0:
        raise RuntimeError("Command failed: " + " ".join(command))


def chip_family_for_env(env_name):
    if env_name == "m5stack-cores3":
        return "ESP32-S3"

    if env_name == "m5stack-core2":
        return "ESP32"

    raise RuntimeError("Unknown chip family for env: " + env_name)


def intermediate_firmware_path(metadata, env_name):
    return FIRMWARE_DIR / f"{firmware_stem(metadata, env_name)}.bin"


def clear_intermediate_firmware(metadata, env_name):
    firmware_bin = intermediate_firmware_path(metadata, env_name)
    firmware_bin.unlink(missing_ok=True)
    firmware_bin.with_suffix(".md5").unlink(missing_ok=True)
    firmware_bin.with_suffix(".sha256").unlink(missing_ok=True)


def require_intermediate_firmware(metadata, env_name):
    firmware_bin = intermediate_firmware_path(metadata, env_name)

    if not firmware_bin.is_file():
        raise RuntimeError(
            "Expected merged firmware was not produced: " + str(firmware_bin)
        )

    return firmware_bin


def write_sha256(bin_file):
    sha256_file = bin_file.with_suffix(".sha256")

    with bin_file.open("rb") as firmware:
        digest = hashlib.sha256(firmware.read()).hexdigest()

    sha256_file.write_text(
        f"{digest}  {bin_file.name}\n",
        encoding="utf-8",
    )
    print("Writing SHA-256: " + str(sha256_file))


def create_webflasher_manifest(metadata, charge_current, firmware_filenames):
    builds = []

    for env_name in ENVS:
        firmware_filename = firmware_filenames.get(env_name)

        if not firmware_filename:
            raise RuntimeError(
                f"Missing release firmware for {env_name} at {charge_current} mA"
            )

        builds.append(
            {
                "chipFamily": chip_family_for_env(env_name),
                "parts": [
                    {
                        "path": firmware_filename,
                        "offset": 0,
                    }
                ],
            }
        )

    manifest = {
        "name": f"{metadata.display_name} - {charge_current} mA charge current",
        "version": metadata.version,
        "new_install_prompt_erase": True,
        "builds": builds,
    }

    manifest_file = OUTPUT_DIR / f"manifest_{charge_current}mA.json"
    print("Writing web flasher manifest: " + str(manifest_file))
    manifest_file.write_text(json.dumps(manifest, indent=4) + "\n", encoding="utf-8")


def copy_release_files(metadata, env_name, charge_current):
    source_bin = require_intermediate_firmware(metadata, env_name)
    target_bin = OUTPUT_DIR / (
        f"{firmware_stem(metadata, env_name)}_charge-{charge_current}mA.bin"
    )

    print("Copying " + str(source_bin) + " -> " + str(target_bin))
    shutil.copy2(source_bin, target_bin)
    write_sha256(target_bin)
    return target_bin.name


def main():
    metadata = load_project_metadata(PROJECT_CONFIG)
    pio_exe = find_platformio()

    print("Using PlatformIO: " + pio_exe)
    print("Building " + metadata.display_name + " " + metadata.version)
    print("WARNING: These merged installers overwrite stored settings.")
    print("The web flasher's optional erase performs an additional full-device erase.")

    if OUTPUT_DIR.exists():
        shutil.rmtree(OUTPUT_DIR)

    OUTPUT_DIR.mkdir(parents=True)
    firmware_filenames = {
        charge_current: {} for charge_current in CHARGE_CURRENTS
    }

    try:
        for env_name in ENVS:
            for charge_current in CHARGE_CURRENTS:
                print("")
                print("========================================")
                print(
                    f"Building {env_name} with {charge_current} mA charge current"
                )
                print("========================================")

                clear_intermediate_firmware(metadata, env_name)
                run_platformio(
                    pio_exe,
                    ["run", "-e", env_name, "-t", "clean"],
                    charge_current,
                )
                run_platformio(
                    pio_exe,
                    ["run", "-e", env_name],
                    charge_current,
                )
                firmware_filenames[charge_current][env_name] = copy_release_files(
                    metadata, env_name, charge_current
                )

        for charge_current in CHARGE_CURRENTS:
            create_webflasher_manifest(
                metadata=metadata,
                charge_current=charge_current,
                firmware_filenames=firmware_filenames[charge_current],
            )
    except BaseException:
        print("")
        print("Removing incomplete release output: " + str(OUTPUT_DIR))
        shutil.rmtree(OUTPUT_DIR, ignore_errors=True)
        raise

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
