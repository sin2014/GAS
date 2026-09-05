// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraNumberPopComponent_MeshText.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Feedback/NumberPops/LyraNumberPopComponent.h"
#include "LyraDamagePopStyle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraNumberPopComponent_MeshText)

class UStaticMesh;

// 初始化网格数字材质参数名、显示寿命和组件池状态。
ULyraNumberPopComponent_MeshText::ULyraNumberPopComponent_MeshText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ComponentLifespan = 1.f;

	SignDigitParameterName = FName(TEXT("+Or-"));
	ColorParameterName = FName(TEXT("Color"));
	AnimationLifespanParameterName = FName(TEXT("Animation Lifespan"));
	IsCriticalHitParameterName = FName(TEXT("isCriticalHit?"));
	MoveToCameraParameterName = FName(TEXT("MoveToCamera"));
	PositionParameterNames = { TEXT("0a"), TEXT("1a"), TEXT("2a"), TEXT("3a"), TEXT("4a"),  TEXT("5a"),  TEXT("6a"),  TEXT("7a"),  TEXT("8a") };
	ScaleRotationAngleParameterNames = { TEXT("0b"), TEXT("1b"), TEXT("2b"), TEXT("3b"), TEXT("4b"),  TEXT("5b"),  TEXT("6b"),  TEXT("7b"),  TEXT("8b") };
	DurationParameterNames = { TEXT("0c"), TEXT("1c"), TEXT("2c"), TEXT("3c"), TEXT("4c"),  TEXT("5c"),  TEXT("6c"),  TEXT("7c"),  TEXT("8c") };

	SpacingPercentageForOnes = 0.8f;


	DistanceFromCameraBeforeDoublingSize = 1024.f;
	CriticalHitSizeMultiplier = 1.7f;

	FontXSize = 10.920001f;
	FontYSize = 21.0f;

	NumberOfNumberRotations = 1.f;
}

