// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MMCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Character/MMCharacterAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

AMMCharacterBase::AMMCharacterBase()
{
	// 当前角色移动由 PlayerController 轮询输入驱动，角色自身不需要 Actor Tick。
	PrimaryActorTick.bCanEverTick = false;

	// 使用角色胶囊作为移动碰撞体。
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);

	// 创建 GAS 能力系统组件。
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// 创建人物属性集合。
	CharacterAttributeSet = CreateDefaultSubobject<UMMCharacterAttributeSet>(TEXT("CharacterAttributeSet"));

	// 创建临时可见模型，让默认地图里能看到一个角色占位体。
	TemporaryVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TemporaryVisualMesh"));
	TemporaryVisualMesh->SetupAttachment(GetRootComponent());
	TemporaryVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TemporaryVisualMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.6f));

	// 使用引擎内置圆柱体作为临时人物模型，不引入项目内容资产。
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		TemporaryVisualMesh->SetStaticMesh(CylinderMesh.Object);
	}

	// 创建固定俯视相机臂。
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->SetRelativeRotation(FRotator(-45.0f, 0.0f, 0.0f));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;

	// 创建固定俯视相机。
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 四向移动不依赖控制器朝向旋转。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 设置基础行走参数，保持移动反馈直接。
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2048.0f;
}

UAbilitySystemComponent* AMMCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UMMCharacterAttributeSet* AMMCharacterBase::GetCharacterAttributeSet() const
{
	return CharacterAttributeSet;
}

void AMMCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		// 初始化 GAS ActorInfo，让 ASC 认识该角色的 Owner 和 Avatar。
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	if (GEngine && AbilitySystemComponent && CharacterAttributeSet)
	{
		// 输出一次屏幕信息，确认运行时角色已经持有 ASC 和 AttributeSet。
		const FString DebugMessage = FString::Printf(
			TEXT("MMM GAS角色已初始化  Level=%.0f  HP=%.0f/%.0f"),
			CharacterAttributeSet->GetLevel(),
			CharacterAttributeSet->GetCurrentHP(),
			CharacterAttributeSet->GetMaxHP());

		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, DebugMessage);
		UE_LOG(LogTemp, Display, TEXT("%s"), *DebugMessage);
	}
}
