# UE5.8 资产结构化语义导出插件功能需求文档

文档版本：v1.1  
编写日期：2026-07-29  
目标引擎：Unreal Engine 5.8  
建议插件代号：UAsset Semantic Exporter，简称 USEM  
文档目的：定义一个 UE5.8 编辑器插件，用于把 `.uasset` / `.umap` 资产导出为稳定、可 diff、可索引、可离线给 AI 分析的结构化语义文件，并与 C++ 源码、配置、依赖关系联动。

v1.1 修订说明：澄清“离线分析”的含义。本文所说离线分析不是指 AI 在未启动 UE 的情况下直接读取原始 `.uasset` / `.umap` 二进制文件，而是指插件已经提前生成并同步维护结构化语义文件之后，AI 可以在不启动 Unreal Editor、不中转 UE5.8 官方 MCP 的情况下，结合这些语义文件、C++ 源码、配置文件和依赖索引进行准确联合分析。

---

## 1. 核心结论

### 1.1 需求是否可以完全实现

按本文澄清后的离线分析定义，本需求可以完整实现。在“用户拥有完整 UE 工程、对应 UE5.8 引擎、项目 C++ 源码、未 Cook 的编辑器资产，并且允许插件在编辑器或命令行 Editor 进程内加载项目完成导出”的前提下，本需求可以实现为一个可靠的资产语义旁路系统。

本文中的“离线分析”专指：

> **导出阶段**：插件在 Unreal Editor 或 `UnrealEditor-Cmd.exe` 中读取 `.uasset` / `.umap`，生成并同步维护结构化语义文件。  
> **分析阶段**：AI 不需要启动 Unreal Editor，也不需要连接 UE5.8 官方 MCP，只读取已经生成的 `.uesem/` 语义文件、C++ 源码索引、配置文件和依赖图进行联合分析。

在这个定义下，需求边界如下：

1. **可以实现**：在 Unreal Editor 内读取 `.uasset` / `.umap`，通过 UE 反射、Asset Registry、Blueprint/Graph API、Exporter、Python/Commandlet 或 C++ Editor Module 导出结构化语义文件。
2. **可以实现**：资产在编辑器内创建、保存、重命名、删除时，同步生成、更新、移动或删除对应语义文件。
3. **可以实现**：将 Blueprint、Map、DataAsset、DataTable、Behavior Tree、UMG、Anim Blueprint 等资产语义与 C++ 反射符号索引关联，让 AI 离线分析功能逻辑。
4. **可以实现**：AI 在不启动 UE 编辑器的情况下，读取已经导出的语义文件、C++ 源码索引、配置文件和依赖图进行分析。
5. **不作为本需求目标**：AI 在完全不启动 UE、不加载项目模块、不依赖 UE 序列化/反射的情况下，直接从原始 `.uasset` / `.umap` 二进制文件还原所有逻辑。该方向属于低层解析或反解析工具范畴，不是本插件的主路径，也不作为验收标准。
6. **不作为 P0 目标**：从 Cooked/打包后的资产中恢复编辑器图结构。Cooked 内容可能剥离编辑器专用数据，且部分资产只能只读或无法用标准资产编辑器打开，可作为后续研究项但不影响本需求成立。

因此，本插件的产品定位是：

> **在 UE 编辑器或 Editor Commandlet 可运行时，提前导出并持续维护资产语义旁路文件；AI 分析阶段只依赖这些语义文件和源码索引，从而实现离线分析。**

---

## 2. 背景与问题

UE 项目逻辑通常由多层共同组成：

1. C++ 源码：`UCLASS`、`UFUNCTION`、`UPROPERTY`、组件、Subsystem、Gameplay Ability、网络复制、服务端权限、性能敏感逻辑。
2. Blueprint 资产：事件图、函数图、宏、组件树、变量默认值、Timeline、Dispatcher、Interface、父类与覆盖关系。
3. Map/Level 资产：Actor 实例、Transform、组件实例属性、Level Streaming、World Partition、外部 Actor 包、关卡脚本。
4. 数据资产：DataAsset、PrimaryDataAsset、DataTable、CurveTable、Gameplay Tags、配置表。
5. UI/动画/AI 资产：UMG Widget Blueprint、Anim Blueprint、State Machine、Behavior Tree、Blackboard、Control Rig、Niagara、Material Graph、PCG 等。
6. 配置文件：`DefaultEngine.ini`、`DefaultGame.ini`、`DefaultInput.ini`、GameplayTags 配置、Collision 配置、AssetManager 配置。

C++ 文件可以直接被 AI 阅读，但 `.uasset` / `.umap` 是二进制资产，AI 无法直接准确理解其逻辑。因此需要一个稳定的中间层，把编辑器资产转换为结构化、可读、可验证的语义数据。

---

## 3. 现有能力与市场调研

### 3.1 官方能力

| 能力 | 已实现内容 | 对本插件的意义 | 限制 |
|---|---|---|---|
| UE5.8 Unreal MCP | 官方实验性 MCP，可让 Codex、Claude Code、Cursor 等客户端连接正在运行的 Unreal Editor，并调用 Toolset 操作编辑器、Slate、材质、Actor、自动化测试等。 | 可作为本插件的在线查询入口或辅助控制层。 | 必须启动编辑器并连接本地端口；Experimental；默认能力不等于完整资产语义导出系统；离线分析仍需旁路文件。 |
| Asset Registry | 可快速获得资产路径、类、标签、依赖、引用等信息，不必加载所有资产。 | 用于生成资产索引、依赖图、变更检测、批量导出队列。 | 只提供注册表级元数据，不能完整表达 Blueprint 图、Map 实例属性等深层逻辑。 |
| Editor Python / Commandlet | 可在 Editor 或 `UnrealEditor-Cmd.exe -run=pythonscript` 中批量处理资产。 | 用于无界面批量导出、CI 校验、修复缺失语义文件。 | 导出阶段仍要加载 UE 编辑器运行时和项目模块；其目标是生成可离线分析的语义文件，而不是让 AI 直接分析原始二进制资产。 |
| UExporter / Text Export / Copy Nodes | 部分资产、对象和 Blueprint 节点可导出为文本或剪贴板文本。 | 可作为局部导出和调试辅助。 | 格式不是完整、稳定、面向 AI 的项目级语义协议。 |
| Data Validation | 可在编辑器和 CI 中做资产校验。 | 可扩展为“语义文件是否过期/缺失/不可解析”的校验器。 | 不负责导出语义，只负责校验流程。 |
| Source Control 支持 | UE 官方文档明确 `.uasset` / `.umap` 属于二进制资产，通常需要锁定工作流。 | 说明旁路文本文件对 diff、review、AI 分析有实际价值。 | 旁路文件必须与二进制资产保持同步，否则会造成误导。 |

