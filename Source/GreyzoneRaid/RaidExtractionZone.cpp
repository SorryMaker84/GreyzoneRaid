#include "RaidExtractionZone.h"
#include "RaidText.h"

#include "RaidGameMode.h"
#include "RaidPlayer.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
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
    UMaterialInterface* ExtractionPaint(const TCHAR* DepotName, const TCHAR* LegacyName, UMaterialInterface* Fallback)
    {
        const FString DepotPath = FString::Printf(TEXT("/Game/Materials/Depot/%s.%s"), DepotName, DepotName);
        if (UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, *DepotPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) return Paint;
        const FString LegacyPath = FString::Printf(TEXT("/Game/Materials/%s.%s"), LegacyName, LegacyName);
        if (UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, *LegacyPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) return Paint;
        return Fallback;
    }
}

ARaidExtractionZone::ARaidExtractionZone()
{
    PrimaryActorTick.bCanEverTick = true;
    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);
    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ExtractionTrigger"));
    Trigger->SetupAttachment(SceneRoot);
    Trigger->SetRelativeLocation(FVector(0.f, 0.f, 105.f));
    Trigger->SetBoxExtent(FVector(260.f, 260.f, 125.f));
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->SetGenerateOverlapEvents(true);
    Trigger->SetCanEverAffectNavigation(false);

    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationVisualRoot"));
    VisualRoot->SetupAttachment(SceneRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    auto Part = [&](const TCHAR* Name, FVector Location, FVector Size, const TCHAR* PaintTag, int32 Shape = 0)
    {
        UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Mesh->SetupAttachment(VisualRoot);
        Mesh->SetRelativeLocation(Location);
        Mesh->SetRelativeScale3D(Size / 100.f);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->ComponentTags.Add(FName(PaintTag));
        if (Shape == 1 && Cylinder.Succeeded()) Mesh->SetStaticMesh(Cylinder.Object);
        else if (Shape == 2 && Sphere.Succeeded()) Mesh->SetStaticMesh(Sphere.Object);
        else if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
        if (BasicMaterial.Succeeded()) Mesh->SetMaterial(0, BasicMaterial.Object);
        StationParts.Add(Mesh);
        return Mesh;
    };

    // The trigger stays identical; thin paint lines replace the old raised green platform.
    PlatformMesh = Part(TEXT("GreenPlatform"), FVector(235, 0, 20), FVector(60, 60, 40), TEXT("Cabinet"));
    Part(TEXT("BoundaryWest"), FVector(-255, 0, 1.5f), FVector(7, 520, 3), TEXT("Boundary"));
    Part(TEXT("BoundaryEast"), FVector(255, 0, 1.5f), FVector(7, 520, 3), TEXT("Boundary"));
    Part(TEXT("BoundaryNorth"), FVector(0, 255, 1.5f), FVector(520, 7, 3), TEXT("Boundary"));
    Part(TEXT("BoundarySouth"), FVector(0, -255, 1.5f), FVector(520, 7, 3), TEXT("Boundary"));
    for (int32 X : {-1, 1})
    {
        for (int32 Y : {-1, 1})
        {
            const FVector Corner(X * 245.f, Y * 245.f, 0);
            const FString Prefix = FString::Printf(TEXT("Marker_%d_%d"), X, Y);
            Part(*(Prefix + TEXT("_Foot")), Corner + FVector(0, 0, 2.5f), FVector(24, 24, 5), TEXT("Cabinet"));
            Part(*(Prefix + TEXT("_Post")), Corner + FVector(0, 0, 28), FVector(12, 12, 50), TEXT("Metal"));
            SignalLenses.Add(Part(*(Prefix + TEXT("_Lens")), Corner + FVector(0, 0, 59), FVector(17, 17, 13), TEXT("Lens"), 2));
        }
    }
    Part(TEXT("SignalMast"), FVector(235, 0, 148), FVector(10, 10, 220), TEXT("Metal"), 1);
    Part(TEXT("SignalBoard"), FVector(228, 0, 163), FVector(12, 152, 88), TEXT("Cabinet"));
    Part(TEXT("ScreenInset"), FVector(221.5f, 0, 161), FVector(1.5f, 140, 76), TEXT("Screen"));
    Part(TEXT("MastCap"), FVector(235, 0, 261), FVector(30, 30, 7), TEXT("Metal"));
    SignalLenses.Add(Part(TEXT("SignalBeacon"), FVector(235, 0, 278), FVector(26, 26, 27), TEXT("Lens"), 2));

    auto Text = [&](const TCHAR* Name, FVector Location, float Size, const FString& Caption)
    {
        UTextRenderComponent* Label = CreateDefaultSubobject<UTextRenderComponent>(Name);
        Label->SetupAttachment(VisualRoot);
        Label->SetRelativeLocation(Location);
        Label->SetRelativeRotation(FRotator(0, 180, 0));
        Label->SetHorizontalAlignment(EHTA_Center);
        Label->SetVerticalAlignment(EVRTA_TextCenter);
        Label->SetWorldSize(Size);
        Label->SetText(FText::FromString(Caption));
        Label->SetTextRenderColor(FColor(237, 242, 230));
        Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Label->SetCanEverAffectNavigation(false);
        return Label;
    };
    StationTitle = Text(TEXT("StationTitle"), FVector(220.4f, 0, 190), 9.f, TEXT("撤离点 / E-07"));
    StatusDisplay = Text(TEXT("StationStatus"), FVector(220.3f, 0, 155), 11.f, TEXT("待命\n区内停留 08秒"));
    Text(TEXT("CabinetStencil"), FVector(204.8f, 0, 23), 6.f, TEXT("回收\n信号装置"));
    SignalLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SignalLight"));
    SignalLight->SetupAttachment(VisualRoot);
    SignalLight->SetRelativeLocation(FVector(235, 0, 280));
    SignalLight->SetIntensity(180.f);
    SignalLight->SetAttenuationRadius(420.f);
    SignalLight->SetCastShadows(false);
    SignalLight->SetLightColor(FLinearColor(.9f, .6f, .2f));
}

