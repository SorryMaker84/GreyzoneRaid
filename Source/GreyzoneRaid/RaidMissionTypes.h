#pragma once

#include "CoreMinimal.h"
#include "RaidMissionTypes.generated.h"

/** A read-only presentation value. It never grants completion or unlocks a route. */
USTRUCT(BlueprintType)
struct GREYZONERAID_API FRaidMissionSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bPresent = false;
    UPROPERTY(BlueprintReadOnly, meta=(ScriptName="is_in_progress")) bool bInProgress = false;
    UPROPERTY(BlueprintReadOnly) bool bCompleted = false;
    UPROPERTY(BlueprintReadOnly) FName MissionId;
    UPROPERTY(BlueprintReadOnly) FString Title;
    UPROPERTY(BlueprintReadOnly) FString Instruction;
    UPROPERTY(BlueprintReadOnly) FString StatusText;
    UPROPERTY(BlueprintReadOnly) FVector WorldLocation = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float Progress = 0.f;
    UPROPERTY(BlueprintReadOnly) float HoldSeconds = 6.f;
};
