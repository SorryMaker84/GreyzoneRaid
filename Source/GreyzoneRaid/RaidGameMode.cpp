#include "RaidGameMode.h"
#include "RaidPlayer.h"
#include "RaidPlayerController.h"
#include "RaidHUD.h"
#include "RaidTestDirector.h"
#include "RaidVisualDirector.h"
#include "RaidFlowDirector.h"
#include "RaidContainerTestDirector.h"
#include "R6FrameworkBenchmarkDirector.h"
#include "RaidFrameworkTestDirector.h"
#include "RaidMissionObjective.h"
#include "RaidR7TestDirector.h"
#include "RaidR8TestDirector.h"
#include "RaidR81MapProbe.h"
#include "RaidR9TestDirector.h"
#include "RaidR10TestDirector.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ARaidGameMode::ARaidGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = ARaidPlayer::StaticClass();
    PlayerControllerClass = ARaidPlayerController::StaticClass();
    HUDClass = ARaidHUD::StaticClass();
}

void ARaidGameMode::BeginPlay()
{
    const bool bDirectPlay = UGameplayStatics::HasOption(OptionsString, TEXT("RaidStart"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidSelfTest"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidContainerTest"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidFrameworkBenchmark"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidFrameworkTest"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidR7Test"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidR7Preview"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidR7Performance"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidR81MapProbe"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidR9Test"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidR10Test"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidVisualReview"));
    State = bDirectPlay ? ERaidState::Running : ERaidState::MainMenu;
    Super::BeginPlay();
    ApplyInputMode();
    if (IsRaidActive())
    {
        PushMessage(GetMissionSnapshot().bPresent
            ? TEXT("先到前方检查站：按 E 打开容器，搜索后拿取物资。")
            : TEXT("搜集物资，前往东北角的 E-07 撤离点。"), 8.f);
        UE_LOG(LogTemp, Display, TEXT("RAID_BEGIN: new raid, inventory and enemies reset."));
    }
    else if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        const FVector Eye(-2600.f, -2900.f, 1650.f);
        const FRotator View = (FVector(200.f, 500.f, 90.f) - Eye).Rotation();
        ACameraActor* MenuCamera = GetWorld()->SpawnActor<ACameraActor>(Eye, View);
        if (MenuCamera)
        {
            MenuCamera->GetCameraComponent()->SetFieldOfView(68.f);
            PC->SetViewTarget(MenuCamera);
        }
        if (APawn* Pawn = PC->GetPawn()) Pawn->SetActorHiddenInGame(true);
        UE_LOG(LogTemp, Display, TEXT("RAID_FRONTEND: main menu; raid clock and combat inactive."));
    }
    // An opt-in integration run uses the same gameplay methods as player input.
    if (FParse::Param(FCommandLine::Get(), TEXT("RaidR10Test")))
        GetWorld()->SpawnActor<ARaidR10TestDirector>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("RaidR9Test")))
        GetWorld()->SpawnActor<ARaidR9TestDirector>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("RaidR81MapProbe")))
        GetWorld()->SpawnActor<ARaidR81MapProbe>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("RaidR8Test")))
        GetWorld()->SpawnActor<ARaidR8TestDirector>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("RaidSelfTest")))
        GetWorld()->SpawnActor<ARaidTestDirector>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("RaidVisualReview")))
        GetWorld()->SpawnActor<ARaidVisualDirector>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("RaidFlowTest")))
        GetWorld()->SpawnActor<ARaidFlowDirector>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("RaidContainerTest")))
        GetWorld()->SpawnActor<ARaidContainerTestDirector>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("RaidFrameworkBenchmark")))
        GetWorld()->SpawnActor<AR6FrameworkBenchmarkDirector>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("RaidFrameworkTest")))
        GetWorld()->SpawnActor<ARaidFrameworkTestDirector>();
    else if (FParse::Param(FCommandLine::Get(), TEXT("RaidR7Test"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidR7Preview"))
        || FParse::Param(FCommandLine::Get(), TEXT("RaidR7Performance")))
        GetWorld()->SpawnActor<ARaidR7TestDirector>();
}

void ARaidGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    OnMissionChanged.Clear();
    MissionObjective.Reset();
    Super::EndPlay(EndPlayReason);
}

