#include "RaidInventoryModel.h"

namespace
{
    using ECode = ERaidInventoryResultCode;

    FRaidInventoryResult Result(ECode Code, const FRaidInventoryState& State,
        int32 Quantity = 0, int32 EntryId = INDEX_NONE)
    {
        FRaidInventoryResult Out;
        Out.Code = Code;
        Out.Quantity = Quantity;
        Out.EntryId = EntryId;
        Out.Revision = State.Revision;
        return Out;
    }

    FRaidTransferResult TransferResult(ECode Code, const FRaidInventoryState& Source,
        const FRaidInventoryState& Destination, int32 Quantity = 0, int32 EntryId = INDEX_NONE)
    {
        FRaidTransferResult Out;
        Out.Code = Code;
        Out.Quantity = Quantity;
        Out.DestinationEntryId = EntryId;
        Out.SourceRevision = Source.Revision;
        Out.DestinationRevision = Destination.Revision;
        return Out;
    }

    int32 FindIndex(const FRaidInventoryState& State, int32 EntryId)
    {
        return State.Entries.IndexOfByPredicate([EntryId](const FRaidInventoryEntry& Entry)
            { return Entry.EntryId == EntryId; });
    }

    // The component cannot expose mutable state. Keeping validation here also
    // makes malformed imported states safe when the model is used independently.
    ECode ValidateState(const FRaidInventoryState& State)
    {
        if (!State.bInitialized) return ECode::NotInitialized;
        if (State.Capacity < -1 || State.WeightCapacityGrams < -1 || State.Revision < 1 || State.NextEntryId < 1
            || State.NextEntryId > static_cast<int64>(MAX_int32) + 1) return ECode::InvalidState;
        int64 Count = 0;
        TSet<int32> Handles;
        for (int32 Index = 0; Index < State.Entries.Num(); ++Index)
        {
            const FRaidInventoryEntry& Entry = State.Entries[Index];
            if (Entry.EntryId <= 0 || Entry.EntryId >= State.NextEntryId || Handles.Contains(Entry.EntryId)
                || Entry.Quantity < 0 || (!State.bKeepEmptyRows && Entry.Quantity == 0)
                || !FRaidInventoryModel::IsValidDefinition(Entry.Definition)) return ECode::InvalidState;
            Handles.Add(Entry.EntryId);
            Count += Entry.Quantity;
            if (Count > MAX_int32) return ECode::Overflow;
            for (int32 Previous = 0; Previous < Index; ++Previous)
            {
                const FRaidItemDefinition& Other = State.Entries[Previous].Definition;
                if (FRaidInventoryModel::HasSameStackKey(Entry.Definition, Other)
                    && !FRaidInventoryModel::AreDefinitionsCompatible(Entry.Definition, Other))
                    return ECode::IncompatibleDefinition;
            }
        }
        if (State.WeightCapacityGrams >= 0 && FRaidInventoryModel::GetWeightGrams(State) > State.WeightCapacityGrams)
            return ECode::InvalidState;
        return State.Capacity >= 0 && Count > State.Capacity ? ECode::InvalidState : ECode::Success;
    }

    ECode CheckRevision(const FRaidInventoryState& State, int64 ExpectedRevision)
    {
        const ECode Validation = ValidateState(State);
        if (Validation != ECode::Success) return Validation;
        return State.Revision == ExpectedRevision ? ECode::Success : ECode::StaleRevision;
    }

    // Known items must not merge into an unknown row: doing so would either hide
    // the returned item or reveal unrelated contents without searching.
    ECode FindMergeTarget(const FRaidInventoryState& State, const FRaidItemDefinition& Definition,
        int32& OutIndex)
    {
        OutIndex = INDEX_NONE;
        for (int32 Index = 0; Index < State.Entries.Num(); ++Index)
        {
            const FRaidInventoryEntry& Entry = State.Entries[Index];
            if (!FRaidInventoryModel::HasSameStackKey(Entry.Definition, Definition)) continue;
            if (!FRaidInventoryModel::AreDefinitionsCompatible(Entry.Definition, Definition))
                return ECode::IncompatibleDefinition;
            if (OutIndex == INDEX_NONE && Entry.bRevealed) OutIndex = Index;
        }
        return ECode::Success;
    }

    int64 FreeCapacity(const FRaidInventoryState& State)
    {
        const int64 Limit = State.Capacity < 0 ? MAX_int32 : State.Capacity;
        return Limit - FRaidInventoryModel::GetItemCount(State);
    }

