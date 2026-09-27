#include "ResourceActor.h"
#include "ImportedModels.h"
#include "Crafts.h"
#include "Matter.h"
#include "MatterSubsystem.h"
#include "Components/TextRenderComponent.h"
#include "AffordanceComponent.h"
#include "HumanWorldSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    bool ModelOf(EResourceKind Kind, FString& OutFolder, FString& OutFilter, FRotator& OutTurn)
    {
        OutTurn = FRotator::ZeroRotator;
        auto Use = [&OutFolder, &OutFilter](const TCHAR* Folder, const TCHAR* Filter)
        {
            OutFolder = Folder;
            OutFilter = Filter;
            return true;
        };
        switch (Kind)
        {
        case EResourceKind::RawFood:
        case EResourceKind::Berry:
        case EResourceKind::Egg:        return Use(TEXT("PolyHaven/food_apple_01"), TEXT(""));
        case EResourceKind::CookedFood:
        case EResourceKind::Salt:       return Use(TEXT("PolyHaven/wooden_bowl_01"), TEXT(""));
        case EResourceKind::Water:
        case EResourceKind::Milk:
        case EResourceKind::Oil:
        case EResourceKind::Medicine:
        case EResourceKind::Ink:        return Use(TEXT("PolyHaven/jug_01"), TEXT(""));
        case EResourceKind::Honey:
        case EResourceKind::Wax:
        case EResourceKind::Resin:
        case EResourceKind::Tar:
        case EResourceKind::Pot:        return Use(TEXT("PolyHaven/pot_enamel_01"), TEXT(""));
        case EResourceKind::Firewood:   return Use(TEXT("PolyHaven/wooden_barrels_01"), TEXT("wooden_barrels_01_piece0*"));
        case EResourceKind::Herb:       return Use(TEXT("Quaternius/Nature"), TEXT("Clover_1"));
        case EResourceKind::Flax:
        case EResourceKind::Wool:       return Use(TEXT("Quaternius/Nature"), TEXT("Clover_2"));
        case EResourceKind::Seed:
        case EResourceKind::Grain:
        case EResourceKind::Thread:
        case EResourceKind::Rope:       return Use(TEXT("PolyHaven/wicker_basket_01"), TEXT(""));
        case EResourceKind::Flour:      return Use(TEXT("PolyHaven/wooden_crate_01"), TEXT(""));
        case EResourceKind::Cloth:
        case EResourceKind::Leather:
        case EResourceKind::Hide:       return Use(TEXT("KenneyItems/rugSquare"), TEXT(""));
        case EResourceKind::Wood:
            OutTurn = FRotator(0.0f, 0.0f, 90.0f);
            return Use(TEXT("Quaternius/Village"), TEXT("Roof_Log"));
        case EResourceKind::Plank:
        case EResourceKind::Bark:       return Use(TEXT("Quaternius/Village"), TEXT("Floor_WoodDark"));
        case EResourceKind::Reed:
        case EResourceKind::Straw:      return Use(TEXT("Quaternius/Nature"), TEXT("Grass_Wispy_Tall"));
        case EResourceKind::Stone:
        case EResourceKind::Granite:    return Use(TEXT("Quaternius/Nature"), TEXT("Pebble_Round_2"));
        case EResourceKind::Limestone:
        case EResourceKind::Sandstone:  return Use(TEXT("Quaternius/Nature"), TEXT("Pebble_Square_3"));
        case EResourceKind::Chalk:
        case EResourceKind::Quartz:     return Use(TEXT("Quaternius/Nature"), TEXT("Pebble_Round_4"));
        case EResourceKind::Flint:
        case EResourceKind::Sulphur:
        case EResourceKind::Saltpetre:  return Use(TEXT("Quaternius/Nature"), TEXT("Pebble_Square_5"));
        case EResourceKind::Clay:
        case EResourceKind::Soil:
        case EResourceKind::Lime:       return Use(TEXT("Quaternius/Nature"), TEXT("Pebble_Round_5"));
        case EResourceKind::Sand:
        case EResourceKind::Ash:        return Use(TEXT("Quaternius/Nature"), TEXT("Pebble_Round_1"));
        case EResourceKind::Peat:
        case EResourceKind::Coal:
        case EResourceKind::Charcoal:   return Use(TEXT("Quaternius/Nature"), TEXT("Pebble_Square_6"));
        case EResourceKind::IronOre:
        case EResourceKind::CopperOre:
        case EResourceKind::TinOre:
        case EResourceKind::LeadOre:
        case EResourceKind::SilverOre:
        case EResourceKind::GoldOre:    return Use(TEXT("Quaternius/Nature"), TEXT("Rock_Medium_2"));
        case EResourceKind::Brick:      return Use(TEXT("Quaternius/Village"), TEXT("Prop_Brick1"));
        case EResourceKind::Iron:
        case EResourceKind::Steel:
        case EResourceKind::Copper:
        case EResourceKind::Bronze:
        case EResourceKind::Tin:
        case EResourceKind::Lead:
        case EResourceKind::Silver:
        case EResourceKind::Gold:
        case EResourceKind::Soap:       return Use(TEXT("Quaternius/Village"), TEXT("Prop_Brick3"));
        case EResourceKind::Mushroom:   return Use(TEXT("Quaternius/Nature"), TEXT("Mushroom_Common"));
        case EResourceKind::Paper:      return Use(TEXT("PolyHaven/book_encyclopedia_set_01"), TEXT("book_encyclopedia_set_01_book03"));
        case EResourceKind::Axe:
        case EResourceKind::Tool:       return Use(TEXT("PolyHaven/wooden_axe"), TEXT(""));
        case EResourceKind::Rod:
            OutTurn = FRotator::ZeroRotator;
            return Use(TEXT("PolyHaven/wooden_broom"), TEXT("wooden_broom_handle"));
        case EResourceKind::Maize:      return Use(TEXT("PolyHaven/wicker_basket_01"), TEXT(""));
        case EResourceKind::Bread:      return Use(TEXT("PolyHaven/carved_wooden_plate"), TEXT(""));
        case EResourceKind::Sickle:     return Use(TEXT("PolyHaven/wooden_axe"), TEXT(""));
        case EResourceKind::Shirt:      return Use(TEXT("KenneyItems/pillow"), TEXT(""));

        // --- Расширение мира: настоящий вид новых припасов -------------------
        case EResourceKind::Turnip:
        case EResourceKind::Beet:
        case EResourceKind::Potato:     return Use(TEXT("PolyHaven/food_apple_01"), TEXT(""));
        case EResourceKind::Cabbage:    return Use(TEXT("PolyHaven/yellow_onion"), TEXT(""));
        case EResourceKind::Onion:
        case EResourceKind::Garlic:     return Use(TEXT("PolyHaven/yellow_onion"), TEXT(""));
        case EResourceKind::Carrot:     return Use(TEXT("PolyHaven/carved_wooden_plate"), TEXT(""));
        case EResourceKind::Cucumber:   return Use(TEXT("PolyHaven/food_apple_01"), TEXT(""));
        case EResourceKind::Pumpkin:    return Use(TEXT("PolyHaven/yellow_onion"), TEXT(""));
        case EResourceKind::Melon:
        case EResourceKind::Watermelon: return Use(TEXT("PolyHaven/yellow_onion"), TEXT(""));
        case EResourceKind::Apple:
        case EResourceKind::Pear:
        case EResourceKind::Plum:
        case EResourceKind::Cherry:
        case EResourceKind::Grape:      return Use(TEXT("PolyHaven/food_apple_01"), TEXT(""));
        case EResourceKind::Nut:
        case EResourceKind::Acorn:
        case EResourceKind::Sunflower:  return Use(TEXT("PolyHaven/food_apple_01"), TEXT(""));
        case EResourceKind::Cheese:
        case EResourceKind::CottageCheese:
        case EResourceKind::Butter:
        case EResourceKind::Tallow:
        case EResourceKind::SmokedFish:
        case EResourceKind::SmokedMeat:
        case EResourceKind::DriedFish:  return Use(TEXT("PolyHaven/wooden_bowl_01"), TEXT(""));
        case EResourceKind::Kvass:
        case EResourceKind::Beer:
        case EResourceKind::Wine:
        case EResourceKind::Mead:
        case EResourceKind::Vinegar:
        case EResourceKind::GrapeMust:
        case EResourceKind::CurdledMilk: return Use(TEXT("PolyHaven/jug_01"), TEXT(""));
        case EResourceKind::Feather:    return Use(TEXT("KenneyItems/pillow"), TEXT(""));
        case EResourceKind::Fur:        return Use(TEXT("KenneyItems/rugSquare"), TEXT(""));
        case EResourceKind::Antler:     return Use(TEXT("PolyHaven/wooden_axe"), TEXT(""));
        case EResourceKind::Madder:
        case EResourceKind::Woad:
        case EResourceKind::Hops:       return Use(TEXT("Quaternius/Nature"), TEXT("Clover_1"));
        case EResourceKind::Basket:     return Use(TEXT("PolyHaven/wicker_basket_01"), TEXT(""));
        case EResourceKind::Candle:     return Use(TEXT("PolyHaven/pot_enamel_01"), TEXT(""));
        case EResourceKind::OliveOil:   return Use(TEXT("PolyHaven/jug_01"), TEXT(""));
        case EResourceKind::Marble:     return Use(TEXT("Quaternius/Nature"), TEXT("Pebble_Round_2"));
        case EResourceKind::Basalt:
        case EResourceKind::Slate:
        case EResourceKind::Obsidian:
        case EResourceKind::Mica:
        case EResourceKind::Graphite:   return Use(TEXT("Quaternius/Nature"), TEXT("Pebble_Square_3"));
        case EResourceKind::Gravel:
        case EResourceKind::Mud:
        case EResourceKind::Compost:    return Use(TEXT("Quaternius/Nature"), TEXT("Pebble_Square_5"));
        case EResourceKind::ZincOre:    return Use(TEXT("Quaternius/Nature"), TEXT("Rock_Medium_2"));
        case EResourceKind::Zinc:
        case EResourceKind::Brass:
        case EResourceKind::Electrum:   return Use(TEXT("Quaternius/Village"), TEXT("Prop_Brick3"));
        case EResourceKind::Gem:
        case EResourceKind::Diamond:
        case EResourceKind::Ruby:
        case EResourceKind::Emerald:
        case EResourceKind::Sapphire:
        case EResourceKind::Pearl:      return Use(TEXT("PolyHaven/food_apple_01"), TEXT(""));
        case EResourceKind::Gunpowder:
        case EResourceKind::Soda:       return Use(TEXT("PolyHaven/pot_enamel_01"), TEXT(""));
        case EResourceKind::Mortar:     return Use(TEXT("PolyHaven/wooden_bowl_01"), TEXT(""));
        case EResourceKind::CharcoalPencil: return Use(TEXT("PolyHaven/wooden_broom"), TEXT("wooden_broom_handle"));
        case EResourceKind::Vine:       return Use(TEXT("Quaternius/Nature"), TEXT("Clover_2"));
        case EResourceKind::Cork:
        case EResourceKind::Dye:        return Use(TEXT("PolyHaven/pot_enamel_01"), TEXT(""));
        case EResourceKind::BirchBark:  return Use(TEXT("Quaternius/Village"), TEXT("Roof_Log"));
        case EResourceKind::Stump:      return Use(TEXT("Quaternius/Village"), TEXT("Roof_Log"));
        case EResourceKind::Brushwood:  return Use(TEXT("PolyHaven/wooden_barrels_01"), TEXT("wooden_barrels_01_piece0*"));
        case EResourceKind::Cone:
        case EResourceKind::BirchWood:
        case EResourceKind::PineWood:   return Use(TEXT("Quaternius/Village"), TEXT("Roof_Log"));
        case EResourceKind::Hay:        return Use(TEXT("Quaternius/Nature"), TEXT("Grass_Wispy_Tall"));
        case EResourceKind::Hemp:       return Use(TEXT("Quaternius/Nature"), TEXT("Clover_2"));
        case EResourceKind::Wheat:
        case EResourceKind::Rye:
        case EResourceKind::Barley:
        case EResourceKind::Oats:       return Use(TEXT("Quaternius/Nature"), TEXT("Grass_Wispy_Tall"));
        default:                        return false;
        }
    }

    UStaticMesh* MeshFor(EMatterShape Shape)
    {
        const TCHAR* Name = TEXT("Cube");
        switch (Shape)
        {
        case EMatterShape::Cylinder: Name = TEXT("Cylinder"); break;
        case EMatterShape::Sphere:   Name = TEXT("Sphere"); break;
        case EMatterShape::Cone:     Name = TEXT("Cone"); break;
        default:                     break;
        }
        return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
    }
}

