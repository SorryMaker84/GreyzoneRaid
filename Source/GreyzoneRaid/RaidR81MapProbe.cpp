#include "RaidR81MapProbe.h"
#include "RaidGameMode.h"
#include "RaidPlayer.h"
#include "RaidHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace { int32 MapRound=0, Checks=0, Failures=0; double Started=0; }
ARaidR81MapProbe::ARaidR81MapProbe()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    PrimaryActorTick.TickGroup=TG_PostPhysics;
}
void ARaidR81MapProbe::BeginPlay()
{
    Super::BeginPlay();
    if(!Started)Started=FPlatformTime::Seconds();
    Next=FPlatformTime::Seconds()+2;
    FParse::Value(FCommandLine::Get(),TEXT("R81Output="),Output);
    bExpectFixed=FParse::Param(FCommandLine::Get(),TEXT("R81ExpectFixed"));
    IFileManager::Get().MakeDirectory(*Output,true);
    UE_LOG(LogTemp,Display,TEXT("R81_MAP: simulated UE key events; physical input NOT tested; AI unchanged; controlled damage fixture for unpaused/death boundaries; round=%d"),MapRound);
}
void ARaidR81MapProbe::Key(FKey InKey,bool Pressed)
{
    UGameplayStatics::GetPlayerController(this,0)->InputKey(FInputKeyEventArgs::CreateSimulated(InKey,Pressed?IE_Pressed:IE_Released,Pressed?1.f:0.f));
}
void ARaidR81MapProbe::Tap(FKey InKey){Key(InKey,true); PendingKey=InKey; ReleaseAt=FPlatformTime::Seconds()+.06;}
void ARaidR81MapProbe::Check(bool Passed,const TCHAR* Name)
{
    ++Checks; if(!Passed)++Failures;
    UE_LOG(LogTemp,Display,TEXT("R81_MAP_CHECK: %s %s round=%d step=%d"),Passed?TEXT("PASS"):TEXT("FAIL"),Name,MapRound,Step);
}
void ARaidR81MapProbe::Observe(bool Desired,const TCHAR* Name)
{
    UE_LOG(LogTemp,Display,TEXT("R81_MAP_OBSERVATION: %s desired=%d"),Name,Desired);
    if(bExpectFixed)Check(Desired,Name);
}
void ARaidR81MapProbe::Capture(const TCHAR* Name)
{
    FScreenshotRequest::RequestScreenshot(Output/(FString(Name)+TEXT(".png")),true,false);
}
void ARaidR81MapProbe::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double Now=FPlatformTime::Seconds();
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    auto* P=Cast<ARaidPlayer>(PC?PC->GetPawn():nullptr);
    auto* M=Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    auto* H=Cast<ARaidHUD>(PC?PC->GetHUD():nullptr);
    if(Now-Started>60){Check(false,TEXT("watchdog"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
    if(!P||!M||!H)return;
    if(PendingKey.IsValid()&&Now>=ReleaseAt){Key(PendingKey,false);PendingKey=FKey();}
    if(Now<Next)return;
    Next=Now+.4;
    if(MapRound==1)
    {
        switch(Step++)
        {
        case 0:Check(P->Health==100&&P->MagazineAmmo==12&&!H->IsTacticalMapOpen()&&!M->IsPauseMenuOpen(),TEXT("new_raid_clears_map_and_input_state"));Tap(EKeys::M);break;
        case 1:Tap(EKeys::LeftMouseButton);break;
        case 2:Observe(!H->IsTacticalMapOpen(),TEXT("fire_returns_to_world_after_restart"));Check(P->MagazineAmmo<12,TEXT("fire_works_after_restart"));Capture(TEXT("05_restart_fire"));break;
        case 3:
            UE_LOG(LogTemp,Display,TEXT("R81_MAP_RESULT: %s checks=%d failures=%d raids=2"),Failures?TEXT("FAIL"):(bExpectFixed?TEXT("PASS"):TEXT("BASELINE_OBSERVED")),Checks,Failures);
            FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);break;
        }
        return;
    }
    switch(Step++)
    {
    case 0:Check(M->IsRaidActive(),TEXT("running_frontier"));Key(EKeys::LeftMouseButton,true);Next=Now+.18;break;
    case 1:Check(P->MagazineAmmo<12,TEXT("held_fire_is_live_before_map"));Tap(EKeys::M);Next=Now+.15;break;
    case 2:Check(H->IsTacticalMapOpen(),TEXT("M_opens_map"));Ammo=P->MagazineAmmo;Next=Now+.4;break;
    case 3:
        UE_LOG(LogTemp,Display,TEXT("R81_MAP_AMMO: after_open=%d after_wait=%d"),Ammo,P->MagazineAmmo);
        Observe(P->MagazineAmmo==Ammo,TEXT("opening_map_stops_held_fire"));Key(EKeys::LeftMouseButton,false);Capture(TEXT("01_map_held_fire"));break;
    case 4:Ammo=P->MagazineAmmo;Tap(EKeys::LeftMouseButton);break;
    case 5:Observe(!H->IsTacticalMapOpen(),TEXT("fresh_fire_returns_to_world"));Check(P->MagazineAmmo<Ammo,TEXT("fresh_fire_still_fires"));Capture(TEXT("02_fire_view"));if(H->IsTacticalMapOpen())Tap(EKeys::M);break;
    case 6:Tap(EKeys::M);break;
    case 7:Key(EKeys::RightMouseButton,true);break;
    case 8:Observe(!H->IsTacticalMapOpen(),TEXT("aim_returns_to_world"));Check(P->bAiming,TEXT("aim_still_works"));if(H->IsTacticalMapOpen())Tap(EKeys::M);break;
    case 9:Tap(EKeys::M);break;
    case 10:Observe(!P->bAiming,TEXT("opening_map_releases_held_aim"));Key(EKeys::RightMouseButton,false);Tap(EKeys::E);break;
    case 11:Observe(!H->IsTacticalMapOpen(),TEXT("interaction_returns_to_world"));if(H->IsTacticalMapOpen())Tap(EKeys::M);break;
    case 12:Tap(EKeys::M);break;
    case 13:Check(H->IsTacticalMapOpen(),TEXT("map_reopens_normally"));Health=P->Health;Clock=M->RaidElapsed;UGameplayStatics::ApplyDamage(P,10,nullptr,this,nullptr);Capture(TEXT("03_map_damage"));break;
    case 14:Check(P->Health==Health-10&&M->RaidElapsed>Clock&&!UGameplayStatics::IsGamePaused(this),TEXT("map_does_not_pause_damage_or_clock"));Tap(EKeys::Escape);break;
    case 15:Check(!H->IsTacticalMapOpen()&&!M->IsPauseMenuOpen(),TEXT("escape_closes_map_before_pause"));Tap(EKeys::Escape);break;
    case 16:Check(M->IsPauseMenuOpen(),TEXT("escape_then_pauses"));Ammo=P->MagazineAmmo;Tap(EKeys::M);Key(EKeys::LeftMouseButton,true);break;
    case 17:Key(EKeys::LeftMouseButton,false);Check(!H->IsTacticalMapOpen()&&P->MagazineAmmo==Ammo,TEXT("map_and_fire_blocked_in_pause"));Tap(EKeys::Enter);break;
    case 18:Check(!M->IsPauseMenuOpen(),TEXT("pause_resumes"));Tap(EKeys::M);break;
    case 19:UGameplayStatics::ApplyDamage(P,999,nullptr,this,nullptr);break;
    case 20:Check(M->State==ERaidState::Dead&&!H->IsTacticalMapOpen(),TEXT("death_closes_map"));Capture(TEXT("04_death"));break;
    case 21:MapRound=1;Tap(EKeys::Enter);break;
    }
}
