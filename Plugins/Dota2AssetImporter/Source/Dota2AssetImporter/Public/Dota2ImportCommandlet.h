// 防止头文件在同一个编译单元中被重复包含。
#pragma once

// 引入 Unreal 的 Commandlet 基类。
// Commandlet 是一种可通过 UnrealEditor-Cmd.exe -run=Name 执行的编辑器命令行工具。
#include "Commandlets/Commandlet.h"

// 引入 UnrealHeaderTool 生成的反射代码声明。
// 任何包含 UCLASS/USTRUCT/UENUM 的头文件都需要在本文件最后一个 include 位置包含对应 generated.h。
#include "Dota2ImportCommandlet.generated.h"

// 声明一个名为 Dota2Import 的编辑器 Commandlet。
// Unreal 会根据类名 UDota2ImportCommandlet 去掉前缀 U 和后缀 Commandlet，
// 因此命令行中使用 -run=Dota2Import 即可执行它。
UCLASS()
class UDota2ImportCommandlet final : public UCommandlet
{
    // GENERATED_BODY 会展开 Unreal 反射系统所需的构造、类型注册和元数据代码。
    GENERATED_BODY()

public:
    // 构造函数负责声明该 Commandlet 的运行环境，例如是否是编辑器命令、是否输出到控制台。
    UDota2ImportCommandlet();

    // Commandlet 主入口。
    // Params 是 Unreal 传入的命令行参数字符串，例如：
    // -Manifest="..." -Dest="/Game/..." -Character="ShadowFiend" -ReplaceExisting
    // 返回 0 表示成功，非 0 表示失败。
    virtual int32 Main(const FString& Params) override;
};
