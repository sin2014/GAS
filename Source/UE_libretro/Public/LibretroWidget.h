#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LibretroWidget.generated.h"

class ALibretroPawn;
class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

UCLASS()
class UE_LIBRETRO_API ULibretroWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetOwningPawn(ALibretroPawn* InPawn);
    void RefreshFromRunner();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:
    UButton* CreateLauncherButton(UPanelWidget* Parent, const FName& Name, const FString& Text);

    UFUNCTION()
    void HandleStartNesClicked();

    UFUNCTION()
    void HandleStartMM2RClicked();

    UFUNCTION()
    void HandleStartMM3Clicked();

    UFUNCTION()
    void HandleStartMM4Clicked();

private:
    TWeakObjectPtr<ALibretroPawn> OwningPawn;
    TObjectPtr<UImage> VideoImage;
    TObjectPtr<UTextBlock> StatusText;
    TObjectPtr<UButton> StartNesButton;
    TObjectPtr<UButton> StartMM2RButton;
    TObjectPtr<UButton> StartMM3Button;
    TObjectPtr<UButton> StartMM4Button;
    TObjectPtr<UTexture2D> CurrentVideoTexture;
};
