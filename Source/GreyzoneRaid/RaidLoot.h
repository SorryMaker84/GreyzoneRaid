#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidInventoryTypes.h"
#include "RaidLoot.generated.h"

class ARaidPlayer;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class URaidInventoryComponent;
class URaidItemCatalog;

/** Legacy authoring seed only; runtime rows belong to the inventory component. */
USTRUCT(BlueprintType)
struct FRaidLootEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 Quantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 Value = 0;
    UPROPERTY(Transient, BlueprintReadOnly) bool bRevealed = false;
};

/** A searchable container whose contents and search results survive closing its UI. */
UCLASS(Blueprintable)
class GREYZONERAID_API ARaidLoot : public AActor
{
    GENERATED_BODY()

public:
    ARaidLoot();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category="Raid|Loot")
    bool TryCollect(ARaidPlayer* Player);

    void EnsureContents();
    const TArray<FRaidInventoryEntry>& GetContents() const;
    URaidInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
    int32 FindNextUnsearchedIndex() const;
    bool RevealItem(int32 Index);
    bool TryTakeItem(ARaidPlayer* Player, int32 Index);
    bool StoreItem(FName StoredItemId, const FString& StoredDisplayName, int32 StoredQuantity, int32 StoredValue);
    int64 GetRevision() const;
    FString GetContainerName() const;
    void SetContainerOpen(bool bOpen);

    UFUNCTION(BlueprintPure, Category="Raid|Loot")
    FString GetRarityName() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Raid|Loot")
    TArray<FRaidLootEntry> Contents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Loot")
    bool bGenerateSupplementalLoot = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Loot")
    bool bEnemyDrop = false;

    // Keep the saved Blueprint/map fields as the first row and the visual theme.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Loot")
    FName ItemId = TEXT("Tech");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Loot")
    FString DisplayName = TEXT("电子元件");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Loot", meta=(ClampMin="1"))
    int32 Quantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Loot", meta=(ClampMin="0"))
    int32 Value = 120;

    // Searching reveals one row after this duration; taking it is a separate action.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Loot", meta=(ClampMin="0.1"))
    float SearchDuration = 1.1f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Visuals")
    TObjectPtr<UBoxComponent> CollisionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Visuals")
    TObjectPtr<UStaticMeshComponent> LootMesh;

private:
    void RefreshVisuals();
    void AlignCaseToSurface();
    void UpdateLid(float DeltaSeconds);
    bool IsContainerEmpty() const;
    FString GetContainerStateText() const;
    bool LoadItemCatalog();
    void HandleInventoryChanged(URaidInventoryComponent* Inventory, int64 Revision);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Loot", meta=(AllowPrivateAccess="true"))
    TObjectPtr<URaidInventoryComponent> InventoryComponent;
    UPROPERTY(EditDefaultsOnly, Category="Raid|Items") TSoftObjectPtr<URaidItemCatalog> ItemCatalogAsset;
    UPROPERTY(Transient) TObjectPtr<URaidItemCatalog> ItemCatalog;
    FDelegateHandle InventoryChangedHandle;

    UPROPERTY() TObjectPtr<USceneComponent> VisualRoot;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CaseParts;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LidParts;
    TArray<FTransform> ClosedLidTransforms;
    UPROPERTY() TObjectPtr<UTextRenderComponent> FrontStencil;
    UPROPERTY() TObjectPtr<UTextRenderComponent> BackStencil;
    UPROPERTY() TObjectPtr<UTextRenderComponent> ValueStencil;
    UPROPERTY() TObjectPtr<UTextRenderComponent> InteractionLabel;
    FName LastVisualItemId;
    bool bLastVisualEnemyDrop = false;
    FString LastInteractionText;
    bool bContainerOpen = false;
    bool bEverOpened = false;
    float LidOpenAlpha = 0.f;
};