ARaidMissionObjective* ARaidGameMode::GetMissionObjective() const
{
    ARaidMissionObjective* Registered = MissionObjective.Get();
    if (IsValid(Registered) && !Registered->IsActorBeingDestroyed() && Registered->HasValidDefinition()) return Registered;
    // All placed actors already exist while BeginPlay is still being dispatched.
    // This read fallback makes menu/HUD initialization independent of actor order.
    for (TActorIterator<ARaidMissionObjective> It(GetWorld()); It; ++It)
        if (!It->IsActorBeingDestroyed() && It->HasValidDefinition()) return *It;
    return nullptr;
}

FRaidMissionSnapshot ARaidGameMode::GetMissionSnapshot() const
{
    if (bHasMissionResult) return MissionResult;
    const ARaidMissionObjective* Objective = GetMissionObjective();
    return Objective ? Objective->GetSnapshot() : FRaidMissionSnapshot();
}

bool ARaidGameMode::IsMissionCompleted(FName MissionId) const
{
    if (MissionId.IsNone()) return true;
    const FRaidMissionSnapshot Snapshot = GetMissionSnapshot();
    return Snapshot.bPresent && Snapshot.bCompleted && Snapshot.MissionId == MissionId;
}

void ARaidGameMode::RegisterMissionObjective(ARaidMissionObjective* Objective)
{
    if (!IsValid(Objective) || Objective->GetWorld() != GetWorld() || !Objective->HasValidDefinition()) return;
    if (MissionObjective.IsValid() && MissionObjective.Get() != Objective)
    {
        UE_LOG(LogTemp, Error, TEXT("R7_MISSION_CONFIG: only one objective is supported per raid; ignoring %s"), *Objective->GetName());
        return;
    }
    MissionObjective = Objective;
    OnMissionChanged.Broadcast(this);
}

void ARaidGameMode::UnregisterMissionObjective(ARaidMissionObjective* Objective)
{
    if (MissionObjective.Get() != Objective) return;
    MissionObjective.Reset();
    OnMissionChanged.Broadcast(this);
}

void ARaidGameMode::NotifyMissionChanged(ARaidMissionObjective* Objective)
{
    if (Objective == GetMissionObjective()) OnMissionChanged.Broadcast(this);
}

void ARaidGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsRaidActive()) RaidElapsed += DeltaSeconds;
    MessageRemaining -= DeltaSeconds;
    if (MessageRemaining <= 0.f) Message.Empty();
}

void ARaidGameMode::PushMessage(const FString& Text, float Seconds)
{
    Message = Text;
    MessageRemaining = Seconds;
}

void ARaidGameMode::EndRaid(bool bSuccess)
{
    // A single gate prevents a death and an extraction in the same frame.
    if (!IsRaidActive()) return;
    ARaidPlayer* Player = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (bSuccess && (!Player || !Player->IsAlive() || Player->GetLootCount() < 1)) return;
    ResumeRaid();
    MissionResult = GetMissionSnapshot();
    MissionResult.bInProgress = false;
    MissionResult.Progress = MissionResult.bCompleted ? 1.f : 0.f;
    if (MissionResult.bPresent && !MissionResult.bCompleted) MissionResult.StatusText = TEXT("本次情报目标未完成");
    bHasMissionResult = true;
    State = bSuccess ? ERaidState::Extracted : ERaidState::Dead;
    SecuredItems = bSuccess && Player ? Player->GetLootCount() : 0;
    SecuredValue = bSuccess && Player ? Player->GetTotalLootValue() : 0;
    Message.Empty();
    if (ARaidPlayer* Pawn = Player)
    {
        Pawn->CancelTreatment();
        Pawn->StopFire();
        Pawn->StopAim();
        Pawn->CloseLoot();
        Pawn->CancelReload();
        Pawn->StopJumping();
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->GetCharacterMovement()->DisableMovement();
    }
    ApplyInputMode();
    if (MissionResult.bPresent) OnMissionChanged.Broadcast(this);
    UE_LOG(LogTemp, Display, TEXT("RAID_RESULT: %s items=%d value=%lld kills=%d time=%.2f"),
        bSuccess ? TEXT("EXTRACTED") : TEXT("DEAD"), SecuredItems, static_cast<long long>(SecuredValue), EnemiesKilled, RaidElapsed);
    if (MissionResult.bPresent)
    {
        UE_LOG(LogTemp, Display, TEXT("R7_MISSION_RESULT: id=%s completed=%d extracted=%d"),
            *MissionResult.MissionId.ToString(), MissionResult.bCompleted, bSuccess);
    }
}

