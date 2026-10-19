#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RaidMissionTypes.h"
#include "RaidPlayerController.h"
#include "RaidHUD.generated.h"

class ARaidExtractionZone;
class ARaidGameMode;
class ARaidLoot;
class ARaidPlayer;
class UFont;
class URaidInteractionComponent;
class URaidInventoryComponent;

struct FRaidHUDInventoryAction
{
    ERaidInventoryCommand Command = ERaidInventoryCommand::Close;
    int32 EntryId = INDEX_NONE;
    FRaidItemDefinition FeedbackDefinition;
};

/** A Canvas HUD with a cooked Chinese composite font and resolution-aware text. */
UCLASS()
class GREYZONERAID_API ARaidHUD : public AHUD
{
    GENERATED_BODY()
    friend class AR6FrameworkBenchmarkDirector;

public:
    ARaidHUD();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void DrawHUD() override;
    void SetObservedPlayer(ARaidPlayer* Player);
    ARaidPlayer* GetObservedPlayer() const;
    const TArray<FRaidInventoryEntry>& GetInventoryView() const { return InventoryView; }
    const TArray<FRaidInventoryEntry>& GetContainerView() const { return ContainerView; }
    int64 GetInventoryViewRevision() const { return InventoryViewRevision; }
    int64 GetContainerViewRevision() const { return ContainerViewRevision; }
    int64 GetInteractionViewRevision() const { return InteractionViewRevision; }
    bool IsInventoryViewOpen() const { return bInventoryViewOpen; }
    int32 GetInventoryViewItemCount() const { return InventoryItemCount; }
    int64 GetInventoryViewTotalValue() const { return InventoryTotalValue; }
    int32 GetInventoryViewRefreshCount() const { return InventoryViewRefreshCount; }
    void ToggleTacticalMap();
    void CloseTacticalMap() { bTacticalMapOpen = false; }
    bool IsTacticalMapOpen() const { return bTacticalMapOpen; }
    // These are the same callbacks used by real Canvas mouse hitboxes. Keeping
    // them public also lets the opt-in flow director verify the menu actions.
    virtual void NotifyHitBoxClick(FName BoxName) override;
    virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;
    virtual void NotifyHitBoxEndCursorOver(FName BoxName) override;

private:
    void ObserveMissionMode(ARaidGameMode* GameMode);
    void HandleMissionChanged(ARaidGameMode* GameMode);
    void DrawFrontierBrief(const ARaidPlayer* Player);
    void DrawMissionFeedback();
    void DrawTacticalMap(const ARaidPlayer* Player);
    void DrawNavigationMarker(const ARaidPlayer* Player, const FVector& Location,
        const FString& Label, const FLinearColor& Color, float Y);
    FString GetFrontierRegion(const FVector& Location) const;
    void UnbindObservedPlayer();
    void BindContainer(ARaidLoot* Container);
    void HandleInventoryChanged(URaidInventoryComponent* Inventory, int64 Revision);
    void HandleInventoryEndingPlay(URaidInventoryComponent* Inventory);
    void HandleInteractionChanged(URaidInteractionComponent* Interaction);
    void HandleInteractionEndingPlay(URaidInteractionComponent* Interaction);
    void RefreshBackpackView();
    void RefreshContainerView();
    void RegisterInventoryCommand(FName Action, ERaidInventoryCommand Command,
        const FRaidInventoryEntry* Entry = nullptr);
    void DrawMainMenu();
    void DrawInstructions();
    void DrawPauseMenu(const ARaidGameMode* GameMode);
    void DrawMenuButton(FName Action, const FString& Label, float X, float Y,
        float Width, float Height, bool bPrimary = false);
    void DrawRunningHUD(const ARaidPlayer* Player, const ARaidGameMode* GameMode);
    void DrawInventory(const ARaidPlayer* Player);
    void DrawInventoryButton(FName Action, const FString& Label, float X, float Y,
        float Width, float Height, bool bEnabled = true, bool bPrimary = false);
    void InvalidateInventoryButtons();
    void DrawResult(const ARaidGameMode* GameMode);
    void DrawCrosshair(const ARaidPlayer* Player);
    void DrawCompass(const ARaidPlayer* Player, const ARaidExtractionZone* Extraction);
    void DrawDamageVignette(const ARaidPlayer* Player);
    void DrawDamageDirection(const ARaidPlayer* Player);
    void DrawLootToast(const ARaidPlayer* Player);

