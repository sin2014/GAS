// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
using EpicGames.Core;
using System.Collections.Generic;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

public class LyraGameTarget : TargetRules
{
	public LyraGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;

		ExtraModuleNames.AddRange(new string[] { "LyraGame" });

		LyraGameTarget.ApplySharedLyraTargetSettings(this);
	}

	private static bool bHasWarnedAboutShared = false;

	internal static void ApplySharedLyraTargetSettings(TargetRules Target)
	{
		ILogger Logger = Target.Logger;
		
		Target.DefaultBuildSettings = BuildSettingsVersion.V7;
		Target.IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		bool bIsTest = Target.Configuration == UnrealTargetConfiguration.Test;
		bool bIsShipping = Target.Configuration == UnrealTargetConfiguration.Shipping;
		bool bIsDedicatedServer = Target.Type == TargetType.Server;
		if (Target.BuildEnvironment == TargetBuildEnvironment.Unique)
		{
			Target.CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Error;

			Target.bUseLoggingInShipping = true;
			Target.bTrackRHIResourceInfoForTest = true;

			if (bIsShipping && !bIsDedicatedServer)
			{
				// Shipping 客户端拒绝未验证证书，确保 HTTPS 流量执行证书校验。
				// Make sure that we validate certificates for HTTPS traffic
				Target.bDisableUnverifiedCertificates = true;

				// 可启用命令行参数白名单，使 Shipping 仅解析明确允许的启动参数。
				// Uncomment these lines to lock down the command line processing
				// This will only allow the specified command line arguments to be parsed
				//Target.GlobalDefinitions.Add("UE_COMMAND_LINE_USES_ALLOW_LIST=1");
				//Target.GlobalDefinitions.Add("UE_OVERRIDE_COMMAND_LINE_ALLOW_LIST=\"-space -separated -list -of -commands\"");

				// 可过滤连接标识等敏感命令行参数，避免上传日志时泄露。
				// Uncomment this line to filter out sensitive command line arguments that you
				// don't want to go into the log file (e.g., if you were uploading logs)
				//Target.GlobalDefinitions.Add("FILTER_COMMANDLINE_LOGGING=\"-some_connection_id -some_other_arg\"");
			}

			if (bIsShipping || bIsTest)
			{
				// Test/Shipping 的 Cooked 构建禁止读取生成或 Non-UFS ini，避免外部配置覆盖发布设置。
				// Disable reading generated/non-ufs ini files
				Target.bAllowGeneratedIniWhenCooked = false;
				Target.bAllowNonUFSIniWhenCooked = false;
			}

			if (Target.Type != TargetType.Editor)
			{
				// 运行时不使用 Path Tracer，非编辑器 Target 禁用体积较大的 OpenImageDenoise 插件。
				// We don't use the path tracer at runtime, only for beauty shots, and this DLL is quite large
				Target.DisablePlugins.Add("OpenImageDenoise");

				// 使用间接 AssetData 指针降低 AssetRegistry 常驻内存，代价是查询增加 CPU 开销。
				// Reduce memory use in AssetRegistry always-loaded data, but add more cputime expensive queries
				Target.GlobalDefinitions.Add("UE_ASSETREGISTRY_INDIRECT_ASSETDATA_POINTERS=1");
			}

			LyraGameTarget.ConfigureGameFeaturePlugins(Target);
		}
		else
		{
			// Shared 构建环境会复用预编译引擎产物；此分支的设置不得影响 PCH，
			// 否则必须把 Target 切换为 TargetBuildEnvironment.Unique。
			// !!!!!!!!!!!! WARNING !!!!!!!!!!!!!
			// Any changes in here must not affect PCH generation, or the target
			// needs to be set to TargetBuildEnvironment.Unique

			// GameFeature 插件动态启停只允许在 Editor 或 Unique 构建环境配置。
			// This only works in editor or Unique build environments
			if (Target.Type == TargetType.Editor)
			{
				LyraGameTarget.ConfigureGameFeaturePlugins(Target);
			}
			else
			{
				// Shared Monolithic 构建复用已安装引擎二进制，不能动态启停插件或改变编译选项。
				// Shared monolithic builds cannot enable/disable plugins or change any options because it tries to re-use the installed engine binaries
				if (!bHasWarnedAboutShared)
				{
					bHasWarnedAboutShared = true;
					Logger.LogWarning("LyraGameEOS and dynamic target options are disabled when packaging from an installed version of the engine");
				}
			}
		}
	}

	static public bool ShouldEnableAllGameFeaturePlugins(TargetRules Target)
	{
		if (Target.Type == TargetType.Editor)
		{
			// 若返回 true，Editor 会编译所有 GameFeature 插件但不保证运行时加载，
			// 从而允许在编辑器中启用插件时无需临时重新编译代码。
			// With return true, editor builds will build all game feature plugins, but it may or may not load them all.
			// This is so you can enable plugins in the editor without needing to compile code.
			// return true;
		}

		bool bIsBuildMachine = Target.AdditionalProperties.GetProperty("IsBuildMachine") == "1";
		if (bIsBuildMachine)
		{
			// 构建机可选择在此强制编译全部 GameFeature 插件。
			// This could be used to enable all plugins for build machines
			// return true;
		}

		// 默认遵循编辑器 Plugin Browser 写入的插件启用规则；Launcher 安装版引擎可能完全不执行本配置代码。
		// By default use the default plugin rules as set by the plugin browser in the editor
		// This is important because this code may not be run at all for launcher-installed versions of the engine
		return false;
	}

	private static Dictionary<string, JsonObject> AllPluginRootJsonObjectsByName = new Dictionary<string, JsonObject>();

	// 根据插件描述符、Target 类型和当前分支决定 GameFeature 插件是启用、强制禁用还是保持默认。
	// 项目可扩展为按发布版本选择插件，例如主分支编译开发中特性，而发布分支禁用。
	// Configures which game feature plugins we want to have enabled
	// This is a fairly simple implementation, but you might do things like build different
	// plugins based on the target release version of the current branch, e.g., enabling 
	// work-in-progress features in main but disabling them in the current release branch.
	static public void ConfigureGameFeaturePlugins(TargetRules Target)
	{
		ILogger Logger = Target.Logger;
		Log.TraceInformationOnce("Compiling GameFeaturePlugins in branch {0}", Target.Version.BranchName);

		bool bBuildAllGameFeaturePlugins = ShouldEnableAllGameFeaturePlugins(Target);

		// 枚举项目及平台扩展目录下的全部 GameFeature `.uplugin` 描述符。
		// Load all of the game feature .uplugin descriptors
		List<FileReference> CombinedPluginList = new List<FileReference>();

		List<DirectoryReference> GameFeaturePluginRoots = Unreal.GetExtensionDirs(Target.ProjectFile.Directory, Path.Combine("Plugins", "GameFeatures"));
		foreach (DirectoryReference SearchDir in GameFeaturePluginRoots)
		{
			CombinedPluginList.AddRange(PluginsBase.EnumeratePlugins(SearchDir));
		}

		if (CombinedPluginList.Count > 0)
		{
			Dictionary<string, List<string>> AllPluginReferencesByName = new Dictionary<string, List<string>>();

			foreach (FileReference PluginFile in CombinedPluginList)
			{
				if (PluginFile != null && FileReference.Exists(PluginFile))
				{
					bool bEnabled = false;
					bool bForceDisabled = false;
					try
					{
						JsonObject RawObject;
						lock (AllPluginRootJsonObjectsByName)
						{
							if (!AllPluginRootJsonObjectsByName.TryGetValue(PluginFile.GetFileNameWithoutExtension(), out RawObject))
							{
								RawObject = JsonObject.Read(PluginFile);
								AllPluginRootJsonObjectsByName.Add(PluginFile.GetFileNameWithoutExtension(), RawObject);
							}
						}

						// 内置 GameFeature 应显式设置 EnabledByDefault=false；否则即使构建时禁用，插件名称仍会嵌入可执行文件。
						// 如需强制此约束，可恢复警告并修改编辑器插件模板的默认值。
						// Validate that all GameFeaturePlugins are disabled by default
						// If EnabledByDefault is true and a plugin is disabled the name will be embedded in the executable
						// If this is a problem, enable this warning and change the game feature editor plugin templates to disable EnabledByDefault for new plugins
						bool bEnabledByDefault = false;
						if (!RawObject.TryGetBoolField("EnabledByDefault", out bEnabledByDefault) || bEnabledByDefault == true)
						{
							//Log.TraceWarning("GameFeaturePlugin {0}, does not set EnabledByDefault to false. This is required for built-in GameFeaturePlugins.", PluginFile.GetFileNameWithoutExtension());
						}

						// GameFeature 插件必须设置 ExplicitlyLoaded=true，因为其生命周期由项目启动后的 GameFeature 系统控制。
						// Validate that all GameFeaturePlugins are set to explicitly loaded
						// This is important because game feature plugins expect to be loaded after project startup
						bool bExplicitlyLoaded = false;
						if (!RawObject.TryGetBoolField("ExplicitlyLoaded", out bExplicitlyLoaded) || bExplicitlyLoaded == false)
						{
							Logger.LogWarning("GameFeaturePlugin {0}, does not set ExplicitlyLoaded to true. This is required for GameFeaturePlugins.", PluginFile.GetFileNameWithoutExtension());
						}

						// You could read an additional field here that is project specific, e.g.,
						//string PluginReleaseVersion;
						//if (RawObject.TryGetStringField("MyProjectReleaseVersion", out PluginReleaseVersion))
						//{
						//		bEnabled = SomeFunctionOf(PluginReleaseVersion, CurrentReleaseVersion) || bBuildAllGameFeaturePlugins;
						//}

						if (bBuildAllGameFeaturePlugins)
						{
							// 全量构建模式先启用所有可编译插件，后续强制禁用规则仍可覆盖该决定。
							// We are in a mode where we want all game feature plugins, except ones we can't load or compile
							bEnabled = true;
						}

						// 非 Editor Target 强制禁用标记为 EditorOnly 的 GameFeature 插件。
						// Prevent using editor-only feature plugins in non-editor builds
						bool bEditorOnly = false;
						if (RawObject.TryGetBoolField("EditorOnly", out bEditorOnly))
						{
							if (bEditorOnly && (Target.Type != TargetType.Editor) && !bBuildAllGameFeaturePlugins)
							{
								// 当前为非 Editor Target，因此强制禁用该 EditorOnly 插件。
								// The plugin is editor only and we are building a non-editor target, so it is disabled
								bForceDisabled = true;
							}
						}
						else
						{
							// EditorOnly 字段可省略，省略时按普通运行时插件处理。
							// EditorOnly is optional
						}

						// RestrictToBranch 可将插件限制为只在指定源码分支构建。
						// some plugins should only be available in certain branches
						string RestrictToBranch;
						if (RawObject.TryGetStringField("RestrictToBranch", out RestrictToBranch))
						{
							if (!Target.Version.BranchName.Equals(RestrictToBranch, StringComparison.OrdinalIgnoreCase))
							{
								// 当前分支不匹配 RestrictToBranch，强制禁用该插件。
								// The plugin is for a specific branch, and this isn't it
								bForceDisabled = true;
								Logger.LogDebug("GameFeaturePlugin {Name} was marked as restricted to other branches. Disabling.", PluginFile.GetFileNameWithoutExtension());
							}
							else
							{
								Logger.LogDebug("GameFeaturePlugin {Name} was marked as restricted to this branch. Leaving enabled.", PluginFile.GetFileNameWithoutExtension());
							}
						}

						// NeverBuild 具有最高优先级，会覆盖之前的启用决定。
						// Plugins can be marked as NeverBuild which overrides the above
						bool bNeverBuild = false;
						if (RawObject.TryGetBoolField("NeverBuild", out bNeverBuild) && bNeverBuild)
						{
							// 插件标记为 NeverBuild，任何 Target 都不得编译。
							// This plugin was marked to never compile, so don't
							bForceDisabled = true;
							Logger.LogDebug("GameFeaturePlugin {Name} was marked as NeverBuild, disabling.", PluginFile.GetFileNameWithoutExtension());
						}

						// 记录插件依赖关系，供后续验证被启用插件是否引用了被禁用插件。
						// Keep track of plugin references for validation later
						JsonObject[] PluginReferencesArray;
						if (RawObject.TryGetObjectArrayField("Plugins", out PluginReferencesArray))
						{
							foreach (JsonObject ReferenceObject in PluginReferencesArray)
							{
								bool bRefEnabled = false;
								if (ReferenceObject.TryGetBoolField("Enabled", out bRefEnabled) && bRefEnabled == true)
								{
									string PluginReferenceName;
									if (ReferenceObject.TryGetStringField("Name", out PluginReferenceName))
									{
										string ReferencerName = PluginFile.GetFileNameWithoutExtension();
										if (!AllPluginReferencesByName.ContainsKey(ReferencerName))
										{
											AllPluginReferencesByName[ReferencerName] = new List<string>();
										}
										AllPluginReferencesByName[ReferencerName].Add(PluginReferenceName);
									}
								}
							}
						}
					}
					catch (Exception ParseException)
					{
						Logger.LogWarning("Failed to parse GameFeaturePlugin file {Name}, disabling. Exception: {1}", PluginFile.GetFileNameWithoutExtension(), ParseException.Message);
						bForceDisabled = true;
					}

					// 强制禁用优先于任何启用规则。
					// Disabled has priority over enabled
					if (bForceDisabled)
					{
						bEnabled = false;
					}

					// 输出该插件最终的启用、禁用或忽略决定。
					// Print out the final decision for this plugin
					Logger.LogDebug("ConfigureGameFeaturePlugins() has decided to {Action} feature {Name}", bEnabled ? "enable" : (bForceDisabled ? "disable" : "ignore"), PluginFile.GetFileNameWithoutExtension());

					// 将最终决定写入 Target 的 EnablePlugins 或 DisablePlugins。
					// Enable or disable it
					if (bEnabled)
					{
						Target.EnablePlugins.Add(PluginFile.GetFileNameWithoutExtension());
					}
					else if (bForceDisabled)
					{
						Target.DisablePlugins.Add(PluginFile.GetFileNameWithoutExtension());
					}
				}
			}

			// 若引入发布版本字段，应验证早期版本插件不会依赖更晚版本才发布的内容。
			// If you use something like a release version, consider doing a reference validation to make sure
			// that plugins with sooner release versions don't depend on content with later release versions
		}
	}
}
