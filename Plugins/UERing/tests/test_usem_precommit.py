import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from tools.usem_precommit import Change, parse_manifest, validate_changes


class UseMPreCommitTests(unittest.TestCase):
    @staticmethod
    def _write_pair(root: Path, relative: str = "Blueprints/BP_Test") -> tuple[Path, Path]:
        source = root / "Content" / f"{relative}.uasset"
        semantic = root / ".uesem" / "content" / "Game" / f"{relative}.uesem.json"
        source.parent.mkdir(parents=True, exist_ok=True)
        semantic.parent.mkdir(parents=True, exist_ok=True)
        source.write_bytes(b"test-uasset")
        source_hash = f"sha256:{hashlib.sha256(source.read_bytes()).hexdigest()}"
        semantic.write_text(
            json.dumps(
                {
                    "asset": {
                        "packageName": f"/Game/{relative}",
                        "sourceHash": source_hash,
                    }
                }
            ),
            encoding="utf-8",
        )
        return source, semantic

    def test_synchronized_asset_and_semantic_pass(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            self._write_pair(root)
            result = validate_changes(
                root,
                [
                    Change("M", "Content/Blueprints/BP_Test.uasset"),
                    Change("M", ".uesem/content/Game/Blueprints/BP_Test.uesem.json"),
                ],
            )
        self.assertTrue(result.ok)
        self.assertEqual(1, result.checked_assets)

    def test_missing_paired_semantic_change_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            self._write_pair(root)
            result = validate_changes(
                root,
                [Change("M", "Content/Blueprints/BP_Test.uasset")],
            )
        self.assertFalse(result.ok)
        self.assertTrue(any("not in the changelist" in error for error in result.errors))

    def test_stale_source_hash_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source, _ = self._write_pair(root)
            source.write_bytes(b"changed-after-export")
            result = validate_changes(
                root,
                [
                    Change("M", "Content/Blueprints/BP_Test.uasset"),
                    Change("M", ".uesem/content/Game/Blueprints/BP_Test.uesem.json"),
                ],
            )
        self.assertFalse(result.ok)
        self.assertTrue(any("stale sourceHash" in error for error in result.errors))

    def test_deleted_asset_requires_deleted_semantic(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            result = validate_changes(
                root,
                [
                    Change("D", "Content/Blueprints/BP_Test.uasset"),
                    Change("D", ".uesem/content/Game/Blueprints/BP_Test.uesem.json"),
                ],
            )
        self.assertTrue(result.ok)

    def test_rename_manifest_expands_old_and_new_paths(self):
        changes = parse_manifest(
            "R\tContent/Old.uasset\tContent/New.uasset\n"
            "R\t.uesem/content/Game/Old.uesem.json\t.uesem/content/Game/New.uesem.json\n"
        )
        self.assertEqual(
            ["D", "A", "D", "A"],
            [change.status for change in changes],
        )

    def test_require_index_detects_missing_index_change(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            self._write_pair(root)
            result = validate_changes(
                root,
                [
                    Change("M", "Content/Blueprints/BP_Test.uasset"),
                    Change("M", ".uesem/content/Game/Blueprints/BP_Test.uesem.json"),
                ],
                require_index=True,
            )
        self.assertFalse(result.ok)
        self.assertTrue(any("project index" in error for error in result.errors))

    def test_sidecar_only_modification_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            result = validate_changes(
                root,
                [Change("M", ".uesem/content/Game/Blueprints/BP_Test.uesem.json")],
            )

        self.assertFalse(result.ok)
        self.assertTrue(any("paired Unreal asset" in error for error in result.errors))

    def test_sidecar_only_deletion_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            result = validate_changes(
                root,
                [Change("D", ".uesem/maps/Game/Maps/TestMap.uesem.json")],
            )

        self.assertFalse(result.ok)
        self.assertTrue(any("Content/Maps/TestMap.umap" in error for error in result.errors))

    def test_root_sidecar_only_modification_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            result = validate_changes(
                root,
                [Change("M", ".uesem/content/Game/RootAsset.uesem.json")],
            )

        self.assertFalse(result.ok)
        self.assertTrue(any("Content/RootAsset.uasset" in error for error in result.errors))


if __name__ == "__main__":
    unittest.main()
