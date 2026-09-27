#include "TerrainGrid.h"
#include "Matter.h"
#include "MatterSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Components/DecalComponent.h"
#include "LandscapeProxy.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    enum ESurface : uint8
    {
        Grass,
        Bare,
        ClayGround,
        SandGround,
        RockGround,
        GraniteGround,
        PeatGround,
        Tilled
    };

    float Wave(float X, float Y, float Scale, float Shift)
    {
        return FMath::Sin(X * Scale + Shift) * FMath::Cos(Y * Scale * 0.8f - Shift * 0.5f);
    }

    float Smooth(float X)
    {
        const float T = FMath::Clamp(X, 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }

    int32 Salt(int32 X, int32 Y)
    {
        return FMath::Abs(X * 73856093 ^ Y * 19349663) % 997;
    }

    constexpr int32 PrintLimit = 360;
}

ATerrainGrid::ATerrainGrid()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.5f;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    Root->SetMobility(EComponentMobility::Movable);
}

EResourceKind ATerrainGrid::LayerAt(int32 Depth, int32 SaltValue)
{
    if (Depth <= 0)
    {
        return EResourceKind::Soil;
    }
    if (Depth <= 2)
    {
        return EResourceKind::Clay;
    }
    if (Depth <= 4)
    {
        return (SaltValue % 5 == 0) ? EResourceKind::Sand : EResourceKind::Clay;
    }
    if (Depth <= 7)
    {
        return EResourceKind::Limestone;
    }
    if (SaltValue % 23 == 0)
    {
        return EResourceKind::IronOre;
    }
    if (SaltValue % 31 == 0)
    {
        return EResourceKind::Coal;
    }
    return EResourceKind::Granite;
}

float ATerrainGrid::BaseHeight(float WorldX, float WorldY, bool& bOutWet, float& OutWater) const
{
    const float Block = 200.0f;
    const float X = WorldX / Block;
    const float Y = WorldY / Block;

    float Blocks = 3.0f + Wave(X, Y, 0.035f, 0.0f) * 1.6f + Wave(X, Y, 0.012f, 2.1f) * 2.2f;
    const FVector2D Hill(HillAt.X / Block, HillAt.Y / Block);
    const float ToHill = FVector2D::Distance(FVector2D(X, Y), Hill);
    Blocks += (HillHeight / Block) * FMath::Exp(-FMath::Square(ToHill / 19.5f));

    float Height = Blocks * Block;
    const FVector2D Metres(WorldX / 100.0f + 150.0f, WorldY / 100.0f + 150.0f);
    Height += FMath::PerlinNoise2D(Metres * 0.045f) * 60.0f;
    Height += FMath::PerlinNoise2D((Metres + FVector2D(31.7f, -12.3f)) * 0.23f) * 7.0f;
    const float Plain = Height;

    const float River = FMath::Abs((Y - 24.0f) + FMath::Sin(X * 0.05f) * 6.0f);
    const float Creek = FMath::Abs((X + 24.0f) + FMath::Sin(Y * 0.07f) * 5.0f);

    bOutWet = false;
    OutWater = -1.0e6f;
    if (River < 4.0f && ToHill > 9.0f)
    {
        Height -= 260.0f * Smooth((4.0f - River) / 3.0f);
        if (River < 2.6f)
        {
            bOutWet = true;
            OutWater = Plain - 95.0f;
        }
    }
    if (Creek < 2.6f && Y < 33.0f && ToHill > 7.5f)
    {
        Height -= 150.0f * Smooth((2.6f - Creek) / 2.0f);
        if (Creek < 1.6f)
        {
            bOutWet = true;
            OutWater = FMath::Max(OutWater, Plain - 90.0f);
        }
    }
    return Height;
}

void ATerrainGrid::Generate()
{
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Matter->SetTerrain(this);
    }

    const FVector Base = GetActorLocation();
    const int32 Corners = (Side + 1) * (Side + 1);
    const int32 Cells = Side * Side;
    Heights.SetNumZeroed(Corners);
    Original.SetNumZeroed(Corners);
    WaterLevel.Init(-1.0e6f, Cells);
    Surface.Init(ESurface::Grass, Cells);
    WaterCell.Init(0, Cells);
    Slope.Init(0.0f, Cells);
    Moisture.Init(0.12f, Cells);
    SoilTemperature.Init(10.0f, Cells);
    SoilIce.Init(0.0f, Cells);
    Puddle.Init(0.0f, Cells);
    SnowDepth.Init(0.0f, Cells);
    Trodden.Init(0.0f, Cells);
    Topsoil.Init(30.0f, Cells);
    Subsoil.Init(220.0f, Cells);

    auto WorldOf = [this, &Base](float Gx, float Gy)
    {
        return FVector2D(Base.X + (Gx - Side * 0.5f) * CellSize, Base.Y + (Gy - Side * 0.5f) * CellSize);
    };

    for (int32 Y = 0; Y <= Side; ++Y)
    {
        for (int32 X = 0; X <= Side; ++X)
        {
            bool bWet = false;
            float Water = 0.0f;
            const FVector2D W = WorldOf(X, Y);
            Heights[CornerIndex(X, Y)] = BaseHeight(W.X, W.Y, bWet, Water);
        }
    }
    Original = Heights;

    float Sum = 0.0f;
    for (int32 Y = 0; Y < Side; ++Y)
    {
        for (int32 X = 0; X < Side; ++X)
        {
            bool bWet = false;
            float Water = 0.0f;
            const FVector2D W = WorldOf(X + 0.5f, Y + 0.5f);
            BaseHeight(W.X, W.Y, bWet, Water);
            const int32 Cell = CellIndex(X, Y);
            const float A = Heights[CornerIndex(X, Y)];
            const float B = Heights[CornerIndex(X + 1, Y)];
            const float C = Heights[CornerIndex(X, Y + 1)];
            const float D = Heights[CornerIndex(X + 1, Y + 1)];
            const float Low = FMath::Min(FMath::Min(A, B), FMath::Min(C, D));
            if (bWet && Water > Low + 10.0f)
            {
                WaterCell[Cell] = 1;
                WaterLevel[Cell] = Water;
            }
            Slope[Cell] = (FMath::Max(FMath::Max(A, B), FMath::Max(C, D)) - Low) / CellSize;
            Sum += (A + B + C + D) * 0.25f;
            const FVector2D M(W.X / 100.0f, W.Y / 100.0f);
            Topsoil[Cell] = 22.0f + 18.0f * (0.5f + 0.5f * FMath::PerlinNoise2D(M * 0.07f));
            Subsoil[Cell] = 180.0f + 120.0f * (0.5f + 0.5f * FMath::PerlinNoise2D((M + FVector2D(50.0f, 0.0f)) * 0.03f));
        }
    }
    const float Mean = Sum / FMath::Max(1, Side * Side);

    TArray<uint8> NearWater;
    NearWater.Init(0, Side * Side);
    for (int32 Y = 0; Y < Side; ++Y)
    {
        for (int32 X = 0; X < Side; ++X)
        {
            if (!WaterCell[CellIndex(X, Y)])
            {
                continue;
            }
            for (int32 Dy = -3; Dy <= 3; ++Dy)
            {
                for (int32 Dx = -3; Dx <= 3; ++Dx)
                {
                    const int32 Nx = X + Dx;
                    const int32 Ny = Y + Dy;
                    if (Nx >= 0 && Ny >= 0 && Nx < Side && Ny < Side)
                    {
                        NearWater[CellIndex(Nx, Ny)] = 1;
                    }
                }
            }
        }
    }

    for (int32 Y = 0; Y < Side; ++Y)
    {
        for (int32 X = 0; X < Side; ++X)
        {
            const int32 Cell = CellIndex(X, Y);
            const float Centre = (Heights[CornerIndex(X, Y)] + Heights[CornerIndex(X + 1, Y + 1)]) * 0.5f;
            const FVector2D W = WorldOf(X + 0.5f, Y + 0.5f);
            const float Patch = FMath::PerlinNoise2D(FVector2D(W.X, W.Y) / 100.0f * 0.05f + FVector2D(7.3f, 1.9f));
            ESurface Kind = ESurface::Grass;
            if (WaterCell[Cell] || NearWater[Cell])
            {
                Kind = ESurface::SandGround;
            }
            else if (Slope[Cell] > 0.8f)
            {
                Kind = Centre > Mean + 900.0f ? ESurface::GraniteGround : ESurface::RockGround;
            }
            else if (Centre < Mean - 120.0f && Patch > 0.3f)
            {
                Kind = ESurface::PeatGround;
            }
            else if (Centre < Mean && Patch < -0.35f)
            {
                Kind = ESurface::ClayGround;
            }
            Surface[Cell] = Kind;

            const FSubstance& S = FMatter::Of(SurfaceKind(Cell));
            Moisture[Cell] = S.IsSoil() ? S.PlasticLimit * (NearWater[Cell] ? 0.9f : 0.6f) : 0.03f;
            if (Kind == ESurface::PeatGround)
            {
                Moisture[Cell] = S.PlasticLimit * 1.1f;
            }
        }
    }

    bBuilt = false;
    EnsureVisuals();

    int32 Wet = 0;
    for (uint8 Flag : WaterCell)
    {
        Wet += Flag;
    }
    UE_LOG(LogHumanCity, Warning, TEXT("Земля: %d×%d м, клетка %.0f см, воды %d клеток, средняя высота %.0f см"),
        FMath::RoundToInt(Side * CellSize / 100.0f), FMath::RoundToInt(Side * CellSize / 100.0f), CellSize, Wet, Mean);
}

