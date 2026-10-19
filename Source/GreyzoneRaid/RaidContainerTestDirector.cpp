#include "RaidContainerTestDirector.h"
#include "RaidEnemy.h"
#include "RaidExtractionZone.h"
#include "RaidGameMode.h"
#include "RaidHUD.h"
#include "RaidLoot.h"
#include "RaidPlayer.h"
#include "RaidInventoryComponent.h"
#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace
{
    struct FContainerRunMemory
    {
        int32 Phase = 0;
        int32 Checks = 0;
        int32 Failures = 0;
        int32 Captures = 0;
        double Started = 0.0;
        bool Finished = false;
        TWeakObjectPtr<UWorld> PreviousWorld;
        TWeakObjectPtr<ARaidPlayer> PreviousPlayer;
        TWeakObjectPtr<ARaidLoot> PreviousLoot;
    };
    FContainerRunMemory& Run() { static FContainerRunMemory Memory; return Memory; }
    const FVector LanePlayer(-1900.f, -1400.f, 96.f);
    const FVector LaneBox(-1720.f, -1400.f, 48.f);
    FRaidInventorySeed Entry(FName Id, const TCHAR* Label, int32 Quantity, int32 Value, bool Revealed = false)
    {
        FRaidInventorySeed Row;
        Row.Definition.ItemId = Id;
        Row.Definition.DisplayName = Label;
        Row.Definition.UnitValue = Value;
        Row.Definition.UseEffect = Id == FName(TEXT("Medkit")) ? ERaidItemUseEffect::Heal
            : Id == FName(TEXT("Ammo")) ? ERaidItemUseEffect::Ammo : ERaidItemUseEffect::None;
        Row.Definition.UseAmount = Row.Definition.UseEffect == ERaidItemUseEffect::Heal ? 45
            : Row.Definition.UseEffect == ERaidItemUseEffect::Ammo ? 24 : 0;
        Row.Quantity = Quantity;
        Row.bRevealed = Revealed;
        return Row;
    }

    bool GrantContainerFixtureItem(ARaidPlayer* Player, FName ItemId, const FString& Label, int32 Quantity, int32 Value)
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

ARaidContainerTestDirector::ARaidContainerTestDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void ARaidContainerTestDirector::BeginPlay()
{
    Super::BeginPlay();
    if (!FParse::Param(FCommandLine::Get(), TEXT("RaidContainerTest")) || Run().Finished)
    {
        Destroy();
        return;
    }
    if (Run().Started == 0.0) Run().Started = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER: phase=%d; real gameplay, Canvas hitbox dispatch and OpenLevel; hardware input NOT tested."), Run().Phase);
}

void ARaidContainerTestDirector::Advance(EStep Next)
{
    Step = Next;
    StepSeconds = 0.f;
    Issued = false;
}

bool ARaidContainerTestDirector::Check(bool Passed, const TCHAR* Name, const FString& Detail)
{
    ++Run().Checks;
    if (!Passed) ++Run().Failures;
    UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_CHECK:%s %s | phase=%d step=%d %s"),
        Passed ? TEXT("PASS") : TEXT("FAIL"), Name, Run().Phase, static_cast<int32>(Step), *Detail);
    return Passed;
}

void ARaidContainerTestDirector::Abort(const TCHAR* Name, const FString& Detail)
{
    Check(false, Name, Detail);
    Finish();
}

void ARaidContainerTestDirector::Finish()
{
    if (Run().Finished) return;
    if (Run().Failures == 0)
    {
        Check(Run().Phase == 2, TEXT("both_real_restarts_completed"));
        Check(Run().Captures >= 11, TEXT("eleven_required_screenshots_written"));
        Check(Run().Captures >= 13, TEXT("both_normal_AI_combat_screenshots_written"));
    }
    Run().Finished = true;
    UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_RESULT: %s checks=%d failures=%d captures=%d elapsed=%.2fs"),
        Run().Failures == 0 ? TEXT("PASS") : TEXT("FAIL"), Run().Checks, Run().Failures,
        Run().Captures, FPlatformTime::Seconds() - Run().Started);
    FPlatformMisc::RequestExitWithStatus(false, Run().Failures == 0 ? 0 : 1);
}

bool ARaidContainerTestDirector::CheckFreshWorld()
{
    int32 Enemies = 0, Boxes = 0, Rows = 0, Revealed = 0;
    ARaidLoot* First = nullptr;
    for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It) ++Enemies;
    for (TActorIterator<ARaidLoot> It(GetWorld()); It; ++It)
    {
        ++Boxes;
        if (!First) First = *It;
        It->EnsureContents();
        Rows += It->GetContents().Num();
        Revealed += RevealedCount(*It);
    }
    bool Good = Check(Enemies == 3 && Boxes == 9, TEXT("saved_world_has_three_guards_nine_containers"),
        FString::Printf(TEXT("guards=%d containers=%d"), Enemies, Boxes));
    Good &= Check(Rows >= 9 && Revealed == 0, TEXT("every_fresh_saved_container_is_unsearched"),
        FString::Printf(TEXT("rows=%d revealed=%d"), Rows, Revealed));
    Good &= Check(Mode->IsRaidActive() && FMath::IsNearlyEqual(Player->Health, 100.f)
        && Player->MagazineAmmo == 12 && Player->ReserveAmmo == 60,
        TEXT("fresh_running_player_has_health100_ammo12_60"));
    Good &= Check(Player->GetInventoryComponent()->GetEntries().IsEmpty() && Player->GetLootCount() == 0 && Player->GetTotalLootValue() == 0
        && !Player->IsInventoryOpen() && !Player->GetOpenLoot() && !Player->IsSearching(),
        TEXT("fresh_player_inventory_and_interaction_are_empty"));
    Good &= Check(Mode->SecuredItems == 0 && Mode->SecuredValue == 0 && Mode->EnemiesKilled == 0,
        TEXT("fresh_result_counters_are_zero"));
    if (Run().Phase > 0)
    {
        Good &= Check(Run().PreviousWorld != TWeakObjectPtr<UWorld>(GetWorld())
            && Run().PreviousPlayer != TWeakObjectPtr<ARaidPlayer>(Player.Get())
            && Run().PreviousLoot != TWeakObjectPtr<ARaidLoot>(First),
            TEXT("OpenLevel_replaced_world_player_and_container_instances"));
    }
    return Good;
}

void ARaidContainerTestDirector::PlacePlayer(const FVector& Position)
{
    Player->GetCharacterMovement()->StopMovementImmediately();
    Player->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
    Player->GetCapsuleComponent()->UpdateOverlaps();
}

void ARaidContainerTestDirector::AimAt(const FVector& Position)
{
    const FRotator View = (Position - Player->Camera->GetComponentLocation()).Rotation();
    PC->SetControlRotation(View);
    Player->Camera->SetWorldRotation(View);
}

bool ARaidContainerTestDirector::ExecuteAction(FName Name, bool Pressed)
{
    const TArray<UInputComponent*> Inputs = {PC ? PC->InputComponent.Get() : nullptr,
        Player ? Player->InputComponent.Get() : nullptr};
    for (UInputComponent* Input : Inputs)
    {
        if (!Input) continue;
        for (int32 Index = 0; Index < Input->GetNumActionBindings(); ++Index)
        {
            FInputActionBinding& Binding = Input->GetActionBinding(Index);
            if (Binding.GetActionName() == Name && Binding.KeyEvent == (Pressed ? IE_Pressed : IE_Released)
                && Binding.ActionDelegate.IsBound())
            {
                Binding.ActionDelegate.Execute(FKey());
                return true;
            }
        }
    }
    return false;
}

bool ARaidContainerTestDirector::ExecuteAxis(FName Name, float Value)
{
    UInputComponent* Input = Player->InputComponent.Get();
    if (!Input) return false;
    for (FInputAxisBinding& Binding : Input->AxisBindings)
    {
        if (Binding.AxisName == Name && Binding.AxisDelegate.IsBound())
        {
            Binding.AxisDelegate.Execute(Value);
            return true;
        }
    }
    return false;
}

bool ARaidContainerTestDirector::FindButtonPoint(FName Name, FVector2D& Point) const
{
    const FHUDHitBox* Box = HUD->GetHitBoxWithName(Name);
    if (!Box) return false;
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    FVector2D Low(Width, Height), High(-1.f, -1.f);
    for (int32 Y = 4; Y < Height; Y += 8)
        for (int32 X = 4; X < Width; X += 8)
            if (Box->Contains(FVector2D(X, Y)))
            {
                Low.X = FMath::Min(Low.X, static_cast<double>(X));
                Low.Y = FMath::Min(Low.Y, static_cast<double>(Y));
                High.X = FMath::Max(High.X, static_cast<double>(X));
                High.Y = FMath::Max(High.Y, static_cast<double>(Y));
            }
    if (High.X < 0.f) return false;
    Point = (Low + High) * .5;
    const FHUDHitBox* Routed = HUD->GetHitBoxAtCoordinates(Point, true);
    return Routed && Routed->GetName() == Name;
}

