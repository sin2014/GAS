from __future__ import annotations

import argparse
import json
import sqlite3
from pathlib import Path
from typing import Any


def _connect(database: str | Path) -> sqlite3.Connection:
    connection = sqlite3.connect(f"file:{Path(database).resolve().as_posix()}?mode=ro", uri=True)
    connection.row_factory = sqlite3.Row
    return connection


def query_assets(
    database: str | Path,
    *,
    text: str | None = None,
    asset_class: str | None = None,
    semantic_kind: str | None = None,
    status: str | None = None,
    depends_on: str | None = None,
    limit: int = 100,
) -> list[dict[str, Any]]:
    conditions: list[str] = []
    parameters: list[Any] = []
    join = ""
    if text:
        conditions.append("a.search_text LIKE ? ESCAPE '\\'")
        escaped = text.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_")
        parameters.append(f"%{escaped}%")
    if asset_class:
        conditions.append("a.asset_class = ?")
        parameters.append(asset_class)
    if semantic_kind:
        conditions.append("a.semantic_kind = ?")
        parameters.append(semantic_kind)
    if status:
        conditions.append("a.status = ?")
        parameters.append(status)
    if depends_on:
        join = " JOIN dependencies d ON d.source_package = a.package_name"
        conditions.append("d.target_package = ?")
        parameters.append(depends_on)

    where = " WHERE " + " AND ".join(conditions) if conditions else ""
    sql = (
        "SELECT DISTINCT a.package_name, a.object_path, a.asset_class, a.status, "
        "a.semantic_kind, a.exporter, a.semantic_file, a.owner_module, "
        "a.primary_asset_id, a.dependency_count, a.referencer_count "
        f"FROM assets a{join}{where} ORDER BY a.package_name LIMIT ?"
    )
    parameters.append(max(1, min(limit, 10_000)))

    connection = _connect(database)
    try:
        return [dict(row) for row in connection.execute(sql, parameters)]
    finally:
        connection.close()


def query_graph_neighbors(
    database: str | Path,
    node_id: str,
    *,
    direction: str = "both",
    relation: str | None = None,
    limit: int = 100,
) -> list[dict[str, Any]]:
    if direction not in {"outgoing", "incoming", "both"}:
        raise ValueError("direction must be outgoing, incoming, or both")
    clauses: list[str] = []
    parameters: list[Any] = []
    if direction in {"outgoing", "both"}:
        clauses.append("source_node = ?")
        parameters.append(node_id)
    if direction in {"incoming", "both"}:
        clauses.append("target_node = ?")
        parameters.append(node_id)
    where = "(" + " OR ".join(clauses) + ")"
    if relation:
        where += " AND relation = ?"
        parameters.append(relation)
    parameters.append(max(1, min(limit, 10_000)))
    sql = (
        "SELECT source_node, target_node, relation, qualifier, confidence, evidence_source, "
        "evidence_pointer, source_node_id, source_title, source_pin_id, target_pin_id "
        f"FROM graph_edges WHERE {where} "
        "ORDER BY relation, source_node, target_node, qualifier LIMIT ?"
    )
    connection = _connect(database)
    try:
        return [dict(row) for row in connection.execute(sql, parameters)]
    finally:
        connection.close()


def search_graph_nodes(
    database: str | Path,
    *,
    text: str | None = None,
    kind: str | None = None,
    subtype: str | None = None,
    limit: int = 100,
) -> list[dict[str, Any]]:
    conditions: list[str] = []
    parameters: list[Any] = []
    if text:
        conditions.append("search_text LIKE ? ESCAPE '\\'")
        escaped = text.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_")
        parameters.append(f"%{escaped}%")
    if kind:
        conditions.append("kind = ?")
        parameters.append(kind)
    if subtype:
        conditions.append("subtype = ?")
        parameters.append(subtype)
    where = " WHERE " + " AND ".join(conditions) if conditions else ""
    parameters.append(max(1, min(limit, 10_000)))
    connection = _connect(database)
    try:
        return [
            dict(row)
            for row in connection.execute(
                "SELECT node_id, kind, subtype, label, package_name "
                f"FROM graph_nodes{where} ORDER BY node_id LIMIT ?",
                parameters,
            )
        ]
    finally:
        connection.close()


