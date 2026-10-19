#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidR10TestDirector.generated.h"
class ARaidPlayer;
class ARaidPlayerController;
class ARaidGameMode;
class ARaidLoot;
UCLASS()
class ARaidR10TestDirector : public AActor
{
 GENERATED_BODY()
public:
 ARaidR10TestDirector();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
private:
 struct FStep { FString Name; double Seconds; TFunction<void()> Begin; TFunction<void()> Verify; };
 void Build(); void Kernel(); void Check(bool Passed,const TCHAR* Name); void Capture(const TCHAR* Name);
 void Add(const TCHAR* Name,double Seconds,TFunction<void()> Begin,TFunction<void()> Verify={});
 void Heal(); void ClearBag(); void SetupMedical(); void Key(FKey Key,bool Pressed);
 TArray<FStep> Steps;
 UPROPERTY() TObjectPtr<ARaidPlayer> P;
 UPROPERTY() TObjectPtr<ARaidPlayerController> PC;
 UPROPERTY() TObjectPtr<ARaidGameMode> M;
 UPROPERTY() TObjectPtr<ARaidLoot> Box;
 FString Output;
 double Started=0, Next=0;
 int32 Step=0, Checks=0, Failures=0, BeforeCount=0;
 int64 BeforeRevision=0;
 float Progress=0;
 bool bReady=false,bRunning=false;
};
