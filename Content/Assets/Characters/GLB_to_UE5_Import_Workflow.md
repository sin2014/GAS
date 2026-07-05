# GLB 模型导入 UE5 工作流

本文记录这次把 `大元素使_拉克丝.glb` 导入 UE 5.8 的成功路线，目标是以后遇到同类 `.glb` 角色模型时，可以按同一流程把模型、骨骼、材质、贴图、动画完整导入 Unreal Engine。

示例项目：

- UE 项目：`D:\GameDev\Unreal_Projects\GAS`
- 示例源文件：`C:\Users\ZYZ\Downloads\大元素使_拉克丝.glb`
- 示例目标目录：`/Game/Assets/Characters/Test/ElementalistLux`

## 硬性禁用：不要使用 UE Python

本项目的 UE 导入、资产修复、材质修复、资产重命名、引用清理、蓝图调整流程中，禁止使用任何 UE Python 插件、UE Python 控制台、`-ExecutePythonScript`、Python commandlet、Editor Utility Python、蓝图里的 `ExecutePythonScript` 节点或其他依赖 UE PythonScriptPlugin 的脚本程序。

原因是已经在本项目 UE 5.8 环境中验证：UE Python 要么没有实际效果，要么会在启用或执行过程中导致编辑器崩溃。因此后续遇到 UE 资产批处理需求时，应使用项目 C++ 插件、Editor commandlet、Unreal MCP 暴露的非 Python 工具、手工编辑器操作，或离线 Blender/外部脚本处理源资产。Blender 预处理阶段可以继续使用 Blender 自己的 Python；本禁令只针对 UE 内部 Python / PythonScriptPlugin。

## 最终结论

这类网站导出的 `.glb` 角色，不建议直接拖进 UE 5.8 内容浏览器，也不建议优先走 UE 5.8 默认 Interchange 导入链路。

本次成功路线是：

1. 用 Blender 正确读取 `.glb`。
2. 从 Blender 导出一个 **all-in-one FBX**，包含 Skeletal Mesh、Skeleton、全部 Actions/Animations、材质槽和外部贴图路径。
3. 在 UE 中用 **legacy `FbxFactory` 导入器** 导入 FBX，而不是默认 Interchange。
4. 发现材质没有正确接图时，单独导入贴图，并用 UE Editor 脚本/commandlet 创建或修复材质。贴图资产建议统一加 `T_` 前缀，避免和 FBX 自动生成的材质资产同名。
5. 将贴图材质赋给 Skeletal Mesh 的材质槽。
6. 检查动画数量、Skeleton、材质引用，确认资产落盘。

本次成功结果中，旧 FBX 导入链路可以一次导入完整 63 个动画；之前默认导入路线只拿到了少量动画。

## 这次问题的根因

### 1. 为什么直接 GLB/Interchange 不理想

UE 5.8 默认导入 `.glb` 或 FBX 时更倾向使用 Interchange 管线。它能导入模型和部分动画，但对这类来自网页模型查看器/游戏模型导出站的 GLB，容易出现：

- 动画导入不全，只出现几个 AnimSequence。
- 材质资产生成了，但贴图没有正确接到材质图上，模型显示白色。
- 有些导入设置看起来正确，但动画轨道/根骨骼轨道处理不完整。

本次验证里，同一份 FBX 用 legacy `FbxFactory` 导入可以得到 63 个动画；默认路线只有 8 个左右。

### 2. 为什么不能随便修 Blender 里的负缩放

源 GLB 里场景根节点/Armature 带有负缩放或坐标系转换信息。Blender 可以正确播放它，但如果为了“看起来干净”强行 Apply Armature Scale，可能导致：

- 骨骼静态姿势看起来正常；
- 动画播放时四肢拉长、扭曲；
- 武器和身体相对位置错误；
- UE 里动画看起来骨架对，但动作错。

本次对比验证过：

- 保持 Blender 导入后的 Armature 变换，通过 FBX 坐标转换导出，动画能保持正确。
- 强行 Apply Armature Scale 后，动画包围盒误差明显变大，UE 中会出现四肢变形。

所以原则是：**不要为了让 Armature Scale 变成 1 而破坏源动画空间。**

### 3. 为什么材质是白的

源 GLB 使用的是 glTF `KHR_materials_unlit`，并且多个材质槽共用同一张贴图 atlas。

UE 导入 FBX 后通常会生成材质槽，例如：

- `Light_Body`
- `Light_Collar`
- `Light_Staff`
- `Light_Staff_Ends`

但导入器不一定会把贴图正确连到材质的 Base Color 或 Emissive 上。结果就是：

- Skeletal Mesh 有材质槽；
- 材质资产也存在；
- 贴图可能没导入，或者贴图没有连接；
- 模型显示白色。

