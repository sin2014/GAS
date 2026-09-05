// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/CGameInstance.h"
#include "Network/CNetStatics.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "HttpModule.h"

void UCGameInstance::StartMatch()
{
	if (GetWorld()->GetNetMode() == ENetMode::NM_DedicatedServer || GetWorld()->GetNetMode() == ENetMode::NM_ListenServer)
	{
		LoadLevelAndListen(GameLevel);
	}
}

void UCGameInstance::Init()
{
	Super::Init();
	if (GetWorld()->IsEditorWorld())
		return;

	if (UCNetStatics::IsSessionServer(this))
	{
		CreateSession();
	}
}

bool UCGameInstance::IsLoggedIn() const
{
	if (IOnlineIdentityPtr IdentityPtr = UCNetStatics::GetIdentityPtr())
	{
		return IdentityPtr->GetLoginStatus(0) == ELoginStatus::LoggedIn;
	}

	return false;
}

bool UCGameInstance::IsLoggingIn() const
{
	return LoggingInDelegateHandle.IsValid();
}

void UCGameInstance::ClientAccountPortalLogin()
{
	ClientLogin("AccountPortal", "", "");
}

void UCGameInstance::ClientLogin(const FString& Type, const FString& Id, const FString& Token)
{
	if (IOnlineIdentityPtr IdentityPtr = UCNetStatics::GetIdentityPtr())
	{
		if (LoggingInDelegateHandle.IsValid())
		{
			IdentityPtr->OnLoginCompleteDelegates->Remove(LoggingInDelegateHandle);
			LoggingInDelegateHandle.Reset();
		}

		LoggingInDelegateHandle = IdentityPtr->OnLoginCompleteDelegates->AddUObject(this, &UCGameInstance::LoginCompleted);
		if (!IdentityPtr->Login(0, FOnlineAccountCredentials(Type, Id, Token)))
		{
			UE_LOG(LogTemp, Warning, TEXT("Login Failed Right Away!"))
			if (LoggingInDelegateHandle.IsValid())
			{
				IdentityPtr->OnLoginCompleteDelegates->Remove(LoggingInDelegateHandle);
				LoggingInDelegateHandle.Reset();
			}
			OnLoginCompleted.Broadcast(false, "", "Login Failed Right Away!");
		}
	}
}

void UCGameInstance::LoginCompleted(int NumOfLocalPlayer, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (IOnlineIdentityPtr IdentityPtr = UCNetStatics::GetIdentityPtr())
	{
		if (LoggingInDelegateHandle.IsValid())
		{
			IdentityPtr->OnLoginCompleteDelegates->Remove(LoggingInDelegateHandle);
			LoggingInDelegateHandle.Reset();
		}

		FString PlayerNickname = "";
		if (bWasSuccessful)
		{
			PlayerNickname = IdentityPtr->GetPlayerNickname(UserId);
			UE_LOG(LogTemp, Warning, TEXT("Logged in succesfully as: %s"), *(PlayerNickname))
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Loging in failed: %s"), *(Error))
		}

		OnLoginCompleted.Broadcast(bWasSuccessful, PlayerNickname, Error);
	}
	else
	{
		OnLoginCompleted.Broadcast(false, "", "Ca't find the Identity Pointer");
	}
}

void UCGameInstance::RequestCreateAndJoinSession(const FName& NewSessionName)
{
	UE_LOG(LogTemp, Warning, TEXT("Requesting Create and Join Session: %s"), *(NewSessionName.ToString()))
	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	FGuid SessioinSearchId = FGuid::NewGuid();

	FString CoordinatorURL = UCNetStatics::GetCoordinatorURL();

	FString URL = FString::Printf(TEXT("%s/Sessions"), *CoordinatorURL);
	UE_LOG(LogTemp, Warning, TEXT("Sending Request Session Creation to URL: %s"), *URL)

	Request->SetURL(URL);
	Request->SetVerb("POST");

	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(UCNetStatics::GetSessionNameKey().ToString(), NewSessionName.ToString());
	JsonObject->SetStringField(UCNetStatics::GetSessionSearchIdKey().ToString(), SessioinSearchId.ToString());

	FString RequestBoby;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBoby);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	Request->SetContentAsString(RequestBoby);
	Request->OnProcessRequestComplete().BindUObject(this, &UCGameInstance::SessionCreationRequestCompleted, SessioinSearchId);
	
	if (!Request->ProcessRequest())
	{
		UE_LOG(LogTemp, Warning, TEXT("Sesison Creation Request Failed Right Away!"))
	}
}

