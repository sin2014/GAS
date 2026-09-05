# UE_Ring No-UE MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在本机没有 Unreal Editor 的情况下，先建立 UE_Ring 的离线语义格式、样例、校验工具、测试和 UE5.8 Editor 插件源码骨架。

**Architecture:** 第一阶段把可验证部分放在 UE 无关目录：`schemas/`、`examples/`、`tools/`、`tests/`。UE 插件源码骨架放在 `Source/` 和 `.uplugin`，只做静态结构准备，等有 UE5.8 后再编译验证。

**Tech Stack:** Python 3 标准库、JSON、Unreal Engine 5.8 C++ Editor Plugin 源码骨架、PowerShell。

---

## 文件结构

```text
D:\Code\UE_Ring
  README.md
  .gitignore
  .editorconfig
  UERing.uplugin
  schemas\
    usem.asset.schema.json
    usem.index.schema.json
  examples\
    BP_Door.uesem.json
    assets.uesem-index.json
  tools\
    usem_validate.py
  tests\
    test_usem_validate.py
  Source\
    UERingCore\
      UERingCore.Build.cs
      Public\
        UERingVersion.h
      Private\
        UERingCoreModule.cpp
    UERingEditor\
      UERingEditor.Build.cs
      Public\
        UERingEditorModule.h
      Private\
        UERingEditorModule.cpp
```

## Task 1: 项目基础文件

**Files:**
- Create: `D:\Code\UE_Ring\README.md`
- Create: `D:\Code\UE_Ring\.gitignore`
- Create: `D:\Code\UE_Ring\.editorconfig`

- [ ] **Step 1: 创建 README**

写入项目定位、无 UE 环境边界、第一阶段目标。

- [ ] **Step 2: 创建 `.gitignore`**

忽略 UE 生成目录、Python 缓存、参考资料下载临时目录：

```gitignore
Binaries/
Intermediate/
Saved/
DerivedDataCache/
.vs/
*.sln
*.suo
*.opensdf
*.VC.db
__pycache__/
.pytest_cache/
references/_downloads/
references/_extracted/
```

- [ ] **Step 3: 创建 `.editorconfig`**

统一 UTF-8、空格、换行，C++ 文件使用 4 空格，JSON/Markdown 使用 2 空格或默认 Markdown 风格。

## Task 2: USEM Schema 和样例

**Files:**
- Create: `D:\Code\UE_Ring\schemas\usem.asset.schema.json`
- Create: `D:\Code\UE_Ring\schemas\usem.index.schema.json`
- Create: `D:\Code\UE_Ring\examples\BP_Door.uesem.json`
- Create: `D:\Code\UE_Ring\examples\assets.uesem-index.json`

- [ ] **Step 1: 创建资产语义 schema**

`usem.asset.schema.json` 必须要求这些顶层字段：

```json
["schema", "schemaVersion", "engine", "project", "asset", "dependencies", "semantics", "cppLinks", "diagnostics"]
```

- [ ] **Step 2: 创建索引 schema**

`usem.index.schema.json` 必须要求这些顶层字段：

```json
["schema", "schemaVersion", "engine", "project", "generatedAtUtc", "assets"]
```

- [ ] **Step 3: 创建 Blueprint 样例**

`BP_Door.uesem.json` 表达一个 Blueprint，包含 `EventGraph`、两个节点、一个 exec link、一个 C++ 函数链接。

- [ ] **Step 4: 创建资产索引样例**

`assets.uesem-index.json` 引用 `BP_Door.uesem.json`，状态为 `ok`。

## Task 3: 离线校验工具 TDD

**Files:**
- Create: `D:\Code\UE_Ring\tests\test_usem_validate.py`
- Create: `D:\Code\UE_Ring\tools\usem_validate.py`

- [ ] **Step 1: 写失败测试**

测试行为：

1. 有效 asset 文件返回成功。
2. 缺少必填字段的 asset 文件返回失败。
3. 有效 index 文件返回成功。
4. index 引用不存在的语义文件时返回失败。

运行：

```powershell
python -m unittest discover -s tests -v
```

预期：失败，原因是 `tools.usem_validate` 尚不存在。

- [ ] **Step 2: 写最小实现**

实现 `tools/usem_validate.py`：

1. 使用 Python 标准库 `json`。
2. 检查 asset/index 顶层必填字段。
3. 检查 index 中每个 `semanticFile` 是否存在。
4. CLI 支持：

```powershell
python tools/usem_validate.py examples\BP_Door.uesem.json
python tools/usem_validate.py examples\assets.uesem-index.json --root .
```

- [ ] **Step 3: 运行测试**

运行：

```powershell
python -m unittest discover -s tests -v
```

预期：通过。

## Task 4: UE5.8 插件骨架

**Files:**
- Create: `D:\Code\UE_Ring\UERing.uplugin`
- Create: `D:\Code\UE_Ring\Source\UERingCore\UERingCore.Build.cs`
- Create: `D:\Code\UE_Ring\Source\UERingCore\Public\UERingVersion.h`
- Create: `D:\Code\UE_Ring\Source\UERingCore\Private\UERingCoreModule.cpp`
- Create: `D:\Code\UE_Ring\Source\UERingEditor\UERingEditor.Build.cs`
- Create: `D:\Code\UE_Ring\Source\UERingEditor\Public\UERingEditorModule.h`
- Create: `D:\Code\UE_Ring\Source\UERingEditor\Private\UERingEditorModule.cpp`

- [ ] **Step 1: 创建 `.uplugin`**

插件类型为 Editor，包含 `UERingCore` 和 `UERingEditor` 两个模块。`UERingEditor` 依赖 Editor 阶段加载。

- [ ] **Step 2: 创建 Core 模块**

只包含版本常量和模块启动/关闭日志。

- [ ] **Step 3: 创建 Editor 模块**

只包含模块启动/关闭日志，不注册菜单，不读取资产。资产索引和 Blueprint 导出在有 UE 后按单独任务实现。

- [ ] **Step 4: 静态复查**

检查模块名、Build.cs 依赖、include 路径、IMPLEMENT_MODULE 名称一致。由于本机没有 UE，不运行编译。

## Task 5: 文档更新

**Files:**
- Modify: `D:\Code\UE_Ring\README.md`

- [ ] **Step 1: 增加当前状态**

写明：

1. 本机没有 UE Editor。
2. 已完成离线 Schema/校验基础设施。
3. UE C++ 插件骨架未编译。
4. 下一步需要 UE5.8 环境验证 `.uplugin` 加载和模块编译。

## 自检清单

- [ ] `python -m unittest discover -s tests -v` 通过。
- [ ] `python tools/usem_validate.py examples\BP_Door.uesem.json` 返回成功。
- [ ] `python tools/usem_validate.py examples\assets.uesem-index.json --root .` 返回成功。
- [ ] 所有新增文档为中文优先。
- [ ] 没有复制商业插件代码。
- [ ] 没有把 references 目录内容合入插件源码。
- [ ] 明确说明未进行 UE 编译和编辑器加载验证。

