# P4 Design Audit and Roadmap

Date: 2026-08-02

Audited baseline: Git `71d8a38`, plugin `0.19.1 (38)`, USEM `2.8.0`, semantic revision `37`, SQLite user version `6`

Acceptance project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

This document starts P4. It does not reopen P0-P3 implementation or repeat their acceptance work.
The audit is based on the checked-in implementation and the accepted 3,876-asset Lyra export.
`D:\GameDev\Unreal_Projects\Start` is outside the P4 deployment, export, and test scope.

## Executive decisions

1. Replace the current nominal `logic` / `reconstruction` / `forensics` choice with three
   explicit, monotonic contracts: `architecture`, `logic-only`, and `full-fidelity`.
   Multi-profile export must use separate profile roots coordinated by one export-set manifest;
   one sidecar path must never be overwritten by another profile.
2. Replace the 78 MB monolithic project graph as the default JSON exchange artifact with an
   atomic manifest over immutable, deterministic node and edge shards. SQLite remains the query
   implementation, not the authoritative exchange format.
3. Normalize graph node, relation, and contributor identities in SQLite with integer keys while
   preserving the public string IDs through a compatibility query view and JSON output.
4. Extend Blueprint-to-C++ through a closed, typed behavior IR and staged node allowlists. Do not
   generate C++ directly from node titles, raw code fields, or arbitrary pin text, and do not claim
   broad Blueprint equivalence from compile success alone.
5. Introduce an explicit `snapshotId` across the export-set manifest, profile indexes, graph
   manifests, and SQLite metadata. Readers operate within one immutable published snapshot.
6. Add two Lyra acceptance layers: deterministic retrieval/path oracles and evidence-scored
   offline reasoning answers. Existing minimum counts and isolated required edges are necessary
   but are not an end-to-end gameplay reasoning accuracy test.

The first artifact-contract release that emits the new profile wire values and graph manifest is
a breaking USEM change. It must not silently advertise itself as USEM 2.8-compatible. Exact plugin,
schema, semantic revision, graph schema, and SQLite version numbers are assigned in the
implementation iteration, together, after the schemas and migration tests land.

## Audit evidence

### Accepted scale baseline

| Metric | Current Lyra value |
| --- | ---: |
| Indexed assets | 3,876 |
| Asset semantic JSON | 105,319,200 bytes |
| Complete `.uesem` | 441,151,698 bytes |
| Unified graph nodes | 53,047 |
| Unified graph edges | 115,802 |
| Monolithic graph JSON | 78,311,295 bytes |
| SQLite | 238,518,272 bytes |
| Recorded omissions | 2,534 |
| Export failures / dangling edges | 0 / 0 |

All 3,876 current sidecars use the `logic` profile. The accepted export contains no
`/Game/UERingTests/` packages or directories.

### Current profile implementation

`EUERingExportProfile` already declares `Logic`, `Reconstruction`, and `Forensics`, but this is not
a complete multi-profile implementation:

- `UUERingSettings` stores one `ExportProfile`, `FUERingExportManager` writes one sidecar path, and
  `FUERingValidator` requires every sidecar to match that one current setting.
- The Commandlet has no profile override or profile-set argument.
- The profile participates in the input fingerprint, so switching the setting invalidates and
  overwrites the prior profile output instead of retaining both.
- Only Audio, Animation, Material, and Niagara exporters branch on the profile. Material,
  Animation, and Niagara distinguish `Logic` from "not Logic"; `Reconstruction` and `Forensics`
  do not have independently testable contracts.
- The project index, graph, SQLite database, summaries, graph visualizations, bundles, MCP tools,
  and migration report all resolve one global output root.

Consequently, the existing enum is a hint, not proof of three supported export products.

### Current graph and SQLite implementation

The full rebuild creates the monolithic graph as an in-memory JSON object before rebuilding
SQLite. Incremental contributor updates use SQLite as their graph state and stream the same
monolithic compatibility JSON after commit. This removed the old whole-graph read but still
rewrites 78 MB after every graph-changing package update.

SQLite graph storage is dominated by repeated strings and their B-tree indexes:

| SQLite object group | Current bytes |
| --- | ---: |
| `graph_edges` table | 50,753,536 |
| graph edge primary/source/target/contributor indexes | 103,325,696 |
| `graph_nodes` table | 28,553,216 |
| graph node primary/kind/package indexes | 27,140,096 |

