#pragma once
#include "CoreMinimal.h"
#include "RaidWeaponTuning.generated.h"

/** Designer-visible weapon timing and presentation; damage remains in the gameplay rules. */
USTRUCT(BlueprintType)
struct FRaidWeaponTuning
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0.05",ClampMax="1.0")) float ShotInterval=.18f;
    UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0.2",ClampMax="5.0")) float ReloadSeconds=1.5f;
    UPROPERTY(EditAnywhere, Category="Timing", meta=(ClampMin="0",ClampMax="0.3")) float ShotToReloadSeconds=.12f;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0",ClampMax="5")) float HipPitchKick=.85f;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0",ClampMax="5")) float ADSPitchKick=.42f;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0",ClampMax="2")) float HipYawKick=.16f;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0",ClampMax="2")) float ADSYawKick=.06f;
    UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="1",ClampMax="30")) float CameraReturnRate=9.f;
    UPROPERTY(EditAnywhere, Category="Pose", meta=(ClampMin="0",ClampMax="5")) float HipKickCm=1.8f;
    UPROPERTY(EditAnywhere, Category="Pose", meta=(ClampMin="0",ClampMax="5")) float ADSKickCm=.85f;
    UPROPERTY(EditAnywhere, Category="Pose", meta=(ClampMin="0",ClampMax="8")) float HipTiltDegrees=2.4f;
    UPROPERTY(EditAnywhere, Category="Pose", meta=(ClampMin="0",ClampMax="8")) float ADSTiltDegrees=1.1f;
    UPROPERTY(EditAnywhere, Category="Pose", meta=(ClampMin="1",ClampMax="40")) float WeaponReturnRate=18.f;
    UPROPERTY(EditAnywhere, Category="Pose", meta=(ClampMin="1",ClampMax="30")) float ADSTransitionRate=12.f;
};