    void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color,
        float FontScale = 1.f, bool bCentered = false);
    void DrawRightLabel(const FString& Text, float X, float Y,
        const FLinearColor& Color, float FontScale = 1.f);
    void DrawFittedLabel(const FString& Text, float X, float Y, float MaxWidth,
        const FLinearColor& Color, float FontScale = 1.f, bool bCentered = false);
    FVector2D MeasureLabel(const FString& Text, float FontScale) const;
    void DrawSoftBackdrop(float X, float Y, float Width, float Height, float Opacity = 0.65f);
    void DrawPanel(float X, float Y, float Width, float Height,
        const FLinearColor& BorderColor);
    void DrawBar(float X, float Y, float Width, float Height, float Fraction,
        const FLinearColor& Color);
    void DrawUILine(float X1, float Y1, float X2, float Y2,
        const FLinearColor& Color, float Thickness = 1.f);

    ARaidExtractionZone* FindExtraction(const ARaidPlayer* Player) const;
    int32 CountMedkits(const ARaidPlayer* Player) const;
    FString FormatTime(float Seconds) const;

    // The CDO hard reference keeps the composite font and its face in the cook.
    UPROPERTY()
    TObjectPtr<UFont> HUDFont = nullptr;
    float UIScale = 1.f;
    float ScreenWidth = 1280.f;
    float ScreenHeight = 720.f;
    TSet<FName> ActiveMenuButtons;
    TSet<FName> ActiveInventoryButtons;
    TMap<FName, FRaidHUDInventoryAction> InventoryCommands;
    FRaidInventoryCommandContext RenderedContext;
    TWeakObjectPtr<ARaidPlayer> RenderedPlayer;
    TWeakObjectPtr<ARaidLoot> RenderedOpenLoot;
    TWeakObjectPtr<ARaidPlayer> ObservedPlayer;
    TWeakObjectPtr<ARaidGameMode> ObservedMissionMode;
    FDelegateHandle MissionChangedHandle;
    FRaidMissionSnapshot MissionView;
    bool bTacticalMapOpen = false;
    float LastMissionCompletedTime = -100.f;
    float LastMissionInterruptedTime = -100.f;
    TWeakObjectPtr<URaidInteractionComponent> ObservedInteraction;
    TWeakObjectPtr<URaidInventoryComponent> ObservedBackpack;
    TWeakObjectPtr<ARaidLoot> ObservedContainer;
    TWeakObjectPtr<URaidInventoryComponent> ObservedContainerInventory;
    FDelegateHandle BackpackChangedHandle;
    FDelegateHandle BackpackEndingHandle;
    FDelegateHandle ContainerChangedHandle;
    FDelegateHandle ContainerEndingHandle;
    FDelegateHandle InteractionChangedHandle;
    FDelegateHandle InteractionEndingHandle;
    TArray<FRaidInventoryEntry> InventoryView;
    TArray<FRaidInventoryEntry> ContainerView;
    TArray<int32> ContainerRows;
    FString ContainerName;
    int64 InventoryViewRevision = INDEX_NONE;
    int64 ContainerViewRevision = INDEX_NONE;
    int64 InteractionViewRevision = INDEX_NONE;
    int64 InventoryTotalValue = 0;
    int32 InventoryItemCount = 0;
    int32 InventoryCapacity = 0;
    int32 InventoryMedkitCount = 0;
    int32 InventoryViewRefreshCount = 0;
    int32 RevealedRows = 0;
    int32 UnsearchedRows = 0;
    int32 TakenRows = 0;
    int32 SearchEntryId = INDEX_NONE;
    int32 SearchRowIndex = INDEX_NONE;
    bool bInventoryViewOpen = false;
    bool bViewSearching = false;
    bool bEndingPlay = false;
    int32 ContainerPage = 0;
    int32 InventoryPage = 0;
    bool bInventorySnapshotValid = false;
    FName LastInventoryFeedbackItemId = NAME_None;
    FString LastInventoryFeedbackLabel;
    int32 LastInventoryFeedbackUnitValue = INDEX_NONE;
    float LastInventoryFeedbackTime = -100.f;
    bool bLastFeedbackInventoryOnly = false;
    FName HoveredButton = NAME_None;
    uint8 LastMenuState = 255;
    bool bWasExtracting = false;
    float LastExtractionResetTime = -100.f;
};
