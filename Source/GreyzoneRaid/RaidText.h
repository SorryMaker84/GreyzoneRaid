#pragma once

#include "CoreMinimal.h"

class AActor;
class UFont;

/** Shared world-label font. Canvas uses its separate runtime composite font. */
namespace RaidText
{
    UFont* LoadWorldFont();
    void ApplyWorldFont(AActor* Actor);
}
