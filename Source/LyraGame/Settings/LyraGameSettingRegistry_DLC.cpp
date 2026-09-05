// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameSettingRegistry.h"

#include "Containers/Ticker.h"
#include "GameSettingAction.h"
#include "GameSettingCollection.h"
#include "GameSettingFilterState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Messaging/CommonGameDialog.h"
#include "Messaging/CommonMessagingSubsystem.h"
#include "Player/LyraLocalPlayer.h"
#include "PlatformDLC.h"
#include "PlatformDLCModule.h"

#define LOCTEXT_NAMESPACE "Lyra"

// DLC 下载状态文本的刷新间隔，单位为秒。
static constexpr float DLCStatusTickRate = 0.1f;

// 控制是否显示仅供开发测试使用的 DLC 菜单。
static TAutoConsoleVariable<bool> CVarShowDLCMenu(
	TEXT("Lyra.DLCMenu.Show"),
	false,
	TEXT("Whether to show the development-only DLC testing menu, if supported by the platform."));


namespace
{
	class FDLCInitEditCondition : public FGameSettingEditCondition
	{
	public:
		// 将 DLC 初始化条件标记为就绪，并通知设置系统重新计算可编辑状态。
		void SetReady()
		{
			bReady = true;
			BroadcastEditConditionChanged();
		}

		// DLC 尚未初始化完成时禁用相关设置项并显示等待提示。
		virtual void GatherEditState(const ULocalPlayer*, FGameSettingEditableState& InOutEditState) const override
		{
			if (!bReady)
			{
				InOutEditState.Disable(LOCTEXT("DLCInitializing", "Downloadable content is initializing..."));
			}
		}

	private:
		bool bReady = false;
	};

	// 从已加载的 PlatformDLC 模块取得平台 DLC 接口；模块不可用时返回空指针。
	static TSharedPtr<IPlatformDLC> GetPlatformDLC()
	{
		IPlatformDLCModule* PlatformDLCModule = FModuleManager::GetModulePtr<IPlatformDLCModule>(TEXT("PlatformDLC"));
		if (PlatformDLCModule == nullptr)
		{
			return nullptr;
		}

		return PlatformDLCModule->GetPlatformDLC();
	}

	// 根据 DLC 当前状态生成状态文本；下载中且可取得进度时附带百分比。
	static FText BuildDLCStatusText(IPlatformDLC* PlatformDLC, FName Name)
	{
		const IPlatformDLC::EState State = PlatformDLC->GetState(Name);
		if (State == IPlatformDLC::EState::Downloading)
		{
			uint64 Current = 0, Total = 0;
			if (PlatformDLC->GetDownloadSize(Name, Current, Total) && Current > 0 && Total > 0)
			{
				const int32 Pct = FMath::Clamp((int32)(100.0 * (double)Current / (double)Total), 0, 100);
				return FText::FromString(FString::Printf(TEXT("Status: Downloading... %d%%"), Pct));
			}
		}
		return FText::FromString(FString::Printf(TEXT("Status: %s"), *LexToString(State)));
	}
}

