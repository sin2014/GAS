// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FoliageOcclusionFadeComponent.generated.h"

class UFoliageInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 相机植被遮挡淡出组件。
 *
 * 该组件挂在本地玩家控制器上，在相机和角色之间创建一个可调节的有向盒体。
 * 盒体覆盖到的每个植被实例都会获得独立的淡出值，而不是让整个 HISM 批次一起消失。
 *
 * 材质前提：目标植被材质必须读取 PerInstanceCustomData[CustomDataIndex]，
 * 并将 0 解释为正常显示、1 解释为完全隐藏。
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class GAS_API UFoliageOcclusionFadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFoliageOcclusionFadeComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/**
	 * 一个植被实例的唯一键。
	 *
	 * InstanceIndex 只在所属的 FoliageInstancedStaticMeshComponent 内唯一，
	 * 因此必须同时保存组件和实例索引，才能准确表示关卡中的某一棵树。
	 */
	struct FFoliageInstanceKey
	{
		TWeakObjectPtr<UFoliageInstancedStaticMeshComponent> Component;
		int32 InstanceIndex = INDEX_NONE;

		bool operator==(const FFoliageInstanceKey& Other) const
		{
			return Component == Other.Component && InstanceIndex == Other.InstanceIndex;
		}

		friend uint32 GetTypeHash(const FFoliageInstanceKey& Key)
		{
			return HashCombine(GetTypeHash(Key.Component), GetTypeHash(Key.InstanceIndex));
		}
	};

	/** 保存一棵正在淡入或淡出的树的运行时状态。 */
	struct FFoliageFadeState
	{
		// 当前写入材质的隐藏量：0 表示完全显示，1 表示完全隐藏。
		float CurrentHiddenAmount = 0.f;

		// 当前希望到达的隐藏量。检测到遮挡时为 1，不再遮挡时为 0。
		float TargetHiddenAmount = 1.f;

		// 最近一次被检测盒覆盖的游戏时间，用于防止边缘抖动导致频繁淡入淡出。
		float LastSeenTime = 0.f;
	};

	/**
	 * 普通 StaticMeshComponent 的材质运行时数据。
	 * 普通网格没有 InstanceIndex，因此需要为每个组件创建独立 MID，并直接修改 Fade 参数。
	 */
	struct FStandaloneMeshMaterials
	{
		TArray<TWeakObjectPtr<UMaterialInterface>> OriginalMaterials;
		TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
	};

	/** 查找关卡中的植被组件，并为目标树批次分配实例自定义数据槽。 */
	void InitializeFoliageComponents();

	/** 查找单独放入关卡的普通静态网格，并为其创建独立动态材质实例。 */
	void InitializeStandaloneStaticMeshes();

	/** 构造相机到角色之间的检测盒，并找出盒内的实际植被实例。 */
	void ScanForOccludingInstances();

	/** 每帧把活动实例的当前隐藏量插值到目标值，并写入 Per Instance Custom Data。 */
	void UpdateFades(float DeltaTime);

	/** 更新普通静态网格的 Fade 标量参数。 */
	void UpdateStandaloneMeshFades(float DeltaTime);

	/** 组件停止时恢复全部活动实例，避免 PIE 结束或控制器销毁后残留隐藏值。 */
	void RestoreAllInstances();

	/** 判断某个 Foliage HISM 是否属于需要参与相机遮挡的树木批次。 */
	bool IsSupportedTreeComponent(const UFoliageInstancedStaticMeshComponent* Component) const;

	/** 普通静态网格和 Foliage 共用同一套名称筛选规则。 */
	bool IsSupportedTreeMesh(const UStaticMesh* StaticMesh) const;

	/**
	 * 对候选实例做更精确的有向盒体相交检查。
	 * GetInstancesOverlappingBox 使用世界轴对齐盒快速查询，此函数负责排除 AABB 角落里的误命中。
	 */
	bool DoesInstanceOverlapDetectionBox(
		const UFoliageInstancedStaticMeshComponent* Component,
		int32 InstanceIndex,
		const FTransform& DetectionBoxTransform,
		const FBox& LocalDetectionBox) const;

	/** 判断普通静态网格的渲染包围盒是否进入有向检测盒。 */
	bool DoesStandaloneMeshOverlapDetectionBox(
		const UStaticMeshComponent* Component,
		const FTransform& DetectionBoxTransform,
		const FBox& LocalDetectionBox) const;

	/** 两次遮挡扫描之间的时间。值越小响应越快，但查询频率越高。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Detection", meta = (ClampMin = "0.01", Units = "s"))
	float ScanInterval = 0.05f;

	/** 检测盒沿相机到角色方向的最大完整长度。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Detection Box", meta = (ClampMin = "1.0", Units = "cm"))
	float DetectionBoxLength = 750.f;

	/** 检测盒左右方向的完整宽度。过大会选中角色视线两侧并未真正遮挡的树。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Detection Box", meta = (ClampMin = "1.0", Units = "cm"))
	float DetectionBoxWidth = 24.f;

	/** 检测盒上下方向的完整高度。需要足以覆盖角色身体和树冠遮挡区域。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Detection Box", meta = (ClampMin = "1.0", Units = "cm"))
	float DetectionBoxHeight = 32.f;

	/** 检测盒近端沿相机 X 轴/观察方向离开相机的距离。用于避免检测到贴近相机的非目标物体。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Detection Box", meta = (ClampMin = "0.0", Units = "cm"))
	float DetectionBoxDistanceFromCamera = 25.f;

	/** 盒体远端与角色焦点之间保留的距离，防止角色后面的树被误判为遮挡物。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Detection Box", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bClampDetectionBoxToCharacter"))
	float CharacterEndPadding = 25.f;

	/** 开启后，检测盒长度不会越过角色焦点；建议保持开启。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Detection Box")
	bool bClampDetectionBoxToCharacter = true;

	/** 角色 Actor 原点上方的检测焦点高度，通常设置在胸口或头部附近。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Detection", meta = (ClampMin = "0.0", Units = "cm"))
	float CharacterFocusHeight = 32.f;

	/**
	 * 实例离开检测盒后等待多久才开始淡入。
	 * 这个宽限时间可以消除角色或相机轻微移动造成的边缘闪烁。
	 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Detection", meta = (ClampMin = "0.0", Units = "s"))
	float RestoreDelay = 0.2f;

	/** 从完全显示过渡到完全隐藏所需的时间。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Fade", meta = (ClampMin = "0.01", Units = "s"))
	float FadeOutDuration = 0.5f;

	/** 从完全隐藏恢复到完全显示所需的时间。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Fade", meta = (ClampMin = "0.01", Units = "s"))
	float FadeInDuration = 0.5f;

	/** 材质中 PerInstanceCustomData 使用的索引；必须与父材质节点的 Data Index 一致。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Fade", meta = (ClampMin = "0"))
	int32 CustomDataIndex = 0;

	/** 普通静态网格材质中用于控制可见度的标量参数名；1 显示，0 隐藏。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Fade")
	FName StandaloneFadeParameterName = TEXT("Fade");

	/** 是否同时处理手动拖入关卡的普通 StaticMeshComponent。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Filter")
	bool bHandleStandaloneStaticMeshes = true;

	/** 明确允许参与淡出的网格名称。这里保留当前关卡主要使用的三种树。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Filter")
	TSet<FName> SupportedTreeMeshNames = {
		TEXT("SM_TD01_Tree_01"),
		TEXT("SM_TD01_Tree_02"),
		TEXT("SM_TD01_Tree_03"),
		TEXT("SM_TD01_Tree_04"),
		TEXT("SM_TD01_Tree_05"),
		TEXT("SM_TD01_Tree_06"),
		TEXT("SM_TD01_Tree_07")
	};

	/**
	 * 允许的网格名称前缀。
	 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Filter")
	TArray<FString> SupportedTreeMeshPrefixes = {TEXT("SM_TD01_Tree_")};

	/** 名称中包含这些片段的网格不会参与淡出，例如树桩。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Filter")
	TArray<FString> ExcludedTreeMeshNameFragments = {TEXT("Stump")};

	/** 在 PIE 中绘制检测盒。绿色表示没有目标实例，红色表示检测到至少一个目标实例。 */
	UPROPERTY(EditAnywhere, Category = "Camera Occlusion|Debug")
	bool bDrawDebugTrace = false;

	// 已初始化并允许参与遮挡淡出的植被 HISM 批次。
	TSet<TWeakObjectPtr<UFoliageInstancedStaticMeshComponent>> InitializedComponents;

	// 当前正在淡出、保持隐藏或淡入的少量实例。
	TMap<FFoliageInstanceKey, FFoliageFadeState> ActiveFades;

	// 已初始化的普通静态网格以及为它们创建的独立 MID。
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FStandaloneMeshMaterials> StandaloneMeshMaterials;

	// 当前正在淡出、保持隐藏或淡入的普通静态网格。
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FFoliageFadeState> ActiveStandaloneMeshFades;

	// 扫描倒计时；淡入淡出插值仍然每帧执行。
	float TimeUntilNextScan = 0.f;
};
