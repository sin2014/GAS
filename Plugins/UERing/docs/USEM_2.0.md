# USEM 2.3 logic-first contract

USEM 2.3 keeps each asset sidecar focused on the facts needed to understand that asset. Project, engine, and plugin provenance is stored once in `index/project.uesem.index.json`.

## Asset envelope

Required top-level fields are `schema`, `schemaVersion`, `semanticRevision`, `exporter`, `profile`, `inputFingerprint`, `asset`, `semantics`, and `reconstruction`. `dependencies`, `cppLinks`, `diagnostics`, and `omissions` are present only when non-empty. The input fingerprint covers source content, export profile, exporter semantics, reflected class structure, and project Blueprint parent sources. Every intentional omission states its JSON path, reason, recoverability impact, optional original count, and source digest.

The asset object keeps identity, source location, package GUID, native class, and source hash. `semanticFile` and export time are index concerns and are not repeated in each sidecar.

A full project export covers `/Game` and every mounted content plugin loaded from the project's `Plugins` directory. Engine plugin mounts are excluded. The mount name remains the first output path segment, so `/ShooterCore/...` is stored under `content/ShooterCore/...` without colliding with `/Game/...`.

World Partition generated packages below `__ExternalActors__` and `__ExternalObjects__` are excluded before export and index entry construction. They are implementation shards of the owning World rather than independently authored semantic units. `coverage.excludedAssetCount` and `coverage.exclusions` retain the exact count and reason. Other unmatched project assets remain explicit `unsupported` index entries with no semantic file.

## Domain projections

Assets whose real class lineage matches GAS, Lyra Experience, GameFeature, StateTree, DataRegistry, or Enhanced Input receive `semantics.domain`. The projection records the effective generated-class lineage, one or more domain roles, selected asset/CDO properties, relevant persistent owned objects, and external asset references. Detection uses reflection and class ancestry, so project subclasses work without a compile-time dependency on Lyra game modules.

The domain projection complements the primary specialized exporter. It does not replace Blueprint graph topology, Niagara owned-object graphs, typed table rows, or other primary semantics.

## Reconstruction IR

Every sidecar includes a compact `reconstruction` object targeting semantic equivalence. It contains:

- `confidenceMethod: exporter-heuristic-v1` and bounded overall/structure/behavior/default confidence values;
- evidence codes explaining which exported representations support the estimate;
- explicit reconstructable and unrecoverable capability codes;
- ordered `steps`, each with an operation, JSON Pointer source, and step confidence.

Confidence is an exporter-based heuristic, not a measured probability. The IR explicitly marks binary identity, opaque custom serialization, compiled bytecode, native function bodies, macro expansion, and other unavailable information where applicable. It can drive assisted asset/code generation followed by compilation and behavioral testing; it is not a claim of lossless `.uasset` or C++ recovery.

## Unified project graph

UE Ring 0.11.0 derives `index/project.uesem.graph.json` from the authoritative asset sidecars and Asset Registry dependency graph. Project assets, Blueprint graphs, graph-scoped Blueprint nodes, symbols, domain roles, local domain objects, external packages, and GameFeature plugins are stable nodes. Edges retain their source sidecar or Asset Registry provenance, JSON Pointer where applicable, confidence, qualifiers, and original Blueprint Pin identities.

Blueprint node and Pin identities are scoped by `graphPath`; raw GUIDs are only unique within one graph. Asset dependencies remain `dependsOn` and are never promoted to execution claims. Typed domain properties may produce stronger relations such as `grantsAbility`, `usesDefaultPawnData`, or `addsInputMapping`. The graph is also materialized in SQLite user version 4 as `graph_nodes` and `graph_edges` for later path and neighborhood queries.

## Material logic invariants

Material assets use dedicated logic representations rather than the generic owned-object graph:

