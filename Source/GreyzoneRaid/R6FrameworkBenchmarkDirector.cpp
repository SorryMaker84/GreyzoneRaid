#include "R6FrameworkBenchmarkDirector.h"

#include "RaidEnemy.h"
#include "RaidExtractionZone.h"
#include "RaidGameMode.h"
#include "RaidHUD.h"
#include "RaidInventoryComponent.h"
#include "RaidPlayer.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UnrealClient.h"

namespace R6FrameworkBenchmark
{
    constexpr double InitialWarmupSeconds = 10.0;
    constexpr int32 InitialWarmupFrames = 120;
    constexpr double ScenarioWarmupSeconds = 0.25;
    constexpr int32 ScenarioWarmupFrames = 8;
    constexpr double TargetBatchNanoseconds = 2000000.0;
    constexpr int32 MaximumIterations = 200000;

    FString JsonString(FString Text)
    {
        Text.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Text.ReplaceInline(TEXT("\""), TEXT("\\\""));
        Text.ReplaceInline(TEXT("\r"), TEXT("\\r"));
        Text.ReplaceInline(TEXT("\n"), TEXT("\\n"));
        Text.ReplaceInline(TEXT("\t"), TEXT("\\t"));
        return TEXT("\"") + Text + TEXT("\"");
    }

    FString CsvString(FString Text)
    {
        Text.ReplaceInline(TEXT("\""), TEXT("\"\""));
        return TEXT("\"") + Text + TEXT("\"");
    }
}

AR6FrameworkBenchmarkDirector::AR6FrameworkBenchmarkDirector()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AR6FrameworkBenchmarkDirector::BeginPlay()
{
    Super::BeginPlay();
    bEnabled = FParse::Param(FCommandLine::Get(), TEXT("RaidFrameworkBenchmark"));
    if (!bEnabled)
    {
        SetActorTickEnabled(false);
        Destroy();
        return;
    }
    StartedAt = FPlatformTime::Seconds();
    bStress = FParse::Param(FCommandLine::Get(), TEXT("RaidBenchmarkStress"));
    FParse::Value(FCommandLine::Get(), TEXT("RaidBenchmarkRunId="), RunId);
    if (RunId.IsEmpty()) RunId = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
    FParse::Value(FCommandLine::Get(), TEXT("RaidBenchmarkOutputDir="), OutputDirectory);
    if (OutputDirectory.IsEmpty())
        OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("R6FrameworkBenchmark"), RunId);
    OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
    if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true)
        || IFileManager::Get().FileExists(*FPaths::Combine(OutputDirectory, TEXT("FrameworkBenchmark.csv"))))
    {
        Finish(false, TEXT("output directory unavailable or already contains benchmark data"));
        return;
    }
    IsolateBenchmark();
    UE_LOG(LogTemp, Display, TEXT("R6_FRAMEWORK_BENCHMARK: actual HUD queries; game-thread elapsed timings including scheduling noise; no FPS inference. Warmup=10s/120frames, batches=%d, stress=%d, output=%s"),
        BatchCount, bStress ? 1 : 0, *OutputDirectory);
}

void AR6FrameworkBenchmarkDirector::IsolateBenchmark()
{
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport()) Viewport->SetIgnoreInput(true);
    // Only the explicitly requested benchmark process is isolated. No damage or
    // input behavior in an ordinary game is changed by this director.
    for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It)
    {
        It->SetActorTickEnabled(false);
        if (UCharacterMovementComponent* Movement = It->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->DisableMovement();
        }
        if (AController* Controller = It->GetController()) Controller->SetActorTickEnabled(false);
    }
}

