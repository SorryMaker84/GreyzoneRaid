#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaidExtractionZone.generated.h"

class ARaidGameMode;
class ARaidPlayer;
class UBoxComponent;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** A marked extraction area. Leaving its bounds resets the countdown. */
UCLASS(Blueprintable)
class GREYZONERAID_API ARaidExtractionZone : public AActor
{
    GENERATED_BODY()

public:
    ARaidExtractionZone();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintPure, Category="Raid|Extraction")
    float GetSecondsRemaining() const { return RemainingSeconds; }

    UFUNCTION(BlueprintPure, Category="Raid|Extraction")
    bool IsPlayerInside() const { return bPlayerInside; }

    UFUNCTION(BlueprintPure, Category="Raid|Extraction")
    FName GetExtractionId() const { return ExtractionId; }

    UFUNCTION(BlueprintPure, Category="Raid|Extraction")
    FString GetDisplayName() const { return DisplayName; }

    // Availability describes the mission gate. All routes still require carried loot.
    UFUNCTION(BlueprintPure, Category="Raid|Extraction")
    bool IsAvailable() const;

    UFUNCTION(BlueprintPure, Category="Raid|Extraction")
    FString GetUnavailableReason() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Extraction")
    FName ExtractionId = TEXT("E-07");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Extraction")
    FString DisplayName = TEXT("撤离点");

    // None keeps the Depot and ordinary south route behavior unchanged.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Extraction")
    FName RequiredMissionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Raid|Extraction", meta=(ClampMin="0.1"))
    float ExtractSeconds = 8.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Extraction")
    TObjectPtr<UBoxComponent> Trigger;

    // Retained component name/property for existing maps; now the signal cabinet's base.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Raid|Visuals")
    TObjectPtr<UStaticMeshComponent> PlatformMesh;

private:
    void RefreshVisuals();
    void AlignStationToSurface();
    void UpdateSignal(ARaidGameMode* GM, ARaidPlayer* Player);

    UPROPERTY() TObjectPtr<USceneComponent> VisualRoot;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> StationParts;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SignalLenses;
    UPROPERTY() TObjectPtr<UPointLightComponent> SignalLight;
    UPROPERTY() TObjectPtr<UTextRenderComponent> StatusDisplay;
    UPROPERTY() TObjectPtr<UTextRenderComponent> StationTitle;
    UPROPERTY() TObjectPtr<UMaterialInterface> StandbyPaint;
    UPROPERTY() TObjectPtr<UMaterialInterface> ReadyPaint;
    UPROPERTY() TObjectPtr<UMaterialInterface> FailedPaint;
    FString LastStatusText;
    TWeakObjectPtr<ARaidPlayer> CountdownPlayer;
    int32 LastSignalState = -1;
    float RemainingSeconds = 8.f;
    bool bPlayerInside = false;
    bool bCountdownRunning = false;
    float LastCountdownDamageTime = -100.f;
};
