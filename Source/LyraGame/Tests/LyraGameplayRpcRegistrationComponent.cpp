// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameplayRpcRegistrationComponent.h"
#include "Player/LyraPlayerController.h"
#include "Character/LyraPawn.h"
#include "Player/LyraPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "EngineMinimal.h"

#include "Character/LyraHealthComponent.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryItemInstance.h"
#include "Inventory/LyraInventoryManagerComponent.h"
#include "Character/LyraPawnExtensionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameplayRpcRegistrationComponent)

// External RPC 组件的懒创建全局实例；未编译 RPC Registry 的构建始终保持 nullptr。
ULyraGameplayRpcRegistrationComponent* ULyraGameplayRpcRegistrationComponent::ObjectInstance = nullptr;
// RPC Registry 编译启用时延迟创建并 Root 全局注册组件；Shipping 等关闭注册表的构建返回 nullptr。
ULyraGameplayRpcRegistrationComponent* ULyraGameplayRpcRegistrationComponent::GetInstance()
{
#if WITH_RPC_REGISTRY
	if (ObjectInstance == nullptr)
	{
		ObjectInstance = NewObject<ULyraGameplayRpcRegistrationComponent>();
		if (!UExternalRpcRegistry::GetInstance())
		{
			GLog->Log(TEXT("BotRPC"), ELogVerbosity::Warning, FString::Printf(TEXT("Unable to create RPC Registry Instance. This might lead to issues using the RPC Registry.")));
		}
		ObjectInstance->AddToRoot();
	}
#endif
	return ObjectInstance;
}

// 优先返回 GameViewport 所属 GameInstance 的 World；没有视口时回退到 GWorld。
UWorld* FindGameWorld()
{
	// 从 GameViewport 取得当前游戏 World，避免使用编辑器或其他上下文 World。
	//Find Game World
	if (GEngine->GameViewport)
	{
		UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
		return GameInstance ? GameInstance->GetWorld() : nullptr;
	}
	return GWorld;
}

// 从测试 GameWorld 取得首个 LyraPlayerController；World 不存在或 Controller 类型不匹配时返回 nullptr。
ALyraPlayerController* GetPlayerController()
{
	UWorld* LocalWorld = FindGameWorld();
	if (!LocalWorld)
	{
		return nullptr;
	}
	// 取得第一个本地 LyraPlayerController 作为测试命令执行目标。
	//Find PlayerController
	ALyraPlayerController* PlayerController = Cast<ALyraPlayerController>(LocalWorld->GetFirstPlayerController());
	if (!PlayerController)
	{
		return nullptr;
	}
	else
	{
		return PlayerController;
	}
}

#if WITH_RPC_REGISTRY

// 把 UTF-8 请求体反序列化为 JSON 对象；空请求或解析失败时返回空对象指针。
TSharedPtr<FJsonObject> ULyraGameplayRpcRegistrationComponent::GetJsonObjectFromRequestBody(TArray<uint8> InRequestBody)
{
	FUTF8ToTCHAR WByteBuffer(reinterpret_cast<const ANSICHAR*>(InRequestBody.GetData()), InRequestBody.Num());
	const FString IncomingRequestBody =  FString::ConstructFromPtrSize(WByteBuffer.Get(), WByteBuffer.Length());
	TSharedPtr<FJsonObject> BodyObject = MakeShareable(new FJsonObject());
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(IncomingRequestBody);

	if (FJsonSerializer::Deserialize(JsonReader, BodyObject) && BodyObject.IsValid())
	{
		return BodyObject;
	}

	return nullptr;
}


// 注册任意游戏状态都可用的测试 HTTP 路由，包括执行作弊命令。
void ULyraGameplayRpcRegistrationComponent::RegisterAlwaysOnHttpCallbacks()
{
	Super::RegisterAlwaysOnHttpCallbacks();	
	const FExternalRpcArgumentDesc CommandDesc(TEXT("command"), TEXT("string"), TEXT("The command to tell the executable to run."));

	RegisterHttpCallback(FName(TEXT("CheatCommand")),
		FHttpPath("/core/cheatcommand"),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &ThisClass::HttpExecuteCheatCommand),
		true,
		TEXT("Cheats"),
		TEXT("raw"),
		{ CommandDesc });
}

// 注册比赛期间可用的开火与玩家生命查询路由。
void ULyraGameplayRpcRegistrationComponent::RegisterInMatchHttpCallbacks()
{
	RegisterHttpCallback(FName(TEXT("GetPlayerStatus")),
		FHttpPath("/player/status"),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateUObject(this, &ThisClass::HttpGetPlayerVitalsCommand),
		true);

	RegisterHttpCallback(FName(TEXT("PlayerFireOnce")),
		FHttpPath("/player/status"),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &ThisClass::HttpFireOnceCommand),
		true);
	
}

// 为前端状态预留匹配相关 HTTP 路由，当前不注册具体端点。
void ULyraGameplayRpcRegistrationComponent::RegisterFrontendHttpCallbacks()
{
	// TODO：在此注册仅前端可用的匹配相关 RPC。
	// TODO: Add Matchmaking RPCs here
}


