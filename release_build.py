"""Build factory-install and settings-preserving Redux release artifacts."""

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
ARTIFACT_DESCRIPTOR_SCHEMA = 1
ARTIFACT_TYPES = ("factory", "update")


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


def intermediate_artifact_path(metadata, env_name, artifact_type):
    return FIRMWARE_DIR / (
        f"{firmware_stem(metadata, env_name)}_{artifact_type}.bin"
    )


def intermediate_descriptor_path(metadata, env_name):
    return FIRMWARE_DIR / f"{firmware_stem(metadata, env_name)}_artifacts.json"


def clear_intermediate_firmware(metadata, env_name):
    legacy_bin = FIRMWARE_DIR / f"{firmware_stem(metadata, env_name)}.bin"
    artifact_files = [legacy_bin] + [
        intermediate_artifact_path(metadata, env_name, artifact_type)
        for artifact_type in ARTIFACT_TYPES
    ]

    for artifact_file in artifact_files:
        artifact_file.unlink(missing_ok=True)
        artifact_file.with_suffix(".md5").unlink(missing_ok=True)
        artifact_file.with_suffix(".sha256").unlink(missing_ok=True)

    intermediate_descriptor_path(metadata, env_name).unlink(missing_ok=True)


def file_sha256(bin_file):
    with bin_file.open("rb") as firmware:
        return hashlib.sha256(firmware.read()).hexdigest()


def require_intermediate_artifacts(metadata, env_name):
    descriptor_file = intermediate_descriptor_path(metadata, env_name)

    if not descriptor_file.is_file():
        raise RuntimeError(
            "Expected artifact descriptor was not produced: " + str(descriptor_file)
        )

    try:
        descriptor = json.loads(descriptor_file.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError(
            "Could not read artifact descriptor: " + str(descriptor_file)
        ) from exc

    if descriptor.get("schemaVersion") != ARTIFACT_DESCRIPTOR_SCHEMA:
        raise RuntimeError("Unsupported artifact descriptor schema")

    if descriptor.get("environment") != env_name:
        raise RuntimeError(
            f"Artifact descriptor environment does not match {env_name}"
        )

    descriptor_artifacts = descriptor.get("artifacts")
    if not isinstance(descriptor_artifacts, dict):
        raise RuntimeError("Artifact descriptor is missing artifacts")

    artifacts = {}
    for artifact_type in ARTIFACT_TYPES:
        expected_file = intermediate_artifact_path(
            metadata, env_name, artifact_type
        )
        artifact = descriptor_artifacts.get(artifact_type)

        if not isinstance(artifact, dict):
            raise RuntimeError(
                f"Artifact descriptor is missing {artifact_type} firmware"
            )

        if artifact.get("filename") != expected_file.name:
            raise RuntimeError(
                f"Unexpected {artifact_type} firmware filename in descriptor"
            )

        offset = artifact.get("offset")
        if not isinstance(offset, int) or offset < 0:
            raise RuntimeError(
                f"Invalid {artifact_type} firmware offset in descriptor"
            )

        if artifact_type == "factory" and offset != 0:
            raise RuntimeError("Factory firmware must be flashed at offset zero")

        if artifact_type == "update" and offset == 0:
            raise RuntimeError("Update firmware must have a non-zero offset")

        if not expected_file.is_file():
            raise RuntimeError(
                f"Expected {artifact_type} firmware was not produced: "
                + str(expected_file)
            )

        expected_sha256 = artifact.get("sha256")
        actual_sha256 = file_sha256(expected_file)
        if expected_sha256 != actual_sha256:
            raise RuntimeError(
                f"SHA-256 mismatch for intermediate {artifact_type} firmware"
            )

        artifacts[artifact_type] = {
            "path": expected_file,
            "offset": offset,
        }

    return artifacts


def write_sha256(bin_file):
    sha256_file = bin_file.with_suffix(".sha256")

    with bin_file.open("rb") as firmware:
        digest = hashlib.sha256(firmware.read()).hexdigest()

    sha256_file.write_text(
        f"{digest}  {bin_file.name}\n",
        encoding="utf-8",
    )
    print("Writing SHA-256: " + str(sha256_file))


def create_webflasher_manifest(
    metadata, charge_current, artifact_type, firmware_artifacts
):
    builds = []

    for env_name in ENVS:
        environment_artifacts = firmware_artifacts.get(env_name, {})
        artifact = environment_artifacts.get(artifact_type)

        if not artifact:
            raise RuntimeError(
                f"Missing {artifact_type} firmware for {env_name} at "
                f"{charge_current} mA"
            )

        builds.append(
            {
                "chipFamily": chip_family_for_env(env_name),
                "parts": [
                    {
                        "path": artifact["filename"],
                        "offset": artifact["offset"],
                    }
                ],
            }
        )

    action_name = "Full Install" if artifact_type == "factory" else "Update"
    manifest = {
        "name": (
            f"{metadata.display_name} {action_name} - "
            f"{charge_current} mA charge current"
        ),
        "version": metadata.version,
        "new_install_prompt_erase": True,
        "builds": builds,
    }

    manifest_file = OUTPUT_DIR / (
        f"manifest_{artifact_type}_{charge_current}mA.json"
    )
    print("Writing web flasher manifest: " + str(manifest_file))
    manifest_file.write_text(json.dumps(manifest, indent=4) + "\n", encoding="utf-8")


def copy_release_files(metadata, env_name, charge_current):
    intermediate_artifacts = require_intermediate_artifacts(metadata, env_name)
    release_artifacts = {}

    for artifact_type, artifact in intermediate_artifacts.items():
        source_bin = artifact["path"]
        target_bin = OUTPUT_DIR / (
            f"{firmware_stem(metadata, env_name)}_charge-{charge_current}mA_"
            f"{artifact_type}.bin"
        )

        print("Copying " + str(source_bin) + " -> " + str(target_bin))
        shutil.copy2(source_bin, target_bin)
        write_sha256(target_bin)
        release_artifacts[artifact_type] = {
            "filename": target_bin.name,
            "offset": artifact["offset"],
        }

    return release_artifacts


def main():
    metadata = load_project_metadata(PROJECT_CONFIG)
    pio_exe = find_platformio()

    print("Using PlatformIO: " + pio_exe)
    print("Building " + metadata.display_name + " " + metadata.version)
    print("Factory installers reset stored settings.")
    print("Application updates preserve settings only when erase is not selected.")

    if OUTPUT_DIR.exists():
        shutil.rmtree(OUTPUT_DIR)

    OUTPUT_DIR.mkdir(parents=True)
    firmware_artifacts = {
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
                firmware_artifacts[charge_current][env_name] = copy_release_files(
                    metadata, env_name, charge_current
                )

        for charge_current in CHARGE_CURRENTS:
            for artifact_type in ARTIFACT_TYPES:
                create_webflasher_manifest(
                    metadata=metadata,
                    charge_current=charge_current,
                    artifact_type=artifact_type,
                    firmware_artifacts=firmware_artifacts[charge_current],
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
