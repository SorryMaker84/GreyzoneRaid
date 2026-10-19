#include "RaidFlowDirector.h"
#include "RaidEnemy.h"
#include "RaidExtractionZone.h"
#include "RaidGameMode.h"
#include "RaidHUD.h"
#include "RaidInventoryComponent.h"
#include "RaidLoot.h"
#include "RaidPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fonts/CompositeFont.h"
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
    struct FFlowMemory
    {
        // Each OpenLevel destroys the director; this memory survives those worlds.
        int32 Phase = 0; // Menu, raid 1, menu, raid 2, raid 3, final menu.
        int32 Checks = 0;
        int32 Failures = 0;
        int32 Captures = 0;
        double Started = 0.0;
        bool Finished = false;
        TWeakObjectPtr<UWorld> PreviousWorld;
        TWeakObjectPtr<ARaidPlayer> PreviousPlayer;
        TWeakObjectPtr<ARaidEnemy> PreviousEnemy;
        TWeakObjectPtr<ARaidLoot> PreviousLoot;
    };
    FFlowMemory& Memory() { static FFlowMemory Run; return Run; }
}

ARaidFlowDirector::ARaidFlowDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    // Observe the extraction zone after its normal gameplay tick, including damage resets.
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void ARaidFlowDirector::BeginPlay()
{
    Super::BeginPlay();
    if (!FParse::Param(FCommandLine::Get(), TEXT("RaidFlowTest")) || Memory().Finished)
    {
        Destroy();
        return;
    }
    if (Memory().Started == 0.0) Memory().Started = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Display, TEXT("RAID_FLOW: R6 phase=%d; real HUD hitbox dispatch, container search/manual transfer, gameplay actions and OpenLevel; hardware input NOT tested."), Memory().Phase);
}

void ARaidFlowDirector::Advance(EStep Next)
{
    Step = Next;
    StepSeconds = 0.f;
    Issued = false;
}

void ARaidFlowDirector::Check(bool Passed, const TCHAR* Name, const FString& Detail)
{
    ++Memory().Checks;
    if (!Passed) ++Memory().Failures;
    UE_LOG(LogTemp, Display, TEXT("RAID_FLOW_CHECK:%s %s | phase=%d %s"),
        Passed ? TEXT("PASS") : TEXT("FAIL"), Name, Memory().Phase, *Detail);
}

void ARaidFlowDirector::Abort(const TCHAR* Name, const FString& Detail)
{
    Check(false, Name, Detail);
    Memory().Finished = true;
    UE_LOG(LogTemp, Display, TEXT("RAID_FLOW_RESULT: FAIL checks=%d failures=%d"), Memory().Checks, Memory().Failures);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ARaidFlowDirector::CheckInputs(bool Running)
{
    Check(PC->bShowMouseCursor == !Running, TEXT("cursor_matches_screen"));
    Check(PC->bEnableClickEvents == !Running && PC->bEnableMouseOverEvents == !Running,
        TEXT("click_and_hover_match_screen"));
    Check(PC->IsMoveInputIgnored() == !Running && PC->IsLookInputIgnored() == !Running,
        TEXT("move_and_look_ignore_match_screen"));
}

void ARaidFlowDirector::SuppressRaidAI()
{
    if (AISuppressed) return;
    for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It)
    {
        It->DetectionRadius = 0.f;
        It->AttackRange = 0.f;
        It->GetCharacterMovement()->StopMovementImmediately();
    }
    AISuppressed = true;
    UE_LOG(LogTemp, Display, TEXT("RAID_FLOW_FIXTURE: this opt-in run suppresses enemy detection/attack and teleports the player; saved map and normal AI unchanged. Menu guards are tested before suppression."));
}

void ARaidFlowDirector::CheckFreshRaid()
{
    int32 Enemies = 0, Supplies = 0;
    ARaidEnemy* FirstEnemy = nullptr;
    ARaidLoot* FirstLoot = nullptr;
    for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It) { ++Enemies; if (!FirstEnemy) FirstEnemy = *It; }
    for (TActorIterator<ARaidLoot> It(GetWorld()); It; ++It) { ++Supplies; if (!FirstLoot) FirstLoot = *It; }
    Check(Enemies == 3 && Supplies == 9, TEXT("fresh_map_has_3_enemies_9_supplies"),
        FString::Printf(TEXT("enemies=%d supplies=%d"), Enemies, Supplies));
    if (Memory().Phase > 0)
    {
        Check(Memory().PreviousWorld != TWeakObjectPtr<UWorld>(GetWorld()), TEXT("travel_creates_new_world"));
        Check(Memory().PreviousPlayer != TWeakObjectPtr<ARaidPlayer>(Player.Get()), TEXT("travel_creates_new_player"));
        Check(Memory().PreviousEnemy != TWeakObjectPtr<ARaidEnemy>(FirstEnemy)
            && Memory().PreviousLoot != TWeakObjectPtr<ARaidLoot>(FirstLoot), TEXT("travel_creates_fresh_enemy_and_loot_actors"));
    }
    Check(FMath::IsNearlyEqual(Player->Health, 100.f), TEXT("fresh_health_100"));
    Check(Player->MagazineAmmo == 12 && Player->ReserveAmmo == 60, TEXT("fresh_ammo_12_reserve_60"));
    Check(Player->GetLootCount() == 0 && Player->GetInventoryComponent()->GetEntries().IsEmpty() && Player->GetTotalLootValue() == 0,
        TEXT("fresh_inventory_empty"));
    Check(Mode->SecuredItems == 0 && Mode->SecuredValue == 0 && Mode->EnemiesKilled == 0,
        TEXT("fresh_result_counters_zero"));
    Check(!Player->bReloading && !Player->bAiming && !Player->IsSearching() && !Player->IsInventoryOpen()
        && !Player->GetOpenLoot() && Player->GetSearchIndex() == INDEX_NONE,
        TEXT("fresh_actions_not_pending"));
    int32 FreshContainers = 0;
    for (TActorIterator<ARaidLoot> It(GetWorld()); It; ++It)
    {
        It->EnsureContents();
        const TArray<FRaidInventoryEntry>& Contents = It->GetContents();
        const bool HasOriginalItem = Contents.Num() >= 2 && Contents.Num() <= 3
            && Contents[0].Definition.ItemId == It->ItemId && Contents[0].Definition.DisplayName == It->DisplayName
            && Contents[0].Quantity == FMath::Max(1, It->Quantity) && Contents[0].Definition.UnitValue == FMath::Max(0, It->Value);
        const bool AllUnknown = !Contents.ContainsByPredicate([](const FRaidInventoryEntry& Item)
        {
            return Item.bRevealed || Item.Quantity <= 0;
        });
        if (HasOriginalItem && AllUnknown) ++FreshContainers;
    }
    Check(FreshContainers == 9, TEXT("fresh_raid_restores_all_nine_unknown_populated_containers"),
        FString::Printf(TEXT("fresh_containers=%d phase=%d"), FreshContainers, Memory().Phase));
}

