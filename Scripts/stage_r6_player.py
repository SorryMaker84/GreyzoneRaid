from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = root / 'Source/GreyzoneRaid'
dst = root / 'R6Staging/Source/GreyzoneRaid'
dst.mkdir(parents=True, exist_ok=True)
h = (src / 'RaidPlayer.h').read_text(encoding='utf-8-sig')
c = (src / 'RaidPlayer.cpp').read_text(encoding='utf-8-sig')
h = h.replace('#include "GameFramework/Character.h"', '#include "GameFramework/Character.h"\n#include "RaidInventoryTypes.h"')
a = h.index('/** One row in the backpack.')
b = h.index('/** A small, readable', a)
h = h[:a] + 'class URaidInventoryComponent;\nclass URaidInteractionComponent;\nclass URaidItemCatalog;\n\n' + h[b:]
h = h.replace('virtual void BeginPlay() override;', 'virtual void BeginPlay() override;\n    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;')
h = h.replace('int32 GetInteractionRevision() const { return InteractionRevision; }', 'int64 GetInteractionRevision() const;')
h = h.replace('int32 GetSearchIndex() const { return SearchIndex; }', 'int32 GetSearchIndex() const;')
for line in ['    UPROPERTY(BlueprintReadOnly, Category = "Raid") bool bInventoryOpen = false;\n',
             '    UPROPERTY(BlueprintReadOnly, Category = "Raid") int32 TotalLootValue = 0;\n',
             '    UPROPERTY(BlueprintReadOnly, Category = "Raid") int32 LootCount = 0;\n',
             '    UPROPERTY(BlueprintReadOnly, Category = "Raid") TArray<FRaidItemStack> Inventory;\n']:
    assert line in h, line
    h = h.replace(line, '')
for line in ['    TWeakObjectPtr<ARaidLoot> SearchTarget;\n', '    TWeakObjectPtr<ARaidLoot> OpenLootTarget;\n',
             '    float SearchStartedAt = -1.f;\n', '    int32 SearchIndex = INDEX_NONE;\n',
             '    int32 InteractionRevision = 0;\n', '    bool CanAct() const;\n',
             '    void PrepareInventoryInput();\n', '    void RefreshInputMode();\n',
             '    void ToggleInventory();\n', '    void UseMedkit();\n', '    void RequestRaidRestart();\n',
             '    void Quit();\n', '    void RecountInventory();\n']:
    assert line in h, line
    h = h.replace(line, '')
mark = '    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View")'
api = '''    UFUNCTION(BlueprintPure) URaidInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
    UFUNCTION(BlueprintPure) URaidInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
    UFUNCTION(BlueprintPure) bool IsInventoryOpen() const;
    UFUNCTION(BlueprintPure) int32 GetLootCount() const;
    UFUNCTION(BlueprintPure) int64 GetTotalLootValue() const;
    bool CanAct() const;
    void PrepareInventoryInput();
    void StopInteractionForUnpossess();
    void RefreshInputMode();
    void RecordLootReceipt(const FString& Label, int32 Quantity);

'''
h = h.replace(mark, api + mark)
h = h.replace('private:\n', '''private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid", meta=(AllowPrivateAccess="true"))
    TObjectPtr<URaidInventoryComponent> InventoryComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid", meta=(AllowPrivateAccess="true"))
    TObjectPtr<URaidInteractionComponent> InteractionComponent;
    UPROPERTY(EditDefaultsOnly, Category="Raid|Items") TSoftObjectPtr<URaidItemCatalog> ItemCatalogAsset;
    UPROPERTY(Transient) TObjectPtr<URaidItemCatalog> ItemCatalog;
    bool bEndingPlay = false;
''', 1)
c = c.replace('#include "RaidGameMode.h"', '''#include "RaidGameMode.h"
#include "RaidInventoryComponent.h"
#include "RaidInteractionComponent.h"
#include "RaidItemCatalog.h"
#include "RaidPlayerController.h"''')
c = c.replace('    constexpr float InteractRange = 260.f;\n', '')
c = c.replace('    PrimaryActorTick.bCanEverTick = true;', '''    PrimaryActorTick.bCanEverTick = true;
    InventoryComponent = CreateDefaultSubobject<URaidInventoryComponent>(TEXT("Backpack"));
    InteractionComponent = CreateDefaultSubobject<URaidInteractionComponent>(TEXT("Interaction"));
    ItemCatalogAsset = TSoftObjectPtr<URaidItemCatalog>(FSoftObjectPath(TEXT("/Game/Data/DA_RaidItems.DA_RaidItems")));''', 1)
