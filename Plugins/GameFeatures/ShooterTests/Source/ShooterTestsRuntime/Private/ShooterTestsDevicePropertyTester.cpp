// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterTestsDevicePropertyTester.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShooterTestsDevicePropertyTester)

// 定义设备属性测试 Actor 的文件内错误日志分类。
DEFINE_LOG_CATEGORY_STATIC(LogShooterTestDeviceProperty, Log, All);

// 创建触发胶囊和可视平台网格，并绑定重叠开始/结束事件。
AShooterTestsDevicePropertyTester::AShooterTestsDevicePropertyTester()
{
	RootComponent = CollisionVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionVolume"));
	CollisionVolume->InitCapsuleSize(80.f, 80.f);
	CollisionVolume->OnComponentBeginOverlap.AddDynamic(this, &AShooterTestsDevicePropertyTester::OnOverlapBegin);
	CollisionVolume->OnComponentEndOverlap.AddDynamic(this, &AShooterTestsDevicePropertyTester::OnEndOverlap);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(RootComponent);
}

// Pawn 进入触发体时使用其 PlatformUserId 激活配置的设备属性；非 Pawn Actor 被忽略。
void AShooterTestsDevicePropertyTester::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		ApplyDeviceProperties(Pawn->GetPlatformUserId());
	}
}

// Pawn 离开触发体时移除当前测试 Actor 激活的全部设备属性。
void AShooterTestsDevicePropertyTester::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (const APawn* Character = Cast<APawn>(OtherActor))
	{
		RemoveDeviceProperties();
	}
}

// 为有效 PlatformUser 激活每个配置的设备属性，并保存返回句柄供重叠结束时统一移除；无效用户记录错误后返回。
void AShooterTestsDevicePropertyTester::ApplyDeviceProperties(const FPlatformUserId UserId)
{
	if (!UserId.IsValid())
	{
		UE_LOG(LogShooterTestDeviceProperty, Error, TEXT("Cannot apply device properties to an invalid Platform User!"));
		return;
	}

	if (UInputDeviceSubsystem* System = UInputDeviceSubsystem::Get())
	{
		FActivateDevicePropertyParams Params = {};
		Params.UserId = UserId;
		
		for (TSubclassOf<UInputDeviceProperty> DevicePropClass : DeviceProperties)
		{
			ActivePropertyHandles.Emplace(System->ActivateDevicePropertyOfClass(DevicePropClass, Params));			
		}
	}
}

// 通过 InputDeviceSubsystem 移除所有已激活句柄并清空本地集合；子系统不可用时仍清理句柄记录。
void AShooterTestsDevicePropertyTester::RemoveDeviceProperties()
{
	// 移除先前由此测试 Actor 激活的全部设备属性。
	// Remove any device properties that have been applied
	if (UInputDeviceSubsystem* InputDeviceSubsystem = UInputDeviceSubsystem::Get())
	{
		InputDeviceSubsystem->RemoveDevicePropertyHandles(ActivePropertyHandles);
	}
	
	ActivePropertyHandles.Empty();
}
