// ZYZ

#include "Components/FoliageOcclusionFadeComponent.h"

#include "DrawDebugHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "FoliageInstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "InstancedFoliageActor.h"
#include "Materials/MaterialInstanceDynamic.h"

UFoliageOcclusionFadeComponent::UFoliageOcclusionFadeComponent()
{
	// 扫描只按 ScanInterval 执行，但淡入淡出插值需要每帧 Tick 才能保持平滑。
	PrimaryComponentTick.bCanEverTick = true;
}

void UFoliageOcclusionFadeComponent::BeginPlay()
{
	Super::BeginPlay();

	// 相机遮挡只影响本机画面。服务器和其他客户端不需要重复执行查询或修改渲染数据。
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		SetComponentTickEnabled(false);
		return;
	}

	InitializeFoliageComponents();
	InitializeStandaloneStaticMeshes();
}

void UFoliageOcclusionFadeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 在世界切换、PIE 停止或控制器销毁前，把所有实例恢复为完全显示。
	RestoreAllInstances();
	Super::EndPlay(EndPlayReason);
}

void UFoliageOcclusionFadeComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 空间查询不需要每帧执行。50 毫秒一次通常已经能获得及时且稳定的响应。
	TimeUntilNextScan -= DeltaTime;
	if (TimeUntilNextScan <= 0.f)
	{
		ScanForOccludingInstances();
		TimeUntilNextScan = FMath::Max(ScanInterval, 0.01f);
	}

	// 插值和材质数据写入每帧执行，避免出现阶梯式跳变。
	UpdateFades(DeltaTime);
	UpdateStandaloneMeshFades(DeltaTime);
}

void UFoliageOcclusionFadeComponent::InitializeFoliageComponents()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// SetCustomDataValue 要求组件预先分配足够的 Per Instance Custom Data 槽位。
	const int32 RequiredCustomDataFloats = CustomDataIndex + 1;

	// Foliage Mode 绘制的实例都由 AInstancedFoliageActor 管理。
	for (TActorIterator<AInstancedFoliageActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TInlineComponentArray<UFoliageInstancedStaticMeshComponent*> Components(*ActorIt);
		for (UFoliageInstancedStaticMeshComponent* Component : Components)
		{
			if (!IsValid(Component) || !IsSupportedTreeComponent(Component))
			{
				continue;
			}

			// 扩展数据槽会把该组件全部实例的自定义数据重置为 0，即默认完全显示。
			if (Component->NumCustomDataFloats < RequiredCustomDataFloats)
			{
				Component->SetNumCustomDataFloats(RequiredCustomDataFloats);
			}

			InitializedComponents.Add(Component);
		}
	}
}

void UFoliageOcclusionFadeComponent::InitializeStandaloneStaticMeshes()
{
	if (!bHandleStandaloneStaticMeshes)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 遍历关卡 Actor 的普通 StaticMeshComponent。
	// InstancedStaticMesh/HISM 由 Foliage 路径处理，不能在这里重复创建 MID。
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents(*ActorIt);
		for (UStaticMeshComponent* Component : StaticMeshComponents)
		{
			if (!IsValid(Component)
				|| Cast<UInstancedStaticMeshComponent>(Component)
				|| !IsSupportedTreeMesh(Component->GetStaticMesh()))
			{
				continue;
			}

			FStandaloneMeshMaterials& Materials = StandaloneMeshMaterials.FindOrAdd(Component);
			if (!Materials.DynamicMaterials.IsEmpty())
			{
				continue;
			}

			const int32 MaterialCount = Component->GetNumMaterials();
			Materials.OriginalMaterials.Reserve(MaterialCount);
			Materials.DynamicMaterials.Reserve(MaterialCount);

			for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				Materials.OriginalMaterials.Add(Component->GetMaterial(MaterialIndex));

				UMaterialInstanceDynamic* DynamicMaterial = Component->CreateDynamicMaterialInstance(MaterialIndex);
				if (DynamicMaterial)
				{
					// 普通网格以 1 为初始值，保持完全显示。
					DynamicMaterial->SetScalarParameterValue(StandaloneFadeParameterName, 1.f);
				}
				Materials.DynamicMaterials.Add(DynamicMaterial);
			}
		}
	}
}