UMaterialInterface* ATerrainGrid::GroundLook() const
{
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        if (UMaterialInterface* Look = Matter->GroundMaterial())
        {
            return Look;
        }
    }
    return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Matter/M_Ground.M_Ground"));
}

UProceduralMeshComponent* ATerrainGrid::MakeMesh(bool bCollision)
{
    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
    Mesh->SetupAttachment(Root);
    Mesh->bUseAsyncCooking = false;
    Mesh->bUseComplexAsSimpleCollision = true;
    if (bCollision)
    {
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    }
    else
    {
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->RegisterComponent();
    return Mesh;
}

void ATerrainGrid::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    EnsureVisuals();
}

void ATerrainGrid::BeginPlay()
{
    Super::BeginPlay();
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Matter->SetTerrain(this);
    }
    EnsureVisuals();
}

void ATerrainGrid::EnsureVisuals()
{
    if (Heights.Num() != (Side + 1) * (Side + 1))
    {
        return;
    }
    ChunksPerSide = FMath::DivideAndRoundUp(Side, ChunkCells);
    const int32 Wanted = ChunksPerSide * ChunksPerSide;

    bool bValid = bBuilt && Chunks.Num() == Wanted;
    for (const TObjectPtr<UProceduralMeshComponent>& Chunk : Chunks)
    {
        bValid &= IsValid(Chunk) && Chunk->IsRegistered();
    }
    if (bValid)
    {
        return;
    }

    for (const TObjectPtr<UProceduralMeshComponent>& Chunk : Chunks)
    {
        if (IsValid(Chunk))
        {
            Chunk->DestroyComponent();
        }
    }
    Chunks.Reset();
    ChunkDirty.Init(0, Wanted);
    ChunkShown.Init(-1.0f, Wanted);

    for (int32 Cy = 0; Cy < ChunksPerSide; ++Cy)
    {
        for (int32 Cx = 0; Cx < ChunksPerSide; ++Cx)
        {
            Chunks.Add(MakeMesh(true));
            BuildChunk(Cx, Cy, true);
            Chunks.Last()->bUseAsyncCooking = true;
        }
    }

    if (IsValid(WaterMesh))
    {
        WaterMesh->DestroyComponent();
    }
    WaterMesh = nullptr;
    if (IsValid(FarMesh))
    {
        FarMesh->DestroyComponent();
    }
    FarMesh = nullptr;
    BuildWater();
    BuildFar();
    HideLandscapes();
    bBuilt = true;
}

void ATerrainGrid::HideLandscapes()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
    {
        if (!It->IsHidden())
        {
            It->SetActorHiddenInGame(true);
            It->SetActorEnableCollision(false);
        }
    }
}

float ATerrainGrid::CornerHeight(int32 X, int32 Y) const
{
    const int32 Cx = FMath::Clamp(X, 0, Side);
    const int32 Cy = FMath::Clamp(Y, 0, Side);
    return Heights[CornerIndex(Cx, Cy)];
}

EResourceKind ATerrainGrid::SurfaceKind(int32 Cell) const
{
    switch (Surface.IsValidIndex(Cell) ? Surface[Cell] : ESurface::Grass)
    {
    case ESurface::ClayGround:    return EResourceKind::Clay;
    case ESurface::SandGround:    return EResourceKind::Sand;
    case ESurface::RockGround:    return EResourceKind::Limestone;
    case ESurface::GraniteGround: return EResourceKind::Granite;
    case ESurface::PeatGround:    return EResourceKind::Peat;
    default:                      return EResourceKind::Soil;
    }
}

EResourceKind ATerrainGrid::DeepKind(int32 X, int32 Y, float DepthCm) const
{
    const int32 Cell = CellIndex(X, Y);
    const uint8 Kind = Surface[Cell];
    if ((Kind == ESurface::RockGround || Kind == ESurface::GraniteGround) && DepthCm < 400.0f)
    {
        return Kind == ESurface::RockGround ? EResourceKind::Limestone : EResourceKind::Granite;
    }
    if (Kind == ESurface::SandGround && DepthCm < 120.0f)
    {
        return EResourceKind::Sand;
    }
    if (Kind == ESurface::PeatGround && DepthCm < 90.0f)
    {
        return EResourceKind::Peat;
    }
    if (DepthCm < Topsoil[Cell])
    {
        return EResourceKind::Soil;
    }
    const int32 Mix = Salt(X, Y);
    if (DepthCm < Subsoil[Cell])
    {
        return (Mix % 5 == 0 && DepthCm > Subsoil[Cell] - 60.0f) ? EResourceKind::Sand : EResourceKind::Clay;
    }
    if (DepthCm < 600.0f)
    {
        return (Mix % 11 == 0) ? EResourceKind::Chalk : EResourceKind::Limestone;
    }
    if (Mix % 23 == 0)
    {
        return EResourceKind::IronOre;
    }
    if (Mix % 31 == 0)
    {
        return EResourceKind::Coal;
    }
    if (Mix % 41 == 0)
    {
        return EResourceKind::CopperOre;
    }
    return EResourceKind::Granite;
}

bool ATerrainGrid::CellOf(const FVector& World, int32& OutX, int32& OutY) const
{
    if (Heights.Num() == 0 || Surface.Num() != Side * Side)
    {
        return false;
    }
    const FVector Local = World - GetActorLocation();
    OutX = FMath::FloorToInt(Local.X / CellSize + Side * 0.5f);
    OutY = FMath::FloorToInt(Local.Y / CellSize + Side * 0.5f);
    return OutX >= 0 && OutY >= 0 && OutX < Side && OutY < Side;
}