### 3.2 第三方和社区工具

| 工具/项目 | 已实现内容 | 可借鉴点 | 与本需求差距 |
|---|---|---|---|
| BP2AI | 面向 AI 的 Blueprint 到文本/Markdown 导出插件，覆盖 Blueprint、Widget、Anim/Pose、DataAsset/DataTable 等，并提供批量/命令行导出能力。 | 证明“Blueprint 语义导出给 AI”需求已经成立；Markdown 摘要体验值得借鉴。 | 不一定覆盖完整 `.umap`、C++ 符号索引、自动同步、删除/重命名联动、项目级离线包。 |
| BlueprintUE | 通过粘贴 Blueprint 节点剪贴板文本来分享和查看节点图。 | 适合小片段节点交流，说明 Blueprint 复制文本可以被解析。 | 不是项目级导出；依赖人工复制；不处理资产默认值、依赖图、Map、C++ 关联。 |
| Blueprint Exporter / Blueprint to Text 类插件 | 通常支持把 Blueprint 节点、Pins、变量、函数导出为文本、JSON 或 Markdown。 | 可借鉴 JSON/Markdown 双格式、节点连接表达方式。 | 覆盖范围和维护状态不一，通常不是完整资产生命周期同步系统。 |
| BlueprintSerializer | 社区项目尝试把 Blueprint 序列化为结构化 JSON，包含节点、Pin、变量、函数、组件等。 | 证明 Blueprint 图可以被结构化表达，适合借鉴 schema。 | 需要适配 UE5.8；可能不是完整产品级插件；Map/C++ 联动仍需扩展。 |
| UE5 Blueprint Dumper | Python 脚本类方案，把 Blueprint 信息 dump 到 JSON。 | 适合快速原型和调研字段。 | 通常版本覆盖有限，工程化、同步、索引、差量导出不足。 |
| UAssetAPI / UAssetGUI | 低层 `.uasset` 解析、查看、JSON 导入导出、Kismet bytecode 等能力。 | 可作为低层格式研究、诊断或特殊资产恢复参考。 | 不应作为本插件主路径；本需求的离线分析依赖已导出的语义文件，不依赖直接读取原始 `.uasset` 二进制。 |
| CUE4Parse / FModel / UModel 生态 | 偏向读取/浏览 Cooked 包和资源。 | 可用于理解资产包结构、非源码工程研究。 | 面向打包游戏资源，不适合做源工程编辑器语义导出主链路；合规边界也更敏感。 |

结论：市场上已经有“Blueprint 导出给 AI”“Blueprint 节点文本化”“低层 uasset 解析”“UE MCP 在线操作编辑器”等能力，但缺少一个完整产品：

1. 同时覆盖 `.uasset` / `.umap`。
2. 自动跟随编辑器资产生命周期。
3. 生成稳定可 diff 的结构化语义文件。
4. 与 C++ 反射符号、配置、依赖图联动。
5. 支持 AI 离线分析包。
6. 提供 schema 版本、校验、CI 和源码管理集成。

---

## 4. 产品目标

### 4.1 总目标

开发一个 UE5.8 Editor 插件，在资产创建、保存、修改、重命名、删除时自动维护对应的结构化语义旁路文件，并提供手动导出、批量导出、命令行导出、C++ 关联索引和离线 AI 分析包。

### 4.2 目标用户

1. UE C++ 工程师：希望 AI 分析 C++ 与 Blueprint 的完整调用链。
2. Blueprint 开发者：希望把节点逻辑导出给 AI 做审查、解释、迁移或优化建议。
3. 技术美术/关卡设计师：希望离线审查材质、关卡、UMG、动画蓝图、行为树等资产语义。
4. 技术负责人：希望在 Code Review/CI 中看到资产变更的文本 diff。
5. AI 工具开发者：希望构建 UE 项目离线知识库或 RAG 索引。

### 4.3 非目标

1. 不做任意二进制 `.uasset` 的完整反编译器。
2. 不以 Cooked 游戏资源还原为主要目标。
3. 不直接修改 `.uasset` 二进制内容。
4. 不替代 UE5.8 官方 MCP，而是与其互补。
5. 不承诺所有第三方插件资产在无适配器情况下都能完整语义化。
6. 不要求 AI 在离线阶段直接读取原始 `.uasset` / `.umap` 二进制文件。
7. 不把导出的语义文件用于还原、盗用、分发商业资产。

---

## 5. 需求范围与优先级

### 5.1 P0 必须实现

1. 手动导出选中 `.uasset` / `.umap` 为结构化语义文件。
2. 自动在资产创建和保存后生成/更新语义文件。
3. 自动在资产删除和重命名后删除/移动语义文件。
4. 生成项目级资产索引和依赖索引。
5. 支持 Blueprint、Map、DataAsset、DataTable、Widget Blueprint 的基础语义导出。
6. 支持 C++ 反射符号索引，与 Blueprint 调用节点关联。
7. 支持离线 AI 分析包导出。
8. 语义文件采用确定性排序，便于 Git diff。
9. 提供过期检测、缺失检测和错误报告。

### 5.2 P1 应当实现

1. Anim Blueprint、Behavior Tree、Blackboard、Material、Niagara、PCG 的专用语义导出器。
2. 增量导出队列、批处理、去抖动和性能预算。
3. Editor Commandlet / CI 模式。
4. 与 UE5.8 官方 MCP 集成，允许 MCP 查询已有语义文件或触发导出。
5. Graphviz/Mermaid 调用图输出。
6. 支持按资产、模块、文件夹、Primary Asset Type 创建 AI Bundle。
7. 支持资产变更摘要和语义 diff。

### 5.3 P2 可选增强

1. 内置 AI Prompt 模板和上下文裁剪策略。
2. SQLite/Parquet 本地索引用于大型项目快速检索。
3. VS Code/Cursor/Codex 侧插件或 MCP Server。
4. 多语言摘要。
5. 自定义资产类型 SDK，供项目组为自研插件资产编写导出器。
6. Blueprint 到 C++ 迁移辅助报告。
7. 与 Perforce/Git LFS/Plastic SCM 的提交前检查集成。

