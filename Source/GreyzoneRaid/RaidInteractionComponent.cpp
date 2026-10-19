#include "RaidInteractionComponent.h"
#include "RaidTreatmentComponent.h"

#include "RaidGameMode.h"
#include "RaidInventoryComponent.h"
#include "RaidLoot.h"
#include "RaidMissionObjective.h"
#include "RaidPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Templates/UnrealTemplate.h"
#include "TimerManager.h"

namespace RaidInteraction
{
    constexpr float InteractRange = 260.f;
    // Existing R5 display strings are inserted verbatim below.
    const TCHAR* SearchComplete = TEXT("容器已搜索完毕，请选择需要的物品拿取。");
    const TCHAR* HealthFull = TEXT("生命值已满，医疗包未消耗。");
    const TCHAR* HealUsed = TEXT("已使用医疗包，恢复 45 点生命。");
    const TCHAR* AmmoUsed = TEXT("已拆开弹药盒，备用弹药 +24。");
    const TCHAR* CannotUse = TEXT("这件物资可以带出或放回容器。");
    const TCHAR* NoMedkit = TEXT("背包中没有医疗包。");
    const TCHAR* BackpackFull = TEXT("背包已满，按 Q 使用医疗包或前往撤离。");
}

URaidInteractionComponent::URaidInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URaidInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
    SetComponentTickEnabled(false);
    BindBackpack();
}

void URaidInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (!bEndingPlay)
    {
        bEndingPlay = true;
        Cleanup();
        OnInteractionEndingPlay.Broadcast(this);
        OnInteractionEndingPlay.Clear();
        OnInteractionChanged.Clear();
    }
    Super::EndPlay(EndPlayReason);
}

ARaidPlayer* URaidInteractionComponent::GetRaidPlayer() const
{
    return Cast<ARaidPlayer>(GetOwner());
}

bool URaidInteractionComponent::CanInteract() const
{
    const ARaidPlayer* Player = GetRaidPlayer();
    return !bEndingPlay && !bChangingSession && IsValid(Player)
        && !Player->IsActorBeingDestroyed() && Player->CanAct();
}

ARaidLoot* URaidInteractionComponent::GetOpenLoot() const
{
    return OpenLoot.Get();
}

void URaidInteractionComponent::BindBackpack()
{
    ARaidPlayer* Player = GetRaidPlayer();
    URaidInventoryComponent* Inventory = Player ? Player->GetInventoryComponent() : nullptr;
    if (BoundBackpack.Get() == Inventory || !IsValid(Inventory)) return;
    if (URaidInventoryComponent* Previous = BoundBackpack.Get())
        Previous->OnInventoryEndingPlay.Remove(BackpackEndingHandle);
    BoundBackpack = Inventory;
    BackpackEndingHandle = Inventory->OnInventoryEndingPlay.AddUObject(this,
        &URaidInteractionComponent::HandleBackpackEndingPlay);
}

void URaidInteractionComponent::UnbindContainer()
{
    if (URaidInventoryComponent* Inventory = BoundContainer.Get())
    {
        Inventory->OnInventoryChanged.Remove(ContainerChangedHandle);
        Inventory->OnInventoryEndingPlay.Remove(ContainerEndingHandle);
    }
    BoundContainer.Reset();
    ContainerChangedHandle.Reset();
    ContainerEndingHandle.Reset();
}

void URaidInteractionComponent::BroadcastChanged()
{
    check(SessionRevision < MAX_int64);
    ++SessionRevision;
    OnInteractionChanged.Broadcast(this);
}

void URaidInteractionComponent::ClearSearch()
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(SearchTimer);
    SearchEntryId = INDEX_NONE;
    SearchInventoryRevision = 0;
    SearchStartedAt = -1.0;
    SearchSeconds = 0.f;
}

void URaidInteractionComponent::Close()
{
    ReleaseInteract();
    TGuardValue<bool> Guard(bChangingSession, true);
    const bool bChanged = bInventoryOpen || !OpenLoot.IsExplicitlyNull() || SearchEntryId != INDEX_NONE;
    ARaidLoot* Previous = OpenLoot.Get();
    ClearSearch();
    UnbindContainer();
    OpenLoot.Reset();
    bInventoryOpen = false;
    SetComponentTickEnabled(false);
    if (IsValid(Previous) && !Previous->IsActorBeingDestroyed()) Previous->SetContainerOpen(false);
    if (bChanged)
    {
        if (ARaidPlayer* Player = GetRaidPlayer()) Player->RefreshInputMode();
        BroadcastChanged();
    }
}

