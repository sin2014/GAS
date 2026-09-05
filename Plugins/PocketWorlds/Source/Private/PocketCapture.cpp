// Copyright Epic Games, Inc. All Rights Reserved.

#include "PocketCapture.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PocketCaptureSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PocketCapture)

class UWorld;

// UPocketCapture
//---------------------------------------------------------------------------------

// 构造尚未绑定世界和渲染器槽位的 Pocket Capture；捕获组件由 Initialize 按需创建。
UPocketCapture::UPocketCapture()
{
}

// 绑定所属世界和稳定索引，创建仅渲染 ShowOnlyActors 的 2D SceneCapture，并关闭逐帧与移动触发捕获以支持显式离屏渲染。
void UPocketCapture::Initialize(UWorld* InWorld, int32 InRendererIndex)
{
	PrivateWorld = InWorld;
	RendererIndex = InRendererIndex;

	CaptureComponent = NewObject<USceneCaptureComponent2D>(this, "Thumbnail_Capture_Component");
	CaptureComponent->RegisterComponentWithWorld(InWorld);
	CaptureComponent->bConsiderUnrenderedOpaquePixelAsFullyTranslucent = true;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;

	//UE_LOG(LogPocketLevels, Log, TEXT("ThumbnailRenderer: Initialize:%s"), *GetName());
}

// 从世界注销场景捕获组件，停止其参与渲染；对象本身仍可由子系统槽位管理。
void UPocketCapture::Deinitialize()
{
	CaptureComponent->UnregisterComponent();

	//UE_LOG(LogPocketLevels, Log, TEXT("ThumbnailRenderer: Deinitialize:%s"), *GetName());
}

// UObject 销毁时确保捕获组件已从世界注销并清空引用，避免残留已注册组件。
void UPocketCapture::BeginDestroy()
{
	Super::BeginDestroy();

	if (CaptureComponent)
	{
		CaptureComponent->UnregisterComponent();
		CaptureComponent = nullptr;
	}
}

// 更新所有输出面的目标尺寸，仅在尺寸变化时调整已经创建的渲染目标，尚未创建的目标会在首次访问时采用新尺寸。
void UPocketCapture::SetRenderTargetSize(int32 Width, int32 Height)
{
	if (SurfaceWidth != Width || SurfaceHeight != Height)
	{
		SurfaceWidth = Width;
		SurfaceHeight = Height;

		if (DiffuseRT)
		{
			DiffuseRT->ResizeTarget(SurfaceWidth, SurfaceHeight);
		}

		if (AlphaMaskRT)
		{
			AlphaMaskRT->ResizeTarget(SurfaceWidth, SurfaceHeight);
		}

		if (EffectsRT)
		{
			EffectsRT->ResizeTarget(SurfaceWidth, SurfaceHeight);
		}
	}

	//UE_LOG(LogPocketLevels, Log, TEXT("ThumbnailRenderer: SetRenderTargetSize:%dx%d"), Width, Height);
}

// 按需创建 RGBA8 漫反射/最终颜色渲染目标，并立即初始化对应渲染资源。
UTextureRenderTarget2D* UPocketCapture::GetOrCreateDiffuseRenderTarget()
{
	if (DiffuseRT == nullptr)
	{
		DiffuseRT = NewObject<UTextureRenderTarget2D>(this, TEXT("ThumbnailRenderer_Diffuse"));
		DiffuseRT->RenderTargetFormat = RTF_RGBA8;
		DiffuseRT->InitAutoFormat(SurfaceWidth, SurfaceHeight);
		DiffuseRT->UpdateResourceImmediate(true);
	}

	return DiffuseRT;
}

