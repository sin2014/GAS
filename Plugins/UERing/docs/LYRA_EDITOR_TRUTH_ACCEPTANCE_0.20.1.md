# Lyra Editor-Truth Acceptance 0.20.1

## Result

UERing 0.20.1 (USEM schema 2.9.0, semantic revision 44) was audited against the
loaded LyraStarterGame project in Unreal Editor 5.8 through the official Unreal
MCP. The comparison uses the editor as an independent source of truth instead of
only validating one exporter artifact against another.

The final Lyra export contains 3,868 asset sidecars. Every sidecar parses at
revision 44, the Python and Unreal validators report no missing, stale, orphaned,
or invalid sidecars, and the SQLite graph has no dangling endpoint or contributor.
The editor-truth comparisons report no confirmed semantic differences in the
covered surfaces.

Only `D:/GameDev/Unreal_Projects/LyraStarterGame` was deployed to and exported.
No Start project content was changed.

## Gaps Found And Fixed

The first independent editor pass exposed gaps that the previous self-consistency
and deterministic-output tests could not detect:

- Blueprint custom events did not preserve their authoritative event name, RPC
  and function flags, call-in-editor/deprecation metadata, or override state.
- Split, orphaned, and persistent pin identity was incomplete, and nested graphs
  did not identify their parent graph.
- Inherited instanced class defaults were intentionally skipped, so a changed
  inherited UObject graph could disappear from class-default semantics.
- World Partition external actor packages were excluded as generated packages,
  but the owning World sidecar only contained loaded persistent-level actors. The
  owner now contains descriptors plus force-loaded Actor and Component semantics,
  explicit package dependencies, external object packages, and load accounting.
- Control Rig used a Blueprint graph mirror without an authoritative RigVM model
  or authored hierarchy. Both are now exported, including hierarchy parents,
  transforms, control settings, RigVM pins, links, and node properties.
- Widget Blueprints exported hierarchy and bindings but omitted authored
  `UWidgetAnimation` objects. Lyra contained 62 animations in 40 Widget Blueprints.
  They now retain their reachable MovieScene tracks, sections, channels, and keys
  as deterministic owned-object graphs (860 objects in this snapshot).
- Object redirectors could leave obsolete sidecars after a full export. They are
  now an explicit exclusion and full export removes stale redirector artifacts.
- A debounced save could race an asset deletion in the lifecycle manager. A
  missing package now invalidates the index instead of reporting an export error.

## Editor-Truth Matrix

| Surface | Compared | Final result |
| --- | ---: | --- |
| Asset dependencies and referencers | 3,868 assets | Revision 43 full pass: 0 fatal calls and 0 edge differences; revision 44 recheck: 675 assets, 0 fatal calls and 0 edge differences |
| Blueprint, Widget Blueprint, Anim Blueprint | 503 assets | 0 fatal calls; 0 confirmed semantic differences |
| Control Rig | 3 assets | 0 fatal calls; graph, node, and hierarchy identities agree |
| Material and Material Function graphs | 196 assets | 0 fatal calls; node counts agree |
| StateTree | 1 asset | 0 fatal calls; 0 semantic differences |
| Behavior Tree | 2 assets | 0 fatal calls; 0 semantic differences |
| Niagara System | 34 assets | 0 fatal calls; 0 semantic differences |
| Data Registry | 1 asset | 0 fatal calls; 0 semantic differences |
| World Partition | 4 maps / 4,765 descriptors | 4,765 loaded; 0 load failures; dependency closure complete |

Blueprint-family export totals are 1,246 graphs, 16,881 nodes, 66,792 pins,
18,670 links, 144 named custom events with function flags, 11,165 child-pin
records, 4,082 parent pins, and 231 reachable owned class-default objects. There
are no duplicate graph paths.

The three Control Rigs contain 108 authoritative RigVM graphs, 2,332 nodes,
20,880 pins, 2,739 links, and 974 hierarchy elements. The four World Partition
maps contain 4,765 descriptors, all of which loaded successfully for full actor
and component serialization.

## Export And Index Health

The revision-44 full export completed with `exported=3868`, `failed=0`. The final
`.uesem` tree is 505,639,734 bytes (482.22 MiB):