void URaidInteractionComponent::Cleanup()
{
    Close();
    if (URaidInventoryComponent* Inventory = BoundBackpack.Get())
        Inventory->OnInventoryEndingPlay.Remove(BackpackEndingHandle);
    BoundBackpack.Reset();
    BackpackEndingHandle.Reset();
}

void URaidInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bInventoryOpen) { SetComponentTickEnabled(false); return; }
    if (!CanInteract() || OpenLoot.IsStale()
        || (OpenLoot.IsValid() && !CanAccessLoot(OpenLoot.Get()))) Close();
}

bool URaidInteractionComponent::CanAccessLoot(const ARaidLoot* Loot) const
{
    const ARaidPlayer* Player = GetRaidPlayer();
    if (!CanInteract() || !Player->Camera || !IsValid(Loot) || Loot->IsActorBeingDestroyed()
        || Loot->GetWorld() != GetWorld() || !IsValid(Loot->CollisionBox)) return false;
    const FVector Start = Player->Camera->GetComponentLocation();
    const FTransform BoxTransform = Loot->CollisionBox->GetComponentTransform();
    const FVector Extent = Loot->CollisionBox->GetUnscaledBoxExtent();
    const FVector Nearest = BoxTransform.TransformPosition(
        BoxTransform.InverseTransformPosition(Start).BoundToBox(-Extent, Extent));
    if (FVector::DistSquared(Start, Nearest) > FMath::Square(RaidInteraction::InteractRange)) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RaidContainerAccess), false, Player);
    return GetWorld()->LineTraceSingleByChannel(Hit, Start, Loot->CollisionBox->GetComponentLocation(),
        ECC_Visibility, Params) && Hit.GetActor() == Loot;
}

ARaidLoot* URaidInteractionComponent::GetFocusedLoot() const
{
    const ARaidPlayer* Player = GetRaidPlayer();
    if (!CanInteract() || bInventoryOpen || !Player->Camera) return nullptr;
    const FVector Start = Player->Camera->GetComponentLocation();
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RaidInteract), false, Player);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start,
        Start + Player->Camera->GetForwardVector() * RaidInteraction::InteractRange, ECC_Visibility, Params))
        return Cast<ARaidLoot>(Hit.GetActor());
    return nullptr;
}

void URaidInteractionComponent::Interact()
{
    if (!CanInteract()) return;
    if (bInventoryOpen) Close();
    else if (ARaidMissionObjective* Objective = GetFocusedObjective())
    {
        if (Objective->TryBeginInteraction(GetRaidPlayer())) ActiveObjective = Objective;
    }
    else if (ARaidLoot* Loot = GetFocusedLoot()) Open(Loot);
}

ARaidMissionObjective* URaidInteractionComponent::GetFocusedObjective() const
{
    if (!CanInteract() || bInventoryOpen) return nullptr;
    const ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    ARaidMissionObjective* Objective = Mode ? Mode->GetMissionObjective() : nullptr;
    return Objective && Objective->CanInteract(GetRaidPlayer()) ? Objective : nullptr;
}

void URaidInteractionComponent::ReleaseInteract()
{
    if (ARaidMissionObjective* Objective = ActiveObjective.Get()) Objective->CancelInteraction(GetRaidPlayer());
    ActiveObjective.Reset();
}

