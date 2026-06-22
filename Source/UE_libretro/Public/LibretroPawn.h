#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LibretroRunner.h"
#include "LibretroPawn.generated.h"

class ULibretroWidget;
class UAudioComponent;

/**
 * libretro 示例工程的 UE 游戏入口 Pawn。
 *
 * 这个 Pawn 负责把 UE 世界、UMG、键盘输入、音频播放和 libretro Runner 连接起来。
 * 自动截图验证逻辑也放在这里，方便命令行启动后自动加载 ROM、等待出帧、截图并退出。
 */
UCLASS()
class UE_LIBRETRO_API ALibretroPawn : public APawn
{
    GENERATED_BODY()

public:
    /** 开启 Tick，并自动接管 Player0，让键盘输入能直接到达本 Pawn。 */
    ALibretroPawn();

    /** 创建 Runner 和 UMG，并根据命令行参数决定是否自动启动某个 ROM。 */
    virtual void BeginPlay() override;

    /** 上传视频帧、启动音频组件、驱动自动截图倒计时，并刷新 UI 状态。 */
    virtual void Tick(float DeltaSeconds) override;

    /** 退出游戏时停止 Runner，确保 core、线程和 DLL 被按顺序释放。 */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 把 PC 键盘按键绑定到 libretro RetroPad 和触摸输入。 */
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    /** 启动配置好的 FC/NES 重装机兵 1 ROM。 */
    UFUNCTION(BlueprintCallable)
    void StartNesRom();

    /** 启动 Metal Max 2 Reloaded NDS ROM。 */
    UFUNCTION(BlueprintCallable)
    void StartMetalMax2R();

    /** 启动 Metal Max 3 NDS ROM。 */
    UFUNCTION(BlueprintCallable)
    void StartMetalMax3();

    /** 启动重装机兵 4 3DS ROM。 */
    UFUNCTION(BlueprintCallable)
    void StartMetalMax4();

    /** 返回当前 Runner，供 UMG 读取视频纹理和状态文本。 */
    FLibretroRunner* GetRunner() const { return Runner.Get(); }

private:
    /** 命令行 -AutoRom=... 可选择的自动启动目标。 */
    enum class EAutoRomTarget : uint8
    {
        /** 未指定自动启动目标。 */
        None,

        /** 自动启动 FC/NES ROM。 */
        NES,

        /** 自动启动 Metal Max 2 Reloaded。 */
        MM2R,

        /** 自动启动 Metal Max 3。 */
        MM3,

        /** 自动启动重装机兵 4。 */
        MM4
    };

    /** 停止当前音频/会话状态，并用指定配置启动新 ROM。 */
    void StartRom(const FLibretroLaunchConfig& Config);

    /** 构造 FCEUmm + FC/NES ROM 的启动配置。 */
    FLibretroLaunchConfig MakeNesConfig() const;

    /** 构造 DeSmuME + 指定 NDS ROM 的启动配置。 */
    FLibretroLaunchConfig MakeNdsConfig(const FString& RomFileName, const FString& DisplayName) const;

    /** 构造 Azahar + 重装机兵 4 的 3DS 启动配置和 core 选项。 */
    FLibretroLaunchConfig Make3dsConfig() const;

    /** 在目录树中查找指定 ROM 文件名；找不到时返回 Directory/FileName 方便报错。 */
    FString FindRomByFileName(const FString& Directory, const FString& FileName) const;

    /** 从当前进程命令行解析 -AutoRom=NES/MM2R/MM3/MM4。 */
    EAutoRomTarget ParseAutoRomTarget() const;

    /** 返回适合用作自动截图文件名的名称。 */
    FString GetScreenshotStem() const;

    /** 把一个 RetroPad 按键状态转发给 Runner。 */
    void SetButton(ELibretroButton Button, bool bPressed);

    /** 方向键下：请求即时存档到当前 ROM 的单槽状态文件。 */
    void QuickSave();

    /** 方向键上：请求从当前 ROM 的单槽状态文件即时读档。 */
    void QuickLoad();

    /** 方向键左：把 libretro 主循环速度降低一个档位。 */
    void DecreaseEmulationSpeed();

    /** 方向键右：把 libretro 主循环速度提高一个档位。 */
    void IncreaseEmulationSpeed();

    /** 以下 Press/Release 函数是键盘绑定入口，分别转成 RetroPad 状态。 */
    void PressUp();
    void ReleaseUp();
    void PressDown();
    void ReleaseDown();
    void PressLeft();
    void ReleaseLeft();
    void PressRight();
    void ReleaseRight();
    void PressConfirm();
    void ReleaseConfirm();
    void PressCancel();
    void ReleaseCancel();
    void PressX();
    void ReleaseX();
    void PressY();
    void ReleaseY();
    void PressL();
    void ReleaseL();
    void PressR();
    void ReleaseR();
    void PressZL();
    void ReleaseZL();
    void PressZR();
    void ReleaseZR();
    void PressSelect();
    void ReleaseSelect();
    void PressStart();
    void ReleaseStart();
    void PressTouch();
    void ReleaseTouch();

private:
    /** 非 UObject 前端，拥有 libretro core 线程和所有 libretro 回调。 */
    TUniquePtr<FLibretroRunner> Runner;

    /** 纯 C++ UMG，显示启动按钮、状态文本和视频画面。 */
    TObjectPtr<ULibretroWidget> Widget;

    /** 播放 Runner 中 USoundWaveProcedural 的音频组件。 */
    TObjectPtr<UAudioComponent> AudioComponent;

    /** 命令行是否带有 -AutoScreenshot。 */
    bool bAutoScreenshot = false;

    /** 首帧到达后是否已经安排自动截图。 */
    bool bScreenshotRequested = false;

    /** 自动截图倒计时，用于等待画面稳定后再截图。 */
    float ScreenshotDelay = 0.0f;

    /** 当前 ROM 的显示名称，用于 UI 和截图命名。 */
    FString ActiveDisplayName;

    /** 从 ActiveDisplayName 派生出的截图文件名前缀。 */
    FString ActiveScreenshotStem = TEXT("Libretro");
};
