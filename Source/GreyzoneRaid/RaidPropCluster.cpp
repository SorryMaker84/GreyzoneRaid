#include "RaidPropCluster.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

ARaidPropCluster::ARaidPropCluster()
{
    PrimaryActorTick.bCanEverTick = false;
    Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Instances"));
    SetRootComponent(Instances);
    Instances->SetMobility(EComponentMobility::Static);
    Instances->SetCollisionProfileName(TEXT("NoCollision"));
    Instances->SetGenerateOverlapEvents(false);
    Instances->SetCanEverAffectNavigation(false);
}

void ARaidPropCluster::ConfigureCluster(UStaticMesh* Mesh, UMaterialInterface* Material,
    bool bSolid, bool bCastShadows)
{
    Instances->SetStaticMesh(Mesh);
    Instances->SetMaterial(0, Material);
    Instances->SetCollisionProfileName(bSolid ? TEXT("BlockAll") : TEXT("NoCollision"));
    Instances->SetCanEverAffectNavigation(bSolid);
    Instances->SetCastShadow(bCastShadows);
}

int32 ARaidPropCluster::AddWorldInstance(const FTransform& Transform)
{
    return Instances->AddInstance(Transform, true);
}
