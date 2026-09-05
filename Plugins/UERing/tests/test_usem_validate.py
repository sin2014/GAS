import json
import hashlib
import copy
import tempfile
import unittest
from pathlib import Path

from tools.usem_validate import RECONSTRUCTION_OPCODES, validate_file


PROJECT_ROOT = Path(__file__).resolve().parents[1]


class UseMValidateTests(unittest.TestCase):
    def test_state_tree_compile_opcode_is_supported(self):
        self.assertIn("editor.stateTree.compile", RECONSTRUCTION_OPCODES)

    @staticmethod
    def _semantic_hash(path: Path) -> str:
        return f"sha256:{hashlib.sha256(path.read_bytes()).hexdigest()}"

    @staticmethod
    def _project_graph() -> dict:
        return {
            "schema": "com.ue-ring.usem.project-graph",
            "schemaVersion": "1.1.0",
            "usemSchemaVersion": "2.9.0",
            "generatedAtUtc": "2026-08-01T00:00:00Z",
            "nodes": [
                {"id": "asset:/Game/BP_Test", "kind": "asset", "label": "BP_Test"},
                {"id": "graph:/Game/BP_Test:EventGraph", "kind": "graph", "label": "EventGraph"},
            ],
            "edges": [
                {
                    "from": "asset:/Game/BP_Test",
                    "to": "graph:/Game/BP_Test:EventGraph",
                    "relation": "containsGraph",
                    "confidence": 1.0,
                    "evidenceSource": ".uesem/content/Game/BP_Test.uesem.json",
                    "evidencePointer": "/semantics/graphs/0",
                    "contributorPackage": "/Game/BP_Test",
                }
            ],
            "statistics": {
                "nodeCount": 2,
                "edgeCount": 1,
                "nodesByKind": {"asset": 1, "graph": 1},
                "edgesByRelation": {"containsGraph": 1},
            },
        }

    def test_valid_asset_file_succeeds(self):
        result = validate_file(PROJECT_ROOT / "examples" / "BP_Door.uesem.json", PROJECT_ROOT)

        self.assertTrue(result.ok)
        self.assertEqual([], result.errors)

    def test_asset_missing_required_field_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "bad.uesem.json"
            data = json.loads((PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8"))
            del data["asset"]
            path.write_text(json.dumps(data), encoding="utf-8")

            result = validate_file(path, PROJECT_ROOT)

        self.assertFalse(result.ok)
        self.assertIn("missing required field: asset", result.errors)

    def test_blueprint_edge_with_unknown_pin_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "bad-edge.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["semantics"]["graphs"][0]["links"][0]["toPin"] = "missing-pin"
            path.write_text(json.dumps(data), encoding="utf-8")

            result = validate_file(path, PROJECT_ROOT)

        self.assertFalse(result.ok)
        self.assertTrue(any("unknown toPin" in error for error in result.errors))

    def test_legacy_blueprint_type_string_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "legacy-type.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["semantics"]["graphs"][0]["nodes"][0]["pins"][0]["type"] = "exec"
            path.write_text(json.dumps(data), encoding="utf-8")

            result = validate_file(path, PROJECT_ROOT)

        self.assertFalse(result.ok)
        self.assertTrue(any("type must be an object" in error for error in result.errors))

    def test_valid_index_file_succeeds(self):
        result = validate_file(PROJECT_ROOT / "examples" / "assets.uesem-index.json", PROJECT_ROOT)

        self.assertTrue(result.ok)
        self.assertEqual([], result.errors)

    def test_index_inside_uesem_infers_project_root(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir)
            semantic_path = project_root / ".uesem" / "content" / "Game" / "BP_Door.uesem.json"
            semantic_path.parent.mkdir(parents=True)
            semantic = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            semantic_file = ".uesem/content/Game/BP_Door.uesem.json"
            semantic["asset"]["sourceFile"] = "Content/Blueprints/BP_Door.uasset"
            semantic_path.write_text(json.dumps(semantic), encoding="utf-8")

            index_path = project_root / ".uesem" / "index" / "project.uesem.index.json"
            index_path.parent.mkdir(parents=True)
            data = json.loads(
                (PROJECT_ROOT / "examples" / "assets.uesem-index.json").read_text(encoding="utf-8")
            )
            data["assets"][0]["semanticFile"] = semantic_file
            data["assets"][0]["semanticHash"] = self._semantic_hash(semantic_path)
            index_path.write_text(json.dumps(data), encoding="utf-8")

            result = validate_file(index_path)

        self.assertTrue(result.ok)
        self.assertEqual([], result.errors)

    def test_index_missing_semantic_file_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            index_path = root / "assets.uesem-index.json"
            data = json.loads((PROJECT_ROOT / "examples" / "assets.uesem-index.json").read_text(encoding="utf-8"))
            data["assets"][0]["semanticFile"] = "examples/Missing.uesem.json"
            index_path.write_text(json.dumps(data), encoding="utf-8")

            result = validate_file(index_path, root)

        self.assertFalse(result.ok)
        self.assertIn("missing semantic file: examples/Missing.uesem.json", result.errors)

    def test_index_semantic_hash_mismatch_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            semantic_path = root / "asset.uesem.json"
            semantic_path.write_bytes((PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_bytes())
            index_path = root / "assets.uesem-index.json"
            data = json.loads((PROJECT_ROOT / "examples" / "assets.uesem-index.json").read_text(encoding="utf-8"))
            data["assets"][0]["semanticFile"] = "asset.uesem.json"
            data["assets"][0]["semanticHash"] = "sha256:incorrect"
            index_path.write_text(json.dumps(data), encoding="utf-8")

            result = validate_file(index_path, root)

        self.assertFalse(result.ok)
        self.assertTrue(any("semanticHash mismatch" in error for error in result.errors))

    def test_index_package_mismatch_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            semantic_path = root / "asset.uesem.json"
            semantic_path.write_bytes((PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_bytes())
            index_path = root / "assets.uesem-index.json"
            data = json.loads((PROJECT_ROOT / "examples" / "assets.uesem-index.json").read_text(encoding="utf-8"))
            data["assets"][0]["semanticFile"] = "asset.uesem.json"
            semantic = json.loads(semantic_path.read_text(encoding="utf-8"))
            semantic["asset"]["packageName"] = "/Game/Wrong"
            semantic_path.write_text(json.dumps(semantic), encoding="utf-8")
            data["assets"][0]["semanticHash"] = self._semantic_hash(semantic_path)
            index_path.write_text(json.dumps(data), encoding="utf-8")

            result = validate_file(index_path, root)

        self.assertFalse(result.ok)
        self.assertTrue(any("semantic packageName does not match index" in error for error in result.errors))

    def test_unsupported_index_entry_without_semantic_file_succeeds(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            index_path = root / "assets.uesem-index.json"
            data = json.loads((PROJECT_ROOT / "examples" / "assets.uesem-index.json").read_text(encoding="utf-8"))
            data["assets"][0]["status"] = "unsupported"
            data["assets"][0]["semanticFile"] = None
            index_path.write_text(json.dumps(data), encoding="utf-8")

            result = validate_file(index_path, root)

        self.assertTrue(result.ok)
        self.assertEqual([], result.errors)

    def test_unsupported_index_entry_with_semantic_file_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            index_path = root / "assets.uesem-index.json"
            data = json.loads((PROJECT_ROOT / "examples" / "assets.uesem-index.json").read_text(encoding="utf-8"))
            data["assets"][0]["status"] = "unsupported"
            index_path.write_text(json.dumps(data), encoding="utf-8")

            result = validate_file(index_path, root)

        self.assertFalse(result.ok)
        self.assertTrue(any("unsupported asset must not declare semanticFile" in error for error in result.errors))

    def test_valid_migration_report_succeeds(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "migration.uesem.json"
            path.write_text(
                json.dumps(
                    {
                        "schema": "com.ue-ring.usem.blueprint-cpp-migration",
                        "schemaVersion": "2.9.0",
                        "generatedAtUtc": "2026-07-31T00:00:00Z",
                        "method": "semantic-evidence-ranking-v1",
                        "disclaimer": "Assistance only.",
                        "candidates": [
                            {
                                "packageName": "/Game/BP_Test",
                                "priority": "low",
                                "metrics": {"nodes": 1},
                                "recommendations": ["Review behavior."],
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            result = validate_file(path, temp_dir)
        self.assertTrue(result.ok)

    def test_valid_project_graph_succeeds(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "project.uesem.graph.json"
            path.write_text(json.dumps(self._project_graph()), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertTrue(result.ok)
        self.assertEqual([], result.errors)

    def test_project_graph_dangling_edge_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "project.uesem.graph.json"
            graph = self._project_graph()
            graph["edges"][0]["to"] = "graph:missing"
            path.write_text(json.dumps(graph), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("unknown target" in error for error in result.errors))

    def test_project_graph_missing_contributor_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "project.uesem.graph.json"
            graph = self._project_graph()
            del graph["edges"][0]["contributorPackage"]
            path.write_text(json.dumps(graph), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("missing contributorPackage" in error for error in result.errors))

    def test_project_graph_rejects_duplicate_logic_with_different_evidence(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "project.uesem.graph.json"
            graph = self._project_graph()
            duplicate = dict(graph["edges"][0])
            duplicate["evidencePointer"] = "/semantics/properties/0"
            graph["edges"].append(duplicate)
            graph["statistics"]["edgeCount"] = 2
            graph["statistics"]["edgesByRelation"]["containsGraph"] = 2
            path.write_text(json.dumps(graph), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("duplicate edge" in error for error in result.errors))

    def test_project_graph_statistics_and_confidence_are_verified(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "project.uesem.graph.json"
            graph = self._project_graph()
            graph["edges"][0]["confidence"] = 1.1
            graph["statistics"]["edgeCount"] = 2
            path.write_text(json.dumps(graph), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("confidence" in error for error in result.errors))
        self.assertTrue(any("edgeCount mismatch" in error for error in result.errors))

    def test_invalid_migration_candidate_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "migration.uesem.json"
            path.write_text(
                json.dumps(
                    {
                        "schema": "com.ue-ring.usem.blueprint-cpp-migration",
                        "schemaVersion": "2.9.0",
                        "generatedAtUtc": "2026-07-31T00:00:00Z",
                        "method": "semantic-evidence-ranking-v1",
                        "disclaimer": "Assistance only.",
                        "candidates": [{"packageName": "/Game/BP_Test"}],
                    }
                ),
                encoding="utf-8",
            )
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("recommendations" in error for error in result.errors))

    def test_missing_reconstruction_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data.pop("reconstruction")
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("reconstruction" in error for error in result.errors))

    def test_stale_semantic_revision_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["semanticRevision"] = 15
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("semanticRevision" in error for error in result.errors))

    def test_invalid_reconstruction_confidence_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["reconstruction"]["operations"][0]["fidelity"]["confidence"] = 1.1
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("fidelity confidence" in error for error in result.errors))

    def test_reconstruction_source_pointer_must_resolve(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["reconstruction"]["operations"][0]["sourcePointers"] = [
                "/semantics/missing"
            ]
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("does not resolve" in error for error in result.errors))

    def test_reconstruction_duplicate_operation_id_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            duplicate = copy.deepcopy(data["reconstruction"]["operations"][0])
            duplicate["idempotencyKey"] = "duplicate-operation"
            data["reconstruction"]["operations"].append(duplicate)
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("duplicate operation id" in error for error in result.errors))

    def test_reconstruction_dependency_cycle_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["reconstruction"]["operations"][0]["dependsOn"] = ["op:0002"]
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("must be acyclic" in error for error in result.errors))

    def test_reconstruction_unknown_dependency_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["reconstruction"]["operations"][1]["dependsOn"] = ["op:missing"]
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("unknown dependency" in error for error in result.errors))

    def test_reconstruction_unknown_target_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["reconstruction"]["operations"][0]["targetId"] = "target:missing"
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("unknown target" in error for error in result.errors))

    def test_reconstruction_unknown_symbol_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["reconstruction"]["operations"][0]["operands"]["parentSymbolId"] = (
                "symbol:missing"
            )
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("unknown symbol" in error for error in result.errors))

    def test_reconstruction_unknown_loss_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["reconstruction"]["targets"][0]["blockerRefs"] = ["loss:missing"]
            data["reconstruction"]["targets"][0]["status"] = "blocked"
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("unknown loss" in error for error in result.errors))

    def test_reconstruction_summary_mismatch_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["reconstruction"]["coverage"]["totalOperationCount"] = 99
            data["reconstruction"]["execution"]["executableOperationCount"] = 1
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("totalOperationCount mismatch" in error for error in result.errors))
        self.assertTrue(any("executableOperationCount mismatch" in error for error in result.errors))

    def test_reconstruction_readiness_and_target_status_must_be_derived(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "asset.uesem.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
            )
            data["reconstruction"]["coverage"]["readiness"] = "partial"
            data["reconstruction"]["targets"][0]["status"] = "blocked"
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir)
        self.assertFalse(result.ok)
        self.assertTrue(any("readiness mismatch" in error for error in result.errors))
        self.assertTrue(any("status mismatch" in error for error in result.errors))

    def test_reconstruction_rejects_raw_code_and_shell_fields_recursively(self):
        for field in ("rawCode", "shellCommand"):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as temp_dir:
                path = Path(temp_dir) / "asset.uesem.json"
                data = json.loads(
                    (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
                )
                data["reconstruction"]["operations"][0]["operands"][field] = "unsafe"
                path.write_text(json.dumps(data), encoding="utf-8")
                result = validate_file(path, temp_dir)
            self.assertFalse(result.ok)
            self.assertTrue(any("forbids raw code or shell" in error for error in result.errors))

    def test_index_coverage_must_match_assets_and_exclusions(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "index.json"
            data = json.loads(
                (PROJECT_ROOT / "examples" / "assets.uesem-index.json").read_text(encoding="utf-8")
            )
            data["coverage"] = {
                "indexedAssetCount": 2,
                "excludedAssetCount": 4,
                "exclusions": [{"reason": "generatedExternalActor", "count": 3}],
            }
            path.write_text(json.dumps(data), encoding="utf-8")
            result = validate_file(path, temp_dir, verify_hashes=False)
        self.assertFalse(result.ok)
        self.assertTrue(any("indexedAssetCount" in error for error in result.errors))
        self.assertTrue(any("excludedAssetCount" in error for error in result.errors))


if __name__ == "__main__":
    unittest.main()