// 按需创建单通道 R8 Alpha Mask 渲染目标，并立即初始化对应渲染资源。
UTextureRenderTarget2D* UPocketCapture::GetOrCreateAlphaMaskRenderTarget()
{
	if (AlphaMaskRT == nullptr)
	{
		AlphaMaskRT = NewObject<UTextureRenderTarget2D>(this, TEXT("ThumbnailRenderer_AlphaMask"));
		AlphaMaskRT->RenderTargetFormat = RTF_R8;
		AlphaMaskRT->InitAutoFormat(SurfaceWidth, SurfaceHeight);
		AlphaMaskRT->UpdateResourceImmediate(true);
	}

	return AlphaMaskRT;
}

// 按需创建单通道 R8 特效遮罩渲染目标，并立即初始化对应渲染资源。
UTextureRenderTarget2D* UPocketCapture::GetOrCreateEffectsRenderTarget()
{
	if (EffectsRT == nullptr)
	{
		EffectsRT = NewObject<UTextureRenderTarget2D>(this, TEXT("ThumbnailRenderer_Fx"));
		EffectsRT->RenderTargetFormat = RTF_R8;
		EffectsRT->InitAutoFormat(SurfaceWidth, SurfaceHeight);
		EffectsRT->UpdateResourceImmediate(true);
	}

	return EffectsRT;
}

// 更新主捕获目标弱引用，并通知派生类同步相机或其他目标相关状态。
void UPocketCapture::SetCaptureTarget(AActor* InCaptureTarget)
{
	CaptureTargetPtr = InCaptureTarget;

	OnCaptureTargetChanged(InCaptureTarget);
}

// 用新的 Actor 弱引用集合替换 Alpha Mask 捕获列表，避免渲染器延长目标生命周期。
void UPocketCapture::SetAlphaMaskedActors(const TArray<AActor*>& InCaptureTargets)
{
	AlphaMaskActorPtrs.Reset();

	for (AActor* CaptureTarget : InCaptureTargets)
	{
		AlphaMaskActorPtrs.Add(CaptureTarget);
	}
}

// 返回作为 Outer 持有本对象的 Pocket Capture 子系统；类型不匹配会触发检查失败。
UPocketCaptureSubsystem* UPocketCapture::GetThumbnailSystem() const
{
	return CastChecked<UPocketCaptureSubsystem>(GetOuter());
}

// 收集目标 Actor 及其 ChildActor 中未在游戏内隐藏的 PrimitiveComponent，供 ShowOnly 和纹理流送使用。
TArray<UPrimitiveComponent*> UPocketCapture::GatherPrimitivesForCapture(const TArray<AActor*>& InCaptureActors) const
{
	const bool bIncludeFromChildActors = true;
	TArray<UPrimitiveComponent*> PrimitiveComponents;

	for (AActor* CaptureActor : InCaptureActors)
	{
		TArray<UPrimitiveComponent*> ChildPrimitiveComponents;
		CaptureActor->GetComponents(ChildPrimitiveComponents, bIncludeFromChildActors);

		for (UPrimitiveComponent* ChildPrimitiveComponent : ChildPrimitiveComponents)
		{
			if (!ChildPrimitiveComponent->bHiddenInGame)
			{
				PrimitiveComponents.Add(ChildPrimitiveComponent);
			}
		}
	}

	return PrimitiveComponents;
}