void ARaidExtractionZone::AlignStationToSurface()
{
    float SurfaceOffset = 0.f;
    if (UWorld* World = GetWorld())
    {
        const FVector Origin = GetActorLocation();
        FHitResult Surface;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(RaidExtractionSurface), false, this);
        if (World->LineTraceSingleByChannel(Surface, Origin + FVector(0, 0, 4), Origin - FVector(0, 0, 80), ECC_WorldStatic, Query)
            && !Surface.bStartPenetrating)
        {
            SurfaceOffset = Surface.ImpactPoint.Z - Origin.Z;
        }
    }
    // Painted lines touch the road, while the existing gameplay volume keeps its transform.
    VisualRoot->SetRelativeLocation(FVector(0, 0, SurfaceOffset));
}

void ARaidExtractionZone::RefreshVisuals()
{
    RaidText::ApplyWorldFont(this);
    StationTitle->SetText(FText::FromString(FString::Printf(TEXT("%s / %s"), *DisplayName, *ExtractionId.ToString())));
    UMaterialInterface* Base = PlatformMesh->GetMaterial(0);
    UMaterialInterface* Cabinet = ExtractionPaint(TEXT("M_D_DarkSteel"), TEXT("M_Ground"), Base);
    UMaterialInterface* Metal = ExtractionPaint(TEXT("M_D_Galvanized"), TEXT("M_Wall"), Base);
    UMaterialInterface* Boundary = ExtractionPaint(TEXT("M_D_RoadPaint"), TEXT("M_Marking"), Base);
    UMaterialInterface* Screen = ExtractionPaint(TEXT("M_D_Rubber"), TEXT("M_Ground"), Base);
    StandbyPaint = ExtractionPaint(TEXT("M_D_WarmLamp"), TEXT("M_Orange"), Base);
    ReadyPaint = ExtractionPaint(TEXT("M_D_ExtractionLight"), TEXT("M_Extraction"), Base);
    FailedPaint = ExtractionPaint(TEXT("M_D_RustSteel"), TEXT("M_Orange"), Base);
    // Existing map material overrides cannot turn the signal cabinet into a giant green slab.
    PlatformMesh->SetRelativeLocation(FVector(235, 0, 20));
    PlatformMesh->SetRelativeScale3D(FVector(.6f, .6f, .4f));
    for (UStaticMeshComponent* Part : StationParts)
    {
        UMaterialInterface* Paint = Cabinet;
        if (Part->ComponentHasTag(TEXT("Metal"))) Paint = Metal;
        else if (Part->ComponentHasTag(TEXT("Boundary"))) Paint = Boundary;
        else if (Part->ComponentHasTag(TEXT("Screen"))) Paint = Screen;
        else if (Part->ComponentHasTag(TEXT("Lens"))) Paint = StandbyPaint;
        Part->SetMaterial(0, Paint);
    }
    LastSignalState = -1;
}

void ARaidExtractionZone::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    AlignStationToSurface();
    RefreshVisuals();
}

void ARaidExtractionZone::BeginPlay()
{
    Super::BeginPlay();
    RemainingSeconds = FMath::Max(.1f, ExtractSeconds);
    AlignStationToSurface();
    RefreshVisuals();
    UpdateSignal(Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this)),
        Cast<ARaidPlayer>(UGameplayStatics::GetPlayerCharacter(this, 0)));
}

bool ARaidExtractionZone::IsAvailable() const
{
    if (RequiredMissionId.IsNone()) return true;
    const ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    return Mode && Mode->IsMissionCompleted(RequiredMissionId);
}

FString ARaidExtractionZone::GetUnavailableReason() const
{
    return IsAvailable() ? FString() : TEXT("下载通讯情报后解锁");
}

