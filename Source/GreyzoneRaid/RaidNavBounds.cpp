#include "RaidNavBounds.h"

#include "Components/BoxComponent.h"
#include "NavigationSystem.h"

ARaidNavBounds::ARaidNavBounds(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
    BoundsBox->SetupAttachment(GetRootComponent());
    BoundsBox->SetMobility(EComponentMobility::Static);
    BoundsBox->InitBoxExtent(BoundsExtent);
    BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoundsBox->SetGenerateOverlapEvents(false);
    BoundsBox->SetVisibility(false);
    BoundsBox->SetHiddenInGame(true);
    // This box defines the search region; it must never become a navmesh obstacle.
    BoundsBox->SetCanEverAffectNavigation(false);
}

void ARaidNavBounds::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BoundsBox->SetBoxExtent(BoundsExtent.ComponentMax(FVector(1.0f)), false);
    BoundsBox->SetRelativeLocation(BoundsOffset);
}

void ARaidNavBounds::BeginPlay()
{
    Super::BeginPlay();

    if (UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        // GetComponentsBoundingBox(true) includes BoundsBox even without collision.
        Navigation->OnNavigationBoundsUpdated(this);
    }
}
