#include "RaidLoot.h"
#include "RaidText.h"

#include "RaidGameMode.h"
#include "RaidPlayer.h"
#include "RaidInteractionComponent.h"
#include "RaidInventoryComponent.h"
#include "RaidItemCatalog.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    UMaterialInterface* LootPaint(const TCHAR* DepotName, const TCHAR* LegacyName, UMaterialInterface* Fallback)
    {
        const FString DepotPath = FString::Printf(TEXT("/Game/Materials/Depot/%s.%s"), DepotName, DepotName);
        if (UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, *DepotPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) return Paint;
        const FString LegacyPath = FString::Printf(TEXT("/Game/Materials/%s.%s"), LegacyName, LegacyName);
        if (UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, *LegacyPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) return Paint;
        return Fallback;
    }
}

ARaidLoot::ARaidLoot()
{
    PrimaryActorTick.bCanEverTick = true;
    InventoryComponent = CreateDefaultSubobject<URaidInventoryComponent>(TEXT("InventoryComponent"));
    ItemCatalogAsset = TSoftObjectPtr<URaidItemCatalog>(FSoftObjectPath(TEXT("/Game/Data/DA_RaidItems.DA_RaidItems")));
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
    SetRootComponent(CollisionBox);
    // The generous vertical query box keeps low cases easy to search; it never blocks walking.
    CollisionBox->SetBoxExtent(FVector(26.f, 22.f, 48.f));
    CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    CollisionBox->SetGenerateOverlapEvents(true);
    CollisionBox->SetCanEverAffectNavigation(false);

    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CaseVisualRoot"));
    VisualRoot->SetupAttachment(CollisionBox);
    VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -48.f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    auto Part = [&](const TCHAR* Name, FVector Location, FVector Size, const TCHAR* PaintTag, bool bCylinder = false)
    {
        UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Mesh->SetupAttachment(VisualRoot);
        Mesh->SetRelativeLocation(Location);
        Mesh->SetRelativeScale3D(Size / 100.f);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->ComponentTags.Add(FName(PaintTag));
        if (bCylinder && Cylinder.Succeeded()) Mesh->SetStaticMesh(Cylinder.Object);
        else if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
        if (BasicMaterial.Succeeded()) Mesh->SetMaterial(0, BasicMaterial.Object);
        CaseParts.Add(Mesh);
        return Mesh;
    };

    // A compact hard case: rubber feet, reinforced lid, latches, handle and protective corners.
    LootMesh = Part(TEXT("LootMesh"), FVector(0, 0, 18), FVector(44, 32, 30), TEXT("Body"));
    Part(TEXT("CaseLid"), FVector(0, 0, 34), FVector(46, 34, 4), TEXT("Body"));
    Part(TEXT("LidSeam"), FVector(0, 0, 31), FVector(45, 33, 1.3f), TEXT("Rubber"));
    Part(TEXT("FootFront"), FVector(-14, 0, 1.5f), FVector(5, 28, 3), TEXT("Rubber"));
    Part(TEXT("FootRear"), FVector(14, 0, 1.5f), FVector(5, 28, 3), TEXT("Rubber"));
    for (int32 X : {-1, 1})
    {
        for (int32 Y : {-1, 1})
        {
            const FString Name = FString::Printf(TEXT("Corner_%d_%d"), X, Y);
            Part(*Name, FVector(X * 21.f, Y * 15.f, 18), FVector(4, 4, 30), TEXT("Metal"));
        }
    }
    Part(TEXT("LatchA"), FVector(-23, -10, 29), FVector(3, 5, 9), TEXT("Metal"));
    Part(TEXT("LatchB"), FVector(-23, 10, 29), FVector(3, 5, 9), TEXT("Metal"));
    Part(TEXT("HandleA"), FVector(0, -9, 39), FVector(5, 3, 8), TEXT("Rubber"));
    Part(TEXT("HandleB"), FVector(0, 9, 39), FVector(5, 3, 8), TEXT("Rubber"));
    Part(TEXT("HandleGrip"), FVector(0, 0, 43), FVector(5, 20, 3), TEXT("Rubber"));
    Part(TEXT("FrontLabelPlate"), FVector(-22.4f, 0, 17), FVector(1.2f, 24, 16), TEXT("Rubber"));
    Part(TEXT("RearLabelPlate"), FVector(22.4f, 0, 22), FVector(1.2f, 24, 9), TEXT("Rubber"));
    Part(TEXT("ClassificationStrip"), FVector(-23.2f, 0, 8), FVector(.6f, 24, 2), TEXT("Accent"));
    Part(TEXT("MedicalCrossHorizontal"), FVector(0, 0, 36.5f), FVector(18, 5, 1), TEXT("Medical"));
    Part(TEXT("MedicalCrossVertical"), FVector(0, 0, 36.7f), FVector(5, 18, 1), TEXT("Medical"));
    Part(TEXT("AmmoRoundA"), FVector(12, -5, 38), FVector(4, 4, 10), TEXT("Ammo"), true);
    Part(TEXT("AmmoRoundB"), FVector(12, 1, 38), FVector(4, 4, 10), TEXT("Ammo"), true);
    Part(TEXT("AmmoRoundC"), FVector(12, 7, 38), FVector(4, 4, 10), TEXT("Ammo"), true);

    // Move the existing lid and its fittings together around the rear hinge.
    // Keeping the original component names preserves the saved child Blueprint.
    for (UStaticMeshComponent* Mesh : CaseParts)
    {
        const FName PartName = Mesh->GetFName();
        if (PartName == TEXT("CaseLid") || PartName == TEXT("HandleA")
            || PartName == TEXT("HandleB") || PartName == TEXT("HandleGrip")
            || Mesh->ComponentHasTag(TEXT("Medical")) || Mesh->ComponentHasTag(TEXT("Ammo")))
        {
            Mesh->SetMobility(EComponentMobility::Movable);
            LidParts.Add(Mesh);
            ClosedLidTransforms.Add(Mesh->GetRelativeTransform());
        }
    }

    auto Stencil = [&](const TCHAR* Name, FVector Location, FRotator Rotation, float Size)
    {
        UTextRenderComponent* Text = CreateDefaultSubobject<UTextRenderComponent>(Name);
        Text->SetupAttachment(VisualRoot);
        Text->SetRelativeLocation(Location);
        Text->SetRelativeRotation(Rotation);
        Text->SetWorldSize(Size);
        Text->SetHorizontalAlignment(EHTA_Center);
        Text->SetVerticalAlignment(EVRTA_TextCenter);
        Text->SetTextRenderColor(FColor(237, 242, 230));
        Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Text->SetCanEverAffectNavigation(false);
        return Text;
    };
    FrontStencil = Stencil(TEXT("FrontStencil"), FVector(-23.2f, 0, 23), FRotator(0, 180, 0), 4.4f);
    BackStencil = Stencil(TEXT("BackStencil"), FVector(23.2f, 0, 22), FRotator::ZeroRotator, 4.4f);
    ValueStencil = Stencil(TEXT("ValueStencil"), FVector(-23.3f, 0, 14), FRotator(0, 180, 0), 3.2f);
    InteractionLabel = Stencil(TEXT("InteractionLabel"), FVector(0, 0, 62), FRotator(0, 180, 0), 8.f);
    InteractionLabel->SetHiddenInGame(true);
    InteractionLabel->SetVisibility(false);
}