float ATerrainGrid::HeightAt(const FVector& World) const
{
    const FVector Base = GetActorLocation();
    if (Heights.Num() == 0)
    {
        return Base.Z;
    }
    const FVector Local = World - Base;
    const float Gx = Local.X / CellSize + Side * 0.5f;
    const float Gy = Local.Y / CellSize + Side * 0.5f;
    if (Gx < 0.0f || Gy < 0.0f || Gx > Side || Gy > Side)
    {
        bool bWet = false;
        float Water = 0.0f;
        return Base.Z + BaseHeight(World.X, World.Y, bWet, Water);
    }
    const int32 X = FMath::Clamp(FMath::FloorToInt(Gx), 0, Side - 1);
    const int32 Y = FMath::Clamp(FMath::FloorToInt(Gy), 0, Side - 1);
    const float Fx = Gx - X;
    const float Fy = Gy - Y;
    const float A = Heights[CornerIndex(X, Y)];
    const float B = Heights[CornerIndex(X + 1, Y)];
    const float C = Heights[CornerIndex(X, Y + 1)];
    const float D = Heights[CornerIndex(X + 1, Y + 1)];
    const float H = Fx + Fy <= 1.0f
        ? A + (B - A) * Fx + (C - A) * Fy
        : D + (C - D) * (1.0f - Fx) + (B - D) * (1.0f - Fy);
    return Base.Z + H;
}

FVector ATerrainGrid::NormalAt(const FVector& World) const
{
    const float Step = CellSize;
    const float Left = HeightAt(World - FVector(Step, 0.0f, 0.0f));
    const float Right = HeightAt(World + FVector(Step, 0.0f, 0.0f));
    const float Back = HeightAt(World - FVector(0.0f, Step, 0.0f));
    const float Front = HeightAt(World + FVector(0.0f, Step, 0.0f));
    return FVector(Left - Right, Back - Front, 2.0f * Step).GetSafeNormal();
}

EResourceKind ATerrainGrid::SurfaceAt(const FVector& World) const
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y))
    {
        return EResourceKind::Soil;
    }
    const int32 Cell = CellIndex(X, Y);
    if (WaterCell[Cell])
    {
        return EResourceKind::Water;
    }
    const float Dug = FMath::Max(0.0f, ((Original[CornerIndex(X, Y)] + Original[CornerIndex(X + 1, Y + 1)])
        - (Heights[CornerIndex(X, Y)] + Heights[CornerIndex(X + 1, Y + 1)])) * 0.5f);
    return Dug > 5.0f ? DeepKind(X, Y, Dug) : SurfaceKind(Cell);
}

EResourceKind ATerrainGrid::SubstanceAtDepth(const FVector& World, float DepthCm) const
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y))
    {
        return EResourceKind::Soil;
    }
    return DeepKind(X, Y, FMath::Max(0.0f, DepthCm));
}

bool ATerrainGrid::IsWaterAt(const FVector& World) const
{
    int32 X = 0;
    int32 Y = 0;
    return CellOf(World, X, Y) && WaterCell[CellIndex(X, Y)] != 0;
}

bool ATerrainGrid::IsGrassAt(const FVector& World) const
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y))
    {
        return false;
    }
    const int32 Cell = CellIndex(X, Y);
    return Surface[Cell] == ESurface::Grass && !WaterCell[Cell] && Trodden[Cell] < 0.35f && Slope[Cell] < 0.7f;
}

float ATerrainGrid::RoughnessAt(const FVector& World) const
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y))
    {
        return 1.0f;
    }
    const int32 Cell = CellIndex(X, Y);
    if (WaterCell[Cell])
    {
        return 1.3f;
    }
    float Rough = 0.8f;
    switch (Surface[Cell])
    {
    case ESurface::Tilled:        Rough = 2.6f; break;
    case ESurface::RockGround:    Rough = 2.0f; break;
    case ESurface::GraniteGround: Rough = 1.5f; break;
    case ESurface::Grass:         Rough = 1.1f; break;
    case ESurface::PeatGround:    Rough = 1.0f; break;
    case ESurface::ClayGround:    Rough = 0.9f; break;
    case ESurface::SandGround:    Rough = 0.7f; break;
    default: break;
    }
    Rough *= FMath::Lerp(1.0f, 0.6f, FMath::Clamp(Trodden[Cell], 0.0f, 1.0f));
    Rough += FMath::Clamp(Slope[Cell], 0.0f, 1.5f) * 0.5f;
    if (SnowDepth[Cell] > 5.0f)
    {
        Rough = FMath::Lerp(Rough, 0.5f, FMath::Clamp(SnowDepth[Cell] / 30.0f, 0.0f, 1.0f));
    }
    return Rough;
}

float ATerrainGrid::WaterLevelAt(const FVector& World) const
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y) || !WaterCell[CellIndex(X, Y)])
    {
        return -1.0e6f;
    }
    return GetActorLocation().Z + WaterLevel[CellIndex(X, Y)];
}

float ATerrainGrid::MoistureAt(const FVector& World) const
{
    int32 X = 0;
    int32 Y = 0;
    return CellOf(World, X, Y) ? Moisture[CellIndex(X, Y)] : 0.1f;
}

float ATerrainGrid::CellWetness(int32 Cell) const
{
    if (WaterCell[Cell])
    {
        return 1.0f;
    }
    const FSubstance& S = FMatter::Of(SurfaceKind(Cell));
    const float Wet = FMatter::WetFraction(S, Moisture[Cell]);
    return FMath::Clamp(Wet * 0.85f + Puddle[Cell] / 12.0f, 0.0f, 1.0f);
}

bool ATerrainGrid::FootingAt(const FVector& World, float BodyKg, FTerrainFooting& Out) const
{
    Out = FTerrainFooting();
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y))
    {
        return false;
    }
    const int32 Cell = CellIndex(X, Y);
    const float Pressure = FMath::Max(10.0f, BodyKg) * FMatter::Gravity * 1.2f / 0.02f / 1000.0f;

    if (WaterCell[Cell])
    {
        const float Bottom = (Heights[CornerIndex(X, Y)] + Heights[CornerIndex(X + 1, Y + 1)]) * 0.5f;
        const float Depth = FMath::Max(0.0f, WaterLevel[Cell] - Bottom);
        Out.WaterCm = Depth;
        Out.SinkCm = 3.0f;
        Out.Grip = 0.38f;
        Out.Effort = 1.0f + 0.03f * Depth + 0.0006f * Depth * Depth + 0.2f;
        Out.Stick = 0.1f;
        Out.Strength = 20.0f;
        Out.bSoft = true;
        return true;
    }

    const FSubstance& S = FMatter::Of(SurfaceKind(Cell));
    const float M = Moisture[Cell];
    const bool bFrozen = SoilIce[Cell] > 0.5f;
    const uint8 Kind = Surface[Cell];
    Out.Grip = FMatter::FrictionOf(S, M, SoilTemperature[Cell], false);

    float Sink = 0.0f;
    float Stick = 0.0f;
    float Extra = 0.0f;
    float Strength = 400.0f;

    if (!bFrozen && S.IsSoil())
    {
        const float Liquidity = (M - S.PlasticLimit) / FMath::Max(0.01f, S.LiquidLimit - S.PlasticLimit);
        Strength = 170.0f * FMath::Exp(-4.6f * FMath::Clamp(Liquidity, -0.6f, 1.6f));
        const float Bearing = Strength * 5.14f * 1.077f;
        const float Layer = FMath::Clamp(Topsoil[Cell] * FMath::Clamp(0.3f + Liquidity, 0.15f, 1.3f) + Puddle[Cell] * 0.1f, 1.0f, 60.0f);
        if (Bearing < Pressure)
        {
            Sink = FMath::Min(Layer, (Pressure - Bearing) / (2.0f * Bearing + 17.0f) * 100.0f);
        }
        Sink += Pressure * 0.1f / (300.0f * FMath::Max(Strength, 1.0f)) * 100.0f;

        const float Clayey = Kind == ESurface::ClayGround ? 1.0f : (Kind == ESurface::PeatGround ? 0.55f : 0.75f);
        Stick = FMath::Clamp((Liquidity - 0.1f) / 0.45f, 0.0f, 1.0f) * FMath::Clamp((1.35f - Liquidity) / 0.45f, 0.0f, 1.0f) * Clayey;

        if (Kind == ESurface::Grass)
        {
            Sink *= 0.55f;
            Stick *= 0.35f;
        }
        else if (Kind == ESurface::Tilled)
        {
            Sink += 2.5f;
            Extra += 0.25f;
        }
        if (Trodden[Cell] > 0.5f && Liquidity < 0.3f)
        {
            Sink *= 0.5f;
        }
        Out.bMud = Liquidity > 0.35f && Kind != ESurface::Grass;
    }
    else if (!bFrozen && Kind == ESurface::SandGround)
    {
        const float Wet = FMatter::WetFraction(S, M);
        Sink = Wet > 0.85f ? 4.0f : FMath::Lerp(3.2f, 0.7f, FMath::Clamp(Wet * 2.5f, 0.0f, 1.0f));
        Extra += FMath::Lerp(0.75f, 0.15f, FMath::Clamp(Wet * 2.5f, 0.0f, 1.0f));
        Strength = 60.0f;
    }

    Out.SnowCm = SnowDepth[Cell] * 0.3f;
    if (Out.SnowCm > 1.0f)
    {
        Sink += Out.SnowCm * 0.7f;
        Extra += 0.3f + 0.06f * Out.SnowCm;
    }
    if (Puddle[Cell] > 5.0f)
    {
        Out.WaterCm = Puddle[Cell] * 0.1f;
    }

    if (bFrozen && (Puddle[Cell] > 2.0f || (S.IsSoil() && M > S.PlasticLimit)))
    {
        Out.Grip = 0.08f;
    }
    if (Sink > 5.0f)
    {
        Out.Grip = FMath::Max(Out.Grip, 0.35f);
    }

    Out.SinkCm = Sink;
    Out.Stick = Stick;
    Out.Strength = Strength;
    Out.Effort = 1.0f + 0.08f * Sink + 0.003f * Sink * Sink + Stick * 0.6f + Extra;
    Out.bSoft = Sink > 0.8f || Stick > 0.2f || Out.SnowCm > 1.0f;
    return true;
}

