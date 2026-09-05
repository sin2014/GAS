// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraCosmeticCheats.h"
#include "Cosmetics/LyraCharacterPartTypes.h"
#include "LyraControllerComponent_CharacterParts.h"
#include "GameFramework/CheatManagerDefines.h"
#include "System/LyraDevelopmentStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCosmeticCheats)

//////////////////////////////////////////////////////////////////////
// ULyraCosmeticCheats

// 构造外观作弊扩展，不持有独立运行时状态。
ULyraCosmeticCheats::ULyraCosmeticCheats()
{
#if UE_WITH_CHEAT_MANAGER
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda(
			[](UCheatManager* CheatManager)
			{
				CheatManager->AddCheatManagerExtension(NewObject<ThisClass>(CheatManager));
			}));
	}
#endif
}

// 按资产名同步加载角色部件类，并通过 Controller 外观组件添加 CheatManager 来源部件；加载失败时输出错误。
void ULyraCosmeticCheats::AddCharacterPart(const FString& AssetName, bool bSuppressNaturalParts)
{
#if UE_WITH_CHEAT_MANAGER
	if (ULyraControllerComponent_CharacterParts* CosmeticComponent = GetCosmeticComponent())
	{
		TSubclassOf<AActor> PartClass = ULyraDevelopmentStatics::FindClassByShortName<AActor>(AssetName);
		if (PartClass != nullptr)
		{
			FLyraCharacterPart Part;
			Part.PartClass = PartClass;

			CosmeticComponent->AddCheatPart(Part, bSuppressNaturalParts);
		}
	}
#endif	
}

// 清除已有作弊部件后添加指定新部件，实现单项替换。
void ULyraCosmeticCheats::ReplaceCharacterPart(const FString& AssetName, bool bSuppressNaturalParts)
{
	ClearCharacterPartOverrides();
	AddCharacterPart(AssetName, bSuppressNaturalParts);
}

// 清除 Controller 外观组件上的全部 CheatManager 部件和自然部件抑制。
void ULyraCosmeticCheats::ClearCharacterPartOverrides()
{
#if UE_WITH_CHEAT_MANAGER
	if (ULyraControllerComponent_CharacterParts* CosmeticComponent = GetCosmeticComponent())
	{
		CosmeticComponent->ClearCheatParts();
	}
#endif	
}

// 从所属 PlayerController 查找 ControllerComponent_CharacterParts；没有 Controller 或组件时返回 nullptr。
ULyraControllerComponent_CharacterParts* ULyraCosmeticCheats::GetCosmeticComponent() const
{
	if (APlayerController* PC = GetPlayerController())
	{
		return PC->FindComponentByClass<ULyraControllerComponent_CharacterParts>();
	}

	return nullptr;
}