// 为本地玩家解析数字、选择样式、复用或创建网格组件，并写入材质动画参数。
void ULyraNumberPopComponent_MeshText::AddNumberPop(const FLyraNumberPopRequest& NewRequest)
{
	// 忽略非本地玩家的请求，避免监听服务器主机同时收到服务器与本地路径而重复显示跳字。
	// Drop requests for remote players on the floor
	// (this prevents multiple pops from showing up for the host of a listen server)
	if (APlayerController* PC = GetController<APlayerController>())
	{
		if (!PC->IsLocalController())
		{
			return;
		}
	}

	FTempNumberPopInfo PreparedNumberInfo;

	// 将伤害整数拆分为逐位数字，供材质按槽位显示。
	// Prepare the DamageNumberArray with the digits from the damage.
	{
		int32 LocalDamage = NewRequest.NumberToDisplay;
		PreparedNumberInfo.DamageNumberArray.Empty();

		if (LocalDamage == 0)
		{
			// 数值为零时仍显式加入一个 0 数字。
			// We want to just show a zero
			PreparedNumberInfo.DamageNumberArray.Insert(0, 0);
		}
		else
		{
			// 按十进制从低位到高位拆分数值，并插入到数组头部保持正常显示顺序。
			// Parse the base10 number into an array
			while (LocalDamage > 0)
			{
				PreparedNumberInfo.DamageNumberArray.Insert(LocalDamage % 10, 0);
				LocalDamage /= 10;
			}
		}

		// 在首位预留正负号槽位，具体显示由蓝图和材质参数决定。
		// Insert a zero to reserve space for + or -. Used by the blueprint
		PreparedNumberInfo.DamageNumberArray.Insert(0, 0);
	}

	// 根据匹配样式从组件池取出可复用网格组件；池为空时创建新组件。
	// Grab a component from the pool for this number or create one
	{
		UStaticMesh* MeshToUse = DetermineStaticMesh(NewRequest);
		if (MeshToUse == nullptr)
		{
			return;
		}

		FPooledNumberPopComponentList& ComponentPool = PooledComponentMap.FindOrAdd(MeshToUse);

		UStaticMeshComponent* ComponentToUse = nullptr;
		if (ComponentPool.Components.Num() > 0)
		{
			ComponentToUse = ComponentPool.Components.Pop();
		}
		else
		{
			ComponentToUse = NewObject<UStaticMeshComponent>(GetOwner());
			ComponentToUse->SetupAttachment(nullptr);
			ComponentToUse->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			ComponentToUse->SetStaticMesh(MeshToUse);

			// 写入 CustomDepth，使后处理材质可选择不影响跳字数字。
			// Used to allow post-processes to opt out of affecting the number pop digits
			ComponentToUse->SetRenderCustomDepth(true);
			ComponentToUse->SetCustomDepthStencilValue(123);

			// 材质中的 WPO 动画会让数字远离原始包围盒，因此扩大 Bounds，避免被错误裁剪。
			// The digits travel a great distance from their original bounds due to
			// world position offset (WPO) animation in the material, so expand bounds
			ComponentToUse->SetBoundsScale(2000.0f);

			// 每次跳字都要覆盖颜色和数字等材质参数，因此为所有材质槽创建 MID。
			// We'll be overriding values like the desired color and digits to use, so we need MIDs
			for (int32 MatIdx = 0; MatIdx < ComponentToUse->GetNumMaterials(); ++MatIdx)
			{
				ComponentToUse->CreateDynamicMaterialInstance(MatIdx);
			}
		}

		// 注册组件，使其进入 World 并参与渲染。
		// Register
		check(ComponentToUse);
		ComponentToUse->RegisterComponent();

		// 将组件加入按释放时间排序的活动列表。
		// Add to the "live" list
		UWorld* LocalWorld = GetWorld();
		check(LocalWorld);
		LiveComponents.Emplace(ComponentToUse, &ComponentPool, LocalWorld->GetTimeSeconds() + ComponentLifespan);

		// 将选中的组件和样式写回本次数字的临时数据。
		// Assign struct pointers
		PreparedNumberInfo.StaticMeshComponent = ComponentToUse;
		for (int32 MatIdx = 0; MatIdx < ComponentToUse->GetNumMaterials(); ++MatIdx)
		{
			UMaterialInstanceDynamic* NewMID = Cast<UMaterialInstanceDynamic>(ComponentToUse->GetMaterial(MatIdx));
			PreparedNumberInfo.MeshMIDs.Add(NewMID);
		}

		// 若释放定时器尚未运行，则按最早到期组件的时间启动定时器。
		// Start the timer if it wasn't already running
		if (!LocalWorld->GetTimerManager().IsTimerActive(ReleaseTimerHandle))
		{
			LocalWorld->GetTimerManager().SetTimer(ReleaseTimerHandle, this, &ThisClass::ReleaseNextComponents, ComponentLifespan);
		}
	}

	// 根据相机变换与请求位置计算面向相机的最终数字位置。
	// Determine the position
	FTransform CameraTransform;
	FVector NumberLocation(NewRequest.WorldLocation);
	if (APlayerController* PC = GetController<APlayerController>())
	{
		if (APlayerCameraManager* PlayerCameraManager = PC->PlayerCameraManager)
		{
			CameraTransform = FTransform(PlayerCameraManager->GetCameraRotation(), PlayerCameraManager->GetCameraLocation());

			FVector LocationOffset(ForceInitToZero);

			const float RandomMagnitude = 5.0f; /* TODO：将随机位置偏移幅度改为由 Number Pop 样式配置。 */ //@TODO: Make this style driven
			LocationOffset += FMath::RandPointInBox(FBox(FVector(-RandomMagnitude), FVector(RandomMagnitude)));

			NumberLocation += LocationOffset;
		}
	}
	PreparedNumberInfo.StaticMeshComponent->SetWorldTransform(FTransform(CameraTransform.GetRotation(), NumberLocation));

	// 写入数字、颜色、缩放、旋转和持续时间等材质参数，启动本次表现。
	// Now apply the material parameters to make the digits, etc...
	SetMaterialParameters(NewRequest, PreparedNumberInfo, CameraTransform, NumberLocation);
}

