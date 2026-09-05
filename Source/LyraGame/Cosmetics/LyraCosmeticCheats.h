// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/CheatManager.h"

#include "LyraCosmeticCheats.generated.h"

class ULyraControllerComponent_CharacterParts;
class UObject;
struct FFrame;

/** 用于临时添加、替换或清除玩家角色部件的外观作弊命令。 */
/** Cheats related to bots */
UCLASS(NotBlueprintable)
class ULyraCosmeticCheats final : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	ULyraCosmeticCheats();

	// 添加一个 CheatManager 来源的角色部件，可选择抑制自然装配。
	// Adds a character part
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void AddCharacterPart(const FString& AssetName, bool bSuppressNaturalParts = true);

	// 清除先前作弊部件后添加指定新部件。
	// Replaces previous cheat parts with a new one
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void ReplaceCharacterPart(const FString& AssetName, bool bSuppressNaturalParts = true);

	// 清除所有作弊外观覆盖并恢复被抑制的自然部件。
	// Clears any existing cheats
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void ClearCharacterPartOverrides();

private:
	ULyraControllerComponent_CharacterParts* GetCosmeticComponent() const;
};
