// ZYZ

#include "Character/GASEnemy.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"
#include "AbilitySystem/GASAbilitySystemLibrary.h"
#include "AbilitySystem/GASAttributeSet.h"
#include "Components/WidgetComponent.h"
#include "GAS/GAS.h"
#include "UI/Widget/GASUserWidget.h"
#include "GASGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

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
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	InitAbilityActorInfo();
	UGASAbilitySystemLibrary::GiveStartupAbilities(this, AbilitySystemComponent);
	
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
		
		AbilitySystemComponent->RegisterGameplayTagEvent(FGASGameplayTags::Get().Effects_HitReact, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&AGASEnemy::HitReactTagChanged
		);
		
		OnHealthChanged.Broadcast(GASAS->GetHealth());
		OnMaxHealthChanged.Broadcast(GASAS->GetMaxHealth());
	}
}

void AGASEnemy::HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bHitReacting = (NewCount > 0);
	GetCharacterMovement()->MaxWalkSpeed = bHitReacting ? 0.f : BaseWalkSpeed;
}

void AGASEnemy::InitAbilityActorInfo()
{
	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UGASAbilitySystemComponent>(AbilitySystemComponent) -> AbilityActorInfoSet();
	
	InitializeDefaultAttributes();
}

void AGASEnemy::InitializeDefaultAttributes() const
{
	UGASAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, AbilitySystemComponent);
}
