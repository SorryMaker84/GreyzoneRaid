#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidVisualDirector.generated.h"

class ARaidPlayer;
class ARaidEnemy;
class ARaidLoot;

/** Opt-in real renderer capture; never active during ordinary play. */
UCLASS(NotBlueprintable)
class GREYZONERAID_API ARaidVisualDirector : public AActor
{
    GENERATED_BODY()
public:
    ARaidVisualDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    void IgnoreExternalViewportInput();
    void SuppressBackgroundEnemies();
    void Capture(const TCHAR* Name, int32 NextStage);
    void Advance(int32 NextStage);
    void Fail(const TCHAR* Reason);
    void AimAt(const FVector& Point);
    bool ExecuteAction(FName ActionName);
    void TickHandsReview(float DeltaSeconds);
    void HandsInput(FName Name, bool bPressed);
    UPROPERTY() TObjectPtr<ARaidPlayer> Player;
    UPROPERTY() TObjectPtr<ARaidEnemy> ReviewEnemy;
    UPROPERTY() TObjectPtr<ARaidLoot> ReviewLoot;
    float Elapsed = 0.f;
    float StageSeconds = 0.f;
    int32 Stage = 0;
    int32 AfterCapture = 0;
    int32 Captures = 0;
    int32 ReviewLootIndex = INDEX_NONE;
    int32 LootCountBeforeSearch = 0;
    int32 ReviewLootQuantityBeforeTake = 0;
    bool bIssued = false;
    bool bVisualReviewEnabled = false;
    bool bLoggedInputIsolation = false;
    FString PendingPath;
    FString ReviewOutput;
    bool bHandsReview = false;
    int32 HandsIndex = 0;
    int32 HandsPhase = 0;
    float HandsSeconds = 0.f;
    float HandsCancelledAt = -1.f;
    FVector HandsOrigin = FVector::ZeroVector;
    TArray<FString> HandsRows;
};