    int64 FreeWeightUnits(const FRaidInventoryState& State, const FRaidItemDefinition& Item)
    {
        return State.WeightCapacityGrams < 0 ? MAX_int32 :
            FMath::Max<int64>(0, State.WeightCapacityGrams - FRaidInventoryModel::GetWeightGrams(State)) / Item.UnitWeightGrams;
    }

    ECode CheckAppend(const FRaidInventoryState& State, int32 ExistingIndex)
    {
        if (ExistingIndex != INDEX_NONE) return ECode::Success;
        return State.NextEntryId > MAX_int32 || State.Entries.Num() == MAX_int32
            ? ECode::Overflow : ECode::Success;
    }

    int32 CommitKnownAddition(FRaidInventoryState& State, const FRaidItemDefinition& Definition,
        int32 Quantity, int32 ExistingIndex)
    {
        if (ExistingIndex != INDEX_NONE)
        {
            FRaidInventoryEntry& Entry = State.Entries[ExistingIndex];
            Entry.Quantity += Quantity;
            return Entry.EntryId;
        }
        FRaidInventoryEntry Entry;
        Entry.EntryId = static_cast<int32>(State.NextEntryId++);
        Entry.Definition = Definition;
        Entry.Quantity = Quantity;
        Entry.bRevealed = true;
        const int32 NewId = Entry.EntryId;
        State.Entries.Add(MoveTemp(Entry));
        return NewId;
    }
}

bool FRaidInventoryModel::IsValidDefinition(const FRaidItemDefinition& Definition)
{
    if (Definition.ItemId.IsNone() || Definition.UnitValue < 0 || Definition.UseAmount < 0
        || Definition.UnitWeightGrams <= 0 || Definition.UnitWeightGrams > 1000000) return false;
    switch (Definition.UseEffect)
    {
    case ERaidItemUseEffect::None:
        return Definition.UseAmount == 0;
    case ERaidItemUseEffect::Heal:
    case ERaidItemUseEffect::Ammo:
        return Definition.UseAmount > 0;
    default:
        return false;
    }
}

bool FRaidInventoryModel::HasSameStackKey(const FRaidItemDefinition& A, const FRaidItemDefinition& B)
{
    return A.ItemId == B.ItemId && A.DisplayName == B.DisplayName && A.UnitValue == B.UnitValue;
}

bool FRaidInventoryModel::AreDefinitionsCompatible(const FRaidItemDefinition& A, const FRaidItemDefinition& B)
{
    return HasSameStackKey(A, B) && A.UseEffect == B.UseEffect && A.UseAmount == B.UseAmount
        && A.UnitWeightGrams == B.UnitWeightGrams;
}

const FRaidInventoryEntry* FRaidInventoryModel::FindEntry(const FRaidInventoryState& State, int32 EntryId)
{
    return State.Entries.FindByPredicate([EntryId](const FRaidInventoryEntry& Entry)
        { return Entry.EntryId == EntryId; });
}

int32 FRaidInventoryModel::GetItemCount(const FRaidInventoryState& State)
{
    int64 Count = 0;
    for (const FRaidInventoryEntry& Entry : State.Entries)
    {
        Count += FMath::Max(0, Entry.Quantity);
        if (Count >= MAX_int32) return MAX_int32;
    }
    return static_cast<int32>(Count);
}

int64 FRaidInventoryModel::GetTotalValue(const FRaidInventoryState& State)
{
    int64 Value = 0;
    for (const FRaidInventoryEntry& Entry : State.Entries)
    {
        const int64 RowValue = static_cast<int64>(FMath::Max(0, Entry.Quantity))
            * FMath::Max(0, Entry.Definition.UnitValue);
        if (RowValue > MAX_int64 - Value) return MAX_int64;
        Value += RowValue;
    }
    return Value;
}

int64 FRaidInventoryModel::GetWeightGrams(const FRaidInventoryState& State)
{
    int64 Grams = 0;
    for (const FRaidInventoryEntry& Entry : State.Entries)
        Grams += static_cast<int64>(FMath::Max(0, Entry.Quantity)) * FMath::Max(0, Entry.Definition.UnitWeightGrams);
    return Grams;
}

