#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RaidInventoryModel.h"
#include "Templates/Function.h"
#include "RaidInventoryComponent.generated.h"

class URaidInventoryComponent;
DECLARE_MULTICAST_DELEGATE_TwoParams(FRaidInventoryChanged, URaidInventoryComponent*, int64);
DECLARE_MULTICAST_DELEGATE_OneParam(FRaidInventoryEndingPlay, URaidInventoryComponent*);

/** The sole mutable inventory owner. Runtime entries are exposed only as const views. */
UCLASS(ClassGroup=(Raid), meta=(BlueprintSpawnableComponent))
class GREYZONERAID_API URaidInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URaidInventoryComponent();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    const TArray<FRaidInventoryEntry>& GetEntries() const { return State.Entries; }
    const FRaidInventoryEntry* FindEntry(int32 EntryId) const;
    int64 GetRevision() const { return State.Revision; }
    int32 GetItemCount() const;
    int64 GetTotalValue() const;
    int32 GetCapacity() const { return State.Capacity; }
    int64 GetWeightGrams() const { return FRaidInventoryModel::GetWeightGrams(State); }
    int32 GetWeightCapacityGrams() const { return State.WeightCapacityGrams; }
    bool IsInitialized() const { return State.bInitialized; }

    FRaidInventoryResult InitializeOnce(const TArray<FRaidInventorySeed>& Seeds,
        int32 Capacity, bool bKeepEmptyRows, int32 WeightCapacityGrams = -1);
    FRaidInventoryResult Reveal(int32 EntryId, int64 ExpectedRevision);
    FRaidInventoryResult Grant(const FRaidItemDefinition& Definition, int32 Quantity, int64 ExpectedRevision);
    FRaidInventoryResult Consume(int32 EntryId, int32 Quantity, int64 ExpectedRevision);
    FRaidInventoryResult ConsumeWithCommit(int32 EntryId, int32 Quantity, int64 ExpectedRevision,
        TFunctionRef<void()> CommitEffect);
    FRaidTransferResult TransferTo(URaidInventoryComponent* Destination, const FRaidTransferRequest& Request);

    // All affected state is committed before broadcasting. Writes from callbacks
    // are rejected until the current mutation and its notifications have ended.
    FRaidInventoryChanged OnInventoryChanged;
    FRaidInventoryEndingPlay OnInventoryEndingPlay;

private:
    UPROPERTY(Transient) FRaidInventoryState State;
    bool bMutationInProgress = false;
    bool bEndingPlay = false;

    FRaidInventoryResult RejectedMutation(ERaidInventoryResultCode Code) const;
    ERaidInventoryResultCode GetMutationGate() const;
    void BroadcastCommittedChange();
};
