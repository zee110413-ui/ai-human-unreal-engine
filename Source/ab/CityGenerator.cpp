// CityGenerator.cpp

#include "CityGenerator.h"
#include "ImportedModels.h"
#include "PersonalityComponent.h"
#include "EmotionComponent.h"
#include "NeedComponent.h"
#include "MotivationComponent.h"
#include "IdentityComponent.h"
#include "SocialComponent.h"
#include "SpeechComponent.h"
#include "HumanWorldSubsystem.h"
#include "Textbook.h"
#include "Crafts.h"
#include "Elements.h"
#include "TerrainGrid.h"
#include "MatterSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "FurnitureActor.h"
#include "BookActor.h"
#include "Textbook.h"
#include "PlaceActor.h"
#include "AffordanceComponent.h"
#include "CompleteHumanAI.h"
#include "HumanWorldSubsystem.h"
#include "MemoryComponent.h"
#include "IdentityComponent.h"
#include "SocialComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

ACityGenerator::ACityGenerator()
{
    // Город живёт и без людей: грядки зреют, товар подвозят.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 2.0f;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        BuildingMesh = CubeMesh.Object;
        FurnitureMesh = CubeMesh.Object;
        RoadMesh = CubeMesh.Object;
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylMesh.Succeeded()) { CylinderMesh = CylMesh.Object; }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphMesh.Succeeded()) { SphereMesh = SphMesh.Object; }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (ConMesh.Succeeded()) { ConeMesh = ConMesh.Object; }

    BuildingInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BuildingInstances"));
    BuildingInstances->SetupAttachment(RootComponent);
    if (BuildingMesh)
    {
        BuildingInstances->SetStaticMesh(BuildingMesh);
    }

    RoadInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadInstances"));
    RoadInstances->SetupAttachment(RootComponent);
    RoadInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (RoadMesh)
    {
        RoadInstances->SetStaticMesh(RoadMesh);
    }

    Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
    Ground->SetupAttachment(RootComponent);
    if (BuildingMesh)
    {
        Ground->SetStaticMesh(BuildingMesh);
    }
}

void ACityGenerator::BeginPlay()
{
    Super::BeginPlay();

    const TCHAR* Cmd = FCommandLine::Get();
    if (FParse::Param(Cmd, TEXT("Village")))      Settlement = ESettlement::Village;
    else if (FParse::Param(Cmd, TEXT("Town")))    Settlement = ESettlement::Town;
    else if (FParse::Param(Cmd, TEXT("Modern")))  Settlement = ESettlement::Modern;

    UE_LOG(LogHumanCity, Warning, TEXT("Поселение: %s"),
        Settlement == ESettlement::Village ? TEXT("деревня")
        : (Settlement == ESettlement::Town ? TEXT("средневековый город") : TEXT("прежний город")));
    FVillage::SetMedieval(Settlement != ESettlement::Modern);

    if (bGenerateOnBeginPlay)
    {
        GenerateCity();
        RegisterPlacesInWorld();
        SpawnNPCs();
    }
}

