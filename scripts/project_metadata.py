import configparser
import re
from dataclasses import dataclass
from pathlib import Path


ARTIFACT_NAME_PATTERN = re.compile(r"^[A-Za-z0-9_-]+$")
SEMVER_PATTERN = re.compile(
    r"^(0|[1-9]\d*)\."
    r"(0|[1-9]\d*)\."
    r"(0|[1-9]\d*)"
    r"(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$"
)
METADATA_SECTION = "project_metadata"


@dataclass(frozen=True)
class ProjectMetadata:
    display_name: str
    artifact_name: str
    version: str
    default_charge_current_ma: int


def load_project_metadata(config_file):
    config_file = Path(config_file)
    parser = configparser.ConfigParser(interpolation=None)

    if not parser.read(config_file, encoding="utf-8"):
        raise RuntimeError(f"Could not read project configuration: {config_file}")

    if not parser.has_section(METADATA_SECTION):
        raise RuntimeError(
            f"Missing [{METADATA_SECTION}] metadata section in {config_file}"
        )

    display_name = parser.get(METADATA_SECTION, "display_name").strip()
    artifact_name = parser.get(METADATA_SECTION, "artifact_name").strip()
    version = parser.get(METADATA_SECTION, "version").strip()
    default_charge_current_ma = parser.getint(
        METADATA_SECTION, "default_charge_current_ma"
    )

    if not display_name:
        raise RuntimeError("Project display_name must not be empty")

    if not ARTIFACT_NAME_PATTERN.fullmatch(artifact_name):
        raise RuntimeError(
            "Project artifact_name may contain only letters, digits, hyphens, "
            "and underscores"
        )

    if not SEMVER_PATTERN.fullmatch(version):
        raise RuntimeError(f"Project version is not valid semantic versioning: {version}")

    if default_charge_current_ma <= 0:
        raise RuntimeError("Project default_charge_current_ma must be positive")

    return ProjectMetadata(
        display_name=display_name,
        artifact_name=artifact_name,
        version=version,
        default_charge_current_ma=default_charge_current_ma,
    )


def firmware_stem(metadata, environment):
    version_for_filename = metadata.version.replace(".", "-")
    return f"{metadata.artifact_name}_{environment}_{version_for_filename}"
