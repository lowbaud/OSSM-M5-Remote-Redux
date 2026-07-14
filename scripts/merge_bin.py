import subprocess

Import("env")

if env.IsIntegrationDump():
    Return()

APP_BIN = "$BUILD_DIR/${PROGNAME}.bin"
MERGED_BIN = "$BUILD_DIR/merged-flash.bin"

BOARD_CONFIG = env.BoardConfig()


def merge_bin(target, source, env):
    flash_images = env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])) + [
        "$ESP32_APP_OFFSET",
        APP_BIN,
    ]

    flash_size = BOARD_CONFIG.get("upload.flash_size", "16MB")

    command = [
        env.subst("$PYTHONEXE"),
        env.subst("$OBJCOPY"),
        "--chip",
        BOARD_CONFIG.get("build.mcu", "esp32s3"),
        "merge_bin",
        "-o",
        env.subst(MERGED_BIN),
        "--flash_mode",
        "keep",
        "--flash_size",
        flash_size,
    ] + [env.subst(str(image)) for image in flash_images]

    subprocess.run(
        command,
        check=True,
        cwd=env.subst("$PROJECT_DIR"),
    )


env.AddPostAction(APP_BIN, merge_bin)