void ARaidGameMode::RestartRaid()
{
    if (State != ERaidState::Extracted && State != ERaidState::Dead) return;
    UE_LOG(LogTemp, Display, TEXT("RAID_RESTART: opening clean map."));
    StartRaid();
}

void ARaidGameMode::ApplyInputMode()
{
    // Compatibility entry point: local input/cursor policy belongs to Controller.
    if (ARaidPlayerController* PC = Cast<ARaidPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
        PC->RefreshInputMode();
}

void ARaidGameMode::StartRaid()
{
    if (IsRaidActive() || State == ERaidState::Instructions || bTravelPending) return;
    bTravelPending = true;
    UE_LOG(LogTemp, Display, TEXT("RAID_DEPLOY: reload the saved map with fresh actors."));
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, TEXT("RaidStart=1"));
}

void ARaidGameMode::ReturnToMenu()
{
    if (bTravelPending || IsRaidActive()) return;
    bTravelPending = true;
    UE_LOG(LogTemp, Display, TEXT("RAID_FRONTEND: returning to a clean main menu."));
    // Absolute travel clears RaidStart, including when returning from a retry.
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, TEXT(""));
}

void ARaidGameMode::HandleMenuAction(FName Action)
{
    if (bTravelPending) return;
    if (bPauseMenuOpen)
    {
        if (Action == TEXT("Resume")) ResumeRaid();
        else if (Action == TEXT("Abandon")) bConfirmAbandon = true;
        else if (Action == TEXT("CancelAbandon")) bConfirmAbandon = false;
        else if (Action == TEXT("ConfirmAbandon") && bConfirmAbandon) EndRaid(false);
        return;
    }
    if (Action == TEXT("Start") && State == ERaidState::MainMenu) StartRaid();
    else if (Action == TEXT("Restart")) RestartRaid();
    else if (Action == TEXT("Menu")) ReturnToMenu();
    else if (Action == TEXT("Instructions") && State == ERaidState::MainMenu)
    {
        State = ERaidState::Instructions;
        ApplyInputMode();
    }
    else if (Action == TEXT("Back")) HandleBack();
    else if (Action == TEXT("Quit") && !IsRaidActive())
    {
        UE_LOG(LogTemp, Display, TEXT("RAID_QUIT: requested from frontend."));
        UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
    }
}

void ARaidGameMode::HandleBack()
{
    if (IsRaidActive())
    {
        if (ARaidPlayer* Player = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            if (Player->IsInventoryOpen())
            {
                Player->CloseLoot();
                return;
            }
        }
    }
    if (State == ERaidState::Instructions)
    {
        State = ERaidState::MainMenu;
        ApplyInputMode();
    }
    else if (IsRaidActive())
    {
        if (bConfirmAbandon) { bConfirmAbandon = false; return; }
        if (bPauseMenuOpen) { ResumeRaid(); return; }
        APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
        if (PC && PC->SetPause(true))
        {
            if (ARaidPlayer* Player = Cast<ARaidPlayer>(PC->GetPawn())) Player->PreparePauseInput();
            bPauseMenuOpen = true;
            ApplyInputMode();
        }
    }
    else if (State == ERaidState::Extracted || State == ERaidState::Dead) ReturnToMenu();
}

void ARaidGameMode::ResumeRaid()
{
    if (!bPauseMenuOpen) return;
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->SetPause(false);
    bPauseMenuOpen = false;
    bConfirmAbandon = false;
    ApplyInputMode();
}