void UCGameInstance::CancelSessionCreation()
{
	UE_LOG(LogTemp, Warning, TEXT("Canceling Session Creation"))
	StopAllSessionFindings();

	if (IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr())
	{
		SessionPtr->OnFindSessionsCompleteDelegates.RemoveAll(this);
		SessionPtr->OnJoinSessionCompleteDelegates.RemoveAll(this);
	}

	StartGlobalSessionSearch();
}

void UCGameInstance::StartGlobalSessionSearch()
{
	UE_LOG(LogTemp, Warning, TEXT("Starting Global Session Search"))
	GetWorld()->GetTimerManager().SetTimer(GlobalSessionSearchTimerHandle, this, &UCGameInstance::FindGlobalSessions, GlobalSessionSearchInterval, true, 0.f);
}

bool UCGameInstance::JoinSessionWithId(const FString& SessionIdStr)
{
	if (SessionSearch.IsValid())
	{
		const FOnlineSessionSearchResult* SessionSearchResult = SessionSearch->SearchResults.FindByPredicate(
			[=](const FOnlineSessionSearchResult& Result)
			{
				return Result.GetSessionIdStr() == SessionIdStr;
			}
		);
		if (SessionSearchResult)
		{
			JoinSessionWithSearchResult(*SessionSearchResult);
			return true;
		}
	}

	return false;
}

void UCGameInstance::SessionCreationRequestCompleted(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FGuid SesisonSearchId)
{
	if (!bConnectedSuccessfully)
	{
		UE_LOG(LogTemp, Warning, TEXT("Connection responded with connection was not successful!"))
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Connection to Coordinator Successfully!"))

	int32 REsponseCode = Response->GetResponseCode();
	if (REsponseCode != 200)
	{
		UE_LOG(LogTemp, Warning, TEXT("Session Creation Failed, with code: %d"), REsponseCode)
		return;
	}

	FString ResponseStr = Response->GetContentAsString();
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
	int32 Port = 0;

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		Port = JsonObject->GetIntegerField(*(UCNetStatics::GetPortKey().ToString()));
	}

	UE_LOG(LogTemp, Warning, TEXT("Conntected to Coordinator Successfully and the new sesion created is on port: %d"), Port)
	StartFindingCreatedSession(SesisonSearchId);
}

void UCGameInstance::StartFindingCreatedSession(const FGuid& SessionSearchId)
{
	if (!SessionSearchId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Session Search Id is invalid, can't start finding!"))
		return;
	}

	StopAllSessionFindings();
	UE_LOG(LogTemp, Warning, TEXT("Start Finding Created Sesssion with Id: %s"), *(SessionSearchId.ToString()))

	GetWorld()->GetTimerManager().SetTimer(FindCreatedSesisonTimerHandle, 
		FTimerDelegate::CreateUObject(this, &UCGameInstance::FindCreatedSession, SessionSearchId),
		FindCreatedSessionSearchInterval,
		true, 0.f
		);

	GetWorld()->GetTimerManager().SetTimer(FindCreatedSesisonTimeoutTimerHanle, this, &UCGameInstance::FindCreatedSessionTimeout, FindCreatedSessionTimeoutDuration);
}

void UCGameInstance::StopAllSessionFindings()
{
	UE_LOG(LogTemp, Warning, TEXT("Stoping All Session Search"))
	StopFindingCreatedSession();
	StopGlobalSessionSearch();
}