解决方式是单独导入贴图，然后创建/修复材质：

- Shading Model：`Unlit`
- Blend Mode：`Masked`
- Two Sided：开启
- Texture RGB：接到 `Emissive Color`
- Texture Alpha：接到 `Opacity Mask`
- Opacity Mask Clip Value：可用 `0.33`

## 标准流程

### 第 1 步：分析 GLB

先确认 GLB 里有什么，不要直接导入 UE 猜。

需要确认：

- Mesh 数量
- Skin/Skeleton 数量
- Bone 数量
- Animation 数量
- Material 数量
- Texture/Image 数量
- 是否使用 `KHR_materials_unlit`
- 是否只有一张 atlas 贴图
- Armature/Root Node 是否带负缩放

可用 Blender Python 或 glTF 工具检查。示例结论：

- 1 个 Mesh
- 1 个 Skin
- 63 个 Animations
- 4 个 Materials
- 1 张 Texture/Image atlas
- 使用 `KHR_materials_unlit`
- Armature/场景根节点带坐标转换/负缩放信息

### 第 2 步：用 Blender 打开 GLB 并确认动画

在 Blender 中导入 GLB：

1. `File > Import > glTF 2.0`
2. 选择源 `.glb`
3. 检查角色静态姿势是否正确
4. 在 Action Editor / Dope Sheet 中确认全部动作存在
5. 播放几个代表动画，例如 Idle、Run、Attack、Death、Recall

如果 Blender 中材质和动画都是正确的，说明源数据本身没有坏。

### 第 3 步：从 Blender 导出 all-in-one FBX

核心要求：

- 只导出 Armature 和主 Mesh。
- 不要 Apply Armature Scale。
- 不要把每个动画单独拆成 FBX 后再导入 UE，除非确认根轨道完全匹配。
- 使用 `bake_anim_use_all_actions=True` 导出所有 Actions。
- 贴图使用外部 PNG，不要依赖 FBX 内嵌贴图。

推荐 Blender FBX 导出设置：

```python
bpy.ops.export_scene.fbx(
    filepath=str(FBX_PATH),
    use_selection=True,
    object_types={"ARMATURE", "MESH"},
    add_leaf_bones=False,
    bake_anim=True,
    bake_anim_use_all_actions=True,
    bake_anim_use_nla_strips=False,
    bake_anim_use_all_bones=True,
    bake_anim_force_startend_keying=True,
    bake_anim_step=1.0,
    bake_anim_simplify_factor=0.0,
    path_mode="AUTO",
    embed_textures=False,
    axis_forward="-Z",
    axis_up="Y",
    use_space_transform=True,
    bake_space_transform=False,
    global_scale=1.0,
    apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_NONE",
    primary_bone_axis="Y",
    secondary_bone_axis="X",
    armature_nodetype="NULL",
)
```

重要点：

- `bake_anim_use_all_actions=True` 是导出全部动画的关键。
- `bake_space_transform=False` 避免破坏骨骼动画空间。
- `add_leaf_bones=False` 避免 UE 里多出无用 leaf bones。
- `embed_textures=False`，贴图单独导入 UE 更可控。
- `path_mode="AUTO"` 或明确使用外部相对路径。

### 第 4 步：导出贴图 PNG

如果 GLB 的图片是 packed image，需要从 Blender 保存出来。

示例逻辑：

```python
for image in bpy.data.images:
    if image.name in {"Render Result", "Viewer Node"}:
        continue
    if image.size[0] == 0 or image.size[1] == 0:
        continue

    out = OUT_DIR / f"{image.name}.png"
    if image.packed_file:
        image.filepath_raw = str(out)
        image.file_format = "PNG"
        image.save()
    elif image.filepath:
        shutil.copy2(bpy.path.abspath(image.filepath), out)

    image.filepath = "//" + out.name
```

建议贴图命名为：

- `T_<角色或材质名>.png`
- 示例：`T_Light_Staff.png`

原因是 FBX 导入后 UE 可能已经生成了与源材质同名的 `UMaterial` 资产，例如 `Air_Body.uasset`。如果后续再导入 `Air_Body.png`，`TextureFactory` 会尝试创建同名 `Texture2D`，和现有 `Material` 发生资产名冲突，导致贴图导入失败。批量导入时应先把 PNG 复制或导出为 `T_Air_Body.png` 这种名字，再导入到同一目录。

### 第 5 步：用 UE legacy FbxFactory 导入 FBX

不要直接拖拽导入。用 `UnrealEditor-Cmd.exe -run=ImportAssets` 加 JSON 设置，显式指定：

