#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RaidInventoryTypes.h"
#include "RaidPlayerController.generated.h"

class ARaidLoot;
class ARaidPlayer;
class URaidInteractionComponent;
class URaidInventoryComponent;

/** UI intent is explicit; a displayed row number is never an inventory handle. */
enum class ERaidInventoryCommand : uint8
{
    Close,
    BeginSearch,
    CancelSearch,
    Take,
    Store,
    Use
};

/** Captured when Canvas draws its targets, then checked again at dispatch. */
struct FRaidInventoryCommandContext
{
    TWeakObjectPtr<ARaidPlayer> Player;
    TWeakObjectPtr<ARaidLoot> Container;
    TWeakObjectPtr<URaidInventoryComponent> Backpack;
    TWeakObjectPtr<URaidInventoryComponent> ContainerInventory;
    int64 SessionRevision = INDEX_NONE;
    int64 BackpackRevision = INDEX_NONE;
    int64 ContainerRevision = INDEX_NONE;
    bool bHadContainer = false;
};

/** Owns local input policy and routes UI intent to the possessed pawn's components. */
UCLASS()
class GREYZONERAID_API ARaidPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void SetupInputComponent() override;

    void RefreshInputMode();
    void HandleMenuAction(FName Action);
    void HandleBack();
    void HandleConfirm();
    void HandleInteract();
    void HandleReleaseInteract();
    void HandleToggleInventory();
    void HandleToggleTacticalMap();
    void PrepareTacticalMapInput();
    void ReturnToWorldView();
    void HandleUseMedkit();

    bool IsInventoryContextCurrent(const FRaidInventoryCommandContext& Context) const;
    ERaidInventoryResultCode SubmitInventoryCommand(ERaidInventoryCommand Command,
        int32 EntryId, const FRaidInventoryCommandContext& Context);

private:
    void ObservePawn(ARaidPlayer* RaidPawn);
    void ReleasePawn(bool bCleanup);
    void HandleInteractionChanged(URaidInteractionComponent* Interaction);
    void HandleInteractionEndingPlay(URaidInteractionComponent* Interaction);
    URaidInteractionComponent* GetCommandInteraction() const;

    TWeakObjectPtr<ARaidPlayer> ObservedPlayer;
    TWeakObjectPtr<URaidInteractionComponent> ObservedInteraction;
    FDelegateHandle InteractionChangedHandle;
    FDelegateHandle InteractionEndingHandle;
    bool bEndingPlay = false;
    bool bReleasingPawn = false;
    bool bHasAppliedInputMode = false;
    bool bAppliedUIInput = false;
};