bool AR6FrameworkBenchmarkDirector::InitializeScenarios()
{
    ARaidPlayer* ActivePlayer = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
    ARaidHUD* ActiveHUD = Controller ? Cast<ARaidHUD>(Controller->GetHUD()) : nullptr;
    ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    if (!ActivePlayer || !ActiveHUD || !Mode) return false;
    if (!ActivePlayer->IsAlive() || Mode->State != ERaidState::Running || ActivePlayer->GetInventoryComponent()->GetEntries().Num() != 0)
    {
        Finish(false, TEXT("expected a live Running game with an initially empty backpack"));
        return false;
    }
    Player = ActivePlayer;
    HUD = ActiveHUD;
    ActivePlayer->CloseLoot();
    ActivePlayer->StopFire();
    ActivePlayer->StopAim();
    ActivePlayer->CancelReload();
    ActivePlayer->SetActorTickEnabled(false);
    ActivePlayer->GetCharacterMovement()->StopMovementImmediately();
    ActivePlayer->GetCharacterMovement()->DisableMovement();
    for (TActorIterator<ARaidExtractionZone> It(GetWorld()); It; ++It)
    {
        ++InitialExtractionCount;
        It->SetActorTickEnabled(false);
        It->SetActorEnableCollision(false);
    }
    if (InitialExtractionCount < 1)
    {
        Finish(false, TEXT("baseline map has no live extraction; refusing a misleading empty-query baseline"));
        return false;
    }
    Scenarios.Add({TEXT("harness_empty"), EOperation::Harness, 0, InitialExtractionCount});
    Scenarios.Add({TEXT("find_normal"), EOperation::Find, 0, InitialExtractionCount});
    Scenarios.Add({TEXT("count_empty"), EOperation::Count, 0, InitialExtractionCount});
    Scenarios.Add({TEXT("count_typical"), EOperation::Count, 3, InitialExtractionCount});
    Scenarios.Add({TEXT("count_full"), EOperation::Count, 8, InitialExtractionCount});
    Scenarios.Add({TEXT("hud_closed_full"), EOperation::ClosedHUD, 8, InitialExtractionCount});
    Scenarios.Add({TEXT("hud_open_full"), EOperation::OpenHUD, 8, InitialExtractionCount});
    if (bStress)
    {
        for (const int32 Count : {32, 256})
        {
            if (Count <= InitialExtractionCount) continue;
            Scenarios.Add({FString::Printf(TEXT("find_stress_%d"), Count), EOperation::Find, 8, Count});
            Scenarios.Add({FString::Printf(TEXT("hud_open_stress_%d"), Count), EOperation::OpenHUD, 8, Count});
        }
    }
    Samples.Reserve(Scenarios.Num() * BatchCount);
    ScenarioMetadata.Reserve(Scenarios.Num());
    ScenarioIndex = 0;
    bInitialized = true;
    return true;
}