AResourceActor::AResourceActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    RootComponent = Body;
    Body->SetMobility(EComponentMobility::Movable);

    Affordances = CreateDefaultSubobject<UAffordanceComponent>(TEXT("Affordances"));
    Matter = CreateDefaultSubobject<UMatterComponent>(TEXT("Matter"));

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(RootComponent);
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(20.0f);
    Label->SetTextRenderColor(FColor(255, 246, 200));
    Label->SetCanEverAffectNavigation(false);
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label->SetAbsolute(false, false, true);
    Label->SetHiddenInGame(true);
    Label->SetVisibility(false);
}

void AResourceActor::BeginPlay()
{
    Super::BeginPlay();

    if (Affordances && Affordances->Offers.Num() == 0)
    {
        Setup(Kind, Amount);
    }
}

FString AResourceActor::KindName(EResourceKind Which)
{
    switch (Which)
    {
    case EResourceKind::RawFood:    return TEXT("продукты");
    case EResourceKind::CookedFood: return TEXT("готовая еда");
    case EResourceKind::Water:      return TEXT("вода");
    case EResourceKind::Coin:       return TEXT("деньги");
    case EResourceKind::Firewood:   return TEXT("дрова");
    case EResourceKind::Herb:       return TEXT("травы");
    case EResourceKind::Seed:       return TEXT("семена");
    case EResourceKind::Cloth:      return TEXT("ткань");
    default:                        return FCraftBook::NameOfKind(Which);
    }
}