bool ARaidContainerTestDirector::ClickButton(FName Name)
{
    FVector2D Point;
    if (!FindButtonPoint(Name, Point))
    {
        Abort(TEXT("required_canvas_hitbox_missing"), Name.ToString());
        return false;
    }
    if (!HUD->UpdateAndDispatchHitBoxClickEvents(Point, IE_Pressed))
    {
        Abort(TEXT("canvas_click_dispatch_failed"), Name.ToString());
        return false;
    }
    Check(true, TEXT("real_canvas_hitbox_click_dispatched"), Name.ToString());
    return true;
}

ARaidLoot* ARaidContainerTestDirector::SpawnFixture(const FVector& Position, bool ThreeRows, bool Revealed,
    TFunction<void(TArray<FRaidInventorySeed>&)> CustomizeSeeds)
{
    const FTransform Transform(FRotator::ZeroRotator, Position);
    ARaidLoot* Loot = GetWorld()->SpawnActorDeferred<ARaidLoot>(ARaidLoot::StaticClass(), Transform,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Loot) return nullptr;
    Loot->bGenerateSupplementalLoot = false;
    Loot->SearchDuration = 2.4f;
    TArray<FRaidInventorySeed> Seeds = {Entry(TEXT("Tech"), TEXT("电子元件"), 1, 120, Revealed)};
    if (ThreeRows)
    {
        Seeds.Add(Entry(TEXT("Medkit"), TEXT("医疗包"), 1, 75));
        // Two units make a duplicate same-frame Use click observable, rather than
        // merely addressing an already-removed inventory row.
        Seeds.Add(Entry(TEXT("Ammo"), TEXT("弹药盒"), 2, 40));
    }
    if (CustomizeSeeds) CustomizeSeeds(Seeds);
    if (!Loot->GetInventoryComponent()->InitializeOnce(Seeds, -1, true).IsSuccess())
    {
        Loot->Destroy();
        return nullptr;
    }
    Loot->FinishSpawning(Transform);
    return Loot;
}

int32 ARaidContainerTestDirector::InventoryIndex(FName ItemId) const
{
    return Player->GetInventoryComponent()->GetEntries().IndexOfByPredicate([ItemId](const FRaidInventoryEntry& Item)
        { return Item.Definition.ItemId == ItemId && Item.Quantity > 0; });
}

int32 ARaidContainerTestDirector::RevealedCount(const ARaidLoot* Loot) const
{
    if (!IsValid(Loot)) return 0;
    int32 Count = 0;
    for (const FRaidInventoryEntry& Row : Loot->GetContents()) if (Row.bRevealed) ++Count;
    return Count;
}

void ARaidContainerTestDirector::Capture(const TCHAR* Name, EStep Next)
{
    const FString Folder = FPaths::Combine(FPaths::ProjectDir(), TEXT("Evidence/R6Container"));
    IFileManager::Get().MakeDirectory(*Folder, true);
    ScreenshotPath = FPaths::Combine(Folder, FString(Name) + TEXT(".png"));
    if (IFileManager::Get().FileExists(*ScreenshotPath)
        && !IFileManager::Get().Delete(*ScreenshotPath, false, true))
    {
        Abort(TEXT("cannot_replace_own_screenshot"), ScreenshotPath);
        return;
    }
    LastScreenshotSize = -1;
    ScreenshotStableSeconds = 0.f;
    FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
    AfterCapture = Next;
    Advance(EStep::Screenshot);
}

void ARaidContainerTestDirector::RestartIntoPhase(int32 Phase)
{
    Run().PreviousWorld = GetWorld();
    Run().PreviousPlayer = Player;
    Run().PreviousLoot = BoxA;
    Run().Phase = Phase;
    Advance(EStep::AwaitTravel);
    Mode->RestartRaid();
}

bool ARaidContainerTestDirector::CheckNormalGuards(int32 ExpectedCount, const TCHAR* Name)
{
    int32 Count = 0;
    bool Normal = true;
    for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It)
    {
        const ARaidEnemy* Guard = *It;
        const ARaidEnemy* Defaults = Guard->GetClass()->GetDefaultObject<ARaidEnemy>();
        ++Count;
        Normal &= Guard->Health > 0.f && Guard->IsActorTickEnabled()
            && Guard->GetCharacterMovement()->IsComponentTickEnabled()
            && Cast<AAIController>(Guard->GetController()) != nullptr
            && FMath::IsNearlyEqual(Guard->DetectionRadius, Defaults->DetectionRadius)
            && FMath::IsNearlyEqual(Guard->AttackRange, Defaults->AttackRange)
            && FMath::IsNearlyEqual(Guard->AttackDamage, Defaults->AttackDamage)
            && FMath::IsNearlyEqual(Guard->AttackInterval, Defaults->AttackInterval)
            && FMath::IsNearlyEqual(Guard->GetActorTickInterval(), Defaults->GetActorTickInterval())
            && Guard->DetectionRadius > 0.f && Guard->AttackRange > 0.f && Guard->AttackDamage > 0.f;
        UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_LIVE_GUARD: name=%s location=%s detection=%.1f attack_range=%.1f damage=%.1f interval=%.2f tick=%d tick_interval=%.3f movement_tick=%d controller=%s state=%s"),
            *Guard->GetName(), *Guard->GetActorLocation().ToString(), Guard->DetectionRadius,
            Guard->AttackRange, Guard->AttackDamage, Guard->AttackInterval, Guard->IsActorTickEnabled(),
            Guard->GetActorTickInterval(), Guard->GetCharacterMovement()->IsComponentTickEnabled(),
            *GetNameSafe(Guard->GetController()), *Guard->StateText);
    }
    return Check(Normal && Count == ExpectedCount, Name,
        FString::Printf(TEXT("count=%d expected=%d all_class_default_combat_and_running_ticks=%d"), Count, ExpectedCount, Normal));
}

void ARaidContainerTestDirector::ObserveLiveDamage(AActor* DamagedActor, float Damage,
    const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
    if (Run().Phase != 2 || DamagedActor != Player || Damage <= 0.f) return;
    // The production player broadcasts Super::TakeDamage before applying its
    // health/UI transition, so this observer can prove search was active at hit.
    ARaidEnemy* Source = Cast<ARaidEnemy>(DamageCauser);
    ++LiveDamageEvents;
    LiveDamageSource = Source;
    LiveHealthBefore = Player->Health;
    LiveDamageAmount = Damage;
    LiveDamageAt = GetWorld()->GetTimeSeconds();
    LiveDamageYaw = Source ? (Source->GetActorLocation() - Player->GetActorLocation()).Rotation().Yaw : 0.f;
    bLiveHitWhileSearching = Player->IsInventoryOpen() && Player->GetOpenLoot() == BoxA && Player->IsSearching();
    bLiveDamageFromAI = Source && Cast<AAIController>(InstigatedBy)
        && Source->GetController() == InstigatedBy;
    UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_LIVE_HIT: event=%d source=%s controller=%s normal_AI=%d source_state=%s direction_yaw=%.2f health_before=%.1f applied_damage=%.1f health_expected=%.1f panel_open=%d searching=%d raid_state=%d"),
        LiveDamageEvents, *GetNameSafe(Source), *GetNameSafe(InstigatedBy), bLiveDamageFromAI,
        Source ? *Source->StateText : TEXT("none"), LiveDamageYaw, LiveHealthBefore, Damage,
        FMath::Max(0.f, LiveHealthBefore - Damage), Player->IsInventoryOpen(), Player->IsSearching(),
        static_cast<int32>(Mode->State));
}

void ARaidContainerTestDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Run().Finished) return;
    if (FPlatformTime::Seconds() - Run().Started > 180.0)
    {
        Abort(TEXT("container_suite_wall_clock_timeout_180s"));
        return;
    }