---

## 6. 总体架构

### 6.1 推荐架构

```text
UE Editor / UnrealEditor-Cmd
  |
  |-- USEM Editor Module
  |     |-- Asset Lifecycle Listener
  |     |-- Manual Export UI
  |     |-- Export Queue / Debounce
  |     |-- Exporter Registry
  |     |-- Built-in Asset Exporters
  |     |-- C++ Symbol Indexer
  |     |-- Validation & Staleness Checker
  |     |-- MCP Toolset Adapter
  |
  |-- Project Assets
  |     |-- Content/**/*.uasset
  |     |-- Content/**/*.umap
  |     |-- Source/**/*.h / *.cpp
  |     |-- Config/**/*.ini
  |
  v
ProjectRoot/.uesem/
  |-- schema/
  |-- content/
  |-- maps/
  |-- cpp/
  |-- config/
  |-- graph/
  |-- bundles/
  |-- logs/
```

### 6.2 插件模块划分

| 模块 | 类型 | 职责 |
|---|---|---|
| `USEMCore` | Runtime 或 Developer | 语义数据结构、schema 版本、路径映射、hash、通用序列化工具。 |
| `USEMEditor` | Editor | 菜单、Content Browser 操作、生命周期监听、导出队列、保存/删除/重命名联动。 |
| `USEMExporters` | Editor | Blueprint、Map、DataAsset、DataTable、Widget 等导出器。 |
| `USEMCppIndex` | Editor/Developer | C++ 反射符号、源码路径、UFUNCTION/UPROPERTY、BlueprintCallable 关联索引。 |
| `USEMCommandlets` | Editor | 批量导出、校验、清理、生成离线包。 |
| `USEMMCP` | Editor，可选 | 暴露 MCP Toolset：查询语义、触发导出、获取依赖上下文。 |
| `USEMDeveloperSettings` | Editor | 项目设置、忽略规则、输出目录、自动同步策略、隐私过滤。 |

---

## 7. 语义语言与文件格式

### 7.1 推荐语义语言

建议定义插件自己的领域语义模型：**USEM，Unreal Semantic Export Model**。

USEM 不是自然语言，而是由 JSON Schema 约束的结构化语义协议。字段名使用英文，便于跨语言工具和 AI 稳定解析；字段值保留 UE 原始路径、类名、函数名、Pin 名、资产名、配置名。

### 7.2 推荐保存格式

| 文件 | 格式 | 用途 |
|---|---|---|
| `*.uesem.json` | UTF-8 JSON | 单资产主语义文件，机器读取、AI 分析、diff、校验。 |
| `*.uesem.md` | UTF-8 Markdown | 单资产人类可读摘要，方便快速审查和直接粘给 AI。 |
| `.uesem-index.json` | UTF-8 JSON | 项目级资产索引、导出状态、hash、schema 版本。 |
| `.uesem-deps.json` | UTF-8 JSON | 项目级依赖图、反向引用、软硬引用。 |
| `cpp-symbol-index.json` | UTF-8 JSON | C++ 反射类、函数、属性、源码路径、Blueprint 暴露元数据。 |
| `*.uesem.graph.json` | UTF-8 JSON | 可视化图数据，表达 Blueprint 节点图、调用图、依赖图。 |
| `*.uesem.mmd` | Mermaid | 可选，给人看调用链或资产依赖。 |
| `*.uesem.bundle.zip` | ZIP | 离线 AI 分析包，包含所选资产语义、相关 C++、配置、索引和引用说明。 |
| `events.ndjson` | NDJSON | 可选，资产变更事件日志，适合增量同步和审计。 |

不建议把 Markdown 作为唯一格式，因为 Markdown 难以做严格校验、稳定 diff 和程序化索引。不建议直接使用 YAML 作为主格式，因为 UE 路径、Pin 默认值、多行文本、嵌套数组和大项目批量处理更适合 JSON/JSON Schema。

### 7.3 输出目录策略

默认输出到项目根目录：

```text
<ProjectRoot>/.uesem/
  schema/usem.asset.schema.json
  index/.uesem-index.json
  index/.uesem-deps.json
  content/Game/Blueprints/BP_Door.uesem.json
  content/Game/Blueprints/BP_Door.uesem.md
  maps/Game/Maps/L_Main.uesem.json
  cpp/cpp-symbol-index.json
  bundles/BP_Door_context.uesem.bundle.zip
```

不建议默认把语义文件直接放到 `Content/` 目录旁边，原因：

1. 避免污染 Content Browser 和打包规则。
2. 避免误触发额外资产导入流程。
3. 方便把 `.uesem/` 单独纳入或排除版本控制。
4. 方便 CI、AI 工具和外部索引器读取。

可提供可选模式：

```text
Content/Blueprints/BP_Door.uasset
Content/Blueprints/BP_Door.uasset.uesem.json
```

该模式只建议小项目、强 diff 需求或团队明确接受 Content 旁路文件时使用。

### 7.4 单资产 JSON 顶层结构

```json
{
  "schema": "com.example.usem.asset",
  "schemaVersion": "1.0.0",
  "engine": {
    "version": "5.8.0",
    "compatibleVersions": ["5.8"]
  },
  "project": {
    "name": "MyGame",
    "uproject": "MyGame.uproject"
  },
  "asset": {
    "packageName": "/Game/Blueprints/BP_Door",
    "objectPath": "/Game/Blueprints/BP_Door.BP_Door",
    "assetClass": "Blueprint",
    "nativeClass": "/Script/Engine.Blueprint",
    "sourceFile": "Content/Blueprints/BP_Door.uasset",
    "semanticFile": ".uesem/content/Game/Blueprints/BP_Door.uesem.json",
    "packageGuid": "optional-guid",
    "sourceHash": "sha256:...",
    "exportedAtUtc": "2026-07-29T00:00:00Z"
  },
  "dependencies": {
    "hard": [],
    "soft": [],
    "management": [],
    "searchableNames": [],
    "referencers": []
  },
  "semantics": {},
  "cppLinks": [],
  "diagnostics": []
}
```

### 7.5 Blueprint 语义结构

Blueprint 导出至少需要包含：