c = c.replace('    Super::BeginPlay();', '''    Super::BeginPlay();
    InventoryComponent->InitializeOnce({}, MaxLootItems, false);
    ItemCatalog = ItemCatalogAsset.LoadSynchronous();
    FString CatalogError;
    if (!ItemCatalog || !ItemCatalog->Validate(&CatalogError))
        UE_LOG(LogTemp, Error, TEXT("RAID_ITEM_CATALOG: missing/invalid player catalog: %s"), *CatalogError);''', 1)
a = c.index('void ARaidPlayer::Tick(')
b = c.index('void ARaidPlayer::UpdateWeaponPresentation(', a)
c = c[:a] + '''void ARaidPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    StopInteractionForUnpossess();
    GetWorldTimerManager().ClearTimer(MuzzleTimer);
    Super::EndPlay(EndPlayReason);
}

void ARaidPlayer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateWeaponPresentation(DeltaSeconds);
}

''' + c[b:]
for action, method in [('Interact', 'Interact'), ('Inventory', 'ToggleInventory'), ('Heal', 'UseMedkit'), ('Restart', 'RequestRaidRestart'), ('Quit', 'Quit')]:
    line = f'    PlayerInputComponent->BindAction(TEXT("{action}"), IE_Pressed, this, &ARaidPlayer::{method});\n'
    assert line in c
    c = c.replace(line, '')
