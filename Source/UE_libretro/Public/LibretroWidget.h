#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LibretroWidget.generated.h"

class ALibretroPawn;
class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

/**
 * 纯 C++ 构建的 libretro 启动与显示界面。
 *
 * 这个界面只负责 UI：创建按钮、显示状态、把 Runner 的 UTexture2D 绑定到 Image。
 * 真正的 core 加载、输入状态和音视频处理都由 ALibretroPawn / FLibretroRunner 完成。
 */
UCLASS()
class UE_LIBRETRO_API ULibretroWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 设置所属 Pawn，按钮点击和状态刷新都通过它访问 Runner。 */
    void SetOwningPawn(ALibretroPawn* InPawn);

    /** 从 Runner 拉取最新纹理和状态文本，供 Tick 中调用。 */
    void RefreshFromRunner();

protected:
    /** 用 C++ 构建完整 UMG 控件树，不依赖 .uasset 界面蓝图。 */
    virtual TSharedRef<SWidget> RebuildWidget() override;

    /** 绑定四个启动按钮的点击事件。 */
    virtual void NativeConstruct() override;

private:
    /** 创建一个固定尺寸的 ROM 启动按钮，并加入指定父容器。 */
    UButton* CreateLauncherButton(UPanelWidget* Parent, const FName& Name, const FString& Text);

    /** 点击 FC/NES 按钮时启动重装机兵 1。 */
    UFUNCTION()
    void HandleStartNesClicked();

    /** 点击 MM2R 按钮时启动 Metal Max 2 Reloaded。 */
    UFUNCTION()
    void HandleStartMM2RClicked();

    /** 点击 MM3 按钮时启动 Metal Max 3。 */
    UFUNCTION()
    void HandleStartMM3Clicked();

    /** 点击 MM4 按钮时启动重装机兵 4。 */
    UFUNCTION()
    void HandleStartMM4Clicked();

private:
    /** 弱引用所属 Pawn，避免 UI 和 Pawn 形成强引用生命周期问题。 */
    TWeakObjectPtr<ALibretroPawn> OwningPawn;

    /** 显示 libretro 视频纹理的 UMG Image。 */
    TObjectPtr<UImage> VideoImage;

    /** 显示当前运行状态、错误或操作提示的文本。 */
    TObjectPtr<UTextBlock> StatusText;

    /** 启动 FC/NES ROM 的按钮。 */
    TObjectPtr<UButton> StartNesButton;

    /** 启动 Metal Max 2 Reloaded 的按钮。 */
    TObjectPtr<UButton> StartMM2RButton;

    /** 启动 Metal Max 3 的按钮。 */
    TObjectPtr<UButton> StartMM3Button;

    /** 启动重装机兵 4 的按钮。 */
    TObjectPtr<UButton> StartMM4Button;

    /** 当前已经绑定到 VideoImage 的纹理，用于避免重复绑定。 */
    TObjectPtr<UTexture2D> CurrentVideoTexture;
};