void UCGameInstance::StopFindingCreatedSession()
{
	UE_LOG(LogTemp, Warning, TEXT("Stop Finding Created Session"))
	GetWorld()->GetTimerManager().ClearTimer(FindCreatedSesisonTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(FindCreatedSesisonTimeoutTimerHanle);

	if (IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr())
	{
		SessionPtr->OnFindSessionsCompleteDelegates.RemoveAll(this);
		SessionPtr->OnJoinSessionCompleteDelegates.RemoveAll(this);
	}
}

void UCGameInstance::StopGlobalSessionSearch()
{
	UE_LOG(LogTemp, Warning, TEXT("Stop Gloal Session Search"))
	if (GlobalSessionSearchTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(GlobalSessionSearchTimerHandle);
	}

	IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr();
	if (SessionPtr)
	{
		SessionPtr->OnFindSessionsCompleteDelegates.RemoveAll(this);
	}
}

void UCGameInstance::FindGlobalSessions()
{
	UE_LOG(LogTemp, Warning, TEXT("----- Retrying Global Session Search -------------"))

	IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr();
	if (!SessionPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't Find Sesison Interface, Wait for the next Global Session Search"))
		return;
	}

	SessionSearch = MakeShareable(new FOnlineSessionSearch);
	SessionSearch->bIsLanQuery = false;
	SessionSearch->MaxSearchResults = 20;
	
	SessionPtr->OnFindSessionsCompleteDelegates.RemoveAll(this);
	SessionPtr->OnFindSessionsCompleteDelegates.AddUObject(this, &UCGameInstance::GlobalSessionSearchCompleted);
	if (!SessionPtr->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Find Global Session Failed Right Away"))
		SessionPtr->OnFindSessionsCompleteDelegates.RemoveAll(this);
	}
}

void UCGameInstance::GlobalSessionSearchCompleted(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		OnGlobalSessionSearchCompleted.Broadcast(SessionSearch->SearchResults);
		for (const FOnlineSessionSearchResult& OnlineSessionSearchResult : SessionSearch->SearchResults)
		{
			FString SessionName = "Name_None";
			OnlineSessionSearchResult.Session.SessionSettings.Get<FString>(UCNetStatics::GetSessionNameKey(), SessionName);
			UE_LOG(LogTemp, Warning, TEXT("Found Session: %s after global session search"), *SessionName)
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Global Session Search Completed Unsuccessfully"))
	}

	IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr();
	if (SessionPtr)
	{
		SessionPtr->OnFindSessionsCompleteDelegates.RemoveAll(this);
	}
}


void UCGameInstance::FindCreatedSession(FGuid SessionSearchId)
{
	UE_LOG(LogTemp, Warning, TEXT("Trying to find created session"))
	IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr();
	if (!SessionPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't find Sesison Ptr, canceling session search"))
		return;
	}
	
	SessionSearch = MakeShareable(new FOnlineSessionSearch);
	if (!SessionSearch)
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to create ession search!, canceling session search"))
		return;
	}

	SessionSearch->bIsLanQuery = false;
	SessionSearch->MaxSearchResults = 1;
	SessionSearch->QuerySettings.Set(UCNetStatics::GetSessionSearchIdKey(), SessionSearchId.ToString(), EOnlineComparisonOp::Equals);

	SessionPtr->OnFindSessionsCompleteDelegates.RemoveAll(this);
	SessionPtr->OnFindSessionsCompleteDelegates.AddUObject(this, &UCGameInstance::FindCreateSessionCompleted);
	if (!SessionPtr->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		UE_LOG(LogTemp, Warning, TEXT("Find Session Failed Right Away!..."))
		SessionPtr->OnFindSessionsCompleteDelegates.RemoveAll(this);
	}
}

void UCGameInstance::FindCreatedSessionTimeout()
{
	UE_LOG(LogTemp, Warning, TEXT("Find Created Session Timeout Reached"))
	StopFindingCreatedSession();
}

void UCGameInstance::FindCreateSessionCompleted(bool bWasSuccessful)
{
	if (!bWasSuccessful || SessionSearch->SearchResults.Num() == 0)
	{
		return;
	}

	StopFindingCreatedSession();
	JoinSessionWithSearchResult(SessionSearch->SearchResults[0]);
}

void UCGameInstance::JoinSessionWithSearchResult(const FOnlineSessionSearchResult& SearchResult)
{
	UE_LOG(LogTemp, Warning, TEXT("Joining session with Search Result"))
	IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr();
	if (!SessionPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't Find Session Ptr, Cancel Joining"))
		return;
	}
	
	FString SessionName = "";
	SearchResult.Session.SessionSettings.Get<FString>(UCNetStatics::GetSessionNameKey(), SessionName);

	const FOnlineSessionSetting* PortSetting = SearchResult.Session.SessionSettings.Settings.Find(UCNetStatics::GetPortKey());
	int64 Port = 7777;
	PortSetting->Data.GetValue(Port);

	UE_LOG(LogTemp, Warning, TEXT("Trying to join session: %s, at port: %d"), *(SessionName), Port)
	SessionPtr->OnJoinSessionCompleteDelegates.RemoveAll(this);
	SessionPtr->OnJoinSessionCompleteDelegates.AddUObject(this, &UCGameInstance::JoinSessionCompleted, (int)Port);
	if (!SessionPtr->JoinSession(0, FName(SessionName), SearchResult))
	{
		UE_LOG(LogTemp, Warning, TEXT("Joining Session Failed Right Away! ....."))
		SessionPtr->OnJoinSessionCompleteDelegates.RemoveAll(this);
		OnJoinSessionFailed.Broadcast();
	}
}

