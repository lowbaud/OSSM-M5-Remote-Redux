"""Write metadata displayed by the deployed web flasher."""

import argparse
import json
from pathlib import Path

from project_metadata import load_project_metadata


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output_file", type=Path)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--channel", choices=("release", "preview"), required=True)
    parser.add_argument("--timestamp", required=True)
    args = parser.parse_args()

    project_dir = Path(__file__).resolve().parent.parent
    metadata = load_project_metadata(project_dir / "platformio.ini")
    release_info = {
        "version": metadata.version,
        "tag": args.tag,
        "commit": args.commit,
        "channel": args.channel,
        "timestamp": args.timestamp,
    }

    args.output_file.parent.mkdir(parents=True, exist_ok=True)
    args.output_file.write_text(
        json.dumps(release_info, indent=4) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