void UFoliageOcclusionFadeComponent::ScanForOccludingInstances()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!PlayerController || !Pawn || !World)
	{
		return;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	// 检测终点使用角色上半身，而不是脚底的 Actor 原点。
	const FVector CharacterFocusLocation = Pawn->GetActorLocation() + FVector::UpVector * CharacterFocusHeight;
	const FVector CameraToCharacter = CharacterFocusLocation - CameraLocation;
	const float CameraToCharacterDistance = CameraToCharacter.Size();
	if (CameraToCharacterDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 盒体 X 轴始终准确指向角色焦点，不依赖控制器旋转是否与实际相机位置完全一致。
	const FVector DetectionDirection = CameraToCharacter / CameraToCharacterDistance;
	const FQuat DetectionRotation = FRotationMatrix::MakeFromX(DetectionDirection).ToQuat();

	// Length 是用户允许的最大长度。默认开启钳制后，盒体远端不会越过角色。
	const float AvailableLengthToCharacter = FMath::Max(
		CameraToCharacterDistance - DetectionBoxDistanceFromCamera - CharacterEndPadding,
		0.f);
	const float EffectiveLength = bClampDetectionBoxToCharacter
		? FMath::Min(DetectionBoxLength, AvailableLengthToCharacter)
		: DetectionBoxLength;

	if (EffectiveLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector DetectionHalfExtent(
		EffectiveLength * 0.5f,
		FMath::Max(DetectionBoxWidth * 0.5f, 0.5f),
		FMath::Max(DetectionBoxHeight * 0.5f, 0.5f));

	// DistanceFromCamera 表示盒体近端的位置，因此中心还需要前进半个有效长度。
	const FVector DetectionCenter = CameraLocation + DetectionDirection *
		(DetectionBoxDistanceFromCamera + EffectiveLength * 0.5f);
	const FTransform DetectionBoxTransform(DetectionRotation, DetectionCenter);
	const FBox LocalDetectionBox(-DetectionHalfExtent, DetectionHalfExtent);

	// HISM 的快速查询只接受世界轴对齐盒，因此先计算有向检测盒在世界空间中的 AABB。
	const FBox WorldQueryBox = LocalDetectionBox.TransformBy(DetectionBoxTransform);
	const float CurrentTime = World->GetTimeSeconds();
	TSet<FFoliageInstanceKey> SeenThisScan;

	for (const TWeakObjectPtr<UFoliageInstancedStaticMeshComponent>& WeakComponent : InitializedComponents)
	{
		UFoliageInstancedStaticMeshComponent* Component = WeakComponent.Get();
		if (!IsValid(Component))
		{
			continue;
		}

		// 该查询使用 HISM 的实例包围盒层级，不要求网格拥有简单碰撞。
		// 这正是小树视觉上挡住角色但物理 Sweep 仍为绿色时需要的检测路径。
		const TArray<int32> CandidateIndices = Component->GetInstancesOverlappingBox(WorldQueryBox, true);
		for (const int32 InstanceIndex : CandidateIndices)
		{
			if (InstanceIndex < 0 || InstanceIndex >= Component->GetInstanceCount())
			{
				continue;
			}

			// 世界 AABB 会比旋转后的真实盒体更大，因此还要做一次局部空间精确过滤。
			if (!DoesInstanceOverlapDetectionBox(Component, InstanceIndex, DetectionBoxTransform, LocalDetectionBox))
			{
				continue;
			}

			const FFoliageInstanceKey Key{Component, InstanceIndex};
			if (SeenThisScan.Contains(Key))
			{
				continue;
			}

			SeenThisScan.Add(Key);
			FFoliageFadeState& State = ActiveFades.FindOrAdd(Key);
			State.TargetHiddenAmount = 1.f;
			State.LastSeenTime = CurrentTime;
		}
	}

	// 普通 StaticMeshComponent 没有 InstanceIndex，直接用组件指针作为唯一键。
	TSet<TWeakObjectPtr<UStaticMeshComponent>> SeenStandaloneMeshesThisScan;
	if (bHandleStandaloneStaticMeshes)
	{
		for (const TPair<TWeakObjectPtr<UStaticMeshComponent>, FStandaloneMeshMaterials>& Pair : StandaloneMeshMaterials)
		{
			UStaticMeshComponent* Component = Pair.Key.Get();
			if (!IsValid(Component)
				|| !DoesStandaloneMeshOverlapDetectionBox(Component, DetectionBoxTransform, LocalDetectionBox))
			{
				continue;
			}

			SeenStandaloneMeshesThisScan.Add(Component);
			FFoliageFadeState& State = ActiveStandaloneMeshFades.FindOrAdd(Component);
			State.TargetHiddenAmount = 1.f;
			State.LastSeenTime = CurrentTime;
		}
	}

	// 本次没有再次看到的实例不会立即恢复，而是等待 RestoreDelay，避免盒体边缘抖动。
	for (TPair<FFoliageInstanceKey, FFoliageFadeState>& Pair : ActiveFades)
	{
		if (!SeenThisScan.Contains(Pair.Key) && CurrentTime - Pair.Value.LastSeenTime >= RestoreDelay)
		{
			Pair.Value.TargetHiddenAmount = 0.f;
		}
	}

	for (TPair<TWeakObjectPtr<UStaticMeshComponent>, FFoliageFadeState>& Pair : ActiveStandaloneMeshFades)
	{
		if (!SeenStandaloneMeshesThisScan.Contains(Pair.Key)
			&& CurrentTime - Pair.Value.LastSeenTime >= RestoreDelay)
		{
			Pair.Value.TargetHiddenAmount = 0.f;
		}
	}

	if (bDrawDebugTrace)
	{
		// 绿色：盒内没有目标树实例。红色：至少有一个实例正在被判定为遮挡物。
		DrawDebugBox(
			World,
			DetectionCenter,
			DetectionHalfExtent,
			DetectionRotation,
			SeenThisScan.IsEmpty() && SeenStandaloneMeshesThisScan.IsEmpty() ? FColor::Green : FColor::Red,
			false,
			ScanInterval,
			0,
			2.f);
	}
}

bool UFoliageOcclusionFadeComponent::DoesInstanceOverlapDetectionBox(
	const UFoliageInstancedStaticMeshComponent* Component,
	int32 InstanceIndex,
	const FTransform& DetectionBoxTransform,
	const FBox& LocalDetectionBox) const
{
	if (!Component || !Component->GetStaticMesh())
	{
		return false;
	}

	FTransform InstanceWorldTransform;
	if (!Component->GetInstanceTransform(InstanceIndex, InstanceWorldTransform, true))
	{
		return false;
	}

	// 将静态网格局部包围盒的 8 个角转换到检测盒局部空间。
	// 得到的局部 AABB 与检测盒相交时，说明这棵树的渲染范围进入了相机遮挡区域。
	const FBox MeshLocalBounds = Component->GetStaticMesh()->GetBounds().GetBox();
	FBox InstanceBoundsInDetectionSpace(EForceInit::ForceInit);
	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector MeshLocalCorner(
			(CornerIndex & 1) ? MeshLocalBounds.Max.X : MeshLocalBounds.Min.X,
			(CornerIndex & 2) ? MeshLocalBounds.Max.Y : MeshLocalBounds.Min.Y,
			(CornerIndex & 4) ? MeshLocalBounds.Max.Z : MeshLocalBounds.Min.Z);

		const FVector WorldCorner = InstanceWorldTransform.TransformPosition(MeshLocalCorner);
		const FVector DetectionLocalCorner = DetectionBoxTransform.InverseTransformPosition(WorldCorner);
		InstanceBoundsInDetectionSpace += DetectionLocalCorner;
	}

	return InstanceBoundsInDetectionSpace.Intersect(LocalDetectionBox);
}

bool UFoliageOcclusionFadeComponent::DoesStandaloneMeshOverlapDetectionBox(
	const UStaticMeshComponent* Component,
	const FTransform& DetectionBoxTransform,
	const FBox& LocalDetectionBox) const
{
	if (!Component || !Component->GetStaticMesh())
	{
		return false;
	}

	// 普通网格只有一个组件变换，处理方式与单个 Foliage 实例相同：
	// 把网格局部包围盒的 8 个角转换到检测盒局部空间，再测试两个 AABB 是否相交。
	const FBox MeshLocalBounds = Component->GetStaticMesh()->GetBounds().GetBox();
	const FTransform ComponentWorldTransform = Component->GetComponentTransform();
	FBox ComponentBoundsInDetectionSpace(EForceInit::ForceInit);

	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector MeshLocalCorner(
			(CornerIndex & 1) ? MeshLocalBounds.Max.X : MeshLocalBounds.Min.X,
			(CornerIndex & 2) ? MeshLocalBounds.Max.Y : MeshLocalBounds.Min.Y,
			(CornerIndex & 4) ? MeshLocalBounds.Max.Z : MeshLocalBounds.Min.Z);

		const FVector WorldCorner = ComponentWorldTransform.TransformPosition(MeshLocalCorner);
		ComponentBoundsInDetectionSpace += DetectionBoxTransform.InverseTransformPosition(WorldCorner);
	}

	return ComponentBoundsInDetectionSpace.Intersect(LocalDetectionBox);
}

