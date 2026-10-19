#include "RaidEnemy.h"

#include "RaidGameMode.h"
#include "RaidLoot.h"
#include "RaidPlayer.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "BrainComponent.h"
#include "CollisionShape.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NavigationSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace RaidGuardColors
{
    const FLinearColor Uniform(.055f, .067f, .038f);
    const FLinearColor Armor(.033f, .041f, .027f);
    const FLinearColor Helmet(.065f, .076f, .045f);
    const FLinearColor Dark(.022f, .026f, .024f);
}

ARaidEnemy::ARaidEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    bUseControllerRotationYaw = false;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetCharacterMovement()->MaxWalkSpeed = 170.f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 300.f, 0.f);

    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GuardVisuals"));
    VisualRoot->SetupAttachment(GetRootComponent());
    TorsoRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TorsoEquipment"));
    TorsoRoot->SetupAttachment(VisualRoot);
    TorsoRoot->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
    HeadRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HeadEquipment"));
    HeadRoot->SetupAttachment(VisualRoot);
    HeadRoot->SetRelativeLocation(FVector(0.f, 0.f, 59.f));
    WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponPivot"));
    WeaponRoot->SetupAttachment(VisualRoot);
    WeaponRoot->SetRelativeLocation(FVector(32.f, 13.f, 24.f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    auto Part = [&](FName Name, UStaticMesh* Shape, USceneComponent* Parent, FVector Location, FVector Scale)
    {
        UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Mesh->SetupAttachment(Parent);
        Mesh->SetStaticMesh(Shape);
        if (BasicMaterial.Succeeded()) Mesh->SetMaterial(0, BasicMaterial.Object);
        Mesh->SetRelativeLocation(Location);
        Mesh->SetRelativeScale3D(Scale);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCanEverAffectNavigation(false);
        return Mesh;
    };

    // 装备是原创的多部件模型，动画身体来自本机已有的 Epic UE 模板。
    BodyMesh = Part(TEXT("Body"), Sphere.Object, TorsoRoot, FVector(4.f, 0.f, 0.f), FVector(.29f, .41f, .45f));
    ArmorParts.Add(BodyMesh);
    ArmorParts.Add(Part(TEXT("FrontArmorPlate"), Cube.Object, TorsoRoot, FVector(22.f, 0.f, 4.f), FVector(.055f, .33f, .33f)));
    ArmorParts.Add(Part(TEXT("Backpack"), Cube.Object, TorsoRoot, FVector(-20.f, 0.f, 0.f), FVector(.20f, .34f, .46f)));
    ArmorParts.Add(Part(TEXT("Belt"), Cube.Object, TorsoRoot, FVector(0.f, 0.f, -28.f), FVector(.34f, .43f, .075f)));
    for (int32 Index = 0; Index < 3; ++Index)
    {
        ArmorParts.Add(Part(*FString::Printf(TEXT("MagazinePouch%d"), Index), Cube.Object, TorsoRoot,
            FVector(25.f, -11.f + 11.f * Index, -9.f), FVector(.08f, .09f, .15f)));
    }
    DarkParts.Add(Part(TEXT("Radio"), Cube.Object, TorsoRoot, FVector(23.f, -16.f, 17.f), FVector(.065f, .08f, .14f)));
    DarkParts.Add(Part(TEXT("RadioAntenna"), Cylinder.Object, TorsoRoot, FVector(23.f, -16.f, 28.f), FVector(.018f, .018f, .20f)));
    HeadMesh = Part(TEXT("Head"), Sphere.Object, HeadRoot, FVector::ZeroVector, FVector(.235f, .225f, .28f));
    HelmetParts.Add(Part(TEXT("CombatHelmet"), Sphere.Object, HeadRoot, FVector(0.f, 0.f, 8.f), FVector(.255f, .255f, .17f)));
    HelmetParts.Add(Part(TEXT("HelmetRim"), Sphere.Object, HeadRoot, FVector(0.f, 0.f, 5.f), FVector(.265f, .27f, .045f)));
    DarkParts.Add(Part(TEXT("HelmetMount"), Cube.Object, HeadRoot, FVector(14.f, 0.f, 14.f), FVector(.055f, .09f, .07f)));
    DarkParts.Add(Part(TEXT("GoggleFrame"), Cube.Object, HeadRoot, FVector(10.f, 0.f, 4.f), FVector(.06f, .235f, .085f)));
    DarkParts.Add(Part(TEXT("LeftLens"), Sphere.Object, HeadRoot, FVector(13.f, -6.7f, 4.f), FVector(.055f, .09f, .06f)));
    DarkParts.Add(Part(TEXT("RightLens"), Sphere.Object, HeadRoot, FVector(13.f, 6.7f, 4.f), FVector(.055f, .09f, .06f)));
    DarkParts.Add(Part(TEXT("LowerFaceScarf"), Sphere.Object, HeadRoot, FVector(4.f, 0.f, -7.f), FVector(.20f, .23f, .12f)));

    UniformParts.Add(Part(TEXT("UniformTorso"), Sphere.Object, VisualRoot, FVector(0.f, 0.f, 18.f), FVector(.39f, .43f, .60f)));
    UniformParts.Add(Part(TEXT("LeftShoulder"), Sphere.Object, VisualRoot, FVector(0.f, -25.f, 32.f), FVector(.20f)));
    UniformParts.Add(Part(TEXT("RightShoulder"), Sphere.Object, VisualRoot, FVector(0.f, 25.f, 32.f), FVector(.20f)));
    LeftUpperArm = Part(TEXT("LeftUpperArm"), Cylinder.Object, VisualRoot, FVector::ZeroVector, FVector(.16f));
    RightUpperArm = Part(TEXT("RightUpperArm"), Cylinder.Object, VisualRoot, FVector::ZeroVector, FVector(.16f));
    LeftForearm = Part(TEXT("LeftForearm"), Cylinder.Object, VisualRoot, FVector::ZeroVector, FVector(.135f));
    RightForearm = Part(TEXT("RightForearm"), Cylinder.Object, VisualRoot, FVector::ZeroVector, FVector(.135f));
    LeftThigh = Part(TEXT("LeftThigh"), Cylinder.Object, VisualRoot, FVector::ZeroVector, FVector(.18f));
    RightThigh = Part(TEXT("RightThigh"), Cylinder.Object, VisualRoot, FVector::ZeroVector, FVector(.18f));
    LeftCalf = Part(TEXT("LeftCalf"), Cylinder.Object, VisualRoot, FVector::ZeroVector, FVector(.145f));
    RightCalf = Part(TEXT("RightCalf"), Cylinder.Object, VisualRoot, FVector::ZeroVector, FVector(.145f));
    for (UStaticMeshComponent* Limb : {LeftUpperArm.Get(), RightUpperArm.Get(), LeftForearm.Get(), RightForearm.Get(),
        LeftThigh.Get(), RightThigh.Get(), LeftCalf.Get(), RightCalf.Get()}) UniformParts.Add(Limb);
    FallbackParts = UniformParts;
    LeftHand = Part(TEXT("LeftGlove"), Sphere.Object, VisualRoot, FVector::ZeroVector, FVector(.11f));
    RightHand = Part(TEXT("RightGlove"), Sphere.Object, VisualRoot, FVector::ZeroVector, FVector(.11f));
    LeftBoot = Part(TEXT("LeftBoot"), Cube.Object, VisualRoot, FVector::ZeroVector, FVector(.31f, .205f, .14f));
    RightBoot = Part(TEXT("RightBoot"), Cube.Object, VisualRoot, FVector::ZeroVector, FVector(.31f, .205f, .14f));
    LeftKnee = Part(TEXT("LeftKneepad"), Cube.Object, VisualRoot, FVector::ZeroVector, FVector(.07f, .17f, .16f));
    RightKnee = Part(TEXT("RightKneepad"), Cube.Object, VisualRoot, FVector::ZeroVector, FVector(.07f, .17f, .16f));
    for (UStaticMeshComponent* Equipment : {LeftHand.Get(), RightHand.Get(), LeftBoot.Get(), RightBoot.Get(), LeftKnee.Get(), RightKnee.Get()})
        DarkParts.Add(Equipment);
    FallbackParts.Add(LeftKnee);
    FallbackParts.Add(RightKnee);

    GunMesh = Part(TEXT("Gun"), Cube.Object, WeaponRoot, FVector(15.f, 0.f, 5.f), FVector(.32f, .065f, .11f));
    DarkParts.Add(GunMesh);
    DarkParts.Add(Part(TEXT("RifleStock"), Cube.Object, WeaponRoot, FVector(-16.f, 0.f, 3.f), FVector(.24f, .075f, .15f)));
    DarkParts.Add(Part(TEXT("RifleGrip"), Cube.Object, WeaponRoot, FVector(2.f, 0.f, -5.f), FVector(.07f, .055f, .17f)));
    DarkParts.Add(Part(TEXT("RifleMagazine"), Cube.Object, WeaponRoot, FVector(17.f, 0.f, -5.f), FVector(.085f, .06f, .20f)));
    DarkParts.Add(Part(TEXT("RifleHandguard"), Cylinder.Object, WeaponRoot, FVector(38.f, 0.f, 5.f), FVector(.075f, .075f, .29f)));
    DarkParts.Last()->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
    DarkParts.Add(Part(TEXT("RifleBarrel"), Cylinder.Object, WeaponRoot, FVector(57.f, 0.f, 5.f), FVector(.026f, .026f, .23f)));
    DarkParts.Last()->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
    DarkParts.Add(Part(TEXT("RifleSight"), Cube.Object, WeaponRoot, FVector(13.f, 0.f, 13.f), FVector(.09f, .045f, .06f)));
    Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
    Muzzle->SetupAttachment(WeaponRoot);
    Muzzle->SetRelativeLocation(FVector(70.f, 0.f, 5.f));
    MuzzleFlash = Part(TEXT("MuzzleFlash"), Sphere.Object, Muzzle, FVector(7.f, 0.f, 0.f), FVector(.20f, .085f, .085f));
    MuzzleFlash->SetVisibility(false);
    MuzzleFlash->SetCastShadow(false);
    MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
    MuzzleLight->SetupAttachment(Muzzle);
    MuzzleLight->SetIntensity(450.f);
    MuzzleLight->SetAttenuationRadius(110.f);
    MuzzleLight->SetLightColor(FLinearColor(1.f, .48f, .11f));
    MuzzleLight->SetCastShadows(false);
    MuzzleLight->SetVisibility(false);

    WeaponAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EnemyWeaponAudio"));
    WeaponAudio->SetupAttachment(Muzzle);
    VoiceAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EnemyVoiceAudio"));
    VoiceAudio->SetupAttachment(HeadRoot);
    FootstepAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("EnemyFootstepAudio"));
    FootstepAudio->SetupAttachment(VisualRoot);
    for (UAudioComponent* Audio : {WeaponAudio.Get(), VoiceAudio.Get(), FootstepAudio.Get()}) Audio->bAutoActivate = false;

    // 可缺省：资源未复制时仍显示原创人形，不产生缺失资源错误。
    static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> Manny(
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> Idle(
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS.MF_Rifle_Idle_ADS"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<UAnimSequence> Walk(
        TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd.MF_Rifle_Walk_Fwd"), LOAD_Quiet | LOAD_NoWarn);
    GetMesh()->SetupAttachment(VisualRoot);
    GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    GetMesh()->SetRelativeScale3D(FVector(.96f));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCanEverAffectNavigation(false);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    GetMesh()->SetSkeletalMesh(Manny.Get());
    static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Cloth(
        TEXT("/Game/Materials/M_GuardCloth.M_GuardCloth"), LOAD_Quiet | LOAD_NoWarn);
    if (Cloth.Get())
        for (int32 Slot = 0; Slot < GetMesh()->GetNumMaterials(); ++Slot) GetMesh()->SetMaterial(Slot, Cloth.Get());
    RifleIdleAnimation = Idle.Get();
    RifleWalkAnimation = Walk.Get();
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> Shot(
        TEXT("/Game/Audio/S_RifleShot.S_RifleShot"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> Alert(
        TEXT("/Game/Audio/S_Alert.S_Alert"), LOAD_Quiet | LOAD_NoWarn);
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> Step(
        TEXT("/Game/Audio/S_Footstep.S_Footstep"), LOAD_Quiet | LOAD_NoWarn);
    ShotSound = Shot.Get();
    AlertSound = Alert.Get();
    FootstepSound = Step.Get();
    static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> ImportedRifle(
        TEXT("/Game/Weapons/SM_ClassicRifle.SM_ClassicRifle"), LOAD_Quiet | LOAD_NoWarn);
    if (ImportedRifle.Get())
    {
        GunMesh->SetStaticMesh(ImportedRifle.Get());
        GunMesh->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
        GunMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
        GunMesh->SetRelativeScale3D(FVector(.82f));
        GunMesh->EmptyOverrideMaterials();
        Muzzle->SetRelativeLocation(FVector(57.4f, 0.f, 7.85f));
    }
}

void ARaidEnemy::BeginPlay()
{
    Super::BeginPlay();
    Health = FMath::Clamp(Health, 1.f, FMath::Max(1.f, MaxHealth));
    SideStepSign = GetUniqueID() % 2 ? 1.f : -1.f;
    if (PatrolPoints.IsEmpty())
    {
        const FVector Origin = GetActorLocation();
        PatrolPoints = {Origin, Origin + FVector(350.f, 0.f, 0.f)};
    }
    // Cache the persistent parent before assigning any dynamic instances.
    // A dynamic instance cannot itself be the parent of another instance.
    UMaterialInterface* BaseMaterial = GetMesh()->GetSkeletalMeshAsset()
        ? GetMesh()->GetMaterial(0) : BodyMesh->GetMaterial(0);
    UniformMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
    ArmorMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
    UniformMaterial->SetVectorParameterValue(TEXT("Color"), RaidGuardColors::Uniform);
    ArmorMaterial->SetVectorParameterValue(TEXT("Color"), RaidGuardColors::Armor);
    auto ApplyColor = [&](const TArray<TObjectPtr<UStaticMeshComponent>>& Parts, FLinearColor Color)
    {
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        for (UStaticMeshComponent* Part : Parts)
            if (Part != GunMesh || !GunMesh->GetStaticMesh()
                || GunMesh->GetStaticMesh()->GetName() != TEXT("SM_ClassicRifle"))
                Part->SetMaterial(0, Material);
    };
    for (UStaticMeshComponent* Part : UniformParts) Part->SetMaterial(0, UniformMaterial);
    for (UStaticMeshComponent* Part : ArmorParts) Part->SetMaterial(0, ArmorMaterial);
    ApplyColor(HelmetParts, RaidGuardColors::Helmet);
    ApplyColor(DarkParts, RaidGuardColors::Dark);
    if (UMaterialInstanceDynamic* Skin = HeadMesh->CreateDynamicMaterialInstance(0))
        Skin->SetVectorParameterValue(TEXT("Color"), FLinearColor(.45f, .30f, .20f));
    if (UMaterialInstanceDynamic* Flash = MuzzleFlash->CreateDynamicMaterialInstance(0))
        Flash->SetVectorParameterValue(TEXT("Color"), FLinearColor(5.f, 2.1f, .15f));

    bTemplateBody = GetMesh()->GetSkeletalMeshAsset() != nullptr && RifleIdleAnimation && RifleWalkAnimation;
    GetMesh()->SetVisibility(bTemplateBody);
    for (UStaticMeshComponent* Part : FallbackParts) Part->SetVisibility(!bTemplateBody);
    if (bTemplateBody)
    {
        LeftHand->SetVisibility(false);
        RightHand->SetVisibility(false);
        LeftBoot->SetVisibility(false);
        RightBoot->SetVisibility(false);
        // 隐藏机器人头部，用头盔、护目镜和面罩形成军事人员轮廓。
        GetMesh()->HideBoneByName(TEXT("head"), EPhysBodyOp::PBO_None);
        for (int32 Slot = 0; Slot < GetMesh()->GetNumMaterials(); ++Slot) GetMesh()->SetMaterial(Slot, UniformMaterial);
        GetMesh()->PlayAnimation(RifleIdleAnimation, true);
        PlayingAnimation = RifleIdleAnimation;
    }
    if (GunMesh->GetStaticMesh() && GunMesh->GetStaticMesh()->GetName() == TEXT("SM_ClassicRifle"))
    {
        TArray<UStaticMeshComponent*> StaticParts;
        GetComponents(StaticParts);
        for (UStaticMeshComponent* Part : StaticParts)
            if (Part->GetName().StartsWith(TEXT("Rifle"))) Part->SetVisibility(false);
    }
    FSoundAttenuationSettings Attenuation;
    Attenuation.bAttenuate = true;
    Attenuation.bSpatialize = true;
    Attenuation.AttenuationShapeExtents = FVector(90.f, 0.f, 0.f);
    Attenuation.FalloffDistance = 1800.f;
    for (UAudioComponent* Audio : {WeaponAudio.Get(), VoiceAudio.Get(), FootstepAudio.Get()}) Audio->AdjustAttenuation(Attenuation);
    UpdatePresentation(0.f);
}

void ARaidEnemy::PlaySpatialSound(UAudioComponent* Audio, USoundBase* Sound, float Volume, float Pitch)
{
    if (!Audio || !Sound) return;
    Audio->SetSound(Sound);
    Audio->SetVolumeMultiplier(Volume);
    Audio->SetPitchMultiplier(Pitch);
    Audio->Play();
}

void ARaidEnemy::UpdateLimb(UStaticMeshComponent* Part, const FVector& Start, const FVector& End, float Width)
{
    Part->SetRelativeLocation((Start + End) * .5f);
    Part->SetRelativeRotation(FRotationMatrix::MakeFromZ(End - Start).Rotator());
    Part->SetRelativeScale3D(FVector(Width / 100.f, Width / 100.f, FVector::Distance(Start, End) / 100.f));
}

void ARaidEnemy::UpdatePresentation(float DeltaSeconds)
{
    FlashRemaining = FMath::Max(0.f, FlashRemaining - DeltaSeconds);
    HitFlashRemaining = FMath::Max(0.f, HitFlashRemaining - DeltaSeconds);
    WeaponRecoil = FMath::FInterpTo(WeaponRecoil, 0.f, DeltaSeconds, 18.f);
    MuzzleFlash->SetVisibility(FlashRemaining > 0.f && !bDead);
    MuzzleLight->SetVisibility(FlashRemaining > 0.f && !bDead);
    if (UniformMaterial) UniformMaterial->SetVectorParameterValue(TEXT("Color"),
        HitFlashRemaining > 0.f ? FLinearColor(.65f, .12f, .05f) : RaidGuardColors::Uniform);
    if (bDead)
    {
        DeathElapsed += DeltaSeconds;
        const float Alpha = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(DeathElapsed / .7f, 0.f, 1.f));
        VisualRoot->SetRelativeLocation(FVector(0.f, 0.f, -67.f * Alpha));
        VisualRoot->SetRelativeRotation(FRotator(86.f * Alpha, 0.f, 10.f * Alpha));
        if (DeathElapsed >= .8f) SetActorTickEnabled(false);
        return;
    }

    const float Speed = GetVelocity().Size2D();
    const bool bMoving = Speed > 15.f;
    const bool bAlert = TargetPlayer != nullptr;
    // Use the existing body and rifle together: raise on discovery, dip on impact,
    // and visibly settle the weapon before another shot is allowed.
    const float ReactionRaise = FMath::SmoothStep(0.f, 1.f, 1.f - AlertRemaining / .55f);
    const float DesiredAim = !bAlert ? 0.f : HitReactRemaining > 0.f ? .15f
        : (StateText == TEXT("Search") ? .55f : ReactionRaise);
    AimBlend = FMath::FInterpTo(AimBlend, DesiredAim, DeltaSeconds, 7.f);
    const float HitAmount = HitReactRemaining > 0.f
        ? FMath::Sin(PI * FMath::Clamp(1.f - HitReactRemaining / .38f, 0.f, 1.f)) : 0.f;
    VisualRoot->SetRelativeLocation(FVector(-6.f * HitAmount, 0.f, -3.f * HitAmount));
    VisualRoot->SetRelativeRotation(FRotator(-9.f * HitAmount, 0.f, 4.f * HitAmount * SideStepSign));
    GaitPhase += Speed * DeltaSeconds / 24.f;
    const float Swing = FMath::Sin(GaitPhase) * FMath::Clamp(Speed / 190.f, 0.f, 1.35f);
    const float Bob = bMoving ? FMath::Sin(GaitPhase * 2.f) * .9f : FMath::Sin(GetWorld()->GetTimeSeconds() * 2.f) * .3f;
    float AimPitch = 0.f;
    if (TargetPlayer && CanSeePlayer(TargetPlayer))
    {
        FVector Eye;
        FRotator View;
        TargetPlayer->GetActorEyesViewPoint(Eye, View);
        AimPitch = FMath::Clamp((Eye - Muzzle->GetComponentLocation()).Rotation().Pitch, -22.f, 25.f);
    }
    const float SettleDip = 5.f * FMath::Clamp(ReaimRemaining / .3f, 0.f, 1.f);
    WeaponRoot->SetRelativeRotation(FRotator(FMath::Lerp(-20.f, AimPitch, AimBlend)
        + WeaponRecoil - SettleDip - HitAmount * 12.f, 0.f, HitAmount * 7.f));
    HeadRoot->SetRelativeRotation(FRotator(-HitAmount * 6.f,
        StateText == TEXT("Search") ? FMath::Sin(GetWorld()->GetTimeSeconds() * 2.3f) * 18.f : 0.f, 0.f));

    if (bTemplateBody)
    {
        UAnimationAsset* Desired = bMoving ? RifleWalkAnimation.Get() : RifleIdleAnimation.Get();
        if (PlayingAnimation != Desired)
        {
            GetMesh()->PlayAnimation(Desired, true);
            PlayingAnimation = Desired;
        }
        GetMesh()->SetPlayRate(bMoving ? FMath::Clamp(Speed / 170.f, .65f, 1.8f) : 1.f);
        const FTransform Visual = VisualRoot->GetComponentTransform();
        // 按骨骼位置跟随步态；武器方向由当前瞄准独立控制。
        HeadRoot->SetRelativeLocation(Visual.InverseTransformPosition(GetMesh()->GetBoneLocation(TEXT("head"))));
        TorsoRoot->SetRelativeLocation(Visual.InverseTransformPosition(GetMesh()->GetBoneLocation(TEXT("spine_03"))) - FVector(0.f, 0.f, 4.f));
        WeaponRoot->SetRelativeLocation(Visual.InverseTransformPosition(GetMesh()->GetBoneLocation(TEXT("hand_r"))));
        LeftHand->SetRelativeLocation(Visual.InverseTransformPosition(GetMesh()->GetBoneLocation(TEXT("hand_l"))));
        RightHand->SetRelativeLocation(Visual.InverseTransformPosition(GetMesh()->GetBoneLocation(TEXT("hand_r"))));
        LeftBoot->SetRelativeLocation(Visual.InverseTransformPosition(GetMesh()->GetBoneLocation(TEXT("foot_l"))) + FVector(5.f, 0.f, -3.f));
        RightBoot->SetRelativeLocation(Visual.InverseTransformPosition(GetMesh()->GetBoneLocation(TEXT("foot_r"))) + FVector(5.f, 0.f, -3.f));
    }
    else
    {
        TorsoRoot->SetRelativeLocation(FVector(0.f, 0.f, 20.f + Bob));
        HeadRoot->SetRelativeLocation(FVector(0.f, 0.f, 59.f + Bob));
        WeaponRoot->SetRelativeLocation(FVector(32.f, 13.f, 18.f + AimBlend * 7.f + Bob));
        const FVector LeftElbow(18.f, -27.f, 10.f + Bob);
        const FVector RightElbow(14.f, 27.f, 9.f + Bob);
        const FVector LeftPalm(60.f, 1.f, 19.f + AimBlend * 7.f + Bob);
        const FVector RightPalm(32.f, 13.f, 16.f + AimBlend * 7.f + Bob);
        UpdateLimb(LeftUpperArm, FVector(0.f, -25.f, 32.f + Bob), LeftElbow, 16.f);
        UpdateLimb(RightUpperArm, FVector(0.f, 25.f, 32.f + Bob), RightElbow, 16.f);
        UpdateLimb(LeftForearm, LeftElbow, LeftPalm, 13.f);
        UpdateLimb(RightForearm, RightElbow, RightPalm, 13.f);
        LeftHand->SetRelativeLocation(LeftPalm);
        RightHand->SetRelativeLocation(RightPalm);
        const FVector LK(12.f * Swing, -12.f, -48.f + FMath::Max(0.f, Swing) * 4.f);
        const FVector RK(-12.f * Swing, 12.f, -48.f + FMath::Max(0.f, -Swing) * 4.f);
        const FVector LA(22.f * Swing, -12.f, -78.f + FMath::Max(0.f, Swing) * 9.f);
        const FVector RA(-22.f * Swing, 12.f, -78.f + FMath::Max(0.f, -Swing) * 9.f);
        UpdateLimb(LeftThigh, FVector(0.f, -12.f, -18.f), LK, 18.f);
        UpdateLimb(RightThigh, FVector(0.f, 12.f, -18.f), RK, 18.f);
        UpdateLimb(LeftCalf, LK, LA, 14.f);
        UpdateLimb(RightCalf, RK, RA, 14.f);
        LeftKnee->SetRelativeLocation(LK + FVector(8.f, 0.f, 0.f));
        RightKnee->SetRelativeLocation(RK + FVector(8.f, 0.f, 0.f));
        LeftBoot->SetRelativeLocation(LA + FVector(6.f, 0.f, -5.f));
        RightBoot->SetRelativeLocation(RA + FVector(6.f, 0.f, -5.f));
    }
    FootstepRemaining -= DeltaSeconds;
    if (bMoving && FootstepRemaining <= 0.f)
    {
        PlaySpatialSound(FootstepAudio, FootstepSound, .38f, FMath::FRandRange(.9f, 1.1f));
        FootstepRemaining = bAlert ? .31f : .48f;
    }
}

bool ARaidEnemy::CanSeePlayer(const ARaidPlayer* Player) const
{
    if (!IsValid(Player) || !Player->IsAlive()) return false;
    FVector PlayerEye;
    FRotator PlayerView;
    Player->GetActorEyesViewPoint(PlayerEye, PlayerView);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(RaidEnemySight), false, this);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit,
        GetActorLocation() + FVector(0.f, 0.f, 57.f), PlayerEye, ECC_Visibility, Query);
    return !bHit || Hit.GetActor() == Player;
}

bool ARaidEnemy::HasClearShot(const ARaidPlayer* Player, FHitResult& Hit) const
{
    if (!IsValid(Player) || !Player->IsAlive()) return false;
    FVector Eye;
    FRotator View;
    Player->GetActorEyesViewPoint(Eye, View);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(RaidEnemyShot), false, this);
    FHitResult Obstruction;
    const FVector MuzzlePosition = Muzzle->GetComponentLocation();
    // 枪管也不能穿过近处墙体：先检测胸口到枪口，再检测枪口到目标。
    if (GetWorld()->LineTraceSingleByChannel(Obstruction, GetActorLocation() + FVector(0.f, 0.f, 28.f),
        MuzzlePosition, ECC_Visibility, Query) && Obstruction.GetActor() != Player) return false;
    // 从真正的枪口检测，避免头探出墙边而枪仍在墙后时穿墙开火。
    return GetWorld()->LineTraceSingleByChannel(Hit, MuzzlePosition, Eye, ECC_Visibility, Query)
        && Hit.GetActor() == Player;
}

