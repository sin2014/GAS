from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import os
import re
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any


GENERATOR_VERSION = "1.5.0"
MANIFEST_NAME = ".uesem-generated.json"
NATIVE_CLASS_OPCODES = {
    "cpp.class.declare",
    "cpp.property.declare",
    "cpp.graph.assertEmpty",
}
MATERIAL_INSTANCE_OPCODES = {
    "editor.materialInstance.create",
    "editor.materialInstance.applyOverrides",
    "editor.asset.save",
}
DATA_ASSET_BASE_OPCODES = {
    "editor.dataAsset.create",
    "editor.dataAsset.applyProperties",
    "editor.asset.save",
}
DATA_ASSET_OWNED_OPCODES = {
    "editor.dataAsset.createOwnedObjects",
    "editor.dataAsset.applyOwnedObjectProperties",
}
STATE_TREE_OPCODES = {"editor.stateTree.compile"}
DATA_ASSET_OPCODES = DATA_ASSET_BASE_OPCODES | DATA_ASSET_OWNED_OPCODES | STATE_TREE_OPCODES
IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
CPP_TYPE = re.compile(r"^[A-Za-z_][A-Za-z0-9_:<>, *&]*$")
GENERATED_ASSET_PACKAGE = re.compile(r"^/Game/UERingGenerated(?:/[A-Za-z_][A-Za-z0-9_]*)+$")
UNREAL_CLASS_PATH = re.compile(
    r"^/[A-Za-z_][A-Za-z0-9_]*(?:/[A-Za-z_][A-Za-z0-9_]*)*\.[A-Za-z_][A-Za-z0-9_]*$"
)
GUID = re.compile(r"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")


class ReconstructionError(RuntimeError):
    pass


@dataclass(frozen=True)
class GenerationResult:
    output_root: Path
    files: tuple[str, ...]
    executed_operations: tuple[str, ...]
    blocked_operations: tuple[str, ...]


def _require_object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ReconstructionError(f"{label} must be an object")
    return value


def _require_string(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise ReconstructionError(f"{label} must be a non-empty string")
    return value


def _require_array(value: Any, label: str) -> list[Any]:
    if not isinstance(value, list):
        raise ReconstructionError(f"{label} must be an array")
    return value


def _require_bool(value: Any, label: str) -> bool:
    if not isinstance(value, bool):
        raise ReconstructionError(f"{label} must be a boolean")
    return value


def _is_safe_unreal_class_path(value: str) -> bool:
    return UNREAL_CLASS_PATH.fullmatch(value) is not None


def _number(value: Any, label: str) -> float:
    if not isinstance(value, (int, float)) or isinstance(value, bool) or not math.isfinite(value):
        raise ReconstructionError(f"{label} must be a finite number")
    return float(value)


def _float_literal(value: Any, label: str) -> str:
    number = _number(value, label)
    result = repr(number)
    if "." not in result and "e" not in result.lower():
        result += ".0"
    return result + "f"


def _bool_literal(value: Any, label: str) -> str:
    return "true" if _require_bool(value, label) else "false"


def _object_path(value: Any, label: str, *, allow_empty: bool = True) -> str:
    obj = _require_object(value, label)
    path = obj.get("objectPath")
    if not isinstance(path, str) or (not allow_empty and not path):
        raise ReconstructionError(f"{label}.objectPath must be a valid object path")
    if path and (not path.startswith("/") or any(char in path for char in "\r\n\t\"")):
        raise ReconstructionError(f"{label}.objectPath is unsafe: {path!r}")
    return path


def _guid(value: Any, label: str) -> str:
    text = _require_string(value, label)
    if not GUID.fullmatch(text):
        raise ReconstructionError(f"{label} is not a canonical GUID")
    return f"UERingGuid({_cpp_text(text)})"


def _parameter_info(value: Any, label: str) -> str:
    info = _require_object(value, label)
    name = _require_string(info.get("Name"), f"{label}.Name")
    association = _require_string(info.get("Association"), f"{label}.Association")
    if association not in {"GlobalParameter", "LayerParameter", "BlendParameter"}:
        raise ReconstructionError(f"{label}.Association is unsupported: {association}")
    index = info.get("Index")
    if not isinstance(index, int) or isinstance(index, bool):
        raise ReconstructionError(f"{label}.Index must be an integer")
    return (
        f"FMaterialParameterInfo({_cpp_text(name)}, "
        f"EMaterialParameterAssociation::{association}, {index})"
    )


def _enum_literal(value: Any, prefix: str, label: str) -> str:
    text = _require_string(value, label)
    if not IDENTIFIER.fullmatch(text) or not text.startswith(prefix):
        raise ReconstructionError(f"{label} is not a supported {prefix} enum value: {text!r}")
    return text


def _empty_object_path(value: Any) -> bool:
    return isinstance(value, dict) and value.get("objectPath") == ""


def _is_default_material_layers(value: Any) -> bool:
    if not isinstance(value, dict):
        return False
    return all(not value.get(field) for field in (
        "Blends", "Layers", "DeletedParentLayerGuids", "LayerGuids", "LayerLinkStates",
        "LayerNames", "LayerStates", "RestrictToBlendRelatives", "RestrictToLayerRelatives",
    ))


def _material_instance_unsupported(overrides: dict[str, Any]) -> list[str]:
    unsupported: list[str] = []
    for field in (
        "DoubleVectorParameterValues", "FontParameterValues", "ParameterCollectionParameterValues",
        "RuntimeVirtualTextureParameterValues", "SparseVolumeTextureParameterValues",
        "TextureCollectionParameterValues", "UserSceneTextureOverrides",
    ):
        if overrides.get(field):
            unsupported.append(field)
    for field in ("PhysMaterialMask", "SpecularProfileOverride", "ToonProfileOverride", "NeuralProfile"):
        value = overrides.get(field)
        if value is not None and not _empty_object_path(value):
            unsupported.append(field)
    physical_map = overrides.get("PhysicalMaterialMap", [])
    if not isinstance(physical_map, list) or any(not _empty_object_path(item) for item in physical_map):
        unsupported.append("PhysicalMaterialMap")
    static_runtime = overrides.get("StaticParametersRuntime", {})
    if isinstance(static_runtime, dict):
        if static_runtime.get("bHasMaterialLayers") or not _is_default_material_layers(static_runtime.get("MaterialLayers", {})):
            unsupported.append("StaticParametersRuntime.MaterialLayers")
    editor_static = overrides.get("EditorStaticParameters", {})
    if isinstance(editor_static, dict):
        if not _is_default_material_layers(editor_static.get("MaterialLayers", {})):
            unsupported.append("EditorStaticParameters.MaterialLayers")
        if editor_static.get("TerrainLayerWeightParameters"):
            unsupported.append("EditorStaticParameters.TerrainLayerWeightParameters")
    lightmass = overrides.get("LightmassSettings", {})
    if isinstance(lightmass, dict) and any(
        lightmass.get(field) for field in (
            "bOverrideCastShadowAsMasked", "bOverrideDiffuseBoost", "bOverrideEmissiveBoost",
            "bOverrideExportResolutionScale",
        )
    ):
        unsupported.append("LightmassSettings")
    if overrides.get("bOverrideBlendableLocation") or overrides.get("bOverrideBlendablePriority"):
        unsupported.append("BlendableOverrides")
    if overrides.get("bOverrideSpecularProfile") or overrides.get("bOverrideToonProfile"):
        unsupported.append("ProfileOverrides")
    return sorted(set(unsupported))


def _identifier(value: str, label: str) -> str:
    if not IDENTIFIER.fullmatch(value):
        raise ReconstructionError(f"{label} is not a valid C++ identifier: {value!r}")
    return value


def _cpp_type(value: str) -> str:
    if not CPP_TYPE.fullmatch(value) or any(token in value for token in ("//", "/*", "#", ";", "{", "}")):
        raise ReconstructionError(f"unsafe or unsupported C++ type: {value!r}")
    return value.strip()


def _cpp_text(value: str) -> str:
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\r", "\\r")
        .replace("\n", "\\n")
        .replace("\t", "\\t")
    )
    return f'TEXT("{escaped}")'


def _uht_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _default_expression(cpp_type: str, value: Any) -> str | None:
    normalized = cpp_type.replace(" ", "")
    if value is None:
        return "nullptr" if "*" in normalized or "Ptr<" in normalized else None
    if normalized == "bool" and isinstance(value, bool):
        return "true" if value else "false"
    if normalized in {"float", "double"} and isinstance(value, (int, float)) and not isinstance(value, bool):
        number = repr(float(value))
        return f"{number}f" if normalized == "float" else number
    if normalized in {
        "int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64",
    } and isinstance(value, int) and not isinstance(value, bool):
        return str(value)
    if normalized == "FString" and isinstance(value, str):
        return _cpp_text(value)
    if normalized == "FName" and isinstance(value, str):
        return f"FName({_cpp_text(value)})"
    if normalized == "FText" and isinstance(value, dict):
        text = value.get("source", value.get("displayText", ""))
        if isinstance(text, str):
            return "FText::GetEmpty()" if not text else f"FText::FromString({_cpp_text(text)})"
    if isinstance(value, dict) and "objectPath" in value:
        path = value.get("objectPath")
        if path == "":
            return "nullptr"
    return None


def _default_test_lines(cpp_type: str, name: str, value: Any, expression: str) -> list[str]:
    normalized = cpp_type.replace(" ", "")
    label = f'TEXT("property default {name}")'
    if value is None or (isinstance(value, dict) and value.get("objectPath") == ""):
        return [f"    TestNull({label}, Defaults->{name});"]
    if normalized == "FText":
        return [f"    TestTrue({label}, Defaults->{name}.EqualTo({expression}));"]
    return [f"    TestEqual({label}, Defaults->{name}, {expression});"]


def _property_specifiers(operands: dict[str, Any]) -> str:
    flags = operands.get("flags", [])
    if not isinstance(flags, list) or not all(isinstance(item, str) for item in flags):
        raise ReconstructionError("property flags must be a string array")
    values: list[str] = []
    if "InstanceEditable" in flags:
        values.append("EditAnywhere")
    if "BlueprintReadOnly" in flags:
        values.append("BlueprintReadOnly")
    elif "BlueprintVisible" in flags:
        values.append("BlueprintReadWrite")
    if "SaveGame" in flags:
        values.append("SaveGame")
    if "Replicated" in flags:
        values.append("Replicated")
    metadata: list[str] = []
    if "ExposeOnSpawn" in flags:
        metadata.append("ExposeOnSpawn=true")
    category = operands.get("category")
    if isinstance(category, str) and category:
        values.append(f"Category={_uht_string(category)}")
    if metadata:
        values.append("meta=(" + ", ".join(metadata) + ")")
    return ", ".join(values)


def _safe_relative(root: Path, relative: str) -> Path:
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise ReconstructionError(f"generated path escapes output root: {relative}") from exc
    return candidate


