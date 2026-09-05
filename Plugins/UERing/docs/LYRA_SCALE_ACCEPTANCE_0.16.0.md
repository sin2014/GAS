# Lyra Scale Acceptance 0.16.0

Date: 2026-08-02

Engine: Unreal Engine 5.8.0

Project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

Plugin semantic revision: 29

## Scope

This iteration adds the first executable Unreal Editor asset-construction backend to
Reconstruction IR 2.0. MaterialInstanceConstant sidecars now emit versioned create,
apply-overrides, and save operations. The offline generator consumes only the closed opcode
allowlist and structured JSON; generated assets are restricted to `/Game/UERingGenerated`.

## Export result

- Indexed assets: 3,876
- Failed assets: 0
- Skipped generated external actors: 4,809
- Asset semantic JSON bytes: 107,686,472
- Complete `.uesem` bytes: 426,234,910 (406.49 MiB)
- Complete `.uesem` files: 12,754
- Unified graph nodes: 53,012
- Unified graph edges: 115,777
- Project graph JSON bytes: 77,624,265
- SQLite bytes: 220,987,392
- Recorded semantic omissions: 2,534

The final revision-29 full export reported `exported=3876 unchanged=0 failed=0`. The 4,809
exclusions are all `generatedExternalActor`; they are counted but not loaded and do not
produce sidecars. No revision-28 files, generated golden assets, or automation-test assets
remain in the final export.

## Reconstruction readiness

| Readiness | Assets | Change from 0.15.0 |
| --- | ---: | ---: |
| `ready` | 362 | +338 |
| `partial` | 482 | 0 |
| `blocked` | 3,032 | -338 |

All 338 Lyra MaterialInstanceConstant assets are `ready`. Existing Blueprint-family coverage
remains 24 ready and 482 partial assets. The remaining dominant blockers are:

- `assetBuilderBackendUnavailable`: 3,032 assets
- `unsupportedGraphNodes`: 1,095 operations
- `classDefaultLoweringUnavailable`: 296 operations
- `componentConstructionUnavailable`: 57 operations
- `interfaceGenerationUnavailable`: 21 operations
- `timelineLoweringUnavailable`: 8 operations

## Material Instance backend

The backend preserves parent material, scalar/vector/texture overrides, parameter association
and index, Expression GUID, static switches, static component masks, all serialized base
property overrides, physical material, Subsurface Profile, and Nanite override. Unsupported
non-empty advanced categories are explicit IR blockers instead of optimistic `exact` claims.

The golden asset
`/Game/Characters/Heroes/Mannequin/Materials/Instances/Manny/MI_Manny_01` contains 51 scalar,
2 vector, 7 texture, 1 static-switch, and physical-material overrides. Strict generation
created an Editor plugin, compiled it, built and saved
`/Game/UERingGenerated/MI_Manny_01_Reconstructed`, re-exported the result, and obtained an
exact `semantics` tree match. The temporary plugin, asset, and sidecars were then removed.

## Verification

- Main BuildPlugin Editor/Development/Shipping for Win64: passed.
- Python unit tests: 69 passed.
- UE Automation `UERing`: 11 passed in a single process.
- Generated Material Instance Editor plugin build: passed.
- Generated asset creation/save automation test: passed.
- Material Instance semantic re-export comparison: exact match.
- Full Lyra export: 3,876 indexed assets, 0 failures.
- Strict index validation with semantic and source hashes: passed.
- Strict unified project graph validation: passed.
- Lyra golden acceptance: passed.
- SQLite `quick_check`: `ok`; `user_version=4`.

Logs:

- `Saved/Logs/UERing-0.16.0-Revision28-AllTests-Retry.log`
- `Saved/Logs/UERing-0.16.0-MI-Manny-SourceExport.log`
- `Saved/Logs/UERing-0.16.0-MI-Manny-Rebuild.log`
- `Saved/Logs/UERing-0.16.0-MI-Manny-Reexport.log`
- `Saved/Logs/UERing-0.16.0-Revision29-FullExport.log`

Only LyraStarterGame was deployed and exported. The Start project was not modified.