// 平台支持且开发开关启用时创建 DLC 测试页面，并异步刷新下载状态。
void ULyraGameSettingRegistry::AddDLCPage(UGameSettingCollection* Screen, ULyraLocalPlayer* InLocalPlayer)
{
	if (!CVarShowDLCMenu.GetValueOnGameThread())
	{
		return;
	}

	TSharedPtr<IPlatformDLC> PlatformDLC = GetPlatformDLC();
	if (!PlatformDLC.IsValid())
	{
		return;
	}

	UGameSettingCollectionPage* DLCPage = NewObject<UGameSettingCollectionPage>();
	DLCPage->SetDevName(TEXT("DLCPage"));
	DLCPage->SetDisplayName(LOCTEXT("DLCPage_Name",         "Downloadable Content"));
	DLCPage->SetDescriptionRichText(LOCTEXT("DLCPage_Desc", "Manage downloadable content packages."));
	DLCPage->SetNavigationText(LOCTEXT("DLCPage_Nav", "Manage"));

	TSharedRef<FDLCInitEditCondition> InitCondition = MakeShared<FDLCInitEditCondition>();
	Screen->AddSetting(DLCPage);
	DLCPage->AddEditCondition(InitCondition);

	// DLC 初始化完成后填充各设置分区并启用操作按钮；若已完成初始化，回调会立即执行。
	// Populate sections and enable the button once DLC is initialized. Fires immediately if already done.
	PlatformDLC->RegisterInitializationCallback([
		WeakThis    = TWeakObjectPtr<ULyraGameSettingRegistry>(this),
		WeakDLCPage = TWeakObjectPtr<UGameSettingCollectionPage>(DLCPage),
		InitCondition,
		PlatformDLC
	]()
	{
		ULyraGameSettingRegistry* This        = WeakThis.Get();
		UGameSettingCollectionPage* LocalPage = WeakDLCPage.Get();
		if (!This || !LocalPage)
		{
			return;
		}

		TArray<FName> DLCNames = PlatformDLC->GetAllDLCNames();
		if (DLCNames.IsEmpty())
		{
			return;
		}

		TArray<TWeakObjectPtr<UGameSetting>> AllActions;

		for (const FName& DLCName : DLCNames)
		{
			UGameSettingCollection* Section = NewObject<UGameSettingCollection>();
			Section->SetDevName(FName(*FString::Printf(TEXT("DLC_%s"), *DLCName.ToString())));
			Section->SetDisplayName(FText::FromName(DLCName));
			LocalPage->AddSetting(Section);

			auto AddAction = [This, &Section, &DLCName, &AllActions](FName DevName, FText Label, FText Desc, TFunction<void(ULocalPlayer*)> OnExecute)
			{
				UGameSettingAction* Action = NewObject<UGameSettingAction>();
				Action->SetDevName(DevName);
				Action->SetDisplayName(FText::FromString(TEXT(" "))); /* 设置单个空格作为占位，因为该设置项的显示名称不能是空字符串。 */ // cannot have empty string
				Action->SetDescriptionRichText(Desc);
				Action->SetActionText(Label);
				Action->SetCustomAction(MoveTemp(OnExecute));
				Action->SetDynamicDetails(FGetGameSettingsDetails::CreateWeakLambda(This, [DLCName](ULocalPlayer&) -> FText
				{
					if (TSharedPtr<IPlatformDLC> PlatformDLC = GetPlatformDLC())
					{
						return BuildDLCStatusText(PlatformDLC.Get(), DLCName);
					}

					return FText::GetEmpty();
				}));
				Section->AddSetting(Action);
				AllActions.Add(Action);
			};

			AddAction(
				FName(*FString::Printf(TEXT("DLC_%s_Download"), *DLCName.ToString())),
				LOCTEXT("DLC_Download",      "Download"),
				LOCTEXT("DLC_Download_Desc", "Download this DLC package."),
				[DLCName](ULocalPlayer*)
				{
					if (TSharedPtr<IPlatformDLC> PlatformDLC = GetPlatformDLC())
					{
						PlatformDLC->Download(DLCName);
					}
				});

			AddAction(
				FName(*FString::Printf(TEXT("DLC_%s_Mount"), *DLCName.ToString())),
				LOCTEXT("DLC_Mount",      "Mount"),
				LOCTEXT("DLC_Mount_Desc", "Mount this DLC package so its content is available."),
				[DLCName](ULocalPlayer*)
				{
					if (TSharedPtr<IPlatformDLC> PlatformDLC = GetPlatformDLC())
					{
						PlatformDLC->Mount(DLCName);
					}
				});

			AddAction(
				FName(*FString::Printf(TEXT("DLC_%s_Launch"), *DLCName.ToString())),
				LOCTEXT("DLC_Launch",      "Launch"),
				LOCTEXT("DLC_Launch_Desc", "Open the DLC map, if mounted."),
				[DLCName](ULocalPlayer* LocalPlayer)
				{
					if (LocalPlayer != nullptr)
					{
						// DLC 测试地图遵循“地图名与 DLC 插件名相同”的约定，据此拼出要加载的地图。
						// DLC test map convention is that the map name matches the DLC plugin name
						const FString MapPath = FString::Printf(TEXT("/%s/%s"), *DLCName.ToString(), *DLCName.ToString());
						UGameplayStatics::OpenLevel(LocalPlayer, FName(*MapPath));
					}
				});

			AddAction(
				FName(*FString::Printf(TEXT("DLC_%s_Unmount"), *DLCName.ToString())),
				LOCTEXT("DLC_Unmount",      "Unmount"),
				LOCTEXT("DLC_Unmount_Desc", "Unmount this DLC package."),
				[DLCName](ULocalPlayer* LocalPlayer)
				{
					if (LocalPlayer == nullptr)
					{
						return;
					}

					TSharedPtr<IPlatformDLC> PlatformDLC = GetPlatformDLC();
					if (!PlatformDLC.IsValid())
					{
						return;
					}

					// 仍处于 DLC 地图时卸载其插件，后续垃圾回收很可能访问已卸载资源并崩溃。
					// Unmounting while inside the DLC map will likely crash in GC
					UWorld* World = LocalPlayer->GetWorld();
					if (World && World->GetMapName() == DLCName.ToString())
					{
						UCommonGameDialogDescriptor* Descriptor = UCommonGameDialogDescriptor::CreateConfirmationOkCancel(
							LOCTEXT("UnmountWarning_Header", "Unmount DLC?"),
							LOCTEXT("UnmountWarning_Body", "You are currently inside this DLC's map. Unmounting will likely crash the game. Are you sure?"));

						if (UCommonMessagingSubsystem* Messaging = LocalPlayer->GetSubsystem<UCommonMessagingSubsystem>())
						{
							Messaging->ShowConfirmation(Descriptor,
								FCommonMessagingResultDelegate::CreateWeakLambda(LocalPlayer, [PlatformDLC, DLCName](ECommonMessagingResult Result)
								{
									if (Result == ECommonMessagingResult::Confirmed)
									{
										PlatformDLC->Unmount(DLCName);
									}
								}));
						}
					}
					else
					{
						PlatformDLC->Unmount(DLCName);
					}
				});

			AddAction(
				FName(*FString::Printf(TEXT("DLC_%s_Uninstall"), *DLCName.ToString())),
				LOCTEXT("DLC_Uninstall",      "Uninstall"),
				LOCTEXT("DLC_Uninstall_Desc", "Remove this DLC package from local storage."),
				[DLCName](ULocalPlayer*)
				{
					if (TSharedPtr<IPlatformDLC> PlatformDLC = GetPlatformDLC())
					{
						PlatformDLC->Uninstall(DLCName);
					}
				});
		}

		if (This->DLCTickHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(This->DLCTickHandle);
		}

		// 周期性刷新该设置项，使 DLC 的“状态”文本能随安装或挂载进度自动更新。
		// periodically refresh the setting - this means the 'status' text is updated automatically
		This->DLCTickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(This,
				[This, CapturedActions = MoveTemp(AllActions)](float) -> bool
				{
					for (const TWeakObjectPtr<UGameSetting>& WeakSetting : CapturedActions)
					{
						if (UGameSetting* Setting = WeakSetting.Get())
						{
							Setting->OnSettingChangedEvent.Broadcast(Setting, EGameSettingChangeReason::DependencyChanged);
						}
					}
					return true;
				}),
			DLCStatusTickRate);

		InitCondition->SetReady();
		LocalPage->RefreshEditableState();
	});
}

#undef LOCTEXT_NAMESPACE
