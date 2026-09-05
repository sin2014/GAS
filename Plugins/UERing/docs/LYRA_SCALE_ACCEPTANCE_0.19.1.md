# Lyra Scale Acceptance 0.19.1

Date: 2026-08-02

Engine: Unreal Engine 5.8.0

Project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

Plugin version: 0.19.1 (38)

USEM schema: 2.8.0

Semantic revision: 37

## Scope

This iteration removes the remaining whole-project sidecar and graph reads from package-scoped
derived-index updates. A changed package builds only its current unified-graph contribution,
applies it to SQLite in one transaction, and streams the canonical project graph JSON from ordered
SQLite cursors. The update path no longer parses the previous 78 MB graph JSON or constructs the
complete graph as an in-memory JSON object tree.

SQLite schema 6 stores `graph_id` and `raw_node_id`, allowing streamed graph materialization to
preserve Blueprint node scope exactly. Membership-changing contributors are selected directly from
SQLite. A transaction rejects dangling endpoints before commit, and missing or incompatible
database state falls back to a deterministic full rebuild.

C++ reflection reverse links and Blueprint-to-C++ migration candidates are also maintained by
dirty package. Old references and candidates owned by those packages are removed, current
sidecars are merged, and results are sorted identically to a full rebuild. Missing, corrupt, or
membership-incompatible derived state falls back to a full rebuild.

## Final export

- Indexed assets: 3,876
- Failed assets: 0
- Skipped generated external actors: 4,809
- Asset semantic JSON bytes: 105,319,200
- Complete `.uesem` bytes: 441,151,698 (420.71 MiB)
- Complete `.uesem` files: 12,754
- Recorded semantic omissions: 2,534
- Unified graph nodes: 53,047
- Unified graph edges: 115,802
- Project graph JSON bytes: 78,311,295
- SQLite bytes: 238,518,272

The final full export reported `exported=0 unchanged=3876 failed=0`. Every sidecar remains on
schema 2.8.0 and semantic revision 37 because this release changes only derived-index persistence,
not the per-asset semantic contract. The complete output grew by 2,793,474 bytes (2.66 MiB) from
0.19.0; almost all growth is the two nullable Blueprint scope columns and indexes in SQLite. The
canonical project graph JSON and all asset semantics are unchanged in size.

The 4,809 exclusions are generated World Partition external actor/object packages. They remain
visible in coverage but are skipped before load because they are implementation storage rather
than independent authored assets. No `/Game/UERingTests/` asset, sidecar, or directory remains.

## Incremental performance

Measurements use the 3,876-asset Lyra export and the automation Blueprint contributor.

| Operation | Index time | Graph stage | Notes |
| --- | ---: | ---: | --- |
| 0.19.0 no-change package | about 0.6 s | 0 s | Existing semantic hash is unchanged |
| 0.19.0 changed contributor | 3.510 s | 2.826 s | Parsed and rewrote the prior 78 MB graph JSON |
| 0.19.1 no-change package | 0.669 s | 0 s | SQLite and compact JSON maintenance only |
| 0.19.1 changed contributor | 1.672-1.817 s | 0.978-1.015 s | Fragment update plus SQLite cursor streaming |
| 0.19.1 final full rebuild | 13.310 s | full graph | Deterministic checkpoint for 3,876 assets |

The changed-contributor index path is about 52% faster than 0.19.0. More importantly, its peak
graph memory no longer scales with a parsed old graph plus a second complete output object tree.
The 78 MB compatibility JSON is still rewritten atomically because it remains a supported offline
artifact; SQLite is the incremental source of truth for graph state.

## Correctness contracts

- Full and incremental graph ordering use the same case-insensitive node order and field-tuple edge
  order, including prefix and case-collision cases.
- SQLite graph nodes retain `graphId` and `rawNodeId`; every emitted edge retains
  `contributorPackage` and all evidence fields.
- SQLite contribution updates and endpoint checks occur inside one immediate transaction.
- A changed package removes old graph edges, C++ reverse links, and migration candidates before
  current evidence is merged.
- Deleted sidecars remove their C++ references and migration candidates without scanning unrelated
  sidecars.
- Incompatible SQLite, reflection, source-index, or migration-report state triggers a full rebuild.
- No-change, changed-contributor, and deleted-sidecar automation compares incremental output byte
  for byte with a subsequent full rebuild.
- Full export remains a clean deterministic checkpoint and recursively removes empty orphan paths.

## Verification

- Strict BuildPlugin Editor/Development/Shipping for Win64 with PCH, shared PCH, and unity disabled: passed.
- Python `unittest`: 79 passed.
- UE Automation `UERing.`: 11 passed in one process.
- Incremental project index/dependency graph/project graph equivalence: passed.
- Incremental C++ reflection and migration report equivalence, including deletion: passed.
- Full Lyra export: 3,876 indexed assets, 0 failures.
- UE validation: checked 3,876; missing 0; stale 0; orphan 0; invalid 0.
- Offline source-hash/project-index validation: passed.
- Offline unified project graph validation: passed.
- Lyra golden acceptance: passed.
- SQLite user version: 6; `integrity_check`: `ok`.
- SQLite rows: 3,876 assets; 53,047 graph nodes; 115,802 graph edges.
- Dangling graph edges: 0; graph edges without contributor: 0.

Logs:

- `Saved/Logs/UERing-P3-SqliteStreaming-LowDelimiter.log`
- `Saved/Logs/UERing-P3-IncrementalCppMigration.log`
- `Saved/Logs/UERing-0.19.1-Revision37-AllTests.log`
- `Saved/Logs/UERing-0.19.1-Revision37-FinalFullExport.log`
- `Saved/Logs/UERing-0.19.1-Revision37-FinalValidate.log`

Only `LyraStarterGame` was deployed, exported, and validated. The `Start` project was not modified.

