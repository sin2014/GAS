# Lyra Scale Acceptance 0.17.1

Date: 2026-08-02

Engine: Unreal Engine 5.8.0

Project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

Plugin semantic revision: 31

## Scope

This iteration adds an executable Unreal Editor C++ backend for DataAsset reconstruction.
DataAsset sidecars now emit versioned create, reflection-based property application, and save
operations. The generated backend consumes only structured JSON and a closed opcode allowlist,
requires the original object name to preserve the Primary Asset Id, and writes only below
`/Game/UERingGenerated`.

The property backend supports enums, exact JSON numbers, bools, `FString`, `FUtf8String`,
`FAnsiString`, `FName`, localized `FText`, hard and soft object references, fixed arrays,
arrays, sets, maps, and recursively nested structs. Owned UObject graphs, redacted values,
inexact integers, and unsupported property types remain explicit blockers.

`UPrimaryDataAsset::AssetBundleData` is intentionally omitted. Unreal resets and regenerates
this field in `PreSave` from authored AssetBundles metadata. `UPrimaryAssetLabel` additionally
derives its Directory bundle from the asset package location. Exporting this save-time cache
made three labels larger, created thousands of duplicate blockers, and could not be reproduced
at a generated package path. The authored label rules, scan switches, collections, and explicit
asset lists remain in the semantic document.

## Export result

- Indexed assets: 3,876
- Failed assets: 0
- Skipped generated external actors: 4,809
- Asset semantic JSON bytes: 104,498,742
- Complete `.uesem` bytes: 423,072,655 (403.47 MiB)
- Complete `.uesem` files: 12,754
- Unified graph nodes: 53,012
- Unified graph edges: 115,777
- Project graph JSON bytes: 77,624,257
- SQLite bytes: 221,011,968
- Recorded semantic omissions: 2,534

The final revision-31 full export reported `exported=3875 unchanged=1 failed=0`. The one
unchanged asset was the final-logic `TopDownArena_Label` golden source exported immediately
before the full run. All 3,876 final sidecars use schema 2.6.0 and semantic revision 31. The
4,809 exclusions are all `generatedExternalActor`; they are counted without producing
sidecars. No generated golden asset, generated plugin, automation-test asset, redacted value,
or stale-revision sidecar remains in Lyra.

## Reconstruction readiness

| Readiness | Assets | Change from revision 30 |
| --- | ---: | ---: |
| `ready` | 437 | +3 |
| `partial` | 520 | -3 |
| `blocked` | 2,919 | 0 |

The three moved assets are all `PrimaryAssetLabel` instances. DataAsset readiness is now 75
ready and 38 partial. Every remaining DataAsset loss is
`dataAssetOwnedObjectGraphUnavailable`: 38 assets and 103 source pointers. There are no
`unsupportedDataAssetPropertyType`, `invalidDataAssetPropertyValue`, or DataAsset redaction
losses in the Lyra export.

## Golden reconstruction

Three representative authored DataAssets completed exact USEM -> IR -> generated Editor C++
-> `.uasset` -> re-export semantic comparisons:

- `/Game/Characters/Heroes/SimplePawnData/SimplePawnData`
- `/Game/Environments/Gameplay/AS_InstantHeal`
- `/ShooterExplorer/Input/Mappings/IMC_InventoryTest`

The Input Mapping Context golden also verifies stable localization package namespace handling.
`/TopDownArena/TopDownArena_Label` then exercised the PrimaryAssetLabel boundary. Before the
derived-cache fix its sidecar was 62,097 bytes and its re-export diverged because the generated
asset occupied a different directory. After omitting `AssetBundleData`, the sidecar is 9,372
bytes, contains only seven authored properties, builds successfully, and obtains an exact
semantic re-export match.

`/Game/Input/Actions/IA_Move` verifies the negative boundary: its instanced modifiers/triggers
remain partial with `dataAssetOwnedObjectGraphUnavailable`, rather than being misreported as
exactly reconstructable.

## Verification

- Main BuildPlugin Editor/Development/Shipping for Win64: passed.
- Python unit tests: 72 passed.
- UE Automation `UERing.`: 11 passed in one process.
- Generated DataAsset Editor plugins: compiled successfully.
- Four DataAsset semantic re-export comparisons: exact match.
- Full Lyra export: 3,876 indexed assets, 0 failures.
- Strict index validation with semantic and source hashes: passed.
- Strict unified project graph validation: passed.
- Lyra golden acceptance: passed.
- SQLite `quick_check`: `ok`; `user_version=4`.

Logs:

- `Saved/Logs/UERing-0.17.1-Revision31-AllTests.log`
- `Saved/Logs/UERing-0.17.1-TopDownArenaLabel-Rebuild.log`
- `Saved/Logs/UERing-0.17.1-TopDownArenaLabel-Reexport-DerivedOmitted.log`
- `Saved/Logs/UERing-0.17.1-Revision31-FullExport.log`

Only LyraStarterGame was deployed and exported. The Start project was not modified.