FString ARaidLoot::GetRarityName() const
{
    return Value >= 160 ? TEXT("稀有") : (Value >= 100 ? TEXT("精良") : TEXT("普通"));
}

void ARaidLoot::EnsureContents()
{
    // Deferred fixtures may supply explicit resolved seeds before FinishSpawning.
    if (InventoryComponent->IsInitialized()) { Contents.Empty(); return; }
    if (!LoadItemCatalog()) return;

    if (Contents.IsEmpty())
    {
        auto AddEntry = [this](FName Id, const FString& Label, int32 Count, int32 UnitValue)
        {
            FRaidLootEntry Entry;
            Entry.ItemId = Id;
            Entry.DisplayName = Label;
            Entry.Quantity = Count;
            Entry.Value = UnitValue;
            Contents.Add(MoveTemp(Entry));
        };
        AddEntry(ItemId, DisplayName, FMath::Max(1, Quantity), FMath::Max(0, Value));

        // Authored Contents are authoritative. Legacy map cases receive a small,
        // deterministic set of supplies without replacing their original item.
        if (bGenerateSupplementalLoot)
        {
            if (bEnemyDrop)
            {
                AddEntry(TEXT("Ammo"), TEXT("备用弹药"), 1, 35);
            }
            else if (ItemId == TEXT("Medkit"))
            {
                AddEntry(TEXT("Medkit"), TEXT("简易医疗包"), 1, 35);
                AddEntry(TEXT("Rations"), TEXT("应急口粮"), 1, 25);
            }
            else if (ItemId == TEXT("Rations"))
            {
                AddEntry(TEXT("Rations"), TEXT("压缩口粮"), 1, 25);
            }
            else if (ItemId == TEXT("Ammo"))
            {
                AddEntry(TEXT("Ammo"), TEXT("散装弹药"), 1, 30);
            }
            else
            {
                AddEntry(TEXT("Tech"), TEXT("备用电子零件"), 1, 45);
                if (Value >= 160) AddEntry(TEXT("Ammo"), TEXT("备用弹药"), 1, 35);
            }
        }
    }

    TArray<FRaidInventorySeed> Seeds;
    Seeds.Reserve(Contents.Num());
    for (const FRaidLootEntry& Entry : Contents)
    {
        FRaidInventorySeed Seed;
        FString Error;
        if (!ItemCatalog->ResolveLegacy(Entry.ItemId, Entry.DisplayName, Entry.Value, Seed.Definition, &Error))
        {
            UE_LOG(LogTemp, Error, TEXT("RAID_ITEM_CATALOG: container %s rejected authoring seed: %s"), *GetName(), *Error);
            return;
        }
        Seed.Quantity = FMath::Max(0, Entry.Quantity);
        Seed.bRevealed = Entry.bRevealed;
        Seeds.Add(MoveTemp(Seed));
    }
    const FRaidInventoryResult Result = InventoryComponent->InitializeOnce(Seeds, -1, true);
    if (!Result.IsSuccess())
    {
        UE_LOG(LogTemp, Error, TEXT("RAID_INVENTORY: container %s initialization failed, code=%d"), *GetName(), static_cast<int32>(Result.Code));
        return;
    }
    Contents.Empty(); // Authoring data is not a second runtime ledger.
    RefreshVisuals();
}