```json
{
  "ImportGroups": [
    {
      "GroupName": "Character complete legacy FBX import",
      "Filenames": [
        "D:/Path/To/Character_Full.fbx"
      ],
      "DestinationPath": "/Game/Assets/Characters/Test/ElementalistLux",
      "FactoryName": "/Script/UnrealEd.FbxFactory",
      "bReplaceExisting": true,
      "bSkipReadOnly": false,
      "ImportSettings": {
        "MeshTypeToImport": "FBXIT_SkeletalMesh",
        "bImportAsSkeletal": true,
        "bImportMesh": true,
        "bCreatePhysicsAsset": true,
        "bImportAnimations": true,
        "bImportMaterials": true,
        "bImportTextures": true,
        "SkeletalMeshImportData": {
          "ImportContentType": "FBXICT_All",
          "bUpdateSkeletonReferencePose": false,
          "bUseT0AsRefPose": false,
          "bPreserveSmoothingGroups": false,
          "bKeepSectionsSeparate": false,
          "bImportMeshesInBoneHierarchy": true,
          "bImportMorphTargets": false,
          "bImportVertexAttributes": false,
          "ImportUniformScale": 1.0,
          "bConvertScene": true,
          "bForceFrontXAxis": false,
          "bConvertSceneUnit": true
        },
        "AnimSequenceImportData": {
          "AnimationLength": "FBXALIT_ExportedTime",
          "bUseDefaultSampleRate": true,
          "CustomSampleRate": 30,
          "bSnapToClosestFrameBoundary": true,
          "bImportCustomAttribute": false,
          "bImportBoneTracks": true,
          "bPreserveLocalTransform": false,
          "bImportMeshesInBoneHierarchy": true
        },
        "TextureImportData": {
          "MaterialSearchLocation": "Local"
        }
      }
    }
  ]
}
```

命令示例：

```powershell
& "D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\GameDev\Unreal_Projects\GAS\GAS.uproject" `
  -run=ImportAssets `
  -importsettings="D:\Path\To\legacy_import_complete.json" `
  -nosourcecontrol `
  -unattended `
  -nop4 `
  -NoShaderCompile
```

注意：

- 如果 UE 编辑器已经打开且 MCP 占用了 `127.0.0.1:8000`，命令行可能返回非 0。
- 只要日志里显示动画压缩和资产保存，并且文件实际落盘，就说明导入本体可能已经成功。
- 不要只看 exit code，要同时检查资产目录。

### 第 5.5 步：清理 UE 生成的冗余资产名

导入 FBX 时，staging 文件名通常会带 `_Full` 或 `_Full_Anim_Skeleton`。这些后缀只用于中间流程，最终 UE 资产名里不需要保留。

最终命名规则：

- 骨骼网格体：`<Name>_Full` -> `<Name>`
- Skeleton：`<Name>_Full_Skeleton` -> `<Name>_Skeleton`
- PhysicsAsset：`<Name>_Full_PhysicsAsset` -> `<Name>_PhysicsAsset`
- 动画：`<Name>_Full_Anim_Skeleton_<AnimName>` -> `<Name>_<AnimName>`

示例：

```text
Air_Lux_Full                              -> Air_Lux
Air_Lux_Full_Skeleton                     -> Air_Lux_Skeleton
Air_Lux_Full_PhysicsAsset                 -> Air_Lux_PhysicsAsset
Air_Lux_Full_Anim_Skeleton_Attack1_Skeleton -> Air_Lux_Attack1_Skeleton
```

注意：不要在文件系统里直接重命名 `.uasset`。必须通过 UE 的 AssetTools、Editor Utility、Python/Blueprint 编辑器脚本或一次性 commandlet 执行 Rename，这样才能同步修复资产引用并删除 redirector。

### 第 6 步：验证动画数量

导入后检查目标目录。

PowerShell 示例：

```powershell
$Target = "D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\Test\ElementalistLux"
Get-ChildItem -LiteralPath $Target -File |
  Group-Object {
    if ($_.Name -like "*_Anim_*") { "AnimSequences" }
    elseif ($_.Name -like "*Skeleton.uasset") { "Skeleton" }
    elseif ($_.Name -like "*PhysicsAsset.uasset") { "PhysicsAsset" }
    elseif ($_.Name -like "Light_*") { "Materials" }
    else { "Other" }
  } |
  Select-Object Name, Count
```

成功标准：

- AnimSequence 数量应该接近 GLB 里的 animation 数量。
- 本次示例应为 63。
- SkeletalMesh、Skeleton、PhysicsAsset 都应存在。

### 第 7 步：单独导入贴图

如果 FBX 导入后没有贴图，或者材质是白色，需要单独导入 PNG。

强烈建议导入前把贴图文件名改成 `T_` 前缀：

```text
Air_Body.png      -> T_Air_Body.png
Light_Staff.png   -> T_Light_Staff.png
Nature_Staff.png  -> T_Nature_Staff.png
```