void UFoliageOcclusionFadeComponent::UpdateFades(float DeltaTime)
{
	for (auto It = ActiveFades.CreateIterator(); It; ++It)
	{
		UFoliageInstancedStaticMeshComponent* Component = It.Key().Component.Get();
		if (!IsValid(Component) || It.Key().InstanceIndex < 0 || It.Key().InstanceIndex >= Component->GetInstanceCount())
		{
			// 关卡流送或实例重建后，旧键可能失效，直接移除即可。
			It.RemoveCurrent();
			continue;
		}

		FFoliageFadeState& State = It.Value();
		const float Duration = State.TargetHiddenAmount > State.CurrentHiddenAmount
			? FadeOutDuration
			: FadeInDuration;
		const float InterpSpeed = 1.f / FMath::Max(Duration, KINDA_SMALL_NUMBER);
		const float NewHiddenAmount = FMath::FInterpConstantTo(
			State.CurrentHiddenAmount,
			State.TargetHiddenAmount,
			DeltaTime,
			InterpSpeed);

		if (!FMath::IsNearlyEqual(NewHiddenAmount, State.CurrentHiddenAmount))
		{
			State.CurrentHiddenAmount = NewHiddenAmount;

			// 只更新当前活动的少量实例，不会为每一帧遍历整片森林的所有实例。
			Component->SetCustomDataValue(
				It.Key().InstanceIndex,
				CustomDataIndex,
				State.CurrentHiddenAmount,
				true);
		}

		// 完全恢复显示后移出活动表，后续不再产生每帧更新成本。
		if (State.TargetHiddenAmount == 0.f && FMath::IsNearlyZero(State.CurrentHiddenAmount))
		{
			Component->SetCustomDataValue(It.Key().InstanceIndex, CustomDataIndex, 0.f, true);
			It.RemoveCurrent();
		}
	}
}

