#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RaidMissionDefinition.generated.h"

/** One optional raid objective, authored independently of inventory item behavior. */
UCLASS(BlueprintType)
class GREYZONERAID_API URaidMissionDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Raid|Mission")
    FName MissionId = TEXT("FrontierIntel");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Raid|Mission")
    FString Title = TEXT("下载边境通讯情报");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Raid|Mission")
    FString Instruction = TEXT("前往通讯站，长按 E 下载情报");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Raid|Mission")
    FString CompletedText = TEXT("情报下载完成，北侧撤离点已解锁");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Raid|Mission", meta=(ClampMin="0.1", ClampMax="60.0"))
    float HoldSeconds = 6.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Raid|Mission", meta=(ClampMin="80.0", ClampMax="1000.0"))
    float InteractionRange = 260.f;

    bool Validate(FString* OutError = nullptr) const;
};