bool ARaidEnemy::FindSideStep(FVector& Destination, float Distance, bool bSearching)
{
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav || GetCharacterMovement()->MovementMode == MOVE_None) return false;
    FVector Toward = (LastKnownPlayerLocation - GetActorLocation()).GetSafeNormal2D();
    if (Toward.IsNearlyZero()) Toward = GetActorForwardVector();
    const FVector Side(-Toward.Y, Toward.X, 0.f);
    // Try both sides of a local obstacle; keep the chosen destination long enough
    // to make a real step instead of continually moving the goal with the guard.
    for (int32 Attempt = 0; Attempt < 2; ++Attempt)
    {
        const float Sign = Attempt == 0 ? SideStepSign : -SideStepSign;
        const FVector Candidate = GetActorLocation() + Side * Distance * Sign
            + Toward * (bSearching ? 100.f : -25.f);
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(70.f, 70.f, 180.f))) continue;
        if (FVector::Dist2D(Projected.Location, GetActorLocation()) < 75.f) continue;
        FHitResult Obstruction;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(RaidEnemyStep), false, this);
        if (GetWorld()->LineTraceSingleByChannel(Obstruction,
            GetActorLocation() + FVector(0.f, 0.f, 28.f),
            Projected.Location + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 28.f),
            ECC_Visibility, Query)) continue;
        Destination = Projected.Location;
        SideStepSign = -Sign;
        return true;
    }
    SideStepSign *= -1.f;
    return false;
}