- `material-expression-graph-v1` owns the authored Material expressions, root settings and Material property inputs.
- `material-function-graph-v1` owns the function expressions plus GUID-stable input/output interface definitions and preview defaults.
- `material-instance-v1` stores its parent reference and local runtime/editor static overrides without copying the parent graph.
- `material-function-instance-v1` stores parent/base references and local overrides without claiming ownership of base-function expressions.
- `material-parameter-collection-v1` preserves scalar/vector parameter identity, GUID and default value.

Every expression node has a deterministic asset-local ID. Duplicate expression GUIDs receive deterministic object-name suffixes. Connections preserve source output index and optional name/channel mask together with the exact target node and input. `$material` is the synthetic target for final Material property outputs. `danglingConnectionCount` must be zero for accepted exports, and project graph edges expose the same topology as `containsMaterialNode`, `feedsExpressionInput`, and `feedsMaterialInput`.

Node configuration contains authored values that affect compilation or evaluation, including unconnected arithmetic constants, parameter defaults and GUIDs, function-call port mappings, asset references, and Custom expression code. Editor layout, comments, preview state, localized menus, thumbnails, reference caches, texture-streaming caches and compiled shader data are not part of the logic representation. Their loss or regeneration is explicit in `omissions` and reconstruction IR.

## Blueprint graph invariants

- Every node keeps a graph-unique `nodeGuid/objectName` `id`, node `class`, readable `title`, structured `memberReference` when available, and all pins.
- Every pin keeps a graph-unique `pinGuid` `id`, name, direction, and complete Unreal pin type. A node-qualified fallback is used only for missing or colliding GUIDs. Unconnected input defaults are preserved as `defaultValue`, `defaultObject`, or `defaultText`.
- Every graph edge is represented exactly once by `fromPin` and `toPin`. Pin IDs resolve back to their owning node, so node IDs and display strings are not duplicated on each edge.
- Pin and variable `type` is an object with required `category` and optional `subCategory`, `typeObject`, member signature, container, qualifier, and precision fields. Map key type occupies the root object and its value type is the nested `valueType` object.
- Function graphs may provide sorted `functionFlags`, non-default `functionMetadata`, and typed `localVariables`. Blueprint-owned variables remain in `variables`; inherited CDO overrides are emitted separately as typed `classDefaults`.
- Disabled or development-only nodes keep an explicit `enabledState`. User comments remain available; empty comments are omitted.
- Empty arrays and default-false presentation flags are omitted. Their documented default is empty or false.

## Typed values

Properties use `{name, type, value}`. The typed `value` is authoritative and supports scalars, enums, localized text, object references, containers, maps, and structs. `propertyClass`, duplicate `exportText`, `exportTextOmitted: false`, and `redacted: false` are removed. A redacted value remains explicit, and an oversized fallback string becomes `{omitted: true, length: N}`.

User-defined structs and enums use the `Definition` exporter. Struct fields preserve internal/authored/display identity, stable GUID, Blueprint pin type, flags, tooltip, and typed default. Enum entries preserve internal/authored/display names and numeric values; hidden and synthetic `_MAX` entries are omitted.

## Compatibility

Owned-object graphs include only persistent objects reachable from the root asset through real UObject references. Unreachable same-package objects cannot affect the active root graph and are excluded as stale versions or derived package residue. Root properties appear once in `rootProperties`, not again on the `$root` object entry.

USEM 2.3.0 is a deliberate breaking schema revision. Consumers must require reconstruction IR, honor the selected profile, and treat `omissions` as explicit reconstruction limits rather than missing exporter behavior. Re-exporting with UE Ring 0.14.0 and semantic revision 25 replaces older sidecars and rebuilds the current project, dependency, unified graph, SQLite, C++, migration, summary, and visualization artifacts. Sidecars are condensed by default; `bPrettyJson` changes whitespace only. A full export removes old Bundles, tombstones, logs, and prior diff artifacts first. Change summaries are opt-in and, when enabled, only the latest full-export diff set is retained.
