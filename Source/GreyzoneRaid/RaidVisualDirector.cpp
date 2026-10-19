#include "RaidVisualDirector.h"
#include "RaidPlayer.h"
#include "RaidPlayerController.h"
#include "RaidEnemy.h"
#include "RaidLoot.h"
#include "RaidGameMode.h"
#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

ARaidVisualDirector::ARaidVisualDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    PrimaryActorTick.bHighPriority = true;
}

void ARaidVisualDirector::BeginPlay()
{
    Super::BeginPlay();
    bVisualReviewEnabled = FParse::Param(FCommandLine::Get(), TEXT("RaidVisualReview"));
    bHandsReview = FParse::Param(FCommandLine::Get(), TEXT("RaidHandsReview"));
    FParse::Value(FCommandLine::Get(), TEXT("RaidVisualOutput="), ReviewOutput);
    if (!bVisualReviewEnabled)
    {
        Destroy();
        return;
    }
    IgnoreExternalViewportInput();
    // This director exists only for the opt-in visual review. Protect setup even
    // before possession or the first render/shader-warmup tick is available.
    SuppressBackgroundEnemies();
    UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_SETUP: background enemy suppression active from BeginPlay"));
}

void ARaidVisualDirector::IgnoreExternalViewportInput()
{
    if (!bVisualReviewEnabled) return;
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
    {
        // This blocks external viewport events, not direct gameplay calls or AI.
        // GameOnly/GameAndUI reset this flag, so keep it set across review steps.
        Viewport->SetIgnoreInput(true);
        if (!bLoggedInputIsolation)
        {
            UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_SETUP: external viewport input ignored; internal gameplay/Canvas calls only"));
            bLoggedInputIsolation = true;
        }
    }
}

void ARaidVisualDirector::SuppressBackgroundEnemies()
{
    for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It)
    {
        // The deliberately spawned guard must keep ticking for its hit reaction
        // and the later real AI-damage capture. Never suppress that test subject.
        if (*It == ReviewEnemy) continue;
        It->DetectionRadius = 0.f;
        It->AttackRange = 0.f;
        It->SetActorTickEnabled(false);
        if (AAIController* AI = Cast<AAIController>(It->GetController())) AI->StopMovement();
        It->GetCharacterMovement()->StopMovementImmediately();
        It->GetCharacterMovement()->DisableMovement();
    }
}

void ARaidVisualDirector::Advance(int32 NextStage)
{
    Stage = NextStage;
    StageSeconds = 0.f;
    bIssued = false;
}

