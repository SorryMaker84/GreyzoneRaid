#include "RaidR9TestDirector.h"
#include "RaidPlayer.h"
#include "RaidPlayerController.h"
#include "RaidGameMode.h"
#include "RaidHUD.h"
#include "RaidArmsAnimInstance.h"
#include "RaidLoot.h"
#include "RaidInventoryComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Sound/SoundBase.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"

ARaidR9TestDirector::ARaidR9TestDirector()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}
void ARaidR9TestDirector::BeginPlay()
{
    Super::BeginPlay();
    Started=FPlatformTime::Seconds();Next=Started+2;
    bBaseline=FParse::Param(FCommandLine::Get(),TEXT("R9Baseline"));
    bMeasuresOnly=FParse::Param(FCommandLine::Get(),TEXT("R9MeasuresOnly"));
    FParse::Value(FCommandLine::Get(),TEXT("R9Output="),Output);
    IFileManager::Get().MakeDirectory(*Output,true);
    Frames=TEXT("phase,world_time,dt,shot_count,ammo,reserve,reloading,ads,control_pitch,recoil_pitch,recoil_yaw,kick,tilt,weapon_x,weapon_z,weapon_pitch,visual_reload,visual_weight,shot_age,searching,search_progress,arms_shot_age,arms_reload,left_wrist_gap_cm,right_wrist_gap_cm\n");
    Shots=TEXT("phase,time,ammo,ads\n");
    UE_LOG(LogTemp,Display,TEXT("R9_PROBE: UE simulated input; silent/offscreen; hardware and listening NOT tested; baseline=%d"),bBaseline);
}
void ARaidR9TestDirector::Add(const TCHAR* Name,double Seconds,TFunction<void()> Begin,TFunction<void()> Verify)
{Steps.Add({Name,Seconds,MoveTemp(Begin),MoveTemp(Verify)});}
void ARaidR9TestDirector::Key(FKey InKey,bool Pressed)
{PC->InputKey(FInputKeyEventArgs::CreateSimulated(InKey,Pressed?IE_Pressed:IE_Released,Pressed?1.f:0.f));}
void ARaidR9TestDirector::Tap(FKey InKey)
{Key(InKey,true);PendingKey=InKey;ReleaseAt=FPlatformTime::Seconds()+.06;}
void ARaidR9TestDirector::Check(bool Passed,const TCHAR* Name)
{
    ++Checks;if(!Passed)++Failures;
    UE_LOG(LogTemp,Display,TEXT("R9_CHECK: %s %s step=%s"),Passed?TEXT("PASS"):TEXT("FAIL"),Name,*Steps[Step].Name);
}
void ARaidR9TestDirector::Capture(const TCHAR* Name)
{FScreenshotRequest::RequestScreenshot(Output/(FString(Name)+TEXT(".png")),true,false);}
void ARaidR9TestDirector::ResetWeapon(int32 Magazine,int32 ReserveAmmo)
{
    P->StopFire();P->CancelReload();P->StopAim();P->StopSprint();P->CloseLoot();
    P->MagazineAmmo=Magazine;P->ReserveAmmo=ReserveAmmo;
    PC->SetControlRotation(FRotator(0,35,0));
    P->RecoilPitch=P->RecoilYaw=P->WeaponKick=P->WeaponTilt=0;
    P->NextAllowedFireTime=0;
}
void ARaidR9TestDirector::BuildMeasurements()
{
    Add(TEXT("hip_hold"),2,[this]{ResetWeapon(120,0);Key(EKeys::LeftMouseButton,true);},[this]{Key(EKeys::LeftMouseButton,false);Capture(TEXT("01_hip_fire"));});
    Add(TEXT("hip_recovery"),1,[this]{},[this]{Check(FMath::Abs(P->RecoilPitch)<.02f,TEXT("camera_returns_after_stop"));});
    Add(TEXT("ads_enter"),.4,[this]{ResetWeapon(120,0);Key(EKeys::RightMouseButton,true);});
    Add(TEXT("ads_hold"),2,[this]{Key(EKeys::LeftMouseButton,true);},[this]{Key(EKeys::LeftMouseButton,false);Capture(TEXT("02_ads_fire"));});
    Add(TEXT("ads_recovery"),1,[this]{},[this]{Key(EKeys::RightMouseButton,false);});
    Add(TEXT("tap_first"),.035,[this]{ResetWeapon(12,0);Key(EKeys::LeftMouseButton,true);},[this]{Key(EKeys::LeftMouseButton,false);});
    Add(TEXT("tap_gap"),.055,[this]{});
    Add(TEXT("tap_early_hold"),.25,[this]{Key(EKeys::LeftMouseButton,true);},[this]{Key(EKeys::LeftMouseButton,false);});
    Add(TEXT("tap_recovery"),.7,[this]{});
}
void ARaidR9TestDirector::NewBox()
{
    P->CloseLoot();if(Box)Box->Destroy();
    const FVector At=P->GetActorLocation()+FVector(145,0,-40);
    const FTransform T(FRotator::ZeroRotator,At);
    Box=GetWorld()->SpawnActorDeferred<ARaidLoot>(ARaidLoot::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Box->bGenerateSupplementalLoot=false;Box->SearchDuration=.8f;
    for(int i=0;i<3;++i){FRaidLootEntry E;E.ItemId=TEXT("Tech");E.DisplayName=TEXT("电子元件");E.Quantity=i==0?2:1;E.Value=45;Box->Contents.Add(E);}
    Box->FinishSpawning(T);AimBox();
}
void ARaidR9TestDirector::AimBox()
{
    const FRotator View=(Box->GetActorLocation()-P->Camera->GetComponentLocation()).Rotation();
    PC->SetControlRotation(View);P->Camera->SetWorldRotation(View);
}
bool ARaidR9TestDirector::Click(FName Button)
{
    ARaidHUD* H=Cast<ARaidHUD>(PC->GetHUD());const FHUDHitBox* Hit=H?H->GetHitBoxWithName(Button):nullptr;
    if(!Hit)return false;int32 W=0,V=0;PC->GetViewportSize(W,V);
    for(int32 Y=4;Y<V;Y+=8)for(int32 X=4;X<W;X+=8)
    {FVector2D Pt(X,Y);if(Hit->Contains(Pt)&&H->GetHitBoxAtCoordinates(Pt,true)==Hit)return H->UpdateAndDispatchHitBoxClickEvents(Pt,IE_Pressed);}
    return false;
}
void ARaidR9TestDirector::BuildRegression()
{
    Add(TEXT("last_round_release"),.3,[this]{ResetWeapon(1,24);StartCount=P->SuccessfulShots;Tap(EKeys::LeftMouseButton);},[this]{
        Check(P->MagazineAmmo==0&&P->bReloading&&P->bAutomaticReload,TEXT("last_round_automatically_starts_reload"));
        Check(P->ReserveAmmo==24&&P->SuccessfulShots==StartCount+1,TEXT("reload_start_does_not_credit_ammo"));Capture(TEXT("03_auto_reload_begin"));Clock=P->ReloadStartedAt;});
    Add(TEXT("duplicate_reload"),.25,[this]{Tap(EKeys::R);},[this]{Check(P->ReloadStartedAt==Clock,TEXT("manual_R_during_auto_does_not_restart_timer"));Capture(TEXT("04_auto_reload_hands"));});
    Add(TEXT("released_reload_finish"),1.4,[this]{},[this]{
        Check(!P->bReloading&&P->MagazineAmmo==12&&P->ReserveAmmo==12,TEXT("released_trigger_loads_once_and_stays_idle"));
        Check(P->SuccessfulShots==StartCount+1,TEXT("release_during_reload_prevents_resume"));Capture(TEXT("05_auto_reload_complete"));});
    Add(TEXT("held_last_round"),2.25,[this]{ResetWeapon(1,24);StartCount=P->SuccessfulShots;Key(EKeys::LeftMouseButton,true);},[this]{
        Check(P->SuccessfulShots>StartCount+1,TEXT("still_held_trigger_resumes_after_auto_reload"));
        Check(P->MagazineAmmo+P->ReserveAmmo+P->SuccessfulShots-StartCount==25,TEXT("auto_resume_conserves_ammunition"));Key(EKeys::LeftMouseButton,false);Ammo=P->MagazineAmmo;});
    Add(TEXT("release_stops_resume"),.4,[this]{},[this]{Check(P->MagazineAmmo==Ammo,TEXT("release_clears_scheduled_shot"));});
    Add(TEXT("empty_attempt"),.25,[this]{ResetWeapon(0,5);Tap(EKeys::LeftMouseButton);},[this]{Check(P->bReloading&&P->MagazineAmmo==0&&P->ReserveAmmo==5,TEXT("empty_attempt_starts_auto_reload_with_partial_reserve"));});
    Add(TEXT("partial_reserve_finish"),1.5,[this]{},[this]{Check(P->MagazineAmmo==5&&P->ReserveAmmo==0&&!P->bReloading,TEXT("partial_reserve_transferred_exactly"));});
    Add(TEXT("no_reserve"),.5,[this]{ResetWeapon(0,0);StartCount=P->SuccessfulShots;Key(EKeys::LeftMouseButton,true);},[this]{
        Check(!P->bReloading&&P->MagazineAmmo==0&&P->ReserveAmmo==0,TEXT("no_reserve_does_not_loop_reload"));
        Check(P->SuccessfulShots==StartCount&&!P->bFireHeld,TEXT("dry_fire_has_no_shot_or_held_timer_loop"));Key(EKeys::LeftMouseButton,false);});
    Add(TEXT("manual_partial"),.35,[this]{ResetWeapon(7,3);Tap(EKeys::R);},[this]{Check(P->bReloading&&!P->bAutomaticReload,TEXT("manual_partial_reload_starts"));Clock=P->ReloadStartedAt;});
    Add(TEXT("manual_duplicate"),.25,[this]{Tap(EKeys::R);},[this]{Check(P->ReloadStartedAt==Clock,TEXT("duplicate_manual_R_does_not_restart_timer"));});
    Add(TEXT("manual_finish"),1.2,[this]{},[this]{Check(P->MagazineAmmo==10&&P->ReserveAmmo==0&&!P->bReloading,TEXT("manual_reload_conserves_partial_reserve"));});
    Add(TEXT("press_during_reload"),.3,[this]{ResetWeapon(5,12);Tap(EKeys::R);StartCount=P->SuccessfulShots;},[this]{Key(EKeys::LeftMouseButton,true);});
    Add(TEXT("held_manual_finish"),1.6,[this]{},[this]{Check(P->SuccessfulShots>StartCount&&P->ReserveAmmo==5,TEXT("press_while_reloading_queues_only_held_intent"));Key(EKeys::LeftMouseButton,false);});
    Add(TEXT("ads_auto_setup"),.35,[this]{ResetWeapon(1,12);Key(EKeys::RightMouseButton,true);});
    Add(TEXT("ads_auto_shot"),.3,[this]{Tap(EKeys::LeftMouseButton);},[this]{Check(P->bReloading&&!P->bAiming,TEXT("reload_lowers_ADS_without_clearing_held_aim"));});
    Add(TEXT("ads_auto_finish"),1.65,[this]{},[this]{Check(P->bAiming&&P->ADSAlpha>.9f&&P->MagazineAmmo==12,TEXT("held_ADS_returns_after_reload_without_firing"));Key(EKeys::RightMouseButton,false);});
    Add(TEXT("sprint_auto_setup"),.25,[this]{ResetWeapon(1,12);Key(EKeys::LeftMouseButton,true);});
    Add(TEXT("sprint_clears_trigger"),1.9,[this]{Key(EKeys::LeftShift,true);},[this]{Check(P->MagazineAmmo==12&&!P->bFireHeld&&!P->bReloading,TEXT("sprint_clears_resume_intent_but_reload_finishes"));Key(EKeys::LeftShift,false);});
    Add(TEXT("map_auto_setup"),.25,[this]{ResetWeapon(1,12);Key(EKeys::LeftMouseButton,false);Key(EKeys::LeftMouseButton,true);});
    Add(TEXT("map_auto_finish"),1.9,[this]{Tap(EKeys::M);},[this]{Check(P->MagazineAmmo==12&&!P->bFireHeld&&Cast<ARaidHUD>(PC->GetHUD())->IsTacticalMapOpen(),TEXT("map_keeps_reload_but_stops_resume_intent"));Key(EKeys::LeftMouseButton,false);Tap(EKeys::M);});
    Add(TEXT("pause_auto_setup"),.4,[this]{ResetWeapon(1,12);Key(EKeys::LeftMouseButton,true);});
    Add(TEXT("pause_enter"),.2,[this]{Tap(EKeys::Escape);},[this]{Check(UGameplayStatics::IsGamePaused(this),TEXT("engine_pause_active_during_auto_reload"));Progress=P->ReloadProgress;});
    Add(TEXT("pause_freezes_reload"),1.8,[this]{},[this]{Check(P->bReloading&&P->MagazineAmmo==0&&P->ReserveAmmo==12&&P->ReloadProgress==Progress,TEXT("pause_does_not_advance_or_credit_reload"));Tap(EKeys::Enter);});
    Add(TEXT("resume_finishes_without_fire"),1.6,[this]{Key(EKeys::LeftMouseButton,false);},[this]{Check(!P->bReloading&&P->MagazineAmmo==12&&P->ReserveAmmo==0,TEXT("resume_finishes_remaining_reload_without_phantom_shot"));});
    Add(TEXT("damage_auto_setup"),.25,[this]{ResetWeapon(1,12);Tap(EKeys::LeftMouseButton);});
    Add(TEXT("damage_keeps_reload"),1.8,[this]{UGameplayStatics::ApplyDamage(P,5,nullptr,this,nullptr);},[this]{Check(P->IsAlive()&&!P->bReloading&&P->MagazineAmmo==12&&P->ReserveAmmo==0,TEXT("nonlethal_damage_keeps_reload_transaction"));});
    Add(TEXT("container_cancels_reload_setup"),.25,[this]{ResetWeapon(1,12);Home=P->GetActorLocation();NewBox();Tap(EKeys::LeftMouseButton);});
    Add(TEXT("container_auto_open"),.25,[this]{AimBox();Tap(EKeys::E);},[this]{
        Check(P->GetOpenLoot()==Box&&P->IsSearching(),TEXT("E_opens_and_automatically_searches_unknown_container"));
        Check(!P->bReloading&&P->MagazineAmmo==0&&P->ReserveAmmo==12&&!P->bFireHeld,TEXT("container_cancels_reload_without_ammo_credit"));
        Check(P->GetLootCount()==0&&!Box->GetContents()[0].bRevealed,TEXT("automatic_search_still_has_real_delay_and_no_auto_take"));Capture(TEXT("06_open_auto_search"));});
    Add(TEXT("first_reveal"),.7,[this]{},[this]{
        Check(Box->GetContents()[0].bRevealed&&P->IsSearching()&&P->GetSearchIndex()==1,TEXT("auto_search_reveals_and_advances_per_item"));
        Check(Click(TEXT("Take_0")),TEXT("revealed_row_can_be_taken_through_canvas_during_search"));
        Check(P->GetLootCount()==1&&Box->GetContents()[0].Quantity==1&&P->IsSearching(),TEXT("manual_take_preserves_next_search_and_remaining_quantity"));Capture(TEXT("07_take_during_auto_search"));});
    Add(TEXT("partial_close"),.05,[this]{Progress=P->GetSearchProgress();Tap(EKeys::Escape);},[this]{Check(!P->IsSearching()&&!P->IsInventoryOpen(),TEXT("close_interrupts_current_row"));});
    Add(TEXT("closed_old_deadlines"),1.5,[this]{},[this]{
        Check(!Box->GetContents()[1].bRevealed&&Box->GetContents()[0].bRevealed&&Box->GetContents()[0].Quantity==1,TEXT("closed_row_stays_unknown_and_revealed_quantity_persists"));
        Check(P->MagazineAmmo==0&&P->ReserveAmmo==12,TEXT("cancelled_reload_old_deadline_cannot_add_ammo"));});
    Add(TEXT("reopen_partial_auto"),.15,[this]{AimBox();Tap(EKeys::E);},[this]{Check(P->IsSearching()&&P->GetSearchIndex()==1&&P->GetSearchProgress()<.4f,TEXT("reopen_automatically_restarts_only_unfinished_row"));});
    Add(TEXT("all_revealed"),1.7,[this]{},[this]{Check(!P->IsSearching()&&Box->FindNextUnsearchedIndex()==INDEX_NONE&&P->GetLootCount()==1,TEXT("search_finishes_without_collecting"));P->CloseLoot();AimBox();Tap(EKeys::E);});
    Add(TEXT("reopen_known"),.2,[this]{},[this]{
        Check(P->GetOpenLoot()==Box&&!P->IsSearching()&&Box->GetContents()[0].Quantity==1,TEXT("fully_searched_reopen_has_no_wait_and_retains_quantity"));
        Check(P->TakeLootItem(0)&&P->TakeLootItem(1)&&P->TakeLootItem(2),TEXT("manual_take_all_revealed_rows"));P->CloseLoot();AimBox();Tap(EKeys::E);});
    Add(TEXT("empty_reopen"),.2,[this]{},[this]{Check(P->GetOpenLoot()==Box&&Box->GetInventoryComponent()->GetItemCount()==0&&!P->IsSearching(),TEXT("empty_container_has_no_meaningless_search"));});
    Add(TEXT("range_open"),.25,[this]{NewBox();Tap(EKeys::E);},[this]{Check(P->IsSearching(),TEXT("range_fixture_auto_search_started"));P->SetActorLocation(Home+FVector(0,-650,0),false,nullptr,ETeleportType::TeleportPhysics);});
    Add(TEXT("range_interrupt"),1,[this]{},[this]{Check(!P->IsSearching()&&!P->IsInventoryOpen()&&!Box->GetContents()[0].bRevealed,TEXT("distance_closes_search_and_blocks_old_deadline"));P->SetActorLocation(Home,false,nullptr,ETeleportType::TeleportPhysics);});
    Add(TEXT("damage_search_open"),.25,[this]{AimBox();Tap(EKeys::E);},[this]{Check(P->IsSearching(),TEXT("damage_fixture_auto_search_started"));UGameplayStatics::ApplyDamage(P,5,nullptr,this,nullptr);});
    Add(TEXT("damage_search_interrupt"),1,[this]{},[this]{Check(!P->IsSearching()&&!P->IsInventoryOpen()&&!Box->GetContents()[0].bRevealed,TEXT("damage_closes_search_and_blocks_old_deadline"));});
    Add(TEXT("full_magazine_R"),.3,[this]{ResetWeapon(12,60);Tap(EKeys::R);},[this]{Check(!P->bReloading&&P->MagazineAmmo==12&&P->ReserveAmmo==60,TEXT("full_magazine_manual_reload_noop"));});
    Add(TEXT("presentation_counts"),.2,[this]{},[this]{
        Check(P->SuccessfulShots==P->ShotPresentationCount,TEXT("exactly_one_muzzle_tracer_casing_event_per_accepted_shot"));
        Check(P->FireSound&&P->SuccessfulShots==P->ShotSoundRequestCount,TEXT("fire_sound_asset_and_trigger_count_match_shots_not_listening"));
        Check(P->ImportedArms->GetBoneIndex(TEXT("hand_L"))!=INDEX_NONE&&P->ImportedArms->GetBoneIndex(TEXT("forearm_L_end"))!=INDEX_NONE
            &&P->ImportedArms->GetBoneIndex(TEXT("hand_R"))!=INDEX_NONE&&P->ImportedArms->GetBoneIndex(TEXT("forearm_R_end"))!=INDEX_NONE,TEXT("continuous_wrist_samples_address_real_bones"));});
    Add(TEXT("showcase_fire"),.04,[this]{ResetWeapon(12,24);Tap(EKeys::LeftMouseButton);},[this]{Capture(TEXT("08_live_fire"));});
    Add(TEXT("death_auto_setup"),.3,[this]{ResetWeapon(1,12);Tap(EKeys::LeftMouseButton);},[this]{Check(P->bReloading,TEXT("death_fixture_auto_reload_started"));UGameplayStatics::ApplyDamage(P,999,nullptr,this,nullptr);});
    Add(TEXT("death_cancels_reload"),1.9,[this]{},[this]{Check(!P->IsAlive()&&!P->bReloading&&!P->bFireHeld&&P->MagazineAmmo==0&&P->ReserveAmmo==12,TEXT("death_cancels_reload_and_all_pending_fire_without_ammo_credit"));});
}
void ARaidR9TestDirector::Sample(float Dt)
{
    if(P->LastShotTime!=LastSampleShot)
    {
        if(P->LastShotTime>=0){++ShotCount;Shots+=FString::Printf(TEXT("%s,%.6f,%d,%.5f\n"),*Steps[Step].Name,P->LastShotTime,P->MagazineAmmo,P->ADSAlpha);}
        LastSampleShot=P->LastShotTime;
    }
    const FVector Location=P->WeaponRoot->GetRelativeLocation();
    const auto* Arms=Cast<URaidArmsAnimInstance>(P->ImportedArms->GetAnimInstance());
    const float LeftGap=FVector::Dist(P->ImportedArms->GetSocketLocation(TEXT("hand_L")),P->ImportedArms->GetSocketLocation(TEXT("forearm_L_end")));
    const float RightGap=FVector::Dist(P->ImportedArms->GetSocketLocation(TEXT("hand_R")),P->ImportedArms->GetSocketLocation(TEXT("forearm_R_end")));
    Frames+=FString::Printf(TEXT("%s,%.6f,%.6f,%d,%d,%d,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%.5f,%.5f,%.5f,%.5f,%.5f\n"),
        *Steps[Step].Name,GetWorld()->GetTimeSeconds(),Dt,ShotCount,P->MagazineAmmo,P->ReserveAmmo,P->bReloading,P->ADSAlpha,
        PC->GetControlRotation().Pitch,P->RecoilPitch,P->RecoilYaw,P->WeaponKick,P->WeaponTilt,
        Location.X,Location.Z,P->WeaponRoot->GetRelativeRotation().Pitch,P->VisualReloadProgress,P->VisualReloadWeight,P->GetShotPresentationAge(),P->IsSearching(),P->GetSearchProgress(),Arms?Arms->SampledShotAge:-1.f,Arms?Arms->SampledReloadPosition:-1.f,LeftGap,RightGap);
}
void ARaidR9TestDirector::Finish()
{
    P->StopFire();P->StopAim();
    FFileHelper::SaveStringToFile(Frames,*(Output/TEXT("Frames.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FFileHelper::SaveStringToFile(Shots,*(Output/TEXT("Shots.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp,Display,TEXT("R9_RESULT: %s checks=%d failures=%d shots=%d"),Failures?TEXT("FAIL"):(bBaseline?TEXT("BASELINE_OBSERVED"):TEXT("PASS")),Checks,Failures,ShotCount);
    FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
}
void ARaidR9TestDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);const double Now=FPlatformTime::Seconds();
    if(Now-Started>240){UE_LOG(LogTemp,Error,TEXT("R9 watchdog"));FPlatformMisc::RequestExitWithStatus(false,2);return;}
    if(!P){PC=Cast<ARaidPlayerController>(UGameplayStatics::GetPlayerController(this,0));P=PC?Cast<ARaidPlayer>(PC->GetPawn()):nullptr;if(P)AddTickPrerequisiteComponent(P->ImportedArms);}
    if(!P||!PC||Now<Next)return;
    if(Steps.IsEmpty())
    {
        UE_LOG(LogTemp,Display,TEXT("R9_AUDIO: fire_asset=%s duration=%.3f reload_asset=%s duration=%.3f action=%.3f silent_trigger_verification_only"),
            *GetNameSafe(P->FireSound),P->FireSound?P->FireSound->GetDuration():0.f,*GetNameSafe(P->ReloadSound),P->ReloadSound?P->ReloadSound->GetDuration():0.f,P->WeaponTuning.ReloadSeconds);
        BuildMeasurements();if(!bBaseline&&!bMeasuresOnly)BuildRegression();
    }
    if(PendingKey.IsValid()&&Now>=ReleaseAt){Key(PendingKey,false);PendingKey=FKey();}
    if(Step>=Steps.Num()){Finish();return;}
    if(!bRunning){Steps[Step].Begin();StepStarted=Now;bRunning=true;UE_LOG(LogTemp,Display,TEXT("R9_STEP: %s"),*Steps[Step].Name);}
    Sample(DeltaSeconds);
    if(Now-StepStarted>=Steps[Step].Seconds){if(Steps[Step].Verify)Steps[Step].Verify();++Step;bRunning=false;}
}
