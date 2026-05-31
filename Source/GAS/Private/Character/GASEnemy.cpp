// ZYZ

#include "Character/GASEnemy.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"
#include "AbilitySystem/GASAttributeSet.h"
#include "GAS/GAS.h"

AGASEnemy::AGASEnemy()
{
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	
	AbilitySystemComponent = CreateDefaultSubobject<UGASAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	
	AttributeSet = CreateDefaultSubobject<UGASAttributeSet>(TEXT("AttributeSet"));
}

void AGASEnemy::HighlightActor()
{
	GetMesh()->SetRenderCustomDepth(true);
	GetMesh()->SetCustomDepthStencilValue(CUSTOM_DEPTH_RED);
	Weapon->SetRenderCustomDepth(true);
	Weapon->SetCustomDepthStencilValue(CUSTOM_DEPTH_RED);
}

void AGASEnemy::UnHighlightActor()
{
	GetMesh()->SetRenderCustomDepth(false);
	Weapon->SetRenderCustomDepth(false);
}

int32 AGASEnemy::GetPlayerLevel()
{
	return Level;
}

void AGASEnemy::BeginPlay()
{
	Super::BeginPlay();
	InitAbilityActorInfo();
}

void AGASEnemy::InitAbilityActorInfo()
{
	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UGASAbilitySystemComponent>(AbilitySystemComponent) -> AbilityActorInfoSet();
}
