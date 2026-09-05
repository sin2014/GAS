#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/SceneComponent.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "InputAction.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "UERingNativeBindWidgetTestBase.generated.h"

class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUERingSemanticTestDelegate);

USTRUCT()
struct FUERingSemanticInstancedStructPayload
{
    GENERATED_BODY()

    UPROPERTY()
    float SemanticScale = 0.0f;

    UPROPERTY()
    FName SemanticMode;
};

UCLASS()
class UUERingNativeBindWidgetTestBase : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SemanticLabel;
};

UCLASS()
class AUERingBlueprintDefaultsTestBase : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UERing Test")
    int32 InheritedSemanticValue = 7;

    UPROPERTY(EditAnywhere, Instanced, Category = "UERing Test")
    TObjectPtr<USceneComponent> InstancedSemanticConfig;
};

UCLASS()
class UUERingInputActionDomainTestAsset : public UInputAction
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TOptional<int32> OptionalSetValue;

    UPROPERTY()
    TOptional<int32> OptionalUnsetValue;

    UPROPERTY()
    FUERingSemanticTestDelegate EmptySemanticDelegate;

    UPROPERTY()
    FInstancedPropertyBag SemanticPropertyBag;

    UPROPERTY()
    FInstancedStruct SemanticInstancedStruct;
};
