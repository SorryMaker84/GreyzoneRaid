#include "RaidMissionObjective.h"

#include "RaidGameMode.h"
#include "RaidMissionDefinition.h"
#include "RaidPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARaidMissionObjective::ARaidMissionObjective()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);
    InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TerminalInteractionBox"));
    InteractionBox->SetupAttachment(SceneRoot);
    InteractionBox->SetRelativeLocation(FVector(0.f, 0.f, 85.f));
    InteractionBox->SetBoxExtent(FVector(45.f, 50.f, 85.f));
    InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    InteractionBox->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionBox->SetCollisionResponseToAllChannels(ECR_Block);
    InteractionBox->SetGenerateOverlapEvents(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> CabinetPaint(
        TEXT("/Game/Materials/Depot/M_D_DarkSteel.M_D_DarkSteel"), LOAD_Quiet | LOAD_NoWarn);
    // This project-owned material exposes Color and emissive output, so a locked,
    // downloading and completed terminal has a clearly visible amber/blue/green signal.
    static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> SignalPaint(
        TEXT("/Game/Materials/M_WeaponFX.M_WeaponFX"), LOAD_Quiet | LOAD_NoWarn);
    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalBase"));
    BaseMesh->SetupAttachment(SceneRoot);
    BaseMesh->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
    BaseMesh->SetRelativeScale3D(FVector(.7f, .8f, .9f));
    ScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalScreen"));
    ScreenMesh->SetupAttachment(SceneRoot);
    ScreenMesh->SetRelativeLocation(FVector(22.f, 0.f, 120.f));
    ScreenMesh->SetRelativeScale3D(FVector(.025f, .82f, .48f));
    UStaticMeshComponent* ScreenHousing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalScreenHousing"));
    ScreenHousing->SetupAttachment(SceneRoot);
    ScreenHousing->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
    ScreenHousing->SetRelativeScale3D(FVector(.4f, 1.f, .7f));
    UStaticMeshComponent* ControlShelf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalControlShelf"));
    ControlShelf->SetupAttachment(SceneRoot);
    ControlShelf->SetRelativeLocation(FVector(30.f, 0.f, 82.f));
    ControlShelf->SetRelativeScale3D(FVector(.45f, .95f, .1f));
    for (UStaticMeshComponent* Mesh : {BaseMesh.Get(), ScreenMesh.Get(), ScreenHousing, ControlShelf})
    {
        if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
        if (Paint.Succeeded()) Mesh->SetMaterial(0, Paint.Object);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCanEverAffectNavigation(false);
    }
    if (CabinetPaint.Get())
        for (UStaticMeshComponent* Mesh : {BaseMesh.Get(), ScreenHousing, ControlShelf})
            Mesh->SetMaterial(0, CabinetPaint.Get());
    if (SignalPaint.Get()) ScreenMesh->SetMaterial(0, SignalPaint.Get());
}

void ARaidMissionObjective::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshVisuals();
}

void ARaidMissionObjective::BeginPlay()
{
    Super::BeginPlay();
    SetActorTickEnabled(false);
    FString Error;
    if (!MissionDefinition || !MissionDefinition->Validate(&Error))
    {
        UE_LOG(LogTemp, Error, TEXT("R7_MISSION_CONFIG: %s has no valid mission definition: %s"), *GetName(), *Error);
    }
    else if (ARaidGameMode* Mode = GetRaidMode()) Mode->RegisterMissionObjective(this);
    RefreshVisuals();
}

void ARaidMissionObjective::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    UnbindPlayerDamage();
    ActivePlayer.Reset();
    InteractionStartedAt = -1.0;
    if (ARaidGameMode* Mode = GetRaidMode()) Mode->UnregisterMissionObjective(this);
    Super::EndPlay(EndPlayReason);
}

ARaidGameMode* ARaidMissionObjective::GetRaidMode() const
{
    return Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
}

bool ARaidMissionObjective::HasValidDefinition() const
{
    return IsValid(MissionDefinition) && MissionDefinition->Validate();
}

FName ARaidMissionObjective::GetMissionId() const
{
    return HasValidDefinition() ? MissionDefinition->MissionId : NAME_None;
}

FRaidMissionSnapshot ARaidMissionObjective::GetSnapshot() const
{
    FRaidMissionSnapshot Snapshot;
    if (!HasValidDefinition()) return Snapshot;
    Snapshot.bPresent = true;
    Snapshot.bCompleted = bCompleted;
    Snapshot.bInProgress = ActivePlayer.IsValid() && InteractionStartedAt >= 0.0;
    Snapshot.MissionId = MissionDefinition->MissionId;
    Snapshot.Title = MissionDefinition->Title;
    Snapshot.Instruction = MissionDefinition->Instruction;
    Snapshot.WorldLocation = InteractionBox->GetComponentLocation();
    Snapshot.HoldSeconds = MissionDefinition->HoldSeconds;
    Snapshot.Progress = bCompleted ? 1.f : Snapshot.bInProgress && GetWorld()
        ? FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - InteractionStartedAt)
            / MissionDefinition->HoldSeconds), 0.f, 1.f) : 0.f;
    Snapshot.StatusText = bCompleted ? MissionDefinition->CompletedText
        : Snapshot.bInProgress ? TEXT("正在下载：保持长按 E，松手或受伤会中断")
        : TEXT("可选目标：下载情报后解锁北侧撤离");
    return Snapshot;
}

