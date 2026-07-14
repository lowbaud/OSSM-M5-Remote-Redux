import hashlib
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
MERGED_BIN = "$BUILD_DIR/merged-flash.bin"
FIRMWARE_DIR = PROJECT_DIR / "build" / "firmware"
METADATA = load_project_metadata(PROJECT_DIR / "platformio.ini")


def write_sha256(bin_file):
    sha256_file = bin_file.with_suffix(".sha256")

    with bin_file.open("rb") as firmware:
        digest = hashlib.sha256(firmware.read()).hexdigest()

    sha256_file.write_text(
        f"{digest}  {bin_file.name}\n",
        encoding="utf-8",
    )
    print("Writing SHA-256: " + str(sha256_file))


def package_merged_firmware(target, source, env):
    build_target = env.get("PIOENV") or "unknown"
    merged_bin = Path(env.subst(MERGED_BIN))

    if not merged_bin.is_file():
        print("ERROR: merged firmware not found: " + str(merged_bin))
        print("Make sure merge_bin.py runs before package_firmware.py")
        env.Exit(1)

    FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)
    output_bin = FIRMWARE_DIR / f"{firmware_stem(METADATA, build_target)}.bin"
    legacy_md5 = output_bin.with_suffix(".md5")
    output_sha256 = output_bin.with_suffix(".sha256")

    output_bin.unlink(missing_ok=True)
    legacy_md5.unlink(missing_ok=True)
    output_sha256.unlink(missing_ok=True)

    print("App Version: " + METADATA.version)
    print("App Name: " + METADATA.artifact_name)
    print("Build Target: " + build_target)
    print("Copying merged firmware to " + str(output_bin))

    shutil.copy2(merged_bin, output_bin)
    write_sha256(output_bin)


# merge_bin.py registers its APP_BIN post-action first. Attaching packaging to
# the same target makes the merge/package ordering explicit and reliable.
env.AddPostAction(APP_BIN, package_merged_firmware)