The four edge identity columns contain 30,191,503 bytes of raw UTF-8 text before table and index
overhead. Their distinct dictionaries require only 9,608,050 bytes in the current data:

| Column | Rows | Distinct values | Text bytes | Repeated bytes beyond one dictionary copy |
| --- | ---: | ---: | ---: | ---: |
| `source_node` | 115,802 | 28,053 | 10,998,365 | 7,495,358 |
| `target_node` | 115,802 | 52,944 | 11,844,125 | 5,912,866 |
| `relation` | 115,802 | 45 | 1,473,716 | 1,473,092 |
| `contributor_package` | 115,802 | 3,095 | 5,875,297 | 5,702,137 |

The Python query tool opens a new read-only connection per operation. Neighbor queries use the
source/target indexes; substring node search uses a full ordered scan. An exploratory warm-cache
Lyra measurement produced:

| Query | Result |
| --- | ---: |
| 400 outgoing neighbor queries, 1 worker | p50 0.222 ms, p95 0.340 ms, about 3,243 q/s |
| 400 outgoing neighbor queries, 8 workers | p50 2.249 ms, p95 3.720 ms, about 3,350 q/s |
| Missing node substring scan | 24.237 ms |
| `GameplayAbility` node substring scan | 26.938 ms |

These are diagnostic observations, not a checked-in benchmark baseline. A copied-database test
also showed that a long `BEGIN` reader prevents the current rollback-journal writer from
committing: with a 250 ms timeout the writer failed with `database is locked` after about 357 ms.

### Current executable reconstruction coverage

The accepted export contains 506 Blueprint-family assets, 1,246 graphs, and 16,881 nodes when the
three Control Rig assets are included. Only 24 Blueprint-family assets are `ready`; 482 are
`partial`. Across all asset
types the current totals are 475 `ready`, 482 `partial`, and 2,919 `blocked`, because Material
Instances and DataAssets now have working Editor asset backends.

Blueprint-family blockers are:

| Reason | Blocked operations |
| --- | ---: |
| `unsupportedGraphNodes` | 1,095 |
| `classDefaultLoweringUnavailable` | 296 |
| `componentConstructionUnavailable` | 57 |
| `interfaceGenerationUnavailable` | 21 |
| `derivedCompiledArtifact` | 23 |
| `timelineLoweringUnavailable` | 8 |

Across the K2 Blueprint, Widget, and Animation assets (excluding the separate Control Rig node
model), the most common node classes are `K2Node_CallFunction` (3,874), `K2Node_VariableGet` (2,976),
`K2Node_Knot` (1,100), `K2Node_VariableSet` (707), `K2Node_Event` (616),
`K2Node_PromotableOperator` (604), `K2Node_IfThenElse` (505), and
`K2Node_FunctionEntry` (483). Latent, async, delegate, macro, Animation Graph, Control Rig, UMG,
replication, and GAS-specific nodes are also present. A node-count percentage alone would hide
the semantic risk of those less frequent categories.

### Current Lyra acceptance gap

`acceptance/lyra.golden.json` verifies scale floors, size ceilings, domain counts, selected JSON
pointers, specialized graph integrity, and nine exact unified-graph edges. It does not verify that
an offline consumer can recover a complete gameplay explanation, distinguish evidence from an
Asset Registry dependency, reject an unsupported conclusion, or cite every step of a multi-asset
flow.

The current graph already contains useful evidence for such tests. For example:

- `B_ShooterGame_Elimination` enables ShooterCore, includes four action sets, and selects
  `HeroData_ShooterGame` as default pawn data.
- `HeroData_ShooterGame` selects the pawn class, camera mode, input config, tag relationship, and
  `AbilitySet_ShooterHero`.
- `AbilitySet_ShooterHero` grants Dash with `InputTag.Ability.Dash`; Dash uses
  `GE_HeroDash_Cooldown`.
- `GA_AutoRespawn` binds and removes the health death delegate, calls Delay, broadcasts gameplay
  messages, and calls `RequestPlayerRestartNextFrame`.
- `B_TeamDeathMatchScoring` observes elimination scoring, updates team-score properties, starts
  phases, manages countdown state, and handles victory.

These facts are currently checked, if at all, as unrelated local assertions rather than coherent
workflow answers.

## Target contracts

