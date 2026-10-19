#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidTestDirector.generated.h"

class ARaidPlayer;
class ARaidGameMode;
class ARaidEnemy;
class ARaidLoot;
class ARaidExtractionZone;

/** Opt-in integration checks. Calls gameplay methods; does not simulate hardware input. */
UCLASS(NotBlueprintable)
class GREYZONERAID_API ARaidTestDirector : public AActor
{
    GENERATED_BODY()

public:
    ARaidTestDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    enum class EStep : uint8
    {
        WaitPlayer, Move, Sprint, Jump, JumpLanding, Medkit, Turn, AimIn, AimHold, AimOut,
        FireDamage, FireKill, EmptyMagazine, DryFire,
        Reload, SearchPartial, SearchCancelWait, SearchComplete, SearchHealWait, Collect, FullBackpack,
        InventoryOpen, InventoryClose, Navigation, Patrol, Chase, Attack, CoverSetup, CoverBlocked, CoverRemoved,
        EnterExtraction, PartialExtraction, ExitExtraction, CompleteExtraction,
        Death, Screenshot, ResetScreenshotWait, TravelSuccess, TravelDeath, AwaitTravel, Finish
    };

    void EnterStep(EStep NewStep);
    void Check(bool bPassed, const TCHAR* Name, const FString& Detail = FString());
    void ClearTestActors();
    void PlacePlayer(const FVector& Location);
    void AimAt(const FVector& Location);
    bool ExecuteBoundAction(FName ActionName, bool bPressed = true);
    ARaidEnemy* SpawnEnemy(const FVector& Location, bool bStationary);
    ARaidLoot* SpawnLoot(const FVector& Location, bool bMedkit = false);
    void BeginScreenshot(const TCHAR* Name, EStep NextStep);
    void Finish();
    UFUNCTION() void ProbeMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
    void ProbeSample(const TCHAR* Timing, float DeltaSeconds);
    bool bMovementProbe = false;
    bool bProbeInputFrame = false;
    bool bProbeHitchInjected = false;
    float ProbeAppliedSeconds = 0.f;
    FString ProbeCSV;
    FString ProbeOutput;

    UPROPERTY() TObjectPtr<ARaidPlayer> Player;
    UPROPERTY() TObjectPtr<ARaidGameMode> Mode;
    UPROPERTY() TObjectPtr<ARaidEnemy> Enemy;
    UPROPERTY() TObjectPtr<ARaidLoot> Loot;
    UPROPERTY() TArray<TObjectPtr<ARaidLoot>> LootFixtures;
    UPROPERTY() TObjectPtr<ARaidExtractionZone> Extraction;
    UPROPERTY() TObjectPtr<AActor> TestCover;

    EStep Step = EStep::WaitPlayer;
    EStep AfterScreenshot = EStep::Finish;
    float StepSeconds = 0.f;
    float LastNavigationAttempt = -1.f;
    FVector InitialLocation = FVector::ZeroVector;
    FVector EnemyInitialLocation = FVector::ZeroVector;
    float InitialYaw = 0.f;
    float InitialFOV = 95.f;
    float HeldADSFOV = 65.f;
    float SearchTestDuration = 1.1f;
    float InitialHealth = 100.f;
    float InitialEnemyHealth = 100.f;
    float WalkDistance = 0.f;
    float PeakJumpHeight = 0.f;
    int32 InitialMagazine = 0;
    int32 InitialReserve = 0;
    int32 ExpectedItems = 0;
    int32 MedicalStage = 0;
    int64 ExpectedValue = 0;
    bool bActionIssued = false;
    bool bObservedChase = false;
    bool bReleaseBindingExecuted = false;
    bool bSearchStarted = false;
    bool bObservedCoverResponse = false;
    FString ScreenshotPath;
};