float ATerrainGrid::WalkFactorAt(const FVector& World, float& OutGrip) const
{
    FTerrainFooting Footing;
    OutGrip = 0.7f;
    if (!FootingAt(World, 70.0f, Footing))
    {
        return 1.0f;
    }
    OutGrip = Footing.Grip;
    return FMath::Pow(1.0f / FMath::Max(1.0f, Footing.Effort), 0.8f);
}

float ATerrainGrid::ComplianceAt(const FVector& World) const
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y))
    {
        return 1.0f / 0.02f;
    }
    const int32 Cell = CellIndex(X, Y);
    if (WaterCell[Cell])
    {
        return 1000.0f;
    }
    const FSubstance& S = FMatter::Of(SurfaceKind(Cell));
    return FMatter::Compliance(S, Moisture[Cell], SoilTemperature[Cell]);
}

void ATerrainGrid::AddWater(const FVector& World, float Kilograms, float RadiusCm)
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y) || Kilograms <= 0.0f)
    {
        return;
    }
    const int32 Reach = FMath::Max(0, FMath::CeilToInt(RadiusCm / CellSize));
    const int32 Count = (Reach * 2 + 1) * (Reach * 2 + 1);
    const float PerCell = Kilograms / Count / FMath::Square(CellSize / 100.0f);
    for (int32 Dy = -Reach; Dy <= Reach; ++Dy)
    {
        for (int32 Dx = -Reach; Dx <= Reach; ++Dx)
        {
            const int32 Nx = X + Dx;
            const int32 Ny = Y + Dy;
            if (Nx >= 0 && Ny >= 0 && Nx < Side && Ny < Side)
            {
                Puddle[CellIndex(Nx, Ny)] += PerCell;
                MarkDirty(Nx, Ny, false);
            }
        }
    }
}

void ATerrainGrid::SoakArea(const FVector& Centre, float RadiusCm, float Liquidity, bool bBare)
{
    if (Heights.Num() == 0)
    {
        return;
    }
    const FVector Local = Centre - GetActorLocation();
    const int32 Reach = FMath::CeilToInt(RadiusCm / CellSize);
    const int32 Cx = FMath::FloorToInt(Local.X / CellSize + Side * 0.5f);
    const int32 Cy = FMath::FloorToInt(Local.Y / CellSize + Side * 0.5f);
    for (int32 Dy = -Reach; Dy <= Reach; ++Dy)
    {
        for (int32 Dx = -Reach; Dx <= Reach; ++Dx)
        {
            const int32 X = Cx + Dx;
            const int32 Y = Cy + Dy;
            if (X < 0 || Y < 0 || X >= Side || Y >= Side || FMath::Square(Dx) + FMath::Square(Dy) > Reach * Reach)
            {
                continue;
            }
            const int32 Cell = CellIndex(X, Y);
            if (WaterCell[Cell])
            {
                continue;
            }
            if (bBare && Surface[Cell] == ESurface::Grass)
            {
                Surface[Cell] = ESurface::Bare;
            }
            const FSubstance& S = FMatter::Of(SurfaceKind(Cell));
            if (S.IsSoil())
            {
                Moisture[Cell] = S.PlasticLimit + (S.LiquidLimit - S.PlasticLimit) * Liquidity;
            }
            else
            {
                Moisture[Cell] = FMatter::WaterLimit(S) * FMath::Clamp(Liquidity, 0.0f, 1.0f);
            }
            Trodden[Cell] = FMath::Min(Trodden[Cell], 0.2f);
            Puddle[Cell] = FMath::Max(Puddle[Cell], Liquidity > 0.8f ? 6.0f : 0.0f);
            MarkDirty(X, Y, false);
        }
    }
}

void ATerrainGrid::Trample(const FVector& World, float Kilograms)
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y))
    {
        return;
    }
    const int32 Cell = CellIndex(X, Y);
    const float Before = Trodden[Cell];
    Trodden[Cell] = FMath::Min(1.0f, Trodden[Cell] + 0.0006f * Kilograms / 70.0f);
    if (FMath::FloorToInt(Before * 20.0f) != FMath::FloorToInt(Trodden[Cell] * 20.0f))
    {
        MarkDirty(X, Y, false);
    }
}

void ATerrainGrid::Footprint(const FVector& World, float Yaw, float DepthCm, float Stick, bool bLeft)
{
    UWorld* World0 = GetWorld();
    if (!World0 || !World0->IsGameWorld())
    {
        return;
    }
    if (!bPrintsTried)
    {
        bPrintsTried = true;
        if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Matter/M_Footprint.M_Footprint")))
        {
            for (int32 Level = 0; Level < 4; ++Level)
            {
                UMaterialInstanceDynamic* Look = UMaterialInstanceDynamic::Create(Base, this);
                Look->SetScalarParameterValue(TEXT("Depth"), 0.25f + Level * 0.25f);
                PrintLooks.Add(Look);
            }
        }
    }
    if (PrintLooks.Num() == 0)
    {
        return;
    }

    const float Strength = FMath::Clamp(DepthCm / 8.0f + Stick * 0.5f, 0.0f, 1.0f);
    const int32 Level = FMath::Clamp(FMath::FloorToInt(Strength * 4.0f), 0, 3);
    const FVector Ground(World.X, World.Y, HeightAt(World));

    UDecalComponent* Print = nullptr;
    if (Prints.Num() < PrintLimit)
    {
        Print = NewObject<UDecalComponent>(this, NAME_None, RF_Transient);
        Print->SetupAttachment(Root);
        Print->RegisterComponent();
        Prints.Add(Print);
    }
    else
    {
        NextPrint = NextPrint % PrintLimit;
        Print = Prints[NextPrint++];
    }
    if (!IsValid(Print))
    {
        return;
    }
    Print->SetDecalMaterial(PrintLooks[Level]);
    Print->DecalSize = FVector(12.0f + DepthCm, 5.5f, 14.0f);
    Print->SetWorldLocationAndRotation(Ground, FRotator(-90.0f, Yaw, bLeft ? 0.0f : 180.0f));
    Print->SetFadeScreenSize(0.002f);
    Print->SetFadeOut(420.0f, 240.0f, false);
    Print->MarkRenderStateDirty();
}

