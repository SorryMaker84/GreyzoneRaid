from pathlib import Path
r=Path(__file__).resolve().parents[1]/'Source/GreyzoneRaid'
p=r/'RaidFrameworkTestDirector.cpp';s=p.read_text(encoding='utf-8-sig')
s=s.replace('const float ExpectedHealth = Medkit ? FMath::Min(100.f, 50.f + static_cast<float>(Medkit->UseAmount)) : 50.f;','const float ExpectedHealth = 50.f; // R10: accepted treatment is deferred, not instant healing.')
s=s.replace('MedUsed.Quantity == 1','MedUsed.Quantity == 0').replace('&& Player->Health > 50.f','&& Player->IsTreating()')
s=s.replace('Player->ReserveAmmo == SavedReserve && Backpack->GetItemCount() == OriginalCount\n        && Backpack->GetTotalValue() == OriginalValue && !Backpack->FindEntry(MedEntryId);','Player->ReserveAmmo == SavedReserve && Backpack->GetItemCount() == OriginalCount + 1\n        && Backpack->GetTotalValue() == OriginalValue + Medkit->UnitValue && Backpack->FindEntry(MedEntryId);')
s=s.replace('TEXT("production_catalog_medkit_grant_and_use")','TEXT("production_catalog_medkit_grant_and_deferred_use")')
s=s.replace('    if (!bMedCheck) return false;','    Player->CancelTreatment();\n    if (Backpack->FindEntry(MedEntryId)) Backpack->Consume(MedEntryId,1,Backpack->GetRevision());\n    if (!bMedCheck) return false;')
p.write_text(s,encoding='utf-8')
p=r/'RaidR8TestDirector.cpp';s=p.read_text(encoding='utf-8-sig');s=s.replace('Tap(EKeys::Q); break;\n        case 1:', 'Tap(EKeys::Q); Next=Now+3.4; break;\n        case 1:');p.write_text(s,encoding='utf-8')
p=r/'RaidTestDirector.h';s=p.read_text(encoding='utf-8-sig').replace('int32 ExpectedItems = 0;','int32 ExpectedItems = 0;\n    int32 MedicalStage = 0;');p.write_text(s,encoding='utf-8')
p=r/'RaidTestDirector.cpp';s=p.read_text(encoding='utf-8-sig');a=s.index('    case EStep::Medkit:');b=s.index('    case EStep::Turn:',a)
s=s[:a]+'''    case EStep::Medkit:
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

'''+s[b:];p.write_text(s,encoding='utf-8')
print('Regression timing expectations adapted; delayed completion assertions retained.')
