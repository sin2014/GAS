# UE Ring 0.9.3 Lyra-scale acceptance

Date: 2026-08-01
Engine: Unreal Engine 5.8.0
Plugin: UE Ring 0.9.3
USEM schema: 2.2.3
Semantic revision: 16

## Iteration scope

This iteration closes the Niagara commandlet loading gap, adds portable domain projections for GAS, Lyra Experience, GameFeature, StateTree, DataRegistry, and Enhanced Input, introduces explicit reconstruction IR, and separates intentionally excluded generated packages from unsupported authored assets.

## Acceptance results

| Check | Start | LyraStarterGame |
|---|---:|---:|
| Indexed authored assets | 2,821 | 3,876 |
| Successful current semantics | 2,821 | 3,876 |
| Missing / failed / unsupported index entries | 0 | 0 |
| Generated external packages excluded by coverage | 526 | 4,809 |
| NiagaraScript + NiagaraEmitter successful | 0 | 35 |
| Semantic sidecar bytes | 54,405,384 | 303,104,711 |
| Reconstruction revisions equal to 16 | 2,821 | 3,876 |
| PostLoad-transform diagnostics | 0 | 15 |
| Python contract/validator tests | 36/36 | same implementation |
| UE automation tests | 11/11 | same installed binary |
| JSON/hash cross-validation | pass | pass |
| SQLite quick integrity check | pass | pass |

The final deterministic full exports reported:

- Start: `exported=2821 unchanged=0 failed=0`
- LyraStarterGame: `exported=3876 unchanged=0 failed=0`

No `export-errors.jsonl` remains after either final export.

## Niagara resolution

The prior 35 failures were 14 `NiagaraScript` and 21 `NiagaraEmitter` assets. `UUERingExportCommandlet` had overridden the Unreal `UCommandlet` default with `IsClient=false` while retaining the server/editor contexts. UE package loading consequently filtered client-only Niagara objects even though their package files and classes were valid. The export and validation commandlets now explicitly load Client, Server, and Editor objects, matching the official Python commandlet behavior.

Fifteen additional Niagara packages become dirty during deterministic UE 5.8 `PostLoad` migration. In a fresh commandlet process this is not an unsaved user edit. The exporter reads the on-disk hash, exports the effective engine-loaded state without saving the `.uasset`, and records `enginePostLoadTransform` in diagnostics and reconstruction evidence plus `preTransformObjectState` as unrecoverable. Interactive editor export still rejects genuinely dirty assets.

Owned-object graphs now contain only persistent same-package objects reachable from the root through real UObject references. This removed stale versions and derived package residue while retaining the active Niagara graph and all reachable reference edges. Root properties are no longer duplicated in `$root.properties`.

## Domain coverage

| Domain | Lyra assets |
|---|---:|
| GAS | 129 |
| Lyra Experience | 25 |
| GameFeature | 5 |
| StateTree | 8 |
| DataRegistry | 6 |
| Enhanced Input | 37 |

Representative non-empty projections include `GA_Hero_Dash`, `DA_ExamplePlaylist`, the ShooterCore `GameFeatureData`, `L_STT_FollowPlayer`, `AccoladeDataRegistry`, and `IMC_Default`. Detection uses the effective generated-class lineage and reflection, so project subclasses are covered without linking UE Ring against Lyra game modules.

## Reconstruction IR

Every semantic sidecar contains `reconstruction` with:

- target `semanticEquivalent` and confidence method `exporter-heuristic-v1`;
- bounded overall, structure, behavior, and defaults confidence;
- evidence, reconstructable, and unrecoverable code lists;
- ordered source-backed steps for asset skeleton, dependencies, semantics, and defaults.

The Lyra recoverability distribution is 506 high, 783 partial, and 2,587 low. Low confidence is intentionally common for reflection-fallback binary assets. The IR does not claim to recover binary identity, opaque custom serialization, compiled bytecode, native function bodies, or macro expansion.

## Unsupported and excluded assets

The former Lyra `unsupported=4809` count consisted entirely of World Partition generated packages: 4,769 `__ExternalActors__` and 40 `__ExternalObjects__` packages under ShooterMaps. Start's former 526 unsupported entries had the same generated-package origin.

These packages are implementation shards of their owning World, not independent authored semantic units. They are now skipped before per-asset export and omitted from `assets`; `coverage.exclusions` retains the exact reason and count as `generatedExternalActor`. A genuinely unmatched authored project asset remains an explicit `unsupported` index entry with `semanticFile: null`. The reflection fallback currently means that normal loadable project assets should rarely reach that state.

## Compactness

Sidecars are condensed by default; `bPrettyJson=true` remains available for manual reading. Condensing removes whitespace only. In the measured Lyra export it reduced the complete 0.9.2 representation from 520,620,407 to 303,104,711 bytes (41.8%). The final result is about 1.25% smaller than the 0.8.0 Lyra export of 306,951,388 bytes while adding the 35 formerly missing Niagara assets, all six domain projections, index domain fields, and reconstruction IR.

Each asset keeps a single integer `semanticRevision` rather than repeating project, engine, and full exporter metadata. `HasSameExportState` requires this revision, preventing a generator upgrade from incorrectly preserving sidecars whose source `.uasset` did not change.
