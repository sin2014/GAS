import json
import tempfile
import unittest
from pathlib import Path

from tools.usem_reconstruct import ReconstructionError, generate_cpp_plugin, verify_semantic_reexport


def fixture(*, blocked: bool = False, cpp_type: str = "double") -> dict:
    graph_status = "blocked" if blocked else "executable"
    graph_fidelity = "unsupported" if blocked else "exact"
    graph_opcode = "cpp.graph.translate" if blocked else "cpp.graph.assertEmpty"
    losses = []
    blockers = []
    if blocked:
        losses.append(
            {
                "id": "loss:graph",
                "reasonCode": "unsupportedGraphNodes",
                "impact": "blocksReconstruction",
                "sourcePointers": ["/semantics/graphs/0"],
                "recoverableFromSourceAsset": True,
            }
        )
        blockers.append("loss:graph")
    return {
        "schema": "com.ue-ring.usem.asset",
        "schemaVersion": "2.9.0",
        "semanticRevision": 44,
        "exporter": "Blueprint",
        "profile": "logic",
        "inputFingerprint": "sha256:" + "1" * 64,
        "asset": {
            "packageName": "/Game/Test/BP_Reconstruct",
            "objectPath": "/Game/Test/BP_Reconstruct.BP_Reconstruct",
            "assetClass": "Blueprint",
            "sourceFile": "Content/Test/BP_Reconstruct.uasset",
            "sourceHash": "sha256:" + "2" * 64,
        },
        "semantics": {
            "kind": "Blueprint",
            "parentClass": "/Script/CoreUObject.Object",
            "graphs": [{"name": "Empty", "graphPath": "Empty", "nodes": []}],
        },
        "reconstruction": {
            "irVersion": "2.0.0",
            "contract": "com.ue-ring.reconstruction",
            "assetKind": "Blueprint",
            "profile": "logic",
            "source": {
                "schemaVersion": "2.9.0",
                "semanticRevision": 44,
                "inputFingerprint": "sha256:" + "1" * 64,
                "sourceHash": "sha256:" + "2" * 64,
                "assetPointer": "/asset",
                "semanticsPointer": "/semantics",
            },
            "targets": [
                {
                    "id": "target:cpp",
                    "target": "nativeClassCpp",
                    "backend": "ueCpp",
                    "backendVersion": 1,
                    "fidelity": "semanticEquivalent",
                    "status": "partial" if blocked else "ready",
                    "writePolicy": "replaceGenerated",
                    "blockerRefs": blockers,
                }
            ],
            "symbols": [
                {
                    "id": "symbol:asset",
                    "kind": "asset",
                    "unrealPath": "/Game/Test/BP_Reconstruct.BP_Reconstruct",
                    "resolution": "exact",
                    "sourcePointer": "/asset/objectPath",
                },
                {
                    "id": "symbol:generated",
                    "kind": "generatedClass",
                    "unrealPath": "/Game/Test/BP_Reconstruct.BP_Reconstruct_C",
                    "cppName": "UBP_Reconstruct",
                    "module": "UERingGenerated",
                    "header": "BP_Reconstruct.h",
                    "resolution": "exact",
                    "sourcePointer": "/asset/objectPath",
                },
                {
                    "id": "symbol:parent",
                    "kind": "nativeClass",
                    "unrealPath": "/Script/CoreUObject.Object",
                    "cppName": "UObject",
                    "module": "CoreUObject",
                    "header": "UObject/Object.h",
                    "resolution": "exact",
                    "sourcePointer": "/semantics/parentClass",
                },
            ],
            "operations": [
                {
                    "id": "op:class",
                    "opcode": "cpp.class.declare",
                    "opcodeVersion": 1,
                    "phase": "declare",
                    "targetId": "target:cpp",
                    "dependsOn": [],
                    "operands": {
                        "symbolId": "symbol:generated",
                        "name": "BP_Reconstruct",
                        "cppName": "UBP_Reconstruct",
                        "parentSymbolId": "symbol:parent",
                        "module": "UERingGenerated",
                        "header": "BP_Reconstruct.h",
                    },
                    "results": [],
                    "sourcePointers": ["/asset", "/semantics/parentClass"],
                    "preconditions": [],
                    "postconditions": [],
                    "criticality": "structure",
                    "status": "executable",
                    "fidelity": {"status": "exact", "rule": "blueprint.class.v1", "confidence": 1},
                    "failurePolicy": "abort",
                    "idempotencyKey": "fixture:class",
                },
                {
                    "id": "op:property:Duration",
                    "opcode": "cpp.property.declare",
                    "opcodeVersion": 1,
                    "phase": "declare",
                    "targetId": "target:cpp",
                    "dependsOn": ["op:class"],
                    "operands": {
                        "ownerSymbolId": "symbol:generated",
                        "name": "Duration",
                        "cppType": cpp_type,
                        "arrayDim": 1,
                        "flags": ["BlueprintVisible", "InstanceEditable"],
                        "defaultValue": 1,
                    },
                    "results": [],
                    "sourcePointers": ["/semantics/variables/0"],
                    "preconditions": [],
                    "postconditions": [],
                    "criticality": "structure",
                    "status": "executable",
                    "fidelity": {"status": "exact", "rule": "blueprint.property.v1", "confidence": 1},
                    "failurePolicy": "abort",
                    "idempotencyKey": "fixture:property",
                },
                {
                    "id": "op:graph:0:Empty",
                    "opcode": graph_opcode,
                    "opcodeVersion": 1,
                    "phase": "translate",
                    "targetId": "target:cpp",
                    "dependsOn": ["op:class"],
                    "operands": {"name": "Empty", "graphPath": "Empty", "nodeCount": 1 if blocked else 0, "linkCount": 0},
                    "results": [],
                    "sourcePointers": ["/semantics/graphs/0"],
                    "preconditions": [],
                    "postconditions": [],
                    "criticality": "behavior",
                    "status": graph_status,
                    "fidelity": {
                        "status": graph_fidelity,
                        "rule": "blueprint.graph-lowering.v1" if blocked else "blueprint.empty-graph.v1",
                        "confidence": 0 if blocked else 1,
                        **({"reasonCode": "unsupportedGraphNodes"} if blocked else {}),
                    },
                    "failurePolicy": "abort",
                    "idempotencyKey": "fixture:graph",
                },
            ],
            "losses": losses,
            "coverage": {
                "readiness": "partial" if blocked else "ready",
                "totalOperationCount": 3,
                "exactOperationCount": 2 if blocked else 3,
                "inferredOperationCount": 0,
                "unsupportedOperationCount": 1 if blocked else 0,
                "exactRatio": 2 / 3 if blocked else 1,
            },
            "execution": {
                "fullyExecutable": not blocked,
                "operationCount": 3,
                "executableOperationCount": 2 if blocked else 3,
                "blockedOperationCount": 1 if blocked else 0,
            },
            "verification": [
                {"id": "verify:compile", "kind": "ubtCompile", "targetId": "target:cpp", "required": True}
            ],
        },
    }


