"""Read source-control metadata to embed in firmware builds."""

import subprocess
from dataclasses import dataclass
from pathlib import Path


UNKNOWN_COMMIT = "unknown"
SHORT_COMMIT_LENGTH = 8


@dataclass(frozen=True)
class BuildMetadata:
    commit: str
    short_commit: str
    build_version: str
    dirty: bool
    available: bool
    release: bool


def _run_git(project_dir, *args):
    result = subprocess.run(
        ["git", *args],
        cwd=project_dir,
        check=True,
        capture_output=True,
        text=True,
        timeout=10,
    )
    return result.stdout.strip()


def _is_prerelease(version):
    return "-" in version.split("+", 1)[0]


def _format_build_version(version, short_commit, dirty, available, release):
    if release:
        return f"v{version}"

    separator = "." if "+" in version else "+"
    provenance = short_commit if available else UNKNOWN_COMMIT
    dirty_suffix = ".dirty" if available and dirty else ""
    return f"v{version}{separator}{provenance}{dirty_suffix}"


def load_build_metadata(project_dir, version):
    project_dir = Path(project_dir)

    try:
        commit = _run_git(project_dir, "rev-parse", "--verify", "HEAD")
        status = _run_git(
            project_dir,
            "status",
            "--porcelain=v1",
            "--untracked-files=normal",
        )
        tags = _run_git(project_dir, "tag", "--points-at", "HEAD").splitlines()
    except (OSError, subprocess.SubprocessError):
        return BuildMetadata(
            commit=UNKNOWN_COMMIT,
            short_commit=UNKNOWN_COMMIT,
            build_version=_format_build_version(
                version,
                UNKNOWN_COMMIT,
                dirty=False,
                available=False,
                release=False,
            ),
            dirty=False,
            available=False,
            release=False,
        )

    dirty = bool(status)
    matching_tags = {f"v{version}", f"V{version}"}
    release = not _is_prerelease(version) and not dirty and bool(matching_tags.intersection(tags))
    short_commit = commit[:SHORT_COMMIT_LENGTH]

    return BuildMetadata(
        commit=commit,
        short_commit=short_commit,
        build_version=_format_build_version(
            version,
            short_commit,
            dirty=dirty,
            available=True,
            release=release,
        ),
        dirty=dirty,
        available=True,
        release=release,
    )