### 1. Multi-profile export set

Profiles are ordered semantic projections, not quality labels:

| Profile | Required content | Explicit exclusions |
| --- | --- | --- |
| `architecture` | Asset identity, class/inheritance/interfaces, public function and property signatures, component topology, dependencies, domain projections, and cross-asset relations | Internal node/pin flow, authoring-only values, pose/sample data, editor presentation |
| `logic-only` | Everything in `architecture`, plus behavior graphs, stable node/pin topology, state/transition/timing logic, typed defaults needed to interpret behavior, and executable behavior IR when available | Replaceable media samples, binary/compiled caches, editor presentation not needed for behavior |
| `full-fidelity` | Everything in `logic-only`, plus stable authored data required for semantic reconstruction and diagnostics, including supported editor-only construction data | Transient state, derived caches, platform cook products, secrets, and opaque engine-private data |

The shared portions must be semantically identical. Exporters declare capabilities and produce
profile projections from named semantic sections; they must not scatter ad hoc `Profile != Logic`
checks. Every excluded supported section produces a stable omission code. `full-fidelity` means
maximum stable authored fidelity, not binary-identical `.uasset` recovery.

Recommended layout:

```text
.uesem/
  export-set.manifest.json
  profiles/
    architecture/
      content|maps|index|cpp|reports|graphs/...
    logic-only/
      content|maps|index|cpp|reports|graphs/...
    full-fidelity/
      content|maps|index|cpp|reports|graphs/...
```

The root manifest records `snapshotId`, selected profiles, default profile, schema/revision tuple,
and each profile index/graph manifest. Commandlet selection is explicit (`-Profiles=`); invalid,
empty, duplicate, or incompatible combinations fail without falling back to all profiles. Editor
settings expose a profile set and a default query profile. Bundle and MCP requests identify their
profile, and responses echo both `profile` and `snapshotId`.

Migration rules:

- Existing wire values remain readable only through a documented legacy reader/migration path.
- New exports emit only the new profile IDs and new layout after the breaking schema boundary.
- Do not duplicate the default profile at both the root and `profiles/` paths, and do not depend on
  symlinks or hardlinks for compatibility.
- Profile-specific semantic hashes, section byte counts, reconstruction readiness, graph counts,
  and omissions are indexed independently.
- A package change publishes all requested profiles under one new export-set snapshot or publishes
  none of them.

### 2. Sharded unified graph JSON

Publish `index/project.uesem.graph.manifest.json` as the only mutable graph entry point. Shards are
immutable and content-addressed; the manifest is written atomically after all referenced shards
and SQLite state are durable.

Recommended deterministic partitioning:

- Node shards: first byte of SHA-256 over the canonical external node ID, 256 buckets.
- Edge shards: first byte of SHA-256 over `contributorPackage`, 256 buckets.
- Rows inside every shard retain the existing canonical case-insensitive plus binary tie-break
  ordering.
- Edge ownership stays contributor-local, so one changed package rewrites one edge bucket. Node
  buckets touched by old or new endpoints are recomputed after orphan pruning.

Each manifest shard record includes kind, bucket, relative path, SHA-256, UTF-8 bytes, row count,
first/last canonical key, and partition-key version. The manifest includes total statistics,
`snapshotId`, profile, USEM version, graph schema version, and the SQLite snapshot identity.

Endpoint closure, uniqueness, statistics, ordering, hashes, and path containment are validated in
a streaming or bounded-memory pass. Validation must not reassemble the complete graph object.
Incremental output must be byte-identical to a full rebuild at the manifest and shard level.

The monolithic `project.uesem.graph.json` remains an explicitly requested compatibility artifact
for one migration window. It is not generated by default once all checked-in consumers use the
manifest. Acceptance, Bundle, MCP, validator, and offline tools migrate before default generation
is disabled.

### 3. Normalized SQLite graph schema

Use integer storage keys without changing public graph identities:

```text
graph_nodes(
  node_key INTEGER PRIMARY KEY,
  node_id TEXT NOT NULL UNIQUE,
  ...
)
graph_relations(
  relation_key INTEGER PRIMARY KEY,
  relation TEXT NOT NULL UNIQUE
)
graph_contributors(
  contributor_key INTEGER PRIMARY KEY,
  package_name TEXT NOT NULL UNIQUE
)
graph_edges(
  source_node_key INTEGER NOT NULL REFERENCES graph_nodes,
  target_node_key INTEGER NOT NULL REFERENCES graph_nodes,
  relation_key INTEGER NOT NULL REFERENCES graph_relations,
  contributor_key INTEGER NOT NULL REFERENCES graph_contributors,
  ...
)
```