def material_instance_fixture() -> dict:
    document = fixture()
    document["exporter"] = "MaterialLogic"
    document["asset"]["assetClass"] = "MaterialInstanceConstant"
    document["semantics"] = {
        "kind": "MaterialLogic",
        "role": "instance",
        "representation": "material-instance-v1",
        "parent": {"objectPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"},
        "overrides": {
            "BasePropertyOverrides": {
                "bOverride_BlendMode": False,
                "bOverride_ShadingModel": False,
                "bOverride_TwoSided": False,
                "bOverride_OpacityMaskClipValue": False,
                "BlendMode": "BLEND_Opaque",
                "ShadingModel": "MSM_DefaultLit",
                "TwoSided": False,
                "OpacityMaskClipValue": 0.3333,
            },
            "bOverridePhysMaterial": False,
            "bOverrideSubsurfaceProfile": False,
            "PhysMaterial": {"objectPath": ""},
            "SubsurfaceProfile": {"objectPath": ""},
            "NaniteOverrideMaterial": {"bEnableOverride": True, "OverrideMaterialEditor": {"objectPath": ""}},
            "ScalarParameterValues": [{
                "ParameterInfo": {"Name": "Roughness", "Association": "GlobalParameter", "Index": -1},
                "ParameterValue": 0.5,
                "ExpressionGUID": "11111111-2222-3333-4444-555555555555",
                "AtlasData": {
                    "bIsUsedAsAtlasPosition": False,
                    "Atlas": {"objectPath": ""},
                    "Curve": {"objectPath": ""},
                },
            }],
            "VectorParameterValues": [],
            "TextureParameterValues": [],
            "StaticParametersRuntime": {"MaterialLayers": {"Layers": [], "Blends": []}, "StaticSwitchParameters": []},
            "EditorStaticParameters": {"MaterialLayers": {}, "StaticComponentMaskParameters": [], "TerrainLayerWeightParameters": []},
            "PhysicalMaterialMap": [{"objectPath": ""}] * 8,
        },
    }
    document["reconstruction"]["targets"] = [{
        "id": "target:editor", "target": "editorAssetBuilderCpp", "backend": "unrealEditorCpp",
        "backendVersion": 1, "fidelity": "semanticEquivalent", "status": "ready",
        "writePolicy": "replaceGenerated", "blockerRefs": [],
    }]
    opcodes = (
        ("op:material-instance:create", "editor.materialInstance.create"),
        ("op:material-instance:apply-overrides", "editor.materialInstance.applyOverrides"),
        ("op:material-instance:save", "editor.asset.save"),
    )
    document["reconstruction"]["operations"] = [{
        "id": operation_id, "opcode": opcode, "opcodeVersion": 1, "phase": "configure",
        "targetId": "target:editor", "dependsOn": [], "operands": {}, "results": [],
        "sourcePointers": ["/semantics"], "preconditions": [], "postconditions": [],
        "criticality": "defaults", "status": "executable",
        "fidelity": {"status": "exact", "rule": "fixture", "confidence": 1},
        "failurePolicy": "abort", "idempotencyKey": operation_id,
    } for operation_id, opcode in opcodes]
    return document


