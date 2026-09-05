// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cosmetics/LyraControllerComponent_CharacterParts.h"
#include "Cosmetics/LyraCharacterPartTypes.h"
#include "Cosmetics/LyraPawnComponent_CharacterParts.h"
#include "GameFramework/CheatManagerDefines.h"
#include "LyraCosmeticDeveloperSettings.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraControllerComponent_CharacterParts)

//////////////////////////////////////////////////////////////////////

// 构造 Controller 外观组件，关闭复制与 Tick，并初始化空角色部件请求列表。
ULyraControllerComponent_CharacterParts::ULyraControllerComponent_CharacterParts(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 仅权威端监听 Controller 的 PossessedPawnChanged，并立即把已有部件应用到当前 Pawn。
void ULyraControllerComponent_CharacterParts::BeginPlay()
{
	Super::BeginPlay();

	// 仅权威端监听 Controller 的 Pawn 变化，以迁移可复制的装配条目。
	// Listen for pawn possession changed events
	if (HasAuthority())
	{
		if (AController* OwningController = GetController<AController>())
		{
			OwningController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);

			if (APawn* ControlledPawn = GetPawn<APawn>())
			{
				OnPossessedPawnChanged(nullptr, ControlledPawn);
			}
		}

		ApplyDeveloperSettings();
	}
}

// 权威端解除 Pawn 变化委托，并从当前 Pawn 移除已应用部件后结束组件。
void ULyraControllerComponent_CharacterParts::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllCharacterParts();
	Super::EndPlay(EndPlayReason);
}

// 从所属 Controller 当前 Pawn 查找 PawnComponent_CharacterParts；没有 Pawn 或组件时返回 nullptr。
ULyraPawnComponent_CharacterParts* ULyraControllerComponent_CharacterParts::GetPawnCustomizer() const
{
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		return ControlledPawn->FindComponentByClass<ULyraPawnComponent_CharacterParts>();
	}
	return nullptr;
}

// 以 Natural 来源记录部件，并应用到当前 Pawn。
void ULyraControllerComponent_CharacterParts::AddCharacterPart(const FLyraCharacterPart& NewPart)
{
	AddCharacterPartInternal(NewPart, ECharacterPartSource::Natural);
}

