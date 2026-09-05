# UE Ring 0.8.0 Lyra-scale acceptance

Date: 2026-08-01  
Engine: Unreal Engine 5.8.0  
Plugin: UE Ring 0.8.0  
USEM schema: 2.1.0

## Iteration scope

This iteration targets offline analysis of projects with Lyra-scale module, GameFeature, Blueprint, and dependency graphs. It adds:

- structured Blueprint pin, member, map-value, variable, and local-variable types;
- inherited Blueprint class-default overrides, function flags, metadata, and local variables;
- specialized `UserDefinedStruct` and `UserDefinedEnum` definition semantics;
- graph-local compact pin IDs with deterministic node-qualified fallbacks for real GUID collisions;
- project content-plugin mount discovery for full export, indexing, validation, lifecycle export, bundles, and MCP;
- one canonical index/export asset per package, including packages with multiple redirector registry entries;
- SQLite failure-path statement cleanup so a failed rebuild cannot destroy an open database;
- validator checks that reject legacy Blueprint type strings and invalid structured types.

## Acceptance results

| Check | Start | LyraStarterGame |
|---|---:|---:|
| Index entries | 3,347 | 8,685 |
| Duplicate package entries | 0 | 0 |
| Current semantic assets | 2,821 | 3,841 |
| Unsupported assets | 526 | 4,809 |
| Explicitly missing assets | 0 | 35 |
| Semantic JSON bytes | 62,915,285 | 306,951,388 |
| Blueprint semantics | 93 | 335 |
| Definition semantics | 16 | 16 |
| Structured Blueprint types | 20,845 | 49,725 |
| Legacy Blueprint type strings | 0 | 0 |
| Assets with inherited/default overrides | 32 | 234 |
| Exported class-default entries | 86 | 633 |
| Exported local variables | 28 | 143 |
| Python contract/validator tests | 30/30 | same implementation |
| UE automation tests | 10/10 | same installed binary |
| Index + source-hash validation | pass | pass |

Start's final deterministic full export reported `exported=0 unchanged=2821 failed=0`.

Lyra's final full export reported `exported=0 unchanged=3841 failed=35`. The index, dependency graph, and SQLite search database were still rebuilt successfully and validate correctly; missing semantics are explicit rather than silently treated as complete.

## Lyra mount coverage

Full export now scans `/Game` and mounted, content-capable project plugins. The final Lyra index contains:

| Mount | Assets |
|---|---:|
| `/Game` | 2,840 |
| `/LyraExampleContent` | 78 |
| `/LyraExtTool` | 1 |
| `/PocketWorlds` | 3 |
| `/ShooterCore` | 260 |
| `/ShooterExplorer` | 51 |
| `/ShooterMaps` | 5,330 |
| `/ShooterTests` | 35 |
| `/TopDownArena` | 87 |

Engine-installed plugin content remains excluded. This keeps project architecture complete without turning the export into an engine source dump.

## Compactness

All 2,821 Start and 3,841 Lyra semantic files use centralized project/engine provenance from the index. Per-asset files contain only the schema version, exporter name, asset identity, dependencies, and asset-specific semantics.

For Start `/Game`, semantic JSON totals 58,430,220 bytes versus the pre-iteration baseline of 57,954,974 bytes, an increase of about 0.82% while adding structured types, inherited defaults, function metadata, and specialized definitions. `BP_BattleManager` is 840,252 bytes pretty-printed and 574,867 bytes minified. Its pre-iteration minified size was 643,120 bytes, so semantic payload size fell by about 10.6%; pretty-print whitespace hides most of that reduction on disk.

Pin IDs use a bare 36-character GUID when it is unique within the graph. Real Blueprint assets reuse some pin GUIDs across nodes, so a deterministic graph/node/pin path is retained only for collisions:

| Project | Bare GUID IDs | Collision fallbacks | Fallback share |
|---|---:|---:|---:|
| Start | 18,544 | 2,081 | 10.09% |
| LyraStarterGame | 43,233 | 5,603 | 11.47% |

Removing those fallbacks would make links ambiguous and would reduce correctness, not just size.

## Known gap

Thirty-five Lyra assets remain explicit `missing` entries: 14 `NiagaraScript` and 21 `NiagaraEmitter` packages. In `UERingExportCommandlet`, both the redirect-aware soft path and `FAssetData` entry fail to resolve a UObject, while the Python commandlet can load the same package after Python/editor initialization. These assets are listed in:

`LyraStarterGame/.uesem/logs/export-errors.jsonl`

The plugin deliberately leaves them failed. Marking them unsupported or emitting registry-only placeholders would hide missing Niagara graph logic and violate the completeness goal.

## Next iteration

1. Add a dedicated Niagara load/export path tested in commandlet and normal editor contexts, then require Lyra full export to reach zero missing supported assets.
2. Add domain exporters for Gameplay Ability System, Lyra Experience/GameFeature actions, StateTree, DataRegistry, and Enhanced Input relationships instead of relying mainly on generic owned-object or reflection graphs.
3. Add streaming/incremental index construction so larger projects do not require the full JSON index and dependency graph in memory before SQLite commit.
4. Measure repeated structured type/property fragments and introduce dictionaries only where they produce material savings without making individual sidecars impossible to analyze offline.
5. Define a reconstruction IR and confidence model before generating C++. Current USEM is suitable for analysis and assisted translation, but it does not preserve enough latent-type, macro-expansion, engine-runtime, and generated-code detail for lossless Blueprint-to-C++ reconstruction.