void ARaidFlowDirector::CheckChineseFonts()
{
    // These checks run against loaded assets in both editor and cooked builds.
    const UFont* RuntimeFont = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Fonts/F_RaidChinese.F_RaidChinese"));
    Check(RuntimeFont && RuntimeFont->FontCacheType == EFontCacheType::Runtime
        && RuntimeFont->RuntimeFontSource == ERuntimeFontSource::Asset,
        TEXT("chinese_hud_runtime_font_loads"), GetPathNameSafe(RuntimeFont));
    const FCompositeFont* Composite = RuntimeFont ? RuntimeFont->GetCompositeFont() : nullptr;
    const UFontFace* Face = Composite && Composite->DefaultTypeface.Fonts.Num() == 1
        ? Cast<UFontFace>(Composite->DefaultTypeface.Fonts[0].Font.GetFontFaceAsset()) : nullptr;
    Check(Face && Face->GetLoadingPolicy() == EFontLoadingPolicy::Inline,
        TEXT("chinese_default_typeface_uses_inline_font_face"), GetPathNameSafe(Face));
    const int32 EmbeddedBytes = Face ? Face->GetFontFaceData()->GetData().Num() : 0;
    Check(EmbeddedBytes == 16437364, TEXT("chinese_font_face_contains_complete_embedded_payload"),
        FString::Printf(TEXT("embedded_bytes=%d expected=16437364"), EmbeddedBytes));

    const UFont* WorldFont = LoadObject<UFont>(nullptr, TEXT("/Game/UI/Fonts/F_RaidChineseWorld.F_RaidChineseWorld"));
    Check(WorldFont && WorldFont->FontCacheType == EFontCacheType::Offline,
        TEXT("chinese_world_offline_font_loads"), GetPathNameSafe(WorldFont));
    Check(WorldFont && WorldFont->Textures.Num() == 1 && IsValid(WorldFont->Textures[0].Get()),
        TEXT("chinese_world_font_has_one_loaded_atlas"),
        FString::Printf(TEXT("atlases=%d"), WorldFont ? WorldFont->Textures.Num() : 0));

    int32 Labels = 0, FontMismatches = 0, Glyphs = 0, MissingGlyphs = 0;
    int32 RemappedGlyphs = 0, DirectAsciiGlyphs = 0, ChineseGlyphs = 0;
    TSet<uint16> UniqueGlyphs;
    FString FontFailures, GlyphFailures;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        TArray<UTextRenderComponent*> Components;
        It->GetComponents<UTextRenderComponent>(Components);
        for (const UTextRenderComponent* Component : Components)
        {
            ++Labels;
            if (!WorldFont || Component->Font.Get() != WorldFont)
            {
                ++FontMismatches;
                if (FontMismatches <= 8) FontFailures += Component->GetPathName() + TEXT(" ");
            }
            // TextRender treats <br> as a line break, not as visible glyphs.
            const FString Text = Component->Text.ToString().Replace(TEXT("<br>"), TEXT("\n"));
            for (const TCHAR Character : Text)
            {
                if (FChar::IsWhitespace(Character)) continue;
                ++Glyphs;
                const uint16 CodePoint = static_cast<uint16>(Character);
                UniqueGlyphs.Add(CodePoint);
                if ((CodePoint >= 0x3400 && CodePoint <= 0x4DBF) || (CodePoint >= 0x4E00 && CodePoint <= 0x9FFF)
                    || (CodePoint >= 0xF900 && CodePoint <= 0xFAFF)) ++ChineseGlyphs;
                int32 GlyphIndex = INDEX_NONE;
                if (WorldFont)
                {
                    if (WorldFont->IsRemapped)
                    {
                        if (const uint16* Remapped = WorldFont->CharRemap.Find(CodePoint))
                        {
                            GlyphIndex = *Remapped;
                            ++RemappedGlyphs;
                        }
                    }
                    // UFont only renders direct indices when IsRemapped is false.
                    // Never mistake RemapChar's missing-glyph square for coverage.
                    else if (CodePoint >= 33 && CodePoint <= 126 && WorldFont->RemapChar(Character) == Character)
                    {
                        GlyphIndex = CodePoint;
                        ++DirectAsciiGlyphs;
                    }
                }
                bool HasGlyph = WorldFont && WorldFont->Characters.IsValidIndex(GlyphIndex);
                if (HasGlyph)
                {
                    const FFontCharacter& Glyph = WorldFont->Characters[GlyphIndex];
                    HasGlyph = Glyph.USize > 0 && Glyph.VSize > 0
                        && WorldFont->Textures.IsValidIndex(Glyph.TextureIndex)
                        && IsValid(WorldFont->Textures[Glyph.TextureIndex].Get());
                }
                if (!HasGlyph)
                {
                    ++MissingGlyphs;
                    if (MissingGlyphs <= 8) GlyphFailures += FString::Printf(TEXT("%s U+%04X "),
                        *Component->GetPathName(), static_cast<uint32>(CodePoint));
                }
            }
        }
    }
    Check(Labels > 0 && FontMismatches == 0, TEXT("all_world_text_components_use_chinese_font"),
        FString::Printf(TEXT("labels=%d mismatched_fonts=%d %s"), Labels, FontMismatches, *FontFailures));
    Check(ChineseGlyphs > 0 && MissingGlyphs == 0, TEXT("all_world_text_glyphs_exist_in_loaded_atlas"),
        FString::Printf(TEXT("labels=%d glyphs=%d chinese=%d unique=%d remapped=%d direct_ascii=%d missing=%d %s"),
            Labels, Glyphs, ChineseGlyphs, UniqueGlyphs.Num(), RemappedGlyphs, DirectAsciiGlyphs, MissingGlyphs, *GlyphFailures));

    int32 Supplies = 0, ChineseNames = 0, LatinNames = 0;
    FString NameFailures;
    for (TActorIterator<ARaidLoot> It(GetWorld()); It; ++It)
    {
        ++Supplies;
        bool HasChinese = false, HasLatin = false;
        for (const TCHAR Character : It->DisplayName)
        {
            const uint32 CodePoint = static_cast<uint32>(Character);
            HasChinese |= (CodePoint >= 0x3400 && CodePoint <= 0x4DBF)
                || (CodePoint >= 0x4E00 && CodePoint <= 0x9FFF) || (CodePoint >= 0xF900 && CodePoint <= 0xFAFF);
            HasLatin |= (Character >= TEXT('A') && Character <= TEXT('Z'))
                || (Character >= TEXT('a') && Character <= TEXT('z'));
        }
        if (HasChinese) ++ChineseNames;
        if (HasLatin) ++LatinNames;
        if (!HasChinese || HasLatin) NameFailures += FString::Printf(TEXT("%s=%s "), *It->GetName(), *It->DisplayName);
    }
    Check(Supplies == 9 && ChineseNames == Supplies && LatinNames == 0,
        TEXT("all_nine_saved_loot_display_names_are_chinese"),
        FString::Printf(TEXT("supplies=%d chinese_names=%d latin_names=%d %s"), Supplies, ChineseNames, LatinNames, *NameFailures));
}

