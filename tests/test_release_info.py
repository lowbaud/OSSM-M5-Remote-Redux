import sys
import unittest
from pathlib import Path


PROJECT_DIR = Path(__file__).resolve().parent.parent
SCRIPTS_DIR = PROJECT_DIR / "scripts"
sys.path.insert(0, str(SCRIPTS_DIR))

from build_metadata import BuildMetadata  # noqa: E402
from project_metadata import ProjectMetadata  # noqa: E402
from write_release_info import create_release_info  # noqa: E402


class ReleaseInfoTests(unittest.TestCase):
    def setUp(self):
        self.metadata = ProjectMetadata(
            display_name="Redux Test",
            artifact_name="Redux-Test",
            version="1.2.3",
            default_charge_current_ma=500,
        )
        self.build_metadata = BuildMetadata(
            commit="a" * 40,
            short_commit="a" * 8,
            build_version="v1.2.3+aaaaaaaa",
            dirty=False,
            available=True,
            release=False,
        )

    def test_development_release_includes_source_ref(self):
        release_info = create_release_info(
            metadata=self.metadata,
            build_metadata=self.build_metadata,
            tag="development-12-aaaaaaaa",
            commit="a" * 40,
            channel="development",
            timestamp="2026-07-30T10:00:00Z",
            source_ref="feature/test-build",
        )

        self.assertEqual(release_info["channel"], "development")
        self.assertEqual(release_info["sourceRef"], "feature/test-build")
        self.assertEqual(release_info["commit"], "a" * 40)

    def test_official_release_omits_missing_source_ref(self):
        release_info = create_release_info(
            metadata=self.metadata,
            build_metadata=self.build_metadata,
            tag="v1.2.3",
            commit="a" * 40,
            channel="release",
            timestamp="2026-07-30T10:00:00Z",
        )

        self.assertNotIn("sourceRef", release_info)

    def test_development_release_requires_source_ref(self):
        with self.assertRaisesRegex(ValueError, "requires a source ref"):
            create_release_info(
                metadata=self.metadata,
                build_metadata=self.build_metadata,
                tag="development-12-aaaaaaaa",
                commit="a" * 40,
                channel="development",
                timestamp="2026-07-30T10:00:00Z",
            )

    def test_mismatched_commit_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "does not match source commit"):
            create_release_info(
                metadata=self.metadata,
                build_metadata=self.build_metadata,
                tag="development-12-bbbbbbbb",
                commit="b" * 40,
                channel="development",
                timestamp="2026-07-30T10:00:00Z",
                source_ref="develop",
            )


if __name__ == "__main__":
    unittest.main()