void UFoliageOcclusionFadeComponent::UpdateStandaloneMeshFades(float DeltaTime)
{
	for (auto It = ActiveStandaloneMeshFades.CreateIterator(); It; ++It)
	{
		UStaticMeshComponent* Component = It.Key().Get();
		FStandaloneMeshMaterials* Materials = StandaloneMeshMaterials.Find(It.Key());
		if (!IsValid(Component) || !Materials)
		{
			It.RemoveCurrent();
			continue;
		}

		FFoliageFadeState& State = It.Value();
		const float Duration = State.TargetHiddenAmount > State.CurrentHiddenAmount
			? FadeOutDuration
			: FadeInDuration;
		const float InterpSpeed = 1.f / FMath::Max(Duration, KINDA_SMALL_NUMBER);
		const float NewHiddenAmount = FMath::FInterpConstantTo(
			State.CurrentHiddenAmount,
			State.TargetHiddenAmount,
			DeltaTime,
			InterpSpeed);

		if (!FMath::IsNearlyEqual(NewHiddenAmount, State.CurrentHiddenAmount))
		{
			State.CurrentHiddenAmount = NewHiddenAmount;
			const float MaterialFadeValue = 1.f - State.CurrentHiddenAmount;

			// 每个普通网格都有自己的 MID，因此这里只影响当前这一个关卡对象。
			for (const TWeakObjectPtr<UMaterialInstanceDynamic>& WeakDynamicMaterial : Materials->DynamicMaterials)
			{
				if (UMaterialInstanceDynamic* DynamicMaterial = WeakDynamicMaterial.Get())
				{
					DynamicMaterial->SetScalarParameterValue(StandaloneFadeParameterName, MaterialFadeValue);
				}
			}
		}

		if (State.TargetHiddenAmount == 0.f && FMath::IsNearlyZero(State.CurrentHiddenAmount))
		{
			for (const TWeakObjectPtr<UMaterialInstanceDynamic>& WeakDynamicMaterial : Materials->DynamicMaterials)
			{
				if (UMaterialInstanceDynamic* DynamicMaterial = WeakDynamicMaterial.Get())
				{
					DynamicMaterial->SetScalarParameterValue(StandaloneFadeParameterName, 1.f);
				}
			}
			It.RemoveCurrent();
		}
	}
}