void UCGameInstance::JoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult, int Port)
{
	IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr();
	if (!SessionPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Join Session Completed, but can't find session pointer"))
		OnJoinSessionFailed.Broadcast();
		return;
	}

	if (JoinResult == EOnJoinSessionCompleteResult::Success)
	{
		StopAllSessionFindings();
		UE_LOG(LogTemp, Warning, TEXT("Joining Sesison: %s successful, the port is: %d"), *(SessionName.ToString()), Port)

		FString TravelURL = "";
		SessionPtr->GetResolvedConnectString(SessionName, TravelURL);
		
		FString TestingURL = UCNetStatics::GetTestingURL();
		if (!TestingURL.IsEmpty())
		{
			TravelURL = TestingURL;
		}

		UCNetStatics::ReplacePort(TravelURL, Port);

		UE_LOG(LogTemp, Warning, TEXT("Traveling to Session at: %s"), *TravelURL)

		GetFirstLocalPlayerController(GetWorld())->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
	}
	else
	{
		OnJoinSessionFailed.Broadcast();
	}

	SessionPtr->OnJoinSessionCompleteDelegates.RemoveAll(this);
}

void UCGameInstance::PlayerJoined(const FUniqueNetIdRepl& UniqueId)
{
	if (WaitPlayerJoinTimeoutHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(WaitPlayerJoinTimeoutHandle);
	}

	PlayerRecord.Add(UniqueId);
}

void UCGameInstance::PlayerLeft(const FUniqueNetIdRepl& UniqueId)
{
	PlayerRecord.Remove(UniqueId);
	if (PlayerRecord.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("All player left the session, terminating"))
		TerminateSessionServer();
	}
}

void UCGameInstance::CreateSession()
{
	IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr();
	if (SessionPtr)
	{
		ServerSesisonName = UCNetStatics::GetSessionNameStr();
		FString SessionSearchId = UCNetStatics::GetSesisonSearchIdStr();
		SessionServerPort = UCNetStatics::GetSessionPort();
		UE_LOG(LogTemp, Warning, TEXT("#### Create Session With Name: %s, ID: %s, Port: %d"), *(ServerSesisonName), *(SessionSearchId), SessionServerPort)

		FOnlineSessionSettings OnlineSesisonSetting = UCNetStatics::GenerateOnlineSesisonSettings(FName(ServerSesisonName), SessionSearchId, SessionServerPort);
		SessionPtr->OnCreateSessionCompleteDelegates.RemoveAll(this);
		SessionPtr->OnCreateSessionCompleteDelegates.AddUObject(this, &UCGameInstance::OnSessionCreated);
		if (!SessionPtr->CreateSession(0, FName(ServerSesisonName), OnlineSesisonSetting))
		{
			UE_LOG(LogTemp, Warning, TEXT("Sesison Creating Failed Right away!!!!"))
			SessionPtr->OnCreateSessionCompleteDelegates.RemoveAll(this);
			TerminateSessionServer();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't find sesison ptr, terminating"))
		TerminateSessionServer();
	}
}

void UCGameInstance::OnSessionCreated(FName SessionName, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("------------- Session Created!"))
		GetWorld()->GetTimerManager().SetTimer(WaitPlayerJoinTimeoutHandle, this, &UCGameInstance::WaitPlayerJoinTimeoutReached, WaitPlayerJoinTimeOutDuration);
		LoadLevelAndListen(LobbyLevel);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("------------ Session Creation Failed"))
		TerminateSessionServer();
	}

	if (IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr())
	{
		SessionPtr->OnCreateSessionCompleteDelegates.RemoveAll(this);
	}
}

void UCGameInstance::EndSessisonCompleted(FName SessionName, bool bWasSuccessful)
{
	FGenericPlatformMisc::RequestExit(false);
}

void UCGameInstance::TerminateSessionServer()
{
	if (IOnlineSessionPtr SessionPtr = UCNetStatics::GetSessionPtr())
	{
		SessionPtr->OnEndSessionCompleteDelegates.RemoveAll(this);
		SessionPtr->OnEndSessionCompleteDelegates.AddUObject(this, &UCGameInstance::EndSessisonCompleted);
		if (!SessionPtr->EndSession(FName{ ServerSesisonName }))
		{
			FGenericPlatformMisc::RequestExit(false);
		}
	}
	else
	{
		FGenericPlatformMisc::RequestExit(false);
	}
}

void UCGameInstance::WaitPlayerJoinTimeoutReached()
{
	UE_LOG(LogTemp, Warning, TEXT("Session Sever shut down aftert %f seconds without player joining"), WaitPlayerJoinTimeOutDuration)
	TerminateSessionServer();
}


void UCGameInstance::LoadLevelAndListen(TSoftObjectPtr<UWorld> Level)
{
	const FName LevelURL = FName(*FPackageName::ObjectPathToPackageName(Level.ToString()));

	if (LevelURL != "")
	{
		FString TravelStr = FString::Printf(TEXT("%s?listen?port=%d"), *LevelURL.ToString(), SessionServerPort);
		UE_LOG(LogTemp, Warning, TEXT("Server Traveling to: %s"), *(TravelStr))
		GetWorld()->ServerTravel(TravelStr);
	}
}