不要把贴图以和材质完全相同的资产名导入到同一目录。UE 的一个目录下不能同时存在同名的 `Material` 和 `Texture2D` 资产，本次批量导入中就遇到过这种错误：

```text
Could not delete existing asset Material /Game/.../Air_Body.Air_Body
Texture import failed
```

解决方式是给贴图加 `T_` 前缀后重新导入。

ImportAssets JSON 示例：

```json
{
  "ImportGroups": [
    {
      "GroupName": "Character texture import",
      "Filenames": [
        "D:/Path/To/T_Light_Staff.png"
      ],
      "DestinationPath": "/Game/Assets/Characters/Test/ElementalistLux",
      "FactoryName": "/Script/UnrealEd.TextureFactory",
      "bReplaceExisting": true,
      "bSkipReadOnly": false
    }
  ]
}
```

命令同样使用：

```powershell
UnrealEditor-Cmd.exe Project.uproject -run=ImportAssets -importsettings=texture_import.json
```

成功日志通常会出现：

```text
Image imported as : TSF BGRA8
```

### 第 8 步：修复材质

材质白色时，不要只检查材质槽名字，要检查材质图里有没有 TextureSample。

推荐材质设置：

- Material Domain：Surface
- Blend Mode：Masked
- Shading Model：Unlit
- Two Sided：true
- Texture RGB -> Emissive Color
- Texture Alpha -> Opacity Mask
- Opacity Mask Clip Value：0.33

为什么接 `Emissive Color`：

- GLB 使用 `KHR_materials_unlit`。
- 游戏模型/网页查看器通常希望贴图颜色不受 UE 默认光照强烈影响。
- 用 Unlit + Emissive 更接近原始预览效果。

如果手工处理：

1. 打开材质 `Light_Body`。
2. 添加 `Texture Sample`，选择 `T_Light_Staff`。
3. RGB 接到 `Emissive Color`。
4. A 接到 `Opacity Mask`。
5. 设置 `Blend Mode = Masked`。
6. 设置 `Shading Model = Unlit`。
7. 勾选 `Two Sided`。
8. 保存。
9. 对其他材质槽重复。
10. 打开 Skeletal Mesh，把材质槽指向修好的材质。

如果批量处理，建议写一次性 Editor commandlet 或 Editor Utility，做以下事情：

1. 加载 `USkeletalMesh`。
2. 加载 `UTexture2D`。
3. 创建或打开 `UMaterial`。
4. 清空/建立材质表达式。
5. 创建 `UMaterialExpressionTextureSample`。
6. 设置 `TextureSample->Texture = T_Light_Staff`。
7. `Material->SetShadingModel(MSM_Unlit)`。
8. `Material->BlendMode = BLEND_Masked`。
9. `Material->TwoSided = true`。
10. RGB 连接到 `EmissiveColor`。
11. Alpha 连接到 `OpacityMask`。
12. 保存材质包。
13. 修改 Skeletal Mesh 的 `GetMaterials()` 数组，把对应 slot 指向修好的材质。
14. 保存 Skeletal Mesh。

这次成功验证中，Skeletal Mesh 最终材质引用类似：

```text
Light_Staff      -> /Game/.../M_Light_Staff
Light_Staff_Ends -> /Game/.../M_Light_Staff_Ends
Light_Body       -> /Game/.../M_Light_Body
Light_Collar     -> /Game/.../M_Light_Collar
```

并且每个材质资产中能查到：

```text
T_Light_Staff
BLEND_Masked
MSM_Unlit
MaterialExpressionTextureSample
```

### 第 9 步：验证材质引用

可以用 UE 编辑器打开 Skeletal Mesh，检查材质槽；也可以用文件字符串做粗略确认。

PowerShell 示例：

```powershell
rg -a "Materials|M_Light_|T_Light_Staff|MSM_Unlit|BLEND_Masked" `
  "D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\Test\ElementalistLux\ElementalistLux_Full.uasset" `
  "D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\Test\ElementalistLux\M_Light_Body.uasset"
```

看到以下内容说明引用大概率正确：

- Skeletal Mesh 里有 `M_Light_*` 或修好的 `Light_*` 材质路径。
- 材质里有 `T_Light_Staff`。
- 材质里有 `MSM_Unlit`。
- 材质里有 `BLEND_Masked`。

最终仍建议在 UE 中打开 Skeletal Mesh 目视检查：

- 模型不是白色。
- 身体、头发、武器贴图正确。
- 透明/镂空边缘没有大块白底。
- 播放 Idle/Run/Attack/Death 等动画不拉伸。

### 第 10 步：导入后清理未引用贴图和临时文件

每次 GLB 导入、材质修复、动画验证完成后，都必须做一次收尾清理。目标目录里只保留真正被模型、材质、动画使用的 UE 资产；导入过程中临时生成的脚本、中间 FBX/PNG/JSON、一次性 commandlet 都不能长期留在项目里。

