# StormHeroes_to_UE5_Import_Workflow

本文档记录《风暴英雄》（Heroes of the Storm）角色模型导入 UE5 的可复用流程。

当前已验证角色：Ragnaros（拉格纳罗斯）

当前目标工程：

- UE 项目：`D:\GameDev\Unreal_Projects\GAS\GAS.uproject`
- UE 内容目录：`D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\StormHeroes`
- UE 资源路径：`/Game/Assets/Characters/StormHeroes`
- HotS 源工程目录：`D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm`
- 已修复 Blender 文件：`D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Ragnaros_Blender.blend`
- UE5 中间导出目录：`D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Assets\ArtPackage\Heroes of the Storm\Role-V1\Ragnaros\processed_ue5`

重要约束：

- 本项目禁止使用 UE 内部 Python 自动化导入资源。
- 可以使用 Blender Python / Blender MCP 处理模型、骨骼、材质和动画。
- UE 自动化导入优先使用非 Python 方案，例如 `ImportAssets` commandlet、C++ commandlet、Editor Utility、MCP 或手动编辑器操作。
- 不要直接复用旧 Skeleton。骨骼修复后应为角色创建新的 Skeleton，再导入动画。

## 1. 资源结构判断

`D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm` 不是 UE 工程，而是 Unity 风格资源工程。里面有 `Assets`、`ProjectSettings` 等 Unity 项目结构，角色资源主要在：

```text
D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Assets\ArtPackage\Heroes of the Storm
```

角色通常按 `Role-V*` 分类。Ragnaros 当前使用的源文件为：

```text
模型 FBX：
D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Assets\ArtPackage\Heroes of the Storm\Role-V1\Ragnaros\FBX\Ragnaros.fbx

动画 FBX：
D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Assets\ArtPackage\Heroes of the Storm\Role-V1\Ragnaros\FBX\Ragnaros-Animation.fbx

贴图目录：
D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Assets\ArtPackage\Heroes of the Storm\Role-V1\Ragnaros\Materials\Textures
```

HotS 资源常见特点：

- 模型 FBX 和动画 FBX 经常分开。
- Unity `.mat`、`.controller`、`.prefab` 可以作为参考，但不能直接作为 UE 最终资源。
- 贴图通常是 PNG，命名中包含 `Diff`、`Norm`、`Spec`、`Emis` 等语义。
- 动画 FBX 可能包含独立动画骨架，但这个骨架不一定适合直接绑定模型。
- 同一个英雄可能有多个皮肤或多个独立 FBX，导入前需要先整理模型、皮肤、动画和贴图的对应关系。

## 2. Ragnaros 最终 UE 导入结果

Ragnaros 已导入到：

```text
D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\StormHeroes\Ragnaros
```

UE 资源路径：

```text
/Game/Assets/Characters/StormHeroes/Ragnaros
```

当前导入结果：

| 类型 | 数量 | 说明 |
| --- | ---: | --- |
| SkeletalMesh | 1 | `SK_Ragnaros` |
| Skeleton | 1 | `SK_Ragnaros_Skeleton` |
| PhysicsAsset | 1 | `SK_Ragnaros_PhysicsAsset` |
| AnimSequence | 53 | 每个动作一个独立 AnimSequence |
| Material | 2 | Body、FireSkirt |
| Texture | 6 | Diff / Norm / Spec / Emis |

主要 UE 资源：

```text
SK_Ragnaros.uasset
SK_Ragnaros_Skeleton.uasset
SK_Ragnaros_PhysicsAsset.uasset
Materials\Ragnaros_MAT_Body_Rebuilt.uasset
Materials\Ragnaros_MAT_FireSkirt_Rebuilt.uasset
Textures\T_Ragnaros_Base_Diff.uasset
Textures\T_Ragnaros_Base_Emis.uasset
Textures\T_Ragnaros_Base_Norm.uasset
Textures\T_Ragnaros_Base_Spec.uasset
Textures\T_Ragnaros_FireSkirt_Diff.uasset
Textures\T_Ragnaros_FireSkirt_Emis.uasset
Animations\SK_Ragnaros_Anim_*.uasset
```

动画资源已拆成独立 AnimSequence，但当前使用 all-in-one FBX 导入，因此动画资源名较长，例如：

```text
SK_Ragnaros_Anim_Ragnaros_ModelRig_CORRECT_SKIN_Ragnaros_Attack
SK_Ragnaros_Anim_Ragnaros_ModelRig_CORRECT_SKIN_Ragnaros_Stand
SK_Ragnaros_Anim_Ragnaros_ModelRig_CORRECT_SKIN_Ragnaros_Walk
```

这是可用结果，但不是最干净的命名结果。后续若要做生产级批量导入，建议使用“模型 FBX + 每个动画一个 FBX + C++ commandlet/导入清单”的方式，让动画命名变成：

```text
AS_Ragnaros_Attack
AS_Ragnaros_Stand
AS_Ragnaros_Walk
```

## 3. Blender 总体工作流

推荐先在 Blender 中修复所有问题，再导出一个 UE 友好的 FBX。

Ragnaros 成功流程概览：

1. 导入模型 FBX：`Ragnaros.fbx`。
2. 导入动画 FBX：`Ragnaros-Animation.fbx`。
3. 判断哪个骨架是模型蒙皮骨架，哪个骨架只是动画源骨架。
4. 保留模型蒙皮骨架作为最终骨架。
5. 不把模型重新绑定到动画源骨架。
6. 通过约束把动画源骨架的动作烘焙到模型蒙皮骨架。
7. 每个源动画烘焙成一个独立 Blender Action。
8. 删除动画源骨架和临时约束，只留下最终模型、最终骨架、独立 Actions。
9. 重建材质节点，确认贴图色彩空间。
10. 导出 UE 用 FBX。
11. 用 UE 非 Python 导入流程导入模型、骨骼、材质、贴图和动画。

## 4. 判断正确骨架

Ragnaros 中有两个关键骨架：

| 骨架来源 | 骨骼数 | 用途 | 是否作为最终蒙皮骨架 |
| --- | ---: | --- | --- |
| `Ragnaros.fbx` | 193 | 模型实际蒙皮骨架 | 是 |
| `Ragnaros-Animation.fbx` | 95 | 动画源骨架 | 否 |

