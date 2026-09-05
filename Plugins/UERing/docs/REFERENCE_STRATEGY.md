# UE_Ring 参考实现获取与复用策略

文档版本：v1.0  
编写日期：2026-07-29  
项目目录：`D:\Code\UE_Ring`

## 1. 总原则

UE_Ring 的目标是最终可以开源共享，因此参考成熟产品时必须区分三种行为：

1. **可以直接复用**：许可证明确兼容开源发布的代码，例如 MIT、Apache-2.0，并按许可证保留版权、LICENSE、NOTICE。
2. **可以参考实现思路**：许可证不明、商业插件、闭源插件、Fab/Marketplace 插件、ArtStation 商业脚本，只能用于功能对标、输出格式对比、黑盒行为观察、公共 API 观察和需求校准。
3. **不能复制进本项目**：商业插件源码、闭源源码、许可证不明源码、Fab/Marketplace 插件实现、GPL 代码，以及任何与计划开源许可证不兼容的代码。

“该插件可能由 AI 生成”或“该插件可能参考了开源项目”并不会自动授予复制、改写或开源再发布的权利。若要最终开源，UE_Ring 只能包含：

- 我们自己编写的代码。
- 许可证明确兼容的第三方代码。
- 按许可证合规引入的第三方依赖。

## 2. 商业插件使用边界

商业插件可以作为成熟产品参考，但必须采用以下方式：

1. 可以购买、安装、运行、导出样例、记录功能清单和输出结构。
2. 可以用同一组测试 Blueprint / Map 对比它们的输出与 UE_Ring 的输出。
3. 可以整理“行为规格”，例如导出了哪些字段、执行流如何表达、错误如何报告。
4. 不把商业插件的源码、资源、私有 schema、独有文本模板直接复制进 UE_Ring。
5. 如果用户本机已经合法安装商业插件，Codex 可以读取其公开配置、文档、示例输出和可见 API；源码级阅读只用于理解行为，不作为复制来源。
6. 若未来需要开源，必须在提交前移除所有商业插件代码、资产、模板和许可证不明内容。

推荐采用“行为对标 + 独立实现”：

```text
成熟插件输出样例
  -> 行为与字段清单
  -> UE_Ring 自有 USEM schema
  -> UE_Ring 自有 C++ Editor 插件实现
```

## 3. 已获取参考源码状态

当前已放入 `D:\Code\UE_Ring\references`：

| 项目 | 获取状态 | 本地位置 | 许可证/复用级别 | 用途 |
|---|---|---|---|---|
| BlueprintSerializer | 已通过 GitHub codeload zip 获取 | `references\_extracted\BlueprintSerializer\BlueprintSerializer-main` | 未发现随 zip 提供的 LICENSE 文件；只可参考思路，暂不复制代码 | Blueprint JSON schema、模块划分、导出字段、设置项参考 |
| NodeToCode | 已通过 GitHub codeload zip 获取，媒体资源解压有损但源码可读 | `references\_extracted\NodeToCode_tar\NodeToCode-main` | Apache-2.0，可合规参考；复制需保留 license/notice | LLM/伪代码/C++ 转换下游、Editor 插件结构、Prompt 组织 |
| UeBlueprintDumper | 已通过 GitHub codeload zip 获取 | `references\_extracted\UeBlueprintDumper\UeBlueprintDumper-main` | Apache-2.0，可合规参考；注意其 cooked/逆向场景边界 | Blueprint bytecode/metadata dumper、CLI 参数和版本处理参考 |
| ueblueprint | 已通过 GitHub codeload zip 获取 | `references\_extracted\ueblueprint\ueblueprint-master` | MIT，可合规参考 | Blueprint 剪贴板文本解析和可视化参考 |
| unreal-blueprint-reader | 已通过 git clone 获取 | `references\unreal-blueprint-reader` | MIT，可合规参考 | UE Editor 插件读取 Blueprint 图并暴露 JSON 的最小结构参考 |
| UAssetAPI | 整仓 zip 含大测试资产导致下载/解压不稳定；已通过 GitHub tree + raw 下载核心部分 | `references\UAssetAPI-selected` | MIT，含 NOTICE；可合规参考 | 低层 `.uasset/.umap` 结构、属性、usmap、JSON/二进制对照参考 |

临时下载文件位于：

```text
D:\Code\UE_Ring\references\_downloads
D:\Code\UE_Ring\references\_extracted
```

这些目录是参考资料，不应直接作为 UE_Ring 插件源码的一部分。

## 4. 获取失败或部分失败处理

GitHub `git clone` 对部分仓库多次超时。已改用：

1. GitHub 官方 `codeload.github.com` zip。
2. GitHub API tree + `raw.githubusercontent.com` 分文件下载。
3. NuGet/npm 等官方包渠道作为补充。

对 `UAssetAPI`，整仓包含大型测试资产，后续若需要完整源码，优先继续通过 GitHub API 按路径下载核心 `UAssetAPI/**/*.cs`，不下载 Benchmark/TestAssets 中的大型 `.umap/.uexp`。

镜像代理可以作为最后兜底，但必须满足：

1. 只从原 GitHub 仓库镜像下载。
2. 下载后校验 LICENSE、README、commit/branch 对应关系。
3. 不把镜像站私有修改内容视为可信上游。
4. 重要依赖最终仍以官方 GitHub、NuGet、npm 或作者发布渠道为准。

## 5. UE_Ring 实现参考优先级

### P0：必须优先阅读

1. `unreal-blueprint-reader`：UE Editor 插件如何遍历 Blueprint 图并输出 JSON。
2. `BlueprintSerializer`：面向 AI 的 Blueprint 结构化数据模型和导出范围。
3. `NodeToCode`：成熟 UE 插件目录结构、AI 消费端和 Prompt/输出管理。
4. UE5.8 官方 API：Asset Registry、UPackage 保存事件、BlueprintGraph、Commandlet、ToolsetRegistry。

### P1：作为辅助参考

1. `UAssetAPI-selected`：只用于低层结构理解和诊断，不进入 MVP 主路径。
2. `UeBlueprintDumper`：只用于 bytecode/metadata 和 CLI 思路参考，不采用其 cooked/逆向路径作为主流程。
3. `ueblueprint`：只用于剪贴板文本和图可视化参考。

### P2：商业插件行为对标

1. BP2AI。
2. Blueprint Exporter。
3. ArtStation UE5 Blueprint Dumper。

这些只做产品能力、输出体验和字段覆盖对标。

## 6. 实现路线约束

UE_Ring MVP 主路径必须是 UE5.8 Editor C++ 插件：

1. 用 UE Editor/Commandlet 读取资产。
2. 用 Asset Registry 建索引。
3. 用 `UBlueprint`、`UEdGraph`、`UEdGraphNode`、`UEdGraphPin` 读取 Blueprint 图。
4. 用 `UWorld`、`ULevel`、`AActor` 读取当前 Map/World。
5. 输出自有 `USEM` JSON schema 和 Markdown 摘要。
6. AI 离线阶段读取 `.uesem/`、C++、配置和索引，不直接读取原始 `.uasset/.umap` 二进制。
7. 不写回 `.uasset/.umap`。

