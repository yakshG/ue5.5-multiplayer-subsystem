#include "MultiplayerSubsystem.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineExternalUIInterface.h"

UMultiplayerSubsystem::UMultiplayerSubsystem()
{
	ServerToJoin = "";
	SessionCreated = false;
	IsLAN = false;
}

void UMultiplayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();

	if (OnlineSubsystem)
	{
		SubsystemName = OnlineSubsystem->GetSubsystemName().ToString();
		if (SubsystemName == "NULL")
		{
			IsLAN = true;
		}
		UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubsystem] OnlineSubsystem: %s"), *SubsystemName);
		
		SessionInterface = OnlineSubsystem->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			CreateSessionCompleteHandle = SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UMultiplayerSubsystem::OnSessionCreated);
			StartSessionCompleteHandle = SessionInterface->OnStartSessionCompleteDelegates.AddUObject(this, &UMultiplayerSubsystem::OnSessionStart);
			DestroySessionCompleteHandle = SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UMultiplayerSubsystem::OnSessionDestroyed);
			FindSessionCompleteHandle =SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UMultiplayerSubsystem::OnSessionFound);
			JoinSessionCompleteHandle = SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UMultiplayerSubsystem::OnSessionJoined);
			InviteAcceptedHandle = SessionInterface->OnSessionUserInviteAcceptedDelegates.AddUObject(this, &UMultiplayerSubsystem::OnSessionInviteAccepted);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] Session interface is invalid."));
		}
	}
	else 
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] No online subsystem found."));
	}
}

void UMultiplayerSubsystem::Deinitialize()
{
	if (SessionInterface)
	{
		SessionInterface->OnCreateSessionCompleteDelegates.Remove(CreateSessionCompleteHandle);
		SessionInterface->OnStartSessionCompleteDelegates.Remove(StartSessionCompleteHandle);
		SessionInterface->OnDestroySessionCompleteDelegates.Remove(DestroySessionCompleteHandle);
		SessionInterface->OnFindSessionsCompleteDelegates.Remove(FindSessionCompleteHandle);
		SessionInterface->OnJoinSessionCompleteDelegates.Remove(JoinSessionCompleteHandle);
		SessionInterface->OnSessionUserInviteAcceptedDelegates.Remove(InviteAcceptedHandle);
	}

}

void UMultiplayerSubsystem::CreateSessionInternal(FString ServerName)
{
	if (ServerName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MultiplayerSubsystem] CreateSession: server name cannot be empty."));
		SessionCreateDel.Broadcast(false);
		return;
	}

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] CreateSession: session interface invalid."));
		return;
	}

	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);

	if (ExistingSession)
	{
		UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubsystem] Existing session found. Destroying before creating new one."));
		PendingServerName = ServerName;
		SessionInterface->DestroySession(NAME_GameSession);
		return;
	}

	FOnlineSessionSettings SessionSettings;
	SessionSettings.bIsDedicated = false;
	SessionSettings.bUsesPresence = true;
	SessionSettings.bUseLobbiesIfAvailable = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.NumPublicConnections = 2;
	SessionSettings.bAllowJoinViaPresence = true;
	SessionSettings.bAllowInvites = true;
	SessionSettings.bAllowJoinViaPresenceFriendsOnly = true;
	SessionSettings.bIsLANMatch = IsLAN;
	SessionSettings.Set(FName("SERVER_NAME"), ServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubsystem] Creating session '%s' (%s)..."), *ServerName, IsLAN ? TEXT("LAN") : TEXT("Steam"));

	bool Success = SessionInterface->CreateSession(0, NAME_GameSession, SessionSettings);
	if (!Success)
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] CreateSession returned false."));
	}
}

void UMultiplayerSubsystem::OnSessionDestroyed(FName SessionName, bool WasSuccessful)
{
	if (!WasSuccessful)
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] Failed to destroy existing session '%s'."), *SessionName.ToString());
		PendingServerName = "";
		return;
	}

	if (PendingServerName.IsEmpty())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubsystem] Session destroyed. Creating a new session '%s'."), *PendingServerName);
	FString NewServerName = PendingServerName;
	PendingServerName = "";
	CreateSessionInternal(NewServerName);
	
}

void UMultiplayerSubsystem::OnSessionCreated(FName SessionName, bool WasSuccessful)
{
	if (!WasSuccessful)
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] Session creation failed."));
		SessionCreateDel.Broadcast(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubsystem] Session '%s' created. Starting session..."), *SessionName.ToString());
	
	SessionCreateDel.Broadcast(true);

	TravelURL = GameMapPath.IsEmpty() ? "/Game/ThirdPerson/Maps/ThirdPersonMap?listen" : GameMapPath + "?listen";

	SessionCreated = true;
	SessionInterface->StartSession(NAME_GameSession);
}

void UMultiplayerSubsystem::OnSessionStart(FName SessionName, bool WasSuccessful)
{
	if (!WasSuccessful)
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubystem] Session start failed for '%s'."), *SessionName.ToString());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubystem] Session '%s' started."), *SessionName.ToString());

	if (IsLAN)
	{
		LaunchMap();
	}
	
}

