#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LibretroNESRunner.h"
#include "LibretroPawn.generated.h"

class ULibretroWidget;
class UAudioComponent;

UCLASS()
class UE_LIBRETRO_API ALibretroPawn : public APawn
{
    GENERATED_BODY()

public:
    ALibretroPawn();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UFUNCTION(BlueprintCallable)
    void StartNesRom();

    UFUNCTION(BlueprintCallable)
    void StartMetalMax2R();

    UFUNCTION(BlueprintCallable)
    void StartMetalMax3();

    FLibretroNESRunner* GetRunner() const { return Runner.Get(); }

private:
    enum class EAutoRomTarget : uint8
    {
        None,
        NES,
        MM2R,
        MM3
    };

    void StartRom(const FLibretroLaunchConfig& Config);
    FLibretroLaunchConfig MakeNesConfig() const;
    FLibretroLaunchConfig MakeNdsConfig(const FString& RomFileName, const FString& DisplayName) const;
    FString FindRomByFileName(const FString& Directory, const FString& FileName) const;
    EAutoRomTarget ParseAutoRomTarget() const;
    FString GetScreenshotStem() const;

    void SetButton(ELibretroButton Button, bool bPressed);
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
    void PressSelect();
    void ReleaseSelect();
    void PressStart();
    void ReleaseStart();
    void PressTouch();
    void ReleaseTouch();

private:
    TUniquePtr<FLibretroNESRunner> Runner;
    TObjectPtr<ULibretroWidget> Widget;
    TObjectPtr<UAudioComponent> AudioComponent;
    bool bAutoScreenshot = false;
    bool bScreenshotRequested = false;
    float ScreenshotDelay = 0.0f;
    FString ActiveDisplayName;
    FString ActiveScreenshotStem = TEXT("Libretro");
};
