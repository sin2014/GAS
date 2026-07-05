# Dota2 Asset Importer

Reusable Unreal Editor commandlet for importing Dota 2 assets after Blender has normalized Source2Viewer glTF output into UE-friendly FBX files, copied textures, and emitted a manifest.

This plugin is intentionally an **Editor-only** importer. It is not runtime gameplay code and should not be packaged into the game.

## What This Plugin Does

`Dota2AssetImporter` reads a Blender-generated manifest and imports a Dota 2 character package into a UE Content folder.

It handles:

- Texture import.
- Texture settings for color, normal, ORM, spec mask, detail mask, and rim mask files.
- Automatic PBR material creation from Dota-style texture names.
- Skeletal Mesh import.
- Shared master Skeleton creation from the first skeletal mesh entry.
- Modular Skeletal Mesh import against the master Skeleton.
- Static prop import.
- Standalone FX Skeletal Mesh import where supported.
- Animation import against the master Skeleton.
- Skipping one-frame/static pose or lookFrame animation entries.
- Skeleton and AnimSequence preview mesh setup.
- Optional ShadowFiend-specific blueprint component setup.

It does **not** solve the Source2Viewer glTF axis/scale problem by itself. That must be fixed in Blender before FBX export.

## Required Blender Preprocess

The successful ShadowFiend pipeline depends on Blender doing the real coordinate fix:

- Bake Source2Viewer's hidden axis conversion into mesh vertex data.
- Bake the same axis conversion into Armature edit bones / rest pose.
- Bake the same axis conversion into root-bone location animation tracks.
- Reset mesh and Armature object transforms to identity.
- Export FBX with clean object transforms.

The UE import command should then use:

```text
Import rotation      = 0,0,0
ImportUniformScale   = 1.0
Skeleton             = None on the first master mesh import
```

Do not use `-ImportRotateX=-90` as the normal path for Blender-processed Dota 2 assets. That was useful only for diagnosing earlier failed exports and can produce mismatched mesh, skeleton, and animation spaces.

## Commandlet Example

Successful ShadowFiend command:

```powershell
& "D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\GameDev\Unreal_Projects\GAS\GAS.uproject" `
  -run=Dota2Import `
  -Manifest="D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\processed_ue5\shadowfiend_export_manifest.json" `
  -Dest="/Game/Assets/Characters/Dota2/ShadowFiend" `
  -Character=ShadowFiend `
  -ReplaceExisting `
  -ImportUniformScale=1.0 `
  -Blueprint="/Game/Blueprints/Character/Test/NewBlueprint.NewBlueprint" `
  -AnimBlueprint="/Game/Blueprints/Character/Test/NewAnimBlueprint.NewAnimBlueprint" `
  -IdleAnim=AN_ShadowFiend_Arcana_idle `
  -unattended -nop4 -nosplash -NoSound -stdout -FullStdOutLogOutput
```

Important defaults:

- Omit `-Skeleton=` when creating a fresh character import. The first `skeletal_meshes` manifest entry creates the master Skeleton.
- Omit `-ImportRotateX/Y/Z` for Blender-normalized exports.
- Use `-ImportUniformScale=1.0`.
- Use `-ReplaceExisting` only when you intentionally want to overwrite existing assets.

## Parameters

- `-Manifest="...json"`: Required. Blender-generated manifest path.
- `-Dest="/Game/..."`: Required. UE Content destination root.
- `-Character="ShadowFiend"`: Optional. Used in generated material names.
- `-ReplaceExisting`: Optional. Replaces existing imported assets and import settings.
- `-Skeleton="/Game/...Skeleton.Skeleton"`: Optional, but avoid it for a fresh corrected import. Use only when you know the existing Skeleton has the exact same rest pose, orientation, scale, and bone set.
- `-ImportRotateX/Y/Z=`: Optional diagnostic/import fallback. Do not use for the standard normalized Blender pipeline.
- `-ImportUniformScale=1.0`: Optional. Defaults to `1.0`.
- `-Blueprint="/Game/...BP.BP"`: Optional. Configures a target character blueprint after import.
- `-AnimBlueprint="/Game/...ABP.ABP"`: Optional. Sets the main mesh's animation blueprint during blueprint setup.
- `-IdleAnim="..."`: Optional fallback single-node animation name/path if no animation blueprint is used.