#### 10.1 清理未引用贴图/纹理资产

材质修复完成后，先确认哪些 `Texture2D` 真的被材质引用，再删除未引用贴图。不要只凭文件名猜，也不要一股脑删除所有 `T_*.uasset`。

推荐判断方式：

1. 在 UE 中打开 Skeletal Mesh，确认材质槽已经指向最终材质。
2. 对目标目录下每个材质检查 `TextureSample` / `UMaterialExpressionTextureBase` 引用。
3. 汇总材质实际引用的 `Texture2D` 包路径。
4. 将目标目录下的纹理资产与引用列表比对。
5. 只删除没有被任何最终材质引用的纹理资产。

可以用 UE Reference Viewer、Asset Audit、Editor Utility、Python/Blueprint 编辑器脚本，或一次性 commandlet 做资产级检查和删除。建议删除时走 UE 的 AssetTools/ObjectTools，而不是直接在文件系统里删除 `.uasset`，这样可以同步处理引用、redirector 和保存状态。

清理标准：

- 保留材质仍在引用的贴图，例如 `T_<Name>_100`、`T_<Name>_101`。
- 删除导入器自动生成但材质没有使用的重复贴图、空贴图、测试贴图。
- 删除前保存所有材质和 Skeletal Mesh。
- 删除后在目标目录执行 Fix Up Redirectors。
- 重新打开 Skeletal Mesh，确认模型不是白色，透明边缘正常，动画播放正常。

如果删除失败，常见原因是 UE 编辑器仍打开了贴图、材质、Skeletal Mesh 或动画资产，Windows 锁住了 `.uasset`：

```text
Error Code 32
The process cannot access the file because it is being used by another process.
```

处理方式：

1. 关闭 UE 中打开的相关资产窗口。
2. 点击 Save All。
3. 必要时关闭 UE 主编辑器。
4. 再重新执行删除。

只有在已经通过 UE 资产引用检查确认“未引用”的具体文件，且 UE 已关闭或文件不再被锁定时，才可以对这些明确的 `.uasset` 做文件系统级删除。不要对整个目录执行递归删除。

#### 10.2 清理导入过程产生的脚本和中间文件

导入成功后，清理以下临时产物：

- Blender 中间 `.blend`
- 导出测试 FBX
- 探针 JSON
- 临时 PowerShell 脚本
- 临时 UE commandlet 源码
- 临时 UE commandlet 编译产物
- UE 保存失败产生的 `.tmp`
- 测试导入目录，例如 `LegacyProbe`、`AnimOnlyProbe`
- `Saved\GLBImport_<Name>`、`Saved\BatchLuxImport` 等 staging 目录
- `export_glb_to_fbx.py`、`import_fbx.json`、`texture_import.json` 等一次性导入脚本/配置
- 为了 commandlet 临时加到 `Build.cs` 里的 `UnrealEd`、`AssetRegistry`、`AssetTools` 等依赖
- `Intermediate\Build` 下同名 commandlet 产生的 `.obj`、`.dep.json`、`.rsp`、`.sarif`

注意：

- 如果 UE 编辑器打开了相关资产，Windows 会锁住 `.uasset`，导致删除失败。
- 这时不要强行删除或杀进程，先关闭 UE 中打开的资产窗口，必要时重启 UE 后再删。
- 临时 commandlet 跑完后必须删除对应 `.h/.cpp`，并恢复 `Build.cs`。
- 清理后重新编译一次项目，确认临时代码和临时依赖没有留在正式工程里。
- 项目内可以保留这份工作流文档；除非已经整理成通用工具，否则不要保留某次导入专用的脚本。

## 常见坑

### 坑 1：只导出了几个动画

原因通常是：

- Blender FBX 导出没有启用 `bake_anim_use_all_actions=True`。
- UE 默认 Interchange 导入没完整处理所有动画 stack。
- 拆分动画 FBX 时根骨骼轨道不匹配。

解决：

- 优先导出 all-in-one FBX。
- UE 中使用 legacy `FbxFactory`。
- 导入后统计 AnimSequence 数量。

### 坑 2：骨骼看起来对，动画四肢拉长

原因通常是：

- 在 Blender 里 Apply 了 Armature 负缩放。
- 导出时使用了不合适的 `bake_space_transform`。
- 拆分动画 FBX 后丢了根轨道。

解决：

- 不要为了让 Scale 变成 1 而 Apply Armature。
- 使用已验证的 FBX 导出参数。
- 不要先拆分动画，先用 all-in-one FBX 验证完整导入。

### 坑 3：材质是白的

原因通常是：

- UE 创建了材质资产，但没有 TextureSample。
- 贴图没有导入。
- 材质还是默认 Lit/Opaque，和源 GLB 的 unlit 材质不一致。