FRaidInventoryResult FRaidInventoryModel::InitializeOnce(FRaidInventoryState& State,
    const TArray<FRaidInventorySeed>& Seeds, int32 Capacity, bool bKeepEmptyRows, int32 WeightCapacityGrams)
{
    if (State.bInitialized) return Result(ECode::AlreadyInitialized, State);
    if (Capacity < -1 || WeightCapacityGrams < -1) return Result(ECode::InvalidCapacity, State);
    if (!State.Entries.IsEmpty() || State.Revision < 0 || State.NextEntryId != 1)
        return Result(ECode::InvalidState, State);
    if (State.Revision == MAX_int64) return Result(ECode::Overflow, State);

    int64 Count = 0, Grams = 0;
    for (int32 Index = 0; Index < Seeds.Num(); ++Index)
    {
        const FRaidInventorySeed& Seed = Seeds[Index];
        if (!IsValidDefinition(Seed.Definition)) return Result(ECode::InvalidDefinition, State);
        if (Seed.Quantity < 0) return Result(ECode::InvalidQuantity, State);
        Count += Seed.Quantity;
        Grams += static_cast<int64>(Seed.Quantity) * Seed.Definition.UnitWeightGrams;
        if (Count > MAX_int32) return Result(ECode::Overflow, State);
        for (int32 Previous = 0; Previous < Index; ++Previous)
        {
            if (HasSameStackKey(Seed.Definition, Seeds[Previous].Definition)
                && !AreDefinitionsCompatible(Seed.Definition, Seeds[Previous].Definition))
                return Result(ECode::IncompatibleDefinition, State);
        }
    }
    if (Capacity >= 0 && Count > Capacity) return Result(ECode::InsufficientCapacity, State);
    if (WeightCapacityGrams >= 0 && Grams > WeightCapacityGrams) return Result(ECode::InsufficientCapacity, State);

    // Build the initial rows off to the side. An invalid later seed cannot leave
    // an earlier row, a consumed handle, or an initialized flag behind.
    TArray<FRaidInventoryEntry> Entries;
    Entries.Reserve(Seeds.Num());
    int64 NextId = 1;
    for (const FRaidInventorySeed& Seed : Seeds)
    {
        if (Seed.Quantity == 0 && !bKeepEmptyRows) continue;
        FRaidInventoryEntry Entry;
        Entry.EntryId = static_cast<int32>(NextId++);
        Entry.Definition = Seed.Definition;
        Entry.Quantity = Seed.Quantity;
        Entry.bRevealed = Seed.bRevealed;
        Entries.Add(MoveTemp(Entry));
    }
    State.Entries = MoveTemp(Entries);
    State.Capacity = Capacity;
    State.WeightCapacityGrams = WeightCapacityGrams;
    State.bKeepEmptyRows = bKeepEmptyRows;
    State.NextEntryId = NextId;
    State.bInitialized = true;
    ++State.Revision;
    return Result(ECode::Success, State, static_cast<int32>(Count));
}

FRaidInventoryResult FRaidInventoryModel::Reveal(FRaidInventoryState& State, int32 EntryId, int64 ExpectedRevision)
{
    const ECode Gate = CheckRevision(State, ExpectedRevision);
    if (Gate != ECode::Success) return Result(Gate, State);
    const int32 Index = FindIndex(State, EntryId);
    if (Index == INDEX_NONE) return Result(ECode::InvalidEntry, State);
    FRaidInventoryEntry& Entry = State.Entries[Index];
    if (Entry.Quantity <= 0) return Result(ECode::InsufficientQuantity, State);
    if (Entry.bRevealed) return Result(ECode::AlreadyRevealed, State);
    if (State.Revision == MAX_int64) return Result(ECode::Overflow, State);
    Entry.bRevealed = true;
    ++State.Revision;
    return Result(ECode::Success, State, 0, EntryId);
}

FRaidInventoryResult FRaidInventoryModel::Grant(FRaidInventoryState& State,
    const FRaidItemDefinition& Definition, int32 Quantity, int64 ExpectedRevision)
{
    const ECode Gate = CheckRevision(State, ExpectedRevision);
    if (Gate != ECode::Success) return Result(Gate, State);
    if (!IsValidDefinition(Definition)) return Result(ECode::InvalidDefinition, State);
    if (Quantity <= 0) return Result(ECode::InvalidQuantity, State);
    int32 ExistingIndex = INDEX_NONE;
    const ECode Merge = FindMergeTarget(State, Definition, ExistingIndex);
    if (Merge != ECode::Success) return Result(Merge, State);
    const int64 NewCount = static_cast<int64>(GetItemCount(State)) + Quantity;
    if (NewCount > MAX_int32) return Result(ECode::Overflow, State);
    if (Quantity > FreeCapacity(State)) return Result(ECode::InsufficientCapacity, State);
    if (Quantity > FreeWeightUnits(State, Definition)) return Result(ECode::InsufficientCapacity, State);
    const ECode Append = CheckAppend(State, ExistingIndex);
    if (Append != ECode::Success) return Result(Append, State);
    if (State.Revision == MAX_int64) return Result(ECode::Overflow, State);

    const int32 EntryId = CommitKnownAddition(State, Definition, Quantity, ExistingIndex);
    ++State.Revision;
    return Result(ECode::Success, State, Quantity, EntryId);
}

