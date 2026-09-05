# Lyra-scale acceptance report: UE Ring 0.11.0

Date: 2026-08-01

Target project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

The Start project was not installed, exported, or modified. This run used Unreal Engine 5.8.0 and the default `logic` export profile.

## Result

UE Ring 0.11.0 adds a centralized project semantic graph at `.uesem/index/project.uesem.graph.json` and mirrors it into SQLite `graph_nodes` and `graph_edges` tables. It connects asset dependencies, Blueprint graphs/nodes/pin flows, inheritance, interfaces, member symbols, delegates, domain roles, owned objects, and initial typed GAS/Lyra/GameFeature/DataRegistry/Enhanced Input relations.

The final full export completed with:

```text
exported=0 unchanged=3876 failed=0
index assets=3876, all status=ok
generated external actor exclusions=4809
```

The zero exported count means the immediately preceding revision-20 export had already rewritten every supported sidecar; the final pass verified all 3,876 files as unchanged and rebuilt the centralized graph and SQLite index.

## Unified graph

| Measurement | Count |
|---|---:|
| Graph nodes | 48,074 |
| Graph edges | 132,156 |
| Asset nodes | 3,876 |
| Blueprint node nodes | 16,881 |
| Graph nodes | 1,259 |
| Owned-object nodes | 22,349 |
| Symbol nodes | 3,240 |
| Dependency edges | 21,203 |
| Blueprint execution-flow edges | 5,454 |
| Blueprint data-flow edges | 13,216 |
| Asset-reference edges | 20,532 |
| Owned-object reference edges | 32,110 |

Every JSON edge endpoint resolves. External symbol IDs use canonical lowercase identity casing, while project asset references resolve through the canonical package spelling. This prevents Unreal's case-insensitive string maps from producing case-sensitive JSON dangling edges such as `Fire` versus `fire`.

## Domain coverage

| Domain | Assets |
|---|---:|
| GAS | 122 |
| Lyra Experience | 23 |
| GameFeature | 5 |
| Enhanced Input | 35 |
| StateTree | 1 |
| DataRegistry | 1 |

Domain detection now uses exact reflected type lineage rather than package-name substrings. This removes the known Lyra false positives while retaining the expected authored domain assets.

## Golden flows

The checked-in `acceptance/lyra.golden.json` suite verifies these representative relationships:

| Source | Relation | Target / qualifier |
|---|---|---|
| `B_LyraDefaultExperience` | `usesDefaultPawnData` | `SimplePawnData` |
| `HeroData_ShooterGame` | `usesAbilitySet` | `AbilitySet_ShooterHero` |
| `HeroData_ShooterGame` | `usesInputConfig` | `InputData_Hero` |
| `AbilitySet_ShooterHero` | `grantsAbility` | `GA_Hero_Dash`, `InputTag.Ability.Dash` |
| ShooterCore input action object | `addsInputMapping` | `IMC_ShooterGame` |
| ShooterCore registry action object | `addsDataRegistry` | `AccoladeDataRegistry` |

The acceptance runner also enforces graph and asset-count floors, domain-count floors, absence of stale `/Game/UERingTests/` packages, SQLite/JSON row-count equality, SQLite integrity, and zero dangling SQLite endpoints.

## Size

| Measurement | UE Ring 0.11.0 |
|---|---:|
| Complete `.uesem` tree | 465,295,411 bytes |
| Files in `.uesem` | 13,432 |
| Asset semantic JSON bytes | 137,901,899 bytes |
| Project graph JSON | 80,947,349 bytes |
| SQLite index | 225,755,136 bytes |

The complete output grew from 0.10.0 because 0.11.0 deliberately adds the project-wide graph twice: portable JSON for offline tools and indexed SQLite for fast queries. Per-asset semantic JSON became slightly smaller. A later P3 iteration must make graph/index rebuilding incremental and may add an explicit output mode that omits one representation when storage is more important than portability or query latency.

## Verification

| Check | Result |
|---|---|
| Win64 BuildPlugin host, Development game, Shipping game | pass |
| Python contract, validator, query, and acceptance tests | 45/45 pass |
| UE automation suite before final casing fix | 11/11 pass |
| Focused symbol-casing UE regression after fix | pass |
| Final Lyra full export | 3,876 unchanged, 0 failed |
| Python project graph validation | pass |
| Python index/hash validation | pass |
| Lyra golden flow acceptance | pass |
| Offline graph neighbor/search/path queries | pass |
| Official MCP `ue_ring_query_graph` integration test | pass |
| SQLite `quick_check` / `user_version` | `ok` / 4 |
| SQLite assets / graph nodes / graph edges | 3,876 / 48,074 / 132,156 |
| SQLite dangling graph endpoints | 0 |

Primary logs are under `D:\GameDev\Unreal_Projects\LyraStarterGame\Saved\Logs`:

- `UERing-0.11.0-UnifiedGraph-Tests-Final.log`
- `UERing-0.11.0-SymbolCase-Regression-Retry.log`
- `UERing-0.11.0-FullExport-Final.log`
- `UERing-0.11.0-GraphMCP-Final.log`

One focused-test launch crashed before automation discovery inside UE 5.8 Niagara editor startup while loading the default map. Re-running with `-NoLoadStartupPackages` completed successfully. The final export also used that isolation flag and completed without a plugin export failure.
