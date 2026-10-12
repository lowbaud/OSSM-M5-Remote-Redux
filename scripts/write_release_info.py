"""Write metadata displayed by the deployed web flasher."""

import argparse
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from build_metadata import load_build_metadata
from project_metadata import load_project_metadata


MAX_LISTED_CHANGES = 20


def load_changes(project_dir, base):
    """Return the commits since base, newest first, and how many there are.

    Commits that were cherry-picked into base are left out.
    """
    revisions = ["--cherry-pick", "--right-only", f"{base}...HEAD"]

    def git(*args):
        return subprocess.run(
            ["git", *args],
            cwd=project_dir,
            check=True,
            capture_output=True,
            text=True,
            timeout=10,
        ).stdout

    count = int(git("rev-list", "--count", "--no-merges", *revisions))
    log = git(
        "log",
        "--no-merges",
        f"--max-count={MAX_LISTED_CHANGES}",
        "--abbrev=8",
        "--format=%h%x09%s",
        *revisions,
    )
    changes = []
    for line in log.splitlines():
        commit, _, subject = line.partition("\t")
        changes.append({"commit": commit, "subject": subject})
    return changes, count


def create_release_info(
    metadata,
    build_metadata,
    tag,
    commit,
    channel,
    timestamp,
    source_ref=None,
    description=None,
    changes=None,
    change_count=None,
    changes_base=None,
):
    if channel not in ("release", "development"):
        raise ValueError(f"Unknown release channel: {channel}")

    if channel == "development" and not source_ref:
        raise ValueError("Development release information requires a source ref")

    if build_metadata.available and build_metadata.commit != commit:
        raise RuntimeError(
            f"Release commit {commit} does not match source commit "
            f"{build_metadata.commit}"
        )

    release_info = {
        "version": metadata.version,
        "buildVersion": build_metadata.build_version,
        "tag": tag,
        "commit": commit,
        "channel": channel,
        "timestamp": timestamp,
    }

    if source_ref:
        release_info["sourceRef"] = source_ref

    if description and description.strip():
        release_info["description"] = description.strip()

    if changes_base:
        release_info["changesBase"] = changes_base
        release_info["changes"] = list(changes or [])
        release_info["changeCount"] = (
            change_count if change_count is not None else len(release_info["changes"])
        )

    return release_info


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output_file", type=Path, nargs="?")
    parser.add_argument("--tag", default="local")
    parser.add_argument("--commit")
    parser.add_argument(
        "--channel",
        choices=("release", "development"),
        default="release",
    )
    parser.add_argument("--timestamp")
    parser.add_argument("--source-ref")
    parser.add_argument("--description")
    parser.add_argument(
        "--changes-base",
        help="list the commits since this ref, e.g. origin/master",
    )
    parser.add_argument(
        "--changes-label",
        help="the name shown for --changes-base, e.g. master",
    )
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
    source_ref = args.source_ref
    if args.channel == "development" and not source_ref:
        source_ref = "local"

    changes = None
    change_count = None
    if args.changes_base:
        changes, change_count = load_changes(project_dir, args.changes_base)

    release_info = create_release_info(
        metadata=metadata,
        build_metadata=build_metadata,
        tag=args.tag,
        commit=commit,
        channel=args.channel,
        timestamp=timestamp,
        source_ref=source_ref,
        description=args.description,
        changes=changes,
        change_count=change_count,
        changes_base=args.changes_label or args.changes_base,
    )

    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(
        json.dumps(release_info, indent=4) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