#if WITH_EDITOR
    if (Step != EStep::Screenshot && GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
    StepSeconds += DeltaSeconds;
    if (Step != EStep::Ready && (!IsValid(Player) || !IsValid(Mode) || !IsValid(HUD) || !IsValid(PC)))
    {
        Abort(TEXT("world_references_invalid"));
        return;
    }
    // Canvas action snapshots must have at least one fresh rendered frame after
    // each prior mutation. Deliberate stale-click probes below run within one tick.
    if (StepSeconds < .2f) return;
    switch (Step)
    {
    case EStep::Ready:
        Player = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerCharacter(this, 0));
        PC = UGameplayStatics::GetPlayerController(this, 0);
        Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
        HUD = PC ? Cast<ARaidHUD>(PC->GetHUD()) : nullptr;
        if (!Player || !PC || !Mode || !HUD || !Player->Camera)
        {
            if (StepSeconds > 10.f) Abort(TEXT("player_hud_not_ready"));
            break;
        }
        if (!Issued)
        {
            if (Run().Phase < 2)
            {
                for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It)
                {
                    It->DetectionRadius = 0.f;
                    It->AttackRange = 0.f;
                }
                UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_FIXTURE: scope=isolated_container_edges phase=%d; saved guards detection/attack suppressed; player teleports and explicit runtime cases are test fixtures; saved map is unchanged."), Run().Phase);
            }
            else UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_SCOPE: normal_AI_looting_risk; all three saved guards retain default detection/attack/ticks; only explicit lane/container/low-health fixtures are added; no injected damage in this phase."));
            Issued = true;
        }
        if (StepSeconds < .6f) break;
        if (!CheckFreshWorld()) { Finish(); break; }
        for (TActorIterator<ARaidExtractionZone> It(GetWorld()); It; ++It) { Zone = *It; break; }
        if (!Zone || !Zone->Trigger) { Abort(TEXT("saved_extraction_zone_missing")); break; }
        if (Run().Phase == 2) { Advance(EStep::LiveSetup); break; }
        PlacePlayer(LanePlayer);
        BoxA = SpawnFixture(LaneBox, true);
        if (!BoxA) { Abort(TEXT("container_A_spawn_failed")); break; }
        AimAt(BoxA->GetActorLocation());
        if (Run().Phase == 1) { Advance(EStep::DeathSetup); break; }
        BoxB = SpawnFixture(FVector(-1740.f, -1300.f, 48.f), false, true,
            [](TArray<FRaidInventorySeed>& Seeds) { Seeds[0] = Entry(TEXT("Rations"), TEXT("应急口粮"), 1, 40, true); });
        if (!BoxB) { Abort(TEXT("container_B_spawn_failed")); break; }
        Advance(EStep::OpenA);
        break;

    case EStep::OpenA:
        Check(Player->GetFocusedLoot() == BoxA, TEXT("world_trace_focuses_container_A"));
        if (!ExecuteAction(TEXT("Interact")) || Player->GetOpenLoot() != BoxA)
        { Abort(TEXT("bound_E_did_not_open_container_A")); break; }
        Check(Player->IsInventoryOpen() && Player->IsSearching() && Player->GetLootCount() == 0
            && BoxA->GetContents().Num() == 3 && RevealedCount(BoxA) == 0,
            TEXT("E_opens_three_unknown_rows_without_collecting"));
        Advance(EStep::Unknown);
        break;
    case EStep::Unknown:
        Capture(TEXT("01_unknown"), EStep::StartPartial);
        break;
    case EStep::StartPartial:
        if (!Check(Player->IsSearching(), TEXT("opening_container_automatically_searches_without_extra_click"))) { Finish(); break; }
        Check(Player->IsSearching() && Player->GetSearchIndex() == 0, TEXT("automatic_search_starts_first_unknown_row"));
        Advance(EStep::Partial);
        break;
    case EStep::Partial:
        if (StepSeconds < .35f) break;
        Check(Player->IsSearching() && Player->GetSearchProgress() > .05f
            && Player->GetSearchProgress() < .8f && RevealedCount(BoxA) == 0,
            TEXT("search_has_real_partial_progress_without_reveal"));
        Capture(TEXT("02_search_partial"), EStep::CancelPartial);
        break;
    case EStep::CancelPartial:
        Check(ExecuteAction(TEXT("Quit")) && !Player->IsInventoryOpen() && !Player->GetOpenLoot()
            && !Player->IsSearching() && Mode->IsRaidActive(),
            TEXT("bound_ESC_closes_container_and_cancels_without_abandoning"));
        Advance(EStep::CancelWait);
        break;
    case EStep::CancelWait:
        if (StepSeconds < BoxA->SearchDuration + .3f) break;
        Check(RevealedCount(BoxA) == 0 && Player->GetLootCount() == 0 && !Player->IsSearching()
            && FMath::IsNearlyZero(Player->GetSearchProgress()), TEXT("cancelled_search_never_completes_after_original_duration"));
        Advance(EStep::ReopenA);
        break;
    case EStep::ReopenA:
        AimAt(BoxA->GetActorLocation());
        if (!ExecuteAction(TEXT("Interact")) || Player->GetOpenLoot() != BoxA)
        { Abort(TEXT("reopen_A_failed")); break; }
        Advance(EStep::StartFull);
        break;
    case EStep::StartFull:
        if (!Check(Player->IsSearching(), TEXT("opening_container_automatically_searches_without_extra_click"))) { Finish(); break; }
        Advance(EStep::FirstReveal);
        break;
    case EStep::FirstReveal:
        if (BoxA->GetContents()[0].bRevealed)
        {
            Check(RevealedCount(BoxA) == 1 && Player->GetLootCount() == 0 && Player->GetTotalLootValue() == 0
                && Player->IsSearching() && Player->GetSearchIndex() == 1,
                TEXT("first_reveal_does_not_auto_collect_and_next_row_searches"));
            Capture(TEXT("03_first_revealed"), EStep::TakeFirst);
        }
        else if (StepSeconds > 6.f) Abort(TEXT("first_row_search_timeout"));
        break;
    case EStep::TakeFirst:
        Check(Player->IsSearching() && Player->GetSearchIndex() > 0, TEXT("manual_take_occurs_while_other_row_searches"));
        if (!ClickButton(TEXT("Take_0"))) break;
        Check(Player->GetLootCount() == 1 && Player->GetTotalLootValue() == 120
            && BoxA->GetContents()[0].Quantity == 0 && Player->IsSearching(),
            TEXT("manual_take_moves_one_item_without_interrupting_other_search"));
        Capture(TEXT("04_chosen_first_item"), EStep::RevealAll);
        break;
    case EStep::RevealAll:
        if (!Player->IsSearching() && RevealedCount(BoxA) == 3)
        {
            Check(Player->GetLootCount() == 1 && BoxA->GetContents()[1].Quantity == 1
                && BoxA->GetContents()[2].Quantity == 2, TEXT("remaining_reveals_leave_items_in_source"));
            BeforeSourceRevision = BoxA->GetRevision();
            Player->CloseLoot();
            Advance(EStep::Revisit);
        }
        else if (StepSeconds > 9.f) Abort(TEXT("remaining_rows_search_timeout"));
        break;
    case EStep::Revisit:
        if (!Player->OpenLoot(BoxA)) { Abort(TEXT("revealed_container_revisit_failed")); break; }
        Check(RevealedCount(BoxA) == 3 && BoxA->GetRevision() == BeforeSourceRevision
            && BoxA->GetContents()[0].Quantity == 0 && Player->GetLootCount() == 1,
            TEXT("close_and_reopen_preserves_reveals_quantities_and_revision"));
        Advance(EStep::FillBackpack);
        break;
    case EStep::FillBackpack:
        Check(GrantContainerFixtureItem(Player, TEXT("ContainerFill"), TEXT("备用物资"), Player->MaxLootItems - Player->GetLootCount(), 5),
            TEXT("explicit_fixture_fills_backpack_to_eight"));
        Player->CloseLoot();
        Check(Player->OpenLoot(BoxA) && Player->GetLootCount() == 8, TEXT("full_backpack_can_still_open_container"));
        BeforeSourceRevision = BoxA->GetRevision();
        BeforePlayerRevision = Player->GetInteractionRevision();
        BeforeValue = Player->GetTotalLootValue();
        Check(!Player->TakeLootItem(1) && BoxA->GetContents()[1].Quantity == 1
            && BoxA->GetRevision() == BeforeSourceRevision && Player->GetInteractionRevision() == BeforePlayerRevision
            && Player->GetLootCount() == 8 && Player->GetTotalLootValue() == BeforeValue,
            TEXT("capacity_failure_preserves_both_sides_and_revisions"));
        Player->CloseLoot();
        LateBox = SpawnFixture(FVector(-1810.f, -1570.f, 48.f), false);
        if (!LateBox) { Abort(TEXT("full_backpack_search_fixture_spawn_failed")); break; }
        LateBox->SearchDuration = 1.f;
        if (!Player->OpenLoot(LateBox)) { Abort(TEXT("full_backpack_unknown_case_open_failed")); break; }
        Advance(EStep::FullView);
        break;
    case EStep::FullView:
        if (!Issued)
        {
            if (!Check(Player->IsSearching(), TEXT("opening_container_automatically_searches_without_extra_click"))) { Finish(); break; }
            Check(Player->IsSearching() && Player->GetLootCount() == 8, TEXT("full_backpack_can_start_searching_unknown_source"));
            Issued = true;
        }
        if (RevealedCount(LateBox) == 1)
        {
            Check(Player->GetLootCount() == 8 && LateBox->GetContents()[0].Quantity == 1 && !Player->IsSearching(),
                TEXT("full_backpack_search_reveals_without_taking_item"));
            Capture(TEXT("05_full_backpack"), EStep::StoreForMedkit);
        }
        else if (StepSeconds > 4.f) Abort(TEXT("full_backpack_search_timeout"));
        break;
    case EStep::StoreForMedkit:
    {
        if (Player->GetOpenLoot() != BoxA)
        {
            Player->CloseLoot();
            LateBox->Destroy(); // Clean up only this temporary full-capacity fixture.
            LateBox = nullptr;
            if (!Player->OpenLoot(BoxA)) { Abort(TEXT("return_to_A_after_capacity_probe_failed")); break; }
            Advance(EStep::StoreForMedkit);
            break;
        }
        const int32 Index = InventoryIndex(TEXT("ContainerFill"));
        if (Index == INDEX_NONE) { Abort(TEXT("fill_stack_missing")); break; }
        if (!ClickButton(FName(*FString::Printf(TEXT("Store_%d"), Index)))) break;
        Check(Player->GetLootCount() == 7 && Player->GetTotalLootValue() == BeforeValue - 5,
            TEXT("store_button_returns_exactly_one_item_and_frees_one_slot"));
        Advance(EStep::TakeMedkit);
        break;
    }
    case EStep::TakeMedkit:
        if (!ClickButton(TEXT("Take_1"))) break;
        Check(Player->GetLootCount() == 8 && InventoryIndex(TEXT("Medkit")) != INDEX_NONE
            && BoxA->GetContents()[1].Quantity == 0 && Player->GetTotalLootValue() == BeforeValue - 5 + 75,
            TEXT("player_can_choose_medkit_after_storing_lower_value_item"));
        Capture(TEXT("06_inventory_choice"), EStep::PrepareStale);
        break;
    case EStep::PrepareStale:
    {
        // Restore a live Take_0 button in A; leave real capacity for B's item.
        const int32 Tech = InventoryIndex(TEXT("Tech"));
        if (Tech != INDEX_NONE)
        {
            if (!ClickButton(FName(*FString::Printf(TEXT("Store_%d"), Tech)))) break;
            Advance(EStep::PrepareStale);
        }
        else if (Player->GetLootCount() > 5)
        {
            const int32 Fill = InventoryIndex(TEXT("ContainerFill"));
            if (Fill == INDEX_NONE) { Abort(TEXT("cannot_free_capacity_for_stale_probe")); break; }
            if (!ClickButton(FName(*FString::Printf(TEXT("Store_%d"), Fill)))) break;
            Advance(EStep::PrepareStale);
        }
        else Advance(EStep::StaleClick);
        break;
    }
    case EStep::StaleClick:
    {
        FVector2D OldPoint;
        if (!Check(FindButtonPoint(TEXT("Take_0"), OldPoint) && BoxA->GetContents()[0].Quantity == 1
            && Player->GetLootCount() < Player->MaxLootItems, TEXT("stale_take_probe_has_live_A_button_and_free_capacity")))
        { Finish(); break; }
        BeforeItems = Player->GetLootCount();
        BeforeValue = Player->GetTotalLootValue();
        Player->CloseLoot();
        if (!Player->OpenLoot(BoxB)) { Abort(TEXT("same_tick_B_open_failed")); break; }
        Check(BoxB->GetContents()[0].bRevealed && BoxB->GetContents()[0].Quantity == 1,
            TEXT("stale_take_destination_B_has_a_real_available_item"));
        const int64 Revision = BoxB->GetRevision();
        HUD->NotifyHitBoxClick(TEXT("Take_0")); // Deliberately stale A frame.
        Check(Player->GetLootCount() == BeforeItems && Player->GetTotalLootValue() == BeforeValue
            && BoxA->GetContents()[0].Quantity == 1 && BoxB->GetContents()[0].Quantity == 1
            && BoxB->GetRevision() == Revision, TEXT("old_A_frame_cannot_take_from_new_B"));
        Advance(EStep::TakeB);
        break;
    }
    case EStep::TakeB:
        if (!ClickButton(TEXT("Take_0"))) break;
        Check(Player->GetLootCount() == BeforeItems + 1 && Player->GetTotalLootValue() == BeforeValue + 40
            && BoxB->GetContents()[0].Quantity == 0, TEXT("fresh_B_frame_can_take_the_same_available_item"));
        Player->CloseLoot();
        Advance(EStep::EmptyB);
        break;
    case EStep::EmptyB:
        if (!Issued)
        {
            if (!Player->OpenLoot(BoxB)) { Abort(TEXT("empty_B_reopen_failed")); break; }
            Player->BeginSearch();
            Check(IsValid(BoxB) && !BoxB->IsActorBeingDestroyed() && BoxB->GetContents().Num() == 1
                && BoxB->GetContents()[0].Quantity == 0 && !Player->IsSearching(),
                TEXT("empty_container_survives_and_does_not_regenerate"));
            Issued = true;
        }
        if (StepSeconds < .4f) break;
        if (!ClickButton(TEXT("ContainerClose"))) break;
        Check(!Player->IsInventoryOpen() && !Player->GetOpenLoot(), TEXT("canvas_close_button_exits_container_panel"));
        Check(!PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor,
            TEXT("closing_container_restores_game_input_and_hides_cursor"));
        // Set a safe lane heading, then prove movement and looking through the
        // actual bound delegates without repairing movement mode or input flags.
        AimAt(Player->Camera->GetComponentLocation() + FVector(1000.f, 0.f, 0.f));
        ControlStartPosition = Player->GetActorLocation();
        Advance(EStep::ControlMove);
        break;
    case EStep::ControlMove:
        if (StepSeconds < .65f)
        {
            if (!ExecuteAxis(TEXT("MoveForward"), 1.f)) Abort(TEXT("post_close_MoveForward_binding_missing"));
        }
        else
        {
            const FVector Moved = Player->GetActorLocation() - ControlStartPosition;
            Check(Moved.X > 60.f && FMath::Abs(Moved.Y) < 30.f,
                bLiveControlProbe ? TEXT("live_AI_hit_bound_MoveForward_produces_real_lane_movement")
                    : TEXT("post_close_bound_MoveForward_produces_real_lane_movement"), Moved.ToString());
            ExecuteAxis(TEXT("MoveForward"), 0.f);
            Player->ConsumeMovementInputVector();
            Player->GetCharacterMovement()->StopMovementImmediately();
            ControlStartView = PC->GetControlRotation();
            Advance(EStep::ControlLook);
        }
        break;
    case EStep::ControlLook:
        if (StepSeconds < .6f)
        {
            if (!ExecuteAxis(TEXT("Turn"), 30.f * DeltaSeconds)
                || !ExecuteAxis(TEXT("LookUp"), 15.f * DeltaSeconds))
                Abort(TEXT("post_close_view_axis_binding_missing"));
        }
        else
        {
            const FRotator View = PC->GetControlRotation();
            const float YawChange = FMath::Abs(FMath::FindDeltaAngleDegrees(ControlStartView.Yaw, View.Yaw));
            const float PitchChange = FMath::Abs(FMath::FindDeltaAngleDegrees(ControlStartView.Pitch, View.Pitch));
            Check(YawChange > 2.f && PitchChange > 1.f,
                bLiveControlProbe ? TEXT("live_AI_hit_bound_view_axes_change_yaw_and_pitch")
                    : TEXT("post_close_bound_view_axes_change_yaw_and_pitch"),
                FString::Printf(TEXT("yaw=%.2f pitch=%.2f"), YawChange, PitchChange));
            ExecuteAxis(TEXT("Turn"), 0.f);
            ExecuteAxis(TEXT("LookUp"), 0.f);
            // Fire away from every fixture/guard. Release in the same tick so a
            // broken release still has three shot intervals to expose itself.
            AimAt(Player->Camera->GetComponentLocation() + FVector(0.f, -1000.f, 800.f));
            BeforeMagazine = Player->MagazineAmmo;
            BeforeReserve = Player->ReserveAmmo;
            Check(ExecuteAction(TEXT("Fire"), true) && BeforeMagazine > 0
                && Player->MagazineAmmo == BeforeMagazine - 1 && Player->ReserveAmmo == BeforeReserve,
                bLiveControlProbe ? TEXT("live_AI_hit_bound_fire_press_consumes_exactly_one_round")
                    : TEXT("post_close_bound_fire_press_consumes_exactly_one_round"));
            Check(ExecuteAction(TEXT("Fire"), false), bLiveControlProbe ? TEXT("live_AI_hit_bound_fire_release_executes")
                : TEXT("post_close_bound_fire_release_executes"));
            Advance(EStep::ControlFireWait);
        }
        break;
    case EStep::ControlFireWait:
        if (StepSeconds < .65f) break;
        Check(Player->MagazineAmmo == BeforeMagazine - 1 && Player->ReserveAmmo == BeforeReserve,
            bLiveControlProbe ? TEXT("live_AI_hit_released_fire_stays_stopped")
                : TEXT("released_fire_stays_stopped_across_multiple_shot_intervals"),
            FString::Printf(TEXT("magazine=%d expected=%d waited=%.2f"), Player->MagazineAmmo, BeforeMagazine - 1, StepSeconds));
        Player->ConsumeMovementInputVector();
        PlacePlayer(LanePlayer);
        AimAt(BoxA->GetActorLocation());
        if (bLiveControlProbe)
        {
            Operation = 0;
            LiveDeathWindowStarted = GetWorld()->GetTimeSeconds();
            Advance(EStep::LiveDeathPrepare);
        }
        else Advance(EStep::AmmoOpen);
        break;
    case EStep::AmmoOpen:
        if (!Player->OpenLoot(BoxA)) { Abort(TEXT("ammo_case_open_failed")); break; }
        BeforeReserve = Player->ReserveAmmo;
        Operation = 0;
        Advance(EStep::AmmoTake);
        break;
    case EStep::AmmoTake:
        BeforeItems = Player->GetLootCount();
        if (!ClickButton(TEXT("Take_2"))) break;
        Check(Player->GetLootCount() == BeforeItems + 1 && Player->ReserveAmmo == BeforeReserve,
            TEXT("taking_ammo_box_does_not_grant_reserve_ammunition"));
        ++Operation;
        Advance(Operation < 2 ? EStep::AmmoTake : EStep::AmmoStore);
        break;
    case EStep::AmmoStore:
    {
        const int32 Index = InventoryIndex(TEXT("Ammo"));
        if (Index == INDEX_NONE) { Abort(TEXT("ammo_stack_missing_before_store")); break; }
        BeforeItems = Player->GetLootCount();
        if (!ClickButton(FName(*FString::Printf(TEXT("Store_%d"), Index)))) break;
        Check(Player->GetLootCount() == BeforeItems - 1 && BoxA->GetContents()[2].Quantity == 1
            && Player->ReserveAmmo == BeforeReserve, TEXT("storing_ammo_does_not_change_reserve"));
        Advance(EStep::AmmoRetake);
        break;
    }
    case EStep::AmmoRetake:
        if (!ClickButton(TEXT("Take_2"))) break;
        Check(Player->ReserveAmmo == BeforeReserve && BoxA->GetContents()[2].Quantity == 0,
            TEXT("take_store_retake_ammo_cycle_cannot_mint_rounds"));
        Advance(EStep::AmmoUse);
        break;
    case EStep::AmmoUse:
    {
        const int32 Index = InventoryIndex(TEXT("Ammo"));
        if (Index == INDEX_NONE || Player->GetInventoryComponent()->GetEntries()[Index].Quantity != 2)
        { Abort(TEXT("duplicate_use_probe_requires_two_live_ammo_units")); break; }
        const FName Button(*FString::Printf(TEXT("Use_%d"), Index));
        BeforeItems = Player->GetLootCount();
        if (!ClickButton(Button)) break;
        HUD->NotifyHitBoxClick(Button); // Same rendered button; a second live unit still exists.
        const int32 Remaining = InventoryIndex(TEXT("Ammo"));
        Check(Player->ReserveAmmo == BeforeReserve + 24 && Player->GetLootCount() == BeforeItems - 1
            && Remaining != INDEX_NONE && Player->GetInventoryComponent()->GetEntries()[Remaining].Quantity == 1,
            TEXT("one_use_consumes_one_box_adds_24_and_stale_second_use_is_rejected"));
        Player->CloseLoot();
        GuardScenario = 0;
        Advance(EStep::GuardSetup);
        break;
    }

    case EStep::GuardSetup:
        PlacePlayer(LanePlayer);
        if (!GuardBox)
        {
            GuardBox = SpawnFixture(FVector(-1770.f, -1530.f, 48.f), false, true,
                [](TArray<FRaidInventorySeed>& Seeds) { Seeds.Add(Entry(TEXT("Tech"), TEXT("备用电子零件"), 1, 45)); });
            if (!GuardBox) { Abort(TEXT("access_guard_fixture_spawn_failed")); break; }
            GuardBox->SearchDuration = 1.2f;
        }
        if (!Player->OpenLoot(GuardBox)) { Abort(TEXT("access_guard_fixture_not_accessible")); break; }
        AimAt(GuardBox->GetActorLocation());
        Advance(EStep::GuardStart);
        break;
    case EStep::GuardStart:
        if (!Check(Player->IsSearching(), TEXT("opening_container_automatically_searches_without_extra_click"))) { Finish(); break; }
        Check(Player->IsSearching() && Player->GetSearchIndex() == 1
            && GuardBox->GetContents()[0].bRevealed, TEXT("access_guard_starts_search_with_an_existing_revealed_item"));
        BeforeItems = Player->GetLootCount();
        BeforeValue = Player->GetTotalLootValue();
        BeforeSourceRevision = GuardBox->GetRevision();
        Advance(EStep::GuardApply);
        break;
    case EStep::GuardApply:
        if (StepSeconds < .35f) break;
        if (!Check(Player->IsSearching() && Player->GetSearchProgress() > 0.f,
            TEXT("access_failure_probe_interrupts_live_partial_search"))) { Finish(); break; }
        if (GuardScenario == 0)
        {
            const float HP = Player->Health;
            const float Applied = UGameplayStatics::ApplyDamage(Player, 5.f, nullptr, this, UDamageType::StaticClass());
            Check(FMath::IsNearlyEqual(Applied, 5.f) && FMath::IsNearlyEqual(Player->Health, HP - 5.f),
                TEXT("real_nonlethal_damage_applies_while_container_is_open"));
        }
        else if (GuardScenario == 1)
        {
            PlacePlayer(LanePlayer - FVector(400.f, 0.f, 0.f));
        }
        else
        {
            FActorSpawnParameters Spawn;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Cover = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
            if (!Cover) { Abort(TEXT("access_occluder_spawn_failed")); break; }
            UBoxComponent* Blocker = NewObject<UBoxComponent>(Cover, TEXT("ContainerAccessOccluder"));
            Cover->SetRootComponent(Blocker);
            Cover->AddInstanceComponent(Blocker);
            Blocker->SetMobility(EComponentMobility::Movable);
            Blocker->SetBoxExtent(FVector(10.f, 70.f, 140.f));
            Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Blocker->SetCollisionObjectType(ECC_WorldStatic);
            Blocker->SetCollisionResponseToAllChannels(ECR_Block);
            Blocker->SetCanEverAffectNavigation(false);
            Blocker->RegisterComponent();
            FVector Midpoint = (LanePlayer + GuardBox->GetActorLocation()) * .5f;
            Midpoint.Z = 140.f;
            Cover->SetActorLocation(Midpoint);
            const float Yaw = (GuardBox->GetActorLocation() - LanePlayer).Rotation().Yaw;
            Cover->SetActorRotation(FRotator(0.f, Yaw, 0.f));
            Check(!Player->CanAccessLoot(GuardBox), TEXT("runtime_cover_really_blocks_open_container_access"));
        }
        Advance(EStep::GuardWait);
        break;
    case EStep::GuardWait:
        if (!Issued)
        {
            const TCHAR* Name = GuardScenario == 0 ? TEXT("damage_closes_panel_cancels_search_and_blocks_transfer")
                : GuardScenario == 1 ? TEXT("distance_loss_closes_panel_cancels_search_and_blocks_transfer")
                : TEXT("occlusion_closes_panel_cancels_search_and_blocks_transfer");
            Check(!Player->IsInventoryOpen() && !Player->GetOpenLoot() && !Player->IsSearching()
                && !Player->TakeLootItem(0) && GuardBox->GetContents()[0].Quantity == 1
                && Player->GetLootCount() == BeforeItems && Player->GetTotalLootValue() == BeforeValue, Name);
            Issued = true;
        }
        if (StepSeconds < GuardBox->SearchDuration + .3f) break;
        Check(GuardBox->GetContents()[0].bRevealed && !GuardBox->GetContents()[1].bRevealed
            && GuardBox->GetRevision() == BeforeSourceRevision,
            TEXT("interrupted_search_preserves_prior_reveal_and_never_finishes_hidden_row"),
            FString::Printf(TEXT("scenario=%d"), GuardScenario));
        if (IsValid(Cover)) Cover->Destroy();
        Cover = nullptr;
        ++GuardScenario;
        if (GuardScenario < 3) Advance(EStep::GuardSetup);
        else
        {
            PlacePlayer(LanePlayer);
            Advance(EStep::PatrolSetup);
        }
        break;

    case EStep::PatrolSetup:
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        PatrolOrigin = FVector(-1900.f, -1650.f, 96.f);
        PatrolEnemy = GetWorld()->SpawnActor<ARaidEnemy>(ARaidEnemy::StaticClass(), PatrolOrigin, FRotator::ZeroRotator, Spawn);
        if (!PatrolEnemy) { Abort(TEXT("blocked_patrol_fixture_spawn_failed")); break; }
        PatrolEnemy->DetectionRadius = 0.f;
        PatrolEnemy->AttackRange = 0.f;
        PatrolEnemy->PatrolPoints = {FVector(5000.f, 0.f, 96.f), FVector(-1400.f, -1600.f, 96.f)};
        Check(PatrolEnemy->GetPatrolRecoveryCount() == 0, TEXT("blocked_patrol_probe_starts_without_prior_recovery"));
        Advance(EStep::PatrolWait);
        break;
    }
    case EStep::PatrolWait:
    {
        if (!IsValid(PatrolEnemy)) { Abort(TEXT("patrol_fixture_lost")); break; }
        const float Moved = FVector::Dist2D(PatrolEnemy->GetActorLocation(), PatrolOrigin);
        if (PatrolEnemy->GetPatrolRecoveryCount() > 0 && Moved > 100.f)
        {
            Check(PatrolEnemy->StateText == TEXT("Patrol"), TEXT("unreachable_patrol_point_recovers_with_real_movement"),
                FString::Printf(TEXT("recoveries=%d moved=%.1f elapsed=%.2f"),
                    PatrolEnemy->GetPatrolRecoveryCount(), Moved, StepSeconds));
            PatrolEnemy->Destroy();
            PatrolEnemy = nullptr;
            Advance(EStep::WallKill);
        }
        else if (StepSeconds > 9.f)
            Abort(TEXT("unreachable_patrol_did_not_recover_in_nine_seconds"),
                FString::Printf(TEXT("recoveries=%d moved=%.1f"), PatrolEnemy->GetPatrolRecoveryCount(), Moved));
        break;
    }
    case EStep::WallKill:
    {
        int32 OldDrops = 0, OldBoxes = 0;
        for (TActorIterator<ARaidLoot> It(GetWorld()); It; ++It)
        {
            ++OldBoxes;
            if (It->bEnemyDrop) ++OldDrops;
        }
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ARaidEnemy* Victim = GetWorld()->SpawnActor<ARaidEnemy>(ARaidEnemy::StaticClass(),
            FVector(2360.f, 200.f, 96.f), FRotator::ZeroRotator, Spawn);
        if (!Victim) { Abort(TEXT("wall_guard_spawn_failed")); break; }
        Victim->DetectionRadius = 0.f;
        Victim->AttackRange = 0.f;
        const int32 Kills = Mode->EnemiesKilled;
        const float Applied = UGameplayStatics::ApplyDamage(Victim, 999.f, PC, Player, UDamageType::StaticClass());
        Check(Applied > 0.f && Victim->Health <= 0.f && Victim->StateText == TEXT("Dead")
            && Mode->EnemiesKilled == Kills + 1, TEXT("real_damage_kills_guard_at_saved_east_wall"));
        int32 Drops = 0, Boxes = 0;
        for (TActorIterator<ARaidLoot> It(GetWorld()); It; ++It)
        {
            ++Boxes;
            if (It->bEnemyDrop) { ++Drops; EnemyDrop = *It; }
        }
        if (!Check(Drops == OldDrops + 1 && Boxes == OldBoxes + 1 && IsValid(EnemyDrop),
            TEXT("one_real_guard_death_creates_exactly_one_enemy_container"))) { Finish(); break; }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(RaidContainerDropEvidence), false, EnemyDrop);
        const bool Overlaps = GetWorld()->OverlapAnyTestByObjectType(EnemyDrop->GetActorLocation(), FQuat::Identity,
            FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeBox(EnemyDrop->CollisionBox->GetScaledBoxExtent()), Query);
        Check(!Overlaps && EnemyDrop->GetActorLocation().X < 2400.f,
            TEXT("enemy_drop_full_query_box_does_not_penetrate_saved_wall_or_floor"), EnemyDrop->GetActorLocation().ToString());
        EnemyDrop->SearchDuration = 1.5f;
        PlacePlayer(FVector(2180.f, 200.f, 96.f));
        AimAt(EnemyDrop->GetActorLocation());
        Advance(EStep::DropOpen);
        break;
    }
    case EStep::DropOpen:
        Check(Player->GetFocusedLoot() == EnemyDrop, TEXT("wall_drop_is_reachable_by_real_focus_trace"));
        if (!ExecuteAction(TEXT("Interact")) || Player->GetOpenLoot() != EnemyDrop)
        { Abort(TEXT("bound_E_cannot_open_wall_drop")); break; }
        Check(EnemyDrop->GetContainerName() == TEXT("战利品包") && EnemyDrop->GetContents().Num() >= 2
            && RevealedCount(EnemyDrop) == 0, TEXT("enemy_drop_has_unknown_searchable_contents_and_correct_source_name"));
        BeforeItems = Player->GetLootCount();
        BeforeValue = Player->GetTotalLootValue();
        Capture(TEXT("07_enemy_loot_unknown"), EStep::DropStart);
        break;
    case EStep::DropStart:
        if (!Check(Player->IsSearching(), TEXT("opening_container_automatically_searches_without_extra_click"))) { Finish(); break; }
        Advance(EStep::DropReveal);
        break;
    case EStep::DropReveal:
        if (EnemyDrop->GetContents()[0].bRevealed)
        {
            Check(Player->GetLootCount() == BeforeItems && Player->GetTotalLootValue() == BeforeValue,
                TEXT("searching_enemy_loot_never_auto_collects"));
            Advance(EStep::DropTake);
        }
        else if (StepSeconds > 5.f) Abort(TEXT("enemy_loot_search_timeout"));
        break;
    case EStep::DropTake:
        if (!Check(Player->GetLootCount() < Player->MaxLootItems, TEXT("enemy_take_probe_has_free_capacity"))) { Finish(); break; }
        if (!ClickButton(TEXT("Take_0"))) break;
        Check(Player->GetLootCount() == BeforeItems + 1
            && Player->GetTotalLootValue() == BeforeValue + EnemyDrop->GetContents()[0].Definition.UnitValue
            && EnemyDrop->GetContents()[0].Quantity == 0,
            TEXT("enemy_loot_requires_and_accepts_manual_take"));
        Capture(TEXT("08_enemy_loot_taken"), EStep::ExtractSetup);
        break;

    case EStep::ExtractSetup:
    {
        Player->CloseLoot();
        ExpectedItems = Player->GetLootCount();
        ExpectedValue = Player->GetTotalLootValue();
        Check(ExpectedItems > 0 && ExpectedValue > 0 && FMath::IsNearlyEqual(Zone->ExtractSeconds, 8.f),
            TEXT("extraction_uses_actual_carried_inventory_and_saved_eight_second_timer"));
        const FVector Inside = Zone->Trigger->GetComponentLocation();
        FVector Nearby = Inside + FVector(150.f, 0.f, 0.f);
        Nearby.Z = 48.f;
        LateBox = SpawnFixture(Nearby, false, false, [](TArray<FRaidInventorySeed>& Seeds)
        {
            Seeds[0].Definition.UnitValue = 999;
            Seeds[0].Definition.DisplayName = TEXT("未搜索物资");
        });
        if (!LateBox) { Abort(TEXT("late_search_fixture_spawn_failed")); break; }
        LateBox->SearchDuration = 20.f;
        PlacePlayer(Inside);
        AimAt(LateBox->GetActorLocation());
        ExtractionStarted = GetWorld()->GetTimeSeconds();
        Advance(EStep::ExtractCountdown);
        break;
    }
    case EStep::ExtractCountdown:
        if (Zone->IsPlayerInside() && Zone->GetSecondsRemaining() <= 5.f && Mode->IsRaidActive())
        {
            Check(Zone->GetSecondsRemaining() > 2.f && Player->GetLootCount() == ExpectedItems,
                TEXT("real_saved_trigger_counts_down_with_carried_items"));
            Capture(TEXT("09_extraction_countdown"), EStep::ExtractOpen);
        }
        else if (StepSeconds > 7.f) Abort(TEXT("real_extraction_countdown_not_observed"));
        break;
    case EStep::ExtractOpen:
        if (!Mode->IsRaidActive() || !Player->OpenLoot(LateBox))
        { Abort(TEXT("could_not_open_late_container_before_extraction")); break; }
        Advance(EStep::ExtractSearch);
        break;
    case EStep::ExtractSearch:
        if (!Check(Player->IsSearching(), TEXT("opening_container_automatically_searches_without_extra_click"))) { Finish(); break; }
        Check(Player->IsSearching() && Zone->IsPlayerInside() && Zone->GetSecondsRemaining() > 0.f,
            TEXT("twenty_second_search_is_active_during_final_extraction_seconds"));
        Advance(EStep::ExtractFinish);
        break;
    case EStep::ExtractFinish:
        if (Mode->State == ERaidState::Extracted)
        {
            Check(GetWorld()->GetTimeSeconds() - ExtractionStarted >= 7.8f
                && Zone->GetSecondsRemaining() <= KINDA_SMALL_NUMBER && Zone->IsPlayerInside(),
                TEXT("saved_trigger_completes_full_eight_second_extraction"));
            Check(Mode->SecuredItems == ExpectedItems && Mode->SecuredValue == ExpectedValue
                && Player->GetLootCount() == ExpectedItems && Player->GetTotalLootValue() == ExpectedValue,
                TEXT("success_secures_only_backpack_not_unsearched_source_value"));
            Check(!Player->IsInventoryOpen() && !Player->GetOpenLoot() && !Player->IsSearching()
                && RevealedCount(LateBox) == 0 && !Player->TakeLootItem(0),
                TEXT("success_closes_container_cancels_search_and_blocks_transfer"));
            Capture(TEXT("10_extraction_success"), EStep::Success);
        }
        else if (StepSeconds > 10.f) Abort(TEXT("extraction_did_not_finish"));
        break;
    case EStep::Success:
        RestartIntoPhase(1);
        break;

    case EStep::DeathSetup:
        BoxA->SearchDuration = 1.2f;
        Check(GrantContainerFixtureItem(Player, TEXT("DeathCargo"), TEXT("未撤离物资"), 1, 75),
            TEXT("death_fixture_has_real_unsecured_backpack_item"));
        if (!ExecuteAction(TEXT("Interact")) || Player->GetOpenLoot() != BoxA)
        { Abort(TEXT("death_search_container_open_failed")); break; }
        Advance(EStep::DeathStart);
        break;
    case EStep::DeathStart:
        if (!Check(Player->IsSearching(), TEXT("opening_container_automatically_searches_without_extra_click"))) { Finish(); break; }
        Advance(EStep::DeathApply);
        break;
    case EStep::DeathApply:
        if (StepSeconds < .35f) break;
        Check(Player->IsSearching() && Player->GetSearchProgress() > .1f && RevealedCount(BoxA) == 0,
            TEXT("death_probe_interrupts_real_partial_search"));
        Check(UGameplayStatics::ApplyDamage(Player, 999.f, nullptr, this, UDamageType::StaticClass()) > 0.f
            && !Player->IsAlive() && Mode->State == ERaidState::Dead, TEXT("real_damage_during_search_causes_failed_result"));
        Advance(EStep::DeathWait);
        break;
    case EStep::DeathWait:
        if (StepSeconds < BoxA->SearchDuration + .3f) break;
        Check(!Player->IsInventoryOpen() && !Player->GetOpenLoot() && !Player->IsSearching()
            && RevealedCount(BoxA) == 0 && !Player->TakeLootItem(0),
            TEXT("death_cancels_search_without_delayed_reveal_or_transfer"));
        Check(Mode->SecuredItems == 0 && Mode->SecuredValue == 0 && Player->GetLootCount() == 1,
            TEXT("death_does_not_secure_uncollected_or_carried_items"));
        Capture(TEXT("11_death_during_search"), EStep::RestartDeath);
        break;
    case EStep::RestartDeath:
        RestartIntoPhase(2);
        break;

    case EStep::LiveSetup:
        if (!CheckNormalGuards(3, TEXT("live_world_keeps_three_saved_guards_at_normal_defaults"))) { Finish(); break; }
        PlacePlayer(LanePlayer);
        BoxA = SpawnFixture(LaneBox, true, false,
            [](TArray<FRaidInventorySeed>& Seeds) { Seeds[1].bRevealed = true; });
        if (!BoxA) { Abort(TEXT("live_container_spawn_failed")); break; }
        BoxA->SearchDuration = 20.f;
        // One revealed, available row makes blocked post-hit taking non-vacuous.
        // The valuable first row remains unknown and is the real search target.
        Player->OnTakeAnyDamage.AddDynamic(this, &ARaidContainerTestDirector::ObserveLiveDamage);
        AimAt(BoxA->GetActorLocation());
        Advance(EStep::LiveOpen);
        break;
    case EStep::LiveOpen:
        if (!Check(Player->GetFocusedLoot() == BoxA && ExecuteAction(TEXT("Interact"))
            && Player->GetOpenLoot() == BoxA && Player->IsInventoryOpen() && Player->GetLootCount() == 0,
            TEXT("live_AI_phase_bound_E_opens_world_focused_container"))) { Finish(); break; }
        Advance(EStep::LiveStartSearch);
        break;
    case EStep::LiveStartSearch:
    {
        if (!Check(Player->IsSearching(), TEXT("opening_container_automatically_searches_without_extra_click"))) { Finish(); break; }
        if (!Check(Player->IsSearching() && Player->GetSearchIndex() == 0 && RevealedCount(BoxA) == 1
            && BoxA->GetContents()[1].Quantity == 1 && Player->GetLootCount() < Player->MaxLootItems,
            TEXT("live_search_active_with_available_revealed_item_and_free_backpack"))) { Finish(); break; }
        BeforeItems = Player->GetLootCount();
        BeforeValue = Player->GetTotalLootValue();
        BeforeSourceRevision = BoxA->GetRevision();
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // Spawn after search starts; every combat parameter and movement/tick
        // setting remains at the native guard default throughout this phase.
        LiveThreat = GetWorld()->SpawnActor<ARaidEnemy>(ARaidEnemy::StaticClass(),
            FVector(-1200.f, -1400.f, 96.f), FRotator(0.f, 180.f, 0.f), Spawn);
        if (!LiveThreat) { Abort(TEXT("normal_live_guard_spawn_failed")); break; }
        if (!CheckNormalGuards(4, TEXT("live_attack_guard_and_saved_guards_use_normal_combat_defaults"))) { Finish(); break; }
        UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_LIVE_START: player=%s container=%s threat=%s health=%.1f search=%d scope=real_AI_bullets"),
            *Player->GetActorLocation().ToString(), *BoxA->GetActorLocation().ToString(),
            *LiveThreat->GetActorLocation().ToString(), Player->Health, Player->IsSearching());
        Advance(EStep::LiveWaitHit);
        break;
    }
    case EStep::LiveWaitHit:
        if (LiveDamageEvents > 0)
        {
            Check(bLiveDamageFromAI && LiveDamageSource.IsValid() && bLiveHitWhileSearching
                && LiveDamageAmount > 0.f && LiveHealthBefore > LiveDamageAmount,
                TEXT("normal_AI_shot_really_hits_during_open_container_search"));
            Check(Player->IsAlive() && Mode->IsRaidActive() && Player->Health < 100.f
                && FMath::IsNearlyEqual(Player->Health, LiveHealthBefore - LiveDamageAmount),
                TEXT("normal_AI_search_hit_is_nonlethal_and_reduces_real_health"),
                FString::Printf(TEXT("before=%.1f damage=%.1f after=%.1f"), LiveHealthBefore, LiveDamageAmount, Player->Health));
            Check(Player->bHasDamageSource && FMath::Abs(FMath::FindDeltaAngleDegrees(Player->DamageSourceYaw, LiveDamageYaw)) < .1f,
                TEXT("normal_AI_hit_preserves_real_attacker_direction"));
            Check(!Player->IsInventoryOpen() && !Player->GetOpenLoot() && !Player->IsSearching()
                && FMath::IsNearlyZero(Player->GetSearchProgress()),
                TEXT("normal_AI_hit_closes_container_and_cancels_search"));
            Check(!Player->TakeLootItem(1) && Player->GetLootCount() == BeforeItems && Player->GetTotalLootValue() == BeforeValue
                && RevealedCount(BoxA) == 1 && BoxA->GetContents()[1].Quantity == 1
                && BoxA->GetRevision() == BeforeSourceRevision,
                TEXT("normal_AI_hit_blocks_available_item_transfer_and_preserves_reveals"));
            Check(!PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor,
                TEXT("normal_AI_hit_restores_game_input_and_hides_cursor"));
            UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_LIVE_INTERRUPTED: health=%.1f state=%d panel=%d search=%d items=%d value=%lld"),
                Player->Health, static_cast<int32>(Mode->State), Player->IsInventoryOpen(),
                Player->IsSearching(), Player->GetLootCount(), static_cast<long long>(Player->GetTotalLootValue()));
            if (Run().Failures) { Finish(); break; }
            bLiveControlProbe = true;
            AimAt(Player->Camera->GetComponentLocation() + FVector(1000.f, 0.f, 0.f));
            ControlStartPosition = Player->GetActorLocation();
            Capture(TEXT("12_live_combat_interrupt"), EStep::ControlMove);
        }
        else if (StepSeconds > 8.f)
        {
            Abort(TEXT("normal_AI_did_not_hit_real_search_within_eight_seconds"),
                FString::Printf(TEXT("guard_state=%s guard=%s player=%s hp=%.1f searching=%d"),
                    *LiveThreat->StateText, *LiveThreat->GetActorLocation().ToString(),
                    *Player->GetActorLocation().ToString(), Player->Health, Player->IsSearching()));
        }
        break;
    case EStep::LiveDeathPrepare:
        if (!Player->IsAlive() || !Mode->IsRaidActive()) { Abort(TEXT("live_death_setup_requires_living_player")); break; }
        if (GetWorld()->GetTimeSeconds() - LiveDeathWindowStarted > 8.f)
        { Abort(TEXT("live_death_container_setup_timeout")); break; }
        if (Operation == 0)
        {
            BoxA->SearchDuration = 3.f;
            Check(GrantContainerFixtureItem(Player, TEXT("LiveUnsecuredCargo"), BoxA->GetContents()[0].Definition.DisplayName, 1, 75),
                TEXT("live_death_fixture_has_real_carried_unsecured_value"));
            ExpectedItems = Player->GetLootCount();
            ExpectedValue = Player->GetTotalLootValue();
            ++Operation;
        }
        AimAt(BoxA->GetActorLocation());
        if (!ExecuteAction(TEXT("Interact")) || Player->GetOpenLoot() != BoxA)
        { Abort(TEXT("live_death_bound_E_open_failed")); break; }
        Advance(EStep::LiveDeathSearch);
        break;
    case EStep::LiveDeathSearch:
        // A normal shot may interrupt the small E-to-Canvas redraw window.
        // Retry while still healthy; only lower health after search is active.
        if (Player->GetOpenLoot() != BoxA || !Player->IsInventoryOpen())
        {
            UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_LIVE_RETRY: normal shot closed the panel before search dispatch; health=%.1f"), Player->Health);
            Advance(EStep::LiveDeathPrepare);
            break;
        }
        if (!Check(Player->IsSearching(), TEXT("opening_container_automatically_searches_without_extra_click"))) { Finish(); break; }
        if (!Check(Player->IsSearching() && Player->GetSearchIndex() == 0 && Player->GetOpenLoot() == BoxA
            && RevealedCount(BoxA) == 1 && Player->IsAlive(),
            TEXT("live_lethal_probe_has_real_search_active_before_low_health_fixture"))) { Finish(); break; }
        Player->Health = 4.f;
        LiveDeathEventBaseline = LiveDamageEvents;
        bLiveHitWhileSearching = false;
        bLiveDamageFromAI = false;
        BeforeSourceRevision = BoxA->GetRevision();
        UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_FIXTURE: scope=normal_AI_looting_risk low_health=4 after_real_search_start; next damage must come from a normally ticking guard; no ApplyDamage call."));
        Advance(EStep::LiveWaitDeath);
        break;
    case EStep::LiveWaitDeath:
        if (!Player->IsAlive() || Mode->State == ERaidState::Dead)
        {
            Check(LiveDamageEvents > LiveDeathEventBaseline && bLiveDamageFromAI && LiveDamageSource.IsValid()
                && bLiveHitWhileSearching && FMath::IsNearlyEqual(LiveHealthBefore, 4.f)
                && FMath::IsNearlyEqual(LiveDamageAmount, 4.f),
                TEXT("normal_AI_next_bullet_is_the_real_lethal_search_interrupt"));
            Check(FMath::IsNearlyZero(Player->Health) && Mode->State == ERaidState::Dead
                && !Player->IsInventoryOpen() && !Player->GetOpenLoot() && !Player->IsSearching(),
                TEXT("normal_AI_search_death_enters_failed_result_and_cleans_container"));
            Check(PC->IsMoveInputIgnored() && PC->IsLookInputIgnored() && PC->bShowMouseCursor
                && Player->GetCharacterMovement()->MovementMode == MOVE_None,
                TEXT("normal_AI_death_hands_control_to_result_UI"));
            Check(Mode->SecuredItems == 0 && Mode->SecuredValue == 0 && ExpectedItems > 0 && ExpectedValue > 0
                && Player->GetLootCount() == ExpectedItems && Player->GetTotalLootValue() == ExpectedValue,
                TEXT("normal_AI_death_secures_neither_carried_nor_container_value"));
            Check(!Player->TakeLootItem(1) && RevealedCount(BoxA) == 1 && BoxA->GetContents()[1].Quantity == 1
                && BoxA->GetRevision() == BeforeSourceRevision,
                TEXT("normal_AI_death_blocks_available_source_item_transfer"));
            UE_LOG(LogTemp, Display, TEXT("RAID_CONTAINER_LIVE_DEATH: health=%.1f state=%d source=%s source_yaw=%.2f panel=%d search=%d secured_items=%d secured_value=%lld"),
                Player->Health, static_cast<int32>(Mode->State), *GetNameSafe(LiveDamageSource.Get()),
                Player->DamageSourceYaw, Player->IsInventoryOpen(), Player->IsSearching(), Mode->SecuredItems, static_cast<long long>(Mode->SecuredValue));
            if (Run().Failures) { Finish(); break; }
            Capture(TEXT("13_live_combat_death"), EStep::LiveDeadWait);
        }
        else if (StepSeconds > 5.f) Abort(TEXT("normal_AI_lethal_search_hit_timeout"));
        break;
    case EStep::LiveDeadWait:
        if (GetWorld()->GetTimeSeconds() - LiveDamageAt < BoxA->SearchDuration + .3f) break;
        Check(!Player->IsSearching() && !Player->GetOpenLoot() && RevealedCount(BoxA) == 1
            && BoxA->GetRevision() == BeforeSourceRevision && Player->GetLootCount() == ExpectedItems
            && Player->GetTotalLootValue() == ExpectedValue && Mode->SecuredItems == 0 && Mode->SecuredValue == 0,
            TEXT("normal_AI_death_has_no_delayed_reveal_or_income_after_search_duration"));
        CheckNormalGuards(4, TEXT("live_phase_never_disables_or_weakens_any_guard"));
        Advance(EStep::Finish);
        break;
    case EStep::Screenshot:
    {
        const int64 Size = IFileManager::Get().FileSize(*ScreenshotPath);
        ScreenshotStableSeconds = Size > 128 && Size == LastScreenshotSize ? ScreenshotStableSeconds + DeltaSeconds : 0.f;
        LastScreenshotSize = Size;
        if (ScreenshotStableSeconds > .2f && !FScreenshotRequest::IsScreenshotRequested())
        {
            ++Run().Captures;
            Check(true, TEXT("screenshot_written_and_stable"), ScreenshotPath);
            Advance(AfterCapture);
        }
        else if (StepSeconds > 12.f) Abort(TEXT("screenshot_write_timeout"), ScreenshotPath);
        break;
    }
    case EStep::AwaitTravel:
        if (StepSeconds > 15.f) Abort(TEXT("real_OpenLevel_restart_timeout"));
        break;
    case EStep::Finish:
        Finish();
        break;
    }
}
