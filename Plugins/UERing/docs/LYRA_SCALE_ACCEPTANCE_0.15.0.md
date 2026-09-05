# Lyra Scale Acceptance 0.15.0

Date: 2026-08-02

Engine: Unreal Engine 5.8.0

Project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

Plugin semantic revision: 27

## Scope

This iteration introduces executable Reconstruction IR 2.0, a closed JSON Schema and
strict structural validator, and a deterministic offline Unreal C++ plugin generator. The
generator supports native class declarations, Blueprint properties, authored defaults, and
empty-graph assertions. Unsupported behavior remains explicit and blocks strict generation;
the backend never embeds raw C++ or shell fragments from semantic input.

## Export result

- Indexed assets: 3,876
- Failed assets: 0
- Skipped generated external actors: 4,809
- Asset semantic JSON bytes: 107,198,482
- Complete `.uesem` bytes: 425,730,098 (406.01 MiB)
- Complete `.uesem` files: 12,754
- Unified graph nodes: 53,012
- Unified graph edges: 115,777
- Project graph JSON bytes: 77,624,265
- SQLite bytes: 220,971,008
- Recorded semantic omissions: 2,534

The 4,809 exclusions are all `generatedExternalActor`. They are counted in coverage but are
not loaded or exported and do not produce sidecars. The final incremental full export
reported `exported=0 unchanged=3876 failed=0` after removing two abandoned automation-test
assets; the preceding revision-27 export rebuilt all 3,878 then-present assets.

## Reconstruction readiness

| Readiness | Assets | Meaning |
| --- | ---: | --- |
| `ready` | 24 | Every emitted operation is executable by the current C++ backend. |
| `partial` | 482 | Class/property structure is available, but at least one behavior/default/component operation is blocked. |
| `blocked` | 3,370 | Semantic analysis is available, but no asset-construction backend exists for the asset class. |

Blueprint-family coverage consists of 24 ready assets and 482 partial assets: 321 regular
Blueprints, 150 Widget Blueprints, 20 Animation Blueprints, and several smaller Blueprint
classes. The dominant reconstruction blockers are:

- `assetBuilderBackendUnavailable`: 3,370 assets
- `unsupportedGraphNodes`: 1,095 operations
- `classDefaultLoweringUnavailable`: 296 operations
- `componentConstructionUnavailable`: 57 operations
- `interfaceGenerationUnavailable`: 21 operations
- `timelineLoweringUnavailable`: 8 operations

These blockers are reconstruction limitations, not semantic-export failures. Domain analysis,
querying, project-graph traversal, GAS/Lyra projections, Material logic, MetaSound, Niagara,
StateTree, DataRegistry, and Enhanced Input semantics remain available offline.

## Executable golden asset

`/ShooterExplorer/UserInterface/ItemAcquiredToastEntry` exports five exact operations:

- one native class declaration derived from `UObject` using `UObject/Object.h`
- three property declarations (`double`, `UTexture2D*`, and `FText`)
- one empty Event Graph assertion

Strict generation produced a standalone `UERingGolden` plugin. It compiled for Editor,
Development Game, and Shipping Game. Its UE reflection test verified the native parent,
property types, `DisplayDuration=1`, the null texture reference, and the empty text default.

## Verification

- Main `BuildPlugin` Editor/Development/Shipping for Win64: passed.
- Python unit tests: 65 passed.
- UE Automation `UERing`: 11 passed.
- Validation cleanup regression test: passed and left no `Content/UERingTests` directory.
- Full Lyra export: 3,876 indexed assets, 0 failures.
- Strict index validation with semantic and source hashes: passed.
- Strict unified project graph validation: passed.
- Lyra golden acceptance: passed.
- SQLite `quick_check`: `ok`; `user_version=4`.
- Generated plugin Editor/Development/Shipping builds: passed.
- Generated plugin reflection/default comparison: passed.

Logs:

- `Saved/Logs/UERing-0.15.0-Revision27-AllTests.log`
- `Saved/Logs/UERing-0.15.0-Revision27-ValidationCleanup.log`
- `Saved/Logs/UERing-0.15.0-Revision27-FullExport-Clean.log`
- `Saved/Logs/UERing-0.15.0-GeneratedReflection-Final-2.log`

Only LyraStarterGame was deployed and exported. The Start project was not modified.
