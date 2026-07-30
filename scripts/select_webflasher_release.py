"""Select official or development web-flasher releases from GitHub JSON."""

import argparse
import json
import sys
from pathlib import Path


DEVELOPMENT_TAG_PREFIX = "development-"


def select_release(releases, channel):
    if channel == "release":
        candidates = [
            release
            for release in releases
            if not release.get("isDraft") and not release.get("isPrerelease")
        ]
    elif channel == "development":
        candidates = [
            release
            for release in releases
            if not release.get("isDraft")
            and release.get("isPrerelease")
            and str(release.get("tagName", "")).startswith(
                DEVELOPMENT_TAG_PREFIX
            )
        ]
    else:
        raise ValueError(f"Unknown release channel: {channel}")

    candidates = [
        release
        for release in candidates
        if release.get("tagName") and release.get("publishedAt")
    ]
    if not candidates:
        return None

    return max(candidates, key=lambda release: release["publishedAt"])["tagName"]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("releases_file", type=Path)
    parser.add_argument(
        "--channel",
        choices=("release", "development"),
        required=True,
    )
    parser.add_argument("--optional", action="store_true")
    args = parser.parse_args()

    releases = json.loads(args.releases_file.read_text(encoding="utf-8"))
    if not isinstance(releases, list):
        raise RuntimeError("GitHub release data must be a JSON array")

    tag = select_release(releases, args.channel)
    if tag:
        print(tag)
        return

    if not args.optional:
        print(
            f"No published {args.channel} firmware release was found.",
            file=sys.stderr,
        )
        raise SystemExit(1)


if __name__ == "__main__":
    main()
