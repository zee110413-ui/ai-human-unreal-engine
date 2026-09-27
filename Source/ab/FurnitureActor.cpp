// FurnitureActor.cpp

#include "FurnitureActor.h"
#include "AffordanceComponent.h"
#include "CompleteHumanAI.h"
#include "MindComponent.h"
#include "Textbook.h"
#include "Crafts.h"
#include "Village.h"
#include "ResourceActor.h"
#include "MatterComponent.h"
#include "MatterSubsystem.h"
#include "ImportedModels.h"
#include "EngineUtils.h"

#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

AFurnitureActor::AFurnitureActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    Root->SetMobility(EComponentMobility::Movable);

    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
    Body->SetupAttachment(Root);
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetBoxExtent(FVector(25.0f, 25.0f, 40.0f));
    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetCollisionObjectType(ECC_PhysicsBody);
    Body->SetCollisionResponseToAllChannels(ECR_Block);
    Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Body->SetGenerateOverlapEvents(false);

    Affordances = CreateDefaultSubobject<UAffordanceComponent>(TEXT("Affordances"));

    Matter = CreateDefaultSubobject<UMatterComponent>(TEXT("Matter"));
    Matter->bMassFromShape = false;
}

EResourceKind AFurnitureActor::SubstanceOf(EFurnitureType Type)
{
    switch (Type)
    {
    case EFurnitureType::Stove:
    case EFurnitureType::Forge:
    case EFurnitureType::Kiln:        return EResourceKind::Brick;
    case EFurnitureType::Fridge:
    case EFurnitureType::Washer:
    case EFurnitureType::Computer:
    case EFurnitureType::TV:          return EResourceKind::Steel;
    case EFurnitureType::Lamp:        return EResourceKind::Iron;
    case EFurnitureType::Toilet:
    case EFurnitureType::Sink:
    case EFurnitureType::Bath:
    case EFurnitureType::Shower:
    case EFurnitureType::Plant:       return EResourceKind::Pot;
    case EFurnitureType::Mirror:      return EResourceKind::Glass;
    case EFurnitureType::Rug:         return EResourceKind::Cloth;
    case EFurnitureType::StonePile:
    case EFurnitureType::Well:
    case EFurnitureType::Spring:      return EResourceKind::Stone;
    case EFurnitureType::ClayPit:     return EResourceKind::Clay;
    case EFurnitureType::GardenBed:
    case EFurnitureType::WildField:   return EResourceKind::Soil;
    case EFurnitureType::Mushrooms:   return EResourceKind::Mushroom;
    case EFurnitureType::Bush:        return EResourceKind::Herb;
    case EFurnitureType::Textbook:    return EResourceKind::Paper;
    default:                          return EResourceKind::Wood;
    }
}

void AFurnitureActor::BeginPlay()
{
    Super::BeginPlay();

    if (Parts.Num() == 0)
    {
        BuildLook();
    }
    if (Affordances && Affordances->Offers.Num() == 0)
    {
        BuildOffers();
    }

    MakeSolid();
}

float AFurnitureActor::MaterialNearby(EResourceKind Kind, float Radius) const
{
    UWorld* World = GetWorld();
    if (!World || Kind == EResourceKind::None)
    {
        return 0.0f;
    }

    const FVector Here = GetActorLocation();
    float Total = 0.0f;

    for (TActorIterator<AFurnitureActor> It(World); It; ++It)
    {
        AFurnitureActor* Thing = *It;
        if (Thing && FVector::Dist(Thing->GetActorLocation(), Here) <= Radius)
        {
            if (const float* Stock = Thing->Stored.Find(Kind))
            {
                Total += *Stock;
            }
        }
    }

    for (TActorIterator<AResourceActor> It(World); It; ++It)
    {
        AResourceActor* Pile = *It;
        if (Pile && Pile->Kind == Kind && Pile->IsAvailable()
            && FVector::Dist(Pile->GetActorLocation(), Here) <= Radius)
        {
            Total += Pile->Amount;
        }
    }

    return Total;
}

float AFurnitureActor::SpendNearby(EResourceKind Kind, float HowMuch, float Radius)
{
    UWorld* World = GetWorld();
    if (!World || Kind == EResourceKind::None || HowMuch <= 0.0f)
    {
        return 0.0f;
    }

    const FVector Here = GetActorLocation();
    float Left = HowMuch;

    for (TActorIterator<AFurnitureActor> It(World); It && Left > 0.0f; ++It)
    {
        AFurnitureActor* Thing = *It;
        if (Thing && FVector::Dist(Thing->GetActorLocation(), Here) <= Radius)
        {
            Left -= Thing->TakeOut(Kind, Left);
        }
    }

    for (TActorIterator<AResourceActor> It(World); It && Left > 0.0f; ++It)
    {
        AResourceActor* Pile = *It;
        if (Pile && Pile->Kind == Kind && Pile->IsAvailable()
            && FVector::Dist(Pile->GetActorLocation(), Here) <= Radius)
        {
            Left -= Pile->Consume(Left);
        }
    }

    return HowMuch - FMath::Max(0.0f, Left);
}

void AFurnitureActor::RefreshCraftOffers()
{
    if (!Affordances)
    {
        return;
    }

    TArray<const FCraft*> Possible;
    FCraftBook::AtStation(FurnitureType, Possible);

    Affordances->Offers.RemoveAll([](const FAffordance& A) { return !A.Craft.IsNone(); });
    if (Possible.Num() == 0)
    {
        return;
    }

    const int32 Day = FVillage::DayOfYear(this);
    for (const FCraft* Deed : Possible)
    {
        const FVillageWork* Chore = FVillage::FindWork(Deed->Id);
        if (!Chore && Substance != EResourceKind::None && Deed->Inputs.Num() == 0
            && Deed->Output != EResourceKind::None && Deed->Output != Substance)
        {
            continue;
        }
        if (Chore && !FVillage::InSeason(*Chore, Day))
        {
            continue;
        }
        if (Chore && FurnitureType == EFurnitureType::GardenBed)
        {
            // Жатва — дело без входов с выходом; посев — дело без выхода.
            const bool bReaping = Chore->Input == EResourceKind::None && Chore->Output != EResourceKind::None;
            const bool bSowing = Chore->Output == EResourceKind::None;
            if ((bReaping && Ripeness < 0.55f) || (bSowing && Ripeness > 0.0f))
            {
                continue;
            }
        }

        bool bHaveAll = true;
        for (const FCraftPart& Need : Deed->Inputs)
        {
            if (Need.Amount > 0.0f && MaterialNearby(Need.Kind, Chore ? 1200.0f : 800.0f) < Need.Amount)
            {
                bHaveAll = false;
                break;
            }
        }
        if (!bHaveAll)
        {
            continue;
        }

        FAffordance Offer;
        Offer.Action = Deed->Output == EResourceKind::CookedFood || Deed->Output == EResourceKind::Bread ? EActionType::Cook : EActionType::Work;
        Offer.Source = EAffordanceSource::Object;
        Offer.Craft = Deed->Id;
        Offer.Label = Deed->Label;
        Offer.Key = FName(*FString::Printf(TEXT("%s@%s"), *Deed->Id.ToString(), *GetName()));
        Offer.CategoryKey = Deed->Id;
        Offer.Duration = Deed->Duration;
        Offer.EffortCost = Deed->Effort;
        Offer.Difficulty = Deed->Difficulty;
        Offer.RequiredSkill = Deed->Skill;
        if (Chore)
        {
            FNeedPromise Pride;
            Pride.Need = ENeedType::Achievement;
            Pride.Amount = 0.14f;
            Offer.Promises.Add(Pride);
            FNeedPromise Respect;
            Respect.Need = ENeedType::Esteem;
            Respect.Amount = 0.08f;
            Offer.Promises.Add(Respect);
            if (FVillage::IsFood(Deed->Output) || Deed->Output == EResourceKind::Firewood || Deed->Output == EResourceKind::Water)
            {
                FNeedPromise Provide;
                Provide.Need = ENeedType::Safety;
                Provide.Amount = 0.12f;
                Offer.Promises.Add(Provide);
            }
        }

        FNeedPromise Made;
        Made.Need = ENeedType::Competence;
        Made.Amount = 0.16f;
        Offer.Promises.Add(Made);

        FNeedPromise Sense;
        Sense.Need = ENeedType::Meaning;
        Sense.Amount = 0.1f;
        Offer.Promises.Add(Sense);

        if (Deed->Output == EResourceKind::CookedFood || Deed->Output == EResourceKind::Bread)
        {
            FNeedPromise Food;
            Food.Need = ENeedType::Hunger;
            Food.Amount = 0.14f;
            Offer.Promises.Add(Food);
        }
        else if (FVillage::IsFood(Deed->Output))
        {
            // Новые припасы — сыр, масло, копчёности, плоды — тоже кормят.
            // Без этого человек варил бы вино, но не желал его.
            FNeedPromise Food;
            Food.Need = ENeedType::Hunger;
            Food.Amount = FVillage::Portions(Deed->Output) > 0.0f ? 0.10f : 0.04f;
            Offer.Promises.Add(Food);

            // А вино, пиво и медовуха ещё и веселят: за них и берутся.
            if (Deed->Output == EResourceKind::Wine || Deed->Output == EResourceKind::Beer
                || Deed->Output == EResourceKind::Mead || Deed->Output == EResourceKind::Kvass)
            {
                FNeedPromise Cheer;
                Cheer.Need = ENeedType::Comfort;
                Cheer.Amount = 0.14f;
                Offer.Promises.Add(Cheer);
                FNeedPromise Guests;
                Guests.Need = ENeedType::SocialContact;
                Guests.Amount = 0.08f;
                Offer.Promises.Add(Guests);
            }
        }

        Affordances->Offers.Add(Offer);
    }
}

float AFurnitureActor::MassOf(EFurnitureType Type)
{
    switch (Type)
    {
    case EFurnitureType::Bed:         return 120.0f;
    case EFurnitureType::Chair:       return 7.0f;
    case EFurnitureType::Table:       return 35.0f;
    case EFurnitureType::Stove:       return 80.0f;
    case EFurnitureType::TV:          return 12.0f;
    case EFurnitureType::Sofa:        return 90.0f;
    case EFurnitureType::Wardrobe:    return 140.0f;
    case EFurnitureType::Fridge:      return 90.0f;
    case EFurnitureType::Toilet:      return 60.0f;
    case EFurnitureType::Shower:      return 70.0f;
    case EFurnitureType::Sink:        return 40.0f;
    case EFurnitureType::Desk:        return 30.0f;
    case EFurnitureType::Computer:    return 8.0f;
    case EFurnitureType::Bookshelf:   return 60.0f;
    case EFurnitureType::Lamp:        return 3.0f;
    case EFurnitureType::Plant:       return 6.0f;
    case EFurnitureType::Bath:        return 110.0f;
    case EFurnitureType::Mirror:      return 9.0f;
    case EFurnitureType::Nightstand:  return 11.0f;
    case EFurnitureType::Rug:         return 4.0f;
    case EFurnitureType::Counter:     return 45.0f;
    case EFurnitureType::Washer:      return 70.0f;
    case EFurnitureType::Crib:        return 25.0f;
    case EFurnitureType::Piano:       return 300.0f;
    case EFurnitureType::Textbook:    return 1.2f;
    case EFurnitureType::Blackboard:  return 30.0f;
    case EFurnitureType::SchoolDesk:  return 20.0f;
    case EFurnitureType::Globe:       return 2.0f;
    case EFurnitureType::Abacus:      return 1.0f;
    case EFurnitureType::GardenBed:   return 400.0f;
    case EFurnitureType::Barrel:      return 40.0f;
    case EFurnitureType::MarketStall: return 50.0f;
    default:                          return 20.0f;
    }
}