// 按到期时间释放活动数字组件，优先归还对象池，并为下一项重新安排定时器。
void ULyraNumberPopComponent_MeshText::ReleaseNextComponents()
{
	UWorld* LocalWorld = GetWorld();
	check(LocalWorld);

	const float CurrentTime = LocalWorld->GetTimeSeconds();

	int32 NumReleased = 0;
	for (const FLiveNumberPopEntry& LiveComp : LiveComponents)
	{
		if (CurrentTime >= LiveComp.ReleaseTime)
		{
			NumReleased++;
			if (ensure(LiveComp.Component))
			{
				LiveComp.Component->UnregisterComponent();

				if (ensure(LiveComp.Pool))
				{
					// 有关联对象池时重置并归还组件，供后续跳字复用。
					// Return this component to the pool
					LiveComp.Pool->Components.Push(LiveComp.Component);
				}
				else
				{
					// 没有关联对象池时，将该临时组件注销并交由垃圾回收。
					// No pool. Just remove it.
					LiveComp.Component->SetFlags(RF_Transient);
					LiveComp.Component->Rename(nullptr, GetTransientPackage(), RF_NoFlags);
				}
			}
		}
		else
		{
			// 活动列表按时间排序，遇到首个未到期项即可停止检查后续元素。
			// These are in chronological order so none of the other elements will be deleted
			break;
		}
	}

	// 从活动组件数组头部移除本次已释放的条目。
	// Actually remove it from the live components array
	LiveComponents.RemoveAt(0, NumReleased);

	// 若仍有数字在播放，则按下一个条目的到期时间重新安排释放定时器。
	// If we still have live components animating, set the timer to remove the next one
	if (LiveComponents.Num() > 0)
	{
		const float TimeUntilNextRelease = LiveComponents[0].ReleaseTime - CurrentTime;
		LocalWorld->GetTimerManager().SetTimer(ReleaseTimerHandle, this, &ThisClass::ReleaseNextComponents, TimeUntilNextRelease);
	}
}

// 按请求的来源/目标标签和暴击状态选择首个匹配样式颜色。
FLinearColor ULyraNumberPopComponent_MeshText::DetermineColor(const FLyraNumberPopRequest& Request) const
{
	for (ULyraDamagePopStyle* Style : Styles)
	{
		if ((Style != nullptr) && Style->bOverrideColor)
		{
			if (Style->MatchPattern.Matches(Request.TargetTags))
			{
				return Request.bIsCriticalDamage ? Style->CriticalColor : Style->Color;
			}
		}
	}

	return FLinearColor::White;
}

// 按请求标签与暴击状态选择首个匹配样式使用的数字网格。
UStaticMesh* ULyraNumberPopComponent_MeshText::DetermineStaticMesh(const FLyraNumberPopRequest& Request) const
{
	for (ULyraDamagePopStyle* Style : Styles)
	{
		if ((Style != nullptr) && Style->bOverrideMesh)
		{
			if (Style->MatchPattern.Matches(Request.TargetTags))
			{
				return Style->TextMesh;
			}
		}
	}

	return nullptr;
}

