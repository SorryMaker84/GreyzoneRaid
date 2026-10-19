#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidFlowDirector.generated.h"

class ARaidPlayer;
class ARaidGameMode;
class ARaidHUD;
class ARaidLoot;
class ARaidExtractionZone;
class APlayerController;

/** Opt-in real frontend/travel integration run. Never injects OS input. */
UCLASS(NotBlueprintable)
class GREYZONERAID_API ARaidFlowDirector : public AActor
{
    GENERATED_BODY()
public:
    ARaidFlowDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    double SearchReopenedAt = 0.0;
    enum class EStep : uint8
    {
        Ready, MenuGuard, Instructions, Back, Deploy, EmptyExtraction, PrepareSearch, ContainerOpen,
        SearchStart, SearchPartial, SearchCancelled, SearchResume, Search, Take, ContainerEmpty, ContainerRevisit,
        PartialExtraction, LeaveReset, DamageCountdown, DamageReset,
        CompleteExtraction, Success, Death, Restart, Abandon, Abandoned, ReturnMenu,
        Finish, Screenshot, AwaitTravel
    };
    void Advance(EStep Next);
    void Check(bool Passed, const TCHAR* Name, const FString& Detail = FString());
    void Abort(const TCHAR* Name, const FString& Detail = FString());
    void CheckInputs(bool Running);
    void CheckFreshRaid();
    void CheckChineseFonts();
    void SuppressRaidAI();
    void PlacePlayer(const FVector& Position);
    void AimAt(const FVector& Position);
    bool ExecuteAction(FName Name, bool Pressed);
    bool FindButtonPoint(FName Name, FVector2D& Point) const;
    bool ClickButton(FName Name);
    void Travel(FName Button, int32 NextPhase);
    void Capture(const TCHAR* Name, EStep Next);
    void FinishAndQuit();

    UPROPERTY() TObjectPtr<ARaidPlayer> Player;
    UPROPERTY() TObjectPtr<ARaidGameMode> Mode;
    UPROPERTY() TObjectPtr<ARaidHUD> HUD;
    UPROPERTY() TObjectPtr<APlayerController> PC;
    UPROPERTY() TObjectPtr<ARaidLoot> Loot;
    UPROPERTY() TObjectPtr<ARaidExtractionZone> Zone;
    EStep Step = EStep::Ready;
    EStep AfterCapture = EStep::Finish;
    float StepSeconds = 0.f;
    bool Issued = false;
    bool AISuppressed = false;
    bool SawDamageReset = false;
    float BeforeHealth = 100.f;
    float CountdownStart = 0.f;
    int32 BeforeAmmo = 12;
    int32 ExpectedItems = 0;
    int64 ExpectedValue = 0;
    int32 ContainerRows = 0;
    int32 InterruptedSearchIndex = INDEX_NONE;
    FVector BeforePosition = FVector::ZeroVector;
    FRotator BeforeView = FRotator::ZeroRotator;
    FVector InsidePosition = FVector::ZeroVector;
    FString ScreenshotPath;
    int64 LastScreenshotSize = -1;
    float ScreenshotStableSeconds = 0.f;
};
