#include "CityGenerator.h"
#include "TerrainGrid.h"
#include "MatterSubsystem.h"
#include "MatterStructure.h"
#include "CompleteHumanAI.h"
#include "FurnitureActor.h"
#include "BookActor.h"
#include "ResourceActor.h"
#include "AffordanceComponent.h"
#include "HumanWorldSubsystem.h"
#include "IdentityComponent.h"
#include "Textbook.h"
#include "Village.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"

// ---------------------------------------------------------------------------
//  ДЕРЕВНЯ И ГОРОД
//
//  Оба поселения сложены по тому, как их ставили на самом деле: деревня —
//  порядок дворов вдоль дороги у воды, с полями за околицей и лесом кругом;
//  город — стена с воротами, торг посередине, плотные дома вдоль улиц.
//
//  Ни мастерских, ни печей, ни припасов тут нет: людям дают только кров,
//  воду, книги и землю. Остальное они делают сами.
// ---------------------------------------------------------------------------

FName ACityGenerator::LookOf(EResourceKind Substance)
{
    switch (Substance)
    {
    case EResourceKind::Wood:
    case EResourceKind::Plank:      return TEXT("Wood");
    case EResourceKind::Straw:
    case EResourceKind::Reed:       return TEXT("Moss");
    case EResourceKind::Clay:       return TEXT("Grime");
    case EResourceKind::Brick:      return TEXT("BrickNew");
    case EResourceKind::Limestone:
    case EResourceKind::Sandstone:  return TEXT("Sandstone");
    case EResourceKind::Granite:
    case EResourceKind::Stone:      return TEXT("Stone");
    case EResourceKind::Glass:      return TEXT("Glass");
    case EResourceKind::Iron:
    case EResourceKind::Steel:      return TEXT("Steel");
    case EResourceKind::Sand:       return TEXT("Gravel");
    case EResourceKind::Water:      return TEXT("Glass");
    case EResourceKind::Grain:      return TEXT("Grass");
    default:                        return TEXT("Slate");
    }
}

void ACityGenerator::PieceOf(EResourceKind Substance, UStaticMesh* Mesh, const FVector& Centre,
                             const FVector& SizeCm, const FRotator& Rotation, bool bCollision)
{
    const FName Key(*FString::Printf(TEXT("Matter%d"), static_cast<int32>(Substance)));
    if (!Palette.Contains(Key))
    {
        if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
        {
            if (UMaterialInterface* Look = Matter->LookOf(Substance))
            {
                Palette.Add(Key, Look);
            }
        }
    }
    Piece(Mesh, Palette.Contains(Key) ? Key : LookOf(Substance), Centre, SizeCm, Rotation, bCollision);

    if (UnderConstruction)
    {
        // Кубометры в штуки: бревно — примерно четверть куба, камень — десятая.
        const float Volume = (SizeCm.X * SizeCm.Y * SizeCm.Z) / 1000000.0f;
        float& Spent = UnderConstruction->Made.FindOrAdd(Substance);
        Spent += FMath::Max(0.1f, Volume * 4.0f);
    }
}

void ACityGenerator::BuildStream(const FVector& From, const FVector& To, float Width)
{
    const FVector Along = To - From;
    const float Length = Along.Size2D();
    if (Length < 100.0f)
    {
        return;
    }

    const FVector Middle = (From + To) * 0.5f;
    const float Yaw = Along.Rotation().Yaw;

    PieceOf(EResourceKind::Sand, BuildingMesh, Middle + FVector(0, 0, 6.0f),
          FVector(Length + 200.0f, Width + 260.0f, 12.0f), FRotator(0.0f, Yaw, 0.0f), true);
    PieceOf(EResourceKind::Water, BuildingMesh, Middle + FVector(0, 0, 14.0f),
          FVector(Length, Width, 10.0f), FRotator(0.0f, Yaw, 0.0f));

    const int32 Stones = FMath::Max(3, FMath::RoundToInt(Length / 900.0f));
    for (int32 i = 0; i < Stones; ++i)
    {
        const float Along01 = (i + 0.5f) / Stones;
        const FVector At = FMath::Lerp(From, To, Along01);
        Decor(TEXT("Quaternius/Nature"), *FString::Printf(TEXT("Rock_Medium_%d"), 1 + i % 3), At + FVector(0, Width * 0.6f, 0.0f),
            FVector(90.0f, 80.0f, 55.0f), FMath::FRandRange(0.0f, 360.0f), true);
    }
}

void ACityGenerator::BuildField(const FVector& Centre, const FVector& Size, FName Crop)
{
    if (Terrain)
    {
        Terrain->MarkSurface(Centre, FVector2D(Size.X * 0.5f, Size.Y * 0.5f), 0.0f, EResourceKind::Grain, 0.0f);
    }
    if (Buildings.Num() == 0)
    {
        return;
    }
    FBuildingInfo& Keeper = Buildings[0];
    const int32 Cols = FMath::Clamp(FMath::RoundToInt(Size.X / 460.0f), 2, 8);
    const int32 Rows = FMath::Clamp(FMath::RoundToInt(Size.Y / 360.0f), 2, 8);
    for (int32 Row = 0; Row < Rows; ++Row)
    {
        for (int32 Col = 0; Col < Cols; ++Col)
        {
            const FVector At = Centre + FVector(-Size.X * 0.5f + Size.X * (Col + 0.5f) / Cols,
                                                -Size.Y * 0.5f + Size.Y * (Row + 0.5f) / Rows, 0.0f);
            if (AFurnitureActor* Strip = PlaceFurniture(Keeper, EFurnitureType::GardenBed, FVector(At.X, At.Y, GroundAt(At)), 0.0f))
            {
                Strip->Ripeness = FMath::FRand() < 0.75f ? FMath::FRandRange(0.85f, 1.0f) : 0.0f;
                Strip->RefreshLook();
                Strip->BuildOffers();
            }
        }
    }
}

void ACityGenerator::BuildWood(const FVector& Centre, float Radius, int32 Trees)
{
    if (Buildings.Num() == 0)
    {
        return;
    }
    FBuildingInfo& Keeper = Buildings[0];

    for (int32 i = 0; i < Trees; ++i)
    {
        const float Angle = FMath::FRandRange(0.0f, 360.0f);
        const float Away = FMath::Sqrt(FMath::FRand()) * Radius;
        const FVector At = Centre + FVector(FMath::Cos(FMath::DegreesToRadians(Angle)) * Away,
                                            FMath::Sin(FMath::DegreesToRadians(Angle)) * Away, 0.0f);

        const FVector Stand(At.X, At.Y, GroundAt(At));
        const float Roll = FMath::FRand();
        if (Roll < 0.62f)
        {
            PlaceFurniture(Keeper, EFurnitureType::Tree, Stand, FMath::FRandRange(0.0f, 360.0f));
        }
        else if (Roll < 0.82f)
        {
            PlaceFurniture(Keeper, EFurnitureType::Bush, Stand, FMath::FRandRange(0.0f, 360.0f));
        }
        else
        {
            PlaceFurniture(Keeper, EFurnitureType::Mushrooms, Stand, 0.0f);
        }
    }
}

void ACityGenerator::BuildHut(const FString& Type, const FVector& Centre, const FVector& Footprint, float Yaw)
{
    const bool bHome = Type == TEXT("Residential");
    const FString Title = bHome ? TEXT("изба") : (Type == TEXT("Library") ? TEXT("церковь") : TEXT("королевский замок"));
    if (Terrain)
    {
        const float Reach = FMath::Max(Footprint.X, Footprint.Y) * 0.5f + 150.0f;
        Terrain->Flatten(Centre, FVector2D(Reach, Reach), 300.0f);
    }
    AMatterStructure* House = AMatterStructure::BuildIzba(GetWorld(), Centre, Yaw, Footprint,
        bHome ? EResourceKind::Straw : EResourceKind::Plank, Title, bHome);
    if (!House)
    {
        BuildHutBlocks(Type, Centre, Footprint, Yaw);
        return;
    }
    if (UnderConstruction)
    {
        UnderConstruction->Structure = House;
        for (const FStructurePiece& Piece : House->GetPieces())
        {
            float& Spent = UnderConstruction->Made.FindOrAdd(Piece.Substance);
            Spent += UMatterComponent::VolumeOf(Piece.Shape, Piece.Size) * 4.0f;
        }
    }
}

