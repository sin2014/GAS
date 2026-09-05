// Copyright Epic Games, Inc. All Rights Reserved.

#include "ApplyFrontendPerfSettingsAction.h"

#include "Settings/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ApplyFrontendPerfSettingsAction)

struct FGameFeatureActivatingContext;
struct FGameFeatureDeactivatingContext;

//////////////////////////////////////////////////////////////////////
// UApplyFrontendPerfSettingsAction

// 用户设置及其驱动的引擎性能和可伸缩性状态是进程全局的，因此多玩家 PIE 不按 World 分别维护；
// 只要任一 PIE World 处于前端菜单，就通过引用计数启用前端性能策略。
// 编辑器默认仍不会真正应用前端性能选项，除非开启 ApplyFrontEndPerformanceOptionsInPIE。
// Game user settings (and engine performance/scalability settings they drive)
// are global, so there's no point in tracking this per world for multi-player PIE:
// we just apply it if any PIE world is in the menu.
//
// However, by default we won't apply front-end performance stuff in the editor
// unless the developer setting ApplyFrontEndPerformanceOptionsInPIE is enabled
int32 UApplyFrontendPerfSettingsAction::ApplicationCounter = 0;

// 增加前端性能策略的全局引用计数，并在首个功能激活时启用前端帧率限制。
void UApplyFrontendPerfSettingsAction::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	ApplicationCounter++;
	if (ApplicationCounter == 1)
	{
		ULyraSettingsLocal::Get()->SetShouldUseFrontendPerformanceSettings(true);
	}
}

// 减少全局引用计数，并在最后一个功能停用时关闭前端性能策略；计数下溢会触发检查。
void UApplyFrontendPerfSettingsAction::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	ApplicationCounter--;
	check(ApplicationCounter >= 0);

	if (ApplicationCounter == 0)
	{
		ULyraSettingsLocal::Get()->SetShouldUseFrontendPerformanceSettings(false);
	}
}