bool AFurnitureActor::FixtureOf(EFurnitureType Type)
{
    switch (Type)
    {
    case EFurnitureType::Toilet:
    case EFurnitureType::Shower:
    case EFurnitureType::Sink:
    case EFurnitureType::Bath:
    case EFurnitureType::GardenBed:
    case EFurnitureType::Spring:
    case EFurnitureType::Bush:
    case EFurnitureType::Mushrooms:
    case EFurnitureType::WildField:
    case EFurnitureType::Tree:
    case EFurnitureType::StonePile:
    case EFurnitureType::ClayPit:
        return true;
    default:
        return false;
    }
}

void AFurnitureActor::MakeSolid()
{
    if (!Body)
    {
        return;
    }

    MassKg = MassOf(FurnitureType);
    bFixture = FixtureOf(FurnitureType);

    FBox Bounds(ForceInit);
    for (UStaticMeshComponent* Piece : Parts)
    {
        if (Piece)
        {
            Bounds += Piece->Bounds.GetBox();
        }
    }

    if (!Bounds.IsValid)
    {
        return;
    }

    FVector Extent = Bounds.GetExtent().ComponentMax(FVector(6.0f, 6.0f, 6.0f));
    FVector Centre = Bounds.GetCenter();
    if (FurnitureType == EFurnitureType::Tree)
    {
        const float Tall = FMath::Min(static_cast<float>(Extent.Z), 150.0f);
        Extent = FVector(26.0f, 26.0f, Tall);
        Centre = GetActorLocation() + FVector(0.0f, 0.0f, Tall);
    }
    else if (FurnitureType == EFurnitureType::WildField || FurnitureType == EFurnitureType::Rug)
    {
        Extent = FVector(Extent.X, Extent.Y, 2.0f);
        Centre = FVector(Centre.X, Centre.Y, GetActorLocation().Z + 1.0f);
    }
    Body->SetWorldLocation(Centre);
    Body->SetWorldRotation(GetActorRotation());
    Body->SetBoxExtent(Extent);
    Body->SetMassOverrideInKg(NAME_None, MassKg, true);

    if (Matter)
    {
        Matter->SetDryMass(MassKg);
        Matter->Bind(Body, EMatterShape::Box, Extent * 2.0f, SubstanceOf(FurnitureType));
        for (UStaticMeshComponent* Piece : Parts)
        {
            Matter->AddVisual(Piece);
        }
    }
}

bool AFurnitureActor::CanBeLifted() const
{
    return !bFixture && MassKg <= 12.0f && !HeldBy.IsValid();
}

bool AFurnitureActor::Shove(const FVector& Direction, float Effort)
{
    if (bFixture || HeldBy.IsValid() || Effort <= 0.0f)
    {
        return false;
    }

    FVector Push = Direction;
    Push.Z = 0.0f;
    Push = Push.GetSafeNormal();
    if (Push.IsNearlyZero())
    {
        return false;
    }

    const float Slide = Effort * FMath::Clamp(16.0f / FMath::Max(1.0f, MassKg), 0.02f, 1.4f);
    if (Slide < 0.05f)
    {
        return false;
    }

    FHitResult Hit;
    AddActorWorldOffset(Push * Slide, true, &Hit);
    return true;
}

bool AFurnitureActor::PickedUp(AActor* Who)
{
    if (!Who || !CanBeLifted())
    {
        return false;
    }

    HeldBy = Who;
    if (Body)
    {
        Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    AttachToActor(Who, FAttachmentTransformRules::KeepWorldTransform);
    return true;
}

void AFurnitureActor::PutDown()
{
    if (HeldBy.IsValid())
    {
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }
    HeldBy = nullptr;

    UWorld* World = GetWorld();
    if (World)
    {
        const FVector From = GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(FurnitureDrop), false, this);
        FHitResult Ground;
        if (World->LineTraceSingleByChannel(Ground, From, From - FVector(0.0f, 0.0f, 400.0f), ECC_WorldStatic, Params))
        {
            SetActorLocation(Ground.ImpactPoint);
        }
    }

    SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, 0.0f));
    if (Body)
    {
        Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    MakeSolid();
}

// ---------------------------------------------------------------------------
//  Как вещь выглядит
//
//  Вся мебель была одинаковым кубом метр на метр, наполовину утопленным
//  в полу. Комната от этого выглядела складом ящиков, и понять, где кровать,
//  а где плита, было нельзя.
//
//  Теперь вещь складывается из частей и стоит на полу.
// ---------------------------------------------------------------------------

UStaticMesh* AFurnitureActor::Shape(const TCHAR* Name)
{
    return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
}

UMaterialInterface* AFurnitureActor::Surface(const TCHAR* Name)
{
    return LoadObject<UMaterialInterface>(nullptr,
        *FString::Printf(TEXT("/Game/StarterContent/Materials/%s.%s"), Name, Name));
}

UStaticMeshComponent* AFurnitureActor::Part(const TCHAR* ShapeName, const TCHAR* Material,
                                            const FVector& Offset, const FVector& Size,
                                            float YawDegrees)
{
    UStaticMesh* MeshAsset = Shape(ShapeName);
    if (!MeshAsset)
    {
        return nullptr;
    }

    UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
    Piece->SetStaticMesh(MeshAsset);
    Piece->SetMobility(EComponentMobility::Movable);
    Piece->SetupAttachment(Root);

    // Базовые формы движка — сто сантиметров и отсчитываются от центра.
    // Мебель удобнее задавать по её низу.
    Piece->SetRelativeLocation(Offset + FVector(0.0f, 0.0f, Size.Z * 0.5f));
    Piece->SetRelativeRotation(FRotator(0.0f, YawDegrees, 0.0f));
    Piece->SetRelativeScale3D(Size / 100.0f);

    // Мебель не должна быть препятствием: человек идёт к плите «на глаз»,
    // и стул посреди комнаты он воспринял бы как стену и начал бы её обходить.
    Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    UMaterialInterface* Look = nullptr;
    if (UMatterSubsystem* MatterWorld = UMatterSubsystem::Get(this))
    {
        const FString Starter(Material);
        EMatterLook Kind = EMatterLook::Count;
        if (Starter == TEXT("M_Wood_Oak"))                   Kind = EMatterLook::Oak;
        else if (Starter == TEXT("M_Wood_Walnut"))           Kind = EMatterLook::Walnut;
        else if (Starter == TEXT("M_Wood_Pine"))             Kind = EMatterLook::Pine;
        else if (Starter == TEXT("M_Metal_Steel"))           Kind = EMatterLook::Steel;
        else if (Starter == TEXT("M_Metal_Chrome"))          Kind = EMatterLook::Silver;
        else if (Starter == TEXT("M_Metal_Burnished_Steel")) Kind = EMatterLook::Iron;
        else if (Starter == TEXT("M_Concrete_Tiles"))        Kind = EMatterLook::Tiles;
        else if (Starter == TEXT("M_Basic_Wall"))            Kind = EMatterLook::Cloth;
        else if (Starter == TEXT("M_Glass"))                 Kind = EMatterLook::Glass;
        else if (Starter == TEXT("M_Ground_Moss"))           Kind = EMatterLook::Moss;
        else if (Starter == TEXT("M_Ground_Grass"))          Kind = EMatterLook::Greens;
        else if (Starter == TEXT("M_Ground_Gravel"))         Kind = EMatterLook::Soil;

        switch (FurnitureType)
        {
        case EFurnitureType::StonePile:
        case EFurnitureType::Well:
        case EFurnitureType::Spring:
            if (Kind == EMatterLook::Cloth || Kind == EMatterLook::Tiles)
            {
                Kind = EMatterLook::Stone;
            }
            break;
        case EFurnitureType::ClayPit:
            if (Kind == EMatterLook::Tiles)
            {
                Kind = EMatterLook::Clay;
            }
            break;
        case EFurnitureType::FishingSpot:
            if (Kind == EMatterLook::Tiles)
            {
                Kind = EMatterLook::Sand;
            }
            break;
        case EFurnitureType::Mushrooms:
            if (Kind == EMatterLook::Tiles)
            {
                Kind = EMatterLook::Plaster;
            }
            break;
        case EFurnitureType::Tree:
        case EFurnitureType::Bush:
            if (Kind == EMatterLook::Walnut)
            {
                Kind = EMatterLook::Bark;
            }
            else if (Kind == EMatterLook::Moss)
            {
                Kind = EMatterLook::Greens;
            }
            break;
        case EFurnitureType::MarketStall:
            if (Kind == EMatterLook::Cloth)
            {
                Kind = EMatterLook::Straw;
            }
            break;
        default:
            break;
        }

        if (Kind != EMatterLook::Count)
        {
            Look = MatterWorld->Look(Kind);
        }
    }
    if (!Look)
    {
        Look = Surface(Material);
    }
    if (Look)
    {
        Piece->SetMaterial(0, Look);
    }

    Piece->RegisterComponent();
    Parts.Add(Piece);

    if (!Mesh)
    {
        Mesh = Piece;
    }
    return Piece;
}

bool AFurnitureActor::Dress(const TCHAR* Folder, const TCHAR* Filter, const FVector& Offset, const FVector& Size, float YawDegrees, bool bStretch)
{
    TArray<UStaticMeshComponent*> Made;
    if (FImportedModels::Place(this, Root, Folder, Filter, Offset, Size, YawDegrees, false, &Made, bStretch) == 0)
    {
        return false;
    }
    for (UStaticMeshComponent* Piece : Made)
    {
        Parts.Add(Piece);
        if (!Mesh)
        {
            Mesh = Piece;
        }
    }
    return true;
}

