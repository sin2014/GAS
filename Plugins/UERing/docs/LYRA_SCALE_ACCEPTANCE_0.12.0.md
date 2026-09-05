# Lyra-scale acceptance report: UE Ring 0.12.0

Date: 2026-08-01

Target project: `D:\GameDev\Unreal_Projects\LyraStarterGame`

The Start project was not installed, exported, or modified. This run used Unreal Engine 5.8.0 and the default `logic` export profile.

## Result

UE Ring 0.12.0 replaces reflection fallback for all 65 MetaSound Source and 24 MetaSound Patch assets with a dedicated `metasound-frontend-graph-v1` representation. Semantic revision 21 forced all supported Lyra assets through the current implementation:

```text
first pass: exported=3876 unchanged=0 failed=0
final pass: exported=0 unchanged=3876 failed=0
```

The final index contains 3,876 `ok` assets and no unsupported entry. The 4,809 generated World Partition external actor packages remain centralized exclusions and do not produce sidecars.

## MetaSound semantics

The specialized documents preserve:

- root and subgraph pages;
- stable node, class, page, vertex, and variable identities;
- typed input, output, and environment vertices;
- directed node/vertex edges;
- node input literal defaults;
- dependency class names, types, and interface signatures;
- Source playback, loading, routing, attenuation, concurrency, volume, and pitch settings.

They omit duplicated editor graph UObjects, frontend display/style metadata, registry caches, import metadata, and repeated raw reflection wrappers. None of the 89 assets contains `fallbackReflectionOnly` or the old top-level `properties` dump.

| MetaSound measurement | Count |
|---|---:|
| Assets | 89 |
| Pages | 89 |
| Nodes | 2,622 |
| Edges | 3,127 |
| Variables | 70 |
| Input literals | 1,804 |
| Dependency signatures | 1,082 |
| Dangling page edge endpoints | 0 |

## Size

| Measurement | 0.11.0 | 0.12.0 | Change |
|---|---:|---:|---:|
| Complete `.uesem` tree | 465,295,411 | 447,711,086 | -17,584,325 bytes (-3.78%) |
| Asset semantic JSON | 137,901,899 | 123,350,102 | -14,551,797 bytes (-10.55%) |
| MetaSound Source JSON | 15,004,120 | 2,897,672 | -12,106,448 bytes (-80.69%) |
| MetaSound Patch JSON | 2,984,085 | 535,396 | -2,448,689 bytes (-82.06%) |
| Project graph JSON | 80,947,349 | 80,170,263 | -777,086 bytes |
| SQLite index | 225,755,136 | 223,535,104 | -2,220,032 bytes |

`mx_System` fell from 1,325,427 bytes to 272,087 bytes while retaining 220 nodes, 322 edges, 59 dependency signatures, 32 inputs, and 2 outputs.

## Project graph correction

The unified graph parser previously retained an `id` after an attempted `id + class` owned-object match failed. Compact MetaSound node and vertex IDs exposed that bug by creating approximately 9,600 false object nodes. Owned-object creation now requires both fields. The final graph contains 47,199 nodes and 131,191 edges, with 21,474 real/local object targets and zero dangling endpoints.

## Verification

| Check | Result |
|---|---|
| Win64 BuildPlugin host, Development game, Shipping game | pass |
| Python contract, validator, query, and acceptance tests | 46/46 pass |
| UE automation tests | 11/11 pass |
| Revision-21 full Lyra export | 3,876 exported, 0 failed |
| Final canonical full export | 3,876 unchanged, 0 failed |
| Python asset-index/hash validation | pass |
| Python unified-graph validation | pass |
| Lyra golden flow and specialized semantic acceptance | pass |
| MetaSound page edge endpoint audit | 0 dangling |
| Test assets remaining in Content or `.uesem` | 0 |

Primary logs are under `D:\GameDev\Unreal_Projects\LyraStarterGame\Saved\Logs`:

- `UERing-0.12.0-MetaSound-FullExport.log`
- `UERing-0.12.0-AllTests.log`
- `UERing-0.12.0-FinalExport.log`
