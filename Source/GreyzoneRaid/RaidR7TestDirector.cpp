#include "RaidR7TestDirector.h"
#include "RaidTreatmentComponent.h"
#include "RaidPlayer.h"
#include "RaidGameMode.h"
#include "RaidHUD.h"
#include "RaidEnemy.h"
#include "RaidLoot.h"
#include "RaidExtractionZone.h"
#include "RaidMissionObjective.h"
#include "RaidMissionDefinition.h"
#include "RaidInventoryComponent.h"
#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
struct FJourney
{
    FString Id;
    int32 Round = 0;
    int32 Checks = 0;
    int32 Failures = 0;
    int32 Captures = 0;
    int32 LootInteractions = 0;
    bool bPatrolMoved = false;
    bool bChaseMoved = false;
    bool bLiveDamage = false;
    bool bSouthSuccess = false;
    bool bNorthSuccess = false;
    bool bRealDeath = false;
    bool bR10Loaded = false, bR10Healed = false, bR10FoughtAfterHeal = false;
    TWeakObjectPtr<UWorld> PreviousWorld;
    TWeakObjectPtr<ARaidPlayer> PreviousPawn;
    TArray<TSharedPtr<FJsonValue>> CapturedViews;
    TArray<TSharedPtr<FJsonValue>> Walks;
    TArray<FString> Telemetry;
    TSet<FString> CompletedRoutes;
};

FJourney& Journey()
{
    static FJourney State;
    return State;
}

bool ReadVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FVector& Result)
{
    const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Key, Parts) || Parts->Num() != 3) return false;
    double XYZ[3];
    for (int32 Index = 0; Index < 3; ++Index)
        if (!(*Parts)[Index]->TryGetNumber(XYZ[Index]) || !FMath::IsFinite(XYZ[Index])) return false;
    Result = FVector(XYZ[0], XYZ[1], XYZ[2]);
    return true;
}

TArray<TSharedPtr<FJsonValue>> VectorJson(const FVector& Value)
{
    return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y),
        MakeShared<FJsonValueNumber>(Value.Z)};
}

bool SaveJson(const FString& Path, const TSharedRef<FJsonObject>& Object)
{
    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    return FJsonSerializer::Serialize(Object, Writer)
        && FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString OptionalMilliseconds(float Value, bool bAvailable)
{
    return bAvailable && FMath::IsFinite(Value) && Value > 0.f
        ? FString::Printf(TEXT("%.6f"), Value) : FString();
}
}

ARaidR7TestDirector::ARaidR7TestDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    PrimaryActorTick.bHighPriority = true;
}

void ARaidR7TestDirector::BeginPlay()
{
    Super::BeginPlay();
    const bool bPreview = FParse::Param(FCommandLine::Get(), TEXT("RaidR7Preview"));
    const bool bPerformance = FParse::Param(FCommandLine::Get(), TEXT("RaidR7Performance"));
    const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("RaidR7Test"));
    bR10Journey = FParse::Param(FCommandLine::Get(), TEXT("RaidR10Journey"));
    bFinaleOnly = bTest && FParse::Param(FCommandLine::Get(), TEXT("RaidR7FinaleOnly"));
    bEnabled = bPreview || bPerformance || bTest;
    if (!bEnabled) { Destroy(); return; }
    Mode = bPreview ? EMode::Preview : bPerformance ? EMode::Performance : EMode::Test;
    StartedWallSeconds = StageStartedWallSeconds = PreviousTickWallSeconds = FPlatformTime::Seconds();
    StartedUtc = FDateTime::UtcNow().ToIso8601();
    FParse::Value(FCommandLine::Get(), TEXT("RaidR7RunId="), RunId);
    if (RunId.IsEmpty()) RunId = Mode == EMode::Test && !Journey().Id.IsEmpty()
        ? Journey().Id : FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
    if (Mode == EMode::Test)
    {
        if (Journey().Id != RunId) { Journey() = FJourney(); Journey().Id = RunId; }
        Checks = Journey().Checks;
        Failures = Journey().Failures;
        Captures = Journey().Captures;
        ViewEvidence = Journey().CapturedViews;
        TestStepStartedAt = FPlatformTime::Seconds();
    }
    FParse::Value(FCommandLine::Get(), TEXT("RaidR7OutputDir="), OutputDirectory);
    if (OutputDirectory.IsEmpty()) OutputDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Evidence/R7"), RunId);
    OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    ManifestPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Validation/FrontierValidation.json"));
    FParse::Value(FCommandLine::Get(), TEXT("RaidR7Manifest="), ManifestPath);
    FParse::Value(FCommandLine::Get(), TEXT("RaidR7SampleSeconds="), SampleSeconds);
    SampleSeconds = FMath::Clamp(SampleSeconds, 10.f, 120.f);
    IgnoreExternalInput();
    if (static_cast<int32>(bPreview) + static_cast<int32>(bPerformance) + static_cast<int32>(bTest) != 1)
    {
        Fail(TEXT("exactly_one_r7_mode_required"));
        return;
    }
    bManifestLoaded = LoadManifest();
    if (!bManifestLoaded) return;
    UE_LOG(LogTemp, Display, TEXT("R7_RUN_START: run_id=%s mode=%s map=%s metadata=%s output=%s"),
        *RunId, Mode == EMode::Preview ? TEXT("Preview") : Mode == EMode::Performance ? TEXT("Performance") : TEXT("Test"),
        *GetWorld()->GetPackage()->GetName(), *ManifestPath, *OutputDirectory);
    UE_LOG(LogTemp, Display, TEXT("R7_EVIDENCE_SCOPE: preview/performance use fixed-view teleports, not traversability proof; AI/health/damage remain production values; hardware input/audio not tested"));
}

bool ARaidR7TestDirector::LoadManifest()
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *ManifestPath)
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), MapManifest) || !MapManifest.IsValid())
    {
        Fail(TEXT("frontier_validation_metadata_missing_or_invalid"), ManifestPath);
        return false;
    }
    FString ExpectedMap;
    double Schema = 0.0;
    if (!MapManifest->TryGetStringField(TEXT("map"), ExpectedMap)
        || !MapManifest->TryGetNumberField(TEXT("schema_version"), Schema) || Schema != 1.0
        || ExpectedMap != TEXT("/Game/Maps/Frontier")
        || GetWorld()->GetPackage()->GetName() != ExpectedMap)
    {
        Fail(TEXT("frontier_map_identity_mismatch"), GetWorld()->GetPackage()->GetName());
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* JsonViews = nullptr;
    if (!MapManifest->TryGetArrayField(TEXT("preview_views"), JsonViews) || JsonViews->Num() < 5 || JsonViews->Num() > 12)
    {
        Fail(TEXT("metadata_requires_five_to_twelve_authored_preview_views"));
        return false;
    }
    TSet<FString> Ids;
    for (const TSharedPtr<FJsonValue>& Value : *JsonViews)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        FView View;
        double Yaw = 0.0, Pitch = 0.0;
        if (!Object.IsValid() || !Object->TryGetStringField(TEXT("id"), View.Id) || View.Id.IsEmpty()
            || Ids.Contains(View.Id) || View.Id.Contains(TEXT("..")) || View.Id.Contains(TEXT("/")) || View.Id.Contains(TEXT("\\"))
            || !ReadVector(Object, TEXT("location_cm"), View.Location)
            || !Object->TryGetNumberField(TEXT("yaw"), Yaw) || !Object->TryGetNumberField(TEXT("pitch"), Pitch)
            || !FMath::IsFinite(Yaw) || !FMath::IsFinite(Pitch))
        {
            Fail(TEXT("invalid_authored_preview_view"));
            return false;
        }
        Object->TryGetBoolField(TEXT("show_tactical_map"), View.bTacticalMap);
        View.Rotation = FRotator(Pitch, Yaw, 0.f);
        Ids.Add(View.Id);
        Views.Add(View);
    }
    if (Mode == EMode::Performance)
    {
        FString RequestedView;
        FParse::Value(FCommandLine::Get(), TEXT("RaidR7PerformanceView="), RequestedView);
        int32 Selected = 0;
        if (!RequestedView.IsEmpty())
        {
            Selected = Views.IndexOfByPredicate([&RequestedView](const FView& View) { return View.Id == RequestedView; });
            if (Selected == INDEX_NONE) { Fail(TEXT("performance_view_not_in_metadata"), RequestedView); return false; }
        }
        const FView FixedView = Views[Selected];
        Views = {FixedView};
        Views[0].bTacticalMap = false;
    }
    Check(true, TEXT("frontier_map_and_authored_metadata_match"), ExpectedMap);
    return true;
}

void ARaidR7TestDirector::IgnoreExternalInput()
{
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
    {
        Viewport->SetIgnoreInput(true);
        if (!bInputIsolationLogged)
        {
            UE_LOG(LogTemp, Display, TEXT("R7_INPUT: external viewport input ignored; internal gameplay/Canvas calls only"));
            bInputIsolationLogged = true;
        }
    }
}

bool ARaidR7TestDirector::ResolveGameplay()
{
    RaidPawn = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerCharacter(this, 0));
    RaidPC = UGameplayStatics::GetPlayerController(this, 0);
    RaidMode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    RaidHUD = RaidPC ? Cast<ARaidHUD>(RaidPC->GetHUD()) : nullptr;
    return RaidPawn && RaidPC && RaidMode && RaidHUD && RaidPawn->InputComponent && RaidPC->InputComponent;
}

bool ARaidR7TestDirector::ExecuteAction(FName Name, bool bPressed)
{
    UInputComponent* Inputs[] = {RaidPC ? RaidPC->InputComponent.Get() : nullptr,
        RaidPawn ? RaidPawn->InputComponent.Get() : nullptr};
    for (UInputComponent* Input : Inputs)
    {
        if (!Input) continue;
        for (int32 Index = 0; Index < Input->GetNumActionBindings(); ++Index)
        {
            FInputActionBinding& Binding = Input->GetActionBinding(Index);
            if (Binding.GetActionName() == Name && Binding.KeyEvent == (bPressed ? IE_Pressed : IE_Released)
                && Binding.ActionDelegate.IsBound())
            {
                Binding.ActionDelegate.Execute(FKey());
                return true;
            }
        }
        if (Name == TEXT("TacticalMap"))
        {
            for (FInputKeyBinding& Binding : Input->KeyBindings)
            {
                if (Binding.Chord.Key == EKeys::M && Binding.KeyEvent == (bPressed ? IE_Pressed : IE_Released)
                    && Binding.KeyDelegate.IsBound())
                {
                    Binding.KeyDelegate.Execute(EKeys::M);
                    return true;
                }
            }
        }
    }
    return false;
}

bool ARaidR7TestDirector::ExecuteAxis(FName Name, float Value)
{
    UInputComponent* Input = RaidPawn ? RaidPawn->InputComponent.Get() : nullptr;
    if (!Input) return false;
    for (FInputAxisBinding& Binding : Input->AxisBindings)
        if (Binding.AxisName == Name && Binding.AxisDelegate.IsBound()) { Binding.AxisDelegate.Execute(Value); return true; }
    return false;
}

