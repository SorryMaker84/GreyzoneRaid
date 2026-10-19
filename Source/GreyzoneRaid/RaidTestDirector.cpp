#include "RaidTestDirector.h"

#include "RaidEnemy.h"
#include "RaidExtractionZone.h"
#include "RaidGameMode.h"
#include "RaidLoot.h"
#include "RaidPlayer.h"
#include "RaidInventoryComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "UnrealClient.h"

namespace
{
    // Function-static memory survives OpenLevel while the old director is destroyed.
    struct FRunMemory
    {
        int32 WorldPhase = 0; // Initial raid, restart after success, restart after death.
        int32 Failures = 0;
        int32 Checks = 0;
        TWeakObjectPtr<UWorld> PreviousWorld;
        double StartSeconds = 0.0;
        bool bFinished = false;
    };

    FRunMemory& RunMemory()
    {
        static FRunMemory Memory;
        return Memory;
    }

    const FVector LaneStart(-1900.f, -1400.f, 96.f);
    const FVector LaneEnemy(-1100.f, -1400.f, 96.f);
    const FVector ExtractionPosition(-1400.f, -1400.f, 0.f);

    bool GrantSelfTestItem(ARaidPlayer* Player, FName ItemId, const FString& Label, int32 Quantity, int32 Value)
    {
        URaidInventoryComponent* Inventory = Player ? Player->GetInventoryComponent() : nullptr;
        if (!Inventory) return false;
        FRaidItemDefinition Definition;
        Definition.ItemId = ItemId;
        Definition.DisplayName = Label;
        Definition.UnitValue = Value;
        Definition.UseEffect = ERaidItemUseEffect::None;
        Definition.UseAmount = 0;
        return Inventory->Grant(Definition, Quantity, Inventory->GetRevision()).IsSuccess();
    }
}

ARaidTestDirector::ARaidTestDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    // Inject movement before CharacterMovement consumes its input this frame.
    PrimaryActorTick.TickGroup = TG_PrePhysics;
}

void ARaidTestDirector::BeginPlay()
{
    Super::BeginPlay();
    if (!FParse::Param(FCommandLine::Get(), TEXT("RaidSelfTest")))
    {
        Destroy();
        return;
    }
    FRunMemory& Memory = RunMemory();
    if (Memory.bFinished)
    {
        Destroy();
        return;
    }
    if (Memory.StartSeconds == 0.0) Memory.StartSeconds = FPlatformTime::Seconds();
    bMovementProbe = FParse::Param(FCommandLine::Get(), TEXT("R81MovementProbe"));
    if (bMovementProbe)
    {
        FParse::Value(FCommandLine::Get(), TEXT("R81Output="), ProbeOutput);
        IFileManager::Get().MakeDirectory(*ProbeOutput, true);
        ProbeCSV=TEXT("frame,phase,timing,dt,sample_elapsed,physics_input_seconds,input_applied,x,y,z,vx,vy,vz,accel_x,max_speed,can_act,input_ignored,movement_mode\n");
    }
    UE_LOG(LogTemp, Display, TEXT("RAID_SELFTEST: world phase=%d; gameplay-method injection, hardware keyboard/mouse NOT tested."), Memory.WorldPhase);
    if (Memory.WorldPhase == 0)
        UE_LOG(LogTemp, Display, TEXT("RAID_SELFTEST: R6 gameplay regression includes bound Aim, open/search/reveal/manual take and live enemy cover; frontend has separate RaidFlowTest; hardware input NOT tested."));
    EnterStep(EStep::WaitPlayer);
}

void ARaidTestDirector::EnterStep(EStep NewStep)
{
    Step = NewStep;
    StepSeconds = 0.f;
    bActionIssued = false;
    if (bMovementProbe && (Step==EStep::Move || Step==EStep::Sprint)) ProbeAppliedSeconds=0;
}

void ARaidTestDirector::ProbeSample(const TCHAR* Timing, float DeltaSeconds)
{
    if (!Player) return;
    const FVector Pos=Player->GetActorLocation(), Vel=Player->GetVelocity();
    const UCharacterMovementComponent* Movement=Player->GetCharacterMovement();
    ProbeCSV += FString::Printf(TEXT("%llu,%s,%s,%.6f,%.6f,%.6f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d\n"),
        static_cast<unsigned long long>(GFrameCounter), Step==EStep::Move?TEXT("walk"):TEXT("sprint"),Timing,
        DeltaSeconds,StepSeconds,ProbeAppliedSeconds,bProbeInputFrame,Pos.X,Pos.Y,Pos.Z,Vel.X,Vel.Y,Vel.Z,
        Movement->GetCurrentAcceleration().X,Movement->MaxWalkSpeed,Player->CanAct(),Player->IsMoveInputIgnored(),int32(Movement->MovementMode));
}

void ARaidTestDirector::ProbeMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
    if (!bMovementProbe || (Step!=EStep::Move && Step!=EStep::Sprint)) return;
    if (bProbeInputFrame) ProbeAppliedSeconds+=DeltaSeconds;
    ProbeSample(TEXT("after_movement"),DeltaSeconds);
}

void ARaidTestDirector::Check(bool bPassed, const TCHAR* Name, const FString& Detail)
{
    FRunMemory& Memory = RunMemory();
    ++Memory.Checks;
    if (!bPassed) ++Memory.Failures;
    UE_LOG(LogTemp, Display, TEXT("RAID_CHECK:%s %s | %s"),
        bPassed ? TEXT("PASS") : TEXT("FAIL"), Name, *Detail);
}

void ARaidTestDirector::ClearTestActors()
{
    // Only the test world's runtime instances are removed; the saved map is untouched.
    for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<ARaidLoot> It(GetWorld()); It; ++It) It->Destroy();
    LootFixtures.Reset();
    if (IsValid(TestCover)) TestCover->Destroy();
    TestCover = nullptr;
    Enemy = nullptr;
    Loot = nullptr;
}

void ARaidTestDirector::PlacePlayer(const FVector& Location)
{
    Player->GetCharacterMovement()->StopMovementImmediately();
    Player->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    Player->SetActorRotation(FRotator::ZeroRotator);
    if (AController* Controller = Player->GetController()) Controller->SetControlRotation(FRotator::ZeroRotator);
    Player->Camera->SetWorldRotation(FRotator::ZeroRotator);
}

void ARaidTestDirector::AimAt(const FVector& Location)
{
    const FRotator Aim = (Location - Player->Camera->GetComponentLocation()).Rotation();
    if (AController* Controller = Player->GetController()) Controller->SetControlRotation(Aim);
    Player->Camera->SetWorldRotation(Aim);
}