Use names such as `node_key` for database keys so they cannot be confused with Blueprint
`raw_node_id`. Official consumers query a compatibility view, provisionally `graph_edges_text`,
that exposes `source_node`, `target_node`, `relation`, and `contributor_package` strings. Exact
neighbor queries first resolve one external `node_id` to `node_key`, then use integer source/target
indexes. Foreign keys are enabled and checked, but the existing explicit dangling-endpoint check
remains an acceptance invariant.

Full rebuild assigns dictionary keys deterministically. Incremental insertion must not make JSON
or query ordering depend on integer allocation order. Schema compatibility is checked by user
version plus required metadata and columns; incompatible state falls back to a deterministic full
rebuild. There is no in-place migration of an accepted older database during package update.

The implementation iteration must report `dbstat` before/after measurements. P4 acceptance
requires at least a 25% reduction in total SQLite bytes from the 238,518,272-byte Lyra baseline,
unless the iteration documents a stronger measured reason to revise the threshold. Indexed
neighbor-query p95 may not regress by more than 10%.

### 4. Executable Blueprint behavior IR

Replace `cpp.graph.translate` blockers incrementally with a versioned, closed behavior IR. The IR
contains typed values, basic blocks, explicit control-flow successors, data dependencies, calls,
property access, conversions, source graph/node/pin IDs, and network/latent policy. Generated C++
is a backend result, never input data embedded in USEM.

Coverage tiers:

1. Synchronous functions and events: entry/result, literals and pin defaults, reroutes, variables,
   pure and impure reflected calls, parent calls, branches, sequence, selects, casts, common typed
   operators, enum switches, arrays, and supported struct make/break.
2. Class construction: class defaults, components, implemented interfaces, Timeline lowering, and
   delegate bind/unbind/call with lifecycle verification.
3. Stateful behavior: latent calls, timers, async proxies, GAS ability tasks, gameplay-message
   listeners, RPC/authority metadata, cancellation, and generated callback/state-machine code.

Animation Graph and Control Rig execution remain separate backend projects until their engine
runtime contracts are modeled. They must not be counted as ordinary K2 tier coverage.

Every node adapter has an allowlisted node class, pin contract, type/conversion rules, and explicit
failure reason. Unknown nodes, wildcard types that cannot be resolved, opaque macros, bound
delegates, or unsupported latent behavior continue to block strict output. Comments and reroutes
may be semantically elided only after preserving source traceability.

Acceptance progresses from small real Lyra assets to workflows, not synthetic node totals:

- Tier 1 starts with non-empty assets using event/function entry, reflected call, variable, branch,
  and return behavior; candidate selection is pinned by the P4 benchmark/oracle iteration.
- Generated plugins build Editor, Development, and Shipping with strict BuildPlugin settings.
- UE Automation executes original Blueprint and generated native class against the same fixture and
  compares observable calls, outputs, property changes, delegate state, and latent completion.
- Re-export compares normalized class/property/behavior semantics; compile-only success is not an
  equivalence result.
- Coverage reports ready assets, ready graphs, executable nodes, and blocker reasons by tier. The
  final gate is based on pinned Lyra golden assets and traces, not a headline percentage.

### 5. Snapshots, concurrent reads, and performance

`snapshotId` is a deterministic hash of the project/profile schema tuple and sorted asset semantic
identities. It is stored in:

- `export-set.manifest.json`;
- each profile index and graph manifest;
- SQLite `metadata`;
- Bundle manifests and MCP/query responses.

A reader opens the root manifest, chooses a profile and snapshot, then opens only artifacts named
by that snapshot. Immutable graph shards prevent mixed old/new JSON. SQLite query sessions begin a
short deferred read transaction, verify the requested `snapshotId`, execute a bounded request, and
end the transaction. Long-lived cursors are paged with a snapshot-bound continuation token; they
never hold a read transaction across user think time.

