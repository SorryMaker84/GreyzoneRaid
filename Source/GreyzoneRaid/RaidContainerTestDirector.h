#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidInventoryTypes.h"
#include "Templates/Function.h"
#include "RaidContainerTestDirector.generated.h"

class ARaidPlayer;
class ARaidLoot;
class ARaidEnemy;
class ARaidGameMode;
class ARaidHUD;
class ARaidExtractionZone;
class APlayerController;
class AController;
class UDamageType;

/** Opt-in real-world container regression; never injects hardware input. */
UCLASS(NotBlueprintable)
class GREYZONERAID_API ARaidContainerTestDirector : public AActor
{
    GENERATED_BODY()
public:
    ARaidContainerTestDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    enum class EStep : uint8
    {
        Ready, OpenA, Unknown, StartPartial, Partial, CancelPartial, CancelWait,
        ReopenA, StartFull, FirstReveal, TakeFirst, RevealAll, Revisit,
        FillBackpack, FullView, StoreForMedkit, TakeMedkit, PrepareStale, StaleClick,
        TakeB, EmptyB, ControlMove, ControlLook, ControlFireWait,
        AmmoOpen, AmmoTake, AmmoStore, AmmoRetake, AmmoUse,
        GuardSetup, GuardStart, GuardApply, GuardWait,
        PatrolSetup, PatrolWait, WallKill, DropOpen, DropStart, DropReveal, DropTake,
        ExtractSetup, ExtractCountdown, ExtractOpen, ExtractSearch, ExtractFinish,
        Success, DeathSetup, DeathStart, DeathApply, DeathWait, RestartDeath,
        LiveSetup, LiveOpen, LiveStartSearch, LiveWaitHit,
        LiveDeathPrepare, LiveDeathSearch, LiveWaitDeath, LiveDeadWait,
        Screenshot, AwaitTravel, Finish
    };
    void Advance(EStep Next);
    bool Check(bool Passed, const TCHAR* Name, const FString& Detail = FString());
    void Abort(const TCHAR* Name, const FString& Detail = FString());
    void Finish();
    bool CheckFreshWorld();
    bool ExecuteAction(FName Name, bool Pressed = true);
    bool ExecuteAxis(FName Name, float Value);
    bool FindButtonPoint(FName Name, FVector2D& Point) const;
    bool ClickButton(FName Name);
    void PlacePlayer(const FVector& Position);
    void AimAt(const FVector& Position);
    ARaidLoot* SpawnFixture(const FVector& Position, bool ThreeRows, bool Revealed = false,
        TFunction<void(TArray<FRaidInventorySeed>&)> CustomizeSeeds = nullptr);
    int32 InventoryIndex(FName ItemId) const;
    int32 RevealedCount(const ARaidLoot* Loot) const;
    void Capture(const TCHAR* Name, EStep Next);
    void RestartIntoPhase(int32 Phase);
    bool CheckNormalGuards(int32 ExpectedCount, const TCHAR* Name);
    UFUNCTION()
    void ObserveLiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
        AController* InstigatedBy, AActor* DamageCauser);

    UPROPERTY() TObjectPtr<ARaidPlayer> Player;
    UPROPERTY() TObjectPtr<ARaidGameMode> Mode;
    UPROPERTY() TObjectPtr<ARaidHUD> HUD;
    UPROPERTY() TObjectPtr<APlayerController> PC;
    UPROPERTY() TObjectPtr<ARaidExtractionZone> Zone;
    UPROPERTY() TObjectPtr<ARaidLoot> BoxA;
    UPROPERTY() TObjectPtr<ARaidLoot> BoxB;
    UPROPERTY() TObjectPtr<ARaidLoot> GuardBox;
    UPROPERTY() TObjectPtr<ARaidLoot> EnemyDrop;
    UPROPERTY() TObjectPtr<ARaidLoot> LateBox;
    UPROPERTY() TObjectPtr<ARaidEnemy> PatrolEnemy;
    UPROPERTY() TObjectPtr<ARaidEnemy> LiveThreat;
    UPROPERTY() TObjectPtr<AActor> Cover;
    TWeakObjectPtr<ARaidEnemy> LiveDamageSource;
    EStep Step = EStep::Ready;
    EStep AfterCapture = EStep::Finish;
    float StepSeconds = 0.f;
    float ScreenshotStableSeconds = 0.f;
    float ExtractionStarted = 0.f;
    int64 LastScreenshotSize = -1;
    FString ScreenshotPath;
    FVector PatrolOrigin = FVector::ZeroVector;
    FVector ControlStartPosition = FVector::ZeroVector;
    FRotator ControlStartView = FRotator::ZeroRotator;
    int32 Operation = 0;
    int32 GuardScenario = 0;
    int32 ExpectedItems = 0;
    int64 ExpectedValue = 0;
    int32 BeforeItems = 0;
    int64 BeforeValue = 0;
    int32 BeforeReserve = 0;
    int32 BeforeMagazine = 0;
    int64 BeforeSourceRevision = 0;
    int64 BeforePlayerRevision = 0;
    int32 LiveDamageEvents = 0;
    int32 LiveDeathEventBaseline = 0;
    float LiveHealthBefore = 0.f;
    float LiveDamageAmount = 0.f;
    float LiveDamageYaw = 0.f;
    float LiveDamageAt = -1.f;
    float LiveDeathWindowStarted = -1.f;
    bool bLiveHitWhileSearching = false;
    bool bLiveDamageFromAI = false;
    bool bLiveControlProbe = false;
    bool Issued = false;
};
