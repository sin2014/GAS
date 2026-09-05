// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraCharacterWithAbilities.h"

#include "AbilitySystem/Attributes/LyraCombatSet.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Async/TaskGraphInterfaces.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCharacterWithAbilities)

// 创建角色自持有的 ASC、HealthSet 和 CombatSet，并提高网络更新频率以复制技能状态。
ALyraCharacterWithAbilities::ALyraCharacterWithAbilities(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<ULyraAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// ASC 会在 InitializeComponent 时发现这些 AttributeSet；成员引用确保它们在此之前不会被垃圾回收。
	// These attribute sets will be detected by AbilitySystemComponent::InitializeComponent. Keeping a reference so that the sets don't get garbage collected before that.
	HealthSet = CreateDefaultSubobject<ULyraHealthSet>(TEXT("HealthSet"));
	CombatSet = CreateDefaultSubobject<ULyraCombatSet>(TEXT("CombatSet"));

	// ASC 依赖较高频率的网络更新，因此提高该角色的 NetUpdateFrequency。
	// AbilitySystemComponent needs to be updated at a high frequency.
	SetNetUpdateFrequency(100.0f);
}

// 在组件初始化完成后连接 ALyraCharacterWithAbilities 的运行时依赖。
void ALyraCharacterWithAbilities::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

// 返回该角色自身创建并持有的 LyraAbilitySystemComponent。
UAbilitySystemComponent* ALyraCharacterWithAbilities::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

