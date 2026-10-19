#include "RaidR8TestDirector.h"
#include "RaidGameMode.h"
#include "RaidPlayer.h"
#include "RaidHUD.h"
#include "RaidEnemy.h"
#include "RaidExtractionZone.h"
#include "RaidInventoryComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace { int32 R8Round = 0, R8Checks = 0, R8Failures = 0; double R8Started = 0; }

ARaidR8TestDirector::ARaidR8TestDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}
void ARaidR8TestDirector::BeginPlay()
{
    Super::BeginPlay();
    if (!FParse::Param(FCommandLine::Get(), TEXT("RaidR8Test"))) { Destroy(); return; }
    if (!R8Started) R8Started = FPlatformTime::Seconds();
    FParse::Value(FCommandLine::Get(), TEXT("RaidR8Output="), Output);
    if (Output.IsEmpty()) Output = FPaths::ProjectDir() / TEXT("Evidence/R8UX");
    IFileManager::Get().MakeDirectory(*Output, true);
    Next = FPlatformTime::Seconds() + 2;
    UE_LOG(LogTemp, Display, TEXT("R8_UX: round=%d; simulated UE key events and Canvas hitbox dispatch; AI unchanged; grant/teleport extraction fixture; hardware NOT tested"), R8Round);
}
void ARaidR8TestDirector::Check(bool Passed, const TCHAR* Name)
{
    ++R8Checks; if (!Passed) ++R8Failures;
    UE_LOG(LogTemp, Display, TEXT("R8_UX_CHECK: %s %s round=%d step=%d"), Passed ? TEXT("PASS") : TEXT("FAIL"), Name, R8Round, Step);
}
void ARaidR8TestDirector::Tap(FKey Key)
{
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.f));
    PendingKey = Key; ReleaseAt = FPlatformTime::Seconds() + .06;
}
void ARaidR8TestDirector::Click(FName Name)
{
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    auto* HUD = Cast<ARaidHUD>(PC->GetHUD());
    const FHUDHitBox* Box = HUD->GetHitBoxWithName(Name);
    Check(Box != nullptr, *FString::Printf(TEXT("visible_button_%s"), *Name.ToString()));
    if (!Box) return;
    int32 W, H; PC->GetViewportSize(W, H);
    for (int32 Y=4; Y<H; Y+=8) for (int32 X=4; X<W; X+=8)
        if (Box->Contains(FVector2D(X,Y)))
        {
            Check(HUD->UpdateAndDispatchHitBoxClickEvents(FVector2D(X,Y), IE_Pressed), TEXT("canvas_dispatch"));
            return;
        }
}
void ARaidR8TestDirector::Capture(const TCHAR* Name)
{
    FScreenshotRequest::RequestScreenshot(Output / (FString(Name) + TEXT(".png")), true, false);
}
void ARaidR8TestDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double Now = FPlatformTime::Seconds();
    auto* PC = UGameplayStatics::GetPlayerController(this, 0);
    auto* P = Cast<ARaidPlayer>(PC ? PC->GetPawn() : nullptr);
    auto* M = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    auto* H = Cast<ARaidHUD>(PC ? PC->GetHUD() : nullptr);
    if (Now - R8Started > 100) { Check(false,TEXT("watchdog")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    if (!PC || !P || !M || !H) return;
    if (PendingKey.IsValid() && Now >= ReleaseAt)
    {
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(PendingKey, IE_Released, 0.f));
        PendingKey = FKey();
    }
    if (Now < Next) return;
    Next = Now + .6;
    if (R8Round == 0)
    {
        if (Step++ == 0) { Check(M->State==ERaidState::MainMenu,TEXT("default_frontier_menu")); Capture(TEXT("01_menu")); }
        else { R8Round=1; Click(TEXT("Start")); }
        return;
    }
    if (Step == 0)
    {
        Check(M->IsRaidActive() && !M->IsPauseMenuOpen() && !UGameplayStatics::IsGamePaused(this),TEXT("new_world_not_paused"));
        Check(P->Health==100 && P->MagazineAmmo==12 && P->ReserveAmmo==60 && P->GetLootCount()==0 && P->DeathCause.IsEmpty(),TEXT("fresh_health_ammo_inventory_death_reason"));
    }
    if (R8Round == 2)
    {
        switch (Step++)
        {
        case 0:
            UGameplayStatics::ApplyDamage(P,5,nullptr,this,nullptr);
            { FRaidItemDefinition Med; Med.ItemId=TEXT("R8MedFixture"); Med.DisplayName=TEXT("医疗包"); Med.UseEffect=ERaidItemUseEffect::Heal; Med.UseAmount=45;
              Check(P->GetInventoryComponent()->Grant(Med,2,P->GetInventoryComponent()->GetRevision()).IsSuccess(),TEXT("explicit_medical_fixture_grant")); }
            Tap(EKeys::Q); Next=Now+3.4; break;
        case 1:
            Check(P->Health==100 && P->GetLootCount()==1 && M->Message.Contains(TEXT("5 点生命")),TEXT("heal_reports_actual_five_health_and_consumes_one"));
            Capture(TEXT("07_actual_healing")); Tap(EKeys::Q); break;
        case 2:
            Check(P->Health==100 && P->GetLootCount()==1,TEXT("full_health_does_not_consume_medkit"));
            UGameplayStatics::ApplyDamage(P,999,nullptr,this,nullptr); break;
        case 3:
            Check(M->State==ERaidState::Dead && !P->IsAlive() && P->DeathCause.Contains(TEXT("其他来源")),TEXT("unknown_cause_is_not_falsely_labelled_enemy"));
            Capture(TEXT("08_death")); break;
        case 4: R8Round=3; Tap(EKeys::Enter); break;
        }
        return;
    }
    if (R8Round == 3)
    {
        if (Step++ == 0) { Capture(TEXT("09_fresh_after_death")); }
        else if (Step==2) Tap(EKeys::Escape);
        else if (Step==3) { Check(M->IsPauseMenuOpen(),TEXT("pause_after_second_restart")); Tap(EKeys::Enter); }
        else if (Step==4)
        {
            Check(!M->IsPauseMenuOpen() && P->CanAct(),TEXT("resume_after_second_restart"));
            Tap(EKeys::SpaceBar); Next=Now+.12;
        }
        else if (Step==5)
        {
            Check(P->GetVelocity().Z > 100.f,TEXT("actual_jump_is_rising_before_pause"));
            Tap(EKeys::Escape);
        }
        else if (Step==6)
        {
            Check(M->IsPauseMenuOpen() && P->GetVelocity().Z > 100.f,TEXT("pause_preserves_rising_jump_velocity"));
            PausedJumpLocation=P->GetActorLocation(); PausedJumpVelocity=P->GetVelocity(); Next=Now+1.2;
        }
        else if (Step==7)
        {
            Check(P->GetActorLocation().Equals(PausedJumpLocation,.01f) && P->GetVelocity().Equals(PausedJumpVelocity,.01f),TEXT("paused_jump_position_and_velocity_frozen"));
            Tap(EKeys::Enter); Next=Now+.12;
        }
        else
        {
            Check(!M->IsPauseMenuOpen() && P->CanAct(),TEXT("resume_after_second_restart"));
            Check(P->GetActorLocation().Z > PausedJumpLocation.Z + 1.f,TEXT("jump_continues_rising_after_resume"));
            UE_LOG(LogTemp, Display, TEXT("R8_UX_RESULT: %s checks=%d failures=%d raids=3"),R8Failures?TEXT("FAIL"):TEXT("PASS"),R8Checks,R8Failures);
            FPlatformMisc::RequestExitWithStatus(false,R8Failures?1:0);
        }
        return;
    }
    switch (Step++)
    {
    case 0: Capture(TEXT("02_first_step")); Tap(EKeys::LeftMouseButton); break;
    case 1: Check(P->MagazineAmmo < 12,TEXT("simulated_fire_reaches_weapon")); Tap(EKeys::R); Next=Now+.3; break;
    case 2: Check(P->bReloading,TEXT("reload_started")); Tap(EKeys::Escape); break;
    case 3:
        Check(M->IsPauseMenuOpen() && UGameplayStatics::IsGamePaused(this) && M->IsRaidActive(),TEXT("escape_pauses_without_abandon"));
        Check(PC->bShowMouseCursor && PC->IsMoveInputIgnored() && PC->IsLookInputIgnored(),TEXT("pause_cursor_and_input"));
        Clock=M->RaidElapsed; Progress=P->ReloadProgress; Ammo=P->MagazineAmmo; Health=P->Health;
        for(TActorIterator<ARaidEnemy> It(GetWorld());It;++It) GuardPositions.Add(*It,It->GetActorLocation());
        Capture(TEXT("03_pause")); Tap(EKeys::LeftMouseButton); Next=Now+2; break;
    case 4:
        Check(M->RaidElapsed==Clock && P->ReloadProgress==Progress && P->MagazineAmmo==Ammo && P->Health==Health,TEXT("two_seconds_pause_freezes_clock_reload_ammo_health"));
        for(auto& Guard:GuardPositions) Check(Guard.Key.IsValid() && Guard.Key->GetActorLocation().Equals(Guard.Value,.01),TEXT("guard_frozen_by_world_pause"));
        Tap(EKeys::Enter); break;
    case 5: Check(!M->IsPauseMenuOpen() && !UGameplayStatics::IsGamePaused(this) && !PC->bShowMouseCursor,TEXT("enter_resumes_input")); Next=Now+1.7; break;
    case 6: Check(!P->bReloading && P->MagazineAmmo==12 && P->ReserveAmmo==60-(12-Ammo),TEXT("reload_completes_once_after_resume_no_extra_shot")); Tap(EKeys::Tab); break;
    case 7: Check(P->IsInventoryOpen(),TEXT("tab_opens_bag")); Tap(EKeys::Escape); break;
    case 8: Check(!P->IsInventoryOpen() && !M->IsPauseMenuOpen(),TEXT("escape_closes_bag_first")); Tap(EKeys::M); break;
    case 9: Check(H->IsTacticalMapOpen(),TEXT("map_opens")); Tap(EKeys::Escape); break;
    case 10:
        Check(!H->IsTacticalMapOpen() && !M->IsPauseMenuOpen(),TEXT("escape_closes_map_first"));
        { FRaidItemDefinition Item; Item.ItemId=TEXT("R8Fixture"); Item.DisplayName=TEXT("测试物资"); Item.UnitValue=75;
          Check(P->GetInventoryComponent()->Grant(Item,1,P->GetInventoryComponent()->GetRevision()).IsSuccess(),TEXT("explicit_extraction_fixture_grant")); }
        Capture(TEXT("04_carry_step")); break;
    case 11:
        for(TActorIterator<ARaidExtractionZone> It(GetWorld());It;++It) if(It->GetExtractionId()==TEXT("S-01"))Zone=*It;
        Check(Zone.IsValid(),TEXT("south_exit_present"));
        if(Zone.IsValid())P->SetActorLocation(Zone->GetActorLocation()+FVector(0,0,90),false,nullptr,ETeleportType::TeleportPhysics);
        break;
    case 12: Check(Zone.IsValid()&&Zone->IsPlayerInside()&&Zone->GetSecondsRemaining()<8,TEXT("real_exit_countdown_started")); Tap(EKeys::Escape); break;
    case 13: Extraction=Zone.IsValid()?Zone->GetSecondsRemaining():-1; Next=Now+2; break;
    case 14: Check(Zone.IsValid()&&Zone->GetSecondsRemaining()==Extraction&&M->IsRaidActive(),TEXT("paused_extraction_cannot_complete")); Click(TEXT("Resume")); break;
    case 15: Check(Zone.IsValid()&&Zone->GetSecondsRemaining()<Extraction&&!M->IsPauseMenuOpen(),TEXT("canvas_resume_continues_extraction")); Tap(EKeys::Escape); break;
    case 16: Click(TEXT("Abandon")); break;
    case 17: Check(M->IsAbandonConfirmationOpen()&&M->IsRaidActive(),TEXT("abandon_requires_confirmation")); Capture(TEXT("05_confirm")); Tap(EKeys::Escape); break;
    case 18: Check(!M->IsAbandonConfirmationOpen()&&M->IsPauseMenuOpen(),TEXT("escape_cancels_confirmation")); Click(TEXT("Abandon")); break;
    case 19: Click(TEXT("CancelAbandon")); break;
    case 20: Check(!M->IsAbandonConfirmationOpen()&&M->IsPauseMenuOpen(),TEXT("canvas_cancel_confirmation")); Click(TEXT("Abandon")); break;
    case 21: Click(TEXT("ConfirmAbandon")); break;
    case 22: Check(M->State==ERaidState::Dead&&P->IsAlive()&&!UGameplayStatics::IsGamePaused(this)&&M->SecuredItems==0,TEXT("confirmed_abandon_unpauses_and_loses_cargo")); Capture(TEXT("06_abandoned")); break;
    case 23: R8Round=2; Tap(EKeys::Enter); break;
    }
}
