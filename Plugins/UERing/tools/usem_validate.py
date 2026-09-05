from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


USEM_SCHEMA_VERSION = "2.9.0"
USEM_SEMANTIC_REVISION = 44
RECONSTRUCTION_OPCODES = {
    "cpp.class.declare",
    "cpp.property.declare",
    "cpp.graph.assertEmpty",
    "cpp.graph.translate",
    "cpp.asset.unsupported",
    "cpp.component.translate",
    "cpp.interface.translate",
    "cpp.defaults.translate",
    "cpp.timeline.translate",
    "cpp.omission.blocked",
    "editor.materialInstance.create",
    "editor.materialInstance.applyOverrides",
    "editor.dataAsset.create",
    "editor.dataAsset.createOwnedObjects",
    "editor.dataAsset.applyOwnedObjectProperties",
    "editor.dataAsset.applyProperties",
    "editor.stateTree.compile",
    "editor.asset.save",
}
RECONSTRUCTION_RAW_CONTENT_KEYS = {
    "bash",
    "body",
    "code",
    "codetext",
    "command",
    "commandline",
    "cppcode",
    "functionbody",
    "powershell",
    "rawbody",
    "rawcode",
    "script",
    "scripttext",
    "shell",
    "shellcommand",
    "sourcecode",
}


ASSET_REQUIRED_FIELDS = [
    "schema",
    "schemaVersion",
    "semanticRevision",
    "exporter",
    "profile",
    "inputFingerprint",
    "asset",
    "semantics",
    "reconstruction",
]

INDEX_REQUIRED_FIELDS = [
    "schema",
    "schemaVersion",
    "generator",
    "engine",
    "project",
    "generatedAtUtc",
    "coverage",
    "statistics",
    "assets",
]

MIGRATION_REQUIRED_FIELDS = [
    "schema",
    "schemaVersion",
    "generatedAtUtc",
    "method",
    "disclaimer",
    "candidates",
]

PROJECT_GRAPH_REQUIRED_FIELDS = [
    "schema",
    "schemaVersion",
    "usemSchemaVersion",
    "generatedAtUtc",
    "nodes",
    "edges",
    "statistics",
]


@dataclass(frozen=True)
class ValidationResult:
    ok: bool
    errors: list[str]


