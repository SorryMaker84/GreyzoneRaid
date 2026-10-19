#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "RaidR81MapProbe.generated.h"

/** Opt-in map/input regression through simulated UE keys; normal AI unchanged. */
UCLASS()
class ARaidR81MapProbe : public AActor
{
    GENERATED_BODY()
public:
    ARaidR81MapProbe();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    void Key(FKey InKey, bool Pressed);
    void Tap(FKey InKey);
    void Check(bool Passed, const TCHAR* Name);
    void Observe(bool Desired, const TCHAR* Name);
    void Capture(const TCHAR* Name);
    int32 Step=0, Ammo=0;
    float Health=0, Clock=0;
    double Next=0, ReleaseAt=0;
    bool bExpectFixed=false;
    FKey PendingKey;
    FString Output;
};