判断标准：

- 模型网格的 vertex groups 是否能在骨架中全部找到对应骨骼。
- 武器、裙摆、身体、脸部等权重是否绑定在这个骨架上。
- 动画 FBX 的骨架是否缺少模型使用的骨骼。
- 动画 FBX 是否只有动画，没有真正的角色网格。

Ragnaros 的动画源骨架缺少一批模型实际使用的脸部骨骼，例如：

```text
Bone_Cheek_L
Bone_Cheek_R
Bone_EyebrowA_L
Bone_EyebrowA_R
Bone_EyebrowB_L
Bone_EyebrowB_R
Bone_MouthCorner_L
Bone_MouthCorner_R
Bone_Mouth_Lower
Bone_Mouth_Lower_L
Bone_Mouth_Lower_R
Bone_Mouth_Upper
Bone_Mouth_Upper_L
Bone_Mouth_Upper_R
Bone_NoseTip
Bone_Nostril_L
Bone_Nostril_R
```

结论：

- `Ragnaros-Animation.fbx` 的骨架“有骨骼”，但不是正确的最终蒙皮骨架。
- 最终必须使用 `Ragnaros.fbx` 里的 193 骨模型骨架。
- 动画 FBX 的 95 骨骨架只能作为动作来源，不能拿来替换模型骨架。

## 5. 之前遇到的问题和错误操作

### 5.1 错误：把模型直接绑定到动画 FBX 骨架

表现：

- 武器不在角色手上。
- 手臂、手指、身体发生扭曲。
- 动画越修越乱。
- 脸部、嘴部、眉毛等细节骨骼丢失或无法正确驱动。

原因：

- 动画 FBX 骨架只有 95 根骨。
- 模型 FBX 的真实蒙皮骨架有 193 根骨。
- 动画骨架缺少模型 mesh 的一部分 vertex group 对应骨骼。
- 直接换骨架会破坏模型原本的 bind pose 和权重关系。

规避：

- 永远先检查模型 mesh 的 vertex groups 和骨架骨骼是否一一对应。
- 不要因为动画骨架“能播放动作”就把它当成最终骨架。
- 模型骨架和动画骨架不一致时，动画只能 retarget/bake 到模型骨架。

### 5.2 错误：直接复制动画曲线到模型骨架

表现：

- 每个 Action 看起来都有曲线。
- 动画名字正确。
- 但姿态扭曲，尤其是手臂、武器、身体朝向明显不对。

原因：

- 两套骨架的 rest pose、局部坐标、父子空间不完全一致。
- 直接复制 F-Curve 只复制了局部 transform 数值，没有把源骨架世界空间姿态转换到目标骨架。
- 同名骨骼不等于同一套坐标语义。

规避：

- 不直接复制 F-Curve。
- 使用 `Copy Transforms` 约束在世界空间跟随源骨架。
- 再用 visual keying / `nla.bake` 烘焙到模型骨架。

### 5.3 错误：误判武器骨骼

Ragnaros 的锤子模型看起来像独立武器，但实际与身体在同一套 mesh/权重系统中。之前容易把某些 mesh/root marker 当成武器驱动节点。

实际应关注的武器相关骨骼包括：

```text
Bone_Weapon
Bone_Weapon_Smear
Bone_Weapon_Smear01
```

规避：

- 不要只看对象名判断武器绑定。
- 用 vertex group 和权重检查武器 mesh 实际受哪些骨骼驱动。
- 修复武器位置时优先检查骨骼/权重/动画空间，不要手动挪 mesh。

### 5.4 错误：在 Blender 里选错对象预览 Action

表现：

- 动画、模型、骨骼其实已经正确。
- 但所有动画看起来都像同一个预览图里的动作。
- 误以为所有 Action 都被烘焙成同一个动作。

原因：

- Blender Action Editor / Dope Sheet 当前选中的不是最终 Armature。
- 或者看到的是 mesh/camera/旧对象的 action 数据。
- 动作确实存在，但没有切换到最终骨架上预览。

规避：

- 预览前先选中最终骨架：`Ragnaros_ModelRig_CORRECT_SKIN`。
- 在 Action Editor 中切换 `Ragnaros_*` Action。
- 确认场景里没有旧动画源骨架、旧 NLA track 干扰。
- 确认最终骨架没有残留约束。

### 5.5 错误：把多个动画做成一个长条

表现：

- UE 或 Blender 里只看到一个很长的动画。
- 无法在动画摄影表或 AnimSequence 中单独选择动作。

原因：

- 把多个动作合并到一个 Timeline/NLA Strip。
- 或 FBX 导出时只按当前时间线导出，没有按 Actions 导出。

规避：

- Blender 中每个动作必须是独立 Action。
- 不使用 NLA 合并成长条。
- FBX 导出时使用 `bake_anim_use_all_actions=True`。
- UE 导入时启用 `bImportAnimations=True`。

### 5.6 错误：把非骨骼动画也当成角色动作

Ragnaros 动画 FBX 导入 Blender 后曾出现 106 个 Action：

- 53 个是真正的骨骼 pose 动画。
- 53 个是类似 `Camera.002` 的非角色动作曲线。

规避：

- 只处理绑定到动画源 Armature / PoseBone 的 Action。
- 过滤 camera、empty、mesh object transform 等非骨骼 Action。
- 烘焙前检查 Action 的 fcurves 是否指向 `pose.bones[...]`。

### 5.7 错误：在 UE 导入阶段修轴向、修比例、修骨骼

表现：

- UE 中看似可以通过 Import Rotation、Scale 修正方向。
- 但骨骼、动画、物理资产和后续重导入会更难维护。

规避：

- 坐标、单位、骨骼和动画问题优先在 Blender 修干净。
- UE 导入时保持稳定参数：
  - `ImportUniformScale=1.0`
  - 不临时旋转骨架。
  - 不复用旧 Skeleton。
  - 不在 UE 内修改 reference pose 来掩盖源数据问题。

### 5.8 注意：UE commandlet 返回非 0 不一定代表导入失败

本次 Ragnaros 导入时，`UnrealEditor-Cmd.exe -run=ImportAssets` 返回 exit code 1，但资源已经成功生成。

