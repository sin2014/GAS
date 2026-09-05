// Copyright Epic Games, Inc. All Rights Reserved.


#include "Widgets/GameSettingDetailView.h"

#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameSetting.h"
#include "Widgets/GameSettingDetailExtension.h"
#include "Widgets/GameSettingVisualData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingDetailView)

#define LOCTEXT_NAMESPACE "GameSetting"

// 创建详情视图并让扩展控件对象池以当前视图作为生命周期宿主。
UGameSettingDetailView::UGameSettingDetailView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ExtensionWidgetPool(*this)
{
}

// 释放设置详情视图持有的 Slate 控件引用，并按需释放子控件资源。
void UGameSettingDetailView::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	ExtensionWidgetPool.ReleaseAllSlateResources();
}

// 运行时初始化时先清空当前详情，确保尚未选择设置时不显示编辑器预览内容。
void UGameSettingDetailView::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!IsDesignTime())
	{
		FillSettingDetails(nullptr);
	}
}

// 保留详情视图的运行时构造入口；实际设置订阅在 FillSettingDetails 中按选择动态建立。
void UGameSettingDetailView::NativeConstruct()
{
	Super::NativeConstruct();
}

// 当前设置值变化时重新填充详情，确保动态说明和扩展内容同步。
void UGameSettingDetailView::HandleCurrentSettingChanged(UGameSetting* InSetting, EGameSettingChangeReason)
{
	if (RichText_Description)
	{
		RichText_Description->SetText(InSetting->GetDescriptionRichText());
	}
	if (RichText_DynamicDetails)
	{
		const FText DynamicDetails = InSetting->GetDynamicDetails();
		RichText_DynamicDetails->SetText(DynamicDetails);
		RichText_DynamicDetails->SetVisibility(DynamicDetails.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

// 释放旧扩展、更新标题和说明，并从对象池创建适用于当前设置的详情扩展控件。
void UGameSettingDetailView::FillSettingDetails(UGameSetting* InSetting)
{
	// 连续请求显示同一设置时直接忽略，避免重复重建详情。
	// Ignore requests to show the same setting multiple times in a row.
	if (InSetting && InSetting == CurrentSetting)
	{
		return;
	}

	if (CurrentSetting)
	{
		CurrentSetting->OnSettingChangedEvent.RemoveAll(this);
	}

	CurrentSetting = InSetting;

	if (CurrentSetting)
	{
		CurrentSetting->OnSettingChangedEvent.AddUObject(this, &ThisClass::HandleCurrentSettingChanged);
	}

	if (Text_SettingName)
	{
		Text_SettingName->SetText(InSetting ? InSetting->GetDisplayName() : FText::GetEmpty());
	}

	if (RichText_Description)
	{
		RichText_Description->SetText(InSetting ? InSetting->GetDescriptionRichText() : FText::GetEmpty());
	}

	if (RichText_DynamicDetails)
	{
		const FText DynamicDetails = InSetting ? InSetting->GetDynamicDetails() : FText::GetEmpty();
		RichText_DynamicDetails->SetText(DynamicDetails);
		RichText_DynamicDetails->SetVisibility(DynamicDetails.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (RichText_WarningDetails)
	{
		if (InSetting && !InSetting->GetWarningRichText().IsEmpty())
		{
			const FText WarningText = FText::Format(LOCTEXT("WarningReasonLine", "<Icon.Warning></> {0}"), InSetting->GetWarningRichText());
			RichText_WarningDetails->SetText(WarningText);
			RichText_WarningDetails->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			RichText_WarningDetails->SetText(FText::GetEmpty());
			RichText_WarningDetails->SetVisibility(ESlateVisibility::Collapsed);
		}

	}

	if (RichText_DisabledDetails)
	{
		TArray<FText> DisabledDetailLines;

		if (InSetting)
		{
			FGameSettingEditableState EditableState = InSetting->GetEditState();

			if (!EditableState.IsEnabled())
			{
				for (FText Reason : EditableState.GetDisabledReasons())
				{
					DisabledDetailLines.Add(FText::Format(LOCTEXT("DisabledReasonLine", "<Icon.Warning></> {0}"), Reason));
				}
			}

			if (EditableState.GetDisabledOptions().Num() > 0)
			{
				DisabledDetailLines.Add(LOCTEXT("DisabledOptionReasonLine", "<Icon.Warning></> There are fewer options than available due to Parental Controls."));
			}
		}

		RichText_DisabledDetails->SetText(FText::Join(FText::FromString(TEXT("\n")), DisabledDetailLines));
		RichText_DisabledDetails->SetVisibility(DisabledDetailLines.Num() == 0 ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	
	if (Box_DetailsExtension)
	{
		// 先把当前详情扩展控件归还对象池。
		// First release the widgets back into the pool.
		for (UWidget* ChildExtension : Box_DetailsExtension->GetAllChildren())
		{
			ExtensionWidgetPool.Release(Cast<UUserWidget>(ChildExtension));
		}

		// 再从详情容器中移除这些控件。
		// Remove the widgets from their container.
		Box_DetailsExtension->ClearChildren();

		if (InSetting)
		{
			TArray<TSoftClassPtr<UGameSettingDetailExtension>> ExtensionClassPtrs;
			if (VisualData)
			{
				ExtensionClassPtrs = VisualData->GatherDetailExtensions(InSetting);
			}
			
			if (StreamingHandle.IsValid())
			{
				StreamingHandle->CancelHandle();
			}

			bool bEverythingAlreadyLoaded = true;

			TArray<FSoftObjectPath> ExtensionPaths;
			ExtensionPaths.Reserve(ExtensionClassPtrs.Num());
			for (TSoftClassPtr<UGameSettingDetailExtension> SoftClassPtr : ExtensionClassPtrs)
			{
				bEverythingAlreadyLoaded &= SoftClassPtr.IsValid();
				ExtensionPaths.Add(SoftClassPtr.ToSoftObjectPath());
			}

			if (bEverythingAlreadyLoaded)
			{
				for (TSoftClassPtr<UGameSettingDetailExtension> SoftClassPtr : ExtensionClassPtrs)
				{
					CreateDetailsExtension(InSetting, SoftClassPtr.Get());
				}

				ExtensionWidgetPool.ReleaseInactiveSlateResources();
			}
			else
			{
				TWeakObjectPtr<UGameSetting> SettingPtr = InSetting;

				StreamingHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
					MoveTemp(ExtensionPaths),
					FStreamableDelegate::CreateWeakLambda(this, [this, SettingPtr, ExtensionClassPtrs] {
						for (TSoftClassPtr<UGameSettingDetailExtension> SoftClassPtr : ExtensionClassPtrs)
						{
							CreateDetailsExtension(SettingPtr.Get(), SoftClassPtr.Get());
						}

						ExtensionWidgetPool.ReleaseInactiveSlateResources();
					}
				));
			}
		}
	}
}

// 从对象池取得指定扩展控件、绑定当前设置并加入详情容器。
void UGameSettingDetailView::CreateDetailsExtension(UGameSetting* InSetting, TSubclassOf<UGameSettingDetailExtension> ExtensionClass)
{
	if (InSetting && ExtensionClass)
	{
		if (UGameSettingDetailExtension* Extension = ExtensionWidgetPool.GetOrCreateInstance(ExtensionClass))
		{
			Extension->SetSetting(InSetting);
			UVerticalBoxSlot* ExtensionSlot = Box_DetailsExtension->AddChildToVerticalBox(Extension);
			ExtensionSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

#undef LOCTEXT_NAMESPACE