bool AR6FrameworkBenchmarkDirector::PrepareScenario()
{
    ARaidPlayer* ActivePlayer = Player.Get();
    const FScenario& Scenario = Scenarios[ScenarioIndex];
    URaidInventoryComponent* Inventory = ActivePlayer->GetInventoryComponent();
    // Fill monotonically through the public gameplay API. No replacement copy
    // of the HUD algorithm or manual inventory counter is used as the subject.
    while (Inventory->GetEntries().Num() < Scenario.InventoryRows)
    {
        const int32 Row = Inventory->GetEntries().Num();
        FRaidItemDefinition Definition;
        Definition.ItemId = Row == 0 ? FName(TEXT("Medkit")) : FName(*FString::Printf(TEXT("BenchmarkItem%d"), Row));
        Definition.DisplayName = TEXT("Benchmark fixture");
        Definition.UnitValue = 1;
        Definition.UseEffect = Row == 0 ? ERaidItemUseEffect::Heal : ERaidItemUseEffect::None;
        Definition.UseAmount = Row == 0 ? 45 : 0;
        if (!Inventory->Grant(Definition, 1, Inventory->GetRevision()).IsSuccess()) return false;
    }
    int32 ExtractionCount = 0;
    for (TActorIterator<ARaidExtractionZone> It(GetWorld()); It; ++It) ++ExtractionCount;
    while (ExtractionCount < Scenario.TargetExtractions)
    {
        FActorSpawnParameters Parameters;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FVector Location = ActivePlayer->GetActorLocation() + FVector(100000.f + ExtractionCount * 1000.f, 100000.f, 0.f);
        ARaidExtractionZone* Extraction = GetWorld()->SpawnActor<ARaidExtractionZone>(Location, FRotator::ZeroRotator, Parameters);
        if (!Extraction) return false;
        Extraction->SetActorTickEnabled(false);
        Extraction->SetActorEnableCollision(false);
        Extraction->SetActorHiddenInGame(true);
        TInlineComponentArray<UActorComponent*> Components(Extraction);
        for (UActorComponent* Component : Components) Component->SetComponentTickEnabled(false);
        SpawnedExtractions.Add(Extraction);
        ++ExtractionCount;
    }
    uint64 Expected = 0;
    if (!ValidateScenario(Expected)) return false;

    int32 WorldSlots = 0;
    int32 WorldActors = 0;
    for (const ULevel* Level : GetWorld()->GetLevels())
    {
        if (!Level) continue;
        WorldSlots += Level->Actors.Num();
        for (const TObjectPtr<AActor>& Actor : Level->Actors) if (IsValid(Actor.Get())) ++WorldActors;
    }
    TArray<UObject*> GlobalExtractions;
    GetObjectsOfClass(ARaidExtractionZone::StaticClass(), GlobalExtractions, true, RF_ClassDefaultObject, EInternalObjectFlags::Garbage);
    TArray<UClass*> DerivedClasses;
    GetDerivedClasses(ARaidExtractionZone::StaticClass(), DerivedClasses, true);
    int32 WorldContexts = GEngine ? GEngine->GetWorldContexts().Num() : 0;
    ScenarioMetadata.Add(FString::Printf(TEXT("    {\"scenario\":%s,\"world_extractions\":%d,\"global_extraction_instances\":%d,\"loaded_derived_extraction_classes\":%d,\"world_actor_slots\":%d,\"valid_world_actors\":%d,\"world_levels\":%d,\"engine_world_contexts\":%d,\"inventory_rows\":%d,\"inventory_items\":%d}"),
        *R6FrameworkBenchmark::JsonString(Scenario.Name), ExtractionCount, GlobalExtractions.Num(), DerivedClasses.Num(),
        WorldSlots, WorldActors, GetWorld()->GetLevels().Num(), WorldContexts, ActivePlayer->GetInventoryComponent()->GetEntries().Num(), ActivePlayer->GetLootCount()));
    ScenarioPreparedAt = FPlatformTime::Seconds();
    ScenarioWarmupFrames = 0;
    IterationsPerBatch = 0;
    BatchIndex = 0;
    bPrepared = true;
    UE_LOG(LogTemp, Display, TEXT("R6_FRAMEWORK_BENCHMARK_SCENARIO: %s; operation=%s; rows=%d; worldExtractions=%d; globalExtractions=%d; worldActorSlots=%d"),
        *Scenario.Name, GetOperationName(Scenario.Operation), Scenario.InventoryRows, ExtractionCount, GlobalExtractions.Num(), WorldSlots);
    return true;
}