日志中的主要非致命错误：

```text
LogHttpListener: Error: HttpListener unable to bind to 127.0.0.1:8000
LogAutomatedImport: Error: Invalid Destination Path (): FilenameToLongPackageName failed to convert ''
```

原因：

- UE 编辑器已经打开，占用了 MCP/HTTP 端口。
- `ImportAssets` commandlet 在结束阶段有空路径噪声。

判断导入是否成功应看：

- `.uasset` 是否生成。
- UE log 是否出现 `FactoryCreateFile: SkeletalMesh`、`Building Anim DDC data`、`Importing animation` 等记录。
- Content Browser 中资源是否可打开。
- SkeletalMesh 是否绑定正确 Skeleton。
- AnimSequence 数量是否和 Blender Actions 数量一致。

### 5.9 错误：把 `processed_ue5` 放在 HotS 工程根目录

之前曾把 Ragnaros 的 UE 中间导出目录放在：

```text
D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\processed_ue5\Ragnaros
```

这个位置不适合长期保留，因为多个英雄的中间产物会脱离自己的源资源目录，后续排查皮肤、动画、贴图来源会变乱。

正确规范：

```text
D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Assets\ArtPackage\Heroes of the Storm\<RoleFolder>\<HeroEnglishName>\processed_ue5
```

Ragnaros 当前正确位置：

```text
D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Assets\ArtPackage\Heroes of the Storm\Role-V1\Ragnaros\processed_ue5
```

规避：

- 每个英雄自己的 `processed_ue5` 必须放在该英雄源目录下面。
- 不要在 HotS Unity 工程根目录建立全局 `processed_ue5`。
- manifest 中记录源 FBX、源贴图、修复后 Blender 文件和导出 FBX，便于从英雄目录直接追踪整条链路。

### 5.10 错误：UE 导入后所有资产堆在英雄根目录

第一次导入 Ragnaros 后，SkeletalMesh、Skeleton、PhysicsAsset、AnimSequence、Material、Texture 全部落在：

```text
D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\StormHeroes\Ragnaros
```

这个结构短期能用，但不适合后续管理。角色目录应当根目录只放核心模型资产，动画、材质、贴图分别进入英文子目录。

正确规范：

```text
/Game/Assets/Characters/StormHeroes/<Hero>
  SK_<Hero>
  SK_<Hero>_Skeleton
  SK_<Hero>_PhysicsAsset
  Animations
    ...
  Materials
    ...
  Textures
    ...
```

规避：

- UE 导入后立即用 UE 资产系统移动资产，不要手动移动 `.uasset` 文件。
- 子目录使用 UE 常规英文命名：`Animations`、`Materials`、`Textures`。
- 模型、骨架、物理资产保留在角色根目录。
- AnimSequence 必须进入 `Animations`。
- Material 必须进入 `Materials`。
- Texture / Texture2D 必须进入 `Textures`。

### 5.11 注意：UE 编辑器打开资产会锁住 `.uasset`

整理 Ragnaros 目录时，UE 编辑器中仍打开了 Ragnaros 的 Skeleton 和一个 AnimSequence，导致 commandlet 移动/删除旧包时出现 Windows 文件锁：

```text
LogFileManager: Warning: MoveFile was unable to move ...
Error Code 32
LogSavePackage: Error: Error saving ...
```

结果：

- 部分资产已经移动到子目录。
- 根目录留下同名 `ObjectRedirector` 或旧包残留。
- 再次运行整理工具时可能遇到 `Destination already exists`。

规避：

- 整理或修复 redirector 前，关闭 UE 中打开的相关资产编辑器标签。
- 最稳妥做法是关闭整个 UE Editor，只用 `UnrealEditor-Cmd.exe` 跑整理和修复。
- 整理工具必须支持重复运行和半整理恢复。
- 如果已经产生 redirector，先保存引用它的包，再删除 redirector。

### 5.12 注意：不要在 commandlet 中调用会弹 Slate UI 的 redirector 修复

`AssetTools::FixupReferencers` 在 commandlet 环境中可能尝试创建 Slate 报告窗口，导致：

```text
Assertion failed: CurrentBaseApplication.IsValid()
SFixupRedirectorsReport
```

规避：

- StormHeroes 整理 commandlet 只负责移动/合并资产，不在内部调用可能弹 UI 的 `FixupReferencers`。
- redirector 收尾使用 UE 自带 `ResavePackages -FixupRedirectors`。
- 若 `PackageFolder` 没有删除 redirector，则显式传入 redirector 包和引用它的包再跑一次。

## 6. 成功的 Blender 修复流程

### 6.1 导入源 FBX

在 Blender 中：

1. 清空场景。
2. 导入模型 FBX：`Ragnaros.fbx`。
3. 导入动画 FBX：`Ragnaros-Animation.fbx`。
4. 重命名关键对象，避免混淆：

```text
最终模型骨架：
Ragnaros_ModelRig_CORRECT_SKIN

身体与武器 mesh：
Ragnaros_BodyAndWeapon_Mesh

火焰裙摆 mesh：
Ragnaros_FireSkirt_Mesh

临时动画源骨架：
Ragnaros_Animation_SourceRig_TEMP
```

### 6.2 检查蒙皮骨架

检查每个 mesh 的 vertex groups：

- 所有有权重的 vertex group 都应在最终模型骨架中找到同名骨骼。
- 如果动画源骨架缺少这些 vertex groups 对应骨骼，不允许用它替换模型骨架。

Ragnaros 最终有效结构：

```text
Armature:
  Ragnaros_ModelRig_CORRECT_SKIN

Meshes:
  Ragnaros_BodyAndWeapon_Mesh
  Ragnaros_FireSkirt_Mesh

Actions:
  Ragnaros_Attack
  Ragnaros_Attack_01
  ...
  Ragnaros_Stand
  Ragnaros_Walk
```

### 6.3 通过约束烘焙动画

正确方式是把动画源骨架的世界空间姿态烘焙到模型骨架。

核心原则：

- 源骨架：只提供动作。
- 目标骨架：最终模型蒙皮骨架。
- 同名骨骼：建立 `Copy Transforms` 约束。
- 约束空间：使用世界空间。
- 烘焙方式：visual keying。
- 烘焙结果：每个源 Action 生成一个目标 Action。
- 烘焙完成后：删除约束和源骨架。