Do not adopt WAL only to make a concurrency test pass. WAL is evaluated against rollback journal
for writer latency, reader latency, checkpoint cost, crash recovery, deployment copying, and stray
`-wal`/`-shm` files. The chosen mode must publish a complete database snapshot and leave no journal
artifacts in the accepted `.uesem` tree. Writer busy timeout/retry is bounded and observable; it
must not silently skip an index update.

Add `tools/usem_benchmark.py` with machine-readable output and these scenarios:

- full export, no-change package, changed contributor, and deleted contributor;
- graph manifest open, one node shard, one edge shard, and complete streaming validation;
- exact node lookup, incoming/outgoing neighbors, typed path, asset filters, existing substring,
  missing substring, and bounded result serialization;
- 1, 4, and 8 concurrent readers during repeated snapshot publication;
- cold and warm runs, p50/p95/max, throughput, peak RSS, artifact bytes, SQLite page counts, and
  fallback/full-rebuild counts.

The benchmark records engine/plugin/schema/revision, OS, CPU, database snapshot, command, and
iteration count. Checked-in thresholds use medians across repeated runs and allow explicit machine
classes. P4 targets are: no query correctness errors or `SQLITE_BUSY`, no full-export regression
above 10%, changed-contributor p95 no worse than 0.19.1, and graph publication that no longer scales
with rewriting the full 78 MB JSON.

### 6. Lyra end-to-end offline reasoning accuracy

Create `acceptance/lyra.reasoning.golden.json`. Each case records:

- a stable question/task ID and natural-language prompt;
- allowed profile and snapshot requirements;
- required and forbidden entities, relations, qualifiers, and ordered/partially ordered trace
  steps;
- acceptable alternative paths;
- required evidence source and JSON pointer policy;
- claims that must be answered `unknown` or rejected;
- severity and metric weight.

Seed workflows:

1. Shooter Elimination experience activation through GameFeature/action sets to pawn data.
2. Pawn data through input config and ability set to Dash input tag and cooldown effect.
3. Dash activation, commit, direction/montage selection, authority/local control, gameplay cue, and
   termination behavior.
4. Death observation through health delegate, respawn delay/message/UI, ability cleanup, and
   `RequestPlayerRestartNextFrame`.
5. Elimination scoring through team-score updates, target-score victory, countdown, phase change,
   and player restart.
6. Elimination feed authority/message relay to the relevant UI data path.
7. Negative controls, including facts true for ShooterCore but not the default SimplePawnData
   experience and claims supported only by generic `dependsOn` edges.

Use two gates:

- Deterministic oracle gate: retrieval, paths, evidence closure, and negative controls must pass
  100%. This tests exporter and query correctness without model variance.
- Pinned offline reasoning gate: a versioned model/prompt emits structured claims. Required-fact
  recall must be at least 90%, claim precision at least 95%, evidence resolution 100%, and critical
  unsupported claims zero in the worst of three runs. Store summaries and hashes, not sensitive or
  unbounded model transcripts.

An answer receives credit only when its evidence resolves within the same snapshot. A path made
only from generic Asset Registry dependencies cannot satisfy a typed gameplay claim. The evaluator
reports missing facts, wrong relations, unsupported claims, invalid evidence, and abstention errors
separately so exporter regressions are distinguishable from reasoning regressions.

## Delivery roadmap

### P4.0 - Design audit and contracts

Status: completed by this document.

- Freeze the 0.19.1 Lyra measurements above as the comparison baseline.
- Record the six target contracts and compatibility boundaries.
- Make no plugin, schema, semantic, SQLite, or output changes.

### P4.1 - Benchmark and reasoning oracle foundation

- Add the benchmark result schema/tool and deterministic unit fixtures.
- Add the Lyra reasoning golden format, deterministic evaluator, and initial seven workflows.
- Capture current 0.19.1 retrieval, path, latency, and evidence baseline without changing exports.
- Add failure diagnostics that identify the exact missing edge, pointer, query, or unsupported
  claim.

Exit gate: Python tests, deterministic Lyra oracle baseline, and repeatable benchmark JSON pass;
no golden threshold is weakened to accommodate a missing current fact.

### P4.2 - Multi-profile export set

- Land profile schema, export-set manifest, path resolver, Commandlet/settings selection, and
  per-profile incremental fingerprints.
- Convert exporters to declared section capabilities and add cross-profile invariants.
- Migrate validator, Bundle, MCP, summaries, indexes, cleanup, auto-export, delete, and rename.
- Perform a full Lyra export for each profile and publish per-profile size/omission/readiness data.