bool AR6FrameworkBenchmarkDirector::ValidateScenario(uint64& ExpectedValue) const
{
    const ARaidPlayer* ActivePlayer = Player.Get();
    ARaidHUD* ActiveHUD = HUD.Get();
    if (!ActivePlayer || !ActiveHUD || !ActivePlayer->IsAlive()) return false;
    const FScenario& Scenario = Scenarios[ScenarioIndex];
    const TArray<FRaidInventoryEntry>& Entries = ActivePlayer->GetInventoryComponent()->GetEntries();
    if (Entries.Num() != Scenario.InventoryRows || ActivePlayer->GetLootCount() != Scenario.InventoryRows) return false;
    int32 ExpectedMedkits = 0;
    for (const FRaidInventoryEntry& Entry : Entries)
        if (Entry.Definition.UseEffect == ERaidItemUseEffect::Heal) ExpectedMedkits += Entry.Quantity;
    if (ExpectedMedkits != (Scenario.InventoryRows == 0 ? 0 : 1)) return false;
    if (ActiveHUD->CountMedkits(ActivePlayer) != ExpectedMedkits) return false;
    const ARaidExtractionZone* Actual = ActiveHUD->FindExtraction(ActivePlayer);
    if (!IsValid(Actual)) return false;
    // Match the existing HUD's float distance precision, including equal-distance ties.
    const float ActualDistance = static_cast<float>(FVector::DistSquared(ActivePlayer->GetActorLocation(), Actual->GetActorLocation()));
    int32 Count = 0;
    bool bFoundActual = false;
    for (TActorIterator<ARaidExtractionZone> It(GetWorld()); It; ++It)
    {
        ++Count;
        bFoundActual |= *It == Actual;
        const float Distance = static_cast<float>(FVector::DistSquared(ActivePlayer->GetActorLocation(), It->GetActorLocation()));
        if (Distance < ActualDistance) return false;
    }
    if (!bFoundActual || Count != Scenario.TargetExtractions) return false;
    const uint64 Address = static_cast<uint64>(reinterpret_cast<UPTRINT>(Actual));
    switch (Scenario.Operation)
    {
    case EOperation::Harness: ExpectedValue = 1; break;
    case EOperation::Find: ExpectedValue = Address; break;
    case EOperation::Count: ExpectedValue = static_cast<uint64>(ExpectedMedkits); break;
    case EOperation::ClosedHUD: ExpectedValue = Address + 2 * static_cast<uint64>(ExpectedMedkits); break;
    case EOperation::OpenHUD: ExpectedValue = 2 * Address + 2 * static_cast<uint64>(ExpectedMedkits); break;
    }
    return true;
}

FORCENOINLINE uint64 AR6FrameworkBenchmarkDirector::InvokeHarness(ARaidHUD*, const ARaidPlayer*)
{
    return 1;
}

FORCENOINLINE uint64 AR6FrameworkBenchmarkDirector::InvokeFind(ARaidHUD* ActiveHUD, const ARaidPlayer* ActivePlayer)
{
    return static_cast<uint64>(reinterpret_cast<UPTRINT>(ActiveHUD->FindExtraction(ActivePlayer)));
}

FORCENOINLINE uint64 AR6FrameworkBenchmarkDirector::InvokeCount(ARaidHUD* ActiveHUD, const ARaidPlayer* ActivePlayer)
{
    return static_cast<uint64>(ActiveHUD->CountMedkits(ActivePlayer));
}

FORCENOINLINE uint64 AR6FrameworkBenchmarkDirector::InvokeClosedHUD(ARaidHUD* ActiveHUD, const ARaidPlayer* ActivePlayer)
{
    // Volatile method pointers preserve the two requested calls even when a
    // unity/LTO build can inspect the identical inventory between those calls.
    using FCountMethod = int32 (ARaidHUD::*)(const ARaidPlayer*) const;
    FCountMethod volatile Count = &ARaidHUD::CountMedkits;
    const uint64 Extraction = InvokeFind(ActiveHUD, ActivePlayer);
    const uint64 FirstCount = static_cast<uint64>((ActiveHUD->*Count)(ActivePlayer));
    const uint64 SecondCount = static_cast<uint64>((ActiveHUD->*Count)(ActivePlayer));
    return Extraction + FirstCount + SecondCount;
}

FORCENOINLINE uint64 AR6FrameworkBenchmarkDirector::InvokeOpenHUD(ARaidHUD* ActiveHUD, const ARaidPlayer* ActivePlayer)
{
    using FFindMethod = ARaidExtractionZone* (ARaidHUD::*)(const ARaidPlayer*) const;
    FFindMethod volatile Find = &ARaidHUD::FindExtraction;
    const uint64 ClosedValue = InvokeClosedHUD(ActiveHUD, ActivePlayer);
    return ClosedValue + static_cast<uint64>(reinterpret_cast<UPTRINT>((ActiveHUD->*Find)(ActivePlayer)));
}

AR6FrameworkBenchmarkDirector::FOperationFunction AR6FrameworkBenchmarkDirector::GetOperation(EOperation Operation)
{
    switch (Operation)
    {
    case EOperation::Find: return &InvokeFind;
    case EOperation::Count: return &InvokeCount;
    case EOperation::ClosedHUD: return &InvokeClosedHUD;
    case EOperation::OpenHUD: return &InvokeOpenHUD;
    default: return &InvokeHarness;
    }
}