1. 资产基本信息：包路径、类、父类、接口、GeneratedClass、SkeletonClass。
2. 组件树：组件名称、类、Attach 关系、默认属性、可编辑属性。
3. 变量：名称、类型、默认值、分类、可见性、Replication、ExposeOnSpawn、SaveGame、InstanceEditable、Tooltip。
4. 函数：函数图、参数、返回值、纯函数/非纯函数、权限/网络、Override 关系。
5. 事件图：事件节点、调用节点、分支、循环、Timeline、Delay、Latent 节点、Delegate 绑定。
6. 宏和折叠图：展开引用关系、输入输出 Pin、调用处。
7. 节点：GUID、节点类、标题、注释、坐标可选、Pins、默认值、连接。
8. Pin：名称、方向、类型、默认值、LinkedTo、是否隐藏、是否高级、容器类型。
9. 调用目标：`UK2Node_CallFunction` 对应的 `UFunction`、所属类、模块、源码路径。
10. 成员访问：变量 Get/Set 对应的属性、所属类、是否本地变量。
11. 资源引用：组件、材质、音效、DataAsset、WidgetClass、AnimClass 等。
12. 编译状态：最后编译是否成功、编译警告/错误摘要。

示例：

```json
{
  "semantics": {
    "kind": "Blueprint",
    "parentClass": "/Script/MyGame.DoorBase",
    "interfaces": ["/Script/MyGame.Interactable"],
    "components": [
      {
        "name": "DoorMesh",
        "class": "/Script/Engine.StaticMeshComponent",
        "attachParent": "DefaultSceneRoot",
        "properties": {
          "StaticMesh": "/Game/Meshes/SM_Door.SM_Door"
        }
      }
    ],
    "variables": [
      {
        "name": "bIsLocked",
        "type": "bool",
        "defaultValue": true,
        "category": "Door",
        "flags": ["InstanceEditable", "BlueprintReadWrite"]
      }
    ],
    "graphs": [
      {
        "name": "EventGraph",
        "type": "Ubergraph",
        "nodes": [
          {
            "id": "node-guid",
            "class": "K2Node_Event",
            "title": "Event ActorBeginOverlap",
            "pins": [
              {
                "id": "pin-guid",
                "name": "Then",
                "direction": "output",
                "type": "exec",
                "links": ["other-pin-guid"]
              }
            ]
          }
        ],
        "links": [
          {
            "from": "node-guid:Then",
            "to": "branch-guid:Exec"
          }
        ]
      }
    ]
  }
}
```

### 7.6 Map / `.umap` 语义结构

Map 导出至少包含：

1. World/Level 名称、Persistent Level、Streaming Levels、World Partition 状态。
2. Actor 列表：Label、Name、Class、Path、Transform、Tags、Folder、Layer/DataLayer。
3. 组件树：组件类、Attach 层级、重要属性。
4. 实例属性覆盖：与类默认值不同的属性。
5. Actor 引用关系：硬引用、软引用、接口引用、Level Script 引用。
6. 关卡脚本 Blueprint：事件图和调用链。
7. World Partition 外部 Actor 包映射。
8. 灯光、NavMesh、Collision、Physics、Gameplay Volume 等影响运行逻辑的摘要。

示例：

```json
{
  "semantics": {
    "kind": "Map",
    "world": {
      "name": "L_Main",
      "worldPartition": true,
      "persistentLevel": "/Game/Maps/L_Main"
    },
    "actors": [
      {
        "label": "Door_A_01",
        "name": "BP_Door_C_12",
        "class": "/Game/Blueprints/BP_Door.BP_Door_C",
        "transform": {
          "location": [100.0, 250.0, 0.0],
          "rotation": [0.0, 90.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "propertyOverrides": {
          "bIsLocked": true,
          "RequiredKeyId": "Key_A"
        },
        "references": ["/Game/Data/DA_Key_A.DA_Key_A"]
      }
    ]
  }
}
```

### 7.7 C++ 符号索引结构

C++ 索引用于把资产语义里的函数、类、属性链接回源码。它不需要完整替代 clang AST，但至少要覆盖 Unreal 反射层。

```json
{
  "schema": "com.example.usem.cpp-symbol-index",
  "schemaVersion": "1.0.0",
  "modules": [
    {
      "name": "MyGame",
      "path": "Source/MyGame",
      "classes": [
        {
          "name": "ADoorBase",
          "unrealPath": "/Script/MyGame.DoorBase",
          "header": "Source/MyGame/DoorBase.h",
          "source": "Source/MyGame/DoorBase.cpp",
          "superClass": "/Script/Engine.Actor",
          "functions": [
            {
              "name": "OpenDoor",
              "signature": "void OpenDoor(AActor* InstigatorActor)",
              "flags": ["BlueprintCallable"],
              "metadata": {
                "Category": "Door"
              },
              "declarationLine": 42,
              "definitionLine": 128
            }
          ],
          "properties": [
            {
              "name": "bIsLocked",
              "type": "bool",
              "flags": ["EditAnywhere", "BlueprintReadWrite"],
              "declarationLine": 31
            }
          ]
        }
      ]
    }
  ]
}
```

索引来源建议分层：

1. P0：使用 UE 反射系统枚举 `UClass`、`UFunction`、`FProperty`，获取稳定的运行时反射信息。
2. P0：通过项目源码路径规则和简单扫描，把反射类映射到 `.h` / `.cpp` 文件。
3. P1：可选集成 clang tooling 或 `compile_commands.json`，获得更准确的定义位置、调用关系、非反射函数。
4. P2：生成 C++ 内部调用图，但要避免把范围扩大到完整静态分析器。

---

## 8. 功能需求

### 8.1 手动导出

#### 8.1.1 Content Browser 操作

用户在 Content Browser 中选中一个或多个资产，可执行：

```text
Asset Actions > Export AI Semantic
Asset Actions > Export AI Semantic with Dependencies
Asset Actions > Create AI Analysis Bundle
```

要求：

1. 支持 `.uasset`、`.umap`。
2. 支持文件夹右键批量导出。
3. 支持按资产类型筛选导出。
4. 支持仅导出索引、不加载深层资产。
5. 支持导出当前资产、硬依赖、软依赖、反向引用。
6. 导出结束后显示成功、跳过、失败数量。
7. 失败资产写入 `.uesem/logs/export-errors.jsonl`。

#### 8.1.2 菜单和工具栏

提供主菜单：

```text
Tools > AI Semantic Export
  - Export Selected Assets
  - Export All Dirty Assets
  - Export Project Index
  - Export C++ Symbol Index
  - Validate Semantic Files
  - Open USEM Settings
```

