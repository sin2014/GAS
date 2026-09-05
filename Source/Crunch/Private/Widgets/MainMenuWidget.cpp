// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/MainMenuWidget.h"
#include "Components/EditableText.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Framework/CGameInstance.h"
#include "Network/CNetStatics.h"
#include "Widgets/WaitingWidget.h"
#include "Widgets/SessionEntryWidget.h"

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CGameInstance = GetGameInstance<UCGameInstance>();
	if (CGameInstance)
	{
		CGameInstance->OnLoginCompleted.AddUObject(this, &UMainMenuWidget::LoginCompleted);
		if (CGameInstance->IsLoggedIn())
		{
			SwitchToMainWidget();
		}

		CGameInstance->OnJoinSessionFailed.AddUObject(this, &UMainMenuWidget::JoinSessionFailed);
		CGameInstance->OnGlobalSessionSearchCompleted.AddUObject(this, &UMainMenuWidget::UpdateLobbyList);
		CGameInstance->StartGlobalSessionSearch();

	}

	LoginBtn->OnClicked.AddDynamic(this, &UMainMenuWidget::LoginBtnClicked);

	CreateSessionBtn->OnClicked.AddDynamic(this, &UMainMenuWidget::CreateSesisonBtnClicked);
	CreateSessionBtn->SetIsEnabled(false);

	NewSessionNameText->OnTextChanged.AddDynamic(this, &UMainMenuWidget::NewSessionNameTextChanged);
	JoinSessionBtn->OnClicked.AddDynamic(this, &UMainMenuWidget::JoinSessionBtnClicked);
	JoinSessionBtn->SetIsEnabled(false);
}

void UMainMenuWidget::SwitchToMainWidget()
{
	if (MainSwitcher)
	{
		MainSwitcher->SetActiveWidget(MainWidgetRoot);
	}
}

void UMainMenuWidget::CreateSesisonBtnClicked()
{
	if (CGameInstance && CGameInstance->IsLoggedIn())
	{
		CGameInstance->RequestCreateAndJoinSession(FName(NewSessionNameText->GetText().ToString()));
		SwitchToWaitingWidget(FText::FromString("Creating Lobby"), true).AddDynamic(this, &UMainMenuWidget::CancelSessionCreation);
	}
}

void UMainMenuWidget::CancelSessionCreation()
{
	if (CGameInstance)
	{
		CGameInstance->CancelSessionCreation();
	}
	SwitchToMainWidget();
}

void UMainMenuWidget::NewSessionNameTextChanged(const FText& NewText)
{
	CreateSessionBtn->SetIsEnabled(!NewText.IsEmpty());
}

void UMainMenuWidget::JoinSessionFailed()
{
	SwitchToMainWidget();
}

void UMainMenuWidget::UpdateLobbyList(const TArray<FOnlineSessionSearchResult>& SearchResults)
{
	UE_LOG(LogTemp, Warning, TEXT("Updating Session Search Results"))
	SessionScrollBox->ClearChildren();

	bool bCurrentSelectedSessionValid = false;
	for (const FOnlineSessionSearchResult& SearchResult : SearchResults)
	{
		USessionEntryWidget* NewSessionWidget = CreateWidget<USessionEntryWidget>(GetOwningPlayer(), SessionEntryWidgetClass);
		if (NewSessionWidget)
		{
			FString SessionName = "Name_None";
			SearchResult.Session.SessionSettings.Get<FString>(UCNetStatics::GetSessionNameKey(), SessionName);

			FString SessionIdStr = SearchResult.Session.GetSessionIdStr();
			NewSessionWidget->InitializeEntry(SessionName, SessionIdStr);
			NewSessionWidget->OnSessionEntrySelected.AddUObject(this, &UMainMenuWidget::SessionEntrySelected);
			SessionScrollBox->AddChild(NewSessionWidget);
			if (CurrentSelectedSessionId == SessionIdStr)
			{
				bCurrentSelectedSessionValid = true;
			}
		}
	}

	CurrentSelectedSessionId = bCurrentSelectedSessionValid ? CurrentSelectedSessionId : "";
	JoinSessionBtn->SetIsEnabled(bCurrentSelectedSessionValid);
}

void UMainMenuWidget::JoinSessionBtnClicked()
{
	if (CGameInstance && !CurrentSelectedSessionId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Trying to join session with id: %s"), *CurrentSelectedSessionId)
			if (CGameInstance->JoinSessionWithId(CurrentSelectedSessionId))
			{
				SwitchToWaitingWidget(FText::FromString("Joining"));
			}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't join session, no session selected"))
	}
}


void UMainMenuWidget::SessionEntrySelected(const FString& SelectedEntryIdStr)
{
	CurrentSelectedSessionId = SelectedEntryIdStr;
}

void UMainMenuWidget::LoginBtnClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("Logining In!"))
	if (CGameInstance && !CGameInstance->IsLoggedIn() && !CGameInstance->IsLoggingIn())
	{
		CGameInstance->ClientAccountPortalLogin();
		SwitchToWaitingWidget(FText::FromString("Logging In"));
	}
}

void UMainMenuWidget::LoginCompleted(bool bWasSuccessful, const FString& PlayerNickname, const FString& ErrorMsg)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("Login successful"))
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Login Failed!"))
	}
		
	SwitchToMainWidget();
}

FOnButtonClickedEvent& UMainMenuWidget::SwitchToWaitingWidget(const FText& WaitInfo, bool bAllowCancel)
{
	MainSwitcher->SetActiveWidget(WaitingWidget);
	WaitingWidget->SetWaitInfo(WaitInfo, bAllowCancel);
	return WaitingWidget->ClearAndGetButtonClickedEvent();
}