解决：

- 单独导入 PNG 贴图。
- 建立 Unlit + Masked + TwoSided 材质。
- RGB 接 Emissive，Alpha 接 Opacity Mask。
- 把 Skeletal Mesh 材质槽指向修好的材质。

### 坑 4：UE commandlet 返回 1，但资产已经生成

如果编辑器已经打开，MCP 插件可能占用端口，例如：

```text
LogHttpListener: Error: HttpListener unable to bind to 127.0.0.1:8000
```

这会让 commandlet 最终返回失败码，但资产导入可能已经完成。

判断方式：

- 看日志中是否有 AnimSequence 压缩和保存记录。
- 看目标目录是否生成 `.uasset`。
- 用文件数量和资产引用验证。

### 坑 5：Skeletal Mesh 保存失败

如果日志出现：

```text
Error Code 32
The process cannot access the file because it is being used by another process.
```

说明当前 UE 编辑器正在占用这个 `.uasset`。

解决：

- 关闭 UE 中打开的 Skeletal Mesh/Animation/Material。
- 必要时重启 UE。
- 再运行材质赋值/保存流程。

### 坑 6：贴图和材质资产同名导致贴图导入失败

FBX 导入时会按源材质槽创建材质资产，例如：

```text
Air_Body.uasset
Air_Collar.uasset
Air_Staff.uasset
```

如果源贴图也叫 `Air_Body.png`，再用 `TextureFactory` 导入到同一目录时，UE 会尝试创建 `/Game/.../Air_Body` 这个 `Texture2D`，但这个名字已经被 `UMaterial` 占用，结果导入失败。

解决：

- 导入贴图前统一重命名为 `T_<原名>.png`。
- 材质资产可以继续保留源材质槽名，例如 `Air_Body`。
- 材质图里引用 `T_Air_Body`。
- Skeletal Mesh 材质槽继续指向修复后的 `Air_Body` 材质。

这种方式比额外创建 `M_` 材质更适合批量修复，因为 FBX 导入后 Skeletal Mesh 已经引用了同名材质；只要把这些原始材质资产“原地修好”，很多情况下不需要大规模重写 Skeletal Mesh 引用。

### 坑 7：ImportAssets 返回失败码，但贴图实际导入成功

本次批量贴图导入时，命令行返回了 `1`，但 10 张贴图都已经成功落盘。日志中的有效成功信息是：

```text
Image imported as : TSF BGRA8
```

同时还出现了几个容易误判的噪声：

```text
LogHttpListener: Error: HttpListener unable to bind to 127.0.0.1:8000
LogAutomatedImport: Error: Invalid Destination Path ()
LogImageWrapper: Warning: PNG Warning() eXIf: duplicate
```

判断方式：

- `127.0.0.1:8000` 端口错误通常是已打开的 UE 编辑器/MCP 插件占用端口，不代表资产导入失败。
- `Invalid Destination Path ()` 在本次 ImportAssets 中是命令let额外报出的空路径噪声，需要结合后续导入日志判断。
- `PNG Warning() eXIf: duplicate` 是 PNG 元数据警告，不影响贴图资产生成。
- 最终以目标目录是否生成 `T_*.uasset`、材质是否引用 `T_*`、UE 是否能打开资产为准。

### 坑 8：FBX 导入警告不一定代表动画坏了

Legacy FBX 导入时可能出现类似警告：

```text
No smoothing group information was found
The following bones are missing from the bind pose
```

对这类低多边形/游戏导出模型，这些警告不一定是致命问题。本次批量导入里，动画数量、Skeleton、Skeletal Mesh、材质引用都通过资产级验证后，相关警告没有影响最终可用性。

真正需要警惕的是：

- AnimSequence 数量明显少于 GLB 动画数量。
- 播放动画时四肢拉长。
- 武器相对身体飞走。
- 材质完全白色且材质图里没有 TextureSample。

### 坑 9：批量转换时不要复用一个脏的 Blender 会话

批量处理多个 GLB 时，最好每个模型都用独立的 Blender 后台进程：

```powershell
& "D:\GameEngine\Blender Foundation\Blender 4.2\blender.exe" `
  -b `
  --python "D:\Path\To\export_one.py" `
  -- "C:\Users\ZYZ\Downloads\Air_Lux.glb" "D:\Temp\Air_Lux"
