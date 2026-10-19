#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "RaidR9TestDirector.generated.h"
class ARaidPlayer;
class ARaidPlayerController;
class ARaidLoot;

/** Opt-in UE input and continuous presentation measurements; never normal gameplay. */
UCLASS()
class ARaidR9TestDirector : public AActor
{
    GENERATED_BODY()
public:
    ARaidR9TestDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    struct FStep { FString Name; double Seconds; TFunction<void()> Begin; TFunction<void()> Verify; };
    void Add(const TCHAR* Name,double Seconds,TFunction<void()> Begin,TFunction<void()> Verify={});
    void Key(FKey InKey,bool Pressed);
    void Tap(FKey InKey);
    void Check(bool Passed,const TCHAR* Name);
    void Capture(const TCHAR* Name);
    void ResetWeapon(int32 Magazine=12,int32 Reserve=60);
    void NewBox();
    void AimBox();
    bool Click(FName Button);
    void BuildMeasurements();
    void BuildRegression();
    void Sample(float DeltaSeconds);
    void Finish();
    TArray<FStep> Steps;
    int32 Step=0,Checks=0,Failures=0,ShotCount=0,StartCount=0,Ammo=0,Reserve=0;
    double Started=0,StepStarted=0,Next=0,ReleaseAt=0;
    float LastSampleShot=-100.f,Clock=0,Progress=0;
    FVector Home;
    bool bRunning=false,bBaseline=false,bMeasuresOnly=false;
    FKey PendingKey;
    FString Output,Frames,Shots;
    UPROPERTY() TObjectPtr<ARaidPlayer> P;
    UPROPERTY() TObjectPtr<ARaidPlayerController> PC;
    UPROPERTY() TObjectPtr<ARaidLoot> Box;
};