EResourceKind AResourceActor::KindFromWord(const FString& Word)
{
    if (Word.IsEmpty())
    {
        return EResourceKind::None;
    }
    if (const UEnum* Kinds = StaticEnum<EResourceKind>())
    {
        const int64 Value = Kinds->GetValueByNameString(Word, EGetByNameFlags::CaseSensitive);
        if (Value != INDEX_NONE)
        {
            return static_cast<EResourceKind>(Value);
        }
        for (int32 i = 0; i < Kinds->NumEnums() - 1; ++i)
        {
            if (Kinds->GetNameStringByIndex(i).Equals(Word, ESearchCase::IgnoreCase))
            {
                return static_cast<EResourceKind>(Kinds->GetValueByIndex(i));
            }
        }
    }
    for (const FSubstance& S : FMatter::All())
    {
        if (FString(S.Name).StartsWith(Word, ESearchCase::IgnoreCase))
        {
            return S.Kind;
        }
    }
    return EResourceKind::None;
}

void AResourceActor::BaseLook(EResourceKind Which, EMatterShape& OutShape, FVector& OutSize, const TCHAR*& OutFallback)
{
    using S = EMatterShape;
    OutShape = S::Box;
    OutFallback = TEXT("M_Wood_Oak");
    OutSize = FVector(24.0f, 20.0f, 16.0f);

    auto Set = [&](S Shape, const TCHAR* Fallback, float X, float Y, float Z)
    {
        OutShape = Shape;
        OutFallback = Fallback;
        OutSize = FVector(X, Y, Z);
    };

    switch (Which)
    {
    case EResourceKind::RawFood:    Set(S::Box, TEXT("M_Ground_Moss"), 30, 24, 20); break;
    case EResourceKind::CookedFood: Set(S::Cylinder, TEXT("M_Metal_Chrome"), 26, 26, 9); break;
    case EResourceKind::Water:      Set(S::Cylinder, TEXT("M_Glass"), 14, 14, 28); break;
    case EResourceKind::Coin:       Set(S::Cylinder, TEXT("M_Metal_Gold"), 9, 9, 3); break;
    case EResourceKind::Firewood:   Set(S::Box, TEXT("M_Wood_Pine"), 40, 18, 18); break;
    case EResourceKind::Herb:       Set(S::Sphere, TEXT("M_Ground_Grass"), 18, 18, 14); break;
    case EResourceKind::Seed:       Set(S::Sphere, TEXT("M_Ground_Gravel"), 12, 12, 10); break;
    case EResourceKind::Cloth:      Set(S::Box, TEXT("M_Basic_Wall"), 28, 22, 8); break;
    case EResourceKind::Wood:       Set(S::Cylinder, TEXT("M_Wood_Oak"), 22, 22, 140); break;
    case EResourceKind::Plank:      Set(S::Box, TEXT("M_Wood_Pine"), 150, 24, 5); break;
    case EResourceKind::Bark:       Set(S::Box, TEXT("M_Wood_Walnut"), 40, 26, 4); break;
    case EResourceKind::Reed:       Set(S::Cylinder, TEXT("M_Ground_Grass"), 8, 8, 120); break;
    case EResourceKind::Straw:      Set(S::Cylinder, TEXT("M_Ground_Grass"), 40, 40, 60); break;
    case EResourceKind::Stone:
    case EResourceKind::Granite:    Set(S::Sphere, TEXT("M_Rock_Basalt"), 30, 26, 22); break;
    case EResourceKind::Limestone:
    case EResourceKind::Sandstone:  Set(S::Box, TEXT("M_Rock_Sandstone"), 30, 26, 20); break;
    case EResourceKind::Chalk:
    case EResourceKind::Quartz:     Set(S::Sphere, TEXT("M_Rock_Marble_Polished"), 22, 20, 18); break;
    case EResourceKind::Flint:      Set(S::Cone, TEXT("M_Rock_Basalt"), 20, 20, 16); break;
    case EResourceKind::Clay:       Set(S::Sphere, TEXT("M_Ground_Gravel"), 26, 26, 18); break;
    case EResourceKind::Soil:       Set(S::Sphere, TEXT("M_Ground_Gravel"), 30, 30, 20); break;
    case EResourceKind::Sand:       Set(S::Cone, TEXT("M_Rock_Sandstone"), 34, 34, 16); break;
    case EResourceKind::Brick:      Set(S::Box, TEXT("M_Brick_Clay_New"), 25, 12, 8); break;
    case EResourceKind::Peat:
    case EResourceKind::Coal:
    case EResourceKind::Charcoal:   Set(S::Box, TEXT("M_Basic_Floor"), 22, 20, 14); break;
    case EResourceKind::Ash:        Set(S::Cone, TEXT("M_Concrete_Tiles"), 24, 24, 10); break;
    case EResourceKind::IronOre:
    case EResourceKind::CopperOre:
    case EResourceKind::TinOre:
    case EResourceKind::LeadOre:
    case EResourceKind::SilverOre:
    case EResourceKind::GoldOre:    Set(S::Sphere, TEXT("M_Metal_Rust"), 26, 24, 20); break;
    case EResourceKind::Iron:
    case EResourceKind::Steel:      Set(S::Box, TEXT("M_Metal_Steel"), 28, 14, 8); break;
    case EResourceKind::Copper:
    case EResourceKind::Bronze:     Set(S::Box, TEXT("M_Metal_Copper"), 26, 13, 8); break;
    case EResourceKind::Tin:
    case EResourceKind::Lead:       Set(S::Box, TEXT("M_Metal_Burnished_Steel"), 24, 12, 8); break;
    case EResourceKind::Silver:     Set(S::Box, TEXT("M_Metal_Chrome"), 20, 10, 6); break;
    case EResourceKind::Gold:       Set(S::Box, TEXT("M_Metal_Gold"), 18, 9, 6); break;
    case EResourceKind::Grain:      Set(S::Sphere, TEXT("M_Ground_Grass"), 24, 20, 18); break;
    case EResourceKind::Flour:      Set(S::Box, TEXT("M_Basic_Wall"), 24, 20, 22); break;
    case EResourceKind::Berry:      Set(S::Sphere, TEXT("M_Rock_Marble_Polished"), 14, 14, 12); break;
    case EResourceKind::Mushroom:   Set(S::Cone, TEXT("M_Ground_Moss"), 16, 16, 14); break;
    case EResourceKind::Egg:        Set(S::Sphere, TEXT("M_Basic_Wall"), 5, 5, 6); break;
    case EResourceKind::Meat:       Set(S::Box, TEXT("M_Brick_Clay_Old"), 26, 18, 12); break;
    case EResourceKind::Fish:       Set(S::Box, TEXT("M_Metal_Chrome"), 34, 12, 9); break;
    case EResourceKind::Milk:
    case EResourceKind::Oil:        Set(S::Cylinder, TEXT("M_Basic_Wall"), 16, 16, 24); break;
    case EResourceKind::Honey:
    case EResourceKind::Wax:
    case EResourceKind::Resin:      Set(S::Cylinder, TEXT("M_Metal_Gold"), 15, 15, 20); break;
    case EResourceKind::Tar:        Set(S::Cylinder, TEXT("M_Basic_Floor"), 18, 18, 22); break;
    case EResourceKind::Salt:       Set(S::Box, TEXT("M_Rock_Marble_Polished"), 18, 18, 14); break;
    case EResourceKind::Medicine:   Set(S::Cylinder, TEXT("M_Ground_Moss"), 12, 12, 18); break;
    case EResourceKind::Soap:       Set(S::Box, TEXT("M_Concrete_Tiles"), 16, 10, 6); break;
    case EResourceKind::Paper:      Set(S::Box, TEXT("M_Basic_Wall"), 21, 30, 2); break;
    case EResourceKind::Ink:        Set(S::Cylinder, TEXT("M_Basic_Floor"), 10, 10, 12); break;
    case EResourceKind::Thread:     Set(S::Cylinder, TEXT("M_Basic_Wall"), 10, 10, 14); break;
    case EResourceKind::Rope:       Set(S::Cylinder, TEXT("M_Wood_Walnut"), 26, 26, 10); break;
    case EResourceKind::Leather:
    case EResourceKind::Hide:       Set(S::Box, TEXT("M_Wood_Walnut"), 34, 28, 3); break;
    case EResourceKind::Flax:
    case EResourceKind::Wool:       Set(S::Sphere, TEXT("M_Basic_Wall"), 24, 24, 18); break;
    case EResourceKind::Pot:        Set(S::Cylinder, TEXT("M_Brick_Clay_Old"), 22, 22, 20); break;
    case EResourceKind::Glass:      Set(S::Box, TEXT("M_Glass"), 24, 24, 4); break;
    case EResourceKind::Snow:
    case EResourceKind::Ice:        Set(S::Box, TEXT("M_Glass"), 22, 22, 16); break;
    case EResourceKind::Bone:       Set(S::Cylinder, TEXT("M_Basic_Wall"), 8, 8, 30); break;
    case EResourceKind::Sulphur:
    case EResourceKind::Saltpetre:  Set(S::Sphere, TEXT("M_Metal_Gold"), 18, 18, 14); break;
    case EResourceKind::Lime:       Set(S::Cone, TEXT("M_Basic_Wall"), 24, 24, 18); break;
    case EResourceKind::Axe:        Set(S::Box, TEXT("M_Metal_Steel"), 26, 6, 14); break;
    case EResourceKind::Shovel:     Set(S::Box, TEXT("M_Metal_Burnished_Steel"), 18, 16, 70); break;
    case EResourceKind::Saw:        Set(S::Box, TEXT("M_Metal_Chrome"), 60, 3, 16); break;
    case EResourceKind::Hammer:     Set(S::Box, TEXT("M_Metal_Steel"), 16, 10, 34); break;
    case EResourceKind::Needle:     Set(S::Cylinder, TEXT("M_Metal_Chrome"), 3, 3, 16); break;
    case EResourceKind::Rod:        Set(S::Cylinder, TEXT("M_Wood_Pine"), 5, 5, 150); break;
    case EResourceKind::Tool:       Set(S::Cylinder, TEXT("M_Metal_Steel"), 6, 6, 12); break;
    case EResourceKind::Maize:      Set(S::Cylinder, TEXT("M_Ground_Grass"), 30, 30, 22); break;
    case EResourceKind::Bread:      Set(S::Box, TEXT("M_Wood_Walnut"), 24, 18, 10); break;
    case EResourceKind::Sickle:     Set(S::Box, TEXT("M_Metal_Steel"), 40, 4, 20); break;
    case EResourceKind::Shirt:      Set(S::Box, TEXT("M_Basic_Wall"), 36, 28, 6); break;
    case EResourceKind::Turnip:     Set(S::Sphere,  TEXT("M_Ground_Moss"), 16, 16, 14); break;
    case EResourceKind::Cabbage:    Set(S::Sphere,  TEXT("M_Ground_Grass"), 22, 22, 18); break;
    case EResourceKind::Carrot:     Set(S::Cone,    TEXT("M_Wood_Oak"), 10, 10, 26); break;
    case EResourceKind::Onion:      Set(S::Sphere,  TEXT("M_Basic_Wall"), 14, 14, 12); break;
    case EResourceKind::Garlic:     Set(S::Sphere,  TEXT("M_Basic_Wall"), 12, 12, 10); break;
    case EResourceKind::Beet:       Set(S::Sphere,  TEXT("M_Wood_Walnut"), 14, 14, 12); break;
    case EResourceKind::Cucumber:   Set(S::Cylinder, TEXT("M_Ground_Grass"), 10, 10, 28); break;
    case EResourceKind::Pumpkin:    Set(S::Sphere,  TEXT("M_Wood_Oak"), 30, 30, 24); break;
    case EResourceKind::Melon:      Set(S::Sphere,  TEXT("M_Ground_Grass"), 26, 26, 22); break;
    case EResourceKind::Watermelon: Set(S::Sphere,  TEXT("M_Ground_Grass"), 32, 32, 28); break;
    case EResourceKind::Potato:     Set(S::Sphere,  TEXT("M_Ground_Gravel"), 14, 14, 12); break;
    case EResourceKind::Sunflower:  Set(S::Cylinder, TEXT("M_Wood_Oak"), 18, 18, 30); break;
    case EResourceKind::Wheat:      Set(S::Cylinder, TEXT("M_Wood_Oak"), 14, 14, 30); break;
    case EResourceKind::Rye:        Set(S::Cylinder, TEXT("M_Wood_Walnut"), 14, 14, 30); break;
    case EResourceKind::Barley:     Set(S::Cylinder, TEXT("M_Ground_Grass"), 14, 14, 28); break;
    case EResourceKind::Oats:       Set(S::Cylinder, TEXT("M_Basic_Wall"), 14, 14, 28); break;
    case EResourceKind::Hemp:       Set(S::Cylinder, TEXT("M_Ground_Grass"), 12, 12, 40); break;
    case EResourceKind::Hay:        Set(S::Cylinder, TEXT("M_Ground_Grass"), 36, 36, 50); break;
    case EResourceKind::Apple:      Set(S::Sphere,  TEXT("M_Wood_Oak"), 14, 14, 13); break;
    case EResourceKind::Pear:       Set(S::Sphere,  TEXT("M_Ground_Grass"), 14, 14, 16); break;
    case EResourceKind::Plum:       Set(S::Sphere,  TEXT("M_Metal_Gold"), 12, 12, 12); break;
    case EResourceKind::Cherry:     Set(S::Sphere,  TEXT("M_Wood_Walnut"), 10, 10, 10); break;
    case EResourceKind::Grape:      Set(S::Sphere,  TEXT("M_Metal_Gold"), 16, 16, 20); break;
    case EResourceKind::Nut:        Set(S::Sphere,  TEXT("M_Wood_Walnut"), 12, 12, 10); break;
    case EResourceKind::Feather:    Set(S::Box,     TEXT("M_Basic_Wall"), 6, 14, 2); break;
    case EResourceKind::Fur:        Set(S::Box,     TEXT("M_Wood_Walnut"), 34, 28, 4); break;
    case EResourceKind::Antler:     Set(S::Cylinder, TEXT("M_Basic_Wall"), 6, 6, 36); break;
    case EResourceKind::Tallow:     Set(S::Box,     TEXT("M_Basic_Wall"), 18, 14, 10); break;
    case EResourceKind::CurdledMilk: Set(S::Cylinder, TEXT("M_Glass"), 14, 14, 20); break;
    case EResourceKind::Butter:     Set(S::Box,     TEXT("M_Wood_Oak"), 16, 10, 8); break;
    case EResourceKind::Cheese:     Set(S::Cylinder, TEXT("M_Wood_Oak"), 20, 20, 12); break;
    case EResourceKind::CottageCheese: Set(S::Box,     TEXT("M_Basic_Wall"), 16, 12, 8); break;
    case EResourceKind::SmokedFish: Set(S::Box,     TEXT("M_Wood_Walnut"), 24, 8, 6); break;
    case EResourceKind::SmokedMeat: Set(S::Box,     TEXT("M_Wood_Walnut"), 22, 10, 8); break;
    case EResourceKind::DriedFish:  Set(S::Box,     TEXT("M_Basic_Wall"), 22, 8, 5); break;
    case EResourceKind::Hops:       Set(S::Sphere,  TEXT("M_Ground_Grass"), 14, 14, 16); break;
    case EResourceKind::GrapeMust:  Set(S::Cylinder, TEXT("M_Metal_Gold"), 16, 16, 22); break;
    case EResourceKind::Kvass:      Set(S::Cylinder, TEXT("M_Wood_Oak"), 14, 14, 26); break;
    case EResourceKind::Beer:       Set(S::Cylinder, TEXT("M_Wood_Oak"), 14, 14, 26); break;
    case EResourceKind::Wine:       Set(S::Cylinder, TEXT("M_Glass"), 12, 12, 26); break;
    case EResourceKind::Mead:       Set(S::Cylinder, TEXT("M_Wood_Oak"), 14, 14, 26); break;
    case EResourceKind::Vinegar:    Set(S::Cylinder, TEXT("M_Glass"), 12, 12, 20); break;
    case EResourceKind::Madder:     Set(S::Cylinder, TEXT("M_Wood_Walnut"), 8, 8, 24); break;
    case EResourceKind::Woad:       Set(S::Sphere,  TEXT("M_Ground_Grass"), 18, 18, 14); break;
    case EResourceKind::Dye:        Set(S::Cylinder, TEXT("M_Metal_Gold"), 12, 12, 16); break;
    case EResourceKind::Candle:     Set(S::Cylinder, TEXT("M_Basic_Wall"), 6, 6, 24); break;
    case EResourceKind::OliveOil:   Set(S::Cylinder, TEXT("M_Metal_Gold"), 12, 12, 20); break;
    case EResourceKind::Soda:       Set(S::Box,     TEXT("M_Basic_Wall"), 14, 14, 10); break;
    case EResourceKind::Vine:       Set(S::Cylinder, TEXT("M_Wood_Walnut"), 6, 6, 100); break;
    case EResourceKind::Basket:     Set(S::Cylinder, TEXT("M_Wood_Oak"), 26, 26, 18); break;
    case EResourceKind::Cork:       Set(S::Cylinder, TEXT("M_Wood_Oak"), 10, 10, 12); break;
    case EResourceKind::Gunpowder:  Set(S::Cylinder, TEXT("M_Basic_Floor"), 12, 12, 14); break;
    case EResourceKind::Mortar:     Set(S::Box,     TEXT("M_Concrete_Tiles"), 20, 20, 12); break;
    case EResourceKind::CharcoalPencil: Set(S::Cylinder, TEXT("M_Basic_Floor"), 4, 4, 20); break;
    case EResourceKind::BirchWood:  Set(S::Cylinder, TEXT("M_Basic_Wall"), 22, 22, 140); break;
    case EResourceKind::PineWood:   Set(S::Cylinder, TEXT("M_Wood_Pine"), 22, 22, 140); break;
    case EResourceKind::BirchBark:  Set(S::Box,     TEXT("M_Basic_Wall"), 40, 26, 4); break;
    case EResourceKind::Stump:      Set(S::Cylinder, TEXT("M_Wood_Walnut"), 44, 44, 40); break;
    case EResourceKind::Brushwood:  Set(S::Cylinder, TEXT("M_Wood_Walnut"), 24, 24, 90); break;
    case EResourceKind::Cone:       Set(S::Cone,    TEXT("M_Wood_Walnut"), 16, 16, 24); break;
    case EResourceKind::Acorn:      Set(S::Sphere,  TEXT("M_Wood_Oak"), 10, 10, 12); break;
    case EResourceKind::Marble:     Set(S::Box,     TEXT("M_Rock_Marble_Polished"), 30, 26, 20); break;
    case EResourceKind::Basalt:     Set(S::Sphere,  TEXT("M_Rock_Basalt"), 30, 26, 22); break;
    case EResourceKind::Slate:      Set(S::Box,     TEXT("M_Rock_Sandstone"), 30, 26, 12); break;
    case EResourceKind::Obsidian:   Set(S::Sphere,  TEXT("M_Glass"), 20, 18, 16); break;
    case EResourceKind::Mica:       Set(S::Box,     TEXT("M_Glass"), 18, 18, 4); break;
    case EResourceKind::Graphite:   Set(S::Box,     TEXT("M_Basic_Floor"), 14, 14, 10); break;
    case EResourceKind::Gravel:     Set(S::Sphere,  TEXT("M_Ground_Gravel"), 20, 20, 14); break;
    case EResourceKind::Mud:        Set(S::Box,     TEXT("M_Ground_Moss"), 20, 20, 12); break;
    case EResourceKind::Compost:    Set(S::Box,     TEXT("M_Ground_Moss"), 22, 22, 14); break;
    case EResourceKind::ZincOre:    Set(S::Sphere,  TEXT("M_Rock_Basalt"), 28, 26, 22); break;
    case EResourceKind::Zinc:       Set(S::Box,     TEXT("M_Metal_Chrome"), 16, 14, 10); break;
    case EResourceKind::Brass:      Set(S::Box,     TEXT("M_Metal_Gold"), 18, 14, 10); break;
    case EResourceKind::Electrum:   Set(S::Cylinder, TEXT("M_Metal_Gold"), 10, 10, 4); break;
    case EResourceKind::Gem:        Set(S::Sphere,  TEXT("M_Glass"), 12, 12, 10); break;
    case EResourceKind::Diamond:    Set(S::Box,     TEXT("M_Glass"), 8, 8, 8); break;
    case EResourceKind::Ruby:       Set(S::Box,     TEXT("M_Glass"), 8, 8, 8); break;
    case EResourceKind::Emerald:    Set(S::Box,     TEXT("M_Glass"), 8, 8, 8); break;
    case EResourceKind::Sapphire:   Set(S::Box,     TEXT("M_Glass"), 8, 8, 8); break;
    case EResourceKind::Pearl:      Set(S::Sphere,  TEXT("M_Glass"), 8, 8, 8); break;
    default:                        break;
    }

    if (Which == EResourceKind::Axe || Which == EResourceKind::Shovel || Which == EResourceKind::Saw
        || Which == EResourceKind::Hammer || Which == EResourceKind::Needle || Which == EResourceKind::Tool
        || Which == EResourceKind::Sickle)
    {
        OutSize *= 0.35f;
    }
}