const TCHAR* AR6FrameworkBenchmarkDirector::GetOperationName(EOperation Operation)
{
    switch (Operation)
    {
    case EOperation::Find: return TEXT("find_extraction");
    case EOperation::Count: return TEXT("count_medkits");
    case EOperation::ClosedHUD: return TEXT("find_x1_count_x2");
    case EOperation::OpenHUD: return TEXT("find_x2_count_x2");
    default: return TEXT("harness_only");
    }
}

AR6FrameworkBenchmarkDirector::FSample AR6FrameworkBenchmarkDirector::TimeBatch(int32 Iterations) const
{
    ARaidHUD* ActiveHUD = HUD.Get();
    const ARaidPlayer* ActivePlayer = Player.Get();
    // A volatile dispatch load prevents the compiler from replacing repeated
    // pure-query results with one call. Its cost remains in the raw samples.
    FOperationFunction volatile Operation = GetOperation(Scenarios[ScenarioIndex].Operation);
    uint64 Checksum = 0;
    const uint64 StartedCycles = FPlatformTime::Cycles64();
    for (int32 Index = 0; Index < Iterations; ++Index) Checksum += Operation(ActiveHUD, ActivePlayer);
    const uint64 ElapsedCycles = FPlatformTime::Cycles64() - StartedCycles;
    ResultSink ^= Checksum;
    return {ScenarioIndex, BatchIndex, Iterations, FPlatformTime::ToSeconds64(ElapsedCycles) * 1.0e9, Checksum};
}

void AR6FrameworkBenchmarkDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bEnabled || bFinished) return;
    const double Now = FPlatformTime::Seconds();
    if (Now - StartedAt > 180.0)
    {
        Finish(false, TEXT("180 second benchmark timeout"));
        return;
    }
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport()) Viewport->SetIgnoreInput(true);
    if (!bInitialized)
    {
        IsolateBenchmark();
        if (!InitializeScenarios()) return;
    }
    ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Player.IsValid() || !HUD.IsValid() || !Player->IsAlive() || !Mode || Mode->State != ERaidState::Running)
    {
        Finish(false, TEXT("game left the valid live Running state"));
        return;
    }
    ++WarmupFrames;
    if (Now - StartedAt < R6FrameworkBenchmark::InitialWarmupSeconds
        || WarmupFrames < R6FrameworkBenchmark::InitialWarmupFrames) return;
    if (!bPrepared)
    {
        if (!PrepareScenario()) Finish(false, TEXT("scenario fixture or query correctness failed"));
        return;
    }
    ++ScenarioWarmupFrames;
    if (Now - ScenarioPreparedAt < R6FrameworkBenchmark::ScenarioWarmupSeconds
        || ScenarioWarmupFrames < R6FrameworkBenchmark::ScenarioWarmupFrames)
    {
        TimeBatch(16);
        return;
    }
    if (IterationsPerBatch == 0)
    {
        const FSample Pilot = TimeBatch(128);
        const double Estimated = 128.0 * R6FrameworkBenchmark::TargetBatchNanoseconds / FMath::Max(1.0, Pilot.ElapsedNanoseconds);
        IterationsPerBatch = FMath::Clamp(static_cast<int32>(FMath::Min(Estimated, static_cast<double>(R6FrameworkBenchmark::MaximumIterations))), 1, R6FrameworkBenchmark::MaximumIterations);
        // Calibration and these extra hot-cache calls are excluded from samples.
        TimeBatch(IterationsPerBatch);
        return;
    }
    uint64 ExpectedValue = 0;
    if (!ValidateScenario(ExpectedValue))
    {
        Finish(false, TEXT("query result or fixture changed while benchmarking"));
        return;
    }
    const FSample Sample = TimeBatch(IterationsPerBatch);
    if (Sample.Checksum != ExpectedValue * static_cast<uint64>(IterationsPerBatch))
    {
        Finish(false, TEXT("timed query checksum differs from validated result"));
        return;
    }
    Samples.Add(Sample);
    if (++BatchIndex < BatchCount) return;
    ++ScenarioIndex;
    bPrepared = false;
    if (ScenarioIndex == Scenarios.Num())
    {
        const bool bSaved = WriteResults();
        Finish(bSaved, bSaved ? TEXT("all query checks and artifact writes passed") : TEXT("failed to write benchmark artifacts"));
    }
}