void ATerrainGrid::Impact(const FVector& World, float Joules)
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y) || Joules < 20.0f)
    {
        return;
    }
    const int32 Cell = CellIndex(X, Y);
    const FSubstance& S = FMatter::Of(SurfaceKind(Cell));
    const EMatterPhase Phase = FMatter::PhaseOf(S, Moisture[Cell], SoilTemperature[Cell], false, 0.0f, 0.0f);
    if (Phase != EMatterPhase::Plastic && Phase != EMatterPhase::Slurry)
    {
        return;
    }
    const float Dent = FMath::Min(6.0f, Joules / 250.0f);
    const FVector Local = World - GetActorLocation();
    const int32 Cx = FMath::Clamp(FMath::RoundToInt(Local.X / CellSize + Side * 0.5f), 0, Side);
    const int32 Cy = FMath::Clamp(FMath::RoundToInt(Local.Y / CellSize + Side * 0.5f), 0, Side);
    float& Corner = Heights[CornerIndex(Cx, Cy)];
    Corner = FMath::Max(Original[CornerIndex(Cx, Cy)] - 25.0f, Corner - Dent);
    Trodden[Cell] = FMath::Min(1.0f, Trodden[Cell] + 0.05f);
    MarkDirty(X, Y, true);
}

EResourceKind ATerrainGrid::Dig(const FVector& World, float DepthCm, float& OutCubicMetres)
{
    OutCubicMetres = 0.0f;
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y) || DepthCm <= 0.0f)
    {
        return EResourceKind::None;
    }
    const int32 Cell = CellIndex(X, Y);
    const FVector Local = World - GetActorLocation();
    const int32 Cx = FMath::Clamp(FMath::RoundToInt(Local.X / CellSize + Side * 0.5f), 1, Side - 1);
    const int32 Cy = FMath::Clamp(FMath::RoundToInt(Local.Y / CellSize + Side * 0.5f), 1, Side - 1);
    float& Corner = Heights[CornerIndex(Cx, Cy)];
    const float Dug = FMath::Max(0.0f, Original[CornerIndex(Cx, Cy)] - Corner);
    const float Take = FMath::Min(DepthCm, 800.0f - Dug);
    if (Take <= 0.0f)
    {
        return EResourceKind::None;
    }
    const EResourceKind Taken = WaterCell[Cell] ? EResourceKind::Sand : DeepKind(X, Y, Dug + Take * 0.5f);
    Corner -= Take;
    OutCubicMetres = Take / 100.0f * FMath::Square(CellSize / 100.0f);
    for (int32 Dy = -1; Dy <= 0; ++Dy)
    {
        for (int32 Dx = -1; Dx <= 0; ++Dx)
        {
            const int32 Around = CellIndex(Cx + Dx, Cy + Dy);
            if (Surface[Around] == ESurface::Grass)
            {
                Surface[Around] = ESurface::Bare;
            }
            MarkDirty(Cx + Dx, Cy + Dy, true);
        }
    }
    return Taken;
}

bool ATerrainGrid::Place(const FVector& World, EResourceKind Substance)
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y))
    {
        return false;
    }
    for (int32 Dy = 0; Dy <= 1; ++Dy)
    {
        for (int32 Dx = 0; Dx <= 1; ++Dx)
        {
            Heights[CornerIndex(X + Dx, Y + Dy)] += 25.0f;
        }
    }
    const int32 Cell = CellIndex(X, Y);
    switch (Substance)
    {
    case EResourceKind::Sand:      Surface[Cell] = ESurface::SandGround; break;
    case EResourceKind::Clay:      Surface[Cell] = ESurface::ClayGround; break;
    case EResourceKind::Peat:      Surface[Cell] = ESurface::PeatGround; break;
    case EResourceKind::Stone:
    case EResourceKind::Limestone: Surface[Cell] = ESurface::RockGround; break;
    case EResourceKind::Granite:   Surface[Cell] = ESurface::GraniteGround; break;
    default:                       Surface[Cell] = ESurface::Bare; break;
    }
    MarkDirty(X, Y, true);
    return true;
}

void ATerrainGrid::Flatten(const FVector& Centre, const FVector2D& HalfExtent, float Margin)
{
    if (Heights.Num() == 0)
    {
        return;
    }
    const float Target = HeightAt(Centre) - GetActorLocation().Z;
    const FVector Local = Centre - GetActorLocation();
    const int32 X0 = FMath::Clamp(FMath::FloorToInt((Local.X - HalfExtent.X - Margin) / CellSize + Side * 0.5f), 0, Side);
    const int32 X1 = FMath::Clamp(FMath::CeilToInt((Local.X + HalfExtent.X + Margin) / CellSize + Side * 0.5f), 0, Side);
    const int32 Y0 = FMath::Clamp(FMath::FloorToInt((Local.Y - HalfExtent.Y - Margin) / CellSize + Side * 0.5f), 0, Side);
    const int32 Y1 = FMath::Clamp(FMath::CeilToInt((Local.Y + HalfExtent.Y + Margin) / CellSize + Side * 0.5f), 0, Side);

    for (int32 Y = Y0; Y <= Y1; ++Y)
    {
        for (int32 X = X0; X <= X1; ++X)
        {
            const float Px = (X - Side * 0.5f) * CellSize - Local.X;
            const float Py = (Y - Side * 0.5f) * CellSize - Local.Y;
            const float Outside = FMath::Max(FMath::Abs(Px) - HalfExtent.X, FMath::Abs(Py) - HalfExtent.Y);
            const float Blend = Outside <= 0.0f ? 1.0f : 1.0f - Smooth(Outside / FMath::Max(1.0f, Margin));
            float& Corner = Heights[CornerIndex(X, Y)];
            Corner = FMath::Lerp(Corner, Target, Blend);
            Original[CornerIndex(X, Y)] = Corner;
            if (X < Side && Y < Side && Outside <= 0.0f)
            {
                const int32 Cell = CellIndex(X, Y);
                if (Surface[Cell] == ESurface::Grass)
                {
                    Surface[Cell] = ESurface::Bare;
                }
                Trodden[Cell] = FMath::Max(Trodden[Cell], 0.6f);
                WaterCell[Cell] = 0;
            }
            MarkDirty(FMath::Min(X, Side - 1), FMath::Min(Y, Side - 1), true);
        }
    }
}