float AResourceActor::UnitVolume(EResourceKind Which)
{
    EMatterShape Shape = EMatterShape::Box;
    FVector Size = FVector::OneVector;
    const TCHAR* Fallback = nullptr;
    BaseLook(Which, Shape, Size, Fallback);
    return UMatterComponent::VolumeOf(Shape, Size);
}

FVector AResourceActor::CurrentSize() const
{
    if (!CustomSize.IsNearlyZero())
    {
        return CustomSize;
    }
    return Matter ? Matter->GetSizeCm() : FVector(20.0f);
}

void AResourceActor::Setup(EResourceKind InKind, float InAmount)
{
    Kind = InKind;
    Amount = FMath::Max(0.0f, InAmount);

    EMatterShape Shape = EMatterShape::Box;
    FVector Size = FVector(20.0f);
    const TCHAR* Fallback = TEXT("M_Wood_Oak");
    BaseLook(Kind, Shape, Size, Fallback);
    if (bKeepShape)
    {
        Shape = KeptShape;
    }
    if (!CustomSize.IsNearlyZero())
    {
        Size = CustomSize;
    }
    else
    {
        Size *= FMath::Clamp(FMath::Pow(FMath::Max(Amount, 0.05f), 1.0f / 3.0f), 0.45f, 3.0f);
    }

    if (UStaticMesh* M = MeshFor(Shape))
    {
        Body->SetStaticMesh(M);
    }
    Body->SetRelativeScale3D(Size / 100.0f);

    UMaterialInterface* Look = nullptr;
    if (UMatterSubsystem* MatterWorld = UMatterSubsystem::Get(this))
    {
        Look = MatterWorld->LookOf(Kind);
    }
    if (!Look)
    {
        Look = LoadObject<UMaterialInterface>(nullptr,
            *FString::Printf(TEXT("/Game/StarterContent/Materials/%s.%s"), Fallback, Fallback));
    }
    if (Look)
    {
        Body->SetMaterial(0, Look);
    }

    FString ModelFolder;
    FString ModelFilter;
    FRotator ModelTurn;
    TArray<UStaticMesh*> Models;
    if (ModelOf(Kind, ModelFolder, ModelFilter, ModelTurn))
    {
        FImportedModels::Gather(TEXT("/Game/Imported/") + ModelFolder, ModelFilter, Models);
    }
    if (Models.Num() > 0)
    {
        if (!Shape3D)
        {
            Shape3D = NewObject<UStaticMeshComponent>(this);
            Shape3D->SetMobility(EComponentMobility::Movable);
            Shape3D->SetupAttachment(Body);
            Shape3D->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Shape3D->SetCanEverAffectNavigation(false);
            Shape3D->RegisterComponent();
        }
        Shape3D->SetStaticMesh(Models[0]);
        const FBox Box = Models[0]->GetBoundingBox();
        const FVector Extent = Box.GetSize().ComponentMax(FVector(0.1f));
        const FVector Scale(100.0f / Extent.X, 100.0f / Extent.Y, 100.0f / Extent.Z);
        const FQuat Turn = ModelTurn.Quaternion();
        Shape3D->SetRelativeTransform(FTransform(Turn, -Turn.RotateVector(Box.GetCenter() * Scale), Scale));
        Shape3D->SetVisibility(true);
        Body->SetVisibility(false, false);
    }
    else
    {
        if (Shape3D)
        {
            Shape3D->SetVisibility(false);
        }
        Body->SetVisibility(true, false);
    }

    if (Label)
    {
        Label->SetText(FText::FromString(KindName(Kind)));
        const float Height = FMath::Max(1.0f, static_cast<float>(Size.Z));
        Label->SetRelativeLocation(FVector(0.0f, 0.0f, (Height * 0.5f + 22.0f) / (Height / 100.0f)));
        Label->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    }

    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetCollisionObjectType(ECC_PhysicsBody);
    Body->SetCollisionResponseToAllChannels(ECR_Block);
    Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Body->SetAngularDamping(2.5f);
    Body->SetLinearDamping(0.05f);
    Body->BodyInstance.SleepFamily = ESleepFamily::Custom;
    Body->BodyInstance.CustomSleepThresholdMultiplier = 6.0f;
    if (!Body->IsSimulatingPhysics())
    {
        Body->SetSimulatePhysics(false);
    }

    if (Matter)
    {
        Matter->Bind(Body, Shape, Size, Kind);
    }

    const FString Called = KindName(Kind);
    const bool bSpoiled = Matter && Matter->Rot >= 0.5f && FMatter::Of(Kind).bSpoils;
    Affordances->DisplayName = Called;
    Affordances->Category = Called;
    Affordances->NoticeRadius = 800.0f;
    Affordances->Capacity = 1;
    Affordances->Offers.Reset();

    if (bSpoiled && FMatter::Of(Kind).Food > 0.0f)
    {
        Affordances->AddOffer(EActionType::Eat, TEXT("съесть порченое"), 400.0f);
        Affordances->AddPromise(ENeedType::Hunger, FMath::Min(0.4f, 0.15f * Amount));
        Affordances->AddPromise(ENeedType::Health, -0.35f);
        Affordances->Offers.Last().EffortCost = 0.02f;
        Affordances->Offers.Last().bNeedsInHand = true;
        Affordances->Offers.Last().Risk = 0.6f;
    }
    else
    {
        switch (Kind)
        {
        case EResourceKind::CookedFood:
        case EResourceKind::Bread:
            Affordances->AddOffer(EActionType::Eat, TEXT("съесть"), 600.0f);
            Affordances->AddPromise(ENeedType::Hunger, FMath::Min(1.0f, 0.45f * Amount));
            Affordances->Offers.Last().EffortCost = 0.0f;
            Affordances->Offers.Last().bNeedsInHand = true;
            Affordances->Offers.Last().SetupSeconds = 3.0f;
            break;

        case EResourceKind::RawFood:
            Affordances->AddOffer(EActionType::Eat, TEXT("погрызть сырое"), 400.0f);
            Affordances->AddPromise(ENeedType::Hunger, FMath::Min(0.5f, 0.22f * Amount));
            Affordances->AddPromise(ENeedType::Health, -0.12f);
            Affordances->Offers.Last().EffortCost = 0.02f;
            Affordances->Offers.Last().bNeedsInHand = true;
            Affordances->Offers.Last().Risk = 0.18f;
            break;

        case EResourceKind::Water:
            Affordances->AddOffer(EActionType::Drink, TEXT("попить"), 90.0f);
            Affordances->AddPromise(ENeedType::Thirst, FMath::Min(1.0f, 0.6f * Amount));
            Affordances->Offers.Last().EffortCost = 0.0f;
            Affordances->Offers.Last().bNeedsInHand = true;
            break;

        case EResourceKind::Herb:
            Affordances->AddOffer(EActionType::Eat, TEXT("пожевать травы"), 300.0f);
            Affordances->AddPromise(ENeedType::Health, 0.25f);
            Affordances->Offers.Last().EffortCost = 0.02f;
            Affordances->Offers.Last().bNeedsInHand = true;
            break;

        default:
        {
            // --- Расширение мира: каждый съедобный припас можно откусить ------
            // Берут в руки, подносят ко рту — и кусок исчезает. Питьё пьют.
            const FSubstance& S = FMatter::Of(Kind);
            if (S.Food > 0.0f)
            {
                const bool bDrink = S.Form == EMatterForm::Liquid;
                const float Bite = FMath::Clamp(0.18f + S.Food / 6000.0f, 0.18f, 0.75f);

                Affordances->AddOffer(bDrink ? EActionType::Drink : EActionType::Eat,
                    bDrink ? TEXT("отхлебнуть") : TEXT("откусить"), 400.0f);
                Affordances->AddPromise(ENeedType::Hunger, bDrink ? Bite * 0.4f : Bite);
                if (bDrink)
                {
                    Affordances->AddPromise(ENeedType::Thirst, Bite);
                }
                // Угощение и вино согревают душу: не всякая еда — только приятная.
                if (Kind == EResourceKind::Grape || Kind == EResourceKind::Cherry
                    || Kind == EResourceKind::Plum || Kind == EResourceKind::Melon
                    || Kind == EResourceKind::Watermelon || Kind == EResourceKind::Cheese
                    || Kind == EResourceKind::Honey)
                {
                    Affordances->AddPromise(ENeedType::Comfort, 0.12f);
                }
                Affordances->Offers.Last().EffortCost = 0.02f;
                Affordances->Offers.Last().bNeedsInHand = true;
                Affordances->Offers.Last().SetupSeconds = 3.0f;
                // Кусок списывается с того, что в руках.
                Affordances->Offers.Last().Requires = Kind;
                Affordances->Offers.Last().RequiresAmount = FMath::Max(0.15f, Bite * 0.8f);
                break;
            }
            Affordances->AddOffer(EActionType::Observe, FString::Printf(TEXT("подобрать: %s"), *Called), 60.0f);
            Affordances->AddPromise(ENeedType::Order, 0.08f);
            Affordances->Offers.Last().EffortCost = 0.02f;
            Affordances->Offers.Last().bNeedsInHand = true;
            break;
        }
        }
    }

    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldMind->RegisterAffordanceSource(Affordances);
        }
    }
}

