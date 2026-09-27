#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HumanTypes.h"
#include "TerrainGrid.generated.h"

class UProceduralMeshComponent;
class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FClimate;

struct AB_API FTerrainFooting
{
    float SinkCm = 0.0f;
    float Grip = 0.7f;
    float Effort = 1.0f;
    float Stick = 0.0f;
    float WaterCm = 0.0f;
    float SnowCm = 0.0f;
    float Strength = 200.0f;
    bool bSoft = false;
    bool bMud = false;
};

UCLASS()
class AB_API ATerrainGrid : public AActor
{
    GENERATED_BODY()

public:
    ATerrainGrid();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "Terrain")
    float CellSize = 100.0f;

    UPROPERTY(EditAnywhere, Category = "Terrain")
    int32 Side = 300;

    UPROPERTY(EditAnywhere, Category = "Terrain")
    int32 ChunkCells = 30;

    UPROPERTY(EditAnywhere, Category = "Terrain")
    FVector2D HillAt = FVector2D(-2400.0f, 0.0f);

    UPROPERTY(EditAnywhere, Category = "Terrain")
    float HillHeight = 1800.0f;

    UPROPERTY(EditAnywhere, Category = "Terrain")
    float FarReach = 200000.0f;

    UPROPERTY(EditAnywhere, Category = "Terrain")
    float FarCell = 2500.0f;

    void Generate();
    void EnsureVisuals();
    void RebuildDirty();
    void Flatten(const FVector& Centre, const FVector2D& HalfExtent, float Margin);
    void MarkSurface(const FVector& Centre, const FVector2D& HalfExtent, float Yaw, EResourceKind Substance, float Trodden);

    float HeightAt(const FVector& World) const;
    FVector NormalAt(const FVector& World) const;
    EResourceKind SurfaceAt(const FVector& World) const;
    EResourceKind SubstanceAtDepth(const FVector& World, float DepthCm) const;
    EResourceKind Dig(const FVector& World, float DepthCm, float& OutCubicMetres);
    bool Place(const FVector& World, EResourceKind Substance);
    bool IsWaterAt(const FVector& World) const;
    float WaterLevelAt(const FVector& World) const;
    bool IsGrassAt(const FVector& World) const;
    float RoughnessAt(const FVector& World) const;
    bool HasData() const { return Heights.Num() > 0; }

    float MoistureAt(const FVector& World) const;
    bool FootingAt(const FVector& World, float BodyKg, FTerrainFooting& Out) const;
    float WalkFactorAt(const FVector& World, float& OutGrip) const;
    float ComplianceAt(const FVector& World) const;
    void AddWater(const FVector& World, float Kilograms, float RadiusCm);
    void SoakArea(const FVector& Centre, float RadiusCm, float Liquidity, bool bBare);
    void Trample(const FVector& World, float Kilograms);
    void Impact(const FVector& World, float Joules);
    void Footprint(const FVector& World, float Yaw, float DepthCm, float Stick, bool bLeft);
    FString DescribeAt(const FVector& World) const;

    void AdvanceSoil(float Seconds, const FClimate& Sky);

    static EResourceKind LayerAt(int32 Depth, int32 Salt);

private:
    float BaseHeight(float WorldX, float WorldY, bool& bOutWet, float& OutWater) const;
    bool CellOf(const FVector& World, int32& OutX, int32& OutY) const;
    int32 CellIndex(int32 X, int32 Y) const { return Y * Side + X; }
    int32 CornerIndex(int32 X, int32 Y) const { return Y * (Side + 1) + X; }
    float CornerHeight(int32 X, int32 Y) const;
    EResourceKind SurfaceKind(int32 Cell) const;
    EResourceKind DeepKind(int32 X, int32 Y, float DepthCm) const;
    void MarkDirty(int32 X, int32 Y, bool bShape);
    void BuildChunk(int32 ChunkX, int32 ChunkY, bool bShape);
    void BuildWater();
    void BuildFar();
    void HideLandscapes();
    void CornerLook(int32 X, int32 Y, FLinearColor& OutColour, FVector2D& OutExtra) const;
    float CellWetness(int32 Cell) const;
    UMaterialInterface* GroundLook() const;
    UProceduralMeshComponent* MakeMesh(bool bCollision);

    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UProceduralMeshComponent>> Chunks;

    UPROPERTY(Transient)
    TObjectPtr<UProceduralMeshComponent> WaterMesh;

    UPROPERTY(Transient)
    TObjectPtr<UProceduralMeshComponent> FarMesh;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UDecalComponent>> Prints;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> PrintLooks;

    UPROPERTY()
    TArray<float> Heights;

    UPROPERTY()
    TArray<float> Original;

    UPROPERTY()
    TArray<float> WaterLevel;

    UPROPERTY()
    TArray<uint8> Surface;

    UPROPERTY()
    TArray<uint8> WaterCell;

    UPROPERTY()
    TArray<float> Slope;

    UPROPERTY()
    TArray<float> Moisture;

    UPROPERTY()
    TArray<float> SoilTemperature;

    UPROPERTY()
    TArray<float> SoilIce;

    UPROPERTY()
    TArray<float> Puddle;

    UPROPERTY()
    TArray<float> SnowDepth;

    UPROPERTY()
    TArray<float> Trodden;

    UPROPERTY()
    TArray<float> Topsoil;

    UPROPERTY()
    TArray<float> Subsoil;

    TArray<uint8> ChunkDirty;
    TArray<float> ChunkShown;

    int32 ChunksPerSide = 0;
    int32 NextPrint = 0;
    bool bBuilt = false;
    bool bPrintsTried = false;
    float LandscapeTimer = 0.0f;
};