// 追加带来源的 Controller 条目；若当前 Pawn 可定制且该来源未被抑制，则生成部件并保存句柄。
void ULyraControllerComponent_CharacterParts::AddCharacterPartInternal(const FLyraCharacterPart& NewPart, ECharacterPartSource Source)
{
	FLyraControllerCharacterPartEntry& NewEntry = CharacterParts.AddDefaulted_GetRef();
	NewEntry.Part = NewPart;
	NewEntry.Source = Source;

	if (ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
	{
		if (NewEntry.Source != ECharacterPartSource::NaturalSuppressedViaCheat)
		{
			NewEntry.Handle = PawnCustomizer->AddCharacterPart(NewPart);
		}
	}

}

// 移除第一个与部件类和 Socket 等价的 Controller 条目，并从当前 Pawn 删除其有效句柄。
void ULyraControllerComponent_CharacterParts::RemoveCharacterPart(const FLyraCharacterPart& PartToRemove)
{
	for (auto EntryIt = CharacterParts.CreateIterator(); EntryIt; ++EntryIt)
	{
		if (FLyraCharacterPart::AreEquivalentParts(EntryIt->Part, PartToRemove))
		{
			if (ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
			{
				PawnCustomizer->RemoveCharacterPart(EntryIt->Handle);
			}

			EntryIt.RemoveCurrent();
			break;
		}
	}
}

// 从当前 Pawn 移除全部有效句柄，并清空 Controller 持有的装配请求。
void ULyraControllerComponent_CharacterParts::RemoveAllCharacterParts()
{
	if (ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer())
	{
		for (FLyraControllerCharacterPartEntry& Entry : CharacterParts)
		{
			PawnCustomizer->RemoveCharacterPart(Entry.Handle);
		}
	}

	CharacterParts.Reset();
}

// 从旧 Pawn 撤下所有句柄，再把未抑制且尚未应用的条目装配到新 Pawn。
void ULyraControllerComponent_CharacterParts::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	// 从旧 Pawn 移除已应用部件，并使本地句柄失效。
	// Remove from the old pawn
	if (ULyraPawnComponent_CharacterParts* OldCustomizer = OldPawn ? OldPawn->FindComponentByClass<ULyraPawnComponent_CharacterParts>() : nullptr)
	{
		for (FLyraControllerCharacterPartEntry& Entry : CharacterParts)
		{
			OldCustomizer->RemoveCharacterPart(Entry.Handle);
			Entry.Handle.Reset();
		}
	}

	// 将所有未被抑制的条目应用到新 Pawn。
	// Apply to the new pawn
	if (ULyraPawnComponent_CharacterParts* NewCustomizer = NewPawn ? NewPawn->FindComponentByClass<ULyraPawnComponent_CharacterParts>() : nullptr)
	{
		for (FLyraControllerCharacterPartEntry& Entry : CharacterParts)
		{
			// 旧 Pawn 可能为空，因此仅对尚无有效句柄的条目重新添加，避免重复装配。
			// Don't readd if it's already there, this can get called with a null oldpawn
			if (!Entry.Handle.IsValid() && Entry.Source != ECharacterPartSource::NaturalSuppressedViaCheat)
			{
				Entry.Handle = NewCustomizer->AddCharacterPart(Entry.Part);
			}
		}
	}
}

// 按 ReplaceParts 或 AddParts 设置抑制自然部件，移除旧开发者条目并重新添加当前配置。
void ULyraControllerComponent_CharacterParts::ApplyDeveloperSettings()
{
#if UE_WITH_CHEAT_MANAGER
	const ULyraCosmeticDeveloperSettings* Settings = GetDefault<ULyraCosmeticDeveloperSettings>();

	// ReplaceParts 模式且配置了作弊部件时抑制自然部件，否则恢复自然部件。
	// Suppress or unsuppress natural parts if needed
	const bool bSuppressNaturalParts = (Settings->CheatMode == ECosmeticCheatMode::ReplaceParts) && (Settings->CheatCosmeticCharacterParts.Num() > 0);
	SetSuppressionOnNaturalParts(bSuppressNaturalParts);

	// 先移除旧开发者设置条目，再按当前设置重新应用，保证 PIE 热更新一致。
	// Remove anything added by developer settings and re-add it
	ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer();
	for (auto It = CharacterParts.CreateIterator(); It; ++It)
	{
		if (It->Source == ECharacterPartSource::AppliedViaDeveloperSettingsCheat)
		{
			if (PawnCustomizer != nullptr)
			{
				PawnCustomizer->RemoveCharacterPart(It->Handle);
			}
			It.RemoveCurrent();
		}
	}

	// 添加当前开发者设置中配置的新部件。
	// Add new parts
	for (const FLyraCharacterPart& PartDesc : Settings->CheatCosmeticCharacterParts)
	{
		AddCharacterPartInternal(PartDesc, ECharacterPartSource::AppliedViaDeveloperSettingsCheat);
	}
#endif
}


// 添加 CheatManager 来源部件，并按参数决定是否同时抑制自然部件。
void ULyraControllerComponent_CharacterParts::AddCheatPart(const FLyraCharacterPart& NewPart, bool bSuppressNaturalParts)
{
#if UE_WITH_CHEAT_MANAGER
	SetSuppressionOnNaturalParts(bSuppressNaturalParts);
	AddCharacterPartInternal(NewPart, ECharacterPartSource::AppliedViaCheatManager);
#endif
}

// 移除全部 CheatManager 来源条目，再解除自然部件抑制。
void ULyraControllerComponent_CharacterParts::ClearCheatParts()
{
#if UE_WITH_CHEAT_MANAGER
	ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer();

	// 清除由 CheatManager 临时添加的所有部件。
	// Remove anything added by cheat manager cheats
	for (auto It = CharacterParts.CreateIterator(); It; ++It)
	{
		if (It->Source == ECharacterPartSource::AppliedViaCheatManager)
		{
			if (PawnCustomizer != nullptr)
			{
				PawnCustomizer->RemoveCharacterPart(It->Handle);
			}
			It.RemoveCurrent();
		}
	}

	ApplyDeveloperSettings();
#endif
}

// 切换自然部件条目的抑制来源；抑制时移除 Pawn 句柄，解除时重新生成。
void ULyraControllerComponent_CharacterParts::SetSuppressionOnNaturalParts(bool bSuppressed)
{
#if UE_WITH_CHEAT_MANAGER
	ULyraPawnComponent_CharacterParts* PawnCustomizer = GetPawnCustomizer();

	for (FLyraControllerCharacterPartEntry& Entry : CharacterParts)
	{
		if ((Entry.Source == ECharacterPartSource::Natural) && bSuppressed)
		{
			// 抑制自然部件时，从当前 Pawn 移除其已生成实例，但保留 Controller 条目。
			// Suppress
			if (PawnCustomizer != nullptr)
			{
				PawnCustomizer->RemoveCharacterPart(Entry.Handle);
				Entry.Handle.Reset();
			}
			Entry.Source = ECharacterPartSource::NaturalSuppressedViaCheat;
		}
		else if ((Entry.Source == ECharacterPartSource::NaturalSuppressedViaCheat) && !bSuppressed)
		{
			// 解除抑制时，将自然部件重新应用到当前 Pawn。
			// Unsuppress
			if (PawnCustomizer != nullptr)
			{
				Entry.Handle = PawnCustomizer->AddCharacterPart(Entry.Part);
			}
			Entry.Source = ECharacterPartSource::Natural;
		}
	}
#endif
}