bool ARaidLoot::LoadItemCatalog()
{
    if (ItemCatalog) return true;
    ItemCatalog = ItemCatalogAsset.LoadSynchronous();
    FString Error;
    if (!ItemCatalog || !ItemCatalog->Validate(&Error))
    {
        UE_LOG(LogTemp, Error, TEXT("RAID_ITEM_CATALOG: container %s cannot use catalog %s: %s"),
            *GetName(), *ItemCatalogAsset.ToString(), *Error);
        ItemCatalog = nullptr;
        return false;
    }
    return true;
}

const TArray<FRaidInventoryEntry>& ARaidLoot::GetContents() const
{
    return InventoryComponent->GetEntries();
}

int64 ARaidLoot::GetRevision() const
{
    return InventoryComponent->GetRevision();
}

int32 ARaidLoot::FindNextUnsearchedIndex() const
{
    return GetContents().IndexOfByPredicate([](const FRaidInventoryEntry& Entry)
    {
        return Entry.Quantity > 0 && !Entry.bRevealed;
    });
}

bool ARaidLoot::RevealItem(int32 Index)
{
    const TArray<FRaidInventoryEntry>& Entries = GetContents();
    if (!Entries.IsValidIndex(Index)) return false;
    return InventoryComponent->Reveal(Entries[Index].EntryId, GetRevision()).IsSuccess();
}

bool ARaidLoot::TryTakeItem(ARaidPlayer* Player, int32 Index)
{
    if (!IsValid(Player) || !Player->GetInteractionComponent() || Player->GetOpenLoot() != this
        || !GetContents().IsValidIndex(Index)) return false;
    return Player->GetInteractionComponent()->TakeEntry(GetContents()[Index].EntryId,
        GetRevision(), Player->GetInventoryComponent()->GetRevision()).IsSuccess();
}

bool ARaidLoot::StoreItem(FName StoredItemId, const FString& StoredDisplayName,
    int32 StoredQuantity, int32 StoredValue)
{
    if (StoredItemId.IsNone() || StoredQuantity <= 0 || !LoadItemCatalog()) return false;
    EnsureContents();
    if (!InventoryComponent->IsInitialized()) return false;
    FRaidItemDefinition Definition;
    FString Error;
    if (!ItemCatalog->ResolveLegacy(StoredItemId, StoredDisplayName, StoredValue, Definition, &Error))
    {
        UE_LOG(LogTemp, Warning, TEXT("RAID_ITEM_CATALOG: container %s rejected legacy grant: %s"), *GetName(), *Error);
        return false;
    }
    return InventoryComponent->Grant(Definition, StoredQuantity, GetRevision()).IsSuccess();
}