bool ARaidTestDirector::ExecuteBoundAction(FName ActionName, bool bPressed)
{
    // Exercise the game's existing action binding without OS input or native UI automation.
    AController* Controller = Player ? Player->GetController() : nullptr;
    const TArray<UInputComponent*> Inputs = {Controller ? Controller->InputComponent.Get() : nullptr,
        Player ? Player->InputComponent.Get() : nullptr};
    for (UInputComponent* Input : Inputs)
    {
        if (!Input) continue;
        for (int32 Index = 0; Index < Input->GetNumActionBindings(); ++Index)
        {
            FInputActionBinding& Binding = Input->GetActionBinding(Index);
            if (Binding.GetActionName() == ActionName
                && Binding.KeyEvent == (bPressed ? IE_Pressed : IE_Released)
                && Binding.ActionDelegate.IsBound())
            {
                Binding.ActionDelegate.Execute(FKey());
                return true;
            }
        }
    }
    return false;
}

ARaidEnemy* ARaidTestDirector::SpawnEnemy(const FVector& Location, bool bStationary)
{
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ARaidEnemy* Spawned = GetWorld()->SpawnActor<ARaidEnemy>(ARaidEnemy::StaticClass(), Location, FRotator::ZeroRotator, Parameters);
    if (Spawned)
    {
        Spawned->DetectionRadius = 0.f;
        Spawned->AttackRange = 0.f;
        Spawned->AttackDamage = 8.f;
        Spawned->PatrolPoints = {Location + FVector(300.f, 0.f, 0.f), Location - FVector(300.f, 0.f, 0.f)};
        if (bStationary) Spawned->GetCharacterMovement()->DisableMovement();
    }
    return Spawned;
}

ARaidLoot* ARaidTestDirector::SpawnLoot(const FVector& Location, bool bMedkit)
{
    const FTransform Transform(FRotator::ZeroRotator, Location);
    ARaidLoot* Spawned = GetWorld()->SpawnActorDeferred<ARaidLoot>(ARaidLoot::StaticClass(), Transform,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (Spawned)
    {
        Spawned->ItemId = FName(bMedkit ? TEXT("Medkit") : TEXT("SelfTestSalvage"));
        LootFixtures.Add(Spawned);
        Spawned->bGenerateSupplementalLoot = false; // Preserve this regression's exact single-item fixture.
        Spawned->DisplayName = TEXT("测试回收物资");
        Spawned->Quantity = 1;
        Spawned->Value = 40;
        if (bMedkit) Spawned->DisplayName = TEXT("搜索测试医疗包");
        FRaidInventorySeed Seed;
        Seed.Definition.ItemId = Spawned->ItemId;
        Seed.Definition.DisplayName = Spawned->DisplayName;
        Seed.Definition.UnitValue = Spawned->Value;
        Seed.Definition.UseEffect = bMedkit ? ERaidItemUseEffect::Heal : ERaidItemUseEffect::None;
        Seed.Definition.UseAmount = bMedkit ? 45 : 0;
        Seed.Quantity = 1;
        Seed.bRevealed = false;
        if (!Spawned->GetInventoryComponent()->InitializeOnce({Seed}, -1, true).IsSuccess())
        {
            Spawned->Destroy();
            return nullptr;
        }
        Spawned->FinishSpawning(Transform);
    }
    return Spawned;
}

void ARaidTestDirector::BeginScreenshot(const TCHAR* Name, EStep NextStep)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Evidence/R6SelfTest"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    ScreenshotPath = FPaths::Combine(Directory, FString(Name) + TEXT(".png"));
    // Remove stale evidence so FileExists can only confirm this run's screenshot.
    IFileManager::Get().Delete(*ScreenshotPath, false, true);
    FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
    AfterScreenshot = NextStep;
    EnterStep(EStep::Screenshot);
}

void ARaidTestDirector::Finish()
{
    FRunMemory& Memory = RunMemory();
    if (Memory.bFinished) return;
    Memory.bFinished = true;
    UE_LOG(LogTemp, Display, TEXT("RAID_SELFTEST_RESULT: %s checks=%d failures=%d elapsed=%.2fs; hardware input NOT tested."),
        Memory.Failures == 0 ? TEXT("PASS") : TEXT("FAIL"), Memory.Checks, Memory.Failures,
        FPlatformTime::Seconds() - Memory.StartSeconds);
    FPlatformMisc::RequestExitWithStatus(false, Memory.Failures > 0 ? 1 : 0);
}

void ARaidTestDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FRunMemory& Memory = RunMemory();
    if (Memory.bFinished) return;
    if (FPlatformTime::Seconds() - Memory.StartSeconds > 120.0)
    {
        Check(false, TEXT("total_timeout"), FString::Printf(TEXT("phase=%d step=%d exceeded 120 seconds"), Memory.WorldPhase, static_cast<int32>(Step)));
        Finish();
        return;
    }
    StepSeconds += DeltaSeconds;
    bProbeInputFrame=false;
    if (bMovementProbe && (Step==EStep::Move || Step==EStep::Sprint))
    {
        ProbeSample(TEXT("before_movement"),DeltaSeconds);
        if (Step==EStep::Sprint && !bProbeHitchInjected && StepSeconds>.6f && StepSeconds<1.f &&
            FParse::Param(FCommandLine::Get(), TEXT("R81InjectHitch")))
        {
            bProbeHitchInjected=true;
            UE_LOG(LogTemp,Display,TEXT("R81_CONTROLLED_HITCH: sleep=0.45 seconds; diagnostic fixture, NOT historical root-cause proof"));
            FPlatformProcess::Sleep(.45f);
        }
    }
    if (Step != EStep::WaitPlayer && (!IsValid(Player) || !IsValid(Mode)))
    {
        Check(false, TEXT("world_references"), TEXT("Player or GameMode became invalid before travel."));
        Finish();
        return;
    }
    if (Step >= EStep::SearchPartial && Step <= EStep::FullBackpack
        && (!IsValid(Loot) || Loot->IsActorBeingDestroyed()))
    {
        Check(false, TEXT("container_fixture_must_survive_search_and_manual_take"));
        Finish();
        return;
    }

    switch (Step)
    {
    case EStep::WaitPlayer:
        Player = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerCharacter(this, 0));
        Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
        if (IsValid(Player) && IsValid(Mode) && Player->GetController() && Player->Camera)
        {
            if (Memory.WorldPhase > 0)
            {
                // UObject array indices can be recycled; a weak reference also tracks the serial.
                Check(Memory.PreviousWorld.Get() != GetWorld(), TEXT("restart_loads_new_world"));
                Check(FMath::IsNearlyEqual(Player->Health, 100.f) && Player->MagazineAmmo == 12
                    && Player->ReserveAmmo == 60 && Player->GetLootCount() == 0 && Player->GetInventoryComponent()->GetEntries().IsEmpty()
                    && Player->GetTotalLootValue() == 0 && !Player->bReloading && Mode->State == ERaidState::Running
                    && Mode->SecuredItems == 0 && Mode->SecuredValue == 0
                    && !Player->IsInventoryOpen() && !Player->GetOpenLoot() && !Player->IsSearching()
                    && Player->GetSearchIndex() == INDEX_NONE,
                    Memory.WorldPhase == 1 ? TEXT("reset_after_success") : TEXT("reset_after_death"),
                    FString::Printf(TEXT("HP=%.1f magazine=%d reserve=%d items=%d state=%d"), Player->Health,
                        Player->MagazineAmmo, Player->ReserveAmmo, Player->GetLootCount(), static_cast<int32>(Mode->State)));
            }
            ClearTestActors();
            PlacePlayer(LaneStart);
            Player->GetCharacterMovement()->AddTickPrerequisiteActor(this);
            if (bMovementProbe)
            {
                Player->OnCharacterMovementUpdated.AddDynamic(this,&ARaidTestDirector::ProbeMovementUpdated);
                FHitResult Hit;
                const UCapsuleComponent* Capsule=Player->GetCapsuleComponent();
                const FVector Start=Player->GetActorLocation()+FVector(0,0,2);
                FCollisionQueryParams Params(SCENE_QUERY_STAT(R81Lane),false,Player);
                const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,Start,Start+FVector(600,0,0),FQuat::Identity,ECC_Pawn,
                    FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius()-1,Capsule->GetScaledCapsuleHalfHeight()-2),Params);
                UE_LOG(LogTemp,Display,TEXT("R81_LANE: blocked=%d actor=%s start_penetrating=%d normal=%s map=%s"),
                    Blocked,*GetNameSafe(Hit.GetActor()),Hit.bStartPenetrating,*Hit.Normal.ToString(),*GetWorld()->GetMapName());
            }
            if (Memory.WorldPhase == 0)
            {
                InitialLocation = Player->GetActorLocation();
                EnterStep(EStep::Move);
            }
            else if (Memory.WorldPhase == 1)
            {
                Check(GrantSelfTestItem(Player, FName(TEXT("DeathTest")), TEXT("未保全回收物资"), 1, 75), TEXT("death_raid_has_unsecured_loot"));
                Enemy = SpawnEnemy(LaneStart + FVector(350.f, 0.f, 0.f), false);
                Check(IsValid(Enemy), TEXT("death_enemy_spawn"));
                if (!Enemy) { Finish(); return; }
                Enemy->DetectionRadius = 2000.f;
                Enemy->AttackRange = 850.f;
                Enemy->AttackDamage = 45.f;
                Enemy->AttackInterval = .4f;
                EnterStep(EStep::Death);
            }
            else EnterStep(EStep::ResetScreenshotWait);
        }
        else if (StepSeconds > 10.f)
        {
            Check(false, TEXT("player_spawn_and_possession"));
            Finish();
        }
        break;

    case EStep::Move:
        if (StepSeconds <= 1.f)
        {
            bProbeInputFrame=true;
            Player->MoveForward(1.f);
            if (!bActionIssued)
            {
                UE_LOG(LogTemp, Display, TEXT("RAID_MOVE_DIAGNOSTIC: mode=%d active=%d tick=%d ignored=%d pending=%s"),
                    static_cast<int32>(Player->GetCharacterMovement()->MovementMode),
                    Player->GetCharacterMovement()->IsActive(), Player->GetCharacterMovement()->IsComponentTickEnabled(),
                    Player->IsMoveInputIgnored(), *Player->GetPendingMovementInputVector().ToString());
                bActionIssued = true;
            }
        }
        else
        {
            const float Distance = FVector::Dist2D(Player->GetActorLocation(), InitialLocation);
            if(bMovementProbe) UE_LOG(LogTemp,Display,TEXT("R81_MEASUREMENT: phase=walk elapsed=%.6f physics_input_seconds=%.6f distance=%.3f current_dt=%.6f sampling_before_current_movement=1"),StepSeconds,ProbeAppliedSeconds,Distance,DeltaSeconds);
            Check(Distance > 120.f, TEXT("MoveForward_real_movement"), FString::Printf(TEXT("one-second displacement=%.1f cm mode=%d velocity=%s lastinput=%s"), Distance,
                static_cast<int32>(Player->GetCharacterMovement()->MovementMode), *Player->GetVelocity().ToString(), *Player->GetLastMovementInputVector().ToString()));
            WalkDistance = Distance;
            PlacePlayer(LaneStart);
            InitialLocation = Player->GetActorLocation();
            Check(ExecuteBoundAction(TEXT("Sprint"))
                && FMath::IsNearlyEqual(Player->GetCharacterMovement()->MaxWalkSpeed, 560.f),
                TEXT("Sprint_action_sets_560_speed"));
            EnterStep(EStep::Sprint);
        }
        break;

    case EStep::Sprint:
        if (StepSeconds <= 1.f) { bProbeInputFrame=true; Player->MoveForward(1.f); }
        else
        {
            const float Distance = FVector::Dist2D(Player->GetActorLocation(), InitialLocation);
            if(bMovementProbe) UE_LOG(LogTemp,Display,TEXT("R81_MEASUREMENT: phase=sprint elapsed=%.6f physics_input_seconds=%.6f distance=%.3f current_dt=%.6f sampling_before_current_movement=1"),StepSeconds,ProbeAppliedSeconds,Distance,DeltaSeconds);
            Check(Distance > WalkDistance * 1.3f && Player->GetVelocity().Size2D() > 450.f,
                TEXT("Sprint_real_movement_faster_than_walk"),
                FString::Printf(TEXT("walk=%.1f sprint=%.1f cm velocity=%.1f"), WalkDistance, Distance, Player->GetVelocity().Size2D()));
            Check(ExecuteBoundAction(TEXT("Sprint"), false)
                && FMath::IsNearlyEqual(Player->GetCharacterMovement()->MaxWalkSpeed, 350.f),
                TEXT("Sprint_release_restores_walk_speed"));
            if(bMovementProbe)
            {
                FFileHelper::SaveStringToFile(ProbeCSV,*(ProbeOutput/TEXT("Movement.csv")));
                UE_LOG(LogTemp,Display,TEXT("R81_MOVEMENT_DIAGNOSTIC_COMPLETE: injected=%d checks=%d failures=%d"),bProbeHitchInjected,RunMemory().Checks,RunMemory().Failures);
                Finish(); return;
            }
            PlacePlayer(LaneStart);
            EnterStep(EStep::Jump);
        }
        break;

    case EStep::Jump:
        if (!bActionIssued && StepSeconds > .25f && Player->GetCharacterMovement()->IsMovingOnGround())
        {
            InitialLocation = Player->GetActorLocation();
            PeakJumpHeight = 0.f;
            Check(ExecuteBoundAction(TEXT("Jump")), TEXT("Jump_action_binding_executes"));
            bActionIssued = true;
        }
        if (bActionIssued)
        {
            PeakJumpHeight = FMath::Max(PeakJumpHeight, Player->GetActorLocation().Z - InitialLocation.Z);
            if (PeakJumpHeight > 50.f && Player->GetCharacterMovement()->IsFalling())
            {
                Check(true, TEXT("Jump_real_character_rises_and_enters_Falling"),
                    FString::Printf(TEXT("rise=%.1f cm velocityZ=%.1f"), PeakJumpHeight, Player->GetVelocity().Z));
                Check(ExecuteBoundAction(TEXT("Jump"), false), TEXT("Jump_release_binding_executes"));
                EnterStep(EStep::JumpLanding);
            }
        }
        if (Step == EStep::Jump && StepSeconds > 3.f)
        {
            Check(false, TEXT("Jump_real_character_rises_and_enters_Falling"));
            ExecuteBoundAction(TEXT("Jump"), false);
            EnterStep(EStep::JumpLanding);
        }
        break;

    case EStep::JumpLanding:
        if ((StepSeconds > .2f && Player->GetCharacterMovement()->IsMovingOnGround()) || StepSeconds > 3.f)
        {
            Check(Player->GetCharacterMovement()->IsMovingOnGround()
                && FMath::Abs(Player->GetActorLocation().Z - InitialLocation.Z) < 20.f,
                TEXT("Jump_lands_back_on_floor"));
            PlacePlayer(LaneStart);
            EnterStep(EStep::Medkit);
        }
        break;

    case EStep::Medkit:
        if (!bActionIssued)
        {
            UGameplayStatics::ApplyDamage(Player,65.f,nullptr,this,UDamageType::StaticClass());
            Check(Player->Health==35.f && Player->AddItem(TEXT("Medkit"),TEXT("医疗包"),2,25),TEXT("Medkit_test_damage_and_inventory_setup"));
            ExecuteBoundAction(TEXT("Heal")); bActionIssued=true; MedicalStage=0; StepSeconds=0;
            break;
        }
        if (StepSeconds<3.3f) break;
        if (MedicalStage==0)
        {
            Check(Player->Health==80.f && Player->GetLootCount()==1 && Player->GetTotalLootValue()==25
                && Player->GetInventoryComponent()->GetEntries().Num()==1 && Player->GetInventoryComponent()->GetEntries()[0].Quantity==1,
                TEXT("Heal_action_restores_45_after_timer_and_consumes_one_medkit"));
            ExecuteBoundAction(TEXT("Heal")); MedicalStage=1; StepSeconds=0; break;
        }
        if (MedicalStage==1)
        {
            Check(Player->Health==100.f && Player->GetLootCount()==0 && Player->GetTotalLootValue()==0
                && Player->GetInventoryComponent()->GetEntries().IsEmpty(),TEXT("Heal_caps_at_100_and_removes_last_stack"));
            UGameplayStatics::ApplyDamage(Player,10.f,nullptr,this,UDamageType::StaticClass());
            Check(ExecuteBoundAction(TEXT("Heal")) && Player->Health==90.f && Player->GetLootCount()==0,TEXT("Heal_without_medkit_does_not_change_health"));
            Player->Health=100.f;
            Check(Player->AddItem(TEXT("Medkit"),TEXT("医疗包"),1,25) && ExecuteBoundAction(TEXT("Heal"))
                && Player->Health==100.f && Player->GetLootCount()==1 && Player->GetTotalLootValue()==25,TEXT("Heal_at_full_health_keeps_medkit"));
            UGameplayStatics::ApplyDamage(Player,45.f,nullptr,this,UDamageType::StaticClass());
            ExecuteBoundAction(TEXT("Heal"));MedicalStage=2;StepSeconds=0;break;
        }
        Check(Player->Health==100.f && Player->GetLootCount()==0 && Player->GetInventoryComponent()->GetEntries().IsEmpty(),TEXT("Medkit_test_restores_clean_player_state"));
        InitialYaw=Player->GetController()->GetControlRotation().Yaw; EnterStep(EStep::Turn);break;

    case EStep::Turn:
        if (StepSeconds < .3f) Player->Turn(30.f * DeltaSeconds);
        else
        {
            const float YawChange = FMath::Abs(FMath::FindDeltaAngleDegrees(InitialYaw, Player->GetController()->GetControlRotation().Yaw));
            Check(YawChange > 2.f, TEXT("Turn_changes_view"), FString::Printf(TEXT("yaw change=%.2f degrees"), YawChange));
            PlacePlayer(LaneStart);
            Enemy = SpawnEnemy(LaneStart + FVector(600.f, 0.f, 0.f), true);
            Check(IsValid(Enemy), TEXT("target_enemy_spawn"));
            if (!Enemy) { Finish(); return; }
            InitialEnemyHealth = Enemy->Health;
            InitialMagazine = Player->MagazineAmmo;
            EnterStep(EStep::AimIn);
        }
        break;

    case EStep::AimIn:
        if (!bActionIssued)
        {
            InitialFOV = Player->Camera->FieldOfView;
            Check(ExecuteBoundAction(TEXT("Aim")) && Player->bAiming,
                TEXT("Aim_press_binding_starts_ADS"), TEXT("Existing IE_Pressed action delegate; no hardware mouse event."));
            bActionIssued = true;
        }
        if (StepSeconds > .6f)
        {
            HeldADSFOV = Player->Camera->FieldOfView;
            EnterStep(EStep::AimHold);
        }
        break;

    case EStep::AimHold:
        if (StepSeconds > .25f)
        {
            Check(Player->bAiming && Player->ADSAlpha > .98f
                && FMath::IsNearlyEqual(HeldADSFOV, 65.f, .6f)
                && FMath::IsNearlyEqual(Player->Camera->FieldOfView, 65.f, .6f)
                && FMath::Abs(Player->Camera->FieldOfView - HeldADSFOV) < .3f
                && FMath::IsNearlyEqual(Player->GetCharacterMovement()->MaxWalkSpeed, 245.f),
                TEXT("Aim_hold_ticks_to_stable_ADS_and_65_FOV"),
                FString::Printf(TEXT("alpha=%.3f FOV=%.2f heldFOV=%.2f speed=%.1f"),
                    Player->ADSAlpha, Player->Camera->FieldOfView, HeldADSFOV,
                    Player->GetCharacterMovement()->MaxWalkSpeed));
            bReleaseBindingExecuted = ExecuteBoundAction(TEXT("Aim"), false);
            EnterStep(EStep::AimOut);
        }
        break;

    case EStep::AimOut:
        if (StepSeconds > .6f)
        {
            Check(bReleaseBindingExecuted && !Player->bAiming && Player->ADSAlpha < .02f
                && FMath::IsNearlyEqual(Player->Camera->FieldOfView, InitialFOV, .6f)
                && FMath::IsNearlyEqual(Player->Camera->FieldOfView, 95.f, .6f)
                && FMath::IsNearlyEqual(Player->GetCharacterMovement()->MaxWalkSpeed, 350.f),
                TEXT("Aim_release_binding_restores_hip_FOV_and_walk_speed"),
                FString::Printf(TEXT("release=%d alpha=%.3f FOV=%.2f initialFOV=%.2f"),
                    bReleaseBindingExecuted, Player->ADSAlpha, Player->Camera->FieldOfView, InitialFOV));
            EnterStep(EStep::FireDamage);
        }
        break;

    case EStep::FireDamage:
        AimAt(Enemy->GetActorLocation() + FVector(0.f, 0.f, 20.f));
        if (!bActionIssued && StepSeconds > .15f) { Player->Fire(); bActionIssued = true; }
        if (bActionIssued && StepSeconds > .35f)
        {
            Check(Enemy->Health < InitialEnemyHealth && Enemy->Health > 0.f, TEXT("Fire_hits_and_damages_enemy"),
                FString::Printf(TEXT("enemy HP %.1f -> %.1f"), InitialEnemyHealth, Enemy->Health));
            Check(Player->MagazineAmmo == InitialMagazine - 1, TEXT("Fire_consumes_one_round"));
            EnterStep(EStep::FireKill);
        }
        break;

    case EStep::FireKill:
        AimAt(Enemy->GetActorLocation() + FVector(0.f, 0.f, 20.f));
        Player->Fire();
        if (Enemy->Health <= 0.f || StepSeconds > 3.f)
        {
            Check(Enemy->Health <= 0.f && Enemy->StateText == TEXT("Dead") && Mode->EnemiesKilled > 0, TEXT("Fire_kills_enemy"));
            ClearTestActors(); // Includes loot created by the actual enemy death.
            AimAt(LaneStart + FVector(5000.f, 0.f, 64.f));
            EnterStep(EStep::EmptyMagazine);
        }
        break;

    case EStep::EmptyMagazine:
        Player->Fire();
        if (Player->MagazineAmmo == 0 || StepSeconds > 6.f)
        {
            Check(Player->MagazineAmmo == 0, TEXT("magazine_can_be_emptied"));
            InitialReserve = Player->ReserveAmmo;
            EnterStep(EStep::DryFire);
        }
        break;

    case EStep::DryFire:
        Player->Fire();
        if (StepSeconds > .5f)
        {
            Check(Player->MagazineAmmo == 0 && Player->ReserveAmmo == InitialReserve, TEXT("empty_magazine_does_not_consume_ammo"));
            Player->Reload();
            Check(Player->bReloading && Player->MagazineAmmo == 0, TEXT("automatic_reload_stays_active_without_duplicate_manual_timer"));
            EnterStep(EStep::Reload);
        }
        break;

    case EStep::Reload:
        if ((!Player->bReloading && StepSeconds > .2f) || StepSeconds > 5.f)
        {
            Check(!Player->bReloading && Player->MagazineAmmo == 12 && Player->ReserveAmmo == InitialReserve - 12
                && StepSeconds >= .8f, TEXT("automatic_reload_timer_completes_and_transfers_ammo_after_dry_phase"),
                FString::Printf(TEXT("elapsed=%.2f magazine=%d reserve=%d"), StepSeconds, Player->MagazineAmmo, Player->ReserveAmmo));
            PlacePlayer(LaneStart);
            Loot = SpawnLoot(Player->Camera->GetComponentLocation() + FVector(150.f, 0.f, 0.f), true);
            Check(IsValid(Loot), TEXT("loot_spawn"));
            if (!Loot) { Finish(); return; }
            // A consumable fixture lets timed search return to the original empty-backpack chain.
            Loot->SearchDuration = 1.1f;
            SearchTestDuration = Loot->SearchDuration;
            ExpectedItems = Player->GetLootCount();
            ExpectedValue = Player->GetTotalLootValue();
            InitialHealth = Player->Health;
            EnterStep(EStep::SearchPartial);
        }
        break;

    case EStep::SearchPartial:
        if (!bActionIssued)
        {
            AimAt(Loot->GetActorLocation());
            Check(Player->GetFocusedLoot() == Loot && ExecuteBoundAction(TEXT("Interact"))
                && Player->GetOpenLoot() == Loot && Player->IsInventoryOpen() && Player->IsSearching(),
                TEXT("E_binding_opens_focused_container_and_auto_searches_without_take"),
                TEXT("Existing IE_Pressed Interact delegate; no hardware keyboard event."));
            Check(Loot->GetContents().Num() == 1 && !Loot->GetContents()[0].bRevealed
                && Player->GetLootCount() == ExpectedItems && Player->GetTotalLootValue() == ExpectedValue,
                TEXT("new_container_starts_with_unknown_item_and_unchanged_backpack"));
            Player->BeginSearch();
            Check(Player->IsSearching() && Player->GetSearchIndex() == 0,
                TEXT("open_container_search_starts_real_item_timer"));
            bActionIssued = true;
        }
        if (StepSeconds > SearchTestDuration * .35f)
        {
            const float Progress = Player->GetSearchProgress();
            const bool bWasPartial = Player->IsSearching() && Progress > .1f && Progress < .8f;
            Player->CancelSearch();
            Check(bWasPartial && !Player->IsSearching()
                && FMath::IsNearlyZero(Player->GetSearchProgress()) && IsValid(Loot)
                && !Loot->IsActorBeingDestroyed() && Player->GetLootCount() == ExpectedItems
                && Player->GetTotalLootValue() == ExpectedValue && Loot->FindNextUnsearchedIndex() == 0,
                TEXT("cancel_button_action_discards_partial_progress_without_revealing_or_taking"),
                FString::Printf(TEXT("progress_before_cancel=%.3f items=%d value=%lld"),
                    Progress, Player->GetLootCount(), static_cast<long long>(Player->GetTotalLootValue())));
            EnterStep(EStep::SearchCancelWait);
        }
        break;

    case EStep::SearchCancelWait:
        if (StepSeconds > SearchTestDuration + .2f)
        {
            Check(!Player->IsSearching() && FMath::IsNearlyZero(Player->GetSearchProgress())
                && IsValid(Loot) && !Loot->IsActorBeingDestroyed() && Loot->FindNextUnsearchedIndex() == 0
                && Player->GetLootCount() == ExpectedItems && Player->GetTotalLootValue() == ExpectedValue,
                TEXT("cancelled_search_does_not_reveal_or_collect_after_original_duration"),
                FString::Printf(TEXT("wait_after_cancel=%.2fs required=%.2fs"), StepSeconds, SearchTestDuration));
            EnterStep(EStep::SearchComplete);
        }
        break;

    case EStep::SearchComplete:
        if (!bActionIssued)
        {
            if (!IsValid(Loot)) { Check(false, TEXT("continuous_search_fixture_exists")); Finish(); return; }
            Player->BeginSearch();
            bSearchStarted = Player->IsSearching();
            bActionIssued = true;
        }
        if ((!Player->IsSearching() && StepSeconds > .2f) || StepSeconds > SearchTestDuration + 1.f)
        {
            const bool bCompleted = bSearchStarted && StepSeconds >= SearchTestDuration - .1f
                && IsValid(Loot) && !Loot->IsActorBeingDestroyed() && !Player->IsSearching()
                && Loot->GetContents().Num() == 1 && Loot->GetContents()[0].bRevealed
                && Loot->GetContents()[0].Quantity == 1
                && Player->GetLootCount() == ExpectedItems && Player->GetTotalLootValue() == ExpectedValue;
            Check(bCompleted, TEXT("search_timer_only_reveals_item_and_keeps_backpack_empty"),
                FString::Printf(TEXT("elapsed=%.2f duration=%.2f items=%d value=%lld"),
                    StepSeconds, SearchTestDuration, Player->GetLootCount(), static_cast<long long>(Player->GetTotalLootValue())));
            Check(Player->TakeLootItem(0), TEXT("manual_take_collects_revealed_medkit"));
            const FRaidInventoryEntry* Kit = Player->GetInventoryComponent()->GetEntries().FindByPredicate([](const FRaidInventoryEntry& Item)
            {
                return Item.Definition.ItemId == FName(TEXT("Medkit")) && Item.Quantity == 1 && Item.Definition.UnitValue == 40;
            });
            Check(Kit && Player->GetLootCount() == ExpectedItems + 1 && Player->GetTotalLootValue() == ExpectedValue + 40
                && IsValid(Loot) && !Loot->IsActorBeingDestroyed() && Loot->GetContents()[0].Quantity == 0,
                TEXT("manual_take_updates_exact_stack_and_preserves_empty_container"));
            Player->CloseLoot();
            // Consume through the existing Heal action, keeping the original inventory/health chain.
            if (Kit)
            {
                UGameplayStatics::ApplyDamage(Player, 45.f, nullptr, this, UDamageType::StaticClass());
                ExecuteBoundAction(TEXT("Heal"));
            }
            EnterStep(EStep::SearchHealWait);
        }
        break;

    case EStep::SearchHealWait:
        if (StepSeconds < 3.3f) break;
        Check(Player->GetLootCount() == ExpectedItems && Player->GetTotalLootValue() == ExpectedValue
            && FMath::IsNearlyEqual(Player->Health, InitialHealth),
            TEXT("search_consumable_fixture_restores_original_inventory_chain"));
        // Preserve the emptied case; the next fixture is closer to the player.
        Loot = SpawnLoot(Player->Camera->GetComponentLocation() + FVector(120.f, 0.f, 0.f));
        if (!Loot) { Check(false, TEXT("manual_collect_fixture_spawn")); Finish(); return; }
        EnterStep(EStep::Collect);
        break;

    case EStep::Collect:
        if (!bActionIssued)
        {
            AimAt(Loot->GetActorLocation());
            Check(Player->GetFocusedLoot() == Loot, TEXT("line_of_sight_finds_loot"));
            Player->Interact();
            Check(Player->GetOpenLoot() == Loot && Player->IsSearching() && Player->GetLootCount() == 0,
                TEXT("Interact_opens_case_without_collecting"));
            Player->BeginSearch();
            bActionIssued = true;
        }
        if ((!Player->IsSearching() && StepSeconds > .2f) || StepSeconds > Loot->SearchDuration + 1.f)
        {
            Check(Player->GetLootCount() == 0 && Player->GetTotalLootValue() == 0,
                TEXT("second_search_also_leaves_inventory_empty_until_manual_take"));
            Check(Player->TakeLootItem(0) && Player->GetLootCount() == 1 && Player->GetTotalLootValue() == 40
                && IsValid(Loot) && !Loot->IsActorBeingDestroyed() && Loot->GetContents()[0].Quantity == 0,
                TEXT("manual_take_updates_inventory_and_keeps_empty_case"));
            Player->CloseLoot();
            const int32 Slots = Player->MaxLootItems - Player->GetLootCount();
            Check(Slots >= 0 && (Slots == 0 || GrantSelfTestItem(Player, FName(TEXT("CapacityTest")), TEXT("容量测试物资"), Slots, 10)), TEXT("fill_backpack_to_capacity"));
            ExpectedItems = Player->GetLootCount();
            ExpectedValue = Player->GetTotalLootValue();
            Loot = SpawnLoot(Player->Camera->GetComponentLocation() + FVector(90.f, 0.f, 0.f));
            if (!Loot) { Check(false, TEXT("full_backpack_loot_spawn")); Finish(); return; }
            EnterStep(EStep::FullBackpack);
        }
        break;

    case EStep::FullBackpack:
        if (!bActionIssued)
        {
            AimAt(Loot->GetActorLocation());
            Player->Interact();
            Player->BeginSearch();
            Check(Player->GetOpenLoot() == Loot && Player->IsSearching(),
                TEXT("full_backpack_still_allows_container_open_and_search"));
            bActionIssued = true;
        }
        if ((!Player->IsSearching() && StepSeconds > .2f) || StepSeconds > Loot->SearchDuration + 1.f)
        {
            Check(Loot->GetContents().Num() == 1 && Loot->GetContents()[0].bRevealed
                && !Player->TakeLootItem(0), TEXT("full_backpack_manual_take_returns_false"));
            Check(IsValid(Loot) && !Loot->IsActorBeingDestroyed() && Loot->GetContents().Num() == 1
                && Loot->GetContents()[0].Quantity == 1
                && Player->GetLootCount() == Player->MaxLootItems && Player->GetLootCount() == ExpectedItems
                && Player->GetTotalLootValue() == ExpectedValue, TEXT("full_backpack_keeps_world_loot"));
            Player->CloseLoot();
            Check(ExecuteBoundAction(TEXT("Inventory")) && Player->IsInventoryOpen() && !Player->GetOpenLoot(),
                TEXT("Tab_action_opens_inventory_panel_state"));
            InitialMagazine = Player->MagazineAmmo;
            Player->Fire();
            Player->StartFire();
            EnterStep(EStep::InventoryOpen);
        }
        break;
    case EStep::InventoryOpen:
        if (StepSeconds > .5f)
        {
            Check(Player->IsInventoryOpen() && Player->MagazineAmmo == InitialMagazine
                && Player->GetLootCount() == ExpectedItems && Player->GetTotalLootValue() == ExpectedValue,
                TEXT("Inventory_open_blocks_fire_and_preserves_items"));
            BeginScreenshot(TEXT("05_inventory_panel"), EStep::InventoryClose);
        }
        break;

    case EStep::InventoryClose:
        Check(ExecuteBoundAction(TEXT("Inventory")) && !Player->IsInventoryOpen(),
            TEXT("Tab_action_closes_inventory_panel_state"));
        {
            PlacePlayer(LaneStart);
            // The R6 container assertions above deliberately keep the two emptied
            // cases and the full-backpack case alive. These camera-height fixtures
            // block Visibility, so remove only this director's loot fixtures before
            // the independent AI/cover lane test. Normal world containers are not
            // changed, and all container-persistence assertions have already run.
            FVector PlayerEye;
            FRotator PlayerView;
            Player->GetActorEyesViewPoint(PlayerEye, PlayerView);
            const FVector SightStart = LaneEnemy + FVector(0.f, 0.f, 57.f);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(RaidSelfTestFixtureSight), false, this);
            FHitResult BeforeCleanup;
            GetWorld()->LineTraceSingleByChannel(BeforeCleanup, SightStart, PlayerEye, ECC_Visibility, Query);
            const FString PreviousBlocker = GetNameSafe(BeforeCleanup.GetActor());
            const FVector PreviousImpact = BeforeCleanup.ImpactPoint;
            int32 RemovedFixtures = 0;
            for (ARaidLoot* Fixture : LootFixtures)
                if (IsValid(Fixture) && Fixture->Destroy()) ++RemovedFixtures;
            LootFixtures.Reset();
            Loot = nullptr;
            FHitResult AfterCleanup;
            const bool bHit = GetWorld()->LineTraceSingleByChannel(AfterCleanup, SightStart, PlayerEye, ECC_Visibility, Query);
            UE_LOG(LogTemp, Display, TEXT("RAID_SELFTEST_FIXTURE_CLEANUP: removed=%d sightStart=%s playerEye=%s before=%s impact=%s after=%s"),
                RemovedFixtures, *SightStart.ToString(), *PlayerEye.ToString(), *PreviousBlocker,
                *PreviousImpact.ToString(), *GetNameSafe(AfterCleanup.GetActor()));
            Check(!bHit || AfterCleanup.GetActor() == Player, TEXT("independent_AI_lane_has_clear_sight_after_container_fixture_cleanup"),
                FString::Printf(TEXT("removed=%d previousBlocker=%s remainingBlocker=%s"),
                    RemovedFixtures, *PreviousBlocker, *GetNameSafe(AfterCleanup.GetActor())));
            LastNavigationAttempt = -1.f;
            EnterStep(EStep::Navigation);
        }
        break;

    case EStep::Navigation:
        if (StepSeconds - LastNavigationAttempt > .5f)
        {
            LastNavigationAttempt = StepSeconds;
            UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, LaneStart, LaneEnemy, Player);
            if (Path && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() > 1 && Path->GetPathLength() > 120.0)
            {
                UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld());
                const ANavigationData* Data = Navigation ? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
                Check(Data && Data->GetRuntimeGenerationMode() == ERuntimeGenerationType::Dynamic,
                    TEXT("navigation_runtime_generation_is_Dynamic"));
                Check(true, TEXT("dynamic_navigation_has_complete_path"), FString::Printf(TEXT("points=%d length=%.1f cm wait=%.2f s"), Path->PathPoints.Num(), Path->GetPathLength(), StepSeconds));
                Enemy = SpawnEnemy(LaneEnemy, false);
                if (!Enemy) { Check(false, TEXT("AI_enemy_spawn")); Finish(); return; }
                EnemyInitialLocation = Enemy->GetActorLocation();
                EnterStep(EStep::Patrol);
            }
        }
        if (Step == EStep::Navigation && StepSeconds > 25.f)
        {
            Check(false, TEXT("dynamic_navigation_has_complete_path"), TEXT("No complete lane path within 25 seconds."));
            Enemy = SpawnEnemy(LaneEnemy, false);
            if (!Enemy) { Check(false, TEXT("AI_enemy_spawn")); Finish(); return; }
            EnemyInitialLocation = Enemy->GetActorLocation();
            EnterStep(EStep::Patrol);
        }
        break;

    case EStep::Patrol:
        if (FVector::Dist2D(Enemy->GetActorLocation(), EnemyInitialLocation) > 100.f || StepSeconds > 5.f)
        {
            Check(Enemy->StateText == TEXT("Patrol") && Enemy->GetController()
                && FVector::Dist2D(Enemy->GetActorLocation(), EnemyInitialLocation) > 100.f,
                TEXT("AI_ticks_and_patrols_using_navigation"), FString::Printf(TEXT("state=%s displacement=%.1f"), *Enemy->StateText, FVector::Dist2D(Enemy->GetActorLocation(), EnemyInitialLocation)));
            Enemy->DetectionRadius = 2000.f;
            Enemy->AttackRange = 120.f;
            EnemyInitialLocation = Enemy->GetActorLocation();
            bObservedChase = false;
            EnterStep(EStep::Chase);
        }
        break;

    case EStep::Chase:
        bObservedChase |= Enemy->StateText == TEXT("Chase");
        if (FVector::Dist2D(EnemyInitialLocation, Player->GetActorLocation())
            - FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation()) > 120.f || StepSeconds > 7.f)
        {
            Check(bObservedChase, TEXT("AI_detects_player_and_enters_Chase"));
            Check(FVector::Dist2D(EnemyInitialLocation, Player->GetActorLocation())
                - FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation()) > 120.f,
                TEXT("AI_chases_player_with_real_movement"));
            Enemy->AttackRange = 1400.f;
            InitialHealth = Player->Health;
            EnterStep(EStep::Attack);
        }
        break;

    case EStep::Attack:
        if (Player->Health < InitialHealth || StepSeconds > 5.f)
        {
            Check(Player->Health < InitialHealth && Player->Health > 0.f && Enemy->StateText == TEXT("Attack"),
                TEXT("AI_attack_applies_real_player_damage"), FString::Printf(TEXT("HP %.1f -> %.1f state=%s"), InitialHealth, Player->Health, *Enemy->StateText));
            BeginScreenshot(TEXT("01_combat_inventory"), EStep::CoverSetup);
        }
        break;

    case EStep::CoverSetup:
        if (!IsValid(Enemy)) { Check(false, TEXT("cover_enemy_fixture_exists")); Finish(); return; }
        // Keep the already-verified attacker ticking in place so it cannot walk around this
        // controlled blocker while we measure damage across several attack intervals.
        Enemy->GetCharacterMovement()->DisableMovement();
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            TestCover = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector,
                FRotator::ZeroRotator, Parameters);
            if (!TestCover) { Check(false, TEXT("cover_collision_fixture_spawn")); Finish(); return; }
            UBoxComponent* Blocker = NewObject<UBoxComponent>(TestCover, TEXT("RuntimeBulletCover"));
            TestCover->SetRootComponent(Blocker);
            TestCover->AddInstanceComponent(Blocker);
            Blocker->SetMobility(EComponentMobility::Movable);
            Blocker->SetBoxExtent(FVector(24.f, 260.f, 170.f));
            Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Blocker->SetCollisionObjectType(ECC_WorldStatic);
            Blocker->SetCollisionResponseToAllChannels(ECR_Block);
            Blocker->SetCanEverAffectNavigation(false);
            Blocker->RegisterComponent();
            FVector Midpoint = (Enemy->GetActorLocation() + Player->GetActorLocation()) * .5f;
            Midpoint.Z = 145.f;
            TestCover->SetActorLocation(Midpoint);
            FHitResult Hit;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(RaidCoverEvidence), false, Enemy);
            const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit,
                Enemy->GetActorLocation() + FVector(0.f, 0.f, 57.f),
                Player->Camera->GetComponentLocation(), ECC_Visibility, Query);
            Check(bHit && Hit.GetActor() == TestCover, TEXT("runtime_cover_blocks_enemy_to_player_visibility_ray"),
                FString::Printf(TEXT("blockingActor=%s impact=%s"), *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToString()));
        }
        InitialHealth = Player->Health;
        bObservedCoverResponse = false;
        EnterStep(EStep::CoverBlocked);
        break;

    case EStep::CoverBlocked:
        bObservedCoverResponse |= Enemy->StateText == TEXT("Search") || Enemy->StateText == TEXT("Chase");
        if (StepSeconds > FMath::Max(2.8f, Enemy->AttackInterval * 2.f + .5f))
        {
            Check(IsValid(TestCover) && Enemy->IsActorTickEnabled() && Enemy->Health > 0.f
                && Mode->IsRaidActive() && bObservedCoverResponse && FMath::IsNearlyEqual(Player->Health, InitialHealth),
                TEXT("live_enemy_cover_prevents_damage_across_attack_intervals"),
                FString::Printf(TEXT("blocked=%.2fs interval=%.2fs HP %.1f -> %.1f AI=%s tick=%d"),
                    StepSeconds, Enemy->AttackInterval, InitialHealth, Player->Health, *Enemy->StateText, Enemy->IsActorTickEnabled()));
            TestCover->Destroy();
            TestCover = nullptr;
            InitialHealth = Player->Health;
            EnterStep(EStep::CoverRemoved);
        }
        break;

    case EStep::CoverRemoved:
        if (Player->Health < InitialHealth || StepSeconds > 5.f)
        {
            Check(Player->Health < InitialHealth && Player->Health > 0.f && Enemy->StateText == TEXT("Attack"),
                TEXT("removing_cover_allows_real_enemy_damage_again"),
                FString::Printf(TEXT("HP %.1f -> %.1f state=%s elapsed=%.2fs"),
                    InitialHealth, Player->Health, *Enemy->StateText, StepSeconds));
            EnterStep(EStep::EnterExtraction);
        }
        break;

    case EStep::EnterExtraction:
        ClearTestActors();
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Extraction = GetWorld()->SpawnActor<ARaidExtractionZone>(ARaidExtractionZone::StaticClass(), ExtractionPosition, FRotator::ZeroRotator, Parameters);
        }
        Check(IsValid(Extraction), TEXT("extraction_spawn"));
        if (!Extraction) { Finish(); return; }
        Extraction->ExtractSeconds = 8.f;
        PlacePlayer(ExtractionPosition + FVector(0.f, 0.f, 96.f));
        EnterStep(EStep::PartialExtraction);
        break;

    case EStep::PartialExtraction:
        if (StepSeconds > 1.5f)
        {
            Check(Extraction->IsPlayerInside() && Extraction->GetSecondsRemaining() < 7.f
                && Extraction->GetSecondsRemaining() > 4.f && Mode->State == ERaidState::Running,
                TEXT("extraction_starts_countdown_with_loot"), FString::Printf(TEXT("remaining=%.2f"), Extraction->GetSecondsRemaining()));
            PlacePlayer(LaneStart);
            EnterStep(EStep::ExitExtraction);
        }
        break;

    case EStep::ExitExtraction:
        if (StepSeconds > .4f)
        {
            Check(!Extraction->IsPlayerInside() && FMath::IsNearlyEqual(Extraction->GetSecondsRemaining(), 8.f, .05f)
                && Mode->State == ERaidState::Running, TEXT("leaving_extraction_resets_countdown"));
            PlacePlayer(ExtractionPosition + FVector(0.f, 0.f, 96.f));
            EnterStep(EStep::CompleteExtraction);
        }
        break;

    case EStep::CompleteExtraction:
        if (Mode->State == ERaidState::Extracted)
        {
            Check(StepSeconds >= 7.8f && Extraction->GetSecondsRemaining() <= .01f, TEXT("eight_second_extraction_completes"), FString::Printf(TEXT("elapsed=%.2f"), StepSeconds));
            Check(Mode->SecuredItems == ExpectedItems && Mode->SecuredItems > 0 && Mode->SecuredValue == ExpectedValue,
                TEXT("success_secures_exact_inventory"), FString::Printf(TEXT("items=%d value=%lld"), Mode->SecuredItems, static_cast<long long>(Mode->SecuredValue)));
            BeginScreenshot(TEXT("02_extracted_success"), EStep::TravelSuccess);
        }
        else if (StepSeconds > 12.f)
        {
            Check(false, TEXT("eight_second_extraction_completes"), TEXT("Did not reach Extracted after 12 seconds inside."));
            BeginScreenshot(TEXT("02_extraction_failure"), EStep::Finish);
        }
        break;

    case EStep::Death:
        if (!Player->IsAlive() || StepSeconds > 8.f)
        {
            Check(!Player->IsAlive() && FMath::IsNearlyZero(Player->Health) && Mode->State == ERaidState::Dead,
                TEXT("real_enemy_attack_causes_death_and_failed_raid"), FString::Printf(TEXT("HP=%.1f state=%d"), Player->Health, static_cast<int32>(Mode->State)));
            Check(Mode->State == ERaidState::Dead && Mode->SecuredItems == 0 && Mode->SecuredValue == 0,
                TEXT("death_does_not_secure_carried_loot"));
            BeginScreenshot(TEXT("03_dead_failure"), Mode->State == ERaidState::Dead ? EStep::TravelDeath : EStep::Finish);
        }
        break;

    case EStep::Screenshot:
        if (StepSeconds > .25f && IFileManager::Get().FileSize(*ScreenshotPath) > 0)
        {
            Check(true, TEXT("screenshot_saved"), ScreenshotPath);
            EnterStep(AfterScreenshot);
        }
        else if (StepSeconds > 8.f)
        {
            Check(false, TEXT("screenshot_saved"), ScreenshotPath);
            EnterStep(AfterScreenshot);
        }
        break;

    case EStep::ResetScreenshotWait:
        if (StepSeconds > 3.f) BeginScreenshot(TEXT("04_restart_clean"), EStep::Finish);
        break;

    case EStep::TravelSuccess:
    case EStep::TravelDeath:
        Memory.WorldPhase = Step == EStep::TravelSuccess ? 1 : 2;
        Memory.PreviousWorld = GetWorld();
        Mode->RestartRaid();
        EnterStep(EStep::AwaitTravel);
        break;

    case EStep::AwaitTravel:
        if (StepSeconds > 10.f)
        {
            Check(false, TEXT("RestartRaid_OpenLevel_travel"), TEXT("The old world remained active after RestartRaid."));
            Finish();
        }
        break;

    case EStep::Finish:
        Finish();
        break;
    }
}