void ARaidVisualDirector::Fail(const TCHAR* Reason)
{
    UE_LOG(LogTemp, Error, TEXT("R6_VISUAL_REVIEW_FAILED: stage=%d; %s"), Stage, Reason);
    SetActorTickEnabled(false);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ARaidVisualDirector::Capture(const TCHAR* Name, int32 NextStage)
{
    const FString Folder = FPaths::Combine(FPaths::ProjectDir(), TEXT("Evidence/R6Visual"));
    IFileManager::Get().MakeDirectory(*Folder, true);
    PendingPath = FPaths::Combine(Folder, FString(Name) + TEXT(".png"));
    IFileManager::Get().Delete(*PendingPath, false, true);
    FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
    AfterCapture = NextStage;
    Advance(100);
}

void ARaidVisualDirector::AimAt(const FVector& Point)
{
    const FRotator View = (Point - Player->Camera->GetComponentLocation()).Rotation();
    if (Player->GetController()) Player->GetController()->SetControlRotation(View);
    Player->Camera->SetWorldRotation(View);
}

bool ARaidVisualDirector::ExecuteAction(FName ActionName)
{
    const ARaidPlayerController* Controller = Player
        ? Cast<ARaidPlayerController>(Player->GetController()) : nullptr;
    UInputComponent* Inputs[] = {
        Controller ? Controller->InputComponent.Get() : nullptr,
        Player ? Player->InputComponent.Get() : nullptr
    };
    for (UInputComponent* Input : Inputs)
    {
        if (!Input) continue;
        for (int32 Index = 0; Index < Input->GetNumActionBindings(); ++Index)
        {
            FInputActionBinding& Binding = Input->GetActionBinding(Index);
            if (Binding.GetActionName() == ActionName && Binding.KeyEvent == IE_Pressed
                && Binding.ActionDelegate.IsBound())
            {
                Binding.ActionDelegate.Execute(FKey());
                return true;
            }
        }
    }
    return false;
}

void ARaidVisualDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    IgnoreExternalViewportInput();
    ON_SCOPE_EXIT
    {
        // Opening/closing a panel applies a new input mode inside this tick.
        IgnoreExternalViewportInput();
    };
    // Repeat before every possible wait: map actor initialization may follow
    // this director's BeginPlay, and possession/shader readiness can be delayed.
    SuppressBackgroundEnemies();
    Elapsed += DeltaSeconds;
    StageSeconds += DeltaSeconds;
    if (Elapsed > 120.f)
    {
        UE_LOG(LogTemp, Error, TEXT("R6_VISUAL_REVIEW: timeout; captures=%d"), Captures);
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    if (!Player)
    {
        Player = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerCharacter(this, 0));
        if (!Player) return;
        UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_REVIEW: real rendering; hardware input NOT tested; background AI frozen, review guard remains live"));
        if (FParse::Param(FCommandLine::Get(), TEXT("RaidScenePreview")))
        {
            TArray<UMeshComponent*> PlayerMeshes;
            Player->GetComponents<UMeshComponent>(PlayerMeshes);
            for (UMeshComponent* MeshComponent : PlayerMeshes) MeshComponent->SetVisibility(false);
            UE_LOG(LogTemp, Display, TEXT("R6_SCENE_PREVIEW: player weapon and arms hidden for environment-only review"));
        }
    }
    const ARaidGameMode* ActiveMode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Player->IsAlive() || (ActiveMode && ActiveMode->State == ERaidState::Dead))
    {
        UE_LOG(LogTemp, Error, TEXT("R6_VISUAL_INVALID_STATE: stage=%d elapsed=%.2f HP=%.1f state=%d; dead raid may mean death or abandonment"),
            Stage, Elapsed, Player->Health, ActiveMode ? static_cast<int32>(ActiveMode->State) : -1);
        Fail(TEXT("raid ended before visual review completed; refusing invalid captures"));
        return;
    }
    if (Stage == 0 && (!ActiveMode || !ActiveMode->IsRaidActive()))
    {
        UE_LOG(LogTemp, Error, TEXT("R6_VISUAL_INVALID_STATE: stage=%d elapsed=%.2f HP=%.1f state=%d"),
            Stage, Elapsed, Player->Health, ActiveMode ? static_cast<int32>(ActiveMode->State) : -1);
        Fail(TEXT("visual review setup requires a live running raid"));
        return;
    }
