#pragma once

#include "CoreMinimal.h"
#include "RaidInventoryTypes.generated.h"

UENUM(BlueprintType)
enum class ERaidItemUseEffect : uint8
{
    None,
    Heal,
    Ammo
};

/** Resolved item data. Inventory commands never mutate a definition. */
USTRUCT(BlueprintType)
struct GREYZONERAID_API FRaidItemDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ItemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) int32 UnitValue = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ERaidItemUseEffect UseEffect = ERaidItemUseEffect::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) int32 UseAmount = 0;
    // Integer grams keep capacity checks deterministic; data comes from the item catalog.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 UnitWeightGrams = 1000;
};

/** Authoring/import data; no runtime handle or revision is accepted from a seed. */
USTRUCT(BlueprintType)
struct GREYZONERAID_API FRaidInventorySeed
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRaidItemDefinition Definition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 Quantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRevealed = false;
};

/** EntryId survives array compaction and is never reused within this inventory. */
USTRUCT(BlueprintType)
struct GREYZONERAID_API FRaidInventoryEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 EntryId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) FRaidItemDefinition Definition;
    UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;
    UPROPERTY(BlueprintReadOnly) bool bRevealed = false;
};

UENUM(BlueprintType)
enum class ERaidTransferPolicy : uint8
{
    Exact,
    UpToCapacity
};

UENUM(BlueprintType)
enum class ERaidInventoryResultCode : uint8
{
    Success,
    NotInitialized,
    AlreadyInitialized,
    InvalidState,
    InvalidDefinition,
    IncompatibleDefinition,
    InvalidQuantity,
    InvalidCapacity,
    InvalidEntry,
    InvalidPolicy,
    HiddenEntry,
    AlreadyRevealed,
    StaleRevision,
    InsufficientQuantity,
    InsufficientCapacity,
    Overflow,
    SelfTransfer,
    InvalidDestination,
    Busy,
    EndingPlay
};

USTRUCT(BlueprintType)
struct GREYZONERAID_API FRaidInventoryResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) ERaidInventoryResultCode Code = ERaidInventoryResultCode::InvalidState;
    UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;
    UPROPERTY(BlueprintReadOnly) int32 EntryId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) int64 Revision = 0;

    bool IsSuccess() const { return Code == ERaidInventoryResultCode::Success; }
};

USTRUCT(BlueprintType)
struct GREYZONERAID_API FRaidTransferRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SourceEntryId = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ERaidTransferPolicy Policy = ERaidTransferPolicy::Exact;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 ExpectedSourceRevision = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 ExpectedDestinationRevision = 0;
};

USTRUCT(BlueprintType)
struct GREYZONERAID_API FRaidTransferResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) ERaidInventoryResultCode Code = ERaidInventoryResultCode::InvalidState;
    UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;
    UPROPERTY(BlueprintReadOnly) int32 DestinationEntryId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) int64 SourceRevision = 0;
    UPROPERTY(BlueprintReadOnly) int64 DestinationRevision = 0;

    bool IsSuccess() const { return Code == ERaidInventoryResultCode::Success; }
};