## Manifest Contract

The Blender step should create a JSON manifest with these top-level keys:

- `skeletal_meshes`: Array of FBX skeletal mesh entries.
- `animations`: Array of FBX animation entries.
- `props`: Array of static props or standalone FX mesh entries.
- `textures`: Object/map of copied texture paths.

The first `skeletal_meshes` entry is treated as the master mesh. It creates the shared UE Skeleton and PhysicsAsset unless `-Skeleton=` is explicitly supplied.

Animation entries should include `frame_start` and `frame_end` when available. Entries with less than one frame of range are treated as static pose/lookFrame entries and skipped.

Example entry:

```json
{
  "name": "AN_ShadowFiend_Arcana_idle",
  "file": "D:/GameDev/Unreal_Projects/Asset/Dota2/ShadowFiend/processed_ue5/FBX/AN_ShadowFiend_Arcana_idle.fbx",
  "frame_start": 1.0,
  "frame_end": 154.0
}
```

## Texture and Material Rules

The importer groups textures by Dota-style file names:

- `*_color*`: sRGB color texture, connected to Base Color.
- `*_normal*`: non-sRGB normal map, connected to Normal.
- `*_orm*`: non-sRGB mask texture. R connects to Ambient Occlusion, G to Roughness, B to Metallic.
- `*_specmask*`: non-sRGB mask texture. R connects to Specular.
- `*_detailmask*`: non-sRGB mask texture. Imported and grouped, not connected by default.
- `*_rimmask*`: non-sRGB mask texture. Imported and grouped, not connected by default.

Mask textures must be imported with mask compression and sampled as `SAMPLERTYPE_Masks`; otherwise UE SM6 can report:

```text
Sampler type is Linear Color, should be Masks
```

## ShadowFiend Blueprint Setup

The current blueprint setup helper is project-specific and expects ShadowFiend Arcana-style asset names:

- `SK_ShadowFiend_Arcana_Body`
- `SK_ShadowFiend_Arcana_Head`
- `SK_ShadowFiend_Arcana_Arms`
- `SK_ShadowFiend_Arcana_Shoulders`
- `SK_ShadowFiend_Arcana_Wings`

It configures:

```text
CharacterMesh0        -> Body, animation driver
  Dota2Head           -> Leader Pose follower
  Dota2Arms           -> Leader Pose follower
  Dota2Shoulders      -> Leader Pose follower
  Dota2Wings          -> Leader Pose follower
```

This is not yet a fully generic Dota 2 loadout system. For another hero, either update this helper or configure the blueprint manually.

## Generated Folders

`Binaries/` and `Intermediate/` are generated by Unreal Build Tool. They can be deleted when cleaning the plugin. UE will rebuild them when the project is opened or compiled again.

Source files and plugin metadata should be kept:

```text
Dota2AssetImporter.uplugin
README.md
Source/
```

## Troubleshooting

If Skeletal Mesh preview is correct but Skeleton preview is tiny, rotated, or lying down:

- The Blender export is still wrong.
- Re-bake Source2 axis into mesh vertices, armature rest pose, and root animation locations.
- Reimport with no UE import rotation and a fresh Skeleton.

If all animations are static or look like idle:

- Verify Blender exports one active Action per FBX.
- Verify `bake_anim_use_all_actions=False`.
- Check manifest `frame_start` and `frame_end`.
- Skip one-frame pose/lookFrame entries.

If materials compile with sampler errors:

- Confirm mask textures use mask compression.
- Confirm material Texture Sample sampler type is Masks.

See the main workflow document for the complete pipeline:

```text
D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\Dota2_to_UE5_Import_Workflow.md
```