def _atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(handle, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def _load_previous_manifest(root: Path) -> dict[str, Any] | None:
    if not root.exists():
        return None
    entries = list(root.iterdir())
    if not entries:
        return None
    manifest_path = root / MANIFEST_NAME
    if not manifest_path.is_file():
        raise ReconstructionError("refusing to write into a non-empty directory without a generation manifest")
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as exc:
        raise ReconstructionError("existing generation manifest is unreadable") from exc
    if manifest.get("schema") != "com.ue-ring.generated-cpp":
        raise ReconstructionError("existing generation manifest has an unexpected schema")
    files = manifest.get("files")
    if not isinstance(files, list) or not all(isinstance(item, dict) and isinstance(item.get("path"), str) for item in files):
        raise ReconstructionError("existing generation manifest has an invalid files list")
    return manifest


def _material_instance_files(
    document: dict[str, Any],
    module: str,
    strict: bool,
    asset_package: str | None,
) -> tuple[dict[str, str], list[str], list[str]]:
    reconstruction = _require_object(document.get("reconstruction"), "reconstruction")
    targets = _require_array(reconstruction.get("targets"), "reconstruction.targets")
    target = next((item for item in targets if isinstance(item, dict)
                   and item.get("backend") == "unrealEditorCpp"
                   and item.get("target") == "editorAssetBuilderCpp"), None)
    if target is None:
        raise ReconstructionError("document has no editorAssetBuilderCpp unrealEditorCpp target")
    target_id = _require_string(target.get("id"), "target.id")
    if asset_package is None:
        raise ReconstructionError("material instance reconstruction requires --asset-package")
    if not GENERATED_ASSET_PACKAGE.fullmatch(asset_package):
        raise ReconstructionError("asset package must be below /Game/UERingGenerated and use identifier path segments")
    object_name = _identifier(asset_package.rsplit("/", 1)[-1], "generated asset name")

    operations = _require_array(reconstruction.get("operations"), "reconstruction.operations")
    executed: list[str] = []
    blocked: list[str] = []
    seen_opcodes: set[str] = set()
    seen_ids: set[str] = set()
    for raw in operations:
        operation = _require_object(raw, "operation")
        if operation.get("targetId") != target_id:
            continue
        operation_id = _require_string(operation.get("id"), "operation.id")
        if operation_id in seen_ids:
            raise ReconstructionError(f"duplicate operation id: {operation_id}")
        seen_ids.add(operation_id)
        opcode = _require_string(operation.get("opcode"), f"{operation_id}.opcode")
        if opcode in MATERIAL_INSTANCE_OPCODES:
            seen_opcodes.add(opcode)
            if operation.get("status") == "executable":
                executed.append(operation_id)
            else:
                blocked.append(operation_id)
        else:
            blocked.append(operation_id)
    missing = MATERIAL_INSTANCE_OPCODES - seen_opcodes
    if missing:
        raise ReconstructionError("material instance IR is missing executable opcodes: " + ", ".join(sorted(missing)))

    semantics = _require_object(document.get("semantics"), "semantics")
    if semantics.get("representation") != "material-instance-v1":
        raise ReconstructionError("editorAssetBuilderCpp only supports material-instance-v1")
    parent_path = _object_path(semantics.get("parent"), "semantics.parent", allow_empty=False)
    overrides = _require_object(semantics.get("overrides"), "semantics.overrides")
    unsupported = _material_instance_unsupported(overrides)
    blocked.extend(f"unsupported:{item}" for item in unsupported)
    if strict and blocked:
        raise ReconstructionError("strict reconstruction blocked by material instance fields: " + ", ".join(blocked))

    apply_lines: list[str] = []
    verify_lines: list[str] = []
    base = _require_object(overrides.get("BasePropertyOverrides", {}), "BasePropertyOverrides")
    if base:
        apply_lines.append("        FMaterialInstanceBasePropertyOverrides BaseOverrides;")
        bool_fields = (
            "bCastDynamicShadowAsMasked", "bCompatibleWithLumenCardSharing", "bEnableDisplacementFade",
            "bEnableTessellation", "bHasPixelAnimation", "bIsThinSurface", "bOutputTranslucentVelocity",
            "bOverride_bEnableDisplacementFade", "bOverride_bEnableTessellation", "bOverride_bHasPixelAnimation",
            "bOverride_bIsThinSurface", "bOverride_BlendMode", "bOverride_CastDynamicShadowAsMasked",
            "bOverride_CompatibleWithLumenCardSharing", "bOverride_DisplacementFadeRange",
            "bOverride_DisplacementScaling", "bOverride_DitheredLODTransition",
            "bOverride_MaxWorldPositionOffsetDisplacement", "bOverride_OpacityMaskClipValue",
            "bOverride_OutputTranslucentVelocity", "bOverride_ShadingModel", "bOverride_TwoSided", "DitheredLODTransition",
            "TwoSided",
        )
        for field in bool_fields:
            if field in base:
                apply_lines.append(f"        BaseOverrides.{field} = {_bool_literal(base[field], 'BasePropertyOverrides.' + field)};")
        for field in ("bOverride_UsageFlags", "UsageFlags"):
            if field in base:
                value = base[field]
                if not isinstance(value, int) or isinstance(value, bool) or value < 0:
                    raise ReconstructionError(f"BasePropertyOverrides.{field} must be a non-negative integer")
                apply_lines.append(f"        BaseOverrides.{field} = {value}u;")
        if "BlendMode" in base:
            apply_lines.append(f"        BaseOverrides.BlendMode = {_enum_literal(base['BlendMode'], 'BLEND_', 'BlendMode')};")
        if "ShadingModel" in base:
            apply_lines.append(f"        BaseOverrides.ShadingModel = {_enum_literal(base['ShadingModel'], 'MSM_', 'ShadingModel')};")
        for field in ("OpacityMaskClipValue", "MaxWorldPositionOffsetDisplacement"):
            if field in base:
                apply_lines.append(f"        BaseOverrides.{field} = {_float_literal(base[field], field)};")
        scaling = base.get("DisplacementScaling")
        if isinstance(scaling, dict):
            for field in ("Magnitude", "Center"):
                if field in scaling:
                    apply_lines.append(f"        BaseOverrides.DisplacementScaling.{field} = {_float_literal(scaling[field], 'DisplacementScaling.' + field)};")
        fade = base.get("DisplacementFadeRange")
        if isinstance(fade, dict):
            for field in ("StartSizePixels", "EndSizePixels"):
                if field in fade:
                    apply_lines.append(f"        BaseOverrides.DisplacementFadeRange.{field} = {_float_literal(fade[field], 'DisplacementFadeRange.' + field)};")
        apply_lines.append("        UpdateContext.SetBasePropertyOverrides(BaseOverrides);")

    static_runtime = _require_object(overrides.get("StaticParametersRuntime", {}), "StaticParametersRuntime")
    for index, raw in enumerate(_require_array(static_runtime.get("StaticSwitchParameters", []), "StaticSwitchParameters")):
        entry = _require_object(raw, f"StaticSwitchParameters[{index}]")
        info = _parameter_info(entry.get("ParameterInfo"), f"StaticSwitchParameters[{index}].ParameterInfo")
        apply_lines.append(
            "        StaticParameters.StaticSwitchParameters.Emplace("
            f"{info}, {_bool_literal(entry.get('Value'), 'StaticSwitch.Value')}, "
            f"{_bool_literal(entry.get('bOverride'), 'StaticSwitch.bOverride')}, "
            f"{_guid(entry.get('ExpressionGUID'), 'StaticSwitch.ExpressionGUID')});"
        )
    editor_static = _require_object(overrides.get("EditorStaticParameters", {}), "EditorStaticParameters")
    for index, raw in enumerate(_require_array(editor_static.get("StaticComponentMaskParameters", []), "StaticComponentMaskParameters")):
        entry = _require_object(raw, f"StaticComponentMaskParameters[{index}]")
        info = _parameter_info(entry.get("ParameterInfo"), f"StaticComponentMaskParameters[{index}].ParameterInfo")
        apply_lines.append(
            "        StaticParameters.StaticComponentMaskParameters.Emplace("
            f"{info}, {_bool_literal(entry.get('R'), 'mask.R')}, {_bool_literal(entry.get('G'), 'mask.G')}, "
            f"{_bool_literal(entry.get('B'), 'mask.B')}, {_bool_literal(entry.get('A'), 'mask.A')}, "
            f"{_bool_literal(entry.get('bOverride'), 'mask.bOverride')}, {_guid(entry.get('ExpressionGUID'), 'mask.ExpressionGUID')});"
        )

    uniform_lines: list[str] = []
    scalar_values = _require_array(overrides.get("ScalarParameterValues", []), "ScalarParameterValues")
    for index, raw in enumerate(scalar_values):
        entry = _require_object(raw, f"ScalarParameterValues[{index}]")
        info = _parameter_info(entry.get("ParameterInfo"), f"ScalarParameterValues[{index}].ParameterInfo")
        atlas = _require_object(entry.get("AtlasData", {
            "bIsUsedAsAtlasPosition": False, "Atlas": {"objectPath": ""}, "Curve": {"objectPath": ""},
        }), f"ScalarParameterValues[{index}].AtlasData")
        atlas_path = _object_path(atlas.get("Atlas", {"objectPath": ""}), f"ScalarParameterValues[{index}].AtlasData.Atlas")
        curve_path = _object_path(atlas.get("Curve", {"objectPath": ""}), f"ScalarParameterValues[{index}].AtlasData.Curve")
        uniform_lines.extend([
            "    {",
            "        FScalarParameterAtlasInstanceData AtlasData;",
            f"        AtlasData.bIsUsedAsAtlasPosition = {_bool_literal(atlas.get('bIsUsedAsAtlasPosition', False), 'AtlasData.bIsUsedAsAtlasPosition')};",
            f"        AtlasData.Atlas = TSoftObjectPtr<UCurveLinearColorAtlas>(FSoftObjectPath({_cpp_text(atlas_path)}));",
            f"        AtlasData.Curve = TSoftObjectPtr<UCurveLinearColor>(FSoftObjectPath({_cpp_text(curve_path)}));",
            f"        FScalarParameterValue& Value = Instance->ScalarParameterValues.Emplace_GetRef({info}, {_float_literal(entry.get('ParameterValue'), 'scalar value')}, AtlasData);",
            f"        Value.ExpressionGUID = {_guid(entry.get('ExpressionGUID'), 'scalar ExpressionGUID')};",
            "    }",
        ])
    vector_values = _require_array(overrides.get("VectorParameterValues", []), "VectorParameterValues")
    for index, raw in enumerate(vector_values):
        entry = _require_object(raw, f"VectorParameterValues[{index}]")
        info = _parameter_info(entry.get("ParameterInfo"), f"VectorParameterValues[{index}].ParameterInfo")
        color = _require_object(entry.get("ParameterValue"), f"VectorParameterValues[{index}].ParameterValue")
        color_expr = "FLinearColor(" + ", ".join(_float_literal(color.get(c), f"color.{c}") for c in "RGBA") + ")"
        uniform_lines.extend([
            "    {",
            f"        FVectorParameterValue& Value = Instance->VectorParameterValues.Emplace_GetRef({info}, {color_expr});",
            f"        Value.ExpressionGUID = {_guid(entry.get('ExpressionGUID'), 'vector ExpressionGUID')};",
            "    }",
        ])
    texture_values = _require_array(overrides.get("TextureParameterValues", []), "TextureParameterValues")
    for index, raw in enumerate(texture_values):
        entry = _require_object(raw, f"TextureParameterValues[{index}]")
        info = _parameter_info(entry.get("ParameterInfo"), f"TextureParameterValues[{index}].ParameterInfo")
        path = _object_path(entry.get("ParameterValue"), f"TextureParameterValues[{index}].ParameterValue")
        load = "nullptr" if not path else f"LoadObject<UTexture>(nullptr, {_cpp_text(path)})"
        uniform_lines.extend([
            "    {",
            f"        FTextureParameterValue& Value = Instance->TextureParameterValues.Emplace_GetRef({info}, {load});",
            f"        Value.ExpressionGUID = {_guid(entry.get('ExpressionGUID'), 'texture ExpressionGUID')};",
            "    }",
        ])

    phys_path = _object_path(overrides.get("PhysMaterial", {"objectPath": ""}), "PhysMaterial")
    subsurface_path = _object_path(overrides.get("SubsurfaceProfile", {"objectPath": ""}), "SubsurfaceProfile")
    nanite = _require_object(overrides.get("NaniteOverrideMaterial", {"bEnableOverride": True, "OverrideMaterialEditor": {"objectPath": ""}}), "NaniteOverrideMaterial")
    nanite_path = _object_path(nanite.get("OverrideMaterialEditor", {"objectPath": ""}), "NaniteOverrideMaterial.OverrideMaterialEditor")
    tail_lines = [
        f"    Instance->bOverridePhysMaterial = {_bool_literal(overrides.get('bOverridePhysMaterial', False), 'bOverridePhysMaterial')};",
        f"    Instance->PhysMaterial = {('nullptr' if not phys_path else 'LoadObject<UPhysicalMaterial>(nullptr, ' + _cpp_text(phys_path) + ')')};",
        f"    Instance->bOverrideSubsurfaceProfile = {_bool_literal(overrides.get('bOverrideSubsurfaceProfile', False), 'bOverrideSubsurfaceProfile')};",
        f"    Instance->SubsurfaceProfile = {('nullptr' if not subsurface_path else 'LoadObject<USubsurfaceProfile>(nullptr, ' + _cpp_text(subsurface_path) + ')')};",
        f"    Instance->NaniteOverrideMaterial.bEnableOverride = {_bool_literal(nanite.get('bEnableOverride'), 'NaniteOverrideMaterial.bEnableOverride')};",
        f"    Instance->NaniteOverrideMaterial.OverrideMaterialEditor = {('nullptr' if not nanite_path else 'LoadObject<UMaterialInterface>(nullptr, ' + _cpp_text(nanite_path) + ')')};",
    ]
    verify_lines.extend([
        f"    TestEqual(TEXT(\"scalar override count\"), Instance->ScalarParameterValues.Num(), {len(scalar_values)});",
        f"    TestEqual(TEXT(\"vector override count\"), Instance->VectorParameterValues.Num(), {len(vector_values)});",
        f"    TestEqual(TEXT(\"texture override count\"), Instance->TextureParameterValues.Num(), {len(texture_values)});",
    ])

    blocker_lines = [f'#error "UERing reconstruction blocked: {item}"' for item in sorted(set(blocked))]
    build_cs = "\n".join([
        "using UnrealBuildTool;", "", f"public class {module} : ModuleRules", "{",
        f"    public {module}(ReadOnlyTargetRules Target) : base(Target)", "    {",
        "        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;",
        '        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UnrealEd", "AssetRegistry", "PhysicsCore" });',
        "    }", "}", "",
    ])
    module_cpp = "\n".join(['#include "Modules/ModuleManager.h"', "", f"IMPLEMENT_MODULE(FDefaultModuleImpl, {module});", ""])
    builder_test = "\n".join([
        '#include "AssetRegistry/AssetRegistryModule.h"', '#include "Engine/SubsurfaceProfile.h"',
        '#include "Engine/Texture.h"', '#include "Materials/MaterialInstanceConstant.h"',
        '#include "Materials/MaterialInstanceBasePropertyOverrides.h"', '#include "Misc/AutomationTest.h"',
        '#include "Misc/PackageName.h"', '#include "PhysicalMaterials/PhysicalMaterial.h"',
        '#include "StaticParameterSet.h"', '#include "UObject/Package.h"', '#include "UObject/SavePackage.h"', "",
        "#if WITH_DEV_AUTOMATION_TESTS", "namespace", "{",
        "FGuid UERingGuid(const TCHAR* Text)", "{", "    FGuid Value;", "    check(FGuid::Parse(Text, Value));", "    return Value;", "}", "}", "",
        f"IMPLEMENT_SIMPLE_AUTOMATION_TEST(F{object_name}MaterialInstanceBuildTest,",
        f"    \"UERing.Generated.MaterialInstance.{object_name}.Build\",",
        "    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)", "",
        f"bool F{object_name}MaterialInstanceBuildTest::RunTest(const FString& Parameters)", "{",
        f"    const FString TargetPackageName = {_cpp_text(asset_package)};",
        f"    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, {_cpp_text(parent_path)});",
        "    TestNotNull(TEXT(\"parent material\"), Parent);", "    if (Parent == nullptr) return false;",
        "    UPackage* Package = CreatePackage(*TargetPackageName);",
        f"    UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(Package, FName({_cpp_text(object_name)}), RF_Public | RF_Standalone);",
        "    Instance->SetParentEditorOnly(Parent);", "    {",
        "        FMaterialInstanceParameterUpdateContext UpdateContext(Instance, EMaterialInstanceClearParameterFlag::All);",
        "        FStaticParameterSet& StaticParameters = UpdateContext.GetStaticParameters();",
        *apply_lines, "    }", *uniform_lines, *tail_lines,
        "    Instance->PostEditChange();", "    Instance->MarkPackageDirty();",
        "    FAssetRegistryModule::AssetCreated(Instance);", *verify_lines,
        "    const FString Filename = FPackageName::LongPackageNameToFilename(TargetPackageName, FPackageName::GetAssetPackageExtension());",
        "    FSavePackageArgs SaveArgs;", "    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;", "    SaveArgs.SaveFlags = SAVE_NoError;",
        "    const bool bSaved = UPackage::SavePackage(Package, Instance, *Filename, SaveArgs);",
        "    TestTrue(TEXT(\"asset saved\"), bSaved);", "    return bSaved;", "}", "#endif", "",
    ])
    plugin = {
        "FileVersion": 3, "Version": 1, "VersionName": GENERATOR_VERSION,
        "FriendlyName": f"UE Ring Generated {object_name}",
        "Description": "Deterministic Unreal asset builder generated from UE Ring reconstruction IR.",
        "Category": "Generated", "CanContainContent": False, "Installed": False,
        "Modules": [{"Name": module, "Type": "Editor", "LoadingPhase": "Default"}],
    }
    files = {
        f"{module}.uplugin": json.dumps(plugin, ensure_ascii=True, indent=2) + "\n",
        f"Source/{module}/{module}.Build.cs": build_cs,
        f"Source/{module}/Private/{module}Module.cpp": module_cpp,
        f"Source/{module}/Private/{object_name}MaterialInstanceBuilderTest.cpp": builder_test,
    }
    if blocker_lines:
        files[f"Source/{module}/Private/UERingReconstructionBlockers.cpp"] = "\n".join(blocker_lines) + "\n"
    return files, sorted(executed), sorted(set(blocked))


def _cpp_text_sequence(value: str, chunk_size: int = 6000) -> str:
    parts = [_cpp_text(value[index:index + chunk_size]) for index in range(0, len(value), chunk_size)]
    return "\n        ".join(parts or [_cpp_text("")])


def _data_asset_package_namespace(properties: list[Any]) -> str:
    namespaces: set[str] = set()

    def visit(value: Any) -> None:
        if isinstance(value, dict):
            namespace = value.get("namespace")
            if isinstance(namespace, str):
                match = re.match(r"^\[([^\]]+)\]", namespace)
                if match:
                    namespaces.add(match.group(1))
            for child in value.values():
                visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)

    visit(properties)
    if len(namespaces) > 1:
        raise ReconstructionError("DataAsset FText values contain multiple package localization namespaces")
    return next(iter(namespaces), "")