void ATerrainGrid::MarkSurface(const FVector& Centre, const FVector2D& HalfExtent, float Yaw, EResourceKind Substance, float TroddenLevel)
{
    if (Heights.Num() == 0)
    {
        return;
    }
    const FVector Local = Centre - GetActorLocation();
    const float Reach = HalfExtent.Size();
    const int32 X0 = FMath::Clamp(FMath::FloorToInt((Local.X - Reach) / CellSize + Side * 0.5f), 0, Side - 1);
    const int32 X1 = FMath::Clamp(FMath::CeilToInt((Local.X + Reach) / CellSize + Side * 0.5f), 0, Side - 1);
    const int32 Y0 = FMath::Clamp(FMath::FloorToInt((Local.Y - Reach) / CellSize + Side * 0.5f), 0, Side - 1);
    const int32 Y1 = FMath::Clamp(FMath::CeilToInt((Local.Y + Reach) / CellSize + Side * 0.5f), 0, Side - 1);
    const FRotator Turn(0.0f, -Yaw, 0.0f);

    ESurface Kind = ESurface::Bare;
    switch (Substance)
    {
    case EResourceKind::Sand:  Kind = ESurface::SandGround; break;
    case EResourceKind::Clay:  Kind = ESurface::ClayGround; break;
    case EResourceKind::Peat:  Kind = ESurface::PeatGround; break;
    case EResourceKind::Grain: Kind = ESurface::Tilled; break;
    case EResourceKind::Herb:  Kind = ESurface::Grass; break;
    default:                   Kind = ESurface::Bare; break;
    }

    for (int32 Y = Y0; Y <= Y1; ++Y)
    {
        for (int32 X = X0; X <= X1; ++X)
        {
            const FVector Offset((X + 0.5f - Side * 0.5f) * CellSize - Local.X, (Y + 0.5f - Side * 0.5f) * CellSize - Local.Y, 0.0f);
            const FVector Inside = Turn.RotateVector(Offset);
            if (FMath::Abs(Inside.X) > HalfExtent.X || FMath::Abs(Inside.Y) > HalfExtent.Y)
            {
                continue;
            }
            const int32 Cell = CellIndex(X, Y);
            if (WaterCell[Cell])
            {
                continue;
            }
            Surface[Cell] = Kind;
            Trodden[Cell] = TroddenLevel;
            MarkDirty(X, Y, false);
        }
    }
}

void ATerrainGrid::MarkDirty(int32 X, int32 Y, bool bShape)
{
    if (ChunksPerSide <= 0 || ChunkDirty.Num() != ChunksPerSide * ChunksPerSide)
    {
        return;
    }
    auto Mark = [this, bShape](int32 Cx, int32 Cy)
    {
        if (Cx < 0 || Cy < 0 || Cx >= ChunksPerSide || Cy >= ChunksPerSide)
        {
            return;
        }
        uint8& Flag = ChunkDirty[Cy * ChunksPerSide + Cx];
        Flag = FMath::Max<uint8>(Flag, bShape ? 2 : 1);
    };
    const int32 Cx = X / ChunkCells;
    const int32 Cy = Y / ChunkCells;
    Mark(Cx, Cy);
    if (X % ChunkCells == 0)
    {
        Mark(Cx - 1, Cy);
    }
    if (Y % ChunkCells == 0)
    {
        Mark(Cx, Cy - 1);
    }
    if (X % ChunkCells == 0 && Y % ChunkCells == 0)
    {
        Mark(Cx - 1, Cy - 1);
    }
}

void ATerrainGrid::RebuildDirty()
{
    if (!bBuilt)
    {
        EnsureVisuals();
        return;
    }
    for (int32 Cy = 0; Cy < ChunksPerSide; ++Cy)
    {
        for (int32 Cx = 0; Cx < ChunksPerSide; ++Cx)
        {
            const uint8 Flag = ChunkDirty[Cy * ChunksPerSide + Cx];
            if (Flag > 0)
            {
                BuildChunk(Cx, Cy, Flag > 1);
            }
        }
    }
    BuildWater();
}

void ATerrainGrid::CornerLook(int32 X, int32 Y, FLinearColor& OutColour, FVector2D& OutExtra) const
{
    float Bare = 0.0f;
    float Sand = 0.0f;
    float Rock = 0.0f;
    float Wet = 0.0f;
    float Snow = 0.0f;
    float Clay = 0.0f;
    int32 Count = 0;
    for (int32 Dy = -1; Dy <= 0; ++Dy)
    {
        for (int32 Dx = -1; Dx <= 0; ++Dx)
        {
            const int32 Cx = X + Dx;
            const int32 Cy = Y + Dy;
            if (Cx < 0 || Cy < 0 || Cx >= Side || Cy >= Side)
            {
                continue;
            }
            const int32 Cell = CellIndex(Cx, Cy);
            const uint8 Kind = Surface[Cell];
            const FSubstance& S = FMatter::Of(SurfaceKind(Cell));
            const bool bMud = S.IsSoil() && Moisture[Cell] > S.PlasticLimit;
            float B = (Kind == ESurface::Bare || Kind == ESurface::ClayGround || Kind == ESurface::PeatGround || Kind == ESurface::Tilled) ? 1.0f : 0.0f;
            B = FMath::Max(B, Smooth((Trodden[Cell] - 0.2f) / 0.6f));
            if (bMud && Kind == ESurface::Grass)
            {
                B = FMath::Max(B, 0.35f * Smooth(Trodden[Cell] * 3.0f));
            }
            Bare += B;
            Sand += (Kind == ESurface::SandGround) ? 1.0f : 0.0f;
            Rock += (Kind == ESurface::RockGround || Kind == ESurface::GraniteGround) ? 1.0f : 0.0f;
            Wet += CellWetness(Cell);
            Snow += FMath::Clamp(SnowDepth[Cell] / 15.0f, 0.0f, 1.0f);
            Clay += Kind == ESurface::ClayGround ? 1.0f : (Kind == ESurface::PeatGround ? -0.5f : (Kind == ESurface::Tilled ? -0.2f : 0.0f));
            ++Count;
        }
    }
    const float Inv = 1.0f / FMath::Max(1, Count);
    OutColour = FLinearColor(Bare * Inv, Sand * Inv, Rock * Inv, Wet * Inv);
    OutExtra = FVector2D(Snow * Inv, FMath::Clamp(Clay * Inv, 0.0f, 1.0f));
}

