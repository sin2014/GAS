// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"
#include "LyraCharacterPartTypes.h"

#include "LyraControllerComponent_CharacterParts.generated.h"

class APawn;
class ULyraPawnComponent_CharacterParts;
class UObject;
struct FFrame;

enum class ECharacterPartSource : uint8
{
	Natural,

	NaturalSuppressedViaCheat,

	AppliedViaDeveloperSettingsCheat,

	AppliedViaCheatManager
};

//////////////////////////////////////////////////////////////////////

// Controller 侧保存的角色部件请求，可跨 Pawn 更换重新应用。
// A character part requested on a controller component
USTRUCT()
struct FLyraControllerCharacterPartEntry
{
	GENERATED_BODY()

	FLyraControllerCharacterPartEntry()
	{}

public:
	// 该条目描述的角色部件。
	// The character part being represented
	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))
	FLyraCharacterPart Part;

	// 已应用到当前 Pawn 时，由 Pawn 侧部件列表返回的句柄。
	// The handle if already applied to a pawn
	FLyraCharacterPartHandle Handle;

	// 部件来源，用于区分自然装配、开发者设置和 CheatManager 覆盖。
	// The source of this part
	ECharacterPartSource Source = ECharacterPartSource::Natural;
};

//////////////////////////////////////////////////////////////////////

// 保存 Controller 的外观装配方案，并在其接管新 Pawn 时将有效部件迁移到 Pawn 侧复制组件。
// A component that configure what cosmetic actors to spawn for the owning controller when it possesses a pawn
UCLASS(meta = (BlueprintSpawnableComponent))
class ULyraControllerComponent_CharacterParts : public UControllerComponent
{
	GENERATED_BODY()

public:
	ULyraControllerComponent_CharacterParts(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	// 权威端为该 Controller 的装配方案添加部件，并立即应用到当前 Pawn。
	// Adds a character part to the actor that owns this customization component, should be called on the authority only
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Cosmetics)
	void AddCharacterPart(const FLyraCharacterPart& NewPart);

	// 权威端移除等价部件，并从当前 Pawn 销毁对应实例。
	// Removes a previously added character part from the actor that owns this customization component, should be called on the authority only
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Cosmetics)
	void RemoveCharacterPart(const FLyraCharacterPart& PartToRemove);

	// 权威端清除全部角色部件请求及其当前 Pawn 实例。
	// Removes all added character parts, should be called on the authority only
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Cosmetics)
	void RemoveAllCharacterParts();

	// PIE 中根据开发者设置替换或追加测试外观，并处理自然部件抑制。
	// Applies relevant developer settings if in PIE
	void ApplyDeveloperSettings();

protected:
	UPROPERTY(EditAnywhere, Category=Cosmetics)
	TArray<FLyraControllerCharacterPartEntry> CharacterParts;

private:
	ULyraPawnComponent_CharacterParts* GetPawnCustomizer() const;

	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void AddCharacterPartInternal(const FLyraCharacterPart& NewPart, ECharacterPartSource Source);

	void AddCheatPart(const FLyraCharacterPart& NewPart, bool bSuppressNaturalParts);
	void ClearCheatParts();

	void SetSuppressionOnNaturalParts(bool bSuppressed);

	friend class ULyraCosmeticCheats;
};
