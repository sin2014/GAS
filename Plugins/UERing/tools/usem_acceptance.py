from __future__ import annotations

import argparse
import json
import sqlite3
from collections import Counter
from pathlib import Path
from typing import Any


def _read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        value = json.load(handle)
    if not isinstance(value, dict):
        raise ValueError(f"expected a JSON object: {path}")
    return value


def _json_pointer(document: Any, pointer: str) -> Any:
    if pointer == "":
        return document
    if not isinstance(pointer, str) or not pointer.startswith("/"):
        raise ValueError(f"invalid JSON pointer: {pointer}")
    current = document
    for raw_token in pointer[1:].split("/"):
        token = raw_token.replace("~1", "/").replace("~0", "~")
        if isinstance(current, dict):
            if token not in current:
                raise KeyError(pointer)
            current = current[token]
        elif isinstance(current, list):
            try:
                current = current[int(token)]
            except (ValueError, IndexError) as exc:
                raise KeyError(pointer) from exc
        else:
            raise KeyError(pointer)
    return current


def validate_acceptance(project_root: str | Path, expectations_path: str | Path) -> list[str]:
    root = Path(project_root).resolve()
    expectations = _read_json(Path(expectations_path).resolve())
    index_dir = root / ".uesem" / "index"
    index = _read_json(index_dir / "project.uesem.index.json")
    graph = _read_json(index_dir / "project.uesem.graph.json")
    errors: list[str] = []

    expected_project = expectations.get("project")
    actual_project = index.get("project", {}).get("name")
    if actual_project != expected_project:
        errors.append(f"project mismatch: expected {expected_project}, got {actual_project}")

    assets = index.get("assets", [])
    nodes = graph.get("nodes", [])
    edges = graph.get("edges", [])
    if not all(isinstance(value, list) for value in (assets, nodes, edges)):
        return errors + ["index assets and graph nodes/edges must be arrays"]

    minimums = expectations.get("minimums", {})
    actual_counts = {
        "assets": len(assets),
        "graphNodes": len(nodes),
        "graphEdges": len(edges),
        "semanticFileBytes": index.get("statistics", {}).get("semanticFileBytes", 0),
    }
    for name, expected in minimums.items():
        actual = actual_counts.get(name)
        if not isinstance(expected, int) or actual is None:
            errors.append(f"unknown or invalid minimum: {name}")
        elif actual < expected:
            errors.append(f"{name} below minimum: expected >= {expected}, got {actual}")

    for name, expected in expectations.get("maximums", {}).items():
        actual = actual_counts.get(name)
        if not isinstance(expected, int) or actual is None:
            errors.append(f"unknown or invalid maximum: {name}")
        elif actual > expected:
            errors.append(f"{name} above maximum: expected <= {expected}, got {actual}")

    domain_counts: Counter[str] = Counter()
    for asset in assets:
        if not isinstance(asset, dict):
            continue
        if asset.get("status") != "ok":
            errors.append(f"non-ok index entry: {asset.get('packageName')}")
        for domain in asset.get("domains", []):
            if isinstance(domain, str):
                domain_counts[domain] += 1
    for domain, expected in expectations.get("minimumDomains", {}).items():
        if domain_counts[domain] < expected:
            errors.append(
                f"domain {domain} below minimum: expected >= {expected}, got {domain_counts[domain]}"
            )

    packages = {asset.get("packageName") for asset in assets if isinstance(asset, dict)}
    for prefix in expectations.get("forbiddenPackagePrefixes", []):
        matches = sorted(package for package in packages if isinstance(package, str) and package.startswith(prefix))
        if matches:
            errors.append(f"forbidden package prefix {prefix}: {matches[0]}")

    edge_keys = {
        (
            edge.get("from"),
            edge.get("relation"),
            edge.get("to"),
            edge.get("qualifier", ""),
        )
        for edge in edges
        if isinstance(edge, dict)
    }
    for expected in expectations.get("requiredEdges", []):
        key = (
            expected.get("from"),
            expected.get("relation"),
            expected.get("to"),
            expected.get("qualifier", ""),
        )
        if key not in edge_keys:
            errors.append(f"missing required edge: {key[0]} {key[1]} {key[2]} [{key[3]}]")

    assets_by_package = {
        asset.get("packageName"): asset for asset in assets if isinstance(asset, dict)
    }
    for contract in expectations.get("assetContracts", []):
        package_name = contract.get("packageName")
        asset = assets_by_package.get(package_name)
        if asset is None:
            errors.append(f"asset contract package missing: {package_name}")
            continue
        semantic_file = asset.get("semanticFile")
        if not isinstance(semantic_file, str) or not semantic_file:
            errors.append(f"asset contract semantic file missing: {package_name}")
            continue
        semantic_path = Path(semantic_file)
        if not semantic_path.is_absolute():
            semantic_path = root / semantic_path
        semantic = _read_json(semantic_path)
        for requirement in contract.get("requiredPointers", []):
            pointer = requirement.get("pointer")
            try:
                actual = _json_pointer(semantic, pointer)
            except (KeyError, ValueError):
                errors.append(f"{package_name}: missing required pointer {pointer}")
                continue
            if "equals" in requirement and actual != requirement["equals"]:
                errors.append(
                    f"{package_name}: {pointer} expected {requirement['equals']!r}, got {actual!r}"
                )
            minimum_length = requirement.get("minimumLength")
            if minimum_length is not None:
                if not isinstance(actual, (list, dict, str)) or len(actual) < minimum_length:
                    errors.append(
                        f"{package_name}: {pointer} length below minimum {minimum_length}"
                    )
        for pointer in contract.get("forbiddenPointers", []):
            try:
                _json_pointer(semantic, pointer)
            except (KeyError, ValueError):
                continue
            errors.append(f"{package_name}: forbidden pointer exists {pointer}")

    for contract in expectations.get("semanticContracts", []):
        asset_classes = set(contract.get("assetClasses", []))
        matching = [asset for asset in assets if asset.get("assetClass") in asset_classes]
        minimum_assets = contract.get("minimumAssets", 0)
        if len(matching) < minimum_assets:
            errors.append(
                f"semantic contract {contract.get('name')} assets below minimum: "
                f"expected >= {minimum_assets}, got {len(matching)}"
            )
        totals: Counter[str] = Counter()
        for asset in matching:
            semantic_file = asset.get("semanticFile")
            if not isinstance(semantic_file, str) or not semantic_file:
                errors.append(f"semantic contract asset missing semanticFile: {asset.get('packageName')}")
                continue
            semantic_path = Path(semantic_file)
            if not semantic_path.is_absolute():
                semantic_path = root / semantic_path
            semantic = _read_json(semantic_path)
            model = semantic.get("semantics", {})
            for field, expected_value in (
                ("exporter", contract.get("exporter")),
                ("kind", contract.get("kind")),
                ("representation", contract.get("representation")),
            ):
                if expected_value is None:
                    continue
                actual_value = semantic.get(field) if field == "exporter" else model.get(field)
                if actual_value != expected_value:
                    errors.append(
                        f"{asset.get('packageName')}: expected {field}={expected_value}, got {actual_value}"
                    )
            for field in contract.get("forbiddenSemanticFields", []):
                if field in model:
                    errors.append(f"{asset.get('packageName')}: forbidden semantic field {field}")
            if contract.get("verifyMaterialConnectionEndpoints"):
                material_nodes = model.get("nodes", [])
                material_connections = model.get("connections", [])
                if not isinstance(material_nodes, list) or not isinstance(material_connections, list):
                    errors.append(f"{asset.get('packageName')}: invalid material nodes/connections")
                else:
                    material_node_ids = {
                        node.get("id") for node in material_nodes if isinstance(node, dict)
                    }
                    totals["nodes"] += len(material_nodes)
                    totals["edges"] += len(material_connections)
                    if model.get("danglingConnectionCount") != 0:
                        errors.append(f"{asset.get('packageName')}: non-zero danglingConnectionCount")
                    for connection in material_connections:
                        if not isinstance(connection, dict):
                            errors.append(f"{asset.get('packageName')}: invalid material connection")
                            continue
                        source = connection.get("sourceNode")
                        target = connection.get("targetNode")
                        if source not in material_node_ids or (
                            target != "$material" and target not in material_node_ids
                        ):
                            errors.append(f"{asset.get('packageName')}: dangling material connection")
            graphs = [model.get("rootGraph", {})] + model.get("subgraphs", [])
            for graph_model in graphs:
                if not isinstance(graph_model, dict):
                    continue
                for page in graph_model.get("pages", []):
                    if not isinstance(page, dict):
                        continue
                    page_nodes = page.get("nodes", [])
                    page_edges = page.get("edges", [])
                    totals["pages"] += 1
                    totals["nodes"] += len(page_nodes)
                    totals["edges"] += len(page_edges)
                    totals["variables"] += len(page.get("variables", []))
                    totals["literals"] += sum(
                        len(node.get("inputLiterals", []))
                        for node in page_nodes
                        if isinstance(node, dict)
                    )
                    if contract.get("verifyPageEdgeEndpoints"):
                        page_node_ids = {
                            node.get("id") for node in page_nodes if isinstance(node, dict)
                        }
                        for edge in page_edges:
                            if not isinstance(edge, dict):
                                continue
                            if edge.get("fromNodeId") not in page_node_ids or edge.get("toNodeId") not in page_node_ids:
                                errors.append(f"{asset.get('packageName')}: dangling specialized graph edge")
        for name, expected in contract.get("minimumTotals", {}).items():
            if totals[name] < expected:
                errors.append(
                    f"semantic contract {contract.get('name')} {name} below minimum: "
                    f"expected >= {expected}, got {totals[name]}"
                )

    database_path = index_dir / "project.uesem.sqlite"
    connection = sqlite3.connect(f"file:{database_path.as_posix()}?mode=ro", uri=True)
    try:
        user_version = connection.execute("PRAGMA user_version").fetchone()[0]
        if user_version < expectations.get("minimumSqliteUserVersion", 0):
            errors.append(f"SQLite user_version too old: {user_version}")
        quick_check = connection.execute("PRAGMA quick_check").fetchone()[0]
        if quick_check != "ok":
            errors.append(f"SQLite quick_check failed: {quick_check}")
        sqlite_counts = {
            table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
            for table in ("assets", "graph_nodes", "graph_edges")
        }
        expected_counts = {"assets": len(assets), "graph_nodes": len(nodes), "graph_edges": len(edges)}
        if sqlite_counts != expected_counts:
            errors.append(f"SQLite/JSON count mismatch: expected {expected_counts}, got {sqlite_counts}")
        dangling = connection.execute(
            "SELECT COUNT(*) FROM graph_edges e "
            "LEFT JOIN graph_nodes s ON s.node_id=e.source_node "
            "LEFT JOIN graph_nodes t ON t.node_id=e.target_node "
            "WHERE s.node_id IS NULL OR t.node_id IS NULL"
        ).fetchone()[0]
        if dangling:
            errors.append(f"SQLite graph has {dangling} dangling edge endpoints")
    finally:
        connection.close()

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a project-scale USEM golden acceptance suite.")
    parser.add_argument("project_root", help="Unreal project containing the .uesem directory.")
    parser.add_argument("expectations", help="Golden acceptance expectation JSON.")
    args = parser.parse_args()
    try:
        errors = validate_acceptance(args.project_root, args.expectations)
    except (OSError, ValueError, json.JSONDecodeError, sqlite3.Error) as exc:
        print(f"ERROR: {exc}")
        return 2
    if not errors:
        print("OK")
        return 0
    for error in errors:
        print(f"ERROR: {error}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
