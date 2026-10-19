from pathlib import Path
p=Path(__file__).resolve().parents[1]/'Source/GreyzoneRaid/RaidR7TestDirector.cpp'
s=p.read_text(encoding='utf-8-sig')
def sub(a,b):
 global s
 assert a in s,a
 s=s.replace(a,b,1)
sub('#include "RaidR7TestDirector.h"','#include "RaidR7TestDirector.h"\n#include "RaidTreatmentComponent.h"')
sub('bool bRealDeath = false;', 'bool bRealDeath = false;\n    bool bR10Loaded = false, bR10Healed = false, bR10FoughtAfterHeal = false;')
sub('bFinaleOnly = bTest &&', 'bR10Journey = FParse::Param(FCommandLine::Get(), TEXT("RaidR10Journey"));\n    bFinaleOnly = bTest &&')
start=s.index('    if (RaidPawn->Health <= 65.f)',s.index('bool ARaidR7TestDirector::UpdateCombat'))
end=s.index('    ARaidEnemy* Threat',start)
s=s[:start]+'''    if (bSeekingR10Cover) { RaidPawn->StopFire(); bTestFiring=false; return false; }
    if (bR10Journey && Journey().Round==0 && !Journey().bR10Healed && Journey().bR10Loaded
        && Journey().bLiveDamage && RaidPawn->Health<100.f && TestStep==ETestStep::Walk)
        if (SeekR10Cover()) return true;
'''+s[end:]
sub('if (!Threat)\n    {', '''if (!Threat)
    {
        if (!bR10Journey && RaidPawn->Health <= 65.f && !RaidPawn->IsTreating())
            for (const FRaidInventoryEntry& Entry : RaidPawn->GetInventoryComponent()->GetEntries())
                if (Entry.Definition.UseEffect == ERaidItemUseEffect::Heal && Entry.Quantity > 0)
                { ExecuteAction(TEXT("Heal")); break; }
        if (RaidPawn->IsTreating()) return true;''')
sub('{ ExecuteAction(TEXT("Fire")); bTestFiring = true; }', '''{
        const int32 BeforeAmmo=RaidPawn->MagazineAmmo;
        ExecuteAction(TEXT("Fire")); bTestFiring = true;
        if (bR10Journey && Journey().bR10Healed && RaidPawn->MagazineAmmo < BeforeAmmo)
            Journey().bR10FoughtAfterHeal=true;
    }''')
sub('if (TouchedLoot.Num() >= (Journey().Round == 0 && !bFinaleOnly ? 24 : 1))',
    'if (TouchedLoot.Num() >= (bR10Journey && Journey().Round==0 ? 4 : Journey().Round == 0 && !bFinaleOnly ? 24 : 1))')
sub('TouchedLoot.Add(TestLoot.Get());','''TouchedLoot.Add(TestLoot.Get());
        if (bR10Journey && RaidPawn->GetInventoryComponent()->GetWeightGrams()>6000)
        {
            Journey().bR10Loaded=true;
            Check(RaidPawn->GetLoadSpeedScale()<1.f,TEXT("R10_authored_loot_causes_real_load_penalty"));
        }''')
sub('else SetTestStep(ETestStep::LootClose);','''else if (bR10Journey && Journey().Round==0 && TouchedLoot.Num()==4)
            CaptureTest(TEXT("r1_r10_weighted_backpack"),ETestStep::LootClose);
        else SetTestStep(ETestStep::LootClose);''')
sub('case ETestStep::MissionApproach:', '''case ETestStep::R10TreatmentStart:
        RaidPawn->StopFire(); bTestFiring=false;
        R10HealthBefore=RaidPawn->Health; R10MedicalCount=RaidPawn->GetLootCount();
        ExecuteAction(TEXT("Heal"));
        if(!Check(RaidPawn->IsTreating(),TEXT("R10_cover_starts_catalog_medical_treatment"))) { Fail(TEXT("R10_no_medical_item_at_cover")); return; }
        SetTestStep(ETestStep::R10TreatmentWait);
        break;
    case ETestStep::R10TreatmentWait:
        if(RaidPawn->IsTreating())
        {
            if(StepTime>.8 && StepTime<1.0) CaptureTest(TEXT("r1_r10_healing_at_cover"),ETestStep::R10TreatmentWait);
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
    case ETestStep::MissionApproach:''')
sub('if (Journey().Round == 3)\n        {','''if (Journey().Round == 3)
        {
            if(bR10Journey && !Check(Journey().bR10Loaded && Journey().bR10Healed && Journey().bR10FoughtAfterHeal,
                TEXT("R10_real_loot_load_cover_treatment_combat_extraction_chain"))) { Fail(TEXT("R10_incomplete_new_mechanic_journey")); return; }''')
sub('Receipt->SetBoolField(TEXT("audio_listened"), false);', '''Receipt->SetBoolField(TEXT("audio_listened"), false);
    Receipt->SetBoolField(TEXT("r10_journey"),bR10Journey);
    Receipt->SetBoolField(TEXT("r10_authored_loot_load"),Journey().bR10Loaded);
    Receipt->SetBoolField(TEXT("r10_cover_treatment"),Journey().bR10Healed);
    Receipt->SetBoolField(TEXT("r10_fired_after_treatment"),Journey().bR10FoughtAfterHeal);''')
s+='''
bool ARaidR7TestDirector::SeekR10Cover()
{
    if(R10CoverAttempts>=5)return false;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if(!Nav)return false;
    const FVector Origin=RaidPawn->GetActorLocation();
    FVector BestPoint; float BestDistance=FLT_MAX; bool bFound=false;
    for(int32 Radius=400;Radius<=1600;Radius+=400)
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
                FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(R10Cover),false,RaidPawn);Params.AddIgnoredActor(E);
                const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,E->GetActorLocation()+FVector(0,0,57),Candidate+FVector(0,0,64),ECC_Visibility,Params);
                if(!Blocked){bExposed=true;break;} ++Occluded;
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
'''
p.write_text(s,encoding='utf-8')
