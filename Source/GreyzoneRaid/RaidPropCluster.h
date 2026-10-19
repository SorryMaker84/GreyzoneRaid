#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidPropCluster.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** Serialized, static environment instances authored by the Frontier map script. */
UCLASS()
class GREYZONERAID_API ARaidPropCluster : public AActor
{
    GENERATED_BODY()

public:
    ARaidPropCluster();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Environment")
    TObjectPtr<UInstancedStaticMeshComponent> Instances;

    UFUNCTION(BlueprintCallable, Category = "Environment")
    void ConfigureCluster(UStaticMesh* Mesh, UMaterialInterface* Material, bool bSolid, bool bCastShadows);

    UFUNCTION(BlueprintCallable, Category = "Environment")
    int32 AddWorldInstance(const FTransform& Transform);
};