Exit gate: all requested profiles publish atomically, shared sections match, profile switching does
not overwrite other output, and every old single-root consumer is either migrated or explicitly
rejected.

### P4.3 - Graph manifest, shards, and atomic snapshots

- Land graph manifest/shard schemas, immutable publication, streaming validator, and garbage
  collection that preserves all manifests retained for concurrent readers.
- Migrate acceptance, Bundle, MCP, query tooling, and incremental graph equivalence tests.
- Keep monolithic output behind an explicit compatibility option only.

Exit gate: full/incremental byte equivalence, zero dangling edges, bounded-memory validation, no
mixed snapshot under fault injection, and changed contributors do not rewrite the entire graph.

### P4.4 - Normalized SQLite and concurrent query sessions

- Land the normalized node/relation/contributor dictionaries, compatibility view, integer indexes,
  snapshot metadata, bounded continuation tokens, text-search strategy, and shared query contract
  for Python and MCP.
- Test fallback from missing/corrupt/incompatible databases and fault-injected commits.
- Run 1/4/8-reader publication stress and before/after `dbstat` benchmarks.

Exit gate: integrity and foreign-key checks pass, JSON and SQLite snapshot/count/hash identities
match, no busy or mixed-snapshot responses occur, total SQLite size drops at least 25%, and indexed
query p95 remains within the agreed regression budget.

### P4.5 - Blueprint behavior C++ tiers

- Land typed behavior IR and Tier 1 adapters first, then class construction and stateful adapters in
  separate commits with separate golden assets.
- Expand strict C++ backend, code injection defenses, deterministic generation, UBT targets,
  original-vs-generated UE Automation fixtures, and normalized re-export comparison.
- Publish blocker reduction by reason and pinned Lyra workflow coverage after each tier.

Exit gate: every newly `ready` graph has compile, runtime trace, and re-export evidence; unknown,
latent, network, delegate, AnimGraph, and Control Rig cases remain blocked until their own contract
passes.

### P4.6 - Final Lyra reasoning and performance acceptance

- Run all profile exports, graph/SQLite validation, Blueprint golden execution, benchmark suite,
  deterministic workflow oracle, and pinned three-run reasoning evaluation.
- Update golden thresholds only from reviewed, reproducible evidence.
- Document final artifact sizes, profile coverage, query latency/concurrency, reconstruction
  readiness, reasoning metrics, and residual blockers.

Exit gate: all P4 contracts and operational constraints below pass in one final Lyra acceptance
checkpoint with no `UERingTests` directories and a clean repository.

## Verification and operational constraints

Every code-bearing P4 iteration finishes with the established sequence:

1. Python `unittest`.
2. Strict BuildPlugin for Win64 Editor/Development/Shipping with PCH, shared PCH, and unity disabled.
3. In-place `robocopy /MIR` deployment only to
   `D:\GameDev\Unreal_Projects\LyraStarterGame\Plugins\UERing`.
4. UE Automation only in Lyra.
5. Final Lyra `-All` export using `/Engine/Maps/Entry` and `-NoLoadStartupPackages`.
6. UE validation, offline schema/source-hash/graph checks, SQLite integrity and dangling checks,
   golden acceptance, P4 benchmark/oracle gates, and `UERingTests` directory check.
7. Removal of the complete plugin `.codex-work` directory and any versioned build/test artifacts.
8. One reviewed Git commit with a clean working tree.

P4 work never deploys, exports, validates, or creates tests in
`D:\GameDev\Unreal_Projects\Start`. Temporary development files are confined to
`D:\GameDev\Plugins\UE_ring\.codex-work`. Build packages and generated reconstruction plugins are
temporary evidence, not versioned repository content.

## Explicit non-goals

- Reimplementing P0-P3 export coverage or rerunning their historical acceptance as new work.
- Parallel UObject traversal before export is split into documented game-thread collection and
  thread-safe serialization/index stages.
- Binary-identical `.uasset`, Blueprint bytecode, cooked asset, native function body, derived cache,
  or engine-private state reconstruction.
- Treating full-fidelity as permission to export secrets, transient objects, or replaceable binary
  media samples without an explicit stable semantic contract.
- Treating a generic dependency path, successful C++ compile, graph node count, or model prose as
  proof of gameplay semantic equivalence.
