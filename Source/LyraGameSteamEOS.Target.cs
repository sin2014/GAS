// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

// SteamEOS 表示通过 Steam 发布和启动，同时使用 EOS 提供在线服务与跨平台联机的集成平台 Target。
// SteamEOS refers to a game published and launched on Steam while still taking advantage of EOS for online and crossplay (integrated platform).

public class LyraGameSteamEOSTarget : LyraGameTarget
{
	public LyraGameSteamEOSTarget(TargetInfo Target) : base(Target)
	{
		CustomConfig = "SteamEOS";

		EnablePlugins.AddRange(
			new string[]
			{
				"OnlineServicesEOS",
				"OnlineSubsystemEOS"
			}
		);

		OptionalPlugins.Add("EOSReservedHooks");
	}
}
