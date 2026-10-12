"""Pick the development builds the web flasher offers and index them."""

import argparse
import json
import re
from pathlib import Path


DEVELOPMENT_TAG_PREFIX = "development-"
DEFAULT_BRANCH = "develop"
INDEX_FILE = "builds.json"
RELEASE_INFO_FILE = "release-info.json"

# The publish workflow titles each release "Development build from <branch> (<tag>)".
RELEASE_NAME_PATTERN = re.compile(
    r"^Development build from (?P<branch>.+) \((?P<tag>development-[^()]+)\)$"
)


def release_branch(release):
    """Return the branch a development release was built from, or None."""
    tag = str(release.get("tagName", ""))
    if (
        release.get("isDraft")
        or not release.get("isPrerelease")
        or not tag.startswith(DEVELOPMENT_TAG_PREFIX)
        or not release.get("publishedAt")
    ):
        return None

    match = RELEASE_NAME_PATTERN.match(str(release.get("name", "")))
    if not match or match.group("tag") != tag:
        return None
    return match.group("branch")


def branch_slug(branch, taken):
    """Return a folder name for the branch that no other branch uses.

    Jekyll leaves out folders that start with "." or "_".
    """
    base = re.sub(r"[^A-Za-z0-9._-]+", "-", branch).strip("-._") or "branch"
    slug = base
    suffix = 2
    while slug in taken:
        slug = f"{base}-{suffix}"
        suffix += 1
    taken.add(slug)
    return slug


def select_builds(releases, branches):
    """Keep the newest build of each existing branch.

    Returns the builds to publish as (branch, tag, slug) tuples sorted by
    branch, and the tags of every other development build.
    """
    newest = {}
    stale = []
    for release in releases:
        branch = release_branch(release)
        if branch is None:
            continue
        if branch not in branches:
            stale.append(release["tagName"])
            continue
        current = newest.get(branch)
        if current is None or release["publishedAt"] > current["publishedAt"]:
            if current is not None:
                stale.append(current["tagName"])
            newest[branch] = release
        else:
            stale.append(release["tagName"])

    taken = set()
    builds = [
        (branch, newest[branch]["tagName"], branch_slug(branch, taken))
        for branch in sorted(newest)
    ]
    return builds, sorted(stale)


def create_index(directory):
    """List the builds unpacked into the directory's subfolders.

    The default branch comes first and the others follow newest first.
    """
    builds = []
    for info_file in sorted(Path(directory).glob(f"*/{RELEASE_INFO_FILE}")):
        release = json.loads(info_file.read_text(encoding="utf-8"))
        if release.get("channel") != "development" or not release.get("sourceRef"):
            continue
        builds.append({"path": info_file.parent.name, **release})

    builds.sort(key=lambda build: build.get("timestamp", ""), reverse=True)
    builds.sort(key=lambda build: build["sourceRef"] != DEFAULT_BRANCH)
    return {"builds": builds}


def main():
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)

    select = commands.add_parser(
        "select", help="print the builds to publish as tab-separated tag and folder"
    )
    select.add_argument("releases_file", type=Path)
    select.add_argument("branches_file", type=Path)
    select.add_argument("--stale", type=Path, required=True)

    index = commands.add_parser("index", help=f"write {INDEX_FILE} for unpacked builds")
    index.add_argument("directory", type=Path)

    args = parser.parse_args()

    if args.command == "select":
        releases = json.loads(args.releases_file.read_text(encoding="utf-8"))
        if not isinstance(releases, list):
            raise RuntimeError("GitHub release data must be a JSON array")
        branches = set(args.branches_file.read_text(encoding="utf-8").split())
        if not branches:
            # An empty list would mark every build stale.
            raise RuntimeError("No repository branches were found")

        builds, stale = select_builds(releases, branches)
        for _branch, tag, slug in builds:
            print(f"{tag}\t{slug}")
        args.stale.write_text("".join(f"{tag}\n" for tag in stale), encoding="utf-8")
        return

    args.directory.mkdir(parents=True, exist_ok=True)
    (args.directory / INDEX_FILE).write_text(
        json.dumps(create_index(args.directory), indent=4) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