Blender Python 逻辑示例：

```python
import bpy

target_rig = bpy.data.objects["Ragnaros_ModelRig_CORRECT_SKIN"]
source_rig = bpy.data.objects["Ragnaros_Animation_SourceRig_TEMP"]

common_bones = [
    name for name in target_rig.pose.bones.keys()
    if name in source_rig.pose.bones
]

for bone_name in common_bones:
    target_pb = target_rig.pose.bones[bone_name]
    con = target_pb.constraints.new(type="COPY_TRANSFORMS")
    con.name = "BAKE_FROM_SOURCE_TEMP"
    con.target = source_rig
    con.subtarget = bone_name
    con.target_space = "WORLD"
    con.owner_space = "WORLD"

for action in source_actions:
    source_rig.animation_data.action = action
    target_action = bpy.data.actions.new(action.name.replace("Armature Object|", "Ragnaros_"))
    target_rig.animation_data_create()
    target_rig.animation_data.action = target_action

    frame_start, frame_end = map(int, action.frame_range)
    bpy.context.view_layer.objects.active = target_rig
    target_rig.select_set(True)

    bpy.ops.nla.bake(
        frame_start=frame_start,
        frame_end=frame_end,
        only_selected=False,
        visual_keying=True,
        clear_constraints=False,
        clear_parents=False,
        use_current_action=True,
        bake_types={"POSE"},
    )

for pb in target_rig.pose.bones:
    for con in list(pb.constraints):
        if con.name == "BAKE_FROM_SOURCE_TEMP":
            pb.constraints.remove(con)
```

注意：

- 上面是流程示例，不是固定脚本。不同英雄可能需要增加骨名映射或 rest pose 校正。
- 如果同名骨骼姿态仍不正确，先检查源/目标骨骼局部轴和 rest pose，而不是直接改 mesh。
- 烘焙后要逐个 Action 抽查站立、走路、攻击、死亡等动作。

### 6.4 清理场景

烘焙成功后，最终 Blender 文件应满足：

- 场景只保留最终 Armature 和最终 Mesh。
- 删除动画源 Armature。
- 删除临时约束。
- 没有旧 NLA track 干扰。
- 每个动作是独立 Action。
- Action 名称有角色前缀，例如 `Ragnaros_Stand`。
- Mesh 只绑定到最终 Armature。
- 武器位置跟随最终骨架动作。

Ragnaros 当前已验证结果：

```text
Armature:
  Ragnaros_ModelRig_CORRECT_SKIN

Bone count:
  193

Meshes:
  Ragnaros_BodyAndWeapon_Mesh
  Ragnaros_FireSkirt_Mesh

Actions:
  53 independent Ragnaros_* actions

NLA tracks:
  None
```

## 7. 材质重建规范

HotS 的 Unity 材质不能直接作为 UE 材质使用。建议在 Blender 先重建可读材质，并确保导出 FBX 时能带出贴图引用。

Ragnaros 当前材质：

```text
Ragnaros_MAT_Body_Rebuilt
Ragnaros_MAT_FireSkirt_Rebuilt
```

贴图命名规范：

```text
T_Ragnaros_Base_Diff.png
T_Ragnaros_Base_Emis.png
T_Ragnaros_Base_Norm.png
T_Ragnaros_Base_Spec.png
T_Ragnaros_FireSkirt_Diff.png
T_Ragnaros_FireSkirt_Emis.png
```

Blender 中推荐材质连接：

Body：

- `Diff` -> Principled BSDF Base Color，sRGB。
- `Norm` -> Normal Map -> Principled BSDF Normal，Non-Color。
- `Spec` -> Specular / Roughness 参考通道，Non-Color。
- `Emis` -> Emission Color / Strength，sRGB 或按实际贴图检查。

FireSkirt：

- `Diff` -> Base Color / Alpha 参考。
- `Emis` -> Emission。
- 如果需要透明火焰效果，在 Blender 中设置 blend mode，UE 中仍需复查材质混合模式。

UE 自动导入材质注意事项：

- FBX 自动材质能保留贴图引用，但不会总是生成理想的火焰/透明/发光效果。
- 本次 UE 自动导入后 Body 材质已引用 Diff、Emis、Norm、Spec。
- FireSkirt 材质已引用 Diff、Emis，但 UE 自动导入结果仍是普通材质，需要在 UE 中复查透明、双面、发光强度。
- 生产流程建议后续编写非 Python 的 `StormHeroesAssetImporter` 或材质修复 commandlet，统一设置：
  - Body：Default Lit，Opaque，Normal，Specular/Roughness，Emission。
  - Fire：Unlit 或 Default Lit，Two Sided，Translucent/Additive，Emission。

## 8. UE5 导出前的中间目录规范

每个英雄必须导出到自己源资源目录下的独立中间目录：

```text
D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Assets\ArtPackage\Heroes of the Storm\<RoleFolder>\<HeroEnglishName>\processed_ue5
```

Ragnaros 当前目录：

```text
D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Assets\ArtPackage\Heroes of the Storm\Role-V1\Ragnaros\processed_ue5
```

建议结构：

```text
processed_ue5
  FBX
    SK_Ragnaros.fbx
    SK_Ragnaros.fbm
      T_Ragnaros_*.png
  Textures
    T_Ragnaros_Base_Diff.png
    T_Ragnaros_Base_Emis.png
    T_Ragnaros_Base_Norm.png
    T_Ragnaros_Base_Spec.png
    T_Ragnaros_FireSkirt_Diff.png
    T_Ragnaros_FireSkirt_Emis.png
  stormheroes_ragnaros_export_manifest.json
```

说明：

- `Textures` 是主动维护的贴图中间目录。
- `FBX\SK_Ragnaros.fbm` 是 Blender FBX 导出器为了外部贴图引用自动生成的伴随目录，可以保留。
- 不允许再使用 HotS 工程根目录下的全局 `processed_ue5`。

命名建议：

