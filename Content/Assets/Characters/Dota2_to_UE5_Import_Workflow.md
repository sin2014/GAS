# Dota 2 Source 2 模型导入 UE5 工作流

本文记录把 Source2Viewer / ValveResourceFormat 导出的 Dota 2 角色资源，处理成 UE 5.8 可用资产的完整流程。重点记录 2026-07-05 最终成功的 Shadow Fiend / Nevermore 处理方案，因为这一次解决了之前反复出现的三个核心问题：

- UE 里模型方向需要手动 X -90 度才看起来正确。
- UE 里网格和 Skeleton 的方向、大小、空间不一致。
- 动画导入后变成静态、扭曲、只有单个部件，或者和角色网格错位。

## 硬性禁用：不要使用 UE Python

本项目的 UE 导入、资产修复、蓝图调整、Skeleton/Socket 调整流程中，禁止使用任何 UE Python 插件、UE Python 控制台、`-ExecutePythonScript`、Python commandlet、Editor Utility Python、蓝图里的 `ExecutePythonScript` 节点或其他依赖 UE PythonScriptPlugin 的脚本程序。

原因是已经在本项目 UE 5.8 环境中验证：UE Python 要么没有实际效果，要么会在启用或执行过程中导致编辑器崩溃。因此后续遇到 UE 资产批处理需求时，应使用项目 C++ 插件、Editor commandlet、Unreal MCP 暴露的非 Python 工具、手工编辑器操作，或离线 Blender/外部脚本处理源资产。Blender 预处理阶段可以继续使用 Blender 自己的 Python；本禁令只针对 UE 内部 Python / PythonScriptPlugin。

最终结论很明确：

**Dota 2 Source2Viewer 导出的 glTF 不能直接靠 UE 导入旋转或蓝图旋转修正。正确做法是在 Blender 预处理阶段把 Source2Viewer glTF 导入时隐藏在 Armature 对象上的轴向变换烘焙进网格顶点、骨架 rest pose 和动画 root location 轨道，然后用干净的对象变换导出 FBX。UE 导入时使用零旋转、统一缩放 1.0，并让第一个主 Skeletal Mesh 重新创建全新的 Skeleton。**

## 目录约定

本文使用的实际路径：

```text
Source2Viewer 原始导出：
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend

ShadowFiend 当前保留的 glTF 分组目录：
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\01_base_shadow_fiend
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\02_arcana_shadow_fiend_full_set
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\03_legacy_arcana_parts
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\04_fx_arcana_hand
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\05_fx_rocks
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\06_item_arms_deso

Blender 处理脚本：
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\shadowfiend_blender_export.py

Blender 处理输出：
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\processed_ue5

UE 项目：
D:\GameDev\Unreal_Projects\GAS\GAS.uproject

UE 目标 Content 路径：
/Game/Assets/Characters/Dota2/ShadowFiend

UE 磁盘目录：
D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\Dota2\ShadowFiend

UE 导入插件：
D:\GameDev\Unreal_Projects\GAS\Plugins\Dota2AssetImporter
```

## 最终成功结果

ShadowFiend 最后一轮成功导入后，UE 目标目录中资源数量为：

```text
Animations      152
Materials        17
Props             5
SkeletalMeshes   16
Textures         85
```

其中 `SkeletalMeshes` 的 16 个 `.uasset` 包含：

- 14 个 Skeletal Mesh 资产。
- 1 个主 Skeleton。
- 1 个 Physics Asset。

Blender manifest 中记录的原始输出为：

```text
skeletal_meshes  14
animations      160
props             4
textures         85
```

UE 中最终动画是 152 个，不是 160 个。原因是 8 个 Source2 动画条目是单帧 pose/lookFrame，`frame_start == frame_end == 0`，导入插件会主动跳过它们，避免 UE 里出现“看起来是动画但其实不动”的 AnimSequence。

被跳过的 8 个条目是：

```text
AN_ShadowFiend_pose01
AN_ShadowFiend_turns_lookFrame_0
AN_ShadowFiend_turns_lookFrame_1
AN_ShadowFiend_turns_lookFrame_2
AN_ShadowFiend_Arcana_pose01_arcana
AN_ShadowFiend_Arcana_turns_arcana_lookFrame_0
AN_ShadowFiend_Arcana_turns_arcana_lookFrame_1
AN_ShadowFiend_Arcana_turns_arcana_lookFrame_2
```

所以：

- 如果统计 Blender 导出的原始 FBX 动画文件，数量是 `160`。
- 如果统计 UE 中实际可用的 AnimSequence，正确数量是 `152`。

## Source2Viewer 导出原则

Source2Viewer 导出的 Dota 2 角色通常不是一个完整角色文件，而是一组模块：

- 主体：例如 `shadow_fiend.gltf`、`shadow_fiend_arcana.gltf`。
- 五个核心部件：body、head、arms、shoulders、wings。
- 饰品替换部件：例如 `arms_deso`。
- 展示或特效部件：例如 pedestal、arcana_hand、rocks。
- 贴图：color、normal、detailmask、specmask、orm、rimmask 等。
- 动画：普通款、至宝款、loadout、portrait、attack、run、cast、death、taunt 等。

建议导出时优先使用 glTF + 外部贴图，而不是期待单个 FBX 直接进 UE。Source2Viewer 负责“拿出资源”，Blender 负责“变成 UE 友好的中间包”，UE 插件负责“稳定导入并生成材质/动画/预览设置”。

## Dota 2 模型在 Blender 中的类型

处理 Dota 2 角色前，必须先判断每个组件属于哪一类。

### 类型 A：所有组件天然共用同一套骨架

表现：

- body、head、arms、wings 等 Armature 骨骼名和层级基本一致。
- 各组件只是不同 mesh，rest pose 一致。
- 任意一个 Action 播放时，各组件都能跟随同一套骨架。

处理：

- 保留多个 mesh 对象没有问题。
- 只保留一个主 Armature。
- UE 中可以导成一个合体 Skeletal Mesh，也可以导出多个模块 Skeletal Mesh，但必须共享同一个 Skeleton。

### 类型 B：组件只包含主骨架的子集

表现：

- 头、手、肩、翅膀、武器等只带自己用到的一小段骨骼。
- 骨骼名和主骨架部分重合，但不是完整骨架。
- 单独导入 UE 会生成自己的 Skeleton，后续组合会麻烦。

处理：