#if WITH_EDITOR
    // Suppression and validity checks above must also run while shaders warm up.
    if (Stage != 100 && GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
    if (bHandsReview) { TickHandsReview(DeltaSeconds); return; }
    switch (Stage)
    {
    case 0:
#if WITH_EDITOR
        // Fresh imported materials must finish real shader compilation before
        // their visual review; a fixed delay alone can capture checker fallbacks.
        if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) break;
#endif
        if (StageSeconds > 12.f)
        {
            UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_ASSETS: weapon=%s arms=%s animation=%s"),
                *GetNameSafe(Player->WeaponModel), *GetNameSafe(Player->ArmsModel), *GetNameSafe(Player->ArmsIdleAnimation));
            TArray<USkeletalMeshComponent*> SkeletalComponents;
            Player->GetComponents(SkeletalComponents);
            for (USkeletalMeshComponent* ArmComponent : SkeletalComponents)
                if (ArmComponent->GetName() == TEXT("OptionalFirstPersonArms"))
                    for (const FName Bone : {FName(TEXT("hand_R")), FName(TEXT("hand_L"))})
                        UE_LOG(LogTemp, Display, TEXT("R6_HAND_POSE: %s root-relative %s"), *Bone.ToString(),
                            *ArmComponent->GetAttachParent()->GetComponentTransform().InverseTransformPosition(
                                ArmComponent->GetBoneLocation(Bone)).ToString());
            Capture(TEXT("01_depot_hip"), 1);
        }
        break;
    case 1:
        if (!bIssued) { Player->StartAim(); bIssued = true; }
        if (StageSeconds > .8f)
        {
            UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_ADS: alpha=%.3f FOV=%.1f"), Player->ADSAlpha, Player->Camera->FieldOfView);
            Capture(TEXT("02_depot_ads"), 2);
        }
        break;
    case 2:
        if (StageSeconds > .5f)
        {
            const int32 Before = Player->MagazineAmmo;
            Player->Fire();
            UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_FIRE: rounds=%d -> %d sound=%s"), Before, Player->MagazineAmmo, *GetNameSafe(Player->FireSound));
            Capture(TEXT("03_rifle_fire"), 3);
        }
        break;
    case 3:
        if (!bIssued) { Player->StopAim(); Player->Reload(); bIssued = true; }
        if (StageSeconds > .6f)
        {
            UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_RELOAD: active=%d progress=%.2f"), Player->bReloading, Player->ReloadProgress);
            Capture(TEXT("04_rifle_reload"), 4);
        }
        break;
    case 4:
        if (StageSeconds > 1.6f)
        {
            FActorSpawnParameters Parameters;
            Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ReviewEnemy = GetWorld()->SpawnActor<ARaidEnemy>(ARaidEnemy::StaticClass(),
                Player->GetActorLocation() + FVector(600.f, 50.f, 0.f), FRotator(0, 180, 0), Parameters);
            if (ReviewEnemy)
            {
                ReviewEnemy->DetectionRadius = 0.f;
                ReviewEnemy->AttackRange = 0.f;
                ReviewEnemy->GetCharacterMovement()->DisableMovement();
                AimAt(ReviewEnemy->GetActorLocation() + FVector(0, 0, 35));
            }
            Advance(5);
        }
        break;
    case 5:
        if (StageSeconds > 1.2f) Capture(TEXT("05_guard_midrange"), 50);
        break;
    case 50:
        if (!bIssued) { Player->Fire(); bIssued = true; }
        if (StageSeconds > .12f) Capture(TEXT("07_guard_hit_reaction"), 51);
        break;
    case 51:
        if (!bIssued)
        {
            ReviewEnemy->DetectionRadius = 1600.f;
            ReviewEnemy->AttackRange = 850.f;
            bIssued = true;
        }
        if (Player->Health < 100.f)
        {
            UE_LOG(LogTemp,Display,TEXT("R6_VISUAL_DAMAGE: live enemy hit HP=%.1f bearing=%.1f"),Player->Health,Player->DamageSourceYaw);
            Capture(TEXT("08_damage_direction"),52);
        }
        else if (StageSeconds > 6.f)
        {
            UE_LOG(LogTemp,Error,TEXT("R6_VISUAL_DAMAGE: no live enemy damage before deadline"));
            FPlatformMisc::RequestExitWithStatus(false,1);
        }
        break;
    case 52:
    {
        if (ReviewEnemy) ReviewEnemy->Destroy();
        FVector Location = Player->GetActorLocation() + Player->GetActorForwardVector() * 165.f;
        Location.Z = 48.f;
        const FTransform LootTransform(FRotator::ZeroRotator, Location);
        ReviewLoot = GetWorld()->SpawnActorDeferred<ARaidLoot>(ARaidLoot::StaticClass(), LootTransform,
            nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!ReviewLoot) { Fail(TEXT("review container could not spawn")); break; }
        ReviewLoot->ItemId = TEXT("Tech");
        ReviewLoot->DisplayName = TEXT("加密电台");
        ReviewLoot->Value = 180;
        // Keep normal supplemental contents and make the in-progress capture readable.
        ReviewLoot->SearchDuration = 2.f;
        UGameplayStatics::FinishSpawningActor(ReviewLoot, LootTransform);
        ReviewLoot->EnsureContents();
        if (ReviewLoot->GetContents().Num() < 2 || ReviewLoot->GetContents().Num() > 3)
        {
            Fail(TEXT("normal review container did not generate two or three rows"));
            break;
        }
        AimAt(ReviewLoot->GetActorLocation() + FVector(0,0,5));
        Advance(53);
        break;
    }
    case 53:
        if (!bIssued && StageSeconds > .15f)
        {
            if (Player->GetFocusedLoot() != ReviewLoot || !Player->CanAccessLoot(ReviewLoot))
            {
                Fail(TEXT("review container was not focused and accessible"));
                break;
            }
            LootCountBeforeSearch = Player->GetLootCount();
            if (!ExecuteAction(TEXT("Interact")))
            {
                Fail(TEXT("interact action was not bound"));
                break;
            }
            ReviewLootIndex = ReviewLoot->FindNextUnsearchedIndex();
            if (Player->GetOpenLoot() != ReviewLoot || ReviewLootIndex == INDEX_NONE
                || Player->IsSearching() || Player->GetLootCount() != LootCountBeforeSearch)
            {
                Fail(TEXT("Interact did not open an unknown container without searching or taking"));
                break;
            }
            for (const FRaidInventoryEntry& Entry : ReviewLoot->GetContents())
            {
                if (Entry.bRevealed)
                {
                    Fail(TEXT("opening the container revealed a row"));
                    return;
                }
            }
            ReviewLootQuantityBeforeTake = ReviewLoot->GetContents()[ReviewLootIndex].Quantity;
            UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_LOOT_OPEN: rows=%d next=%d revision=%lld inventory=%d"),
                ReviewLoot->GetContents().Num(), ReviewLootIndex,
                static_cast<long long>(ReviewLoot->GetRevision()), Player->GetLootCount());
            bIssued = true;
        }
        if (bIssued && StageSeconds > .4f) Capture(TEXT("09_container_unknown"),54);
        break;
    case 54:
        if (!bIssued)
        {
            Player->BeginSearch();
            if (!Player->IsSearching() || Player->GetSearchIndex() != ReviewLootIndex)
            {
                Fail(TEXT("search did not start on the first unknown row of the open container"));
                break;
            }
            bIssued = true;
        }
        if (Player->GetLootCount() != LootCountBeforeSearch)
        {
            Fail(TEXT("search transferred loot before a manual take"));
            break;
        }
        if (Player->GetSearchProgress() > .35f)
        {
            UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_LOOT_SEARCH: index=%d progress=%.2f inventory=%d"),
                Player->GetSearchIndex(), Player->GetSearchProgress(), Player->GetLootCount());
            Capture(TEXT("10_container_searching"),55);
        }
        else if (StageSeconds > 3.f)
        {
            Fail(TEXT("open-container search did not reach the capture point"));
        }
        break;
    case 55:
        if (!IsValid(ReviewLoot) || Player->GetOpenLoot() != ReviewLoot
            || !ReviewLoot->GetContents().IsValidIndex(ReviewLootIndex))
        {
            Fail(TEXT("container closed or disappeared while searching"));
            break;
        }
        if (ReviewLoot->GetContents()[ReviewLootIndex].bRevealed)
        {
            // Searches advance row by row. Pause after the first reveal so the
            // captured result remains stable until the separate manual take.
            Player->CancelSearch();
            if (Player->GetLootCount() != LootCountBeforeSearch
                || ReviewLoot->GetContents()[ReviewLootIndex].Quantity != ReviewLootQuantityBeforeTake)
            {
                Fail(TEXT("revealing a row automatically collected an item"));
                break;
            }
            UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_LOOT_REVEALED: index=%d quantity=%d inventory=%d"),
                ReviewLootIndex, ReviewLootQuantityBeforeTake, Player->GetLootCount());
            Capture(TEXT("11_container_revealed"),56);
        }
        else if (StageSeconds > ReviewLoot->SearchDuration + 2.f)
            Fail(TEXT("search did not reveal its row before the deadline"));
        break;
    case 56:
        if (!bIssued)
        {
            if (!Player->TakeLootItem(ReviewLootIndex) || !IsValid(ReviewLoot)
                || Player->GetOpenLoot() != ReviewLoot
                || !ReviewLoot->GetContents().IsValidIndex(ReviewLootIndex)
                || Player->GetLootCount() != LootCountBeforeSearch + 1
                || ReviewLoot->GetContents()[ReviewLootIndex].Quantity != ReviewLootQuantityBeforeTake - 1)
            {
                Fail(TEXT("manual take did not move exactly one item and retain the container row"));
                break;
            }
            UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_LOOT_TAKEN: index=%d remaining=%d inventory=%d revision=%lld"),
                ReviewLootIndex, ReviewLoot->GetContents()[ReviewLootIndex].Quantity,
                Player->GetLootCount(), static_cast<long long>(ReviewLoot->GetRevision()));
            bIssued = true;
        }
        if (StageSeconds > .3f) Capture(TEXT("12_container_taken"),57);
        break;
    case 57:
        if (!bIssued)
        {
            Player->CloseLoot();
            if (Player->GetOpenLoot() || Player->IsSearching() || Player->IsInventoryOpen())
            {
                Fail(TEXT("container did not close before the backpack review"));
                break;
            }
            // Dispatch the real Tab action so the standalone backpack also sets
            // its cursor/input mode. No OS or hardware input is injected here.
            if (!ExecuteAction(TEXT("Inventory")) || !Player->IsInventoryOpen() || Player->GetOpenLoot())
            {
                Fail(TEXT("inventory action did not open the standalone backpack"));
                break;
            }
            bIssued = true;
        }
        if (StageSeconds > .3f) Capture(TEXT("13_backpack"),6);
        break;
    case 6:
        if (!bIssued)
        {
            Player->CloseLoot();
            Player->GetCharacterMovement()->StopMovementImmediately();
            Player->SetActorLocation(FVector(1450, 930, 96), false, nullptr, ETeleportType::TeleportPhysics);
            AimAt(FVector(2010, 1400, 165));
            bIssued = true;
        }
        if (StageSeconds > 1.5f) Capture(TEXT("06_extraction_landmark"), 60);
        break;
    case 60:
        if (!bIssued)
        {
            Player->SetActorLocation(FVector(2010,1400,96),false,nullptr,ETeleportType::TeleportPhysics);
            AimAt(FVector(2340,1400,190));
            bIssued = true;
        }
        if (StageSeconds > 2.5f) Capture(TEXT("14_extraction_countdown"),61);
        break;
    case 61:
        if (const ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this)))
            if (Mode->State == ERaidState::Extracted) Capture(TEXT("15_extracted_result"),7);
        break;
    case 7:
        UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_REVIEW_COMPLETE: screenshots=%d; experience review remains required"), Captures);
        FPlatformMisc::RequestExitWithStatus(false, 0);
        SetActorTickEnabled(false);
        break;
    case 100:
        if (StageSeconds > .2f && IFileManager::Get().FileSize(*PendingPath) > 0)
        {
            ++Captures;
            UE_LOG(LogTemp, Display, TEXT("R6_VISUAL_CAPTURE: %s"), *PendingPath);
            Advance(AfterCapture);
        }
        else if (StageSeconds > 10.f)
        {
            UE_LOG(LogTemp, Error, TEXT("R6_VISUAL_CAPTURE_FAILED: %s"), *PendingPath);
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        break;
    }
}