| Area | Files | Bytes | MiB |
| --- | ---: | ---: | ---: |
| Indexes | 4 | 345,213,449 | 329.22 |
| Content sidecars | 11,544 | 117,791,498 | 112.33 |
| Maps | 60 | 35,857,255 | 34.20 |
| Graph fragments | 1,118 | 4,663,577 | 4.45 |
| C++ semantics | 2 | 1,346,663 | 1.28 |
| Reports | 2 | 767,292 | 0.73 |

The 3,868 sidecar JSON files occupy 149,060,180 bytes. SQLite reports
`integrity_check=ok`, user version 6, 3,868 assets, 59,649 graph nodes, 123,239
graph edges, 0 dangling edges, and 0 contributorless edges.

## Official MCP Boundaries

The official MCP is broad enough to independently verify the important authored
logic and relationship surfaces, but it is not a raw Unreal object-memory oracle.
The audit records these cases as limitations, never as passes:

- Blueprint node listing filters comment nodes and some animation state-machine
  nodes. Those nodes are present in USEM, but the official node API cannot return
  them for a direct property comparison.
- The official object tools do not expose `UEdGraph::Nodes`; graph membership has
  to be compared through graph and node toolsets.
- Object lookup redirects a Blueprint asset to its generated CDO in several
  calls. `UWidgetBlueprint::Animations` therefore cannot be read from that API;
  animation existence is additionally protected by the authored automation
  fixture and by the editor export implementation itself.
- A small number of CDOs or individual node packages are unreadable through the
  official property endpoint. The audit retains the exact package and exception.
- Control Rig official bulk APIs do not expose every serialized property for all
  pin/link types. Identity, topology, graph counts, node counts, hierarchy names,
  and hierarchy parents are independently compared; richer property preservation
  is verified by exporter automation and schema validation.
- Runtime-spawned objects, transient compiler caches, cooked bytecode, and
  platform-derived data are intentionally outside authored project semantics.

These limitations mean the result is strong evidence for the covered authored
semantics, not a claim that every private/transient Unreal field was observable.

The complete dependency/referencer pass was captured after the revision-43 World
Partition dependency fix. Revision 44 changes only Widget animation semantics and
the lifecycle delete/save race, so it does not change dependency construction. A
revision-44 recheck independently queried 675 assets and again found no edge
differences. The report records both passes instead of presenting the recheck as
a second complete pass.

## Verification Evidence

- Bundled Python contract, validation, and reconstruction tests: 79 passed.
- Unreal Automation `UERing` suite: 11 passed, 0 failed.
- Full export: 3,868 exported, 0 failed.
- Immediate no-change full export: 0 exported, 3,868 unchanged, 0 failed.
- Python validation with source hashes: OK.
- Unreal validation: 3,868 checked; missing 0, stale 0, orphan 0, invalid 0.
- Full editor-truth results and audit scripts:
  `D:/GameDev/Unreal_Projects/LyraStarterGame/Saved/UERingAudit`.
- Unreal Automation log:
  `D:/GameDev/Unreal_Projects/LyraStarterGame/Saved/Logs/UERing-0.20.1-Revision44-AllTests-Final.log`.
- Full export log:
  `D:/GameDev/Unreal_Projects/LyraStarterGame/Saved/Logs/UERing-0.20.1-Revision44-FullExport.log`.
- No-change export log:
  `D:/GameDev/Unreal_Projects/LyraStarterGame/Saved/Logs/UERing-0.20.1-Revision44-NoChange.log`.
- Unreal validation log:
  `D:/GameDev/Unreal_Projects/LyraStarterGame/Saved/Logs/UERing-0.20.1-Revision44-PostNoChangeValidate.log`.

## Why Earlier Tests Passed

Earlier acceptance was real but narrower. It proved schema validity,
determinism, exporter-internal invariants, index closure, incremental/full
equivalence, selected golden assets, and reconstruction contracts. Many fixtures
were generated from the same assumptions as the exporter, so an omitted domain
could be consistently absent on both sides. In particular, there was no authored
Widget animation fixture, World Partition generated-package exclusion was assumed
to imply owner-side consolidation, generic Blueprint topology checks did not
assert CustomEvent RPC and split-pin details, inherited instanced defaults were
explicitly excluded by the old contract, and Control Rig was assessed through its
K2 mirror.

Revision 44 adds editor-truth acceptance as a separate layer. Future releases
should retain both layers: fast deterministic regression tests for every change,
and a complete Lyra editor/MCP comparison before declaring fidelity work complete.