#### 8.1.3 命令行

提供 Commandlet：

```powershell
UnrealEditor-Cmd.exe "D:\Project\MyGame.uproject" `
  -run=USEMExport `
  -Mode=Changed `
  -Output=".uesem" `
  -IncludeCpp=true `
  -Unattended
```

支持参数：

| 参数 | 说明 |
|---|---|
| `-Mode=Selected|Changed|All|Folder|AssetList|Bundle|Validate|Clean` | 导出模式。 |
| `-Asset=/Game/Blueprints/BP_Door` | 单资产导出。 |
| `-Folder=/Game/Blueprints` | 文件夹导出。 |
| `-AssetList=path.txt` | 从列表读取资产路径。 |
| `-IncludeDeps=Hard|Soft|All|None` | 依赖范围。 |
| `-IncludeReferencers=true|false` | 是否包含反向引用。 |
| `-IncludeCpp=true|false` | 是否生成 C++ 索引。 |
| `-Bundle=true|false` | 是否创建离线分析包。 |
| `-FailOnStale=true|false` | CI 中发现过期文件是否失败。 |
| `-Pretty=true|false` | 是否格式化 JSON。 |

### 8.2 自动创建语义文件

当用户在编辑器内创建 `.uasset` 或 `.umap` 时，插件必须自动生成对应语义文件。

触发来源：

1. Asset Registry 的资产新增事件。
2. Object 保存事件。
3. Content Browser 创建流程。
4. Map 保存流程。
5. 外部 Actor 包保存流程。

要求：

1. 新资产第一次保存后生成语义文件。
2. 如果资产尚未保存到磁盘，只记录待导出状态，不生成最终文件。
3. 对 Blueprint，新建后如果尚未编译，也必须导出结构，同时在 `diagnostics` 中标记编译状态。
4. 对 Map，新建后保存时导出 Persistent Level 基础结构。
5. 输出文件路径必须由资产 package path 确定，不能依赖显示名称。

### 8.3 自动更新语义文件

当资产在编辑器内修改并保存后，插件必须更新语义文件。

要求：

1. 使用导出队列，避免每次属性变化都立即导出。
2. 以“保存成功后”为最终导出时机。
3. 对频繁保存的资产做 debounce。
4. 导出前计算源资产状态，导出后写入 `sourceHash`、`packageGuid`、`exportedAtUtc`。
5. 如果导出失败，保留旧语义文件，并写入失败报告，避免生成半截文件误导 AI。
6. 文件写入采用临时文件 + 原子替换策略。
7. JSON 字段排序、数组排序、浮点精度必须稳定。

### 8.4 删除同步

当资产在编辑器内删除后，插件必须处理对应语义文件。

推荐策略：

1. 默认删除对应 `.uesem.json` 和 `.uesem.md`。
2. 可选保留 tombstone：

```json
{
  "schema": "com.example.usem.tombstone",
  "deletedAsset": "/Game/Blueprints/BP_Door",
  "deletedAtUtc": "2026-07-29T00:00:00Z",
  "previousSemanticHash": "sha256:..."
}
```

3. 更新项目索引，删除资产项或标记为 deleted。
4. 更新依赖图，移除相关边。
5. 对源码管理集成，提示用户把语义文件删除一同提交。

### 8.5 重命名与移动同步

当资产重命名或移动后：

1. 移动语义文件到新路径。
2. 更新语义文件内部 `asset.packageName`、`asset.objectPath`、`asset.sourceFile`、`asset.semanticFile`。
3. 更新项目索引。
4. 更新依赖图。
5. 记录旧路径到 `previousPaths`。
6. 如果 UE 创建 Redirector，必须在语义文件中标记：

```json
{
  "redirector": {
    "from": "/Game/Old/BP_Door",
    "to": "/Game/New/BP_Door"
  }
}
```

### 8.6 项目级资产索引

必须生成：

```text
.uesem/index/.uesem-index.json
.uesem/index/.uesem-deps.json
```

索引字段：

1. 资产路径。
2. 资产类。
3. 原始文件路径。
4. 语义文件路径。
5. 源资产 hash。
6. 语义文件 hash。
7. 最后导出时间。
8. 导出状态：`ok`、`stale`、`missing`、`failed`、`unsupported`。
9. 依赖数量。
10. 反向引用数量。
11. 所属插件/模块。
12. Primary Asset Id。
13. 资产标签。

### 8.7 C++ 联动分析

插件必须支持把资产语义和 C++ 源码建立连接。

#### 8.7.1 Blueprint 到 C++ 链接

对于 Blueprint 节点：

1. `CallFunction` 节点链接到 `UFunction`。
2. 变量 Get/Set 节点链接到 `FProperty`。
3. Component 节点链接到组件类。
4. Event Override 链接到父类虚函数或 BlueprintNativeEvent/BlueprintImplementableEvent。
5. Interface Message 链接到接口函数。
6. Delegate Bind/Assign 链接到 Dispatcher 声明。
7. Gameplay Ability 节点链接到 Ability 类、AttributeSet、GameplayEffect、GameplayTag。

#### 8.7.2 C++ 到 Blueprint 反向链接

在 C++ 符号索引中记录：

1. 哪些 Blueprint 继承该 C++ 类。
2. 哪些 Blueprint 调用了该 `UFUNCTION`。
3. 哪些 Blueprint 设置或读取该 `UPROPERTY`。
4. 哪些 Map 中实例化了该类或其 Blueprint 子类。
5. 哪些 DataAsset 引用了该类。

#### 8.7.3 AI 分析上下文生成

对某个资产生成 AI Bundle 时，必须包含：

1. 当前资产语义文件。
2. 当前资产直接依赖的关键语义文件。
3. 当前资产反向引用摘要。
4. 相关 C++ `.h` / `.cpp` 文件或精简片段。
5. 相关 `.ini` 配置片段。
6. 调用图。
7. 依赖图。
8. `README.md`，说明包内容、导出时间、引擎版本、schema 版本。

### 8.8 离线分析

插件必须支持在 UE 编辑器关闭后，AI 读取 `.uesem/` 目录完成分析。

这里的离线分析必须按以下定义实现：

1. 插件已经在 UE Editor、Editor Commandlet、CI 或用户手动导出流程中生成过结构化语义文件。
2. AI 分析时不需要启动 Unreal Editor。
3. AI 分析时不需要连接 UE5.8 官方 MCP 端口。
4. AI 分析输入是 `.uesem/` 语义文件、C++ 源码/源码索引、配置文件、依赖索引和可选 AI Bundle。
5. AI 分析阶段不要求、也不依赖直接读取原始 `.uasset` / `.umap` 二进制文件。

要求：

1. 所有语义文件自包含关键上下文。
2. 项目索引能让 AI 从资产路径找到语义文件。
3. C++ 符号索引能让 AI 从 Blueprint 节点找到源码路径。
4. Bundle 能打包最小相关上下文，避免 AI 读取整个项目。
5. 每个语义文件包含 schema 版本和导出器版本。
6. 每个语义文件包含 stale 检测字段。
7. 提供只读 CLI 工具：

```powershell
uesem.exe query --asset /Game/Blueprints/BP_Door
uesem.exe bundle --asset /Game/Blueprints/BP_Door --include-deps hard --include-cpp
uesem.exe validate --project D:\Project\MyGame.uproject
```

### 8.9 UE5.8 官方 MCP 集成

本插件应提供可选 MCP Toolset，供已经连接 UE5.8 官方 MCP 的 AI 使用。

建议工具：

| MCP Tool | 功能 |
|---|---|
| `usem.export_asset` | 导出指定资产。 |
| `usem.export_selected` | 导出当前选中资产。 |
| `usem.query_asset_semantic` | 返回指定资产语义摘要。 |
| `usem.query_dependency_context` | 返回资产依赖上下文。 |
| `usem.create_ai_bundle` | 创建离线 AI 分析包。 |
| `usem.validate_semantics` | 检查缺失/过期/失败。 |
| `usem.find_cpp_links` | 查询资产关联的 C++ 类、函数、属性。 |

重要边界：

1. MCP 是在线编辑器桥接能力。
2. 离线分析依赖 `.uesem/` 文件，不依赖 MCP。
3. MCP 只用于触发导出、查询实时编辑器状态和辅助定位。
4. 对已经导出的语义文件，AI 应能在没有 MCP 和没有 UE 编辑器进程的情况下完成阅读、检索、调用链推理和功能逻辑分析。

### 8.10 资产类型导出器

每类资产应有专用导出器。通用反射导出器只能作为兜底。

| 资产类型 | P0/P1/P2 | 导出重点 |
|---|---|---|
| Blueprint | P0 | 类、组件、变量、图、节点、Pins、C++ 调用链接。 |
| Widget Blueprint | P0 | Widget Tree、绑定、动画、事件图、样式引用。 |
| Map / World | P0 | Actor、Transform、实例属性、Level Script、Streaming、World Partition。 |
| DataAsset / PrimaryDataAsset | P0 | 反射属性、资产引用、Primary Asset Id。 |
| DataTable / CurveTable | P0 | RowStruct、行数据、CSV/JSON 内容摘要。 |
| Anim Blueprint | P1 | AnimGraph、State Machine、Transition Rule、EventGraph、Linked Anim Layer。 |
| Behavior Tree | P1 | 节点树、Decorator、Service、Task、Blackboard Key。 |
| Blackboard | P1 | Key、类型、默认值、用途引用。 |
| Material / Material Instance | P1 | 表达式图、参数、Texture、Static Switch、Parent 链。 |
| Niagara System | P1 | Emitters、Modules、Parameters、Script Graph。 |
| PCG Graph | P1 | 节点图、输入输出、参数、资源引用。 |
| Control Rig | P2 | Rig Graph、Controls、Hierarchy、Constraints。 |
| Level Sequence | P2 | Tracks、Bindings、Possessables、Spawnables、事件轨。 |
| Sound / MetaSound | P2 | Graph、输入输出、Wave 引用。 |
| Custom Plugin Asset | P2 | 通过 Exporter SDK 扩展。 |

### 8.11 通用反射兜底导出

对未支持的资产类型，使用通用 UObject 反射导出：

1. 类名、包路径、Outer。
2. 所有可序列化属性。
3. UObject/SoftObject 引用。
4. 数组、Map、Set。
5. Struct 属性。
6. Enum 值。
7. 基础 flags 和 metadata。

限制：

1. 不保证表达图逻辑。
2. 不保证编辑器私有数据。
3. 不保证第三方插件自定义序列化字段可读。
4. 在 `diagnostics` 中标记 `fallbackReflectionOnly=true`。

---

## 9. 同步机制设计

### 9.1 事件监听

插件需要监听：

1. Asset 新增。
2. Asset 保存。
3. Asset 删除。
4. Asset 重命名/移动。
5. Package 保存。
6. Map 保存。
7. Blueprint 编译。
8. Source Control 状态变化，可选。
9. 文件系统外部变更，可选。

推荐策略：

1. 资产变更时只入队，不立即导出。
2. 保存完成后导出。
3. 编辑器空闲时处理低优先级队列。
4. 用户关闭编辑器前提示仍有未导出的资产。
5. Commandlet 和 CI 模式可强制同步。

### 9.2 状态机

```text
Unknown
  -> PendingInitialExport
  -> Exporting
  -> Synced
  -> Stale
  -> ExportFailed
  -> Deleted
