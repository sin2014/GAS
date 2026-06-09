// ZYZ

#include "Character/GASEnemy.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"
#include "AbilitySystem/GASAttributeSet.h"
#include "Components/WidgetComponent.h"
#include "GAS/GAS.h"
#include "UI/Widget/GASUserWidget.h"

AGASEnemy::AGASEnemy()
{
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	
	AbilitySystemComponent = CreateDefaultSubobject<UGASAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	
	AttributeSet = CreateDefaultSubobject<UGASAttributeSet>(TEXT("AttributeSet"));
	
	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(GetRootComponent());
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
	
	if (UGASUserWidget* GASUserWidget = Cast<UGASUserWidget>(HealthBar->GetUserWidgetObject()))
	{
		GASUserWidget->SetWidgetController(this);
	}
	
	if (const UGASAttributeSet* GASAS = Cast<UGASAttributeSet>(AttributeSet))
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GASAS->GetHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnHealthChanged.Broadcast(Data.NewValue);
			}	
		);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GASAS->GetMaxHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnMaxHealthChanged.Broadcast(Data.NewValue);
			}	
		);
		
		OnHealthChanged.Broadcast(GASAS->GetHealth());
		OnMaxHealthChanged.Broadcast(GASAS->GetMaxHealth());
	}
}

void AGASEnemy::InitAbilityActorInfo()
{
	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UGASAbilitySystemComponent>(AbilitySystemComponent) -> AbilityActorInfoSet();
	
	InitializeDefaultAttributes();
}