bool URaidInteractionComponent::Open(ARaidLoot* Loot)
{
    if (!CanAccessLoot(Loot)) return false;
    if (bInventoryOpen && GetOpenLoot() == Loot) return true;
    Loot->EnsureContents();
    if (!CanAccessLoot(Loot) || !Loot->GetInventoryComponent()->IsInitialized()) return false;
    Close();
    if (!CanAccessLoot(Loot)) return false;
    {
        TGuardValue<bool> Guard(bChangingSession, true);
        ARaidPlayer* Player = GetRaidPlayer();
        Player->PrepareInventoryInput();
        BindBackpack();
        OpenLoot = Loot;
        BoundContainer = Loot->GetInventoryComponent();
        ContainerChangedHandle = BoundContainer->OnInventoryChanged.AddUObject(this,
            &URaidInteractionComponent::HandleContainerChanged);
        ContainerEndingHandle = BoundContainer->OnInventoryEndingPlay.AddUObject(this,
            &URaidInteractionComponent::HandleContainerEndingPlay);
        bInventoryOpen = true;
        SetComponentTickEnabled(true);
        Loot->SetContainerOpen(true);
        Player->RefreshInputMode();
        BroadcastChanged();
    }
    // Start only after the session guard is released and callbacks are checked.
    // Revealed rows and quantities live in the container; partial row time does not.
    if (bInventoryOpen && GetOpenLoot() == Loot && CanAccessLoot(Loot)
        && Loot->FindNextUnsearchedIndex() != INDEX_NONE) BeginSearch();
    return bInventoryOpen && OpenLoot.Get() == Loot;
}

void URaidInteractionComponent::ToggleInventory()
{
    if (!CanInteract()) return;
    ReleaseInteract();
    if (bInventoryOpen) { Close(); return; }
    TGuardValue<bool> Guard(bChangingSession, true);
    ARaidPlayer* Player = GetRaidPlayer();
    Player->PrepareInventoryInput();
    BindBackpack();
    OpenLoot.Reset();
    bInventoryOpen = true;
    SetComponentTickEnabled(true);
    Player->RefreshInputMode();
    BroadcastChanged();
}

bool URaidInteractionComponent::IsSearching() const
{
    return bInventoryOpen && OpenLoot.IsValid() && SearchEntryId != INDEX_NONE && SearchStartedAt >= 0.0;
}

float URaidInteractionComponent::GetSearchProgress() const
{
    if (!IsSearching() || !GetWorld()) return 0.f;
    return FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - SearchStartedAt)
        / FMath::Max(.1f, SearchSeconds)), 0.f, 1.f);
}

void URaidInteractionComponent::BeginSearch()
{
    ARaidLoot* Loot = GetOpenLoot();
    if (!bInventoryOpen || !CanAccessLoot(Loot) || IsSearching()) return;
    const int32 Index = Loot->FindNextUnsearchedIndex();
    if (Index == INDEX_NONE) { Notify(RaidInteraction::SearchComplete); return; }
    const FRaidInventoryEntry& Entry = Loot->GetContents()[Index];
    SearchEntryId = Entry.EntryId;
    SearchInventoryRevision = Loot->GetRevision();
    SearchStartedAt = GetWorld()->GetTimeSeconds();
    SearchSeconds = FMath::IsFinite(Loot->SearchDuration) ? FMath::Max(.1f, Loot->SearchDuration) : 1.1f;
    GetWorld()->GetTimerManager().SetTimer(SearchTimer, this,
        &URaidInteractionComponent::CompleteSearch, SearchSeconds, false);
    TGuardValue<bool> Guard(bChangingSession, true);
    BroadcastChanged();
}

void URaidInteractionComponent::CancelSearch()
{
    const bool bChanged = SearchEntryId != INDEX_NONE || SearchStartedAt >= 0.0;
    ClearSearch();
    if (bChanged)
    {
        TGuardValue<bool> Guard(bChangingSession, true);
        BroadcastChanged();
    }
}

void URaidInteractionComponent::CompleteSearch()
{
    ARaidLoot* Loot = GetOpenLoot();
    if (!IsSearching() || !CanAccessLoot(Loot)) { Close(); return; }
    URaidInventoryComponent* Inventory = BoundContainer.Get();
    if (!Inventory) { Close(); return; }
    const int32 EntryId = SearchEntryId;
    const int64 ExpectedRevision = SearchInventoryRevision;
    ClearSearch();
    const int64 CompletingSession = SessionRevision;
    const FRaidInventoryResult Result = Inventory->Reveal(EntryId, ExpectedRevision);
    // Inventory callbacks may close the panel, destroy a container or unpossess.
    if (SessionRevision != CompletingSession || !bInventoryOpen || GetOpenLoot() != Loot) return;
    if (!CanAccessLoot(Loot)) { Close(); return; }
    {
        TGuardValue<bool> Guard(bChangingSession, true);
        BroadcastChanged();
    }
    if (Result.IsSuccess() && bInventoryOpen && GetOpenLoot() == Loot && CanAccessLoot(Loot)) BeginSearch();
}

