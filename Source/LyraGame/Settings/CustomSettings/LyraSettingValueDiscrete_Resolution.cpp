// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingValueDiscrete_Resolution.h"

#include "DynamicRHI.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "UnrealEngine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingValueDiscrete_Resolution)

#define LOCTEXT_NAMESPACE "LyraSettings"

// 构造屏幕分辨率设置项并初始化内部状态。
ULyraSettingValueDiscrete_Resolution::ULyraSettingValueDiscrete_Resolution()
{
}

// 销毁设置项前解除与屏幕分辨率相关的外部事件监听。
void ULyraSettingValueDiscrete_Resolution::BeginDestroy()
{
	Super::BeginDestroy();

	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<class GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
		if (PlatformApplication.IsValid())
		{
			GenericApplication::FOnDisplayMetricsChanged& DisplayMetricsChangedEvent = PlatformApplication->OnDisplayMetricsChanged();
			DisplayMetricsChangedEvent.Remove(DisplayMetricsChangedHandle);
		}
	}
}

// 初始化屏幕分辨率选项、数据源及依赖关系。
void ULyraSettingValueDiscrete_Resolution::OnInitialized()
{
	Super::OnInitialized();

	TSharedPtr<class GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
	if (ensure(PlatformApplication.IsValid()))
	{
		FDisplayMetrics::RebuildDisplayMetrics(CurrentDisplayMetrics);

		GenericApplication::FOnDisplayMetricsChanged& DisplayMetricsChangedEvent = PlatformApplication->OnDisplayMetricsChanged();
		if (!DisplayMetricsChangedEvent.IsBoundToObject(this))
		{
			DisplayMetricsChangedHandle = DisplayMetricsChangedEvent.AddUObject(this, &ULyraSettingValueDiscrete_Resolution::OnDisplayMetricsChanged);
		}
	}

	InitializeResolutions();
}

// 此设置项不另存分辨率初始值，由 UGameUserSettings 的视频模式确认与回退流程管理。
void ULyraSettingValueDiscrete_Resolution::StoreInitial()
{
	// 分辨率初始状态由 UGameUserSettings 的视频模式确认与回退流程管理，此设置项无需另存副本。
	// Ignored
}

// 当前未在此离散设置项中实现独立的默认分辨率重置。
void ULyraSettingValueDiscrete_Resolution::ResetToDefault()
{
	// 当前不在此离散设置项中单独实现恢复默认分辨率。
	// Ignored
}

// 当前未在此离散设置项中实现初始分辨率恢复，由视频模式回退流程负责。
void ULyraSettingValueDiscrete_Resolution::RestoreToInitial()
{
	// 初始分辨率回退交由 UGameUserSettings 的视频模式恢复流程处理。
	// Ignored
}