bool ARaidR7TestDirector::SetView(const FView& View)
{
    if (RaidHUD->IsTacticalMapOpen())
        if (!ExecuteAction(TEXT("TacticalMap"))) { Fail(TEXT("tactical_map_input_binding_missing")); return false; }
    RaidPawn->CloseLoot();
    RaidPawn->StopFire();
    RaidPawn->StopAim();
    RaidPawn->GetCharacterMovement()->StopMovementImmediately();
    const UCapsuleComponent* Capsule = RaidPawn->GetCapsuleComponent();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(R7PreviewPlacement), false, RaidPawn);
    if (GetWorld()->OverlapBlockingTestByChannel(View.Location, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() - 1.f, Capsule->GetScaledCapsuleHalfHeight() - 2.f), Params))
    {
        Fail(TEXT("authored_preview_player_capsule_blocked"), View.Id + TEXT(" ") + View.Location.ToCompactString());
        return false;
    }
    RaidPawn->SetActorLocation(View.Location, false, nullptr, ETeleportType::TeleportPhysics);
    RaidPawn->SetActorRotation(FRotator(0.f, View.Rotation.Yaw, 0.f));
    RaidPC->SetControlRotation(View.Rotation);
    RaidPawn->Camera->SetWorldRotation(View.Rotation);
    if (View.bTacticalMap && (!ExecuteAction(TEXT("TacticalMap")) || !RaidHUD->IsTacticalMapOpen()))
    {
        Fail(TEXT("tactical_map_did_not_open_from_real_binding"), View.Id);
        return false;
    }
    bMapVisible = View.bTacticalMap;
    UE_LOG(LogTemp, Display, TEXT("R7_FIXED_VIEW: id=%s player_cm=%s yaw=%.2f pitch=%.2f tactical_map=%d teleport_for_composition=1"),
        *View.Id, *View.Location.ToCompactString(), View.Rotation.Yaw, View.Rotation.Pitch, View.bTacticalMap);
    return true;
}