- Skeletal Mesh FBX：`SK_<Hero>.fbx`
- Skeleton：UE 导入后自动生成 `SK_<Hero>_Skeleton`
- PhysicsAsset：UE 导入后自动生成 `SK_<Hero>_PhysicsAsset`
- Body 材质：`M_<Hero>_Body` 或保留 Blender 材质名后再统一重命名
- Texture：必须使用 `T_` 前缀，避免和材质/mesh 同名冲突
- Animation：建议未来改为 `AS_<Hero>_<Action>`

## 9. Blender FBX 导出规范

Ragnaros 当前使用 all-in-one FBX 导出：一个 FBX 包含模型、骨架、材质引用和全部动作。

成功导出文件：

```text
D:\GameDev\Unreal_Projects\Asset\风暴英雄\Heroes of the Storm\Assets\ArtPackage\Heroes of the Storm\Role-V1\Ragnaros\processed_ue5\FBX\SK_Ragnaros.fbx
```

Blender FBX 导出关键参数：

```python
bpy.ops.export_scene.fbx(
    filepath=str(fbx_path),
    use_selection=False,
    object_types={"ARMATURE", "MESH"},
    add_leaf_bones=False,
    bake_anim=True,
    bake_anim_use_all_actions=True,
    bake_anim_use_nla_strips=False,
    bake_anim_use_all_bones=True,
    bake_anim_force_startend_keying=True,
    bake_anim_step=1.0,
    bake_anim_simplify_factor=0.0,
    path_mode="COPY",
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

说明：

- `add_leaf_bones=False`：禁止 Blender 额外添加 leaf bones。
- `bake_anim_use_all_actions=True`：导出所有独立 Action。
- `bake_anim_use_nla_strips=False`：避免 NLA 长条干扰。
- `bake_anim_simplify_factor=0.0`：不简化动画曲线，避免动作细节丢失。
- `path_mode="COPY"` 且 `embed_textures=False`：复制贴图引用，不把贴图嵌入 FBX。
- `use_selection=False`：本次 Blender MCP 环境中 `use_selection=True` 遇到上下文 selected_objects 问题，因此使用干净场景 + object_types 导出。

导出后必须做一次反向检查：

1. 新开 Blender 空场景。
2. 导入导出的 `SK_Ragnaros.fbx`。
3. 检查是否有 1 个 Armature。
4. 检查骨骼数是否仍为 193。
5. 检查 mesh 是否为 2 个。
6. 检查 Action 是否为 53 个。
7. 抽查 `Stand`、`Walk`、`Attack`、`Death` 是否不同且姿态正确。

注意：

- Blender 反向导入 all-in-one FBX 时，Action 名可能带上 Armature 前缀，这是 FBX 导入器行为。
- UE 也可能生成较长动画资源名。功能可用，但命名不够理想。

## 10. UE5 导入规范

### 10.1 推荐目标路径

每个英雄单独一个目录：

```text
/Game/Assets/Characters/StormHeroes/<HeroEnglishName>
```

Ragnaros：

```text
/Game/Assets/Characters/StormHeroes/Ragnaros
```

磁盘路径：

```text
D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\StormHeroes\Ragnaros
```

### 10.2 ImportAssets JSON 示例

本次 Ragnaros 使用临时导入配置：

```text
C:\Users\ZYZ\Documents\Codex\2026-07-06\new-chat\stormheroes_ragnaros_legacy_import.json
```

核心配置：

```json
{
  "ImportGroups": [
    {
      "FileNames": [
        "D:/GameDev/Unreal_Projects/Asset/风暴英雄/Heroes of the Storm/Assets/ArtPackage/Heroes of the Storm/Role-V1/Ragnaros/processed_ue5/FBX/SK_Ragnaros.fbx"
      ],
      "DestinationPath": "/Game/Assets/Characters/StormHeroes/Ragnaros",
      "FactoryName": "/Script/UnrealEd.FbxFactory",
      "bReplaceExisting": true,
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
          "bPreserveSmoothingGroups": true,
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
          "bImportBoneTracks": true,
          "bPreserveLocalTransform": false,
          "bImportMeshesInBoneHierarchy": true,
          "ImportUniformScale": 1.0,
          "bConvertScene": true,
          "bForceFrontXAxis": false,
          "bConvertSceneUnit": true
        }
      }
    }
  ]
}
```

### 10.3 Commandlet 命令

使用 UE 非 Python commandlet：

```powershell
& 'D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\GameDev\Unreal_Projects\GAS\GAS.uproject' `
  -run=ImportAssets `
  -importsettings='C:\Users\ZYZ\Documents\Codex\2026-07-06\new-chat\stormheroes_ragnaros_legacy_import.json' `
  -nosourcecontrol `
  -unattended `
  -nop4 `
  -nosplash `
  -NoSound `
  -stdout `
  -FullStdOutLogOutput
```

导入后检查：

```powershell
Get-ChildItem 'D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\StormHeroes\Ragnaros' -Filter *.uasset
```

也可以按类型统计：

```powershell
Get-ChildItem 'D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\StormHeroes\Ragnaros' -Filter *.uasset |
  Group-Object {
    if ($_.Name -like '*.Skeleton.uasset' -or $_.Name -like '*_Skeleton.uasset') { 'Skeleton' }
    elseif ($_.Name -like '*PhysicsAsset.uasset') { 'PhysicsAsset' }
    elseif ($_.Name -like 'SK_*.uasset' -and $_.Name -notlike '*_Anim_*') { 'SkeletalMesh' }
    elseif ($_.Name -like '*_Anim_*') { 'AnimSequence' }
    elseif ($_.Name -like 'T_*.uasset') { 'Texture' }
    else { 'Material/Other' }
  }
```

### 10.4 UE 内容目录分层规范

导入完成后，角色目录必须整理成固定结构：

```text
D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\StormHeroes\<Hero>
  SK_<Hero>.uasset
  SK_<Hero>_Skeleton.uasset
  SK_<Hero>_PhysicsAsset.uasset
  Animations
    *.uasset
  Materials
    *.uasset
  Textures
    *.uasset
```

UE 包路径：

```text
/Game/Assets/Characters/StormHeroes/<Hero>
  /Animations
  /Materials
  /Textures
```

规则：

