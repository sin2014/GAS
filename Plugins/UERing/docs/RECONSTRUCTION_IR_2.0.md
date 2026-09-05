# Reconstruction IR 2.0

Reconstruction IR 2.0 is a deterministic execution contract embedded in each USEM asset
sidecar. It describes what a backend can rebuild, what evidence each operation came from,
and what prevents semantic-equivalent reconstruction. It does not claim binary-identical
`.uasset` output, original Blueprint bytecode, native function bodies, or engine-private state.

## Contract

The `reconstruction` object contains:

- `source`: schema/revision fingerprints and resolvable document pointers.
- `targets`: backend, artifact kind, write policy, readiness, and blocker references.
- `symbols`: Unreal paths, resolved C++ names, modules, and canonical headers.
- `operations`: stable typed operations ordered by ID with an acyclic `dependsOn` graph.
- `losses`: explicit reason codes and source pointers for unavailable information.
- `coverage`: exact, inferred, and unsupported operation counts derived from operations.
- `execution`: executable/blocked counts and the derived `fullyExecutable` flag.
- `verification`: required UBT, reflection, and semantic re-export checks.

Operations use `exact`, `inferred`, or `unsupported` fidelity. An unsupported operation must
be blocked and carry a reason code. A target is `ready` only when all of its operations are
executable and exact. Mixed executable and blocked operations produce `partial`; a target
with no executable operation is `blocked`.

The contract rejects arbitrary source-code and shell fields. Backends consume only versioned
opcode allowlists and structured operands.

## C++ backends

`tools/usem_reconstruct.py` currently executes:

- `cpp.class.declare`
- `cpp.property.declare`
- `cpp.graph.assertEmpty`

The Unreal Editor asset backend executes:

- `editor.materialInstance.create`
- `editor.materialInstance.applyOverrides`
- `editor.dataAsset.create`
- `editor.dataAsset.createOwnedObjects`
- `editor.dataAsset.applyOwnedObjectProperties`
- `editor.dataAsset.applyProperties`
- `editor.stateTree.compile`
- `editor.asset.save`

It emits a standalone Unreal plugin containing a module, generated class, deterministic
constructor defaults, and a reflection automation test. `--strict` refuses any blocked or
unknown operation. Non-strict generation writes a compile-time `#error` for every blocker,
so incomplete behavior cannot silently ship.

The output root must be empty or contain a valid `.uesem-generated.json` from the previous
run. Re-generation may replace only files owned by that manifest. Identifiers, include paths,
and C++ type strings are validated before any file is written.

Material Instance generation additionally requires `--asset-package` below
`/Game/UERingGenerated/...`. It emits an Editor module whose automation test creates and
saves a `UMaterialInstanceConstant`. Parent references, uniform/static parameters,
Expression GUIDs, base-property overrides, physical material, Subsurface Profile, and Nanite
override are restored from structured operands and semantic fields. Unsupported non-empty
override categories block strict generation. `--verify-reexport` compares the source and
rebuilt `semantics` trees and reports the first differing JSON Pointer.

DataAsset generation uses the same restricted package root and additionally requires the target
object name to equal the source object name so `PrimaryAssetId` remains stable. The generated
Editor builder loads the native asset class by Unreal path, creates the ordered owned-UObject
graph, restores object references, and applies structured values through reflected `FProperty`
types. The exact property backend includes optional values, empty delegates, dynamic Property
Bags, and `FInstancedStruct` values in addition to ordinary scalars, references, containers, and
structs. Redacted or omitted values, bound delegates, unsupported property classes, and
non-recoverable localized display text block strict generation instead of falling back to
source-asset copying.

StateTree assets use `state-tree-editor-compile-v1`. `EditorData` and its owned objects are the
authored source; root `Schema`, states, transitions, nodes, bindings, parameters, instance data,
and compiled lookup tables are explicitly declared derived. After restoring EditorData the
generated backend calls `UStateTreeEditingSubsystem::CompileStateTree`. Semantic re-export
verification compares the full authored projection and ignores only root properties named by
`derivedRootProperties`; any EditorData or owned-object difference still fails at its first JSON
Pointer.

## Required verification

For a `ready` native-class target:

1. Generate twice and compare every output byte.
2. Compile Editor, Development Game, and Shipping Game targets with UBT.
3. Load the generated module and run its reflection automation test.
4. Compare parent class, properties, types, flags, and authored defaults.
5. Re-export any generated asset target and run a normalized semantic diff.

Complex Blueprint graphs, latent/async behavior, components, interfaces, timelines,
AnimGraph, Control Rig, base Material graph builders, Material Function builders, bound
delegates, and advanced Material Instance override categories remain blocked until their
versioned opcodes and round-trip tests are implemented.