bool ARaidR7TestDirector::Check(bool bPassed, const TCHAR* Name, const FString& Detail)
{
    ++Checks;
    if (!bPassed) ++Failures;
    UE_LOG(LogTemp, Display, TEXT("R7_CHECK:%s %s | %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Name, *Detail);
    return bPassed;
}

void ARaidR7TestDirector::Fail(const TCHAR* Name, const FString& Detail)
{
    if (bFinished) return;
    Check(false, Name, Detail);
    UE_LOG(LogTemp, Error, TEXT("R7_RESULT: FAIL mode=%s checks=%d failures=%d captures=%d reason=%s"),
        Mode == EMode::Preview ? TEXT("Preview") : Mode == EMode::Performance ? TEXT("Performance") : TEXT("Test"),
        Checks, Failures, Captures, Name);
    WriteReceipt(false);
    bFinished = true;
    SetActorTickEnabled(false);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ARaidR7TestDirector::Advance(EStage Next)
{
    Stage = Next;
    StageStartedWallSeconds = FPlatformTime::Seconds();
}

void ARaidR7TestDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bEnabled || bFinished || !bManifestLoaded) return;
    IgnoreExternalInput();
    const double Now = FPlatformTime::Seconds();
    if (Now - StartedWallSeconds > (Mode == EMode::Test ? 900.0 : 240.0)) { Fail(TEXT("r7_director_timeout")); return; }
    if (!ResolveGameplay())
    {
        if (Mode == EMode::Test && TestStep == ETestStep::Travel)
        {
            if (Now - TestStepStartedAt > 20.0) Fail(TEXT("restart_travel_lost_gameplay_objects"));
            return;
        }
        // An intentional unpossess leaves no PlayerCharacter for one tick.
        if (Mode == EMode::Test && TestStep == ETestStep::ExtractRepossess && OriginalPawn && RaidPC)
        {
            TickTest(DeltaSeconds);
            return;
        }
        if (Now - StartedWallSeconds > 30.0) Fail(TEXT("gameplay_objects_not_ready"));
        return;
    }
    if (Mode != EMode::Test && (!RaidPawn->IsAlive() || RaidMode->State != ERaidState::Running))
    {
        Fail(TEXT("raid_not_alive_and_running"), FString::Printf(TEXT("health=%.1f state=%d"), RaidPawn->Health, static_cast<int32>(RaidMode->State)));
        return;
    }
    if (Stage == EStage::Ready)
    {
        ++WarmupFrames;
#if WITH_EDITOR
        if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
        if (Now - StartedWallSeconds < 2.0 || WarmupFrames < 10) return;
        Check(true, TEXT("normal_running_player_ready_without_health_or_ai_override"));
        Advance(EStage::SetView);
    }
    if (Mode == EMode::Preview) TickPreview(DeltaSeconds);
    else if (Mode == EMode::Performance) TickPerformance(DeltaSeconds);
    else TickTest(DeltaSeconds);
    PreviousTickWallSeconds = Now;
    IgnoreExternalInput();
}

void ARaidR7TestDirector::TickPreview(float DeltaSeconds)
{
    switch (Stage)
    {
    case EStage::SetView:
        if (ViewIndex >= Views.Num()) { Finish(); return; }
        if (SetView(Views[ViewIndex])) Advance(EStage::SettleView);
        break;
    case EStage::SettleView:
        if (FPlatformTime::Seconds() - StageStartedWallSeconds >= .35) RequestCapture();
        break;
    case EStage::Capture:
        if (PollCapture(DeltaSeconds)) { ++ViewIndex; Advance(EStage::SetView); }
        break;
    default: break;
    }
}

void ARaidR7TestDirector::RequestCapture()
{
    const FString Filename = FString::Printf(TEXT("%02d_%s.png"), ViewIndex + 1, *Views[ViewIndex].Id);
    PendingScreenshot = FPaths::Combine(OutputDirectory, Filename);
    PendingCaptureId = Views[ViewIndex].Id;
    if (IFileManager::Get().FileExists(*PendingScreenshot)) { Fail(TEXT("refusing_to_replace_existing_capture"), PendingScreenshot); return; }
    FScreenshotRequest::RequestScreenshot(PendingScreenshot, true, false);
    LastScreenshotBytes = -1;
    StableScreenshotSeconds = 0.f;
    Advance(EStage::Capture);
}

bool ARaidR7TestDirector::PollCapture(float DeltaSeconds)
{
    const int64 Size = IFileManager::Get().FileSize(*PendingScreenshot);
    if (Size > 128 && Size == LastScreenshotBytes) StableScreenshotSeconds += DeltaSeconds;
    else StableScreenshotSeconds = 0.f;
    LastScreenshotBytes = Size;
    if (FPlatformTime::Seconds() - StageStartedWallSeconds > 15.0) { Fail(TEXT("screenshot_write_timeout"), PendingScreenshot); return false; }
    if (StableScreenshotSeconds < .2f) return false;
    const TSharedRef<FJsonObject> Evidence = MakeShared<FJsonObject>();
    Evidence->SetStringField(TEXT("id"), PendingCaptureId);
    Evidence->SetStringField(TEXT("file"), FPaths::GetCleanFilename(PendingScreenshot));
    Evidence->SetArrayField(TEXT("player_location_cm"), VectorJson(RaidPawn->GetActorLocation()));
    Evidence->SetArrayField(TEXT("camera_location_cm"), VectorJson(RaidPawn->Camera->GetComponentLocation()));
    Evidence->SetNumberField(TEXT("yaw"), RaidPC->GetControlRotation().Yaw);
    Evidence->SetNumberField(TEXT("pitch"), RaidPC->GetControlRotation().Pitch);
    Evidence->SetNumberField(TEXT("health"), RaidPawn->Health);
    Evidence->SetBoolField(TEXT("tactical_map_open"), RaidHUD->IsTacticalMapOpen());
    Evidence->SetBoolField(TEXT("teleported_for_fixed_view"), Mode != EMode::Test);
    Evidence->SetStringField(TEXT("captured_utc"), FDateTime::UtcNow().ToIso8601());
    ViewEvidence.Add(MakeShared<FJsonValueObject>(Evidence));
    ++Captures;
    Check(true, Mode == EMode::Test ? TEXT("fresh_gameplay_capture_written") : TEXT("fresh_fixed_view_capture_written"), PendingCaptureId);
    return true;
}

void ARaidR7TestDirector::TickPerformance(float DeltaSeconds)
{
    const double Now = FPlatformTime::Seconds();
    if (Stage == EStage::SetView)
    {
        if (!SetView(Views[0])) return;
        RaidPC->ConsoleCommand(TEXT("stat unit"), false);
        WarmupFrames = 0;
        FrameRows.Add(TEXT("run_id,view,frame,elapsed_s,wall_frame_ms,simulation_delta_ms,engine_frame_ms,game_ms,render_ms,gpu_ms,stat_unit_available,stat_unit_fresh,gpu_available,health,player_x,player_y,player_z"));
        Advance(EStage::SettleView);
        return;
    }
    if (Stage == EStage::SettleView)
    {
        ++WarmupFrames;
        if (Now - StageStartedWallSeconds < 10.0 || WarmupFrames < 120) return;
        SampleStartedWallSeconds = Now;
        PreviousTickWallSeconds = Now;
        Advance(EStage::Sample);
        return;
    }
    if (Stage != EStage::Sample) return;
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const FStatUnitData* Unit = Viewport ? Viewport->GetStatUnitData() : nullptr;
    const bool bAvailable = Unit && Unit->LastTime > 0.0;
    const bool bFresh = bAvailable && Unit->LastTime != PreviousStatTime;
    const bool bGpuAvailable = bFresh && Unit->RawGPUFrameTime[0] > 0.f;
    const FVector Position = RaidPawn->GetActorLocation();
    FrameRows.Add(FString::Printf(TEXT("%s,%s,%d,%.6f,%.6f,%.6f,%s,%s,%s,%s,%d,%d,%d,%.3f,%.3f,%.3f,%.3f"),
        *RunId, *Views[0].Id, SampleFrames, Now - SampleStartedWallSeconds, (Now - PreviousTickWallSeconds) * 1000.0,
        DeltaSeconds * 1000.0, *OptionalMilliseconds(Unit ? Unit->RawFrameTime : 0.f, bFresh),
        *OptionalMilliseconds(Unit ? Unit->RawGameThreadTime : 0.f, bFresh),
        *OptionalMilliseconds(Unit ? Unit->RawRenderThreadTime : 0.f, bFresh),
        *OptionalMilliseconds(Unit ? Unit->RawGPUFrameTime[0] : 0.f, bGpuAvailable),
        bAvailable, bFresh, bGpuAvailable, RaidPawn->Health, Position.X, Position.Y, Position.Z));
    if (Unit) PreviousStatTime = Unit->LastTime;
    ++SampleFrames;
    if (Now - SampleStartedWallSeconds >= SampleSeconds)
    {
        if (SampleFrames < 120) { Fail(TEXT("insufficient_performance_frame_samples")); return; }
        if (!FFileHelper::SaveStringArrayToFile(FrameRows, *FPaths::Combine(OutputDirectory, TEXT("FrameTimes.csv")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { Fail(TEXT("frame_csv_write_failed")); return; }
        Check(true, TEXT("raw_frame_times_recorded_without_speed_assertion"), FString::Printf(TEXT("frames=%d seconds=%.3f"), SampleFrames, Now - SampleStartedWallSeconds));
        Finish();
    }
}

AActor* ARaidR7TestDirector::FindTaggedActor(const FString& Tag) const
{
    AActor* Found = nullptr;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(FName(*Tag)))
        {
            if (Found) return nullptr;
            Found = *It;
        }
    return Found;
}

void ARaidR7TestDirector::SetTestStep(ETestStep Next)
{
    TestStep = Next;
    TestStepStartedAt = FPlatformTime::Seconds();
}

void ARaidR7TestDirector::AimAt(const FVector& Target)
{
    const FRotator Rotation = (Target - RaidPawn->Camera->GetComponentLocation()).Rotation();
    RaidPC->SetControlRotation(Rotation);
    RaidPawn->Camera->SetWorldRotation(Rotation);
}

bool ARaidR7TestDirector::PrepareTestWorld()
{
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!MapManifest->TryGetArrayField(TEXT("loot"), Entries) || Entries->Num() != 24)
    { Fail(TEXT("metadata_requires_twenty_four_map_containers")); return false; }
    for (const TSharedPtr<FJsonValue>& Value : *Entries)
    {
        FString Tag;
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        ARaidLoot* Loot = Object && Object->TryGetStringField(TEXT("actor_tag"), Tag)
            ? Cast<ARaidLoot>(FindTaggedActor(Tag)) : nullptr;
        if (!Loot || Loot->bEnemyDrop) { Fail(TEXT("authored_container_tag_missing_or_ambiguous"), Tag); return false; }
        MapLoot.Add(Loot);
    }
    if (!MapManifest->TryGetArrayField(TEXT("guards"), Entries) || Entries->Num() != 12)
    { Fail(TEXT("metadata_requires_twelve_live_map_guards")); return false; }
    for (const TSharedPtr<FJsonValue>& Value : *Entries)
    {
        FString Tag;
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        ARaidEnemy* Enemy = Object && Object->TryGetStringField(TEXT("actor_tag"), Tag)
            ? Cast<ARaidEnemy>(FindTaggedActor(Tag)) : nullptr;
        if (!Enemy || Enemy->Health <= 0.f || !Enemy->IsActorTickEnabled()
            || !Enemy->GetCharacterMovement()->IsComponentTickEnabled() || !Cast<AAIController>(Enemy->GetController()))
        { Fail(TEXT("authored_guard_is_not_live_production_ai"), Tag); return false; }
        MapEnemies.Add(Enemy);
        PatrolOrigins.Add(Enemy, Enemy->GetActorLocation());
    }
    TestObjective = Cast<ARaidMissionObjective>(FindTaggedActor(TEXT("R7Id:FrontierIntel")));
    SouthExit = Cast<ARaidExtractionZone>(FindTaggedActor(TEXT("R7Id:S-01")));
    NorthExit = Cast<ARaidExtractionZone>(FindTaggedActor(TEXT("R7Id:N-02")));
    if (!TestObjective || !TestObjective->HasValidDefinition() || !SouthExit || !NorthExit)
    { Fail(TEXT("production_objective_and_two_exit_tags_required")); return false; }
    const TSharedPtr<FJsonObject>* Mission = nullptr;
    if (!MapManifest->TryGetObjectField(TEXT("mission"), Mission)
        || !ReadVector(*Mission, TEXT("interaction_stand_cm"), MissionStand))
    { Fail(TEXT("mission_stand_missing_from_metadata")); return false; }
    MissionStand.Z += RaidPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (!MapManifest->TryGetArrayField(TEXT("routes"), Entries) || Entries->Num() < 8)
    { Fail(TEXT("eight_authored_routes_required")); return false; }
    for (const TSharedPtr<FJsonValue>& Value : *Entries)
    {
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        FRoute Route;
        const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
        if (!Object || !Object->TryGetStringField(TEXT("id"), Route.Id)
            || !Object->TryGetArrayField(TEXT("waypoints_cm"), Points) || Points->Num() < 2)
        { Fail(TEXT("invalid_authored_walk_route")); return false; }
        for (int32 Index = 0; Index < Points->Num(); ++Index)
        {
            const TArray<TSharedPtr<FJsonValue>>* Coordinates = nullptr;
            if (!(*Points)[Index]->TryGetArray(Coordinates) || Coordinates->Num() != 3)
            { Fail(TEXT("invalid_route_coordinate"), Route.Id); return false; }
            double X, Y, Z;
            if (!(*Coordinates)[0]->TryGetNumber(X) || !(*Coordinates)[1]->TryGetNumber(Y) || !(*Coordinates)[2]->TryGetNumber(Z))
            { Fail(TEXT("non_numeric_route_coordinate"), Route.Id); return false; }
            const FVector Point(X, Y, Z);
            AActor* Anchor = FindTaggedActor(FString::Printf(TEXT("R7Route:%s:%d"), *Route.Id, Index));
            if (!Anchor || FVector::Dist(Anchor->GetActorLocation(), Point) > 5.f)
            { Fail(TEXT("cooked_route_anchor_tag_does_not_match_metadata"), Route.Id); return false; }
            Route.FloorPoints.Add(Point);
        }
        Routes.Add(MoveTemp(Route));
    }
    const FRaidMissionSnapshot Snapshot = RaidMode->GetMissionSnapshot();
    if (!Check(Snapshot.bPresent && !Snapshot.bCompleted && !Snapshot.bInProgress && FMath::IsNearlyZero(Snapshot.Progress)
        && FMath::IsNearlyEqual(Snapshot.HoldSeconds, 6.f), TEXT("new_raid_objective_starts_locked_with_six_second_hold"))) return false;
    if (!Check(SouthExit->IsAvailable() && !NorthExit->IsAvailable() && SouthExit->RequiredMissionId.IsNone()
        && NorthExit->RequiredMissionId == TEXT("FrontierIntel"), TEXT("south_free_north_requires_production_mission"))) return false;
    if (!Check(RaidPawn->GetLootCount() == 0 && FMath::IsNearlyEqual(RaidPawn->Health, 100.f)
        && !RaidPawn->IsInventoryOpen() && !RaidPawn->IsSearching(), TEXT("new_raid_inventory_health_and_interaction_are_fresh"))) return false;
    if (Journey().Round > 0)
        if (!Check(GetWorld() != Journey().PreviousWorld.Get() && RaidPawn != Journey().PreviousPawn.Get(),
            TEXT("restart_created_distinct_world_and_pawn"), FString::Printf(TEXT("round=%d"), Journey().Round + 1))) return false;
    RaidPawn->OnTakeAnyDamage.AddDynamic(this, &ARaidR7TestDirector::ObserveDamage);
    Check(true, TEXT("authored_entities_resolved_by_cooked_tags_not_editor_labels"), TEXT("24 containers; 12 live guards; two exits; objective; eight route anchor chains"));
    return true;
}

bool ARaidR7TestDirector::ProbePath(const FVector& From, const FVector& To, const FString& Label)
{
    UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, From, To, RaidPawn);
    const bool bValid = Path && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() >= 2
        && FVector::Dist2D(Path->PathPoints.Last(), To) <= 100.f && FMath::Abs(Path->PathPoints.Last().Z - To.Z) < 125.f;
    if (!Check(bValid, TEXT("technical_nav_path_complete"), Label)) return false;
    return true;
}

bool ARaidR7TestDirector::AuditNavigation()
{
    for (const FRoute& Route : Routes)
        for (int32 Index = 1; Index < Route.FloorPoints.Num(); ++Index)
            if (!ProbePath(Route.FloorPoints[Index - 1], Route.FloorPoints[Index],
                FString::Printf(TEXT("route=%s segment=%d"), *Route.Id, Index))) return false;
    for (const TWeakObjectPtr<ARaidEnemy>& Ref : MapEnemies)
    {
        const ARaidEnemy* Enemy = Ref.Get();
        if (!Enemy || Enemy->PatrolPoints.IsEmpty()) { Check(false, TEXT("guard_has_authored_patrol_or_post")); return false; }
        if (Enemy->PatrolPoints.Num() == 1)
        {
            // The mezzanine and upper terminal guards intentionally hold a single
            // authored post; validate its nav placement, not a zero-length path.
            UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
            FNavLocation Post;
            const FVector Position = Enemy->PatrolPoints[0];
            const bool bValidPost = Nav && Nav->ProjectPointToNavigation(Position, Post, FVector(80.f, 80.f, 150.f))
                && FVector::Dist2D(Post.Location, Position) < 80.f && FMath::Abs(Post.Location.Z - Position.Z) < 125.f;
            if (!Check(bValidPost, TEXT("authored_guard_post_is_on_navigation"), Enemy->GetName())) return false;
            continue;
        }
        for (int32 Index = 0; Index < Enemy->PatrolPoints.Num(); ++Index)
            if (!ProbePath(Enemy->PatrolPoints[Index], Enemy->PatrolPoints[(Index + 1) % Enemy->PatrolPoints.Num()],
                FString::Printf(TEXT("guard=%s patrol=%d"), *Enemy->GetName(), Index))) return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
    if (!MapManifest->TryGetArrayField(TEXT("buildings"), Buildings)) return false;
    const float HalfHeight = RaidPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Radius = RaidPawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(R7DoorClearance), false, RaidPawn);
    for (const TSharedPtr<FJsonValue>& Value : *Buildings)
    {
        const TSharedPtr<FJsonObject> Building = Value->AsObject();
        const TArray<TSharedPtr<FJsonValue>>* Doors = nullptr;
        if (!Building || !Building->TryGetArrayField(TEXT("doors"), Doors)) return false;
        for (const TSharedPtr<FJsonValue>& DoorValue : *Doors)
        {
            FVector Position;
            const TSharedPtr<FJsonObject> Door = DoorValue->AsObject();
            double Width = 0.0, Height = 0.0;
            if (!ReadVector(Door, TEXT("location_cm"), Position)
                || !Door->TryGetNumberField(TEXT("width_cm"), Width) || !Door->TryGetNumberField(TEXT("height_cm"), Height)) return false;
            Position.Z += HalfHeight;
            const bool bClear = Width >= Radius * 2.f + 10.f && Height >= HalfHeight * 2.f + 10.f
                && !GetWorld()->OverlapBlockingTestByChannel(Position, FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeCapsule(Radius - 1.f, HalfHeight - 2.f), Params);
            if (!Check(bClear, TEXT("technical_door_capsule_clearance"), Position.ToCompactString())) return false;
        }
    }
    for (const TWeakObjectPtr<ARaidLoot>& Ref : MapLoot)
    {
        FVector Approach;
        if (!FindLootApproach(Ref.Get(), Approach)
            || !ProbePath(RaidPawn->GetActorLocation(), Approach, TEXT("container=") + Ref->GetName())) return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Ramps = nullptr;
    if (!MapManifest->TryGetArrayField(TEXT("ramps"), Ramps) || Ramps->IsEmpty()) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Ramps)
    {
        const TSharedPtr<FJsonObject> Ramp = Value->AsObject();
        FVector Bottom, Top;
        double Slope = 0.0, Width = 0.0;
        if (!ReadVector(Ramp, TEXT("bottom_cm"), Bottom) || !ReadVector(Ramp, TEXT("top_cm"), Top)
            || !Ramp->TryGetNumberField(TEXT("slope_degrees"), Slope) || !Ramp->TryGetNumberField(TEXT("width_cm"), Width)) return false;
        if (!Check(Slope <= RaidPawn->GetCharacterMovement()->GetWalkableFloorAngle() && Width >= Radius * 2.f + 10.f,
            TEXT("authored_ramp_slope_and_width_fit_real_player"), Bottom.ToCompactString())
            || !ProbePath(Bottom, Top, TEXT("authored_ramp_bottom_to_top"))) return false;
    }
    Check(true, TEXT("technical_navigation_audit_is_not_player_traversal_proof"));
    return true;
}

bool ARaidR7TestDirector::FindLootApproach(ARaidLoot* Loot, FVector& Centre) const
{
    if (!Loot) return false;
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav) return false;
    float Best = TNumericLimits<float>::Max();
    const FVector Target = Loot->CollisionBox->GetComponentLocation();
    for (int32 Index = 0; Index < 16; ++Index)
    {
        const float Angle = 2.f * PI * Index / 16.f;
        const FVector Candidate = Target + FVector(FMath::Cos(Angle) * 155.f, FMath::Sin(Angle) * 155.f, 0.f);
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(50.f, 50.f, 180.f))) continue;
        const FVector Position = Projected.Location + FVector(0.f, 0.f, RaidPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        FCollisionQueryParams Params(SCENE_QUERY_STAT(R7ContainerApproach), false, RaidPawn);
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, Position + FVector(0.f, 0.f, 64.f), Target, ECC_Visibility, Params)
            || Hit.GetActor() != Loot) continue;
        const float Distance = FVector::DistSquared2D(Position, RaidPawn->GetActorLocation());
        if (Distance < Best) { Best = Distance; Centre = Position; }
    }
    return Best < TNumericLimits<float>::Max();
}