void ATerrainGrid::BuildChunk(int32 ChunkX, int32 ChunkY, bool bShape)
{
    const int32 ChunkId = ChunkY * ChunksPerSide + ChunkX;
    if (!Chunks.IsValidIndex(ChunkId) || !IsValid(Chunks[ChunkId]))
    {
        return;
    }
    UProceduralMeshComponent* Mesh = Chunks[ChunkId];
    ChunkDirty[ChunkId] = 0;

    const int32 X0 = ChunkX * ChunkCells;
    const int32 Y0 = ChunkY * ChunkCells;
    const int32 X1 = FMath::Min(X0 + ChunkCells, Side);
    const int32 Y1 = FMath::Min(Y0 + ChunkCells, Side);
    const int32 Width = X1 - X0 + 1;
    const int32 Height = Y1 - Y0 + 1;

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FVector2D> UV1;
    TArray<FVector2D> Empty;
    TArray<FLinearColor> Colours;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles;
    const int32 Reserve = Width * Height + (Width + Height) * 2;
    Vertices.Reserve(Reserve);
    Normals.Reserve(Reserve);
    UV0.Reserve(Reserve);
    UV1.Reserve(Reserve);
    Colours.Reserve(Reserve);

    float WetSum = 0.0f;
    for (int32 Y = Y0; Y <= Y1; ++Y)
    {
        for (int32 X = X0; X <= X1; ++X)
        {
            const float H = CornerHeight(X, Y);
            Vertices.Add(FVector((X - Side * 0.5f) * CellSize, (Y - Side * 0.5f) * CellSize, H));
            const float Dx = CornerHeight(X - 1, Y) - CornerHeight(X + 1, Y);
            const float Dy = CornerHeight(X, Y - 1) - CornerHeight(X, Y + 1);
            Normals.Add(FVector(Dx, Dy, 2.0f * CellSize).GetSafeNormal());
            UV0.Add(FVector2D(X, Y) * 0.25f);
            FLinearColor Colour;
            FVector2D Extra;
            CornerLook(X, Y, Colour, Extra);
            Colours.Add(Colour);
            UV1.Add(Extra);
            WetSum += Colour.A + Extra.X + Colour.R * 0.5f;
        }
    }

    for (int32 Y = 0; Y < Height - 1; ++Y)
    {
        for (int32 X = 0; X < Width - 1; ++X)
        {
            const int32 A = Y * Width + X;
            const int32 B = A + 1;
            const int32 C = A + Width;
            const int32 D = C + 1;
            Triangles.Add(A);
            Triangles.Add(C);
            Triangles.Add(B);
            Triangles.Add(B);
            Triangles.Add(C);
            Triangles.Add(D);
        }
    }

    auto Skirt = [&](int32 FromIndex, int32 Step, int32 Count)
    {
        const int32 Start = Vertices.Num();
        for (int32 i = 0; i < Count; ++i)
        {
            const int32 Source = FromIndex + i * Step;
            const FVector Low = Vertices[Source] - FVector(0.0f, 0.0f, 400.0f);
            const FVector Normal = Normals[Source];
            const FVector2D First = UV0[Source];
            const FVector2D Second = UV1[Source];
            const FLinearColor Colour = Colours[Source];
            Vertices.Add(Low);
            Normals.Add(Normal);
            UV0.Add(First);
            UV1.Add(Second);
            Colours.Add(Colour);
        }
        for (int32 i = 0; i < Count - 1; ++i)
        {
            const int32 TopA = FromIndex + i * Step;
            const int32 TopB = FromIndex + (i + 1) * Step;
            const int32 LowA = Start + i;
            const int32 LowB = Start + i + 1;
            Triangles.Append({ TopA, LowA, TopB, TopB, LowA, LowB });
            Triangles.Append({ TopA, TopB, LowA, TopB, LowB, LowA });
        }
    };
    if (ChunkY == 0)
    {
        Skirt(0, 1, Width);
    }
    if (ChunkY == ChunksPerSide - 1)
    {
        Skirt((Height - 1) * Width, 1, Width);
    }
    if (ChunkX == 0)
    {
        Skirt(0, Width, Height);
    }
    if (ChunkX == ChunksPerSide - 1)
    {
        Skirt(Width - 1, Width, Height);
    }

    ChunkShown[ChunkId] = WetSum / FMath::Max(1, Width * Height);

    if (bShape || Mesh->GetNumSections() == 0)
    {
        Mesh->ClearAllMeshSections();
        Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, UV1, Empty, Empty, Colours, Tangents, true, false);
        if (UMaterialInterface* Look = GroundLook())
        {
            Mesh->SetMaterial(0, Look);
        }
    }
    else
    {
        Mesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, UV0, UV1, Empty, Empty, Colours, Tangents, false);
    }
}

void ATerrainGrid::BuildWater()
{
    if (!IsValid(WaterMesh))
    {
        WaterMesh = MakeMesh(false);
        WaterMesh->SetCastShadow(false);
    }

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> Colours;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles;
    for (int32 Y = 0; Y < Side; ++Y)
    {
        for (int32 X = 0; X < Side; ++X)
        {
            const int32 Cell = CellIndex(X, Y);
            if (!WaterCell[Cell])
            {
                continue;
            }
            const float Z = WaterLevel[Cell];
            const int32 Start = Vertices.Num();
            for (int32 Corner = 0; Corner < 4; ++Corner)
            {
                const int32 Dx = Corner % 2;
                const int32 Dy = Corner / 2;
                Vertices.Add(FVector((X + Dx - Side * 0.5f) * CellSize, (Y + Dy - Side * 0.5f) * CellSize, Z));
                Normals.Add(FVector::UpVector);
                UV0.Add(FVector2D(X + Dx, Y + Dy));
                Colours.Add(FLinearColor::White);
            }
            Triangles.Append({ Start, Start + 2, Start + 1, Start + 1, Start + 2, Start + 3 });
        }
    }

    WaterMesh->ClearAllMeshSections();
    if (Triangles.Num() > 0)
    {
        WaterMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, Colours, Tangents, false, false);
        UMaterialInterface* Look = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Matter/M_Water.M_Water"));
        if (!Look)
        {
            if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
            {
                Look = Matter->LookOf(EResourceKind::Water);
            }
        }
        if (Look)
        {
            WaterMesh->SetMaterial(0, Look);
        }
    }
}

void ATerrainGrid::BuildFar()
{
    if (!IsValid(FarMesh))
    {
        FarMesh = MakeMesh(true);
        FarMesh->bUseAsyncCooking = true;
    }

    const FVector Base = GetActorLocation();
    const int32 Half = FMath::CeilToInt(FarReach / FarCell);
    const int32 Width = Half * 2 + 1;
    const float Core = Side * CellSize * 0.5f;

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FVector2D> UV1;
    TArray<FVector2D> Empty;
    TArray<FLinearColor> Colours;
    TArray<FProcMeshTangent> Tangents;
    TArray<int32> Triangles;
    Vertices.Reserve(Width * Width);

    auto FarHeight = [this, Core, &Base](float Lx, float Ly)
    {
        bool bWet = false;
        float Water = 0.0f;
        float H = BaseHeight(Base.X + Lx, Base.Y + Ly, bWet, Water);
        const float Beyond = FMath::Max(0.0f, FMath::Max(FMath::Abs(Lx), FMath::Abs(Ly)) - Core);
        const float Rise = FMath::Pow(Beyond / 20000.0f, 1.3f);
        const float Hills = 0.55f + 0.45f * FMath::PerlinNoise2D(FVector2D(Lx, Ly) / 45000.0f);
        return H + Rise * 2600.0f * Hills + Rise * 900.0f * FMath::PerlinNoise2D(FVector2D(Lx, Ly) / 9000.0f);
    };

    for (int32 J = -Half; J <= Half; ++J)
    {
        for (int32 I = -Half; I <= Half; ++I)
        {
            const float Lx = I * FarCell;
            const float Ly = J * FarCell;
            const float H = FarHeight(Lx, Ly);
            Vertices.Add(FVector(Lx, Ly, H));
            const float Dx = FarHeight(Lx - FarCell, Ly) - FarHeight(Lx + FarCell, Ly);
            const float Dy = FarHeight(Lx, Ly - FarCell) - FarHeight(Lx, Ly + FarCell);
            Normals.Add(FVector(Dx, Dy, 2.0f * FarCell).GetSafeNormal());
            UV0.Add(FVector2D(I, J));
            UV1.Add(FVector2D(0.0f, 0.0f));
            const float Patchy = 0.5f + 0.5f * FMath::PerlinNoise2D(FVector2D(Lx, Ly) / 7000.0f);
            Colours.Add(FLinearColor(Patchy > 0.8f ? 0.6f : 0.0f, 0.0f, 0.0f, 0.1f));
        }
    }

    for (int32 J = 0; J < Width - 1; ++J)
    {
        for (int32 I = 0; I < Width - 1; ++I)
        {
            const float MinX = (I - Half) * FarCell;
            const float MinY = (J - Half) * FarCell;
            const float MaxX = MinX + FarCell;
            const float MaxY = MinY + FarCell;
            const bool bInside = MinX >= -Core - 1.0f && MaxX <= Core + 1.0f && MinY >= -Core - 1.0f && MaxY <= Core + 1.0f;
            if (bInside)
            {
                continue;
            }
            const int32 A = J * Width + I;
            const int32 B = A + 1;
            const int32 C = A + Width;
            const int32 D = C + 1;
            Triangles.Append({ A, C, B, B, C, D });
        }
    }

    FarMesh->ClearAllMeshSections();
    FarMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, UV1, Empty, Empty, Colours, Tangents, true, false);
    if (UMaterialInterface* Look = GroundLook())
    {
        FarMesh->SetMaterial(0, Look);
    }
}