- 以主体骨架为主 Armature。
- 把组件 mesh 的 Armature Modifier 指向主 Armature。
- 确保组件 vertex group 名称能在主 Armature 中找到对应骨骼。
- 如果组件需要的骨骼主 Armature 没有，要按源 rest transform 加入主 Armature。

### 类型 C：组件有独立骨架，但语义上属于角色主体

表现：

- 翅膀、尾巴、某些至宝部件可能有独立骨骼。
- 这些骨骼不完全是主体骨架的子集，而是额外骨骼。
- UE 中分开导入会产生第二套 Skeleton。

处理：

- 如果目标是一个容易使用的角色，优先在 Blender 中把这些骨骼并入主 Armature。
- mesh 仍可保持分体，但所有核心部件要绑定到同一套主骨架。
- 动画统一导入到同一 Skeleton。

### 类型 D：静态或刚性挂件

表现：

- pedestal、展示底座、部分武器、装饰物没有真正变形需求。
- 只需要挂到某个 socket 或场景中。

处理：

- 导出 Static Mesh。
- UE 中作为 StaticMesh 导入到 `Props`。
- 如果以后需要跟随角色某个骨骼，再用 socket 或蓝图挂载。

### 类型 E：独立 FX / 召唤物 / 特效占位

表现：

- 有自己的骨架或特殊网格。
- 与角色主体动画不是同一套。
- Source2 中可能由粒子、材质、脚本驱动。

处理：

- 可以作为独立 Skeletal Mesh 或 Static Mesh 导入。
- 不要强行塞进角色主 Skeleton。
- `rocks` 这类资源在本次流程中被识别为 FX placeholder，UE FBX 管线不能稳定作为普通网格导入，插件会跳过。

## 为什么前几轮会失败

前几轮出错的根因不是“UE 某个选项没勾”，而是 Source2Viewer glTF 导入 Blender 后的空间关系被误读了。

Source2Viewer glTF 导入 Blender 后，Armature 对象上可能带有这样的对象矩阵：

```text
matrix_world ~= Source2AxisConversion * 0.0254
```

在 Blender 视口里看起来正常，是因为 mesh 作为子对象继承了 Armature 的对象变换。问题是 FBX/UE 导入时会把下面几类信息拆开解释：

- mesh 顶点数据
- Armature 对象变换
- 骨架 rest pose
- bind pose
- animation bone tracks
- root bone location tracks

如果只在 UE 里设置 `ImportRotateX=-90`，或者只在蓝图里把 Mesh 组件旋转 `X=-90`，只能让某个预览角度“看起来对”，但不能保证 SkeletalMesh、Skeleton、AnimSequence、Preview Mesh、Blueprint 组件都在同一空间。

前几轮常见错误：

- 在 UE 里用 `ImportRotateX=-90` 修模型，导致动画和骨架空间不一致。
- 在蓝图里旋转 `CharacterMesh0`，导致角色蓝图看起来对，但单独打开 SkeletalMesh/Skeleton/AnimSequence 仍然错。
- 用旧 Skeleton 复用新导入网格，旧 Skeleton 的 ref pose、比例、轴向与新 FBX 不完全一致，动画扭曲。
- 使用错误的 `ImportUniformScale`，让网格和骨架大小差出一个数量级。
- 只处理 mesh 或只处理 armature，没有同步处理动画 root location。

最终成功方案把问题提前解决在 Blender：

- 烘焙 Source2 轴向到 mesh 顶点。
- 烘焙 Source2 轴向到 Armature edit bones / rest pose。
- 烘焙 Source2 轴向到 root bone 的 location 动画轨道。
- 对象层面的 location、rotation、scale 全部归零/归一。
- 不把 `0.0254` 显示缩放烘焙进数据，保留 Source2 接近厘米的源单位。
- UE 导入时使用 `ImportUniformScale=1.0`、零旋转、全新 Skeleton。

## Blender 最终成功流程

ShadowFiend 的 Blender 自动处理脚本是：

```text
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\shadowfiend_blender_export.py
```

### 1. 清空 Blender 场景

每次导出前清空当前场景，删除旧 mesh、armature、material、image、action、collection 中无用户的数据，避免旧对象污染新导出。

### 2. 导入 Source2Viewer glTF

导入 `shadow_fiend` 目录下的主体、至宝、头、肩、手、翅膀、道具、FX 等 glTF 文件。导入后记录：

- 新增 Armature
- 新增 Mesh
- 新增 Action
- 新增贴图引用

同时过滤掉 glTF 导入器生成的低价值辅助 mesh，例如顶点数太少、没有材质、没有 Armature Modifier 的 helper object。

### 3. 建立 ShadowFiend 主骨架

以主体/至宝相关骨架为基础，建立角色级主 Armature。对不同来源部件中的骨骼做并集，确保核心部件需要的骨骼都存在于主骨架中。

处理要点：

- 保留主骨架层级。
- 对大小写或命名风格不同但语义相同的骨骼做 alias 映射。
- 从源 Armature 的 `data.bones` 读取 `head_local`、`tail_local`、`parent` 等 rest 信息，再写入目标 Armature 的 `edit_bones`。
- 不要在目标 Armature 的 Edit Mode 里直接读取另一个 Armature 的 `edit_bones`，这会导致读不到或读错源骨骼。

本次 ShadowFiend 最终主骨架骨骼数为 `106`。

### 4. 烘焙 Source2Viewer 轴向

最终脚本中使用的核心矩阵：

```python
SOURCE2_AXIS_TO_UE = Matrix((
    (0.0, 0.0, 1.0, 0.0),
    (-1.0, 0.0, 0.0, 0.0),
    (0.0, -1.0, 0.0, 0.0),
    (0.0, 0.0, 0.0, 1.0),
))
```

这一步是最终成功的关键。脚本会对每个导入 glTF 执行：

```python
normalize_source2_import(armatures, meshes, actions)
```

它做三件事：

1. 对 Armature 的 edit bones 执行 `edit_bone.transform(SOURCE2_AXIS_TO_UE, scale=True, roll=True)`。
2. 对 mesh 顶点数据执行 `mesh.data.transform(SOURCE2_AXIS_TO_UE)`。
3. 对 root bone 的 location 动画轨道执行同一轴向旋转。

然后把 Armature 和 mesh 对象变换清为：

```text
location = 0,0,0
rotation = 0,0,0
scale    = 1,1,1
matrix_world = identity
```

