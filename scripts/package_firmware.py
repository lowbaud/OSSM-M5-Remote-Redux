import hashlib
import json
import shutil
import sys
from pathlib import Path

Import("env")

if env.IsIntegrationDump():
    Return()

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
sys.path.insert(0, str(PROJECT_DIR / "scripts"))

from project_metadata import firmware_stem, load_project_metadata


APP_BIN = "$BUILD_DIR/${PROGNAME}.bin"
DEFAULT_FACTORY_SOURCE = "$BUILD_DIR/merged-flash.bin"
FACTORY_SOURCE = env.GetProjectOption(
    "custom_firmware_package_source",
    DEFAULT_FACTORY_SOURCE,
)
FIRMWARE_DIR = PROJECT_DIR / "build" / "firmware"
METADATA = load_project_metadata(PROJECT_DIR / "platformio.ini")
ARTIFACT_DESCRIPTOR_SCHEMA = 1


def write_sha256(bin_file):
    sha256_file = bin_file.with_suffix(".sha256")

    with bin_file.open("rb") as firmware:
        digest = hashlib.sha256(firmware.read()).hexdigest()

    sha256_file.write_text(
        f"{digest}  {bin_file.name}\n",
        encoding="utf-8",
    )
    print("Writing SHA-256: " + str(sha256_file))
    return digest


def parse_flash_offset(value):
    try:
        offset = int(str(value), 0)
    except ValueError as exc:
        raise RuntimeError(f"Invalid application flash offset: {value}") from exc

    if offset <= 0:
        raise RuntimeError(f"Application flash offset must be positive: {value}")

    return offset


def remove_artifact(path):
    path.unlink(missing_ok=True)
    path.with_suffix(".md5").unlink(missing_ok=True)
    path.with_suffix(".sha256").unlink(missing_ok=True)


def package_firmware(target, source, env):
    build_target = env.get("PIOENV") or "unknown"
    artifact_stem = firmware_stem(METADATA, build_target)
    factory_source = Path(env.subst(FACTORY_SOURCE))
    update_source = Path(env.subst(APP_BIN))
    app_offset = parse_flash_offset(env.subst("$ESP32_APP_OFFSET"))

    for artifact_type, artifact_source in (
        ("factory", factory_source),
        ("update", update_source),
    ):
        if not artifact_source.is_file():
            print(
                f"ERROR: {artifact_type} firmware source not found: "
                + str(artifact_source)
            )
            env.Exit(1)

    FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)
    legacy_output = FIRMWARE_DIR / f"{artifact_stem}.bin"
    factory_output = FIRMWARE_DIR / f"{artifact_stem}_factory.bin"
    update_output = FIRMWARE_DIR / f"{artifact_stem}_update.bin"
    descriptor_output = FIRMWARE_DIR / f"{artifact_stem}_artifacts.json"

    for artifact in (legacy_output, factory_output, update_output):
        remove_artifact(artifact)
    descriptor_output.unlink(missing_ok=True)

    print("App Version: " + METADATA.version)
    print("App Name: " + METADATA.artifact_name)
    print("Build Target: " + build_target)
    print("Packaging " + str(factory_source) + " as " + str(factory_output))
    shutil.copy2(factory_source, factory_output)
    factory_sha256 = write_sha256(factory_output)

    print("Packaging " + str(update_source) + " as " + str(update_output))
    shutil.copy2(update_source, update_output)
    update_sha256 = write_sha256(update_output)

    descriptor = {
        "schemaVersion": ARTIFACT_DESCRIPTOR_SCHEMA,
        "environment": build_target,
        "artifacts": {
            "factory": {
                "filename": factory_output.name,
                "offset": 0,
                "sha256": factory_sha256,
            },
            "update": {
                "filename": update_output.name,
                "offset": app_offset,
                "sha256": update_sha256,
            },
        },
    }
    print("Writing artifact descriptor: " + str(descriptor_output))
    descriptor_output.write_text(
        json.dumps(descriptor, indent=4) + "\n",
        encoding="utf-8",
    )


# The factory image is produced before this action: either by merge_bin.py for
# legacy environments or by pioarduino's factory-image post-action. APP_BIN is
# the application-only update image attached to this post-action.
env.AddPostAction(APP_BIN, package_firmware)
