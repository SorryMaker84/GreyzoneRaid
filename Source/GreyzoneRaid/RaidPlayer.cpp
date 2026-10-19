#include "RaidPlayer.h"
#include "RaidTreatmentComponent.h"
#include "RaidArmsAnimInstance.h"

#include "RaidGameMode.h"
#include "RaidInventoryComponent.h"
#include "RaidInteractionComponent.h"
#include "RaidItemCatalog.h"
#include "RaidPlayerController.h"
#include "RaidLoot.h"
#include "RaidEnemy.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace RaidPlayerSettings
{
    constexpr float WalkSpeed = 350.f;
    constexpr float SprintSpeed = 560.f;
    constexpr float WeaponDamage = 34.f;
    constexpr float WeaponRange = 7000.f;
}

ARaidPlayer::ARaidPlayer()
{
    PrimaryActorTick.bCanEverTick = true;
    InventoryComponent = CreateDefaultSubobject<URaidInventoryComponent>(TEXT("Backpack"));
    InteractionComponent = CreateDefaultSubobject<URaidInteractionComponent>(TEXT("Interaction"));
    TreatmentComponent = CreateDefaultSubobject<URaidTreatmentComponent>(TEXT("Treatment"));
    ItemCatalogAsset = TSoftObjectPtr<URaidItemCatalog>(FSoftObjectPath(TEXT("/Game/Data/DA_RaidItems.DA_RaidItems")));
    GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
    // Enemy shots trace Visibility; the default Pawn profile ignores that channel.
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    bUseControllerRotationYaw = true;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxWalkSpeed = RaidPlayerSettings::WalkSpeed;
    GetCharacterMovement()->JumpZVelocity = 450.f;
    GetCharacterMovement()->AirControl = 0.25f;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
    Camera->bUsePawnControlRotation = true;
    Camera->FieldOfView = 95.f;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SimpleMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponPose"));
    WeaponRoot->SetupAttachment(Camera);
    WeaponRoot->SetRelativeLocation(FVector(24.f, 17.f, -16.f));

    const FLinearColor Graphite(0.075f, 0.082f, 0.078f);
    const FLinearColor Metal(0.15f, 0.17f, 0.18f);
    const FLinearColor Polymer(0.035f, 0.043f, 0.037f);
    const FLinearColor Sand(0.31f, 0.255f, 0.17f);
    const FLinearColor Glove(0.09f, 0.102f, 0.072f);
    const FLinearColor Sleeve(0.18f, 0.20f, 0.135f);

    // All lengths below are centimetres. These original parts form a recognisable
    // carbine: receiver, ventilated handguard, barrel, stock, magazine, grip and optic.
    auto Part = [this](const TCHAR* Name, UStaticMesh* PartMesh,
        const FVector& Position, const FVector& Size, const FRotator& Rotation,
        const FLinearColor& Color, bool bGunPart = true)
    {
        UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(FName(Name));
        Component->SetupAttachment(WeaponRoot);
        Component->SetStaticMesh(PartMesh);
        Component->SetRelativeLocation(Position);
        Component->SetRelativeScale3D(Size / 100.f);
        Component->SetRelativeRotation(Rotation);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(false);
        Component->SetCanEverAffectNavigation(false);
        if (SimpleMaterial.Succeeded()) Component->SetMaterial(0, SimpleMaterial.Object);
        WeaponParts.Add(Component);
        PartColors.Add(Color);
        PartPositions.Add(Position);
        if (bGunPart) OriginalGunParts.Add(Component);
        if (!bGunPart && (FString(Name).Contains(TEXT("Glove")) || FString(Name).Contains(TEXT("Finger")) || FString(Name).Contains(TEXT("Sleeve"))))
            OriginalArmsParts.Add(Component);
        return Component;
    };
    const FRotator Straight = FRotator::ZeroRotator;
    const FRotator AlongBarrel(-90.f, 0.f, 0.f);
    GunMesh = Part(TEXT("GunBody"), Cube.Object, FVector(16,0,0), FVector(24,6,7), Straight, Graphite);
    Part(TEXT("LowerReceiver"), Cube.Object, FVector(13,0,-4.5f), FVector(17,5.3f,4), Straight, Graphite);
    Part(TEXT("BufferTube"), Cylinder.Object, FVector(0,0,-1), FVector(3.3f,3.3f,14), AlongBarrel, Metal);
    Part(TEXT("AdjustableStock"), Cube.Object, FVector(-4,0,-3), FVector(15,5.3f,5.5f), FRotator(8,0,0), Sand);
    Part(TEXT("StockHeel"), Cube.Object, FVector(-11,0,-5), FVector(2.3f,5.8f,11), Straight, Polymer);
    Part(TEXT("CheekRest"), Cube.Object, FVector(-4,0,0.2f), FVector(12,4.8f,2), Straight, Graphite);
    Part(TEXT("Handguard"), Cube.Object, FVector(40,0,0.5f), FVector(25,5.6f,6), Straight, Sand);
    Part(TEXT("HandguardTopRail"), Cube.Object, FVector(40,0,4), FVector(27,3,1), Straight, Metal);
    BarrelMesh = Part(TEXT("GunBarrel"), Cylinder.Object, FVector(61,0,1), FVector(1.9f,1.9f,25), AlongBarrel, Metal);
    Part(TEXT("GasBlock"), Cube.Object, FVector(56,0,2.4f), FVector(3,2.6f,3.5f), Straight, Graphite);
    Part(TEXT("FlashHider"), Cylinder.Object, FVector(74,0,1), FVector(2.8f,2.8f,4.5f), AlongBarrel, Graphite);
    Part(TEXT("FlashHiderOpening"), Cylinder.Object, FVector(76.3f,0,1), FVector(1.9f,1.9f,0.2f), AlongBarrel, Polymer);
    Part(TEXT("MagazineWell"), Cube.Object, FVector(22,0,-5.5f), FVector(7,5.7f,5), Straight, Graphite);
    MagazineMesh = Part(TEXT("RemovableMagazine"), Cube.Object, FVector(21,0,-14.5f), FVector(6.8f,4.7f,15), FRotator(-12,0,0), Polymer);
    MagazineMesh->ComponentTags.Add(TEXT("Magazine"));
    Part(TEXT("MagazineBase"), Cube.Object, FVector(23,0,-22), FVector(7.1f,5,1.4f), FRotator(-12,0,0), Graphite)->ComponentTags.Add(TEXT("Magazine"));
    Part(TEXT("PistolGrip"), Cube.Object, FVector(5,0,-10.5f), FVector(4.5f,4,11), FRotator(-19,0,0), Polymer);
    Part(TEXT("TriggerGuardFront"), Cube.Object, FVector(14,0,-7.9f), FVector(1,4.8f,3.5f), Straight, Graphite);
    Part(TEXT("TriggerGuardBase"), Cube.Object, FVector(11,0,-9.5f), FVector(7,4.8f,1), Straight, Graphite);
    Part(TEXT("Trigger"), Cube.Object, FVector(10,0,-7.5f), FVector(0.8f,1,3), FRotator(-20,0,0), Metal);
    Part(TEXT("EjectionPort"), Cube.Object, FVector(20,-3.1f,0.2f), FVector(8,0.25f,2.5f), Straight, Polymer);
    Part(TEXT("Bolt"), Cube.Object, FVector(20,-3.3f,0.3f), FVector(4.8f,0.2f,1.6f), Straight, Metal);
    Part(TEXT("ChargingHandle"), Cube.Object, FVector(7,-3.7f,2.7f), FVector(2.2f,3,1.3f), Straight, Polymer);
    Part(TEXT("FireSelector"), Cylinder.Object, FVector(7,-2.8f,-2.4f), FVector(1.3f,1.3f,1), FRotator(0,0,90), Metal);
    Part(TEXT("OpticMount"), Cube.Object, FVector(18,0,5), FVector(8,4,2), Straight, Graphite);
    Part(TEXT("OpticBottom"), Cube.Object, FVector(18,0,6.9f), FVector(4.5f,8,1.2f), Straight, Polymer);
    Part(TEXT("OpticTop"), Cube.Object, FVector(18,0,14.3f), FVector(4.5f,8,1.2f), Straight, Polymer);
    Part(TEXT("OpticLeft"), Cube.Object, FVector(18,-3.5f,10.6f), FVector(4.5f,1,7.2f), Straight, Polymer);
    Part(TEXT("OpticRight"), Cube.Object, FVector(18,3.5f,10.6f), FVector(4.5f,1,7.2f), Straight, Polymer);
    OpticDot = Part(TEXT("OpticRedDot"), Sphere.Object, FVector(19,0,10.6f), FVector(.28f,.28f,.28f), Straight, FLinearColor(4.f,.025f,.012f));
    for (int32 Index = 0; Index < 11; ++Index)
    {
        Part(*FString::Printf(TEXT("RailTooth_%02d"), Index), Cube.Object,
            FVector(28.f + Index * 2.3f,0,4.9f), FVector(1.1f,3.7f,.8f), Straight, Metal);
        if (Index < 8)
        {
            Part(*FString::Printf(TEXT("VentSlot_%02d"), Index), Cube.Object,
                FVector(32.f + Index * 2.65f,-2.86f,0), FVector(1.65f,.16f,2.5f), Straight, Polymer);
            Part(*FString::Printf(TEXT("MagazineRib_%02d"), Index), Cube.Object,
                FVector(21.7f,-2.45f,-10.f-Index*1.35f), FVector(5.4f,.3f,.5f), FRotator(-12,0,0), Graphite)->ComponentTags.Add(TEXT("Magazine"));
        }
    }
    Part(TEXT("RightGlove"), Sphere.Object, FVector(5,-2.2f,-10), FVector(6.5f,5,7), FRotator(-19,0,0), Glove, false);
    Part(TEXT("TriggerFinger"), Cube.Object, FVector(10,-2.6f,-6.5f), FVector(5.6f,1.3f,1.4f), FRotator(-12,0,0), Glove, false);
    SupportHand = Part(TEXT("SupportGlove"), Sphere.Object, FVector(40,-3.8f,-3.6f), FVector(7.4f,5.5f,6), Straight, Glove, false);
    Part(TEXT("RightSleeve"), Cylinder.Object, FVector(-1,-6,-23), FVector(8,8,27), FRotator(35,22,0), Sleeve, false);
    Part(TEXT("LeftSleeve"), Cylinder.Object, FVector(26,-8,-17), FVector(8,8,32), FRotator(45,20,0), Sleeve, false);

    ImportedWeapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OptionalWeaponModel"));
    ImportedWeapon->SetupAttachment(WeaponRoot);
    ImportedWeapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ImportedWeapon->SetCastShadow(false);
    ImportedWeapon->SetCanEverAffectNavigation(false);
    ImportedWeapon->SetVisibility(false);
    ImportedMagazine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OptionalMagazineModel"));
    ImportedMagazine->SetupAttachment(WeaponRoot);
    ImportedMagazine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ImportedMagazine->SetCastShadow(false);
    ImportedMagazine->SetCanEverAffectNavigation(false);
    ImportedMagazine->SetVisibility(false);
    ImportedArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("OptionalFirstPersonArms"));
    ImportedArms->SetupAttachment(WeaponRoot);
    ImportedArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ImportedArms->SetCastShadow(false);
    ImportedArms->SetCanEverAffectNavigation(false);
    ImportedArms->SetVisibility(false);
    // Fire/reload timers run after physics. Sample their presentation state after
    // those timers, so the hands and rigid magazine use the same frame's phase.
    ImportedArms->PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

    MuzzleFlame = Part(TEXT("MuzzleFlame"), Sphere.Object, FVector(79,0,1), FVector(7.5f,1.6f,1.6f), Straight, FLinearColor(7,2.2f,.12f), false);
    MuzzleFlameCross = Part(TEXT("MuzzleFlameCross"), Cube.Object, FVector(78,0,1), FVector(5,4.3f,.4f), FRotator(0,0,35), FLinearColor(6,1.3f,.06f), false);
    MuzzleFlame->SetVisibility(false);
    MuzzleFlameCross->SetVisibility(false);

    MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleFlash"));
    MuzzleLight->SetupAttachment(WeaponRoot);
    MuzzleLight->SetRelativeLocation(FVector(77.f, 0.f, 1.f));
    MuzzleLight->SetLightColor(FLinearColor(1.f, 0.55f, 0.12f));
    MuzzleLight->SetIntensity(1800.f);
    MuzzleLight->SetAttenuationRadius(180.f);
    MuzzleLight->SetCastShadows(false);
    MuzzleLight->SetVisibility(false);

    for (int32 Index = 0; Index < 8; ++Index)
    {
        UStaticMeshComponent* Casing = Part(*FString::Printf(TEXT("SpentCasing_%02d"), Index),
            Cylinder.Object, FVector::ZeroVector, FVector(.65f,.65f,1.9f), Straight, FLinearColor(.65f,.4f,.09f), false);
        Casing->SetVisibility(false);
        CasingPool.Add(Casing);
        CasingPositions.Add(FVector::ZeroVector);
        CasingVelocities.Add(FVector::ZeroVector);
        CasingAges.Add(-1.f);
        UStaticMeshComponent* Tracer = Part(*FString::Printf(TEXT("ShotTracer_%02d"), Index),
            Cube.Object, FVector::ZeroVector, FVector(1.f,.14f,.14f), Straight, FLinearColor(8,4,.7f), false);
        Tracer->SetVisibility(false);
        TracerPool.Add(Tracer);
        TracerAges.Add(-1.f);
    }

    // Assets are optional. When supplied at these paths they are hard references,
    // so the cooker includes them without an additional runtime file dependency.
    static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> FX(TEXT("/Game/Materials/M_WeaponFX.M_WeaponFX"), LOAD_Quiet | LOAD_NoWarn);
    EffectMaterial = FX.Get();
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> ShotAudio(TEXT("/Game/Audio/S_RifleShot.S_RifleShot"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> ReloadAudio(TEXT("/Game/Audio/S_Reload.S_Reload"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> EmptyAudio(TEXT("/Game/Audio/S_Empty.S_Empty"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> HitAudio(TEXT("/Game/Audio/S_Hit.S_Hit"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> FootstepAudio(TEXT("/Game/Audio/S_Footstep.S_Footstep"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> RifleModel(TEXT("/Game/Weapons/SM_ClassicRifle.SM_ClassicRifle"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> FPSArms(TEXT("/Game/Weapons/SK_FPSArms.SK_FPSArms"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> ArmsIdle(TEXT("/Game/Weapons/A_FPSArms_Idle.A_FPSArms_Idle"), LOAD_Quiet | LOAD_NoWarn);
    FireSound = ShotAudio.Get();
    ReloadSound = ReloadAudio.Get();
    EmptySound = EmptyAudio.Get();
    HitSound = HitAudio.Get();
    FootstepSound = FootstepAudio.Get();
    WeaponModel = RifleModel.Get();
    ArmsModel = FPSArms.Get();
    ArmsIdleAnimation = ArmsIdle.Get();
}

void ARaidPlayer::BeginPlay()
{
    Super::BeginPlay();
    InventoryComponent->InitializeOnce({}, MaxLootItems, false, MaxLootWeightGrams);
    ItemCatalog = ItemCatalogAsset.LoadSynchronous();
    FString CatalogError;
    if (!ItemCatalog || !ItemCatalog->Validate(&CatalogError))
        UE_LOG(LogTemp, Error, TEXT("RAID_ITEM_CATALOG: missing/invalid player catalog: %s"), *CatalogError);

    for (int32 Index = 0; Index < WeaponParts.Num(); ++Index)
    {
        UStaticMeshComponent* Part = WeaponParts[Index];
        const bool bEmissive = Part == MuzzleFlame || Part == MuzzleFlameCross || Part == OpticDot || TracerPool.Contains(Part);
        if (bEmissive && EffectMaterial) Part->SetMaterial(0, EffectMaterial);
        if (UMaterialInstanceDynamic* Material = Part->CreateAndSetMaterialInstanceDynamic(0))
        {
            Material->SetVectorParameterValue(TEXT("Color"), PartColors[Index]);
            Material->SetScalarParameterValue(TEXT("Roughness"), .5f);
        }
    }
    if (WeaponModel)
    {
        ImportedWeapon->SetStaticMesh(WeaponModel);
        ImportedWeapon->SetRelativeLocation(WeaponModelOffset);
        ImportedWeapon->SetRelativeRotation(WeaponModelRotation);
        ImportedWeapon->SetRelativeScale3D(WeaponModelScale);
        ImportedWeapon->SetVisibility(true);
        for (UStaticMeshComponent* Part : OriginalGunParts) Part->SetVisibility(false);
    }
    if (ArmsModel)
    {
        ImportedArms->SetSkeletalMesh(ArmsModel);
        ImportedArms->EmptyOverrideMaterials();
        ImportedArms->SetRelativeLocation(ArmsOffset);
        ImportedArms->SetRelativeRotation(ArmsRotation);
        ImportedArms->SetRelativeScale3D(ArmsScale);
        ImportedArms->SetVisibility(true);
        if (ArmsIdleAnimation)
        {
            ImportedArms->SetAnimInstanceClass(URaidArmsAnimInstance::StaticClass());
            ImportedArms->AddTickPrerequisiteActor(this);
        }
        for (UStaticMeshComponent* Part : OriginalArmsParts) Part->SetVisibility(false);
    }
    if (MagazineModel)
    {
        ImportedMagazine->SetStaticMesh(MagazineModel);
        ImportedMagazine->SetRelativeLocation(WeaponModelOffset);
        ImportedMagazine->SetRelativeRotation(WeaponModelRotation);
        ImportedMagazine->SetRelativeScale3D(WeaponModelScale);
        ImportedMagazine->SetVisibility(true);
    }
    MuzzleLight->SetRelativeLocation(MuzzleOffset);
    MuzzleFlame->SetRelativeLocation(MuzzleOffset + FVector(2,0,0));
    MuzzleFlameCross->SetRelativeLocation(MuzzleOffset + FVector(1,0,0));
    for (UStaticMeshComponent* Part : CasingPool) Part->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    for (UStaticMeshComponent* Part : TracerPool) Part->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
}

void ARaidPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void ARaidPlayer::UpdateWeaponPresentation(float DeltaSeconds)
{
    bAiming = bAimHeld && CanAct() && !bReloading && !IsInventoryOpen() && !IsSearching();
    ADSAlpha = FMath::Lerp(ADSAlpha, bAiming ? 1.f : 0.f,
        1.f - FMath::Exp(-WeaponTuning.ADSTransitionRate * DeltaSeconds));
    Camera->SetFieldOfView(FMath::Lerp(95.f, 65.f, ADSAlpha));
    if (CanAct()) GetCharacterMovement()->MaxWalkSpeed =
        (IsTreating() ? 140.f : (bSprintHeld && !IsTooHeavyToSprint()) ? RaidPlayerSettings::SprintSpeed
        : (bAiming ? 245.f : RaidPlayerSettings::WalkSpeed)) * GetLoadSpeedScale();

    // Recover the camera by subtracting only our previous recoil offset.
    // Mouse input remains independent, so the player can still compensate manually.
    const float PreviousPitch = RecoilPitch;
    const float PreviousYaw = RecoilYaw;
    RecoilPitch *= FMath::Exp(-WeaponTuning.CameraReturnRate * DeltaSeconds);
    RecoilYaw *= FMath::Exp(-WeaponTuning.CameraReturnRate * DeltaSeconds);
    if (Controller && CanAct())
    {
        FRotator View = Controller->GetControlRotation();
        View.Pitch += RecoilPitch - PreviousPitch;
        View.Yaw += RecoilYaw - PreviousYaw;
        Controller->SetControlRotation(View);
    }
    WeaponKick *= FMath::Exp(-WeaponTuning.WeaponReturnRate * DeltaSeconds);
    WeaponTilt *= FMath::Exp(-WeaponTuning.WeaponReturnRate * DeltaSeconds);
    if (DeltaSeconds > SMALL_NUMBER)
    {
        // Mouse axes are per-frame deltas; normalize the visual sway to a 60Hz
        // reference. This does not change the player's raw mouse sensitivity.
        const float InputScale = 1.f / (60.f * DeltaSeconds);
        const float Blend = 1.f - FMath::Exp(-9.f * DeltaSeconds);
        SwayYaw = FMath::Lerp(SwayYaw, FMath::Clamp(-LookInputX * InputScale * .32f, -2.5f, 2.5f), Blend);
        SwayPitch = FMath::Lerp(SwayPitch, FMath::Clamp(LookInputY * InputScale * .3f, -2.f, 2.f), Blend);
    }

    const float Speed = GetVelocity().Size2D();
    const float MoveAlpha = FMath::Clamp(Speed / RaidPlayerSettings::WalkSpeed, 0.f, 1.4f);
    if (CanAct() && GetCharacterMovement()->IsMovingOnGround() && MoveAlpha > .18f)
    {
        FootstepCooldown -= DeltaSeconds;
        if (FootstepCooldown <= 0.f)
        {
            PlayWeaponSound(FootstepSound, bSprintHeld ? .4f : .24f);
            FootstepCooldown = bSprintHeld ? .29f : .43f;
        }
    }
    else FootstepCooldown = 0.f;
    GaitPhase += DeltaSeconds * (bSprintHeld ? 12.f : 8.f) * FMath::Max(.2f, MoveAlpha);
    const float ReducedMotion = FMath::Lerp(1.f, .14f, ADSAlpha);
    const float Breathing = FMath::Sin(GetWorld()->GetTimeSeconds() * 1.8f);
    FVector Pose = FMath::Lerp(HipWeaponLocation, ADSWeaponLocation, ADSAlpha);
    Pose.X -= WeaponKick;
    Pose.Y += FMath::Sin(GaitPhase) * .45f * MoveAlpha * ReducedMotion;
    Pose.Z += FMath::Abs(FMath::Cos(GaitPhase)) * .55f * MoveAlpha * ReducedMotion + Breathing * .08f * ReducedMotion;
    FRotator Rotation(SwayPitch * ReducedMotion + WeaponTilt,
        SwayYaw * ReducedMotion, -SwayYaw * .7f * ReducedMotion);

    ReloadProgress = bReloading ? FMath::Clamp((GetWorld()->GetTimeSeconds() - ReloadStartedAt)
        / FMath::Max(.2f, WeaponTuning.ReloadSeconds), 0.f, 1.f) : 0.f;
    if (bReloading)
    {
        VisualReloadProgress = ReloadProgress;
        // Let the last shot finish before taking the support hand off the gun.
        VisualReloadWeight = GetWorld()->GetTimeSeconds() >= ReloadStartedAt ? 1.f : 0.f;
        ReloadRecoveryElapsed = -1.f;
    }
    else if (ReloadRecoveryElapsed >= 0.f)
    {
        ReloadRecoveryElapsed += DeltaSeconds;
        // Follow the authored, IK-solved path back to a seated magazine and grip.
        // Early aborts rewind; late aborts finish the presentation without loading ammo.
        const float ReturnEnd = ReloadRecoveryStartProgress < .54f ? 0.f : 1.f;
        VisualReloadProgress = FMath::Lerp(ReloadRecoveryStartProgress, ReturnEnd,
            FMath::SmoothStep(0.f, .24f, ReloadRecoveryElapsed));
        VisualReloadWeight = 1.f;
        if (ReloadRecoveryElapsed >= .24f)
        {
            ReloadRecoveryElapsed = -1.f;
            VisualReloadWeight = 0.f;
        }
    }
    else { VisualReloadWeight = 0.f; VisualReloadProgress = 0.f; }
    const float ReloadArc = FMath::Sin(VisualReloadProgress * PI) * VisualReloadWeight;
    // Keep the receiver and support hand visible while the magazine is handled.
    Pose += FVector(6.f, -2.f, 10.f) * ReloadArc;
    Rotation.Pitch -= 4.f * ReloadArc;
    Rotation.Roll -= 10.f * ReloadArc;
    SprintPoseAlpha = FMath::FInterpTo(SprintPoseAlpha, bSprintHeld ? 1.f : 0.f, DeltaSeconds, 9.f);
    SearchPoseAlpha = FMath::FInterpTo(SearchPoseAlpha, (IsSearching() || IsTreating()) ? 1.f : 0.f, DeltaSeconds, 10.f);
    Pose.Z -= 4.f * MoveAlpha * SprintPoseAlpha + 6.f * SearchPoseAlpha;
    Rotation.Roll -= 12.f * MoveAlpha * SprintPoseAlpha;
    Rotation.Pitch -= 8.f * SearchPoseAlpha;
    DamageFlinch = FMath::FInterpTo(DamageFlinch, 0.f, DeltaSeconds, 11.f);
    Rotation.Pitch -= DamageFlinch;
    Rotation.Roll += DamageFlinch * DamageFlinchSide * .7f;
    WeaponRoot->SetRelativeLocation(Pose);
    WeaponRoot->SetRelativeRotation(Rotation);

    const float MagPhase = FMath::Clamp((VisualReloadProgress - .15f) / .68f, 0.f, 1.f);
    const float MagazineDrop = FMath::Sin(MagPhase * PI) * VisualReloadWeight;
    for (int32 Index = 0; Index < WeaponParts.Num(); ++Index)
    {
        if (WeaponParts[Index]->ComponentHasTag(TEXT("Magazine")))
            WeaponParts[Index]->SetRelativeLocation(PartPositions[Index] + FVector(2.f,0,-12.f) * MagazineDrop);
    }
    SupportHand->SetRelativeLocation(FMath::Lerp(FVector(40,-3.8f,-3.6f), FVector(22,-4,-17), ReloadArc));
    if (MagazineModel)
    {
        // Match the authored left-hand trajectory: grasp, pull, replace, seat.
        static const float Times[] = {0.f,.20f,.36f,.54f,.67f,.79f,1.f};
        static const FVector Offsets[] = {FVector::ZeroVector,FVector::ZeroVector,
            FVector(1.5,0,-6),FVector(4,0,-10),FVector(1.5,0,-5),FVector::ZeroVector,FVector::ZeroVector};
        FVector Offset = FVector::ZeroVector;
        float Rock = 0.f;
        if (VisualReloadWeight > 0.f)
        {
            for (int32 Key = 0; Key < 6; ++Key)
                if (VisualReloadProgress >= Times[Key] && VisualReloadProgress <= Times[Key+1])
                {
                    const float Alpha = FMath::SmoothStep(0.f,1.f,(VisualReloadProgress-Times[Key])/(Times[Key+1]-Times[Key]));
                    Offset = FMath::Lerp(Offsets[Key],Offsets[Key+1],Alpha) * VisualReloadWeight;
                    break;
                }
            Rock = -Offset.Z * .45f;
        }
        const FQuat RockRotation = FRotator(Rock,0,0).Quaternion();
        ImportedMagazine->SetRelativeLocation(WeaponModelOffset + Offset + MagazinePivot - RockRotation.RotateVector(MagazinePivot));
        ImportedMagazine->SetRelativeRotation(RockRotation * WeaponModelRotation.Quaternion());
    }

    for (int32 Index = 0; Index < CasingPool.Num(); ++Index)
    {
        if (CasingAges[Index] >= 0.f)
        {
            CasingAges[Index] += DeltaSeconds;
            CasingVelocities[Index].Z -= 360.f * DeltaSeconds;
            CasingPositions[Index] += CasingVelocities[Index] * DeltaSeconds;
            CasingPool[Index]->SetWorldLocation(CasingPositions[Index]);
            CasingPool[Index]->AddWorldRotation(FRotator(420.f, 260.f, 190.f) * DeltaSeconds);
            if (CasingAges[Index] > .65f)
            {
                CasingPool[Index]->SetVisibility(false);
                CasingAges[Index] = -1.f;
            }
        }
        if (TracerAges[Index] >= 0.f)
        {
            TracerAges[Index] += DeltaSeconds;
            if (TracerAges[Index] > .06f)
            {
                TracerPool[Index]->SetVisibility(false);
                TracerAges[Index] = -1.f;
            }
        }
    }
}

float ARaidPlayer::GetShotPresentationAge() const
{
    return GetWorld() ? GetWorld()->GetTimeSeconds() - LastShotTime : 100.f;
}

void ARaidPlayer::PlayWeaponSound(USoundBase* Sound, float Volume) const
{
    if (Sound) UGameplayStatics::PlaySound2D(this, Sound, Volume, 1.f, 0.f, nullptr, this, false);
}

void ARaidPlayer::SpawnShotPresentation(const FVector& EndPoint, bool bHit)
{
    ++ShotPresentationCount;
    MuzzleLight->SetVisibility(true);
    MuzzleFlame->SetVisibility(true);
    MuzzleFlameCross->SetVisibility(true);
    MuzzleFlameCross->SetRelativeRotation(FRotator(0,0,FMath::RandRange(0.f,90.f)));
    GetWorldTimerManager().SetTimer(MuzzleTimer, this, &ARaidPlayer::HideMuzzleFlash, .045f, false);

    const FVector Muzzle = MuzzleLight->GetComponentLocation();
    const FVector Segment = EndPoint - Muzzle;
    UStaticMeshComponent* Tracer = TracerPool[NextTracer];
    Tracer->SetWorldLocation((Muzzle + EndPoint) * .5f);
    Tracer->SetWorldRotation(Segment.Rotation());
    Tracer->SetWorldScale3D(FVector(Segment.Size()/100.f,.0014f,.0014f));
    Tracer->SetVisibility(true);
    TracerAges[NextTracer] = 0.f;
    NextTracer = (NextTracer + 1) % TracerPool.Num();

    CasingPositions[NextCasing] = WeaponRoot->GetComponentTransform().TransformPosition(FVector(20,-3,1));
    CasingVelocities[NextCasing] = Camera->GetRightVector() * 125.f + Camera->GetUpVector() * 70.f
        + Camera->GetForwardVector() * 15.f;
    CasingPool[NextCasing]->SetWorldLocation(CasingPositions[NextCasing]);
    CasingPool[NextCasing]->SetWorldScale3D(FVector(.0065f,.0065f,.019f));
    CasingPool[NextCasing]->SetVisibility(true);
    CasingAges[NextCasing] = 0.f;
    NextCasing = (NextCasing + 1) % CasingPool.Num();
    if (bHit) DrawDebugPoint(GetWorld(), EndPoint, 5.f, FColor(255,190,90), false, .12f);
    if (FireSound) ++ShotSoundRequestCount; // A trigger count, never an audio-listening claim.
    PlayWeaponSound(FireSound, .7f);
}

void ARaidPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ARaidPlayer::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ARaidPlayer::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Turn"), this, &ARaidPlayer::Turn);
    PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &ARaidPlayer::LookUp);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &ARaidPlayer::StartJump);
    PlayerInputComponent->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
    PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &ARaidPlayer::StartFire);
    PlayerInputComponent->BindAction(TEXT("Fire"), IE_Released, this, &ARaidPlayer::StopFire);
    PlayerInputComponent->BindAction(TEXT("Aim"), IE_Pressed, this, &ARaidPlayer::StartAim);
    PlayerInputComponent->BindAction(TEXT("Aim"), IE_Released, this, &ARaidPlayer::StopAim);
    PlayerInputComponent->BindAction(TEXT("Reload"), IE_Pressed, this, &ARaidPlayer::Reload);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &ARaidPlayer::StartSprint);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &ARaidPlayer::StopSprint);
}

bool ARaidPlayer::CanAct() const
{
    const ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    return !bEndingPlay && IsAlive() && Controller && Controller->GetPawn() == this
        && Mode && Mode->IsRaidActive() && !Mode->IsPauseMenuOpen();
}

bool ARaidPlayer::CanUseWorldInput() const
{
    // Damage and inventory changes still use CanAct while the world keeps running.
    return CanAct() && !IsInventoryOpen();
}

void ARaidPlayer::MoveForward(float Value)
{
    if (CanUseWorldInput() && Controller && !FMath::IsNearlyZero(Value))
    {
        const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
    }
}

void ARaidPlayer::MoveRight(float Value)
{
    if (CanUseWorldInput() && Controller && !FMath::IsNearlyZero(Value))
    {
        const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
    }
}

void ARaidPlayer::Turn(float Value)
{
    LookInputX = CanUseWorldInput() ? Value : 0.f;
    if (CanUseWorldInput()) AddControllerYawInput(Value * FMath::Lerp(1.f,.58f,ADSAlpha));
}

void ARaidPlayer::LookUp(float Value)
{
    LookInputY = CanUseWorldInput() ? Value : 0.f;
    if (CanUseWorldInput()) AddControllerPitchInput(Value * FMath::Lerp(1.f,.58f,ADSAlpha));
}

void ARaidPlayer::StartJump()
{
    if (CanUseWorldInput()) { CancelTreatment(); Jump(); }
}

void ARaidPlayer::StartSprint()
{
    if (CanUseWorldInput())
    {
        CancelTreatment();
        if (IsTooHeavyToSprint()) { Notify(TEXT("负重达到 10 kg，放回部分物资后才能冲刺。")); return; }
        StopFire();
        StopAim();
        CancelSearch();
        bSprintHeld = true;
        GetCharacterMovement()->MaxWalkSpeed = RaidPlayerSettings::SprintSpeed;
    }
}

void ARaidPlayer::StopSprint()
{
    GetCharacterMovement()->MaxWalkSpeed = RaidPlayerSettings::WalkSpeed;
    bSprintHeld = false;
}

void ARaidPlayer::StartAim()
{
    if (!CanUseWorldInput()) return;
    CancelTreatment();
    if (ARaidPlayerController* PC=Cast<ARaidPlayerController>(Controller)) PC->ReturnToWorldView();
    bAimHeld = true;
    bAiming = !bReloading;
    StopSprint();
    CancelSearch();
}

void ARaidPlayer::StopAim()
{
    bAimHeld = false;
    bAiming = false;
}

void ARaidPlayer::StartFire()
{
    if (!CanUseWorldInput()) return;
    CancelTreatment();
    if (ARaidPlayerController* PC=Cast<ARaidPlayerController>(Controller)) PC->ReturnToWorldView();
    StopSprint();
    CancelSearch();
    bFireHeld = true;
    FireWhileHeld();
}

void ARaidPlayer::StopFire()
{
    bFireHeld = false;
    GetWorldTimerManager().ClearTimer(FireTimer);
}

void ARaidPlayer::ScheduleHeldFire()
{
    GetWorldTimerManager().ClearTimer(FireTimer);
    if (!bFireHeld || bReloading || !CanUseWorldInput()) return;
    const float Recovery = ReloadRecoveryElapsed >= 0.f ? .24f - ReloadRecoveryElapsed : 0.f;
    const float Delay = static_cast<float>(FMath::Max3<double>(.001, NextAllowedFireTime - GetWorld()->GetTimeSeconds(), Recovery));
    GetWorldTimerManager().SetTimer(FireTimer, this, &ARaidPlayer::FireWhileHeld, Delay, false);
}

void ARaidPlayer::FireWhileHeld()
{
    if (!bFireHeld) return;
    Fire();
    ScheduleHeldFire();
}

void ARaidPlayer::Fire()
{
    if (!CanUseWorldInput())
    {
        StopFire();
        return;
    }
    CancelTreatment();
    // A held trigger can wait for a reload; releasing it clears that intent.
    if (bReloading || ReloadRecoveryElapsed >= 0.f) return;
    if (GetWorld()->GetTimeSeconds() + KINDA_SMALL_NUMBER < NextAllowedFireTime) return;
    if (MagazineAmmo <= 0)
    {
        if (ReserveAmmo > 0) BeginReload(true);
        else
        {
            StopFire();
            NextAllowedFireTime = GetWorld()->GetTimeSeconds() + .18f;
            PlayWeaponSound(EmptySound, .4f);
            Notify(TEXT("弹药已耗尽；搜集弹药盒后在背包中使用。"));
        }
        return;
    }

    --MagazineAmmo;
    ++SuccessfulShots;
    LastShotTime = GetWorld()->GetTimeSeconds();
    CancelSearch();
    const float Now = GetWorld()->GetTimeSeconds();
    const float Interval = FMath::Max(.05f, WeaponTuning.ShotInterval);
    // Preserve the fractional deadline across ordinary frame quantization. After
    // a long stall start a new beat; never burst several catch-up shots in a frame.
    NextAllowedFireTime = bFireHeld && NextAllowedFireTime > 0.f && Now-NextAllowedFireTime < Interval
        ? NextAllowedFireTime + Interval : Now + Interval;
    const FVector Start = Camera->GetComponentLocation();
    const FVector Direction = Controller ? Controller->GetControlRotation().Vector() : Camera->GetForwardVector();
    const FVector End = Start + Direction * RaidPlayerSettings::WeaponRange;
    FHitResult Hit;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RaidWeapon), false, this);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, TraceParams);

    if (bHit && IsValid(Hit.GetActor()))
    {
        const float AppliedDamage = UGameplayStatics::ApplyPointDamage(Hit.GetActor(),
            RaidPlayerSettings::WeaponDamage, Direction, Hit, GetController(), this, UDamageType::StaticClass());
        // Plain world actors report the requested damage even without health.
        // Confirm a hit only on a pawn, so shooting a wall does not show an enemy hit.
        if (AppliedDamage > 0.f && Hit.GetActor()->IsA<APawn>())
        {
            LastHitTime = GetWorld()->GetTimeSeconds();
            PlayWeaponSound(HitSound, .35f);
            if (const ARaidEnemy* Enemy = Cast<ARaidEnemy>(Hit.GetActor()))
                if (Enemy->Health <= 0.f) LastKillTime = LastHitTime;
        }
    }

    const float PitchKick = FMath::Lerp(WeaponTuning.HipPitchKick,WeaponTuning.ADSPitchKick,ADSAlpha);
    const float YawKick = FMath::RandRange(-1.f,1.f) * FMath::Lerp(WeaponTuning.HipYawKick,WeaponTuning.ADSYawKick,ADSAlpha);
    RecoilYaw += YawKick;
    WeaponKick = FMath::Min(5.f, WeaponKick + FMath::Lerp(WeaponTuning.HipKickCm,WeaponTuning.ADSKickCm,ADSAlpha));
    WeaponTilt = FMath::Min(8.f, WeaponTilt + FMath::Lerp(WeaponTuning.HipTiltDegrees,WeaponTuning.ADSTiltDegrees,ADSAlpha));
    if (Controller)
    {
        FRotator View = Controller->GetControlRotation();
        const float OldPitch = FRotator::NormalizeAxis(View.Pitch);
        View.Pitch = FMath::Clamp(OldPitch + PitchKick, -85.f, 85.f);
        RecoilPitch += View.Pitch - OldPitch;
        View.Yaw += YawKick;
        Controller->SetControlRotation(View);
    }
    // Gameplay, pose impulse, muzzle, tracer and sound share one accepted shot.
    UpdateWeaponPresentation(0.f);
    SpawnShotPresentation(bHit ? Hit.ImpactPoint : End, bHit);
    if (MagazineAmmo == 0 && ReserveAmmo > 0) BeginReload(true);
}

void ARaidPlayer::HideMuzzleFlash()
{
    MuzzleLight->SetVisibility(false);
    MuzzleFlame->SetVisibility(false);
    MuzzleFlameCross->SetVisibility(false);
}

void ARaidPlayer::Reload()
{
    BeginReload(false);
}

void ARaidPlayer::BeginReload(bool bAutomatic)
{
    if (!CanUseWorldInput() || bReloading || MagazineAmmo >= MaxMagazineAmmo) return;
    CancelTreatment();
    if (ReserveAmmo <= 0)
    {
        Notify(TEXT("没有备用弹药。"));
        return;
    }
    // Clear the scheduled shot, retain only physical held-trigger intent.
    GetWorldTimerManager().ClearTimer(FireTimer);
    CancelSearch();
    StopSprint();
    bReloading = true;
    bAutomaticReload = bAutomatic;
    bAiming = false;
    const float LeadIn = FMath::Max(0.f, WeaponTuning.ShotToReloadSeconds - GetShotPresentationAge());
    ReloadStartedAt = GetWorld()->GetTimeSeconds() + LeadIn;
    ReloadProgress = 0.f;
    if (LeadIn > .001f)
        GetWorldTimerManager().SetTimer(ReloadSoundTimer, this, &ARaidPlayer::PlayReloadAudio, LeadIn, false);
    else PlayReloadAudio();
    GetWorldTimerManager().SetTimer(ReloadTimer, this, &ARaidPlayer::FinishReload,
        FMath::Max(.2f, WeaponTuning.ReloadSeconds) + LeadIn, false);
}

void ARaidPlayer::FinishReload()
{
    if (!bReloading) return; // Cancelled or duplicate callbacks cannot credit ammo.
    if (!CanUseWorldInput()) { CancelReload(); return; }
    StopReloadAudio();
    bReloading = false;
    bAutomaticReload = false;
    ReloadProgress = 0.f;
    const int32 Needed = FMath::Max(0, MaxMagazineAmmo - MagazineAmmo);
    const int32 Loaded = FMath::Min(Needed, ReserveAmmo);
    MagazineAmmo += Loaded;
    ReserveAmmo -= Loaded;
    ReloadStartedAt = -1.f;
    ReloadRecoveryElapsed = -1.f;
    VisualReloadProgress = 1.f;
    VisualReloadWeight = 0.f;
    ScheduleHeldFire();
}

void ARaidPlayer::CancelReload()
{
    StopReloadAudio();
    if (bReloading)
    {
        VisualReloadProgress = FMath::Clamp((GetWorld()->GetTimeSeconds() - ReloadStartedAt)
            / FMath::Max(.2f, WeaponTuning.ReloadSeconds), 0.f, 1.f);
        VisualReloadWeight = 1.f;
        ReloadRecoveryElapsed = 0.f;
        ReloadRecoveryStartProgress = VisualReloadProgress;
    }
    GetWorldTimerManager().ClearTimer(ReloadTimer);
    StopFire();
    bReloading = false;
    bAutomaticReload = false;
    ReloadProgress = 0.f;
    ReloadStartedAt = -1.f;
}

void ARaidPlayer::PlayReloadAudio()
{
    if (!bReloading || !ReloadSound || !CanUseWorldInput()) return;
    // The retained 2.1s Foley clip follows the configured action duration. Keep
    // ownership so cancellation cannot leave its magazine/bolt sounds running.
    const float Rate = FMath::Clamp(ReloadSound->GetDuration() / FMath::Max(.2f, WeaponTuning.ReloadSeconds), .5f, 2.f);
    ActiveReloadAudio = UGameplayStatics::SpawnSound2D(this, ReloadSound, .6f, Rate);
    if (IsValid(ActiveReloadAudio)) ActiveReloadAudio->SetUISound(false); // Follow engine pause.
}

void ARaidPlayer::StopReloadAudio()
{
    GetWorldTimerManager().ClearTimer(ReloadSoundTimer);
    if (IsValid(ActiveReloadAudio)) ActiveReloadAudio->Stop();
    ActiveReloadAudio = nullptr;
}

ARaidLoot* ARaidPlayer::GetFocusedLoot() const { return InteractionComponent->GetFocusedLoot(); }
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

void ARaidPlayer::PreparePauseInput()
{
    // The world pause preserves reload/extraction timers. Held combat and move
    // inputs are released so resuming cannot produce an unintended shot.
    StopFire();
    StopAim();
    StopSprint();
    StopJumping();
    InteractionComponent->ReleaseInteract();
    ConsumeMovementInputVector();
    // Engine pause already freezes physics. Keep velocity so a mid-air pause
    // resumes the same jump, rather than cancelling its upward momentum.
    LookInputX = LookInputY = 0.f;
}

void ARaidPlayer::PrepareInventoryInput()
{
    CancelTreatment();
    StopFire();
    StopAim();
    StopSprint();
    StopJumping();
    CancelReload();
    CancelSearch();
    ConsumeMovementInputVector();
    GetCharacterMovement()->StopMovementImmediately();
    if (Controller)
    {
        FRotator View = Controller->GetControlRotation();
        View.Pitch -= RecoilPitch;
        View.Yaw -= RecoilYaw;
        Controller->SetControlRotation(View);
    }
    RecoilPitch = 0.f;
    RecoilYaw = 0.f;
    LookInputX = 0.f;
    LookInputY = 0.f;
}

void ARaidPlayer::StopInteractionForUnpossess()
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

float ARaidPlayer::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!CanAct() || DamageAmount <= 0.f) return 0.f;
    CancelTreatment();
    const float AppliedDamage = FMath::Min(Health, DamageAmount);
    Super::TakeDamage(AppliedDamage, DamageEvent, EventInstigator, DamageCauser);
    Health = FMath::Max(0.f, Health - AppliedDamage);
    LastDamageTime = GetWorld()->GetTimeSeconds();
    // Searching never pauses combat. A hit closes the panel before any later
    // search tick or mouse callback can reveal or transfer another item.
    if (IsAlive()) CloseLoot();
    const AActor* Source = DamageCauser ? DamageCauser : (EventInstigator ? EventInstigator->GetPawn() : nullptr);
    bHasDamageSource = IsValid(Source) && Source != this;
    if (bHasDamageSource)
    {
        DamageSourceYaw = (Source->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
        DamageFlinchSide = FMath::FindDeltaAngleDegrees(Camera->GetComponentRotation().Yaw, DamageSourceYaw) >= 0.f ? 1.f : -1.f;
    }
    DamageFlinch = FMath::Clamp(AppliedDamage * .32f, 1.f, 6.f);
    if (!IsAlive())
    {
        if (Cast<ARaidEnemy>(Source))
        {
            const TCHAR* Directions[] = { TEXT("东"), TEXT("东北"), TEXT("北"), TEXT("西北"),
                TEXT("西"), TEXT("西南"), TEXT("南"), TEXT("东南") };
            const int32 Direction = FMath::RoundToInt(FRotator::ClampAxis(DamageSourceYaw) / 45.f) % 8;
            const int32 Metres = FMath::Max(1, FMath::RoundToInt(FVector::Dist(GetActorLocation(), Source->GetActorLocation()) / 100.f));
            DeathCause = FString::Printf(TEXT("致命伤害：巡逻队射击 · %s侧 · %d 米"), Directions[Direction], Metres);
        }
        else DeathCause = TEXT("致命伤害：其他来源");
        UE_LOG(LogTemp, Display, TEXT("R8_DEATH_CAUSE: %s source=%s"), *DeathCause, *GetNameSafe(Source));
        StopFire();
        CancelReload();
        StopAim();
        CancelSearch();
        GetCharacterMovement()->DisableMovement();
        if (ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this))) Mode->EndRaid(false);
        else CloseLoot();
    }
    return AppliedDamage;
}

void ARaidPlayer::Notify(const FString& Text) const
{
    if (ARaidGameMode* Mode = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this))) Mode->PushMessage(Text);
}


bool ARaidPlayer::IsTreating() const { return TreatmentComponent && TreatmentComponent->IsTreating(); }
void ARaidPlayer::CancelTreatment() { if (TreatmentComponent) TreatmentComponent->CancelTreatment(); }
bool ARaidPlayer::IsTooHeavyToSprint() const { return InventoryComponent->GetWeightGrams() >= 10000; }
float ARaidPlayer::GetLoadSpeedScale() const
{
    const float Kg = static_cast<float>(InventoryComponent->GetWeightGrams()) / 1000.f;
    return FMath::Lerp(1.f, .8f, FMath::Clamp((Kg - 6.f) / 6.f, 0.f, 1.f));
}