def validate_file(
    path: str | Path,
    root: str | Path | None = None,
    *,
    verify_hashes: bool = True,
    verify_source_hashes: bool = False,
) -> ValidationResult:
    file_path = Path(path)
    root_path = Path(root) if root is not None else _infer_project_root(file_path)
    errors: list[str] = []

    try:
        data = json.loads(file_path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return ValidationResult(False, [f"file not found: {file_path}"])
    except json.JSONDecodeError as exc:
        return ValidationResult(False, [f"invalid json: {exc.msg}"])

    if not isinstance(data, dict):
        return ValidationResult(False, ["root value must be an object"])

    schema = data.get("schema")
    if schema == "com.ue-ring.usem.asset":
        errors.extend(_validate_required(data, ASSET_REQUIRED_FIELDS))
        if data.get("schemaVersion") != USEM_SCHEMA_VERSION:
            errors.append("unsupported asset schemaVersion")
        if data.get("semanticRevision") != USEM_SEMANTIC_REVISION:
            errors.append("unsupported asset semanticRevision")
        if not isinstance(data.get("exporter"), str) or not data.get("exporter"):
            errors.append("exporter must be a non-empty string")
        if data.get("profile") not in {"logic", "reconstruction", "forensics"}:
            errors.append("invalid export profile")
        fingerprint = data.get("inputFingerprint")
        if not isinstance(fingerprint, str) or not _is_sha256(fingerprint):
            errors.append("inputFingerprint must be a sha256 digest")
        errors.extend(_validate_omissions(data))
        errors.extend(_validate_asset_semantics(data))
        errors.extend(_validate_reconstruction(data))
    elif schema == "com.ue-ring.usem.index":
        errors.extend(_validate_required(data, INDEX_REQUIRED_FIELDS))
        if data.get("schemaVersion") != USEM_SCHEMA_VERSION:
            errors.append("unsupported index schemaVersion")
        errors.extend(
            _validate_index_semantic_files(
                data,
                root_path,
                verify_hashes=verify_hashes,
                verify_source_hashes=verify_source_hashes,
            )
        )
    elif schema == "com.ue-ring.usem.blueprint-cpp-migration":
        errors.extend(_validate_required(data, MIGRATION_REQUIRED_FIELDS))
        candidates = data.get("candidates")
        if not isinstance(candidates, list):
            errors.append("candidates must be an array")
        else:
            for index, candidate in enumerate(candidates):
                if not isinstance(candidate, dict):
                    errors.append(f"candidate {index} must be an object")
                    continue
                for field in ("packageName", "priority", "metrics", "recommendations"):
                    if field not in candidate:
                        errors.append(f"candidate {index} missing required field: {field}")
                if not isinstance(candidate.get("recommendations"), list):
                    errors.append(f"candidate {index} recommendations must be an array")
    elif schema == "com.ue-ring.usem.project-graph":
        errors.extend(_validate_project_graph(data))
    else:
        errors.append(f"unsupported schema: {schema}")

    return ValidationResult(len(errors) == 0, errors)


def _validate_project_graph(data: dict[str, Any]) -> list[str]:
    errors = _validate_required(data, PROJECT_GRAPH_REQUIRED_FIELDS)
    if data.get("schemaVersion") != "1.1.0":
        errors.append("unsupported project graph schemaVersion")
    if data.get("usemSchemaVersion") != USEM_SCHEMA_VERSION:
        errors.append("unsupported project graph USEM schemaVersion")
    nodes = data.get("nodes")
    edges = data.get("edges")
    if not isinstance(nodes, list):
        return errors + ["project graph nodes must be an array"]
    if not isinstance(edges, list):
        return errors + ["project graph edges must be an array"]

    node_ids: set[str] = set()
    nodes_by_kind: dict[str, int] = {}
    for index, node in enumerate(nodes):
        if not isinstance(node, dict):
            errors.append(f"project graph node {index} must be an object")
            continue
        node_id = node.get("id")
        kind = node.get("kind")
        label = node.get("label")
        if not isinstance(node_id, str) or not node_id:
            errors.append(f"project graph node {index} missing id")
        elif node_id in node_ids:
            errors.append(f"project graph duplicate node id: {node_id}")
        else:
            node_ids.add(node_id)
        if not isinstance(kind, str) or not kind:
            errors.append(f"project graph node {index} missing kind")
        else:
            nodes_by_kind[kind] = nodes_by_kind.get(kind, 0) + 1
        if not isinstance(label, str):
            errors.append(f"project graph node {index} label must be a string")

    edge_keys: set[tuple[Any, ...]] = set()
    edges_by_relation: dict[str, int] = {}
    for index, edge in enumerate(edges):
        if not isinstance(edge, dict):
            errors.append(f"project graph edge {index} must be an object")
            continue
        source = edge.get("from")
        target = edge.get("to")
        relation = edge.get("relation")
        confidence = edge.get("confidence")
        if source not in node_ids:
            errors.append(f"project graph edge {index} has unknown source: {source}")
        if target not in node_ids:
            errors.append(f"project graph edge {index} has unknown target: {target}")
        if not isinstance(relation, str) or not relation:
            errors.append(f"project graph edge {index} missing relation")
        else:
            edges_by_relation[relation] = edges_by_relation.get(relation, 0) + 1
        if not isinstance(confidence, (int, float)) or isinstance(confidence, bool) or not 0 <= confidence <= 1:
            errors.append(f"project graph edge {index} confidence must be between 0 and 1")
        evidence_source = edge.get("evidenceSource")
        if not isinstance(evidence_source, str) or not evidence_source:
            errors.append(f"project graph edge {index} missing evidenceSource")
        contributor = edge.get("contributorPackage")
        if not isinstance(contributor, str) or not contributor.startswith("/"):
            errors.append(f"project graph edge {index} missing contributorPackage")
        pointer = edge.get("evidencePointer")
        if pointer is not None and (not isinstance(pointer, str) or not pointer.startswith("/")):
            errors.append(f"project graph edge {index} evidencePointer must be a JSON pointer")
        source_pin = edge.get("sourcePinId")
        target_pin = edge.get("targetPinId")
        if (source_pin is None) != (target_pin is None):
            errors.append(f"project graph edge {index} must declare both flow pin ids")
        key = (
            source,
            target,
            relation,
            edge.get("qualifier", ""),
            edge.get("sourceNodeId", ""),
            source_pin or "",
            target_pin or "",
        )
        if key in edge_keys:
            errors.append(f"project graph duplicate edge: {source} {relation} {target}")
        edge_keys.add(key)

    statistics = data.get("statistics")
    if not isinstance(statistics, dict):
        errors.append("project graph statistics must be an object")
    else:
        if statistics.get("nodeCount") != len(nodes):
            errors.append("project graph statistics nodeCount mismatch")
        if statistics.get("edgeCount") != len(edges):
            errors.append("project graph statistics edgeCount mismatch")
        if statistics.get("nodesByKind") != nodes_by_kind:
            errors.append("project graph statistics nodesByKind mismatch")
        if statistics.get("edgesByRelation") != edges_by_relation:
            errors.append("project graph statistics edgesByRelation mismatch")
    return errors


def _infer_project_root(file_path: Path) -> Path:
    """Resolve project-relative `.uesem/...` paths from files inside the output tree."""
    for parent in file_path.resolve().parents:
        if parent.name == ".uesem":
            return parent.parent
    return file_path.parent


def _validate_required(data: dict[str, Any], fields: list[str]) -> list[str]:
    return [f"missing required field: {field}" for field in fields if field not in data]


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return f"sha256:{digest.hexdigest()}"


def _is_sha256(value: str) -> bool:
    return len(value) == 71 and value.startswith("sha256:") and all(
        character in "0123456789abcdef" for character in value[7:]
    )


def _validate_omissions(data: dict[str, Any]) -> list[str]:
    omissions = data.get("omissions", [])
    if not isinstance(omissions, list):
        return ["omissions must be an array"]
    errors: list[str] = []
    for index, omission in enumerate(omissions):
        if not isinstance(omission, dict):
            errors.append(f"omission {index} must be an object")
            continue
        errors.extend(
            f"omission {index} {error}"
            for error in _validate_required(
                omission, ["path", "code", "reason", "recoverabilityImpact"]
            )
        )
        if not isinstance(omission.get("path"), str) or not omission["path"].startswith("/"):
            errors.append(f"omission {index} path must be a JSON pointer")
        for field in ("code", "reason", "recoverabilityImpact"):
            if not isinstance(omission.get(field), str) or not omission[field]:
                errors.append(f"omission {index} {field} must be non-empty")
    return errors


def _validate_pin_type(value: Any, label: str) -> list[str]:
    if not isinstance(value, dict):
        return [f"{label} type must be an object"]
    category = value.get("category")
    errors = [] if isinstance(category, str) and category else [f"{label} type missing category"]
    container = value.get("container")
    if container is not None and container not in {"array", "set", "map"}:
        errors.append(f"{label} type has invalid container: {container}")
    if container == "map":
        errors.extend(_validate_pin_type(value.get("valueType"), f"{label} map value"))
    elif "valueType" in value:
        errors.append(f"{label} non-map type must not declare valueType")
    return errors


def _validate_asset_semantics(data: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    semantics = data.get("semantics")
    if not isinstance(semantics, dict):
        return ["semantics must be an object"]
    for variable_index, variable in enumerate(semantics.get("variables", [])):
        if not isinstance(variable, dict):
            errors.append(f"variable {variable_index} must be an object")
        else:
            errors.extend(_validate_pin_type(variable.get("type"), f"variable {variable_index}"))
    graphs = semantics.get("graphs", [])
    if not isinstance(graphs, list):
        return ["semantics.graphs must be an array when present"]

    for graph_index, graph in enumerate(graphs):
        label = f"graph {graph_index}"
        if not isinstance(graph, dict):
            errors.append(f"{label} must be an object")
            continue
        nodes = graph.get("nodes")
        if not isinstance(nodes, list):
            errors.append(f"{label} nodes must be an array")
            continue
        for local_index, variable in enumerate(graph.get("localVariables", [])):
            if not isinstance(variable, dict):
                errors.append(f"{label} local variable {local_index} must be an object")
            else:
                errors.extend(
                    _validate_pin_type(
                        variable.get("type"), f"{label} local variable {local_index}"
                    )
                )

        node_ids: set[str] = set()
        pins: dict[str, str] = {}
        for node_index, node in enumerate(nodes):
            if not isinstance(node, dict):
                errors.append(f"{label} node {node_index} must be an object")
                continue
            node_id = node.get("id")
            if not isinstance(node_id, str) or not node_id:
                errors.append(f"{label} node {node_index} missing id")
            elif node_id in node_ids:
                errors.append(f"{label} duplicate node id: {node_id}")
            else:
                node_ids.add(node_id)
            node_pins = node.get("pins")
            if not isinstance(node_pins, list):
                errors.append(f"{label} node {node_index} pins must be an array")
                continue
            for pin_index, pin in enumerate(node_pins):
                if not isinstance(pin, dict):
                    errors.append(f"{label} node {node_index} pin {pin_index} must be an object")
                    continue
                pin_id = pin.get("id")
                direction = pin.get("direction")
                errors.extend(
                    _validate_pin_type(
                        pin.get("type"), f"{label} node {node_index} pin {pin_index}"
                    )
                )
                if not isinstance(pin_id, str) or not pin_id:
                    errors.append(f"{label} node {node_index} pin {pin_index} missing id")
                elif pin_id in pins:
                    errors.append(f"{label} duplicate pin id: {pin_id}")
                else:
                    pins[pin_id] = direction

        edges = graph.get("links", [])
        if not isinstance(edges, list):
            errors.append(f"{label} links must be an array when present")
            continue
        seen_edges: set[tuple[str, str]] = set()
        for edge_index, edge in enumerate(edges):
            if not isinstance(edge, dict):
                errors.append(f"{label} edge {edge_index} must be an object")
                continue
            source = edge.get("fromPin")
            target = edge.get("toPin")
            if not isinstance(source, str) or not source:
                errors.append(f"{label} edge {edge_index} missing fromPin")
                source = "<invalid>"
            if not isinstance(target, str) or not target:
                errors.append(f"{label} edge {edge_index} missing toPin")
                target = "<invalid>"
            key = (source, target)
            if key in seen_edges:
                errors.append(f"{label} duplicate edge: {source} -> {target}")
            seen_edges.add(key)
            if source not in pins:
                errors.append(f"{label} edge {edge_index} has unknown fromPin: {source}")
            elif pins[source] != "output":
                errors.append(f"{label} edge {edge_index} fromPin is not output: {source}")
            if target not in pins:
                errors.append(f"{label} edge {edge_index} has unknown toPin: {target}")
            elif pins[target] != "input":
                errors.append(f"{label} edge {edge_index} toPin is not input: {target}")
    return errors


def _validate_reconstruction(data: dict[str, Any]) -> list[str]:
    value = data.get("reconstruction")
    if not isinstance(value, dict):
        return ["reconstruction must be an object"]
    required = {
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
    }
    errors = _validate_closed_object(value, required, required, "reconstruction")
    if value.get("irVersion") != "2.0.0":
        errors.append("unsupported reconstruction irVersion")
    if value.get("contract") != "com.ue-ring.reconstruction":
        errors.append("unsupported reconstruction contract")
    if not isinstance(value.get("assetKind"), str) or not value.get("assetKind"):
        errors.append("reconstruction assetKind must be a non-empty string")
    if value.get("profile") != data.get("profile"):
        errors.append("reconstruction profile must match export profile")
    errors.extend(_validate_reconstruction_source(data, value.get("source")))
    errors.extend(_find_raw_reconstruction_content(value))

    losses = value.get("losses")
    loss_ids, loss_errors = _validate_reconstruction_losses(data, losses)
    errors.extend(loss_errors)
    targets = value.get("targets")
    target_ids, target_values, target_errors = _validate_reconstruction_targets(targets, loss_ids)
    errors.extend(target_errors)
    symbol_ids, symbol_errors = _validate_reconstruction_symbols(data, value.get("symbols"))
    errors.extend(symbol_errors)
    operation_ids, operation_values, operation_errors = _validate_reconstruction_operations(
        data, value.get("operations"), target_ids, symbol_ids
    )
    errors.extend(operation_errors)
    errors.extend(_validate_operation_graph(operation_ids, operation_values))
    errors.extend(_validate_target_status(target_values, operation_values))
    errors.extend(_validate_reconstruction_verification(value.get("verification"), target_ids))
    errors.extend(
        _validate_reconstruction_summaries(
            value.get("coverage"), value.get("execution"), operation_values
        )
    )
    errors.extend(_validate_global_reconstruction_ids(value))
    return errors


def _validate_closed_object(
    value: dict[str, Any], required: set[str], allowed: set[str], label: str
) -> list[str]:
    errors = [f"{label} missing required field: {field}" for field in sorted(required - value.keys())]
    errors.extend(f"{label} has unsupported field: {field}" for field in sorted(value.keys() - allowed))
    return errors


def _valid_id(value: Any) -> bool:
    return isinstance(value, str) and bool(value) and not any(character.isspace() for character in value)


def _validate_global_reconstruction_ids(value: dict[str, Any]) -> list[str]:
    seen: dict[str, str] = {}
    errors: list[str] = []
    for section in ("targets", "symbols", "operations", "losses", "verification"):
        entries = value.get(section)
        if not isinstance(entries, list):
            continue
        for entry in entries:
            if not isinstance(entry, dict) or not _valid_id(entry.get("id")):
                continue
            identifier = entry["id"]
            previous = seen.get(identifier)
            if previous is not None and previous != section:
                errors.append(
                    f"reconstruction id must be globally unique: {identifier} appears in {previous} and {section}"
                )
            else:
                seen[identifier] = section
    return errors


def _validate_sorted_unique_strings(value: Any, label: str, *, non_empty: bool = False) -> list[str]:
    if not isinstance(value, list) or any(not isinstance(item, str) or not item for item in value):
        return [f"{label} must be a string array"]
    errors: list[str] = []
    if non_empty and not value:
        errors.append(f"{label} must not be empty")
    if len(value) != len(set(value)):
        errors.append(f"{label} must not contain duplicates")
    if value != sorted(value):
        errors.append(f"{label} must be sorted")
    return errors


def _resolve_json_pointer(document: Any, pointer: Any) -> tuple[bool, str]:
    if not isinstance(pointer, str) or (pointer and not pointer.startswith("/")):
        return False, "must be a JSON pointer"
    current = document
    if pointer == "":
        return True, ""
    for encoded_token in pointer[1:].split("/"):
        index = 0
        while index < len(encoded_token):
            if encoded_token[index] == "~":
                if index + 1 >= len(encoded_token) or encoded_token[index + 1] not in "01":
                    return False, "contains an invalid JSON pointer escape"
                index += 2
            else:
                index += 1
        token = encoded_token.replace("~1", "/").replace("~0", "~")
        if isinstance(current, dict):
            if token not in current:
                return False, "does not resolve"
            current = current[token]
        elif isinstance(current, list):
            if not token.isdigit() or (len(token) > 1 and token.startswith("0")):
                return False, "has an invalid array index"
            item_index = int(token)
            if item_index >= len(current):
                return False, "does not resolve"
            current = current[item_index]
        else:
            return False, "does not resolve"
    return True, ""


def _validate_pointer(document: dict[str, Any], pointer: Any, label: str) -> list[str]:
    ok, reason = _resolve_json_pointer(document, pointer)
    return [] if ok else [f"{label} {reason}: {pointer}"]


def _validate_reconstruction_source(data: dict[str, Any], source: Any) -> list[str]:
    if not isinstance(source, dict):
        return ["reconstruction source must be an object"]
    fields = {"schemaVersion", "semanticRevision", "inputFingerprint", "sourceHash", "assetPointer"}
    errors = _validate_closed_object(
        source, fields, fields | {"semanticsPointer"}, "reconstruction source"
    )
    if source.get("schemaVersion") != data.get("schemaVersion"):
        errors.append("reconstruction source schemaVersion must match asset schemaVersion")
    if source.get("semanticRevision") != data.get("semanticRevision"):
        errors.append("reconstruction source semanticRevision must match asset semanticRevision")
    if source.get("inputFingerprint") != data.get("inputFingerprint"):
        errors.append("reconstruction source inputFingerprint must match asset inputFingerprint")
    if source.get("sourceHash") != data.get("asset", {}).get("sourceHash"):
        errors.append("reconstruction source sourceHash must match asset sourceHash")
    errors.extend(_validate_pointer(data, source.get("assetPointer"), "reconstruction source assetPointer"))
    if "semanticsPointer" in source:
        errors.extend(
            _validate_pointer(
                data, source.get("semanticsPointer"), "reconstruction source semanticsPointer"
            )
        )
    return errors


def _find_raw_reconstruction_content(value: Any, path: str = "/reconstruction") -> list[str]:
    errors: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            normalized = "".join(character.lower() for character in key if character.isalnum())
            child_path = f"{path}/{key}"
            if normalized in RECONSTRUCTION_RAW_CONTENT_KEYS:
                errors.append(f"reconstruction forbids raw code or shell field: {child_path}")
            errors.extend(_find_raw_reconstruction_content(child, child_path))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            errors.extend(_find_raw_reconstruction_content(child, f"{path}/{index}"))
    return errors


def _validate_reconstruction_losses(
    data: dict[str, Any], losses: Any
) -> tuple[set[str], list[str]]:
    if not isinstance(losses, list):
        return set(), ["reconstruction losses must be an array"]
    ids: set[str] = set()
    errors: list[str] = []
    fields = {"id", "reasonCode", "impact", "sourcePointers", "recoverableFromSourceAsset"}
    for index, loss in enumerate(losses):
        label = f"reconstruction loss {index}"
        if not isinstance(loss, dict):
            errors.append(f"{label} must be an object")
            continue
        errors.extend(_validate_closed_object(loss, fields, fields, label))
        loss_id = loss.get("id")
        if not _valid_id(loss_id):
            errors.append(f"{label} has invalid id")
        elif loss_id in ids:
            errors.append(f"reconstruction duplicate loss id: {loss_id}")
        else:
            ids.add(loss_id)
        if not isinstance(loss.get("reasonCode"), str) or not loss.get("reasonCode"):
            errors.append(f"{label} reasonCode must be non-empty")
        if loss.get("impact") not in {"blocksReconstruction", "degradesFidelity", "informational"}:
            errors.append(f"{label} has invalid impact")
        if not isinstance(loss.get("recoverableFromSourceAsset"), bool):
            errors.append(f"{label} recoverableFromSourceAsset must be boolean")
        pointers = loss.get("sourcePointers")
        errors.extend(_validate_sorted_unique_strings(pointers, f"{label} sourcePointers", non_empty=True))
        if isinstance(pointers, list):
            for pointer in pointers:
                errors.extend(_validate_pointer(data, pointer, f"{label} sourcePointer"))
    return ids, errors


def _validate_reconstruction_targets(
    targets: Any, loss_ids: set[str]
) -> tuple[set[str], list[dict[str, Any]], list[str]]:
    if not isinstance(targets, list) or not targets:
        return set(), [], ["reconstruction targets must be a non-empty array"]
    ids: set[str] = set()
    values: list[dict[str, Any]] = []
    errors: list[str] = []
    fields = {"id", "target", "backend", "backendVersion", "status", "fidelity", "writePolicy", "blockerRefs"}
    for index, target in enumerate(targets):
        label = f"reconstruction target {index}"
        if not isinstance(target, dict):
            errors.append(f"{label} must be an object")
            continue
        values.append(target)
        errors.extend(_validate_closed_object(target, fields, fields, label))
        target_id = target.get("id")
        if not _valid_id(target_id):
            errors.append(f"{label} has invalid id")
        elif target_id in ids:
            errors.append(f"reconstruction duplicate target id: {target_id}")
        else:
            ids.add(target_id)
        if target.get("target") not in {"nativeClassCpp", "editorAssetBuilderCpp"}:
            errors.append(f"{label} has invalid target kind")
        if target.get("backend") not in {"ueCpp", "unrealEditorCpp"}:
            errors.append(f"{label} has unsupported backend")
        if target.get("backendVersion") != 1:
            errors.append(f"{label} has unsupported backendVersion")
        if target.get("status") not in {"ready", "partial", "blocked"}:
            errors.append(f"{label} has invalid status")
        if target.get("fidelity") not in {"exact", "semanticEquivalent", "unsupported"}:
            errors.append(f"{label} has invalid fidelity")
        if target.get("writePolicy") != "replaceGenerated":
            errors.append(f"{label} writePolicy must be replaceGenerated")
        blockers = target.get("blockerRefs")
        errors.extend(_validate_sorted_unique_strings(blockers, f"{label} blockerRefs"))
        if isinstance(blockers, list):
            for blocker in blockers:
                if blocker not in loss_ids:
                    errors.append(f"{label} references unknown loss: {blocker}")
    if [target.get("id") for target in values] != sorted(target.get("id") for target in values if isinstance(target.get("id"), str)):
        errors.append("reconstruction targets must be sorted by id")
    return ids, values, errors


def _validate_reconstruction_symbols(
    data: dict[str, Any], symbols: Any
) -> tuple[set[str], list[str]]:
    if not isinstance(symbols, list):
        return set(), ["reconstruction symbols must be an array"]
    ids: set[str] = set()
    errors: list[str] = []
    required = {"id", "kind", "unrealPath", "resolution", "sourcePointer"}
    allowed = required | {"cppName", "module", "header"}
    for index, symbol in enumerate(symbols):
        label = f"reconstruction symbol {index}"
        if not isinstance(symbol, dict):
            errors.append(f"{label} must be an object")
            continue
        errors.extend(_validate_closed_object(symbol, required, allowed, label))
        symbol_id = symbol.get("id")
        if not _valid_id(symbol_id):
            errors.append(f"{label} has invalid id")
        elif symbol_id in ids:
            errors.append(f"reconstruction duplicate symbol id: {symbol_id}")
        else:
            ids.add(symbol_id)
        if not isinstance(symbol.get("kind"), str) or not symbol.get("kind"):
            errors.append(f"{label} kind must be non-empty")
        if not isinstance(symbol.get("unrealPath"), str):
            errors.append(f"{label} unrealPath must be a string")
        if symbol.get("resolution") not in {"exact", "inferred", "unresolved"}:
            errors.append(f"{label} has invalid resolution")
        errors.extend(_validate_pointer(data, symbol.get("sourcePointer"), f"{label} sourcePointer"))
        for optional in ("cppName", "module", "header"):
            if optional in symbol and not isinstance(symbol[optional], str):
                errors.append(f"{label} {optional} must be a string")
    return ids, errors


def _validate_reconstruction_operations(
    data: dict[str, Any], operations: Any, target_ids: set[str], symbol_ids: set[str]
) -> tuple[set[str], list[dict[str, Any]], list[str]]:
    if not isinstance(operations, list) or not operations:
        return set(), [], ["reconstruction operations must be a non-empty array"]
    ids: set[str] = set()
    values: list[dict[str, Any]] = []
    errors: list[str] = []
    fields = {
        "id", "opcode", "opcodeVersion", "phase", "targetId", "dependsOn", "operands",
        "results", "sourcePointers", "preconditions", "postconditions", "criticality", "status",
        "fidelity", "failurePolicy", "idempotencyKey",
    }
    idempotency_keys: set[str] = set()
    for index, operation in enumerate(operations):
        label = f"reconstruction operation {index}"
        if not isinstance(operation, dict):
            errors.append(f"{label} must be an object")
            continue
        values.append(operation)
        errors.extend(_validate_closed_object(operation, fields, fields, label))
        operation_id = operation.get("id")
        if not _valid_id(operation_id):
            errors.append(f"{label} has invalid id")
        elif operation_id in ids:
            errors.append(f"reconstruction duplicate operation id: {operation_id}")
        else:
            ids.add(operation_id)
        if operation.get("opcode") not in RECONSTRUCTION_OPCODES:
            errors.append(f"{label} has unsupported opcode: {operation.get('opcode')}")
        if operation.get("opcodeVersion") != 1:
            errors.append(f"{label} has unsupported opcodeVersion")
        if operation.get("phase") not in {"declare", "configure", "translate", "verify"}:
            errors.append(f"{label} has invalid phase")
        if operation.get("targetId") not in target_ids:
            errors.append(f"{label} references unknown target: {operation.get('targetId')}")
        errors.extend(_validate_sorted_unique_strings(operation.get("dependsOn"), f"{label} dependsOn"))
        errors.extend(_validate_sorted_unique_strings(operation.get("sourcePointers"), f"{label} sourcePointers", non_empty=True))
        pointers = operation.get("sourcePointers")
        if isinstance(pointers, list):
            for pointer in pointers:
                errors.extend(_validate_pointer(data, pointer, f"{label} sourcePointer"))
        if not isinstance(operation.get("operands"), dict):
            errors.append(f"{label} operands must be an object")
        if not isinstance(operation.get("results"), (dict, list)):
            errors.append(f"{label} results must be an object or array")
        errors.extend(
            _validate_symbol_references(operation.get("operands"), symbol_ids, f"{label} operands")
        )
        errors.extend(
            _validate_symbol_references(operation.get("results"), symbol_ids, f"{label} results")
        )
        for field in ("preconditions", "postconditions"):
            if not isinstance(operation.get(field), list) or any(
                not isinstance(item, (str, dict)) for item in operation.get(field, [])
            ):
                errors.append(f"{label} {field} must be an array of strings or objects")
        if operation.get("criticality") not in {"structure", "defaults", "behavior", "presentation"}:
            errors.append(f"{label} has invalid criticality")
        status = operation.get("status")
        if status not in {"executable", "blocked"}:
            errors.append(f"{label} has invalid status")
        errors.extend(_validate_operation_fidelity(operation.get("fidelity"), label, status))
        if operation.get("failurePolicy") != "abort":
            errors.append(f"{label} failurePolicy must be abort")
        key = operation.get("idempotencyKey")
        if not isinstance(key, str) or not key:
            errors.append(f"{label} idempotencyKey must be non-empty")
        elif key in idempotency_keys:
            errors.append(f"reconstruction duplicate idempotencyKey: {key}")
        else:
            idempotency_keys.add(key)
    actual_ids = [operation.get("id") for operation in values]
    if all(isinstance(operation_id, str) for operation_id in actual_ids) and actual_ids != sorted(actual_ids):
        errors.append("reconstruction operations must be sorted by id")
    return ids, values, errors


def _validate_symbol_references(value: Any, symbol_ids: set[str], path: str) -> list[str]:
    errors: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            normalized = key.lower()
            child_path = f"{path}.{key}"
            if normalized.endswith("symbolid"):
                if not isinstance(child, str) or child not in symbol_ids:
                    errors.append(f"{child_path} references unknown symbol: {child}")
            elif normalized.endswith("symbolids"):
                if not isinstance(child, list):
                    errors.append(f"{child_path} must be a symbol id array")
                else:
                    for symbol_id in child:
                        if not isinstance(symbol_id, str) or symbol_id not in symbol_ids:
                            errors.append(f"{child_path} references unknown symbol: {symbol_id}")
            else:
                errors.extend(_validate_symbol_references(child, symbol_ids, child_path))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            errors.extend(_validate_symbol_references(child, symbol_ids, f"{path}[{index}]"))
    return errors


def _validate_operation_fidelity(value: Any, label: str, operation_status: Any) -> list[str]:
    if not isinstance(value, dict):
        return [f"{label} fidelity must be an object"]
    required = {"status", "rule", "confidence"}
    errors = _validate_closed_object(value, required, required | {"reasonCode"}, f"{label} fidelity")
    status = value.get("status")
    if status not in {"exact", "inferred", "unsupported"}:
        errors.append(f"{label} fidelity has invalid status")
    if not isinstance(value.get("rule"), str) or not value.get("rule"):
        errors.append(f"{label} fidelity rule must be non-empty")
    confidence = value.get("confidence")
    if not isinstance(confidence, (int, float)) or isinstance(confidence, bool) or not 0 <= confidence <= 1:
        errors.append(f"{label} fidelity confidence must be between 0 and 1")
    if status == "unsupported" and not value.get("reasonCode"):
        errors.append(f"{label} unsupported fidelity requires reasonCode")
    if status == "unsupported" and operation_status != "blocked":
        errors.append(f"{label} unsupported fidelity must be blocked")
    if operation_status == "blocked" and status != "unsupported":
        errors.append(f"{label} blocked operation must have unsupported fidelity")
    return errors


def _validate_operation_graph(
    operation_ids: set[str], operations: list[dict[str, Any]]
) -> list[str]:
    errors: list[str] = []
    dependencies: dict[str, list[str]] = {}
    for operation in operations:
        operation_id = operation.get("id")
        if not isinstance(operation_id, str):
            continue
        depends_on = operation.get("dependsOn")
        dependencies[operation_id] = depends_on if isinstance(depends_on, list) else []
        for dependency in dependencies[operation_id]:
            if dependency not in operation_ids:
                errors.append(f"reconstruction operation {operation_id} references unknown dependency: {dependency}")
            if dependency == operation_id:
                errors.append(f"reconstruction operation {operation_id} depends on itself")
    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(operation_id: str) -> bool:
        if operation_id in visiting:
            return True
        if operation_id in visited:
            return False
        visiting.add(operation_id)
        cycle = any(dependency in dependencies and visit(dependency) for dependency in dependencies.get(operation_id, []))
        visiting.remove(operation_id)
        visited.add(operation_id)
        return cycle

    if any(visit(operation_id) for operation_id in sorted(dependencies)):
        errors.append("reconstruction operation dependency graph must be acyclic")
    return errors


def _validate_target_status(
    targets: list[dict[str, Any]], operations: list[dict[str, Any]]
) -> list[str]:
    errors: list[str] = []
    for target in targets:
        target_id = target.get("id")
        target_operations = [operation for operation in operations if operation.get("targetId") == target_id]
        has_blocked_operation = any(operation.get("status") == "blocked" for operation in target_operations)
        has_inferred_operation = any(
            isinstance(operation.get("fidelity"), dict)
            and operation["fidelity"].get("status") == "inferred"
            for operation in target_operations
        )
        has_blocker = bool(target.get("blockerRefs"))
        status = target.get("status")
        has_executable_operation = any(
            operation.get("status") == "executable" for operation in target_operations
        )
        expected_status = (
            "partial"
            if (has_blocked_operation or has_blocker) and has_executable_operation
            else "blocked"
            if has_blocked_operation or has_blocker
            else "partial"
            if has_inferred_operation
            else "ready"
        )
        if status != expected_status:
            errors.append(
                f"reconstruction target {target_id} status mismatch: expected {expected_status}"
            )
        fidelity = target.get("fidelity")
        if fidelity == "unsupported" and status != "blocked":
            errors.append(f"reconstruction target {target_id} unsupported fidelity must be blocked")
        if status == "ready" and fidelity == "unsupported":
            errors.append(f"reconstruction target {target_id} ready target cannot be unsupported")
    return errors


def _validate_reconstruction_verification(value: Any, target_ids: set[str]) -> list[str]:
    if not isinstance(value, list):
        return ["reconstruction verification must be an array"]
    errors: list[str] = []
    ids: set[str] = set()
    fields = {"id", "kind", "targetId", "required"}
    for index, verification in enumerate(value):
        label = f"reconstruction verification {index}"
        if not isinstance(verification, dict):
            errors.append(f"{label} must be an object")
            continue
        errors.extend(_validate_closed_object(verification, fields, fields, label))
        verification_id = verification.get("id")
        if not _valid_id(verification_id):
            errors.append(f"{label} has invalid id")
        elif verification_id in ids:
            errors.append(f"reconstruction duplicate verification id: {verification_id}")
        else:
            ids.add(verification_id)
        if verification.get("kind") not in {"ubtCompile", "reflectionCompare", "semanticReexportDiff"}:
            errors.append(f"{label} has invalid kind")
        if verification.get("targetId") not in target_ids:
            errors.append(f"{label} references unknown target: {verification.get('targetId')}")
        if not isinstance(verification.get("required"), bool):
            errors.append(f"{label} required must be boolean")
    return errors


def _validate_reconstruction_summaries(
    coverage: Any, execution: Any, operations: list[dict[str, Any]]
) -> list[str]:
    errors: list[str] = []
    total = len(operations)
    exact = sum(
        isinstance(operation.get("fidelity"), dict) and operation["fidelity"].get("status") == "exact"
        for operation in operations
    )
    inferred = sum(
        isinstance(operation.get("fidelity"), dict) and operation["fidelity"].get("status") == "inferred"
        for operation in operations
    )
    unsupported = sum(
        isinstance(operation.get("fidelity"), dict) and operation["fidelity"].get("status") == "unsupported"
        for operation in operations
    )
    executable = sum(operation.get("status") == "executable" for operation in operations)
    blocked = sum(operation.get("status") == "blocked" for operation in operations)
    readiness = (
        "partial"
        if (blocked or unsupported) and executable
        else "blocked"
        if blocked or unsupported
        else "partial"
        if inferred
        else "ready"
    )
    exact_ratio = exact / total if total else 0.0
    if not isinstance(coverage, dict):
        errors.append("reconstruction coverage must be an object")
    else:
        fields = {"readiness", "totalOperationCount", "exactOperationCount", "inferredOperationCount", "unsupportedOperationCount", "exactRatio"}
        errors.extend(_validate_closed_object(coverage, fields, fields, "reconstruction coverage"))
        expected = {
            "readiness": readiness,
            "totalOperationCount": total,
            "exactOperationCount": exact,
            "inferredOperationCount": inferred,
            "unsupportedOperationCount": unsupported,
        }
        for field, expected_value in expected.items():
            if coverage.get(field) != expected_value:
                errors.append(f"reconstruction coverage {field} mismatch")
        ratio = coverage.get("exactRatio")
        if not isinstance(ratio, (int, float)) or isinstance(ratio, bool) or abs(ratio - exact_ratio) > 1e-6:
            errors.append("reconstruction coverage exactRatio mismatch")
    if not isinstance(execution, dict):
        errors.append("reconstruction execution must be an object")
    else:
        fields = {"fullyExecutable", "operationCount", "executableOperationCount", "blockedOperationCount"}
        errors.extend(_validate_closed_object(execution, fields, fields, "reconstruction execution"))
        expected = {
            "fullyExecutable": blocked == 0,
            "operationCount": total,
            "executableOperationCount": executable,
            "blockedOperationCount": blocked,
        }
        for field, expected_value in expected.items():
            if execution.get(field) != expected_value:
                errors.append(f"reconstruction execution {field} mismatch")
    return errors


def _validate_coverage(data: dict[str, Any]) -> list[str]:
    coverage = data.get("coverage")
    if not isinstance(coverage, dict):
        return ["coverage must be an object"]
    errors: list[str] = []
    indexed = coverage.get("indexedAssetCount")
    excluded = coverage.get("excludedAssetCount")
    assets = data.get("assets")
    if not isinstance(indexed, int) or isinstance(indexed, bool) or indexed < 0:
        errors.append("coverage indexedAssetCount must be a non-negative integer")
    elif isinstance(assets, list) and indexed != len(assets):
        errors.append("coverage indexedAssetCount does not match assets length")
    if not isinstance(excluded, int) or isinstance(excluded, bool) or excluded < 0:
        errors.append("coverage excludedAssetCount must be a non-negative integer")
    exclusions = coverage.get("exclusions", [])
    if not isinstance(exclusions, list):
        errors.append("coverage exclusions must be an array")
        return errors
    total = 0
    reasons: set[str] = set()
    for index, entry in enumerate(exclusions):
        if not isinstance(entry, dict):
            errors.append(f"coverage exclusion {index} must be an object")
            continue
        reason = entry.get("reason")
        count = entry.get("count")
        if not isinstance(reason, str) or not reason:
            errors.append(f"coverage exclusion {index} missing reason")
        elif reason in reasons:
            errors.append(f"coverage duplicate exclusion reason: {reason}")
        else:
            reasons.add(reason)
        if not isinstance(count, int) or isinstance(count, bool) or count < 1:
            errors.append(f"coverage exclusion {index} count must be positive")
        else:
            total += count
    if isinstance(excluded, int) and total != excluded:
        errors.append("coverage excludedAssetCount does not match exclusions")
    return errors


def _validate_index_semantic_files(
    data: dict[str, Any],
    root: Path,
    *,
    verify_hashes: bool,
    verify_source_hashes: bool,
) -> list[str]:
    errors: list[str] = []
    errors.extend(_validate_coverage(data))
    assets = data.get("assets")
    if not isinstance(assets, list):
        return ["assets must be an array"]

    for asset in assets:
        if not isinstance(asset, dict):
            errors.append("asset entry must be an object")
            continue

        package_name = asset.get("packageName", "<unknown>")
        status = asset.get("status")
        semantic_file = asset.get("semanticFile")
        domains = asset.get("domains")
        recoverability = asset.get("recoverability")
        reconstruction_confidence = asset.get("reconstructionConfidence")
        if (
            not isinstance(domains, list)
            or any(not isinstance(domain, str) or not domain for domain in domains)
            or len(domains) != len(set(domains))
        ):
            errors.append(f"{package_name}: domains must be a unique string array")
        if recoverability not in {"", "high", "partial", "low"}:
            errors.append(f"{package_name}: invalid recoverability")
        if (
            not isinstance(reconstruction_confidence, (int, float))
            or isinstance(reconstruction_confidence, bool)
            or not 0 <= reconstruction_confidence <= 1
        ):
            errors.append(f"{package_name}: reconstructionConfidence must be between 0 and 1")
        if status == "unsupported":
            if semantic_file not in (None, ""):
                errors.append(f"{package_name}: unsupported asset must not declare semanticFile")
            continue

        if status != "ok":
            continue

        if not isinstance(semantic_file, str) or not semantic_file:
            errors.append(f"{package_name}: ok asset entry missing semanticFile")
            continue

        semantic_path = Path(semantic_file)
        if not semantic_path.is_absolute():
            semantic_path = root / semantic_path
        if not semantic_path.exists():
            errors.append(f"missing semantic file: {semantic_file}")
            continue

        try:
            semantic = json.loads(semantic_path.read_text(encoding="utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            errors.append(f"{package_name}: invalid semantic JSON {semantic_file}: {exc}")
            continue

        if not isinstance(semantic, dict):
            errors.append(f"{package_name}: semantic root must be an object")
            continue
        errors.extend(
            f"{package_name}: {error}"
            for error in _validate_required(semantic, ASSET_REQUIRED_FIELDS)
        )
        if semantic.get("schemaVersion") != USEM_SCHEMA_VERSION:
            errors.append(f"{package_name}: semantic file has unsupported schemaVersion")
        if semantic.get("semanticRevision") != USEM_SEMANTIC_REVISION:
            errors.append(f"{package_name}: semantic file has unsupported semanticRevision")
        errors.extend(
            f"{package_name}: {error}"
            for error in _validate_asset_semantics(semantic)
        )
        errors.extend(
            f"{package_name}: {error}"
            for error in _validate_reconstruction(semantic)
        )
        semantic_domains: list[str] = []
        semantic_model = semantic.get("semantics")
        if isinstance(semantic_model, dict):
            domain_model = semantic_model.get("domain")
            if isinstance(domain_model, dict):
                projections = domain_model.get("projections")
                if isinstance(projections, dict):
                    semantic_domains = sorted(projections)
        if domains != semantic_domains:
            errors.append(f"{package_name}: domains do not match semantic projections")
        semantic_reconstruction = semantic.get("reconstruction", {})
        semantic_readiness = semantic_reconstruction.get("coverage", {}).get("readiness")
        semantic_recoverability = {
            "ready": "high",
            "partial": "partial",
            "blocked": "low",
        }.get(semantic_readiness)
        if recoverability != semantic_recoverability:
            errors.append(f"{package_name}: recoverability does not match semantic file")
        semantic_exact_ratio = semantic_reconstruction.get("coverage", {}).get("exactRatio")
        if reconstruction_confidence != semantic_exact_ratio:
            errors.append(f"{package_name}: reconstructionConfidence does not match semantic file")
        if semantic.get("schema") != "com.ue-ring.usem.asset":
            errors.append(f"{package_name}: semantic file has unsupported schema")

        semantic_asset = semantic.get("asset")
        if not isinstance(semantic_asset, dict):
            errors.append(f"{package_name}: semantic asset field must be an object")
        else:
            if semantic_asset.get("packageName") != package_name:
                errors.append(f"{package_name}: semantic packageName does not match index")
            if semantic_asset.get("sourceFile") != asset.get("sourceFile"):
                errors.append(f"{package_name}: sourceFile does not match index")
            if semantic_asset.get("sourceHash") != asset.get("sourceHash"):
                errors.append(f"{package_name}: sourceHash does not match semantic file")

        if verify_hashes:
            expected_semantic_hash = asset.get("semanticHash")
            actual_semantic_hash = _sha256(semantic_path)
            if expected_semantic_hash != actual_semantic_hash:
                errors.append(
                    f"{package_name}: semanticHash mismatch: "
                    f"expected {expected_semantic_hash}, got {actual_semantic_hash}"
                )

        if verify_source_hashes:
            source_file = asset.get("sourceFile")
            if not isinstance(source_file, str) or not source_file:
                errors.append(f"{package_name}: ok asset entry missing sourceFile")
                continue
            source_path = Path(source_file)
            if not source_path.is_absolute():
                source_path = root / source_path
            if not source_path.exists():
                errors.append(f"{package_name}: missing source file: {source_file}")
            elif asset.get("sourceHash") != _sha256(source_path):
                errors.append(f"{package_name}: sourceHash does not match source file")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate USEM asset or index JSON files.")
    parser.add_argument("path", help="Path to a .uesem.json or .uesem-index.json file.")
    parser.add_argument("--root", default=None, help="Project root used to resolve index semanticFile paths.")
    parser.add_argument(
        "--skip-hashes",
        action="store_true",
        help="Skip semantic SHA-256 verification.",
    )
    parser.add_argument(
        "--verify-source-hashes",
        action="store_true",
        help="Also hash every source .uasset/.umap; this can be slow on large projects.",
    )
    args = parser.parse_args()

    result = validate_file(
        args.path,
        args.root,
        verify_hashes=not args.skip_hashes,
        verify_source_hashes=args.verify_source_hashes,
    )
    if result.ok:
        print("OK")
        return 0

    for error in result.errors:
        print(f"ERROR: {error}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