bool ARaidMissionObjective::CanInteract(const ARaidPlayer* Player) const
{
    const ARaidGameMode* Mode = GetRaidMode();
    if (bEndingPlay || IsActorBeingDestroyed() || !HasValidDefinition() || bCompleted
        || !Mode || !Mode->IsRaidActive() || !IsValid(Player) || !Player->CanAct()
        || !Player->Camera || Player->GetWorld() != GetWorld() || Player->IsInventoryOpen()) return false;
    const FVector Start = Player->Camera->GetComponentLocation();
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(RaidMissionFocus), false, Player);
    return GetWorld()->LineTraceSingleByChannel(Hit, Start,
        Start + Player->Camera->GetForwardVector() * MissionDefinition->InteractionRange,
        ECC_Visibility, Query) && Hit.GetActor() == this;
}

bool ARaidMissionObjective::TryBeginInteraction(ARaidPlayer* Player)
{
    if (!CanInteract(Player)) return false;
    if (ActivePlayer.IsValid()) return ActivePlayer.Get() == Player;
    ARaidGameMode* Mode = GetRaidMode();
    if (!Mode || Mode->GetMissionObjective() != this) return false;
    Player->StopFire();
    Player->StopAim();
    Player->CancelReload();
    ActivePlayer = Player;
    InteractionStartedAt = GetWorld()->GetTimeSeconds();
    StartingDamageTime = Player->LastDamageTime;
    Player->OnTakeAnyDamage.AddDynamic(this, &ARaidMissionObjective::HandlePlayerDamage);
    SetActorTickEnabled(true);
    Mode->PushMessage(TEXT("正在下载情报：保持长按 E，松手、离开或受伤会中断。"), 4.f);
    NotifyChanged();
    UE_LOG(LogTemp, Display, TEXT("R7_MISSION_BEGIN: id=%s duration=%.2f"), *GetMissionId().ToString(), MissionDefinition->HoldSeconds);
    return true;
}

void ARaidMissionObjective::UnbindPlayerDamage()
{
    if (ARaidPlayer* Player = ActivePlayer.Get())
        Player->OnTakeAnyDamage.RemoveDynamic(this, &ARaidMissionObjective::HandlePlayerDamage);
}

void ARaidMissionObjective::CancelInteraction(ARaidPlayer* Player)
{
    if (InteractionStartedAt < 0.0 || (Player && ActivePlayer.Get() != Player)) return;
    UnbindPlayerDamage();
    ActivePlayer.Reset();
    InteractionStartedAt = -1.0;
    SetActorTickEnabled(false);
    if (ARaidGameMode* Mode = GetRaidMode(); Mode && Mode->IsRaidActive())
        Mode->PushMessage(TEXT("下载已中断。重新长按 E 可再次尝试；南侧撤离仍然开放。"), 4.f);
    NotifyChanged();
    UE_LOG(LogTemp, Display, TEXT("R7_MISSION_CANCEL: id=%s progress_reset=1"), *GetMissionId().ToString());
}

void ARaidMissionObjective::HandlePlayerDamage(AActor* DamagedActor, float Damage,
    const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
    if (Damage > 0.f && DamagedActor == ActivePlayer.Get()) CancelInteraction();
}

void ARaidMissionObjective::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ARaidPlayer* Player = ActivePlayer.Get();
    if (!Player || !CanInteract(Player) || Player->LastDamageTime > StartingDamageTime)
    {
        CancelInteraction();
        return;
    }
    if (GetSnapshot().Progress >= 1.f) CompleteInteraction();
}

void ARaidMissionObjective::CompleteInteraction()
{
    ARaidPlayer* Player = ActivePlayer.Get();
    if (!Player || !CanInteract(Player) || Player->LastDamageTime > StartingDamageTime) return;
    UnbindPlayerDamage();
    ActivePlayer.Reset();
    InteractionStartedAt = -1.0;
    bCompleted = true;
    SetActorTickEnabled(false);
    if (ARaidGameMode* Mode = GetRaidMode()) Mode->PushMessage(MissionDefinition->CompletedText, 8.f);
    NotifyChanged();
    UE_LOG(LogTemp, Display, TEXT("R7_MISSION_COMPLETE: id=%s"), *GetMissionId().ToString());
}

void ARaidMissionObjective::NotifyChanged()
{
    RefreshVisuals();
    if (!bEndingPlay)
        if (ARaidGameMode* Mode = GetRaidMode()) Mode->NotifyMissionChanged(this);
}

void ARaidMissionObjective::RefreshVisuals()
{
    if (!ScreenMaterial && ScreenMesh->GetMaterial(0))
        ScreenMaterial = ScreenMesh->CreateDynamicMaterialInstance(0);
    if (ScreenMaterial)
    {
        const FLinearColor Color = bCompleted ? FLinearColor(.08f, .8f, .22f)
            : ActivePlayer.IsValid() ? FLinearColor(.06f, .55f, 1.f) : FLinearColor(.9f, .48f, .05f);
        ScreenMaterial->SetVectorParameterValue(TEXT("Color"), Color * .08f);
    }
}