void URaidInteractionComponent::HandleContainerChanged(URaidInventoryComponent* Inventory, int64 Revision)
{
    if (Inventory != BoundContainer.Get() || SearchEntryId == INDEX_NONE) return;
    const FRaidInventoryEntry* Entry = Inventory->FindEntry(SearchEntryId);
    if (!Entry || Entry->Quantity <= 0 || Entry->bRevealed) CancelSearch();
    else SearchInventoryRevision = Revision; // Taking a different revealed row does not restart search.
}

void URaidInteractionComponent::HandleContainerEndingPlay(URaidInventoryComponent* Inventory)
{
    if (Inventory == BoundContainer.Get()) Close();
}

void URaidInteractionComponent::HandleBackpackEndingPlay(URaidInventoryComponent* Inventory)
{
    if (Inventory == BoundBackpack.Get()) Cleanup();
}

FRaidTransferResult URaidInteractionComponent::TakeEntry(int32 EntryId, int64 ExpectedContainerRevision,
    int64 ExpectedBackpackRevision, int32 Quantity, ERaidTransferPolicy Policy)
{
    ARaidPlayer* Player = GetRaidPlayer();
    ARaidLoot* Loot = GetOpenLoot();
    URaidInventoryComponent* Backpack = Player ? Player->GetInventoryComponent() : nullptr;
    URaidInventoryComponent* Container = IsValid(Loot) ? Loot->GetInventoryComponent() : nullptr;
    FRaidTransferResult Rejected;
    Rejected.SourceRevision = Container ? Container->GetRevision() : 0;
    Rejected.DestinationRevision = Backpack ? Backpack->GetRevision() : 0;
    if (!bInventoryOpen || !CanAccessLoot(Loot) || !Backpack || Container != BoundContainer.Get()) return Rejected;
    const FRaidInventoryEntry* Entry = Container->FindEntry(EntryId);
    const FString Label = Entry ? Entry->Definition.DisplayName : FString();
    FRaidTransferRequest Request;
    Request.SourceEntryId = EntryId;
    Request.Quantity = Quantity;
    Request.Policy = Policy;
    Request.ExpectedSourceRevision = ExpectedContainerRevision;
    Request.ExpectedDestinationRevision = ExpectedBackpackRevision;
    const FRaidTransferResult Result = Container->TransferTo(Backpack, Request);
    if (Result.IsSuccess() && IsValid(Player) && !Player->IsActorBeingDestroyed())
        Player->RecordLootReceipt(Label, Result.Quantity);
    else if (Result.Code == ERaidInventoryResultCode::InsufficientCapacity) Notify(TEXT("背包件数或重量不足：放回部分物资后再拿取。"));
    return Result;
}

FRaidTransferResult URaidInteractionComponent::StoreEntry(int32 EntryId, int64 ExpectedBackpackRevision,
    int64 ExpectedContainerRevision, int32 Quantity, ERaidTransferPolicy Policy)
{
    ARaidPlayer* Player = GetRaidPlayer();
    ARaidLoot* Loot = GetOpenLoot();
    URaidInventoryComponent* Backpack = Player ? Player->GetInventoryComponent() : nullptr;
    URaidInventoryComponent* Container = IsValid(Loot) ? Loot->GetInventoryComponent() : nullptr;
    FRaidTransferResult Rejected;
    Rejected.SourceRevision = Backpack ? Backpack->GetRevision() : 0;
    Rejected.DestinationRevision = Container ? Container->GetRevision() : 0;
    if (!bInventoryOpen || !CanAccessLoot(Loot) || !Backpack || Container != BoundContainer.Get()) return Rejected;
    FRaidTransferRequest Request;
    Request.SourceEntryId = EntryId;
    Request.Quantity = Quantity;
    Request.Policy = Policy;
    Request.ExpectedSourceRevision = ExpectedBackpackRevision;
    Request.ExpectedDestinationRevision = ExpectedContainerRevision;
    return Backpack->TransferTo(Container, Request);
}