- SkeletalMesh、Skeleton、PhysicsAsset 保留在 `<Hero>` 根目录。
- AnimSequence 统一移动到 `Animations`。
- Material 统一移动到 `Materials`。
- Texture / Texture2D 统一移动到 `Textures`。
- 不要在资源管理器里手动移动 `.uasset` 文件。
- 必须通过 UE 资产系统移动，让硬引用、软引用和 redirector 正常处理。

Ragnaros 当前已整改结果：

```text
Root:        3  (SK_Ragnaros, SK_Ragnaros_Skeleton, SK_Ragnaros_PhysicsAsset)
Animations: 53
Materials:  2
Textures:   6
```

### 10.5 StormHeroesOrganize commandlet

项目中新增了 `StormHeroesAssetImporter` 编辑器插件，提供目录整理 commandlet：

```text
D:\GameDev\Unreal_Projects\GAS\Plugins\StormHeroesAssetImporter
```

用途：

- 扫描一个 StormHeroes 英雄目录。
- 将 AnimSequence 移动到 `Animations`。
- 将 Material 移动到 `Materials`。
- 将 Texture / Texture2D 移动到 `Textures`。
- 保留 SkeletalMesh、Skeleton、PhysicsAsset 在根目录。
- 遇到上次整理中断后留下的同名残留时，合并 stale duplicate 到目标资产。

运行前要求：

- 关闭 UE Editor 中打开的相关角色资产。
- 最稳妥是关闭整个 UE Editor，只保留 `UnrealEditor-Cmd.exe` 执行。
- 如果 UE Editor 正在运行且打开了相关资产，Windows 可能锁住 `.uasset`，导致 Error Code 32。

运行命令：

```powershell
& 'D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\GameDev\Unreal_Projects\GAS\GAS.uproject' `
  -run=StormHeroesOrganize `
  -HeroPath=/Game/Assets/Characters/StormHeroes/Ragnaros `
  -nosourcecontrol `
  -unattended `
  -nop4 `
  -nosplash `
  -NoSound `
  -stdout `
  -FullStdOutLogOutput
```

整理后必须修复 redirector：

```powershell
& 'D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\GameDev\Unreal_Projects\GAS\GAS.uproject' `
  -run=ResavePackages `
  -PackageFolder=/Game/Assets/Characters/StormHeroes/Ragnaros `
  -FixupRedirectors `
  -SearchAllAssets `
  -AutoCheckOutPackages=false `
  -IgnoreChangelist `
  -NoSourceControl `
  -unattended `
  -nop4 `
  -nosplash `
  -NoSound `
  -stdout `
  -FullStdOutLogOutput
```

如果仍有少量 redirector 留在根目录，先查是否还有引用：

```powershell
rg -a -l "ObjectRedirector|/Game/Assets/Characters/StormHeroes/Ragnaros/Ragnaros_MAT_|/Game/Assets/Characters/StormHeroes/Ragnaros/T_Ragnaros_|/Game/Assets/Characters/StormHeroes/Ragnaros/SK_Ragnaros_Anim" `
  'D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\StormHeroes\Ragnaros' `
  -g '*.uasset'
```

然后显式传入 redirector 包和引用它的包再次修复。例如本次 Ragnaros 的材质 redirector 需要先保存 `SK_Ragnaros`，再删除两个材质 redirector：

```powershell
& 'D:\GameEngine\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\GameDev\Unreal_Projects\GAS\GAS.uproject' `
  -run=ResavePackages `
  -Package=/Game/Assets/Characters/StormHeroes/Ragnaros/SK_Ragnaros `
  -Package=/Game/Assets/Characters/StormHeroes/Ragnaros/Ragnaros_MAT_Body_Rebuilt `
  -Package=/Game/Assets/Characters/StormHeroes/Ragnaros/Ragnaros_MAT_FireSkirt_Rebuilt `
  -FixupRedirectors `
  -SearchAllAssets `
  -AutoCheckOutPackages=false `
  -IgnoreChangelist `
  -NoSourceControl `
  -unattended `
  -nop4 `
  -nosplash `
  -NoSound `
  -stdout `
  -FullStdOutLogOutput
```

最终目录检查：

```powershell
Get-ChildItem 'D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\StormHeroes\Ragnaros' -Recurse -Filter *.uasset |
  Group-Object DirectoryName
```

Ragnaros 最终应为：

```text
Root        3
Animations 53
Materials  2
Textures   6
```

## 11. UE 导入警告处理

本次 Ragnaros 导入日志有以下警告：

```text
LogFbx: Warning: Not valid bind pose for Pose (Ragnaros_FireSkirt_MeshData)
FBXImport: Warning: no smoothing group information was found in this FBX scene
```

处理原则：

- `no smoothing group`：如果法线显示正常，可以接受；后续需要更严格时在 Blender 导出前补平滑组/自定义法线。
- `Not valid bind pose`：不能简单忽略，要打开 SkeletalMesh 和动画抽查。Ragnaros 当前模型、武器、骨骼和动画已基本正确，因此该警告目前作为可接受警告记录。
- 如果新英雄出现 bind pose 警告并伴随 mesh 扭曲、武器错位、动画错位，应回到 Blender 检查绑定姿势、Armature modifier、mesh transform 和动画烘焙流程。

## 12. 验收标准

Blender 验收：

- 模型没有错位。
- 武器跟随手部动作。
- 动画不扭曲。
- 每个动作都是独立 Action。
- 切换 Action 时能看到不同动作。
- 最终场景没有动画源骨架。
- 没有残留临时约束。
- mesh 只绑定最终骨架。
- 材质节点可读，贴图路径有效。

UE 验收：

- `SK_<Hero>` 可以打开。
- Skeleton 是新建且来自修复后的 FBX。
- PhysicsAsset 自动生成。
- AnimSequence 数量和 Blender 中最终 Action 数量一致。
- 打开不同 AnimSequence 时动作不同。
- 武器、手臂、身体不扭曲。
- 材质引用贴图正确。
- Emissive/透明/双面等特殊材质在 UE 中复查。
- 导入日志无致命 FBX 解析失败。

Ragnaros 当前验收结果：

```text
SkeletalMesh: 1
Skeleton: 1
PhysicsAsset: 1
AnimSequence: 53
Material: 2
Texture: 6
```

## 13. 后续批量导入建议

当前 all-in-one FBX 方案适合先把角色完整导入 UE，优点是简单、可验证、一次导入模型和全部动画。

缺点：

- 动画资源名较长。
- UE 自动材质质量一般。
- 不利于后续单个动画重导入。
- 不利于跨英雄批处理和统一命名。

推荐后续为 StormHeroes 单独实现非 Python 批量导入器：

```text
Plugins/StormHeroesAssetImporter
```

建议功能：

- 读取每个英雄的 manifest。
- 导入 SkeletalMesh。
- 创建或复用本英雄 Skeleton。
- 导入每个独立动画 FBX。
- 自动命名 AnimSequence 为 `AS_<Hero>_<Action>`。
- 导入 Texture 并设置 sRGB / Compression / NormalMap。
- 创建标准 UE Material / Material Instance。
- 为 Fire / Emissive / Alpha 材质设置统一模板。
- 输出导入报告。

生产级推荐导出结构：

```text
processed_ue5\<Hero>
  Mesh
    SK_<Hero>.fbx
  Animations
    AS_<Hero>_Stand.fbx
    AS_<Hero>_Walk.fbx
    AS_<Hero>_Attack.fbx
  Textures
    T_<Hero>_*.png
  manifest.json