c = c.replace('    return IsAlive() && (!Mode || Mode->IsRaidActive());', '    return !bEndingPlay && IsAlive() && Controller && Controller->GetPawn() == this\n        && Mode && Mode->IsRaidActive();')
c = c.replace('bInventoryOpen', 'IsInventoryOpen()')
a = c.index('ARaidLoot* ARaidPlayer::GetFocusedLoot() const')
b = c.index('void ARaidPlayer::PrepareInventoryInput()', a)
c = c[:a] + '''ARaidLoot* ARaidPlayer::GetFocusedLoot() const { return InteractionComponent->GetFocusedLoot(); }
void ARaidPlayer::Interact() { InteractionComponent->Interact(); }
ARaidLoot* ARaidPlayer::GetOpenLoot() const { return InteractionComponent->GetOpenLoot(); }
bool ARaidPlayer::CanAccessLoot(const ARaidLoot* Loot) const { return InteractionComponent->CanAccessLoot(Loot); }
bool ARaidPlayer::IsInventoryOpen() const { return InteractionComponent->IsInventoryOpen(); }
int32 ARaidPlayer::GetLootCount() const { return InventoryComponent->GetItemCount(); }
int64 ARaidPlayer::GetTotalLootValue() const { return InventoryComponent->GetTotalValue(); }
int64 ARaidPlayer::GetInteractionRevision() const { return InteractionComponent->GetSessionRevision(); }

int32 ARaidPlayer::GetSearchIndex() const
{
    const ARaidLoot* Loot = GetOpenLoot();
    if (!Loot) return INDEX_NONE;
    const int32 EntryId = InteractionComponent->GetSearchEntryId();
    return Loot->GetInventoryComponent()->GetEntries().IndexOfByPredicate(
        [EntryId](const FRaidInventoryEntry& Entry) { return Entry.EntryId == EntryId; });
}

''' + c[b:]
a = c.index('void ARaidPlayer::RefreshInputMode()')
b = c.index('float ARaidPlayer::TakeDamage(', a)
c = c[:a] + '''void ARaidPlayer::StopInteractionForUnpossess()
{
    PrepareInventoryInput();
    InteractionComponent->Cleanup();
    GetWorldTimerManager().ClearTimer(MuzzleTimer);
    HideMuzzleFlash();
}

void ARaidPlayer::RefreshInputMode()
{
    if (bEndingPlay) return;
    if (ARaidPlayerController* PC = Cast<ARaidPlayerController>(Controller))
        if (PC->GetPawn() == this) PC->RefreshInputMode();
}

bool ARaidPlayer::OpenLoot(ARaidLoot* Loot) { return InteractionComponent->Open(Loot); }
void ARaidPlayer::CloseLoot() { InteractionComponent->Close(); }
void ARaidPlayer::BeginSearch() { InteractionComponent->BeginSearch(); }
void ARaidPlayer::CancelSearch() { InteractionComponent->CancelSearch(); }
bool ARaidPlayer::IsSearching() const { return InteractionComponent->IsSearching(); }
float ARaidPlayer::GetSearchProgress() const { return InteractionComponent->GetSearchProgress(); }

// Legacy Blueprint/director entry points resolve row indices immediately. The
// interactive HUD submits stable handles and captured revisions via Controller.
bool ARaidPlayer::TakeLootItem(int32 Index)
{
    ARaidLoot* Loot = GetOpenLoot();
    if (!Loot) return false;
    URaidInventoryComponent* Source = Loot->GetInventoryComponent();
    if (!Source->GetEntries().IsValidIndex(Index)) return false;
    return InteractionComponent->TakeEntry(Source->GetEntries()[Index].EntryId,
        Source->GetRevision(), InventoryComponent->GetRevision()).IsSuccess();
}

bool ARaidPlayer::StoreInventoryItem(int32 Index)
{
    ARaidLoot* Loot = GetOpenLoot();
    if (!Loot || !InventoryComponent->GetEntries().IsValidIndex(Index)) return false;
    return InteractionComponent->StoreEntry(InventoryComponent->GetEntries()[Index].EntryId,
        InventoryComponent->GetRevision(), Loot->GetInventoryComponent()->GetRevision()).IsSuccess();
}

bool ARaidPlayer::UseInventoryItem(int32 Index)
{
    if (!InventoryComponent->GetEntries().IsValidIndex(Index)) return false;
    return InteractionComponent->UseEntry(InventoryComponent->GetEntries()[Index].EntryId,
        InventoryComponent->GetRevision()).IsSuccess();
}

bool ARaidPlayer::AddItem(FName ItemId, const FString& Label, int32 Quantity, int32 Value)
{
    if (!CanAct() || !ItemCatalog) return false;
    FRaidItemDefinition Definition;
    FString Error;
    if (!ItemCatalog->ResolveLegacy(ItemId, Label, Value, Definition, &Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("RAID_ITEM_CATALOG: rejected grant: %s"), *Error);
        return false;
    }
    const auto Result = InventoryComponent->Grant(Definition, Quantity, InventoryComponent->GetRevision());
    if (Result.IsSuccess()) RecordLootReceipt(Label, Quantity);
    return Result.IsSuccess();
}

void ARaidPlayer::RecordLootReceipt(const FString& Label, int32 Quantity)
{
    LastLootTime = GetWorld()->GetTimeSeconds();
    LastLootLabel = Label;
    LastLootQuantity = Quantity;
}

''' + c[b:]
a = c.index('void ARaidPlayer::RequestRaidRestart()')
c = c[:a]
(dst / 'RaidPlayer.h').write_text(h, encoding='utf-8')
(dst / 'RaidPlayer.cpp').write_text(c, encoding='utf-8')
print('Staged Player component integration.')