void ARaidFlowDirector::PlacePlayer(const FVector& Position)
{
    Player->GetCharacterMovement()->StopMovementImmediately();
    Player->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
    Player->GetCapsuleComponent()->UpdateOverlaps();
}

void ARaidFlowDirector::AimAt(const FVector& Position)
{
    const FRotator View = (Position - Player->Camera->GetComponentLocation()).Rotation();
    PC->SetControlRotation(View);
    Player->Camera->SetWorldRotation(View);
}

bool ARaidFlowDirector::ExecuteAction(FName Name, bool Pressed)
{
    // UI intent is owned by the controller; weapon and movement input stays on the pawn.
    UInputComponent* Inputs[] = {
        PC ? PC->InputComponent.Get() : nullptr,
        Player ? Player->InputComponent.Get() : nullptr
    };
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

bool ARaidFlowDirector::FindButtonPoint(FName Name, FVector2D& Point) const
{
    const FHUDHitBox* Box = HUD->GetHitBoxWithName(Name);
    if (!Box) return false;
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    FVector2D Low(Width, Height), High(-1.f, -1.f);
    // This installed FHUDHitBox has private Coords/Size and no GetPos/GetSize.
    // Public Contains finds its drawn bounds without assuming the HUD's layout.
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

bool ARaidFlowDirector::ClickButton(FName Name)
{
    FVector2D Point;
    const bool Found = FindButtonPoint(Name, Point);
    Check(Found, TEXT("visible_button_hitbox_found"), Name.ToString());
    if (!Found) return false;
    const bool Dispatched = HUD->UpdateAndDispatchHitBoxClickEvents(Point, IE_Pressed);
    Check(Dispatched, TEXT("canvas_button_engine_click_dispatch"),
        FString::Printf(TEXT("%s at %s; OS mouse NOT injected"), *Name.ToString(), *Point.ToString()));
    return Dispatched;
}

void ARaidFlowDirector::Travel(FName Button, int32 NextPhase)
{
    Memory().PreviousWorld = GetWorld();
    Memory().PreviousPlayer = Player;
    for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It) { Memory().PreviousEnemy = *It; break; }
    for (TActorIterator<ARaidLoot> It(GetWorld()); It; ++It) { Memory().PreviousLoot = *It; break; }
    Memory().Phase = NextPhase;
    Advance(EStep::AwaitTravel);
    if (!ClickButton(Button)) Abort(TEXT("travel_button_dispatch_failed"), Button.ToString());
}

void ARaidFlowDirector::Capture(const TCHAR* Name, EStep Next)
{
    const FString Folder = FPaths::Combine(FPaths::ProjectDir(), TEXT("Evidence/R6Flow"));
    IFileManager::Get().MakeDirectory(*Folder, true);
    ScreenshotPath = FPaths::Combine(Folder, FString(Name) + TEXT(".png"));
    if (IFileManager::Get().FileExists(*ScreenshotPath)
        && !IFileManager::Get().Delete(*ScreenshotPath, false, true))
    {
        Abort(TEXT("cannot_replace_own_old_screenshot"), ScreenshotPath);
        return;
    }
    LastScreenshotSize = -1;
    ScreenshotStableSeconds = 0.f;
    FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
    AfterCapture = Next;
    Advance(EStep::Screenshot);
}

void ARaidFlowDirector::FinishAndQuit()
{
    FVector2D Point;
    const bool Found = FindButtonPoint(TEXT("Quit"), Point);
    Check(Found, TEXT("final_quit_button_visible"));
    Check(Mode->State == ERaidState::MainMenu && Memory().Phase == 5, TEXT("full_flow_reached_clean_final_menu"));
    Check(Memory().Captures == 12, TEXT("required_screenshots_written_before_travel"),
        FString::Printf(TEXT("captures=%d"), Memory().Captures));
    Memory().Finished = true;
    UE_LOG(LogTemp, Display, TEXT("RAID_FLOW_RESULT: %s checks=%d failures=%d elapsed=%.2fs captures=%d; hardware input NOT tested."),
        Memory().Failures == 0 ? TEXT("PASS") : TEXT("FAIL"), Memory().Checks, Memory().Failures,
        FPlatformTime::Seconds() - Memory().Started, Memory().Captures);
    // The shell separately requires RAID_QUIT and a normal process exit after this marker.
    if (!Found || !HUD->UpdateAndDispatchHitBoxClickEvents(Point, IE_Pressed))
        FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ARaidFlowDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Memory().Finished) return;
    if (FPlatformTime::Seconds() - Memory().Started > 210.0)
    {
        Abort(TEXT("flow_timeout"), FString::Printf(TEXT("phase=%d step=%d"), Memory().Phase, static_cast<int32>(Step)));
        return;
    }
#if WITH_EDITOR
    if (Step != EStep::Screenshot && GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
    StepSeconds += DeltaSeconds;
    if (Step != EStep::Ready && (!IsValid(Player) || !IsValid(Mode) || !IsValid(HUD) || !IsValid(PC)))
    {
        Abort(TEXT("flow_world_references_invalid"));
        return;
    }
    if (Step >= EStep::ContainerOpen && Step <= EStep::ContainerRevisit
        && (!IsValid(Loot) || Loot->IsActorBeingDestroyed()))
    {
        Abort(TEXT("container_must_survive_open_search_take_and_revisit"));
        return;
    }

    switch (Step)
    {
    case EStep::Ready:
        Player = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerCharacter(this, 0));
        Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
        PC = UGameplayStatics::GetPlayerController(this, 0);
        HUD = PC ? Cast<ARaidHUD>(PC->GetHUD()) : nullptr;
        if (!Player || !Mode || !PC || !HUD || !Player->Camera)
        {
            if (StepSeconds > 10.f) Abort(TEXT("flow_player_controller_hud_ready"));
            break;
        }
        if (Mode->IsRaidActive()) SuppressRaidAI();
        if (StepSeconds < .5f) break; // The Canvas must draw before its hitboxes can be tested.
        CheckFreshRaid();
        if (Memory().Phase == 0) CheckChineseFonts();
        for (TActorIterator<ARaidExtractionZone> It(GetWorld()); It; ++It) { Zone = *It; break; }
        if (!Zone || !Zone->Trigger) { Abort(TEXT("saved_extraction_zone_exists")); break; }
        Check(FMath::IsNearlyEqual(Zone->ExtractSeconds, 8.f), TEXT("saved_zone_uses_real_8_second_countdown"));
        InsidePosition = Zone->Trigger->GetComponentLocation();
        if (Memory().Phase == 0 || Memory().Phase == 2 || Memory().Phase == 5)
        {
            Check(Mode->State == ERaidState::MainMenu && !Mode->IsRaidActive(), TEXT("ordinary_start_or_return_is_main_menu"));
            CheckInputs(false);
            Check(Player->IsHidden() && PC->GetViewTarget() != Player, TEXT("menu_has_scene_camera_and_hidden_pawn"));
            Player->GetCharacterMovement()->StopMovementImmediately();
            BeforePosition = Player->GetActorLocation();
            BeforeView = PC->GetControlRotation();
            BeforeHealth = Player->Health;
            BeforeAmmo = Player->MagazineAmmo;
            // A nearby, fully enabled enemy proves the frontend combat gate, rather than AI suppression.
            for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It)
            {
                It->SetActorLocation(BeforePosition + FVector(250, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
                It->DetectionRadius = 5000.f;
                It->AttackRange = 1000.f;
                It->AttackInterval = .1f;
                break;
            }
            UE_LOG(LogTemp, Display, TEXT("RAID_FLOW_FIXTURE: menu threat enemy repositioned nearby with AI enabled; no AI suppression in frontend."));
            Advance(EStep::MenuGuard);
        }
        else
        {
            Check(Mode->State == ERaidState::Running && Mode->IsRaidActive(), TEXT("start_or_retry_enters_running"));
            Check(UGameplayStatics::HasOption(Mode->OptionsString, TEXT("RaidStart")), TEXT("running_travel_has_RaidStart_option"));
            CheckInputs(true);
            Check(!Player->IsHidden(), TEXT("running_pawn_visible"));
            if (Memory().Phase == 1) Capture(TEXT("03_running"), EStep::EmptyExtraction);
            else if (Memory().Phase == 3) Advance(EStep::Death);
            else Capture(TEXT("07_restart_clean"), EStep::Abandon);
        }
        break;

    case EStep::MenuGuard:
        if (!Issued)
        {
            const float Applied = UGameplayStatics::ApplyDamage(Player, 50.f, nullptr, this, UDamageType::StaticClass());
            Check(Applied == 0.f && Player->Health == BeforeHealth, TEXT("menu_player_damage_guard"));
            for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It)
            {
                const float HP = It->Health;
                Check(UGameplayStatics::ApplyDamage(*It, 50.f, PC, this, UDamageType::StaticClass()) == 0.f
                    && It->Health == HP, TEXT("menu_enemy_damage_guard"));
                break;
            }
            Player->Fire(); Player->StartFire(); Player->Reload(); Player->StartAim(); Player->BeginSearch();
            // Use a catalog item so rejection proves the menu guard, not an unknown item ID.
            Check(!Player->AddItem(TEXT("Tech"), TEXT("菜单测试物资"), 1, 1), TEXT("menu_inventory_add_guard"));
            Issued = true;
        }
        Player->MoveForward(1.f); Player->MoveRight(1.f); Player->Turn(30.f * DeltaSeconds); Player->LookUp(20.f * DeltaSeconds);
        if (StepSeconds > 1.25f)
        {
            Check(Player->Health == BeforeHealth, TEXT("enabled_enemy_does_not_attack_in_menu"));
            Check(Player->MagazineAmmo == BeforeAmmo && Player->ReserveAmmo == 60 && !Player->bReloading,
                TEXT("menu_fire_and_reload_guards"));
            Check(!Player->bAiming && !Player->IsSearching() && Player->GetLootCount() == 0, TEXT("menu_aim_search_and_loot_guards"));
            Check(FVector::Dist2D(BeforePosition, Player->GetActorLocation()) < .5f
                && Player->GetPendingMovementInputVector().IsNearlyZero(), TEXT("menu_movement_guard"));
            Check(PC->GetControlRotation().Equals(BeforeView, .05f), TEXT("menu_view_guard"));
            Check(FMath::IsNearlyZero(Mode->RaidElapsed), TEXT("menu_raid_clock_stays_zero"));
            Check(FMath::IsNearlyEqual(Zone->GetSecondsRemaining(), 8.f), TEXT("menu_extraction_does_not_count_down"));
            Check(!UGameplayStatics::HasOption(Mode->OptionsString, TEXT("RaidStart")), TEXT("menu_travel_clears_RaidStart_option"));
            Player->StopFire(); Player->StopAim(); Player->CancelSearch();
            if (Memory().Phase == 0) Capture(TEXT("01_main_menu"), EStep::Instructions);
            else if (Memory().Phase == 2) Travel(TEXT("Start"), 3);
            else Capture(TEXT("08_return_menu"), EStep::Finish);
        }
        break;

    case EStep::Instructions:
        if (!Issued)
        {
            if (!ClickButton(TEXT("Instructions"))) { Abort(TEXT("instructions_button_failed")); break; }
            Check(Mode->State == ERaidState::Instructions, TEXT("instructions_button_changes_screen"));
            CheckInputs(false);
            Mode->HandleMenuAction(TEXT("Start"));
            Check(Mode->State == ERaidState::Instructions, TEXT("instructions_rejects_hidden_start_action"));
            Issued = true;
        }
        if (StepSeconds > .5f) Capture(TEXT("02_instructions"), EStep::Back);
        break;

    case EStep::Back:
        if (StepSeconds > .3f)
        {
            if (!ClickButton(TEXT("Back"))) { Abort(TEXT("instructions_back_button_failed")); break; }
            Check(Mode->State == ERaidState::MainMenu, TEXT("instructions_back_returns_main_menu"));
            CheckInputs(false);
            Advance(EStep::Deploy);
        }
        break;

    case EStep::Deploy:
        if (StepSeconds > .4f) Travel(TEXT("Start"), 1);
        break;

    case EStep::EmptyExtraction:
        if (!Issued) { PlacePlayer(InsidePosition); Issued = true; }
        if (StepSeconds > .5f)
        {
            Check(Zone->IsPlayerInside() && Player->GetLootCount() == 0, TEXT("empty_backpack_real_zone_overlap"));
            Check(Mode->State == ERaidState::Running && FMath::IsNearlyEqual(Zone->GetSecondsRemaining(), 8.f),
                TEXT("extraction_requires_materials"));
            Mode->EndRaid(true);
            Check(Mode->State == ERaidState::Running && Mode->SecuredItems == 0, TEXT("empty_backpack_success_guard"));
            for (TActorIterator<ARaidLoot> It(GetWorld()); It; ++It) { Loot = *It; break; }
            if (!Loot) { Abort(TEXT("saved_supply_exists_for_search")); break; }
            bool Focused = false;
            for (const FVector Offset : {FVector(-160, 0, 96), FVector(160, 0, 96), FVector(0, -160, 96), FVector(0, 160, 96)})
            {
                PlacePlayer(Loot->GetActorLocation() + Offset);
                AimAt(Loot->CollisionBox->GetComponentLocation());
                if (Player->GetFocusedLoot() == Loot) { Focused = true; break; }
            }
            Check(Focused, TEXT("real_saved_supply_found_by_visibility_trace"));
            if (!Focused) { Abort(TEXT("no_unblocked_saved_supply_search_position")); break; }
            ExpectedItems = 0;
            ExpectedValue = 0;
            // Teleporting above a saved cache is only a test fixture. Let the
            // CharacterMovement land, then aim again before opening the container.
            Advance(EStep::PrepareSearch);
        }
        break;

    case EStep::PrepareSearch:
        if (StepSeconds > .6f && IsValid(Loot))
        {
            AimAt(Loot->CollisionBox->GetComponentLocation());
            Check(Player->GetFocusedLoot() == Loot, TEXT("landed_player_refocuses_saved_supply"));
            Check(ExecuteAction(TEXT("Interact"), true) && Player->GetOpenLoot() == Loot
                && Player->IsInventoryOpen() && Player->IsSearching() && Player->GetLootCount() == 0,
                TEXT("E_action_opens_real_container_with_automatic_search_without_collection"));
            ContainerRows = Loot->GetContents().Num();
            ExpectedItems = 0;
            ExpectedValue = 0;
            for (const FRaidInventoryEntry& Item : Loot->GetContents())
            {
                ExpectedItems += Item.Quantity;
                ExpectedValue += static_cast<int64>(Item.Quantity) * Item.Definition.UnitValue;
            }
            Check(ContainerRows >= 2 && ContainerRows <= 3 && ExpectedItems <= Player->MaxLootItems
                && !Loot->GetContents().ContainsByPredicate([](const FRaidInventoryEntry& Item) { return Item.bRevealed; }),
                TEXT("saved_container_has_multiple_unknown_items_before_search"));
            Advance(EStep::ContainerOpen);
        }
        else if (StepSeconds > 3.f) Abort(TEXT("saved_container_missing_before_open"));
        break;

    case EStep::ContainerOpen:
        if (!Issued)
        {
            CheckInputs(false);
            BeforePosition = Player->GetActorLocation();
            BeforeView = PC->GetControlRotation();
            BeforeAmmo = Player->MagazineAmmo;
            Player->Fire(); Player->StartFire(); Player->Reload(); Player->StartAim();
            Player->MoveForward(1.f); Player->MoveRight(1.f); Player->Turn(30.f); Player->LookUp(20.f);
            Issued = true;
        }
        if (StepSeconds > .25f)
        {
            Check(Player->MagazineAmmo == BeforeAmmo && !Player->bReloading && !Player->bAiming,
                TEXT("container_panel_blocks_weapon_actions"));
            Check(FVector::Dist2D(BeforePosition, Player->GetActorLocation()) < .5f
                && PC->GetControlRotation().Equals(BeforeView, .05f), TEXT("container_panel_blocks_movement_and_view"));
            Check(Player->GetLootCount() == 0 && Player->GetTotalLootValue() == 0 && Player->IsSearching(),
                TEXT("opening_container_searches_automatically_without_auto_pickup"));
            Capture(TEXT("03a_container_unknown"), EStep::SearchStart);
        }
        break;

    case EStep::SearchStart:
        if (StepSeconds > .2f)
        {
            Check(Player->IsSearching() && Player->GetSearchIndex() == 0,
                TEXT("automatic_search_started_real_per_item_timer_without_extra_click"));
            Advance(EStep::SearchPartial);
        }
        break;

    case EStep::SearchPartial:
        if (IsValid(Loot) && Loot->GetContents().Num() > 1 && Loot->GetContents()[0].bRevealed
            && Player->IsSearching() && Player->GetSearchIndex() == 1 && Player->GetSearchProgress() > .15f)
        {
            InterruptedSearchIndex = Player->GetSearchIndex();
            const float Progress = Player->GetSearchProgress();
            Check(Progress < .9f && !Loot->GetContents()[InterruptedSearchIndex].bRevealed
                && Player->GetLootCount() == 0 && Player->GetTotalLootValue() == 0,
                TEXT("search_reveals_first_item_and_advances_to_next_without_auto_take"));
            if (!ClickButton(TEXT("ContainerSearch"))) { Abort(TEXT("container_cancel_search_button_failed")); break; }
            Check(!Player->IsSearching() && Player->GetSearchIndex() == INDEX_NONE
                && FMath::IsNearlyZero(Player->GetSearchProgress()) && Loot->GetContents()[0].bRevealed
                && !Loot->GetContents()[InterruptedSearchIndex].bRevealed,
                TEXT("canvas_cancel_discards_current_progress_and_preserves_revealed_item"));
            Advance(EStep::SearchCancelled);
        }
        else if (StepSeconds > 5.f) Abort(TEXT("real_per_item_search_did_not_reach_interrupt_point"));
        break;

    case EStep::SearchCancelled:
        if (StepSeconds > Loot->SearchDuration + .2f)
        {
            Check(!Player->IsSearching() && Loot->FindNextUnsearchedIndex() == InterruptedSearchIndex
                && Loot->GetContents()[0].bRevealed && Player->GetLootCount() == 0,
                TEXT("cancelled_item_stays_unknown_after_original_timer_expires"));
            Check(ExecuteAction(TEXT("Quit"), true) && !Player->IsInventoryOpen() && !Player->GetOpenLoot()
                && Mode->State == ERaidState::Running, TEXT("Escape_closes_container_without_abandoning_raid"));
            CheckInputs(true);
            AimAt(Loot->CollisionBox->GetComponentLocation());
            Check(ExecuteAction(TEXT("Interact"), true) && Player->GetOpenLoot() == Loot
                && Loot->GetContents()[0].bRevealed && Loot->FindNextUnsearchedIndex() == InterruptedSearchIndex,
                TEXT("E_reopen_retains_previous_reveal_and_unsearched_next_item"));
            SearchReopenedAt = GetWorld()->GetTimeSeconds();
            Advance(EStep::SearchResume);
        }
        break;

    case EStep::SearchResume:
        if (StepSeconds > .2f)
        {
            Check(Player->IsSearching() && Player->GetSearchIndex() == InterruptedSearchIndex
                && Player->GetSearchProgress() < .5f, TEXT("reopen_automatically_restarts_only_unfinished_item_timer"));
            Advance(EStep::Search);
        }
        break;

    case EStep::Search:
        if (IsValid(Loot) && Loot->FindNextUnsearchedIndex() == INDEX_NONE && !Player->IsSearching())
        {
            Check(GetWorld()->GetTimeSeconds() - SearchReopenedAt >= (ContainerRows - InterruptedSearchIndex) * Loot->SearchDuration - .1f,
                TEXT("search_waited_for_all_remaining_gameplay_timers"));
            Check(Player->GetLootCount() == 0 && Player->GetTotalLootValue() == 0 && Player->GetInventoryComponent()->GetEntries().IsEmpty(),
                TEXT("completed_search_reveals_contents_but_backpack_is_still_empty"));
            Check(IsValid(Loot) && !Loot->IsActorBeingDestroyed() && Loot->GetContents().Num() == ContainerRows
                && !Loot->GetContents().ContainsByPredicate([](const FRaidInventoryEntry& Item)
                    { return !Item.bRevealed || Item.Quantity <= 0; }),
                TEXT("searched_saved_supply_preserves_all_revealed_contents"));
            Capture(TEXT("03b_container_revealed"), EStep::Take);
        }
        else if (StepSeconds > 8.f) Abort(TEXT("real_search_did_not_complete"),
            FString::Printf(TEXT("searching=%d open=%s position=%s items=%d"), Player->IsSearching(),
                *GetNameSafe(Player->GetOpenLoot()), *Player->GetActorLocation().ToString(), Player->GetLootCount()));
        break;

    case EStep::Take:
        if (StepSeconds > .25f)
        {
            int32 NextItem = INDEX_NONE;
            for (int32 Index = 0; Index < Loot->GetContents().Num(); ++Index)
                if (Loot->GetContents()[Index].Quantity > 0) { NextItem = Index; break; }
            if (NextItem != INDEX_NONE)
            {
                const int32 PreviousQuantity = Loot->GetContents()[NextItem].Quantity;
                const int32 PreviousItems = Player->GetLootCount();
                const int64 PreviousValue = Player->GetTotalLootValue();
                const int32 UnitValue = Loot->GetContents()[NextItem].Definition.UnitValue;
                const FName Button(*FString::Printf(TEXT("Take_%d"), NextItem));
                if (!ClickButton(Button)) { Abort(TEXT("container_take_button_failed"), Button.ToString()); break; }
                Check(Loot->GetContents()[NextItem].Quantity == PreviousQuantity - 1
                    && Player->GetLootCount() == PreviousItems + 1 && Player->GetTotalLootValue() == PreviousValue + UnitValue,
                    TEXT("canvas_take_transfers_exactly_one_revealed_item"), Button.ToString());
                Advance(EStep::Take); // Let DrawHUD rebuild hitboxes before taking the next stable row.
            }
            else
            {
                Check(Player->GetLootCount() == ExpectedItems && Player->GetTotalLootValue() == ExpectedValue
                    && Player->GetInventoryComponent()->GetEntries().Num() > 0, TEXT("manual_take_updates_backpack_and_exact_total_value"));
                Check(IsValid(Loot) && !Loot->IsActorBeingDestroyed() && Player->GetOpenLoot() == Loot
                    && Loot->GetContents().Num() == ContainerRows,
                    TEXT("fully_taken_saved_container_remains_in_world_with_stable_empty_rows"));
                Capture(TEXT("03c_container_empty"), EStep::ContainerEmpty);
            }
        }
        break;

    case EStep::ContainerEmpty:
        if (StepSeconds > .2f)
        {
            if (!ClickButton(TEXT("ContainerClose"))) { Abort(TEXT("container_close_button_failed")); break; }
            Check(!Player->IsInventoryOpen() && !Player->GetOpenLoot(), TEXT("canvas_close_exits_container_panel"));
            CheckInputs(true);
            AimAt(Loot->CollisionBox->GetComponentLocation());
            Check(ExecuteAction(TEXT("Interact"), true) && Player->GetOpenLoot() == Loot,
                TEXT("empty_saved_container_can_be_reopened_with_E"));
            Check(Loot->GetContents().Num() == ContainerRows
                && !Loot->GetContents().ContainsByPredicate([](const FRaidInventoryEntry& Item) { return Item.Quantity > 0; })
                && Player->GetLootCount() == ExpectedItems && Player->GetTotalLootValue() == ExpectedValue,
                TEXT("empty_container_revisit_does_not_regenerate_contents"));
            Advance(EStep::ContainerRevisit);
        }
        break;

    case EStep::ContainerRevisit:
        if (StepSeconds > .2f)
        {
            FVector2D HiddenPoint;
            Check(!FindButtonPoint(TEXT("Take_0"), HiddenPoint), TEXT("empty_container_has_no_stale_take_button"));
            if (!ClickButton(TEXT("ContainerClose"))) { Abort(TEXT("empty_revisit_close_button_failed")); break; }
            CheckInputs(true);
            PlacePlayer(InsidePosition);
            AimAt(Zone->GetActorLocation() + FVector(228, 0, 155));
            Advance(EStep::PartialExtraction);
        }
        break;
    case EStep::PartialExtraction:
        if (StepSeconds > .8f)
        {
            Check(Zone->IsPlayerInside() && Zone->GetSecondsRemaining() < 7.5f
                && Zone->GetSecondsRemaining() > 5.f && Mode->IsRaidActive(), TEXT("real_zone_countdown_runs_with_loot"));
            PlacePlayer(InsidePosition + Zone->Trigger->GetForwardVector() * 650.f);
            Advance(EStep::LeaveReset);
        }
        break;

    case EStep::LeaveReset:
        if (StepSeconds > .25f)
        {
            Check(!Zone->IsPlayerInside() && FMath::IsNearlyEqual(Zone->GetSecondsRemaining(), 8.f),
                TEXT("leaving_real_zone_resets_countdown"));
            Check(Mode->IsRaidActive() && Mode->SecuredItems == 0, TEXT("leaving_zone_cannot_complete_result"));
            PlacePlayer(InsidePosition);
            Advance(EStep::DamageCountdown);
        }
        break;

    case EStep::DamageCountdown:
        if (StepSeconds > .8f)
        {
            Check(Zone->GetSecondsRemaining() < 7.5f, TEXT("reentry_restarts_countdown"));
            BeforeHealth = Player->Health;
            const float Applied = UGameplayStatics::ApplyDamage(Player, 10.f, nullptr, this, UDamageType::StaticClass());
            Check(FMath::IsNearlyEqual(Applied, 10.f) && FMath::IsNearlyEqual(Player->Health, BeforeHealth - 10.f),
                TEXT("real_damage_applies_during_countdown"));
            CountdownStart = GetWorld()->GetTimeSeconds();
            SawDamageReset = false;
            Advance(EStep::DamageReset);
        }
        break;

    case EStep::DamageReset:
        SawDamageReset |= Zone->GetSecondsRemaining() >= 7.98f;
        if (StepSeconds > .25f)
        {
            Check(SawDamageReset && Zone->GetSecondsRemaining() > 7.4f && Mode->IsRaidActive(),
                TEXT("taking_damage_resets_real_extraction_countdown"));
            Capture(TEXT("04_extraction_countdown"), EStep::CompleteExtraction);
        }
        break;

    case EStep::CompleteExtraction:
        if (Mode->State == ERaidState::Extracted)
        {
            Check(GetWorld()->GetTimeSeconds() - CountdownStart >= 7.8f, TEXT("success_waited_full_real_8_second_zone_timer"));
            Check(Zone->GetSecondsRemaining() <= KINDA_SMALL_NUMBER && Zone->IsPlayerInside(), TEXT("zone_actually_completed_while_inside"));
            Check(Mode->SecuredItems == ExpectedItems && Mode->SecuredValue == ExpectedValue,
                TEXT("successful_result_secures_actual_collected_inventory"));
            CheckInputs(false);
            BeforeAmmo = Player->MagazineAmmo;
            Player->Fire(); Player->StartFire(); Player->BeginSearch(); Player->StartAim();
            Check(Player->MagazineAmmo == BeforeAmmo && !Player->bAiming && !Player->IsSearching(),
                TEXT("success_result_stops_and_blocks_combat_actions"));
            Capture(TEXT("05_extracted_success"), EStep::Success);
        }
        else if (StepSeconds > 12.f) Abort(TEXT("real_extraction_did_not_finish"));
        break;

    case EStep::Success:
        if (StepSeconds > .4f) Travel(TEXT("Menu"), 2);
        break;

    case EStep::Death:
        if (!Issued)
        {
            FRaidItemDefinition UnsecuredDefinition;
            UnsecuredDefinition.ItemId = TEXT("FlowUnsecured");
            UnsecuredDefinition.DisplayName = TEXT("未保全测试物资");
            UnsecuredDefinition.UnitValue = 75;
            UnsecuredDefinition.UseEffect = ERaidItemUseEffect::None;
            UnsecuredDefinition.UseAmount = 0;
            URaidInventoryComponent* Inventory = Player->GetInventoryComponent();
            Check(Inventory && Inventory->Grant(UnsecuredDefinition, 1, Inventory->GetRevision()).IsSuccess(),
                TEXT("death_fixture_has_unsecured_cargo"));
            const float Applied = UGameplayStatics::ApplyDamage(Player, 999.f, nullptr, this, UDamageType::StaticClass());
            Check(Applied > 0.f && !Player->IsAlive() && Mode->State == ERaidState::Dead, TEXT("real_ApplyDamage_causes_death_result"));
            Check(Mode->SecuredItems == 0 && Mode->SecuredValue == 0, TEXT("death_does_not_secure_inventory"));
            CheckInputs(false);
            Issued = true;
        }
        if (StepSeconds > .5f) Capture(TEXT("06_dead_failure"), EStep::Restart);
        break;

    case EStep::Restart:
        if (StepSeconds > .4f) Travel(TEXT("Restart"), 4);
        break;

    case EStep::Abandon:
        Mode->HandleBack();
        Check(Mode->IsPauseMenuOpen() && Mode->IsRaidActive(), TEXT("back_opens_safe_pause_menu"));
        Mode->HandleMenuAction(TEXT("Abandon"));
        Mode->HandleMenuAction(TEXT("ConfirmAbandon"));
        Check(Mode->State == ERaidState::Dead && Player->IsAlive(), TEXT("confirmed_abandon_without_fake_damage"));
        Check(Mode->SecuredItems == 0 && Mode->SecuredValue == 0, TEXT("abandon_does_not_secure_cargo"));
        CheckInputs(false);
        Advance(EStep::Abandoned);
        break;

    case EStep::Abandoned:
        if (StepSeconds > .5f) Capture(TEXT("09_abandoned"), EStep::ReturnMenu);
        break;

    case EStep::ReturnMenu:
        if (StepSeconds > .4f)
        {
            Memory().PreviousWorld = GetWorld(); Memory().PreviousPlayer = Player;
            for (TActorIterator<ARaidEnemy> It(GetWorld()); It; ++It) { Memory().PreviousEnemy = *It; break; }
            for (TActorIterator<ARaidLoot> It(GetWorld()); It; ++It) { Memory().PreviousLoot = *It; break; }
            Memory().Phase = 5;
            Advance(EStep::AwaitTravel);
            Mode->HandleBack();
        }
        break;

    case EStep::Screenshot:
    {
        const int64 Size = IFileManager::Get().FileSize(*ScreenshotPath);
        ScreenshotStableSeconds = Size > 128 && Size == LastScreenshotSize ? ScreenshotStableSeconds + DeltaSeconds : 0.f;
        LastScreenshotSize = Size;
        if (ScreenshotStableSeconds > .2f && !FScreenshotRequest::IsScreenshotRequested())
        {
            ++Memory().Captures;
            Check(true, TEXT("screenshot_written_and_stable"), ScreenshotPath);
            Advance(AfterCapture);
        }
        else if (StepSeconds > 12.f) Abort(TEXT("screenshot_write_timeout"), ScreenshotPath);
        break;
    }
    case EStep::AwaitTravel:
        if (StepSeconds > 15.f) Abort(TEXT("OpenLevel_did_not_create_new_world"));
        break;
    case EStep::Finish:
        if (StepSeconds > .3f) FinishAndQuit();
        break;
    }
}
