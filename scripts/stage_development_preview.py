"""Offer the last release build as development builds in the local site preview.

Run after release_build.py. Each --branch becomes an entry in the development
flasher's branch list; all of them flash the same firmware.
"""

import argparse
import json
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from build_metadata import load_build_metadata
from development_builds import INDEX_FILE, RELEASE_INFO_FILE, branch_slug, create_index
from project_metadata import load_project_metadata
from write_release_info import create_release_info, load_changes


PROJECT_DIR = Path(__file__).resolve().parent.parent
RELEASE_DIR = PROJECT_DIR / "build" / "release"
DEVELOPMENT_DIR = RELEASE_DIR / "development"
REQUIRED_MANIFEST = "manifest_factory_500mA.json"
PREVIEW_URL = "http://localhost:4000/flash/development/"


def current_branch():
    try:
        branch = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            cwd=PROJECT_DIR,
            check=True,
            capture_output=True,
            text=True,
            timeout=10,
        ).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return "local"
    return branch if branch and branch != "HEAD" else "local"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--branch",
        dest="branches",
        action="append",
        help="branch name to list; may be repeated (default: the current branch)",
    )
    parser.add_argument(
        "--changes-base",
        default="master",
        help="list the commits since this ref (default: master)",
    )
    parser.add_argument("--description", help="note shown for every listed branch")
    args = parser.parse_args()

    if not (RELEASE_DIR / REQUIRED_MANIFEST).is_file():
        raise SystemExit(
            f"No 500 mA build found in {RELEASE_DIR}. Run python release_build.py first."
        )

    metadata = load_project_metadata(PROJECT_DIR / "platformio.ini")
    build_metadata = load_build_metadata(PROJECT_DIR, metadata.version)
    try:
        changes, change_count = load_changes(PROJECT_DIR, args.changes_base)
    except (OSError, subprocess.SubprocessError):
        print(f"Could not list the commits since {args.changes_base}; showing none.")
        changes, change_count = [], 0
    timestamp = (
        datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    )

    shutil.rmtree(DEVELOPMENT_DIR, ignore_errors=True)
    release_files = [path for path in RELEASE_DIR.iterdir() if path.is_file()]
    taken = set()
    for branch in args.branches or [current_branch()]:
        build_dir = DEVELOPMENT_DIR / branch_slug(branch, taken)
        build_dir.mkdir(parents=True)
        for path in release_files:
            shutil.copy2(path, build_dir)

        release_info = create_release_info(
            metadata=metadata,
            build_metadata=build_metadata,
            tag="local",
            commit=build_metadata.commit,
            channel="development",
            timestamp=timestamp,
            source_ref=branch,
            description=args.description,
            changes=changes,
            change_count=change_count,
            changes_base=args.changes_base,
        )
        (build_dir / RELEASE_INFO_FILE).write_text(
            json.dumps(release_info, indent=4) + "\n", encoding="utf-8"
        )
        print(f"Staged {branch} in {build_dir}")

    (DEVELOPMENT_DIR / INDEX_FILE).write_text(
        json.dumps(create_index(DEVELOPMENT_DIR), indent=4) + "\n", encoding="utf-8"
    )
    print(f"Start the preview from docs/ with docker compose up, then open {PREVIEW_URL}")


if __name__ == "__main__":
    main()