// 将有效索引对应的选项写入屏幕分辨率；索引无效时不修改。
void ULyraSettingValueDiscrete_Resolution::SetDiscreteOptionByIndex(int32 Index)
{
	TArrayView<const TSharedPtr<ULyraSettingValueDiscrete_Resolution::FScreenResolutionEntry>> Resolutions = GetSelectedResolutionList();
	if (Resolutions.IsValidIndex(Index) && Resolutions[Index].IsValid())
	{
		GEngine->GetGameUserSettings()->SetScreenResolution(Resolutions[Index]->GetResolution());
		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

// 返回当前屏幕分辨率的精确匹配索引；未找到时返回 INDEX_NONE。
int32 ULyraSettingValueDiscrete_Resolution::GetDiscreteOptionIndex() const
{
	const UGameUserSettings* const UserSettings = GEngine->GetGameUserSettings();

	return FindIndexOfDisplayResolution(UserSettings->GetScreenResolution());
}

// 返回当前可供界面显示的屏幕分辨率选项文本。
TArray<FText> ULyraSettingValueDiscrete_Resolution::GetDiscreteOptions() const
{
	TArray<FText> ReturnResolutionTexts;

	TArrayView<const TSharedPtr<ULyraSettingValueDiscrete_Resolution::FScreenResolutionEntry>> Resolutions = GetSelectedResolutionList();
	for (int32 i = 0; i < Resolutions.Num(); ++i)
	{
		ReturnResolutionTexts.Add(Resolutions[i]->GetDisplayText());
	}

	return ReturnResolutionTexts;
}

// 依赖设置变化后重新生成屏幕分辨率选项并刷新当前值。
void ULyraSettingValueDiscrete_Resolution::OnDependencyChanged()
{
	InitializeResolutions();
	const FIntPoint CurrentResolution = GEngine->GetGameUserSettings()->GetScreenResolution();
	SetDiscreteOptionByIndex(FindClosestResolutionIndex(CurrentResolution));
}

// 显示器拓扑或工作区变化后重新生成屏幕分辨率选项。
void ULyraSettingValueDiscrete_Resolution::OnDisplayMetricsChanged(const FDisplayMetrics& NewDisplayMetrics)
{
	CurrentDisplayMetrics = NewDisplayMetrics;
	InitializeResolutions();
}

// 根据当前窗口位置查找所属显示器；无法确定时返回空指针。
const FMonitorInfo* ULyraSettingValueDiscrete_Resolution::GetCurrentMonitor() const
{
	const UGameUserSettings* const UserSettings = GEngine->GetGameUserSettings();
	const FString DisplayID = UserSettings->GetDisplayID();
	const int32 DisplayIndex = UserSettings->GetDisplayIndex();
	const int32 MonitorIndex = CurrentDisplayMetrics.GetClosestMonitorFromIDAndIndex(DisplayID, DisplayIndex);
	return CurrentDisplayMetrics.MonitorInfo.IsValidIndex(MonitorIndex) ? &CurrentDisplayMetrics.MonitorInfo[MonitorIndex] : nullptr;
}

// 按当前窗口模式和显示器指标重新构建全屏、无边框及窗口分辨率列表。
void ULyraSettingValueDiscrete_Resolution::InitializeResolutions()
{
	ResolutionsFullscreen.Empty();
	ResolutionsWindowed.Empty();
	ResolutionsWindowedFullscreen.Empty();

	const FMonitorInfo* const Monitor = GetCurrentMonitor();
	FScreenResolutionArray ResArray;
	if (Monitor)
	{
		RHIGetAvailableResolutionsForDisplay(ResArray, true, Monitor->NativeHandle);
	}
	else
	{
		RHIGetAvailableResolutions(ResArray, true);
	}

	// 生成普通窗口模式可用的分辨率列表，范围受当前显示器工作区约束。
	// Determine available windowed modes
	{
		TArray<FIntPoint> WindowedResolutions;
		const FIntPoint MinResolution(1280, 720);
		FIntPoint MaxResolution;
		if (Monitor)
		{
			MaxResolution = FIntPoint(Monitor->WorkArea.Right - Monitor->WorkArea.Left, Monitor->WorkArea.Bottom - Monitor->WorkArea.Top);
		}
		else
		{
			MaxResolution = FIntPoint(CurrentDisplayMetrics.PrimaryDisplayWorkAreaRect.Right - CurrentDisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
				CurrentDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - CurrentDisplayMetrics.PrimaryDisplayWorkAreaRect.Top);
		}
		// 排除 4:3 及更窄的宽高比，仅保留至少 16:10 的窗口分辨率。
		// Excluding 4:3 and below
		const float MinAspectRatio = 16 / 10.f;

		if (MaxResolution.X >= MinResolution.X && MaxResolution.Y >= MinResolution.Y)
		{
			GetStandardWindowResolutions(MinResolution, MaxResolution, MinAspectRatio, WindowedResolutions);
		}

		if (GSystemResolution.WindowMode == EWindowMode::Windowed)
		{
			if (GSystemResolution.ResX <= MaxResolution.X && GSystemResolution.ResY <= MaxResolution.Y)
			{
				WindowedResolutions.AddUnique(FIntPoint(GSystemResolution.ResX, GSystemResolution.ResY));
			}
			WindowedResolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
		}

		// 若标准分辨率列表为空，则加入主显示器尺寸作为保底选项；非标准显示设备上可能出现这种情况。
		// If there were no standard resolutions. Add the primary display size, just so one exists.
		// This might happen if we are running on a non-standard device.
		if (WindowedResolutions.Num() == 0)
		{
			WindowedResolutions.Add(FIntPoint(CurrentDisplayMetrics.PrimaryDisplayWidth, CurrentDisplayMetrics.PrimaryDisplayHeight));
		}

		ResolutionsWindowed.Empty(WindowedResolutions.Num());
		for (const FIntPoint& Res : WindowedResolutions)
		{
			TSharedRef<FScreenResolutionEntry> Entry = MakeShared<FScreenResolutionEntry>();
			Entry->Width = Res.X;
			Entry->Height = Res.Y;

			ResolutionsWindowed.Add(Entry);
		}
	}

	// 无边框窗口全屏始终采用当前显示器的完整显示区域尺寸。
	// Determine available windowed full-screen modes
	{
		TSharedRef<FScreenResolutionEntry> Entry = MakeShared<FScreenResolutionEntry>();
		if (Monitor)
		{
			const FPlatformRect& DisplayRect = Monitor->DisplayRect;
			Entry->Width = DisplayRect.Right - DisplayRect.Left;
			Entry->Height = DisplayRect.Bottom - DisplayRect.Top;
		}
		else
		{
			Entry->Width = CurrentDisplayMetrics.PrimaryDisplayWidth;
			Entry->Height = CurrentDisplayMetrics.PrimaryDisplayHeight;
		}

		ResolutionsWindowedFullscreen.Add(Entry);
	}

	// 从 RHI 报告的显示模式中筛选独占全屏可用分辨率。
	// Determine available full-screen modes
	if (!ResArray.IsEmpty())
	{
		// 从最严格条件逐级放宽筛选，确保最终至少保留一个可用分辨率。
		// try more strict first then more relaxed, we want at least one resolution to remain
		for (int32 FilterThreshold = 0; FilterThreshold < 3; ++FilterThreshold)
		{
			for (int32 ModeIndex = 0; ModeIndex < ResArray.Num(); ModeIndex++)
			{
				const FScreenResolutionRHI& ScreenRes = ResArray[ModeIndex];

				// 先执行严格筛选；当前阈值没有结果时，外层循环会改用更宽松条件重试。
				// first try with struct test, than relaxed test
				if (ShouldAllowFullScreenResolution(ScreenRes, FilterThreshold))
				{
					TSharedRef<FScreenResolutionEntry> Entry = MakeShared<FScreenResolutionEntry>();
					Entry->Width = ScreenRes.Width;
					Entry->Height = ScreenRes.Height;
					Entry->RefreshRate = ScreenRes.RefreshRate;

					ResolutionsFullscreen.Add(Entry);
				}
			}

			if (!ResolutionsFullscreen.IsEmpty())
			{
				// 已取得可用分辨率便停止放宽条件；否则继续提高 FilterThreshold。
				// we found some resolutions, otherwise we try with more relaxed tests
				break;
			}
		}
	}

	if (ResolutionsFullscreen.IsEmpty())
	{
		ResolutionsFullscreen.Emplace(ResolutionsWindowedFullscreen[0]);
	}
}

// 根据当前窗口模式返回对应的分辨率列表视图。
TArrayView<const TSharedPtr<ULyraSettingValueDiscrete_Resolution::FScreenResolutionEntry>> ULyraSettingValueDiscrete_Resolution::GetSelectedResolutionList() const
{
	TArrayView<const TSharedPtr<ULyraSettingValueDiscrete_Resolution::FScreenResolutionEntry>> Result;

	EWindowMode::Type const WindowMode = GEngine->GetGameUserSettings()->GetFullscreenMode();
	switch (WindowMode)
	{
	case EWindowMode::Windowed:
		Result = MakeArrayView(ResolutionsWindowed);
		break;
	case EWindowMode::WindowedFullscreen:
		Result = MakeArrayView(ResolutionsWindowedFullscreen);
		break;
	case EWindowMode::Fullscreen:
		Result = MakeArrayView(ResolutionsFullscreen);
		break;
	}

	return Result;
}

// 按本项目规则过滤可能导致 UI 或测试问题的异常全屏分辨率。
// @param ScreenRes 待检查的 RHI 分辨率及刷新率信息。
// @param FilterThreshold 取 0、1、2；数值越大条件越宽松，用于保证至少产生若干分辨率选项。
// To filter out odd resolution so UI and testing has less issues. This is game specific.
// @param ScreenRes resolution and
// @param FilterThreshold 0/1/2 to make sure we get at least some resolutions (might be an issues with UI but at least we get some resolution entries)
bool ULyraSettingValueDiscrete_Resolution::ShouldAllowFullScreenResolution(const FScreenResolutionRHI& SrcScreenRes, int32 FilterThreshold) const
{
	FScreenResolutionRHI ScreenRes = SrcScreenRes;

	// 常见宽高比约为：4:3=1.333、16:9=1.777、16:10=1.6；多显示器超宽比例通常大于 2。
	// expected: 4:3=1.333, 16:9=1.777, 16:10=1.6, multi-monitor-wide: >2
	bool bIsPortrait = ScreenRes.Width < ScreenRes.Height;
	float AspectRatio = (float)ScreenRes.Width / (float)ScreenRes.Height;

	// 纵向分辨率先交换为横向尺寸再检查，后续筛选逻辑无需为纵向模式单独分支。
	// If portrait, flip values back to landscape so we can don't have to special case all the tests below
	if (bIsPortrait)
	{
		AspectRatio = 1.0f / AspectRatio;
		ScreenRes.Width = SrcScreenRes.Height;
		ScreenRes.Height = SrcScreenRes.Width;
	}

	// 在严格筛选阶段排除与当前显示器原生宽高比不匹配的分辨率。
	// TODO：若允许用户指定全屏显示器，便可在多台显示器规格不同时准确筛选。当前游戏可能根据窗口重叠面积等因素切换目标显示器，
	// 因而这里仍有可能选中目标显示器不支持的分辨率。
	// Filter out resolutions that don't match the native aspect ratio of the current monitor
	// TODO: Other games allow the user to choose which monitor the games goes fullscreen on. This would allow
	// this filtering to be correct when the users monitors are of different types! ATM, the game can change
	// which monitor it uses based on other factors (max window overlap etc.) so we could end up choosing a
	// resolution which the target monitor doesn't support.
	if (FilterThreshold < 1)
	{
		// 默认令显示器宽高比等于候选分辨率；平台无法提供显示器信息时会接受该分辨率，不做误判过滤。
		// Default display aspect to required aspect in case this platform can't provide the information. Forces acceptance of this resolution.
		float DisplayAspect = AspectRatio;

		// 某些平台无法检测显示设备原生分辨率，此时不按宽高比过滤。
		// Some platforms might not be able to detect the native resolution of the display device, so don't filter in that case
		const FMonitorInfo* const Monitor = GetCurrentMonitor();
		if (Monitor)
		{
			DisplayAspect = (float)Monitor->NativeWidth / (float)Monitor->NativeHeight;
		}
		else
		{
			for (int32 MonitorIndex = 0; MonitorIndex < CurrentDisplayMetrics.MonitorInfo.Num(); ++MonitorIndex)
			{
				const FMonitorInfo& MonitorInfo = CurrentDisplayMetrics.MonitorInfo[MonitorIndex];

				if (MonitorInfo.bIsPrimary)
				{
					DisplayAspect = (float)MonitorInfo.NativeWidth / (float)MonitorInfo.NativeHeight;
					break;
				}
			}
		}

		// 候选宽高比与显示器原生宽高比不近似相等时拒绝该分辨率。
		// If aspects are not almost exactly equal, reject
		if (FMath::Abs(DisplayAspect - AspectRatio) > KINDA_SMALL_NUMBER)
		{
			return false;
		}
	}

	// FilterThreshold 越大，跳过的限制越多，筛选条件越宽松。
	// more relaxed tests have a larger FilterThreshold

	// 在最宽松层级之前，最低分辨率限制为 1280x720。
	// minimum is 1280x720
	if (FilterThreshold < 2 && (ScreenRes.Width < 1280 || ScreenRes.Height < 720))
	{
		// 过滤低于最低尺寸的分辨率。
		// filter resolutions that are too small
		return false;
	}

	return true;
}

// 返回与指定分辨率完全匹配的选项索引；未找到时返回 INDEX_NONE。
int32 ULyraSettingValueDiscrete_Resolution::FindIndexOfDisplayResolution(const FIntPoint& InPoint) const
{
	TArrayView<const TSharedPtr<ULyraSettingValueDiscrete_Resolution::FScreenResolutionEntry>> Resolutions = GetSelectedResolutionList();
	for (int32 i = 0, Num = Resolutions.Num(); i < Num; ++i)
	{
		if (Resolutions[i]->GetResolution() == InPoint)
		{
			return i;
		}
	}

	if (!Resolutions.IsEmpty())
	{
		return Resolutions.Num() - 1;
	}

	return INDEX_NONE;
}

// 按宽高距离查找最接近目标值的分辨率选项索引。
int32 ULyraSettingValueDiscrete_Resolution::FindClosestResolutionIndex(const FIntPoint& Resolution) const
{	
	TArrayView<const TSharedPtr<ULyraSettingValueDiscrete_Resolution::FScreenResolutionEntry>> Resolutions = GetSelectedResolutionList();
	int32 Index = 0;
	int32 LastDiff = Resolution.SizeSquared();

	for (int32 i = 0, Num = Resolutions.Num(); i < Num; ++i)
	{
		// 使用分辨率向量长度的平方近似比较总尺寸，避免开方运算。
		// We compare the squared diagonals
		int32 Diff = FMath::Abs(Resolution.SizeSquared() - Resolutions[i]->GetResolution().SizeSquared());
		if (Diff <= LastDiff)
		{				
			Index = i;
		}
		LastDiff = Diff;
	}

	return Index;
}

// 在尺寸和宽高比约束内生成常用窗口分辨率，并写入输出数组。
void ULyraSettingValueDiscrete_Resolution::GetStandardWindowResolutions(const FIntPoint& MinResolution, const FIntPoint& MaxResolution, float MinAspectRatio, TArray<FIntPoint>& OutResolutions)
{
	static TArray<FIntPoint> StandardResolutions;
	if (StandardResolutions.Num() == 0)
	{
		// 以下为 Wikipedia 图形显示分辨率条目中列出的标准分辨率。
		// Standard resolutions as provided by Wikipedia (http://en.wikipedia.org/wiki/Graphics_display_resolution)

		// 扩展图形阵列（XGA）系列。
		// Extended Graphics Array
		{
			new(StandardResolutions) FIntPoint(1024, 768); // XGA

														   /* WXGA 的三个常见变体。 */ // WXGA (3 versions)
			new(StandardResolutions) FIntPoint(1366, 768); // FWXGA
			new(StandardResolutions) FIntPoint(1360, 768);
			new(StandardResolutions) FIntPoint(1280, 800);

			new(StandardResolutions) FIntPoint(1152, 864); // XGA+
			new(StandardResolutions) FIntPoint(1440, 900); // WXGA+
			new(StandardResolutions) FIntPoint(1280, 1024); // SXGA
			new(StandardResolutions) FIntPoint(1400, 1050); // SXGA+
			new(StandardResolutions) FIntPoint(1680, 1050); // WSXGA+
			new(StandardResolutions) FIntPoint(1600, 1200); // UXGA
			new(StandardResolutions) FIntPoint(1920, 1200); // WUXGA
		}

		// 四倍扩展图形阵列（QXGA）系列。
		// Quad Extended Graphics Array
		{
			new(StandardResolutions) FIntPoint(2048, 1152); // QWXGA
			new(StandardResolutions) FIntPoint(2048, 1536); // QXGA
			new(StandardResolutions) FIntPoint(2560, 1600); // WQXGA
			new(StandardResolutions) FIntPoint(2560, 2048); // QSXGA
			new(StandardResolutions) FIntPoint(3200, 2048); // WQSXGA
			new(StandardResolutions) FIntPoint(3200, 2400); // QUXGA
			new(StandardResolutions) FIntPoint(3840, 2400); // WQUXGA
		}

		// 超扩展图形阵列（HXGA）系列。
		// Hyper Extended Graphics Array
		{
			new(StandardResolutions) FIntPoint(4096, 3072); // HXGA
			new(StandardResolutions) FIntPoint(5120, 3200); // WHXGA
			new(StandardResolutions) FIntPoint(5120, 4096); // HSXGA
			new(StandardResolutions) FIntPoint(6400, 4096); // WHSXGA
			new(StandardResolutions) FIntPoint(6400, 4800); // HUXGA
			new(StandardResolutions) FIntPoint(7680, 4800); // WHUXGA
		}

		// 高清（HD）系列。
		// High-Definition
		{
			new(StandardResolutions) FIntPoint(640, 360); // nHD
			new(StandardResolutions) FIntPoint(960, 540); // qHD
			new(StandardResolutions) FIntPoint(1280, 720); // HD
			new(StandardResolutions) FIntPoint(1920, 1080); // FHD
			new(StandardResolutions) FIntPoint(2560, 1440); // QHD
			new(StandardResolutions) FIntPoint(3200, 1800); // WQXGA+
			new(StandardResolutions) FIntPoint(3840, 2160); // UHD 4K
			new(StandardResolutions) FIntPoint(4096, 2160); // Digital Cinema Initiatives 4K
			new(StandardResolutions) FIntPoint(7680, 4320); // FUHD
			new(StandardResolutions) FIntPoint(5120, 2160); // UHD 5K
			new(StandardResolutions) FIntPoint(5120, 2880); // UHD+
			new(StandardResolutions) FIntPoint(15360, 8640); // QUHD
		}

		// 按总像素数从小到大排列标准分辨率。
		// Sort the list by total resolution size
		StandardResolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return (A.X * A.Y) < (B.X * B.Y); });
	}

	// 返回尺寸处于给定上下限内且宽高比不低于要求的所有标准分辨率。
	// Return all standard resolutions that are within the size constraints
	for (const auto& Resolution : StandardResolutions)
	{
		if (Resolution.X >= MinResolution.X && Resolution.Y >= MinResolution.Y && Resolution.X <= MaxResolution.X && Resolution.Y <= MaxResolution.Y)
		{
			const float AspectRatio = Resolution.X / (float)Resolution.Y;
			if (AspectRatio > MinAspectRatio || FMath::IsNearlyEqual(AspectRatio, MinAspectRatio))
			{
				OutResolutions.Add(Resolution);
			}
		}
	}
}