这保证导出 FBX 时，UE 看到的是干净的数据，而不是依赖 Blender 对象父子变换才能显示正确的数据。

### 5. 绑定核心部件到主 Armature

ShadowFiend 核心部件包括：

- Body
- Head
- Arms
- Shoulders
- Wings

脚本会把核心 mesh 的 Armature Modifier 指向主 Armature，并保持 vertex group 与骨骼名对应。UE 中这些部件可以单独替换，但它们必须共用同一个 ShadowFiend Skeleton。

### 6. 导出 Skeletal Mesh FBX

最终成功的 FBX 导出设置：

```python
bpy.ops.export_scene.fbx(
    filepath=str(filepath),
    use_selection=True,
    object_types={"ARMATURE", "MESH"},
    global_scale=1.0,
    apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_NONE",
    axis_forward="-Y",
    axis_up="Z",
    use_space_transform=True,
    bake_space_transform=False,
    use_mesh_modifiers=True,
    mesh_smooth_type="FACE",
    use_custom_props=False,
    add_leaf_bones=False,
    primary_bone_axis="Y",
    secondary_bone_axis="X",
    use_armature_deform_only=False,
    bake_anim=False,
)
```

注意：

- mesh FBX 不带动画。
- 主合体网格放在 manifest 的 `skeletal_meshes[0]`，由 UE 导入时创建主 Skeleton 和 PhysicsAsset。
- 分体部件也导出为单独 Skeletal Mesh FBX，用同一 Skeleton 导入，方便 UE 里换装。

### 7. 按 Action 单独导出动画 FBX

动画 FBX 只导出 Armature：

```python
object_types={"ARMATURE"}
bake_anim=True
bake_anim_use_all_bones=True
bake_anim_use_nla_strips=False
bake_anim_use_all_actions=False
bake_anim_force_startend_keying=True
```

每个 Action 单独导出一个 FBX，并在 manifest 中记录：

```json
{
  "name": "AN_ShadowFiend_Arcana_idle",
  "file": "D:/.../AN_ShadowFiend_Arcana_idle.fbx",
  "frame_start": 1.0,
  "frame_end": 154.0
}
```

UE 插件会利用 `frame_start` / `frame_end` 跳过单帧静态 pose/lookFrame。

### 8. 导出贴图和 manifest

Blender 输出目录结构：

```text
D:\GameDev\Unreal_Projects\Asset\Dota2\ShadowFiend\processed_ue5
  FBX
  Textures
  shadowfiend_export_manifest.json
```

manifest 是 UE 插件的输入契约，包含：

- `skeletal_meshes`
- `animations`
- `props`
- `textures`

## UE 最终成功导入流程

UE 导入由项目插件执行：

```text
D:\GameDev\Unreal_Projects\GAS\Plugins\Dota2AssetImporter
```

最终成功命令：

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

关键点：

- 不传 `-ImportRotateX=-90`。
- 不传 `-Skeleton=...` 复用旧骨架。
- `-ImportUniformScale=1.0`。
- 由第一个主 Skeletal Mesh 创建全新的 `SK_ShadowFiend_Modular_AllParts_Skeleton`。

### 1. 清理错误资产

重导入前，先删除前几轮错误导入产生的 UE 资产：

- 错误的 Skeletal Mesh
- 错误的 Skeleton
- 错误的 PhysicsAsset
- 错误的 AnimSequence
- 错误的 Props

Materials 和 Textures 可以保留并用 `-ReplaceExisting` 覆盖，也可以一起重建。关键是不要用旧 Skeleton 迁就新网格。

### 2. 导入贴图

插件先导入贴图并根据文件名配置：

| 文件名特征 | UE 设置 | 材质用途 |
| --- | --- | --- |
| `_color` | sRGB=true, Default compression | Base Color |
| `_normal` | sRGB=false, Normalmap compression | Normal |
| `_orm` | sRGB=false, Masks compression | R=AO, G=Roughness, B=Metallic |
| `_specmask` | sRGB=false, Masks compression | R=Specular |
| `_detailmask` | sRGB=false, Masks compression | 当前只归档，不强接 |
| `_rimmask` | sRGB=false, Masks compression | 当前只归档，不强接 |

mask 贴图必须使用 `TC_Masks`，材质里的 Texture Sample 也必须是 `SAMPLERTYPE_Masks`。否则 UE SM6 会报：

```text
Sampler type is Linear Color, should be Masks
```

### 3. 自动创建材质

插件按贴图组自动创建 PBR 材质：

- `color.RGB -> Base Color`
- `normal.RGB -> Normal`
- `orm.R -> Ambient Occlusion`
- `orm.G -> Roughness`
- `orm.B -> Metallic`
- `specmask.R -> Specular`

Dota 2 翅膀、披风、尖刺、薄片类 mesh 常见单面面片，所以角色材质默认开启 Two Sided，避免背面剔除导致部件消失。

### 4. 导入 Skeletal Mesh

插件按 manifest 的 `skeletal_meshes` 顺序导入：

1. 第一个 `SK_ShadowFiend_Modular_AllParts` 是主合体网格，用来创建主 Skeleton 和 PhysicsAsset。
2. 后续 13 个分体部件使用同一个主 Skeleton 导入。

本次 14 个 Skeletal Mesh 条目：

```text
SK_ShadowFiend_Modular_AllParts
SK_ShadowFiend_Base_Body
SK_ShadowFiend_Arcana_Body
SK_ShadowFiend_Base_Arms
SK_ShadowFiend_Arcana_Arms
SK_ShadowFiend_Item_Arms_Deso
SK_ShadowFiend_Base_Head
SK_ShadowFiend_Arcana_Head
SK_ShadowFiend_Legacy_Head_Arcana
SK_ShadowFiend_Base_Shoulders
SK_ShadowFiend_Arcana_Shoulders
SK_ShadowFiend_Base_Wings
SK_ShadowFiend_Arcana_Wings
SK_ShadowFiend_Legacy_Arcana_Wings
```

### 5. 导入 Props / FX

manifest 中 props 有 4 个：

```text
SM_ShadowFiend_legacy_pedestal_Part1
SM_ShadowFiend_arcana_pedestal_Part1
SK_ShadowFiend_rocks
SK_ShadowFiend_arcana_hand
```

处理策略：

