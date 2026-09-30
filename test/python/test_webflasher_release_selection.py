import unittest

from scripts.select_webflasher_release import select_release


class WebflasherReleaseSelectionTests(unittest.TestCase):
    def setUp(self):
        self.releases = [
            {
                "tagName": "development-14-bbbbbbbb",
                "isDraft": False,
                "isPrerelease": True,
                "publishedAt": "2026-07-30T12:00:00Z",
            },
            {
                "tagName": "v1.2.3",
                "isDraft": False,
                "isPrerelease": False,
                "publishedAt": "2026-07-29T12:00:00Z",
            },
            {
                "tagName": "development-13-aaaaaaaa",
                "isDraft": False,
                "isPrerelease": True,
                "publishedAt": "2026-07-28T12:00:00Z",
            },
            {
                "tagName": "v1.3.0-rc.1",
                "isDraft": False,
                "isPrerelease": True,
                "publishedAt": "2026-07-31T12:00:00Z",
            },
            {
                "tagName": "v2.0.0",
                "isDraft": True,
                "isPrerelease": False,
                "publishedAt": "2026-08-01T12:00:00Z",
            },
        ]

    def test_official_selection_excludes_prereleases_and_drafts(self):
        self.assertEqual(select_release(self.releases, "release"), "v1.2.3")

    def test_development_selection_uses_latest_matching_prerelease(self):
        self.assertEqual(
            select_release(self.releases, "development"),
            "development-14-bbbbbbbb",
        )

    def test_development_selection_ignores_unrelated_prereleases(self):
        unrelated = [self.releases[3]]

        self.assertIsNone(select_release(unrelated, "development"))


if __name__ == "__main__":
    unittest.main()
