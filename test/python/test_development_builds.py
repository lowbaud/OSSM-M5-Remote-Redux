import json
import tempfile
import unittest
from pathlib import Path

from scripts.development_builds import branch_slug, create_index, select_builds


def development_release(branch, tag, published_at, **overrides):
    release = {
        "tagName": tag,
        "name": f"Development build from {branch} ({tag})",
        "isDraft": False,
        "isPrerelease": True,
        "publishedAt": published_at,
    }
    release.update(overrides)
    return release


class SelectBuildsTests(unittest.TestCase):
    def test_keeps_the_newest_build_of_each_branch(self):
        releases = [
            development_release("develop", "development-3-1-cccccccc", "2026-08-03T12:00:00Z"),
            development_release("develop", "development-1-1-aaaaaaaa", "2026-08-01T12:00:00Z"),
            development_release(
                "feature/menu", "development-2-1-bbbbbbbb", "2026-08-02T12:00:00Z"
            ),
        ]

        builds, stale = select_builds(releases, {"develop", "feature/menu"})

        self.assertEqual(
            builds,
            [
                ("develop", "development-3-1-cccccccc", "develop"),
                ("feature/menu", "development-2-1-bbbbbbbb", "feature-menu"),
            ],
        )
        self.assertEqual(stale, ["development-1-1-aaaaaaaa"])

    def test_builds_of_deleted_branches_are_stale(self):
        releases = [
            development_release("old", "development-1-1-aaaaaaaa", "2026-08-01T12:00:00Z"),
        ]

        builds, stale = select_builds(releases, {"develop"})

        self.assertEqual(builds, [])
        self.assertEqual(stale, ["development-1-1-aaaaaaaa"])

    def test_other_releases_are_left_alone(self):
        releases = [
            development_release(
                "develop", "v1.3.0-rc.1", "2026-08-01T12:00:00Z", name="Release candidate"
            ),
            development_release(
                "develop", "development-1-1-aaaaaaaa", "2026-08-01T12:00:00Z", isPrerelease=False
            ),
            development_release(
                "develop", "development-2-1-bbbbbbbb", "2026-08-02T12:00:00Z", isDraft=True
            ),
            development_release(
                "develop",
                "development-3-1-cccccccc",
                "2026-08-03T12:00:00Z",
                name="Hand-written title",
            ),
        ]

        self.assertEqual(select_builds(releases, {"develop"}), ([], []))

    def test_branches_with_parentheses_parse_from_the_title(self):
        releases = [
            development_release("fix(ui)", "development-1-1-aaaaaaaa", "2026-08-01T12:00:00Z"),
        ]

        builds, _stale = select_builds(releases, {"fix(ui)"})

        self.assertEqual(builds, [("fix(ui)", "development-1-1-aaaaaaaa", "fix-ui")])


class BranchSlugTests(unittest.TestCase):
    def test_colliding_slugs_get_a_suffix(self):
        taken = set()

        self.assertEqual(branch_slug("feature/menu", taken), "feature-menu")
        self.assertEqual(branch_slug("feature-menu", taken), "feature-menu-2")

    def test_slugs_do_not_start_with_folders_jekyll_skips(self):
        self.assertEqual(branch_slug("_draft", set()), "draft")
        self.assertEqual(branch_slug(".hidden", set()), "hidden")

    def test_unusable_names_get_a_fallback(self):
        self.assertEqual(branch_slug("///", set()), "branch")


class CreateIndexTests(unittest.TestCase):
    def write_build(self, directory, path, **release):
        build_dir = Path(directory) / path
        build_dir.mkdir()
        (build_dir / "release-info.json").write_text(json.dumps(release), encoding="utf-8")

    def test_default_branch_comes_first_then_newest(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write_build(
                directory,
                "older",
                channel="development",
                sourceRef="older",
                timestamp="2026-08-01T12:00:00Z",
            )
            self.write_build(
                directory,
                "develop",
                channel="development",
                sourceRef="develop",
                timestamp="2026-08-02T12:00:00Z",
            )
            self.write_build(
                directory,
                "newer",
                channel="development",
                sourceRef="newer",
                timestamp="2026-08-03T12:00:00Z",
            )
            self.write_build(directory, "official", channel="release")

            index = create_index(directory)

        self.assertEqual(
            [build["path"] for build in index["builds"]], ["develop", "newer", "older"]
        )
        self.assertEqual(index["builds"][1]["sourceRef"], "newer")


if __name__ == "__main__":
    unittest.main()