void ACityGenerator::BuildHutBlocks(const FString& Type, const FVector& Centre, const FVector& Footprint, float Yaw)
{
    const float HalfX = Footprint.X * 0.5f;
    const float HalfY = Footprint.Y * 0.5f;
    const float WallH = 240.0f;
    const float Thick = 30.0f;
    const FRotator Turn(0.0f, Yaw, 0.0f);

    auto Local = [&](float X, float Y, float Z)
    {
        return Centre + Turn.RotateVector(FVector(X, Y, Z));
    };

    // Подклет: венец на камнях, чтобы пол не гнил.
    PieceOf(EResourceKind::Limestone, BuildingMesh, Local(0, 0, 18.0f),
            FVector(Footprint.X + 50.0f, Footprint.Y + 50.0f, 36.0f), Turn, true);
    PieceOf(EResourceKind::Plank, BuildingMesh, Local(0, 0, 44.0f),
            FVector(Footprint.X, Footprint.Y, 16.0f), Turn, true);

    // Сруб: три глухие стены и передняя с дверью.
    PieceOf(EResourceKind::Wood, BuildingMesh, Local(-HalfX, 0, 52.0f + WallH * 0.5f),
            FVector(Thick, Footprint.Y, WallH), Turn, true);
    PieceOf(EResourceKind::Wood, BuildingMesh, Local(0, -HalfY, 52.0f + WallH * 0.5f),
            FVector(Footprint.X, Thick, WallH), Turn, true);
    PieceOf(EResourceKind::Wood, BuildingMesh, Local(0, HalfY, 52.0f + WallH * 0.5f),
            FVector(Footprint.X, Thick, WallH), Turn, true);

    const float DoorWidth = 300.0f;
    const float SideWidth = (Footprint.Y - DoorWidth) * 0.5f;
    if (SideWidth > 10.0f)
    {
        for (int32 Side = -1; Side <= 1; Side += 2)
        {
            const float OffsetY = Side * (DoorWidth * 0.5f + SideWidth * 0.5f);
            PieceOf(EResourceKind::Wood, BuildingMesh, Local(HalfX, OffsetY, 52.0f + WallH * 0.5f),
                    FVector(Thick, SideWidth, WallH), Turn, true);
        }
    }
    PieceOf(EResourceKind::Wood, BuildingMesh, Local(HalfX, 0, 52.0f + WallH - 25.0f),
            FVector(Thick, DoorWidth, 50.0f), Turn, true);

    // Соломенная кровля на два ската.
    const float RoofH = 150.0f;
    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        PieceOf(EResourceKind::Straw, BuildingMesh,
                Local(0, Side * HalfY * 0.5f, 52.0f + WallH + RoofH * 0.5f),
                FVector(Footprint.X + 80.0f, Footprint.Y * 0.62f, 26.0f),
                Turn + FRotator(0.0f, 0.0f, Side * 34.0f), true);
    }
    PieceOf(EResourceKind::Wood, BuildingMesh, Local(0, 0, 52.0f + WallH + RoofH),
            FVector(Footprint.X + 60.0f, 26.0f, 22.0f), Turn, false);

    // Дымник над очагом: глина по плетню.
    PieceOf(EResourceKind::Clay, BuildingMesh, Local(-HalfX * 0.5f, 0, 52.0f + WallH + RoofH + 40.0f),
            FVector(70.0f, 70.0f, 110.0f), Turn, false);

    // Крыльцо в две ступени.
    for (int32 Step = 0; Step < 2; ++Step)
    {
        PieceOf(EResourceKind::Plank, BuildingMesh,
                Local(HalfX + 60.0f + Step * 70.0f, 0, 14.0f + (1 - Step) * 22.0f),
                FVector(140.0f, DoorWidth, 30.0f + (1 - Step) * 22.0f), Turn, true);
    }
}

void ACityGenerator::BuildTimberHouse(const FString& Type, const FVector& Centre, const FVector& Footprint,
                                      int32 Floors, float Yaw)
{
    const float HalfX = Footprint.X * 0.5f;
    const float HalfY = Footprint.Y * 0.5f;
    const float Thick = 26.0f;
    const float FloorH = 280.0f;
    const FRotator Turn(0.0f, Yaw, 0.0f);

    auto Local = [&](float X, float Y, float Z)
    {
        return Centre + Turn.RotateVector(FVector(X, Y, Z));
    };

    PieceOf(EResourceKind::Limestone, BuildingMesh, Local(0, 0, 30.0f),
          FVector(Footprint.X + 40.0f, Footprint.Y + 40.0f, 60.0f), Turn, true);
    PieceOf(EResourceKind::Plank, BuildingMesh, Local(0, 0, 68.0f),
          FVector(Footprint.X, Footprint.Y, 16.0f), Turn, true);

    const float DoorWidth = 300.0f;

    for (int32 Level = 0; Level < FMath::Max(1, Floors); ++Level)
    {
        const float Base = 76.0f + Level * FloorH;
        // Верхние этажи нависают над улицей — как и строили.
        const float Jetty = Level * 26.0f;
        const FName Wall = Level == 0 ? FName(TEXT("Stone")) : FName(TEXT("Sandstone"));

        PieceOf(Level == 0 ? EResourceKind::Limestone : EResourceKind::Clay, BuildingMesh, Local(-HalfX - Jetty, 0, Base + FloorH * 0.5f),
              FVector(Thick, Footprint.Y + Jetty * 2.0f, FloorH), Turn, true);
        PieceOf(Level == 0 ? EResourceKind::Limestone : EResourceKind::Clay, BuildingMesh, Local(0, -HalfY - Jetty, Base + FloorH * 0.5f),
              FVector(Footprint.X + Jetty * 2.0f, Thick, FloorH), Turn, true);
        PieceOf(Level == 0 ? EResourceKind::Limestone : EResourceKind::Clay, BuildingMesh, Local(0, HalfY + Jetty, Base + FloorH * 0.5f),
              FVector(Footprint.X + Jetty * 2.0f, Thick, FloorH), Turn, true);

        if (Level == 0)
        {
            const float SideWidth = (Footprint.Y - DoorWidth) * 0.5f;
            for (int32 Side = -1; Side <= 1; Side += 2)
            {
                const float OffsetY = Side * (DoorWidth * 0.5f + SideWidth * 0.5f);
                PieceOf(Level == 0 ? EResourceKind::Limestone : EResourceKind::Clay, BuildingMesh, Local(HalfX, OffsetY, Base + FloorH * 0.5f),
                      FVector(Thick, SideWidth, FloorH), Turn, true);
            }
            PieceOf(Level == 0 ? EResourceKind::Limestone : EResourceKind::Clay, BuildingMesh, Local(HalfX, 0, Base + FloorH - 30.0f),
                  FVector(Thick, DoorWidth, 60.0f), Turn, true);
        }
        else
        {
            PieceOf(Level == 0 ? EResourceKind::Limestone : EResourceKind::Clay, BuildingMesh, Local(HalfX + Jetty, 0, Base + FloorH * 0.5f),
                  FVector(Thick, Footprint.Y + Jetty * 2.0f, FloorH), Turn, true);

            // Фахверк: тёмные балки по светлой стене.
            for (int32 Beam = -1; Beam <= 1; ++Beam)
            {
                PieceOf(EResourceKind::Wood, BuildingMesh,
                        Local(HalfX + Jetty + 4.0f, Beam * Footprint.Y * 0.3f, Base + FloorH * 0.5f),
                        FVector(10.0f, 18.0f, FloorH), Turn, false);
            }
            PieceOf(EResourceKind::Wood, BuildingMesh, Local(HalfX + Jetty + 4.0f, 0, Base + 10.0f),
                  FVector(10.0f, Footprint.Y + Jetty * 2.0f, 20.0f), Turn);

            // Окно.
            PieceOf(EResourceKind::Glass, BuildingMesh, Local(HalfX + Jetty + 6.0f, 0, Base + FloorH * 0.62f),
                  FVector(8.0f, Footprint.Y * 0.35f, FloorH * 0.3f), Turn);
        }

        PieceOf(EResourceKind::Plank, BuildingMesh, Local(0, 0, Base + FloorH),
              FVector(Footprint.X + Jetty * 2.0f + 20.0f, Footprint.Y + Jetty * 2.0f + 20.0f, 18.0f), Turn, true);
    }

    const float Top = 76.0f + FMath::Max(1, Floors) * FloorH;
    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        PieceOf(EResourceKind::Brick, BuildingMesh, Local(0, Side * HalfY * 0.5f, Top + 80.0f),
              FVector(Footprint.X + 70.0f, Footprint.Y * 0.66f, 24.0f),
              Turn + FRotator(0.0f, 0.0f, Side * 40.0f), true);
    }
    PieceOf(EResourceKind::Brick, BuildingMesh, Local(-HalfX * 0.6f, HalfY * 0.6f, Top + 150.0f),
          FVector(60.0f, 60.0f, 180.0f), Turn);
}