void ARaidEnemy::ResetPatrolProgress()
{
    PatrolObservedIndex = PatrolIndex;
    PatrolProgressLocation = GetActorLocation();
    PatrolNoProgressSeconds = 0.f;
    PatrolFailedMoveRequests = 0;
}

void ARaidEnemy::UpdatePatrol(AAIController* AI, float DeltaSeconds)
{
    PatrolIndex %= PatrolPoints.Num();
    if (PatrolObservedIndex != PatrolIndex) ResetPatrolProgress();
    if (FVector::Dist2D(GetActorLocation(), PatrolPoints[PatrolIndex]) < 120.f)
    {
        PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
        PatrolWaitRemaining = .65f;
        ResetPatrolProgress();
    }
    if (PatrolWaitRemaining > 0.f || GetCharacterMovement()->MovementMode == MOVE_None)
    {
        AI->StopMovement();
        ResetPatrolProgress();
        return;
    }

    // Runtime navigation gets its initial build window. Rebuilding/locked data,
    // intentional waits and combat must not look like a blocked patrol route.
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const bool bCanJudgeProgress = PatrolStartupGraceRemaining <= 0.f && Nav
        && Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)
        && !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(this);
    if (!bCanJudgeProgress) ResetPatrolProgress();
    else if (FVector::DistSquared2D(GetActorLocation(), PatrolProgressLocation) >= FMath::Square(20.f))
    {
        PatrolProgressLocation = GetActorLocation();
        PatrolNoProgressSeconds = 0.f;
    }
    else PatrolNoProgressSeconds += DeltaSeconds;

    if (MoveUpdateRemaining <= 0.f)
    {
        const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(
            PatrolPoints[PatrolIndex], 65.f, true, true, true, false);
        MoveUpdateRemaining = .7f;
        if (bCanJudgeProgress)
        {
            // AlreadyAtGoal can refer to a projected/partial endpoint while the
            // authored waypoint remains outside our arrival radius.
            PatrolFailedMoveRequests = Result == EPathFollowingRequestResult::RequestSuccessful
                ? 0 : PatrolFailedMoveRequests + 1;
        }
    }
    if (bCanJudgeProgress && (PatrolFailedMoveRequests >= 3 || PatrolNoProgressSeconds >= 3.f))
    {
        const int32 FailedIndex = PatrolIndex;
        AI->StopMovement();
        PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
        ++PatrolRecoveryCount;
        PatrolWaitRemaining = .35f;
        MoveUpdateRemaining = 0.f;
        UE_LOG(LogTemp, Display, TEXT("RAID_AI_PATROL_RECOVERY: %s waypoint=%d next=%d failures=%d stalled=%.2fs"),
            *GetName(), FailedIndex, PatrolIndex, PatrolFailedMoveRequests, PatrolNoProgressSeconds);
        ResetPatrolProgress();
    }
}

