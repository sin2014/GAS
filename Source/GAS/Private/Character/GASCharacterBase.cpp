// ZYZ

#include "Character/GASCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "GASGameplayTags.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GAS/GAS.h"
#include "Kismet/GameplayStatics.h"

AGASCharacterBase::AGASCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	GetMesh()->SetGenerateOverlapEvents(true);

	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), FName("WeaponHandSocket"));
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UAbilitySystemComponent* AGASCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UAttributeSet* AGASCharacterBase::GetAttributeSet() const
{
	return AttributeSet;
}

UAnimMontage* AGASCharacterBase::GetHitReactMontage_Implementation()
{
	return HitReactMontage;
}

void AGASCharacterBase::Die()
{
	Weapon->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
	MuticastHandleDeath();
}

void AGASCharacterBase::MuticastHandleDeath_Implementation()
{
	UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation(), GetActorRotation());
	
	Weapon->SetSimulatePhysics(true);
	Weapon->SetEnableGravity(true);
	Weapon->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetEnableGravity(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dissolve();
	bDead = true;
}

void AGASCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

FVector AGASCharacterBase::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)
{
	const FGASGameplayTags& GameplayTags = FGASGameplayTags::Get();
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_Weapon) && IsValid(Weapon))
	{
		return Weapon->GetSocketLocation(WeaponTipSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_LeftHand))
	{
		return GetMesh()->GetSocketLocation(LeftHandSocketName);
	}	
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_RightHand))
	{
		return GetMesh()->GetSocketLocation(RightHandSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_LeftFoot))
	{
		return GetMesh()->GetSocketLocation(LeftFootSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_RightFoot))
	{
		return GetMesh()->GetSocketLocation(RightFootSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_Tail))
	{
		return GetMesh()->GetSocketLocation(TailSocketName);
	}
	
	return FVector();
}

TArray<FTaggedMontage> AGASCharacterBase::GetAttackMontages_Implementation()
{
	return AttackMontages;
}

FTaggedMontage AGASCharacterBase::GetTaggedMontageByTag_Implementation(const FGameplayTag& MontageTag)
{
	for (const FTaggedMontage& TaggedMontage : AttackMontages)
	{
		if (TaggedMontage.MontageTag.MatchesTagExact(MontageTag))
		{
			return TaggedMontage;
		}
	}
	return FTaggedMontage();
}

UNiagaraSystem* AGASCharacterBase::GetBloodEffect_Implementation()
{
	return BloodEffect;
}

AActor* AGASCharacterBase::GetAvatar_Implementation()
{
	return this;
}

bool AGASCharacterBase::IsDead_Implementation() const
{
	return bDead;
}

void AGASCharacterBase::InitAbilityActorInfo()
{
}

void AGASCharacterBase::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const
{
	check(IsValid(GetAbilitySystemComponent()));
	check(GameplayEffectClass);
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponent()->MakeOutgoingSpec(GameplayEffectClass, Level, ContextHandle);
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), GetAbilitySystemComponent());
}

void AGASCharacterBase::InitializeDefaultAttributes() const
{
	ApplyEffectToSelf(DefaultPrimaryAttributes, 1.0f);
	ApplyEffectToSelf(DefaultSecondaryAttributes, 1.0f);
	ApplyEffectToSelf(DefaultVitalAttributes, 1.0f);
}

void AGASCharacterBase::AddCharacterAbilities()
{
	UGASAbilitySystemComponent* GASASC = CastChecked<UGASAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!HasAuthority()) return;
	GASASC->AddCharacterAbilities(StartupAbilities);
}

void AGASCharacterBase::Dissolve()
{
	if (IsValid(DissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicMatInst);  // 0 is the first material，多个材质的话，需要为每个材质都创建动态材质实例
		StartDissolveTimeline(DynamicMatInst);
	}
	if (IsValid(WeaponDissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(WeaponDissolveMaterialInstance, this);
		Weapon->SetMaterial(0, DynamicMatInst);  // 0 is the first material，多个材质的话，需要为每个材质都创建动态材质实例
		StartWeaponDissolveTimeline(DynamicMatInst);
	}
}
