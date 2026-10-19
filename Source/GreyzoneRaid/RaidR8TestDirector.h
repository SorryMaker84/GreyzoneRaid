#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "RaidR8TestDirector.generated.h"

/** Opt-in runtime UX regression. Simulated UE key events, never hardware input. */
UCLASS()
class ARaidR8TestDirector : public AActor
{
    GENERATED_BODY()
public:
    ARaidR8TestDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    void Check(bool Passed, const TCHAR* Name);
    void Tap(FKey Key);
    void Click(FName Name);
    void Capture(const TCHAR* Name);
    int32 Step = 0;
    double Next = 0, ReleaseAt = 0;
    FKey PendingKey;
    FString Output;
    float Clock = 0, Progress = 0, Health = 0, Extraction = 0;
    int32 Ammo = 0;
    FVector PausedJumpLocation, PausedJumpVelocity;
    TWeakObjectPtr<class ARaidExtractionZone> Zone;
    TMap<TWeakObjectPtr<AActor>, FVector> GuardPositions;
};