bool ARaidR7TestDirector::StartWalk(const FVector& Destination, const FString& Label, ETestStep Next, float Radius)
{
    RaidPawn->CloseLoot();
    if (RaidHUD->IsTacticalMapOpen()) ExecuteAction(TEXT("TacticalMap"));
    if (FVector::Dist2D(RaidPawn->GetActorLocation(), Destination) <= Radius
        && FMath::Abs(RaidPawn->GetActorLocation().Z - Destination.Z) < 115.f)
    {
        Check(true, TEXT("walk_checkpoint_already_reached_by_previous_segment"), Label);
        SetTestStep(Next);
        return true;
    }
    UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, RaidPawn->GetActorLocation(), Destination, RaidPawn);
    if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2)
    { Fail(TEXT("actual_player_walk_has_no_complete_path"), Label); return false; }
    WalkPoints = Path->PathPoints;
    WalkPointIndex = 1;
    WalkDestination = Destination;
    WalkLabel = Label;
    ArrivalRadius = Radius;
    AfterWalk = Next;
    WalkDistance = 0.0;
    PlannedWalkLength = 0.0;
    for (int32 Index = 1; Index < WalkPoints.Num(); ++Index) PlannedWalkLength += FVector::Dist(WalkPoints[Index - 1], WalkPoints[Index]);
    PreviousWalkPosition = WalkProgressPosition = RaidPawn->GetActorLocation();
    WalkStartedAt = WalkProgressAt = WalkTelemetryAt = FPlatformTime::Seconds();
    SetTestStep(ETestStep::Walk);
    UE_LOG(LogTemp, Display, TEXT("R7_WALK_BEGIN: round=%d label=%s planned_m=%.2f from=%s to=%s method=player_input_bindings ai_live=1"),
        Journey().Round + 1, *Label, PlannedWalkLength / 100.0, *PreviousWalkPosition.ToCompactString(), *Destination.ToCompactString());
    return true;
}

void ARaidR7TestDirector::ObserveEnemies()
{
    for (const TWeakObjectPtr<ARaidEnemy>& Ref : MapEnemies)
    {
        ARaidEnemy* Enemy = Ref.Get();
        if (!Enemy || Enemy->Health <= 0.f) continue;
        if (Enemy->StateText == TEXT("Patrol") && PatrolOrigins.Contains(Ref)
            && FVector::Dist2D(PatrolOrigins[Ref], Enemy->GetActorLocation()) > 80.f) Journey().bPatrolMoved = true;
        if (Enemy->StateText == TEXT("Chase"))
        {
            if (!ChaseOrigins.Contains(Ref)) ChaseOrigins.Add(Ref, Enemy->GetActorLocation());
            else if (FVector::Dist2D(ChaseOrigins[Ref], Enemy->GetActorLocation()) > 80.f) Journey().bChaseMoved = true;
        }
    }
    if (bPendingLiveDamage && RaidPawn && RaidPawn->Health < LiveDamageHealthBefore)
    {
        Journey().bLiveDamage = true;
        bPendingLiveDamage = false;
    }
}

void ARaidR7TestDirector::ObserveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
    AController* InstigatedBy, AActor* DamageCauser)
{
    if (DamagedActor != RaidPawn || Damage <= 0.f) return;
    ARaidEnemy* Enemy = Cast<ARaidEnemy>(DamageCauser);
    bLastDamageFromMapAI = Enemy && MapEnemies.Contains(Enemy);
    if (!bLastDamageFromMapAI) return;
    ++LiveDamageEvents;
    LiveDamageHealthBefore = RaidPawn->Health;
    bPendingLiveDamage = true;
    UE_LOG(LogTemp, Display, TEXT("R7_REAL_AI_DAMAGE: round=%d source=%s state=%s raw_damage=%.2f health_before=%.2f injected=0"),
        Journey().Round + 1, *Enemy->GetName(), *Enemy->StateText, Damage, LiveDamageHealthBefore);
}

bool ARaidR7TestDirector::UpdateCombat()
{
    if (!RaidPawn->CanAct() || RaidPawn->IsInventoryOpen() || Journey().Round >= 2) return false;
    if (bSeekingR10Cover) { RaidPawn->StopFire(); bTestFiring=false; return false; }
    if (bR10Journey && Journey().Round==0 && !Journey().bR10Healed && Journey().bR10Loaded
        && Journey().bLiveDamage && RaidPawn->Health<100.f && TestStep==ETestStep::Walk)
        if (SeekR10Cover()) return true;
    ARaidEnemy* Threat = nullptr;
    float Nearest = 1800.f;
    const FVector Eye = RaidPawn->Camera->GetComponentLocation();
    for (const TWeakObjectPtr<ARaidEnemy>& Ref : MapEnemies)
    {
        ARaidEnemy* Enemy = Ref.Get();
        if (!Enemy || Enemy->Health <= 0.f) continue;
        const float Distance = FVector::Dist(Eye, Enemy->GetActorLocation());
        const float EngagementRange = Journey().bLiveDamage ? 1800.f : FMath::Min(1800.f, Enemy->DetectionRadius * .95f);
        if (Distance >= Nearest || Distance >= EngagementRange) continue;
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(R7LiveCombat), false, RaidPawn);
        if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Enemy->GetActorLocation() + FVector(0.f, 0.f, 30.f), ECC_Visibility, Params)
            && Hit.GetActor() == Enemy) { Threat = Enemy; Nearest = Distance; }
    }
    if (!Threat)
    {
        if (!bR10Journey && RaidPawn->Health <= 65.f && !RaidPawn->IsTreating())
            for (const FRaidInventoryEntry& Entry : RaidPawn->GetInventoryComponent()->GetEntries())
                if (Entry.Definition.UseEffect == ERaidItemUseEffect::Heal && Entry.Quantity > 0)
                { ExecuteAction(TEXT("Heal")); break; }
        if (RaidPawn->IsTreating()) { bTestFiring=false; return true; }
        if (bTestFiring) { ExecuteAction(TEXT("Fire"), false); bTestFiring = false; }
        return false;
    }
    AimAt(Threat->GetActorLocation() + FVector(0.f, 0.f, 30.f));
    if (RaidPawn->MagazineAmmo == 0 && !RaidPawn->bReloading)
    {
        if (RaidPawn->ReserveAmmo == 0)
            for (int32 Index = 0; Index < RaidPawn->GetInventoryComponent()->GetEntries().Num(); ++Index)
                if (RaidPawn->GetInventoryComponent()->GetEntries()[Index].Definition.UseEffect == ERaidItemUseEffect::Ammo)
                { RaidPawn->UseInventoryItem(Index); break; }
        ExecuteAction(TEXT("Reload"));
        bTestFiring = false; // Reload clears the production fire timer.
    }
    // Let the first genuine contact demonstrate pursuit and actual damage before
    // the route driver fights back. No enemy range, damage or player HP is altered.
    const bool bMayShoot = Journey().bLiveDamage || RaidPawn->Health <= 50.f;
    if (bMayShoot && !bTestFiring && !RaidPawn->bReloading)
    {
        const int32 BeforeAmmo=RaidPawn->MagazineAmmo;
        ExecuteAction(TEXT("Fire")); bTestFiring = true;
        if (bR10Journey && Journey().Round==0 && Journey().bR10Healed && RaidPawn->MagazineAmmo < BeforeAmmo)
            Journey().bR10FoughtAfterHeal=true;
    }
    return true;
}

void ARaidR7TestDirector::TickWalk(float DeltaSeconds)
{
    const double Now = FPlatformTime::Seconds();
    const FVector Position = RaidPawn->GetActorLocation();
    const float Moved = FVector::Dist(Position, PreviousWalkPosition);
    if (Moved > FMath::Max(250.f, DeltaSeconds * 2000.f)) { Fail(TEXT("unexpected_teleport_during_input_walk"), WalkLabel); return; }
    WalkDistance += Moved;
    PreviousWalkPosition = Position;
    if (FVector::Dist2D(Position, WalkProgressPosition) > 35.f)
    { WalkProgressPosition = Position; WalkProgressAt = Now; }
    if (Now - WalkTelemetryAt >= .2)
    {
        Journey().Telemetry.Add(FString::Printf(TEXT("%s,%d,%s,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f"),
            *RunId, Journey().Round + 1, *WalkLabel, Now - WalkStartedAt, Position.X, Position.Y, Position.Z, WalkDistance / 100.0, RaidPawn->Health));
        WalkTelemetryAt = Now;
    }
    const float HalfHeight = RaidPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    // This separate approach seeks live AI contact, not an authored route endpoint.
    // Once a real guard has hit the unarmed player, stop and let the death scenario
    // run instead of pushing past the guard/cover until the navigation watchdog fires.
    const bool bDeathContact = Journey().Round == 2 && LiveDamageEvents > DeathWalkDamageBaseline && bLastDamageFromMapAI;
    if (bDeathContact || (FVector::Dist2D(Position, WalkDestination) <= ArrivalRadius
        && FMath::Abs(Position.Z - WalkDestination.Z) < 115.f))
    {
        ExecuteAxis(TEXT("MoveForward"), 0.f); ExecuteAxis(TEXT("MoveRight"), 0.f);
        RaidPawn->GetCharacterMovement()->StopMovementImmediately();
        const TSharedRef<FJsonObject> Walk = MakeShared<FJsonObject>();
        Walk->SetStringField(TEXT("label"), WalkLabel); Walk->SetNumberField(TEXT("round"), Journey().Round + 1);
        Walk->SetNumberField(TEXT("planned_m"), PlannedWalkLength / 100.0); Walk->SetNumberField(TEXT("actual_m"), WalkDistance / 100.0);
        Walk->SetNumberField(TEXT("seconds"), Now - WalkStartedAt); Walk->SetBoolField(TEXT("teleported"), false);
        Walk->SetArrayField(TEXT("arrived_cm"), VectorJson(Position)); Journey().Walks.Add(MakeShared<FJsonValueObject>(Walk));
        Walk->SetBoolField(TEXT("stopped_on_actual_ai_contact"), bDeathContact);
        Check(true, bDeathContact ? TEXT("unarmed_approach_stops_after_actual_ai_damage") : TEXT("actual_player_input_walk_arrived"),
            FString::Printf(TEXT("%s metres=%.2f seconds=%.2f"), *WalkLabel, WalkDistance / 100.0, Now - WalkStartedAt));
        SetTestStep(AfterWalk);
        return;
    }
    if (Now - WalkProgressAt > 8.0 || Now - WalkStartedAt > FMath::Max(30.0, PlannedWalkLength / 100.0 + 20.0))
    { Fail(TEXT("actual_player_route_stuck_or_timed_out"), WalkLabel + TEXT(" at ") + Position.ToCompactString()); return; }
    while (WalkPointIndex < WalkPoints.Num() - 1 && FVector::Dist2D(Position, WalkPoints[WalkPointIndex]) < 20.f
        && FMath::Abs(Position.Z - WalkPoints[WalkPointIndex].Z - HalfHeight) < 120.f) ++WalkPointIndex;
    const FVector Target = WalkPointIndex < WalkPoints.Num() - 1 ? WalkPoints[WalkPointIndex] : WalkDestination;
    const FVector Direction = (Target - Position).GetSafeNormal2D();
    const float InputScale = FMath::Clamp(FVector::Dist2D(Target, Position) / 100.f, .25f, 1.f);
    const bool bThreat = UpdateCombat();
    if (!bThreat) RaidPC->SetControlRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
    const FRotationMatrix Basis(FRotator(0.f, RaidPC->GetControlRotation().Yaw, 0.f));
    if (!ExecuteAxis(TEXT("MoveForward"), InputScale * FVector::DotProduct(Direction, Basis.GetUnitAxis(EAxis::X)))
        || !ExecuteAxis(TEXT("MoveRight"), InputScale * FVector::DotProduct(Direction, Basis.GetUnitAxis(EAxis::Y))))
        Fail(TEXT("real_player_movement_axis_binding_missing"));
}