```

这样做的好处：

- 每次都是干净场景，避免上一个模型残留的 Mesh、Action、Material、Image 污染下一个模型。
- 导出日志和报告可以按模型分开保存。
- 某一个 GLB 转换失败时，不影响其他模型。
- 比在已经打开的交互式 Blender/MCP 会话中反复清场更稳定。

如果使用 MCP 或交互式 Blender 操作遇到 glTF import context、插件上下文或残留数据问题，优先切换到 `blender.exe -b --python` 的可重复批处理路线。

### 坑 10：临时 UE commandlet 编译时内存不足

如果为了批量修复材质临时添加 Editor commandlet，编译时可能遇到 UBA 因内存压力杀掉编译进程，例如：

```text
UbaSessionServer - Killed process Module.GAS.cpp - Low on memory
```

解决：

- 降低并行编译数，例如加 `-MaxParallelActions=1`。
- 关闭不必要的大型程序或 UE 里打开的重资源编辑器。
- 临时 commandlet 跑完后删除 `.h/.cpp`，撤回 `Build.cs` 里的 `UnrealEd`、`AssetRegistry` 等临时依赖。
- 撤回后重新编译一次项目，确认源码恢复正常。

## 批量导入补充记录：`*_Lux.glb`

这次又按同一流程批量导入了下载目录下所有 `*_Lux.glb` 文件，目标路径为：

```text
D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\Test\<模型名>
```

每个 GLB 单独一个目录，例如：

```text
Light_Lux.glb -> /Game/Assets/Characters/Test/Light_Lux
Fire_Lux.glb  -> /Game/Assets/Characters/Test/Fire_Lux
```

### 本次批量导入结果

| 模型目录 | 动画数量 | 材质数量 | 贴图数量 | 骨骼数量 | 顶点数量 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `Air_Lux` | 38 | 3 | 1 | 115 | 3963 |
| `Dark_Lux` | 59 | 2 | 1 | 115 | 4409 |
| `Fire_Lux` | 36 | 2 | 1 | 115 | 3374 |
| `Ice_Lux` | 33 | 3 | 1 | 115 | 3165 |
| `Light_Lux` | 63 | 4 | 1 | 115 | 3821 |
| `Magma_Lux` | 35 | 4 | 1 | 115 | 3655 |
| `Mystic_Lux` | 33 | 3 | 1 | 115 | 4190 |
| `Nature_Lux` | 36 | 4 | 1 | 109 | 4431 |
| `Storm_Lux` | 35 | 2 | 1 | 109 | 2991 |
| `Water_Lux` | 38 | 2 | 1 | 115 | 4474 |

### 最终 UE 资产检查结果

每个目录最终都应包含：

- 1 个 Skeletal Mesh：`<Name>`
- 1 个 Skeleton：`<Name>_Skeleton`
- 1 个 PhysicsAsset：`<Name>_PhysicsAsset`
- 与 GLB 动画数量一致的 AnimSequence，命名为 `<Name>_<AnimName>`
- 1 个 `T_*.uasset` 图集贴图
- 与源材质槽数量一致的材质资产

如果导入后资产名仍然包含 `_Full` 或 `_Full_Anim_Skeleton`，应在 UE 内使用 AssetTools 做批量重命名，不要直接改 `.uasset` 文件名。

本次批量导入最终检查结果：

| 模型目录 | 动画 | Mesh | Skeleton | PhysicsAsset | Texture |
| --- | ---: | ---: | ---: | ---: | ---: |
| `Air_Lux` | 38/38 | 1 | 1 | 1 | 1 |
| `Dark_Lux` | 59/59 | 1 | 1 | 1 | 1 |
| `Fire_Lux` | 36/36 | 1 | 1 | 1 | 1 |
| `Ice_Lux` | 33/33 | 1 | 1 | 1 | 1 |
| `Light_Lux` | 63/63 | 1 | 1 | 1 | 1 |
| `Magma_Lux` | 35/35 | 1 | 1 | 1 | 1 |
| `Mystic_Lux` | 33/33 | 1 | 1 | 1 | 1 |
| `Nature_Lux` | 36/36 | 1 | 1 | 1 | 1 |
| `Storm_Lux` | 35/35 | 1 | 1 | 1 | 1 |
| `Water_Lux` | 38/38 | 1 | 1 | 1 | 1 |

### 批量导入时遇到的问题和处理

1. **贴图和材质同名冲突**

   问题：FBX 导入后已经生成 `Air_Body` 材质，再导入 `Air_Body.png` 会失败。

   解决：把贴图复制/导出为 `T_Air_Body.png`，导入后材质引用 `T_Air_Body`。

2. **UE commandlet 返回非 0**

   问题：编辑器已打开时，命令行 UE 会加载 MCP 插件并尝试绑定 `127.0.0.1:8000`，端口被当前编辑器占用后 commandlet 返回失败码。

   解决：不只看 exit code；结合日志中的实际导入信息和目标 `.uasset` 文件验证。

3. **材质仍然发白**

   问题：FBX 自动生成的材质存在，但没有正确连接贴图。

   解决：用一次性 commandlet 原地修复材质资产：

   - 保留源材质名，例如 `Air_Body`。
   - 加载对应 `T_Air_Body` 贴图。
   - 清空材质表达式。
   - 新建 `TextureSample`。
   - RGB 接 `Emissive Color`。
   - Alpha 接 `Opacity Mask`。
   - 设置 `Unlit + Masked + Two Sided`。
   - 保存材质。
   - 检查 Skeletal Mesh 材质槽引用。

4. **批量导入中间文件容易污染项目**

   问题：批量流程会产生临时 FBX、PNG、JSON、PowerShell 脚本和临时 commandlet 源码。

   解决：导入验证完成后清理：

   ```text
   Saved\BatchLuxImport
   Saved\BatchLuxImport_export_one.py
   Saved\BatchLuxImport_create_ue_import_settings.ps1
   临时 UE commandlet .h/.cpp
   临时 Build.cs 依赖
   ```

   清理后再重新编译项目，确认源码恢复到正常状态。

### 批量导入推荐策略

如果以后需要批量导入同类 `*_Lux.glb`：

1. 每个 GLB 使用独立 Blender 后台进程转换，避免多个模型的数据块互相污染。
2. 每个模型输出独立 staging 目录。
3. 每个模型单独生成目标 UE 目录。
4. FBX 导入仍使用 legacy `FbxFactory`。
5. 贴图统一 `T_` 前缀。
6. 材质优先原地修复 FBX 自动生成的同名材质资产。
7. 验证动画数量等于 Blender/GLB 报告中的 animation/action 数量。
8. 导入完成后用 UE AssetTools 清理资产名中的 `_Full` 和 `_Full_Anim_Skeleton`。
9. 验证材质资产中能查到 `MSM_Unlit`、`BLEND_Masked` 和对应 `T_` 贴图名。
10. 验证完成后删除 staging 文件和临时代码。

## 推荐目录结构

建议最终目录保持清爽：

```text
/Game/Assets/Characters/Test/ElementalistLux
  ElementalistLux
  ElementalistLux_Skeleton
  ElementalistLux_PhysicsAsset
  ElementalistLux_Attack1_Skeleton
  ElementalistLux_Idle1_Skeleton
  ElementalistLux_Run_Base_Skeleton
  T_Light_Staff
  M_Light_Body
  M_Light_Collar
  M_Light_Staff
  M_Light_Staff_Ends
