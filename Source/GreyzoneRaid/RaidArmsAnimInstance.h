#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "RaidArmsAnimInstance.generated.h"

/** Small native pose evaluator: gameplay owns timing; this class only blends arms. */
UCLASS(Transient)
class GREYZONERAID_API URaidArmsAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    // Copied on the game thread in PreUpdate, for opt-in synchronization evidence.
    float SampledShotAge = 100.f;
    float SampledReloadPosition = 0.f;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
