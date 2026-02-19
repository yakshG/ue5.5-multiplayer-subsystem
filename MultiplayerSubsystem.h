#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MultiplayerSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSessionCreateDelegate, bool, WasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSessionJoinDelegate, bool, WasSuccessful);

UCLASS()
class MULTIPLAYERCOOP_API UMultiplayerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	UMultiplayerSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintReadWrite)
	FString GameMapPath;

	UPROPERTY(BlueprintReadOnly)
	bool IsLAN;

	FString SubsystemName;
	FString TravelURL;
	FString PendingServerName;
	FString ServerToJoin;

	bool SessionCreated;
	
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;

	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle StartSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
	FDelegateHandle FindSessionCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle InviteAcceptedHandle;

	UPROPERTY(BlueprintAssignable)
	FSessionCreateDelegate SessionCreateDel;

	UPROPERTY(BlueprintAssignable)
	FSessionJoinDelegate SessionJoinDel;

	UFUNCTION(BlueprintCallable)
	void CreateSessionInternal(FString ServerName);

	UFUNCTION(BlueprintCallable)
	void FindSession(FString ServerName);

	void OnSessionCreated(FName SessionName, bool WasSuccessful);
	void OnSessionStart(FName SessionName, bool WasSuccessful);
	void OnSessionDestroyed(FName SessionName, bool WasSuccessful);
	void OnSessionFound(bool WasSuccessful);
	void OnSessionJoined(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnSessionInviteAccepted(const bool WasSuccessful, int32 LocaluserNum, TSharedPtr<const FUniqueNetId> FriendId, const FOnlineSessionSearchResult& InviteResult);

	void PrintSessionResults(const FOnlineSessionSearchResult& SearchResult);

	UFUNCTION(BlueprintCallable)
	void ShowInviteUI();

	UFUNCTION(BlueprintCallable)
	void LaunchMap();

};
