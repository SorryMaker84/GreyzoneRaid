#include "RaidInventoryComponent.h"
#include "Templates/UnrealTemplate.h"

URaidInventoryComponent::URaidInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void URaidInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (!bEndingPlay)
    {
        // Subscribers can close their interaction session while this component
        // is still readable. Any attempted mutation is rejected from now on.
        bEndingPlay = true;
        OnInventoryEndingPlay.Broadcast(this);
        OnInventoryEndingPlay.Clear();
        OnInventoryChanged.Clear();
    }
    Super::EndPlay(EndPlayReason);
}

const FRaidInventoryEntry* URaidInventoryComponent::FindEntry(int32 EntryId) const
{
    return FRaidInventoryModel::FindEntry(State, EntryId);
}

int32 URaidInventoryComponent::GetItemCount() const
{
    return FRaidInventoryModel::GetItemCount(State);
}

int64 URaidInventoryComponent::GetTotalValue() const
{
    return FRaidInventoryModel::GetTotalValue(State);
}

FRaidInventoryResult URaidInventoryComponent::RejectedMutation(ERaidInventoryResultCode Code) const
{
    FRaidInventoryResult Result;
    Result.Code = Code;
    Result.Revision = State.Revision;
    return Result;
}

ERaidInventoryResultCode URaidInventoryComponent::GetMutationGate() const
{
    if (bEndingPlay) return ERaidInventoryResultCode::EndingPlay;
    return bMutationInProgress ? ERaidInventoryResultCode::Busy : ERaidInventoryResultCode::Success;
}

void URaidInventoryComponent::BroadcastCommittedChange()
{
    if (!bEndingPlay) OnInventoryChanged.Broadcast(this, State.Revision);
}

FRaidInventoryResult URaidInventoryComponent::InitializeOnce(const TArray<FRaidInventorySeed>& Seeds,
    int32 Capacity, bool bKeepEmptyRows, int32 WeightCapacityGrams)
{
    const ERaidInventoryResultCode Gate = GetMutationGate();
    if (Gate != ERaidInventoryResultCode::Success) return RejectedMutation(Gate);
    TGuardValue<bool> Guard(bMutationInProgress, true);
    const FRaidInventoryResult Result = FRaidInventoryModel::InitializeOnce(State, Seeds, Capacity, bKeepEmptyRows, WeightCapacityGrams);
    if (Result.IsSuccess()) BroadcastCommittedChange();
    return Result;
}

FRaidInventoryResult URaidInventoryComponent::Reveal(int32 EntryId, int64 ExpectedRevision)
{
    const ERaidInventoryResultCode Gate = GetMutationGate();
    if (Gate != ERaidInventoryResultCode::Success) return RejectedMutation(Gate);
    TGuardValue<bool> Guard(bMutationInProgress, true);
    const FRaidInventoryResult Result = FRaidInventoryModel::Reveal(State, EntryId, ExpectedRevision);
    if (Result.IsSuccess()) BroadcastCommittedChange();
    return Result;
}

FRaidInventoryResult URaidInventoryComponent::Grant(const FRaidItemDefinition& Definition,
    int32 Quantity, int64 ExpectedRevision)
{
    const ERaidInventoryResultCode Gate = GetMutationGate();
    if (Gate != ERaidInventoryResultCode::Success) return RejectedMutation(Gate);
    TGuardValue<bool> Guard(bMutationInProgress, true);
    const FRaidInventoryResult Result = FRaidInventoryModel::Grant(State, Definition, Quantity, ExpectedRevision);
    if (Result.IsSuccess()) BroadcastCommittedChange();
    return Result;
}

FRaidInventoryResult URaidInventoryComponent::Consume(int32 EntryId, int32 Quantity, int64 ExpectedRevision)
{
    return ConsumeWithCommit(EntryId, Quantity, ExpectedRevision, []() {});
}

FRaidInventoryResult URaidInventoryComponent::ConsumeWithCommit(int32 EntryId, int32 Quantity,
    int64 ExpectedRevision, TFunctionRef<void()> CommitEffect)
{
    const ERaidInventoryResultCode Gate = GetMutationGate();
    if (Gate != ERaidInventoryResultCode::Success) return RejectedMutation(Gate);
    TGuardValue<bool> Guard(bMutationInProgress, true);
    const FRaidInventoryResult Result = FRaidInventoryModel::Consume(State, EntryId, Quantity, ExpectedRevision);
    if (Result.IsSuccess())
    {
        // The caller prevalidates the effect. This non-failing commit only writes
        // its already-approved health/ammo values and must not broadcast events.
        // Both the inventory and effect are visible by the first change callback.
        CommitEffect();
        BroadcastCommittedChange();
    }
    return Result;
}

FRaidTransferResult URaidInventoryComponent::TransferTo(URaidInventoryComponent* Destination,
    const FRaidTransferRequest& Request)
{
    FRaidTransferResult Rejected;
    Rejected.SourceRevision = State.Revision;
    if (!IsValid(Destination))
    {
        Rejected.Code = ERaidInventoryResultCode::InvalidDestination;
        return Rejected;
    }
    Rejected.DestinationRevision = Destination->State.Revision;
    if (Destination == this)
    {
        Rejected.Code = ERaidInventoryResultCode::SelfTransfer;
        return Rejected;
    }
    const ERaidInventoryResultCode SourceGate = GetMutationGate();
    if (SourceGate != ERaidInventoryResultCode::Success)
    {
        Rejected.Code = SourceGate;
        return Rejected;
    }
    const ERaidInventoryResultCode DestinationGate = Destination->GetMutationGate();
    if (DestinationGate != ERaidInventoryResultCode::Success)
    {
        Rejected.Code = DestinationGate;
        return Rejected;
    }

    // Guard both owners until both notifications return. A callback may inspect
    // either committed ledger, but cannot start a nested transfer or consumption.
    TGuardValue<bool> SourceGuard(bMutationInProgress, true);
    TGuardValue<bool> DestinationGuard(Destination->bMutationInProgress, true);
    const FRaidTransferResult Result = FRaidInventoryModel::Transfer(State, Destination->State, Request);
    if (Result.IsSuccess())
    {
        BroadcastCommittedChange();
        Destination->BroadcastCommittedChange();
    }
    return Result;
}