def _data_asset_files(
    document: dict[str, Any],
    module: str,
    strict: bool,
    asset_package: str | None,
) -> tuple[dict[str, str], list[str], list[str]]:
    reconstruction = _require_object(document.get("reconstruction"), "reconstruction")
    targets = _require_array(reconstruction.get("targets"), "reconstruction.targets")
    target = next((item for item in targets if isinstance(item, dict)
                   and item.get("backend") == "unrealEditorCpp"
                   and item.get("target") == "editorAssetBuilderCpp"), None)
    if target is None:
        raise ReconstructionError("document has no editorAssetBuilderCpp unrealEditorCpp target")
    target_id = _require_string(target.get("id"), "target.id")
    if asset_package is None:
        raise ReconstructionError("DataAsset reconstruction requires --asset-package")
    if not GENERATED_ASSET_PACKAGE.fullmatch(asset_package):
        raise ReconstructionError("asset package must be below /Game/UERingGenerated and use identifier path segments")
    object_name = _identifier(asset_package.rsplit("/", 1)[-1], "generated asset name")

    semantics = _require_object(document.get("semantics"), "semantics")
    if semantics.get("kind") != "DataAsset" or semantics.get("representation") != "data-asset-properties-v2":
        raise ReconstructionError("DataAsset backend requires data-asset-properties-v2 semantics")
    class_path = _require_string(semantics.get("class"), "semantics.class")
    if not _is_safe_unreal_class_path(class_path):
        raise ReconstructionError("semantics.class must be a safe Unreal class object path")
    source_object_name = _identifier(
        _require_string(document.get("asset", {}).get("objectPath"), "asset.objectPath").rsplit(".", 1)[-1],
        "source DataAsset object name",
    )
    if object_name != source_object_name:
        raise ReconstructionError(
            "DataAsset target name must match the source object name to preserve PrimaryAssetId semantics"
        )
    all_properties = _require_array(semantics.get("properties"), "semantics.properties")
    properties = all_properties
    state_tree_compile = False
    policy = semantics.get("reconstructionPolicy")
    if policy is not None:
        policy = _require_object(policy, "semantics.reconstructionPolicy")
        strategy = _require_string(policy.get("strategy"), "semantics.reconstructionPolicy.strategy")
        if strategy != "state-tree-editor-compile-v1":
            raise ReconstructionError(f"unsupported DataAsset reconstruction strategy: {strategy}")
        if class_path != "/Script/StateTreeModule.StateTree":
            raise ReconstructionError("state-tree reconstruction strategy requires the native UStateTree class")
        authored_values = _require_array(
            policy.get("authoredRootProperties"),
            "semantics.reconstructionPolicy.authoredRootProperties",
        )
        derived_values = _require_array(
            policy.get("derivedRootProperties"),
            "semantics.reconstructionPolicy.derivedRootProperties",
        )
        authored_names = {
            _require_string(value, "authored root property") for value in authored_values
        }
        derived_names = {
            _require_string(value, "derived root property") for value in derived_values
        }
        if len(authored_names) != len(authored_values) or len(derived_names) != len(derived_values):
            raise ReconstructionError("StateTree reconstruction property lists contain duplicates")
        if authored_names & derived_names:
            raise ReconstructionError("StateTree authored and derived property lists overlap")
        property_by_name: dict[str, Any] = {}
        for raw_property in all_properties:
            property_entry = _require_object(raw_property, "semantics property")
            property_name = _require_string(property_entry.get("name"), "semantics property.name")
            if property_name in property_by_name:
                raise ReconstructionError(f"duplicate DataAsset property: {property_name}")
            property_by_name[property_name] = raw_property
        if authored_names | derived_names != set(property_by_name):
            raise ReconstructionError("StateTree reconstruction policy does not cover every root property exactly")
        properties = [property_by_name[name] for name in sorted(authored_names)]
        state_tree_compile = True
    owned_objects = _require_array(semantics.get("ownedObjects"), "semantics.ownedObjects")
    known_owned_ids = {"$asset"}
    for index, raw_owned in enumerate(owned_objects):
        owned = _require_object(raw_owned, f"semantics.ownedObjects[{index}]")
        owned_id = _require_string(owned.get("id"), f"semantics.ownedObjects[{index}].id")
        name = _require_string(owned.get("name"), f"semantics.ownedObjects[{index}].name")
        owned_class = _require_string(owned.get("class"), f"semantics.ownedObjects[{index}].class")
        outer_id = _require_string(owned.get("outerId"), f"semantics.ownedObjects[{index}].outerId")
        creation_method = _require_string(
            owned.get("creationMethod"), f"semantics.ownedObjects[{index}].creationMethod"
        )
        if owned_id in known_owned_ids or any(char in owned_id for char in '\r\n\t"/:'):
            raise ReconstructionError(f"owned object id is invalid or duplicated: {owned_id!r}")
        if not name or any(char in name for char in '\r\n\t"/.:'):
            raise ReconstructionError(f"owned object name is unsafe: {name!r}")
        if not _is_safe_unreal_class_path(owned_class):
            raise ReconstructionError(f"owned object class path is unsafe: {owned_class!r}")
        if outer_id not in known_owned_ids:
            raise ReconstructionError(f"owned object outer must precede the object: {outer_id!r}")
        if creation_method not in {"newObject", "findDefaultSubobject"}:
            raise ReconstructionError(f"unsupported owned object creation method: {creation_method!r}")
        _require_array(owned.get("properties"), f"semantics.ownedObjects[{index}].properties")
        known_owned_ids.add(owned_id)
    package_namespace = _data_asset_package_namespace([
        properties,
        [
            _require_array(_require_object(item, "owned object").get("properties"), "owned object.properties")
            for item in owned_objects
        ],
    ])
    primary_asset_id = semantics.get("primaryAssetId", "")
    if not isinstance(primary_asset_id, str):
        raise ReconstructionError("semantics.primaryAssetId must be a string")

    operations = _require_array(reconstruction.get("operations"), "reconstruction.operations")
    executed: list[str] = []
    blocked: list[str] = []
    seen_opcodes: set[str] = set()
    seen_ids: set[str] = set()
    for raw in operations:
        operation = _require_object(raw, "operation")
        if operation.get("targetId") != target_id:
            continue
        operation_id = _require_string(operation.get("id"), "operation.id")
        if operation_id in seen_ids:
            raise ReconstructionError(f"duplicate operation id: {operation_id}")
        seen_ids.add(operation_id)
        opcode = _require_string(operation.get("opcode"), f"{operation_id}.opcode")
        if opcode in DATA_ASSET_OPCODES:
            seen_opcodes.add(opcode)
            if operation.get("status") == "executable":
                executed.append(operation_id)
            else:
                blocked.append(operation_id)
        else:
            blocked.append(operation_id)
    required_opcodes = (
        DATA_ASSET_BASE_OPCODES
        | (DATA_ASSET_OWNED_OPCODES if owned_objects else set())
        | (STATE_TREE_OPCODES if state_tree_compile else set())
    )
    missing = required_opcodes - seen_opcodes
    if missing:
        raise ReconstructionError("DataAsset IR is missing executable opcodes: " + ", ".join(sorted(missing)))
    if strict and blocked:
        raise ReconstructionError("strict reconstruction blocked by operations: " + ", ".join(sorted(blocked)))

    blocker_lines = [
        f'#error "UERing reconstruction blocked: {item}"'
        for item in sorted(set(blocked))
    ]
    build_cs = "\n".join([
        "using UnrealBuildTool;", "", f"public class {module} : ModuleRules", "{",
        f"    public {module}(ReadOnlyTargetRules Target) : base(Target)", "    {",
        "        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;",
        '        PrivateDependencyModuleNames.AddRange(new string[] { '
        + ('"AssetRegistry", "Core", "CoreUObject", "Engine", "Json", "PropertyBindingUtils", "StateTreeEditorModule", "StateTreeModule", "UnrealEd"'
           if state_tree_compile else '"AssetRegistry", "Core", "CoreUObject", "Engine", "Json", "UnrealEd"')
        + ' });',
        "    }", "}", "",
    ])
    module_cpp = "\n".join([
        '#include "Modules/ModuleManager.h"', "", f"IMPLEMENT_MODULE(FDefaultModuleImpl, {module});", "",
    ])
    properties_json = json.dumps(properties, ensure_ascii=True, separators=(",", ":"), sort_keys=True)
    owned_objects_json = json.dumps(owned_objects, ensure_ascii=True, separators=(",", ":"), sort_keys=True)
    builder_test = r'''#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/DataAsset.h"
#include "Internationalization/Text.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
__STATE_TREE_INCLUDES__
#include "UObject/AnsiStrProperty.h"
#include "UObject/Package.h"
#include "UObject/PropertyOptional.h"
#include "UObject/SavePackage.h"
#include "UObject/ScriptDelegates.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Utf8StrProperty.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    bool Failure(const FProperty& Property, const FString& Detail, FString& OutError)
    {
        OutError = Property.GetPathName() + TEXT(": ") + Detail;
        return false;
    }

    bool ReadEnumNumber(
        const UEnum& Enum,
        const TSharedPtr<FJsonValue>& Value,
        int64& OutNumber,
        FString& OutError)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
        {
            OutError = TEXT("enum value must be an object");
            return false;
        }
        FString EnumPath;
        FString EnumName;
        double Number = 0.0;
        if (!(*Object)->TryGetStringField(TEXT("enum"), EnumPath)
            || EnumPath != Enum.GetPathName()
            || !(*Object)->TryGetStringField(TEXT("name"), EnumName)
            || !(*Object)->TryGetNumberField(TEXT("value"), Number)
            || !FMath::IsFinite(Number)
            || Number != FMath::RoundToDouble(Number)
            || FMath::Abs(Number) > 9007199254740991.0)
        {
            OutError = TEXT("enum identity or numeric value is invalid");
            return false;
        }
        OutNumber = static_cast<int64>(Number);
        if (Enum.GetAuthoredNameStringByValue(OutNumber) != EnumName)
        {
            OutError = TEXT("enum name does not match its numeric value");
            return false;
        }
        return true;
    }

    bool ApplyProperty(
        const FProperty& Property,
        void* Container,
        const TSharedPtr<FJsonValue>& Value,
        UObject* Owner,
        const TMap<FString, UObject*>& OwnedObjects,
        FString& OutError);

    bool ApplyPropertyBag(
        const FProperty& Property,
        void* ValuePtr,
        const TSharedPtr<FJsonValue>& Value,
        UObject* Owner,
        const TMap<FString, UObject*>& OwnedObjects,
        FString& OutError)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Properties = nullptr;
        double Version = 0.0;
        bool bIsValid = false;
        if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr
            || !(*Object)->TryGetNumberField(TEXT("propertyBagVersion"), Version)
            || Version != 1.0
            || !(*Object)->TryGetBoolField(TEXT("isValid"), bIsValid)
            || !(*Object)->TryGetArrayField(TEXT("properties"), Properties)
            || Properties == nullptr)
        {
            return Failure(Property, TEXT("property bag JSON value is invalid"), OutError);
        }

        FInstancedPropertyBag& Bag = *static_cast<FInstancedPropertyBag*>(ValuePtr);
        if (!bIsValid)
        {
            if (!Properties->IsEmpty())
            {
                return Failure(Property, TEXT("invalid property bag cannot contain properties"), OutError);
            }
            Bag.Reset();
            return true;
        }

        FString LayoutId;
        if (!(*Object)->TryGetStringField(TEXT("layoutId"), LayoutId) || LayoutId.IsEmpty())
        {
            return Failure(Property, TEXT("property bag layout id is invalid"), OutError);
        }

        const UEnum* ValueTypeEnum = StaticEnum<EPropertyBagPropertyType>();
        const UEnum* ContainerTypeEnum = StaticEnum<EPropertyBagContainerType>();
        TArray<FPropertyBagPropertyDesc> Descs;
        TArray<TSharedPtr<FJsonObject>> Entries;
        Descs.Reserve(Properties->Num());
        Entries.Reserve(Properties->Num());
        for (const TSharedPtr<FJsonValue>& EntryValue : *Properties)
        {
            const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
            FString Id;
            FString Name;
            FString ValueTypeName;
            FString KeyTypeName;
            FString PropertyFlags;
            const TArray<TSharedPtr<FJsonValue>>* ContainerTypes = nullptr;
            if (!EntryValue.IsValid() || !EntryValue->TryGetObject(EntryPtr) || EntryPtr == nullptr
                || !(*EntryPtr)->TryGetStringField(TEXT("id"), Id)
                || !(*EntryPtr)->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()
                || !(*EntryPtr)->TryGetStringField(TEXT("valueType"), ValueTypeName)
                || !(*EntryPtr)->TryGetStringField(TEXT("keyType"), KeyTypeName)
                || !(*EntryPtr)->TryGetStringField(TEXT("propertyFlags"), PropertyFlags)
                || !(*EntryPtr)->TryGetArrayField(TEXT("containerTypes"), ContainerTypes)
                || ContainerTypes == nullptr
                || !(*EntryPtr)->HasField(TEXT("value")))
            {
                return Failure(Property, TEXT("property bag descriptor is invalid"), OutError);
            }
            const TSharedPtr<FJsonObject> Entry = *EntryPtr;

            FGuid Guid;
            uint64 Flags = 0;
            const int64 ValueTypeValue = ValueTypeEnum != nullptr
                ? ValueTypeEnum->GetValueByNameString(ValueTypeName)
                : INDEX_NONE;
            const int64 KeyTypeValue = ValueTypeEnum != nullptr
                ? ValueTypeEnum->GetValueByNameString(KeyTypeName)
                : INDEX_NONE;
            if (!FGuid::Parse(Id, Guid)
                || ValueTypeValue <= static_cast<int64>(EPropertyBagPropertyType::None)
                || ValueTypeValue >= static_cast<int64>(EPropertyBagPropertyType::Count)
                || KeyTypeValue < static_cast<int64>(EPropertyBagPropertyType::None)
                || KeyTypeValue >= static_cast<int64>(EPropertyBagPropertyType::Count)
                || !LexTryParseString(Flags, *PropertyFlags))
            {
                return Failure(Property, TEXT("property bag descriptor identity is invalid"), OutError);
            }

            FPropertyBagContainerTypes ParsedContainerTypes;
            if (ContainerTypes->Num() > 2)
            {
                return Failure(Property, TEXT("property bag container nesting is invalid"), OutError);
            }
            for (const TSharedPtr<FJsonValue>& ContainerValue : *ContainerTypes)
            {
                FString ContainerName;
                const int64 ContainerTypeValue =
                    ContainerValue.IsValid() && ContainerValue->TryGetString(ContainerName)
                        && ContainerTypeEnum != nullptr
                    ? ContainerTypeEnum->GetValueByNameString(ContainerName)
                    : INDEX_NONE;
                if (ContainerTypeValue <= static_cast<int64>(EPropertyBagContainerType::None)
                    || ContainerTypeValue >= static_cast<int64>(EPropertyBagContainerType::Count)
                    || !ParsedContainerTypes.Add(
                        static_cast<EPropertyBagContainerType>(ContainerTypeValue)))
                {
                    return Failure(Property, TEXT("property bag container type is invalid"), OutError);
                }
            }

            const auto LoadTypeObject = [&](const TCHAR* Field, const UObject*& OutObject) -> bool
            {
                OutObject = nullptr;
                const TSharedPtr<FJsonObject>* Reference = nullptr;
                if (!Entry->TryGetObjectField(Field, Reference) || Reference == nullptr)
                {
                    return true;
                }
                FString ObjectPath;
                if (!(*Reference)->TryGetStringField(TEXT("objectPath"), ObjectPath)
                    || ObjectPath.IsEmpty())
                {
                    return false;
                }
                OutObject = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
                return OutObject != nullptr;
            };

            const UObject* ValueTypeObject = nullptr;
            const UObject* KeyTypeObject = nullptr;
            const UObject* MetaClassObject = nullptr;
            if (!LoadTypeObject(TEXT("valueTypeObject"), ValueTypeObject)
                || !LoadTypeObject(TEXT("keyTypeObject"), KeyTypeObject)
                || !LoadTypeObject(TEXT("metaClass"), MetaClassObject))
            {
                return Failure(Property, TEXT("property bag type object could not be loaded"), OutError);
            }

            FPropertyBagPropertyDesc Desc;
            Desc.ID = Guid;
            Desc.Name = FName(*Name);
            Desc.ValueType = static_cast<EPropertyBagPropertyType>(ValueTypeValue);
            Desc.ContainerTypes = ParsedContainerTypes;
            Desc.PropertyFlags = Flags;
            Desc.KeyType = static_cast<EPropertyBagPropertyType>(KeyTypeValue);
            Desc.ValueTypeObject = ValueTypeObject;
            Desc.KeyTypeObject = KeyTypeObject;
#if WITH_EDITORONLY_DATA
            const TArray<TSharedPtr<FJsonValue>>* MetaData = nullptr;
            if (Entry->TryGetArrayField(TEXT("metadata"), MetaData) && MetaData != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& MetaValue : *MetaData)
                {
                    const TSharedPtr<FJsonObject>* Meta = nullptr;
                    FString Key;
                    FString MetaText;
                    if (!MetaValue.IsValid() || !MetaValue->TryGetObject(Meta) || Meta == nullptr
                        || !(*Meta)->TryGetStringField(TEXT("key"), Key)
                        || !(*Meta)->TryGetStringField(TEXT("value"), MetaText))
                    {
                        return Failure(Property, TEXT("property bag metadata is invalid"), OutError);
                    }
                    Desc.MetaData.Emplace(FName(*Key), MetaText);
                }
            }
            Desc.MetaClass = Cast<UClass>(const_cast<UObject*>(MetaClassObject));
            if (MetaClassObject != nullptr && Desc.MetaClass == nullptr)
            {
                return Failure(Property, TEXT("property bag meta class is invalid"), OutError);
            }
#endif
            Descs.Add(MoveTemp(Desc));
            Entries.Add(Entry);
        }

        const UPropertyBag* BagStruct = UPropertyBag::GetOrCreateFromDescs(Descs);
        if (BagStruct == nullptr
            || BagStruct->GetName() != LayoutId
            || BagStruct->GetPropertyDescs().Num() != Descs.Num())
        {
            return Failure(Property, TEXT("property bag layout does not match descriptors"), OutError);
        }
        Bag.InitializeFromBagStruct(BagStruct);
        FStructView BagValue = Bag.GetMutableValue();
        if (BagValue.GetMemory() == nullptr)
        {
            return Failure(Property, TEXT("property bag storage is unavailable"), OutError);
        }
        for (int32 Index = 0; Index < Entries.Num(); ++Index)
        {
            const FPropertyBagPropertyDesc& Desc = BagStruct->GetPropertyDescs()[Index];
            const FProperty* DynamicProperty = nullptr;
            for (TFieldIterator<FProperty> It(BagStruct); It; ++It)
            {
                if (BagStruct->FindPropertyDescByProperty(*It) == &Desc)
                {
                    DynamicProperty = *It;
                    break;
                }
            }
            if (DynamicProperty == nullptr
                || !ApplyProperty(
                    *DynamicProperty,
                    BagValue.GetMemory(),
                    Entries[Index]->Values.FindRef(TEXT("value")),
                    Owner,
                    OwnedObjects,
                    OutError))
            {
                return DynamicProperty == nullptr
                    ? Failure(Property, TEXT("property bag dynamic property is unavailable"), OutError)
                    : false;
            }
        }
        return true;
    }

    bool ApplyInstancedStruct(
        const FProperty& Property,
        void* ValuePtr,
        const TSharedPtr<FJsonValue>& Value,
        UObject* Owner,
        const TMap<FString, UObject*>& OwnedObjects,
        FString& OutError)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        const TSharedPtr<FJsonObject>* Fields = nullptr;
        double Version = 0.0;
        bool bIsValid = false;
        if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr
            || !(*Object)->TryGetNumberField(TEXT("instancedStructVersion"), Version)
            || Version != 1.0
            || !(*Object)->TryGetBoolField(TEXT("isValid"), bIsValid)
            || !(*Object)->TryGetObjectField(TEXT("fields"), Fields)
            || Fields == nullptr)
        {
            return Failure(Property, TEXT("instanced struct JSON value is invalid"), OutError);
        }

        FInstancedStruct& Instanced = *static_cast<FInstancedStruct*>(ValuePtr);
        if (!bIsValid)
        {
            if (!(*Fields)->Values.IsEmpty() || (*Object)->HasField(TEXT("valueStruct")))
            {
                return Failure(Property, TEXT("invalid instanced struct cannot contain a type or fields"), OutError);
            }
            Instanced.Reset();
            return true;
        }

        FString ValueStructPath;
        if (!(*Object)->TryGetStringField(TEXT("valueStruct"), ValueStructPath)
            || ValueStructPath.IsEmpty())
        {
            return Failure(Property, TEXT("instanced struct type is invalid"), OutError);
        }
        const UScriptStruct* ValueStruct = LoadObject<UScriptStruct>(nullptr, *ValueStructPath);
        if (ValueStruct == nullptr)
        {
            return Failure(Property, TEXT("instanced struct type could not be loaded: ") + ValueStructPath, OutError);
        }
        Instanced.InitializeAs(ValueStruct);
        if (!Instanced.IsValid() || Instanced.GetMutableMemory() == nullptr)
        {
            return Failure(Property, TEXT("instanced struct storage could not be initialized"), OutError);
        }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Fields)->Values)
        {
            const FProperty* Field = FindFProperty<FProperty>(ValueStruct, *Pair.Key);
            if (Field == nullptr)
            {
                return Failure(Property, TEXT("instanced struct field is unavailable: ") + Pair.Key, OutError);
            }
            if (!ApplyProperty(
                *Field,
                Instanced.GetMutableMemory(),
                Pair.Value,
                Owner,
                OwnedObjects,
                OutError))
            {
                return false;
            }
        }
        return true;
    }

    bool ApplyValue(
        const FProperty& Property,
        void* ValuePtr,
        const TSharedPtr<FJsonValue>& Value,
        UObject* Owner,
        const TMap<FString, UObject*>& OwnedObjects,
        FString& OutError)
    {
        if (const FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(&Property))
        {
            const TSharedPtr<FJsonObject>* Object = nullptr;
            bool bIsSet = false;
            if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr
                || !(*Object)->TryGetBoolField(TEXT("isSet"), bIsSet)
                || (*Object)->Values.Num() != (bIsSet ? 2 : 1)
                || (bIsSet && !(*Object)->HasField(TEXT("value"))))
            {
                return Failure(Property, TEXT("optional JSON value is invalid"), OutError);
            }
            if (!bIsSet)
            {
                OptionalProperty->MarkUnset(ValuePtr);
                return true;
            }
            void* OptionalValue = OptionalProperty->MarkSetAndGetInitializedValuePointerToReplace(ValuePtr);
            return ApplyValue(
                *OptionalProperty->GetValueProperty(),
                OptionalValue,
                (*Object)->Values.FindRef(TEXT("value")),
                Owner,
                OwnedObjects,
                OutError);
        }
        if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
        {
            int64 Number = 0;
            if (!ReadEnumNumber(*EnumProperty->GetEnum(), Value, Number, OutError))
            {
                return Failure(Property, OutError, OutError);
            }
            EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, Number);
            return true;
        }
        if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(&Property))
        {
            if (const UEnum* Enum = NumericProperty->GetIntPropertyEnum())
            {
                int64 Number = 0;
                if (!ReadEnumNumber(*Enum, Value, Number, OutError))
                {
                    return Failure(Property, OutError, OutError);
                }
                if (!NumericProperty->CanHoldValue(Number)) return Failure(Property, TEXT("enum value is out of range"), OutError);
                NumericProperty->SetIntPropertyValue(ValuePtr, Number);
                return true;
            }
            double Number = 0.0;
            if (!Value.IsValid() || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number))
            {
                return Failure(Property, TEXT("numeric JSON value is invalid"), OutError);
            }
            if (NumericProperty->IsFloatingPoint())
            {
                if (!NumericProperty->CanHoldValue(Number)) return Failure(Property, TEXT("floating value is out of range"), OutError);
                NumericProperty->SetFloatingPointPropertyValue(ValuePtr, Number);
                return true;
            }
            if (Number != FMath::RoundToDouble(Number) || FMath::Abs(Number) > 9007199254740991.0)
            {
                return Failure(Property, TEXT("integer JSON value is not exact"), OutError);
            }
            const int64 Integer = static_cast<int64>(Number);
            if (!NumericProperty->CanHoldValue(Integer)) return Failure(Property, TEXT("integer value is out of range"), OutError);
            NumericProperty->SetIntPropertyValue(ValuePtr, Integer);
            return true;
        }
        if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(&Property))
        {
            bool Boolean = false;
            if (!Value.IsValid() || !Value->TryGetBool(Boolean)) return Failure(Property, TEXT("boolean JSON value is invalid"), OutError);
            BoolProperty->SetPropertyValue(ValuePtr, Boolean);
            return true;
        }
        if (const FStrProperty* StringProperty = CastField<FStrProperty>(&Property))
        {
            FString String;
            if (!Value.IsValid() || !Value->TryGetString(String)) return Failure(Property, TEXT("string JSON value is invalid"), OutError);
            StringProperty->SetPropertyValue(ValuePtr, String);
            return true;
        }
        if (const FUtf8StrProperty* StringProperty = CastField<FUtf8StrProperty>(&Property))
        {
            FString String;
            if (!Value.IsValid() || !Value->TryGetString(String)) return Failure(Property, TEXT("UTF-8 string JSON value is invalid"), OutError);
            StringProperty->SetPropertyValue(ValuePtr, FUtf8String(String));
            return true;
        }
        if (const FAnsiStrProperty* StringProperty = CastField<FAnsiStrProperty>(&Property))
        {
            FString String;
            if (!Value.IsValid() || !Value->TryGetString(String)) return Failure(Property, TEXT("ANSI string JSON value is invalid"), OutError);
            StringProperty->SetPropertyValue(ValuePtr, FAnsiString(String));
            return true;
        }
        if (const FNameProperty* NameProperty = CastField<FNameProperty>(&Property))
        {
            FString String;
            if (!Value.IsValid() || !Value->TryGetString(String)) return Failure(Property, TEXT("name JSON value is invalid"), OutError);
            NameProperty->SetPropertyValue(ValuePtr, FName(*String));
            return true;
        }
        if (const FTextProperty* TextProperty = CastField<FTextProperty>(&Property))
        {
            const TSharedPtr<FJsonObject>* Object = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
            {
                return Failure(Property, TEXT("text JSON value is invalid"), OutError);
            }
            FString Source;
            FString Namespace;
            FString Key;
            (*Object)->TryGetStringField(TEXT("source"), Source);
            (*Object)->TryGetStringField(TEXT("namespace"), Namespace);
            (*Object)->TryGetStringField(TEXT("key"), Key);
            FText Text = Source.IsEmpty() ? FText::GetEmpty() : FText::FromString(Source);
            if (!Namespace.IsEmpty() || !Key.IsEmpty())
            {
                Text = FText::ChangeKey(FTextKey(Namespace), FTextKey(Key), Text);
            }
            TextProperty->SetPropertyValue(ValuePtr, Text);
            return true;
        }
        if (const FDelegateProperty* DelegateProperty = CastField<FDelegateProperty>(&Property))
        {
            const TSharedPtr<FJsonObject>* Object = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
            FString Kind;
            FString Signature;
            const FString ExpectedSignature = DelegateProperty->SignatureFunction != nullptr
                ? DelegateProperty->SignatureFunction->GetPathName()
                : FString();
            if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr
                || !(*Object)->TryGetStringField(TEXT("delegateKind"), Kind)
                || Kind != TEXT("single")
                || !(*Object)->TryGetStringField(TEXT("signature"), Signature)
                || Signature != ExpectedSignature
                || !(*Object)->TryGetArrayField(TEXT("bindings"), Bindings)
                || Bindings == nullptr)
            {
                return Failure(Property, TEXT("single delegate JSON value is invalid"), OutError);
            }
            if (!Bindings->IsEmpty())
            {
                return Failure(Property, TEXT("bound single delegates are unsupported"), OutError);
            }
            DelegateProperty->SetPropertyValue(ValuePtr, FScriptDelegate());
            return true;
        }
        if (const FMulticastDelegateProperty* DelegateProperty =
            CastField<FMulticastDelegateProperty>(&Property))
        {
            const TSharedPtr<FJsonObject>* Object = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
            FString Kind;
            FString Signature;
            double UnresolvedBindingCount = 0.0;
            const FString ExpectedSignature = DelegateProperty->SignatureFunction != nullptr
                ? DelegateProperty->SignatureFunction->GetPathName()
                : FString();
            if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr
                || !(*Object)->TryGetStringField(TEXT("delegateKind"), Kind)
                || Kind != TEXT("multicast")
                || !(*Object)->TryGetStringField(TEXT("signature"), Signature)
                || Signature != ExpectedSignature
                || !(*Object)->TryGetArrayField(TEXT("bindings"), Bindings)
                || Bindings == nullptr)
            {
                return Failure(Property, TEXT("multicast delegate JSON value is invalid"), OutError);
            }
            if (!Bindings->IsEmpty()
                || ((*Object)->TryGetNumberField(TEXT("unresolvedBindingCount"), UnresolvedBindingCount)
                    && UnresolvedBindingCount > 0.0))
            {
                return Failure(Property, TEXT("bound multicast delegates are unsupported"), OutError);
            }
            DelegateProperty->ClearDelegate(Owner, ValuePtr);
            return true;
        }
        if (const FSoftObjectProperty* SoftProperty = CastField<FSoftObjectProperty>(&Property))
        {
            const TSharedPtr<FJsonObject>* Object = nullptr;
            FString ObjectPath;
            FString OwnedObjectId;
            if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
            {
                return Failure(Property, TEXT("soft object reference is invalid"), OutError);
            }
            const bool bHasObjectPath = (*Object)->TryGetStringField(TEXT("objectPath"), ObjectPath);
            const bool bHasOwnedObjectId = (*Object)->TryGetStringField(TEXT("ownedObjectId"), OwnedObjectId);
            if (bHasObjectPath == bHasOwnedObjectId)
            {
                return Failure(Property, TEXT("soft object reference identity is invalid"), OutError);
            }
            if (bHasOwnedObjectId)
            {
                UObject* const* Referenced = OwnedObjects.Find(OwnedObjectId);
                if (Referenced == nullptr || *Referenced == nullptr
                    || !(*Referenced)->IsA(SoftProperty->PropertyClass))
                {
                    return Failure(Property, TEXT("owned soft object is unavailable: ") + OwnedObjectId, OutError);
                }
                SoftProperty->SetPropertyValue(ValuePtr, FSoftObjectPtr(*Referenced));
                return true;
            }
            SoftProperty->SetPropertyValue(ValuePtr, FSoftObjectPtr(FSoftObjectPath(ObjectPath)));
            return true;
        }
        if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(&Property))
        {
            const TSharedPtr<FJsonObject>* Object = nullptr;
            FString ObjectPath;
            FString OwnedObjectId;
            if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr)
            {
                return Failure(Property, TEXT("object reference is invalid"), OutError);
            }
            const bool bHasObjectPath = (*Object)->TryGetStringField(TEXT("objectPath"), ObjectPath);
            const bool bHasOwnedObjectId = (*Object)->TryGetStringField(TEXT("ownedObjectId"), OwnedObjectId);
            if (bHasObjectPath == bHasOwnedObjectId)
            {
                return Failure(Property, TEXT("object reference identity is invalid"), OutError);
            }
            UObject* Referenced = nullptr;
            if (bHasOwnedObjectId)
            {
                Referenced = OwnedObjects.FindRef(OwnedObjectId);
                if (Referenced == nullptr)
                {
                    return Failure(Property, TEXT("owned object is unavailable: ") + OwnedObjectId, OutError);
                }
            }
            else if (!ObjectPath.IsEmpty())
            {
                Referenced = StaticLoadObject(ObjectProperty->PropertyClass, nullptr, *ObjectPath);
                if (Referenced == nullptr) return Failure(Property, TEXT("referenced object could not be loaded: ") + ObjectPath, OutError);
            }
            if (Referenced != nullptr)
            {
                if (!Referenced->IsA(ObjectProperty->PropertyClass))
                {
                    return Failure(Property, TEXT("referenced object violates PropertyClass"), OutError);
                }
                if (const FClassProperty* ClassProperty = CastField<FClassProperty>(&Property))
                {
                    const UClass* ReferencedClass = Cast<UClass>(Referenced);
                    if (ReferencedClass == nullptr || !ReferencedClass->IsChildOf(ClassProperty->MetaClass))
                    {
                        return Failure(Property, TEXT("referenced class violates MetaClass"), OutError);
                    }
                }
            }
            ObjectProperty->SetObjectPropertyValue(ValuePtr, Referenced);
            return true;
        }
        if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Value.IsValid() || !Value->TryGetArray(Values) || Values == nullptr)
            {
                return Failure(Property, TEXT("array JSON value is invalid"), OutError);
            }
            FScriptArrayHelper Helper(ArrayProperty, ValuePtr);
            Helper.Resize(Values->Num());
            for (int32 Index = 0; Index < Values->Num(); ++Index)
            {
                if (!ApplyValue(*ArrayProperty->Inner, Helper.GetRawPtr(Index), (*Values)[Index], Owner, OwnedObjects, OutError)) return false;
            }
            return true;
        }
        if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Value.IsValid() || !Value->TryGetArray(Values) || Values == nullptr)
            {
                return Failure(Property, TEXT("set JSON value is invalid"), OutError);
            }
            FScriptSetHelper Helper(SetProperty, ValuePtr);
            Helper.EmptyElements(Values->Num());
            for (const TSharedPtr<FJsonValue>& Element : *Values)
            {
                const int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
                if (!ApplyValue(*SetProperty->ElementProp, Helper.GetElementPtr(Index), Element, Owner, OwnedObjects, OutError)) return false;
            }
            Helper.Rehash();
            return true;
        }
        if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
        {
            const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
            if (!Value.IsValid() || !Value->TryGetArray(Entries) || Entries == nullptr)
            {
                return Failure(Property, TEXT("map JSON value is invalid"), OutError);
            }
            FScriptMapHelper Helper(MapProperty, ValuePtr);
            Helper.EmptyValues(Entries->Num());
            for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
            {
                const TSharedPtr<FJsonObject>* Entry = nullptr;
                if (!EntryValue.IsValid() || !EntryValue->TryGetObject(Entry) || Entry == nullptr)
                {
                    return Failure(Property, TEXT("map entry is invalid"), OutError);
                }
                const int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
                if (!ApplyValue(*MapProperty->KeyProp, Helper.GetKeyPtr(Index), (*Entry)->Values.FindRef(TEXT("key")), Owner, OwnedObjects, OutError)
                    || !ApplyValue(*MapProperty->ValueProp, Helper.GetValuePtr(Index), (*Entry)->Values.FindRef(TEXT("value")), Owner, OwnedObjects, OutError))
                {
                    return false;
                }
            }
            Helper.Rehash();
            return true;
        }
        if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
        {
            if (StructProperty->Struct == FInstancedStruct::StaticStruct())
            {
                return ApplyInstancedStruct(
                    Property, ValuePtr, Value, Owner, OwnedObjects, OutError);
            }
            if (StructProperty->Struct == FInstancedPropertyBag::StaticStruct())
            {
                return ApplyPropertyBag(
                    Property, ValuePtr, Value, Owner, OwnedObjects, OutError);
            }
            const TSharedPtr<FJsonObject>* Object = nullptr;
            const TSharedPtr<FJsonObject>* Fields = nullptr;
            FString StructType;
            if (!Value.IsValid() || !Value->TryGetObject(Object) || Object == nullptr
                || !(*Object)->TryGetStringField(TEXT("structType"), StructType)
                || StructType != StructProperty->Struct->GetPathName()
                || !(*Object)->TryGetObjectField(TEXT("fields"), Fields)
                || Fields == nullptr)
            {
                return Failure(Property, TEXT("struct JSON value is invalid"), OutError);
            }
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Fields)->Values)
            {
                const FProperty* Field = FindFProperty<FProperty>(StructProperty->Struct, *Pair.Key);
                if (Field == nullptr) return Failure(Property, TEXT("struct field is unavailable: ") + Pair.Key, OutError);
                if (!ApplyProperty(*Field, ValuePtr, Pair.Value, Owner, OwnedObjects, OutError)) return false;
            }
            return true;
        }
        return Failure(Property, TEXT("property class is unsupported"), OutError);
    }

    bool ApplyProperty(
        const FProperty& Property,
        void* Container,
        const TSharedPtr<FJsonValue>& Value,
        UObject* Owner,
        const TMap<FString, UObject*>& OwnedObjects,
        FString& OutError)
    {
        if (Property.ArrayDim <= 1)
        {
            return ApplyValue(Property, Property.ContainerPtrToValuePtr<void>(Container), Value, Owner, OwnedObjects, OutError);
        }
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Value.IsValid() || !Value->TryGetArray(Values) || Values == nullptr || Values->Num() != Property.ArrayDim)
        {
            return Failure(Property, TEXT("fixed array JSON value is invalid"), OutError);
        }
        for (int32 Index = 0; Index < Property.ArrayDim; ++Index)
        {
            if (!ApplyValue(Property, Property.ContainerPtrToValuePtr<void>(Container, Index), (*Values)[Index], Owner, OwnedObjects, OutError)) return false;
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(F__OBJECT_NAME__DataAssetBuildTest,
    "UERing.Generated.DataAsset.__OBJECT_NAME__.Build",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool F__OBJECT_NAME__DataAssetBuildTest::RunTest(const FString& Parameters)
{
    const FString TargetPackageName = __ASSET_PACKAGE__;
    UClass* AssetClass = LoadObject<UClass>(nullptr, __CLASS_PATH__);
    TestNotNull(TEXT("DataAsset class"), AssetClass);
    if (AssetClass == nullptr || !AssetClass->IsChildOf(UDataAsset::StaticClass())) return false;

    TArray<TSharedPtr<FJsonValue>> SerializedProperties;
    const FString PropertiesJson =
        __PROPERTIES_JSON__;
    const TSharedRef<TJsonReader<>> PropertiesReader = TJsonReaderFactory<>::Create(PropertiesJson);
    TestTrue(TEXT("serialized properties parse"), FJsonSerializer::Deserialize(PropertiesReader, SerializedProperties));

    TArray<TSharedPtr<FJsonValue>> SerializedOwnedObjects;
    const FString OwnedObjectsJson =
        __OWNED_OBJECTS_JSON__;
    const TSharedRef<TJsonReader<>> OwnedObjectsReader = TJsonReaderFactory<>::Create(OwnedObjectsJson);
    TestTrue(TEXT("serialized owned objects parse"), FJsonSerializer::Deserialize(OwnedObjectsReader, SerializedOwnedObjects));

    UPackage* Package = CreatePackage(*TargetPackageName);
    __PACKAGE_NAMESPACE_SETUP__
    UObject* Asset = NewObject<UObject>(Package, AssetClass, FName(__OBJECT_NAME_TEXT__), RF_Public | RF_Standalone);
    TestNotNull(TEXT("generated DataAsset"), Asset);
    if (Asset == nullptr) return false;
    Asset->Modify();

    TMap<FString, UObject*> OwnedObjects;
    OwnedObjects.Add(TEXT("$asset"), Asset);
    for (const TSharedPtr<FJsonValue>& EntryValue : SerializedOwnedObjects)
    {
        const TSharedPtr<FJsonObject>* Entry = nullptr;
        FString Id;
        FString Name;
        FString ClassPath;
        FString OuterId;
        FString CreationMethod;
        if (!EntryValue.IsValid() || !EntryValue->TryGetObject(Entry) || Entry == nullptr
            || !(*Entry)->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty()
            || !(*Entry)->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()
            || !(*Entry)->TryGetStringField(TEXT("class"), ClassPath)
            || !(*Entry)->TryGetStringField(TEXT("outerId"), OuterId)
            || !(*Entry)->TryGetStringField(TEXT("creationMethod"), CreationMethod)
            || OwnedObjects.Contains(Id))
        {
            AddError(TEXT("serialized owned object entry is invalid"));
            return false;
        }
        UObject* Outer = OuterId == TEXT("$asset") ? Asset : OwnedObjects.FindRef(OuterId);
        UClass* OwnedClass = LoadObject<UClass>(nullptr, *ClassPath);
        if (Outer == nullptr || OwnedClass == nullptr)
        {
            AddError(TEXT("owned object outer or class is unavailable: ") + Id);
            return false;
        }

        UObject* Object = nullptr;
        if (CreationMethod == TEXT("findDefaultSubobject"))
        {
            Object = StaticFindObjectFast(
                OwnedClass, Outer, FName(*Name), EFindObjectFlags::ExactClass);
        }
        else if (CreationMethod == TEXT("newObject") && !OwnedClass->HasAnyClassFlags(CLASS_Abstract))
        {
            Object = NewObject<UObject>(Outer, OwnedClass, FName(*Name), RF_Transactional);
        }
        if (Object == nullptr || Object->GetOuter() != Outer || Object->GetClass() != OwnedClass)
        {
            AddError(TEXT("owned object could not be created exactly: ") + Id);
            return false;
        }
        OwnedObjects.Add(Id, Object);
    }

    int32 AppliedOwnedPropertyCount = 0;
    int32 ExpectedOwnedPropertyCount = 0;
    for (const TSharedPtr<FJsonValue>& EntryValue : SerializedOwnedObjects)
    {
        const TSharedPtr<FJsonObject>* Entry = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* ObjectProperties = nullptr;
        FString Id;
        if (!EntryValue.IsValid() || !EntryValue->TryGetObject(Entry) || Entry == nullptr
            || !(*Entry)->TryGetStringField(TEXT("id"), Id)
            || !(*Entry)->TryGetArrayField(TEXT("properties"), ObjectProperties)
            || ObjectProperties == nullptr)
        {
            AddError(TEXT("serialized owned object properties are invalid"));
            return false;
        }
        UObject* Object = OwnedObjects.FindRef(Id);
        if (Object == nullptr)
        {
            AddError(TEXT("owned object is unavailable during property application: ") + Id);
            return false;
        }
        Object->Modify();
        ExpectedOwnedPropertyCount += ObjectProperties->Num();
        for (const TSharedPtr<FJsonValue>& PropertyValue : *ObjectProperties)
        {
            const TSharedPtr<FJsonObject>* PropertyEntry = nullptr;
            FString Name;
            if (!PropertyValue.IsValid() || !PropertyValue->TryGetObject(PropertyEntry)
                || PropertyEntry == nullptr
                || !(*PropertyEntry)->TryGetStringField(TEXT("name"), Name))
            {
                AddError(TEXT("serialized owned property entry is invalid: ") + Id);
                return false;
            }
            const FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), *Name);
            FString Error;
            if (Property == nullptr
                || !ApplyProperty(
                    *Property,
                    Object,
                    (*PropertyEntry)->Values.FindRef(TEXT("value")),
                    Object,
                    OwnedObjects,
                    Error))
            {
                AddError(Property == nullptr
                    ? TEXT("owned reflected property is unavailable: ") + Id + TEXT(".") + Name
                    : Error);
                return false;
            }
            ++AppliedOwnedPropertyCount;
        }
        Object->PostEditChange();
    }

    int32 AppliedPropertyCount = 0;
    for (const TSharedPtr<FJsonValue>& EntryValue : SerializedProperties)
    {
        const TSharedPtr<FJsonObject>* Entry = nullptr;
        FString Name;
        if (!EntryValue.IsValid() || !EntryValue->TryGetObject(Entry) || Entry == nullptr
            || !(*Entry)->TryGetStringField(TEXT("name"), Name))
        {
            AddError(TEXT("serialized property entry is invalid"));
            return false;
        }
        const FProperty* Property = FindFProperty<FProperty>(AssetClass, *Name);
        if (Property == nullptr)
        {
            AddError(TEXT("reflected property is unavailable: ") + Name);
            return false;
        }
        FString Error;
        if (!ApplyProperty(
            *Property,
            Asset,
            (*Entry)->Values.FindRef(TEXT("value")),
            Asset,
            OwnedObjects,
            Error))
        {
            AddError(Error);
            return false;
        }
        ++AppliedPropertyCount;
    }
    TestEqual(TEXT("applied property count"), AppliedPropertyCount, SerializedProperties.Num());
    TestTrue(TEXT("owned objects created"), OwnedObjects.Num() == SerializedOwnedObjects.Num() + 1);
    TestEqual(TEXT("owned properties applied"), AppliedOwnedPropertyCount, ExpectedOwnedPropertyCount);
    TestEqual(TEXT("PrimaryAssetId"), Asset->GetPrimaryAssetId().ToString(), FString(__PRIMARY_ASSET_ID__));

    Asset->PostEditChange();
    __STATE_TREE_COMPILE__
    Asset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Asset);
    const FString Filename = FPackageName::LongPackageNameToFilename(
        TargetPackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    const bool bSaved = UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
    TestTrue(TEXT("asset saved"), bSaved);
    return bSaved;
}
#endif
'''
    replacements = {
        "__OBJECT_NAME__": object_name,
        "__ASSET_PACKAGE__": _cpp_text(asset_package),
        "__CLASS_PATH__": _cpp_text(class_path),
        "__PROPERTIES_JSON__": _cpp_text_sequence(properties_json),
        "__OWNED_OBJECTS_JSON__": _cpp_text_sequence(owned_objects_json),
        "__OBJECT_NAME_TEXT__": _cpp_text(object_name),
        "__PRIMARY_ASSET_ID__": _cpp_text(primary_asset_id),
        "__STATE_TREE_INCLUDES__": (
            '#include "StateTree.h"\n'
            '#include "StateTreeCompilerLog.h"\n'
            '#include "StateTreeEditingSubsystem.h"'
            if state_tree_compile else ""
        ),
        "__STATE_TREE_COMPILE__": (
            "UStateTree* StateTree = Cast<UStateTree>(Asset);\n"
            "    TestNotNull(TEXT(\"generated StateTree\"), StateTree);\n"
            "    if (StateTree == nullptr) return false;\n"
            "    FStateTreeCompilerLog CompilerLog;\n"
            "    const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(StateTree, CompilerLog);\n"
            "    TestTrue(TEXT(\"StateTree compiled from editor data\"), bCompiled);\n"
            "    if (!bCompiled) return false;"
            if state_tree_compile else "// No asset-specific post-apply compiler is required."
        ),
        "__PACKAGE_NAMESPACE_SETUP__": (
            "#if USE_STABLE_LOCALIZATION_KEYS\n"
            f"    TextNamespaceUtil::ForcePackageNamespace(Package, {_cpp_text(package_namespace)});\n"
            "#endif"
            if package_namespace else "// This source asset has no package-localized FText identity."
        ),
    }
    for marker in sorted(replacements, key=len, reverse=True):
        replacement = replacements[marker]
        builder_test = builder_test.replace(marker, replacement)
    plugin = {
        "FileVersion": 3, "Version": 1, "VersionName": GENERATOR_VERSION,
        "FriendlyName": f"UE Ring Generated {object_name}",
        "Description": "Deterministic Unreal DataAsset builder generated from UE Ring reconstruction IR.",
        "Category": "Generated", "CanContainContent": False, "Installed": False,
        "Modules": [{"Name": module, "Type": "Editor", "LoadingPhase": "Default"}],
    }
    if state_tree_compile:
        plugin["Plugins"] = [
            {"Name": "PropertyBindingUtils", "Enabled": True},
            {"Name": "StateTree", "Enabled": True},
        ]
    files = {
        f"{module}.uplugin": json.dumps(plugin, ensure_ascii=True, indent=2) + "\n",
        f"Source/{module}/{module}.Build.cs": build_cs,
        f"Source/{module}/Private/{module}Module.cpp": module_cpp,
        f"Source/{module}/Private/{object_name}DataAssetBuilderTest.cpp": builder_test,
    }
    if blocker_lines:
        files[f"Source/{module}/Private/UERingReconstructionBlockers.cpp"] = "\n".join(blocker_lines) + "\n"
    return files, sorted(executed), sorted(set(blocked))


