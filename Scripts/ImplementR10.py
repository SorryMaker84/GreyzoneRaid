from pathlib import Path
root=Path(__file__).resolve().parents[1]/'Source/GreyzoneRaid'
def edit(name,changes):
 p=root/name;s=p.read_text(encoding='utf-8-sig')
 for old,new in changes:
  assert old in s,(name,old)
  s=s.replace(old,new,1)
 p.write_text(s,encoding='utf-8')
edit('RaidPlayer.cpp',[
 ('if (CanUseWorldInput()) Jump();','if (CanUseWorldInput()) { CancelTreatment(); Jump(); }'),
 ('void ARaidPlayer::StartAim()\n{\n    if (!CanUseWorldInput()) return;', 'void ARaidPlayer::StartAim()\n{\n    if (!CanUseWorldInput()) return;\n    CancelTreatment();'),
 ('// A held trigger can wait for a reload; releasing it clears that intent.', 'CancelTreatment();\n    // A held trigger can wait for a reload; releasing it clears that intent.'),
 ('if (!CanUseWorldInput() || bReloading || MagazineAmmo >= MaxMagazineAmmo) return;', 'if (!CanUseWorldInput() || bReloading || MagazineAmmo >= MaxMagazineAmmo) return;\n    CancelTreatment();')])
p=root/'RaidPlayer.cpp'
with p.open('a',encoding='utf-8') as f:f.write('''
bool ARaidPlayer::IsTreating() const { return TreatmentComponent && TreatmentComponent->IsTreating(); }
void ARaidPlayer::CancelTreatment() { if (TreatmentComponent) TreatmentComponent->CancelTreatment(); }
bool ARaidPlayer::IsTooHeavyToSprint() const { return InventoryComponent->GetWeightGrams() >= 10000; }
float ARaidPlayer::GetLoadSpeedScale() const
{
    const float Kg = static_cast<float>(InventoryComponent->GetWeightGrams()) / 1000.f;
    return FMath::Lerp(1.f, .8f, FMath::Clamp((Kg - 6.f) / 6.f, 0.f, 1.f));
}
''')
edit('RaidInteractionComponent.cpp',[
 ('#include "RaidInteractionComponent.h"','#include "RaidInteractionComponent.h"\n#include "RaidTreatmentComponent.h"'),
 ('NewHealth = FMath::Min(100.f, NewHealth + static_cast<float>(Amount));','return Player->GetTreatmentComponent()->BeginTreatment(EntryId, ExpectedBackpackRevision);'),
 ('void URaidInteractionComponent::UseMedkit()\n{','void URaidInteractionComponent::UseMedkit()\n{\n    if (ARaidPlayer* Player = GetRaidPlayer())\n        if (Player->IsTreating()) { Player->CancelTreatment(); Notify(TEXT("治疗已取消，医疗包未消耗。")); return; }'),
 ('Notify(RaidInteraction::BackpackFull);','Notify(TEXT("背包件数或重量不足：放回部分物资后再拿取。"));')])
edit('RaidGameMode.cpp', [('Pawn->StopFire();','Pawn->CancelTreatment();\n        Pawn->StopFire();')])
edit('RaidHUD.cpp',[
 ('#include "RaidHUD.h"','#include "RaidHUD.h"\n#include "RaidTreatmentComponent.h"'),
 ('DrawRightLabel(TEXT("每件占 1 格 · 活着撤离后保留"),', 'DrawRightLabel(FString::Printf(TEXT("%.1f / %.1f kg"), Player->GetInventoryComponent()->GetWeightGrams()/1000.f, Player->GetInventoryComponent()->GetWeightCapacityGrams()/1000.f),'),
 ('TEXT("可逐件取舍；使用医疗包或弹药会消耗 1 件。")','TEXT("6 kg 起减速 · 10 kg 禁冲刺 · 12 kg 上限")'),
 ('DrawInventoryButton(TakeAction, bFull ? TEXT("背包已满") : TEXT("拿取 1"),','const bool bFits = !bFull && Player->GetInventoryComponent()->GetWeightGrams() + Entry.Definition.UnitWeightGrams <= Player->GetInventoryComponent()->GetWeightCapacityGrams();\n                    DrawInventoryButton(TakeAction, !bFits ? TEXT("容量不足") : TEXT("拿取 1"),'),
 ('RowY + 9.f, 94.f, 32.f, !bFull, true);','RowY + 9.f, 94.f, 32.f, bFits, true);'),
 ('TEXT("×%d · 单件 $%d"), Entry.Quantity, Entry.Definition.UnitValue','TEXT("×%d · $%d · %.1f kg/件"), Entry.Quantity, Entry.Definition.UnitValue, Entry.Definition.UnitWeightGrams / 1000.f'),
 ('TEXT("×%d · $%lld"), Item.Quantity,\n                static_cast<int64>(Item.Definition.UnitValue) * Item.Quantity)', 'TEXT("×%d · $%lld · %.1fkg"), Item.Quantity,\n                static_cast<int64>(Item.Definition.UnitValue) * Item.Quantity, Item.Quantity * Item.Definition.UnitWeightGrams / 1000.f)'),
 ('TEXT(" · 生命 +%d")','TEXT(" · 3秒 +%d")'),
 ('const float StatusY = ScreenHeight - 140.f;', '''if (!bInventoryViewOpen)
    {
        const float Kg = Player->GetInventoryComponent()->GetWeightGrams()/1000.f;
        DrawRightLabel(FString::Printf(TEXT("负重 %.1f / 12 kg · %s"), Kg,
            Player->IsTooHeavyToSprint() ? TEXT("无法冲刺") : Kg > 6.f ? TEXT("移动减速") : TEXT("轻装")),
            ScreenWidth - Margin, 82.f, Kg > 6.f ? RaidUI::Amber : RaidUI::Muted, .78f);
        if (Player->IsTreating())
        {
            const float Progress = Player->GetTreatmentComponent()->GetProgress();
            DrawSoftBackdrop(CenterX - 230.f, ScreenHeight - 230.f, 460.f, 62.f, .9f);
            DrawLabel(FString::Printf(TEXT("治疗中 %d%% · [Q] 取消"), FMath::FloorToInt(Progress * 100)),
                CenterX, ScreenHeight - 225.f, RaidUI::Green, .95f, true);
            DrawLabel(TEXT("开火、冲刺或受伤中断 · 完成才消耗"), CenterX, ScreenHeight - 201.f, RaidUI::Muted, .75f, true);
            DrawBar(CenterX - 210.f, ScreenHeight - 178.f, 420.f, 4.f, Progress, RaidUI::Green);
        }
    }
    const float StatusY = ScreenHeight - 140.f;''')])
print('R10 production adapters updated')
