"""Write metadata displayed by the deployed web flasher."""

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

from build_metadata import load_build_metadata
from project_metadata import load_project_metadata


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output_file", type=Path, nargs="?")
    parser.add_argument("--tag", default="local")
    parser.add_argument("--commit")
    parser.add_argument(
        "--channel",
        choices=("release", "preview"),
        default="preview",
    )
    parser.add_argument("--timestamp")
    args = parser.parse_args()

    project_dir = Path(__file__).resolve().parent.parent
    metadata = load_project_metadata(project_dir / "platformio.ini")
    build_metadata = load_build_metadata(project_dir, metadata.version)
    output_file = args.output_file or (
        project_dir / "build" / "release" / "release-info.json"
    )
    commit = args.commit or build_metadata.commit
    timestamp = args.timestamp or (
        datetime.now(timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z")
    )

    if build_metadata.available and build_metadata.commit != commit:
        raise RuntimeError(
            f"Release commit {commit} does not match source commit "
            f"{build_metadata.commit}"
        )

    release_info = {
        "version": metadata.version,
        "buildVersion": build_metadata.build_version,
        "tag": args.tag,
        "commit": commit,
        "channel": args.channel,
        "timestamp": timestamp,
    }

    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(
        json.dumps(release_info, indent=4) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