- pedestal 作为 Static Mesh 导入。
- arcana_hand 作为独立 FX Skeletal Mesh 导入。
- rocks 被识别为 FX placeholder，本次跳过，不作为普通角色部件导入。

UE 中 Props 文件夹最终有 5 个资产，是因为静态/骨骼导入会附带生成相关资产，例如独立 Skeleton 或 PhysicsAsset。

### 6. 导入动画

插件把动画 FBX 导入到主 Skeleton：

```text
/Game/Assets/Characters/Dota2/ShadowFiend/SkeletalMeshes/SK_ShadowFiend_Modular_AllParts_Skeleton
```

导入规则：

- 正常 Action 导入为 AnimSequence。
- `frame_end - frame_start < 1.0` 的单帧 pose/lookFrame 跳过。
- 不为每个动画创建自己的 Skeleton。
- 导入后把 Skeleton Preview Mesh 和每个 AnimSequence Preview Mesh 设置为完整合体网格 `SK_ShadowFiend_Modular_AllParts`。

这样打开任意 `AN_ShadowFiend_*` 动画时，预览都是完整角色，而不是只有某个翅膀或某个部件。

### 7. 配置角色蓝图

ShadowFiend 角色蓝图使用模块化部件结构。父类 `GASCharacterBase` 已经有主 Mesh 组件，因此五个主体部件中：

- Body 使用继承来的 `CharacterMesh0`。
- Head / Arms / Shoulders / Wings 使用额外的 SkeletalMeshComponent。

蓝图结构：

```text
NewBlueprint
  CapsuleComponent
  Arrow
  CharacterMesh0        -> Body，播放 AnimBlueprint
    Dota2Head           -> Head，Leader Pose 跟随 CharacterMesh0
    Dota2Arms           -> Arms，Leader Pose 跟随 CharacterMesh0
    Dota2Shoulders      -> Shoulders，Leader Pose 跟随 CharacterMesh0
    Dota2Wings          -> Wings，Leader Pose 跟随 CharacterMesh0
  Weapon
  CharMoveComp
```

关键规则：

- 所有核心部件都必须共用同一个 Skeleton。
- 分体组件相对位置/旋转/缩放保持 `0,0,0 / 0,0,0 / 1,1,1`。
- 不在蓝图组件里用 `X=-90` 修正资产方向。
- 后续换装只替换对应部件的 Skeletal Mesh，不换 Skeleton。
- 动画只在主 Mesh 播放，分体通过 Leader Pose 跟随。

## 手动导入 UE 时的设置

如果不用插件，手动导入也要遵守相同原则。

### 主 Skeletal Mesh

- Import Skeletal Mesh：开启。
- Skeleton：第一次导入选 None，让 UE 创建新 Skeleton。
- Import Content Type：Geometry and Skin Weights。
- Combine Skeletal Meshes：按需求，合体预览网格建议合并。
- Import Morph Targets：没有 morph 时关闭。
- Create Physics Asset：开启，让 UE 生成基础 PhysicsAsset。
- Import Materials / Import Textures：可以关闭，建议单独按贴图语义创建材质。
- Import Rotation：`0,0,0`。
- Import Uniform Scale：`1.0`。
- Use T0 As Ref Pose：默认不要开，除非明确发现 ref pose 错误。

### 分体 Skeletal Mesh

- Skeleton：选择主 Skeleton。
- Import Rotation：`0,0,0`。
- Import Uniform Scale：`1.0`。
- 不要让每个部件生成自己的 Skeleton。

### 动画 FBX

- Import Animations：开启。
- Import Mesh：关闭，或选择 Animation Only。
- Skeleton：选择主 Skeleton。
- Animation Length：Source Timeline。
- Import Bone Tracks：开启。
- Import Rotation：`0,0,0`。
- Import Uniform Scale：`1.0`。
- 不导入单帧 pose/lookFrame 为正式动画。

## 验证清单

导入后必须逐项检查：

1. 打开主 Skeletal Mesh，模型方向正确，不需要在资产里补旋转。
2. 打开 Skeleton，骨架方向和大小与网格匹配。
3. 打开多个 AnimSequence，动画播放时网格不飞、不缩、不扭曲。
4. 打开 attack、run、death、cast、idle、loadout 等不同动作，确认不是全部 idle。
5. AnimSequence 的 Skeleton 都指向同一个主 Skeleton。
6. Animation Preview Mesh 是完整角色合体网格。
7. 材质无 SM6 sampler 报错。
8. ORM/spec/detail/rim mask 贴图使用 Masks 类型。
9. Blueprint 中 Body 使用 `CharacterMesh0`，其他四个核心部件使用 Leader Pose。
10. Blueprint 组件没有用 `X=-90` 修资产方向。

## 失败症状与修复方向

### UE 预览里网格正确，Skeleton 很小或躺着

原因通常是 mesh 和 armature 只在 Blender 对象父子变换下看起来一致，导出后 UE 把它们拆开解释。

修复：

- 回 Blender 烘焙 Source2 轴向到 mesh 顶点和 armature edit bones。
- 对象 transform 清零/归一。
- UE 重新创建 Skeleton，不复用旧 Skeleton。

### 蓝图里旋转 -90 后看起来正确，但动画资产预览仍然错

原因是蓝图只修正实例组件，不修正资产本身。

修复：

- 不在蓝图里修资产方向。
- 回 Blender 重新导出干净 FBX。
- UE 用零导入旋转重导入。

### 动画变成静态或全是 idle

原因可能是：

- Blender 导出动画时没有切换 active Action。
- 导出了 NLA 里同一个默认动作。
- UE 导入时没有读取骨骼轨道。
- 导入了单帧 pose/lookFrame。

修复：

- 每个 Action 单独导出 FBX。
- `bake_anim_use_all_actions=False`，只导出当前 action。
- manifest 记录 frame range。
- UE 跳过单帧静态条目。

### 材质灰球或报 ComponentMask / Sampler Type

原因是 FBX 自动材质不可靠，mask 贴图被当成 Linear Color 或未连接输入。

修复：

- 不依赖 FBX 材质。
- 按 color/normal/orm/specmask 语义重建 UE 材质。
- mask 贴图和采样器都用 Masks。

## 清理策略

应该保留：

- Source2Viewer 原始 glTF/bin/贴图。
- 角色分组目录，如果已经人工整理过。
- 可复现 Blender 导出脚本。
- 最后一轮成功的 `processed_ue5` 输出包，至少在确认不需要重导入前保留。
- 最后一轮成功 UE 导入日志。
- `Dota2AssetImporter` 插件源码和 README。