void UFoliageOcclusionFadeComponent::RestoreAllInstances()
{
	for (const TPair<FFoliageInstanceKey, FFoliageFadeState>& Pair : ActiveFades)
	{
		if (UFoliageInstancedStaticMeshComponent* Component = Pair.Key.Component.Get())
		{
			if (Pair.Key.InstanceIndex >= 0 && Pair.Key.InstanceIndex < Component->GetInstanceCount())
			{
				Component->SetCustomDataValue(Pair.Key.InstanceIndex, CustomDataIndex, 0.f, true);
			}
		}
	}

	// 普通网格先恢复 Fade，再把组件材质换回初始化前保存的原始材质。
	for (const TPair<TWeakObjectPtr<UStaticMeshComponent>, FStandaloneMeshMaterials>& Pair : StandaloneMeshMaterials)
	{
		if (UStaticMeshComponent* Component = Pair.Key.Get())
		{
			for (const TWeakObjectPtr<UMaterialInstanceDynamic>& WeakDynamicMaterial : Pair.Value.DynamicMaterials)
			{
				if (UMaterialInstanceDynamic* DynamicMaterial = WeakDynamicMaterial.Get())
				{
					DynamicMaterial->SetScalarParameterValue(StandaloneFadeParameterName, 1.f);
				}
			}

			for (int32 MaterialIndex = 0; MaterialIndex < Pair.Value.OriginalMaterials.Num(); ++MaterialIndex)
			{
				Component->SetMaterial(MaterialIndex, Pair.Value.OriginalMaterials[MaterialIndex].Get());
			}
		}
	}

	ActiveFades.Reset();
	ActiveStandaloneMeshFades.Reset();
	StandaloneMeshMaterials.Reset();
	InitializedComponents.Reset();
}

bool UFoliageOcclusionFadeComponent::IsSupportedTreeComponent(
	const UFoliageInstancedStaticMeshComponent* Component) const
{
	return Component && IsSupportedTreeMesh(Component->GetStaticMesh());
}

bool UFoliageOcclusionFadeComponent::IsSupportedTreeMesh(const UStaticMesh* StaticMesh) const
{
	if (!StaticMesh)
	{
		return false;
	}

	const FName MeshFName = StaticMesh->GetFName();
	const FString MeshName = MeshFName.ToString();

	// 排除规则优先级最高，避免 SM_TD01_Tree_Stump 等对象被树木前缀误包含。
	for (const FString& ExcludedFragment : ExcludedTreeMeshNameFragments)
	{
		if (!ExcludedFragment.IsEmpty() && MeshName.Contains(ExcludedFragment, ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	if (SupportedTreeMeshNames.Contains(MeshFName))
	{
		return true;
	}

	for (const FString& SupportedPrefix : SupportedTreeMeshPrefixes)
	{
		if (!SupportedPrefix.IsEmpty() && MeshName.StartsWith(SupportedPrefix, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// 两个允许列表都为空时表示不限制网格名称，适合用户自行确保该 Foliage Actor 只包含目标树。
	return SupportedTreeMeshNames.IsEmpty() && SupportedTreeMeshPrefixes.IsEmpty();
}
