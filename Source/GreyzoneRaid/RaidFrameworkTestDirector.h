#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidInventoryTypes.h"
#include "RaidPlayerController.h"
#include "RaidFrameworkTestDirector.generated.h"

class ARaidGameMode;
class ARaidHUD;
class ARaidLoot;
class ARaidPlayer;
class URaidInventoryComponent;

/** Opt-in inventory transactions and real world lifecycle regression. */
UCLASS(NotBlueprintable)
class GREYZONERAID_API ARaidFrameworkTestDirector : public AActor
{
    GENERATED_BODY()
public:
    ARaidFrameworkTestDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    enum class EStep : uint8
    {
        Ready, LifecycleSetup, SearchBeforeUnpossess, WaitAfterUnpossess,
        SearchBeforeReplacement, WaitAfterReplacement, ControlMove, ControlLook, ControlFireWait, SearchBeforeDestroy,
        WaitAfterDestroy, Finish
    };
    bool Check(bool Passed, const TCHAR* Name, const FString& Detail = FString());
    void Abort(const TCHAR* Name, const FString& Detail = FString());
    void Finish();
    void Advance(EStep Next);
    bool RunKernelCases();
    bool RunComponentCases();
    bool CheckProductionCatalog();
    void ObserveChange(URaidInventoryComponent* Inventory, int64 Revision);
    void ObserveEnding(URaidInventoryComponent* Inventory);
    URaidInventoryComponent* SpawnInventory(AActor*& OutOwner);
    FRaidInventoryCommandContext CaptureContext() const;
    ARaidLoot* SpawnSearchCase(const FVector& Position, bool bDifferentUnknown = false);
    void PlaceAndAim(ARaidPlayer* Pawn, ARaidLoot* Loot);
    bool OpenAndSearch(ARaidLoot* Loot);
    bool ExecuteAxis(FName Name, float Value);
    bool ExecuteAction(FName Name, bool Pressed);

    UPROPERTY() TObjectPtr<ARaidGameMode> Mode;
    UPROPERTY() TObjectPtr<ARaidPlayerController> PC;
    UPROPERTY() TObjectPtr<ARaidHUD> HUD;
    UPROPERTY() TObjectPtr<ARaidPlayer> Player;
    UPROPERTY() TObjectPtr<ARaidPlayer> PreviousPlayer;
    UPROPERTY() TObjectPtr<ARaidLoot> SearchBox;
    UPROPERTY() TObjectPtr<ARaidLoot> ReplacementBox;
    UPROPERTY() TObjectPtr<URaidInventoryComponent> ObservedSource;
    UPROPERTY() TObjectPtr<URaidInventoryComponent> ObservedDestination;
    FRaidInventoryCommandContext OldContext;
    FRaidItemDefinition ProbeDefinition;
    EStep Step = EStep::Ready;
    double StartedAt = 0.0;
    float StepSeconds = 0.f;
    int32 Checks = 0;
    int32 Failures = 0;
    int32 ObserverMode = 0;
    int32 ChangeCalls = 0;
    int32 EndingCalls = 0;
    int32 ExpectedSourceCount = 0;
    int32 ExpectedDestinationCount = 0;
    int64 ExpectedSourceRevision = 0;
    int64 ExpectedDestinationRevision = 0;
    int32 ConsumeEntryId = INDEX_NONE;
    int32 EffectValue = 0;
    int32 EffectCommits = 0;
    int32 KnownEntryId = INDEX_NONE;
    int64 SavedSourceRevision = 0;
    int64 SavedBackpackRevision = 0;
    FVector ControlStartPosition = FVector::ZeroVector;
    FRotator ControlStartView = FRotator::ZeroRotator;
    int32 BeforeMagazine = 0;
    int32 BeforeReserve = 0;
    bool bReplacementSearchStarted = false;
    bool bCheckedOldDeadline = false;
    bool bObserversSawCompleteCommit = true;
    bool bReentryRejected = true;
    bool bEndingReadWasValid = false;
    bool bEndingMutationRejected = false;
    bool bFinished = false;
};