void ATerrainGrid::AdvanceSoil(float Seconds, const FClimate& Sky)
{
    if (!bBuilt || Seconds <= 0.0f || Moisture.Num() != Side * Side)
    {
        return;
    }

    const float Hours = Seconds / 3600.0f;
    const float RainMm = Sky.Rain * Hours;
    const float SnowMm = Sky.Snow * Hours;
    const float Air = Sky.Celsius;
    const float SunWarm = Sky.Sun / 1000.0f * 6.0f;
    const float Keep = FMath::Exp(-Hours / 6.0f);
    const float Melt = Air > 0.0f ? 4.0f * Air * Hours / 24.0f + Sky.Sun * Seconds * 0.2f / FMatter::LatentMelt : 0.0f;
    const float Potential = FMatter::Evaporation(Air + SunWarm, Air, Sky.Humidity, Sky.Wind, Sky.Sun) * Seconds;
    const int32 Cells = Side * Side;

    for (int32 Cell = 0; Cell < Cells; ++Cell)
    {
        float& T = SoilTemperature[Cell];
        T = (Air + SunWarm) + (T - (Air + SunWarm)) * Keep;
        if (WaterCell[Cell])
        {
            continue;
        }

        const FSubstance& S = FMatter::Of(SurfaceKind(Cell));
        const float Layer = FMath::Max(50.0f, S.Density * 0.3f);
        const bool bFrozen = T < -0.5f;

        float& Snow = SnowDepth[Cell];
        Snow += SnowMm;
        float Water = RainMm;
        if (Snow > 0.0f && Melt > 0.0f)
        {
            const float Melted = FMath::Min(Snow, Melt);
            Snow -= Melted;
            Water += Melted;
        }

        float& Pool = Puddle[Cell];
        float& M = Moisture[Cell];
        Pool += Water;
        const float Room = FMath::Max(0.0f, FMatter::WaterLimit(S) * Layer - M * Layer);
        const float Intake = bFrozen ? 0.0f : FMath::Min(Pool, FMath::Min(Room, S.Permeability * 3.6e6f * Hours));
        Pool -= Intake;
        M += Intake / Layer;
        Pool *= FMath::Exp(-Hours * Slope[Cell] * 6.0f);

        float Evaporate = Potential * (bFrozen ? 0.1f : 1.0f);
        const float FromPool = FMath::Min(Pool, Evaporate);
        Pool -= FromPool;
        Evaporate -= FromPool;
        const float Open = S.IsSoil()
            ? FMath::Clamp(M / FMath::Max(0.01f, S.PlasticLimit), 0.05f, 1.0f)
            : FMath::Clamp(M * 20.0f, 0.0f, 1.0f);
        const float FromSoil = FMath::Min(M * Layer, Evaporate * Open * (Surface[Cell] == ESurface::Grass ? 1.2f : 1.0f));
        M -= FromSoil / Layer;

        const float Field = S.IsSoil() ? S.PlasticLimit * 0.85f : FMatter::WaterLimit(S) * 0.4f;
        if (M > Field && !bFrozen)
        {
            M -= (M - Field) * FMath::Clamp(S.Permeability * 3.6e6f * Hours / 50.0f, 0.0f, 1.0f);
        }
        M = FMath::Clamp(M, 0.0f, FMatter::WaterLimit(S));
        Pool = FMath::Clamp(Pool, 0.0f, 150.0f);
        SoilIce[Cell] = bFrozen ? FMath::Min(1.0f, SoilIce[Cell] + Hours * 0.5f) : FMath::Max(0.0f, SoilIce[Cell] - Hours * 0.5f);
        Trodden[Cell] = FMath::Max(0.0f, Trodden[Cell] - Hours / (24.0f * 20.0f));
    }

    for (int32 Cy = 0; Cy < ChunksPerSide; ++Cy)
    {
        for (int32 Cx = 0; Cx < ChunksPerSide; ++Cx)
        {
            const int32 ChunkId = Cy * ChunksPerSide + Cx;
            if (!ChunkDirty.IsValidIndex(ChunkId) || ChunkDirty[ChunkId] > 0)
            {
                continue;
            }
            const int32 X = FMath::Min(Cx * ChunkCells + ChunkCells / 2, Side - 1);
            const int32 Y = FMath::Min(Cy * ChunkCells + ChunkCells / 2, Side - 1);
            const int32 Cell = CellIndex(X, Y);
            const float Probe = CellWetness(Cell) + FMath::Clamp(SnowDepth[Cell] / 15.0f, 0.0f, 1.0f);
            if (FMath::Abs(Probe - ChunkShown[ChunkId]) > 0.04f)
            {
                ChunkDirty[ChunkId] = 1;
            }
        }
    }
}

void ATerrainGrid::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bBuilt)
    {
        return;
    }

    LandscapeTimer += DeltaSeconds;
    if (LandscapeTimer > 3.0f)
    {
        LandscapeTimer = 0.0f;
        HideLandscapes();
    }

    int32 Budget = 6;
    for (int32 ChunkId = 0; ChunkId < ChunkDirty.Num() && Budget > 0; ++ChunkId)
    {
        const uint8 Flag = ChunkDirty[ChunkId];
        if (Flag == 0)
        {
            continue;
        }
        BuildChunk(ChunkId % ChunksPerSide, ChunkId / ChunksPerSide, Flag > 1);
        --Budget;
    }
}

FString ATerrainGrid::DescribeAt(const FVector& World) const
{
    int32 X = 0;
    int32 Y = 0;
    if (!CellOf(World, X, Y))
    {
        return TEXT("дальняя земля");
    }
    const int32 Cell = CellIndex(X, Y);
    if (WaterCell[Cell])
    {
        const float Depth = WaterLevel[Cell] - (Heights[CornerIndex(X, Y)] + Heights[CornerIndex(X + 1, Y + 1)]) * 0.5f;
        return FString::Printf(TEXT("вода: глубина %.0f см; %.0f °C; дно — песок"), Depth, SoilTemperature[Cell]);
    }

    const FSubstance& S = FMatter::Of(SurfaceKind(Cell));
    FString Out = FString::Printf(TEXT("земля под ногами — %s"),
        *FMatter::Describe(S, Moisture[Cell], SoilTemperature[Cell], 0.0f, 0.0f, 0.0f, false));
    if (Puddle[Cell] > 0.5f)
    {
        Out += FString::Printf(TEXT("; лужа %.0f мм"), Puddle[Cell]);
    }
    if (SnowDepth[Cell] > 0.5f)
    {
        Out += FString::Printf(TEXT("; снег %.0f см"), SnowDepth[Cell] * 0.3f);
    }
    if (SoilIce[Cell] > 0.3f)
    {
        Out += TEXT("; промёрзла");
    }
    if (Trodden[Cell] > 0.2f)
    {
        Out += FString::Printf(TEXT("; вытоптано %.0f%%"), Trodden[Cell] * 100.0f);
    }
    FTerrainFooting Footing;
    if (FootingAt(World, 70.0f, Footing))
    {
        Out += FString::Printf(TEXT("; прочность %.0f кПа, нога уходит на %.1f см, липкость %.0f%%, сцепление %.2f, шаг тяжелее в %.2f раза"),
            Footing.Strength, Footing.SinkCm, Footing.Stick * 100.0f, Footing.Grip, Footing.Effort);
    }
    Out += FString::Printf(TEXT("; вглубь: %s до %.0f см, %s до %.0f см, дальше %s"),
        FMatter::Of(DeepKind(X, Y, 1.0f)).Name, Topsoil[Cell],
        FMatter::Of(DeepKind(X, Y, Topsoil[Cell] + 1.0f)).Name, Subsoil[Cell],
        FMatter::Of(DeepKind(X, Y, 450.0f)).Name);
    return Out;
}
