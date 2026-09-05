# Lyra Scale Acceptance 0.18.0

Date: 2026-08-02

Engine: Unreal Engine 5.8.0

Project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

Plugin semantic revision: 36

## Scope

This iteration completes executable owned-object DataAsset reconstruction and the first exact
StateTree authored-data round trip. DataAsset semantics now preserve persistent nested UObjects,
their Outer graph, creation method, reflected properties, and internal references. The typed
property backend adds `TOptional`, empty delegates, `FInstancedPropertyBag`, and
`FInstancedStruct`.

StateTree uses an authored/derived lifecycle contract. `EditorData` and its owned objects are
restored from USEM. Compiled root properties remain available for offline analysis but are not
copied into the target. The generated plugin then calls the official UE 5.8 StateTree compiler.

## Export result

- Indexed assets: 3,876
- Failed assets: 0
- Skipped generated external actors: 4,809
- Asset semantic JSON bytes: 105,318,308
- Complete `.uesem` bytes: 424,021,683 (404.38 MiB)
- Complete `.uesem` files: 12,754
- Unified graph nodes: 53,047
- Unified graph edges: 115,802
- Project graph JSON bytes: 77,647,042
- SQLite bytes: 221,118,464
- Recorded semantic omissions: 2,534

The final clean revision-36 export reported `exported=3875 unchanged=1 failed=0`. All 3,876
sidecars use schema 2.7.0 and semantic revision 36. All 4,809 exclusions are
`generatedExternalActor`; they are generated World Partition packages counted in coverage but
not loaded and do not produce sidecars. No generated reconstruction plugin, generated asset,
test asset, stale schema, or `/Engine/Transient` semantic reference remains in Lyra.

## Reconstruction readiness

| Readiness | Assets | Change from 0.17.1 |
| --- | ---: | ---: |
| `ready` | 475 | +38 |
| `partial` | 482 | -38 |
| `blocked` | 2,919 | 0 |

All 113 assets exported by the DataAsset backend are now `ready` with confidence 1.0. The 38
assets moved from partial to ready because their persistent owned-UObject graphs are now
represented and executable. The remaining blocked assets are dominated by asset families with
no asset-construction backend, while Blueprint-family assets retain explicit graph, component,
interface, Timeline, and class-default lowering blockers.

## StateTree golden

The golden asset is
`/ShooterExplorer/AI/StateTrees/Trees/L_STT_FollowPlayer`. Its sidecar is 376,117 bytes, has
16 owned objects, 30 root properties, and a `state-tree-editor-compile-v1` policy with one
authored root property (`EditorData`) and 29 derived root properties.

The source contains 218 `FInstancedStruct` occurrences across the complete semantic document:
82 valid dynamic structures and 136 explicit empty structures. These include compare-distance
conditions and instance data, debug-text and delay tasks, Blueprint task wrappers, and binding
descriptors. Root parameters preserve stable Property Bag layout
`PropertyBag_925ee960c9556cc9`, `StopDistance=200`, and `StartFollowDistance=1000`, with no
transient object path.

Strict generation emitted six executable exact operations: create asset, create owned objects,
apply owned properties, apply authored root properties, compile StateTree, and save. The
generated plugin compiled, its Lyra automation test built and saved
`/Game/UERingGenerated/L_STT_FollowPlayer`, and the official StateTree compiler completed with
no error. Re-export differed only in a declared derived save-version cache; after removing the
29 policy-declared derived root properties, the full authored semantics trees were identical.

## Verification

- Main BuildPlugin Editor/Development/Shipping for Win64 with strict includes and no PCH/unity: passed.
- Python unit tests: 78 passed.
- UE Automation `UERing.`: 11 passed in one process.
- Generated StateTree Editor plugin compile/link: passed.
- Generated StateTree build/save automation test: passed.
- StateTree authored semantic re-export comparison: exact match.
- Full Lyra export: 3,876 indexed assets, 0 failures.
- Strict index validation with semantic and source hashes: passed.
- Strict unified project graph validation: passed.
- SQLite `integrity_check`: `ok`; asset rows: 3,876.

Logs:

- `Saved/Logs/UERing-0.18.0-Revision36-FullTests.log`
- `Saved/Logs/UERing-0.18.0-Revision36-StateTree-InstancedSource-Forced.log`
- `Saved/Logs/UERing-0.18.0-Revision36-StateTree-InstancedBuild.log`
- `Saved/Logs/UERing-0.18.0-Revision36-StateTree-Reexport.log`
- `Saved/Logs/UERing-0.18.0-Revision36-FullExport.log`

Only LyraStarterGame was deployed and exported. The Start project was not modified.
