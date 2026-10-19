#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RaidInventoryTypes.h"
#include "RaidTreatmentComponent.generated.h"
class URaidInventoryComponent;

DECLARE_MULTICAST_DELEGATE(FRaidTreatmentChanged);

/** Owns a timed medical transaction. No inventory or health is spent until commit. */
UCLASS(ClassGroup=(Raid), meta=(BlueprintSpawnableComponent))
class GREYZONERAID_API URaidTreatmentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URaidTreatmentComponent();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    FRaidInventoryResult BeginTreatment(int32 EntryId, int64 ExpectedRevision);
    void CancelTreatment();
    bool IsTreating() const { return ActiveEntryId != INDEX_NONE; }
    float GetProgress() const;
    UPROPERTY(EditDefaultsOnly, Category="Treatment", meta=(ClampMin="0.2")) float Duration = 3.f;
    FRaidTreatmentChanged OnTreatmentChanged;
private:
    void FinishTreatment();
    void InventoryChanged(URaidInventoryComponent* Inventory, int64 Revision);
    FTimerHandle Timer;
    TWeakObjectPtr<URaidInventoryComponent> Backpack;
    int32 ActiveEntryId = INDEX_NONE;
    int64 RevisionAtStart = 0;
    float StartedAt = 0.f;
    float ActiveDuration = 0.f;
    bool bTransition = false;
};
