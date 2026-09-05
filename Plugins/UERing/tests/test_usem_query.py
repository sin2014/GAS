import sqlite3
import tempfile
import unittest
from pathlib import Path

from tools.usem_query import find_graph_path, query_assets, query_graph_neighbors, search_graph_nodes


class UseMQueryTests(unittest.TestCase):
    @staticmethod
    def _database(root: Path) -> Path:
        path = root / "project.uesem.sqlite"
        connection = sqlite3.connect(path)
        connection.executescript(
            """
            CREATE TABLE assets(
              package_name TEXT PRIMARY KEY, object_path TEXT, asset_class TEXT, status TEXT,
              semantic_kind TEXT, exporter TEXT, semantic_file TEXT, owner_module TEXT,
              primary_asset_id TEXT, dependency_count INTEGER, referencer_count INTEGER,
              search_text TEXT
            );
            CREATE TABLE dependencies(source_package TEXT, target_package TEXT, kind TEXT);
            CREATE TABLE graph_nodes(
              node_id TEXT PRIMARY KEY, kind TEXT, subtype TEXT, label TEXT,
              package_name TEXT, search_text TEXT
            );
            CREATE TABLE graph_edges(
              source_node TEXT, target_node TEXT, relation TEXT, qualifier TEXT,
              confidence REAL, evidence_source TEXT, evidence_pointer TEXT,
              source_node_id TEXT, source_title TEXT, source_pin_id TEXT, target_pin_id TEXT
            );
            INSERT INTO assets VALUES(
              '/Game/BP_Door', '/Game/BP_Door.BP_Door', 'Blueprint', 'ok', 'Blueprint',
              'Blueprint', '.uesem/content/Game/BP_Door.uesem.json', 'Sample', '', 1, 0,
              '/Game/BP_Door Blueprint Door Interactable'
            );
            INSERT INTO assets VALUES(
              '/Game/M_Test', '/Game/M_Test.M_Test', 'Material', 'ok', 'Material',
              'DomainGraph', '.uesem/content/Game/M_Test.uesem.json', 'Sample', '', 0, 1,
              '/Game/M_Test Material'
            );
            INSERT INTO dependencies VALUES('/Game/BP_Door', '/Game/M_Test', 'hard');
            INSERT INTO graph_nodes VALUES(
              'asset:/Game/BP_Door', 'asset', 'Blueprint', 'BP_Door', '/Game/BP_Door',
              '/Game/BP_Door BP_Door Blueprint Interactable'
            );
            INSERT INTO graph_nodes VALUES(
              'asset:/Game/M_Test', 'asset', 'Material', 'M_Test', '/Game/M_Test',
              '/Game/M_Test M_Test Material'
            );
            INSERT INTO graph_nodes VALUES(
              'symbol:function:test#open', 'symbol', 'function', 'Open', '',
              'symbol function Open'
            );
            INSERT INTO graph_edges VALUES(
              'asset:/Game/BP_Door', 'asset:/Game/M_Test', 'referencesAsset', '', 1.0,
              '.uesem/content/Game/BP_Door.uesem.json', '/semantics/properties/0', '', '', '', ''
            );
            INSERT INTO graph_edges VALUES(
              'asset:/Game/M_Test', 'symbol:function:test#open', 'calls', '', 1.0,
              '.uesem/content/Game/M_Test.uesem.json', '/semantics/graphs/0', '', '', '', ''
            );
            """
        )
        connection.commit()
        connection.close()
        return path

    def test_text_and_kind_filters(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            database = self._database(Path(temp_dir))
            rows = query_assets(database, text="Interactable", semantic_kind="Blueprint")
        self.assertEqual(["/Game/BP_Door"], [row["package_name"] for row in rows])

    def test_dependency_filter(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            database = self._database(Path(temp_dir))
            rows = query_assets(database, depends_on="/Game/M_Test")
        self.assertEqual(["/Game/BP_Door"], [row["package_name"] for row in rows])

    def test_parameterized_text_treats_wildcards_literally(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            database = self._database(Path(temp_dir))
            rows = query_assets(database, text="%")
        self.assertEqual([], rows)

    def test_graph_neighbor_direction_and_relation(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            database = self._database(Path(temp_dir))
            rows = query_graph_neighbors(
                database,
                "asset:/Game/M_Test",
                direction="incoming",
                relation="referencesAsset",
            )
        self.assertEqual(["asset:/Game/BP_Door"], [row["source_node"] for row in rows])

    def test_graph_node_search(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            database = self._database(Path(temp_dir))
            rows = search_graph_nodes(database, text="Open", kind="symbol")
        self.assertEqual(["symbol:function:test#open"], [row["node_id"] for row in rows])

    def test_bounded_graph_path(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            database = self._database(Path(temp_dir))
            path = find_graph_path(
                database,
                "asset:/Game/BP_Door",
                "symbol:function:test#open",
                max_depth=2,
            )
        self.assertIsNotNone(path)
        self.assertEqual(
            ["asset:/Game/BP_Door", "asset:/Game/M_Test", "symbol:function:test#open"],
            path["nodes"],
        )


if __name__ == "__main__":
    unittest.main()