bool ARaidR7TestDirector::ClickButton(FName Name)
{
    const FHUDHitBox* Box = RaidHUD->GetHitBoxWithName(Name);
    if (!Box) { Fail(TEXT("r7_required_canvas_button_missing"), Name.ToString()); return false; }
    int32 Width = 0, Height = 0;
    RaidPC->GetViewportSize(Width, Height);
    FVector2D Low(Width, Height), High(-1.f, -1.f);
    for (int32 Y = 4; Y < Height; Y += 8)
        for (int32 X = 4; X < Width; X += 8)
            if (Box->Contains(FVector2D(X, Y)))
            { Low.X = FMath::Min(Low.X, static_cast<double>(X)); Low.Y = FMath::Min(Low.Y, static_cast<double>(Y));
              High.X = FMath::Max(High.X, static_cast<double>(X)); High.Y = FMath::Max(High.Y, static_cast<double>(Y)); }
    const FVector2D Point = (Low + High) * .5;
    const FHUDHitBox* Routed = High.X >= 0.f ? RaidHUD->GetHitBoxAtCoordinates(Point, true) : nullptr;
    if (!Routed || Routed->GetName() != Name || !RaidHUD->UpdateAndDispatchHitBoxClickEvents(Point, IE_Pressed))
    { Fail(TEXT("r7_real_canvas_dispatch_failed"), Name.ToString()); return false; }
    return Check(true, TEXT("r7_real_canvas_button_dispatched"), Name.ToString());
}

void ARaidR7TestDirector::CaptureTest(const FString& Name, ETestStep Next)
{
    PendingCaptureId = Name;
    PendingScreenshot = FPaths::Combine(OutputDirectory, Name + TEXT(".png"));
    if (IFileManager::Get().FileExists(*PendingScreenshot)) { Fail(TEXT("refusing_to_replace_existing_gameplay_capture"), Name); return; }
    FScreenshotRequest::RequestScreenshot(PendingScreenshot, true, false);
    LastScreenshotBytes = -1; StableScreenshotSeconds = 0.f;
    StageStartedWallSeconds = FPlatformTime::Seconds();
    AfterTestCapture = Next;
    SetTestStep(ETestStep::Capture);
}

void ARaidR7TestDirector::RestartTestRound()
{
    Journey().Checks = Checks; Journey().Failures = Failures; Journey().Captures = Captures;
    Journey().CapturedViews = ViewEvidence;
    Journey().PreviousWorld = GetWorld(); Journey().PreviousPawn = RaidPawn;
    ++Journey().Round;
    WriteReceipt(true);
    SetTestStep(ETestStep::Travel);
    if (!ExecuteAction(TEXT("Restart"))) Fail(TEXT("restart_input_binding_missing"));
}

