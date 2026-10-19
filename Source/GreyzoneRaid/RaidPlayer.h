#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RaidInventoryTypes.h"
#include "RaidWeaponTuning.h"
#include "RaidPlayer.generated.h"

class ARaidLoot;
class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UMaterialInterface;
class USoundBase;
class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequence;
class UAudioComponent;

class URaidInventoryComponent;
class URaidInteractionComponent;
class URaidItemCatalog;
class URaidTreatmentComponent;

/** A small, readable first-person character using the engine's movement component. */
UCLASS(Blueprintable)
class GREYZONERAID_API ARaidPlayer : public ACharacter
{
    GENERATED_BODY()

public:
    ARaidPlayer();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

    UFUNCTION(BlueprintCallable) void Fire();
    UFUNCTION(BlueprintCallable) void Reload();
    UFUNCTION(BlueprintCallable) void CancelReload();
    UFUNCTION(BlueprintCallable) void Interact();
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void StartFire();
    void StopFire();
    UFUNCTION(BlueprintCallable) void StartAim();
    UFUNCTION(BlueprintCallable) void StopAim();
    UFUNCTION(BlueprintCallable) void BeginSearch();
    UFUNCTION(BlueprintCallable) void CancelSearch();
    UFUNCTION(BlueprintCallable) bool OpenLoot(ARaidLoot* Loot);
    UFUNCTION(BlueprintCallable) void CloseLoot();
    UFUNCTION(BlueprintPure) ARaidLoot* GetOpenLoot() const;
    UFUNCTION(BlueprintPure) bool CanAccessLoot(const ARaidLoot* Loot) const;
    UFUNCTION(BlueprintCallable) bool TakeLootItem(int32 Index);
    UFUNCTION(BlueprintCallable) bool StoreInventoryItem(int32 Index);
    UFUNCTION(BlueprintCallable) bool UseInventoryItem(int32 Index);
    UFUNCTION(BlueprintPure) int64 GetInteractionRevision() const;
    UFUNCTION(BlueprintPure) int32 GetSearchIndex() const;

    UFUNCTION(BlueprintCallable) bool AddItem(FName ItemId, const FString& Label, int32 Quantity, int32 Value);
    UFUNCTION(BlueprintPure) bool IsAlive() const { return Health > 0.f; }
    UFUNCTION(BlueprintPure) ARaidLoot* GetFocusedLoot() const;
    UFUNCTION(BlueprintPure) bool IsSearching() const;
    UFUNCTION(BlueprintPure) float GetSearchProgress() const;

    UFUNCTION(BlueprintPure) URaidInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
    UFUNCTION(BlueprintPure) URaidInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
    URaidTreatmentComponent* GetTreatmentComponent() const { return TreatmentComponent; }
    bool IsTreating() const;
    void CancelTreatment();
    float GetLoadSpeedScale() const;
    bool IsTooHeavyToSprint() const;
    const URaidItemCatalog* GetItemCatalog() const { return ItemCatalog; }
    UFUNCTION(BlueprintPure) bool IsInventoryOpen() const;
    UFUNCTION(BlueprintPure) int32 GetLootCount() const;
    UFUNCTION(BlueprintPure) int64 GetTotalLootValue() const;
    bool CanAct() const;
    void PrepareInventoryInput();
    void PreparePauseInput();
    void StopInteractionForUnpossess();
    void RefreshInputMode();
    void RecordLootReceipt(const FString& Label, int32 Quantity);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View") UCameraComponent* Camera;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon") int32 MaxMagazineAmmo = 12;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon") FString WeaponDisplayName = TEXT("AK-47");
    UPROPERTY(BlueprintReadOnly, Category = "Raid") float Health = 100.f;
    UPROPERTY(BlueprintReadOnly, Category = "Weapon") int32 MagazineAmmo = 12;
    UPROPERTY(BlueprintReadOnly, Category = "Weapon") int32 ReserveAmmo = 60;
    UPROPERTY(BlueprintReadOnly, Category = "Weapon") bool bReloading = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Weapon") FRaidWeaponTuning WeaponTuning;
    UPROPERTY(BlueprintReadOnly, Category="Weapon") bool bAutomaticReload = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Raid") int32 MaxLootItems = 8;
    UPROPERTY(EditDefaultsOnly, Category="Raid|Load", meta=(ClampMin="1")) int32 MaxLootWeightGrams = 12000;
    UPROPERTY(BlueprintReadOnly, Category = "Weapon") float LastHitTime = -100.f;
    UPROPERTY(BlueprintReadOnly, Category = "Weapon") float LastKillTime = -100.f;
    UPROPERTY(BlueprintReadOnly, Category = "Raid") float LastDamageTime = -100.f;
    UPROPERTY(BlueprintReadOnly, Category = "Raid") float DamageSourceYaw = 0.f;
    UPROPERTY(BlueprintReadOnly, Category = "Raid") bool bHasDamageSource = false;
    // Captured on the lethal hit, before the source can move or be destroyed.
    UPROPERTY(BlueprintReadOnly, Category = "Raid") FString DeathCause;
    UPROPERTY(BlueprintReadOnly, Category = "Raid") float LastLootTime = -100.f;
    UPROPERTY(BlueprintReadOnly, Category = "Raid") FString LastLootLabel;
    UPROPERTY(BlueprintReadOnly, Category = "Raid") int32 LastLootQuantity = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Weapon") bool bAiming = false;
    UPROPERTY(BlueprintReadOnly, Category = "Weapon") float ADSAlpha = 0.f;
    UPROPERTY(BlueprintReadOnly, Category = "Weapon") float ReloadProgress = 0.f;
    float GetVisualReloadProgress() const { return VisualReloadProgress; }
    float GetVisualReloadWeight() const { return VisualReloadWeight; }
    float GetShotPresentationAge() const;

    // Optional assets can be assigned in a child blueprint; the original model is the fallback.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") UStaticMesh* WeaponModel = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") UStaticMesh* MagazineModel = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") FVector MagazinePivot = FVector(22.f,0.f,-8.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") FVector WeaponModelOffset = FVector::ZeroVector;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") FRotator WeaponModelRotation = FRotator::ZeroRotator;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") FVector WeaponModelScale = FVector::OneVector;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") FVector HipWeaponLocation = FVector(24.f,17.f,-16.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") FVector ADSWeaponLocation = FVector(24.f,0.f,-10.6f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") FVector MuzzleOffset = FVector(77.f,0.f,1.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") USkeletalMesh* ArmsModel = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") UAnimSequence* ArmsIdleAnimation = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") UAnimSequence* ArmsReloadAnimation = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") UAnimSequence* ArmsFireAnimation = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") FVector ArmsOffset = FVector::ZeroVector;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") FRotator ArmsRotation = FRotator::ZeroRotator;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Assets") FVector ArmsScale = FVector::OneVector;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Audio") USoundBase* FireSound = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Audio") USoundBase* ReloadSound = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Audio") USoundBase* EmptySound = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Audio") USoundBase* HitSound = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Audio") USoundBase* FootstepSound = nullptr;

