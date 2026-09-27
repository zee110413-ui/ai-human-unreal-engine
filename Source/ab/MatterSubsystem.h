#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Matter.h"
#include "MatterSubsystem.generated.h"

class UMatterComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPhysicalMaterial;
class UPrimitiveComponent;
class ATerrainGrid;

struct AB_API FClimate
{
    int32 DayOfYear = 130;
    float Hour = 8.0f;
    float Celsius = 12.0f;
    float Humidity = 0.7f;
    float Rain = 0.0f;
    float Snow = 0.0f;
    float Wind = 2.0f;
    float Cloud = 0.5f;
    float Sun = 0.0f;
    float SunElevation = 0.0f;
    float SunAzimuth = 180.0f;
    float SnowOnGround = 0.0f;
    float SeasonMean = 12.0f;
    float Wetness = 0.0f;
};

UCLASS()
class AB_API UMatterSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

    static UMatterSubsystem* Get(const UObject* WorldContext);

    const FClimate& Climate() const { return Weather; }
    FMatterAir AirAt(const FVector& Where, bool bSheltered) const;
    bool IsSheltered(const FVector& Where, const AActor* Ignore) const;
    FString DescribeClimate() const;

    void ForceRain(float MillimetresPerHour, float GameHours);
    void ForceTemperature(float Celsius, float GameHours);
    void ForceSeason(int32 DayOfYear);

    void Register(UMatterComponent* Item);
    void Unregister(UMatterComponent* Item);
    void RegisterStructure(class AMatterStructure* Structure);
    void UnregisterStructure(class AMatterStructure* Structure);
    const TArray<TWeakObjectPtr<class AMatterStructure>>& GetStructures() const { return Structures; }
    void SetTerrain(ATerrainGrid* InTerrain);
    ATerrainGrid* GetTerrain() const { return Terrain.Get(); }
    int32 CountItems() const { return Items.Num(); }

    UMaterialInterface* LookOf(EResourceKind Kind);
    UMaterialInterface* Look(EMatterLook Kind, const FLinearColor& Tint = FLinearColor::White);
    UMaterialInterface* GroundMaterial();
    UPhysicalMaterial* SurfaceOf(EResourceKind Kind, float Moisture, float Celsius);

    void AddHeat(const FVector& Where, float Watts);
    float RadiantAt(const FVector& Where) const;
    float GameSeconds(float RealDelta) const;
    float WorldTime() const;

    FString DescribeAt(const FVector& Where, UPrimitiveComponent* Component, int32 Item) const;

private:
    struct FHeat
    {
        FVector Where = FVector::ZeroVector;
        float Watts = 0.0f;
    };

    void AdvanceClimate(float Seconds);
    void AdvanceMatter(float Seconds);
    void PushWeatherToPeople() const;

    FClimate Weather;
    FRandomStream Chance;
    float CloudDrift = 0.45f;
    float Anomaly = 0.0f;
    float WindDrift = 3.0f;
    int32 StartDayOfYear = 130;

    float ForcedRain = 0.0f;
    float ForcedRainUntil = -1.0f;
    float ForcedCelsius = 0.0f;
    float ForcedCelsiusUntil = -1.0f;

    float ClimateTimer = 0.0f;
    float ClimateSeconds = 0.0f;
    float MatterTimer = 0.0f;
    float MatterSeconds = 0.0f;
    int32 ShelterIndex = 0;

    TArray<TWeakObjectPtr<UMatterComponent>> Items;
    TArray<TWeakObjectPtr<class AMatterStructure>> Structures;
    TWeakObjectPtr<ATerrainGrid> Terrain;

    TArray<FHeat> Heat;
    TArray<FHeat> HeatBuilding;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> MatterBase;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> GroundBase;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> Ground;

    UPROPERTY()
    TMap<FName, TObjectPtr<UMaterialInstanceDynamic>> Looks;

    UPROPERTY()
    TMap<int32, TObjectPtr<UPhysicalMaterial>> Surfaces;
};