可以删除：

- 明确失败轮次的 UE 导入日志。
- 临时测试 FBX，例如 `_test_normalized_base.fbx`。
- 临时测试 `.fbm` 贴图目录。
- UE 插件 `Intermediate/` 生成目录。
- 不再需要的旧临时插件。
- Codex 工作区里的临时验证脚本。
- 角色专用的一次性资产修复 commandlet / 脚本。它们如果写死了某个角色的资产路径、socket 名、组件名或网格名，用完并确认资产已保存后应删除，或迁移成参数化通用工具；不要长期留在通用导入插件里。

谨慎删除：

- `processed_ue5`。它是生成产物，但包含可直接重导入 UE 的最终 FBX/贴图/manifest。只有在确认 Blender 脚本和 Source2Viewer 原始文件足以复现，并且不再需要快速重导入时再删。
- 插件 `Binaries/`。这是编译产物，但如果当前 UE 正在使用插件，Windows 可能锁定 DLL；删除后下次需要重新编译插件。

## 插件职责

`Dota2AssetImporter` 是通用导入插件，不应该每个角色都新建临时插件。它负责：

- 读取 Blender 输出的 manifest。
- 批量导入贴图。
- 设置贴图 sRGB 和 compression。
- 按 Dota 贴图命名创建 PBR 材质。
- 导入主 Skeletal Mesh 并创建 Skeleton/PhysicsAsset。
- 在同一次完整导入中，让后续分体 Skeletal Mesh 复用本次新建的主 Skeleton。
- 导入 Static Props。
- 导入独立 FX Skeletal Mesh。
- 跳过单帧 pose/lookFrame 动画。
- 批量导入 AnimSequence。
- 设置 Skeleton 和 AnimSequence 的 Preview Mesh。
- 正式导入时禁止通过 `-Skeleton=` 复用项目里已经存在的 Skeleton；该参数只允许用于 `-VerifyOnly` 只读验证。
- 提供 manifest 驱动的只读验证：`-VerifyOnly -VerifyManifest="...json" -Skeleton="/Game/...Skeleton.Skeleton"`。
- 可选配置 ShadowFiend 当前项目蓝图。

插件不负责：

- 修正 Source2Viewer glTF 的轴向/缩放根因。
- 在 UE 里强行旋转错误 FBX。
- 自动复刻 Source2 粒子、材质特效、技能特效。
- 把所有 Dota 英雄的换装逻辑自动抽象成通用系统。

## Codex 操作 UE 资产的推荐方式

本项目已经验证：Codex 直接修改 UE5 资产时，最有效、最稳定的路线是 **非 Python 的项目 C++ Editor 工具**，例如 Editor commandlet、Editor subsystem、或由 MCP 调用的 C++ 工具函数。原因是这些方式能使用 UE 原生类型系统和资产 API，直接加载、修改、编译、保存指定 package，并且能输出明确日志。

推荐优先级：

1. 项目 C++ Editor commandlet / Editor subsystem。
2. Unreal MCP 暴露的非 Python 工具，内部仍应调用 C++ 资产 API。
3. 手工编辑器操作，用于少量视觉调参、检查和最终确认。
4. 外部脚本只处理 UE 外部源文件，例如 Blender、FBX、贴图、manifest；不要让外部脚本直接改 `.uasset` 二进制。

禁止路线：

- UE Python、PythonScriptPlugin、`-ExecutePythonScript`、Python commandlet。
- 直接字符串或二进制改 `.uasset`。
- 用 UI 自动化批量点编辑器控件来做结构性资产修改。
- 写死角色路径和组件名的一次性 commandlet 长期留在通用插件里。

Codex 写 C++ 资产修改工具时必须遵守：

- 工具必须参数化：目标 Skeleton、Blueprint、socket、组件、mesh、动画都应来自参数或 manifest，不应写死到代码里。
- 必须支持只改某一类资产，例如 `SkeletonOnly`、`BlueprintOnly`、`DryRun`。
- 必须只保存明确目标 package，不保存当前编辑器里无关资产。
- 正式导入新 Skeletal Mesh 时不要传入已有 Skeleton。UE FBX 导入器可能会把传入的 Skeleton package 标记 dirty、写入新 FBX 的 root/armature 信息，并在保存时破坏旧网格和旧动画绑定。
- 必须只修改被请求字段。比如只修 Blueprint socket 引用时，不要重置 mesh、动画、材质、相对位置、旋转、缩放。
- 必须打印 before / after 摘要：旧 socket、新 socket、旧 mesh、新 mesh、保存了哪些 package。
- 修改 socket 名时，必须同步迁移所有引用：Skeleton preview attached assets、Blueprint component socket、动画通知、特效挂点、蓝图节点里的字符串引用等。不能只改 Skeleton 名。
- 如果目标资产正在编辑器中打开，应提醒用户关闭重开或重启编辑器；旧内存窗口可能覆盖新落盘资产。

后续可以把这套能力做成通用工具：

- 在 `Dota2AssetImporter` 中保留通用导入能力，不放角色专用修补逻辑。
- 新增一个通用 Editor 工具模块或 MCP 工具集，例如 `UEAssetOps`。
- MCP schema 可以提供明确操作：
  - `skeleton.ensure_socket`
  - `skeleton.attach_preview_asset`
  - `skeleton.rename_socket_and_migrate_references`
  - `blueprint.set_component_socket`
  - `blueprint.set_component_mesh`
  - `animation.set_preview_mesh`
  - `asset.save_packages`
- MCP 工具内部必须是 C++ 实现或调用 C++ subsystem，不走 UE Python。
- 工具默认 dry-run，用户明确执行后才保存。

这类通用化是值得做的：它能让 Codex 以后稳定操作 Skeleton、Blueprint、AnimSequence、Preview Mesh、socket、组件挂点等资产，同时避免每个角色写一份临时 commandlet。

## Invoker Kid 补充记录

`Invoker_kid` 按 ShadowFiend 最后成功流程进入第二阶段，但有自己的硬约束：

