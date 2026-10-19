#include "RaidR10TestDirector.h"
#include "RaidPlayer.h"
#include "RaidPlayerController.h"
#include "RaidGameMode.h"
#include "RaidInventoryComponent.h"
#include "RaidInteractionComponent.h"
#include "RaidTreatmentComponent.h"
#include "RaidLoot.h"
#include "RaidEnemy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"
#include "TimerManager.h"

ARaidR10TestDirector::ARaidR10TestDirector()
{ PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bTickEvenWhenPaused=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork; }
void ARaidR10TestDirector::BeginPlay()
{ Super::BeginPlay(); Started=FPlatformTime::Seconds(); Next=Started+2; FParse::Value(FCommandLine::Get(),TEXT("R10Output="),Output); IFileManager::Get().MakeDirectory(*Output,true); }
void ARaidR10TestDirector::Add(const TCHAR* N,double S,TFunction<void()> B,TFunction<void()> V)
{ Steps.Add({N,S,MoveTemp(B),MoveTemp(V)}); }
void ARaidR10TestDirector::Check(bool Passed,const TCHAR* N)
{ ++Checks; if(!Passed)++Failures; UE_LOG(LogTemp,Display,TEXT("R10_CHECK: %s %s"),Passed?TEXT("PASS"):TEXT("FAIL"),N); }
void ARaidR10TestDirector::Capture(const TCHAR* N)
{ FScreenshotRequest::RequestScreenshot(Output/(FString(N)+TEXT(".png")),true,false); }
void ARaidR10TestDirector::Key(FKey K,bool Pressed)
{ PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Pressed?IE_Pressed:IE_Released,Pressed?1.f:0.f)); }
void ARaidR10TestDirector::Heal() { P->GetInteractionComponent()->UseMedkit(); }
void ARaidR10TestDirector::ClearBag()
{
 P->CancelTreatment(); auto* I=P->GetInventoryComponent();
 while(!I->GetEntries().IsEmpty()){auto E=I->GetEntries()[0];I->Consume(E.EntryId,E.Quantity,I->GetRevision());}
}
void ARaidR10TestDirector::SetupMedical()
{ P->StopFire(); P->CancelReload(); P->StopAim(); P->CloseLoot(); ClearBag(); P->Health=50; Check(P->AddItem(TEXT("Medkit"),TEXT("医疗包"),2,75),TEXT("catalog_medical_fixture")); }
void ARaidR10TestDirector::Kernel()
{
 using K=FRaidInventoryModel; using C=ERaidInventoryResultCode;
 FRaidItemDefinition Heavy; Heavy.ItemId=TEXT("Heavy");Heavy.UnitWeightGrams=3000;Heavy.UnitValue=120;
 FRaidInventorySeed Seed;Seed.Definition=Heavy;Seed.Quantity=5;Seed.bRevealed=true;
 FRaidInventoryState S,D;K::InitializeOnce(S,{Seed},-1,false);K::InitializeOnce(D,{},8,false,6000);
 FRaidTransferRequest R;R.SourceEntryId=S.Entries[0].EntryId;R.Quantity=3;R.ExpectedSourceRevision=S.Revision;R.ExpectedDestinationRevision=D.Revision;
 Check(K::Transfer(S,D,R).Code==C::InsufficientCapacity && K::GetItemCount(S)==5 && K::GetItemCount(D)==0 && S.Revision==1 && D.Revision==1,TEXT("overweight_exact_transfer_is_atomic"));
 R.Policy=ERaidTransferPolicy::UpToCapacity;
 Check(K::Transfer(S,D,R).Quantity==2 && K::GetWeightGrams(D)==6000 && K::GetItemCount(S)==3,TEXT("partial_transfer_honors_weight_limit"));
 Check(K::Grant(D,Heavy,1,D.Revision).Code==C::InsufficientCapacity && D.Revision==2,TEXT("grant_cannot_bypass_weight"));
 R.ExpectedSourceRevision=1; Check(K::Transfer(S,D,R).Code==C::StaleRevision,TEXT("stale_transfer_still_rejected"));
 auto Invalid=Heavy;Invalid.UnitWeightGrams=0; Check(!K::IsValidDefinition(Invalid),TEXT("zero_weight_rejected"));
 Invalid=Heavy;Invalid.UnitWeightGrams=1; Check(!K::AreDefinitionsCompatible(Heavy,Invalid),TEXT("weight_is_part_of_stack_compatibility"));
 FRaidInventoryState Bad;Check(K::InitializeOnce(Bad,{Seed},8,false,12000).Code==C::InsufficientCapacity && !Bad.bInitialized,TEXT("overweight_seed_no_partial_initialization"));
 Check(K::Consume(D,D.Entries[0].EntryId,1,D.Revision).IsSuccess() && K::GetWeightGrams(D)==3000,TEXT("consume_releases_weight"));
}
void ARaidR10TestDirector::Build()
{
 Kernel();
 Add(TEXT("load_threshold"),.3,[this]{ClearBag();Check(P->AddItem(TEXT("Tech"),TEXT("重型电子组件"),2,120),TEXT("two_catalog_tech_granted"));},[this]{Check(P->GetInventoryComponent()->GetWeightGrams()==6000 && P->GetLoadSpeedScale()==1.f,TEXT("six_kg_no_penalty_boundary"));});
 Add(TEXT("load_slow"),.3,[this]{P->AddItem(TEXT("Tech"),TEXT("重型电子组件"),1,120);},[this]{Check(FMath::IsNearlyEqual(P->GetCharacterMovement()->MaxWalkSpeed,315.f),TEXT("nine_kg_reduces_actual_movement_speed"));Capture(TEXT("01_load_warning"));});
 Add(TEXT("ten_kg_boundary"),.3,[this]{auto* I=P->GetInventoryComponent();FRaidItemDefinition Item;Item.ItemId=TEXT("1kgBoundaryFixture");I->Grant(Item,1,I->GetRevision());Key(EKeys::LeftShift,true);},[this]{Key(EKeys::LeftShift,false);Check(P->GetInventoryComponent()->GetWeightGrams()==10000&&P->IsTooHeavyToSprint()&&P->GetCharacterMovement()->MaxWalkSpeed<350,TEXT("exact_ten_kg_blocks_actual_sprint"));auto* I=P->GetInventoryComponent();I->Consume(I->GetEntries().Last().EntryId,1,I->GetRevision());});
 Add(TEXT("load_full"),.3,[this]{P->AddItem(TEXT("Tech"),TEXT("重型电子组件"),1,120);Key(EKeys::LeftShift,true);},[this]{Key(EKeys::LeftShift,false);Check(P->IsTooHeavyToSprint() && P->GetCharacterMovement()->MaxWalkSpeed<=280.1f,TEXT("twelve_kg_blocks_sprint"));Check(!P->AddItem(TEXT("Ammo"),TEXT("弹药"),1,35)&&P->GetLootCount()==4,TEXT("weight_full_before_eight_slots"));});
 Add(TEXT("world_capacity"),.4,[this]{
  FTransform T(FRotator::ZeroRotator,P->GetActorLocation()+FVector(130,0,-40));Box=GetWorld()->SpawnActorDeferred<ARaidLoot>(ARaidLoot::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);Box->bGenerateSupplementalLoot=false;Box->SearchDuration=.2f;Box->FinishSpawning(T);Check(P->OpenLoot(Box),TEXT("full_weight_opens_and_searches"));
 },[this]{auto* I=Box->GetInventoryComponent();BeforeRevision=I->GetRevision();BeforeCount=I->GetItemCount();Check(!P->TakeLootItem(0)&&I->GetItemCount()==BeforeCount&&I->GetRevision()==BeforeRevision,TEXT("rejected_world_take_keeps_source_quantity_and_revision"));Capture(TEXT("02_capacity_rejected"));});
 Add(TEXT("store_back"),.3,[this]{Check(P->StoreInventoryItem(0),TEXT("store_heavy_item_back_in_world"));},[this]{Check(P->GetInventoryComponent()->GetWeightGrams()==9000&&!P->IsTooHeavyToSprint(),TEXT("store_restores_sprint_eligibility"));P->CloseLoot();Box->Destroy();});
 Add(TEXT("medical_start"),1,[this]{SetupMedical();P->MagazineAmmo=5;P->Reload();Heal();BeforeRevision=P->GetInventoryComponent()->GetRevision();},[this]{Check(P->IsTreating()&&!P->bReloading&&P->Health==50&&P->GetLootCount()==2,TEXT("heal_cancels_reload_without_early_consumption"));Check(P->GetCharacterMovement()->MaxWalkSpeed<=140.1f,TEXT("treatment_limits_actual_move_speed"));Capture(TEXT("03_treatment_progress"));});
 Add(TEXT("duplicate_begin"),.4,[this]{const auto* I=P->GetInventoryComponent();Check(P->GetTreatmentComponent()->BeginTreatment(I->GetEntries()[0].EntryId,I->GetRevision()).Code==ERaidInventoryResultCode::Busy,TEXT("duplicate_begin_cannot_restart_timer"));},[this]{Check(P->GetTreatmentComponent()->GetProgress()>.4f,TEXT("original_heal_deadline_preserved"));});
 Add(TEXT("heal_commit"),2,[this]{},[this]{Check(P->Health==95&&P->GetLootCount()==1&&!P->IsTreating()&&P->GetInventoryComponent()->GetRevision()==BeforeRevision+1,TEXT("heal_commits_once_after_three_seconds"));Check(P->GetInventoryComponent()->GetWeightGrams()==600&&P->GetCharacterMovement()->MaxWalkSpeed==350,TEXT("heal_consumption_restores_weight_and_speed"));Capture(TEXT("04_treatment_complete"));});
 Add(TEXT("heal_cap"),3.3,[this]{Heal();},[this]{Check(P->Health==100&&P->GetLootCount()==0,TEXT("heal_caps_at_100_and_consumes_last_item"));});
 Add(TEXT("q_cancel"),.3,[this]{SetupMedical();Heal();Heal();},[this]{Check(!P->IsTreating()&&P->Health==50&&P->GetLootCount()==2,TEXT("second_Q_cancels_without_consumption"));});
 Add(TEXT("fire_cancel"),.3,[this]{Heal();Key(EKeys::LeftMouseButton,true);Key(EKeys::LeftMouseButton,false);},[this]{Check(!P->IsTreating()&&P->GetLootCount()==2,TEXT("fire_interrupts_treatment"));});
 Add(TEXT("reload_cancel"),.3,[this]{Heal();P->MagazineAmmo=1;P->Reload();},[this]{Check(!P->IsTreating()&&P->bReloading&&P->GetLootCount()==2,TEXT("reload_interrupts_treatment"));P->CancelReload();});
 Add(TEXT("sprint_cancel"),.3,[this]{Heal();Key(EKeys::LeftShift,true);},[this]{Key(EKeys::LeftShift,false);Check(!P->IsTreating()&&P->GetLootCount()==2,TEXT("sprint_interrupts_treatment"));});
 Add(TEXT("inventory_cancel"),.3,[this]{Heal();Key(EKeys::Tab,true);Key(EKeys::Tab,false);},[this]{Check(!P->IsTreating()&&P->IsInventoryOpen()&&P->GetLootCount()==2,TEXT("backpack_interrupts_treatment"));P->CloseLoot();});
 Add(TEXT("container_cancel"),.3,[this]{Heal();FTransform T(FRotator::ZeroRotator,P->GetActorLocation()+FVector(130,0,-40));Box=GetWorld()->SpawnActorDeferred<ARaidLoot>(ARaidLoot::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);Box->FinishSpawning(T);Check(P->OpenLoot(Box),TEXT("open_container_during_treatment"));},[this]{Check(!P->IsTreating()&&P->IsSearching()&&P->GetLootCount()==2,TEXT("container_cancels_heal_and_preserves_auto_search"));P->CloseLoot();Box->Destroy();});
 Add(TEXT("cancel_reentry"),.2,[this]{Heal();const FDelegateHandle H=P->GetTreatmentComponent()->OnTreatmentChanged.AddLambda([this]{auto* I=P->GetInventoryComponent();Check(P->GetTreatmentComponent()->BeginTreatment(I->GetEntries()[0].EntryId,I->GetRevision()).Code==ERaidInventoryResultCode::Busy,TEXT("cancel_callback_cannot_reenter_treatment"));});P->CancelTreatment();P->GetTreatmentComponent()->OnTreatmentChanged.Remove(H);},[this]{Check(!P->IsTreating()&&P->GetLootCount()==2,TEXT("cancel_transition_remains_idle"));});
 Add(TEXT("pause_start"),.6,[this]{Heal();},[this]{M->HandleBack();Progress=P->GetTreatmentComponent()->GetProgress();});
 Add(TEXT("pause_frozen"),3.5,[this]{},[this]{Check(P->IsTreating()&&P->Health==50&&P->GetTreatmentComponent()->GetProgress()==Progress,TEXT("world_pause_freezes_treatment_without_completion"));M->ResumeRaid();});
 Add(TEXT("pause_resume"),2.7,[this]{},[this]{Check(P->Health==95&&P->GetLootCount()==1,TEXT("resume_completes_once"));});
 Add(TEXT("damage_cancel"),3.4,[this]{SetupMedical();Heal();UGameplayStatics::ApplyDamage(P,5,nullptr,this,nullptr);},[this]{Check(P->Health==45&&P->GetLootCount()==2&&!P->IsTreating(),TEXT("damage_cancels_and_no_late_heal"));});
 Add(TEXT("revision_cancel"),3.3,[this]{Heal();auto* I=P->GetInventoryComponent();I->Consume(I->GetEntries()[0].EntryId,1,I->GetRevision());},[this]{Check(P->Health==45&&P->GetLootCount()==1&&!P->IsTreating(),TEXT("changed_item_revision_cancels_deferred_effect"));});
 Add(TEXT("unpossess"),3.3,[this]{Heal();PC->UnPossess();},[this]{Check(!P->IsTreating()&&P->Health==45&&P->GetLootCount()==1,TEXT("unpossess_clears_timer_without_item_loss"));PC->Possess(P);});
 Add(TEXT("threshold_heal"),3.3,[this]{SetupMedical();auto* I=P->GetInventoryComponent();I->Consume(I->GetEntries()[0].EntryId,1,I->GetRevision());P->AddItem(TEXT("Tech"),TEXT("电子组件"),2,120);Check(P->GetLoadSpeedScale()<1.f,TEXT("medkit_weight_crosses_slow_threshold"));Heal();},[this]{Check(P->GetInventoryComponent()->GetWeightGrams()==6000&&P->GetLoadSpeedScale()==1.f&&P->GetCharacterMovement()->MaxWalkSpeed==350.f,TEXT("consuming_medkit_removes_load_penalty"));});
 for(int32 Order=0;Order<2;++Order)
 Add(Order==0?TEXT("same_deadline_damage_first"):TEXT("same_deadline_heal_first"),3.4,[this,Order]{
  SetupMedical();FTimerHandle DamageTimer;
  auto Schedule=[this,&DamageTimer]{GetWorldTimerManager().SetTimer(DamageTimer,FTimerDelegate::CreateWeakLambda(this,[this]{UGameplayStatics::ApplyDamage(P,5,nullptr,this,nullptr);}),3.f,false);};
  if(Order==0){Schedule();Heal();}else{Heal();Schedule();}
 },[this]{Check((P->Health==45&&P->GetLootCount()==2)||(P->Health==90&&P->GetLootCount()==1),TEXT("equal_deadline_damage_and_heal_serializes_without_double_effect"));Check(!P->IsTreating(),TEXT("equal_deadline_leaves_no_pending_treatment"));});
 Add(TEXT("lethal_cancel"),3.3,[this]{SetupMedical();Heal();UGameplayStatics::ApplyDamage(P,999,nullptr,this,nullptr);},[this]{Check(M->State==ERaidState::Dead&&P->Health==0&&!P->IsTreating()&&P->GetLootCount()==2,TEXT("death_never_revives_from_old_heal_timer"));Capture(TEXT("05_death_cleanup"));});
}
void ARaidR10TestDirector::Tick(float D)
{
 Super::Tick(D); const double Now=FPlatformTime::Seconds();
 if(Now-Started>130){Check(false,TEXT("watchdog"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
 if(Now<Next)return;
 if(!bReady){PC=Cast<ARaidPlayerController>(UGameplayStatics::GetPlayerController(this,0));P=PC?Cast<ARaidPlayer>(PC->GetPawn()):nullptr;M=Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));if(!P||!M)return;
  for(TActorIterator<ARaidEnemy> It(GetWorld());It;++It){It->DetectionRadius=0;It->AttackRange=0;}
  UE_LOG(LogTemp,Display,TEXT("R10_FIXTURE: isolated medical/load probes; catalog grants and controlled damage; normal AI and full raid are separate journey evidence."));Build();bReady=true;}
 if(bRunning){if(Steps[Step].Verify)Steps[Step].Verify();++Step;bRunning=false;Next=Now+.15;return;} // Let queued screenshots render before the next mutation.
 if(Step>=Steps.Num()) {UE_LOG(LogTemp,Display,TEXT("R10_RESULT: checks=%d failures=%d"),Checks,Failures);FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"checks\":%d,\"failures\":%d,\"status\":\"%s\"}"),Checks,Failures,Failures?TEXT("FAIL"):TEXT("PASS")),*(Output/TEXT("Result.json")));FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);SetActorTickEnabled(false);return;}
 Steps[Step].Begin();bRunning=true;Next=Now+Steps[Step].Seconds;
}