bool AR6FrameworkBenchmarkDirector::WriteResults() const
{
    using namespace R6FrameworkBenchmark;
    FString Raw = TEXT("run_id,scenario,operation,world_extractions,inventory_rows,batch_index,iterations,elapsed_ns,ns_per_iteration,checksum\n");
    FString Summary = TEXT("run_id,scenario,operation,world_extractions,inventory_rows,batches,iterations_per_batch,min_ns_per_iteration,median_ns_per_iteration,p95_ns_per_iteration,mean_ns_per_iteration,max_ns_per_iteration,checksum\n");
    for (int32 Index = 0; Index < Scenarios.Num(); ++Index)
    {
        const FScenario& Scenario = Scenarios[Index];
        TArray<double> Values;
        double Sum = 0.0;
        uint64 Checksum = 0;
        int32 Iterations = 0;
        for (const FSample& Sample : Samples)
        {
            if (Sample.ScenarioIndex != Index) continue;
            const double Value = Sample.ElapsedNanoseconds / Sample.Iterations;
            Values.Add(Value);
            Sum += Value;
            Checksum += Sample.Checksum;
            Iterations = Sample.Iterations;
            Raw += FString::Printf(TEXT("%s,%s,%s,%d,%d,%d,%d,%.3f,%.6f,%llu\n"),
                *CsvString(RunId), *CsvString(Scenario.Name), GetOperationName(Scenario.Operation), Scenario.TargetExtractions,
                Scenario.InventoryRows, Sample.BatchIndex, Sample.Iterations, Sample.ElapsedNanoseconds, Value,
                static_cast<unsigned long long>(Sample.Checksum));
        }
        if (Values.Num() != BatchCount) return false;
        Values.Sort();
        const double Median = (Values[(Values.Num() - 1) / 2] + Values[Values.Num() / 2]) * 0.5;
        const double P95 = Values[FMath::Clamp(FMath::CeilToInt(Values.Num() * 0.95) - 1, 0, Values.Num() - 1)];
        Summary += FString::Printf(TEXT("%s,%s,%s,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%llu\n"),
            *CsvString(RunId), *CsvString(Scenario.Name), GetOperationName(Scenario.Operation), Scenario.TargetExtractions,
            Scenario.InventoryRows, Values.Num(), Iterations, Values[0], Median, P95, Sum / Values.Num(), Values.Last(),
            static_cast<unsigned long long>(Checksum));
        UE_LOG(LogTemp, Display, TEXT("R6_FRAMEWORK_BENCHMARK_SUMMARY: %s operation=%s batches=%d iterations=%d median_ns_per_iteration=%.6f p95_batch_mean_ns=%.6f"),
            *Scenario.Name, GetOperationName(Scenario.Operation), Values.Num(), Iterations, Median, P95);
    }
    FIntPoint ViewportSize = FIntPoint::ZeroValue;
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        if (Viewport->Viewport) ViewportSize = Viewport->Viewport->GetSizeXY();
    FString Metadata = FString::Printf(TEXT("{\n  \"schema_version\":1,\n  \"run_id\":%s,\n  \"utc\":%s,\n  \"engine_version\":%s,\n  \"build_configuration_enum\":%d,\n  \"with_editor\":%s,\n  \"map\":%s,\n  \"command_line\":%s,\n  \"cpu\":%s,\n  \"viewport_width\":%d,\n  \"viewport_height\":%d,\n  \"stress_enabled\":%s,\n  \"initial_extractions\":%d,\n  \"initial_warmup_seconds\":%.1f,\n  \"initial_warmup_min_frames\":%d,\n  \"batches_per_scenario\":%d,\n  \"target_batch_ns\":%.0f,\n  \"maximum_iterations\":%d,\n  \"result_sink\":\"%llu\",\n"),
        *JsonString(RunId), *JsonString(FDateTime::UtcNow().ToIso8601()), *JsonString(FEngineVersion::Current().ToString()),
        static_cast<int32>(FApp::GetBuildConfiguration()), WITH_EDITOR ? TEXT("true") : TEXT("false"),
        *JsonString(GetWorld()->GetOutermost()->GetName()), *JsonString(FCommandLine::Get()), *JsonString(FPlatformMisc::GetCPUBrand()),
        ViewportSize.X, ViewportSize.Y, bStress ? TEXT("true") : TEXT("false"), InitialExtractionCount, InitialWarmupSeconds,
        InitialWarmupFrames, BatchCount, TargetBatchNanoseconds, MaximumIterations, static_cast<unsigned long long>(ResultSink));
    Metadata += TEXT("  \"measurement\":\"Actual ARaidHUD methods via friend access; complete iterator construction/destruction included. Calibrated repeated hot-cache calls on the game thread; one batch per frame. Windows Cycles64 uses QueryPerformanceCounter: elapsed scope time includes scheduling delays, not pure thread CPU usage. Raw timings include volatile indirect dispatch, wrapper and checksum overhead.\",\n");
    Metadata += TEXT("  \"count_medkits_semantics\":\"The R5 baseline scans inventory ItemId for Medkit on every CountMedkits call. R6 CountMedkits reads an observed-player cache refreshed on binding and inventory change events; that cache sums Quantity for definitions with UseEffect Heal. Fixtures preserve 0/3/8 rows and items with exactly one Medkit/Heal item whenever nonempty, so validated results match on these fixtures. This compares an event-cache query and a changed item predicate, not a same-semantics loop optimization; event/cache refresh work occurs outside timed batches.\",\n");
    Metadata += TEXT("  \"statistics\":\"Each sample is elapsed batch time divided by iterations. p95 is the p95 of batch averages, not the p95 latency of individual HUD frames. Three or more fresh processes should be reported separately and together. Harness is reported separately, never mechanically subtracted.\",\n");
    Metadata += TEXT("  \"isolation\":\"Background enemies, player movement/tick, and extraction tick/collision are stopped; rendering remains enabled. No forced GC or streaming changes during timing. Fixture creation, correctness validation, warmup, logging and export are outside timed batches.\",\n");
    Metadata += TEXT("  \"limitations\":\"Query CPU microbenchmark only; not full HUD rendering, real-frame latency, allocator-count or FPS evidence. Editor and non-Editor use different actor iterator paths. Stress fixtures are synthetic hidden extraction actors; normal backpack capacity remains eight. Cold-cache, zero-extraction, travel, spawn/destruction correctness and hardware-input behavior require separate experiments.\",\n");
    Metadata += TEXT("  \"baseline_hud_calls_per_successful_running_draw\":{\"closed_find\":1,\"closed_count\":2,\"open_find\":2,\"open_count\":2},\n  \"scenarios\":[\n");
    Metadata += FString::Join(ScenarioMetadata, TEXT(",\n"));
    Metadata += TEXT("\n  ]\n}\n");
    return FFileHelper::SaveStringToFile(Raw, *FPaths::Combine(OutputDirectory, TEXT("FrameworkBenchmark.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        && FFileHelper::SaveStringToFile(Summary, *FPaths::Combine(OutputDirectory, TEXT("FrameworkBenchmarkSummary.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        && FFileHelper::SaveStringToFile(Metadata, *FPaths::Combine(OutputDirectory, TEXT("FrameworkBenchmarkMetadata.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void AR6FrameworkBenchmarkDirector::Finish(bool bSuccess, const FString& Reason)
{
    if (bFinished) return;
    bFinished = true;
    SetActorTickEnabled(false);
    UE_LOG(LogTemp, Display, TEXT("RAID_FRAMEWORK_BENCHMARK_RESULT: %s scenarios=%d samples=%d elapsed=%.3fs reason=%s"),
        bSuccess ? TEXT("PASS") : TEXT("FAIL"), Scenarios.Num(), Samples.Num(), FPlatformTime::Seconds() - StartedAt, *Reason);
    FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
}