void ARaidExtractionZone::UpdateSignal(ARaidGameMode* GM, ARaidPlayer* Player)
{
    int32 SignalState = 0;
    FString Status = FString::Printf(TEXT("待命\n区内停留 %02d秒"), FMath::CeilToInt(ExtractSeconds));
    if (GM && GM->State == ERaidState::Extracted)
    {
        SignalState = 1;
        Status = TEXT("撤离完成\n物资已保全");
    }
    else if ((GM && GM->State == ERaidState::Dead) || (IsValid(Player) && !Player->IsAlive()))
    {
        SignalState = 2;
        Status = TEXT("信号中断\n行动失败");
    }
    else if (!IsAvailable())
    {
        SignalState = 2;
        Status = TEXT("暂未开放\n需要通讯情报");
    }
    else if (bPlayerInside && IsValid(Player) && Player->GetLootCount() < 1)
    {
        Status = TEXT("需要物资\n搜索仓储区");
    }
    else if (bCountdownRunning)
    {
        SignalState = 1;
        Status = FString::Printf(TEXT("撤离倒计时\n%04.1f 秒"), RemainingSeconds);
    }
    if (Status != LastStatusText)
    {
        StatusDisplay->SetText(FText::FromString(Status));
        LastStatusText = Status;
    }
    const FColor Color = SignalState == 1 ? FColor(126, 211, 145)
        : (SignalState == 2 ? FColor(220, 103, 77) : FColor(227, 183, 93));
    if (SignalState != LastSignalState)
    {
        UMaterialInterface* Paint = SignalState == 1 ? ReadyPaint.Get()
            : (SignalState == 2 ? FailedPaint.Get() : StandbyPaint.Get());
        for (UStaticMeshComponent* Lens : SignalLenses) Lens->SetMaterial(0, Paint);
        SignalLight->SetLightColor(FLinearColor(Color));
        StatusDisplay->SetTextRenderColor(Color);
        LastSignalState = SignalState;
    }
    const float Pulse = bCountdownRunning && GetWorld() ? .75f + .25f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.f) : 1.f;
    SignalLight->SetIntensity((SignalState == 1 ? 300.f : 180.f) * Pulse);
}

void ARaidExtractionZone::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ARaidPlayer* Player = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerCharacter(this, 0));
    ARaidGameMode* GM = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    // Progress belongs to one possessed pawn. A replacement must not inherit a
    // nearly completed countdown even when both pawns overlap in the same tick.
    if (CountdownPlayer.Get() != Player)
    {
        CountdownPlayer = Player;
        RemainingSeconds = FMath::Max(.1f, ExtractSeconds);
        bCountdownRunning = false;
        LastCountdownDamageTime = IsValid(Player) ? Player->LastDamageTime : -100.f;
    }
    const bool bWasInside = bPlayerInside;
    // Poll the actual overlap so missed startup/end overlap callbacks cannot extract a distant player.
    bPlayerInside = IsValid(Player) && Trigger->IsOverlappingActor(Player);
    if (!GM || !GM->IsRaidActive() || !IsValid(Player) || !Player->CanAct())
    {
        // Preserve the completed zero for result presentation and the Depot
        // regression checks. An interrupted live raid always returns to full time.
        if (!GM || GM->State != ERaidState::Extracted)
            RemainingSeconds = FMath::Max(.1f, ExtractSeconds);
        bCountdownRunning = false;
        LastCountdownDamageTime = IsValid(Player) ? Player->LastDamageTime : -100.f;
        UpdateSignal(GM, Player);
        return;
    }
    if (!bPlayerInside || !IsAvailable() || Player->GetLootCount() < 1)
    {
        LastCountdownDamageTime = Player->LastDamageTime;
        RemainingSeconds = FMath::Max(.1f, ExtractSeconds);
        bCountdownRunning = false;
        if (bPlayerInside && !bWasInside && !IsAvailable())
        {
            GM->PushMessage(TEXT("此撤离点需要通讯情报。可以先前往南侧公路撤离。"), 5.f);
        }
        else if (bPlayerInside && !bWasInside && Player->GetLootCount() < 1)
        {
            GM->PushMessage(TEXT("至少搜集一件物资后才能撤离。"));
        }
        UpdateSignal(GM, Player);
        return;
    }
    if (Player->LastDamageTime > LastCountdownDamageTime)
    {
        LastCountdownDamageTime = Player->LastDamageTime;
        RemainingSeconds = FMath::Max(.1f, ExtractSeconds);
        bCountdownRunning = true;
        GM->PushMessage(TEXT("受伤导致撤离中断，倒计时已重新开始。"));
        UpdateSignal(GM, Player);
        return;
    }
    if (!bCountdownRunning)
    {
        bCountdownRunning = true;
        GM->PushMessage(TEXT("撤离已开始，请留在标记区域内。"));
    }
    RemainingSeconds = FMath::Max(0.f, RemainingSeconds - DeltaSeconds);
    if (RemainingSeconds <= 0.f && GM->IsRaidActive() && Player->IsAlive()
        && IsAvailable() && Trigger->IsOverlappingActor(Player) && Player->GetLootCount() > 0)
    {
        GM->EndRaid(true);
    }
    UpdateSignal(GM, Player);
}