void ACityGenerator::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Хозяйство идёт своим чередом: грядки зреют, на лоток подвозят товар.
    // Это происходит независимо от людей — мир не ждёт, пока на него посмотрят.
    const UHumanWorldSubsystem* WorldMind = GetWorld()
        ? GetWorld()->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    const float GameDelta = WorldMind ? WorldMind->RealToGameSeconds(DeltaSeconds) : DeltaSeconds;

    for (FBuildingInfo& Building : Buildings)
    {
        for (AFurnitureActor* Item : Building.Furniture)
        {
            if (IsValid(Item))
            {
                Item->AdvanceHousehold(GameDelta);
            }
        }
    }

    if (const UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        const FClimate& Sky = Matter->Climate();
        const float Wet = Sky.Wetness;
        const float Frost = FMath::Clamp(Sky.SnowOnGround / 30.0f, 0.0f, 1.0f);
        if (FMath::Abs(Wet - ShownWetness) > 0.02f || FMath::Abs(Frost - ShownFrost) > 0.02f)
        {
            ShownWetness = Wet;
            ShownFrost = Frost;
            for (const TPair<FString, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : Instancers)
            {
                if (Pair.Value)
                {
                    Pair.Value->SetCustomPrimitiveDataFloat(0, Wet);
                    Pair.Value->SetCustomPrimitiveDataFloat(3, Frost);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Постройка
// ---------------------------------------------------------------------------

void ACityGenerator::GenerateCity()
{
    Buildings.Reset();
    LitLampsPlaced = 0;

    // Сносим то, что было построено прежде.
    for (const TPair<FString, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : Instancers)
    {
        if (Pair.Value)
        {
            Pair.Value->ClearInstances();
        }
    }
    if (BuildingInstances) BuildingInstances->ClearInstances();
    if (RoadInstances)     RoadInstances->ClearInstances();

    LoadPalette();

    const FVector Origin = GetActorLocation();
    const float Width = BlocksX * BlockSize;
    const float Depth = BlocksY * BlockSize;

    // --- Земля --------------------------------------------------------------
    if (Ground)
    {
        if (bSpawnGround)
        {
            Ground->SetVisibility(true);
            Ground->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Ground->SetRelativeLocation(FVector(Width * 0.5f - BlockSize * 0.5f,
                                                Depth * 0.5f - BlockSize * 0.5f, -50.0f));
            Ground->SetRelativeScale3D(FVector((Width + BlockSize * 4.0f) / 100.0f,
                                               (Depth + BlockSize * 4.0f) / 100.0f,
                                               1.0f));
            if (UMaterialInterface* GrassMat = Mat(TEXT("Grass")))
            {
                Ground->SetMaterial(0, GrassMat);
            }
        }
        else
        {
            Ground->SetVisibility(false);
            Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }

    if (Settlement != ESettlement::Modern)
    {
        // Под средневековым поселением лежит настоящая земля: дёрн, глина,
        // песок, известняк, гранит и руда — блоками, которые можно копать.
        if (Ground)
        {
            Ground->SetVisibility(false);
            Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }

        if (!Terrain)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            // Земля кладётся так, чтобы обычная поверхность была около нуля,
            // а холм поднимался над ней.
            Terrain = GetWorld()->SpawnActor<ATerrainGrid>(ATerrainGrid::StaticClass(),
                FVector(4800.0f, 0.0f, -600.0f), FRotator::ZeroRotator, Params);
        }
        if (Terrain)
        {
            Terrain->Generate();
        }

        if (Settlement == ESettlement::Village)
        {
            BuildVillage();
        }
        else
        {
            BuildTown();
        }
        if (Terrain)
        {
            Terrain->RebuildDirty();
        }
        return;
    }

    // --- Улицы --------------------------------------------------------------
    BuildStreets(Origin, Width, Depth);

    // --- Кварталы -----------------------------------------------------------
    for (int32 X = 0; X < BlocksX; ++X)
    {
        for (int32 Y = 0; Y < BlocksY; ++Y)
        {
            const FVector BlockCentre = Origin + FVector(X * BlockSize, Y * BlockSize, 0.0f);

            // Раскладка кварталов: жильё преобладает, но есть где работать,
            // где поесть и куда просто сходить.
            // =============================================================
            //  ЧТО ГДЕ СТОИТ
            //
            //  В городе должно быть куда пойти, кроме дома и работы:
            //  лечиться, мыться, учиться ремеслу, читать, торговать.
            //  Жильё всё равно преобладает — как и в настоящем городе.
            // =============================================================
            FString Type;
            const int32 Pattern = (X * 3 + Y * 5) % 11;
            if (Pattern == 0 || Pattern == 6)      Type = TEXT("Commercial");
            else if (Pattern == 2 || Pattern == 9) Type = TEXT("Office");
            else if (Pattern == 4)                 Type = TEXT("Park");
            else if (Pattern == 8)                 Type = TEXT("Workshop");
            else if (Pattern == 5)                 Type = TEXT("Office");
            else                                   Type = TEXT("Residential");

            // Единственные в городе здания — по одному, на своих местах.
            // Школу ставим в середине, чтобы до неё доходили отовсюду.
            const int32 MidX = BlocksX / 2;
            const int32 MidY = BlocksY / 2;

            if (X == MidX && Y == MidY)                      Type = TEXT("School");
            else if (X == MidX && Y == MidY - 1)             Type = TEXT("Library");
            else if (X == MidX - 1 && Y == MidY)             Type = TEXT("Hospital");
            else if (X == MidX + 1 && Y == MidY)             Type = TEXT("Market");
            else if (X == MidX && Y == MidY + 1)             Type = TEXT("TownHall");
            else if (X == 0 && Y == 0)                       Type = TEXT("Bathhouse");
            else if (X == BlocksX - 1 && Y == BlocksY - 1)   Type = TEXT("Bakery");

            // Сквер — открытое пространство, а не здание.
            if (Type == TEXT("Park"))
            {
                const float ParkSize = BlockSize - RoadWidth - SidewalkWidth * 2.0f;
                SpawnBuilding(Type, BlockCentre, FVector(ParkSize, ParkSize, 1.0f));
                BuildPark(BlockCentre, ParkSize);
                continue;
            }

            // Этажность задаёт назначение: контора выше жилья, лавка ниже.
            int32 Floors = 3;
            if (Type == TEXT("Office"))            Floors = FMath::RandRange(5, 11);
            else if (Type == TEXT("Commercial"))   Floors = FMath::RandRange(2, 3);
            else if (Type == TEXT("School"))       Floors = 3;
            else if (Type == TEXT("TownHall"))     Floors = 4;
            else if (Type == TEXT("Hospital"))     Floors = 3;
            else if (Type == TEXT("Library"))      Floors = 2;
            else if (Type == TEXT("Workshop"))     Floors = 1;
            else if (Type == TEXT("Bathhouse"))    Floors = 1;
            else if (Type == TEXT("Bakery"))       Floors = 2;
            else if (Type == TEXT("Market"))       Floors = 1;
            else                                   Floors = FMath::RandRange(3, 6);

            const float Height = Floors * FloorHeight;

            // Общественные здания шире жилых: в них есть залы.
            float Spread = 0.48f;
            if (Type == TEXT("School"))                                   Spread = 0.78f;
            else if (Type == TEXT("Market") || Type == TEXT("Hospital"))  Spread = 0.64f;
            else if (Type == TEXT("Library") || Type == TEXT("TownHall")) Spread = 0.58f;
            else if (Type == TEXT("Workshop") || Type == TEXT("Bathhouse")
                  || Type == TEXT("Bakery"))                              Spread = 0.54f;
            const float FootprintX = BlockSize * FMath::FRandRange(Spread - 0.14f, Spread);
            const float FootprintY = BlockSize * FMath::FRandRange(Spread - 0.14f, Spread);

            SpawnBuilding(Type, BlockCentre, FVector(FootprintX, FootprintY, Height));
            BuildHouse(Type, BlockCentre, FVector(FootprintX, FootprintY, 0.0f), Height);

            // Вещи внутри. Именно они дают человеку возможность что-то
            // сделать: пьют из раковины, спят в кровати, готовят на плите.
            if (Buildings.Num() > 0)
            {
                FurnishRoom(Buildings.Last(), Type);
            }

            // Немного зелени во дворе — город перестаёт быть камнем.
            if (bGreenery && FMath::FRand() < 0.7f)
            {
                const FVector Yard = BlockCentre + FVector(
                    -FootprintX * 0.5f - FMath::FRandRange(150.0f, 320.0f),
                    FMath::FRandRange(-FootprintY * 0.5f, FootprintY * 0.5f), 0.0f);
                PlantTree(Yard, FMath::FRandRange(0.7f, 1.1f));
            }

            // --- Хозяйство во дворе ---------------------------------------
            //
            // Отсюда берётся еда. У жилого дома грядка и бочка с водой,
            // у лавки — лоток с товаром. Без них припасы неоткуда взять,
            // а с ними у города появляется своё хозяйство.
            if (Buildings.Num() > 0)
            {
                FBuildingInfo& Yard = Buildings.Last();
                const FVector Side = BlockCentre + FVector(0.0f, FootprintY * 0.5f + 260.0f, 0.0f);

                // Ни лотков с едой, ни готовых грядок: город не кормит никого.
                // Есть только дикое — родник, кусты, грибница, поле.
                if (FMath::FRand() < 0.5f)
                {
                    PlaceFurniture(Yard, EFurnitureType::Spring, Side, 0.0f);
                }
                if (FMath::FRand() < 0.6f)
                {
                    PlaceFurniture(Yard, EFurnitureType::Bush,
                                   Side + FVector(-320.0f, 60.0f, 0.0f), FMath::FRandRange(0.0f, 360.0f));
                }
                if (FMath::FRand() < 0.45f)
                {
                    PlaceFurniture(Yard, EFurnitureType::Mushrooms,
                                   Side + FVector(320.0f, 120.0f, 0.0f), 0.0f);
                }
                if (FMath::FRand() < 0.4f)
                {
                    PlaceFurniture(Yard, EFurnitureType::WildField,
                                   Side + FVector(0.0f, 430.0f, 0.0f), 0.0f);
                }
                // Мастерских, печей и станков в городе нет: их делают сами,
                // прочитав, как. Мир даёт только то, что растёт и лежит.
                if (FMath::FRand() < 0.55f)
                {
                    PlaceFurniture(Yard, EFurnitureType::Tree,
                                   BlockCentre + FVector(-FootprintX * 0.5f - 420.0f,
                                                         FootprintY * 0.35f, 0.0f),
                                   FMath::FRandRange(0.0f, 360.0f));
                }
                // Порода лежит не где попало: на холмах известняк, в скалах
                // руда и уголь, в низине глина, на болоте торф, у берега песок.
                {
                    const float FromCentre = FVector::Dist2D(BlockCentre, FVector::ZeroVector);
                    const float Roll = FMath::FRand();

                    FName Ground0 = TEXT("равнина");
                    if (FromCentre > 9000.0f)      Ground0 = TEXT("скала");
                    else if (FromCentre > 5200.0f) Ground0 = TEXT("холм");
                    if (Roll < 0.14f)              Ground0 = TEXT("низина");
                    else if (Roll < 0.22f)         Ground0 = TEXT("болото");
                    else if (Roll < 0.30f)         Ground0 = TEXT("берег");

                    TArray<const FMineral*> Here;
                    FElements::MineralsOfGround(Ground0, Here);

                    if (Here.Num() > 0 && FMath::FRand() < 0.5f)
                    {
                        const FMineral* Found = Here[0];
                        float Best = -1.0f;
                        for (const FMineral* Candidate : Here)
                        {
                            const float Weight = FMath::FRand() * Candidate->Common;
                            if (Weight > Best)
                            {
                                Best = Weight;
                                Found = Candidate;
                            }
                        }

                        const bool bRock = Found->Hardness >= 2.5f && Found->Kind != EResourceKind::Peat
                                        && Found->Kind != EResourceKind::Clay && Found->Kind != EResourceKind::Sand;

                        const FVector At = Side + FVector(bRock ? -620.0f : 620.0f, 240.0f, 0.0f);
                        if (AFurnitureActor* Node = PlaceFurniture(Yard,
                                bRock ? EFurnitureType::StonePile : EFurnitureType::ClayPit,
                                At, FMath::FRandRange(0.0f, 360.0f)))
                        {
                            Node->Substance = Found->Kind;
                            Node->Name = Found->Name;
                            Node->RefreshCraftOffers();
                        }
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  ОБЛИК ГОРОДА
//
//  Всё собирается из простых форм и материалов стартового набора: ничего
//  скачивать не нужно. Задача — не фотореализм, а читаемый город: чтобы
//  было видно, где дом, где контора, где улица, а где сквер.
// ---------------------------------------------------------------------------

void ACityGenerator::LoadPalette()
{
    Palette.Reset();

    UMatterSubsystem* Matter = UMatterSubsystem::Get(this);
    auto Add = [this, Matter](FName Key, EMatterLook Look, const TCHAR* Fallback)
    {
        UMaterialInterface* M = Matter ? Matter->Look(Look) : nullptr;
        if (!M)
        {
            M = LoadObject<UMaterialInterface>(nullptr,
                *FString::Printf(TEXT("/Game/StarterContent/Materials/%s.%s"), Fallback, Fallback));
        }
        if (M)
        {
            Palette.Add(Key, M);
        }
    };

    Add(TEXT("Grass"), EMatterLook::Lawn, TEXT("M_Ground_Grass"));
    Add(TEXT("Gravel"), EMatterLook::Gravel, TEXT("M_Ground_Gravel"));
    Add(TEXT("Asphalt"), EMatterLook::Asphalt, TEXT("M_Concrete_Poured"));
    Add(TEXT("Sidewalk"), EMatterLook::Cobble, TEXT("M_CobbleStone_Smooth"));
    Add(TEXT("Line"), EMatterLook::Plaster, TEXT("M_Basic_Wall"));
    Add(TEXT("BrickOld"), EMatterLook::OldBrick, TEXT("M_Brick_Clay_Old"));
    Add(TEXT("BrickNew"), EMatterLook::Brick, TEXT("M_Brick_Clay_New"));
    Add(TEXT("Stone"), EMatterLook::CutStone, TEXT("M_Brick_Cut_Stone"));
    Add(TEXT("Panels"), EMatterLook::Panels, TEXT("M_Concrete_Panels"));
    Add(TEXT("Tiles"), EMatterLook::Tiles, TEXT("M_Concrete_Tiles"));
    Add(TEXT("Sandstone"), EMatterLook::Sandstone, TEXT("M_Rock_Sandstone"));
    Add(TEXT("Grime"), EMatterLook::Concrete, TEXT("M_Concrete_Grime"));
    Add(TEXT("Slate"), EMatterLook::Slate, TEXT("M_Rock_Slate"));
    Add(TEXT("Glass"), EMatterLook::Glass, TEXT("M_Glass"));
    Add(TEXT("Steel"), EMatterLook::Steel, TEXT("M_Metal_Steel"));
    Add(TEXT("Wood"), EMatterLook::Oak, TEXT("M_Wood_Oak"));
    Add(TEXT("Moss"), EMatterLook::Moss, TEXT("M_Ground_Moss"));
}

UMaterialInterface* ACityGenerator::Mat(FName Key) const
{
    if (const TObjectPtr<UMaterialInterface>* Found = Palette.Find(Key))
    {
        return Found->Get();
    }
    return nullptr;
}

UInstancedStaticMeshComponent* ACityGenerator::Instancer(UStaticMesh* Mesh, UMaterialInterface* Material, const FString& Key)
{
    if (!Mesh)
    {
        return nullptr;
    }

    if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Instancers.Find(Key))
    {
        return Found->Get();
    }

    UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this);
    Component->SetStaticMesh(Mesh);
    if (Material)
    {
        Component->SetMaterial(0, Material);
    }
    // Подвижность должна совпадать с корнем актора: статичную деталь
    // к подвижному корню движок прикрепить откажется, и город просто
    // не появится. Город строится на лету, так что статичным он быть и не может.
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetupAttachment(RootComponent);
    Component->RegisterComponent();
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Instancers.Add(Key, Component);
    return Component;
}

void ACityGenerator::Piece(UStaticMesh* Mesh, FName MaterialKey, const FVector& Centre, const FVector& SizeCm,
                           const FRotator& Rotation, bool bCollision)
{
    UMaterialInterface* Material = Mat(MaterialKey);
    const FString Key = FString::Printf(TEXT("%s|%s|%d"),
        Mesh ? *Mesh->GetName() : TEXT("none"),
        *MaterialKey.ToString(),
        bCollision ? 1 : 0);

    UInstancedStaticMeshComponent* Component = Instancer(Mesh, Material, Key);
    if (!Component)
    {
        return;
    }

    if (bCollision && Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
    {
        Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Component->SetCollisionProfileName(TEXT("BlockAll"));
    }

    // Базовые формы движка — 100 см, поэтому размер в сантиметрах делим на 100.
    FTransform T;
    T.SetLocation(Centre);
    T.SetRotation(Rotation.Quaternion());
    T.SetScale3D(SizeCm / 100.0f);
    Component->AddInstance(T, true);
}

// ---------------------------------------------------------------------------
//  Улицы
// ---------------------------------------------------------------------------

void ACityGenerator::BuildStreets(const FVector& Origin, float Width, float Depth)
{
    // Улицы идут по границам кварталов, а дома стоят в середине квартала.
    const float HalfBlock = BlockSize * 0.5f;
    const float SpanX = Width + BlockSize;
    const float SpanY = Depth + BlockSize;
    const float CentreX = Origin.X + Width * 0.5f - HalfBlock;
    const float CentreY = Origin.Y + Depth * 0.5f - HalfBlock;

    auto Street = [&](bool bAlongY, float Offset)
    {
        const FVector RoadCentre = bAlongY
            ? FVector(Offset, CentreY, Origin.Z + 3.0f)
            : FVector(CentreX, Offset, Origin.Z + 3.0f);

        const FVector RoadSize = bAlongY
            ? FVector(RoadWidth, SpanY, 6.0f)
            : FVector(SpanX, RoadWidth, 6.0f);

        Piece(BuildingMesh, TEXT("Asphalt"), RoadCentre, RoadSize);

        // Тротуары по обе стороны — чуть приподняты, из брусчатки.
        for (int32 Side = -1; Side <= 1; Side += 2)
        {
            const float Shift = (RoadWidth + SidewalkWidth) * 0.5f * Side;
            const FVector WalkCentre = bAlongY
                ? FVector(Offset + Shift, CentreY, Origin.Z + 9.0f)
                : FVector(CentreX, Offset + Shift, Origin.Z + 9.0f);
            const FVector WalkSize = bAlongY
                ? FVector(SidewalkWidth, SpanY, 18.0f)
                : FVector(SpanX, SidewalkWidth, 18.0f);

            Piece(BuildingMesh, TEXT("Sidewalk"), WalkCentre, WalkSize);
        }

        // Прерывистая осевая разметка.
        const float Span = bAlongY ? SpanY : SpanX;
        const float Start = (bAlongY ? CentreY : CentreX) - Span * 0.5f;
        const int32 Dashes = FMath::Max(1, FMath::FloorToInt(Span / 320.0f));
        for (int32 i = 0; i < Dashes; ++i)
        {
            const float Along = Start + (i + 0.5f) * (Span / Dashes);
            const FVector MarkCentre = bAlongY
                ? FVector(Offset, Along, Origin.Z + 7.0f)
                : FVector(Along, Offset, Origin.Z + 7.0f);
            const FVector MarkSize = bAlongY
                ? FVector(16.0f, 150.0f, 3.0f)
                : FVector(150.0f, 16.0f, 3.0f);

            Piece(BuildingMesh, TEXT("Line"), MarkCentre, MarkSize);
        }
    };

    for (int32 X = 0; X <= BlocksX; ++X)
    {
        Street(true, Origin.X + X * BlockSize - HalfBlock);
    }
    for (int32 Y = 0; Y <= BlocksY; ++Y)
    {
        Street(false, Origin.Y + Y * BlockSize - HalfBlock);
    }

    // Заметим: никакой сети маршрутов город людям не выдаёт. Улицы — просто
    // свободное место между домами. Как по ним пройти, человек выясняет
    // сам, глазами, и это его дело, а не города.

    // --- Фонари по перекрёсткам ---------------------------------------------
    if (bStreetLamps)
    {
        for (int32 X = 0; X <= BlocksX; ++X)
        {
            for (int32 Y = 0; Y <= BlocksY; ++Y)
            {
                const FVector Corner(
                    Origin.X + X * BlockSize - HalfBlock + (RoadWidth * 0.5f + SidewalkWidth * 0.5f),
                    Origin.Y + Y * BlockSize - HalfBlock + (RoadWidth * 0.5f + SidewalkWidth * 0.5f),
                    Origin.Z + 18.0f);
                PlaceLamp(Corner);
            }
        }
    }
}

void ACityGenerator::PlaceLamp(const FVector& Base)
{
    // Столб и плафон.
    Piece(CylinderMesh, TEXT("Steel"), Base + FVector(0, 0, 230.0f), FVector(18.0f, 18.0f, 460.0f));
    Piece(SphereMesh, TEXT("Line"), Base + FVector(0, 0, 475.0f), FVector(46.0f, 46.0f, 46.0f));

    // Настоящий свет — только у части фонарей: сотня источников посадит
    // любую сцену, а разница в картинке будет невелика.
    if (LitLampsPlaced < MaxLitLamps)
    {
        ++LitLampsPlaced;

        UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
        Light->SetupAttachment(RootComponent);
        Light->RegisterComponent();
        Light->SetWorldLocation(Base + FVector(0, 0, 470.0f));
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetIntensity(3600.0f);
        Light->SetAttenuationRadius(1100.0f);
        Light->SetLightColor(FLinearColor(1.0f, 0.86f, 0.62f));
        Light->SetCastShadows(false);   // тени от каждого фонаря не нужны
    }
}

// ---------------------------------------------------------------------------
//  Дома
// ---------------------------------------------------------------------------

void ACityGenerator::BuildHouse(const FString& Type, const FVector& Centre, const FVector& Footprint, float Height)
{
    // Каждому типу застройки — своя фактура. По ней город читается с высоты:
    // жильё кирпичное, конторы бетонно-стеклянные, торговля песчаниковая.
    FName WallMat = TEXT("BrickOld");
    FName TrimMat = TEXT("Slate");

    if (Type == TEXT("Office"))
    {
        WallMat = (FMath::FRand() < 0.5f) ? TEXT("Panels") : TEXT("Tiles");
        TrimMat = TEXT("Steel");
    }
    else if (Type == TEXT("Commercial"))
    {
        WallMat = TEXT("Sandstone");
        TrimMat = TEXT("Stone");
    }
    else if (Type == TEXT("School"))
    {
        WallMat = TEXT("BrickNew");
        TrimMat = TEXT("Stone");
    }
    else
    {
        WallMat = (FMath::FRand() < 0.5f) ? TEXT("BrickOld") : TEXT("BrickNew");
    }

    // --- Цоколь --------------------------------------------------------------
    Piece(BuildingMesh, TrimMat,
          Centre + FVector(0, 0, 30.0f),
          FVector(Footprint.X + 60.0f, Footprint.Y + 60.0f, 60.0f),
          FRotator::ZeroRotator, true);

    // =======================================================================
    //  ПЕРВЫЙ ЭТАЖ — НАСТОЯЩЕЕ ПОМЕЩЕНИЕ
    //
    //  Раньше дом был сплошным кубом, и «попить» висело абстракцией на
    //  подъезде: человек подходил к глухой стене и пил из ниоткуда.
    //  Теперь у дома есть комната с дверным проёмом, а внутри стоят вещи —
    //  раковина, кровать, плита. Пить он ходит к раковине.
    // =======================================================================
    const float WallThickness = 26.0f;
    const float RoomHeight = FloorHeight;
    // Проём должен быть заметно шире человека: он идёт не по маршруту,
    // а на глаз, и в щель метром шириной попадает далеко не с первого раза.
    const float DoorWidth = 320.0f;
    const float HalfX = Footprint.X * 0.5f;
    const float HalfY = Footprint.Y * 0.5f;
    const float FloorZ = 60.0f;

    // Пол комнаты.
    Piece(BuildingMesh, TEXT("Tiles"),
          Centre + FVector(0, 0, FloorZ + 8.0f),
          FVector(Footprint.X, Footprint.Y, 16.0f),
          FRotator::ZeroRotator, true);

    const float WallZ = FloorZ + RoomHeight * 0.5f;

    // Задняя и боковые стены — сплошные.
    Piece(BuildingMesh, WallMat, Centre + FVector(-HalfX, 0, WallZ),
          FVector(WallThickness, Footprint.Y, RoomHeight), FRotator::ZeroRotator, true);
    Piece(BuildingMesh, WallMat, Centre + FVector(0, -HalfY, WallZ),
          FVector(Footprint.X, WallThickness, RoomHeight), FRotator::ZeroRotator, true);
    Piece(BuildingMesh, WallMat, Centre + FVector(0, HalfY, WallZ),
          FVector(Footprint.X, WallThickness, RoomHeight), FRotator::ZeroRotator, true);

    // Передняя стена с дверным проёмом посередине.
    const float SideWidth = (Footprint.Y - DoorWidth) * 0.5f;
    if (SideWidth > 10.0f)
    {
        for (int32 Side = -1; Side <= 1; Side += 2)
        {
            const float OffsetY = Side * (DoorWidth * 0.5f + SideWidth * 0.5f);
            Piece(BuildingMesh, WallMat, Centre + FVector(HalfX, OffsetY, WallZ),
                  FVector(WallThickness, SideWidth, RoomHeight), FRotator::ZeroRotator, true);
        }
    }
    // Перемычка над дверью.
    Piece(BuildingMesh, WallMat,
          Centre + FVector(HalfX, 0, FloorZ + RoomHeight - 30.0f),
          FVector(WallThickness, DoorWidth, 60.0f), FRotator::ZeroRotator, true);

    // =======================================================================
    //  ВНУТРЕННИЕ ПЕРЕГОРОДКИ
    //
    //  Жильё — не одна комната-ангар. Спальня отдельно от кухни, санузел
    //  отдельно от всего. Человек ходит между комнатами через проёмы,
    //  и это видно: он идёт на кухню, а не «к точке в пространстве».
    //
    //       вход (+X)
    //   +-------------------+
    //   |     гостиная      |
    //   +------[ ]----------+   перегородка с проёмом
    //   | кухня  |  спальня |
    //   |        [ ]        |   вторая перегородка с проёмом
    //   +--------+----------+
    // =======================================================================
    if (Type == TEXT("Residential") && Footprint.X > 600.0f && Footprint.Y > 600.0f)
    {
        const float InnerDoor = 280.0f;
        const float PartX = Centre.X - Footprint.X * 0.08f;   // граница гостиной

        // Поперечная перегородка с проёмом посередине.
        {
            const float SideLen = (Footprint.Y - InnerDoor) * 0.5f;
            for (int32 Side = -1; Side <= 1; Side += 2)
            {
                const float OffsetY = Side * (InnerDoor * 0.5f + SideLen * 0.5f);
                Piece(BuildingMesh, TrimMat,
                      FVector(PartX, Centre.Y + OffsetY, WallZ),
                      FVector(WallThickness, SideLen, RoomHeight),
                      FRotator::ZeroRotator, true);
            }
        }

        // Продольная перегородка в задней половине: кухня и спальня.
        {
            const float BackDepth = (PartX - (Centre.X - HalfX));
            const float SegLen = (BackDepth - InnerDoor) * 0.5f;
            if (SegLen > 40.0f)
            {
                const float BackStart = Centre.X - HalfX;
                for (int32 Seg = 0; Seg < 2; ++Seg)
                {
                    const float SegCentreX = (Seg == 0)
                        ? BackStart + SegLen * 0.5f
                        : PartX - SegLen * 0.5f;

                    Piece(BuildingMesh, TrimMat,
                          FVector(SegCentreX, Centre.Y, WallZ),
                          FVector(SegLen, WallThickness, RoomHeight),
                          FRotator::ZeroRotator, true);
                }
            }
        }
    }

    // --- Школа: коридор и четыре кабинета -----------------------------------
    //
    //   вход (+X)
    //   +--------------------------+
    //   |        коридор           |
    //   +---[ ]----+----[ ]--------+
    //   | язык     |  счёт         |
    //   +---[ ]----+----[ ]--------+
    //   | природа  |  ремесло      |
    //   +----------+---------------+
    if (Type == TEXT("School") && Footprint.X > 700.0f && Footprint.Y > 700.0f)
    {
        // Одна стена и два широких прохода. Сложнее делать нельзя: человек
        // ходит на глаз, и всякая лишняя перегородка — это ещё одно место,
        // где он застрянет и бросит начатое. Здесь я это уже проверил.
        const float InnerDoor = 480.0f;
        const float CorridorX = Centre.X + Footprint.X * 0.24f;

        const float SideLen = (Footprint.Y - InnerDoor * 2.0f) / 3.0f;
        if (SideLen > 60.0f)
        {
            for (int32 Seg = 0; Seg < 3; ++Seg)
            {
                const float OffsetY = -Footprint.Y * 0.5f + SideLen * (Seg + 0.5f)
                                    + InnerDoor * Seg;
                Piece(BuildingMesh, TrimMat,
                      FVector(CorridorX, Centre.Y + OffsetY, WallZ),
                      FVector(WallThickness, SideLen, RoomHeight),
                      FRotator::ZeroRotator, true);
            }
        }
    }

    // Потолок первого этажа — он же пол второго.
    Piece(BuildingMesh, TrimMat,
          Centre + FVector(0, 0, FloorZ + RoomHeight),
          FVector(Footprint.X + 20.0f, Footprint.Y + 20.0f, 24.0f),
          FRotator::ZeroRotator, true);

    // --- Верхние этажи: сплошные, внутрь них не ходят ------------------------
    const float UpperHeight = FMath::Max(0.0f, Height - RoomHeight);
    if (UpperHeight > 10.0f)
    {
        Piece(BuildingMesh, WallMat,
              Centre + FVector(0, 0, FloorZ + RoomHeight + UpperHeight * 0.5f),
              FVector(Footprint.X, Footprint.Y, UpperHeight),
              FRotator::ZeroRotator, true);
    }

    // --- Окна: горизонтальные ленты по этажам --------------------------------
    // Отдельные окна на таком масштабе не читаются, а ленты сразу дают
    // понять, что перед нами дом, и показывают его этажность.
    const int32 Floors = FMath::Max(1, FMath::FloorToInt(Height / FloorHeight));
    for (int32 F = 1; F < Floors; ++F)   // первый этаж — жилая комната, там окна не нужны
    {
        const float BandZ = 60.0f + F * FloorHeight + FloorHeight * 0.62f;
        if (BandZ > Height + 40.0f)
        {
            break;
        }
        Piece(BuildingMesh, TEXT("Glass"),
              Centre + FVector(0, 0, BandZ),
              FVector(Footprint.X + 14.0f, Footprint.Y + 14.0f, FloorHeight * 0.34f));
    }

    // --- Крыша ---------------------------------------------------------------
    Piece(BuildingMesh, TEXT("Grime"),
          Centre + FVector(0, 0, 60.0f + Height + 18.0f),
          FVector(Footprint.X + 90.0f, Footprint.Y + 90.0f, 36.0f));

    // Надстройка на крыше — силуэт перестаёт быть коробкой.
    if (FMath::FRand() < 0.6f)
    {
        const float BoxSize = FMath::FRandRange(0.25f, 0.45f);
        Piece(BuildingMesh, TrimMat,
              Centre + FVector(Footprint.X * 0.15f, -Footprint.Y * 0.12f, 60.0f + Height + 120.0f),
              FVector(Footprint.X * BoxSize, Footprint.Y * BoxSize, 170.0f));
    }

    // --- Крыльцо у входа -----------------------------------------------------
    //
    // Здесь была ошибка, которая стоила жителям жизни: пол комнаты лежит на
    // высоте 76 см, а крыльцо было одной плитой в 50 см. Человек перешагивает
    // сорок пять. Дом был наглухо закрыт — внутри стояли раковина, плита и
    // кровать, то есть вода, еда и сон. Люди умирали от жажды у своего порога.
    //
    // Теперь ступени по 26 см: на такие поднимаются.
    const FVector Door = Centre + FVector(HalfX, 0.0f, 0.0f);
    const float StepRise = 26.0f;
    const float StepRun = 85.0f;

    for (int32 Step = 0; Step < 3; ++Step)
    {
        const float Top = StepRise * (Step + 1);
        const float OutX = StepRun * (2.5f - Step);   // нижняя ступень дальше от двери
        Piece(BuildingMesh, TEXT("Sidewalk"),
              Door + FVector(OutX, 0.0f, Top * 0.5f),
              FVector(StepRun + 8.0f, DoorWidth + 60.0f, Top),
              FRotator::ZeroRotator, true);
    }

    // Створка — распахнутая, у края проёма. Раньше она стояла посреди входа
    // и загораживала его: дверь, в которую нельзя пройти, — это стена.
    Piece(BuildingMesh, TEXT("Wood"),
          Door + FVector(80.0f, -(DoorWidth * 0.5f) - 10.0f, FloorZ + 120.0f),
          FVector(160.0f, 22.0f, 240.0f), FRotator::ZeroRotator, true);
}

// ---------------------------------------------------------------------------
//  Обстановка
// ---------------------------------------------------------------------------

AFurnitureActor* ACityGenerator::PlaceFurniture(FBuildingInfo& Building, EFurnitureType Kind,
                                                const FVector& At, float YawDegrees)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    const FTransform T(FRotator(0.0f, YawDegrees, 0.0f), At);

    AFurnitureActor* Item = World->SpawnActorDeferred<AFurnitureActor>(
        AFurnitureActor::StaticClass(), T, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!Item)
    {
        return nullptr;
    }

    Item->FurnitureType = Kind;
    Item->FinishSpawning(T);

    // Вещь должна быть готова немедленно: жители получают знания о городе
    // следующим шагом, и незаполненную вещь они запомнили бы как бесполезную.
    Item->BuildLook();
    Item->BuildOffers();
    if (UAffordanceComponent* Offers = Item->Affordances)
    {
        // Вид у всех кроватей общий, а вещь — своя. Это и позволяет
        // одновременно переносить опыт на подобное и помнить, что
        // именно ЭТА плита однажды подвела.
        Offers->Category = Item->Name;
        Offers->DisplayName = FString::Printf(TEXT("%s №%d"), *Item->Name, ++FurnitureCounter);

        // Обстановка жилого дома принадлежит его жильцам. Посторонний
        // не ляжет спать в чужую кровать, как бы ни хотел спать.
        const bool bOwnedHouse = Building.Type == TEXT("Residential") || (Settlement != ESettlement::Modern && Building.Type == TEXT("TownHall"));
        const float Yard = FMath::Max(Building.Scale.X, Building.Scale.Y) * 0.5f + 700.0f;
        if (bOwnedHouse && FVector::Dist2D(At, Building.Location) < Yard)
        {
            Offers->bPrivate = true;
            Offers->OwnerAnchor = Building.Location;
        }
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldMind->RegisterAffordanceSource(Offers);
        }
    }

    Building.Furniture.Add(Item);
    return Item;
}

ABookActor* ACityGenerator::SpawnBook(FName Subject, const FVector& At)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    const FTransform T(FRotator(0.0f, FMath::FRandRange(-8.0f, 8.0f), 0.0f), At);

    ABookActor* Book = World->SpawnActorDeferred<ABookActor>(
        ABookActor::StaticClass(), T, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!Book)
    {
        return nullptr;
    }

    Book->Subject = Subject;
    Book->ShelfLocation = At;
    Book->FinishSpawning(T);
    Book->Setup(Subject);

    ++BooksPlaced;
    return Book;
}

void ACityGenerator::FurnishRoom(FBuildingInfo& Building, const FString& Type)
{
    // Комната — это первый этаж: от пола на высоте цоколя до потолка.
    const float Inset = 130.0f;
    const float HalfX = FMath::Max(120.0f, Building.Scale.X * 0.5f - Inset);
    const float HalfY = FMath::Max(120.0f, Building.Scale.Y * 0.5f - Inset);
    const FVector Floor = Building.Location + FVector(0.0f, 0.0f, 76.0f);

    auto Spot = [&](float FracX, float FracY)
    {
        return Floor + FVector(HalfX * FracX, HalfY * FracY, 0.0f);
    };

    if (Type == TEXT("Residential"))
    {
        // Каждая вещь — в своей комнате. Гостиная у входа (+X),
        // за перегородкой кухня (-Y) и спальня (+Y).

        // --- Гостиная ---
        PlaceFurniture(Building, EFurnitureType::Rug,       Spot( 0.55f, -0.05f),   0.0f);
        PlaceFurniture(Building, EFurnitureType::Sofa,      Spot( 0.62f,  0.55f), 270.0f);
        PlaceFurniture(Building, EFurnitureType::TV,        Spot( 0.62f, -0.72f),  90.0f);
        PlaceFurniture(Building, EFurnitureType::Lamp,      Spot( 0.88f,  0.85f),   0.0f);
        // Дома на полке — то, с чего начинают: азбука и книга для чтения.
        {
            const FVector ShelfAt = Spot(0.30f, 0.90f);
            PlaceFurniture(Building, EFurnitureType::Bookshelf, ShelfAt, 180.0f);
            SpawnBook(TEXT("Reader"), ShelfAt + FVector(0.0f, -14.0f, 62.0f));
            SpawnBook(TEXT("Alphabet"), ShelfAt + FVector(0.0f, 14.0f, 104.0f));
        }
        if (FMath::FRand() < 0.5f)
        {
            PlaceFurniture(Building, EFurnitureType::Plant, Spot(0.88f, -0.88f), 0.0f);
        }
        if (FMath::FRand() < 0.18f)
        {
            PlaceFurniture(Building, EFurnitureType::Piano, Spot(0.25f, -0.80f), 90.0f);
        }

        // --- Кухня (задняя половина, сторона -Y) ---
        PlaceFurniture(Building, EFurnitureType::Stove,   Spot(-0.86f, -0.28f),   0.0f);
        PlaceFurniture(Building, EFurnitureType::Counter, Spot(-0.86f, -0.62f),  90.0f);
        PlaceFurniture(Building, EFurnitureType::Fridge,  Spot(-0.86f, -0.90f),   0.0f);
        PlaceFurniture(Building, EFurnitureType::Sink,    Spot(-0.42f, -0.90f),  90.0f);
        PlaceFurniture(Building, EFurnitureType::Table,   Spot(-0.50f, -0.45f),   0.0f);
        PlaceFurniture(Building, EFurnitureType::Chair,   Spot(-0.30f, -0.45f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Chair,   Spot(-0.70f, -0.45f),   0.0f);

        // --- Спальня (задняя половина, сторона +Y) ---
        PlaceFurniture(Building, EFurnitureType::Bed,        Spot(-0.82f,  0.42f),  0.0f);
        PlaceFurniture(Building, EFurnitureType::Nightstand, Spot(-0.82f,  0.72f),  0.0f);
        PlaceFurniture(Building, EFurnitureType::Wardrobe,   Spot(-0.45f,  0.30f), 90.0f);
        if (FMath::FRand() < 0.35f)
        {
            PlaceFurniture(Building, EFurnitureType::Crib, Spot(-0.55f, 0.62f), 0.0f);
        }

        // --- Санузел: ванна, раковина, уборная, зеркало ---
        // Раньше здесь был один душ. Помыться можно было только стоя,
        // а руки — вообще негде.
        // Запас в холодильнике: столько, чтобы хватило на несколько дней,
        // а потом пришлось бы идти за продуктами — как у всех.
        for (AFurnitureActor* Item : Building.Furniture)
        {
            if (Item && Item->FurnitureType == EFurnitureType::Fridge)
            {
                Item->Store(EResourceKind::RawFood, 0.0f);
                Item->Store(EResourceKind::CookedFood, 0.0f);
            }
        }

        PlaceFurniture(Building, EFurnitureType::Bath,   Spot(-0.85f,  0.92f),   0.0f);
        PlaceFurniture(Building, EFurnitureType::Toilet, Spot(-0.30f,  0.92f),  90.0f);
        PlaceFurniture(Building, EFurnitureType::Sink,   Spot(-0.12f,  0.92f),  90.0f);
        PlaceFurniture(Building, EFurnitureType::Mirror, Spot(-0.12f,  0.99f),  90.0f);
        if (FMath::FRand() < 0.5f)
        {
            PlaceFurniture(Building, EFurnitureType::Washer, Spot(-0.60f, 0.92f), 0.0f);
        }
    }
    else if (Type == TEXT("Commercial"))
    {
        // Кафе: столы, стулья, кухня, уборная.
        PlaceFurniture(Building, EFurnitureType::Stove,   Spot(-0.85f,  0.70f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Counter, Spot(-0.85f,  0.20f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Fridge,  Spot(-0.85f, -0.25f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Sink,    Spot(-0.85f, -0.70f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Toilet,  Spot(-0.30f, -0.90f), 90.0f);

        for (int32 i = 0; i < 3; ++i)
        {
            const float FracY = -0.5f + i * 0.5f;
            PlaceFurniture(Building, EFurnitureType::Table, Spot(0.35f, FracY), 0.0f);
            PlaceFurniture(Building, EFurnitureType::Chair, Spot(0.62f, FracY), 270.0f);
            PlaceFurniture(Building, EFurnitureType::Chair, Spot(0.08f, FracY),  90.0f);
        }
        PlaceFurniture(Building, EFurnitureType::Plant, Spot(0.85f, 0.85f), 0.0f);
    }
    else if (Type == TEXT("Office"))
    {
        // Контора: рабочие места и то, без чего рабочий день невозможен.
        for (int32 i = 0; i < 3; ++i)
        {
            const float FracY = -0.6f + i * 0.6f;
            PlaceFurniture(Building, EFurnitureType::Desk,     Spot(-0.40f, FracY), 90.0f);
            PlaceFurniture(Building, EFurnitureType::Computer, Spot(-0.40f, FracY + 0.08f), 90.0f);
            PlaceFurniture(Building, EFurnitureType::Chair,    Spot(-0.05f, FracY), 270.0f);
        }
        PlaceFurniture(Building, EFurnitureType::Sink,      Spot(0.80f,  0.70f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Toilet,    Spot(0.80f, -0.70f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Bookshelf, Spot(-0.88f, 0.00f),  90.0f);
        PlaceFurniture(Building, EFurnitureType::Plant,     Spot(0.85f,  0.00f),   0.0f);
    }
    else if (Type == TEXT("School"))
    {
        // =======================================================================
        //  ШКОЛА
        //
        //  Четыре кабинета, в каждом — доска, парты и учебники по предмету.
        //  Учебники настоящие: в них написан текст, и человек, прочитав
        //  страницу, узнаёт именно то, что на ней сказано. Так знание
        //  впервые в этом городе может достаться человеку не через
        //  собственную шкуру, а через книгу.
        // =======================================================================

        // Кабинет: доска у дальней стены, парты рядами, у стены — учебники.
        auto Classroom = [&](FName RoomKind, float CX, float CY, float Spread)
        {
            PlaceFurniture(Building, EFurnitureType::Blackboard, Spot(CX - Spread, CY), 0.0f);

            for (int32 Row = 0; Row < 2; ++Row)
            {
                for (int32 Col = 0; Col < 2; ++Col)
                {
                    PlaceFurniture(Building, EFurnitureType::SchoolDesk,
                                   Spot(CX - Spread * 0.3f + Row * Spread * 0.55f,
                                        CY - Spread * 0.35f + Col * Spread * 0.7f),
                                   0.0f);
                }
            }

            // Учебники этого кабинета. Шкаф — мебель, а книги на нём —
            // отдельные предметы: их берут в руки, носят и кладут обратно.
            TArray<const FTextbook*> Books;
            FLibrary::ForRoom(RoomKind, Books);

            const int32 Shelves = FMath::Max(1, FMath::DivideAndRoundUp(Books.Num(), 4));
            for (int32 S = 0; S < Shelves; ++S)
            {
                const float Along = -0.5f + (Shelves > 1 ? S / float(Shelves - 1) : 0.5f);
                const FVector ShelfAt = Spot(CX + Spread * 0.62f, CY + Spread * Along);
                PlaceFurniture(Building, EFurnitureType::Bookshelf, ShelfAt, 270.0f);

                for (int32 i = S * 4; i < FMath::Min((S + 1) * 4, Books.Num()); ++i)
                {
                    // Книги стоят на полках в ряд, на высоте полок.
                    const int32 Slot = i - S * 4;
                    const FVector BookAt = ShelfAt
                        + FVector(14.0f, -33.0f + Slot * 22.0f, 62.0f + Slot * 42.0f);
                    SpawnBook(Books[i]->Subject, BookAt);
                }
            }
        };

        // Два зала вместо четырёх тесных кабинетов: так до полок доходят.
        Classroom(TEXT("Language"), -0.35f, -0.52f, 0.36f);
        Classroom(TEXT("Numbers"),  -0.35f,  0.52f, 0.36f);

        // Третий кабинет — природа и ремёсла, раз школа теперь просторнее.
        Classroom(TEXT("Nature"), -0.80f, 0.00f, 0.30f);

        // Коридор у входа: там ждут, разговаривают и пьют воду.
        PlaceFurniture(Building, EFurnitureType::Globe,  Spot(0.55f,  0.00f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Abacus, Spot(0.55f,  0.40f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Sink,   Spot(0.80f, -0.80f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Toilet, Spot(0.55f, -0.85f), 90.0f);
        PlaceFurniture(Building, EFurnitureType::Chair,  Spot(0.80f,  0.55f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Chair,  Spot(0.80f,  0.80f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Plant,  Spot(0.88f, -0.35f), 0.0f);
    }
    else if (Type == TEXT("Hospital"))
    {
        // Больница: палаты с кроватями, где лежат, и место, где принимают.
        for (int32 i = 0; i < 4; ++i)
        {
            const float FracY = -0.72f + i * 0.48f;
            PlaceFurniture(Building, EFurnitureType::Bed,        Spot(-0.70f, FracY), 0.0f);
            PlaceFurniture(Building, EFurnitureType::Nightstand, Spot(-0.40f, FracY), 0.0f);
        }
        PlaceFurniture(Building, EFurnitureType::Desk,   Spot(0.55f,  0.55f),  90.0f);
        PlaceFurniture(Building, EFurnitureType::Chair,  Spot(0.55f,  0.20f), 270.0f);
        PlaceFurniture(Building, EFurnitureType::Sink,   Spot(0.80f, -0.60f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Toilet, Spot(0.80f, -0.88f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Wardrobe, Spot(0.20f, -0.85f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Plant,  Spot(0.86f,  0.85f), 0.0f);
    }
    else if (Type == TEXT("Workshop"))
    {
        // Мастерская: верстаки, инструмент, запас дров и готовые изделия.
        for (int32 i = 0; i < 3; ++i)
        {
            const float FracY = -0.6f + i * 0.6f;
            PlaceFurniture(Building, EFurnitureType::Counter, Spot(-0.60f, FracY), 90.0f);
            PlaceFurniture(Building, EFurnitureType::Chair,   Spot(-0.25f, FracY), 270.0f);
        }
        PlaceFurniture(Building, EFurnitureType::Stove,     Spot(-0.85f,  0.80f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Wardrobe,  Spot(0.55f,  0.80f),  90.0f);
        PlaceFurniture(Building, EFurnitureType::Table,     Spot(0.45f, -0.30f),  0.0f);
        PlaceFurniture(Building, EFurnitureType::Sink,      Spot(0.80f, -0.80f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Toilet,    Spot(0.80f,  0.20f), 180.0f);
    }
    else if (Type == TEXT("Library"))
    {
        // Библиотека: полки вдоль стен и столы для тех, кто читает.
        TArray<const FTextbook*> Everything;
        FLibrary::ForRoom(TEXT("All"), Everything);

        int32 Placed = 0;
        for (int32 Row = 0; Row < 4; ++Row)
        {
            const float FracY = -0.78f + Row * 0.52f;
            const FVector ShelfAt = Spot(-0.72f, FracY);
            PlaceFurniture(Building, EFurnitureType::Bookshelf, ShelfAt, 270.0f);

            for (int32 Slot = 0; Slot < 4 && Placed < Everything.Num(); ++Slot, ++Placed)
            {
                SpawnBook(Everything[Placed]->Subject,
                          ShelfAt + FVector(14.0f, -33.0f + Slot * 22.0f, 62.0f + Slot * 42.0f));
            }
        }

        for (int32 i = 0; i < 3; ++i)
        {
            const float FracY = -0.55f + i * 0.55f;
            PlaceFurniture(Building, EFurnitureType::Table, Spot(0.30f, FracY), 0.0f);
            PlaceFurniture(Building, EFurnitureType::Chair, Spot(0.58f, FracY), 270.0f);
            PlaceFurniture(Building, EFurnitureType::Chair, Spot(0.02f, FracY),  90.0f);
        }
        PlaceFurniture(Building, EFurnitureType::Lamp,   Spot(0.80f,  0.85f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Sink,   Spot(0.82f, -0.85f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Toilet, Spot(0.55f, -0.88f),  90.0f);
    }
    else if (Type == TEXT("Market"))
    {
        // Рынок: ряды лотков с товаром.
        for (int32 Row = 0; Row < 3; ++Row)
        {
            for (int32 Col = 0; Col < 2; ++Col)
            {
                AFurnitureActor* Stall = PlaceFurniture(Building, EFurnitureType::MarketStall,
                    Spot(-0.60f + Col * 0.75f, -0.65f + Row * 0.65f), Col == 0 ? 0.0f : 180.0f);
                if (Stall)
                {
                    Stall->Store(EResourceKind::RawFood, 0.0f);
                }
            }
        }
        PlaceFurniture(Building, EFurnitureType::Barrel, Spot(0.85f,  0.00f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Toilet, Spot(0.85f, -0.85f), 180.0f);
    }
    else if (Type == TEXT("Bathhouse"))
    {
        // Баня: где моются и где сидят после.
        for (int32 i = 0; i < 3; ++i)
        {
            PlaceFurniture(Building, EFurnitureType::Bath, Spot(-0.70f, -0.6f + i * 0.6f), 0.0f);
        }
        PlaceFurniture(Building, EFurnitureType::Shower, Spot(-0.30f,  0.85f), 90.0f);
        PlaceFurniture(Building, EFurnitureType::Shower, Spot(-0.30f, -0.85f), 90.0f);
        PlaceFurniture(Building, EFurnitureType::Sofa,   Spot(0.55f,  0.45f), 270.0f);
        PlaceFurniture(Building, EFurnitureType::Sofa,   Spot(0.55f, -0.45f), 270.0f);
        PlaceFurniture(Building, EFurnitureType::Table,  Spot(0.30f,  0.00f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Mirror, Spot(0.85f,  0.85f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Sink,   Spot(0.85f, -0.20f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Toilet, Spot(0.85f, -0.85f), 180.0f);
    }
    else if (Type == TEXT("Bakery"))
    {
        // Пекарня: печи в глубине, прилавок у входа.
        PlaceFurniture(Building, EFurnitureType::Stove,   Spot(-0.82f,  0.55f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Stove,   Spot(-0.82f, -0.55f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Counter, Spot(-0.45f,  0.00f), 90.0f);

        AFurnitureActor* Shop = PlaceFurniture(Building, EFurnitureType::MarketStall,
                                               Spot(0.50f, 0.00f), 180.0f);
        if (Shop)
        {
            Shop->Store(EResourceKind::RawFood, 0.0f);
        }
        PlaceFurniture(Building, EFurnitureType::Fridge, Spot(-0.82f,  0.00f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Sink,   Spot(0.82f, -0.80f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Table,  Spot(0.70f,  0.65f), 0.0f);
        PlaceFurniture(Building, EFurnitureType::Chair,  Spot(0.45f,  0.65f), 180.0f);
    }
    else if (Type == TEXT("TownHall"))
    {
        // Управа: столы с бумагами, зал для собраний.
        for (int32 i = 0; i < 4; ++i)
        {
            const float FracY = -0.72f + i * 0.48f;
            PlaceFurniture(Building, EFurnitureType::Desk,  Spot(-0.65f, FracY), 90.0f);
            PlaceFurniture(Building, EFurnitureType::Chair, Spot(-0.32f, FracY), 270.0f);
        }
        PlaceFurniture(Building, EFurnitureType::Bookshelf, Spot(-0.88f,  0.00f), 90.0f);
        PlaceFurniture(Building, EFurnitureType::Table,     Spot(0.40f,  0.00f), 0.0f);
        for (int32 i = 0; i < 3; ++i)
        {
            PlaceFurniture(Building, EFurnitureType::Chair, Spot(0.65f, -0.5f + i * 0.5f), 270.0f);
        }
        PlaceFurniture(Building, EFurnitureType::Sink,   Spot(0.85f, -0.85f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Toilet, Spot(0.85f,  0.85f), 180.0f);
        PlaceFurniture(Building, EFurnitureType::Plant,  Spot(0.85f,  0.20f), 0.0f);
    }
}

// ---------------------------------------------------------------------------
//  Зелень
// ---------------------------------------------------------------------------

void ACityGenerator::PlantTree(const FVector& Base, float Scale)
{
    if (Buildings.Num() == 0)
    {
        return;
    }
    PlaceFurniture(Buildings[0], EFurnitureType::Tree, FVector(Base.X, Base.Y, GroundAt(Base)), FMath::FRandRange(0.0f, 360.0f));
}

void ACityGenerator::Decor(const TCHAR* Folder, const TCHAR* Filter, const FVector& At, const FVector& Size, float Yaw, bool bCollide, bool bStretch)
{
    FImportedModels::Place(this, RootComponent, Folder, Filter, At - GetActorLocation(), Size, Yaw, bCollide, nullptr, bStretch);
}

void ACityGenerator::BuildPark(const FVector& Centre, float Size)
{
    const float Half = Size * 0.5f;

    // Газон.
    Piece(BuildingMesh, TEXT("Grass"), Centre + FVector(0, 0, 12.0f), FVector(Size, Size, 24.0f));

    // Дорожка через сквер.
    Piece(BuildingMesh, TEXT("Gravel"), Centre + FVector(0, 0, 26.0f), FVector(Size, 180.0f, 12.0f));

    if (!bGreenery)
    {
        return;
    }

    // Деревья по краям, чтобы середина оставалась открытой.
    const int32 Trees = FMath::RandRange(4, 7);
    for (int32 i = 0; i < Trees; ++i)
    {
        const float Angle = (i / static_cast<float>(Trees)) * 2.0f * PI + FMath::FRandRange(-0.3f, 0.3f);
        const float Radius = Half * FMath::FRandRange(0.55f, 0.85f);
        const FVector At = Centre + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 24.0f);
        PlantTree(At, FMath::FRandRange(0.8f, 1.35f));
    }

    if (Buildings.Num() == 0)
    {
        return;
    }
    FBuildingInfo& Keeper = Buildings[0];

    for (int32 i = 0; i < 6; ++i)
    {
        const FVector At = Centre + FVector(
            FMath::FRandRange(-Half * 0.8f, Half * 0.8f),
            FMath::FRandRange(-Half * 0.8f, Half * 0.8f), 24.0f);
        PlaceFurniture(Keeper, EFurnitureType::Bush, At, FMath::FRandRange(0.0f, 360.0f));
    }
    for (int32 i = 0; i < 3; ++i)
    {
        const FVector At = Centre + FVector(
            FMath::FRandRange(-Half * 0.7f, Half * 0.7f),
            FMath::FRandRange(-Half * 0.7f, Half * 0.7f), 24.0f);
        PlaceFurniture(Keeper, EFurnitureType::StonePile, At, FMath::FRandRange(0.0f, 360.0f));
    }

    for (int32 i = 0; i < 4; ++i)
    {
        const float Angle = i * PI * 0.5f + PI * 0.25f;
        const FVector At = Centre + FVector(FMath::Cos(Angle) * Half * 0.35f,
                                            FMath::Sin(Angle) * Half * 0.35f, 24.0f);
        PlaceFurniture(Keeper, EFurnitureType::Bench, At, FMath::RadiansToDegrees(Angle));
    }
}

void ACityGenerator::SpawnBuilding(const FString& Type, const FVector& Location, const FVector& Scale)
{
    FBuildingInfo NewBuilding;
    NewBuilding.Type = Type;
    NewBuilding.Location = Location;
    NewBuilding.Scale = Scale;
    NewBuilding.Capacity = (Type == TEXT("Residential")) ? 4 : 40;

    // Вход — со стороны улицы, чуть в стороне от стены, чтобы человек
    // мог до него дойти и не упереться в дом.
    NewBuilding.EntranceLocation = Location + FVector(Scale.X * 0.5f + 260.0f, 0.0f, 0.0f);
    NewBuilding.EntranceLocation.Z = Location.Z;

    Buildings.Add(NewBuilding);
}

EPlaceKind ACityGenerator::PlaceKindForBuilding(const FString& Type) const
{
    if (Type == TEXT("Commercial")) return EPlaceKind::Food;
    if (Type == TEXT("Office"))     return EPlaceKind::Work;
    if (Type == TEXT("Park"))       return EPlaceKind::Beautiful;
    if (Type == TEXT("Residential"))return EPlaceKind::Home;
    if (Type == TEXT("School"))     return EPlaceKind::Study;
    if (Type == TEXT("Hospital"))   return EPlaceKind::Hospital;
    if (Type == TEXT("Workshop"))   return EPlaceKind::Workshop;
    if (Type == TEXT("Library"))    return Settlement == ESettlement::Modern ? EPlaceKind::Library : EPlaceKind::Church;
    if (Type == TEXT("Market"))     return EPlaceKind::Market;
    if (Type == TEXT("Bathhouse"))  return EPlaceKind::Bathhouse;
    if (Type == TEXT("Bakery"))     return EPlaceKind::Bakery;
    if (Type == TEXT("TownHall"))   return Settlement == ESettlement::Modern ? EPlaceKind::TownHall : EPlaceKind::Castle;
    if (Type == TEXT("Church"))     return EPlaceKind::Church;
    return EPlaceKind::Landmark;
}

void ACityGenerator::RegisterPlacesInWorld()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.Owner = this;

    // Ставим настоящие места, а не отметки на карте. Каждое из них само
    // объявит, что предлагает, — и люди будут решать, нужно им это или нет.
    int32 Counter = 0;

    auto SpawnPlace = [&](EPlaceKind Kind, const FVector& Location, const FString& Name, int32 Capacity)
    {
        APlaceActor* Place = World->SpawnActorDeferred<APlaceActor>(
            APlaceActor::StaticClass(),
            FTransform(FRotator::ZeroRotator, Location),
            this, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

        if (!Place)
        {
            return;
        }

        Place->Kind = Kind;
        Place->PlaceName = FString::Printf(TEXT("%s №%d"), *Name, ++Counter);
        Place->Capacity = Capacity;
        Place->bShowMarker = Settlement == ESettlement::Modern;
        Place->FinishSpawning(FTransform(FRotator::ZeroRotator, Location));
        if (Place->Affordances && Kind == EPlaceKind::Home && Settlement != ESettlement::Modern)
        {
            Place->Affordances->bPrivate = true;
            Place->Affordances->OwnerAnchor = Location;
        }

        // Место должно быть готово ПРЯМО СЕЙЧАС, а не когда до него дойдёт
        // очередь BeginPlay: жители получают знания о городе следующим шагом,
        // и пустую контору они запомнили бы навсегда как место, где нечего
        // делать. Настраиваем и ставим на учёт немедленно.
        if (Place->Affordances)
        {
            Place->Affordances->DisplayName = Place->PlaceName;
            Place->Affordances->Capacity = Capacity;
            if (Place->Affordances->Offers.Num() == 0)
            {
                Place->Affordances->MakeTypicalFor(Kind);
            }

            if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
            {
                WorldMind->RegisterAffordanceSource(Place->Affordances);
                WorldMind->RegisterPlace(Kind, Location, Place->PlaceName);
            }
        }

        SpawnedPlaces.Add(Place);
    };

    ON_SCOPE_EXIT
    {
        // Короткая сводка: без неё невозможно понять, почему в городе
        // никто не работает — просто нет контор или дело в другом.
        int32 Food = 0, Work = 0, Homes = 0, Other = 0;
        for (const TObjectPtr<APlaceActor>& P : SpawnedPlaces)
        {
            if (!P) continue;
            switch (P->Kind)
            {
            case EPlaceKind::Food: ++Food; break;
            case EPlaceKind::Work: ++Work; break;
            case EPlaceKind::Home: ++Homes; break;
            default: ++Other; break;
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("[Город] мест: еда=%d работа=%d дома=%d прочее=%d; книг разложено: %d"),
               Food, Work, Homes, Other, BooksPlaced);
    };

    for (const FBuildingInfo& B : Buildings)
    {
        const EPlaceKind Kind = PlaceKindForBuilding(B.Type);

        // Место находится ВНУТРИ здания, а не у входа. Иначе человек
        // «отдыхает дома», стоя на крыльце, и «работает в конторе»,
        // не заходя в неё.
        // Ставим не в геометрический центр — там может оказаться
        // перегородка, — а в переднюю комнату, сразу за дверью.
        const FVector Inside = B.Location + FVector(B.Scale.X * 0.22f, 0.0f, 0.0f);

        // Общественные места — у входа.
        //
        // «Прийти в кафе» для человека означает оказаться у его дверей,
        // а не в дальнем углу зала. Когда я загнал их вглубь, люди
        // перестали доходить: голод не гасился, и весь день уходил на
        // дорогу до обеда. Внутрь имеет смысл заводить только туда,
        // где человек живёт, — там он знает каждый угол.
        if (Kind == EPlaceKind::Food)
        {
            SpawnPlace(EPlaceKind::Food, B.EntranceLocation, TEXT("кафе"), 6);
        }
        else if (Kind == EPlaceKind::Work)
        {
            SpawnPlace(EPlaceKind::Work, B.EntranceLocation, TEXT("контора"), 12);
        }
        else if (Kind == EPlaceKind::Beautiful)
        {
            SpawnPlace(EPlaceKind::Beautiful, B.Location, TEXT("сквер"), 0);
            SpawnPlace(EPlaceKind::Social, B.Location + FVector(400.0f, 0.0f, 0.0f), TEXT("людное место"), 0);
        }
        else if (Kind == EPlaceKind::Home)
        {
            // Дом — это комната внутри, а не дверь снаружи.
            SpawnPlace(EPlaceKind::Home, Inside, TEXT("дом"), B.Capacity);
        }
        else if (Kind == EPlaceKind::Study)
        {
            SpawnPlace(EPlaceKind::Study, B.EntranceLocation, TEXT("школа"), 30);
        }
        else if (Kind != EPlaceKind::Landmark)
        {
            // Больница, мастерская, библиотека, рынок, баня, пекарня, управа —
            // все они стоят у входа: человек приходит к дверям, а не в угол зала.
            SpawnPlace(Kind, B.EntranceLocation, HumanText::Place(Kind), 16);
        }
    }

    for (const TPair<EPlaceKind, FVector>& Spot : VillagePlaces)
    {
        SpawnPlace(Spot.Key, Spot.Value, HumanText::Place(Spot.Key), 20);
    }
}

// ---------------------------------------------------------------------------
//  Люди
// ---------------------------------------------------------------------------

FVector ACityGenerator::GetStreetPoint() const
{
    // В средневековом поселении людей ставят у своих дворов, на землю,
    // а не в пустоту: земля тут блочная, и высота у каждой клетки своя.
    if (Settlement != ESettlement::Modern)
    {
        FVector At(FMath::FRandRange(-1800.0f, 9000.0f), FMath::FRandRange(-2400.0f, 2400.0f), 0.0f);

        for (const FBuildingInfo& Building : Buildings)
        {
            if (Building.Type == TEXT("Residential") && FMath::FRand() < 0.5f)
            {
                At = Building.EntranceLocation + FVector(FMath::FRandRange(-160.0f, 160.0f),
                                                         FMath::FRandRange(-160.0f, 160.0f), 0.0f);
                break;
            }
        }

        return FVector(At.X, At.Y, GroundAt(At) + 140.0f);
    }

    // Улицы проходят между кварталами — там людям и место.
    const FVector Origin = GetActorLocation();
    const int32 X = FMath::RandRange(0, BlocksX);
    const int32 Y = FMath::RandRange(0, BlocksY);

    return Origin + FVector(
        X * BlockSize - BlockSize * 0.5f + FMath::FRandRange(-200.0f, 200.0f),
        Y * BlockSize - BlockSize * 0.5f + FMath::FRandRange(-200.0f, 200.0f),
        120.0f);
}

void ACityGenerator::SpawnNPCs()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    if (Settlement == ESettlement::Village)
    {
        SpawnVillagers();
        return;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    for (int32 i = 0; i < NPCsCount; ++i)
    {
        const FVector Location = GetStreetPoint();
        const FRotator Rotation(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);

        ACompleteHumanNPC* NPC = World->SpawnActor<ACompleteHumanNPC>(
            ACompleteHumanNPC::StaticClass(), Location, Rotation, Params);

        if (!NPC)
        {
            continue;
        }

        // Дом.
        FBuildingInfo* HomeBuilding = GetRandomResidentialBuilding();
        if (HomeBuilding)
        {
            // Дом — это комната, а не крыльцо: «быть дома» значит
            // находиться внутри.
            NPC->SetHome(HomeBuilding->Location);
            HomeBuilding->Occupants.Add(NPC);
        }

        SeedKnowledge(NPC, HomeBuilding);
    }

    // --- Знакомства: соседи по дому знают друг друга ------------------------
    // Город не начинается с нуля: у людей уже есть прошлое.
    UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>();
    const float T = WorldMind ? WorldMind->WorldSeconds : 0.0f;

    for (const FBuildingInfo& B : Buildings)
    {
        if (B.Occupants.Num() < 2)
        {
            continue;
        }

        for (ACompleteHumanNPC* A : B.Occupants)
        {
            for (ACompleteHumanNPC* Other : B.Occupants)
            {
                if (!A || !Other || A == Other || !A->SocialComponent)
                {
                    continue;
                }

                FRelationship& R = A->SocialComponent->FindOrAdd(Other, T);
                R.Familiarity = FMath::FRandRange(0.25f, 0.6f);
                R.Liking = FMath::FRandRange(-0.2f, 0.5f);
                R.Trust = FMath::FRandRange(0.2f, 0.6f);
                R.InteractionCount = FMath::RandRange(3, 20);
                R.InGroup = 0.5f;
                R.FirstMetAt = T - FMath::FRandRange(86400.0f, 86400.0f * 60.0f);
                R.LastInteractionAt = T - FMath::FRandRange(3600.0f, 86400.0f);

                if (Other->IdentityComponent)
                {
                    R.KnownName = Other->IdentityComponent->FirstName;
                }
                R.Kind = USocialComponent::ClassifyRelation(R);
            }
        }
    }

    // =======================================================================
    //  ОДИН ОДАРЁННЫЙ
    //
    //  Одному от рождения дано больше: он общителен, собран, добр, любопытен
    //  и спокоен. Это не должность и не уважение — уважать его пока не за
    //  что. Станет ли он тем, к кому идут, решат его поступки.
    // =======================================================================
    if (!WorldMind)
    {
        return;
    }

    TArray<ACompleteHumanNPC*> Adults;
    for (ACompleteHumanNPC* Human : WorldMind->GetAllHumans())
    {
        if (Human && Human->IdentityComponent
            && Human->IdentityComponent->Age >= 25.0f && Human->IdentityComponent->Age <= 55.0f)
        {
            Adults.Add(Human);
        }
    }
    if (Adults.Num() == 0)
    {
        return;
    }

    ACompleteHumanNPC* Gifted = Adults[FMath::RandRange(0, Adults.Num() - 1)];

    if (UPersonalityComponent* P = Gifted->PersonalityComponent)
    {
        P->Traits.Extraversion = FMath::FRandRange(0.8f, 0.92f);
        P->Traits.Conscientiousness = FMath::FRandRange(0.8f, 0.92f);
        P->Traits.Agreeableness = FMath::FRandRange(0.78f, 0.9f);
        P->Traits.Openness = FMath::FRandRange(0.8f, 0.95f);
        P->Traits.Neuroticism = FMath::FRandRange(0.08f, 0.2f);
        P->DeriveFromTraits();

        P->Facets.Honesty = 0.9f;
        P->Facets.Empathy = 0.88f;
        P->Facets.Curiosity = 0.85f;
        P->Facets.SelfControl = 0.85f;
        P->Facets.Sociability = 0.85f;
        P->Facets.Ambition = 0.75f;
        P->Facets.Optimism = 0.8f;
        P->Facets.Impulsivity = 0.2f;
        P->Facets.Vengefulness = 0.1f;
        P->Facets.Stubbornness = 0.35f;

        if (Gifted->EmotionComponent)    Gifted->EmotionComponent->Setup(P);
        if (Gifted->NeedComponent)       Gifted->NeedComponent->Setup(P);
        if (Gifted->MotivationComponent) Gifted->MotivationComponent->Setup(P);
    }

    if (UIdentityComponent* Id = Gifted->IdentityComponent)
    {
        Id->SelfEsteem = 0.72f;
        Id->SelfEfficacy = 0.75f;
        UE_LOG(LogHumanCity, Warning,
            TEXT("Одарён от рождения: %s, %d лет. Никто об этом не знает — станет ли тем, к кому идут, решат люди."),
            *Id->FirstName, FMath::FloorToInt(Id->Age));
    }
}

void ACityGenerator::SeedKnowledge(ACompleteHumanNPC* NPC, const FBuildingInfo* HomeBuilding)
{
    // =======================================================================
    //  ЧЕЛОВЕК НАЧИНАЕТ, НЕ ЗНАЯ НИЧЕГО
    //
    //  Раньше сюда вкладывали готовую картину мира: где твой дом, где твоя
    //  работа, где кафе, и вдобавок многолетнюю привычку ходить на службу.
    //  Человек открывал глаза и уже знал город.
    //
    //  Теперь не знает. Ни одного места, ни одной дороги, ни одной привычки.
    //  Всё, что он будет знать, он узнает сам: увидит по пути, услышит от
    //  других, вычитает из книги. Оттого первые дни у него и уходят на то,
    //  чтобы понять, где тут вода и где можно заработать, — как у всякого,
    //  кто попал в незнакомый город.
    //
    //  Здесь намеренно не остаётся ничего. Это и есть суть правки.
    // =======================================================================
    if (!NPC)
    {
        return;
    }

    // Работа за ним не закреплена: место надо найти и получить.
    if (NPC->IdentityComponent)
    {
        NPC->IdentityComponent->bEmployed = false;
        NPC->IdentityComponent->Occupation = TEXT("без работы");
        NPC->IdentityComponent->HourlyWage = 0.0f;
        NPC->IdentityComponent->LastWorkedAt = 0.0f;
    }

    // Только для проверки разговора: с ключом -LiterateTest каждый уже
    // прочитал «Родную речь». Без ключа слов нет ни у кого.
    if (NPC->SpeechComponent && FParse::Param(FCommandLine::Get(), TEXT("LiterateTest")))
    {
        NPC->SpeechComponent->MasterLetters();
        if (const FTextbook* Reader = FLibrary::Find(TEXT("Reader")))
        {
            for (int32 Pass = 0; Pass < 4; ++Pass)
            {
                for (const FTextbookPage& Page : Reader->Pages)
                {
                    NPC->SpeechComponent->LearnWordsFromBook(Page.Text, 1.0f, false);
                }
            }
        }
    }
}
// ---------------------------------------------------------------------------
//  Поиск зданий
// ---------------------------------------------------------------------------

FBuildingInfo* ACityGenerator::FindBuildingByType(const FString& Type)
{
    for (FBuildingInfo& B : Buildings)
    {
        if (B.Type == Type)
        {
            return &B;
        }
    }
    return nullptr;
}

FBuildingInfo* ACityGenerator::GetRandomBuildingByType(const FString& Type)
{
    TArray<FBuildingInfo*> Matching;
    for (FBuildingInfo& B : Buildings)
    {
        if (B.Type == Type)
        {
            Matching.Add(&B);
        }
    }
    if (Matching.Num() == 0)
    {
        return nullptr;
    }
    return Matching[FMath::RandRange(0, Matching.Num() - 1)];
}

FBuildingInfo* ACityGenerator::GetRandomResidentialBuilding()
{
    // Сначала пробуем дом, где ещё есть место.
    TArray<FBuildingInfo*> Free;
    for (FBuildingInfo& B : Buildings)
    {
        if (B.Type == TEXT("Residential") && B.Occupants.Num() < B.Capacity)
        {
            Free.Add(&B);
        }
    }
    if (Free.Num() > 0)
    {
        return Free[FMath::RandRange(0, Free.Num() - 1)];
    }
    return GetRandomBuildingByType(TEXT("Residential"));
}