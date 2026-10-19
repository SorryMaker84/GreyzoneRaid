#include "RaidTreatmentComponent.h"
#include "RaidPlayer.h"
#include "RaidInventoryComponent.h"
#include "RaidGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

URaidTreatmentComponent::URaidTreatmentComponent() { PrimaryComponentTick.bCanEverTick = false; }

FRaidInventoryResult URaidTreatmentComponent::BeginTreatment(int32 EntryId, int64 ExpectedRevision)
{
    FRaidInventoryResult Result;
    if (bTransition) { Result.Code = ERaidInventoryResultCode::Busy; return Result; }
    TGuardValue<bool> Transition(bTransition, true);
    ARaidPlayer* Player = Cast<ARaidPlayer>(GetOwner());
    URaidInventoryComponent* Inventory = Player ? Player->GetInventoryComponent() : nullptr;
    if (!Player || !Player->CanAct() || !Player->GetController() || !Inventory) return Result;
    Result.Revision = Inventory->GetRevision();
    if (IsTreating()) { Result.Code = ERaidInventoryResultCode::Busy; return Result; }
    if (ExpectedRevision != Result.Revision) { Result.Code = ERaidInventoryResultCode::StaleRevision; return Result; }
    const FRaidInventoryEntry* Entry = Inventory->FindEntry(EntryId);
    if (!Entry || Entry->Quantity < 1 || !Entry->bRevealed || Entry->Definition.UseEffect != ERaidItemUseEffect::Heal)
        { Result.Code = ERaidInventoryResultCode::InvalidEntry; return Result; }
    if (!FMath::IsFinite(Player->Health) || Player->Health >= 100.f) return Result;
    // Leave inventory UI and cancel firing/reloading before beginning the timer.
    Player->PrepareInventoryInput();
    Player->CloseLoot();
    Backpack = Inventory;
    ActiveEntryId = EntryId;
    RevisionAtStart = ExpectedRevision;
    StartedAt = GetWorld()->GetTimeSeconds();
    ActiveDuration = FMath::IsFinite(Duration) ? FMath::Clamp(Duration, .2f, 30.f) : 3.f;
    Inventory->OnInventoryChanged.AddUObject(this, &URaidTreatmentComponent::InventoryChanged);
    GetWorld()->GetTimerManager().SetTimer(Timer, this, &URaidTreatmentComponent::FinishTreatment, ActiveDuration, false);
    Result.Code = ERaidInventoryResultCode::Success; // Accepted; quantity zero until the deferred commit.
    Result.EntryId = EntryId;
    OnTreatmentChanged.Broadcast();
    return Result;
}

float URaidTreatmentComponent::GetProgress() const
{
    return IsTreating() && GetWorld() ? FMath::Clamp((GetWorld()->GetTimeSeconds() - StartedAt) / ActiveDuration, 0.f, 1.f) : 0.f;
}

void URaidTreatmentComponent::InventoryChanged(URaidInventoryComponent*, int64 Revision)
{
    if (IsTreating() && Revision != RevisionAtStart) CancelTreatment();
}

void URaidTreatmentComponent::CancelTreatment()
{
    TGuardValue<bool> Transition(bTransition, true);
    const bool bWasActive = IsTreating();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Timer);
    if (Backpack.IsValid()) Backpack->OnInventoryChanged.RemoveAll(this);
    Backpack.Reset();
    ActiveEntryId = INDEX_NONE;
    if (bWasActive) OnTreatmentChanged.Broadcast();
}

void URaidTreatmentComponent::FinishTreatment()
{
    TGuardValue<bool> Transition(bTransition, true);
    ARaidPlayer* Player = Cast<ARaidPlayer>(GetOwner());
    URaidInventoryComponent* Inventory = Backpack.Get();
    const int32 EntryId = ActiveEntryId;
    const int64 ExpectedRevision = RevisionAtStart;
    // Clear timer/state/listeners before the inventory's atomic commit broadcasts.
    CancelTreatment();
    if (!Player || !Player->CanAct() || !Player->GetController() || !Inventory || Player->Health >= 100.f) return;
    const FRaidInventoryEntry* Entry = Inventory->FindEntry(EntryId);
    if (!Entry || Entry->Definition.UseEffect != ERaidItemUseEffect::Heal || !FMath::IsFinite(Player->Health)) return;
    const float PreviousHealth = Player->Health;
    const float NewHealth = FMath::Min(100.f, Player->Health + Entry->Definition.UseAmount);
    const FRaidInventoryResult Result = Inventory->ConsumeWithCommit(EntryId, 1, ExpectedRevision,
        [Player, NewHealth] { Player->Health = NewHealth; });
    if (Result.IsSuccess())
        if (ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this)))
            Mode->PushMessage(FString::Printf(TEXT("治疗完成 · 恢复 %d 点生命 · 消耗 1 个医疗包"), FMath::RoundToInt(NewHealth - PreviousHealth)));
}

void URaidTreatmentComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelTreatment();
    OnTreatmentChanged.Clear();
    Super::EndPlay(Reason);
}