void AResourceActor::BecomeKind(EResourceKind NewKind, float SizeScale)
{
    if (NewKind == EResourceKind::None)
    {
        Destroy();
        return;
    }
    const FVector Size = CurrentSize() * FMath::Max(0.05f, SizeScale);
    const EMatterShape Shape = Matter ? Matter->GetShape() : EMatterShape::Box;
    const float Temperature = Matter ? Matter->Temperature : 12.0f;
    const bool bWasSimulating = Body && Body->IsSimulatingPhysics();

    CustomSize = Size;
    bKeepShape = true;
    KeptShape = FMatter::Of(NewKind).Form == EMatterForm::Liquid ? EMatterShape::Cylinder : Shape;
    const float NewAmount = UMatterComponent::VolumeOf(KeptShape, Size) / FMath::Max(1.0e-6f, UnitVolume(NewKind));

    Setup(NewKind, NewAmount);
    if (Matter)
    {
        Matter->Temperature = Temperature;
    }
    if (Body && bWasSimulating)
    {
        Body->SetSimulatePhysics(true);
    }
}

bool AResourceActor::PickUp(AActor* Who)
{
    if (!Who || (HeldBy && HeldBy != Who) || Amount <= 0.0f)
    {
        return false;
    }

    HeldBy = Who;
    Body->SetSimulatePhysics(false);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AttachToActor(Who, FAttachmentTransformRules::KeepWorldTransform);
    return true;
}

