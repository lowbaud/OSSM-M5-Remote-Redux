import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import release_build
from scripts.project_metadata import ProjectMetadata, firmware_stem


class ReleaseBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.original_firmware_dir = release_build.FIRMWARE_DIR
        self.original_output_dir = release_build.OUTPUT_DIR
        self.addCleanup(self.restore_directories)

        root = Path(self.temp_dir.name)
        release_build.FIRMWARE_DIR = root / "firmware"
        release_build.OUTPUT_DIR = root / "release"
        release_build.FIRMWARE_DIR.mkdir()
        release_build.OUTPUT_DIR.mkdir()
        self.metadata = ProjectMetadata(
            display_name="Redux Test",
            artifact_name="Redux-Test",
            version="1.2.3",
            default_charge_current_ma=500,
        )

    def restore_directories(self):
        release_build.FIRMWARE_DIR = self.original_firmware_dir
        release_build.OUTPUT_DIR = self.original_output_dir

    def write_intermediate_artifacts(self, env_name, valid_hashes=True):
        stem = firmware_stem(self.metadata, env_name)
        artifacts = {}

        for artifact_type, offset in (("factory", 0), ("update", 0x10000)):
            bin_file = release_build.FIRMWARE_DIR / f"{stem}_{artifact_type}.bin"
            bin_file.write_bytes(f"{env_name}-{artifact_type}".encode())
            digest = hashlib.sha256(bin_file.read_bytes()).hexdigest()
            artifacts[artifact_type] = {
                "filename": bin_file.name,
                "offset": offset,
                "sha256": digest if valid_hashes else "0" * 64,
            }

        descriptor = {
            "schemaVersion": release_build.ARTIFACT_DESCRIPTOR_SCHEMA,
            "environment": env_name,
            "artifacts": artifacts,
        }
        descriptor_file = release_build.intermediate_descriptor_path(
            self.metadata, env_name
        )
        descriptor_file.write_text(json.dumps(descriptor), encoding="utf-8")

    def test_copy_release_files_publishes_both_artifact_types(self):
        env_name = "m5stack-core2"
        self.write_intermediate_artifacts(env_name)

        artifacts = release_build.copy_release_files(
            self.metadata, env_name, charge_current=500
        )

        self.assertEqual(artifacts["factory"]["offset"], 0)
        self.assertEqual(artifacts["update"]["offset"], 0x10000)
        for artifact_type in release_build.ARTIFACT_TYPES:
            release_file = release_build.OUTPUT_DIR / artifacts[artifact_type][
                "filename"
            ]
            self.assertTrue(release_file.is_file())
            self.assertTrue(release_file.with_suffix(".sha256").is_file())
            self.assertIn(f"_{artifact_type}.bin", release_file.name)

    def test_charge_current_selection_defaults_to_all_supported_currents(self):
        self.assertEqual(
            release_build.resolve_charge_currents(None),
            release_build.CHARGE_CURRENTS,
        )

    def test_charge_current_selection_preserves_unique_requested_currents(self):
        self.assertEqual(
            release_build.resolve_charge_currents([500, 500]),
            [500],
        )

    def test_charge_current_argument_accepts_development_current(self):
        args = release_build.parse_arguments(["--charge-current", "500"])

        self.assertEqual(args.charge_currents, [500])

    def test_descriptor_hash_mismatch_is_rejected(self):
        env_name = "m5stack-core2"
        self.write_intermediate_artifacts(env_name, valid_hashes=False)

        with self.assertRaisesRegex(RuntimeError, "SHA-256 mismatch"):
            release_build.require_intermediate_artifacts(self.metadata, env_name)

    def test_clear_intermediate_firmware_removes_current_and_legacy_outputs(self):
        env_name = "m5stack-core2"
        stem = firmware_stem(self.metadata, env_name)
        artifact_files = [
            release_build.FIRMWARE_DIR / f"{stem}.bin",
            release_build.FIRMWARE_DIR / f"{stem}_factory.bin",
            release_build.FIRMWARE_DIR / f"{stem}_update.bin",
        ]
        descriptor_file = release_build.intermediate_descriptor_path(
            self.metadata, env_name
        )

        for artifact_file in artifact_files:
            artifact_file.write_bytes(b"stale")
            artifact_file.with_suffix(".md5").write_text("stale", encoding="utf-8")
            artifact_file.with_suffix(".sha256").write_text(
                "stale", encoding="utf-8"
            )
        descriptor_file.write_text("{}", encoding="utf-8")

        release_build.clear_intermediate_firmware(self.metadata, env_name)

        for artifact_file in artifact_files:
            self.assertFalse(artifact_file.exists())
            self.assertFalse(artifact_file.with_suffix(".md5").exists())
            self.assertFalse(artifact_file.with_suffix(".sha256").exists())
        self.assertFalse(descriptor_file.exists())

    def test_manifests_use_artifact_specific_paths_and_offsets(self):
        firmware_artifacts = {
            "m5stack-core2": {
                "factory": {"filename": "core2-factory.bin", "offset": 0},
                "update": {"filename": "core2-update.bin", "offset": 0x10000},
            },
            "m5stack-cores3": {
                "factory": {"filename": "cores3-factory.bin", "offset": 0},
                "update": {"filename": "cores3-update.bin", "offset": 0x10000},
            },
        }

        for artifact_type, expected_offset in (("factory", 0), ("update", 0x10000)):
            release_build.create_webflasher_manifest(
                metadata=self.metadata,
                charge_current=500,
                artifact_type=artifact_type,
                firmware_artifacts=firmware_artifacts,
            )
            manifest_file = release_build.OUTPUT_DIR / (
                f"manifest_{artifact_type}_500mA.json"
            )
            manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
            self.assertEqual(len(manifest["builds"]), 2)
            for build in manifest["builds"]:
                self.assertEqual(build["parts"][0]["offset"], expected_offset)
                self.assertIn(artifact_type, build["parts"][0]["path"])


if __name__ == "__main__":
    unittest.main()
