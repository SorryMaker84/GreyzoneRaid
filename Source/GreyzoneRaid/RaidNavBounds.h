#pragma once

#include "CoreMinimal.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "RaidNavBounds.generated.h"

class UBoxComponent;

/** A navigation bounds volume that does not require an editor-built BSP brush. */
UCLASS()
class GREYZONERAID_API ARaidNavBounds : public ANavMeshBoundsVolume
{
    GENERATED_BODY()

public:
    ARaidNavBounds(const FObjectInitializer& ObjectInitializer);

    // NavigationSystem queries all registered primitive bounds, including hidden boxes.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation")
    TObjectPtr<UBoxComponent> BoundsBox;

    // Extent is half the full size, in Unreal units (centimeters).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation", meta = (ClampMin = "1.0"))
    FVector BoundsExtent = FVector(2500.0f, 2000.0f, 500.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    FVector BoundsOffset = FVector::ZeroVector;

    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void BeginPlay() override;
};
