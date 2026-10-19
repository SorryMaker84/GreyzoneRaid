#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidR7TestDirector.generated.h"

class ARaidPlayer;
class ARaidGameMode;
class ARaidHUD;
class ARaidLoot;
class ARaidEnemy;
class ARaidMissionObjective;
class ARaidExtractionZone;
class APlayerController;
class AController;
class UDamageType;
class FJsonObject;
class FJsonValue;

/** Opt-in Frontier evidence director. Never injects operating-system input. */
UCLASS(NotBlueprintable)
class GREYZONERAID_API ARaidR7TestDirector : public AActor
{
    GENERATED_BODY()
public:
    ARaidR7TestDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    enum class EMode : uint8 { Test, Preview, Performance };
    enum class EStage : uint8 { Ready, SetView, SettleView, Capture, Sample, Finish };
    struct FView
    {
        FString Id;
        FVector Location = FVector::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;
        bool bTacticalMap = false;
    };
    struct FRoute
    {
        FString Id;
        TArray<FVector> FloorPoints;
    };
    enum class ETestStep : uint8
    {
        Init, Observe, ChooseLoot, Walk, LootFocus, LootUnknown, LootSearch, LootReveal,
        LootRoom, LootTake, LootTaken, LootClose, RouteSelect, RouteCheckpoint,
        MissionApproach, MissionStart, MissionHold, MissionRange, MissionCancelled, MissionReturn,
        NorthApproach, NorthLocked, SouthApproach, ExtractPartial, ExtractLeave,
        ExtractLeft, ExtractReenter, ExtractDamage, ExtractDamageWait,
        ExtractUnpossess, ExtractRepossess, ExtractSwap, ExtractSwapWait, ExtractRestore,
        ExtractFinish, Result, Capture, Travel, DeathApproach, DeathWait, Finish,
        R10TreatmentStart, R10TreatmentWait, R10TreatmentResume
    };