def _module_files(
    document: dict[str, Any],
    module: str,
    strict: bool,
    asset_package: str | None = None,
) -> tuple[dict[str, str], list[str], list[str]]:
    reconstruction = _require_object(document.get("reconstruction"), "reconstruction")
    if reconstruction.get("irVersion") != "2.0.0":
        raise ReconstructionError("only reconstruction IR 2.0.0 is supported")
    if reconstruction.get("contract") != "com.ue-ring.reconstruction":
        raise ReconstructionError("unsupported reconstruction contract")

    targets = reconstruction.get("targets")
    if isinstance(targets, list) and any(
        isinstance(item, dict)
        and item.get("backend") == "unrealEditorCpp"
        and item.get("target") == "editorAssetBuilderCpp"
        for item in targets
    ):
        semantics = _require_object(document.get("semantics"), "semantics")
        if semantics.get("representation") == "material-instance-v1":
            return _material_instance_files(document, module, strict, asset_package)
        if semantics.get("kind") == "DataAsset" and semantics.get("representation") == "data-asset-properties-v2":
            return _data_asset_files(document, module, strict, asset_package)
        raise ReconstructionError("editorAssetBuilderCpp has no backend for this semantic representation")

    if not isinstance(targets, list):
        raise ReconstructionError("reconstruction.targets must be an array")
    target = next(
        (
            item
            for item in targets
            if isinstance(item, dict)
            and item.get("backend") == "ueCpp"
            and item.get("target") == "nativeClassCpp"
        ),
        None,
    )
    if target is None:
        raise ReconstructionError("document has no nativeClassCpp ueCpp target")
    target_id = _require_string(target.get("id"), "target.id")

    symbols = reconstruction.get("symbols")
    if not isinstance(symbols, list):
        raise ReconstructionError("reconstruction.symbols must be an array")
    symbol_by_id = {
        item.get("id"): item
        for item in symbols
        if isinstance(item, dict) and isinstance(item.get("id"), str)
    }

    operations = reconstruction.get("operations")
    if not isinstance(operations, list) or not operations:
        raise ReconstructionError("reconstruction.operations must be a non-empty array")
    seen: set[str] = set()
    executed: list[str] = []
    blocked: list[str] = []
    class_operation: dict[str, Any] | None = None
    property_operations: list[dict[str, Any]] = []
    for operation in operations:
        operation = _require_object(operation, "operation")
        operation_id = _require_string(operation.get("id"), "operation.id")
        if operation_id in seen:
            raise ReconstructionError(f"duplicate operation id: {operation_id}")
        seen.add(operation_id)
        if operation.get("targetId") != target_id:
            continue
        opcode = _require_string(operation.get("opcode"), f"{operation_id}.opcode")
        status = operation.get("status")
        if status != "executable" or opcode not in NATIVE_CLASS_OPCODES:
            blocked.append(operation_id)
            continue
        if opcode == "cpp.class.declare":
            if class_operation is not None:
                raise ReconstructionError("multiple cpp.class.declare operations are not supported")
            class_operation = operation
        elif opcode == "cpp.property.declare":
            property_operations.append(operation)
        executed.append(operation_id)

    if class_operation is None:
        raise ReconstructionError("no executable cpp.class.declare operation")
    if strict and blocked:
        raise ReconstructionError("strict reconstruction blocked by operations: " + ", ".join(blocked))

    class_operands = _require_object(class_operation.get("operands"), "class operands")
    cpp_name = _identifier(_require_string(class_operands.get("cppName"), "class cppName"), "class cppName")
    parent_symbol_id = _require_string(class_operands.get("parentSymbolId"), "parentSymbolId")
    parent = _require_object(symbol_by_id.get(parent_symbol_id), "parent symbol")
    if parent.get("resolution") != "exact":
        raise ReconstructionError("parent class symbol is unresolved")
    parent_cpp = _identifier(_require_string(parent.get("cppName"), "parent cppName"), "parent cppName")
    parent_header = _require_string(parent.get("header"), "parent header")
    for prefix in ("Public/", "Private/", "Classes/"):
        if parent_header.startswith(prefix):
            parent_header = parent_header[len(prefix):]
            break
    if any(token in parent_header for token in ("..", "\\", "\n", "\r", '"')) or parent_header.startswith("/"):
        raise ReconstructionError(f"unsafe parent header: {parent_header!r}")

    property_lines: list[str] = []
    constructor_lines: list[str] = []
    reflection_lines: list[str] = []
    default_reflection_lines: list[str] = []
    forward_types: set[str] = set()
    for operation in sorted(property_operations, key=lambda item: item["id"]):
        operands = _require_object(operation.get("operands"), f"{operation['id']} operands")
        name = _identifier(_require_string(operands.get("name"), "property name"), "property name")
        cpp_type = _cpp_type(_require_string(operands.get("cppType"), f"{name} cppType"))
        if int(operands.get("arrayDim", 1)) != 1:
            raise ReconstructionError(f"fixed array property is not supported yet: {name}")
        for match in re.finditer(r"\b(?:TObjectPtr|TSoftObjectPtr|TWeakObjectPtr)<([UA][A-Za-z0-9_]*)>", cpp_type):
            forward_types.add(match.group(1))
        for match in re.finditer(r"\b([UA][A-Za-z0-9_]*)\s*\*", cpp_type):
            forward_types.add(match.group(1))
        specifiers = _property_specifiers(operands)
        property_lines.extend(
            [
                f"    UPROPERTY({specifiers})" if specifiers else "    UPROPERTY()",
                f"    {cpp_type} {name};",
                "",
            ]
        )
        if "defaultValue" in operands:
            expression = _default_expression(cpp_type, operands["defaultValue"])
            if expression is None:
                message = f"unsupported default value for {name} ({cpp_type})"
                if strict:
                    raise ReconstructionError(message)
                blocked.append(f"default:{name}")
            else:
                constructor_lines.append(f"    {name} = {expression};")
                default_reflection_lines.extend(_default_test_lines(cpp_type, name, operands["defaultValue"], expression))
        expected_type = cpp_type.replace('"', "")
        reflection_lines.extend(
            [
                f'    const FProperty* Property_{name} = FindFProperty<FProperty>(Class, TEXT("{name}"));',
                f'    TestNotNull(TEXT("property {name}"), Property_{name});',
                f'    if (Property_{name} != nullptr)',
                "    {",
                f'        TestEqual(TEXT("property type {name}"), Property_{name}->GetCPPType(), FString(TEXT("{expected_type}")));',
                "    }",
            ]
        )

    module_dependencies = {"Core", "CoreUObject", "Engine"}
    parent_module = parent.get("module")
    if isinstance(parent_module, str) and parent_module:
        module_dependencies.add(_identifier(parent_module, "parent module"))
    dependency_literals = ", ".join(f'"{item}"' for item in sorted(module_dependencies))
    api_macro = re.sub(r"[^A-Za-z0-9_]", "_", module).upper() + "_API"
    forward_lines = [f"class {name};" for name in sorted(forward_types)]
    blocker_lines = [
        f'#error "UERing reconstruction blocked: {item}"'
        for item in sorted(set(blocked))
    ]

    header = "\n".join(
        [
            "#pragma once",
            "",
            '#include "CoreMinimal.h"',
            f'#include "{parent_header}"',
            f'#include "{cpp_name}.generated.h"',
            "",
            *forward_lines,
            "" if forward_lines else "",
            "UCLASS(BlueprintType)",
            f"class {api_macro} {cpp_name} : public {parent_cpp}",
            "{",
            "    GENERATED_BODY()",
            "",
            "public:",
            f"    {cpp_name}();",
            "",
            *property_lines,
            "};",
            "",
        ]
    )
    source = "\n".join(
        [
            f'#include "{cpp_name}.h"',
            "",
            f"{cpp_name}::{cpp_name}()",
            "{",
            *(constructor_lines or ["    // No authored property defaults require constructor initialization."]),
            "}",
            "",
        ]
    )
    build_cs = "\n".join(
        [
            "using UnrealBuildTool;",
            "",
            f"public class {module} : ModuleRules",
            "{",
            f"    public {module}(ReadOnlyTargetRules Target) : base(Target)",
            "    {",
            "        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;",
            f"        PublicDependencyModuleNames.AddRange(new string[] {{ {dependency_literals} }});",
            "    }",
            "}",
            "",
        ]
    )
    module_cpp = "\n".join(
        [
            '#include "Modules/ModuleManager.h"',
            "",
            f"IMPLEMENT_MODULE(FDefaultModuleImpl, {module});",
            "",
        ]
    )
    reflection_test = "\n".join(
        [
            f'#include "{cpp_name}.h"',
            '#include "Misc/AutomationTest.h"',
            '#include "UObject/UnrealType.h"',
            "",
            "#if WITH_DEV_AUTOMATION_TESTS",
            f"IMPLEMENT_SIMPLE_AUTOMATION_TEST(F{cpp_name}ReflectionTest,",
            f'    "UERing.Generated.{cpp_name}.Reflection",',
            "    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)",
            "",
            f"bool F{cpp_name}ReflectionTest::RunTest(const FString& Parameters)",
            "{",
            f"    UClass* Class = {cpp_name}::StaticClass();",
            "    TestNotNull(TEXT(\"generated class\"), Class);",
            "    if (Class == nullptr) return false;",
            f"    TestTrue(TEXT(\"parent class\"), Class->IsChildOf({parent_cpp}::StaticClass()));",
            f"    const {cpp_name}* Defaults = GetDefault<{cpp_name}>();",
            "    TestNotNull(TEXT(\"class defaults\"), Defaults);",
            *reflection_lines,
            *default_reflection_lines,
            "    return true;",
            "}",
            "#endif",
            "",
        ]
    )
    plugin = {
        "FileVersion": 3,
        "Version": 1,
        "VersionName": GENERATOR_VERSION,
        "FriendlyName": f"UE Ring Generated {cpp_name}",
        "Description": "Deterministic C++ generated from UE Ring reconstruction IR.",
        "Category": "Generated",
        "CanContainContent": False,
        "Installed": False,
        "Modules": [{"Name": module, "Type": "Runtime", "LoadingPhase": "Default"}],
    }
    files = {
        f"{module}.uplugin": json.dumps(plugin, ensure_ascii=True, indent=2) + "\n",
        f"Source/{module}/{module}.Build.cs": build_cs,
        f"Source/{module}/Public/{cpp_name}.h": header,
        f"Source/{module}/Private/{cpp_name}.cpp": source,
        f"Source/{module}/Private/{module}Module.cpp": module_cpp,
        f"Source/{module}/Private/{cpp_name}ReflectionTest.cpp": reflection_test,
    }
    if blocker_lines:
        files[f"Source/{module}/Private/UERingReconstructionBlockers.cpp"] = "\n".join(blocker_lines) + "\n"
    return files, sorted(executed), sorted(set(blocked))