FString ARaidLoot::GetContainerName() const
{
    if (bEnemyDrop) return TEXT("战利品包");
    if (ItemId == TEXT("Medkit")) return TEXT("医疗物资箱");
    if (ItemId == TEXT("Rations")) return TEXT("口粮补给箱");
    if (ItemId == TEXT("Ammo")) return TEXT("弹药箱");
    return TEXT("回收物资箱");
}

bool ARaidLoot::IsContainerEmpty() const
{
    return InventoryComponent->IsInitialized() && !GetContents().ContainsByPredicate([](const FRaidInventoryEntry& Entry)
    {
        return Entry.Quantity > 0;
    });
}

FString ARaidLoot::GetContainerStateText() const
{
    if (IsContainerEmpty()) return TEXT("空箱");
    if (!InventoryComponent->IsInitialized()) return TEXT("未搜索");
    if (FindNextUnsearchedIndex() == INDEX_NONE) return TEXT("已搜索");
    const bool bPartlySearched = GetContents().ContainsByPredicate([](const FRaidInventoryEntry& Entry)
    {
        return Entry.bRevealed;
    });
    return bPartlySearched ? TEXT("部分已搜索") : TEXT("未搜索");
}

void ARaidLoot::SetContainerOpen(bool bOpen)
{
    if (bOpen)
    {
        EnsureContents();
        if (!InventoryComponent->IsInitialized()) return;
    }
    bContainerOpen = bOpen;
    if (bOpen && !bEverOpened)
    {
        bEverOpened = true;
        RefreshVisuals();
    }
}

void ARaidLoot::UpdateLid(float DeltaSeconds)
{
    if (!bEverOpened || LidOpenAlpha >= 1.f) return;
    LidOpenAlpha = FMath::Min(1.f, LidOpenAlpha + FMath::Max(0.f, DeltaSeconds) / .35f);
    const float Eased = FMath::SmoothStep(0.f, 1.f, LidOpenAlpha);
    const FVector Hinge(23.f, 0.f, 32.f);
    const FQuat Rotation(FVector::RightVector, FMath::DegreesToRadians(78.f * Eased));
    for (int32 Index = 0; Index < LidParts.Num(); ++Index)
    {
        if (!IsValid(LidParts[Index]) || !ClosedLidTransforms.IsValidIndex(Index)) continue;
        const FTransform& Closed = ClosedLidTransforms[Index];
        LidParts[Index]->SetRelativeLocationAndRotation(
            Hinge + Rotation.RotateVector(Closed.GetLocation() - Hinge), Rotation * Closed.GetRotation());
    }
}

void ARaidLoot::AlignCaseToSurface()
{
    float BaseHeight = -48.f;
    if (UWorld* World = GetWorld())
    {
        FHitResult Surface;
        const FVector Origin = GetActorLocation();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(RaidLootSurface), false, this);
        if (World->LineTraceSingleByChannel(Surface, Origin + FVector(0, 0, 4), Origin - FVector(0, 0, 220), ECC_WorldStatic, Query)
            && !Surface.bStartPenetrating)
        {
            BaseHeight = Surface.ImpactPoint.Z - Origin.Z;
        }
    }
    // Keep the gameplay root/query box unchanged, while placing the case on a floor or table.
    VisualRoot->SetRelativeLocation(FVector(0, 0, BaseHeight));
}