def data_asset_fixture(
    *, blocked: bool = False, owned: bool = False, property_bag: bool = False,
    instanced_struct: bool = False
) -> dict:
    document = fixture()
    document["exporter"] = "DataAsset"
    document["asset"] = {
        "packageName": "/Game/Test/TestDataAsset",
        "objectPath": "/Game/Test/TestDataAsset.TestDataAsset",
        "assetClass": "PrimaryAssetLabel",
        "sourceFile": "Content/Test/TestDataAsset.uasset",
        "sourceHash": "sha256:" + "2" * 64,
    }
    document["semantics"] = {
        "kind": "DataAsset",
        "representation": "data-asset-properties-v2",
        "class": (
            "/ShooterExplorer/Test/BP_InputAction.BP_InputAction_C"
            if owned else "/Script/Engine.PrimaryAssetLabel"
        ),
        "primaryAssetId": "" if owned else "PrimaryAssetLabel:TestDataAsset",
        "ownedObjects": ([{
            "id": "InputModifierDeadZone_0",
            "name": "InputModifierDeadZone_0",
            "class": "/ShooterExplorer/Test/BP_Modifier.BP_Modifier_C",
            "outerId": "$asset",
            "creationMethod": "newObject",
            "properties": [
                {"name": "LowerThreshold", "type": "float", "value": 0.37},
                {"name": "UpperThreshold", "type": "float", "value": 0.91},
                {
                    "name": "Type", "type": "EDeadZoneType",
                    "value": {
                        "enum": "/Script/EnhancedInput.EDeadZoneType",
                        "name": "Axial", "value": 0,
                    },
                },
            ],
        }] if owned else []),
        "properties": ([
            {
                "name": "Modifiers", "type": "TArray",
                "value": [{
                    "ownedObjectId": "InputModifierDeadZone_0",
                    "class": "/Script/EnhancedInput.InputModifierDeadZone",
                }],
            },
            {"name": "Triggers", "type": "TArray", "value": []},
        ] if owned else [
            {"name": "bIsRuntimeLabel", "type": "bool", "value": True},
            {
                "name": "OptionalValue", "type": "TOptional<int32>",
                "value": {"isSet": True, "value": 73},
            },
            {
                "name": "EmptyDelegate", "type": "FTestDelegate",
                "value": {
                    "delegateKind": "multicast",
                    "signature": "/Script/Test.TestDelegate__DelegateSignature",
                    "bindings": [],
                },
            },
            {
                "name": "DisplayName",
                "type": "FText",
                "value": {"source": "Test", "namespace": "[11111111222233334444555555555555]", "key": "TestKey"},
            },
            {
                "name": "ExplicitAssets",
                "type": "TArray",
                "value": [{"objectPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"}],
            },
        ]),
    }
    blocker_refs = ["loss:data-asset"] if blocked else []
    document["reconstruction"]["targets"] = [{
        "id": "target:editor", "target": "editorAssetBuilderCpp", "backend": "unrealEditorCpp",
        "backendVersion": 1, "fidelity": "semanticEquivalent", "status": "partial" if blocked else "ready",
        "writePolicy": "replaceGenerated", "blockerRefs": blocker_refs,
    }]
    operations = []
    opcode_pairs = [
        ("op:data-asset:create", "editor.dataAsset.create"),
    ]
    if owned:
        opcode_pairs.extend([
            ("op:data-asset:create-owned-objects", "editor.dataAsset.createOwnedObjects"),
            ("op:data-asset:apply-owned-object-properties", "editor.dataAsset.applyOwnedObjectProperties"),
        ])
    opcode_pairs.extend([
        ("op:data-asset:apply-properties", "editor.dataAsset.applyProperties"),
        ("op:data-asset:save", "editor.asset.save"),
    ])
    previous: str | None = None
    for operation_id, opcode in opcode_pairs:
        is_blocked = blocked and opcode == "editor.dataAsset.applyProperties"
        operations.append({
            "id": operation_id, "opcode": opcode, "opcodeVersion": 1, "phase": "configure",
            "targetId": "target:editor", "dependsOn": [previous] if previous else [],
            "operands": {}, "results": [],
            "sourcePointers": [
                "/semantics/ownedObjects" if "Owned" in opcode else "/semantics/properties"
            ], "preconditions": [], "postconditions": [],
            "criticality": "defaults", "status": "blocked" if is_blocked else "executable",
            "fidelity": {
                "status": "unsupported" if is_blocked else "exact", "rule": "fixture",
                "confidence": 0 if is_blocked else 1,
                **({"reasonCode": "dataAssetOwnedObjectGraphUnavailable"} if is_blocked else {}),
            },
            "failurePolicy": "abort", "idempotencyKey": operation_id,
        })
        previous = operation_id
    document["reconstruction"]["operations"] = operations
    document["reconstruction"]["losses"] = ([{
        "id": "loss:data-asset", "reasonCode": "dataAssetOwnedObjectGraphUnavailable",
        "impact": "blocksReconstruction", "sourcePointers": ["/semantics/properties/0/value"],
        "recoverableFromSourceAsset": True,
    }] if blocked else [])
    if property_bag:
        document["semantics"]["properties"].append({
            "name": "Parameters",
            "type": "FInstancedPropertyBag",
            "value": {
                "propertyBagVersion": 1,
                "isValid": True,
                "layoutId": "PropertyBag_fixture",
                "properties": [{
                    "id": "11111111-2222-3333-4444-555555555555",
                    "name": "StartFollowDistance",
                    "valueType": "Float",
                    "containerTypes": [],
                    "propertyFlags": "1",
                    "keyType": "None",
                    "metadata": [{"key": "Category", "value": "Parameters"}],
                    "value": 420.0,
                }],
            },
        })
    if instanced_struct:
        document["semantics"]["properties"].append({
            "name": "SemanticInstancedStruct",
            "type": "FInstancedStruct",
            "value": {
                "instancedStructVersion": 1,
                "isValid": True,
                "valueStruct": "/Script/StateTreeModule.StateTreeCompareFloatCondition",
                "fields": {
                    "Left": 1000.0,
                    "Right": 200.0,
                },
            },
        })
    return document