def find_graph_path(
    database: str | Path,
    source: str,
    target: str,
    *,
    relation: str | None = None,
    max_depth: int = 6,
) -> dict[str, Any] | None:
    depth_limit = max(1, min(max_depth, 12))
    if source == target:
        return {"nodes": [source], "edges": []}
    connection = _connect(database)
    try:
        visited = {source}
        frontier = [source]
        parent: dict[str, tuple[str, dict[str, Any]]] = {}
        for _ in range(depth_limit):
            placeholders = ",".join("?" for _ in frontier)
            parameters: list[Any] = list(frontier)
            relation_clause = ""
            if relation:
                relation_clause = " AND relation = ?"
                parameters.append(relation)
            rows = connection.execute(
                "SELECT source_node, target_node, relation, qualifier, confidence, "
                "evidence_source, evidence_pointer FROM graph_edges "
                f"WHERE source_node IN ({placeholders}){relation_clause} "
                "ORDER BY source_node, "
                "CASE relation WHEN 'dependsOn' THEN 2 WHEN 'referencesAsset' THEN 1 ELSE 0 END, "
                "relation, target_node, qualifier",
                parameters,
            )
            next_frontier: list[str] = []
            for row in rows:
                edge = dict(row)
                child = edge["target_node"]
                if child in visited:
                    continue
                visited.add(child)
                parent[child] = (edge["source_node"], edge)
                if child == target:
                    nodes = [target]
                    edges: list[dict[str, Any]] = []
                    cursor = target
                    while cursor != source:
                        previous, path_edge = parent[cursor]
                        edges.append(path_edge)
                        nodes.append(previous)
                        cursor = previous
                    nodes.reverse()
                    edges.reverse()
                    return {"nodes": nodes, "edges": edges}
                next_frontier.append(child)
            if not next_frontier:
                break
            frontier = next_frontier
    finally:
        connection.close()
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Query a UE Ring project.uesem.sqlite index.")
    parser.add_argument("database", help="Path to .uesem/index/project.uesem.sqlite.")
    parser.add_argument("--text", help="Case-insensitive substring search over indexed identity and tags.")
    parser.add_argument("--class", dest="asset_class", help="Exact Unreal asset class.")
    parser.add_argument("--kind", dest="semantic_kind", help="Exact USEM semantic kind.")
    parser.add_argument("--status", help="Exact export status such as ok, stale, or missing.")
    parser.add_argument("--depends-on", help="Only assets with a dependency edge to this package.")
    parser.add_argument("--node", help="Query incoming/outgoing graph edges for an exact node ID.")
    parser.add_argument("--direction", choices=("outgoing", "incoming", "both"), default="both")
    parser.add_argument("--relation", help="Exact graph relation filter.")
    parser.add_argument("--node-text", help="Search graph node identity and labels.")
    parser.add_argument("--node-kind", help="Exact graph node kind filter.")
    parser.add_argument("--node-subtype", help="Exact graph node subtype filter.")
    parser.add_argument("--path-from", help="Find a directed graph path from this node ID.")
    parser.add_argument("--path-to", help="Find a directed graph path to this node ID.")
    parser.add_argument("--max-depth", type=int, default=6, help="Path depth limit, clamped to 1..12.")
    parser.add_argument("--limit", type=int, default=100, help="Maximum rows, clamped to 1..10000.")
    args = parser.parse_args()
    try:
        if args.path_from or args.path_to:
            if not args.path_from or not args.path_to:
                parser.error("--path-from and --path-to must be provided together")
            path = find_graph_path(
                args.database,
                args.path_from,
                args.path_to,
                relation=args.relation,
                max_depth=args.max_depth,
            )
            print(json.dumps({"found": path is not None, "path": path}, ensure_ascii=False, indent=2))
            return 0
        if args.node:
            rows = query_graph_neighbors(
                args.database,
                args.node,
                direction=args.direction,
                relation=args.relation,
                limit=args.limit,
            )
            payload = {"count": len(rows), "edges": rows}
        elif args.node_text or args.node_kind or args.node_subtype:
            rows = search_graph_nodes(
                args.database,
                text=args.node_text,
                kind=args.node_kind,
                subtype=args.node_subtype,
                limit=args.limit,
            )
            payload = {"count": len(rows), "nodes": rows}
        else:
            rows = query_assets(
                args.database,
                text=args.text,
                asset_class=args.asset_class,
                semantic_kind=args.semantic_kind,
                status=args.status,
                depends_on=args.depends_on,
                limit=args.limit,
            )
            payload = {"count": len(rows), "assets": rows}
    except (OSError, ValueError, sqlite3.Error) as exc:
        print(json.dumps({"error": str(exc)}, ensure_ascii=False))
        return 2
    print(json.dumps(payload, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
