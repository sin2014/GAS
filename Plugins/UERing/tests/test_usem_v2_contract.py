import json
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]


class UseMV2ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(
            (PROJECT_ROOT / "examples" / "BP_Door.uesem.json").read_text(encoding="utf-8")
        )

    def test_asset_header_is_local_and_compact(self):
        self.assertEqual("2.9.0", self.document["schemaVersion"])
        self.assertEqual(44, self.document["semanticRevision"])
        self.assertEqual("Blueprint", self.document["exporter"])
        self.assertEqual("logic", self.document["profile"])
        self.assertNotIn("engine", self.document)
        self.assertNotIn("project", self.document)
        self.assertNotIn("semanticFile", self.document["asset"])
        self.assertNotIn("exportedAtUtc", self.document["asset"])

    def test_blueprint_edges_preserve_complete_pin_connectivity(self):
        for graph in self.document["semantics"]["graphs"]:
            pins = {
                pin["id"]: pin
                for node in graph["nodes"]
                for pin in node["pins"]
            }
            self.assertEqual(len(pins), sum(len(node["pins"]) for node in graph["nodes"]))
            for pin in pins.values():
                self.assertNotIn("links", pin)
            for edge in graph.get("links", []):
                self.assertIn(edge["fromPin"], pins)
                self.assertIn(edge["toPin"], pins)
                self.assertEqual("output", pins[edge["fromPin"]]["direction"])
                self.assertEqual("input", pins[edge["toPin"]]["direction"])

    def test_blueprint_types_are_structured(self):
        for variable in self.document["semantics"].get("variables", []):
            self.assertIsInstance(variable["type"], dict)
            self.assertTrue(variable["type"]["category"])
        for graph in self.document["semantics"]["graphs"]:
            for node in graph["nodes"]:
                for pin in node["pins"]:
                    self.assertIsInstance(pin["type"], dict)
                    self.assertTrue(pin["type"]["category"])

    def test_removed_property_and_graph_keys_do_not_reappear(self):
        banned = {
            "propertyClass",
            "exportText",
            "exportTextOmitted",
            "exportTextLength",
            "defaultValueTyped",
        }

        def walk(value):
            if isinstance(value, dict):
                self.assertTrue(banned.isdisjoint(value))
                for child in value.values():
                    walk(child)
            elif isinstance(value, list):
                for child in value:
                    walk(child)

        walk(self.document)

    def test_project_provenance_is_centralized(self):
        index = json.loads(
            (PROJECT_ROOT / "examples" / "assets.uesem-index.json").read_text(encoding="utf-8")
        )
        self.assertEqual("2.9.0", index["schemaVersion"])
        self.assertEqual("0.20.1", index["generator"]["version"])
        self.assertIn("engine", index)
        self.assertIn("project", index)
        self.assertEqual(len(index["assets"]), index["coverage"]["indexedAssetCount"])

    def test_reconstruction_ir_is_explicit_and_bounded(self):
        reconstruction = self.document["reconstruction"]
        self.assertEqual(
            {
                "irVersion",
                "contract",
                "assetKind",
                "profile",
                "source",
                "targets",
                "symbols",
                "operations",
                "losses",
                "verification",
                "coverage",
                "execution",
            },
            set(reconstruction),
        )
        self.assertEqual("2.0.0", reconstruction["irVersion"])
        self.assertEqual("com.ue-ring.reconstruction", reconstruction["contract"])
        self.assertEqual(self.document["profile"], reconstruction["profile"])
        self.assertEqual(self.document["inputFingerprint"], reconstruction["source"]["inputFingerprint"])
        self.assertEqual(self.document["asset"]["sourceHash"], reconstruction["source"]["sourceHash"])
        self.assertTrue(reconstruction["targets"])
        self.assertEqual(
            {
                "id",
                "target",
                "backend",
                "backendVersion",
                "status",
                "fidelity",
                "writePolicy",
                "blockerRefs",
            },
            set(reconstruction["targets"][0]),
        )
        self.assertEqual(["op:0001", "op:0002"], [operation["id"] for operation in reconstruction["operations"]])
        self.assertEqual(["op:0001"], reconstruction["operations"][1]["dependsOn"])
        self.assertTrue(reconstruction["execution"]["fullyExecutable"])
        self.assertEqual(1.0, reconstruction["coverage"]["exactRatio"])
        for operation in reconstruction["operations"]:
            self.assertEqual(
                {
                    "id",
                    "opcode",
                    "opcodeVersion",
                    "phase",
                    "targetId",
                    "dependsOn",
                    "operands",
                    "results",
                    "sourcePointers",
                    "preconditions",
                    "postconditions",
                    "criticality",
                    "status",
                    "fidelity",
                    "failurePolicy",
                    "idempotencyKey",
                },
                set(operation),
            )
            self.assertIn(operation["opcode"], {
                "cpp.class.declare",
                "cpp.property.declare",
            })
            self.assertEqual("executable", operation["status"])
            self.assertEqual("exact", operation["fidelity"]["status"])
            self.assertGreaterEqual(operation["fidelity"]["confidence"], 0)
            self.assertLessEqual(operation["fidelity"]["confidence"], 1)

    def test_reconstruction_schema_declares_material_instance_backend_opcodes(self):
        schema = json.loads((PROJECT_ROOT / "schemas" / "usem.asset.schema.json").read_text(encoding="utf-8"))
        operation_schema = schema["properties"]["reconstruction"]["properties"]["operations"]["items"]
        opcodes = set(operation_schema["properties"]["opcode"]["enum"])
        self.assertTrue({
            "editor.materialInstance.create",
            "editor.materialInstance.applyOverrides",
            "editor.asset.save",
        }.issubset(opcodes))

    def test_reconstruction_schema_declares_data_asset_backend_opcodes(self):
        schema = json.loads((PROJECT_ROOT / "schemas" / "usem.asset.schema.json").read_text(encoding="utf-8"))
        operation_schema = schema["properties"]["reconstruction"]["properties"]["operations"]["items"]
        opcodes = set(operation_schema["properties"]["opcode"]["enum"])
        self.assertTrue({
            "editor.dataAsset.create",
            "editor.dataAsset.createOwnedObjects",
            "editor.dataAsset.applyOwnedObjectProperties",
            "editor.dataAsset.applyProperties",
            "editor.stateTree.compile",
            "editor.asset.save",
        }.issubset(opcodes))


if __name__ == "__main__":
    unittest.main()