void AResourceActor::PutDown()
{
    if (!HeldBy)
    {
        return;
    }

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    HeldBy = nullptr;

    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetSimulatePhysics(true);
    Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
    Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    if (Matter)
    {
        Matter->WatchRest();
    }
}

float AResourceActor::Consume(float Portions)
{
    const float Before = Amount;
    const float Taken = FMath::Min(Amount, FMath::Max(0.0f, Portions));
    Amount -= Taken;

    if (Amount <= 0.01f)
    {
        if (UWorld* World = GetWorld())
        {
            if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
            {
                WorldMind->UnregisterAffordanceSource(Affordances);
            }
        }
        Destroy();
    }
    else if (Affordances)
    {
        if (!CustomSize.IsNearlyZero() && Before > 0.0f)
        {
            CustomSize *= FMath::Pow(Amount / Before, 1.0f / 3.0f);
        }
        Setup(Kind, Amount);
    }

    return Taken;
}

AResourceActor* AResourceActor::Spawn(UWorld* World, EResourceKind Kind, float Amount, const FVector& At)
{
    if (!World)
    {
        return nullptr;
    }

    const FTransform T(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), At);

    AResourceActor* Item = World->SpawnActorDeferred<AResourceActor>(
        AResourceActor::StaticClass(), T, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!Item)
    {
        return nullptr;
    }

    Item->Kind = Kind;
    Item->Amount = Amount;
    Item->FinishSpawning(T);
    Item->Setup(Kind, Amount);

    if (Item->Body)
    {
        Item->Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Item->Body->SetSimulatePhysics(true);
    }
    if (Item->Matter)
    {
        Item->Matter->WatchRest();
    }

    return Item;
}

AResourceActor* AResourceActor::SpawnPiece(UWorld* World, EResourceKind Kind, float Amount, const FTransform& Where, const FVector& SizeCm, EMatterShape Shape)
{
    if (!World || Kind == EResourceKind::None)
    {
        return nullptr;
    }

    FTransform T = Where;
    T.SetScale3D(FVector::OneVector);

    AResourceActor* Item = World->SpawnActorDeferred<AResourceActor>(
        AResourceActor::StaticClass(), T, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Item)
    {
        return nullptr;
    }

    Item->Kind = Kind;
    Item->Amount = FMath::Max(0.01f, Amount);
    Item->CustomSize = SizeCm;
    Item->bKeepShape = true;
    Item->KeptShape = Shape;
    Item->FinishSpawning(T);

    if (Item->Body)
    {
        Item->Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Item->Body->SetSimulatePhysics(true);
    }
    if (Item->Matter)
    {
        Item->Matter->WatchRest();
    }
    return Item;
}