// 调用 ExternalRpcRegistrationComponent 的统一注销流程，移除本组件注册的 HTTP 路由。
void ULyraGameplayRpcRegistrationComponent::DeregisterHttpCallbacks()
{
	Super::DeregisterHttpCallbacks();
}

// 解析 JSON 中的 cheat 字符串，在 GameViewport World 的首个 LyraPlayerController 上执行，并返回成功或参数错误响应。
bool ULyraGameplayRpcRegistrationComponent::HttpExecuteCheatCommand(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TSharedPtr<FJsonObject> BodyObject = GetJsonObjectFromRequestBody(Request.Body);

	if (!BodyObject.IsValid())
	{
		TUniquePtr<FHttpServerResponse>Response = CreateSimpleResponse(false, TEXT("Invalid body object"));
		OnComplete(MoveTemp(Response));
		return true;
	}
	if (BodyObject->GetStringField(TEXT("command")).IsEmpty())
	{
		TUniquePtr<FHttpServerResponse>Response = CreateSimpleResponse(false, TEXT("command not found in json body"));
		OnComplete(MoveTemp(Response));
		return true;
	}
	ALyraPlayerController* LPC = GetPlayerController();
	if (!LPC)
	{
		TUniquePtr<FHttpServerResponse>Response = CreateSimpleResponse(false, TEXT("player controller not found"));
		OnComplete(MoveTemp(Response));
		return true;
	}
	FString CheatCommand = FString::Printf(TEXT("%s"), *BodyObject->GetStringField(TEXT("command")));
	LPC->ConsoleCommand(*CheatCommand, true);

	TUniquePtr<FHttpServerResponse>Response = CreateSimpleResponse(true);
	OnComplete(MoveTemp(Response));
	return true;
}

// 返回一次开火请求的占位成功响应，实际输入触发尚未实现。
bool ULyraGameplayRpcRegistrationComponent::HttpFireOnceCommand(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	ALyraPlayerController* LPC = GetPlayerController();
	if (!LPC)
	{
		TUniquePtr<FHttpServerResponse> Response = CreateSimpleResponse(false, TEXT("No player controller found"));
		OnComplete(MoveTemp(Response));
		return true;
	}
	APawn* LyraPlayerPawn = LPC->GetPawn();
	if (!LyraPlayerPawn)
	{
		TUniquePtr<FHttpServerResponse> Response = CreateSimpleResponse(false, TEXT("Player pawn not found"));
		OnComplete(MoveTemp(Response));
		return true;
	}

	// TODO：在此触发一次真实开火输入。
	// TODO: Fire Once here
	TUniquePtr<FHttpServerResponse> Response = CreateSimpleResponse(true);
	OnComplete(MoveTemp(Response));
	return true;
}

// 取得首个本地玩家，校验 Pawn 与 PlayerState 后返回生命值及库存数组 JSON；任一前置对象缺失时返回失败响应。
bool ULyraGameplayRpcRegistrationComponent::HttpGetPlayerVitalsCommand(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	ALyraPlayerController* LPC = GetPlayerController();
	if (!LPC)
	{
		TUniquePtr<FHttpServerResponse> Response = CreateSimpleResponse(false, TEXT("No player controller found"));
		OnComplete(MoveTemp(Response));
		return true;
	}
	APawn* PlayerPawn = LPC->GetPawn();
	if (!PlayerPawn)
	{
		TUniquePtr<FHttpServerResponse> Response = CreateSimpleResponse(false, TEXT("Player pawn not found"));
		OnComplete(MoveTemp(Response));
		return true;
	}

	ALyraPlayerState* LyraPlayerState = LPC->GetLyraPlayerState();
	if (!LyraPlayerState)
	{
		TUniquePtr<FHttpServerResponse> Response = CreateSimpleResponse(false, TEXT("Player state not found"));
		OnComplete(MoveTemp(Response));
		return true;
	}

	FString ResponseStr; 
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&ResponseStr);
	TSharedPtr<FJsonObject> BodyObject = MakeShareable(new FJsonObject());
	JsonWriter->WriteObjectStart();
	if (ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(PlayerPawn))
	{
		JsonWriter->WriteValue(TEXT("health"), FString::SanitizeFloat(HealthComponent->GetHealth()));
	}
	if (ULyraInventoryManagerComponent* InventoryComponent = LPC->GetComponentByClass<ULyraInventoryManagerComponent>())
	{
		JsonWriter->WriteArrayStart(TEXT("inventory"));
		for (ULyraInventoryItemInstance* ItemInstance : InventoryComponent->GetAllItems())
		{
			// TODO：在响应中写入需要观察的玩家状态信息。
			// TODO: Dump any relevant player info here.
		}
		JsonWriter->WriteArrayEnd();
	}
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	TUniquePtr<FHttpServerResponse>Response = FHttpServerResponse::Create(ResponseStr, TEXT("application/json"));
	OnComplete(MoveTemp(Response));
	return true;
}

#endif