```

这样 UE 中资源命名更干净，也更适合批量管理。

## 14. 新英雄导入检查清单

导入 Blender 前：

- 找到模型 FBX。
- 找到动画 FBX。
- 找到贴图目录。
- 判断是否有多个皮肤/多个模型 FBX。
- 记录中文名和英文名。

Blender 中：

- 导入模型 FBX。
- 导入动画 FBX。
- 检查模型骨架骨骼数。
- 检查动画骨架骨骼数。
- 检查 mesh vertex groups 是否被模型骨架覆盖。
- 不直接把 mesh 绑定到动画骨架。
- 不直接复制 F-Curve。
- 使用世界空间约束 + visual bake。
- 过滤非 pose Action。
- 每个动作生成独立 Action。
- 删除源动画骨架和临时约束。
- 重建材质。
- 保存 `<Hero>_Blender.blend`。

导出 FBX：

- 禁止 leaf bones。
- 导出 Armature + Mesh。
- 导出全部 Actions。
- 不使用 NLA 长条。
- 贴图复制到中间目录。
- 反向导入 FBX 做 sanity check。

导入 UE：

- 目标路径 `/Game/Assets/Characters/StormHeroes/<Hero>`。
- 使用新 Skeleton。
- 导入 SkeletalMesh、PhysicsAsset、Materials、Textures、Animations。
- 立即运行 `StormHeroesOrganize`，把 Animations、Materials、Textures 分到英文子目录。
- 运行 `ResavePackages -FixupRedirectors`，确保根目录没有 redirector 残留。
- 检查 AnimSequence 数量。
- 打开多个动作预览。
- 检查武器是否在手上。
- 检查材质贴图是否引用正确。
- 对特殊材质进行 UE 端复查。

## 15. Ragnaros 本次工作记录

已完成：

1. 从 HotS Unity 资源中定位 Ragnaros 模型、动画、贴图。
2. 在 Blender 中确认模型骨架和动画骨架不是同一套可直接替换的骨架。
3. 确认 `Ragnaros.fbx` 中的 193 骨骨架才是正确蒙皮骨架。
4. 确认 `Ragnaros-Animation.fbx` 中的 95 骨骨架只是动画源骨架。
5. 排除了直接换骨架、直接复制 F-Curve、误选 Action 预览对象等错误路径。
6. 通过源骨架到目标骨架的世界空间约束和 visual bake，把 53 个动作烘焙到最终模型骨架。
7. 清理 Blender 场景，只保留最终骨架、模型和独立 Actions。
8. 重建 Body 与 FireSkirt 材质。
9. 导出 UE 用 all-in-one FBX：`SK_Ragnaros.fbx`。
10. 使用 UE `ImportAssets` commandlet 导入到 `/Game/Assets/Characters/StormHeroes/Ragnaros`。
11. 在 UE 内容目录中生成模型、骨架、物理资产、53 个动画、2 个材质和 6 张贴图。
12. 将源侧中间目录移动并压平到 `Role-V1\Ragnaros\processed_ue5`。
13. 新增 `StormHeroesAssetImporter` 插件和 `StormHeroesOrganize` commandlet。
14. 用 UE 资产系统将 Ragnaros UE 目录整理为根目录 + `Animations`、`Materials`、`Textures`。
15. 关闭 UE 中打开的 Ragnaros 资产，避免 `.uasset` 文件锁。
16. 通过 `ResavePackages -FixupRedirectors` 删除移动后留下的 redirector。
17. 最终确认根目录只剩 `SK_Ragnaros`、`SK_Ragnaros_Skeleton`、`SK_Ragnaros_PhysicsAsset`。

当前可继续改进：

- 将 all-in-one FBX 流程升级为“模型 + 单动画 FBX”流程，优化 UE 动画命名。
- 编写 StormHeroes 专用 C++ 导入器，统一材质、贴图设置和动画命名。
- 为 FireSkirt 创建更准确的 UE 火焰材质模板。
- 为角色创建标准预览关卡或测试蓝图，快速检查全部动画。

## 16. 2026-07-06 Batch Import Record

This section records the reusable batch workflow used after the Ragnaros validation pass. It is written as an additional record because the original document already contains mixed historical encoding in some terminals; do not rewrite the earlier sections only to fix display output.

### 16.1 Batch Source Scripts

Batch workspace:
```text
C:\Users\ZYZ\Documents\Codex\2026-07-06\new-chat\work\stormheroes_batch
```

Important files:
```text
stormheroes_variants.json
stormheroes_blender_prepare.py
stormheroes_ue_import.py
stormheroes_ue_import_all.py
blender_export_summary.json
blender_export_sgthammer_fixed_summary.json
blender_export_chogall_fixed_summary.json
ue_import_all_summary.json
ue_asset_counts.json
```

The Blender batch script performs the following steps:
- Reads the model FBX and animation FBX pair from `stormheroes_variants.json`.
- Creates `processed_ue5` under the hero source directory, not under the HotS project root.
- Uses `processed_ue5\<VariantName>` only when multiple independent model variants share one source directory, such as Diablo, Jaina, Kerrigan, Sgthammer, and Thrall.
- Imports the model FBX and keeps the model FBX armature as the final skinned armature.
- Imports the animation FBX only as an animation source armature.
- Transfers animation by `Copy Transforms` constraints plus visual baking to the model armature.
- Keeps each source animation as an independent Blender Action; it does not merge actions into one long NLA strip.
- Rebuilds Blender materials and copies Diff / Norm / Spec / Emis textures into the hero `processed_ue5\Textures` directory.
- Exports an all-in-one FBX named `SK_<Hero>.fbx` with mesh, skeleton, materials, textures, and all independent actions.
- Writes `stormheroes_<hero>_export_manifest.json` with source paths, processed paths, bone counts, mesh count, material count, and action count.

The UE batch script performs the following steps:
- Imports each `SK_<Hero>.fbx` with `ImportAssets` commandlet into `/Game/Assets/Characters/StormHeroes/<Hero>`.
- Immediately runs `StormHeroesOrganize` for the hero path.
- Runs `ResavePackages -FixupRedirectors -SearchAllAssets` after organization.
- Verifies that the hero root directory contains only `SK_<Hero>`, `SK_<Hero>_Skeleton`, and `SK_<Hero>_PhysicsAsset`.
- Verifies that `Animations`, `Materials`, and `Textures` subdirectories exist and contain the imported asset types.
- Verifies that Blender manifest `action_count` equals the number of UE animation assets.
- Scans for `ObjectRedirector` and expects no results.

### 16.2 ImportAssets Exit Code Note

`ImportAssets` returned exit code `1` for every batch hero, while the actual assets were generated successfully. This matches the earlier Ragnaros behavior.

The non-fatal log line is usually:
```text
LogAutomatedImport: Error: Invalid Destination Path (): FilenameToLongPackageName failed to convert ''
```

Do not judge success only by the `ImportAssets` exit code. The required checks are:
- `.uasset` files exist in the target hero directory.
- `StormHeroesOrganize` returns `0`.
- `ResavePackages -FixupRedirectors` returns `0`.
- Root, `Animations`, `Materials`, and `Textures` counts match expectations.
- Blender Action count matches UE AnimSequence count.

### 16.3 New Pitfall: Blender `.001` Action Names

Chogall initially exported 83 Blender Actions but UE generated only 82 AnimSequences. The cause was not failed baking. The cause was naming collision after UE normalized Blender-style suffixes such as `.001`.

Correct handling:
- Convert Blender action suffixes like `.001` to `_001` before FBX export.
- Maintain a per-hero `used_action_names` set while baking actions.
- If a cleaned action name already exists, append a stable suffix such as `_001`, `_002`.
- Always compare Blender manifest `action_count` with UE `Animations` directory asset count after import.

Chogall was re-exported and re-imported after this fix. Final result: Blender Actions `83`, UE AnimSequences `83`.

### 16.4 New Pitfall: Multi-FBX Animation Pairing

Do not pair model FBX and animation FBX only by filename intuition. Multi-model directories must be checked by model armature bone count, animation armature bone count, and common bone names.

Sgthammer example:
```text
Sgthammer01 + Sgthammer-Animation.fbx      common bones = 13   wrong
Sgthammer02 + Sgthammer-Animation.fbx      common bones = 13   wrong
Sgthammer03 + Sgthammer-Animation.fbx      common bones = 21   wrong
Sgthammer01 + Sgthammer_v07-Animation.fbx  common bones = 135  correct
Sgthammer02 + Sgthammer_v07-Animation.fbx  common bones = 132  correct
Sgthammer03 + Sgthammer_v07-Animation.fbx  common bones = 147  correct
```

Rule:
- If common bone count is very low, stop and find the correct animation FBX.
- This is especially important for vehicles, mechs, multi-character rigs, and hero directories with several skins or versions.

### 16.5 Batch Import Result

All assets were imported under:
```text
D:\GameDev\Unreal_Projects\GAS\Content\Assets\Characters\StormHeroes
```

New UE hero folders generated by this batch:
```text
Artanis
Chogall
Diablo
DiabloPrime
Garrosh
GarroshHighWarlord
Genji
Hanzo
Jaina01
Jaina02
Kaelthas
Kerrigan
Kerrigan01
Kerrigan02
Kerrigan03
Arthas
Medivh
Raynor
Samuro
Sgthammer
SgthammerMaster
SgthammerWarWorldNeon
Sylvanas
Tassadar
Thrall
ThrallHellhammer
ThrallUltimate
Tychus
Tracer
Malthael
Imperius
Orphea
```

Final asset counts:
```text
Hero                  Root  Animations  Materials  Textures
Artanis               3     58          3          12
Chogall               3     83          6          24
Diablo                3     52          2          8
DiabloPrime           3     54          3          12
Garrosh               3     54          1          4
GarroshHighWarlord    3     54          2          6
Genji                 3     80          3          12
Hanzo                 3     63          4          16
Jaina01               3     52          5          16
Jaina02               3     20          4          8
Kaelthas              3     52          5          20
Kerrigan              3     67          2          8
Kerrigan01            3     62          4          8
Kerrigan02            3     62          2          8
Kerrigan03            3     62          2          8
Arthas                3     52          6          24
Medivh                3     30          4          16
Raynor                3     44          4          16
Samuro                3     55          5          19
Sgthammer             3     59          3          10
SgthammerMaster       3     59          5          18
SgthammerWarWorldNeon 3     59          4          9
Sylvanas              3     57          4          16
Tassadar              3     57          4          16
Thrall                3     51          2          8
ThrallHellhammer      3     51          3          12
ThrallUltimate        3     51          5          20
Tychus                3     58          2          8
Tracer                3     74          3          12
Malthael              3     60          4          11
Imperius              3     45          4          16
Orphea                3     55          2          8
```

Final validation:
- 32 new hero folders exist.
- Every hero root contains exactly 3 core assets.
- Every hero has `Animations`, `Materials`, and `Textures` subdirectories.
- Blender Action count matches UE AnimSequence count for all 32 heroes.
- `rg -a -l "ObjectRedirector" <StormHeroesRoot> -g "*.uasset"` returned no results.
- Tychus appeared twice in the requested list and was imported once.