- 核心主体部件共用一个新主 Skeleton：`SK_InvokerKid_Modular_AllParts_Skeleton`。
- `exort`、`quas`、`wex` 三个小精灵各自使用独立 Skeleton，不并入角色主 Skeleton。
- `trainer_dragon` 使用独立 Skeleton，不并入角色主 Skeleton。
- `TrainerSpirits` 和 `TrainerDragon` 是最终目录名。它们是人工新增目录，资产已经从旧的 `Spirits` / `Dragon` 迁移过来，后续流程不要再改回旧目录。
- 默认不拆 `invoker_kid.gltf` 里的 head 材质区。

导入阶段边界：

- 导入日志里如果出现历史遗留的 SavePackage / FileManager / 打开的编辑器锁文件类报错，不要直接把它判定为导入失败。
- 当导入流程已经输出完成摘要、目标资产已经落盘，并且用户确认导入成功时，按“导入已完成/已成功”处理。
- Codex 后续只负责确保导入命令完成和目标资产写入成功；导入 UE 后的视觉检查、资产检查和问题反馈由用户手工完成。

本次遇到的问题和难点：

- `invoker_kid` 主体、三只小精灵、`trainer_dragon` 不是同一种骨架关系。主体需要共用新主 Skeleton，小精灵和龙则保持独立 Skeleton。
- 小精灵视觉上围绕角色，但资产结构上不是角色主 Skeleton 的普通分体部件，因此不能简单并入主 Skeleton。
- `TrainerSpirits` / `TrainerDragon` 是人工迁移后的新目录，自动化流程不能再假设旧的 `Spirits` / `Dragon` 路径。
- Skeleton socket、Skeleton 视口预览附件、Blueprint 子组件是三套不同数据。创建 socket 不会自动显示预览网格；创建 Blueprint 组件也不会自动出现在 Skeleton 视口。
- 修改 Skeleton socket 名会影响 Blueprint 组件挂点引用。若 Blueprint 组件仍指向旧 socket，UE 会找不到挂点，组件可能回落到父组件原点，看起来像掉到角色脚底。
- 当前打开的 UE 资产窗口可能持有旧内存版本。外部 commandlet 保存 `.uasset` 后，如果旧窗口再保存，可能覆盖新落盘内容。
- 自动工具如果默认重置 relative transform、scale、动画模式、mesh 或材质，会破坏用户已经手工调好的位置、大小和预览状态。

本次值得继续保持的做法：

- 保持 ShadowFiend 成功流程里的核心原则：Blender 预处理修正轴向和骨架根因，UE 导入使用零旋转、统一缩放 1.0、新 Skeleton。
- 用 manifest 明确资产路径、动画、分体、贴图语义，避免靠人工记忆。
- 导入成功标准按“命令完成、目标资产落盘、用户确认成功”判断，不被历史遗留日志误导。
- 小精灵和龙保持独立 Skeleton，主体核心部件共用主 Skeleton，这个边界清晰。
- 用 C++ commandlet 进行 UE 资产写入比 UE Python 稳定，但一次性角色修补代码用完后要删除，避免后续误运行。
- 把目录迁移、Skeleton 关系、socket/组件映射写进文档，后续复盘和迁移都更快。

本次做得不好的地方和后续必须避免：

- 不应该把 Invoker 专用修复 commandlet 长期留在 `Dota2AssetImporter` 通用插件中。
- 不应该在用户已经手工修改蓝图后，再运行可能覆盖 Blueprint 组件配置的工具。
- 不应该只改 Skeleton socket 名，而没有同时检查 Blueprint 组件的旧 socket 引用。
- 不应该在未确认用户要保留哪些相对 transform / scale / animation 设置时，用工具重置它们。
- 当用户说“只改 Skeleton”时，也必须考虑外部引用会不会因 Skeleton 改名而失效；“只保存 Skeleton”不等于“不会影响蓝图显示”。

当前最终三小精灵挂点规则：

```text
Skeleton: /Game/Assets/Characters/Dota2/Invoker_kid/SkeletalMeshes/SK_InvokerKid_Modular_AllParts_Skeleton

bone orb1 -> socket SO_Orb1_Quas  -> component QuasSpirit  -> /Game/Assets/Characters/Dota2/Invoker_kid/TrainerSpirits/SK_InvokerKid_Quas
bone orb2 -> socket SO_Orb2_Exort -> component ExortSpirit -> /Game/Assets/Characters/Dota2/Invoker_kid/TrainerSpirits/SK_InvokerKid_Exort
bone orb3 -> socket SO_Orb3_Wex   -> component WexSpirit   -> /Game/Assets/Characters/Dota2/Invoker_kid/TrainerSpirits/SK_InvokerKid_Wex
```

Skeleton 视口预览规则：

- `SO_Orb1_Quas` / `SO_Orb2_Exort` / `SO_Orb3_Wex` 只是 socket 挂点，本身不会让小精灵网格在 Skeleton 编辑器中显示。
- `BP_InvokerKid` 里的三个 `USkeletalMeshComponent` 只影响角色蓝图和运行时预览，不会自动成为 Skeleton/Persona 视口中的预览附加物。
- 如果需要在 Skeleton 编辑器里直接看到三只小精灵，必须把三只 `TrainerSpirits` Skeletal Mesh 写入 Skeleton 的 `PreviewAttachedAssetContainer`，并分别附加到对应 socket。
- 曾经使用过一次性 C++ commandlet 完成三层写入：创建/更新 socket，写入 Skeleton 预览附加资产，创建/更新 `BP_InvokerKid` 三个组件。该 commandlet 是 Invoker 专用修补代码，不是通用导入功能，后续已从插件源码中清理。
- 如果 commandlet 已经保存成功但当前打开的 Skeleton 视口没有立刻显示小精灵，通常是编辑器窗口仍持有旧的内存版本；关闭并重新打开该 Skeleton 资产，或重启编辑器后再检查。不要在旧窗口里直接保存覆盖新落盘数据。
- 当前最终 Skeleton 预览顺序为：`orb1` 显示 Quas，`orb2` 显示 Exort，`orb3` 显示 Wex；也就是 `SO_Orb2_Exort -> SK_InvokerKid_Exort` 在中间位。

### Invoker Kid Dark Artistry 替换部件记录

源目录：

```text
D:\GameDev\Unreal_Projects\Asset\Dota2\Invoker_kid\dark_artistry_kid
```

它和 `invoker_kid.gltf` 的关系：

