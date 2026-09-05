# Lyra-scale acceptance report: UE Ring 0.10.0

Date: 2026-08-01

Target project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

This acceptance run used Unreal Engine 5.8.0 and the default `logic` export profile. The Start project was not installed, exported, or modified during this iteration.

## Result

UE Ring 0.10.0 exported every supported authored Lyra asset without a load or serialization failure. Semantic revision 19 first forced all semantic documents through the new implementation. After the automation fixtures were removed, the final full export retained the 3,876 valid sidecars and removed all obsolete test output:

```text
exported=0 unchanged=3876 failed=0
checked=3876 missing=0 stale=0 orphan=0 invalid=0
```

All 3,876 sidecars use USEM schema 2.3.0, semantic revision 19, and the `logic` profile. The project index contains 3,876 `ok` assets and no unsupported asset entry.

The 4,809 excluded packages are all World Partition generated External Actor/Object packages. They are represented by their owning World and recorded once in `coverage.exclusions` as `generatedExternalActor`; no redundant sidecar is written for them.

## Size

| Measurement | Before this iteration | UE Ring 0.10.0 | Change |
|---|---:|---:|---:|
| Complete `.uesem` tree | 360,913,410 bytes | 187,481,986 bytes | -48.05% |
| Asset semantic JSON bytes | 309,203,303 bytes | 137,942,396 bytes | -55.39% |
| Files in `.uesem` | 13,601 | 13,431 | -170 |

The final directory allocation is:

| Directory | Files | Bytes |
|---|---:|---:|
| `content` | 11,568 | 131,212,223 |
| `index` | 3 | 37,520,711 |
| `maps` | 60 | 11,410,907 |
| `graphs` | 1,796 | 5,270,711 |
| `cpp` | 2 | 1,300,142 |
| `reports` | 2 | 767,292 |

The index now records `semanticBytes`, omission counts, semantic-section byte totals, and per-class byte totals so future growth can be attributed instead of inferred from the directory size.

## Logic-profile policy

| Asset family | Preserved in the default profile | Intentionally omitted |
|---|---|---|
| Niagara | Asset role, exposed/interface properties, references, dependencies, persistent internal class inventory | Replaceable emitter/system implementation graph and editor/compiled caches |
| SoundWave and impulse response | Playback, routing, attenuation/interface settings, references, dependencies | PCM/compressed samples and impulse sample arrays |
| Control Rig | Blueprint/RigVM graph topology, nodes, pins, links, variables, hierarchy-facing settings, references | Compiled VM bytecode and editor caches |
| Anim Blueprint | Complete Blueprint and animation graph semantics, variables, interfaces, class/default data, animation settings | Derived compiled animation class artifacts |
| AnimSequence | Notifies, explicit sync markers, root-motion/additive/compression settings, sampled/source timing, curves, attribute interfaces, references | Per-bone pose samples and opaque custom attribute payload values |
| Montage | Sections, slots, blends, notifies, sync group/slot, explicit marker-sync data, referenced sequences, timing and curves | Derived/raw presentation data not required for montage behavior |
| Blend Space / Aim Offset | Axis settings, interpolation, notify mode, sample references, sample positions and rate settings | Raw pose samples, which remain owned by referenced sequences |

Sync markers are serialized through animation-specific code so the generic `*Auth*` privacy rule cannot redact `AuthoredSyncMarkers`. A structured scan of all final sidecars found zero redacted authored-sync fields. Source-data frame counts and playback-sampled frame counts use distinct field names to avoid presenting high-resolution source ticks as playback frames.

## Omission accounting

The final index contains 1,803 explicit omission records:

| Omission code | Count |
|---|---:|
| `replaceableAudioBulk` | 780 |
| `replaceableAnimationPoseSamples` | 582 |
| `opaqueAnimationAttributeValues` | 344 |
| `replaceablePresentationImplementation` | 74 |
| `derivedCompiledArtifact` | 23 |

Every omission records a JSON path, reason, recoverability impact, optional original count, and source digest. These are intentional boundaries, not silent data loss.

## Representative class totals

| Asset class | Assets | Semantic bytes |
|---|---:|---:|
| Material | 132 | 24,284,587 |
| AnimSequence | 582 | 22,637,583 |
| MetaSoundSource | 65 | 15,004,120 |
| Blueprint | 321 | 12,336,839 |
| World | 20 | 11,390,561 |
| ControlRigBlueprint | 3 | 3,733,018 |
| SoundWave | 767 | 2,631,410 |
| AnimBlueprint | 20 | 1,556,847 |
| NiagaraSystem | 34 | 582,349 |
| AnimMontage | 66 | 469,842 |
| NiagaraEmitter | 21 | 139,471 |
| AudioImpulseResponse | 13 | 33,793 |

Control Rig remains relatively large because its 94-graph Mannequin rig is behavior, not disposable presentation data. Niagara systems and impulse responses show the intended reduction because their bulky replaceable implementation/sample data is no longer duplicated in the logic profile.

## Reconstruction IR

Each sidecar contains reconstruction IR targeting semantic equivalence. It includes per-dimension confidence, evidence, ordered reconstruction steps, reconstructable features, and unrecoverable features. The IR does not claim byte-identical `.uasset` recovery or recovery of native C++ bodies, compiled bytecode, raw animation poses, audio samples, or omitted Niagara presentation internals.

The exported semantics are suitable for offline architecture analysis and for generating C++ implementation plans or partial semantic-equivalent code. They are not by themselves sufficient for lossless C++ or binary asset regeneration where the IR marks source information as unrecoverable.

## Verification

| Check | Result |
|---|---|
| Win64 `BuildPlugin` host, Development game, Shipping game | pass |
| Python contract and validator tests | 36/36 pass |
| UE automation tests | 11/11 pass |
| Final clean-project full export | 3,876 unchanged valid assets, 0 failed; 8 stale test outputs removed |
| UE semantic validation | 3,876 checked, 0 missing/stale/orphan/invalid |
| Python index/hash validation | pass |
| SQLite `quick_check` / user version / asset rows | `ok` / 3 / 3,876 |
| Redacted authored sync-marker fields | 0 |
| Export error log | absent |

Primary logs are stored in the Lyra project under `Saved/Logs`:

- `UERing-0.10.0-AllTests-Revision19.log`
- `UERing-0.10.0-FullExport-CleanProject.log`
- `UERing-0.10.0-Validation-CleanProject.log`

The canonical final output is `D:\GameDev\Unreal_Projects\LyraStarterGame\.uesem`. Full export cleanup and finalization remove stale semantic files; deployment mirrors the packaged plugin over the existing Lyra plugin directory instead of creating versioned project or build copies.