void UMultiplayerSubsystem::FindSession(FString ServerName)
{
	if (ServerName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MultiplayerSubystem] FindSession: server name cannot be empty."));
		SessionJoinDel.Broadcast(false);
		return;
	}

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubystem] FindSession: session interface invalid."));
		return;
	}

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->bIsLanQuery = IsLAN;
	SessionSearch->MaxSearchResults = 100;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(FName("SERVER_NAME"), ServerName, EOnlineComparisonOp::Equals);

	ServerToJoin = ServerName;

	UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubystem] Searching for session '%s' (%s)..."), *ServerName, IsLAN ? TEXT("LAN") : TEXT("Steam"));
	SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
}

void UMultiplayerSubsystem::OnSessionFound(bool WasSuccessful)
{
	if (!WasSuccessful) 
	{
		UE_LOG(LogTemp, Warning, TEXT("[MultiplayerSubystem] FindSessions failed."));
		SessionJoinDel.Broadcast(false);
		return;
	}

	if (!SessionSearch.IsValid() || !SessionInterface.IsValid() || ServerToJoin.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubystem] OnSessionFound: invalid state (search, interface, or server name)."));
		SessionJoinDel.Broadcast(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubystem] Found %d sessions(s). Searching for '%s'..."), SessionSearch->SearchResults.Num(), *ServerToJoin);

	for (int32 i = 0; i < SessionSearch->SearchResults.Num(); i++)
	{
		PrintSessionResults(SessionSearch->SearchResults[i]);
	}

	FOnlineSessionSearchResult* TargetSession = nullptr;
	
	for (auto& SearchResult : SessionSearch->SearchResults)
	{
		FString FoundServerName;
		SearchResult.Session.SessionSettings.Get(FName("SERVER_NAME"), FoundServerName);

		if (FoundServerName == ServerToJoin)
		{
			TargetSession = &SearchResult;
			break;
		}
	}

	if (!TargetSession)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MultiplayerSubystem] No session found with name '%s'."), *ServerToJoin);
		SessionJoinDel.Broadcast(false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubsystem] Joining session '%s'..."), *ServerToJoin);

	FOnlineSessionSearchResult MutableResult = *TargetSession;
	MutableResult.Session.SessionSettings.bUsesPresence = true;
	MutableResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;

	SessionJoinDel.Broadcast(true);
	SessionInterface->JoinSession(0, NAME_GameSession, MutableResult);
}

void UMultiplayerSubsystem::OnSessionJoined(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	SessionJoinDel.Broadcast(Result == EOnJoinSessionCompleteResult::Success);

	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		FString ConnectString;		
		if (SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
		{
			UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubsystem] Joined Session '%s'. Travelling to: %s"), *SessionName.ToString(), *ConnectString);

			APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();

			if (PC)
			{
				PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] Joined session but failed to resolve connect string."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] JoinSession failed with result: % d"), int32(Result));
	}
}

void UMultiplayerSubsystem::OnSessionInviteAccepted(const bool WasSuccessful, int32 LocaluserNum, TSharedPtr<const FUniqueNetId> FriendId, const FOnlineSessionSearchResult& InviteResult)
{
	if (!WasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MultiplayerSubsystem] Invite not accepted or failed."));
		return;
	}

	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] OnSessionInviteAccepted: session interface invalid."));
		return;
	}

	FString FoundServerName = "Unknown";
	InviteResult.Session.SessionSettings.Get(FName("SERVER_NAME"), FoundServerName);
	UE_LOG(LogTemp, Log, TEXT("[MultiplayerSubsystem] Invite accepted for server: %s"), *FoundServerName);

	FOnlineSessionSearchResult MutableResult = InviteResult;
	MutableResult.Session.SessionSettings.bUsesPresence = true;
	MutableResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;

	bool JoinStarted = SessionInterface->JoinSession(0, NAME_GameSession, MutableResult);
	if (!JoinStarted) 
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] JoinSession via invite returned false immediately."));
	}
}

void UMultiplayerSubsystem::PrintSessionResults(const FOnlineSessionSearchResult& SearchResult)
{
	FString FoundServerName = "Unknown";
	SearchResult.Session.SessionSettings.Get(FName("SERVER_NAME"), FoundServerName);

	UE_LOG(LogTemp, Log, TEXT("  server name : %s"), *FoundServerName);
	UE_LOG(LogTemp, Log, TEXT("  session ID: %s"), *SearchResult.Session.GetSessionIdStr());
	UE_LOG(LogTemp, Log, TEXT("  owner: %s"), *SearchResult.Session.OwningUserName);
	UE_LOG(LogTemp, Log, TEXT("  players: %d / %d"), SearchResult.Session.SessionSettings.NumPublicConnections - SearchResult.Session.NumOpenPublicConnections,
		SearchResult.Session.SessionSettings.NumPublicConnections);
}

void UMultiplayerSubsystem::ShowInviteUI()
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] ShowInviteUI: no online subsystem."));
		return;
	}

	IOnlineExternalUIPtr ExternalUI = OnlineSubsystem->GetExternalUIInterface();
	if (!ExternalUI.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[MultiplayerSubsystem] ShowInviteUI: external UI interface invalid."));
		return;
	}

	bool Shown = ExternalUI->ShowInviteUI(0, NAME_GameSession);
	if (!Shown)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MultiplayerSubsystem] ShowInviteUI: failed to show Steam invite overlay."));
	}

}

void UMultiplayerSubsystem::LaunchMap()
{
	if (SessionCreated)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MultiplayerSubsystem] Server travelling to: %s"), *TravelURL);
		GetWorld()->ServerTravel(TravelURL);
	}
}