void ARaidVisualDirector::HandsInput(FName Name, bool bPressed)
{
    if (!Player || !Player->InputComponent) return;
    for (int32 I = 0; I < Player->InputComponent->GetNumActionBindings(); ++I)
    {
        FInputActionBinding& B = Player->InputComponent->GetActionBinding(I);
        if (B.GetActionName() == Name && B.KeyEvent == (bPressed ? IE_Pressed : IE_Released))
            B.ActionDelegate.Execute(FKey());
    }
}

void ARaidVisualDirector::TickHandsReview(float DeltaSeconds)
{
    struct FSample { const TCHAR* Name; int32 Kind; float At; };
    // Each reload key is a fresh, naturally timed reload from the same starting view.
    // Kind: hip, ADS, walking, sprinting, reload, completed, interrupted, recovered, fire.
    static const FSample Samples[] = {
        {TEXT("01_hip"),0,.5f},{TEXT("02_ads"),1,.6f},{TEXT("03_walk"),2,.6f},
        {TEXT("04_sprint"),3,.6f},{TEXT("05_reload_10"),4,.10f},{TEXT("06_reload_20"),4,.20f},
        {TEXT("07_reload_36"),4,.36f},{TEXT("08_reload_54"),4,.54f},{TEXT("09_reload_67"),4,.67f},
        {TEXT("10_reload_79"),4,.79f},{TEXT("11_reload_92"),4,.92f},{TEXT("12_reload_complete"),5,1.8f},
        {TEXT("13_interrupt_inventory"),6,.36f},{TEXT("14_interrupt_recovered"),7,1.f},
        {TEXT("15_ads_return"),1,.6f},{TEXT("16_fire"),8,.05f},
        {TEXT("17_cancel_early"),9,.04f},{TEXT("18_cancel_middle"),9,.12f},{TEXT("19_cancel_late"),9,.23f}
    };
    HandsSeconds += DeltaSeconds;
    if (HandsPhase == 0)
    {
        if (Elapsed < 12.f) return;
        if (ReviewOutput.IsEmpty()) { Fail(TEXT("hands review needs a distinct output directory")); return; }
        IFileManager::Get().MakeDirectory(*ReviewOutput, true);
        HandsOrigin = Player->GetActorLocation();
        HandsRows.Add(TEXT("sample,reload_progress,ads_alpha,hand_x,hand_y,hand_z,elbow_x,elbow_y,elbow_z,mag_x,mag_y,mag_z,visual_progress,visual_weight,magazine_ammo,reserve_ammo"));
        HandsPhase = 1;
    }
    if (HandsIndex >= UE_ARRAY_COUNT(Samples))
    {
        UE_LOG(LogTemp, Display, TEXT("R7_HANDS_REVIEW_COMPLETE: screenshots=%d; visual fixtures only; AI suppressed; not gameplay proof"), Captures);
        FPlatformMisc::RequestExitWithStatus(false, 0);
        SetActorTickEnabled(false);
        return;
    }
    const FSample& Sample = Samples[HandsIndex];
    if (HandsPhase == 1)
    {
        Player->StopFire(); Player->StopAim(); Player->CancelReload(); HandsInput(TEXT("Sprint"), false);
        if (Player->IsInventoryOpen()) ExecuteAction(TEXT("Inventory"));
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->SetActorLocation(HandsOrigin, false, nullptr, ETeleportType::TeleportPhysics);
        Player->GetController()->SetControlRotation(FRotator(0,30,0));
        Player->Camera->SetWorldRotation(FRotator(0,30,0));
        Player->MagazineAmmo = Player->MaxMagazineAmmo; Player->ReserveAmmo = 60;
        HandsCancelledAt = -1.f;
        HandsSeconds = 0.f; HandsPhase = 2;
        return;
    }
    if (HandsPhase == 2)
    {
        // Let the ordinary presentation recover before starting the next sample.
        if (HandsSeconds < .65f) return;
        if (Sample.Kind == 1) Player->StartAim();
        if (Sample.Kind == 3) HandsInput(TEXT("Sprint"), true);
        if (Sample.Kind >= 4)
        {
            Player->Fire(); Player->StopFire();
            if (Sample.Kind != 8) Player->Reload();
        }
        HandsSeconds = 0.f; HandsPhase = 3;
        return;
    }
    if (HandsPhase == 3)
    {
        if (Sample.Kind == 2 || Sample.Kind == 3)
            Player->AddMovementInput(Player->Camera->GetForwardVector(), 1.f);
        Player->GetController()->SetControlRotation(FRotator(0,30,0));
        Player->Camera->SetWorldRotation(FRotator(0,30,0));
        if ((Sample.Kind == 6 || Sample.Kind == 7 || Sample.Kind == 9) && Player->bReloading && Player->ReloadProgress >= .36f)
        {
            ExecuteAction(TEXT("Inventory"));
            if (Sample.Kind == 7 || Sample.Kind == 9) ExecuteAction(TEXT("Inventory"));
            HandsCancelledAt = HandsSeconds;
        }
        const bool bReady = Sample.Kind == 4 ? Player->ReloadProgress >= Sample.At
            : Sample.Kind == 6 ? Player->IsInventoryOpen()
            : Sample.Kind == 9 ? HandsCancelledAt >= 0.f && HandsSeconds - HandsCancelledAt >= Sample.At
            : HandsSeconds >= Sample.At;
        if (!bReady) return;
        if (Sample.Kind == 4 && !Player->bReloading) { Fail(TEXT("reload key was missed")); return; }
        TArray<USceneComponent*> Components;
        Player->GetComponents(Components);
        USceneComponent* Root = nullptr; USceneComponent* Magazine = nullptr; USkeletalMeshComponent* Arms = nullptr;
        for (USceneComponent* C : Components)
        {
            if (C->GetFName() == TEXT("WeaponPose")) Root = C;
            if (C->GetFName() == TEXT("OptionalMagazineModel")) Magazine = C;
            if (C->GetFName() == TEXT("OptionalFirstPersonArms")) Arms = Cast<USkeletalMeshComponent>(C);
        }
        if (!Root || !Arms) { Fail(TEXT("hands review actual skeletal components missing")); return; }
        const FVector Hand = Root->GetComponentTransform().InverseTransformPosition(Arms->GetSocketLocation(TEXT("hand_L")));
        const FVector Elbow = Root->GetComponentTransform().InverseTransformPosition(Arms->GetSocketLocation(TEXT("forearm_L")));
        const FVector Mag = Magazine ? Magazine->GetRelativeLocation() : FVector::ZeroVector;
        HandsRows.Add(FString::Printf(TEXT("%s,%.5f,%.5f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.5f,%.5f,%d,%d"),
            Sample.Name, Player->ReloadProgress, Player->ADSAlpha, Hand.X,Hand.Y,Hand.Z,Elbow.X,Elbow.Y,Elbow.Z,Mag.X,Mag.Y,Mag.Z,
            Player->GetVisualReloadProgress(),Player->GetVisualReloadWeight(),Player->MagazineAmmo,Player->ReserveAmmo));
        FFileHelper::SaveStringArrayToFile(HandsRows, *FPaths::Combine(ReviewOutput,TEXT("HandSamples.csv")));
        PendingPath = FPaths::Combine(ReviewOutput, FString(Sample.Name) + TEXT(".png"));
        if (IFileManager::Get().FileExists(*PendingPath)) { Fail(TEXT("refusing to overwrite hands evidence")); return; }
        UE_LOG(LogTemp, Display, TEXT("R7_HANDS_SAMPLE: %s reload=%.4f ADS=%.4f hand=%s elbow=%s"),
            Sample.Name, Player->ReloadProgress, Player->ADSAlpha, *Hand.ToCompactString(), *Elbow.ToCompactString());
        FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
        HandsSeconds = 0.f; HandsPhase = 4;
        return;
    }
    if (HandsPhase == 4 && HandsSeconds > .3f && IFileManager::Get().FileSize(*PendingPath) > 128)
    { ++Captures; ++HandsIndex; HandsPhase = 1; }
}
