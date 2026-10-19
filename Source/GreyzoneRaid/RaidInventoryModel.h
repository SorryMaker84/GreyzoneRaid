#pragma once

#include "CoreMinimal.h"
#include "RaidInventoryTypes.h"
#include "RaidInventoryModel.generated.h"

/** Plain data owned by one inventory component; usable without actors or a world. */
USTRUCT()
struct GREYZONERAID_API FRaidInventoryState
{
    GENERATED_BODY()

    UPROPERTY() TArray<FRaidInventoryEntry> Entries;
    UPROPERTY() int64 Revision = 0;
    UPROPERTY() int64 NextEntryId = 1;
    UPROPERTY() int32 Capacity = -1;
    UPROPERTY() int32 WeightCapacityGrams = -1;
    UPROPERTY() bool bKeepEmptyRows = false;
    UPROPERTY() bool bInitialized = false;
};

/** Value-only validation and commits. No actor, UI, delegate or world access. */
class GREYZONERAID_API FRaidInventoryModel
{
public:
    static bool IsValidDefinition(const FRaidItemDefinition& Definition);
    static bool HasSameStackKey(const FRaidItemDefinition& A, const FRaidItemDefinition& B);
    static bool AreDefinitionsCompatible(const FRaidItemDefinition& A, const FRaidItemDefinition& B);
    static const FRaidInventoryEntry* FindEntry(const FRaidInventoryState& State, int32 EntryId);
    static int32 GetItemCount(const FRaidInventoryState& State);
    static int64 GetTotalValue(const FRaidInventoryState& State);
    static int64 GetWeightGrams(const FRaidInventoryState& State);

    static FRaidInventoryResult InitializeOnce(FRaidInventoryState& State,
        const TArray<FRaidInventorySeed>& Seeds, int32 Capacity, bool bKeepEmptyRows, int32 WeightCapacityGrams = -1);
    static FRaidInventoryResult Reveal(FRaidInventoryState& State, int32 EntryId, int64 ExpectedRevision);
    static FRaidInventoryResult Grant(FRaidInventoryState& State,
        const FRaidItemDefinition& Definition, int32 Quantity, int64 ExpectedRevision);
    static FRaidInventoryResult Consume(FRaidInventoryState& State,
        int32 EntryId, int32 Quantity, int64 ExpectedRevision);
    static FRaidTransferResult Transfer(FRaidInventoryState& Source,
        FRaidInventoryState& Destination, const FRaidTransferRequest& Request);
};