// 使用主目标上的相机视图离屏捕获指定 Actor；可临时覆盖全部材质，捕获后恢复原材质。目标、Actor、相机或渲染目标无效时返回 false。
bool UPocketCapture::CaptureScene(UTextureRenderTarget2D* InRenderTarget, const TArray<AActor*>& InCaptureActors, ESceneCaptureSource InCaptureSource, UMaterialInterface* OverrideMaterial)
{
	if (InRenderTarget == nullptr)
	{
		//UE_LOG(LogPocketLevels, Error, TEXT(""));
		return false;
	}

	if (AActor* CaptureTarget = CaptureTargetPtr.Get())
	{
		if (InCaptureActors.Num() > 0)
		{
			TArray<UPrimitiveComponent*> PrimitiveComponents = GatherPrimitivesForCapture(InCaptureActors);
			
			GetThumbnailSystem()->StreamThisFrame(PrimitiveComponents);

			TArray<UMaterialInterface*> OriginalMaterials;
			if (OverrideMaterial)
			{
				for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
				{
					const int32 MaterialCount = PrimitiveComponent->GetNumMaterials();
					for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; MaterialIndex++)
					{
						OriginalMaterials.Add(PrimitiveComponent->GetMaterial(MaterialIndex));

						PrimitiveComponent->SetMaterial(MaterialIndex, OverrideMaterial);
					}
				}
			}

			UCameraComponent* Camera = CaptureTarget->FindComponentByClass<UCameraComponent>();
			if (ensure(Camera))
			{
				CaptureComponent->ShowOnlyActors = InCaptureActors;

				FMinimalViewInfo CaptureView;
				Camera->GetCameraView(0, CaptureView);

				// 离屏相机位置也必须纳入纹理流送判断；请求只持续一个 Tick，因此每次绘制都重新请求，确保纹理保持驻留。
				// We need to make sure the texture streamer takes into account this new location,
				// this request only lasts for one tick, so we call it every time we need to draw, 
				// so that they stay resident.

				CaptureComponent->TextureTarget = InRenderTarget;
				CaptureComponent->PostProcessSettings = Camera->PostProcessSettings;
				CaptureComponent->SetCameraView(CaptureView);

				CaptureComponent->ShowFlags.SetDepthOfField(false);
				CaptureComponent->ShowFlags.SetMotionBlur(false);
				CaptureComponent->ShowFlags.SetScreenPercentage(false);
				CaptureComponent->ShowFlags.SetScreenSpaceReflections(false);
				CaptureComponent->ShowFlags.SetDistanceFieldAO(false);

				CaptureComponent->ShowFlags.SetLensFlares(false);
				CaptureComponent->ShowFlags.SetOnScreenDebug(false);
				//CaptureComponent->ShowFlags.SetEyeAdaptation(false);
				CaptureComponent->ShowFlags.SetColorGrading(false);
				CaptureComponent->ShowFlags.SetCameraImperfections(false);
				CaptureComponent->ShowFlags.SetVignette(false);
				CaptureComponent->ShowFlags.SetGrain(false);
				CaptureComponent->ShowFlags.SetSeparateTranslucency(false);
				CaptureComponent->ShowFlags.SetScreenPercentage(false);
				CaptureComponent->ShowFlags.SetScreenSpaceReflections(false);
				CaptureComponent->ShowFlags.SetTemporalAA(false);
				// 若捕获频率很低，环境光遮蔽可能引发资源重新分配，因此暂时关闭。
				// might cause reallocation if we render rarely to it - for now off
				CaptureComponent->ShowFlags.SetAmbientOcclusion(false);
				// 间接光照缓存依赖 FScene 资源；临时场景每次启用都需重新分配，因此关闭。
				// Requires resources in the FScene, which get reallocated for every temporary scene if enabled
				CaptureComponent->ShowFlags.SetIndirectLightingCache(false);
				CaptureComponent->ShowFlags.SetLightShafts(false);
				CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
				CaptureComponent->ShowFlags.SetHighResScreenshotMask(false);
				CaptureComponent->ShowFlags.SetHMDDistortion(false);
				CaptureComponent->ShowFlags.SetStereoRendering(false);
				CaptureComponent->ShowFlags.SetVolumetricFog(false);
				CaptureComponent->ShowFlags.SetVolumetricLightmap(false);
				CaptureComponent->ShowFlags.SetSkyLighting(false);

				CaptureComponent->CaptureSource = InCaptureSource;
				CaptureComponent->ProfilingEventName = TEXT("Pocket Capture");
				CaptureComponent->CaptureScene();

				if (OriginalMaterials.Num() > 0)
				{
					int32 TotalMaterialIndex = 0;
					for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
					{
						const int32 MaterialCount = PrimitiveComponent->GetNumMaterials();
						for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; MaterialIndex++)
						{
							PrimitiveComponent->SetMaterial(MaterialIndex, OriginalMaterials[TotalMaterialIndex]);
							TotalMaterialIndex++;
						}
					}
				}

				return true;
			}
		}
		else
		{
			//UE_LOG(LogPocketLevels, Warning, TEXT("UPocketCapture: %s CaptureScene Failed: No Capture Actors"), *GetName());
		}
	}
	else
	{
		//UE_LOG(LogPocketLevels, Warning, TEXT("UPocketCapture: %s CaptureScene Failed: No Capture Target"), *GetName());
	}

	return false;
}