class ReconstructionGeneratorTests(unittest.TestCase):
    def write_fixture(self, root: Path, document: dict) -> Path:
        source = root / "asset.uesem.json"
        source.write_text(json.dumps(document), encoding="utf-8")
        return source

    def test_generates_deterministic_plugin_and_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, fixture())
            output = root / "generated"
            first = generate_cpp_plugin(source, output, strict=True)
            snapshot = {
                path.relative_to(output).as_posix(): path.read_bytes()
                for path in output.rglob("*")
                if path.is_file()
            }
            second = generate_cpp_plugin(source, output, strict=True)
            repeated = {
                path.relative_to(output).as_posix(): path.read_bytes()
                for path in output.rglob("*")
                if path.is_file()
            }
            self.assertEqual(snapshot, repeated)
            self.assertEqual(first.files, second.files)
            header = (output / "Source/UERingGenerated/Public/UBP_Reconstruct.h").read_text()
            source_text = (output / "Source/UERingGenerated/Private/UBP_Reconstruct.cpp").read_text()
            self.assertIn("class UERINGGENERATED_API UBP_Reconstruct : public UObject", header)
            self.assertIn("UPROPERTY(EditAnywhere, BlueprintReadWrite)", header)
            self.assertIn("double Duration;", header)
            self.assertIn("Duration = 1.0;", source_text)
            reflection = (output / "Source/UERingGenerated/Private/UBP_ReconstructReflectionTest.cpp").read_text()
            self.assertIn("property default Duration", reflection)
            self.assertIn("Defaults->Duration, 1.0", reflection)
            manifest = json.loads((output / ".uesem-generated.json").read_text())
            self.assertEqual([], manifest["blockedOperations"])
            self.assertEqual(3, len(manifest["executedOperations"]))

    def test_strict_mode_rejects_blocked_behavior(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, fixture(blocked=True))
            with self.assertRaisesRegex(ReconstructionError, "strict reconstruction blocked"):
                generate_cpp_plugin(source, root / "generated", strict=True)

    def test_non_strict_mode_emits_compile_blocker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, fixture(blocked=True))
            output = root / "generated"
            result = generate_cpp_plugin(source, output)
            blocker = output / "Source/UERingGenerated/Private/UERingReconstructionBlockers.cpp"
            self.assertIn("#error", blocker.read_text())
            self.assertEqual(("op:graph:0:Empty",), result.blocked_operations)

    def test_rejects_cpp_type_injection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, fixture(cpp_type="double; #include <evil>"))
            with self.assertRaisesRegex(ReconstructionError, r"unsafe or unsupported C\+\+ type"):
                generate_cpp_plugin(source, root / "generated", strict=True)

    def test_refuses_unowned_non_empty_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, fixture())
            output = root / "generated"
            output.mkdir()
            (output / "user-file.txt").write_text("owned by user")
            with self.assertRaisesRegex(ReconstructionError, "non-empty directory"):
                generate_cpp_plugin(source, output, strict=True)

    def test_rejects_invalid_module_identifier(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, fixture())
            with self.assertRaisesRegex(ReconstructionError, r"valid C\+\+ identifier"):
                generate_cpp_plugin(source, root / "generated", module="../Outside", strict=True)

    def test_generates_material_instance_editor_builder(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, material_instance_fixture())
            output = root / "generated"
            result = generate_cpp_plugin(
                source, output, module="UERingGeneratedMaterial", strict=True,
                asset_package="/Game/UERingGenerated/MI_Reconstructed",
            )
            generated = (output / "Source/UERingGeneratedMaterial/Private/MI_ReconstructedMaterialInstanceBuilderTest.cpp").read_text()
            self.assertIn("FMaterialInstanceParameterUpdateContext", generated)
            self.assertIn("ScalarParameterValues.Emplace_GetRef", generated)
            self.assertIn("FScalarParameterAtlasInstanceData", generated)
            self.assertIn("UPackage::SavePackage", generated)
            plugin = json.loads((output / "UERingGeneratedMaterial.uplugin").read_text())
            self.assertEqual("Editor", plugin["Modules"][0]["Type"])
            self.assertEqual(3, len(result.executed_operations))

    def test_generates_data_asset_editor_builder(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, data_asset_fixture())
            output = root / "generated"
            result = generate_cpp_plugin(
                source, output, module="UERingGeneratedData", strict=True,
                asset_package="/Game/UERingGenerated/TestDataAsset",
            )
            generated = (output / "Source/UERingGeneratedData/Private/TestDataAssetDataAssetBuilderTest.cpp").read_text()
            self.assertIn("FScriptArrayHelper", generated)
            self.assertIn("FScriptSetHelper", generated)
            self.assertIn("FScriptMapHelper", generated)
            self.assertIn("FText::ChangeKey", generated)
            self.assertIn("FOptionalProperty", generated)
            self.assertIn("MarkSetAndGetInitializedValuePointerToReplace", generated)
            self.assertIn("FMulticastDelegateProperty", generated)
            self.assertIn("ClearDelegate(Owner, ValuePtr)", generated)
            self.assertIn("TextNamespaceUtil::ForcePackageNamespace", generated)
            self.assertIn("/Script/Engine.PrimaryAssetLabel", generated)
            self.assertIn("UPackage::SavePackage", generated)
            self.assertNotIn("__OBJECT_NAME__", generated)
            self.assertEqual(3, len(result.executed_operations))

    def test_generates_owned_data_asset_object_graph_builder(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, data_asset_fixture(owned=True))
            output = root / "generated"
            result = generate_cpp_plugin(
                source, output, module="UERingGeneratedInput", strict=True,
                asset_package="/Game/UERingGenerated/TestDataAsset",
            )
            generated = (
                output / "Source/UERingGeneratedInput/Private/TestDataAssetDataAssetBuilderTest.cpp"
            ).read_text()
            self.assertIn("SerializedOwnedObjects", generated)
            self.assertIn("NewObject<UObject>(Outer, OwnedClass", generated)
            self.assertIn("StaticFindObjectFast", generated)
            self.assertIn("OwnedObjects.FindRef(OwnedObjectId)", generated)
            self.assertIn("InputModifierDeadZone_0", generated)
            self.assertEqual(5, len(result.executed_operations))

    def test_generates_property_bag_data_asset_builder(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, data_asset_fixture(property_bag=True))
            output = root / "generated"
            generate_cpp_plugin(
                source, output, module="UERingGeneratedBag", strict=True,
                asset_package="/Game/UERingGenerated/TestDataAsset",
            )
            generated = (
                output / "Source/UERingGeneratedBag/Private/TestDataAssetDataAssetBuilderTest.cpp"
            ).read_text()
            self.assertIn('#include "StructUtils/PropertyBag.h"', generated)
            self.assertIn("UPropertyBag::GetOrCreateFromDescs", generated)
            self.assertIn("Bag.InitializeFromBagStruct", generated)
            self.assertIn("property bag layout does not match descriptors", generated)
            self.assertIn("StartFollowDistance", generated)

    def test_generates_instanced_struct_data_asset_builder(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, data_asset_fixture(instanced_struct=True))
            output = root / "generated"
            generate_cpp_plugin(
                source, output, module="UERingGeneratedStruct", strict=True,
                asset_package="/Game/UERingGenerated/TestDataAsset",
            )
            generated = (
                output
                / "Source/UERingGeneratedStruct/Private/TestDataAssetDataAssetBuilderTest.cpp"
            ).read_text()
            self.assertIn('#include "StructUtils/InstancedStruct.h"', generated)
            self.assertIn("ApplyInstancedStruct", generated)
            self.assertIn("Instanced.InitializeAs(ValueStruct)", generated)
            self.assertIn("StateTreeCompareFloatCondition", generated)
            self.assertIn("SemanticInstancedStruct", generated)

    def test_generates_state_tree_compile_strategy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = data_asset_fixture(property_bag=True)
            document["semantics"]["class"] = "/Script/StateTreeModule.StateTree"
            document["semantics"]["properties"] = [
                {"name": "EditorData", "type": "TObjectPtr<UObject>", "value": {"objectPath": ""}},
                document["semantics"]["properties"][-1],
            ]
            document["semantics"]["reconstructionPolicy"] = {
                "strategy": "state-tree-editor-compile-v1",
                "authoredRootProperties": ["EditorData"],
                "derivedRootProperties": ["Parameters"],
            }
            document["reconstruction"]["operations"].insert(-1, {
                "id": "op:data-asset:compile-state-tree",
                "opcode": "editor.stateTree.compile",
                "opcodeVersion": 1,
                "phase": "configure",
                "targetId": "target:editor",
                "dependsOn": ["op:data-asset:apply-properties"],
                "operands": {"strategy": "state-tree-editor-compile-v1"},
                "results": [],
                "sourcePointers": ["/semantics/reconstructionPolicy"],
                "preconditions": [],
                "postconditions": [],
                "criticality": "behavior",
                "status": "executable",
                "fidelity": {"status": "exact", "rule": "fixture", "confidence": 1},
                "failurePolicy": "abort",
                "idempotencyKey": "fixture:state-tree-compile",
            })
            source = self.write_fixture(root, document)
            output = root / "generated"
            result = generate_cpp_plugin(
                source, output, module="UERingGeneratedStateTree", strict=True,
                asset_package="/Game/UERingGenerated/TestDataAsset",
            )
            generated = (
                output
                / "Source/UERingGeneratedStateTree/Private/TestDataAssetDataAssetBuilderTest.cpp"
            ).read_text()
            build_rules = (
                output / "Source/UERingGeneratedStateTree/UERingGeneratedStateTree.Build.cs"
            ).read_text()
            plugin = json.loads((output / "UERingGeneratedStateTree.uplugin").read_text())
            self.assertIn('#include "StateTreeEditingSubsystem.h"', generated)
            self.assertIn("CompileStateTree(StateTree, CompilerLog)", generated)
            self.assertNotIn("StartFollowDistance", generated)
            self.assertIn('"StateTreeEditorModule"', build_rules)
            self.assertIn('"PropertyBindingUtils"', build_rules)
            self.assertEqual(
                [
                    {"Name": "PropertyBindingUtils", "Enabled": True},
                    {"Name": "StateTree", "Enabled": True},
                ],
                plugin["Plugins"],
            )
            self.assertEqual(4, len(result.executed_operations))

            document["semantics"]["reconstructionPolicy"]["derivedRootProperties"] = []
            source = self.write_fixture(root, document)
            with self.assertRaisesRegex(ReconstructionError, "does not cover every root property"):
                generate_cpp_plugin(
                    source, root / "invalid-policy", module="UERingGeneratedStateTree",
                    strict=True, asset_package="/Game/UERingGenerated/TestDataAsset",
                )

    def test_data_asset_rejects_changed_name_and_strict_blocker(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, data_asset_fixture())
            with self.assertRaisesRegex(ReconstructionError, "must match the source object name"):
                generate_cpp_plugin(
                    source, root / "renamed", strict=True,
                    asset_package="/Game/UERingGenerated/RenamedDataAsset",
                )
            source = self.write_fixture(root, data_asset_fixture(blocked=True))
            with self.assertRaisesRegex(ReconstructionError, "strict reconstruction blocked"):
                generate_cpp_plugin(
                    source, root / "blocked", strict=True,
                    asset_package="/Game/UERingGenerated/TestDataAsset",
                )

            document = data_asset_fixture(owned=True)
            document["semantics"]["ownedObjects"][0]["class"] = "/ShooterExplorer/../Bad.Bad_C"
            source = self.write_fixture(root, document)
            with self.assertRaisesRegex(ReconstructionError, "owned object class path is unsafe"):
                generate_cpp_plugin(
                    source, root / "unsafe-class", strict=True,
                    asset_package="/Game/UERingGenerated/TestDataAsset",
                )

    def test_material_instance_rejects_unsafe_target_and_unsupported_override(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = material_instance_fixture()
            source = self.write_fixture(root, document)
            with self.assertRaisesRegex(ReconstructionError, "below /Game/UERingGenerated"):
                generate_cpp_plugin(source, root / "unsafe", strict=True, asset_package="/Game/Source/Overwrite")
            document["semantics"]["overrides"]["FontParameterValues"] = [{"unsupported": True}]
            source = self.write_fixture(root, document)
            with self.assertRaisesRegex(ReconstructionError, "FontParameterValues"):
                generate_cpp_plugin(source, root / "blocked", strict=True, asset_package="/Game/UERingGenerated/MI_Blocked")

    def test_semantic_reexport_comparison_reports_first_pointer(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = self.write_fixture(root, material_instance_fixture())
            reconstructed = root / "reconstructed.uesem.json"
            reconstructed.write_text(source.read_text(), encoding="utf-8")
            verify_semantic_reexport(source, reconstructed)
            changed = json.loads(reconstructed.read_text())
            changed["semantics"]["overrides"]["ScalarParameterValues"][0]["ParameterValue"] = 0.75
            reconstructed.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaisesRegex(ReconstructionError, "/semantics/overrides/ScalarParameterValues/0/ParameterValue"):
                verify_semantic_reexport(source, reconstructed)
            del changed["semantics"]
            reconstructed.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaisesRegex(ReconstructionError, "reconstructed semantics must be an object"):
                verify_semantic_reexport(source, reconstructed)

    def test_state_tree_reexport_compares_authored_projection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = data_asset_fixture()
            document["semantics"]["properties"] = [
                {
                    "name": "EditorData",
                    "type": "TObjectPtr<UObject>",
                    "value": {"ownedObjectId": "StateTreeEditorData_0"},
                },
                {"name": "CustomAssetSavedVersion", "type": "int32", "value": 0},
            ]
            document["semantics"]["reconstructionPolicy"] = {
                "strategy": "state-tree-editor-compile-v1",
                "authoredRootProperties": ["EditorData"],
                "derivedRootProperties": ["CustomAssetSavedVersion"],
            }
            source = self.write_fixture(root, document)
            changed = json.loads(source.read_text())
            changed["semantics"]["properties"][1]["value"] = 1
            reconstructed = root / "reconstructed.uesem.json"
            reconstructed.write_text(json.dumps(changed), encoding="utf-8")
            verify_semantic_reexport(source, reconstructed)

            changed["semantics"]["properties"][0]["value"]["ownedObjectId"] = "Different"
            reconstructed.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaisesRegex(
                ReconstructionError,
                "/semantics/properties/0/value/ownedObjectId",
            ):
                verify_semantic_reexport(source, reconstructed)


if __name__ == "__main__":
    unittest.main()