void ARaidR7TestDirector::TickTest(float DeltaSeconds)
{
    const double Now = FPlatformTime::Seconds();
    const double StepTime = Now - TestStepStartedAt;
    ARaidExtractionZone* ActiveExit = Journey().Round == 0 ? SouthExit.Get() : NorthExit.Get();
    if (TestStep == ETestStep::ExtractRepossess && !RaidPawn && OriginalPawn && RaidPC)
    {
        if (StepTime < .15) return;
        if (!Check(ActiveExit && ActiveExit->GetSecondsRemaining() >= 7.8f,
            TEXT("unpossessed_player_resets_partial_extraction"))) { Fail(TEXT("unpossess_countdown_reset_failed")); return; }
        RaidPC->Possess(OriginalPawn);
        RaidPawn = OriginalPawn;
        bTestFiring = false; // Possession intentionally releases world input.
        SetTestStep(ETestStep::ExtractSwap);
        return;
    }
    if (!RaidPawn || !RaidMode) { Fail(TEXT("test_pawn_or_mode_unexpectedly_missing")); return; }
    ObserveEnemies();
    if (TestStep == ETestStep::Travel)
    {
        if (StepTime > 20.0) Fail(TEXT("restart_did_not_travel_to_fresh_frontier"));
        return;
    }
    if (!RaidPawn->IsAlive() || RaidMode->State == ERaidState::Dead)
    {
        if (Journey().Round == 2 && TestStep != ETestStep::Capture && TestStep != ETestStep::Result)
        {
            if (!Check(Journey().bLiveDamage && LiveDamageEvents > 0 && bLastDamageFromMapAI
                && RaidPawn->Health <= 0.f && RaidMode->State == ERaidState::Dead,
                TEXT("normal_map_ai_causes_real_player_death"))) { Fail(TEXT("death_not_proven_by_live_ai")); return; }
            Journey().bRealDeath = true;
            Check(RaidPawn->DeathCause.Contains(TEXT("巡逻队射击")) && RaidPawn->DeathCause.Contains(TEXT("米")),
                TEXT("real_ai_death_result_records_source_and_distance"));
            CaptureTest(TEXT("r3_12_real_ai_death"), ETestStep::Result);
            return;
        }
        if (!(Journey().Round == 2 && (TestStep == ETestStep::Capture || TestStep == ETestStep::Result)))
        { Fail(TEXT("player_died_during_required_live_route"), WalkLabel); return; }
    }
    if (RaidMode->State == ERaidState::Extracted && TestStep != ETestStep::ExtractFinish
        && TestStep != ETestStep::Capture && TestStep != ETestStep::Result)
    { Fail(TEXT("unplanned_early_extraction"), WalkLabel); return; }
    if (TestStep == ETestStep::Capture)
    {
        if (PollCapture(DeltaSeconds)) SetTestStep(AfterTestCapture);
        return;
    }
    switch (TestStep)
    {
    case ETestStep::Init:
    {
        if (UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(this))
        { if (StepTime > 60.0) Fail(TEXT("frontier_navigation_never_became_ready")); return; }
        if (!PrepareTestWorld()) { Fail(TEXT("frontier_world_preconditions_failed")); return; }
        if (Journey().Round == 3)
        {
            if(bR10Journey && !Check(Journey().bR10Loaded && Journey().bR10Healed && Journey().bR10FoughtAfterHeal,
                TEXT("R10_real_loot_load_cover_treatment_combat_extraction_chain"))) { Fail(TEXT("R10_incomplete_new_mechanic_journey")); return; }
            if (!Check(Journey().bSouthSuccess && Journey().bNorthSuccess && Journey().bRealDeath,
                TEXT("two_successful_raids_and_death_restart_remain_isolated"))) { Fail(TEXT("missing_required_raid_outcome")); return; }
            if (!bFinaleOnly && !Check(Journey().CompletedRoutes.Num() >= 8 && Journey().LootInteractions >= 25,
                TEXT("eight_authored_routes_and_twenty_four_containers_actually_exercised"))) { Fail(TEXT("incomplete_real_world_coverage")); return; }
            if (!Check(Journey().bPatrolMoved && Journey().bChaseMoved && Journey().bLiveDamage,
                TEXT("production_guards_patrolled_pursued_and_damaged_player"))) { Fail(TEXT("incomplete_live_ai_behavior_proof")); return; }
            CaptureTest(TEXT("r4_13_fresh_after_death"), ETestStep::Finish);
            return;
        }
        if (Journey().Round == 0)
        {
            if (!AuditNavigation()) { Fail(TEXT("technical_navigation_or_door_audit_failed")); return; }
            if (!ExecuteAction(TEXT("TacticalMap")) || !RaidHUD->IsTacticalMapOpen()) { Fail(TEXT("M_binding_did_not_open_tactical_map")); return; }
            if (!ExecuteAction(TEXT("Quit")) || RaidHUD->IsTacticalMapOpen() || RaidMode->State != ERaidState::Running)
            { Fail(TEXT("escape_did_not_close_map_before_abandoning_raid")); return; }
            Check(true, TEXT("real_M_and_escape_bindings_preserve_running_raid"));
            ExecuteAction(TEXT("TacticalMap"));
            ExecuteAction(TEXT("Inventory"));
            if (!Check(!RaidHUD->IsTacticalMapOpen() && RaidPawn->IsInventoryOpen(), TEXT("backpack_closes_tactical_map")))
            { Fail(TEXT("map_inventory_exclusivity_failed")); return; }
            ExecuteAction(TEXT("Inventory"));
            ExecuteAction(TEXT("TacticalMap"));
            Journey().Telemetry.Add(TEXT("run_id,round,walk,elapsed_s,x_cm,y_cm,z_cm,distance_m,health"));
            CaptureTest(TEXT("r1_01_tactical_map"), ETestStep::Observe);
        }
        else SetTestStep(Journey().Round == 2 ? ETestStep::DeathApproach : ETestStep::Observe);
        break;
    }
    case ETestStep::Observe:
        if (RaidHUD->IsTacticalMapOpen()) ExecuteAction(TEXT("TacticalMap"));
        if (StepTime >= 3.0)
        {
            if (Journey().Round == 0 && !Check(Journey().bPatrolMoved, TEXT("placed_guard_patrol_has_real_displacement")))
            { Fail(TEXT("no_real_map_patrol_movement")); return; }
            SetTestStep(ETestStep::ChooseLoot);
        }
        break;
    case ETestStep::ChooseLoot:
    {
        if (TouchedLoot.Num() >= (bR10Journey && Journey().Round==0 ? 4 : Journey().Round == 0 && !bFinaleOnly ? 24 : 1))
        { SetTestStep(Journey().Round == 0 && !bFinaleOnly ? ETestStep::RouteSelect : ETestStep::MissionApproach); break; }
        ARaidLoot* Nearest = nullptr;
        float Best = TNumericLimits<float>::Max();
        for (const TWeakObjectPtr<ARaidLoot>& Ref : MapLoot)
            if (Ref.IsValid() && !TouchedLoot.Contains(Ref))
            {
                const float Distance = FVector::DistSquared2D(RaidPawn->GetActorLocation(), Ref->GetActorLocation());
                if (Distance < Best) { Best = Distance; Nearest = Ref.Get(); }
            }
        FVector Approach;
        if (!Nearest || !FindLootApproach(Nearest, Approach)) { Fail(TEXT("unvisited_container_has_no_real_approach")); return; }
        TestLoot = Nearest;
        bLootOpenedOnce = false;
        StartWalk(Approach, TEXT("container_") + Nearest->GetName(), ETestStep::LootFocus, 32.f);
        break;
    }
    case ETestStep::Walk:
        TickWalk(DeltaSeconds);
        break;
    case ETestStep::LootFocus:
        if (UpdateCombat()) break;
        RaidPawn->StopFire(); bTestFiring = false;
        AimAt(TestLoot->CollisionBox->GetComponentLocation());
        if (StepTime < .25) break;
        if (RaidPawn->GetFocusedLoot() != TestLoot || !ExecuteAction(TEXT("Interact")) || RaidPawn->GetOpenLoot() != TestLoot)
        { Fail(TEXT("real_E_cannot_open_authored_container"), TestLoot->GetName()); return; }
        ExecuteAction(TEXT("Interact"), false);
        LootBeforeCount = RaidPawn->GetLootCount();
        SetTestStep(ETestStep::LootUnknown);
        break;
    case ETestStep::LootUnknown:
        if (StepTime < .25) break;
        if (RaidPawn->GetOpenLoot() != TestLoot) { SetTestStep(ETestStep::LootFocus); break; }
        if (!bLootOpenedOnce)
        {
            bool bUnknown = TestLoot->GetContents().Num() > 0;
            for (const FRaidInventoryEntry& Entry : TestLoot->GetContents()) bUnknown &= !Entry.bRevealed;
            if (!Check(bUnknown && RaidPawn->GetLootCount() == LootBeforeCount, TEXT("authored_container_opens_unknown_without_autopickup"), TestLoot->GetName()))
            { Fail(TEXT("container_initial_visibility_or_autopickup_failed")); return; }
            bLootOpenedOnce = true;
        }
        if (Journey().Round == 0 && TouchedLoot.Num() == 0 && Captures == 1)
            CaptureTest(TEXT("r1_02_container_unknown"), ETestStep::LootSearch);
        else SetTestStep(ETestStep::LootSearch);
        break;
    case ETestStep::LootSearch:
        if (StepTime < .25) break;
        if (RaidPawn->GetOpenLoot() != TestLoot) { SetTestStep(ETestStep::LootFocus); break; }
        if (!Check(RaidPawn->IsSearching() || TestLoot->FindNextUnsearchedIndex()==INDEX_NONE,
            TEXT("opening_container_automatically_searches"))) return;
        SetTestStep(ETestStep::LootReveal);
        break;
    case ETestStep::LootReveal:
    {
        if (RaidPawn->GetOpenLoot() != TestLoot) { SetTestStep(ETestStep::LootFocus); break; }
        const TArray<FRaidInventoryEntry>& Contents = TestLoot->GetContents();
        if (Contents.Num() > 0 && Contents[0].bRevealed)
        {
            if (!Check(RaidPawn->GetLootCount() == LootBeforeCount, TEXT("search_reveals_without_transferring_any_item"), TestLoot->GetName()))
            { Fail(TEXT("search_automatically_transferred_items")); return; }
            RaidPawn->CancelSearch();
            if (Journey().Round == 0 && TouchedLoot.Num() == 0 && Captures == 2)
                CaptureTest(TEXT("r1_03_container_revealed"), ETestStep::LootRoom);
            else SetTestStep(ETestStep::LootRoom);
        }
        else if (StepTime > 8.0) Fail(TEXT("real_container_search_did_not_reveal"), TestLoot->GetName());
        break;
    }
    case ETestStep::LootRoom:
    {
        if (StepTime < .25) break;
        if (RaidPawn->GetOpenLoot() != TestLoot) { SetTestStep(ETestStep::LootFocus); break; }
        if (RaidPawn->GetLootCount() >= RaidPawn->MaxLootItems)
        {
            const TArray<FRaidInventoryEntry>& Backpack = RaidPawn->GetInventoryComponent()->GetEntries();
            int32 StoreIndex = 0;
            for (int32 Index = 0; Index < Backpack.Num(); ++Index)
                if (Backpack[Index].Definition.UseEffect == ERaidItemUseEffect::None) { StoreIndex = Index; break; }
            const FName StoreAction(*FString::Printf(TEXT("Store_%d"), StoreIndex));
            if (!RaidHUD->GetHitBoxWithName(StoreAction))
            {
                // Production inventory displays six rows per page. Navigate the
                // actual Canvas controls before dispatching an off-page action.
                if (!ClickButton(StoreIndex >= 6 ? TEXT("InventoryNext") : TEXT("InventoryPrev"))) return;
                SetTestStep(ETestStep::LootRoom);
                break;
            }
            if (!ClickButton(StoreAction)) return;
            if (!Check(RaidPawn->GetLootCount() == LootBeforeCount - 1, TEXT("full_backpack_uses_real_manual_store_to_make_room")))
            { Fail(TEXT("manual_store_did_not_free_capacity")); return; }
        }
        LootBeforeCount = RaidPawn->GetLootCount();
        SetTestStep(ETestStep::LootTake);
        break;
    }
    case ETestStep::LootTake:
        if (StepTime < .25) break;
        if (TestLoot->GetContents().IsEmpty() || !TestLoot->GetContents()[0].bRevealed) { Fail(TEXT("revealed_container_row_disappeared_before_take")); return; }
        LootTakeEntryId = TestLoot->GetContents()[0].EntryId;
        LootTakeQuantity = TestLoot->GetContents()[0].Quantity;
        if (!ClickButton(TEXT("Take_0"))) return;
        SetTestStep(ETestStep::LootTaken);
        break;
    case ETestStep::LootTaken:
    {
        if (StepTime < .25) break;
        int32 Remaining = 0;
        for (const FRaidInventoryEntry& Entry : TestLoot->GetContents()) if (Entry.EntryId == LootTakeEntryId) Remaining = Entry.Quantity;
        if (!Check(IsValid(TestLoot) && RaidPawn->GetLootCount() == LootBeforeCount + 1 && Remaining == LootTakeQuantity - 1,
            TEXT("authored_container_manual_take_moves_one_and_keeps_actor"), TestLoot->GetName()))
        { Fail(TEXT("manual_take_conservation_failed")); return; }
        TouchedLoot.Add(TestLoot.Get());
        if (bR10Journey && RaidPawn->GetInventoryComponent()->GetWeightGrams()>6000)
        {
            Journey().bR10Loaded=true;
            Check(RaidPawn->GetLoadSpeedScale()<1.f,TEXT("R10_authored_loot_causes_real_load_penalty"));
        }
        ++Journey().LootInteractions;
        if (Journey().Round == 0 && TouchedLoot.Num() == 1) CaptureTest(TEXT("r1_04_container_taken"), ETestStep::LootClose);
        else if (bR10Journey && Journey().Round==0 && TouchedLoot.Num()==4)
            CaptureTest(TEXT("r1_r10_weighted_backpack"),ETestStep::LootClose);
        else SetTestStep(ETestStep::LootClose);
        break;
    }
    case ETestStep::LootClose:
        if (StepTime < .25) break;
        if (RaidPawn->IsInventoryOpen() && !ClickButton(TEXT("ContainerClose"))) return;
        SetTestStep(ETestStep::ChooseLoot);
        break;
    case ETestStep::RouteSelect:
        if (RouteIndex >= Routes.Num()) { SetTestStep(ETestStep::MissionApproach); break; }
        if (RoutePointIndex >= Routes[RouteIndex].FloorPoints.Num())
        {
            Journey().CompletedRoutes.Add(Routes[RouteIndex].Id);
            Check(true, TEXT("entire_authored_route_completed_by_real_player_input"), Routes[RouteIndex].Id);
            ++RouteIndex; RoutePointIndex = 0; break;
        }
        StartWalk(Routes[RouteIndex].FloorPoints[RoutePointIndex] + FVector(0.f, 0.f, RaidPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
            FString::Printf(TEXT("%s_%02d"), *Routes[RouteIndex].Id, RoutePointIndex), ETestStep::RouteCheckpoint);
        break;
    case ETestStep::RouteCheckpoint:
    {
        const FString RouteId = Routes[RouteIndex].Id;
        ++RoutePointIndex;
        if ((RouteId == TEXT("drainage") && RoutePointIndex == 5)
            || (RoutePointIndex == Routes[RouteIndex].FloorPoints.Num()
                && (RouteId == TEXT("warehouse_mezzanine") || RouteId == TEXT("comms_upper"))))
            CaptureTest(TEXT("r1_route_") + RouteId, ETestStep::RouteSelect);
        else SetTestStep(ETestStep::RouteSelect);
        break;
    }
    case ETestStep::R10TreatmentStart:
        bSeekingR10Cover=false;
        if(UpdateCombat()) break; // A pursuing guard can invalidate cover: fight before treating.
        bSeekingR10Cover=true;
        RaidPawn->StopFire(); bTestFiring=false;
        bR10HealingCaptured=false;
        R10HealthBefore=RaidPawn->Health; R10MedicalCount=RaidPawn->GetLootCount();
        ExecuteAction(TEXT("Heal"));
        if(!Check(RaidPawn->IsTreating(),TEXT("R10_cover_starts_catalog_medical_treatment"))) { Fail(TEXT("R10_no_medical_item_at_cover")); return; }
        SetTestStep(ETestStep::R10TreatmentWait);
        break;
    case ETestStep::R10TreatmentWait:
        if(RaidPawn->IsTreating())
        {
            if(!bR10HealingCaptured && StepTime>.8)
            { bR10HealingCaptured=true; CaptureTest(FString::Printf(TEXT("r1_r10_healing_at_cover_%d"),R10CoverAttempts),ETestStep::R10TreatmentWait); }
            break;
        }
        if(RaidPawn->Health>R10HealthBefore && RaidPawn->GetLootCount()==R10MedicalCount-1)
        {
            Journey().bR10Healed=true;
            Check(true,TEXT("R10_normal_AI_damage_healed_once_after_real_cover_walk"));
            CaptureTest(TEXT("r1_r10_healed_ready_to_fight"),ETestStep::R10TreatmentResume);
        }
        else
        {
            bSeekingR10Cover=false;
            if(!SeekR10Cover()) { Fail(TEXT("R10_treatment_interrupted_and_no_new_cover")); return; }
        }
        break;
    case ETestStep::R10TreatmentResume:
        bSeekingR10Cover=false;
        StartWalk(R10ResumeDestination,TEXT("r10_resume_after_treatment"),R10ResumeStep,30.f);
        break;
    case ETestStep::MissionApproach:
        MissionScenario = Journey().Round == 0 ? 0 : 5;
        StartWalk(MissionStand, TEXT("mission_terminal_approach"), ETestStep::MissionStart, 30.f);
        break;
    case ETestStep::MissionStart:
        if (UpdateCombat()) break;
        RaidPawn->StopFire(); bTestFiring = false;
        AimAt(TestObjective->InteractionBox->GetComponentLocation());
        if (StepTime < .25) break;
        if (!ExecuteAction(TEXT("Interact")) || !RaidMode->GetMissionSnapshot().bInProgress)
        { Fail(TEXT("real_E_did_not_begin_terminal_hold")); return; }
        MissionHoldStartedAt = Now;
        SetTestStep(ETestStep::MissionHold);
        break;
    case ETestStep::MissionHold:
    {
        const FRaidMissionSnapshot Snapshot = RaidMode->GetMissionSnapshot();
        if (MissionScenario == 0 && !bCapturedMissionPartial && Now - MissionHoldStartedAt >= .5)
        {
            bCapturedMissionPartial = true;
            CaptureTest(TEXT("r1_mission_partial"), ETestStep::MissionHold);
            break;
        }
        if (MissionScenario == 5)
        {
            if (Snapshot.bCompleted)
            {
                if (!Check(Now - MissionHoldStartedAt >= 5.8 && Snapshot.Progress == 1.f && NorthExit->IsAvailable() && SouthExit->IsAvailable(),
                    TEXT("six_second_real_hold_completes_optional_mission_and_unlocks_north")))
                { Fail(TEXT("mission_completion_timing_or_exit_unlock_failed")); return; }
                ExecuteAction(TEXT("Interact"), false);
                CaptureTest(TEXT("r2_08_mission_completed"), ETestStep::NorthApproach);
            }
            else if (!Snapshot.bInProgress || Now - MissionHoldStartedAt > 8.0) Fail(TEXT("uninterrupted_six_second_mission_hold_failed"));
            break;
        }
        if (Now - MissionHoldStartedAt < 1.1) break;
        if (!Check(Snapshot.bInProgress && Snapshot.Progress > 0.f && Snapshot.Progress < 1.f && !NorthExit->IsAvailable(),
            TEXT("partial_terminal_hold_does_not_unlock_north"), FString::Printf(TEXT("case=%d"), MissionScenario)))
        { Fail(TEXT("partial_mission_hold_was_not_real")); return; }
        if (MissionScenario == 0) ExecuteAction(TEXT("Interact"), false);
        else if (MissionScenario == 1)
        { const FRotator Away(0.f, RaidPC->GetControlRotation().Yaw + 180.f, 0.f); RaidPC->SetControlRotation(Away); RaidPawn->Camera->SetWorldRotation(Away); }
        else if (MissionScenario == 2) { SetTestStep(ETestStep::MissionRange); break; }
        else if (MissionScenario == 3) ExecuteAction(TEXT("Inventory"));
        else
        {
            InjectedDamageHealthBefore = RaidPawn->Health;
            UE_LOG(LogTemp, Display, TEXT("R7_CONTROLLED_DAMAGE: mission cancellation probe amount=3 injected=1; not credited as AI damage"));
            UGameplayStatics::ApplyDamage(RaidPawn, 3.f, nullptr, this, nullptr);
        }
        SetTestStep(ETestStep::MissionCancelled);
        break;
    }
    case ETestStep::MissionRange:
        AimAt(TestObjective->InteractionBox->GetComponentLocation());
        // Interaction range ends at the collision surface, not the terminal centre.
        // Clear the entire box before asserting cancellation, including rotated boxes.
        if (FVector::Dist(RaidPawn->Camera->GetComponentLocation(), TestObjective->InteractionBox->GetComponentLocation())
            > TestObjective->MissionDefinition->InteractionRange + TestObjective->InteractionBox->Bounds.SphereRadius + 30.f)
        { ExecuteAxis(TEXT("MoveForward"), 0.f); RaidPawn->GetCharacterMovement()->StopMovementImmediately(); SetTestStep(ETestStep::MissionCancelled); }
        else if (StepTime > 3.0) Fail(TEXT("range_cancel_backwards_input_did_not_move_player"));
        else ExecuteAxis(TEXT("MoveForward"), -1.f);
        break;
    case ETestStep::MissionCancelled:
    {
        if (StepTime < .2) break;
        const FRaidMissionSnapshot Snapshot = RaidMode->GetMissionSnapshot();
        static const TCHAR* Names[] = {TEXT("release_cancels_mission_and_resets_progress"), TEXT("lost_view_cancels_mission_and_resets_progress"),
            TEXT("out_of_range_cancels_mission_and_resets_progress"), TEXT("backpack_cancels_mission_and_resets_progress"),
            TEXT("production_damage_event_cancels_mission_and_resets_progress")};
        if (!Check(!Snapshot.bInProgress && !Snapshot.bCompleted && FMath::IsNearlyZero(Snapshot.Progress)
            && !NorthExit->IsAvailable(), Names[MissionScenario])) { Fail(TEXT("mission_cancel_contract_failed")); return; }
        if (MissionScenario == 4 && !Check(RaidPawn->Health < InjectedDamageHealthBefore, TEXT("controlled_damage_is_not_immunity")))
        { Fail(TEXT("mission_damage_did_not_reduce_real_health")); return; }
        ExecuteAction(TEXT("Interact"), false);
        if (RaidPawn->IsInventoryOpen()) ExecuteAction(TEXT("Inventory"));
        if (MissionScenario == 2) StartWalk(MissionStand, TEXT("return_after_range_cancel"), ETestStep::MissionReturn, 30.f);
        else SetTestStep(ETestStep::MissionReturn);
        break;
    }
    case ETestStep::MissionReturn:
        ++MissionScenario;
        if (MissionScenario < 5) SetTestStep(ETestStep::MissionStart);
        else CaptureTest(TEXT("r1_05_optional_mission_cancelled"), ETestStep::NorthApproach);
        break;
    case ETestStep::NorthApproach:
        StartWalk(NorthExit->GetActorLocation() + FVector(0.f, 0.f, 88.f), TEXT("northern_exit_actual_approach"),
            Journey().Round == 0 ? ETestStep::NorthLocked : ETestStep::ExtractPartial);
        break;
    case ETestStep::NorthLocked:
        UpdateCombat();
        if (StepTime >= 9.0)
        {
            if (!Check(NorthExit->IsPlayerInside() && !NorthExit->IsAvailable() && NorthExit->GetSecondsRemaining() >= 7.8f
                && RaidPawn->GetLootCount() > 0 && RaidMode->State == ERaidState::Running,
                TEXT("locked_north_cannot_extract_even_with_loot_and_full_wait"))) { Fail(TEXT("locked_north_wrongly_accepted_extraction")); return; }
            CaptureTest(TEXT("r1_06_north_locked"), ETestStep::SouthApproach);
        }
        break;
    case ETestStep::SouthApproach:
        StartWalk(SouthExit->GetActorLocation() + FVector(0.f, 0.f, 88.f), TEXT("southern_optional_exit_actual_approach"), ETestStep::ExtractPartial);
        break;
    case ETestStep::ExtractPartial:
        UpdateCombat();
        if (StepTime >= 1.5)
        {
            if (!Check(ActiveExit->IsPlayerInside() && ActiveExit->IsAvailable() && ActiveExit->GetSecondsRemaining() < 7.f
                && ActiveExit->GetSecondsRemaining() > 3.f, TEXT("real_extraction_countdown_progresses_inside_trigger")))
            { Fail(TEXT("extraction_countdown_did_not_start")); return; }
            CaptureTest(Journey().Round == 0 ? TEXT("r1_07_south_countdown") : TEXT("r2_09_north_countdown"), ETestStep::ExtractLeave);
        }
        break;
    case ETestStep::ExtractLeave:
        StartWalk(ActiveExit->GetActorLocation() + FVector(0.f, -500.f, 88.f), TEXT("walk_out_of_extraction"), ETestStep::ExtractLeft);
        break;
    case ETestStep::ExtractLeft:
        if (!Check(!ActiveExit->IsPlayerInside() && ActiveExit->GetSecondsRemaining() >= 7.8f, TEXT("walking_out_resets_extraction_to_eight_seconds")))
        { Fail(TEXT("exit_countdown_survived_leaving_trigger")); return; }
        StartWalk(ActiveExit->GetActorLocation() + FVector(0.f, 0.f, 88.f), TEXT("walk_back_into_extraction"), ETestStep::ExtractDamage);
        break;
    case ETestStep::ExtractDamage:
        if (ActiveExit->GetSecondsRemaining() < 6.7f)
        {
            InjectedDamageHealthBefore = RaidPawn->Health;
            UE_LOG(LogTemp, Display, TEXT("R7_CONTROLLED_DAMAGE: extraction reset probe amount=3 injected=1; not credited as AI damage"));
            UGameplayStatics::ApplyDamage(RaidPawn, 3.f, nullptr, this, nullptr);
            SetTestStep(ETestStep::ExtractDamageWait);
        }
        else if (StepTime > 4.0) Fail(TEXT("extraction_failed_to_restart_after_reentry"));
        break;
    case ETestStep::ExtractDamageWait:
        if (StepTime < .1) break;
        if (!Check(ActiveExit->GetSecondsRemaining() >= 7.5f && RaidPawn->Health < InjectedDamageHealthBefore,
            TEXT("real_damage_resets_extraction_and_reduces_health"))) { Fail(TEXT("damage_countdown_reset_failed")); return; }
        SetTestStep(ETestStep::ExtractUnpossess);
        break;
    case ETestStep::ExtractUnpossess:
        if (UpdateCombat()) break;
        if (ActiveExit->GetSecondsRemaining() < 6.7f)
        {
            OriginalPawn = RaidPawn;
            OriginalPawnHealth = RaidPawn->Health;
            RaidPC->UnPossess();
            bTestFiring = false;
            SetTestStep(ETestStep::ExtractRepossess);
        }
        break;
    case ETestStep::ExtractSwap:
        // A live guard can arrive while the identity probe waits for progress.
        // Defend through normal weapon input; never suppress AI or restore HP.
        if (UpdateCombat()) break;
        if (ActiveExit->GetSecondsRemaining() < 6.7f)
        {
            OriginalPawnHealth = OriginalPawn->Health;
            BeforePossessionSeconds = ActiveExit->GetSecondsRemaining();
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            PossessionProbePawn = GetWorld()->SpawnActor<ARaidPlayer>(OriginalPawn->GetClass(),
                OriginalPawn->GetActorLocation() + FVector(0.f, 90.f, 0.f), OriginalPawn->GetActorRotation(), Params);
            if (!PossessionProbePawn || OriginalPawn->GetInventoryComponent()->GetEntries().IsEmpty())
            { Fail(TEXT("replacement_pawn_probe_fixture_failed")); return; }
            const FRaidItemDefinition Definition = OriginalPawn->GetInventoryComponent()->GetEntries()[0].Definition;
            // AddItem deliberately requires the currently possessed, actionable pawn.
            // Possession and grant stay in this frame, before the exit's next Tick.
            RaidPC->Possess(PossessionProbePawn);
            if (!PossessionProbePawn->AddItem(Definition.ItemId, Definition.DisplayName, 1, Definition.UnitValue))
            { Fail(TEXT("replacement_pawn_catalog_grant_failed")); return; }
            UE_LOG(LogTemp, Display, TEXT("R7_POSSESSION_FIXTURE: replacement pawn receives one catalog item only for countdown identity test; original pawn health/inventory retained"));
            SetTestStep(ETestStep::ExtractSwapWait);
        }
        break;
    case ETestStep::ExtractSwapWait:
        if (StepTime < .15) break;
        if (!Check(RaidPawn == PossessionProbePawn && ActiveExit->IsPlayerInside()
            && ActiveExit->GetSecondsRemaining() > BeforePossessionSeconds + .5f,
            TEXT("same_frame_replacement_pawn_cannot_inherit_partial_countdown"))) { Fail(TEXT("replacement_pawn_inherited_extraction_progress")); return; }
        RaidPC->Possess(OriginalPawn);
        bTestFiring = false;
        SetTestStep(ETestStep::ExtractRestore);
        break;
    case ETestStep::ExtractRestore:
        if (StepTime < .15) break;
        if (!Check(RaidPawn == OriginalPawn && FMath::IsNearlyEqual(RaidPawn->Health, OriginalPawnHealth)
            && ActiveExit->GetSecondsRemaining() > 7.4f, TEXT("original_pawn_returns_with_unchanged_health_and_fresh_countdown")))
        { Fail(TEXT("original_pawn_restore_contract_failed")); return; }
        PossessionProbePawn->Destroy(); PossessionProbePawn = nullptr;
        ExtractionExpectedItems = RaidPawn->GetLootCount();
        ExtractionExpectedValue = RaidPawn->GetTotalLootValue();
        SetTestStep(ETestStep::ExtractFinish);
        break;
    case ETestStep::ExtractFinish:
        UpdateCombat();
        if (RaidMode->State == ERaidState::Extracted)
        {
            if (!Check(RaidPawn->IsAlive() && RaidMode->SecuredItems == ExtractionExpectedItems
                && RaidMode->SecuredValue == ExtractionExpectedValue && ExtractionExpectedItems > 0,
                Journey().Round == 0 ? TEXT("south_extracts_without_completing_optional_mission") : TEXT("north_extracts_after_real_mission_completion")))
            { Fail(TEXT("real_extraction_result_mismatch")); return; }
            if (Journey().Round == 0)
            {
                if (RaidMode->GetMissionSnapshot().bCompleted) { Fail(TEXT("optional_south_run_unexpectedly_completed_mission")); return; }
                Journey().bSouthSuccess = true;
            }
            else Journey().bNorthSuccess = true;
            CaptureTest(Journey().Round == 0 ? TEXT("r1_08_south_success") : TEXT("r2_10_north_success"), ETestStep::Result);
        }
        else if (StepTime > 11.0) Fail(TEXT("final_eight_second_extraction_never_completed"));
        break;
    case ETestStep::Result:
        RestartTestRound();
        break;
    case ETestStep::DeathApproach:
    {
        ARaidEnemy* Target = nullptr;
        FVector Approach = FVector::ZeroVector;
        float Best = TNumericLimits<float>::Max();
        UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        if (!Nav) { Fail(TEXT("death_route_navigation_unavailable")); return; }
        for (const TWeakObjectPtr<ARaidEnemy>& Ref : MapEnemies)
            if (Ref.IsValid() && Ref->Health > 0.f)
            {
                // A moving guard's radial offset can lie in a wall or off the navmesh.
                // Select a real reachable observation point before asking the player
                // to walk there. The guard and player remain live and unmodified.
                for (const float Radius : {350.f, 600.f})
                    for (int32 AngleIndex = 0; AngleIndex < 16; ++AngleIndex)
                    {
                        const float Angle = AngleIndex * 2.f * PI / 16.f;
                        const FVector Candidate = Ref->GetActorLocation()
                            + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
                        FNavLocation Projected;
                        if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(75.f, 75.f, 200.f))) continue;
                        const FVector Centre = Projected.Location + FVector(0.f, 0.f,
                            RaidPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
                        FCollisionQueryParams Params(SCENE_QUERY_STAT(R7DeathApproach), false, Ref.Get());
                        Params.AddIgnoredActor(RaidPawn);
                        FHitResult Hit;
                        if (GetWorld()->LineTraceSingleByChannel(Hit, Ref->GetActorLocation() + FVector(0.f, 0.f, 57.f),
                            Centre + FVector(0.f, 0.f, 64.f), ECC_Visibility, Params)) continue;
                        UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
                            this, RaidPawn->GetActorLocation(), Centre, RaidPawn);
                        if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2) continue;
                        const float Distance = Path->GetPathLength();
                        if (Distance < Best) { Best = Distance; Target = Ref.Get(); Approach = Centre; }
                    }
            }
        if (!Target) { Fail(TEXT("no_reachable_visible_point_for_live_death_route")); return; }
        Check(true, TEXT("death_route_uses_projected_reachable_visible_point"), Target->GetName());
        DeathWalkDamageBaseline=LiveDamageEvents;
        StartWalk(Approach, TEXT("unarmed_route_to_live_map_threat"), ETestStep::DeathWait, 70.f);
        break;
    }
    case ETestStep::DeathWait:
        RaidPawn->StopFire();
        // A live guard can lose this static viewpoint behind cover. Re-approach
        // by input instead of changing its AI, damage or the player's health.
        if(StepTime>8.0 && GetWorld()->GetTimeSeconds()-RaidPawn->LastDamageTime>6.f)
        { SetTestStep(ETestStep::DeathApproach); break; }
        if (StepTime > 45.0) Fail(TEXT("live_map_enemy_failed_to_cause_real_death"));
        break;
    case ETestStep::Finish:
        Finish();
        break;
    default:
        Fail(TEXT("unhandled_r7_gameplay_state"), FString::FromInt(static_cast<int32>(TestStep)));
        break;
    }
}