bool ARaidEnemy::FindLootDropLocation(FVector& Location) const
{
    const ARaidLoot* LootDefaults = GetDefault<ARaidLoot>();
    if (!LootDefaults || !LootDefaults->CollisionBox) return false;
    const FVector Extent = LootDefaults->CollisionBox->GetScaledBoxExtent();
    const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const FVector Offsets[] = {FVector::ZeroVector,
        FVector(-80.f, 0.f, 0.f), FVector(80.f, 0.f, 0.f),
        FVector(0.f, -80.f, 0.f), FVector(0.f, 80.f, 0.f),
        FVector(-80.f, -80.f, 0.f), FVector(-80.f, 80.f, 0.f),
        FVector(80.f, -80.f, 0.f), FVector(80.f, 80.f, 0.f)};
    FCollisionQueryParams Query(SCENE_QUERY_STAT(RaidEnemyLootDrop), false, this);
    const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
    for (const FVector& Offset : Offsets)
    {
        const FVector Probe = Feet + Offset;
        FHitResult Surface;
        if (!GetWorld()->LineTraceSingleByObjectType(Surface, Probe + FVector(0.f, 0.f, 90.f),
            Probe - FVector(0.f, 0.f, 220.f), StaticObjects, Query)
            || Surface.bStartPenetrating || !GetCharacterMovement()->IsWalkable(Surface)) continue;
        const FVector Candidate = Surface.ImpactPoint + FVector(0.f, 0.f, Extent.Z + 2.f);
        // The full interaction box must fit above the floor. A nearby alternative
        // must also remain on this side of any wall, rather than dropping through it.
        if (GetWorld()->OverlapAnyTestByObjectType(Candidate, FQuat::Identity, StaticObjects,
            FCollisionShape::MakeBox(Extent), Query)) continue;
        FHitResult Obstruction;
        if (GetWorld()->LineTraceSingleByObjectType(Obstruction, GetActorLocation(), Candidate, StaticObjects, Query)) continue;
        Location = Candidate;
        return true;
    }
    return false;
}

void ARaidEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bDead)
    {
        UpdatePresentation(DeltaSeconds);
        return;
    }
    ARaidGameMode* GM = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    AAIController* AI = Cast<AAIController>(GetController());
    if (!GM || !GM->IsRaidActive())
    {
        if (AI) AI->StopMovement();
        ResetPatrolProgress();
        if (bTemplateBody) GetMesh()->SetPlayRate(0.f);
        MuzzleFlash->SetVisibility(false);
        MuzzleLight->SetVisibility(false);
        return;
    }
    ShotCooldown = FMath::Max(0.f, ShotCooldown - DeltaSeconds);
    MoveUpdateRemaining -= DeltaSeconds;
    AlertRemaining = FMath::Max(0.f, AlertRemaining - DeltaSeconds);
    HitReactRemaining = FMath::Max(0.f, HitReactRemaining - DeltaSeconds);
    ReaimRemaining = FMath::Max(0.f, ReaimRemaining - DeltaSeconds);
    CombatStepRemaining = FMath::Max(0.f, CombatStepRemaining - DeltaSeconds);
    RepositionRemaining = FMath::Max(0.f, RepositionRemaining - DeltaSeconds);
    TargetMemoryRemaining = FMath::Max(0.f, TargetMemoryRemaining - DeltaSeconds);
    PatrolWaitRemaining = FMath::Max(0.f, PatrolWaitRemaining - DeltaSeconds);
    PatrolStartupGraceRemaining = FMath::Max(0.f, PatrolStartupGraceRemaining - DeltaSeconds);
    ARaidPlayer* Player = Cast<ARaidPlayer>(UGameplayStatics::GetPlayerCharacter(this, 0));
    const bool bVisible = IsValid(Player) && Player->IsAlive()
        && FVector::DistSquared(GetActorLocation(), Player->GetActorLocation()) <= FMath::Square(DetectionRadius)
        && CanSeePlayer(Player);
    if (bVisible)
    {
        if (!TargetPlayer)
        {
            AlertRemaining = .55f;
            ShotsSinceStep = 0;
            PlaySpatialSound(VoiceAudio, AlertSound, .8f);
        }
        TargetPlayer = Player;
        LastKnownPlayerLocation = Player->GetActorLocation();
        TargetMemoryRemaining = 8.f;
    }
    if (!IsValid(TargetPlayer) || !TargetPlayer->IsAlive() || TargetMemoryRemaining <= 0.f) TargetPlayer = nullptr;
    GetCharacterMovement()->MaxWalkSpeed = TargetPlayer ? 310.f : 170.f;
    if (AI && TargetPlayer)
    {
        ResetPatrolProgress();
        FHitResult ClearShot;
        const float TargetDistance = FVector::Dist2D(GetActorLocation(), LastKnownPlayerLocation);
        const bool bCanAttack = bVisible
            && FVector::Distance(GetActorLocation(), TargetPlayer->GetActorLocation()) <= AttackRange
            && HasClearShot(TargetPlayer, ClearShot);
        // Any lost firing lane forces a short, visible aim reset on reacquisition.
        if (bCanAttack && !bHadClearShot) ReaimRemaining = FMath::Max(ReaimRemaining, .3f);
        if (!bCanAttack && bHadClearShot)
        {
            CombatStepRemaining = 0.f;
            RepositionRemaining = 0.f;
            MoveUpdateRemaining = 0.f;
        }
        bHadClearShot = bCanAttack;
        StateText = bCanAttack ? TEXT("Attack") : bVisible ? TEXT("Chase") : TEXT("Search");
        if (AlertRemaining > 0.f || HitReactRemaining > 0.f)
        {
            AI->StopMovement();
            GetCharacterMovement()->bOrientRotationToMovement = false;
            const FRotator Aim = (LastKnownPlayerLocation - GetActorLocation()).Rotation();
            SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Aim.Yaw, 0.f), DeltaSeconds, 7.f));
        }
        else if (bCanAttack)
        {
            GetCharacterMovement()->MaxWalkSpeed = 190.f;
            GetCharacterMovement()->bOrientRotationToMovement = false;
            const FRotator Aim = (LastKnownPlayerLocation - GetActorLocation()).Rotation();
            SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Aim.Yaw, 0.f), DeltaSeconds, 9.f));
            if (CombatStepRemaining > 0.f)
            {
                ReaimRemaining = FMath::Max(ReaimRemaining, .28f);
            }
            else
            {
                AI->StopMovement();
                const bool bFacingTarget = FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, Aim.Yaw)) < 12.f;
                if (ShotCooldown <= 0.f && ReaimRemaining <= 0.f && AimBlend > .82f && bFacingTarget)
                {
                    ShootAtPlayer(TargetPlayer);
                    ShotCooldown = FMath::Max(.1f, AttackInterval);
                    ReaimRemaining = .3f;
                    ++ShotsSinceStep;
                    // Take one short lateral step between pairs of shots, then
                    // stop and reacquire. No movement is forced on disabled guards.
                    FVector StepDestination;
                    if (ShotsSinceStep >= 2 && TargetDistance > 250.f && FindSideStep(StepDestination, 145.f, false))
                    {
                        AI->MoveToLocation(StepDestination, 30.f, true, true, true, false);
                        CombatStepRemaining = .42f;
                        ShotsSinceStep = 0;
                    }
                }
            }
        }
        else
        {
            GetCharacterMovement()->bOrientRotationToMovement = true;
            if (MoveUpdateRemaining <= 0.f)
            {
                FVector Destination = LastKnownPlayerLocation;
                const bool bNeedSideStep = bVisible ? TargetDistance <= AttackRange
                    : TargetDistance < FMath::Max(AttackRange, 240.f);
                if (bNeedSideStep)
                {
                    if (RepositionRemaining <= 0.f)
                    {
                        if (FindSideStep(RepositionLocation, 190.f, true)) RepositionRemaining = 1.05f;
                    }
                    if (RepositionRemaining > 0.f) Destination = RepositionLocation;
                }
                AI->MoveToLocation(Destination, 45.f, true, true, true, false);
                MoveUpdateRemaining = .35f;
            }
        }
    }
    else if (AI && !PatrolPoints.IsEmpty())
    {
        bHadClearShot = false;
        CombatStepRemaining = 0.f;
        RepositionRemaining = 0.f;
        ShotsSinceStep = 0;
        StateText = TEXT("Patrol");
        GetCharacterMovement()->bOrientRotationToMovement = true;
        UpdatePatrol(AI, DeltaSeconds);
    }
    UpdatePresentation(DeltaSeconds);
}