private:
    // Opt-in integration probe may inspect presentation state without changing it.
    friend class ARaidR9TestDirector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid", meta=(AllowPrivateAccess="true"))
    TObjectPtr<URaidInventoryComponent> InventoryComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid", meta=(AllowPrivateAccess="true"))
    TObjectPtr<URaidInteractionComponent> InteractionComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid", meta=(AllowPrivateAccess="true"))
    TObjectPtr<URaidTreatmentComponent> TreatmentComponent;
    UPROPERTY(EditDefaultsOnly, Category="Raid|Items") TSoftObjectPtr<URaidItemCatalog> ItemCatalogAsset;
    UPROPERTY(Transient) TObjectPtr<URaidItemCatalog> ItemCatalog;
    bool bEndingPlay = false;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* GunMesh;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* BarrelMesh;
    UPROPERTY(VisibleAnywhere) UPointLightComponent* MuzzleLight;
    UPROPERTY(VisibleAnywhere) USceneComponent* WeaponRoot;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* ImportedWeapon;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* ImportedMagazine;
    UPROPERTY(VisibleAnywhere) USkeletalMeshComponent* ImportedArms;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* MagazineMesh;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* SupportHand;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* MuzzleFlame;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* MuzzleFlameCross;
    UPROPERTY(VisibleAnywhere) UStaticMeshComponent* OpticDot;
    UPROPERTY() TArray<UStaticMeshComponent*> WeaponParts;
    UPROPERTY() TArray<UStaticMeshComponent*> OriginalGunParts;
    UPROPERTY() TArray<UStaticMeshComponent*> OriginalArmsParts;
    UPROPERTY() TArray<UStaticMeshComponent*> CasingPool;
    UPROPERTY() TArray<UStaticMeshComponent*> TracerPool;
    UPROPERTY() UMaterialInterface* EffectMaterial = nullptr;
    TArray<FLinearColor> PartColors;
    TArray<FVector> PartPositions;
    TArray<FVector> CasingPositions;
    TArray<FVector> CasingVelocities;
    TArray<float> CasingAges;
    TArray<float> TracerAges;
    FTimerHandle FireTimer;
    FTimerHandle ReloadTimer;
    FTimerHandle ReloadSoundTimer;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> ActiveReloadAudio;
    FTimerHandle MuzzleTimer;
    float NextAllowedFireTime = 0.f;
    float ReloadStartedAt = -1.f;
    float VisualReloadProgress = 0.f;
    float VisualReloadWeight = 0.f;
    float ReloadRecoveryElapsed = -1.f;
    float ReloadRecoveryStartProgress = 0.f;
    float RecoilPitch = 0.f;
    float RecoilYaw = 0.f;
    float WeaponKick = 0.f;
    float WeaponTilt = 0.f;
    float SwayYaw = 0.f;
    float SwayPitch = 0.f;
    float LookInputX = 0.f;
    float LookInputY = 0.f;
    float GaitPhase = 0.f;
    float FootstepCooldown = 0.f;
    float SprintPoseAlpha = 0.f;
    float SearchPoseAlpha = 0.f;
    float DamageFlinch = 0.f;
    float DamageFlinchSide = 1.f;
    float LastShotTime = -100.f;
    bool bAimHeld = false;
    bool bFireHeld = false;
    int32 SuccessfulShots = 0;
    int32 ShotPresentationCount = 0;
    int32 ShotSoundRequestCount = 0;
    bool bSprintHeld = false;
    int32 NextCasing = 0;
    int32 NextTracer = 0;
    bool CanUseWorldInput() const;
    void UpdateWeaponPresentation(float DeltaSeconds);
    void SpawnShotPresentation(const FVector& EndPoint, bool bHit);
    void PlayWeaponSound(USoundBase* Sound, float Volume = 1.f) const;
    void FinishReload();
    void BeginReload(bool bAutomatic);
    void PlayReloadAudio();
    void StopReloadAudio();
    void FireWhileHeld();
    void ScheduleHeldFire();
    void HideMuzzleFlash();
    void StartSprint();
    void StopSprint();
    void StartJump();
    // Do not override ACharacter::Restart: UE uses it to initialise movement on possession.
    void Notify(const FString& Text) const;
};