- `dark_artistry_kid` 是 Invoker Kid 的饰品 / loadout 替换部件，不是另一个完整英雄。
- 这些部件整体以原始 `invoker_kid.gltf` 的基础骨架为参照，但不是每个部件都和主体 100% 同骨骼。
- `invoker_kid_dark_artistry_arms.gltf` 是主骨架子集，可以直接复用现有 Invoker Kid 主 Skeleton。
- `armor`、`back`、`head`、`shoulder`、`magus_apex` 带有饰品专用额外骨骼，例如裙摆、背饰、头发、肩饰末端骨骼。
- 如果把这些额外骨骼并入已有 `SK_InvokerKid_Modular_AllParts_Skeleton`，会改变已经导入并手工调整过的主 Skeleton，风险很高。

事故记录和修正结论：

- 曾经尝试把 Dark Artistry 部件预处理成“折叠额外骨骼、复用现有主 Skeleton”的 FBX，并通过 `-Skeleton=/Game/.../SK_InvokerKid_Modular_AllParts_Skeleton` 导入。
- 这个做法是错误的。即使设置了 `bUpdateSkeletonReferencePose=false`，UE FBX 导入器仍可能把传入的 Skeleton package 标记为 dirty，并把新 FBX 的 root / armature 信息写入 Skeleton。
- 本次事故中，正式 `SK_InvokerKid_Modular_AllParts_Skeleton.uasset` 被保存成包含 `ARM_InvokerKid_DarkArtistry_ExistingSkeleton`、缺少 `ARM_InvokerKid_Master` 的状态，导致 `SK_InvokerKid_Modular_AllParts`、`SK_InvokerKid_BodyHead` 和运行/攻击动画加载时出现 missing bones。
- 正确恢复方式是用 `Saved/Autosaves/.../SK_InvokerKid_Modular_AllParts_Skeleton_Auto1.uasset` 恢复正式 Skeleton。该 autosave 同时包含 `ARM_InvokerKid_Master` 和 `SO_Orb1_Quas` / `SO_Orb2_Exort` / `SO_Orb3_Wex`。
- 错误导入到 Content 里的 `DarkArtistry` UE 资产已经隔离到 `Saved/CodexRecovery/InvokerKidDarkArtistry_BadImport`，不要再把它们移回 Content 或打开使用。
- 后续 `Dota2Import` 已改成：正式导入时如果传入 `-Skeleton=` 会直接拒绝；`-Skeleton=` 只允许用于 `-VerifyOnly` 只读验证。

当前可保留的源侧预处理产物：

- `dark_artistry_kid_blender_export.py` 和 `processed_ue5_dark_artistry` 可以作为分析参考保留。
- 但这批输出的 UE 导入策略不可继续使用，因为它以复用正式 Skeleton 为目标。
- 后续要重做 Dark Artistry，必须走 Superset Skeleton 测试路线。

权重折叠审计：

```text
Arms       -> 无额外骨骼，纯主骨架子集
Armor      -> 裙摆额外骨骼折叠到 thigh_0_L / thigh_0_R / pelvis
Back       -> 背饰额外骨骼折叠到 spine_3
Head       -> 头发额外骨骼折叠到 head
Shoulder   -> 肩饰额外骨骼折叠到 spine_3
MagusApex  -> 头发额外骨骼折叠到 head
```

Blender 预处理脚本：

```text
D:\GameDev\Unreal_Projects\Asset\Dota2\Invoker_kid\dark_artistry_kid_blender_export.py
```

Blender 输出：

```text
D:\GameDev\Unreal_Projects\Asset\Dota2\Invoker_kid\processed_ue5_dark_artistry

FBX:
SK_InvokerKid_DarkArtistry_Armor.fbx
SK_InvokerKid_DarkArtistry_Arms.fbx
SK_InvokerKid_DarkArtistry_Back.fbx
SK_InvokerKid_DarkArtistry_Head.fbx
SK_InvokerKid_DarkArtistry_Shoulder.fbx
SK_InvokerKid_DarkArtistry_MagusApex.fbx

Manifest:
D:\GameDev\Unreal_Projects\Asset\Dota2\Invoker_kid\processed_ue5_dark_artistry\invoker_kid_dark_artistry_export_manifest.json
```

错误导入时使用过的 UE 目标，当前已隔离，不再作为有效 Content 目录：

```text
/Game/Assets/Characters/Dota2/Invoker_kid/DarkArtistry
D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\Dota2\Invoker_kid\DarkArtistry
```

错误导入命令，禁止再使用：

```powershell
& "D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\GameDev\Unreal_Projects\GAS\GAS.uproject" `
  -run=Dota2Import `
  -Manifest="D:\GameDev\Unreal_Projects\Asset\Dota2\Invoker_kid\processed_ue5_dark_artistry\invoker_kid_dark_artistry_export_manifest.json" `
  -Dest="/Game/Assets/Characters/Dota2/Invoker_kid/DarkArtistry" `
  -Character=InvokerKidDarkArtistry `
  -Skeleton="/Game/Assets/Characters/Dota2/Invoker_kid/SkeletalMeshes/SK_InvokerKid_Modular_AllParts_Skeleton.SK_InvokerKid_Modular_AllParts_Skeleton" `
  -ImportUniformScale=1.0 `
  -unattended -nop4 -nosplash -NoSound -stdout -FullStdOutLogOutput
```

错误导入曾产生的资产，当前已从 Content 移走：

```text
SkeletalMeshes:
SK_InvokerKid_DarkArtistry_Armor
SK_InvokerKid_DarkArtistry_Arms
SK_InvokerKid_DarkArtistry_Back
SK_InvokerKid_DarkArtistry_Head
SK_InvokerKid_DarkArtistry_Shoulder
SK_InvokerKid_DarkArtistry_MagusApex

