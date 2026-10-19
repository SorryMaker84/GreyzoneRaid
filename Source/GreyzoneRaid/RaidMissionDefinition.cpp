#include "RaidMissionDefinition.h"

bool URaidMissionDefinition::Validate(FString* OutError) const
{
    if (OutError) OutError->Reset();
    const bool bValid = !MissionId.IsNone() && !Title.TrimStartAndEnd().IsEmpty()
        && !Instruction.TrimStartAndEnd().IsEmpty() && !CompletedText.TrimStartAndEnd().IsEmpty()
        && FMath::IsFinite(HoldSeconds) && HoldSeconds >= .1f && HoldSeconds <= 60.f
        && FMath::IsFinite(InteractionRange) && InteractionRange >= 80.f && InteractionRange <= 1000.f;
    if (!bValid && OutError) *OutError = TEXT("Mission requires an ID, text, a finite hold duration and interaction range.");
    return bValid;
}
