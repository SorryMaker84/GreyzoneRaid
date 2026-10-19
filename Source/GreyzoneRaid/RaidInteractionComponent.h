#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RaidInventoryTypes.h"
#include "RaidInteractionComponent.generated.h"

class ARaidLoot;
class ARaidMissionObjective;
class ARaidPlayer;
class URaidInventoryComponent;
class URaidInteractionComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FRaidInteractionChanged, URaidInteractionComponent*);
DECLARE_MULTICAST_DELEGATE_OneParam(FRaidInteractionEndingPlay, URaidInteractionComponent*);

/** Owns one player's inventory panel, container session and cancellable search. */
UCLASS(ClassGroup=(Raid), meta=(BlueprintSpawnableComponent))
class GREYZONERAID_API URaidInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URaidInteractionComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    ARaidLoot* GetOpenLoot() const;
    bool IsInventoryOpen() const { return bInventoryOpen; }
    int64 GetSessionRevision() const { return SessionRevision; }
    bool IsSearching() const;
    int32 GetSearchEntryId() const { return SearchEntryId; }
    float GetSearchProgress() const;

    void Interact();
    void ReleaseInteract();
    void ToggleInventory();
    void Close();
    void BeginSearch();
    void CancelSearch();
    bool Open(ARaidLoot* Loot);
    bool CanAccessLoot(const ARaidLoot* Loot) const;
    ARaidLoot* GetFocusedLoot() const;
    ARaidMissionObjective* GetFocusedObjective() const;
    void UseMedkit();
    void Cleanup();

    FRaidTransferResult TakeEntry(int32 EntryId, int64 ExpectedContainerRevision,
        int64 ExpectedBackpackRevision, int32 Quantity = 1,
        ERaidTransferPolicy Policy = ERaidTransferPolicy::Exact);
    FRaidTransferResult StoreEntry(int32 EntryId, int64 ExpectedBackpackRevision,
        int64 ExpectedContainerRevision, int32 Quantity = 1,
        ERaidTransferPolicy Policy = ERaidTransferPolicy::Exact);
    FRaidInventoryResult UseEntry(int32 EntryId, int64 ExpectedBackpackRevision);

    FRaidInteractionChanged OnInteractionChanged;
    FRaidInteractionEndingPlay OnInteractionEndingPlay;

private:
    ARaidPlayer* GetRaidPlayer() const;
    bool CanInteract() const;
    void BindBackpack();
    void UnbindContainer();
    void ClearSearch();
    void CompleteSearch();
    void BroadcastChanged();
    void HandleContainerChanged(URaidInventoryComponent* Inventory, int64 Revision);
    void HandleContainerEndingPlay(URaidInventoryComponent* Inventory);
    void HandleBackpackEndingPlay(URaidInventoryComponent* Inventory);
    void Notify(const FString& Text) const;

    TWeakObjectPtr<ARaidLoot> OpenLoot;
    TWeakObjectPtr<ARaidMissionObjective> ActiveObjective;
    TWeakObjectPtr<URaidInventoryComponent> BoundContainer;
    TWeakObjectPtr<URaidInventoryComponent> BoundBackpack;
    FDelegateHandle ContainerChangedHandle;
    FDelegateHandle ContainerEndingHandle;
    FDelegateHandle BackpackEndingHandle;
    FTimerHandle SearchTimer;
    int64 SessionRevision = 0;
    int64 SearchInventoryRevision = 0;
    int32 SearchEntryId = INDEX_NONE;
    double SearchStartedAt = -1.0;
    float SearchSeconds = 0.f;
    bool bInventoryOpen = false;
    bool bChangingSession = false;
    bool bEndingPlay = false;
};