Textures: 35
Materials: 10
PhysicsAsset: 1 个，随第一个 Armor 部件自动生成
Animations: 0
```

本次 UE FBX 导入器出现过 bind pose 对话框栈 / `Not valid bind pose` 日志，进程退出码曾为 1。后来虽然用 manifest 验证确认 DarkArtistry 资产“加载并绑定到了主 Skeleton”，但这个验证没有发现主 Skeleton 本身已经被 UE FBX 导入器改写 root/armature 信息。因此，今后不能把“新资产能加载并绑定到旧 Skeleton”作为安全证据；还必须确认旧 Skeleton 没有被 dirty/save，也没有改变 root/骨骼列表。

只读验证命令：

```powershell
& "D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "D:\GameDev\Unreal_Projects\GAS\GAS.uproject" `
  -run=Dota2Import `
  -Dest="/Game/Assets/Characters/Dota2/Invoker_kid/DarkArtistry" `
  -VerifyOnly `
  -VerifyManifest="D:\GameDev\Unreal_Projects\Asset\Dota2\Invoker_kid\processed_ue5_dark_artistry\invoker_kid_dark_artistry_export_manifest.json" `
  -Skeleton="/Game/Assets/Characters/Dota2/Invoker_kid/SkeletalMeshes/SK_InvokerKid_Modular_AllParts_Skeleton.SK_InvokerKid_Modular_AllParts_Skeleton" `
  -VerifyReport="D:\GameDev\Unreal_Projects\Asset\Dota2\Invoker_kid\processed_ue5_dark_artistry\reports\ue_manifest_verify_dark_artistry.json" `
  -unattended -nop4 -nosplash -NoSound -stdout -FullStdOutLogOutput
```

当时错误导入后的验证结果，仅作为事故复盘记录，不代表可用：

```text
Assets=52
SkeletalEntries=6
Errors=0
6 个 SK_InvokerKid_DarkArtistry_* 全部绑定到：
/Game/Assets/Characters/Dota2/Invoker_kid/SkeletalMeshes/SK_InvokerKid_Modular_AllParts_Skeleton
```

后续正确路线：

- 先在 Blender 中构建 `InvokerKid Superset Skeleton`：保留 `ARM_InvokerKid_Master` 名称、保留旧基础骨骼层级/rest pose，再加入 Dark Artistry 的裙摆、背饰、头发、肩饰等额外骨骼。
- 把原版 BodyHead/Cape/Shoulder/Sleeves/Hair 和 DarkArtistry Armor/Arms/Back/Head/Shoulder/MagusApex 全部重新绑定到这个 Superset Skeleton。
- 先导入到测试目录，例如 `/Game/Assets/Characters/Dota2/Invoker_kid/SupersetTest`，创建新的测试 Skeleton，不覆盖正式 `SkeletalMeshes` 目录。
- 动画也要重新导入或迁移到 Superset Skeleton，确保 run / attack / cast / idle 等关键动画可用后，再讨论是否替换正式资产。
- 不允许再通过 `-Skeleton=` 把新饰品 FBX 直接导入到正式 `SK_InvokerKid_Modular_AllParts_Skeleton`。

2026-07-07 修复记录：

```text
恢复来源：
D:\GameDev\Unreal_Projects\GAS\Saved\Autosaves\Game\Assets\Characters\Dota2\Invoker_kid\SkeletalMeshes\SK_InvokerKid_Modular_AllParts_Skeleton_Auto1.uasset

恢复目标：
D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\Dota2\Invoker_kid\SkeletalMeshes\SK_InvokerKid_Modular_AllParts_Skeleton.uasset

坏文件备份：
D:\GameDev\Unreal_Projects\GAS\Saved\CodexRecovery\InvokerKidSkeleton\SK_InvokerKid_Modular_AllParts_Skeleton.bad_20260707_065803.uasset

错误 DarkArtistry UE 资产隔离目录：
D:\GameDev\Unreal_Projects\GAS\Saved\CodexRecovery\InvokerKidDarkArtistry_BadImport\DarkArtistry_bad_20260707_065932

恢复后验证报告：
D:\GameDev\Unreal_Projects\GAS\Saved\CodexRecovery\InvokerKidSkeleton\verify_after_restore_2.json
```

恢复后验证结果：

```text
Assets=195
Errors=0
SK_InvokerKid_Modular_AllParts -> SK_InvokerKid_Modular_AllParts_Skeleton OK
SK_InvokerKid_BodyHead/Cape/Shoulder/Sleeves/Hair -> 主 Skeleton OK
AN_InvokerKid_idle_anim/run_anim/attack_anim/cast/death2 -> 主 Skeleton OK
TrainerSpirits 和 TrainerDragon 独立 Skeleton/预览动画 OK
```

组件规则：

- 三个组件都创建在角色蓝图 `BP_InvokerKid` 的主网格体 `CharacterMesh0` 下。
- 三个组件类型都是 `USkeletalMeshComponent`。
- 三个组件分别设置 `AttachToName` / attachment socket 为对应 `SO_Orb*_...`。
- 相对位置、旋转、缩放保持 `0,0,0 / 0,0,0 / 1,1,1`，具体视觉偏移如果需要由后续手工检查后再调整。
- 三个组件禁用碰撞，只作为小精灵外观/占位组件。

2026-07-06 清理记录：

- 已清理 Codex 工作区临时验证脚本。
- 已从 `Dota2AssetImporter` 源码中删除 Invoker 专用 `InvokerKidSpiritSetupCommandlet`。
- UE 编辑器关闭后，已清理 `Dota2AssetImporter` 插件的 `Intermediate/` 和 `Binaries/` 生成目录。当前插件目录只保留 `Source/`、`.uplugin` 和 `README.md`；下次打开项目或运行命令前需要重新编译插件。

## 新角色复用流程

处理下一个 Dota 2 英雄时，按这个顺序走：

1. Source2Viewer 导出 glTF/bin/贴图/动画。
2. 按 body/head/arms/shoulders/wings/weapon/prop/fx 分组。
3. 在 Blender 脚本中配置源文件列表、角色名、骨骼 alias。
4. 导入 glTF 后立即执行 Source2 轴向烘焙。
5. 建立主 Armature，合并核心部件骨骼并集。
6. 让核心部件 mesh 绑定到主 Armature。
7. 导出主合体 Skeletal Mesh。
8. 导出分体 Skeletal Mesh。
9. 每个 Action 单独导出动画 FBX。
10. 导出 props、textures、manifest。
11. 用 `Dota2AssetImporter` 导入 UE。
12. 不使用 UE 导入旋转修根因。
13. 第一次导入使用新 Skeleton。
14. 验证模型、Skeleton、动画、材质、蓝图。
15. 只清理失败产物和临时测试文件，保留可复现链路。

## 版权和使用范围

Dota 2 资源归 Valve / 相关权利方所有。即使只用于学习练习，也建议：

- 仅在本地学习、技术验证、非商业项目中使用。
- 不把包含受版权保护模型、贴图、动画的内容用于商业发布。
- 如果公开展示，注意平台规则和版权风险。
