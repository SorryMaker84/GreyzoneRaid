#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RaidMissionTypes.h"
#include "RaidGameMode.generated.h"

class ARaidGameMode;
class ARaidMissionObjective;

DECLARE_MULTICAST_DELEGATE_OneParam(FRaidMissionChanged, ARaidGameMode*);

UENUM(BlueprintType)
enum class ERaidState : uint8 { Running, Extracted, Dead, MainMenu, Instructions };

/** Owns one raid's result. Player, enemies and extraction all consult this state. */
UCLASS(Blueprintable)
class GREYZONERAID_API ARaidGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ARaidGameMode();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable) void EndRaid(bool bSuccess);
    UFUNCTION(BlueprintCallable) void RestartRaid();
    UFUNCTION(BlueprintCallable) void StartRaid();
    UFUNCTION(BlueprintCallable) void ReturnToMenu();
    UFUNCTION(BlueprintCallable) void HandleMenuAction(FName Action);
    UFUNCTION(BlueprintCallable) void HandleBack();
    void ResumeRaid();
    bool IsPauseMenuOpen() const { return bPauseMenuOpen; }
    bool IsAbandonConfirmationOpen() const { return bConfirmAbandon; }
    void ApplyInputMode();
    UFUNCTION(BlueprintPure) bool IsRaidActive() const { return State == ERaidState::Running; }
    UFUNCTION(BlueprintCallable) void PushMessage(const FString& Text, float Seconds = 3.f);
    UFUNCTION(BlueprintPure, Category="Raid|Mission") FRaidMissionSnapshot GetMissionSnapshot() const;
    UFUNCTION(BlueprintPure, Category="Raid|Mission") bool IsMissionCompleted(FName MissionId) const;
    ARaidMissionObjective* GetMissionObjective() const;
    void RegisterMissionObjective(ARaidMissionObjective* Objective);
    void UnregisterMissionObjective(ARaidMissionObjective* Objective);
    void NotifyMissionChanged(ARaidMissionObjective* Objective);
    FRaidMissionChanged OnMissionChanged;
    UPROPERTY(BlueprintReadOnly) ERaidState State = ERaidState::MainMenu;
    UPROPERTY(BlueprintReadOnly) FString Message;
    UPROPERTY(BlueprintReadOnly) int64 SecuredValue = 0;
    UPROPERTY(BlueprintReadOnly) int32 SecuredItems = 0;
    UPROPERTY(BlueprintReadOnly) float RaidElapsed = 0.f;
    UPROPERTY(BlueprintReadOnly) int32 EnemiesKilled = 0;
private:
    TWeakObjectPtr<ARaidMissionObjective> MissionObjective;
    FRaidMissionSnapshot MissionResult;
    bool bHasMissionResult = false;
    bool bTravelPending = false;
    bool bPauseMenuOpen = false;
    bool bConfirmAbandon = false;
    float MessageRemaining = 0.f;
};