def generate_cpp_plugin(
    source_file: str | Path,
    output_root: str | Path,
    *,
    module: str = "UERingGenerated",
    strict: bool = False,
    asset_package: str | None = None,
) -> GenerationResult:
    source_path = Path(source_file).resolve()
    root = Path(output_root).resolve()
    module = _identifier(module, "module")
    try:
        document = json.loads(source_path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ReconstructionError(f"semantic file not found: {source_path}") from exc
    except json.JSONDecodeError as exc:
        raise ReconstructionError(f"invalid semantic JSON: {exc.msg}") from exc
    document = _require_object(document, "document")
    previous = _load_previous_manifest(root)
    files, executed, blocked = _module_files(document, module, strict, asset_package)
    root.mkdir(parents=True, exist_ok=True)

    generated_paths = set(files)
    if previous is not None:
        for entry in previous["files"]:
            relative = entry["path"]
            if relative not in generated_paths:
                stale = _safe_relative(root, relative)
                if stale.is_file():
                    stale.unlink()

    manifest = {
        "schema": "com.ue-ring.generated-cpp",
        "generatorVersion": GENERATOR_VERSION,
        "source": str(source_path),
        "sourceSha256": "sha256:" + hashlib.sha256(source_path.read_bytes()).hexdigest(),
        "module": module,
        "executedOperations": executed,
        "blockedOperations": blocked,
        "files": [
            {
                "path": relative,
                "sha256": "sha256:" + hashlib.sha256(content.encode("utf-8")).hexdigest(),
            }
            for relative, content in sorted(files.items())
        ],
    }
    for relative, content in sorted(files.items()):
        _atomic_write(_safe_relative(root, relative), content)
    _atomic_write(root / MANIFEST_NAME, json.dumps(manifest, ensure_ascii=True, indent=2) + "\n")
    return GenerationResult(root, tuple(sorted(files)), tuple(executed), tuple(blocked))


def verify_semantic_reexport(source_file: str | Path, reconstructed_file: str | Path) -> None:
    def load(path: str | Path) -> dict[str, Any]:
        try:
            return _require_object(json.loads(Path(path).read_text(encoding="utf-8")), str(path))
        except (OSError, json.JSONDecodeError) as exc:
            raise ReconstructionError(f"cannot read semantic re-export {path}: {exc}") from exc

    source = _require_object(load(source_file).get("semantics"), "source semantics")
    reconstructed = _require_object(load(reconstructed_file).get("semantics"), "reconstructed semantics")

    def authored_projection(semantics: dict[str, Any]) -> dict[str, Any]:
        policy = semantics.get("reconstructionPolicy")
        if not isinstance(policy, dict) or policy.get("strategy") != "state-tree-editor-compile-v1":
            return semantics
        derived = _require_array(
            policy.get("derivedRootProperties"),
            "semantics.reconstructionPolicy.derivedRootProperties",
        )
        if not all(isinstance(name, str) and name for name in derived):
            raise ReconstructionError(
                "semantics.reconstructionPolicy.derivedRootProperties must contain property names"
            )
        derived_names = set(derived)
        result = copy.deepcopy(semantics)
        properties = _require_array(result.get("properties"), "semantics.properties")
        result["properties"] = [
            entry for entry in properties
            if not isinstance(entry, dict) or entry.get("name") not in derived_names
        ]
        return result

    source = authored_projection(source)
    reconstructed = authored_projection(reconstructed)
    if source == reconstructed:
        return

    def first_difference(left: Any, right: Any, pointer: str = "/semantics") -> str:
        if type(left) is not type(right):
            return pointer
        if isinstance(left, dict):
            for key in sorted(set(left) | set(right)):
                child = pointer + "/" + key.replace("~", "~0").replace("/", "~1")
                if key not in left or key not in right:
                    return child
                difference = first_difference(left[key], right[key], child)
                if difference:
                    return difference
            return ""
        if isinstance(left, list):
            if len(left) != len(right):
                return pointer
            for index, (left_item, right_item) in enumerate(zip(left, right)):
                difference = first_difference(left_item, right_item, f"{pointer}/{index}")
                if difference:
                    return difference
            return ""
        return "" if left == right else pointer

    raise ReconstructionError("semantic re-export differs at " + first_difference(source, reconstructed))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate deterministic Unreal C++ from UE Ring IR v2")
    parser.add_argument("semantic_file", help="USEM asset sidecar containing reconstruction IR v2")
    parser.add_argument("--output", required=True, help="Generated plugin output directory")
    parser.add_argument("--module", default="UERingGenerated", help="Generated Unreal module name")
    parser.add_argument("--strict", action="store_true", help="Refuse any blocked or unsupported operation")
    parser.add_argument("--asset-package", help="Generated asset package below /Game/UERingGenerated")
    parser.add_argument("--verify-reexport", help="Compare reconstructed asset semantics with the source")
    args = parser.parse_args(argv)
    try:
        result = generate_cpp_plugin(
            args.semantic_file,
            args.output,
            module=args.module,
            strict=args.strict,
            asset_package=args.asset_package,
        )
        if args.verify_reexport:
            verify_semantic_reexport(args.semantic_file, args.verify_reexport)
    except ReconstructionError as exc:
        parser.exit(2, f"reconstruction failed: {exc}\n")
    print(
        json.dumps(
            {
                "output": str(result.output_root),
                "files": len(result.files),
                "executedOperations": len(result.executed_operations),
                "blockedOperations": len(result.blocked_operations),
            },
            ensure_ascii=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