bool ARaidR7TestDirector::WriteReceipt(bool bPassed)
{
    const TSharedRef<FJsonObject> Receipt = MakeShared<FJsonObject>();
    Receipt->SetNumberField(TEXT("schema_version"), 1);
    Receipt->SetStringField(TEXT("test_scope"), bFinaleOnly ? TEXT("finale_only_no_full_route_coverage") : TEXT("full"));
    Receipt->SetStringField(TEXT("run_id"), RunId);
    Receipt->SetStringField(TEXT("mode"), Mode == EMode::Preview ? TEXT("Preview") : Mode == EMode::Performance ? TEXT("Performance") : TEXT("Test"));
    Receipt->SetStringField(TEXT("status"), !bPassed ? TEXT("FAIL")
        : Mode == EMode::Test && TestStep != ETestStep::Finish ? TEXT("RUNNING") : TEXT("PASS"));
    Receipt->SetStringField(TEXT("map"), GetWorld()->GetPackage()->GetName());
    Receipt->SetStringField(TEXT("metadata"), ManifestPath);
    Receipt->SetStringField(TEXT("started_utc"), StartedUtc);
    Receipt->SetStringField(TEXT("finished_utc"), FDateTime::UtcNow().ToIso8601());
    Receipt->SetNumberField(TEXT("checks"), Checks);
    Receipt->SetNumberField(TEXT("failures"), Failures);
    Receipt->SetNumberField(TEXT("screenshots"), Captures);
    Receipt->SetNumberField(TEXT("frame_samples"), SampleFrames);
    Receipt->SetArrayField(TEXT("views"), ViewEvidence);
    Receipt->SetBoolField(TEXT("hardware_input_tested"), false);
    Receipt->SetBoolField(TEXT("audio_listened"), false);
    Receipt->SetBoolField(TEXT("r10_journey"),bR10Journey);
    Receipt->SetBoolField(TEXT("r10_authored_loot_load"),Journey().bR10Loaded);
    Receipt->SetBoolField(TEXT("r10_cover_treatment"),Journey().bR10Healed);
    Receipt->SetBoolField(TEXT("r10_fired_after_treatment"),Journey().bR10FoughtAfterHeal);
    Receipt->SetBoolField(TEXT("ai_or_health_overridden"), false);
    Receipt->SetBoolField(TEXT("fixed_view_teleport_is_route_evidence"), false);
    if (Mode == EMode::Test)
    {
        Receipt->SetNumberField(TEXT("round"), Journey().Round + 1);
        Receipt->SetNumberField(TEXT("container_interactions"), Journey().LootInteractions);
        Receipt->SetBoolField(TEXT("normal_ai_patrol_moved"), Journey().bPatrolMoved);
        Receipt->SetBoolField(TEXT("normal_ai_chase_moved"), Journey().bChaseMoved);
        Receipt->SetBoolField(TEXT("normal_ai_damaged_player"), Journey().bLiveDamage);
        Receipt->SetBoolField(TEXT("south_success_without_mission"), Journey().bSouthSuccess);
        Receipt->SetBoolField(TEXT("north_success_after_mission"), Journey().bNorthSuccess);
        Receipt->SetBoolField(TEXT("normal_ai_caused_death"), Journey().bRealDeath);
        Receipt->SetStringField(TEXT("controlled_probes"), TEXT("mission/extraction cancellation includes explicit 3-damage production ApplyDamage calls, not AI evidence; countdown identity includes a temporary catalog-item replacement pawn; original pawn health retained; neither fixture earns route or loot credit"));
        Receipt->SetArrayField(TEXT("actual_input_walks"), Journey().Walks);
        TArray<TSharedPtr<FJsonValue>> Completed;
        for (const FString& Id : Journey().CompletedRoutes) Completed.Add(MakeShared<FJsonValueString>(Id));
        Receipt->SetArrayField(TEXT("authored_routes_completed"), Completed);
        if (!Journey().Telemetry.IsEmpty())
            if (!FFileHelper::SaveStringArrayToFile(Journey().Telemetry, *FPaths::Combine(OutputDirectory, TEXT("PlayerRoute.csv")),
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return false;
    }
    Receipt->SetStringField(TEXT("frame_timing_scope"), TEXT("wall_frame_ms is director tick-to-tick QPC elapsed time including scheduling/render waits; stat unit raw fields describe latest completed, possibly delayed frames; blank values are unavailable, never zero cost; no FPS improvement claim or baseline comparison"));
    int32 Width = 0, Height = 0;
    if (RaidPC) RaidPC->GetViewportSize(Width, Height);
    Receipt->SetNumberField(TEXT("width"), Width);
    Receipt->SetNumberField(TEXT("height"), Height);
    return SaveJson(FPaths::Combine(OutputDirectory, TEXT("R7DirectorReceipt.json")), Receipt);
}

void ARaidR7TestDirector::Finish()
{
    if (bFinished) return;
    if (Failures > 0) { Fail(TEXT("refusing_pass_after_failed_assertion")); return; }
    if (Mode == EMode::Preview && Captures != Views.Num()) { Fail(TEXT("incomplete_preview_views")); return; }
    if (!WriteReceipt(Failures == 0)) { Fail(TEXT("director_receipt_write_failed")); return; }
    UE_LOG(LogTemp, Display, TEXT("R7_RESULT: PASS mode=%s checks=%d failures=0 captures=%d frames=%d run_id=%s"),
        Mode == EMode::Preview ? TEXT("Preview") : Mode == EMode::Performance ? TEXT("Performance") : TEXT("Test"),
        Checks, Captures, SampleFrames, *RunId);
    bFinished = true;
    SetActorTickEnabled(false);
    FPlatformMisc::RequestExitWithStatus(false, 0);
}

bool ARaidR7TestDirector::SeekR10Cover()
{
    if(R10CoverAttempts>=5)return false;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if(!Nav)return false;
    const FVector Origin=RaidPawn->GetActorLocation();
    FVector BestPoint; float BestDistance=FLT_MAX; bool bFound=false;
    for(int32 Radius=800;Radius<=2400;Radius+=400)
        for(int32 Direction=0;Direction<16;++Direction)
        {
            const float Angle=Direction*2.f*PI/16.f;
            FNavLocation Projected;
            if(!Nav->ProjectPointToNavigation(Origin+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0),Projected,FVector(100,100,200)))continue;
            const FVector Candidate=Projected.Location+FVector(0,0,RaidPawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            if(FMath::Abs(Candidate.Z-Origin.Z)>120 || FVector::Dist2D(Candidate,Origin)<250)continue;
            bool bExposed=false; int32 Occluded=0;
            for(const auto& Ref:MapEnemies)
            {
                auto* E=Ref.Get(); if(!E||E->Health<=0||FVector::Dist2D(E->GetActorLocation(),Candidate)>2800)continue;
                FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(R10Cover),false,RaidPawn);
                for(const auto& Other:MapEnemies) if(Other.IsValid())Params.AddIgnoredActor(Other.Get());
                if(FVector::Dist2D(E->GetActorLocation(),Candidate)<900){bExposed=true;break;}
                for(const FVector Offset:{FVector(0,0,0),FVector(70,0,0),FVector(-70,0,0),FVector(0,70,0),FVector(0,-70,0)})
                {
                    const FVector From=E->GetActorLocation()+FVector(0,0,57)+Offset;
                    const FVector To=Candidate+FVector(0,0,45);
                    const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Params);
                    if(!Blocked || Hit.Distance > FVector::Dist(From,To)-150){bExposed=true;break;}
                }
                if(bExposed)break;
                ++Occluded;
            }
            if(bExposed||Occluded==0)continue;
            auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(this,Origin,Candidate,RaidPawn);
            if(!Path||!Path->IsValid()||Path->IsPartial())continue;
            const float Distance=Path->GetPathLength();
            if(Distance<BestDistance){BestDistance=Distance;BestPoint=Candidate;bFound=true;}
        }
    if(!bFound)return false;
    if(R10CoverAttempts==0){R10ResumeDestination=WalkDestination;R10ResumeStep=AfterWalk;}
    ++R10CoverAttempts;bSeekingR10Cover=true;
    RaidPawn->StopFire();bTestFiring=false;
    Check(true,TEXT("R10_selected_reachable_cover_occluded_from_nearby_living_guards"),BestPoint.ToCompactString());
    return StartWalk(BestPoint,TEXT("r10_actual_walk_to_cover"),ETestStep::R10TreatmentStart,30.f);
}