void ARaidEnemy::ShootAtPlayer(ARaidPlayer* Player)
{
    FHitResult Hit;
    if (!CanSeePlayer(Player) || !HasClearShot(Player, Hit)) return;
    FlashRemaining = .065f;
    WeaponRecoil = 4.f;
    MuzzleFlash->SetVisibility(true);
    MuzzleLight->SetVisibility(true);
    PlaySpatialSound(WeaponAudio, ShotSound, .75f, FMath::FRandRange(.96f, 1.04f));
    DrawDebugLine(GetWorld(), Muzzle->GetComponentLocation(), Hit.ImpactPoint,
        FColor(255, 184, 65), false, .065f, 0, .8f);
    // 感知和每一发射击分别追踪，只对枪口射线真正命中的玩家扣血。
    UGameplayStatics::ApplyPointDamage(Player, AttackDamage,
        (Hit.ImpactPoint - Muzzle->GetComponentLocation()).GetSafeNormal(), Hit, GetController(), this, nullptr);
}

float ARaidEnemy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    ARaidGameMode* GM = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this));
    if (bDead || DamageAmount <= 0.f || !GM || !GM->IsRaidActive()) return 0.f;
    const float AppliedDamage = FMath::Min(Health, DamageAmount);
    Health = FMath::Max(0.f, Health - AppliedDamage);
    HitFlashRemaining = .12f;
    HitReactRemaining = .38f;
    ReaimRemaining = .35f;
    CombatStepRemaining = 0.f;
    ShotCooldown = FMath::Max(ShotCooldown, .55f);
    FlashRemaining = 0.f;
    MuzzleFlash->SetVisibility(false);
    MuzzleLight->SetVisibility(false);
    if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    if (Health <= 0.f) Die();
    else
    {
        ARaidPlayer* Attacker = EventInstigator ? Cast<ARaidPlayer>(EventInstigator->GetPawn()) : nullptr;
        if (!Attacker) Attacker = Cast<ARaidPlayer>(DamageCauser);
        if (IsValid(Attacker))
        {
            if (!TargetPlayer) PlaySpatialSound(VoiceAudio, AlertSound, .8f);
            TargetPlayer = Attacker;
            LastKnownPlayerLocation = Attacker->GetActorLocation();
            TargetMemoryRemaining = 8.f;
            AlertRemaining = FMath::Max(AlertRemaining, .2f);
            MoveUpdateRemaining = 0.f;
            StateText = TEXT("Chase");
        }
    }
    return AppliedDamage;
}