FRaidInventoryResult URaidInteractionComponent::UseEntry(int32 EntryId, int64 ExpectedBackpackRevision)
{
    ARaidPlayer* Player = GetRaidPlayer();
    URaidInventoryComponent* Backpack = Player ? Player->GetInventoryComponent() : nullptr;
    FRaidInventoryResult Rejected;
    Rejected.Revision = Backpack ? Backpack->GetRevision() : 0;
    if (!CanInteract() || !Backpack) return Rejected;
    if (ExpectedBackpackRevision != Backpack->GetRevision())
    { Rejected.Code = ERaidInventoryResultCode::StaleRevision; return Rejected; }
    const FRaidInventoryEntry* Entry = Backpack->FindEntry(EntryId);
    if (!Entry || Entry->Quantity <= 0) { Rejected.Code = ERaidInventoryResultCode::InvalidEntry; return Rejected; }
    if (!Entry->bRevealed) { Rejected.Code = ERaidInventoryResultCode::HiddenEntry; return Rejected; }
    const ERaidItemUseEffect Effect = Entry->Definition.UseEffect;
    const int32 Amount = Entry->Definition.UseAmount;
    if (Effect == ERaidItemUseEffect::None) { Notify(RaidInteraction::CannotUse); return Rejected; }
    if (Amount <= 0) { Rejected.Code = ERaidInventoryResultCode::InvalidDefinition; return Rejected; }
    const float PreviousHealth = Player->Health;
    float NewHealth = PreviousHealth;
    int32 NewAmmo = Player->ReserveAmmo;
    if (Effect == ERaidItemUseEffect::Heal)
    {
        if (!FMath::IsFinite(NewHealth) || NewHealth >= 100.f) { Notify(RaidInteraction::HealthFull); return Rejected; }
        return Player->GetTreatmentComponent()->BeginTreatment(EntryId, ExpectedBackpackRevision);
    }
    else if (Effect == ERaidItemUseEffect::Ammo)
    {
        if (NewAmmo < 0 || static_cast<int64>(NewAmmo) + Amount > MAX_int32)
        { Rejected.Code = ERaidInventoryResultCode::Overflow; return Rejected; }
        NewAmmo += Amount;
    }
    else { Notify(RaidInteraction::CannotUse); return Rejected; }
    // Copy all needed entry data before Consume may compact and invalidate its row.
    const FRaidInventoryResult Result = Backpack->ConsumeWithCommit(EntryId, 1, ExpectedBackpackRevision,
        [Player, NewHealth, NewAmmo]() { Player->Health = NewHealth; Player->ReserveAmmo = NewAmmo; });
    if (Result.IsSuccess())
    {
        const FString AmountText = FString::FromInt(Effect == ERaidItemUseEffect::Heal
            ? FMath::RoundToInt(NewHealth - PreviousHealth) : Amount);
        Notify(Effect == ERaidItemUseEffect::Heal
            ? FString(RaidInteraction::HealUsed).Replace(TEXT("45"), *AmountText)
            : FString(RaidInteraction::AmmoUsed).Replace(TEXT("24"), *AmountText));
    }
    return Result;
}

void URaidInteractionComponent::UseMedkit()
{
    if (ARaidPlayer* Player = GetRaidPlayer())
        if (Player->IsTreating()) { Player->CancelTreatment(); Notify(TEXT("治疗已取消，医疗包未消耗。")); return; }
    if (!CanInteract()) return;
    URaidInventoryComponent* Backpack = GetRaidPlayer()->GetInventoryComponent();
    if (!Backpack) return;
    for (const FRaidInventoryEntry& Entry : Backpack->GetEntries())
    {
        if (Entry.Quantity > 0 && Entry.bRevealed && Entry.Definition.UseEffect == ERaidItemUseEffect::Heal)
        { UseEntry(Entry.EntryId, Backpack->GetRevision()); return; }
    }
    Notify(RaidInteraction::NoMedkit);
}

void URaidInteractionComponent::Notify(const FString& Text) const
{
    if (bEndingPlay || Text.IsEmpty()) return;
    if (ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this))) Mode->PushMessage(Text);
}