    bool LoadManifest();
    bool ResolveGameplay();
    void IgnoreExternalInput();
    bool ExecuteAction(FName Name, bool bPressed = true);
    bool ExecuteAxis(FName Name, float Value);
    bool SetView(const FView& View);
    bool Check(bool bPassed, const TCHAR* Name, const FString& Detail = FString());
    void Fail(const TCHAR* Name, const FString& Detail = FString());
    void Advance(EStage Next);
    void TickPreview(float DeltaSeconds);
    void TickPerformance(float DeltaSeconds);
    void TickTest(float DeltaSeconds);
    bool PrepareTestWorld();
    bool AuditNavigation();
    bool ProbePath(const FVector& From, const FVector& To, const FString& Label);
    AActor* FindTaggedActor(const FString& Tag) const;
    bool FindLootApproach(ARaidLoot* Loot, FVector& Centre) const;
    bool StartWalk(const FVector& Destination, const FString& Label, ETestStep Next, float Radius = 70.f);
    void TickWalk(float DeltaSeconds);
    bool UpdateCombat();
    bool SeekR10Cover();
    void ObserveEnemies();
    void AimAt(const FVector& Target);
    bool ClickButton(FName Name);
    void SetTestStep(ETestStep Next);
    void CaptureTest(const FString& Name, ETestStep Next);
    void RestartTestRound();
    UFUNCTION()
    void ObserveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
        AController* InstigatedBy, AActor* DamageCauser);
    void RequestCapture();
    bool PollCapture(float DeltaSeconds);
    bool WriteReceipt(bool bPassed);
    void Finish();

    UPROPERTY() TObjectPtr<ARaidPlayer> RaidPawn;
    UPROPERTY() TObjectPtr<ARaidGameMode> RaidMode;
    UPROPERTY() TObjectPtr<ARaidHUD> RaidHUD;
    UPROPERTY() TObjectPtr<APlayerController> RaidPC;
    UPROPERTY() TObjectPtr<ARaidLoot> TestLoot;
    UPROPERTY() TObjectPtr<ARaidMissionObjective> TestObjective;
    UPROPERTY() TObjectPtr<ARaidExtractionZone> SouthExit;
    UPROPERTY() TObjectPtr<ARaidExtractionZone> NorthExit;
    UPROPERTY() TObjectPtr<ARaidPlayer> OriginalPawn;
    UPROPERTY() TObjectPtr<ARaidPlayer> PossessionProbePawn;
    TArray<TWeakObjectPtr<ARaidLoot>> MapLoot;
    TArray<TWeakObjectPtr<ARaidEnemy>> MapEnemies;
    TMap<TWeakObjectPtr<ARaidEnemy>, FVector> PatrolOrigins;
    TMap<TWeakObjectPtr<ARaidEnemy>, FVector> ChaseOrigins;
    TSet<TWeakObjectPtr<ARaidLoot>> TouchedLoot;
    TArray<FRoute> Routes;
    TArray<FVector> WalkPoints;
    ETestStep TestStep = ETestStep::Init;
    ETestStep AfterWalk = ETestStep::Finish;
    ETestStep AfterTestCapture = ETestStep::Finish;
    FString WalkLabel;
    FString PendingCaptureId;
    FVector WalkDestination = FVector::ZeroVector;
    FVector PreviousWalkPosition = FVector::ZeroVector;
    FVector WalkProgressPosition = FVector::ZeroVector;
    FVector MissionStand = FVector::ZeroVector;
    double TestStepStartedAt = 0.0;
    double WalkStartedAt = 0.0;
    double WalkProgressAt = 0.0;
    double WalkTelemetryAt = 0.0;
    double MissionHoldStartedAt = 0.0;
    double WalkDistance = 0.0;
    double PlannedWalkLength = 0.0;
    float ArrivalRadius = 70.f;
    float LiveDamageHealthBefore = -1.f;
    float InjectedDamageHealthBefore = 0.f;
    float OriginalPawnHealth = 0.f;
    float BeforePossessionSeconds = 0.f;
    int32 WalkPointIndex = 0;
    int32 RouteIndex = 0;
    int32 RoutePointIndex = 0;
    int32 MissionScenario = 0;
    int32 LootBeforeCount = 0;
    int32 LootTakeEntryId = INDEX_NONE;
    int32 LootTakeQuantity = 0;
    int32 ExtractionExpectedItems = 0;
    int32 LiveDamageEvents = 0;
    int32 DeathWalkDamageBaseline = 0;
    int64 ExtractionExpectedValue = 0;
    bool bTestFiring = false;
    bool bPendingLiveDamage = false;
    bool bLootOpenedOnce = false;
    bool bLastDamageFromMapAI = false;
    bool bCapturedMissionPartial = false;
    bool bFinaleOnly = false;
    bool bR10Journey = false;
    bool bSeekingR10Cover = false;
    bool bR10HealingCaptured = false;
    int32 R10CoverAttempts = 0;
    int32 R10MedicalCount = 0;
    float R10HealthBefore = 0.f;
    FVector R10ResumeDestination;
    ETestStep R10ResumeStep = ETestStep::Finish;
    TSharedPtr<FJsonObject> MapManifest;
    TArray<FView> Views;
    TArray<TSharedPtr<FJsonValue>> ViewEvidence;
    TArray<FString> FrameRows;
    EMode Mode = EMode::Test;
    EStage Stage = EStage::Ready;
    FString OutputDirectory;
    FString ManifestPath;
    FString RunId;
    FString PendingScreenshot;
    double StartedWallSeconds = 0.0;
    double PreviousTickWallSeconds = 0.0;
    double StageStartedWallSeconds = 0.0;
    double SampleStartedWallSeconds = 0.0;
    double PreviousStatTime = -1.0;
    FString StartedUtc;
    int32 ViewIndex = 0;
    int32 Checks = 0;
    int32 Failures = 0;
    int32 Captures = 0;
    int32 WarmupFrames = 0;
    int32 SampleFrames = 0;
    int64 LastScreenshotBytes = -1;
    float StableScreenshotSeconds = 0.f;
    float SampleSeconds = 20.f;
    bool bEnabled = false;
    bool bFinished = false;
    bool bInputIsolationLogged = false;
    bool bManifestLoaded = false;
    bool bMapVisible = false;
};
