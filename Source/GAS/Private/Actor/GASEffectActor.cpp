// ZYZ

#include "Actor/GASEffectActor.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/GASAttributeSet.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

AGASEffectActor::AGASEffectActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	SetRootComponent(Mesh);

	Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
	Sphere->SetupAttachment(GetRootComponent());
}

void AGASEffectActor::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//TODO:Change this to apply a Gameplay Effect. For now, using const_cast as a hack!
	IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(OtherActor);
	if (ASCInterface)
	{
		const UGASAttributeSet* GASAttributeSet = Cast<UGASAttributeSet>(ASCInterface->GetAbilitySystemComponent()->GetAttributeSet(UGASAttributeSet::StaticClass()));
		
		UGASAttributeSet* MutableGASAttributeSet = const_cast<UGASAttributeSet*>(GASAttributeSet);
		MutableGASAttributeSet->SetHealth(GASAttributeSet->GetHealth() + 10.f);
		MutableGASAttributeSet->SetMana(GASAttributeSet->GetMana() + 5.f);
		Destroy();
	}
}

void AGASEffectActor::EndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
}

void AGASEffectActor::BeginPlay()
{
	Super::BeginPlay();
	
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AGASEffectActor::OnOverlap);
	Sphere->OnComponentEndOverlap.AddDynamic(this, &AGASEffectActor::EndOverlap);
}