bool AFurnitureActor::DressFromModels()
{
    const uint32 Seed = GetTypeHash(GetFName()) ^ GetTypeHash(GetActorLocation().GridSnap(10.0f));
    const int32 Pick = static_cast<int32>(Seed % 5u) + 1;
    auto Nature = [](const TCHAR* Name, int32 Index)
    {
        return FString::Printf(TEXT("%s_%d"), Name, Index);
    };
    const TCHAR* PH = TEXT("PolyHaven/");
    const TCHAR* KI = TEXT("KenneyItems/");
    auto In = [](const TCHAR* Root, const TCHAR* Name)
    {
        return FString(Root) + Name;
    };

    bool bDone = false;
    switch (FurnitureType)
    {
    case EFurnitureType::Bed:
        bDone = Dress(*In(KI, TEXT("bedSingle")), TEXT(""), FVector::ZeroVector, FVector(110, 210, 70), 90.0f);
        break;
    case EFurnitureType::Crib:
        bDone = Dress(*In(KI, TEXT("bedSingle")), TEXT(""), FVector::ZeroVector, FVector(65, 115, 55), 90.0f);
        break;
    case EFurnitureType::Chair:
        bDone = Dress(*In(PH, TEXT("wooden_stool_01")), TEXT(""), FVector::ZeroVector, FVector(44, 44, 46));
        break;
    case EFurnitureType::Table:
        bDone = Dress(*In(PH, TEXT("wooden_table_02")), TEXT(""), FVector::ZeroVector, FVector(150, 95, 78));
        break;
    case EFurnitureType::Desk:
        bDone = Dress(*In(KI, TEXT("desk")), TEXT(""), FVector::ZeroVector, FVector(130, 70, 76));
        break;
    case EFurnitureType::SchoolDesk:
        bDone = Dress(*In(KI, TEXT("desk")), TEXT(""), FVector::ZeroVector, FVector(110, 60, 70));
        break;
    case EFurnitureType::Stove:
        bDone = Dress(TEXT("Quaternius/Village"), TEXT("Prop_Chimney2"), FVector::ZeroVector, FVector(150, 130, 160), 0.0f, true);
        Dress(TEXT("Quaternius/Village"), TEXT("Prop_Chimney"), FVector(-35, 0, 160), FVector(55, 55, 120), 0.0f, true);
        break;
    case EFurnitureType::Counter:
        bDone = Dress(*In(KI, TEXT("kitchenCabinet")), TEXT(""), FVector::ZeroVector, FVector(160, 60, 90), 0.0f, true);
        break;
    case EFurnitureType::Fridge:
        bDone = Dress(*In(KI, TEXT("kitchenFridge")), TEXT(""), FVector::ZeroVector, FVector(70, 68, 185));
        break;
    case EFurnitureType::Sink:
        bDone = Dress(*In(KI, TEXT("kitchenSink")), TEXT(""), FVector::ZeroVector, FVector(70, 60, 90));
        break;
    case EFurnitureType::Bath:
        bDone = Dress(*In(KI, TEXT("bathtub")), TEXT(""), FVector::ZeroVector, FVector(175, 80, 60));
        break;
    case EFurnitureType::Shower:
        bDone = Dress(*In(KI, TEXT("shower")), TEXT(""), FVector::ZeroVector, FVector(95, 95, 200));
        break;
    case EFurnitureType::Toilet:
        bDone = Dress(*In(KI, TEXT("toilet")), TEXT(""), FVector::ZeroVector, FVector(45, 60, 80));
        break;
    case EFurnitureType::Washer:
        bDone = Dress(*In(KI, TEXT("washer")), TEXT(""), FVector::ZeroVector, FVector(60, 60, 85));
        break;
    case EFurnitureType::TV:
        bDone = Dress(*In(KI, TEXT("televisionVintage")), TEXT(""), FVector::ZeroVector, FVector(60, 45, 45));
        break;
    case EFurnitureType::Computer:
        bDone = Dress(*In(KI, TEXT("computerScreen")), TEXT(""), FVector::ZeroVector, FVector(55, 20, 42));
        break;
    case EFurnitureType::Sofa:
        bDone = Dress(*In(KI, TEXT("loungeSofa")), TEXT(""), FVector::ZeroVector, FVector(200, 90, 90));
        break;
    case EFurnitureType::Wardrobe:
        bDone = Dress(*In(KI, TEXT("bookcaseClosedDoors")), TEXT(""), FVector::ZeroVector, FVector(110, 60, 200));
        break;
    case EFurnitureType::Bookshelf:
        bDone = Dress(*In(PH, TEXT("wooden_bookshelf_worn")), TEXT(""), FVector::ZeroVector, FVector(100, 45, 180));
        break;
    case EFurnitureType::Lamp:
        bDone = Dress(*In(PH, TEXT("wooden_lantern_01")), TEXT(""), FVector::ZeroVector, FVector(22, 22, 40));
        break;
    case EFurnitureType::Plant:
        bDone = Dress(*In(KI, TEXT("pottedPlant")), TEXT(""), FVector::ZeroVector, FVector(40, 40, 70));
        break;
    case EFurnitureType::Mirror:
        bDone = Dress(*In(KI, TEXT("bathroomMirror")), TEXT(""), FVector::ZeroVector, FVector(60, 20, 90));
        break;
    case EFurnitureType::Nightstand:
        bDone = Dress(*In(KI, TEXT("sideTableDrawers")), TEXT(""), FVector::ZeroVector, FVector(60, 40, 55));
        break;
    case EFurnitureType::Rug:
        bDone = Dress(*In(KI, TEXT("rugRectangle")), TEXT(""), FVector::ZeroVector, FVector(200, 130, 2), 0.0f, true);
        break;
    case EFurnitureType::Piano:
        bDone = Dress(*In(KI, TEXT("bookcaseClosedWide")), TEXT(""), FVector::ZeroVector, FVector(150, 60, 130));
        break;
    case EFurnitureType::Textbook:
        bDone = Dress(*In(PH, TEXT("book_encyclopedia_set_01")), TEXT("book_encyclopedia_set_01_book05"), FVector::ZeroVector, FVector(24, 24, 24));
        break;
    case EFurnitureType::Blackboard:
        bDone = Dress(*In(PH, TEXT("standing_chalkboard_01")), TEXT(""), FVector::ZeroVector, FVector(120, 80, 170));
        break;
    case EFurnitureType::Globe:
        bDone = Dress(*In(KI, TEXT("lampRoundTable")), TEXT(""), FVector::ZeroVector, FVector(35, 35, 45));
        break;
    case EFurnitureType::Abacus:
        bDone = Dress(*In(PH, TEXT("carved_wooden_plate")), TEXT(""), FVector::ZeroVector, FVector(35, 35, 5));
        break;
    case EFurnitureType::GardenBed:
    {
        if (FVillage::IsMedieval(this))
        {
            const float Grown = FMath::Clamp(Ripeness, 0.0f, 1.0f);
            const bool bStubble = Grown < 0.02f;
            for (int32 Tuft = 0; Tuft < 6; ++Tuft)
            {
                const float X = (Tuft % 3 - 1) * 95.0f;
                const float Y = Tuft / 3 == 0 ? -45.0f : 45.0f;
                bDone |= Dress(TEXT("Quaternius/Nature"), bStubble ? TEXT("Grass_Wispy_Short") : TEXT("Grass_Wispy_Tall"),
                    FVector(X, Y, 0.0f), FVector(110.0f, 110.0f, bStubble ? 12.0f : FMath::Lerp(20.0f, 115.0f, Grown)), Tuft * 60.0f);
            }
            break;
        }
        bDone = Dress(*In(PH, TEXT("planter_box_01")), TEXT(""), FVector::ZeroVector, FVector(240, 130, 40), 0.0f, true);
        const float Grown = FMath::Clamp(Ripeness, 0.15f, 1.0f);
        for (int32 Row = -1; Row <= 1; ++Row)
        {
            Dress(TEXT("Quaternius/Nature"), TEXT("Plant_7"), FVector(Row * 70.0f, 0.0f, 34.0f), FVector(60, 60, 40) * Grown);
        }
        break;
    }
    case EFurnitureType::Barrel:
        bDone = Dress(*In(PH, TEXT("wooden_barrels_01")), TEXT("wooden_barrels_01_barrel011"), FVector::ZeroVector, FVector(60, 60, 90));
        break;
    case EFurnitureType::MarketStall:
        bDone = Dress(*In(PH, TEXT("wooden_table_02")), TEXT(""), FVector::ZeroVector, FVector(180, 90, 85), 0.0f, true);
        Dress(*In(PH, TEXT("wicker_basket_01")), TEXT(""), FVector(-45, 0, 85), FVector(45, 34, 14));
        Dress(*In(PH, TEXT("food_apple_01")), TEXT(""), FVector(-50, 5, 88), FVector(9, 9, 9));
        Dress(*In(PH, TEXT("food_apple_01")), TEXT(""), FVector(-40, -6, 88), FVector(9, 9, 9));
        Dress(*In(PH, TEXT("yellow_onion")), TEXT(""), FVector(30, 10, 85), FVector(8, 8, 9));
        Dress(*In(PH, TEXT("yellow_onion")), TEXT(""), FVector(40, -8, 85), FVector(8, 8, 9));
        Dress(*In(PH, TEXT("wooden_crate_01")), TEXT(""), FVector(0, 75, 0), FVector(80, 40, 36));
        break;
    case EFurnitureType::Tree:
        bDone = Dress(TEXT("Quaternius/Nature"), *Nature(Seed % 3u == 0 ? TEXT("Pine") : TEXT("CommonTree"), Pick),
            FVector::ZeroVector, FVector(480, 480, 800), static_cast<float>(Seed % 360u));
        break;
    case EFurnitureType::Bush:
        bDone = Dress(TEXT("Quaternius/Nature"), Seed % 2u == 0 ? TEXT("Bush_Common") : TEXT("Bush_Common_Flowers"),
            FVector::ZeroVector, FVector(170, 170, 140), static_cast<float>(Seed % 360u));
        break;
    case EFurnitureType::Mushrooms:
        bDone = Dress(TEXT("Quaternius/Nature"), TEXT("Mushroom_Common"), FVector::ZeroVector, FVector(55, 55, 40));
        Dress(TEXT("Quaternius/Nature"), TEXT("Mushroom_Laetiporus"), FVector(45, 30, 0), FVector(40, 40, 30), 70.0f);
        break;
    case EFurnitureType::WildField:
        for (int32 Tuft = 0; Tuft < 7; ++Tuft)
        {
            const float Angle = Tuft * 0.9f + (Seed % 7u);
            const float Reach = 40.0f + (Tuft * 37 % 110);
            bDone |= Dress(TEXT("Quaternius/Nature"), Tuft % 2 == 0 ? TEXT("Grass_Wispy_Tall") : TEXT("Grass_Common_Tall"),
                FVector(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach, 0.0f), FVector(120, 120, 95), Tuft * 50.0f);
        }
        break;
    case EFurnitureType::StonePile:
        bDone = Dress(TEXT("Quaternius/Nature"), *Nature(TEXT("Rock_Medium"), 1 + static_cast<int32>(Seed % 3u)),
            FVector::ZeroVector, FVector(150, 150, 100), static_cast<float>(Seed % 360u));
        Dress(TEXT("Quaternius/Nature"), *Nature(TEXT("Pebble_Square"), 1 + static_cast<int32>(Seed % 6u)), FVector(80, 20, 0), FVector(40, 40, 16));
        Dress(TEXT("Quaternius/Nature"), *Nature(TEXT("Pebble_Round"), 1 + static_cast<int32>(Seed % 5u)), FVector(-70, -40, 0), FVector(40, 40, 12));
        break;
    case EFurnitureType::ClayPit:
        bDone = Dress(TEXT("Quaternius/Nature"), TEXT("RockPath_Round_Wide"), FVector::ZeroVector, FVector(200, 200, 12), 0.0f, true);
        Dress(TEXT("Quaternius/Nature"), TEXT("Pebble_Square_2"), FVector(60, -40, 8), FVector(35, 35, 14));
        Dress(TEXT("Quaternius/Nature"), TEXT("Pebble_Square_4"), FVector(-50, 50, 8), FVector(35, 35, 14));
        break;
    case EFurnitureType::Spring:
        for (int32 Stone = 0; Stone < 6; ++Stone)
        {
            const float Angle = Stone * UE_TWO_PI / 6.0f;
            bDone |= Dress(TEXT("Quaternius/Nature"), *Nature(TEXT("Pebble_Round"), 1 + Stone % 5),
                FVector(FMath::Cos(Angle) * 70.0f, FMath::Sin(Angle) * 70.0f, 0.0f), FVector(45, 45, 14), Stone * 60.0f);
        }
        Dress(TEXT("Quaternius/Nature"), TEXT("Fern_1"), FVector(-110, 40, 0), FVector(120, 120, 60));
        break;
    case EFurnitureType::FishingSpot:
        bDone = Dress(*In(PH, TEXT("modular_wooden_pier")), TEXT("modular_wooden_pier_planks1"), FVector(-60, 0, 12), FVector(220, 190, 10), 0.0f, true);
        Dress(*In(PH, TEXT("wooden_bucket_01")), TEXT(""), FVector(40, 50, 22), FVector(34, 34, 34));
        break;
    case EFurnitureType::Well:
        for (int32 Stone = 0; Stone < 8; ++Stone)
        {
            const float Angle = Stone * UE_TWO_PI / 8.0f;
            for (int32 Layer = 0; Layer < 3; ++Layer)
            {
                bDone |= Dress(TEXT("Quaternius/Nature"), *Nature(TEXT("Pebble_Square"), 1 + (Stone + Layer) % 6),
                    FVector(FMath::Cos(Angle) * 62.0f, FMath::Sin(Angle) * 62.0f, Layer * 22.0f), FVector(48, 40, 24),
                    FMath::RadiansToDegrees(Angle) + Layer * 20.0f);
            }
        }
        Dress(TEXT("Quaternius/Village"), TEXT("Prop_Support"), FVector(0, 0, 70), FVector(20, 170, 160), 0.0f, true);
        Dress(*In(PH, TEXT("wooden_bucket_01")), TEXT(""), FVector(55, 0, 66), FVector(30, 30, 30));
        break;
    case EFurnitureType::Workbench:
        bDone = Dress(*In(PH, TEXT("wooden_table_02")), TEXT(""), FVector::ZeroVector, FVector(180, 80, 90), 0.0f, true);
        Dress(*In(PH, TEXT("wooden_axe")), TEXT(""), FVector(50, 10, 90), FVector(8, 25, 60), 90.0f);
        break;
    case EFurnitureType::Sawhorse:
        bDone = Dress(*In(KI, TEXT("tableCross")), TEXT(""), FVector::ZeroVector, FVector(110, 55, 75), 0.0f, true);
        break;
    case EFurnitureType::Forge:
        bDone = Dress(*In(PH, TEXT("stone_fire_pit")), TEXT(""), FVector::ZeroVector, FVector(130, 130, 40));
        Dress(TEXT("Quaternius/Village"), TEXT("Prop_Brick1"), FVector(80, 20, 0), FVector(34, 25, 21));
        Dress(TEXT("Quaternius/Village"), TEXT("Prop_Brick3"), FVector(85, -25, 0), FVector(38, 22, 25));
        Dress(*In(PH, TEXT("wooden_bucket_01")), TEXT(""), FVector(-90, 30, 0), FVector(34, 34, 34));
        break;
    case EFurnitureType::Kiln:
        bDone = Dress(TEXT("Quaternius/Village"), TEXT("Prop_Chimney2"), FVector::ZeroVector, FVector(140, 140, 170), 0.0f, true);
        Dress(*In(PH, TEXT("stone_fire_pit")), TEXT(""), FVector(100, 0, 0), FVector(80, 80, 25));
        break;
    case EFurnitureType::Loom:
        bDone = Dress(*In(PH, TEXT("spinning_wheel_01")), TEXT(""), FVector::ZeroVector, FVector(60, 110, 100));
        break;
    case EFurnitureType::PotteryWheel:
        bDone = Dress(*In(PH, TEXT("wooden_stool_01")), TEXT(""), FVector::ZeroVector, FVector(50, 50, 50));
        Dress(*In(PH, TEXT("jug_01")), TEXT(""), FVector(0, 0, 50), FVector(28, 18, 22));
        break;
    case EFurnitureType::Beehive:
        bDone = Dress(*In(PH, TEXT("wooden_barrels_01")), TEXT("wooden_barrels_01_barrel021"), FVector::ZeroVector, FVector(50, 50, 75));
        break;
    case EFurnitureType::Shed:
        bDone = Dress(TEXT("Quaternius/Village"), TEXT("Prop_Crate"), FVector::ZeroVector, FVector(300, 260, 230), 0.0f, true);
        break;
    case EFurnitureType::Bench:
        bDone = Dress(*In(PH, TEXT("painted_wooden_bench")), TEXT(""), FVector::ZeroVector, FVector(160, 50, 90));
        break;
    case EFurnitureType::Cart:
        bDone = Dress(TEXT("Quaternius/Village"), TEXT("Prop_Wagon"), FVector::ZeroVector, FVector(120, 250, 110), 90.0f);
        break;
    case EFurnitureType::Fence:
        bDone = Dress(TEXT("Quaternius/Village"), TEXT("Prop_WoodenFence_Single"), FVector::ZeroVector, FVector(300, 12, 115), 90.0f, true);
        break;
    default:
        break;
    }
    return bDone;
}