// 捕获主目标及其附加 Actor 的 LDR 最终颜色到漫反射渲染目标。
void UPocketCapture::CaptureDiffuse()
{
	if (UTextureRenderTarget2D* RenderTarget = GetOrCreateDiffuseRenderTarget())
	{
		TArray<AActor*> CaptureActors;
		if (AActor* CaptureTarget = CaptureTargetPtr.Get())
		{
			CaptureTarget->GetAttachedActors(CaptureActors);
			CaptureActors.Add(CaptureTarget);
		}

		CaptureScene(RenderTarget, CaptureActors, ESceneCaptureSource::SCS_FinalColorLDR, nullptr);
	}
}

// 过滤仍有效的 Alpha Mask Actor，使用覆盖材质把其 HDR 场景颜色写入单通道遮罩目标。
void UPocketCapture::CaptureAlphaMask()
{
	if (UTextureRenderTarget2D* RenderTarget = GetOrCreateAlphaMaskRenderTarget())
	{
		TArray<AActor*> CaptureActors;
		for (const TWeakObjectPtr<AActor>& AlphaMaskTargetPtr : AlphaMaskActorPtrs)
		{
			if (AActor* AlphaMaskTarget = AlphaMaskTargetPtr.Get())
			{
				CaptureActors.Add(AlphaMaskTarget);
			}
		}

		CaptureScene(RenderTarget, CaptureActors, ESceneCaptureSource::SCS_SceneColorHDR, AlphaMaskMaterial);
	}
}

// 预留的特效遮罩捕获入口；当前会触发 ensure 提示尚未实现，随后以空 Actor 列表尝试捕获并失败。
void UPocketCapture::CaptureEffects()
{
	if (UTextureRenderTarget2D* RenderTarget = GetOrCreateEffectsRenderTarget())
	{
		// TODO：确定特效捕获 Actor 集合及输出语义后实现该路径。
		ensure(false);//TODO
		TArray<AActor*> CaptureActors;
		CaptureScene(RenderTarget, CaptureActors, ESceneCaptureSource::SCS_SceneColorHDR, EffectMaskMaterial);
	}
}

// 释放已创建渲染目标的底层 RHI 资源，保留 UObject 以便稍后原位恢复。
void UPocketCapture::ReleaseResources()
{
	if (DiffuseRT)
	{
		DiffuseRT->ReleaseResource();
	}

	if (AlphaMaskRT)
	{
		AlphaMaskRT->ReleaseResource();
	}

	if (EffectsRT)
	{
		EffectsRT->ReleaseResource();
	}

	//OnReleaseResources();
}

// 为仍存在的渲染目标重新创建底层渲染资源，使暂停后的 Pocket Capture 可以继续使用。
void UPocketCapture::ReclaimResources()
{
	if (DiffuseRT)
	{
		DiffuseRT->UpdateResource();
	}

	if (AlphaMaskRT)
	{
		AlphaMaskRT->UpdateResource();
	}

	if (EffectsRT)
	{
		EffectsRT->UpdateResource();
	}

	//OnReclaimResources();
}

// 返回子系统分配的稳定渲染器槽位索引。
int32 UPocketCapture::GetRendererIndex() const
{
	return RendererIndex;
}