// 将数字槽、颜色、持续时间、相机变换与深度策略写入所有 MID，驱动本次网格跳字动画。
void ULyraNumberPopComponent_MeshText::SetMaterialParameters(const FLyraNumberPopRequest& Request, FTempNumberPopInfo& NewDamageNumberInfo, const FTransform& CameraTransform, const FVector& NumberLocation)
{
	UWorld* World = GetWorld();
	if (World && GEngine)
	{
		const float RealGameTime = World->GetRealTimeSeconds();

		// 控制首位是否显示正负号，以及显示减号还是加号；当前默认不显示符号。
		// Whether we should show a sign as the first digit, and if so which one
		// (if bIsSignNegative is true, we show minus, false is plus)
		const bool bShouldShowSign = false;
		const bool bIsSignNegative = true;

		for (UMaterialInstanceDynamic* MeshMID : NewDamageNumberInfo.MeshMIDs)
		{
			MeshMID->SetScalarParameterValue(SignDigitParameterName, bIsSignNegative ? 0.5f : 0.0f);
			MeshMID->SetVectorParameterValue(ColorParameterName, DetermineColor(Request));

			// 数值位数超过材质可支持的槽位数量时，改为显示该位数下可表示的最大值。
			// IF the damage number has more digits than we support
			// THEN force the damage number to the highest number we can support
			const int32 MaxSupportedDigits = FMath::Min(FMath::Min(PositionParameterNames.Num(), ScaleRotationAngleParameterNames.Num()), DurationParameterNames.Num());
			if (!ensure(NewDamageNumberInfo.DamageNumberArray.Num() <= MaxSupportedDigits))
			{
				NewDamageNumberInfo.DamageNumberArray.SetNum(MaxSupportedDigits);

				// 将所有数字槽设为 9；索引 0 是正负号槽位，因此跳过。
				// Set all number digits to 9 so we show the largest number we can
				// Skip digit 0 because that digit is for the +/- sign
				for (int32 DigitIndex = 1; DigitIndex < NewDamageNumberInfo.DamageNumberArray.Num(); ++DigitIndex)
				{
					NewDamageNumberInfo.DamageNumberArray[DigitIndex] = 9;
				}
			}

			MeshMID->SetScalarParameterValue(AnimationLifespanParameterName, ComponentLifespan);
			MeshMID->SetScalarParameterValue(IsCriticalHitParameterName, Request.bIsCriticalDamage ? 1.f : 0.f);

			const int32 DamageNumberArrayLength = NewDamageNumberInfo.DamageNumberArray.Num();
			float OffsetAccumulatedValue = (DamageNumberArrayLength * -1.f) + (bShouldShowSign ? 0.f : -1.f);

			const int32 LastIndex = (DamageNumberArrayLength >= 4) ? DamageNumberArrayLength : 4;

			for (int32 NumberIndex = 0; NumberIndex < LastIndex; ++NumberIndex)
			{
				const float NumberYOffset = ((NumberIndex / FMath::Max(1, DamageNumberArrayLength - 1)) - 0.5f) * 2.f;
				const FVector NumberOffset = FVector(0.f, NumberYOffset, 0.f);
				const FVector CameraSpaceDirection = CameraTransform.TransformVectorNoScale(NumberOffset);

				const float SpacingForNumber = ((NumberIndex < DamageNumberArrayLength) && ((NewDamageNumberInfo.DamageNumberArray[NumberIndex] == 1) || ((NumberIndex > 0) && (NewDamageNumberInfo.DamageNumberArray[NumberIndex - 1] == 1)))) ? SpacingPercentageForOnes : 1.f;
				OffsetAccumulatedValue += SpacingForNumber;

				FLinearColor RGBAPositionParameter(CameraSpaceDirection);
				RGBAPositionParameter.A = OffsetAccumulatedValue;

				const FName PositionParameterName = PositionParameterNames[NumberIndex];
				MeshMID->SetVectorParameterValue(PositionParameterName, RGBAPositionParameter);

				const float DistanceFromCameraToNumber = (CameraTransform.GetLocation() - NumberLocation).Size();
				const float DistanceSpriteScale = DistanceFromCameraBeforeDoublingSize == 0.f ? 1.f : FMath::Clamp(DistanceFromCameraToNumber / DistanceFromCameraBeforeDoublingSize, 1.f, 1000000000.f);

				const float ScaleToZeroMultiplier = (NumberIndex < DamageNumberArrayLength) && (((NumberIndex == 0) && bShouldShowSign) || (NumberIndex != 0)) ? 1.f : 0.f;

				const float HitSizeMultiplier = Request.bIsCriticalDamage ? CriticalHitSizeMultiplier : 1.f;
				const float FontSizeMultiplier = HitSizeMultiplier * DistanceSpriteScale * ScaleToZeroMultiplier;

				FLinearColor RGBAScaleRotationParameter;
				RGBAScaleRotationParameter.R = FontXSize * FontSizeMultiplier;
				RGBAScaleRotationParameter.G = FontYSize * FontSizeMultiplier;
				RGBAScaleRotationParameter.B = NewDamageNumberInfo.DamageNumberArray[FMath::Min(DamageNumberArrayLength - 1, NumberIndex)];
				RGBAScaleRotationParameter.A = FMath::Sign(CameraSpaceDirection.X) * NumberOfNumberRotations;

				const FName ScaleRotationAngleParameterName = ScaleRotationAngleParameterNames[NumberIndex];
				MeshMID->SetVectorParameterValue(ScaleRotationAngleParameterName, RGBAScaleRotationParameter);

				FLinearColor RGBADurationParameter;
				RGBADurationParameter.R = RealGameTime + ComponentLifespan;
				RGBADurationParameter.G = FMath::FRand();

				const FName DurationParameterName = DurationParameterNames[NumberIndex];
				MeshMID->SetVectorParameterValue(DurationParameterName, RGBADurationParameter);
			}

			// 非 Gameplay 的观战相机通常使用更电影化的光圈；若仍把数字拉近相机，会因离开焦平面而严重模糊。
			// 因此在电影化观战相机下应禁用数字向相机方向的深度偏移。
			//@TODO：补充可靠的观战状态判定。
			// Non-gameplay cameras while spectating have more cinematic values of aperture as default.
			// This makes damage numbers very blurry as they are brought close to the camera, and away from the point of focus.
			// Disable the shifting of numbers towards the camera here, if in a cinematic spectator camera.
			//@TODO: Determine whether or not we are spectating
			const bool bIsSpectating = false;
			MeshMID->SetScalarParameterValue(MoveToCameraParameterName, bIsSpectating ? 0.0f : 1.0f);
		}
	}
}