void AFurnitureActor::BuildLook()
{
    if (DressFromModels())
    {
        return;
    }
    // Материалы стартового набора: дерево, металл, камень, стекло.
    const TCHAR* Wood   = TEXT("M_Wood_Oak");
    const TCHAR* Walnut = TEXT("M_Wood_Walnut");
    const TCHAR* Pine   = TEXT("M_Wood_Pine");
    const TCHAR* Steel  = TEXT("M_Metal_Steel");
    const TCHAR* Chrome = TEXT("M_Metal_Chrome");
    const TCHAR* Burn   = TEXT("M_Metal_Burnished_Steel");
    const TCHAR* Tile   = TEXT("M_Concrete_Tiles");
    const TCHAR* Brick  = TEXT("M_Basic_Wall");
    const TCHAR* Glass  = TEXT("M_Glass");
    const TCHAR* Moss   = TEXT("M_Ground_Moss");
    const TCHAR* Grass  = TEXT("M_Ground_Grass");

    switch (FurnitureType)
    {
    case EFurnitureType::Bed:
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 0),      FVector(200, 150, 28));   // основание
        Part(TEXT("Cube"), Brick,  FVector(0, 0, 28),     FVector(196, 146, 24));   // матрас
        Part(TEXT("Cube"), Brick,  FVector(-75, 0, 52),   FVector(40, 120, 14));    // подушка
        Part(TEXT("Cube"), Walnut, FVector(-100, 0, 0),   FVector(12, 150, 95));    // изголовье
        break;

    case EFurnitureType::Crib:
        Part(TEXT("Cube"), Pine,   FVector(0, 0, 30),     FVector(110, 65, 12));
        Part(TEXT("Cube"), Brick,  FVector(0, 0, 42),     FVector(104, 60, 12));
        for (int32 S = -1; S <= 1; S += 2)
        {
            Part(TEXT("Cube"), Pine, FVector(0, S * 32, 0), FVector(110, 6, 80));
            Part(TEXT("Cube"), Pine, FVector(S * 55, 0, 0), FVector(6, 65, 80));
        }
        break;

    case EFurnitureType::Chair:
        Part(TEXT("Cube"), Wood, FVector(0, 0, 42),  FVector(46, 46, 7));           // сиденье
        Part(TEXT("Cube"), Wood, FVector(-20, 0, 49), FVector(7, 46, 48));          // спинка
        for (int32 X = -1; X <= 1; X += 2)
        {
            for (int32 Y = -1; Y <= 1; Y += 2)
            {
                Part(TEXT("Cube"), Wood, FVector(X * 19, Y * 19, 0), FVector(6, 6, 42));
            }
        }
        break;

    case EFurnitureType::Table:
        Part(TEXT("Cube"), Wood, FVector(0, 0, 72), FVector(150, 90, 7));
        for (int32 X = -1; X <= 1; X += 2)
        {
            for (int32 Y = -1; Y <= 1; Y += 2)
            {
                Part(TEXT("Cube"), Wood, FVector(X * 68, Y * 38, 0), FVector(8, 8, 72));
            }
        }
        break;

    case EFurnitureType::Desk:
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 72), FVector(130, 65, 6));
        Part(TEXT("Cube"), Walnut, FVector(-45, 0, 0), FVector(38, 63, 72));        // тумба с ящиками
        Part(TEXT("Cube"), Walnut, FVector(62, 0, 0),  FVector(6, 63, 72));
        break;

    case EFurnitureType::Stove:
        Part(TEXT("Cube"),     Steel,  FVector(0, 0, 0),  FVector(62, 62, 85));
        Part(TEXT("Cube"),     Burn,   FVector(0, 0, 85), FVector(64, 64, 5));      // варочная поверхность
        Part(TEXT("Cube"),     Glass,  FVector(31, 0, 18), FVector(3, 48, 44));     // дверца духовки
        for (int32 X = -1; X <= 1; X += 2)
        {
            for (int32 Y = -1; Y <= 1; Y += 2)
            {
                Part(TEXT("Cylinder"), Burn, FVector(X * 15, Y * 15, 90), FVector(20, 20, 2));
            }
        }
        break;

    case EFurnitureType::Counter:
        Part(TEXT("Cube"), Wood,  FVector(0, 0, 0),  FVector(160, 60, 86));
        Part(TEXT("Cube"), Tile,  FVector(0, 0, 86), FVector(164, 64, 6));
        break;

    case EFurnitureType::Fridge:
        Part(TEXT("Cube"),     Chrome, FVector(0, 0, 0),    FVector(70, 68, 185));
        Part(TEXT("Cube"),     Steel,  FVector(35, 0, 60),  FVector(3, 64, 120));   // дверь
        Part(TEXT("Cylinder"), Chrome, FVector(37, 26, 110), FVector(4, 4, 40));    // ручка
        break;

    case EFurnitureType::Sink:
        Part(TEXT("Cube"),     Wood,   FVector(0, 0, 0),   FVector(70, 55, 82));    // тумба
        Part(TEXT("Cube"),     Tile,   FVector(0, 0, 82),  FVector(74, 58, 6));
        Part(TEXT("Cube"),     Chrome, FVector(0, 0, 84),  FVector(48, 38, 14));    // чаша
        Part(TEXT("Cylinder"), Chrome, FVector(-22, 0, 88), FVector(5, 5, 30));     // кран
        Part(TEXT("Cube"),     Chrome, FVector(-14, 0, 114), FVector(20, 5, 4));
        break;

    case EFurnitureType::Bath:
        // Ванна: чаша с бортиком, кран и душевая стойка.
        Part(TEXT("Cube"),     Tile,   FVector(0, 0, 0),   FVector(175, 80, 18));
        for (int32 S = -1; S <= 1; S += 2)
        {
            Part(TEXT("Cube"), Chrome, FVector(0, S * 37, 18), FVector(175, 7, 42));
            Part(TEXT("Cube"), Chrome, FVector(S * 84, 0, 18), FVector(7, 80, 42));
        }
        Part(TEXT("Cylinder"), Chrome, FVector(-80, 0, 60), FVector(5, 5, 34));
        Part(TEXT("Cube"),     Chrome, FVector(-72, 0, 92), FVector(22, 5, 4));
        break;

    case EFurnitureType::Shower:
        Part(TEXT("Cube"),     Tile,   FVector(0, 0, 0),   FVector(95, 95, 12));    // поддон
        Part(TEXT("Cube"),     Glass,  FVector(-45, 0, 12), FVector(5, 95, 195));
        Part(TEXT("Cube"),     Glass,  FVector(0, -45, 12), FVector(95, 5, 195));
        Part(TEXT("Cylinder"), Chrome, FVector(-38, 0, 190), FVector(16, 16, 6));   // лейка
        break;

    case EFurnitureType::Toilet:
        Part(TEXT("Cube"),     Chrome, FVector(0, 0, 0),   FVector(40, 55, 38));
        Part(TEXT("Cylinder"), Chrome, FVector(4, 0, 38),  FVector(38, 38, 8));     // сиденье
        Part(TEXT("Cube"),     Chrome, FVector(-22, 0, 38), FVector(18, 48, 50));   // бачок
        break;

    case EFurnitureType::Washer:
        Part(TEXT("Cube"),     Chrome, FVector(0, 0, 0),   FVector(60, 60, 85));
        Part(TEXT("Cylinder"), Glass,  FVector(31, 0, 45), FVector(38, 38, 4), 90.0f);
        break;

    case EFurnitureType::Sofa:
        Part(TEXT("Cube"), Brick,  FVector(0, 0, 18),   FVector(200, 90, 26));      // сиденье
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 0),    FVector(200, 90, 18));      // основание
        Part(TEXT("Cube"), Brick,  FVector(0, -38, 18), FVector(200, 18, 62));      // спинка
        for (int32 S = -1; S <= 1; S += 2)
        {
            Part(TEXT("Cube"), Brick, FVector(S * 94, 6, 18), FVector(16, 78, 44));
        }
        break;

    case EFurnitureType::TV:
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 0),   FVector(130, 42, 48));       // тумба
        Part(TEXT("Cube"), Steel,  FVector(0, 0, 48),  FVector(8, 20, 12));         // подставка
        Part(TEXT("Cube"), Glass,  FVector(0, 0, 60),  FVector(6, 118, 68));        // экран
        break;

    case EFurnitureType::Computer:
        Part(TEXT("Cube"), Steel, FVector(0, 0, 0),   FVector(16, 22, 14));
        Part(TEXT("Cube"), Glass, FVector(0, 0, 14),  FVector(4, 52, 32));
        break;

    case EFurnitureType::Wardrobe:
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 0),  FVector(60, 130, 210));
        for (int32 S = -1; S <= 1; S += 2)
        {
            Part(TEXT("Cube"),     Wood,   FVector(30, S * 32, 4), FVector(3, 62, 200));
            Part(TEXT("Cylinder"), Chrome, FVector(33, S * 8, 100), FVector(4, 4, 22));
        }
        break;

    case EFurnitureType::Nightstand:
        Part(TEXT("Cube"),     Walnut, FVector(0, 0, 0),  FVector(45, 40, 52));
        Part(TEXT("Cylinder"), Chrome, FVector(23, 0, 26), FVector(5, 5, 3), 90.0f);
        break;

    case EFurnitureType::Bookshelf:
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 0), FVector(32, 110, 190));
        for (int32 Level = 0; Level < 4; ++Level)
        {
            Part(TEXT("Cube"), Pine, FVector(6, 0, 34.0f + Level * 42.0f), FVector(22, 104, 26));
        }
        break;

    case EFurnitureType::Piano:
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 0),   FVector(60, 145, 110));
        Part(TEXT("Cube"), Tile,   FVector(32, 0, 70), FVector(26, 130, 8));        // клавиши
        break;

    case EFurnitureType::Mirror:
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 0),  FVector(7, 70, 130));
        Part(TEXT("Cube"), Glass,  FVector(4, 0, 6),  FVector(3, 60, 118));
        break;

    case EFurnitureType::Rug:
        Part(TEXT("Cube"), Brick, FVector(0, 0, 0), FVector(220, 160, 3));
        break;

    // --- Хозяйство ---
    case EFurnitureType::GardenBed:
    {
        Part(TEXT("Cube"), Pine, FVector(0, 0, 0), FVector(240, 130, 22));   // короб
        Part(TEXT("Cube"), TEXT("M_Ground_Gravel"), FVector(0, 0, 18), FVector(228, 118, 10));
        // Всходы: чем спелее, тем выше. Издали видно, поспело ли.
        const float Grown = FMath::Clamp(Ripeness, 0.08f, 1.0f);
        for (int32 Row = 0; Row < 3; ++Row)
        {
            for (int32 Col = 0; Col < 2; ++Col)
            {
                Part(TEXT("Sphere"), Grass,
                     FVector(-80.0f + Row * 80.0f, -32.0f + Col * 64.0f, 28.0f),
                     FVector(34.0f * Grown, 34.0f * Grown, 40.0f * Grown));
            }
        }
        break;
    }

    case EFurnitureType::Barrel:
        Part(TEXT("Cylinder"), Pine,  FVector(0, 0, 0),  FVector(78, 78, 96));
        Part(TEXT("Cylinder"), Steel, FVector(0, 0, 30), FVector(82, 82, 7));
        Part(TEXT("Cylinder"), Glass, FVector(0, 0, 92), FVector(68, 68, 4));   // вода
        break;

    case EFurnitureType::MarketStall:
        Part(TEXT("Cube"), Pine,  FVector(0, 0, 0),   FVector(180, 80, 92));    // прилавок
        Part(TEXT("Cube"), Brick, FVector(0, 0, 92),  FVector(190, 90, 8));
        for (int32 S = -1; S <= 1; S += 2)
        {
            Part(TEXT("Cube"), Pine, FVector(S * 84, 0, 100), FVector(8, 8, 110));
        }
        Part(TEXT("Cube"), Brick, FVector(0, 0, 210), FVector(200, 110, 12));   // навес
        Part(TEXT("Sphere"), Grass, FVector(-40, 0, 100), FVector(42, 42, 34)); // товар
        Part(TEXT("Sphere"), Moss,  FVector(40, 0, 100), FVector(38, 38, 30));
        break;

    // --- Школа ---
    case EFurnitureType::SchoolDesk:
        Part(TEXT("Cube"), Pine, FVector(0, 0, 66), FVector(120, 50, 5));         // столешница
        Part(TEXT("Cube"), Pine, FVector(0, 0, 0),  FVector(8, 46, 66), 0.0f);
        Part(TEXT("Cube"), Pine, FVector(38, 0, 38), FVector(44, 46, 5));         // скамья
        for (int32 S = -1; S <= 1; S += 2)
        {
            Part(TEXT("Cube"), Pine, FVector(S * 55, 0, 0), FVector(6, 46, 66));
        }
        break;

    case EFurnitureType::Blackboard:
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 90),  FVector(10, 260, 130));    // рама
        Part(TEXT("Cube"), Steel,  FVector(5, 0, 96),  FVector(4, 246, 116));     // доска
        Part(TEXT("Cube"), Walnut, FVector(8, 0, 86),  FVector(14, 246, 6));      // полочка для мела
        break;

    case EFurnitureType::Textbook:
        // Стопка книг на подставке: её видно от двери, и понятно, что это.
        Part(TEXT("Cube"), Pine,   FVector(0, 0, 0),  FVector(44, 32, 60));
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 60), FVector(34, 26, 6));
        Part(TEXT("Cube"), Brick,  FVector(0, 0, 66), FVector(32, 24, 5));
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 71), FVector(34, 26, 6));
        break;

    case EFurnitureType::Globe:
        Part(TEXT("Cylinder"), Walnut, FVector(0, 0, 0),  FVector(30, 30, 6));
        Part(TEXT("Cylinder"), Walnut, FVector(0, 0, 6),  FVector(5, 5, 52));
        Part(TEXT("Sphere"),   Glass,  FVector(0, 0, 58), FVector(46, 46, 46));
        break;

    case EFurnitureType::Abacus:
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 0),  FVector(12, 90, 70));
        for (int32 Row = 0; Row < 5; ++Row)
        {
            Part(TEXT("Cylinder"), Chrome, FVector(6, 0, 14.0f + Row * 12.0f), FVector(4, 80, 4), 90.0f);
        }
        break;

    case EFurnitureType::Tree:
        Part(TEXT("Cylinder"), Walnut, FVector(0, 0, 0),   FVector(46, 46, 420));
        Part(TEXT("Sphere"),   Moss,   FVector(0, 0, 380), FVector(300, 300, 260));
        Part(TEXT("Sphere"),   Grass,  FVector(40, 30, 300), FVector(200, 200, 170));
        break;

    case EFurnitureType::Workbench:
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 84), FVector(180, 80, 12));
        for (int32 S = -1; S <= 1; S += 2)
        {
            Part(TEXT("Cube"), Pine, FVector(S * 78, 32, 0),  FVector(12, 12, 84));
            Part(TEXT("Cube"), Pine, FVector(S * 78, -32, 0), FVector(12, 12, 84));
        }
        Part(TEXT("Cube"), Steel, FVector(-60, 0, 96), FVector(30, 24, 14));
        break;

    case EFurnitureType::Sawhorse:
        for (int32 S = -1; S <= 1; S += 2)
        {
            Part(TEXT("Cube"), Pine, FVector(S * 50, 26, 0),  FVector(10, 10, 88), 12.0f);
            Part(TEXT("Cube"), Pine, FVector(S * 50, -26, 0), FVector(10, 10, 88), -12.0f);
        }
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 88), FVector(150, 16, 14));
        break;

    case EFurnitureType::Forge:
        Part(TEXT("Cube"),     Brick, FVector(0, 0, 0),    FVector(150, 110, 90));
        Part(TEXT("Cube"),     Steel, FVector(0, 0, 90),   FVector(120, 90, 12));
        Part(TEXT("Cylinder"), Brick, FVector(-50, 0, 100), FVector(46, 46, 220));
        Part(TEXT("Cube"),     Walnut, FVector(70, 0, 90),  FVector(50, 50, 70));
        break;

    case EFurnitureType::Kiln:
        Part(TEXT("Cylinder"), Brick, FVector(0, 0, 0),   FVector(170, 170, 150));
        Part(TEXT("Sphere"),   Brick, FVector(0, 0, 140), FVector(170, 170, 110));
        Part(TEXT("Cube"),     Steel, FVector(85, 0, 30), FVector(16, 70, 70));
        break;

    case EFurnitureType::Loom:
        Part(TEXT("Cube"), Pine,   FVector(0, 60, 0),  FVector(12, 12, 190));
        Part(TEXT("Cube"), Pine,   FVector(0, -60, 0), FVector(12, 12, 190));
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 180), FVector(14, 140, 12));
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 60),  FVector(14, 140, 12));
        Part(TEXT("Cube"), Tile,   FVector(4, 0, 120), FVector(3, 130, 110));
        break;

    case EFurnitureType::PotteryWheel:
        Part(TEXT("Cylinder"), Walnut, FVector(0, 0, 0),  FVector(44, 44, 62));
        Part(TEXT("Cylinder"), Steel,  FVector(0, 0, 62), FVector(80, 80, 8));
        Part(TEXT("Cylinder"), Brick,  FVector(0, 0, 70), FVector(30, 30, 24));
        break;

    case EFurnitureType::Beehive:
        Part(TEXT("Cube"), Pine,   FVector(0, 0, 0),   FVector(70, 70, 40));
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 40),  FVector(66, 66, 40));
        Part(TEXT("Cube"), Pine,   FVector(0, 0, 80),  FVector(66, 66, 40));
        Part(TEXT("Cube"), Brick,  FVector(0, 0, 120), FVector(80, 80, 10));
        break;

    case EFurnitureType::ClayPit:
        Part(TEXT("Cylinder"), Tile, FVector(0, 0, 0),  FVector(220, 220, 14));
        Part(TEXT("Cylinder"), Moss,  FVector(0, 0, 12), FVector(150, 150, 10));
        Part(TEXT("Cube"),     Pine,  FVector(110, 0, 14), FVector(20, 90, 24));
        break;

    case EFurnitureType::StonePile:
        Part(TEXT("Cube"),   Brick, FVector(0, 0, 0),     FVector(180, 160, 90));
        Part(TEXT("Sphere"), Brick, FVector(60, 40, 80),  FVector(90, 80, 70));
        Part(TEXT("Sphere"), Tile, FVector(-50, -30, 70), FVector(110, 90, 60));
        break;

    case EFurnitureType::FishingSpot:
        Part(TEXT("Cube"),     Tile, FVector(0, 0, 0),   FVector(200, 200, 10));
        Part(TEXT("Cylinder"), Pine,  FVector(60, 0, 10), FVector(10, 10, 150), 14.0f);
        Part(TEXT("Cube"),     Glass, FVector(-140, 0, 0), FVector(220, 300, 6));
        break;

    case EFurnitureType::Well:
        Part(TEXT("Cylinder"), Brick,  FVector(0, 0, 0),   FVector(150, 150, 80));
        Part(TEXT("Cylinder"), Glass,  FVector(0, 0, 70),  FVector(120, 120, 6));
        for (int32 S = -1; S <= 1; S += 2)
        {
            Part(TEXT("Cube"), Pine, FVector(0, S * 66, 80), FVector(10, 10, 150));
        }
        Part(TEXT("Cube"), Walnut, FVector(0, 0, 230), FVector(170, 170, 12));
        break;

    case EFurnitureType::Shed:
        Part(TEXT("Cube"), Pine,   FVector(0, 0, 0),    FVector(300, 260, 8));
        Part(TEXT("Cube"), Walnut, FVector(-150, 0, 0), FVector(12, 260, 230));
        Part(TEXT("Cube"), Walnut, FVector(0, 130, 0),  FVector(300, 12, 230));
        Part(TEXT("Cube"), Walnut, FVector(0, -130, 0), FVector(300, 12, 230));
        Part(TEXT("Cube"), Brick,  FVector(0, 0, 230),  FVector(330, 290, 14));
        break;

    case EFurnitureType::Bench:
        Part(TEXT("Cube"), Pine, FVector(0, 0, 44), FVector(160, 46, 8));
        for (int32 S = -1; S <= 1; S += 2)
        {
            Part(TEXT("Cube"), Walnut, FVector(S * 70, 0, 0), FVector(10, 42, 44));
        }
        break;

    case EFurnitureType::Cart:
        Part(TEXT("Cube"),     Pine,  FVector(0, 0, 46), FVector(200, 110, 40));
        Part(TEXT("Cylinder"), Walnut, FVector(-70, 60, 0),  FVector(90, 14, 90), 90.0f);
        Part(TEXT("Cylinder"), Walnut, FVector(-70, -60, 0), FVector(90, 14, 90), 90.0f);
        Part(TEXT("Cylinder"), Walnut, FVector(70, 60, 0),   FVector(90, 14, 90), 90.0f);
        Part(TEXT("Cylinder"), Walnut, FVector(70, -60, 0),  FVector(90, 14, 90), 90.0f);
        break;

    case EFurnitureType::Spring:
        Part(TEXT("Cylinder"), Brick, FVector(0, 0, 0),   FVector(190, 190, 26));
        Part(TEXT("Cylinder"), Glass, FVector(0, 0, 22),  FVector(150, 150, 14));
        Part(TEXT("Sphere"),   Brick, FVector(80, 40, 20), FVector(50, 44, 40));
        break;

    case EFurnitureType::Bush:
        Part(TEXT("Sphere"), Moss,  FVector(0, 0, 0),    FVector(150, 140, 120));
        Part(TEXT("Sphere"), Grass, FVector(30, 20, 40), FVector(90, 90, 80));
        break;

    case EFurnitureType::Mushrooms:
        for (int32 Cap = 0; Cap < 5; ++Cap)
        {
            const float Angle = Cap * 72.0f;
            const FVector At(FMath::Cos(FMath::DegreesToRadians(Angle)) * 40.0f,
                             FMath::Sin(FMath::DegreesToRadians(Angle)) * 40.0f, 0.0f);
            Part(TEXT("Cylinder"), Tile, At, FVector(8, 8, 14));
            Part(TEXT("Sphere"),   Walnut, At + FVector(0, 0, 14), FVector(26, 26, 16));
        }
        break;

    case EFurnitureType::WildField:
        Part(TEXT("Cube"), Grass, FVector(0, 0, 0), FVector(320, 320, 8));
        for (int32 Tuft = 0; Tuft < 9; ++Tuft)
        {
            const FVector At(FMath::FRandRange(-140.0f, 140.0f), FMath::FRandRange(-140.0f, 140.0f), 6.0f);
            Part(TEXT("Cylinder"), Moss, At, FVector(10, 10, 70));
        }
        break;

    case EFurnitureType::Fence:
        for (int32 Post = -2; Post <= 2; ++Post)
        {
            Part(TEXT("Cube"), Pine, FVector(0, Post * 70, 0), FVector(12, 12, 130));
        }
        Part(TEXT("Cube"), Pine, FVector(0, 0, 60),  FVector(8, 300, 12));
        Part(TEXT("Cube"), Pine, FVector(0, 0, 110), FVector(8, 300, 12));
        break;

    case EFurnitureType::Lamp:
        Part(TEXT("Cylinder"), Steel, FVector(0, 0, 0),   FVector(28, 28, 4));
        Part(TEXT("Cylinder"), Steel, FVector(0, 0, 4),   FVector(6, 6, 130));
        Part(TEXT("Cylinder"), Glass, FVector(0, 0, 134), FVector(46, 46, 34));
        break;

    case EFurnitureType::Plant:
        Part(TEXT("Cylinder"), Tile,   FVector(0, 0, 0),  FVector(38, 38, 34));     // горшок
        Part(TEXT("Cylinder"), Moss,   FVector(0, 0, 34), FVector(7, 7, 60));       // ствол
        Part(TEXT("Sphere"),   Grass,  FVector(0, 0, 82), FVector(78, 78, 70));     // крона
        break;

    default:
        Part(TEXT("Cube"), Wood, FVector(0, 0, 0), FVector(60, 60, 70));
        break;
    }
}

