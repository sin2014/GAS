// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class LyraEditorTarget : TargetRules
{
	public LyraEditorTarget(TargetInfo Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V7;

		Type = TargetType.Editor;
		ExtraModuleNames.AddRange(new string[] { "LyraGame", "LyraEditor" });

		if (!bBuildAllModules)
		{
			NativePointerMemberBehaviorOverride = PointerMemberBehavior.Disallow;
		}

		LyraGameTarget.ApplySharedLyraTargetSettings(this);

		// 启用 RemoteSession，配合 Unreal Remote 2 在编辑器中测试触屏输入。
		// This is used for touch screen development along with the "Unreal Remote 2" app
		EnablePlugins.Add("RemoteSession");
	}
}
