# Lyra Scale Acceptance 0.14.0

Date: 2026-08-01

Engine: Unreal Engine 5.8.0

Project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

Plugin semantic revision: 25

## Scope

This iteration replaces generic owned-object Material exports with compact, typed Material
Logic semantics. It covers Material, Material Function, Material Instance, Function
Instance, Material Layer/Blend variants, and Material Parameter Collection behavior. It
also projects expression ownership and typed shader-flow connections into the unified
project graph and reconstruction IR.

## Export result

- Exported assets: 3,876
- Unchanged assets: 0
- Failed assets: 0
- Generated external actor/object exclusions: 4,809
- Asset semantic JSON bytes: 99,119,401
- Complete `.uesem` bytes: 417,810,783
- Complete `.uesem` files: 12,754
- Unified graph nodes: 53,012
- Unified graph edges: 115,777
- Project graph JSON bytes: 77,624,265
- SQLite bytes: 221,065,216

Compared with 0.13.0, asset semantic JSON is 24,352,219 bytes smaller (19.7%) and the
complete export is 14,162,856 bytes smaller (3.3%). The SQLite and graph artifacts remain
the dominant part of the complete output.

## Material coverage

| Asset class | Assets | Representation |
| --- | ---: | --- |
| Material | 132 | `material-expression-graph-v1` |
| MaterialFunction | 64 | `material-function-graph-v1` |
| MaterialInstanceConstant | 338 | `material-instance-v1` |
| MaterialParameterCollection | 1 | `material-parameter-collection-v1` |

- Material-related files: 535 / 12,298,047 bytes
- Compact Material semantics: 10,501,858 bytes
- Old 0.13 Material semantics: 35,651,368 bytes
- Semantic reduction: 25,149,510 bytes (70.5%)
- Material/function expression nodes: 8,381
- Typed Material connections: 9,717
- Dangling Material connections: 0

The accepted output preserves unconnected expression defaults, function preview defaults,
full unsigned GUID components, fixed property arrays, static component-mask parameters,
function-instance parent ownership, and duplicate expression GUID disambiguation. Material
root outputs are also present in generated Mermaid/DOT artifacts.

## Golden assets

- `M_Mannequin`: 244 nodes, 285 connections, masked/clear-coat settings, no dangling flow.
- `M_UI_Base_HealthBar`: 320 nodes, 348 connections, no dangling flow.
- `WorldAlignedTextureMip`: 40 nodes, seven interface inputs and three outputs.
- `MI_Manny_01`: parent, 51 scalar overrides, seven texture overrides, `UseLogo=false`,
  `D_occlusion=1.25`, and `PM_Character` are retained.
- `MI_MS_Basic_Metal`: instance parent chain, two local textures, and `PM_Concrete` are retained.
- `MPC_Checker`: distinct MPC representation with 13 scalar parameters.

The project-scale suite additionally verifies every Material and Material Function
connection endpoint and enforces a 110,000,000-byte upper bound for all asset semantics.
This replaces the obsolete minimum-size rule, which rewarded output bloat.

## Verification

- `BuildPlugin` Editor/Development/Shipping for Win64: passed.
- Python unit tests: 50 passed.
- UE Automation `UERing.`: 11 passed.
- Full Lyra export: 3,876 successful, 0 failed.
- Strict index validation with source hashes: passed.
- Strict unified project graph validation: passed.
- Lyra golden acceptance: passed.
- SQLite `quick_check`: `ok`.

Logs:

- `Saved/Logs/UERing-0.14.0-Revision25-AllTests-Final.log`
- `Saved/Logs/UERing-0.14.0-Revision25-MaterialTest.log`
- `Saved/Logs/UERing-0.14.0-Revision25-FullExport.log`

Only LyraStarterGame was deployed and exported. The Start project was not modified.
