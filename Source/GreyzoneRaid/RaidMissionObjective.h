#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidMissionTypes.h"
#include "RaidMissionObjective.generated.h"

class ARaidGameMode;
class ARaidPlayer;
class UBoxComponent;
class UMaterialInstanceDynamic;
class URaidMissionDefinition;
class UStaticMeshComponent;

/** Optional in-world terminal. This actor alone owns its hold and completion state. */
UCLASS(Blueprintable)
class GREYZONERAID_API ARaidMissionObjective : public AActor
{
    GENERATED_BODY()

public:
    ARaidMissionObjective();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    FRaidMissionSnapshot GetSnapshot() const;
    FName GetMissionId() const;
    bool HasValidDefinition() const;
    bool IsCompleted() const { return bCompleted; }
    bool IsInteractionActive() const { return ActivePlayer.IsValid(); }
    bool CanInteract(const ARaidPlayer* Player) const;
    bool TryBeginInteraction(ARaidPlayer* Player);
    void CancelInteraction(ARaidPlayer* Player = nullptr);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Raid|Mission")
    TObjectPtr<URaidMissionDefinition> MissionDefinition;

    // Actor origin is the floor surface. The map supplies the surrounding building.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Mission")
    TObjectPtr<UBoxComponent> InteractionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Visuals")
    TObjectPtr<UStaticMeshComponent> BaseMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Visuals")
    TObjectPtr<UStaticMeshComponent> ScreenMesh;

private:
    ARaidGameMode* GetRaidMode() const;
    void NotifyChanged();
    void RefreshVisuals();
    void UnbindPlayerDamage();
    void CompleteInteraction();

    UFUNCTION()
    void HandlePlayerDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
        AController* InstigatedBy, AActor* DamageCauser);

    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ScreenMaterial;
    TWeakObjectPtr<ARaidPlayer> ActivePlayer;
    double InteractionStartedAt = -1.0;
    float StartingDamageTime = -100.f;
    bool bCompleted = false;
    bool bEndingPlay = false;
};
