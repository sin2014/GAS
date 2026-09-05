// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "LyraActorUtilities.generated.h"

class UObject;
struct FFrame;

UENUM()
enum class EBlueprintExposedNetMode : uint8
{
	/** 单机模式：无网络连接，可有一个或多个本地玩家；因具备完整服务器功能，仍视为服务器。 */
	/** Standalone: a game without networking, with one or more local players. Still considered a server because it has all server functionality. */
	Standalone,

	/** 专用服务器：没有本地玩家的服务器。 */
	/** Dedicated server: server with no local players. */
	DedicatedServer,

	/** 监听服务器：由本地玩家托管、同时允许网络玩家连接的服务器。 */
	/** Listen server: a server that also has a local player who is hosting the game, available to other players on the network. */
	ListenServer,

	/**
	 * 网络客户端：连接到远程服务器的客户端。
	 * 枚举值小于 Client 的模式均属于某种服务器，因此 NetMode < Client 可用于统一判断服务器端。
	 */
	/**
	 * Network client: client connected to a remote server.
	 * Note that every mode less than this value is a kind of server, so checking NetMode < NM_Client is always some variety of server.
	 */
	Client
};


UCLASS()
class ULyraActorUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 获取 Actor、Component 或 WorldContextObject 所在 World 的网络模式。
	 */
	/**
	 * Get the network mode (dedicated server, client, standalone, etc...) for an actor or component.
	 */
	UFUNCTION(BlueprintCallable, Category="Lyra", meta=(WorldContext="WorldContextObject", ExpandEnumAsExecs=ReturnValue))
	static EBlueprintExposedNetMode SwitchOnNetMode(const UObject* WorldContextObject);
};
