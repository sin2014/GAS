# Lyra Scale Acceptance 0.19.0

Date: 2026-08-02

Engine: Unreal Engine 5.8.0

Project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

Plugin semantic revision: 37

## Scope

This iteration adds package-scoped incremental maintenance for the project index, dependency
graph, unified semantic graph, and SQLite query database. Editor save/delete/rename flows,
Content Browser exports, commandlet package exports, and MCP single-asset exports now use the
same invalidation path.

The project index stores each asset's `semanticSectionBytes`, so global section statistics can be
re-aggregated without parsing every sidecar. Project Graph schema 1.1 assigns every edge to a
`contributorPackage`. A semantic change removes and rebuilds only that contributor's graph
fragment, then prunes orphan non-asset nodes and verifies endpoint closure. SQLite schema 5 applies
the corresponding asset, tag, domain, dependency, node, and edge changes inside one immediate
transaction and performs a quick integrity check before commit.

## Export result

- Indexed assets: 3,876
- Failed assets: 0
- Skipped generated external actors: 4,809
- Asset semantic JSON bytes: 105,319,200
- Complete `.uesem` bytes: 438,358,224 (418.05 MiB)
- Complete `.uesem` files: 12,754
- Unified graph nodes: 53,047
- Unified graph edges: 115,802
- Project graph JSON bytes: 78,311,295
- SQLite bytes: 235,724,800
- Recorded semantic omissions: 2,534

The final clean export reported `exported=0 unchanged=3876 failed=0`. All sidecars use USEM
schema 2.8.0 and semantic revision 37. No `/Game/UERingTests/` content or semantic output remains.
The 4,809 explicit exclusions remain generated World Partition packages; they are counted in
coverage and skipped before load because they are generated implementation storage rather than
independent authored assets.

Asset semantic bytes increased by only 892 bytes from revision 36. Complete output increased by
14,336,541 bytes, primarily because SQLite now stores and indexes contributor ownership for every
unified-graph edge. The graph JSON itself increased by 664,253 bytes. Contributor ownership is
required for correct fragment deletion, but integer contributor IDs and a normalized contributor
table should be evaluated in the next storage iteration.

## Incremental performance

Measurements use package `/Game/System/Experiences/B_LyraDefaultExperience` against the clean
3,876-asset Lyra export.

| Operation | Index time | Commandlet time | Notes |
| --- | ---: | ---: | --- |
| Prior package export baseline | about 4.4 s | about 4.9 s | Reprocessed unified graph even when sidecar hash was unchanged |
| Revision 37 no-change package export | 0.564 s | 1.00 s | 1 asset row, 0 graph contributors |
| Revision 37 changed graph contribution | 3.510 s | test scope | 1 asset row, 1 graph contributor |
| Revision 37 final full index rebuild | 13.479 s | 40.97 s full commandlet | 3,876 assets, all sidecars unchanged |

The no-change stage profile was: prior index load 0.103 s, Asset Registry discovery 0.063 s,
entry update 0.045 s, compact index/dependency JSON 0.091 s, unified graph 0 s, and SQLite
transaction 0.262 s. The changed-contribution profile spent 2.826 s parsing and rewriting the
78 MB unified graph. That monolithic JSON operation is now the primary incremental hotspot.

## Correctness contracts

- Dirty asset rows include the requested package and dependency/referencer rows whose counts can
  change.
- Membership-changing updates expand to graph contributors that target affected packages.
- A contributor update removes all old owned edges, rebuilds current evidence, prunes orphan
  non-asset nodes, and rejects dangling endpoints.
- An old graph without compatible schema or contributor ownership falls back to a full rebuild.
- SQLite update falls back to an atomic full rebuild when the database is missing or not schema 5.
- No-change and changed-contribution UE tests compare project index, dependency graph, and unified
  graph output byte for byte against a subsequent full rebuild.
- Lifecycle rename/delete tests verify that old package paths invalidate indexes without being
  queued as source assets and that deleted pending exports are cancelled.
- Full-export finalization removes empty directories left after orphan sidecars, summaries, and
  graph artifacts are deleted.

## Verification

- Strict BuildPlugin Editor/Development/Shipping for Win64 with no PCH, shared PCH, or unity: passed.
- Python unit tests: 79 passed.
- UE Automation `UERing.`: 11 passed in one process.
- Changed-contribution incremental/full byte-equivalence test: passed.
- Full Lyra export: 3,876 indexed assets, 0 failures.
- Project index validation including semantic hashes: passed.
- Unified project graph validation: passed.
- Lyra golden acceptance suite: passed.
- SQLite user version: 5; `integrity_check`: `ok`.
- SQLite asset rows: 3,876; graph nodes: 53,047; graph edges: 115,802.
- Graph edges without contributor: 0; dangling graph edges: 0.

Logs:

- `Saved/Logs/UERing-0.19.0-Revision37-ChangedEquivalence.log`
- `Saved/Logs/UERing-0.19.0-Revision37-Lifecycle.log`
- `Saved/Logs/UERing-0.19.0-Revision37-AllTests-Final.log`
- `Saved/Logs/UERing-0.19.0-Revision37-FinalFullExport.log`

## Next P3 work

The next performance iteration should normalize contributor package strings to integer IDs in
SQLite, split or shard the unified graph persistence so a changed package does not parse and
rewrite 78 MB, and make C++ reverse links and Blueprint migration candidates incrementally
maintainable. Full export still deliberately rebuilds all project-level indexes to provide a clean,
deterministic checkpoint.

Only LyraStarterGame was deployed and exported. The Start project was not modified.
