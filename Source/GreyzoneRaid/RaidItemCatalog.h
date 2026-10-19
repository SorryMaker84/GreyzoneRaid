#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RaidInventoryTypes.h"
#include "RaidItemCatalog.generated.h"

/** Authored item behavior. Runtime legacy labels/values preserve their R5 variants. */
UCLASS(BlueprintType)
class GREYZONERAID_API URaidItemCatalog : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
    TArray<FRaidItemDefinition> Definitions;

    // Duplicate IDs, missing definitions and invalid catalog data are explicit
    // failures. This function never invents a healing/ammunition effect.
    bool ResolveLegacy(FName ItemId, const FString& Label, int32 Value,
        FRaidItemDefinition& OutDefinition, FString* OutError = nullptr) const;
    bool Validate(FString* OutError = nullptr) const;
};