FVector AFurnitureActor::GetFootprint() const
{
    switch (FurnitureType)
    {
    case EFurnitureType::Bed:       return FVector(200, 150, 95);
    case EFurnitureType::Sofa:      return FVector(200, 90, 80);
    case EFurnitureType::Table:     return FVector(150, 90, 79);
    case EFurnitureType::Bath:      return FVector(175, 80, 60);
    case EFurnitureType::Wardrobe:  return FVector(60, 130, 210);
    case EFurnitureType::Bookshelf: return FVector(32, 110, 190);
    case EFurnitureType::Fridge:    return FVector(70, 68, 185);
    case EFurnitureType::Counter:   return FVector(160, 60, 92);
    case EFurnitureType::Rug:       return FVector(220, 160, 3);
    default:                        return FVector(70, 70, 90);
    }
}

// ---------------------------------------------------------------------------
//  Что такое кровать, стул, плита — с точки зрения вещи
// ---------------------------------------------------------------------------

void AFurnitureActor::BuildOffers()
{
    if (!Affordances)
    {
        return;
    }

    if (Name.IsEmpty())
    {
        switch (FurnitureType)
        {
        case EFurnitureType::Bed:        Name = TEXT("кровать"); break;
        case EFurnitureType::Chair:      Name = TEXT("стул"); break;
        case EFurnitureType::Table:      Name = TEXT("стол"); break;
        case EFurnitureType::Stove:      Name = TEXT("плита"); break;
        case EFurnitureType::TV:         Name = TEXT("телевизор"); break;
        case EFurnitureType::Sofa:       Name = TEXT("диван"); break;
        case EFurnitureType::Wardrobe:   Name = TEXT("шкаф"); break;
        case EFurnitureType::Fridge:     Name = TEXT("холодильник"); break;
        case EFurnitureType::Toilet:     Name = TEXT("уборная"); break;
        case EFurnitureType::Shower:     Name = TEXT("душ"); break;
        case EFurnitureType::Sink:       Name = TEXT("раковина"); break;
        case EFurnitureType::Desk:       Name = TEXT("письменный стол"); break;
        case EFurnitureType::Computer:   Name = TEXT("компьютер"); break;
        case EFurnitureType::Bookshelf:  Name = TEXT("книжная полка"); break;
        case EFurnitureType::Lamp:       Name = TEXT("лампа"); break;
        case EFurnitureType::Plant:      Name = TEXT("растение"); break;
        case EFurnitureType::Bath:       Name = TEXT("ванна"); break;
        case EFurnitureType::Mirror:     Name = TEXT("зеркало"); break;
        case EFurnitureType::Nightstand: Name = TEXT("тумбочка"); break;
        case EFurnitureType::Rug:        Name = TEXT("ковёр"); break;
        case EFurnitureType::Counter:    Name = TEXT("кухонная тумба"); break;
        case EFurnitureType::Washer:     Name = TEXT("стиральная машина"); break;
        case EFurnitureType::Crib:       Name = TEXT("детская кроватка"); break;
        case EFurnitureType::Piano:      Name = TEXT("пианино"); break;
        case EFurnitureType::Blackboard: Name = TEXT("доска"); break;
        case EFurnitureType::SchoolDesk: Name = TEXT("парта"); break;
        case EFurnitureType::Globe:      Name = TEXT("глобус"); break;
        case EFurnitureType::Abacus:     Name = TEXT("счёты"); break;
        case EFurnitureType::Textbook:
        {
            const FTextbook* Book = FLibrary::Find(Subject);
            Name = Book ? Book->Title : TEXT("учебник");
            break;
        }
        case EFurnitureType::GardenBed:  Name = TEXT("грядка"); break;
        case EFurnitureType::Barrel:     Name = TEXT("бочка"); break;
        case EFurnitureType::MarketStall: Name = TEXT("лоток"); break;
        case EFurnitureType::Tree:       Name = TEXT("дерево"); break;
        case EFurnitureType::Workbench:  Name = TEXT("верстак"); break;
        case EFurnitureType::Sawhorse:   Name = TEXT("козлы"); break;
        case EFurnitureType::Forge:      Name = TEXT("горн"); break;
        case EFurnitureType::Kiln:       Name = TEXT("печь"); break;
        case EFurnitureType::Loom:       Name = TEXT("ткацкий станок"); break;
        case EFurnitureType::PotteryWheel: Name = TEXT("гончарный круг"); break;
        case EFurnitureType::Beehive:    Name = TEXT("улей"); break;
        case EFurnitureType::ClayPit:    Name = TEXT("яма"); break;
        case EFurnitureType::StonePile:  Name = TEXT("каменоломня"); break;
        case EFurnitureType::FishingSpot: Name = TEXT("берег"); break;
        case EFurnitureType::Well:       Name = TEXT("колодец"); break;
        case EFurnitureType::Shed:       Name = TEXT("сарай"); break;
        case EFurnitureType::Bench:      Name = TEXT("лавка"); break;
        case EFurnitureType::Cart:       Name = TEXT("телега"); break;
        case EFurnitureType::Fence:      Name = TEXT("забор"); break;
        case EFurnitureType::Spring:     Name = TEXT("родник"); break;
        case EFurnitureType::Bush:       Name = TEXT("куст"); break;
        case EFurnitureType::Mushrooms:  Name = TEXT("грибное место"); break;
        case EFurnitureType::WildField:  Name = TEXT("поле"); break;
        default:                         Name = TEXT("вещь"); break;
        }
    }

    if (!Affordances->DisplayName.Contains(TEXT("№")))
    {
        Affordances->DisplayName = Name;
    }
    // Вид вещи. Кровать в этом доме и кровать в соседнем — разные вещи
    // одного вида, и опыт между ними переносится.
    Affordances->Category = Name;
    Affordances->NoticeRadius = 900.0f;
    Affordances->Capacity = 1;   // на стуле сидит один
    Affordances->Offers.Reset();

    if (BuildVillageOffers())
    {
        RefreshCraftOffers();
        return;
    }

    switch (FurnitureType)
    {
    case EFurnitureType::Bed:
        Affordances->AddOffer(EActionType::Sleep, TEXT("лечь в кровать"), 0.0f);
        Affordances->AddPromise(ENeedType::Sleep, 1.0f);
        Affordances->AddPromise(ENeedType::Comfort, 0.5f);
        Affordances->Offers.Last().EffortCost = 0.0f;
        Affordances->Offers.Last().bNeedsLying = true;
        Affordances->Offers.Last().SetupSeconds = 6.0f;
        break;

    case EFurnitureType::Crib:
        Affordances->AddOffer(EActionType::Sleep, TEXT("уложить спать"), 0.0f);
        Affordances->AddPromise(ENeedType::Sleep, 1.0f);
        Affordances->AddPromise(ENeedType::Safety, 0.3f);
        Affordances->Offers.Last().EffortCost = 0.0f;
        break;

    case EFurnitureType::Sofa:
        Affordances->AddOffer(EActionType::Rest, TEXT("развалиться на диване"), 1800.0f);
        Affordances->AddPromise(ENeedType::Comfort, 0.6f);
        Affordances->Offers.Last().EffortCost = 0.0f;
        Affordances->Offers.Last().bNeedsSeat = true;
        Affordances->Offers.Last().SetupSeconds = 4.0f;
        Affordances->Capacity = 3;
        break;

    case EFurnitureType::Chair:
        Affordances->AddOffer(EActionType::Rest, TEXT("присесть"), 900.0f);
        Affordances->AddPromise(ENeedType::Comfort, 0.35f);
        Affordances->Offers.Last().EffortCost = 0.0f;
        Affordances->Offers.Last().bNeedsSeat = true;
        Affordances->Offers.Last().SetupSeconds = 3.0f;
        break;

    case EFurnitureType::Table:
        // За столом едят. Без этого еда дома была только «у плиты».
        Affordances->AddOffer(EActionType::Eat, TEXT("поесть за столом"), 900.0f);
        Affordances->AddPromise(ENeedType::Hunger, 0.75f);
        Affordances->AddPromise(ENeedType::Comfort, 0.15f);
        Affordances->Offers.Last().EffortCost = 0.02f;
        Affordances->Offers.Last().bNeedsSeat = true;
        Affordances->Offers.Last().SetupSeconds = 5.0f;
        Affordances->Capacity = 4;
        break;

    case EFurnitureType::Stove:
        // Готовить не из чего — не приготовишь. Продукты нужно принести.
        Affordances->AddOffer(EActionType::Cook, TEXT("приготовить"), 1200.0f);
        Affordances->AddPromise(ENeedType::Competence, 0.15f);
        Affordances->AddPromise(ENeedType::Comfort, -0.05f);
        Affordances->Offers.Last().RequiredSkill = TEXT("Cooking");
        Affordances->Offers.Last().Difficulty = 0.35f;
        Affordances->Offers.Last().EffortCost = 0.2f;
        Affordances->Offers.Last().Requires = EResourceKind::RawFood;
        Affordances->Offers.Last().RequiresAmount = 1.0f;
        Affordances->Offers.Last().Produces = EResourceKind::CookedFood;
        Affordances->Offers.Last().ProducesAmount = 2.0f;
        Affordances->Offers.Last().SetupSeconds = 6.0f;
        break;

    case EFurnitureType::GardenBed:
        // Урожай снимают, когда он есть. Пока зелено — и предложения нет.
        if (Ripeness > 0.55f)
        {
            Affordances->AddOffer(EActionType::Work,
                FString::Printf(TEXT("собрать урожай (%d%%)"), FMath::RoundToInt(Ripeness * 100.0f)),
                900.0f);
            Affordances->AddPromise(ENeedType::Achievement, 0.3f);
            Affordances->AddPromise(ENeedType::Competence, 0.15f);
            Affordances->AddPromise(ENeedType::Comfort, -0.12f);
            Affordances->Offers.Last().EffortCost = 0.22f;
            Affordances->Offers.Last().Produces = EResourceKind::RawFood;
            Affordances->Offers.Last().ProducesAmount = 2.0f + Ripeness * 3.0f;
            Affordances->Offers.Last().SetupSeconds = 5.0f;
        }
        else
        {
            Affordances->AddOffer(EActionType::Work, TEXT("полить грядку"), 600.0f);
            Affordances->AddPromise(ENeedType::Order, 0.2f);
            Affordances->AddPromise(ENeedType::Achievement, 0.12f);
            Affordances->Offers.Last().EffortCost = 0.14f;
            Affordances->Offers.Last().SetupSeconds = 4.0f;
        }
        break;

    case EFurnitureType::Barrel:
        Affordances->AddOffer(EActionType::Drink, TEXT("зачерпнуть воды"), 120.0f);
        Affordances->AddPromise(ENeedType::Thirst, 0.85f);
        Affordances->Offers.Last().EffortCost = 0.0f;
        Affordances->Capacity = 2;

        Affordances->AddOffer(EActionType::Observe, TEXT("набрать воды с собой"), 200.0f);
        Affordances->AddPromise(ENeedType::Safety, 0.1f);
        Affordances->Offers.Last().Produces = EResourceKind::Water;
        Affordances->Offers.Last().ProducesAmount = 2.0f;
        Affordances->Offers.Last().EffortCost = 0.05f;
        break;

    case EFurnitureType::MarketStall:
    {
        const float Goods = HowMuchInside(EResourceKind::RawFood);
        if (Goods >= 1.0f)
        {
            Affordances->AddOffer(EActionType::Work, TEXT("купить продуктов"), 400.0f);
            Affordances->AddPromise(ENeedType::Safety, 0.15f);
            Affordances->AddPromise(ENeedType::Order, 0.12f);
            Affordances->Offers.Last().MoneyCost = 6.0f;
            Affordances->Offers.Last().EffortCost = 0.06f;
            Affordances->Offers.Last().Produces = EResourceKind::RawFood;
            Affordances->Offers.Last().ProducesAmount = 3.0f;
            Affordances->Offers.Last().SetupSeconds = 6.0f;
            Affordances->Offers.Last().AvailableFromHour = 7.0f;
            Affordances->Offers.Last().AvailableToHour = 20.0f;

            // Готовое дороже сырого — зато есть можно сразу. Кто торопится
            // или не умеет готовить, берёт его и не считает это расточительством.
            Affordances->AddOffer(EActionType::Eat, TEXT("взять горячего с лотка"), 600.0f);
            Affordances->AddPromise(ENeedType::Hunger, 0.7f);
            Affordances->AddPromise(ENeedType::Comfort, 0.1f);
            Affordances->Offers.Last().MoneyCost = 11.0f;
            Affordances->Offers.Last().EffortCost = 0.02f;
            Affordances->Offers.Last().SetupSeconds = 5.0f;
            Affordances->Offers.Last().AvailableFromHour = 7.0f;
            Affordances->Offers.Last().AvailableToHour = 20.0f;
        }
        Affordances->Capacity = 4;
        break;
    }

    case EFurnitureType::Counter:
        Affordances->AddOffer(EActionType::Cook, TEXT("нарезать поесть"), 600.0f);
        Affordances->AddPromise(ENeedType::Hunger, 0.45f);
        Affordances->Offers.Last().EffortCost = 0.08f;
        break;

    case EFurnitureType::Fridge:
    {
        // Холодильник не родник: в нём лежит ровно то, что туда положили.
        const float Ready = HowMuchInside(EResourceKind::CookedFood);
        const float Raw = HowMuchInside(EResourceKind::RawFood);

        if (Ready >= 0.5f)
        {
            Affordances->AddOffer(EActionType::Eat, TEXT("поесть готового"), 500.0f);
            Affordances->AddPromise(ENeedType::Hunger, FMath::Min(0.85f, 0.4f * Ready));
            Affordances->Offers.Last().EffortCost = 0.02f;
            Affordances->Offers.Last().Requires = EResourceKind::CookedFood;
            Affordances->Offers.Last().RequiresAmount = 1.0f;
            Affordances->Offers.Last().SetupSeconds = 4.0f;
        }
        if (Raw >= 1.0f)
        {
            Affordances->AddOffer(EActionType::Observe, TEXT("достать продукты"), 120.0f);
            Affordances->AddPromise(ENeedType::Order, 0.1f);
            Affordances->Offers.Last().EffortCost = 0.02f;
            Affordances->Offers.Last().Requires = EResourceKind::RawFood;
            Affordances->Offers.Last().RequiresAmount = 1.0f;
            Affordances->Offers.Last().Produces = EResourceKind::RawFood;
            Affordances->Offers.Last().ProducesAmount = 1.0f;
            Affordances->Offers.Last().SetupSeconds = 4.0f;
        }

        // Вода в доме есть всегда — но за ней надо к крану, а не в холодильник.
        break;
    }

    case EFurnitureType::Toilet:
        Affordances->AddOffer(EActionType::UseToilet, TEXT("в уборную"), 240.0f);
        Affordances->AddPromise(ENeedType::Bladder, 1.0f);
        Affordances->Offers.Last().EffortCost = 0.0f;
        Affordances->Offers.Last().bNeedsSeat = true;
        Affordances->Offers.Last().SetupSeconds = 3.0f;
        break;

    case EFurnitureType::Shower:
        Affordances->AddOffer(EActionType::Wash, TEXT("в душ"), 900.0f);
        Affordances->AddPromise(ENeedType::Hygiene, 1.0f);
        Affordances->AddPromise(ENeedType::Comfort, 0.25f);
        break;

    case EFurnitureType::Bath:
        // Мыться в ванне дольше, чем в душе, и приятнее. Человек выбирает
        // между ними сам: когда спешит — душ, когда хочется покоя — ванна.
        Affordances->AddOffer(EActionType::Wash, TEXT("полежать в ванне"), 2100.0f);
        Affordances->AddPromise(ENeedType::Hygiene, 1.0f);
        Affordances->AddPromise(ENeedType::Comfort, 0.7f);
        Affordances->AddPromise(ENeedType::Safety, 0.15f);
        Affordances->Offers.Last().EffortCost = 0.05f;
        Affordances->Offers.Last().bNeedsLying = true;
        Affordances->Offers.Last().SetupSeconds = 8.0f;
        break;

    case EFurnitureType::Sink:
        Affordances->AddOffer(EActionType::Drink, TEXT("попить воды"), 90.0f);
        Affordances->AddPromise(ENeedType::Thirst, 0.9f);
        Affordances->Offers.Last().EffortCost = 0.0f;

        Affordances->AddOffer(EActionType::Wash, TEXT("помыть руки"), 120.0f);
        Affordances->AddPromise(ENeedType::Hygiene, 0.35f);
        Affordances->Offers.Last().EffortCost = 0.0f;
        break;

    case EFurnitureType::Washer:
        Affordances->AddOffer(EActionType::Wash, TEXT("постирать"), 1500.0f);
        Affordances->AddPromise(ENeedType::Hygiene, 0.45f);
        Affordances->AddPromise(ENeedType::Order, 0.35f);
        Affordances->Offers.Last().EffortCost = 0.15f;
        break;

    case EFurnitureType::TV:
        Affordances->AddOffer(EActionType::Entertain, TEXT("смотреть телевизор"), 2400.0f);
        Affordances->AddPromise(ENeedType::Novelty, 0.35f);
        Affordances->AddPromise(ENeedType::Comfort, 0.2f);
        // Просидеть вечер перед телевизором легко — и в этом его опасность:
        // он почти ничего не требует и почти ничего не даёт.
        Affordances->Offers.Last().EffortCost = 0.0f;
        Affordances->Capacity = 4;
        break;

    case EFurnitureType::Computer:
        Affordances->AddOffer(EActionType::Study, TEXT("посидеть за компьютером"), 3600.0f);
        Affordances->AddPromise(ENeedType::Competence, 0.3f);
        Affordances->AddPromise(ENeedType::Novelty, 0.3f);
        Affordances->AddPromise(ENeedType::Achievement, 0.15f);
        Affordances->Offers.Last().EffortCost = 0.15f;
        Affordances->Offers.Last().bNeedsSeat = true;
        Affordances->Offers.Last().bNeedsLight = true;
        Affordances->Offers.Last().SetupSeconds = 4.0f;
        break;

    case EFurnitureType::Bookshelf:
        Affordances->AddOffer(EActionType::Read, TEXT("почитать"), 2400.0f);
        Affordances->AddPromise(ENeedType::Novelty, 0.3f);
        Affordances->AddPromise(ENeedType::Meaning, 0.25f);
        Affordances->AddPromise(ENeedType::Beauty, 0.15f);
        Affordances->Offers.Last().RequiredSkill = TEXT("Reading");
        Affordances->Offers.Last().Difficulty = 0.15f;
        Affordances->Offers.Last().EffortCost = 0.18f;
        Affordances->Offers.Last().bNeedsLight = true;
        Affordances->Offers.Last().SetupSeconds = 5.0f;
        break;

    case EFurnitureType::Piano:
        Affordances->AddOffer(EActionType::Practice, TEXT("поиграть на пианино"), 1800.0f);
        Affordances->AddPromise(ENeedType::Beauty, 0.5f);
        Affordances->AddPromise(ENeedType::Competence, 0.25f);
        Affordances->AddPromise(ENeedType::Meaning, 0.2f);
        Affordances->Offers.Last().RequiredSkill = TEXT("Music");
        Affordances->Offers.Last().Difficulty = 0.4f;
        Affordances->Offers.Last().EffortCost = 0.15f;
        break;

    case EFurnitureType::Desk:
        Affordances->AddOffer(EActionType::Practice, TEXT("поработать за столом"), 3600.0f);
        Affordances->AddPromise(ENeedType::Competence, 0.35f);
        Affordances->AddPromise(ENeedType::Achievement, 0.25f);
        Affordances->AddPromise(ENeedType::Comfort, -0.15f);
        Affordances->Offers.Last().EffortCost = 0.3f;
        Affordances->Offers.Last().bNeedsSeat = true;
        Affordances->Offers.Last().bNeedsLight = true;
        Affordances->Offers.Last().SetupSeconds = 5.0f;
        break;

    case EFurnitureType::Mirror:
        // Посмотреть на себя. Что человек при этом почувствует — зависит
        // от того, как он к себе относится, а не от зеркала.
        Affordances->AddOffer(EActionType::Observe, TEXT("посмотреть на себя"), 180.0f);
        Affordances->AddPromise(ENeedType::Esteem, 0.2f);
        Affordances->AddPromise(ENeedType::Order, 0.1f);
        Affordances->Offers.Last().EffortCost = 0.0f;
        break;

    case EFurnitureType::Wardrobe:
        Affordances->AddOffer(EActionType::Wash, TEXT("переодеться"), 300.0f);
        Affordances->AddPromise(ENeedType::Hygiene, 0.3f);
        Affordances->AddPromise(ENeedType::Esteem, 0.15f);
        Affordances->Offers.Last().EffortCost = 0.05f;
        break;

    case EFurnitureType::Plant:
        Affordances->AddOffer(EActionType::Observe, TEXT("посмотреть на растение"), 300.0f);
        Affordances->AddPromise(ENeedType::Beauty, 0.25f);
        Affordances->Offers.Last().EffortCost = 0.0f;
        break;

    case EFurnitureType::Textbook:
    {
        // Учебник — единственная вещь в городе, из которой можно узнать то,
        // чего сам не переживал. Но только если сумеешь прочесть.
        const FTextbook* Book = FLibrary::Find(Subject);
        const float Hard = Book ? Book->Difficulty : 0.4f;

        Affordances->AddOffer(EActionType::Study,
            FString::Printf(TEXT("учить: %s"), Book ? *Book->Title : TEXT("учебник")), 1800.0f);
        Affordances->AddPromise(ENeedType::Competence, 0.45f);
        Affordances->AddPromise(ENeedType::Novelty, 0.3f);
        Affordances->AddPromise(ENeedType::Meaning, 0.2f);
        Affordances->AddPromise(ENeedType::Comfort, -0.1f);
        Affordances->Offers.Last().RequiredSkill = TEXT("Reading");
        Affordances->Offers.Last().Difficulty = Book ? Book->RequiredReading : 0.25f;
        Affordances->Offers.Last().EffortCost = 0.12f + Hard * 0.2f;
        Affordances->Capacity = 2;
        break;
    }

    case EFurnitureType::Blackboard:
        // У доски объясняют другим. Кто объясняет, тот и сам понимает лучше.
        Affordances->AddOffer(EActionType::Practice, TEXT("разобрать у доски"), 1200.0f);
        Affordances->AddPromise(ENeedType::Competence, 0.3f);
        Affordances->AddPromise(ENeedType::Esteem, 0.2f);
        Affordances->Offers.Last().RequiredSkill = TEXT("Teaching");
        Affordances->Offers.Last().Difficulty = 0.2f;
        Affordances->Offers.Last().EffortCost = 0.2f;
        break;

    case EFurnitureType::Abacus:
        Affordances->AddOffer(EActionType::Practice, TEXT("посчитать на счётах"), 900.0f);
        Affordances->AddPromise(ENeedType::Competence, 0.3f);
        Affordances->AddPromise(ENeedType::Order, 0.2f);
        Affordances->Offers.Last().RequiredSkill = TEXT("Counting");
        Affordances->Offers.Last().Difficulty = 0.15f;
        Affordances->Offers.Last().EffortCost = 0.1f;
        break;

    case EFurnitureType::Globe:
        Affordances->AddOffer(EActionType::Observe, TEXT("разглядывать глобус"), 600.0f);
        Affordances->AddPromise(ENeedType::Novelty, 0.4f);
        Affordances->AddPromise(ENeedType::Meaning, 0.15f);
        Affordances->Offers.Last().EffortCost = 0.02f;
        break;

    case EFurnitureType::SchoolDesk:
        Affordances->AddOffer(EActionType::Rest, TEXT("сесть за парту"), 600.0f);
        Affordances->AddPromise(ENeedType::Comfort, 0.2f);
        Affordances->AddPromise(ENeedType::Belonging, 0.15f);
        Affordances->Offers.Last().EffortCost = 0.0f;
        Affordances->Offers.Last().bNeedsSeat = true;
        Affordances->Offers.Last().SetupSeconds = 4.0f;
        Affordances->Capacity = 2;
        break;

    default:
        break;
    }

    struct FPromiseSeed
    {
        ENeedType Need;
        float Amount;
    };

    UAffordanceComponent* A = Affordances;
    auto Offer = [A](EActionType Action, const TCHAR* Label, float Duration, float Effort,
                     std::initializer_list<FPromiseSeed> Promises) -> FAffordance&
    {
        A->AddOffer(Action, Label, Duration);
        for (const FPromiseSeed& P : Promises)
        {
            A->AddPromise(P.Need, P.Amount);
        }
        A->Offers.Last().EffortCost = Effort;
        return A->Offers.Last();
    };

    switch (FurnitureType)
    {
    case EFurnitureType::Table:
    {
        FAffordance& Draughts = Offer(EActionType::Entertain, TEXT("поиграть в шашки"), 1800.0f, 0.08f,
            { { ENeedType::SocialContact, 0.25f }, { ENeedType::Competence, 0.15f }, { ENeedType::Novelty, 0.2f } });
        Draughts.bNeedsSeat = true;
        Draughts.SetupSeconds = 4.0f;
        break;
    }

    case EFurnitureType::Sink:
        Offer(EActionType::Wash, TEXT("помыть посуду"), 600.0f, 0.12f,
            { { ENeedType::Order, 0.35f }, { ENeedType::Hygiene, 0.05f } });
        break;

    case EFurnitureType::Plant:
        Offer(EActionType::Observe, TEXT("полить цветы"), 300.0f, 0.03f,
            { { ENeedType::Order, 0.15f }, { ENeedType::Beauty, 0.2f }, { ENeedType::Meaning, 0.05f } });
        break;

    case EFurnitureType::Sofa:
    {
        FAffordance& Lie = Offer(EActionType::Rest, TEXT("прилечь на диван"), 1800.0f, 0.0f,
            { { ENeedType::Comfort, 0.55f }, { ENeedType::Sleep, 0.15f } });
        Lie.bNeedsLying = true;
        Lie.SetupSeconds = 4.0f;
        break;
    }

    case EFurnitureType::Rug:
        Offer(EActionType::Exercise, TEXT("сделать зарядку на ковре"), 900.0f, 0.3f,
            { { ENeedType::Health, 0.3f }, { ENeedType::Comfort, 0.1f } });
        break;

    case EFurnitureType::Desk:
    {
        FAffordance& Letter = Offer(EActionType::Practice, TEXT("написать письмо"), 1200.0f, 0.12f,
            { { ENeedType::Intimacy, 0.25f }, { ENeedType::Meaning, 0.2f } });
        Letter.RequiredSkill = TEXT("Writing");
        Letter.Difficulty = 0.3f;
        Letter.bNeedsSeat = true;
        Letter.bNeedsLight = true;
        Letter.SetupSeconds = 4.0f;
        break;
    }

    case EFurnitureType::Bookshelf:
        Offer(EActionType::Observe, TEXT("навести порядок на полке"), 600.0f, 0.1f,
            { { ENeedType::Order, 0.3f } });
        break;

    case EFurnitureType::Piano:
    {
        FAffordance& Learn = Offer(EActionType::Practice, TEXT("поучиться играть"), 1500.0f, 0.15f,
            { { ENeedType::Competence, 0.3f }, { ENeedType::Beauty, 0.25f }, { ENeedType::Novelty, 0.15f } });
        Learn.RequiredSkill = TEXT("Music");
        Learn.Difficulty = 0.05f;
        Learn.bNeedsSeat = true;
        Learn.SetupSeconds = 4.0f;
        break;
    }

    case EFurnitureType::Stove:
        Offer(EActionType::Cook, TEXT("вскипятить чайник"), 400.0f, 0.04f,
            { { ENeedType::Thirst, 0.45f }, { ENeedType::Comfort, 0.25f } });
        break;

    case EFurnitureType::Bed:
    {
        FAffordance& Lounge = Offer(EActionType::Rest, TEXT("поваляться в кровати"), 1500.0f, 0.0f,
            { { ENeedType::Comfort, 0.5f }, { ENeedType::Sleep, 0.1f } });
        Lounge.bNeedsLying = true;
        Lounge.SetupSeconds = 4.0f;
        break;
    }

    default:
        break;
    }
    RefreshCraftOffers();
}

