#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RaidEnemy.generated.h"

class AAIController;
class ARaidPlayer;
class UAnimationAsset;
class UAnimSequence;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;

/** 灰盒敌人：导航巡逻 -> 发现玩家 -> 追击并射击。 */
UCLASS(Blueprintable)
class GREYZONERAID_API ARaidEnemy : public ACharacter
{
    GENERATED_BODY()

public:
    ARaidEnemy();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

    // 巡逻点是关卡中的绝对世界坐标；可在蓝图或关卡实例中配置。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|AI")
    TArray<FVector> PatrolPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Combat")
    float Health = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Combat", meta=(ClampMin="1.0"))
    float MaxHealth = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|AI", meta=(ClampMin="0.0"))
    float DetectionRadius = 1400.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Combat", meta=(ClampMin="0.0"))
    float AttackRange = 850.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Combat", meta=(ClampMin="0.0"))
    float AttackDamage = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Combat", meta=(ClampMin="0.1"))
    float AttackInterval = 1.2f;

    UPROPERTY(BlueprintReadOnly, Category="Raid|AI")
    FString StateText = TEXT("Patrol");

    UFUNCTION(BlueprintPure, Category="Raid|AI")
    int32 GetPatrolRecoveryCount() const { return PatrolRecoveryCount; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Visuals")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Visuals")
    TObjectPtr<UStaticMeshComponent> HeadMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Visuals")
    TObjectPtr<UStaticMeshComponent> GunMesh;

    // 使用本机已安装的 UE 模板动画，可在蓝图里替换兼容骨骼动画。
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Raid|Animation")
    TObjectPtr<UAnimSequence> RifleIdleAnimation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Raid|Animation")
    TObjectPtr<UAnimSequence> RifleWalkAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Audio")
    TObjectPtr<USoundBase> ShotSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Audio")
    TObjectPtr<USoundBase> AlertSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Audio")
    TObjectPtr<USoundBase> FootstepSound;

private:
    bool CanSeePlayer(const ARaidPlayer* Player) const;
    bool HasClearShot(const ARaidPlayer* Player, FHitResult& Hit) const;
    bool FindSideStep(FVector& Destination, float Distance, bool bSearching);
    bool FindLootDropLocation(FVector& Location) const;
    void UpdatePatrol(AAIController* AI, float DeltaSeconds);
    void ResetPatrolProgress();
    void ShootAtPlayer(ARaidPlayer* Player);
    void Die();
    void UpdatePresentation(float DeltaSeconds);
    void UpdateLimb(UStaticMeshComponent* Part, const FVector& Start, const FVector& End, float Width);
    void PlaySpatialSound(UAudioComponent* Audio, USoundBase* Sound, float Volume, float Pitch = 1.f);

    UPROPERTY() TObjectPtr<USceneComponent> VisualRoot;
    UPROPERTY() TObjectPtr<USceneComponent> TorsoRoot;
    UPROPERTY() TObjectPtr<USceneComponent> HeadRoot;
    UPROPERTY() TObjectPtr<USceneComponent> WeaponRoot;
    UPROPERTY() TObjectPtr<USceneComponent> Muzzle;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> MuzzleFlash;
    UPROPERTY() TObjectPtr<UPointLightComponent> MuzzleLight;
    UPROPERTY() TObjectPtr<UAudioComponent> WeaponAudio;
    UPROPERTY() TObjectPtr<UAudioComponent> VoiceAudio;
    UPROPERTY() TObjectPtr<UAudioComponent> FootstepAudio;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftUpperArm;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightUpperArm;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftForearm;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightForearm;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftHand;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightHand;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftThigh;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightThigh;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftCalf;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightCalf;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftBoot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightBoot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftKnee;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightKnee;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> UniformParts;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ArmorParts;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HelmetParts;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> DarkParts;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FallbackParts;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> UniformMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ArmorMaterial;
    UPROPERTY() TObjectPtr<UAnimationAsset> PlayingAnimation;

    UPROPERTY()
    TObjectPtr<ARaidPlayer> TargetPlayer;

    FVector LastKnownPlayerLocation = FVector::ZeroVector;
    int32 PatrolIndex = 0;
    int32 PatrolObservedIndex = INDEX_NONE;
    int32 PatrolFailedMoveRequests = 0;
    int32 PatrolRecoveryCount = 0;
    FVector PatrolProgressLocation = FVector::ZeroVector;
    float PatrolNoProgressSeconds = 0.f;
    float PatrolStartupGraceRemaining = 3.f;
    float MoveUpdateRemaining = 0.f;
    float ShotCooldown = 0.f;
    float TargetMemoryRemaining = 0.f;
    float AlertRemaining = 0.f;
    float HitReactRemaining = 0.f;
    float ReaimRemaining = 0.f;
    float CombatStepRemaining = 0.f;
    float RepositionRemaining = 0.f;
    float GaitPhase = 0.f;
    float FootstepRemaining = 0.f;
    float FlashRemaining = 0.f;
    float HitFlashRemaining = 0.f;
    float WeaponRecoil = 0.f;
    float AimBlend = 0.f;
    float DeathElapsed = 0.f;
    float PatrolWaitRemaining = 0.f;
    FVector RepositionLocation = FVector::ZeroVector;
    float SideStepSign = 1.f;
    int32 ShotsSinceStep = 0;
    bool bHadClearShot = false;
    bool bTemplateBody = false;
    bool bDead = false;
};
