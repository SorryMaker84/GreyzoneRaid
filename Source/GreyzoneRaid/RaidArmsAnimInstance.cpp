#include "RaidArmsAnimInstance.h"
#include "RaidPlayer.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

namespace
{
struct FRaidArmsProxy final : FAnimInstanceProxy
{
    explicit FRaidArmsProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    UAnimSequence* Grip = nullptr;
    UAnimSequence* Reload = nullptr;
    UAnimSequence* Fire = nullptr;
    float ReloadPosition = 0.f;
    float ReloadWeight = 0.f;
    float ShotAge = 100.f;

    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        // Copy on the game thread; animation evaluation never queries gameplay.
        if (const ARaidPlayer* Player = Cast<ARaidPlayer>(Instance->GetOwningActor()))
        {
            Grip = Player->ArmsIdleAnimation;
            Reload = Player->ArmsReloadAnimation;
            Fire = Player->ArmsFireAnimation;
            ReloadPosition = Player->GetVisualReloadProgress();
            ReloadWeight = Player->GetVisualReloadWeight();
            ShotAge = Player->GetShotPresentationAge();
            if (URaidArmsAnimInstance* Arms = Cast<URaidArmsAnimInstance>(Instance))
            {
                Arms->SampledShotAge = ShotAge;
                Arms->SampledReloadPosition = ReloadPosition;
            }
        }
    }

    virtual bool Evaluate(FPoseContext& Output) override
    {
        Output.ResetToRefPose();
        UAnimSequence* Base = Fire && ShotAge >= 0.f && ShotAge < Fire->GetPlayLength() ? Fire : Grip;
        if (!Base) return true;
        FAnimationPoseData BaseData(Output);
        Base->GetAnimationPose(BaseData, FAnimExtractContext(Base == Fire ? ShotAge : 0.0, false));
        if (Reload && ReloadWeight > KINDA_SMALL_NUMBER)
        {
            FPoseContext ReloadPose(Output);
            ReloadPose.ResetToRefPose();
            FAnimationPoseData ReloadData(ReloadPose);
            Reload->GetAnimationPose(ReloadData, FAnimExtractContext(ReloadPosition * Reload->GetPlayLength(), false));
            // Cancellation also samples this solved path; interpolating the separate
            // hand-control and forearm hierarchies would twist the wrist in mid-blend.
            FAnimationRuntime::BlendTwoPosesTogetherInPlace(BaseData, ReloadData, 1.f - ReloadWeight);
        }
        return true;
    }
};
}

FAnimInstanceProxy* URaidArmsAnimInstance::CreateAnimInstanceProxy() { return new FRaidArmsProxy(this); }
void URaidArmsAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) { delete InProxy; }