void ACityGenerator::BuildWallRun(const FVector& From, const FVector& To, float Height)
{
    const FVector Along = To - From;
    const float Length = Along.Size2D();
    if (Length < 50.0f)
    {
        return;
    }

    const FVector Middle = (From + To) * 0.5f;
    const float Yaw = Along.Rotation().Yaw;
    const FRotator Turn(0.0f, Yaw, 0.0f);

    PieceOf(EResourceKind::Granite, BuildingMesh, Middle + FVector(0, 0, Height * 0.5f),
          FVector(Length, 120.0f, Height), Turn, true);

    const int32 Merlons = FMath::Max(2, FMath::RoundToInt(Length / 180.0f));
    for (int32 i = 0; i < Merlons; ++i)
    {
        if (i % 2 == 1)
        {
            continue;
        }
        const float Along01 = (i + 0.5f) / Merlons;
        const FVector At = FMath::Lerp(From, To, Along01);
        PieceOf(EResourceKind::Granite, BuildingMesh, At + FVector(0, 0, Height + 35.0f),
              FVector(130.0f, 130.0f, 70.0f), Turn);
    }
}

void ACityGenerator::BuildTower(const FVector& At, float Radius, float Height)
{
    PieceOf(EResourceKind::Granite, CylinderMesh, At + FVector(0, 0, Height * 0.5f),
          FVector(Radius * 2.0f, Radius * 2.0f, Height), FRotator::ZeroRotator, true);
    PieceOf(EResourceKind::Brick, CylinderMesh, At + FVector(0, 0, Height + 20.0f),
          FVector(Radius * 2.3f, Radius * 2.3f, 40.0f));
    PieceOf(EResourceKind::Brick, ConeMesh, At + FVector(0, 0, Height + 40.0f),
          FVector(Radius * 2.2f, Radius * 2.2f, Radius * 2.6f));
}

