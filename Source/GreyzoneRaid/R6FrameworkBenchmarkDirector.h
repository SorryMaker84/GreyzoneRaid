#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "R6FrameworkBenchmarkDirector.generated.h"

class ARaidHUD;
class ARaidPlayer;
class ARaidExtractionZone;

/** Opt-in CPU microbenchmark of the real HUD queries; never an FPS benchmark. */
UCLASS(NotBlueprintable)
class GREYZONERAID_API AR6FrameworkBenchmarkDirector : public AActor
{
    GENERATED_BODY()

public:
    AR6FrameworkBenchmarkDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    enum class EOperation : uint8 { Harness, Find, Count, ClosedHUD, OpenHUD };
    struct FScenario
    {
        FString Name;
        EOperation Operation;
        int32 InventoryRows;
        int32 TargetExtractions;
    };
    struct FSample
    {
        int32 ScenarioIndex;
        int32 BatchIndex;
        int32 Iterations;
        double ElapsedNanoseconds;
        uint64 Checksum;
    };
    using FOperationFunction = uint64 (*)(ARaidHUD*, const ARaidPlayer*);

    static uint64 InvokeHarness(ARaidHUD* HUD, const ARaidPlayer* Player);
    static uint64 InvokeFind(ARaidHUD* HUD, const ARaidPlayer* Player);
    static uint64 InvokeCount(ARaidHUD* HUD, const ARaidPlayer* Player);
    static uint64 InvokeClosedHUD(ARaidHUD* HUD, const ARaidPlayer* Player);
    static uint64 InvokeOpenHUD(ARaidHUD* HUD, const ARaidPlayer* Player);
    static FOperationFunction GetOperation(EOperation Operation);
    static const TCHAR* GetOperationName(EOperation Operation);

    void IsolateBenchmark();
    bool InitializeScenarios();
    bool PrepareScenario();
    bool ValidateScenario(uint64& ExpectedValue) const;
    FSample TimeBatch(int32 Iterations) const;
    bool WriteResults() const;
    void Finish(bool bSuccess, const FString& Reason);

    TWeakObjectPtr<ARaidHUD> HUD;
    TWeakObjectPtr<ARaidPlayer> Player;
    TArray<TWeakObjectPtr<ARaidExtractionZone>> SpawnedExtractions;
    TArray<FScenario> Scenarios;
    TArray<FSample> Samples;
    TArray<FString> ScenarioMetadata;
    FString OutputDirectory;
    FString RunId;
    double StartedAt = 0.0;
    double ScenarioPreparedAt = 0.0;
    int32 WarmupFrames = 0;
    int32 ScenarioWarmupFrames = 0;
    int32 ScenarioIndex = INDEX_NONE;
    int32 BatchIndex = 0;
    int32 IterationsPerBatch = 0;
    int32 InitialExtractionCount = 0;
    int32 BatchCount = 48;
    mutable uint64 ResultSink = 0;
    bool bEnabled = false;
    bool bStress = false;
    bool bInitialized = false;
    bool bPrepared = false;
    bool bFinished = false;
};
