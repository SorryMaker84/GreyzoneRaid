#include "RaidItemCatalog.h"
#include "RaidInventoryModel.h"

bool URaidItemCatalog::Validate(FString* OutError) const
{
    if (OutError) OutError->Reset();
    if (Definitions.IsEmpty())
    {
        if (OutError) *OutError = TEXT("Item catalog has no definitions.");
        return false;
    }
    TSet<FName> Seen;
    for (const FRaidItemDefinition& Definition : Definitions)
    {
        if (!FRaidInventoryModel::IsValidDefinition(Definition))
        {
            if (OutError) *OutError = FString::Printf(TEXT("Invalid item definition: %s."), *Definition.ItemId.ToString());
            return false;
        }
        if (Seen.Contains(Definition.ItemId))
        {
            if (OutError) *OutError = FString::Printf(TEXT("Duplicate catalog item ID: %s."), *Definition.ItemId.ToString());
            return false;
        }
        Seen.Add(Definition.ItemId);
    }
    return true;
}

bool URaidItemCatalog::ResolveLegacy(FName ItemId, const FString& Label, int32 Value,
    FRaidItemDefinition& OutDefinition, FString* OutError) const
{
    OutDefinition = FRaidItemDefinition();
    if (!Validate(OutError)) return false;
    const FRaidItemDefinition* Found = Definitions.FindByPredicate([ItemId](const FRaidItemDefinition& Definition)
        { return Definition.ItemId == ItemId; });
    if (!Found)
    {
        if (OutError) *OutError = FString::Printf(TEXT("Item ID is absent from the catalog: %s."), *ItemId.ToString());
        return false;
    }
    // Item behavior comes only from the catalog. R5's authored label and value
    // variants remain distinct stack keys; old negative values still clamp to 0.
    OutDefinition = *Found;
    OutDefinition.DisplayName = Label;
    OutDefinition.UnitValue = FMath::Max(0, Value);
    return true;
}