void ARaidLoot::RefreshVisuals()
{
    RaidText::ApplyWorldFont(this);
    const bool bMedical = ItemId == FName(TEXT("Medkit"));
    const bool bAmmo = ItemId == FName(TEXT("Ammo"));
    const bool bRations = ItemId == FName(TEXT("Rations"));
    UMaterialInterface* Base = LootMesh->GetMaterial(0);
    UMaterialInterface* Body = bMedical
        ? LootPaint(TEXT("M_D_LightConcrete"), TEXT("M_Marking"), Base)
        : LootPaint(bAmmo || bRations ? TEXT("M_D_OliveSteel") : TEXT("M_D_DarkSteel"), bAmmo || bRations ? TEXT("M_Container") : TEXT("M_Ground"), Base);
    UMaterialInterface* Rubber = LootPaint(TEXT("M_D_Rubber"), TEXT("M_Ground"), Base);
    UMaterialInterface* Metal = LootPaint(TEXT("M_D_Galvanized"), TEXT("M_Wall"), Base);
    UMaterialInterface* Accent = LootPaint(TEXT("M_D_SafetyYellow"), TEXT("M_Orange"), Base);
    UMaterialInterface* MedicalPaint = LootPaint(TEXT("M_D_OliveSteel"), TEXT("M_Extraction"), Base);
    const bool bEmpty = IsContainerEmpty();
    if (bEmpty) Body = Rubber;
    if (bEverOpened) Accent = bEmpty ? Metal : MedicalPaint;
    for (UStaticMeshComponent* Part : CaseParts)
    {
        UMaterialInterface* Paint = Body;
        if (Part->ComponentHasTag(TEXT("Rubber"))) Paint = Rubber;
        else if (Part->ComponentHasTag(TEXT("Metal"))) Paint = Metal;
        else if (Part->ComponentHasTag(TEXT("Accent")) || Part->ComponentHasTag(TEXT("Ammo"))) Paint = Accent;
        else if (Part->ComponentHasTag(TEXT("Medical"))) Paint = MedicalPaint;
        Part->SetMaterial(0, Paint);
        if (Part->ComponentHasTag(TEXT("Medical"))) Part->SetVisibility(bMedical);
        if (Part->ComponentHasTag(TEXT("Ammo"))) Part->SetVisibility(bAmmo);
    }
    const FString Category = GetContainerName();
    FrontStencil->SetText(FText::FromString(Category));
    BackStencil->SetText(FText::FromString(Category));
    ValueStencil->SetText(FText::FromString(GetContainerStateText()));
    ValueStencil->SetTextRenderColor(bEmpty ? FColor(130, 137, 131)
        : (bEverOpened ? FColor(170, 206, 150) : FColor(227, 183, 93)));
    LastVisualItemId = ItemId;
    bLastVisualEnemyDrop = bEnemyDrop;
}

void ARaidLoot::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    AlignCaseToSurface();
    RefreshVisuals();
}

void ARaidLoot::BeginPlay()
{
    Super::BeginPlay();
    InventoryChangedHandle = InventoryComponent->OnInventoryChanged.AddUObject(this, &ARaidLoot::HandleInventoryChanged);
    // Runtime enemy drops set their theme after SpawnActor; keep contents lazy.
    if (InventoryComponent->IsInitialized()) Contents.Empty();
    AlignCaseToSurface();
    RefreshVisuals();
}

void ARaidLoot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    InventoryComponent->OnInventoryChanged.Remove(InventoryChangedHandle);
    InventoryChangedHandle.Reset();
    Super::EndPlay(EndPlayReason);
}

void ARaidLoot::HandleInventoryChanged(URaidInventoryComponent* Inventory, int64 Revision)
{
    if (Inventory == InventoryComponent && !IsActorBeingDestroyed()) RefreshVisuals();
}

void ARaidLoot::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateLid(DeltaSeconds);
    if (LastVisualItemId != ItemId || bLastVisualEnemyDrop != bEnemyDrop) RefreshVisuals();
    ARaidPlayer* Player = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerCharacter(this, 0));
    const bool bFocused = !bContainerOpen && IsValid(Player) && Player->Camera && Player->GetFocusedLoot() == this;
    InteractionLabel->SetVisibility(bFocused);
    InteractionLabel->SetHiddenInGame(!bFocused);
    if (bFocused)
    {
        const FString Text = FString::Printf(TEXT("%s\n%s\n[E] 打开"),
            *GetContainerName(), *GetContainerStateText());
        if (Text != LastInteractionText)
        {
            InteractionLabel->SetText(FText::FromString(Text));
            LastInteractionText = Text;
        }
        const float Yaw = (Player->Camera->GetComponentLocation() - InteractionLabel->GetComponentLocation()).Rotation().Yaw;
        InteractionLabel->SetWorldRotation(FRotator(0, Yaw, 0));
        InteractionLabel->SetTextRenderColor(IsContainerEmpty() ? FColor(154, 166, 154) : FColor(237, 242, 230));
    }
}

bool ARaidLoot::TryCollect(ARaidPlayer* Player)
{
    // Keep the old Blueprint symbol, but it cannot bypass opening or revealing.
    for (int32 Index = 0; Index < GetContents().Num(); ++Index)
    {
        if (GetContents()[Index].bRevealed && GetContents()[Index].Quantity > 0)
            return TryTakeItem(Player, Index);
    }
    return false;
}