FRaidInventoryResult FRaidInventoryModel::Consume(FRaidInventoryState& State,
    int32 EntryId, int32 Quantity, int64 ExpectedRevision)
{
    const ECode Gate = CheckRevision(State, ExpectedRevision);
    if (Gate != ECode::Success) return Result(Gate, State);
    if (Quantity <= 0) return Result(ECode::InvalidQuantity, State);
    const int32 Index = FindIndex(State, EntryId);
    if (Index == INDEX_NONE) return Result(ECode::InvalidEntry, State);
    FRaidInventoryEntry& Entry = State.Entries[Index];
    if (!Entry.bRevealed) return Result(ECode::HiddenEntry, State);
    if (Quantity > Entry.Quantity) return Result(ECode::InsufficientQuantity, State);
    if (State.Revision == MAX_int64) return Result(ECode::Overflow, State);

    Entry.Quantity -= Quantity;
    if (Entry.Quantity == 0 && !State.bKeepEmptyRows) State.Entries.RemoveAt(Index);
    ++State.Revision;
    return Result(ECode::Success, State, Quantity, EntryId);
}

FRaidTransferResult FRaidInventoryModel::Transfer(FRaidInventoryState& Source,
    FRaidInventoryState& Destination, const FRaidTransferRequest& Request)
{
    if (&Source == &Destination) return TransferResult(ECode::SelfTransfer, Source, Destination);
    const ECode SourceGate = CheckRevision(Source, Request.ExpectedSourceRevision);
    if (SourceGate != ECode::Success) return TransferResult(SourceGate, Source, Destination);
    const ECode DestinationGate = CheckRevision(Destination, Request.ExpectedDestinationRevision);
    if (DestinationGate != ECode::Success) return TransferResult(DestinationGate, Source, Destination);
    if (Request.Quantity <= 0) return TransferResult(ECode::InvalidQuantity, Source, Destination);
    if (Request.Policy != ERaidTransferPolicy::Exact && Request.Policy != ERaidTransferPolicy::UpToCapacity)
        return TransferResult(ECode::InvalidPolicy, Source, Destination);

    const int32 SourceIndex = FindIndex(Source, Request.SourceEntryId);
    if (SourceIndex == INDEX_NONE) return TransferResult(ECode::InvalidEntry, Source, Destination);
    FRaidInventoryEntry& SourceEntry = Source.Entries[SourceIndex];
    if (!SourceEntry.bRevealed) return TransferResult(ECode::HiddenEntry, Source, Destination);
    if (SourceEntry.Quantity <= 0) return TransferResult(ECode::InsufficientQuantity, Source, Destination);
    int32 DestinationIndex = INDEX_NONE;
    const ECode Merge = FindMergeTarget(Destination, SourceEntry.Definition, DestinationIndex);
    if (Merge != ECode::Success) return TransferResult(Merge, Source, Destination);

    const int64 Free = FMath::Min(FreeCapacity(Destination), FreeWeightUnits(Destination, SourceEntry.Definition));
    int32 Moved = Request.Quantity;
    if (Request.Policy == ERaidTransferPolicy::Exact)
    {
        if (Moved > SourceEntry.Quantity) return TransferResult(ECode::InsufficientQuantity, Source, Destination);
        if (static_cast<int64>(GetItemCount(Destination)) + Moved > MAX_int32)
            return TransferResult(ECode::Overflow, Source, Destination);
        if (Moved > Free) return TransferResult(ECode::InsufficientCapacity, Source, Destination);
    }
    else
    {
        Moved = static_cast<int32>(FMath::Min<int64>(FMath::Min(Moved, SourceEntry.Quantity), Free));
        if (Moved <= 0) return TransferResult(ECode::InsufficientCapacity, Source, Destination);
    }
    const ECode Append = CheckAppend(Destination, DestinationIndex);
    if (Append != ECode::Success) return TransferResult(Append, Source, Destination);
    if (Source.Revision == MAX_int64 || Destination.Revision == MAX_int64)
        return TransferResult(ECode::Overflow, Source, Destination);

    // All failure paths end above. Commit both ledgers before the component can
    // notify any observer; transferring never calls the one-sided Grant API.
    const int32 DestinationId = CommitKnownAddition(Destination, SourceEntry.Definition, Moved, DestinationIndex);
    SourceEntry.Quantity -= Moved;
    if (SourceEntry.Quantity == 0 && !Source.bKeepEmptyRows) Source.Entries.RemoveAt(SourceIndex);
    ++Source.Revision;
    ++Destination.Revision;
    return TransferResult(ECode::Success, Source, Destination, Moved, DestinationId);
}