```

如果保留原始导入材质名，也可以是：

```text
Light_Body
Light_Collar
Light_Staff
Light_Staff_Ends
```

关键不是名字，而是：

- Skeletal Mesh 材质槽实际指向这些材质。
- 这些材质引用正确贴图。
- 材质设置为 Unlit/Masked/TwoSided。

## 快速检查清单

导入完成后逐项确认：

- [ ] Blender 中源 GLB 动画和材质正确。
- [ ] FBX 从 Blender 导出时启用了全部 Actions。
- [ ] 没有 Apply Armature Scale。
- [ ] UE 用 legacy `FbxFactory` 导入 FBX。
- [ ] UE 目标目录中 AnimSequence 数量等于源 GLB 动画数量。
- [ ] Skeleton 和 Skeletal Mesh 匹配。
- [ ] Texture2D 已导入。
- [ ] 材质中存在 TextureSample。
- [ ] 材质为 `MSM_Unlit`。
- [ ] 材质为 `BLEND_Masked`。
- [ ] 材质开启 Two Sided。
- [ ] Skeletal Mesh 材质槽引用的是修好的材质。
- [ ] 在 UE 中播放多个动画，没有四肢拉长或武器飞走。
- [ ] 已检查并删除目标目录中未被最终材质引用的多余 Texture2D。
- [ ] 清理临时导入目录和中间文件。
- [ ] 清理本次导入专用脚本、JSON、PowerShell、临时 commandlet 源码和编译产物。
- [ ] 如果临时修改过 `Build.cs`，已经恢复并重新编译通过。

## 本次成功经验摘要

这次真正有效的关键点有三个：

1. **导入动画靠 legacy FBX，不靠 UE 5.8 默认 GLB/Interchange。**
   默认路线可以导出部分动画，但不能稳定拿全动作；legacy `FbxFactory` 可以拿到完整 63 个动画。

2. **保持 Blender 的动画空间，不强行 Apply Armature Scale。**
   骨架静态看起来“干净”不代表动画正确。对这类 GLB，强行应用负缩放会破坏动作。

3. **材质单独修，不指望导入器自动接好。**
   源 GLB 是 unlit atlas 材质。UE 生成材质后如果没有接贴图，就手动或脚本创建 Unlit/Masked 材质，并赋回 Skeletal Mesh。

按这套流程做，可以把模型、骨骼、材质、贴图、动画稳定导入 UE，并避免“动画少、动画变形、材质白色”这三个主要问题。