// ---------------------------------------------------------------------------
//  Хозяйство
//
//  Запас — не бесконечный. Грядка зреет неделю, урожай снимают один раз,
//  продукты в холодильнике кончаются. Отсюда и берётся весь смысл работы:
//  деньги нужны, чтобы купить то, что кончилось.
// ---------------------------------------------------------------------------

void AFurnitureActor::Store(EResourceKind What, float HowMuch)
{
    if (What == EResourceKind::None || HowMuch <= 0.0f)
    {
        return;
    }
    float& Have = Stored.FindOrAdd(What);
    Have = FMath::Clamp(Have + HowMuch, 0.0f, 40.0f);
    BuildOffers();
}

float AFurnitureActor::TakeOut(EResourceKind What, float HowMuch)
{
    float* Have = Stored.Find(What);
    if (!Have || *Have <= 0.0f)
    {
        return 0.0f;
    }

    const float Taken = FMath::Min(*Have, FMath::Max(0.0f, HowMuch));
    *Have -= Taken;
    BuildOffers();
    return Taken;
}

float AFurnitureActor::HowMuchInside(EResourceKind What) const
{
    const float* Have = Stored.Find(What);
    return Have ? *Have : 0.0f;
}

void AFurnitureActor::AdvanceHousehold(float GameDelta)
{
    CraftCheck += GameDelta;
    if (CraftCheck > 600.0f)
    {
        CraftCheck = 0.0f;
        RefreshCraftOffers();
    }

    if (FurnitureType == EFurnitureType::GardenBed)
    {
        // Грядка зреет примерно за шесть игровых суток.
        const float Before = Ripeness;
        Ripeness = FMath::Clamp(Ripeness + GameDelta / (6.0f * 86400.0f), 0.0f, 1.0f);

        // Пересобираем вид только когда всходы заметно подросли.
        if (FMath::FloorToInt(Ripeness * 6.0f) != FMath::FloorToInt(Before * 6.0f))
        {
            for (UStaticMeshComponent* P : Parts)
            {
                if (P) { P->DestroyComponent(); }
            }
            Parts.Reset();
            Mesh = nullptr;
            BuildLook();
            BuildOffers();
        }
    }
    else if (FurnitureType == EFurnitureType::MarketStall && !FVillage::IsMedieval(this))
    {
        // Лавку подвозят: товар на прилавке не иссякает совсем, но и
        // не бесконечен — за день привозят понемногу.
        float& Goods = Stored.FindOrAdd(EResourceKind::RawFood);
        Goods = FMath::Min(30.0f, Goods + GameDelta / 900.0f);
    }
}

void AFurnitureActor::Interact(ACompleteHumanNPC* NPC)
{
    // Старый вход оставлен для совместимости. Вещь не приказывает —
    // она лишь обращает на себя внимание, а решение остаётся за человеком.
    if (!NPC || !NPC->Mind)
    {
        return;
    }
    NPC->Mind->Notice(this, TEXT("Touch"), GetActorLocation(), 0.6f);
}
