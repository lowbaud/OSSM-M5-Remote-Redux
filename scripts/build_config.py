import os
import sys
from pathlib import Path

Import("env")

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
sys.path.insert(0, str(PROJECT_DIR / "scripts"))

from project_metadata import load_project_metadata


metadata = load_project_metadata(PROJECT_DIR / "platformio.ini")

charge_current_text = os.environ.get(
    "OSSM_CHARGE_CURRENT_MA", str(metadata.default_charge_current_ma)
)

try:
    charge_current_ma = int(charge_current_text)
except ValueError as exc:
    raise RuntimeError(
        "OSSM_CHARGE_CURRENT_MA must be a positive integer"
    ) from exc

if charge_current_ma <= 0:
    raise RuntimeError("OSSM_CHARGE_CURRENT_MA must be a positive integer")

env.Append(
    CPPDEFINES=[
        ("APP_DISPLAY_NAME", env.StringifyMacro(metadata.display_name)),
        ("APP_NAME", env.StringifyMacro(metadata.artifact_name)),
        ("APP_VERSION", env.StringifyMacro(metadata.version)),
        ("BATTERY_CHARGE_CURRENT", charge_current_ma),
    ]
)