void ARaidEnemy::Die()
{
    bDead = true;
    StateText = TEXT("Dead");
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        AI->StopMovement();
        if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Enemy died"));
    }
    SetActorEnableCollision(false);
    GetMesh()->Stop();
    MuzzleFlash->SetVisibility(false);
    MuzzleLight->SetVisibility(false);
    WeaponAudio->Stop();
    VoiceAudio->Stop();
    FootstepAudio->Stop();
    // 枪留在地上，身体平滑倒下；玩法碰撞在受致命伤时已立即关闭。
    WeaponRoot->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
    WeaponRoot->SetRelativeLocation(FVector(0.f, 45.f, -78.f));
    WeaponRoot->SetRelativeRotation(FRotator(0.f, 28.f, 0.f));
    if (ARaidGameMode* GM = Cast<ARaidGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        ++GM->EnemiesKilled;
        GM->PushMessage(TEXT("敌人已被击毙，可回收其物资。"));
    }
    FVector DropLocation;
    if (FindLootDropLocation(DropLocation))
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (ARaidLoot* Drop = GetWorld()->SpawnActor<ARaidLoot>(ARaidLoot::StaticClass(),
            DropLocation, FRotator::ZeroRotator, Spawn))
        {
            // Contents remain lazy so this source flag is set before they are built.
            // Existing ItemId/DisplayName/Quantity/Value defaults remain the payload.
            Drop->bEnemyDrop = true;
        }
    }
    else UE_LOG(LogTemp, Warning, TEXT("RAID_AI_LOOT_DROP: no unobstructed floor near %s at %s"),
        *GetName(), *GetActorLocation().ToString());
}
