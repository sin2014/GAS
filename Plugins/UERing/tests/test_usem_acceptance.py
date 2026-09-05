import json
import sqlite3
import tempfile
import unittest
from pathlib import Path

from tools.usem_acceptance import validate_acceptance


class UseMAcceptanceTests(unittest.TestCase):
    @staticmethod
    def _fixture(root: Path) -> Path:
        index_dir = root / ".uesem" / "index"
        index_dir.mkdir(parents=True)
        index = {
            "project": {"name": "TestProject"},
            "statistics": {"semanticFileBytes": 100},
            "assets": [{
                "packageName": "/Game/A",
                "assetClass": "SpecialGraph",
                "semanticFile": ".uesem/content/Game/A.uesem.json",
                "status": "ok",
                "domains": ["gas"],
            }],
        }
        graph = {
            "nodes": [
                {"id": "asset:/Game/A"},
                {"id": "asset:/Game/B"},
            ],
            "edges": [
                {
                    "from": "asset:/Game/A",
                    "relation": "uses",
                    "to": "asset:/Game/B",
                    "qualifier": "",
                }
            ],
        }
        (index_dir / "project.uesem.index.json").write_text(json.dumps(index), encoding="utf-8")
        (index_dir / "project.uesem.graph.json").write_text(json.dumps(graph), encoding="utf-8")
        semantic_dir = root / ".uesem" / "content" / "Game"
        semantic_dir.mkdir(parents=True)
        semantic = {
            "exporter": "Special",
            "semantics": {
                "kind": "SpecialGraph",
                "representation": "special-v1",
                "rootGraph": {
                    "pages": [{
                        "nodes": [{"id": "n1"}, {"id": "n2", "inputLiterals": [{}]}],
                        "edges": [{"fromNodeId": "n1", "toNodeId": "n2"}],
                        "variables": [{}],
                    }]
                },
            },
        }
        (semantic_dir / "A.uesem.json").write_text(json.dumps(semantic), encoding="utf-8")
        database = sqlite3.connect(index_dir / "project.uesem.sqlite")
        database.executescript(
            """
            PRAGMA user_version=6;
            CREATE TABLE assets(package_name TEXT PRIMARY KEY);
            CREATE TABLE graph_nodes(node_id TEXT PRIMARY KEY);
            CREATE TABLE graph_edges(source_node TEXT, target_node TEXT, relation TEXT);
            INSERT INTO assets VALUES('/Game/A');
            INSERT INTO graph_nodes VALUES('asset:/Game/A');
            INSERT INTO graph_nodes VALUES('asset:/Game/B');
            INSERT INTO graph_edges VALUES('asset:/Game/A', 'asset:/Game/B', 'uses');
            """
        )
        database.close()
        expectations = {
            "project": "TestProject",
            "minimums": {"assets": 1, "graphNodes": 2, "graphEdges": 1, "semanticFileBytes": 100},
            "maximums": {"semanticFileBytes": 100},
            "minimumDomains": {"gas": 1},
            "minimumSqliteUserVersion": 6,
            "forbiddenPackagePrefixes": ["/Game/UERingTests/"],
            "requiredEdges": [
                {"from": "asset:/Game/A", "relation": "uses", "to": "asset:/Game/B"}
            ],
        }
        path = root / "expectations.json"
        path.write_text(json.dumps(expectations), encoding="utf-8")
        return path

    def test_valid_project_passes(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            expectations = self._fixture(root)
            errors = validate_acceptance(root, expectations)
        self.assertEqual([], errors)

    def test_missing_golden_edge_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            expectations = self._fixture(root)
            data = json.loads(expectations.read_text(encoding="utf-8"))
            data["requiredEdges"][0]["to"] = "asset:/Game/Missing"
            expectations.write_text(json.dumps(data), encoding="utf-8")
            errors = validate_acceptance(root, expectations)
        self.assertTrue(any("missing required edge" in error for error in errors))

    def test_maximum_size_regression_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            expectations = self._fixture(root)
            data = json.loads(expectations.read_text(encoding="utf-8"))
            data["maximums"]["semanticFileBytes"] = 99
            expectations.write_text(json.dumps(data), encoding="utf-8")
            errors = validate_acceptance(root, expectations)
        self.assertTrue(any("semanticFileBytes above maximum" in error for error in errors))

    def test_asset_json_pointer_contracts(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            expectations = self._fixture(root)
            data = json.loads(expectations.read_text(encoding="utf-8"))
            data["assetContracts"] = [{
                "packageName": "/Game/A",
                "requiredPointers": [
                    {"pointer": "/semantics/representation", "equals": "special-v1"},
                    {"pointer": "/semantics/rootGraph/pages/0/nodes", "minimumLength": 2},
                ],
                "forbiddenPointers": ["/semantics/properties"],
            }]
            expectations.write_text(json.dumps(data), encoding="utf-8")
            self.assertEqual([], validate_acceptance(root, expectations))
            data["assetContracts"][0]["requiredPointers"][0]["equals"] = "wrong"
            expectations.write_text(json.dumps(data), encoding="utf-8")
            errors = validate_acceptance(root, expectations)
        self.assertTrue(any("expected 'wrong'" in error for error in errors))

    def test_sqlite_dangling_endpoint_fails(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            expectations = self._fixture(root)
            database = sqlite3.connect(root / ".uesem" / "index" / "project.uesem.sqlite")
            database.execute("DELETE FROM graph_nodes WHERE node_id='asset:/Game/B'")
            database.commit()
            database.close()
            errors = validate_acceptance(root, expectations)
        self.assertTrue(any("dangling edge endpoints" in error for error in errors))

    def test_specialized_semantic_contract_passes(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            expectations = self._fixture(root)
            data = json.loads(expectations.read_text(encoding="utf-8"))
            data["semanticContracts"] = [{
                "name": "special",
                "assetClasses": ["SpecialGraph"],
                "minimumAssets": 1,
                "exporter": "Special",
                "kind": "SpecialGraph",
                "representation": "special-v1",
                "forbiddenSemanticFields": ["properties"],
                "verifyPageEdgeEndpoints": True,
                "minimumTotals": {"pages": 1, "nodes": 2, "edges": 1, "variables": 1, "literals": 1},
            }]
            expectations.write_text(json.dumps(data), encoding="utf-8")
            errors = validate_acceptance(root, expectations)
        self.assertEqual([], errors)

    def test_material_connection_contract_detects_dangling_endpoint(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            expectations = self._fixture(root)
            semantic_path = root / ".uesem" / "content" / "Game" / "A.uesem.json"
            semantic = json.loads(semantic_path.read_text(encoding="utf-8"))
            semantic["semantics"] = {
                "kind": "MaterialLogic",
                "representation": "material-expression-graph-v1",
                "nodes": [{"id": "n1"}],
                "connections": [{
                    "sourceNode": "n1",
                    "sourceOutputIndex": 0,
                    "targetNode": "missing",
                    "targetInput": "A",
                }],
                "danglingConnectionCount": 0,
            }
            semantic["exporter"] = "MaterialLogic"
            semantic_path.write_text(json.dumps(semantic), encoding="utf-8")
            data = json.loads(expectations.read_text(encoding="utf-8"))
            data["semanticContracts"] = [{
                "name": "material",
                "assetClasses": ["SpecialGraph"],
                "minimumAssets": 1,
                "exporter": "MaterialLogic",
                "kind": "MaterialLogic",
                "representation": "material-expression-graph-v1",
                "verifyMaterialConnectionEndpoints": True,
            }]
            expectations.write_text(json.dumps(data), encoding="utf-8")
            errors = validate_acceptance(root, expectations)
        self.assertTrue(any("dangling material connection" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