// 返回分辨率条目的覆盖文本；未提供时按宽、高及可选刷新率生成显示文本。
FText ULyraSettingValueDiscrete_Resolution::FScreenResolutionEntry::GetDisplayText() const
{
	if (!OverrideText.IsEmpty())
	{
		return OverrideText;
	}

	FText Aspect = FText::GetEmpty();

	// 常见宽高比约为：4:3=1.333、16:9=1.777、16:10=1.6；多显示器超宽比例通常大于 2。
	// expected: 4:3=1.333, 16:9=1.777, 16:10=1.6, multi-monitor-wide: >2
	float AspectRatio = (float)Width / (float)Height;

	if (FMath::Abs(AspectRatio - (4.0f / 3.0f)) < KINDA_SMALL_NUMBER)
	{
		Aspect = LOCTEXT("AspectRatio-4:3", "4:3");
	}
	else if (FMath::Abs(AspectRatio - (16.0f / 9.0f)) < KINDA_SMALL_NUMBER)
	{
		Aspect = LOCTEXT("AspectRatio-16:9", "16:9");
	}
	else if (FMath::Abs(AspectRatio - (16.0f / 10.0f)) < KINDA_SMALL_NUMBER)
	{
		Aspect = LOCTEXT("AspectRatio-16:10", "16:10");
	}
	else if (FMath::Abs(AspectRatio - (3.0f / 4.0f)) < KINDA_SMALL_NUMBER)
	{
		Aspect = LOCTEXT("AspectRatio-3:4", "3:4");
	}
	else if (FMath::Abs(AspectRatio - (9.0f / 16.0f)) < KINDA_SMALL_NUMBER)
	{
		Aspect = LOCTEXT("AspectRatio-9:16", "9:16");
	}
	else if (FMath::Abs(AspectRatio - (10.0f / 16.0f)) < KINDA_SMALL_NUMBER)
	{
		Aspect = LOCTEXT("AspectRatio-10:16", "10:16");
	}

	FNumberFormattingOptions Options;
	Options.UseGrouping = false;

	FFormatNamedArguments Args;
	Args.Add(TEXT("X"), FText::AsNumber(Width, &Options));
	Args.Add(TEXT("Y"), FText::AsNumber(Height, &Options));
	Args.Add(TEXT("AspectRatio"), Aspect);
	Args.Add(TEXT("RefreshRate"), RefreshRate);

	return FText::Format(LOCTEXT("AspectRatio", "{X} x {Y}"), Args);
}

#undef LOCTEXT_NAMESPACE