```

状态说明：

| 状态 | 含义 |
|---|---|
| `PendingInitialExport` | 新资产已创建但尚未成功导出。 |
| `Exporting` | 正在导出。 |
| `Synced` | 语义文件与源资产匹配。 |
| `Stale` | 源资产比语义文件新，或 hash 不匹配。 |
| `ExportFailed` | 最近一次导出失败。 |
| `Deleted` | 源资产已删除，语义文件已删除或保留 tombstone。 |

### 9.3 过期检测

每个语义文件记录：

1. 源资产相对路径。
2. 源资产文件大小。
3. 源资产修改时间。
4. 源资产 hash，可配置是否启用。
5. PackageGuid。
6. 导出器版本。
7. Schema 版本。

校验规则：

1. 语义文件不存在：`missing`。
2. 源资产修改时间晚于导出时间：`stale`。
3. hash 不匹配：`stale`。
4. schema 版本旧：`schemaOutdated`。
5. 导出器版本旧：`exporterOutdated`。
6. 源资产不存在：`orphanSemantic`。

### 9.4 性能要求

1. 不在资产属性每次变化时立即深度导出。
2. 支持最大并发数配置，默认 1-2。
3. 大型 Map 导出必须显示进度。
4. 大型项目第一次索引允许较慢，但后续必须增量。
5. 单资产导出失败不能中断整个批量任务。
6. 避免在编辑器交互高峰期卡顿。

---

## 10. 用户体验需求

### 10.1 Project Settings

新增设置页：

```text
Project Settings > Plugins > AI Semantic Exporter
```

设置项：

| 设置 | 默认值 | 说明 |
|---|---|---|
| `EnableAutoExport` | true | 是否启用自动导出。 |
| `OutputRoot` | `.uesem` | 输出目录。 |
| `ExportOnAssetSave` | true | 保存后导出。 |
| `ExportOnBlueprintCompile` | false | Blueprint 编译后立即导出。 |
| `DeleteSemanticOnAssetDelete` | true | 资产删除时删除语义文件。 |
| `KeepTombstone` | false | 是否保留删除 tombstone。 |
| `IncludeMarkdownSummary` | true | 是否生成 Markdown 摘要。 |
| `IncludeCppIndex` | true | 是否生成 C++ 索引。 |
| `IncludeNodePositions` | false | 是否导出节点坐标。 |
| `IncludeEditorOnlyData` | true | 是否导出编辑器专用语义。 |
| `HashSourceAssets` | true | 是否计算源资产 hash。 |
| `PrettyJson` | true | 是否格式化 JSON。 |
| `IgnoredPaths` | `/Game/Developers/**` | 忽略路径。 |
| `IgnoredClasses` | 可配置 | 忽略资产类。 |
| `PrivacyFilters` | 可配置 | 隐私过滤规则。 |

### 10.2 状态面板

提供 Dockable Tab：

```text
Window > Developer Tools > AI Semantic Exporter
```

显示：

1. 总资产数。
2. 已同步数量。
3. 缺失数量。
4. 过期数量。
5. 失败数量。
6. 最近导出队列。
7. 最近错误。
8. 一键修复按钮。

### 10.3 错误提示

导出失败时必须提供：

1. 资产路径。
2. 资产类型。
3. 导出器名称。
4. 错误信息。
5. 是否可重试。
6. 建议处理方式。

示例：

```json
{
  "asset": "/Game/AI/BT_Enemy",
  "exporter": "BehaviorTreeExporter",
  "status": "failed",
  "message": "Blackboard asset is missing",
  "severity": "warning",
  "retryable": true
}
```

---

## 11. 离线 AI 分析包需求

### 11.1 Bundle 内容

对一个资产生成：

```text
BP_Door_context.uesem.bundle.zip
  manifest.json
  README.md
  assets/Game/Blueprints/BP_Door.uesem.json
  assets/Game/Maps/L_Main.uesem.json
  cpp/Source/MyGame/DoorBase.h
  cpp/Source/MyGame/DoorBase.cpp
  config/DefaultEngine.ini
  index/.uesem-index.json
  index/.uesem-deps.json
  graph/callgraph.mmd
  diagnostics/export-report.json
```

### 11.2 Bundle 裁剪策略

支持：

1. 当前资产。
2. 硬依赖。
3. 软依赖。
4. 反向引用。
5. C++ 父类链。
6. 被调用的 C++ 函数源码。
7. 相关配置。
8. 最大 token 预算估算。
9. 大文件摘要替代。
10. 隐私字段过滤。

### 11.3 AI 读取说明

Bundle 内 `README.md` 必须告诉 AI：

1. 先读 `manifest.json`。
2. 再读目标资产语义。
3. 按 `cppLinks` 查 C++ 文件。
4. 按 `dependencies` 查相关资产。
5. 不要猜测不存在的节点、函数或属性。
6. 结论必须引用资产路径、图名、节点名、C++ 文件路径。

---

## 12. 源码管理与 CI

### 12.1 Git/Perforce 策略

推荐把 `.uesem/` 纳入版本控制，但排除临时日志和缓存：

```gitignore
.uesem/logs/
.uesem/cache/
.uesem/tmp/
!.uesem/schema/
!.uesem/index/
!.uesem/content/
!.uesem/maps/
!.uesem/cpp/
```

对于 Perforce：

1. `.uasset` / `.umap` 仍按二进制锁定工作流。
2. `.uesem.json` / `.uesem.md` 按文本文件处理。
3. 提交前检查资产和语义文件是否同步。
4. 删除/重命名资产时，语义文件必须一起提交。

### 12.2 CI 校验

提供命令：

```powershell
UnrealEditor-Cmd.exe MyGame.uproject -run=USEMValidate -FailOnStale=true -Unattended
```

CI 检查：

1. 是否存在缺失语义文件。
2. 是否存在过期语义文件。
3. schema 是否有效。
4. JSON 是否可解析。
5. 是否存在孤儿语义文件。
6. C++ 符号索引是否过期。
7. Bundle 是否可生成。

---

## 13. 安全、隐私与合规

1. 插件默认只读 `.uasset` / `.umap`，只写 `.uesem/`。
2. 不默认上传任何文件到外部服务。
3. MCP Toolset 仅监听本机 loopback。
4. 导出 Bundle 前允许用户预览文件列表。
5. 提供隐私过滤规则：
   - API Key。
   - Token。
   - 私有服务器地址。
   - 用户真实路径。
   - 内部账号。
   - 未发布剧情文本，可按项目规则配置。
6. 日志不记录完整敏感字段值。
7. 对第三方插件资产，只导出用户项目内可访问语义，不绕过权限或加密。

---

## 14. 兼容性与限制

### 14.1 引擎版本

P0 支持 UE5.8。  
P1 可扩展 UE5.7/5.6。  
每个语义文件必须记录：

```json
{
  "engine": {
    "version": "5.8.0"
  },
  "exporter": {
    "name": "USEMBlueprintExporter",
    "version": "1.0.0"
  }
}
```

### 14.2 Cooked 资产限制

本插件主要面向未 Cook 的源工程资产。Cooked 资产不作为 P0 支持对象。

原因：

1. Cooked 过程可能剥离编辑器数据。
2. Blueprint 图结构可能不可完整恢复。
3. 第三方反解析容易受引擎版本和项目设置影响。
4. 合规风险更高。

该限制不影响本文定义的离线分析能力：离线分析的对象是插件提前导出的 `.uesem/` 语义文件，而不是 Cooked 包或原始资产二进制文件。

### 14.3 第三方插件资产

默认使用反射兜底导出。若需要完整语义，应提供专用 Exporter：

```cpp
class IUSEMAssetExporter
{
public:
    virtual bool CanExport(const FAssetData& AssetData) const = 0;
    virtual FUSEMExportResult Export(const FUSEMExportContext& Context) = 0;
};
```

---

## 15. 验收标准

### 15.1 P0 验收

1. 在 UE5.8 空项目中创建 Blueprint，保存后自动生成 `.uesem.json`。
2. 修改 Blueprint 变量默认值，保存后 `.uesem.json` 内容更新。
3. 删除 Blueprint 后，对应语义文件被删除或生成 tombstone。
4. 重命名 Blueprint 后，对应语义文件移动且内部路径更新。
5. 创建 Map 并放置 Actor，保存后 `.umap` 语义文件包含 Actor 列表和 Transform。
6. Blueprint 调用一个 C++ `BlueprintCallable` 函数时，语义文件能链接到 C++ 类、函数、头文件和源文件。
7. 关闭 UE 编辑器后，AI 仅通过 `.uesem/` 和 C++ 文件可以理解该 Blueprint 的主要功能逻辑。
8. 批量导出 100 个资产时，失败资产不影响其他资产导出。
9. 运行 `USEMValidate` 能发现缺失、过期、孤儿语义文件。
10. JSON 输出稳定，同一资产未变化时连续导出结果一致。

### 15.2 P1 验收

1. Anim Blueprint 能导出状态机、Transition Rule 和 AnimGraph。
2. Behavior Tree 能导出 Task、Service、Decorator、Blackboard Key。
3. Material 能导出参数、表达式图和 Texture 引用。
4. AI Bundle 能按依赖范围生成最小上下文包。
5. UE5.8 MCP 能调用本插件 Toolset 查询和触发导出。
6. CI 可在无界面模式下执行导出和校验。

---

## 16. 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| `.uasset` / `.umap` 二进制结构复杂 | 如果误把需求理解为 AI 直接离线读原始资产，会导致实现路线错误 | 明确产品主路径：在 UE Editor/Commandlet 内导出语义文件，AI 离线阶段只读 `.uesem/`、C++、配置和索引。 |
| 不同资产类型语义差异大 | 通用导出不够解释逻辑 | 建立 Exporter Registry，按资产类型逐步覆盖。 |
| 自动导出影响编辑器性能 | 用户保存变慢或卡顿 | 队列、debounce、后台导出、手动/CI 模式可配置。 |
| 语义文件过期 | AI 分析错误 | hash、mtime、PackageGuid、CI 校验、状态面板。 |
| C++ 链接不准确 | AI 找错源码 | UE 反射优先，clang/compile_commands 作为增强。 |
| World Partition 外部 Actor 复杂 | Map 语义不完整 | P0 做 Actor 摘要，P1 增强外部 Actor 包和 DataLayer。 |
| 第三方插件资产不可完整反射 | 语义缺失 | 兜底反射 + 自定义 Exporter SDK。 |
| 导出文件太大 | AI 上下文爆炸 | Bundle 裁剪、摘要、依赖深度、token 预算。 |
| 团队不愿提交生成文件 | 语义无法离线 | 支持 CI 生成、缓存模式、只提交索引或 Bundle。 |

---

## 17. 版本规划

### 17.1 MVP，4-6 周

1. 插件基础框架。
2. Project Settings。
3. 手动导出 Blueprint、Map、DataAsset、DataTable。
4. 自动保存后导出。
5. 删除/重命名同步。
6. 项目索引和依赖索引。
7. C++ 反射符号索引基础版。
8. JSON Schema 和 Markdown 摘要。
9. Validate Commandlet。

### 17.2 v1.0，8-12 周

1. 稳定的 Exporter Registry。
2. Widget Blueprint、Anim Blueprint、Behavior Tree、Material 导出器。
3. AI Bundle。
4. MCP Toolset。
5. CI 集成。
6. 语义 diff。
7. 性能优化。
8. 完整文档和示例项目。

### 17.3 v1.5+

1. Niagara、PCG、Control Rig、Level Sequence、MetaSound。
2. clang 深度 C++ 调用索引。
3. SQLite 本地检索库。
4. 外部 AI 工具插件。
5. 大项目增量索引服务。

---

## 18. 推荐实现顺序

1. 先实现 Blueprint 单资产手动导出。
2. 再实现 Map Actor 摘要导出。
3. 再实现资产索引和依赖图。
4. 再接入保存/删除/重命名同步。
5. 再实现 C++ 反射索引。
6. 再实现离线 Bundle。
7. 最后接 UE5.8 MCP Toolset。

这样可以最快验证核心价值：AI 能不能在不启动 UE 的情况下读懂一个 Blueprint + C++ 类 + Map 实例的完整功能链。

---

## 19. 示例 AI 分析提示

```text
你正在分析 UE5.8 项目导出的 USEM 离线语义包。

请先读取 manifest.json，然后读取目标资产：
/Game/Blueprints/BP_Door

分析要求：
1. 结合 BP_Door.uesem.json、L_Main.uesem.json 和 cpp-symbol-index.json。
2. 找出玩家靠近门后不能打开的可能原因。
3. 结论必须引用资产路径、图名、节点名、Pin 名、C++ 文件路径。
4. 不允许猜测不存在的 Blueprint 节点或 C++ 函数。
5. 如果语义文件 stale，必须先报告 stale 风险。
```

---

## 20. 参考资料

1. Epic Games，Unreal MCP in Unreal Editor：`https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor`
2. Epic Games，Asset Registry：`https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-registry-in-unreal-engine`
3. Epic Games，Scripting the Unreal Editor Using Python：`https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python`
4. Epic Games，Using Perforce as Source Control：`https://dev.epicgames.com/documentation/en-us/unreal-engine/using-perforce-as-source-control-for-unreal-engine`
5. Epic Games，Blueprint Compiler Overview：`https://dev.epicgames.com/documentation/en-us/unreal-engine/compiler-overview-for-blueprints-visual-scripting-in-unreal-engine`
6. Epic Games，Working with Cooked Content：`https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-cooked-content-in-the-unreal-engine`
7. BP2AI，Blueprint to Text / Export to AI Unreal Plugin：`https://www.a-maze.games/blog/blueprint-to-text-bp2ai-export-to-ai-unreal-plugin`
8. BlueprintUE：`https://blueprintue.com/`
9. UAssetAPI：`https://atenfyr.github.io/UAssetAPI/`
10. BlueprintSerializer：`https://github.com/Jinphinity/BlueprintSerializer`