void ACityGenerator::FurnishMedieval(FBuildingInfo& Building, const FString& Type)
{
    const float HalfX = FMath::Max(120.0f, Building.Scale.X * 0.5f - 120.0f);
    const float HalfY = FMath::Max(120.0f, Building.Scale.Y * 0.5f - 120.0f);
    const FVector Floor = Building.Location + FVector(0.0f, 0.0f, 60.0f);

    auto Spot = [&](float FracX, float FracY)
    {
        return Floor + FVector(HalfX * FracX, HalfY * FracY, 0.0f);
    };

    if (Type == TEXT("Residential"))
    {
        // Изба: очаг в углу, стол под окном, лавки, полати, сундук.
        FVector StoveAt = Spot(-0.75f, -0.7f);
        float StoveYaw = 0.0f;
        if (Building.Structure)
        {
            for (const FStructurePiece& Part : Building.Structure->GetPieces())
            {
                if (Part.Role == TEXT("труба") && !Part.bRemoved)
                {
                    const FVector Flue = Part.Frame.GetLocation();
                    StoveAt = FVector(Flue.X, Flue.Y, Floor.Z);
                    StoveYaw = Building.Structure->GetActorRotation().Yaw;
                    break;
                }
            }
        }
        PlaceFurniture(Building, EFurnitureType::Stove, StoveAt, StoveYaw);
        PlaceFurniture(Building, EFurnitureType::Table, Spot(0.3f, -0.2f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Bench, Spot(0.3f, 0.45f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Bench, Spot(0.3f, -0.85f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Bed, Spot(-0.55f, 0.65f), 90.0f);
        PlaceFurniture(Building, EFurnitureType::Wardrobe, Spot(0.8f, 0.8f), 270.0f);
        PlaceFurniture(Building, EFurnitureType::Barrel, Spot(-0.85f, 0.15f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Lamp, Spot(0.0f, 0.0f), 0.0f);
        return;
    }

    if (Type == TEXT("Library") || Type == TEXT("School"))
    {
        // Часовня или монастырская книжница: столы и книги.
        for (int32 Row = 0; Row < 3; ++Row)
        {
            const float FracY = -0.6f + Row * 0.6f;
            const FVector ShelfAt = Spot(-0.75f, FracY);
            PlaceFurniture(Building, EFurnitureType::Bookshelf, ShelfAt, 270.0f);

            TArray<const FTextbook*> Everything;
            for (const FTextbook& Book : FLibrary::All())
            {
                Everything.Add(&Book);
            }

            for (int32 Slot = 0; Slot < 4; ++Slot)
            {
                const int32 Index = (Row * 4 + Slot + BooksPlaced) % FMath::Max(1, Everything.Num());
                SpawnBook(Everything[Index]->Subject,
                          ShelfAt + FVector(14.0f, -33.0f + Slot * 22.0f, 62.0f + Slot * 42.0f));
            }
        }
        PlaceFurniture(Building, EFurnitureType::SchoolDesk, Spot(0.3f, -0.4f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::SchoolDesk, Spot(0.3f, 0.4f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Bench, Spot(0.7f, 0.0f), 0.0f);
        return;
    }

    if (Type == TEXT("TownHall") && Settlement != ESettlement::Modern)
    {
        FurnishCastle(Building);
        return;
    }

    PlaceFurniture(Building, EFurnitureType::Table, Spot(0.0f, 0.0f), 0.0f);
    PlaceFurniture(Building, EFurnitureType::Bench, Spot(0.0f, 0.5f), 0.0f);
    PlaceFurniture(Building, EFurnitureType::Barrel, Spot(-0.7f, -0.6f), 0.0f);
}

void ACityGenerator::FurnishCastle(FBuildingInfo& Building)
{
    const float HalfX = FMath::Max(120.0f, Building.Scale.X * 0.5f - 120.0f);
    const float HalfY = FMath::Max(120.0f, Building.Scale.Y * 0.5f - 120.0f);
    const FVector Floor = Building.Location + FVector(0.0f, 0.0f, 60.0f);
    auto Spot = [&](float FracX, float FracY)
    {
        return Floor + FVector(HalfX * FracX, HalfY * FracY, 0.0f);
    };
    for (int32 Bed = 0; Bed < 5; ++Bed)
    {
        PlaceFurniture(Building, EFurnitureType::Bed, Spot(-0.82f, -0.8f + Bed * 0.4f), 90.0f);
    }
    for (int32 Bench = 0; Bench < 5; ++Bench)
    {
        PlaceFurniture(Building, EFurnitureType::Bench, Spot(-0.35f + Bench * 0.28f, 0.88f), 0.0f);
    }
    PlaceFurniture(Building, EFurnitureType::Stove, Spot(0.15f, -0.78f), 0.0f);
    PlaceFurniture(Building, EFurnitureType::Barrel, Spot(0.45f, -0.85f), 0.0f);
    PlaceFurniture(Building, EFurnitureType::Table, Spot(0.1f, -0.15f), 0.0f);
    PlaceFurniture(Building, EFurnitureType::Table, Spot(0.1f, 0.35f), 0.0f);
    PlaceFurniture(Building, EFurnitureType::Wardrobe, Spot(0.65f, -0.85f), 0.0f);
    PlaceFurniture(Building, EFurnitureType::Lamp, Spot(-0.35f, -0.82f), 0.0f);
    PlaceFurniture(Building, EFurnitureType::Chair, Spot(0.5f, 0.1f), 180.0f);
    PlaceFurniture(Building, EFurnitureType::Loom, Spot(-0.45f, 0.5f), 0.0f);
    const FVector Store = Building.Location + FVector(-HalfX - 420.0f, -HalfY - 160.0f, 0.0f);
    PlaceFurniture(Building, EFurnitureType::Shed, FVector(Store.X, Store.Y, GroundAt(Store)), 0.0f);
}

float ACityGenerator::GroundAt(const FVector& At) const
{
    return Terrain ? Terrain->HeightAt(At) : 0.0f;
}

void ACityGenerator::BuildVillage()
{
    // ------------------------------------------------------------------
    //  Поселение на холме у слияния рек — так стояла Москва через сто лет
    //  после основания: рубленая крепость наверху, посад под горой вдоль
    //  воды, торг у пристани, поля за посадом, лес кругом.
    // ------------------------------------------------------------------
    const FVector Hill(-2400.0f, 0.0f, 0.0f);
    const float HillZ = GroundAt(Hill);
    const float Wall = 1500.0f;
    FVillage::Plan(NPCsCount, 0x1377u, VillagePlans, VillageHouseholds);
    VillagePlaces.Reset();

    // --- Рубленая стена крепости: тын из брёвен, башни по углам ---------
    const int32 Sides = 6;
    TArray<FVector> Corners;
    for (int32 i = 0; i < Sides; ++i)
    {
        const float Angle = 360.0f / Sides * i + 15.0f;
        const FVector At = Hill + FVector(FMath::Cos(FMath::DegreesToRadians(Angle)) * Wall,
                                          FMath::Sin(FMath::DegreesToRadians(Angle)) * Wall, 0.0f);
        Corners.Add(FVector(At.X, At.Y, GroundAt(At)));
    }

    for (int32 i = 0; i < Sides; ++i)
    {
        const FVector From = Corners[i];
        const FVector To = Corners[(i + 1) % Sides];

        // Башня: сруб с шатром.
        PieceOf(EResourceKind::Wood, BuildingMesh, From + FVector(0, 0, 260.0f),
                FVector(300.0f, 300.0f, 520.0f), FRotator::ZeroRotator, true);
        PieceOf(EResourceKind::Straw, ConeMesh, From + FVector(0, 0, 620.0f),
                FVector(380.0f, 380.0f, 320.0f), FRotator::ZeroRotator, false);

        const FVector Middle = (From + To) * 0.5f;
        const float Yaw = (To - From).Rotation().Yaw;
        const float Length = FVector::Dist2D(From, To);

        if (i == 0)
        {
            // Ворота: проезд и надвратная башня.
            const FVector Dir = (To - From).GetSafeNormal();
            PieceOf(EResourceKind::Wood, BuildingMesh, Middle - Dir * (Length * 0.3f) + FVector(0, 0, 200.0f),
                    FVector(Length * 0.4f, 90.0f, 400.0f), FRotator(0.0f, Yaw, 0.0f), true);
            PieceOf(EResourceKind::Wood, BuildingMesh, Middle + Dir * (Length * 0.3f) + FVector(0, 0, 200.0f),
                    FVector(Length * 0.4f, 90.0f, 400.0f), FRotator(0.0f, Yaw, 0.0f), true);
            PieceOf(EResourceKind::Wood, BuildingMesh, Middle + FVector(0, 0, 430.0f),
                    FVector(Length * 0.25f, 120.0f, 160.0f), FRotator(0.0f, Yaw, 0.0f), true);
        }
        else
        {
            // Тын: брёвна стоймя, заострённые сверху.
            const int32 Logs = FMath::Max(4, FMath::RoundToInt(Length / 90.0f));
            for (int32 L = 0; L < Logs; ++L)
            {
                const FVector At = FMath::Lerp(From, To, (L + 0.5f) / Logs);
                PieceOf(EResourceKind::Wood, CylinderMesh, At + FVector(0, 0, 200.0f),
                        FVector(80.0f, 80.0f, 400.0f), FRotator::ZeroRotator, true);
                PieceOf(EResourceKind::Wood, ConeMesh, At + FVector(0, 0, 420.0f),
                        FVector(80.0f, 80.0f, 60.0f), FRotator::ZeroRotator, false);
            }
        }
    }

    // --- Внутри крепости: княжий двор, церковь, амбары -------------------
    const FVector CourtAt(Hill.X - 400.0f, Hill.Y + 300.0f, 0.0f);
    SpawnBuilding(TEXT("TownHall"), FVector(CourtAt.X, CourtAt.Y, GroundAt(CourtAt)), FVector(1000.0f, 760.0f, 320.0f));
    UnderConstruction = Buildings.Num() > 0 ? &Buildings.Last() : nullptr;
    BuildHut(TEXT("TownHall"), FVector(CourtAt.X, CourtAt.Y, GroundAt(CourtAt)), FVector(1000.0f, 760.0f, 0.0f), 0.0f);
    UnderConstruction = nullptr;
    if (Buildings.Num() > 0)
    {
        FBuildingInfo& Court = Buildings.Last();
        Court.EntranceLocation = FVector(CourtAt.X + 620.0f, CourtAt.Y, GroundAt(CourtAt));
        FurnishMedieval(Court, TEXT("TownHall"));
    }

    const FVector ChurchAt(Hill.X + 200.0f, Hill.Y - 600.0f, 0.0f);
    const float ChurchZ = GroundAt(ChurchAt);
    SpawnBuilding(TEXT("Library"), FVector(ChurchAt.X, ChurchAt.Y, ChurchZ), FVector(840.0f, 640.0f, 460.0f));
    UnderConstruction = Buildings.Num() > 0 ? &Buildings.Last() : nullptr;
    BuildHut(TEXT("Library"), FVector(ChurchAt.X, ChurchAt.Y, ChurchZ), FVector(840.0f, 640.0f, 0.0f), 180.0f);
    PieceOf(EResourceKind::Wood, ConeMesh, FVector(ChurchAt.X, ChurchAt.Y, ChurchZ + 620.0f),
            FVector(300.0f, 300.0f, 520.0f), FRotator::ZeroRotator, false);
    PieceOf(EResourceKind::Wood, SphereMesh, FVector(ChurchAt.X, ChurchAt.Y, ChurchZ + 900.0f),
            FVector(160.0f, 160.0f, 200.0f), FRotator::ZeroRotator, false);
    UnderConstruction = nullptr;
    if (Buildings.Num() > 0)
    {
        FBuildingInfo& Church = Buildings.Last();
        Church.EntranceLocation = FVector(ChurchAt.X - 520.0f, ChurchAt.Y, ChurchZ);
        FurnishMedieval(Church, TEXT("Library"));
    }

    if (Buildings.Num() > 0)
    {
        PlaceFurniture(Buildings[0], EFurnitureType::Well,
                       FVector(Hill.X + 500.0f, Hill.Y + 500.0f, GroundAt(Hill + FVector(500.0f, 500.0f, 0.0f))), 0.0f);
    }

    // --- Посад под горой: избы двумя порядками вдоль спуска к реке -------
    const int32 Crofts = FMath::Clamp(VillageHouseholds, 3, 26);
    for (int32 i = 0; i < Crofts; ++i)
    {
        const int32 Row = i % 2;
        const int32 Along = i / 2;
        const float X = Hill.X + 2400.0f + Along * 1500.0f;
        const float Y = Hill.Y + (Row == 0 ? -900.0f : 900.0f) + FMath::Sin(Along * 0.6f) * 300.0f;
        const FVector At(X, Y, 0.0f);
        const FVector Stand(X, Y, GroundAt(At));

        const FVector Footprint(FMath::FRandRange(600.0f, 740.0f), FMath::FRandRange(540.0f, 660.0f), 0.0f);
        const float Yaw = Row == 0 ? 90.0f : 270.0f;

        SpawnBuilding(TEXT("Residential"), Stand, FVector(Footprint.X, Footprint.Y, 240.0f));
        UnderConstruction = Buildings.Num() > 0 ? &Buildings.Last() : nullptr;
        BuildHut(TEXT("Residential"), Stand, Footprint, Yaw);
        UnderConstruction = nullptr;

        if (Buildings.Num() > 0)
        {
            FBuildingInfo& Croft = Buildings.Last();
            Croft.EntranceLocation = Stand + FRotator(0.0f, Yaw, 0.0f).RotateVector(FVector(Footprint.X * 0.5f + 190.0f, 0.0f, 0.0f));
            FurnishMedieval(Croft, TEXT("Residential"));

            const FVector Back = Stand + FVector(0.0f, Row == 0 ? -620.0f : 620.0f, 0.0f);
            PlaceFurniture(Croft, EFurnitureType::Shed, FVector(Back.X + 300.0f, Back.Y, GroundAt(Back)), Yaw);
            PlaceFurniture(Croft, EFurnitureType::Fence, FVector(Stand.X - Footprint.X * 0.5f - 170.0f, Stand.Y, Stand.Z), 0.0f);

            if (FMath::FRand() < 0.55f)
            {
                const FVector BushAt = Back + FVector(-280.0f, 0.0f, 0.0f);
                PlaceFurniture(Croft, EFurnitureType::Bush, FVector(BushAt.X, BushAt.Y, GroundAt(BushAt)),
                               FMath::FRandRange(0.0f, 360.0f));
            }
        }
    }

    // --- Торг и пристань у воды ------------------------------------------
    const FVector Torg(Hill.X + 3600.0f, 3200.0f, 0.0f);
    PieceOf(EResourceKind::Plank, BuildingMesh, FVector(Torg.X, Torg.Y, GroundAt(Torg) + 10.0f),
            FVector(1800.0f, 1400.0f, 20.0f), FRotator::ZeroRotator, true);

    const FVector Pier(Hill.X + 3600.0f, 4400.0f, 0.0f);
    PieceOf(EResourceKind::Plank, BuildingMesh, FVector(Pier.X, Pier.Y, GroundAt(Pier) + 30.0f),
            FVector(500.0f, 1600.0f, 24.0f), FRotator::ZeroRotator, true);
    TorgAt = FVector(Torg.X, Torg.Y, GroundAt(Torg) + 20.0f);
    PierAt = FVector(Pier.X, Pier.Y, GroundAt(Pier) + 42.0f);
    VillagePlaces.Add(TPair<EPlaceKind, FVector>(EPlaceKind::Market, TorgAt));
    VillagePlaces.Add(TPair<EPlaceKind, FVector>(EPlaceKind::River, PierAt + FVector(0.0f, 200.0f, 0.0f)));

    if (Buildings.Num() > 0)
    {
        PlaceFurniture(Buildings[0], EFurnitureType::Well, FVector(Torg.X - 600.0f, Torg.Y, GroundAt(Torg)), 0.0f);
        VillageStalls.Reset();
        for (int32 Stall = 0; Stall < 4; ++Stall)
        {
            const FVector At = TorgAt + FVector(-450.0f + Stall * 300.0f, (Stall % 2 == 0) ? -250.0f : 250.0f, 0.0f);
            if (AFurnitureActor* Made = PlaceFurniture(Buildings[0], EFurnitureType::MarketStall, At, Stall % 2 == 0 ? 0.0f : 180.0f))
            {
                VillageStalls.Add(Made);
            }
        }
        for (int32 Spot = 0; Spot < 3; ++Spot)
        {
            PlaceFurniture(Buildings[0], EFurnitureType::FishingSpot, PierAt + FVector(-160.0f + Spot * 160.0f, 500.0f + Spot * 120.0f, 0.0f),
                90.0f);
        }
        const FVector Clay1 = PierAt + FVector(-1600.0f, -500.0f, 0.0f);
        const FVector Clay2 = PierAt + FVector(1700.0f, -450.0f, 0.0f);
        if (AFurnitureActor* Pit = PlaceFurniture(Buildings[0], EFurnitureType::ClayPit, FVector(Clay1.X, Clay1.Y, GroundAt(Clay1)), 0.0f))
        {
            Pit->Substance = EResourceKind::Clay;
        }
        if (AFurnitureActor* Pit = PlaceFurniture(Buildings[0], EFurnitureType::ClayPit, FVector(Clay2.X, Clay2.Y, GroundAt(Clay2)), 0.0f))
        {
            Pit->Substance = EResourceKind::Clay;
        }
        const FVector Quarry1 = Hill + FVector(1700.0f, -1900.0f, 0.0f);
        const FVector Quarry2 = Hill + FVector(-1900.0f, 1700.0f, 0.0f);
        if (AFurnitureActor* Rock = PlaceFurniture(Buildings[0], EFurnitureType::StonePile, FVector(Quarry1.X, Quarry1.Y, GroundAt(Quarry1)), 0.0f))
        {
            Rock->Substance = EResourceKind::Limestone;
        }
        if (AFurnitureActor* Rock = PlaceFurniture(Buildings[0], EFurnitureType::StonePile, FVector(Quarry2.X, Quarry2.Y, GroundAt(Quarry2)), 0.0f))
        {
            Rock->Substance = EResourceKind::Granite;
        }
    }

    // --- Поля за посадом, выгон и лес ------------------------------------
    BuildField(FVector(Hill.X + 3000.0f, -3400.0f, GroundAt(FVector(Hill.X + 3000.0f, -3400.0f, 0.0f))),
               FVector(2800.0f, 1800.0f, 0.0f), TEXT("Grass"));
    BuildField(FVector(Hill.X + 6200.0f, -3200.0f, GroundAt(FVector(Hill.X + 6200.0f, -3200.0f, 0.0f))),
               FVector(2800.0f, 1800.0f, 0.0f), TEXT("Moss"));
    VillagePlaces.Add(TPair<EPlaceKind, FVector>(EPlaceKind::Field, FVector(Hill.X + 3000.0f, -3400.0f, GroundAt(FVector(Hill.X + 3000.0f, -3400.0f, 0.0f)))));
    VillagePlaces.Add(TPair<EPlaceKind, FVector>(EPlaceKind::Field, FVector(Hill.X + 6200.0f, -3200.0f, GroundAt(FVector(Hill.X + 6200.0f, -3200.0f, 0.0f)))));

    if (Buildings.Num() > 0)
    {
        FBuildingInfo& Commons = Buildings[0];
        const FVector Vineyard1 = FVector(Hill.X + 4200.0f, -2200.0f, GroundAt(FVector(Hill.X + 4200.0f, -2200.0f, 0.0f)));
        const FVector Vineyard2 = FVector(Hill.X + 5200.0f, -2600.0f, GroundAt(FVector(Hill.X + 5200.0f, -2600.0f, 0.0f)));
        for (int32 i = 0; i < 8; ++i)
        {
            const FVector Spot = Vineyard1 + FVector(240.0f * i, FMath::FRandRange(-150.0f, 150.0f), 0.0f);
            PlaceFurniture(Commons, EFurnitureType::Bush, FVector(Spot.X, Spot.Y, GroundAt(Spot)), FMath::FRandRange(0.0f, 360.0f));
        }
        for (int32 i = 0; i < 6; ++i)
        {
            const FVector Spot = Vineyard2 + FVector(180.0f * i, FMath::FRandRange(-120.0f, 120.0f), 0.0f);
            PlaceFurniture(Commons, EFurnitureType::Bush, FVector(Spot.X, Spot.Y, GroundAt(Spot)), FMath::FRandRange(0.0f, 360.0f));
        }
        for (int32 i = 0; i < 7; ++i)
        {
            const FVector At(FMath::FRandRange(Hill.X + 1200.0f, Hill.X + 7000.0f),
                             FMath::FRandRange(-4200.0f, -2200.0f), 0.0f);
            PlaceFurniture(Commons, EFurnitureType::WildField, FVector(At.X, At.Y, GroundAt(At)), 0.0f);
        }
        for (int32 i = 0; i < 5; ++i)
        {
            const FVector At(FMath::FRandRange(Hill.X - 1000.0f, Hill.X + 5000.0f),
                             FMath::FRandRange(2200.0f, 4200.0f), 0.0f);
            PlaceFurniture(Commons, EFurnitureType::Spring, FVector(At.X, At.Y, GroundAt(At)), 0.0f);
        }
    }

    BuildWood(FVector(Hill.X - 4200.0f, -2600.0f, 0.0f), 2400.0f, 34);
    BuildWood(FVector(Hill.X - 3800.0f, 3600.0f, 0.0f), 2200.0f, 30);
    BuildWood(FVector(Hill.X + 8200.0f, 1200.0f, 0.0f), 2400.0f, 34);
    for (const FVector& Wood : { FVector(Hill.X - 4200.0f, -2600.0f, 0.0f), FVector(Hill.X - 3800.0f, 3600.0f, 0.0f), FVector(Hill.X + 8200.0f, 1200.0f, 0.0f) })
    {
        VillagePlaces.Add(TPair<EPlaceKind, FVector>(EPlaceKind::Forest, FVector(Wood.X, Wood.Y, GroundAt(Wood))));
    }

    // =======================================================================
    //  УРОЖАЙ НА ЗЕМЛЕ
    //
    //  Мир не пустой склад: у кустов лежат ягоды и виноград, под деревьями —
    //  яблоки, жёлуди и орехи, на лугу — травы. Эти припасы можно увидеть,
    //  взять в руки, откусить и унести домой. Без них новый мир был голым.
    // =======================================================================
    SeedVisibleHarvest();
}

// ---------------------------------------------------------------------------
//  Что лежит на земле с самого начала: видимые, осязаемые припасы.
// ---------------------------------------------------------------------------
void ACityGenerator::SeedVisibleHarvest()
{
    UWorld* World = GetWorld();
    if (!World || Buildings.Num() == 0)
    {
        return;
    }

    int32 Piles = 0;
    auto Drop = [&Piles, World](EResourceKind Kind, float Amount, const FVector& At)
    {
        if (AResourceActor::Spawn(World, Kind, Amount, At + FVector(0.0f, 0.0f, 30.0f)))
        {
            ++Piles;
        }
    };

    // У каждого куста и дерева — свой дар, и не один.
    for (TActorIterator<AFurnitureActor> It(World); It; ++It)
    {
        AFurnitureActor* Thing = *It;
        if (!Thing)
        {
            continue;
        }
        const FVector At = Thing->GetActorLocation();
        switch (Thing->FurnitureType)
        {
        case EFurnitureType::Bush:
            Drop(EResourceKind::Berry, 2.0f,
                 At + FVector(FMath::FRandRange(-90.0f, 90.0f), FMath::FRandRange(-90.0f, 90.0f), 0.0f));
            if (FMath::FRand() < 0.5f)
            {
                Drop(EResourceKind::Grape, 2.0f,
                     At + FVector(FMath::FRandRange(-110.0f, 110.0f), FMath::FRandRange(-110.0f, 110.0f), 0.0f));
            }
            if (FMath::FRand() < 0.35f)
            {
                Drop(EResourceKind::Cherry, 2.0f, At + FVector(90.0f, 60.0f, 0.0f));
            }
            break;

        case EFurnitureType::Tree:
            Drop(EResourceKind::Apple, 2.0f,
                 At + FVector(FMath::FRandRange(-140.0f, 140.0f), FMath::FRandRange(-140.0f, 140.0f), 0.0f));
            if (FMath::FRand() < 0.45f)
            {
                Drop(EResourceKind::Nut, 2.0f, At + FVector(-120.0f, 90.0f, 0.0f));
            }
            if (FMath::FRand() < 0.4f)
            {
                Drop(EResourceKind::Cone, 3.0f, At + FVector(120.0f, -90.0f, 0.0f));
            }
            if (FMath::FRand() < 0.3f)
            {
                Drop(EResourceKind::Acorn, 3.0f, At + FVector(60.0f, 130.0f, 0.0f));
            }
            if (FMath::FRand() < 0.3f)
            {
                Drop(EResourceKind::Brushwood, 3.0f, At + FVector(-150.0f, -60.0f, 0.0f));
            }
            break;

        case EFurnitureType::Mushrooms:
            Drop(EResourceKind::Mushroom, 2.0f, At + FVector(40.0f, 30.0f, 0.0f));
            break;

        case EFurnitureType::WildField:
            Drop(EResourceKind::Herb, 2.0f,
                 At + FVector(FMath::FRandRange(-120.0f, 120.0f), FMath::FRandRange(-120.0f, 120.0f), 0.0f));
            if (FMath::FRand() < 0.3f)
            {
                Drop(EResourceKind::Hops, 1.0f, At + FVector(100.0f, -70.0f, 0.0f));
            }
            break;

        case EFurnitureType::Spring:
        case EFurnitureType::Well:
            Drop(EResourceKind::Water, 2.0f, At + FVector(80.0f, 0.0f, 0.0f));
            break;

        case EFurnitureType::StonePile:
            Drop(Thing->Substance != EResourceKind::None ? Thing->Substance : EResourceKind::Limestone, 3.0f,
                 At + FVector(-140.0f, 90.0f, 0.0f));
            break;

        case EFurnitureType::ClayPit:
            Drop(EResourceKind::Clay, 3.0f, At + FVector(-120.0f, -80.0f, 0.0f));
            if (FMath::FRand() < 0.4f)
            {
                Drop(EResourceKind::Peat, 2.0f, At + FVector(120.0f, 90.0f, 0.0f));
            }
            break;

        case EFurnitureType::FishingSpot:
            Drop(EResourceKind::Fish, 1.0f, At + FVector(60.0f, -60.0f, 0.0f));
            break;

        default:
            break;
        }
    }

    // На полосе — то, что выросло: овощи и хлеба.
    for (TActorIterator<AFurnitureActor> It(World); It; ++It)
    {
        AFurnitureActor* Thing = *It;
        if (Thing && Thing->FurnitureType == EFurnitureType::GardenBed && Thing->Ripeness > 0.5f)
        {
            const FVector At = Thing->GetActorLocation();
            Drop(EResourceKind::Turnip, 2.0f, At + FVector(90.0f, 0.0f, 0.0f));
            if (FMath::FRand() < 0.5f)
            {
                Drop(EResourceKind::Cabbage, 2.0f, At + FVector(-90.0f, 0.0f, 0.0f));
            }
            if (FMath::FRand() < 0.35f)
            {
                Drop(EResourceKind::Carrot, 3.0f, At + FVector(0.0f, 80.0f, 0.0f));
            }
        }
    }

    UE_LOG(LogHumanCity, Warning, TEXT("Урожай разложен: %d припасов лежит на земле."), Piles);
}


void ACityGenerator::BuildTown()
{
    const float Radius = FMath::Max(5200.0f, NPCsCount * 260.0f);
    const int32 Sides = 8;
    const float WallHeight = 620.0f;

    TArray<FVector> Corners;
    for (int32 i = 0; i < Sides; ++i)
    {
        const float Angle = 360.0f / Sides * i;
        Corners.Add(FVector(FMath::Cos(FMath::DegreesToRadians(Angle)) * Radius,
                            FMath::Sin(FMath::DegreesToRadians(Angle)) * Radius, 0.0f));
    }

    // Стена с воротами на четырёх сторонах.
    for (int32 i = 0; i < Sides; ++i)
    {
        const FVector From = Corners[i];
        const FVector To = Corners[(i + 1) % Sides];
        BuildTower(From, 190.0f, WallHeight + 160.0f);

        if (i % 2 == 0)
        {
            const FVector Middle = (From + To) * 0.5f;
            const FVector Half = (To - From) * 0.5f;
            const FVector GateA = Middle - Half.GetSafeNormal() * 320.0f;
            const FVector GateB = Middle + Half.GetSafeNormal() * 320.0f;
            BuildWallRun(From, GateA, WallHeight);
            BuildWallRun(GateB, To, WallHeight);

            // Ворота: арка и башни по сторонам.
            Piece(BuildingMesh, TEXT("Stone"), Middle + FVector(0, 0, WallHeight - 60.0f),
                  FVector(140.0f, 660.0f, 160.0f), FRotator(0.0f, (To - From).Rotation().Yaw, 0.0f), true);
            BuildTower(GateA, 130.0f, WallHeight + 60.0f);
            BuildTower(GateB, 130.0f, WallHeight + 60.0f);

            // Дорога от ворот к торгу.
            const FVector Dir = -Middle.GetSafeNormal();
            const FVector RoadMiddle = Middle * 0.5f;
            Piece(RoadMesh, TEXT("Sidewalk"), RoadMiddle + FVector(0, 0, 10.0f),
                  FVector(Middle.Size2D(), 620.0f, 20.0f),
                  FRotator(0.0f, Dir.Rotation().Yaw, 0.0f), true);
        }
        else
        {
            BuildWallRun(From, To, WallHeight);
        }
    }

    // Торговая площадь и колодец на ней.
    Piece(RoadMesh, TEXT("Sidewalk"), FVector(0, 0, 12.0f), FVector(2600.0f, 2600.0f, 24.0f),
          FRotator::ZeroRotator, true);

    // Собор на площади.
    const FVector CathedralAt(0.0f, 1700.0f, 0.0f);
    SpawnBuilding(TEXT("Library"), CathedralAt, FVector(1400.0f, 900.0f, 900.0f));
    BuildTimberHouse(TEXT("Library"), CathedralAt, FVector(1400.0f, 900.0f, 0.0f), 2, 180.0f);
    if (Buildings.Num() > 0)
    {
        FBuildingInfo& Cathedral = Buildings.Last();
        Cathedral.EntranceLocation = CathedralAt + FVector(-820.0f, 0.0f, 0.0f);
        FurnishMedieval(Cathedral, TEXT("Library"));
        BuildTower(CathedralAt + FVector(600.0f, 380.0f, 0.0f), 150.0f, 1400.0f);
    }

    // Ратуша.
    const FVector HallAt(0.0f, -1700.0f, 0.0f);
    SpawnBuilding(TEXT("TownHall"), HallAt, FVector(1200.0f, 800.0f, 700.0f));
    BuildTimberHouse(TEXT("TownHall"), HallAt, FVector(1200.0f, 800.0f, 0.0f), 2, 0.0f);
    if (Buildings.Num() > 0)
    {
        FBuildingInfo& Hall = Buildings.Last();
        Hall.EntranceLocation = HallAt + FVector(720.0f, 0.0f, 0.0f);
        FurnishMedieval(Hall, TEXT("TownHall"));
    }

    if (Buildings.Num() > 0)
    {
        PlaceFurniture(Buildings[0], EFurnitureType::Well, FVector(700.0f, 0.0f, 0.0f), 0.0f);
        PlaceFurniture(Buildings[0], EFurnitureType::Well, FVector(-700.0f, 300.0f, 0.0f), 0.0f);
    }

    // Жилые ряды: дома тесно, вдоль улиц, фасадами наружу.
    const int32 Houses = FMath::Clamp(NPCsCount, 8, 40);
    for (int32 i = 0; i < Houses; ++i)
    {
        const float Angle = 360.0f / Houses * i + FMath::FRandRange(-6.0f, 6.0f);
        const float Away = Radius * FMath::FRandRange(0.45f, 0.78f);
        const FVector At(FMath::Cos(FMath::DegreesToRadians(Angle)) * Away,
                         FMath::Sin(FMath::DegreesToRadians(Angle)) * Away, 0.0f);

        const FVector Footprint(FMath::FRandRange(560.0f, 720.0f), FMath::FRandRange(480.0f, 640.0f), 0.0f);
        const float Yaw = At.Rotation().Yaw + 180.0f;
        const int32 Floors = FMath::RandRange(2, 3);

        SpawnBuilding(TEXT("Residential"), At, FVector(Footprint.X, Footprint.Y, Floors * 280.0f));
        UnderConstruction = Buildings.Num() > 0 ? &Buildings.Last() : nullptr;
        BuildTimberHouse(TEXT("Residential"), At, Footprint, Floors, Yaw);
        UnderConstruction = nullptr;

        if (Buildings.Num() > 0)
        {
            FBuildingInfo& House = Buildings.Last();
            House.EntranceLocation = At + FRotator(0.0f, Yaw, 0.0f).RotateVector(FVector(Footprint.X * 0.5f + 200.0f, 0.0f, 0.0f));
            FurnishMedieval(House, TEXT("Residential"));
        }
    }

    // Река за стеной и мост.
    BuildStream(FVector(-Radius * 1.6f, -Radius - 1400.0f, 0.0f),
                FVector(Radius * 1.6f, -Radius - 900.0f, 0.0f), 640.0f);

    // Поля, выпас и лес за стеной.
    BuildField(FVector(-Radius * 0.9f, Radius + 2200.0f, 0.0f), FVector(3200.0f, 2000.0f, 0.0f), TEXT("Grass"));
    BuildField(FVector(Radius * 0.9f, Radius + 2000.0f, 0.0f), FVector(3200.0f, 2000.0f, 0.0f), TEXT("Moss"));
    BuildWood(FVector(-Radius - 2600.0f, 0.0f, 0.0f), 2200.0f, 30);
    BuildWood(FVector(Radius + 2600.0f, 600.0f, 0.0f), 2200.0f, 30);

    if (Buildings.Num() > 0)
    {
        FBuildingInfo& Outside = Buildings[0];
        for (int32 i = 0; i < 8; ++i)
        {
            const float Angle = FMath::FRandRange(0.0f, 360.0f);
            const float Away = Radius * FMath::FRandRange(1.15f, 1.6f);
            PlaceFurniture(Outside, EFurnitureType::Spring,
                           FVector(FMath::Cos(FMath::DegreesToRadians(Angle)) * Away,
                                   FMath::Sin(FMath::DegreesToRadians(Angle)) * Away, 0.0f), 0.0f);
        }
    }
}

void ACityGenerator::SpawnVillagers()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    if (VillagePlans.Num() == 0)
    {
        FVillage::Plan(NPCsCount, 0x1377u, VillagePlans, VillageHouseholds);
    }

    int32 Castle = INDEX_NONE;
    TArray<int32> Homes;
    for (int32 B = 0; B < Buildings.Num(); ++B)
    {
        if (Buildings[B].Type == TEXT("TownHall") && Castle == INDEX_NONE)
        {
            Castle = B;
        }
        else if (Buildings[B].Type == TEXT("Residential"))
        {
            Homes.Add(B);
        }
    }
    HouseholdBuilding.Init(INDEX_NONE, VillageHouseholds);
    for (int32 H = 0; H < VillageHouseholds; ++H)
    {
        if (H == 0)
        {
            HouseholdBuilding[H] = Castle;
        }
        else if (Homes.IsValidIndex(H - 1))
        {
            HouseholdBuilding[H] = Homes[H - 1];
        }
        else
        {
            HouseholdBuilding[H] = Homes.Num() > 0 ? Homes.Last() : Castle;
        }
    }

    TArray<ACompleteHumanNPC*> People;
    FString Roster;
    for (const FPersonPlan& Plan : VillagePlans)
    {
        const int32 B = HouseholdBuilding.IsValidIndex(Plan.Household) ? HouseholdBuilding[Plan.Household] : INDEX_NONE;
        FBuildingInfo* Home = Buildings.IsValidIndex(B) ? &Buildings[B] : nullptr;
        FVector At = Home ? Home->EntranceLocation + FVector(FMath::FRandRange(-160.0f, 160.0f), FMath::FRandRange(-160.0f, 160.0f), 0.0f)
                          : GetStreetPoint();
        At.Z = GroundAt(At) + 140.0f;
        const FTransform T(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), At);
        ACompleteHumanNPC* NPC = World->SpawnActorDeferred<ACompleteHumanNPC>(ACompleteHumanNPC::StaticClass(), T, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if (!NPC)
        {
            People.Add(nullptr);
            continue;
        }
        if (NPC->IdentityComponent)
        {
            NPC->IdentityComponent->Preset(Plan.FirstName, Plan.LastName, Plan.Age, Plan.bFemale);
            NPC->IdentityComponent->bMasterTeacher = Plan.bMasterTeacher;
        }
        NPC->FinishSpawning(T);
        if (Home)
        {
            NPC->SetHome(Home->Location);
            Home->Occupants.Add(NPC);
        }
        People.Add(NPC);
        Roster += FString::Printf(TEXT("%s %s (%s, %d); "), *Plan.FirstName, *Plan.LastName,
            *FVillage::TitleOf(Plan.Role, Plan.Trade, Plan.bFemale, Plan.Age), FMath::FloorToInt(Plan.Age));
    }

    FVillage::Weave(World, People, VillagePlans);
    SetUpTrades(People);
    UE_LOG(LogHumanCity, Warning, TEXT("Королевство %s. Жители: %s"), *FVillage::Kingdom(), *Roster);

    bool bNewborn = FParse::Param(FCommandLine::Get(), TEXT("Newborn"));
    bool bPregnant = FParse::Param(FCommandLine::Get(), TEXT("Pregnant"));
    for (ACompleteHumanNPC* Woman : People)
    {
        if (!bNewborn && !bPregnant)
        {
            break;
        }
        UIdentityComponent* Id = Woman ? Woman->IdentityComponent.Get() : nullptr;
        ACompleteHumanNPC* Husband = Id ? Cast<ACompleteHumanNPC>(Id->Spouse.Get()) : nullptr;
        if (!Id || !Husband || !Id->bFemale || Id->Age < 18.0f || Id->Age > 42.0f || Id->IsRoyal())
        {
            continue;
        }
        if (bNewborn)
        {
            bNewborn = false;
            if (ACompleteHumanNPC* Baby = FVillage::Birth(World, Woman, Husband))
            {
                UE_LOG(LogHumanCity, Warning, TEXT("ДИТЯ: у %s родилось дитя %s"), *Id->FirstName,
                    Baby->IdentityComponent ? *Baby->IdentityComponent->FirstName : TEXT("?"));
            }
            continue;
        }
        bPregnant = false;
        Id->PregnantSince = 0.0f;
        UE_LOG(LogHumanCity, Warning, TEXT("ДИТЯ: %s ждёт ребёнка"), *Id->FirstName);
    }
}

void ACityGenerator::SetUpTrades(const TArray<ACompleteHumanNPC*>& People)
{
    auto StoreOf = [this](int32 Household, EFurnitureType Type) -> AFurnitureActor*
    {
        if (!HouseholdBuilding.IsValidIndex(Household) || !Buildings.IsValidIndex(HouseholdBuilding[Household]))
        {
            return nullptr;
        }
        for (AFurnitureActor* Thing : Buildings[HouseholdBuilding[Household]].Furniture)
        {
            if (Thing && Thing->FurnitureType == Type)
            {
                return Thing;
            }
        }
        return nullptr;
    };
    auto Put = [&StoreOf](int32 Household, EFurnitureType Type, EResourceKind Kind, float Amount)
    {
        if (AFurnitureActor* Store = StoreOf(Household, Type))
        {
            Store->Store(Kind, Amount);
        }
    };

    for (int32 H = 0; H < VillageHouseholds; ++H)
    {
        const float Scale = H == 0 ? 3.0f : 1.0f;
        Put(H, EFurnitureType::Barrel, EResourceKind::Water, 8.0f * Scale);
        Put(H, EFurnitureType::Stove, EResourceKind::Bread, 3.0f * Scale);
        Put(H, EFurnitureType::Stove, EResourceKind::CookedFood, 2.0f * Scale);
        Put(H, EFurnitureType::Shed, EResourceKind::Grain, 10.0f * Scale);
        Put(H, EFurnitureType::Shed, EResourceKind::Flour, 4.0f * Scale);
        Put(H, EFurnitureType::Shed, EResourceKind::RawFood, 6.0f * Scale);
        Put(H, EFurnitureType::Shed, EResourceKind::Firewood, 8.0f * Scale);
        Put(H, EFurnitureType::Shed, EResourceKind::Salt, 1.0f);
        Put(H, EFurnitureType::Shed, EResourceKind::Axe, 1.0f);
        Put(H, EFurnitureType::Shed, EResourceKind::Sickle, 1.0f);
        Put(H, EFurnitureType::Shed, EResourceKind::Shovel, 1.0f);
        Put(H, EFurnitureType::Wardrobe, EResourceKind::Cloth, 1.0f * Scale);
        Put(H, EFurnitureType::Wardrobe, EResourceKind::Thread, 2.0f * Scale);
        Put(H, EFurnitureType::Wardrobe, EResourceKind::Needle, 1.0f);
        Put(H, EFurnitureType::Wardrobe, EResourceKind::Shirt, 1.0f * Scale);
    }
    Put(0, EFurnitureType::Shed, EResourceKind::Fish, 6.0f);
    Put(0, EFurnitureType::Shed, EResourceKind::Meat, 4.0f);
    Put(0, EFurnitureType::Shed, EResourceKind::Honey, 2.0f);
    Put(0, EFurnitureType::Wardrobe, EResourceKind::Flax, 4.0f);

    auto Yard = [this, &StoreOf](int32 Household, float Side) -> FVector
    {
        if (AFurnitureActor* Barn = StoreOf(Household, EFurnitureType::Shed))
        {
            const FVector At = Barn->GetActorLocation() + FVector(-420.0f * Side, 0.0f, 0.0f);
            return FVector(At.X, At.Y, GroundAt(At));
        }
        if (HouseholdBuilding.IsValidIndex(Household) && Buildings.IsValidIndex(HouseholdBuilding[Household]))
        {
            const FVector At = Buildings[HouseholdBuilding[Household]].EntranceLocation + FVector(0.0f, 300.0f, 0.0f);
            return FVector(At.X, At.Y, GroundAt(At));
        }
        return FVector::ZeroVector;
    };
    auto Station = [this, &Yard](int32 Household, EFurnitureType Type, float Side)
    {
        if (!HouseholdBuilding.IsValidIndex(Household) || !Buildings.IsValidIndex(HouseholdBuilding[Household]))
        {
            return;
        }
        const FVector At = Yard(Household, Side);
        if (!At.IsZero())
        {
            PlaceFurniture(Buildings[HouseholdBuilding[Household]], Type, At, 0.0f);
        }
    };

    int32 Lot = 0;
    auto Sell = [this, &Lot](ACompleteHumanNPC* Who, EResourceKind Kind, float Amount)
    {
        if (!Who || VillageStalls.Num() == 0)
        {
            return;
        }
        if (AFurnitureActor* Stall = VillageStalls[Lot++ % VillageStalls.Num()])
        {
            Stall->PutOnSale(Who, Kind, Amount, FVillage::PriceOf(Kind));
        }
    };

    for (int32 I = 0; I < VillagePlans.Num() && I < People.Num(); ++I)
    {
        const FPersonPlan& Plan = VillagePlans[I];
        ACompleteHumanNPC* Who = People[I];
        const int32 H = Plan.Household;
        const FName Trade = Plan.Trade;
        if (Trade == TEXT("Fishing"))
        {
            Put(H, EFurnitureType::Shed, EResourceKind::Rod, 2.0f);
            Put(H, EFurnitureType::Shed, EResourceKind::Fish, 4.0f);
            Sell(Who, EResourceKind::Fish, 4.0f);
        }
        else if (Trade == TEXT("Weaving"))
        {
            Station(H, EFurnitureType::Loom, -1.0f);
            Put(H, EFurnitureType::Wardrobe, EResourceKind::Flax, 6.0f);
            Put(H, EFurnitureType::Wardrobe, EResourceKind::Thread, 4.0f);
            Sell(Who, EResourceKind::Cloth, 2.0f);
        }
        else if (Trade == TEXT("Smithing"))
        {
            Station(H, EFurnitureType::Forge, 1.0f);
            Put(H, EFurnitureType::Shed, EResourceKind::Iron, 3.0f);
            Put(H, EFurnitureType::Shed, EResourceKind::IronOre, 4.0f);
            Put(H, EFurnitureType::Shed, EResourceKind::Charcoal, 8.0f);
            Put(H, EFurnitureType::Shed, EResourceKind::Hammer, 1.0f);
            Sell(Who, EResourceKind::Sickle, 1.0f);
            Sell(Who, EResourceKind::Tool, 4.0f);
        }
        else if (Trade == TEXT("Sewing"))
        {
            Put(H, EFurnitureType::Wardrobe, EResourceKind::Cloth, 4.0f);
            Put(H, EFurnitureType::Wardrobe, EResourceKind::Thread, 4.0f);
            Put(H, EFurnitureType::Wardrobe, EResourceKind::Needle, 2.0f);
            Sell(Who, EResourceKind::Shirt, 1.0f);
        }
        else if (Trade == TEXT("Farming"))
        {
            Station(H, EFurnitureType::Sawhorse, 1.0f);
            Put(H, EFurnitureType::Shed, EResourceKind::Grain, 12.0f);
            Sell(Who, EResourceKind::Grain, 6.0f);
        }
        else if (Trade == TEXT("Carpentry"))
        {
            Station(H, EFurnitureType::Workbench, -1.0f);
            Put(H, EFurnitureType::Shed, EResourceKind::Wood, 4.0f);
            Put(H, EFurnitureType::Shed, EResourceKind::Plank, 4.0f);
            Sell(Who, EResourceKind::Plank, 4.0f);
        }
        else if (Trade == TEXT("Baking"))
        {
            Put(H, EFurnitureType::Shed, EResourceKind::Flour, 10.0f);
            Sell(Who, EResourceKind::Bread, 6.0f);
        }
        else if (Trade == TEXT("Pottery"))
        {
            Station(H, EFurnitureType::PotteryWheel, 1.0f);
            Put(H, EFurnitureType::Shed, EResourceKind::Clay, 8.0f);
            Put(H, EFurnitureType::Shed, EResourceKind::Pot, 4.0f);
            Sell(Who, EResourceKind::Pot, 4.0f);
        }
        else if (Trade == TEXT("Herbalism"))
        {
            Put(H, EFurnitureType::Wardrobe, EResourceKind::Herb, 6.0f);
            Put(H, EFurnitureType::Wardrobe, EResourceKind::Medicine, 2.0f);
            Sell(Who, EResourceKind::Medicine, 2.0f);
        }
        else if (Trade == TEXT("Woodcutting"))
        {
            Put(H, EFurnitureType::Shed, EResourceKind::Axe, 1.0f);
        }
    }

    TArray<TArray<FName>> HouseTrades;
    HouseTrades.SetNum(VillageHouseholds);
    for (const FPersonPlan& Plan : VillagePlans)
    {
        if (HouseTrades.IsValidIndex(Plan.Household) && !Plan.Trade.IsNone())
        {
            HouseTrades[Plan.Household].AddUnique(Plan.Trade);
        }
    }
    int32 Shelved = 0;
    for (int32 H = 0; H < VillageHouseholds; ++H)
    {
        AFurnitureActor* Table = StoreOf(H, EFurnitureType::Table);
        if (!Table || !HouseholdBuilding.IsValidIndex(H) || !Buildings.IsValidIndex(HouseholdBuilding[H]))
        {
            continue;
        }
        const FVector Home = Buildings[HouseholdBuilding[H]].Location;
        TArray<FName> Shelf;
        FVillage::HomeBooks(HouseTrades[H], Shelf);
        if (H == 0)
        {
            Shelf.AddUnique(TEXT("VillageBread"));
            Shelf.AddUnique(TEXT("VillageTrade"));
        }
        const FBox Top = Table->GetComponentsBoundingBox();
        for (int32 K = 0; K < Shelf.Num(); ++K)
        {
            const FVector At(Top.GetCenter().X - 24.0f + 16.0f * (K % 4), Top.GetCenter().Y - 14.0f + 28.0f * ((K / 4) % 2), Top.Max.Z + 2.0f + 4.5f * (K / 8));
            if (ABookActor* Book = SpawnBook(Shelf[K], At))
            {
                if (Book->Affordances)
                {
                    Book->Affordances->bPrivate = true;
                    Book->Affordances->OwnerAnchor = Home;
                }
                ++Shelved;
            }
        }
    }
    UE_LOG(LogHumanCity, Warning, TEXT("КНИГИ: по избам разложено %d книг о ремёслах"), Shelved);

    for (AFurnitureActor* Stall : VillageStalls)
    {
        if (Stall)
        {
            Stall->BuildOffers();
        }
    }
}
