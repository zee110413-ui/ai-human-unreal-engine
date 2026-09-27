#include "Village.h"
#include "Crafts.h"
#include "CompleteHumanAI.h"
#include "FurnitureActor.h"
#include "ResourceActor.h"
#include "IdentityComponent.h"
#include "SocialComponent.h"
#include "MemoryComponent.h"
#include "MindComponent.h"
#include "NeedComponent.h"
#include "Textbook.h"
#include "ClothingActor.h"
#include "Matter.h"
#include "Components/PoseableMeshComponent.h"
#include "SpeechComponent.h"
#include "HumanWorldSubsystem.h"
#include "MatterSubsystem.h"
#include "AffordanceComponent.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "EngineUtils.h"

namespace
{
    bool GMedieval = true;

    struct FFolk
    {
        const TCHAR* First;
        const TCHAR* Last;
        float Age;
        bool bFemale;
        bool bMasterTeacher;
        EVillageRole Role;
        const TCHAR* Trade;
        int32 House;
        int32 Spouse;
        int32 Mother;
        int32 Father;
        float Money;
    };

    const FFolk Folk[] = {
        { TEXT("Всеволод"), TEXT("Светлоградский"), 46.0f, false, false, EVillageRole::King, TEXT(""), 0, 1, 4, -1, 520.0f },
        { TEXT("Елена"), TEXT("Светлоградская"), 41.0f, true, false, EVillageRole::Queen, TEXT(""), 0, 0, -1, -1, 60.0f },
        { TEXT("Ярослав"), TEXT("Светлоградский"), 19.0f, false, false, EVillageRole::Prince, TEXT(""), 0, -1, 1, 0, 30.0f },
        { TEXT("Василиса"), TEXT("Светлоградская"), 16.0f, true, false, EVillageRole::Princess, TEXT(""), 0, -1, 1, 0, 20.0f },
        { TEXT("Евдокия"), TEXT("Светлоградская"), 69.0f, true, false, EVillageRole::QueenMother, TEXT(""), 0, -1, -1, -1, 40.0f },
        { TEXT("Тихон"), TEXT("Ключников"), 52.0f, false, true, EVillageRole::Steward, TEXT(""), 0, -1, -1, -1, 15.0f },
        { TEXT("Пелагея"), TEXT("Стряпухина"), 38.0f, true, false, EVillageRole::Cook, TEXT("Cooking"), 0, -1, -1, -1, 10.0f },
        { TEXT("Илья"), TEXT("Сторожев"), 28.0f, false, false, EVillageRole::Guard, TEXT(""), 0, -1, -1, -1, 12.0f },
        { TEXT("Любава"), TEXT("Горничная"), 22.0f, true, false, EVillageRole::Maid, TEXT(""), 0, -1, -1, -1, 6.0f },
        { TEXT("Савва"), TEXT("Работников"), 30.0f, false, false, EVillageRole::Groom, TEXT("Woodcutting"), 0, -1, -1, -1, 8.0f },

        { TEXT("Степан"), TEXT("Рыбаков"), 44.0f, false, false, EVillageRole::Peasant, TEXT("Fishing"), 1, 11, -1, -1, 32.0f },
        { TEXT("Марфа"), TEXT("Рыбакова"), 40.0f, true, false, EVillageRole::Peasant, TEXT("Weaving"), 1, 10, -1, -1, 18.0f },
        { TEXT("Кузьма"), TEXT("Кузнецов"), 39.0f, false, false, EVillageRole::Peasant, TEXT("Smithing"), 2, 13, -1, -1, 40.0f },
        { TEXT("Ульяна"), TEXT("Кузнецова"), 35.0f, true, false, EVillageRole::Peasant, TEXT("Sewing"), 2, 12, -1, -1, 16.0f },
        { TEXT("Прохор"), TEXT("Пахомов"), 62.0f, false, false, EVillageRole::Peasant, TEXT("Farming"), 3, -1, -1, -1, 22.0f },
        { TEXT("Григорий"), TEXT("Пахомов"), 34.0f, false, false, EVillageRole::Peasant, TEXT("Carpentry"), 3, 16, -1, 14, 26.0f },
        { TEXT("Прасковья"), TEXT("Пахомова"), 31.0f, true, false, EVillageRole::Peasant, TEXT("Baking"), 3, 15, -1, -1, 14.0f },
        { TEXT("Лука"), TEXT("Гончаров"), 47.0f, false, false, EVillageRole::Peasant, TEXT("Pottery"), 4, 18, -1, -1, 30.0f },
        { TEXT("Агафья"), TEXT("Гончарова"), 45.0f, true, false, EVillageRole::Peasant, TEXT("Herbalism"), 4, 17, -1, -1, 20.0f },
        { TEXT("Митрофан"), TEXT("Гончаров"), 20.0f, false, false, EVillageRole::Peasant, TEXT("Trade"), 4, -1, 18, 17, 25.0f },
        { TEXT("Никита"), TEXT("Рыбаков"), 16.0f, false, false, EVillageRole::Peasant, TEXT(""), 1, -1, 11, 10, 4.0f },
        { TEXT("Варвара"), TEXT("Гончарова"), 14.0f, true, false, EVillageRole::Peasant, TEXT(""), 4, -1, 18, 17, 3.0f },
        { TEXT("Фома"), TEXT("Кузнецов"), 12.0f, false, false, EVillageRole::Child, TEXT(""), 2, -1, 13, 12, 1.0f },
        { TEXT("Дарья"), TEXT("Рыбакова"), 9.0f, true, false, EVillageRole::Child, TEXT(""), 1, -1, 11, 10, 0.0f },
        { TEXT("Данила"), TEXT("Пахомов"), 6.0f, false, false, EVillageRole::Child, TEXT(""), 3, -1, 16, 15, 0.0f },
        { TEXT("Аксинья"), TEXT("Кузнецова"), 3.0f, true, false, EVillageRole::Child, TEXT(""), 2, -1, 13, 12, 0.0f }
    };

    const TCHAR* ExtraMen[] = { TEXT("Иван"), TEXT("Василий"), TEXT("Фёдор"), TEXT("Семён"), TEXT("Пётр"), TEXT("Афанасий"),
        TEXT("Елисей"), TEXT("Тимофей"), TEXT("Борис"), TEXT("Глеб"), TEXT("Роман"), TEXT("Олег") };
    const TCHAR* ExtraWomen[] = { TEXT("Анна"), TEXT("Мария"), TEXT("Ирина"), TEXT("Настасья"), TEXT("Татьяна"), TEXT("Акулина"),
        TEXT("Устинья"), TEXT("Милава"), TEXT("Забава"), TEXT("Ольга"), TEXT("Софья"), TEXT("Феодосия") };
    const TCHAR* ExtraFamilies[] = { TEXT("Иванов"), TEXT("Петров"), TEXT("Сидоров"), TEXT("Лапин"), TEXT("Мельников"), TEXT("Бортников") };

    const FVillageTrade TradeTable[] = {
        { TEXT("Fishing"),     TEXT("рыбак"),     TEXT("рыбачка"),   TEXT("ловить рыбу") },
        { TEXT("Weaving"),     TEXT("ткач"),      TEXT("ткачиха"),   TEXT("прясть и ткать") },
        { TEXT("Smithing"),    TEXT("кузнец"),    TEXT("кузнечиха"), TEXT("ковать железо") },
        { TEXT("Sewing"),      TEXT("портной"),   TEXT("швея"),      TEXT("шить") },
        { TEXT("Farming"),     TEXT("пахарь"),    TEXT("пахарка"),   TEXT("растить хлеб") },
        { TEXT("Carpentry"),   TEXT("плотник"),   TEXT("плотница"),  TEXT("тесать и строить") },
        { TEXT("Baking"),      TEXT("хлебник"),   TEXT("хлебница"),  TEXT("печь хлеб") },
        { TEXT("Pottery"),     TEXT("гончар"),    TEXT("гончарка"),  TEXT("лепить горшки") },
        { TEXT("Herbalism"),   TEXT("травник"),   TEXT("травница"),  TEXT("знать травы и лечить") },
        { TEXT("Trade"),       TEXT("торговец"),  TEXT("торговка"),  TEXT("торговать") },
        { TEXT("Cooking"),     TEXT("повар"),     TEXT("стряпуха"),  TEXT("стряпать") },
        { TEXT("Woodcutting"), TEXT("лесоруб"),   TEXT("лесорубка"), TEXT("рубить лес") }
    };

    FVillageWork Work(const TCHAR* Id, const TCHAR* Label, EFurnitureType Station, const TCHAR* Skill, float Difficulty,
        float Minutes, float Effort, EResourceKind Output, float OutputAmount)
    {
        FVillageWork W;
        W.Id = Id;
        W.Label = Label;
        W.Station = static_cast<uint8>(Station);
        W.Skill = Skill;
        W.Difficulty = Difficulty;
        W.Duration = Minutes * 60.0f;
        W.Effort = Effort;
        W.Output = Output;
        W.OutputAmount = OutputAmount;
        return W;
    }

    TArray<FVillageWork> BuildWorks()
    {
        using R = EResourceKind;
        using F = EFurnitureType;
        TArray<FVillageWork> T;

        FVillageWork W = Work(TEXT("v_fish"), TEXT("ловить рыбу"), F::FishingSpot, TEXT("Fishing"), 0.35f, 90.0f, 0.2f, R::Fish, 1.0f);
        W.Tool = R::Rod; W.FromDay = 100; W.ToDay = 320;
        T.Add(W);

        W = Work(TEXT("v_icefish"), TEXT("ловить рыбу из проруби"), F::FishingSpot, TEXT("Fishing"), 0.5f, 120.0f, 0.35f, R::Fish, 1.0f);
        W.Tool = R::Rod; W.FromDay = 335; W.ToDay = 75;
        T.Add(W);

        W = Work(TEXT("v_garden"), TEXT("полоть огород и снять репу с капустой"), F::GardenBed, TEXT("Farming"), 0.15f, 60.0f, 0.4f, R::RawFood, 1.0f);
        W.FromDay = 160; W.ToDay = 285;
        T.Add(W);

        W = Work(TEXT("v_firewood"), TEXT("нарубить дров"), F::Tree, TEXT("Woodcutting"), 0.3f, 60.0f, 0.5f, R::Firewood, 4.0f);
        W.Tool = R::Axe;
        T.Add(W);

        W = Work(TEXT("v_logs"), TEXT("свалить дерево на брёвна"), F::Tree, TEXT("Woodcutting"), 0.45f, 90.0f, 0.6f, R::Wood, 2.0f);
        W.Tool = R::Axe;
        T.Add(W);

        W = Work(TEXT("v_berries"), TEXT("собрать ягоды"), F::Bush, TEXT("Herbalism"), 0.1f, 40.0f, 0.12f, R::Berry, 2.0f);
        W.FromDay = 170; W.ToDay = 265;
        T.Add(W);

        W = Work(TEXT("v_mushrooms"), TEXT("собрать грибы"), F::Mushrooms, TEXT("Herbalism"), 0.15f, 40.0f, 0.12f, R::Mushroom, 2.0f);
        W.FromDay = 180; W.ToDay = 290;
        T.Add(W);

        W = Work(TEXT("v_herbs"), TEXT("набрать трав"), F::WildField, TEXT("Herbalism"), 0.25f, 30.0f, 0.1f, R::Herb, 2.0f);
        W.FromDay = 140; W.ToDay = 260;
        T.Add(W);

        W = Work(TEXT("v_flax"), TEXT("надёргать льна"), F::WildField, TEXT("Farming"), 0.2f, 45.0f, 0.25f, R::Flax, 3.0f);
        W.FromDay = 190; W.ToDay = 250;
        T.Add(W);

        W = Work(TEXT("v_water"), TEXT("набрать воды"), F::Well, TEXT(""), 0.0f, 8.0f, 0.1f, R::Water, 4.0f);
        T.Add(W);
        W.Id = TEXT("v_spring"); W.Station = static_cast<uint8>(F::Spring);
        T.Add(W);

        W = Work(TEXT("v_stone"), TEXT("наломать камня"), F::StonePile, TEXT("Masonry"), 0.35f, 60.0f, 0.55f, R::Stone, 2.0f);
        W.Tool = R::Hammer;
        T.Add(W);

        W = Work(TEXT("v_ore"), TEXT("накопать болотной руды"), F::ClayPit, TEXT("Smithing"), 0.3f, 60.0f, 0.5f, R::IronOre, 2.0f);
        W.Tool = R::Shovel;
        T.Add(W);

        W = Work(TEXT("v_clay"), TEXT("накопать глины"), F::ClayPit, TEXT("Pottery"), 0.15f, 40.0f, 0.4f, R::Clay, 3.0f);
        T.Add(W);

        W = Work(TEXT("v_charcoal"), TEXT("выжечь уголь"), F::ClayPit, TEXT("Smithing"), 0.4f, 120.0f, 0.35f, R::Charcoal, 4.0f);
        W.Input = R::Wood; W.InputAmount = 2.0f;
        T.Add(W);

        W = Work(TEXT("v_smelt"), TEXT("выплавить железо в горне"), F::Forge, TEXT("Smithing"), 0.55f, 90.0f, 0.6f, R::Iron, 1.0f);
        W.Input = R::IronOre; W.InputAmount = 2.0f; W.Fuel = R::Charcoal; W.FuelAmount = 2.0f;
        T.Add(W);

        W = Work(TEXT("v_sickle"), TEXT("выковать серп"), F::Forge, TEXT("Smithing"), 0.5f, 60.0f, 0.55f, R::Sickle, 1.0f);
        W.Input = R::Iron; W.InputAmount = 1.0f; W.Fuel = R::Charcoal; W.FuelAmount = 1.0f; W.Tool = R::Hammer;
        T.Add(W);

        W = Work(TEXT("v_axe"), TEXT("выковать топор"), F::Forge, TEXT("Smithing"), 0.55f, 75.0f, 0.6f, R::Axe, 1.0f);
        W.Input = R::Iron; W.InputAmount = 2.0f; W.Fuel = R::Charcoal; W.FuelAmount = 1.0f; W.Tool = R::Hammer;
        T.Add(W);

        W = Work(TEXT("v_nails"), TEXT("наковать гвоздей"), F::Forge, TEXT("Smithing"), 0.35f, 45.0f, 0.45f, R::Tool, 4.0f);
        W.Input = R::Iron; W.InputAmount = 1.0f; W.Fuel = R::Charcoal; W.FuelAmount = 1.0f; W.Tool = R::Hammer;
        T.Add(W);

        W = Work(TEXT("v_needle"), TEXT("выковать иглы"), F::Forge, TEXT("Smithing"), 0.45f, 30.0f, 0.3f, R::Needle, 3.0f);
        W.Input = R::Iron; W.InputAmount = 1.0f; W.Tool = R::Hammer;
        T.Add(W);

        W = Work(TEXT("v_pots"), TEXT("слепить и обжечь горшки"), F::PotteryWheel, TEXT("Pottery"), 0.45f, 60.0f, 0.35f, R::Pot, 2.0f);
        W.Input = R::Clay; W.InputAmount = 2.0f; W.Fuel = R::Firewood; W.FuelAmount = 1.0f;
        T.Add(W);

        W = Work(TEXT("v_mill"), TEXT("смолоть зерно на жерновах"), F::Sawhorse, TEXT("Farming"), 0.15f, 30.0f, 0.4f, R::Flour, 2.0f);
        W.Input = R::Grain; W.InputAmount = 2.0f;
        T.Add(W);

        W = Work(TEXT("v_maize_flour"), TEXT("смолоть кукурузу"), F::Sawhorse, TEXT("Farming"), 0.2f, 30.0f, 0.4f, R::Flour, 2.0f);
        W.Input = R::Maize; W.InputAmount = 2.0f;
        T.Add(W);

        W = Work(TEXT("v_bread"), TEXT("испечь хлеб"), F::Stove, TEXT("Baking"), 0.35f, 60.0f, 0.3f, R::Bread, 3.0f);
        W.Input = R::Flour; W.InputAmount = 1.0f; W.Fuel = R::Firewood; W.FuelAmount = 0.5f; W.bHome = true;
        T.Add(W);

        W = Work(TEXT("v_shchi"), TEXT("сварить щи"), F::Stove, TEXT("Cooking"), 0.25f, 30.0f, 0.2f, R::CookedFood, 3.0f);
        W.Input = R::RawFood; W.InputAmount = 1.0f; W.Fuel = R::Firewood; W.FuelAmount = 0.3f; W.bHome = true;
        T.Add(W);

        W = Work(TEXT("v_kasha"), TEXT("сварить кашу"), F::Stove, TEXT("Cooking"), 0.2f, 30.0f, 0.2f, R::CookedFood, 3.0f);
        W.Input = R::Grain; W.InputAmount = 1.0f; W.Fuel = R::Firewood; W.FuelAmount = 0.3f; W.bHome = true;
        T.Add(W);

        W = Work(TEXT("v_ukha"), TEXT("сварить уху"), F::Stove, TEXT("Cooking"), 0.25f, 30.0f, 0.2f, R::CookedFood, 3.0f);
        W.Input = R::Fish; W.InputAmount = 1.0f; W.Fuel = R::Firewood; W.FuelAmount = 0.3f; W.bHome = true;
        T.Add(W);

        W = Work(TEXT("v_mushroom_soup"), TEXT("сварить грибную похлёбку"), F::Stove, TEXT("Cooking"), 0.25f, 30.0f, 0.2f, R::CookedFood, 2.0f);
        W.Input = R::Mushroom; W.InputAmount = 1.0f; W.Fuel = R::Firewood; W.FuelAmount = 0.3f; W.bHome = true;
        T.Add(W);

        W = Work(TEXT("v_brew"), TEXT("сварить целебный отвар"), F::Stove, TEXT("Herbalism"), 0.4f, 30.0f, 0.15f, R::Medicine, 1.0f);
        W.Input = R::Herb; W.InputAmount = 2.0f; W.Fuel = R::Firewood; W.FuelAmount = 0.2f; W.bHome = true;
        T.Add(W);

        W = Work(TEXT("v_spin"), TEXT("спрясть нить"), F::Loom, TEXT("Weaving"), 0.3f, 45.0f, 0.2f, R::Thread, 2.0f);
        W.Input = R::Flax; W.InputAmount = 2.0f; W.bHome = true;
        T.Add(W);

        W = Work(TEXT("v_weave"), TEXT("соткать полотно"), F::Loom, TEXT("Weaving"), 0.45f, 90.0f, 0.3f, R::Cloth, 1.0f);
        W.Input = R::Thread; W.InputAmount = 2.0f; W.bHome = true;
        T.Add(W);

        W = Work(TEXT("v_shirt"), TEXT("сшить рубаху"), F::Table, TEXT("Sewing"), 0.4f, 90.0f, 0.2f, R::Shirt, 1.0f);
        W.Input = R::Cloth; W.InputAmount = 1.0f; W.Fuel = R::Thread; W.FuelAmount = 1.0f; W.Tool = R::Needle; W.bHome = true;
        T.Add(W);

        W = Work(TEXT("v_planks"), TEXT("вытесать доски"), F::Workbench, TEXT("Carpentry"), 0.35f, 60.0f, 0.45f, R::Plank, 3.0f);
        W.Input = R::Wood; W.InputAmount = 1.0f; W.Tool = R::Axe;
        T.Add(W);

        W = Work(TEXT("v_rod"), TEXT("вырезать удочку"), F::Workbench, TEXT("Carpentry"), 0.2f, 30.0f, 0.2f, R::Rod, 1.0f);
        W.Input = R::Wood; W.InputAmount = 1.0f;
        T.Add(W);

        W = Work(TEXT("v_harvest"), TEXT("жать хлеб серпом"), F::GardenBed, TEXT("Farming"), 0.25f, 60.0f, 0.5f, R::Grain, 2.0f);
        W.Tool = R::Sickle; W.FromDay = 195; W.ToDay = 265;
        T.Add(W);

        W = Work(TEXT("v_sow"), TEXT("посеять озимую рожь"), F::GardenBed, TEXT("Farming"), 0.2f, 45.0f, 0.4f, R::None, 0.0f);
        W.Input = R::Grain; W.InputAmount = 1.0f; W.FromDay = 215; W.ToDay = 262;
        T.Add(W);

        W = Work(TEXT("v_sow_spring"), TEXT("посеять яровую пшеницу"), F::GardenBed, TEXT("Farming"), 0.2f, 45.0f, 0.4f, R::None, 0.0f);
        W.Input = R::Grain; W.InputAmount = 1.0f; W.FromDay = 95; W.ToDay = 150;
        T.Add(W);

        W = Work(TEXT("v_sow_maize"), TEXT("посеять заморскую кукурузу"), F::GardenBed, TEXT("Farming"), 0.6f, 45.0f, 0.4f, R::None, 0.0f);
        W.Input = R::Maize; W.InputAmount = 1.0f; W.FromDay = 125; W.ToDay = 165;
        T.Add(W);

        // --- Огород: весной сажают, летом и осенью снимают ---
        W = Work(TEXT("v_sow_turnip"), TEXT("посеять репу"), F::GardenBed, TEXT("Farming"), 0.2f, 40.0f, 0.35f, R::None, 0.0f);
        W.Input = R::Seed; W.InputAmount = 1.0f; W.FromDay = 100; W.ToDay = 150; T.Add(W);
        W = Work(TEXT("v_pull_turnip"), TEXT("вытянуть репу"), F::GardenBed, TEXT("Farming"), 0.15f, 50.0f, 0.4f, R::Turnip, 4.0f);
        W.FromDay = 190; W.ToDay = 285; T.Add(W);
        W = Work(TEXT("v_sow_cabbage"), TEXT("посадить капусту"), F::GardenBed, TEXT("Farming"), 0.25f, 40.0f, 0.35f, R::None, 0.0f);
        W.Input = R::Seed; W.InputAmount = 1.0f; W.FromDay = 105; W.ToDay = 150; T.Add(W);
        W = Work(TEXT("v_pull_cabbage"), TEXT("срезать капусту"), F::GardenBed, TEXT("Farming"), 0.2f, 50.0f, 0.4f, R::Cabbage, 3.0f);
        W.Tool = R::Sickle; W.FromDay = 200; W.ToDay = 290; T.Add(W);
        W = Work(TEXT("v_sow_root"), TEXT("посеять морковь и свёклу"), F::GardenBed, TEXT("Farming"), 0.2f, 40.0f, 0.35f, R::None, 0.0f);
        W.Input = R::Seed; W.InputAmount = 1.0f; W.FromDay = 100; W.ToDay = 155; T.Add(W);
        W = Work(TEXT("v_pull_root"), TEXT("надёргать моркови"), F::GardenBed, TEXT("Farming"), 0.15f, 50.0f, 0.4f, R::Carrot, 3.0f);
        W.FromDay = 190; W.ToDay = 290; T.Add(W);
        W = Work(TEXT("v_pull_beet"), TEXT("выкопать свёклу"), F::GardenBed, TEXT("Farming"), 0.15f, 50.0f, 0.4f, R::Beet, 3.0f);
        W.Tool = R::Shovel; W.FromDay = 200; W.ToDay = 295; T.Add(W);
        W = Work(TEXT("v_sow_onion"), TEXT("посадить лук и чеснок"), F::GardenBed, TEXT("Farming"), 0.2f, 35.0f, 0.3f, R::None, 0.0f);
        W.Input = R::Seed; W.InputAmount = 1.0f; W.FromDay = 95; W.ToDay = 160; T.Add(W);
        W = Work(TEXT("v_pull_onion"), TEXT("выдернуть лук"), F::GardenBed, TEXT("Farming"), 0.1f, 40.0f, 0.3f, R::Onion, 4.0f);
        W.FromDay = 195; W.ToDay = 290; T.Add(W);
        W = Work(TEXT("v_pull_garlic"), TEXT("выкопать чеснок"), F::GardenBed, TEXT("Farming"), 0.1f, 40.0f, 0.3f, R::Garlic, 3.0f);
        W.Tool = R::Shovel; W.FromDay = 195; W.ToDay = 290; T.Add(W);
        W = Work(TEXT("v_sow_melon"), TEXT("посеять бахчу: арбузы, дыни, тыквы"), F::GardenBed, TEXT("Farming"), 0.45f, 50.0f, 0.35f, R::None, 0.0f);
        W.Input = R::Seed; W.InputAmount = 1.0f; W.FromDay = 110; W.ToDay = 150; T.Add(W);
        W = Work(TEXT("v_pull_watermelon"), TEXT("снять арбузы с бахчи"), F::GardenBed, TEXT("Farming"), 0.3f, 45.0f, 0.35f, R::Watermelon, 2.0f);
        W.FromDay = 215; W.ToDay = 270; T.Add(W);
        W = Work(TEXT("v_pull_melon"), TEXT("снять дыни с бахчи"), F::GardenBed, TEXT("Farming"), 0.3f, 45.0f, 0.35f, R::Melon, 2.0f);
        W.FromDay = 215; W.ToDay = 270; T.Add(W);
        W = Work(TEXT("v_pull_pumpkin"), TEXT("снять тыквы"), F::GardenBed, TEXT("Farming"), 0.25f, 45.0f, 0.35f, R::Pumpkin, 2.0f);
        W.FromDay = 225; W.ToDay = 295; T.Add(W);
        W = Work(TEXT("v_pull_cucumber"), TEXT("набрать огурцов"), F::GardenBed, TEXT("Farming"), 0.15f, 40.0f, 0.3f, R::Cucumber, 4.0f);
        W.FromDay = 185; W.ToDay = 265; T.Add(W);
        W = Work(TEXT("v_dig_potato"), TEXT("выкопать картофель"), F::GardenBed, TEXT("Farming"), 0.3f, 70.0f, 0.5f, R::Potato, 5.0f);
        W.Tool = R::Shovel; W.FromDay = 215; W.ToDay = 295; T.Add(W);
        W = Work(TEXT("v_sunflower"), TEXT("срезать подсолнухи"), F::GardenBed, TEXT("Farming"), 0.2f, 40.0f, 0.3f, R::Sunflower, 3.0f);
        W.Tool = R::Sickle; W.FromDay = 210; W.ToDay = 285; T.Add(W);

        // --- Сад и лес: плоды, лоза, волокно, красители ---
        W = Work(TEXT("v_apples"), TEXT("собрать яблоки"), F::Tree, TEXT("Farming"), 0.15f, 40.0f, 0.25f, R::Apple, 4.0f);
        W.FromDay = 210; W.ToDay = 280; T.Add(W);
        W = Work(TEXT("v_pears"), TEXT("собрать груши"), F::Tree, TEXT("Farming"), 0.15f, 40.0f, 0.25f, R::Pear, 3.0f);
        W.FromDay = 215; W.ToDay = 285; T.Add(W);
        W = Work(TEXT("v_plums"), TEXT("собрать сливы"), F::Tree, TEXT("Farming"), 0.15f, 35.0f, 0.25f, R::Plum, 4.0f);
        W.FromDay = 205; W.ToDay = 270; T.Add(W);
        W = Work(TEXT("v_cherries"), TEXT("собрать вишню"), F::Bush, TEXT("Farming"), 0.15f, 35.0f, 0.25f, R::Cherry, 3.0f);
        W.FromDay = 185; W.ToDay = 240; T.Add(W);
        W = Work(TEXT("v_grapes"), TEXT("снять гроздья винограда"), F::Bush, TEXT("Farming"), 0.25f, 40.0f, 0.25f, R::Grape, 3.0f);
        W.FromDay = 235; W.ToDay = 290; T.Add(W);
        W = Work(TEXT("v_press"), TEXT("отжать виноградное сусло"), F::Barrel, TEXT("Cooking"), 0.45f, 90.0f, 0.3f, R::GrapeMust, 2.0f);
        W.Input = R::Grape; W.InputAmount = 4.0f; W.FromDay = 235; W.ToDay = 310; T.Add(W);
        W = Work(TEXT("v_wine"), TEXT("выдержать сусло и слить вино"), F::Barrel, TEXT("Cooking"), 0.65f, 240.0f, 0.25f, R::Wine, 1.0f);
        W.Input = R::GrapeMust; W.InputAmount = 2.0f; W.FromDay = 235; W.ToDay = 330; T.Add(W);
        W = Work(TEXT("v_nuts"), TEXT("набить орехов"), F::Tree, TEXT("Herbalism"), 0.2f, 40.0f, 0.3f, R::Nut, 3.0f);
        W.FromDay = 240; W.ToDay = 300; T.Add(W);
        W = Work(TEXT("v_acorns"), TEXT("набрать жёлудей"), F::Tree, TEXT("Herbalism"), 0.1f, 30.0f, 0.2f, R::Acorn, 4.0f);
        W.FromDay = 250; W.ToDay = 310; T.Add(W);
        W = Work(TEXT("v_cones"), TEXT("насобирать шишек"), F::Tree, TEXT("Woodcutting"), 0.05f, 25.0f, 0.2f, R::Cone, 4.0f);
        T.Add(W);
        W = Work(TEXT("v_birchbark"), TEXT("снять бересту"), F::Tree, TEXT("Woodcutting"), 0.25f, 40.0f, 0.25f, R::BirchBark, 2.0f);
        W.Tool = R::Axe; W.FromDay = 120; W.ToDay = 200; T.Add(W);
        W = Work(TEXT("v_brushwood"), TEXT("навязать хвороста"), F::Tree, TEXT("Woodcutting"), 0.05f, 25.0f, 0.25f, R::Brushwood, 4.0f);
        T.Add(W);
        W = Work(TEXT("v_hemp"), TEXT("надёргать конопли"), F::WildField, TEXT("Farming"), 0.2f, 45.0f, 0.3f, R::Hemp, 3.0f);
        W.FromDay = 190; W.ToDay = 250; T.Add(W);
        W = Work(TEXT("v_hay"), TEXT("накосить сена"), F::WildField, TEXT("Farming"), 0.25f, 60.0f, 0.5f, R::Hay, 5.0f);
        W.Tool = R::Sickle; W.FromDay = 165; W.ToDay = 235; T.Add(W);
        W = Work(TEXT("v_hops"), TEXT("нарвать хмеля"), F::Bush, TEXT("Herbalism"), 0.2f, 35.0f, 0.2f, R::Hops, 2.0f);
        W.FromDay = 225; W.ToDay = 280; T.Add(W);
        W = Work(TEXT("v_vine"), TEXT("нарезать лозы"), F::Bush, TEXT("Woodcutting"), 0.2f, 35.0f, 0.25f, R::Vine, 3.0f);
        W.FromDay = 100; W.ToDay = 180; T.Add(W);
        W = Work(TEXT("v_madder"), TEXT("выкопать корень марены"), F::WildField, TEXT("Herbalism"), 0.35f, 50.0f, 0.4f, R::Madder, 2.0f);
        W.Tool = R::Shovel; W.FromDay = 220; W.ToDay = 290; T.Add(W);
        W = Work(TEXT("v_woad"), TEXT("надёргать вайды"), F::WildField, TEXT("Herbalism"), 0.25f, 40.0f, 0.3f, R::Woad, 2.0f);
        W.FromDay = 180; W.ToDay = 240; T.Add(W);

        // --- Охота: мех, перья, рога, сало. Силки — круглый год, пушнина — зимой ---
        W = Work(TEXT("v_hunt"), TEXT("поставить силки: дичь и мясо"), F::WildField, TEXT("Hunting"), 0.4f, 120.0f, 0.4f, R::Meat, 2.0f);
        T.Add(W);
        W = Work(TEXT("v_hunt_feather"), TEXT("собрать перья с дичи"), F::WildField, TEXT("Hunting"), 0.3f, 60.0f, 0.25f, R::Feather, 3.0f);
        T.Add(W);
        W = Work(TEXT("v_hunt_fur"), TEXT("взять зверя: мех и рога"), F::WildField, TEXT("Hunting"), 0.55f, 150.0f, 0.5f, R::Fur, 2.0f);
        W.FromDay = 280; W.ToDay = 60; T.Add(W);
        W = Work(TEXT("v_hunt_antler"), TEXT("подобрать сброшенные рога"), F::WildField, TEXT("Hunting"), 0.2f, 60.0f, 0.25f, R::Antler, 1.0f);
        W.FromDay = 90; W.ToDay = 180; T.Add(W);
        W = Work(TEXT("v_hunt_tallow"), TEXT("снять сало с добычи"), F::WildField, TEXT("Hunting"), 0.45f, 90.0f, 0.35f, R::Tallow, 2.0f);
        W.Input = R::Meat; W.InputAmount = 1.0f; T.Add(W);

        // --- Каменоломня и глинокопня: гравий, грязь, мрамор, самоцветы, цинк ---
        W = Work(TEXT("v_gravel"), TEXT("намыть гравия"), F::ClayPit, TEXT("Masonry"), 0.25f, 50.0f, 0.45f, R::Gravel, 3.0f);
        W.Tool = R::Shovel; T.Add(W);
        W = Work(TEXT("v_mud"), TEXT("набрать грязи"), F::ClayPit, TEXT("Masonry"), 0.05f, 20.0f, 0.3f, R::Mud, 3.0f);
        T.Add(W);
        W = Work(TEXT("v_marble"), TEXT("отбить мрамор"), F::StonePile, TEXT("Masonry"), 0.5f, 90.0f, 0.6f, R::Marble, 1.0f);
        W.Tool = R::Hammer; T.Add(W);
        W = Work(TEXT("v_gems"), TEXT("промыть породу на самоцветы"), F::StonePile, TEXT("Masonry"), 0.65f, 120.0f, 0.4f, R::Gem, 1.0f);
        W.Input = R::Gravel; W.InputAmount = 2.0f; T.Add(W);
        W = Work(TEXT("v_zinc_ore"), TEXT("накопать цинковой руды"), F::ClayPit, TEXT("Smithing"), 0.4f, 70.0f, 0.5f, R::ZincOre, 2.0f);
        W.Tool = R::Shovel; T.Add(W);
        W = Work(TEXT("v_pearl"), TEXT("нырнуть за жемчугом"), F::FishingSpot, TEXT("Fishing"), 0.7f, 120.0f, 0.5f, R::Pearl, 1.0f);
        W.FromDay = 140; W.ToDay = 280; T.Add(W);

        return T;
    }

    void SetMastery(ACompleteHumanNPC* Person, FName Skill, float Level, float T)
    {
        if (!Person || !Person->MemoryComponent || Skill.IsNone() || Level <= 0.0f)
        {
            return;
        }
        FBelief B;
        B.Subject = Skill;
        B.Predicate = TEXT("HowTo");
        B.Value = 1.0f;
        B.Confidence = FMath::Clamp(Level, 0.0f, 1.0f);
        B.LearnedAt = T;
        B.bVerified = true;
        B.Text = UMindComponent::MasteryLabel(Skill);
        Person->MemoryComponent->Learn(B, 1.0f, 1.0f, T);
    }

    bool IsStorage(EFurnitureType Type)
    {
        switch (Type)
        {
        case EFurnitureType::Shed:
        case EFurnitureType::Barrel:
        case EFurnitureType::Wardrobe:
        case EFurnitureType::Stove:
        case EFurnitureType::Table:
        case EFurnitureType::Counter:
        case EFurnitureType::Fridge:
            return true;
        default:
            return false;
        }
    }
}

FString FVillage::CoinWord(int32 Amount)
{
    const int32 N = FMath::Abs(Amount) % 100;
    const int32 Last = N % 10;
    if (N >= 11 && N <= 14)
    {
        return TEXT("звонцов");
    }
    if (Last == 1)
    {
        return TEXT("звонец");
    }
    if (Last >= 2 && Last <= 4)
    {
        return TEXT("звонца");
    }
    return TEXT("звонцов");
}

FString FVillage::Coins(float Amount)
{
    const int32 Whole = FMath::RoundToInt(Amount);
    return FString::Printf(TEXT("%d %s"), Whole, *CoinWord(Whole));
}

float FVillage::PriceOf(EResourceKind Kind)
{
    using R = EResourceKind;
    switch (Kind)
    {
    case R::Bread: case R::CookedFood: case R::RawFood: case R::Grain: case R::Maize:
    case R::Berry: case R::Mushroom: case R::Egg: case R::Milk: case R::Firewood:
    case R::Herb: case R::Flax: case R::Stone: case R::Granite: case R::Limestone:
    case R::Brick: case R::Charcoal: case R::Wool:
        return 1.0f;
    case R::Flour: case R::Fish: case R::Salt: case R::Thread: case R::Plank: case R::Pot:
    case R::IronOre: case R::Needle: case R::Rod: case R::Lime:
        return 2.0f;
    case R::Wood: case R::Meat: case R::Medicine: case R::Tool: case R::Wine: case R::Mead:
        return 3.0f;
    case R::Turnip: case R::Cabbage: case R::Carrot: case R::Onion: case R::Garlic:
    case R::Beet: case R::Cucumber: case R::Potato: case R::Wheat: case R::Rye:
    case R::Barley: case R::Oats: case R::Apple: case R::Pear: case R::Plum:
    case R::Cherry: case R::Hops: case R::Madder: case R::Woad: case R::Hay:
        return 1.0f;
    case R::Pumpkin: case R::Melon: case R::Watermelon: case R::Grape: case R::Nut:
    case R::Feather: case R::Cheese: case R::CottageCheese: case R::Butter:
    case R::SmokedFish: case R::SmokedMeat: case R::DriedFish: case R::Kvass:
    case R::Beer: case R::Vinegar: case R::Candle: case R::OliveOil: case R::Basket:
    case R::BirchBark: case R::Acorn: case R::Cone: case R::Brushwood:
        return 2.0f;
    case R::Fur: case R::Antler: case R::Tallow: case R::Gem:
    case R::Brass: case R::Marble: case R::Slate: case R::Obsidian:
    case R::Mortar: case R::CharcoalPencil: case R::Dye:
        return 6.0f;
    case R::Diamond: case R::Ruby: case R::Emerald: case R::Sapphire:
    case R::Pearl: case R::Electrum:
        return 40.0f;
    case R::Honey: case R::Hide:
        return 4.0f;
    case R::Cloth: case R::Leather:
        return 6.0f;
    case R::Iron:
        return 8.0f;
    case R::Sickle:
        return 12.0f;
    case R::Shirt:
        return 14.0f;
    case R::Shovel: case R::Hammer:
        return 15.0f;
    case R::Saw:
        return 20.0f;
    case R::Axe:
        return 25.0f;
    case R::Clay: case R::Straw: case R::Sand:
        return 0.5f;
    case R::Water: case R::None:
        return 0.0f;
    default:
        return 1.0f;
    }
}

bool FVillage::IsFood(EResourceKind Kind)
{
    using R = EResourceKind;
    switch (Kind)
    {
    case R::Bread: case R::CookedFood: case R::RawFood: case R::Grain: case R::Maize: case R::Flour:
    case R::Berry: case R::Mushroom: case R::Egg: case R::Milk: case R::Fish: case R::Meat: case R::Honey:
    case R::Turnip: case R::Cabbage: case R::Carrot: case R::Onion: case R::Garlic:
    case R::Beet: case R::Cucumber: case R::Pumpkin: case R::Melon: case R::Watermelon:
    case R::Potato: case R::Wheat: case R::Rye: case R::Barley: case R::Oats:
    case R::Apple: case R::Pear: case R::Plum: case R::Cherry: case R::Grape:
    case R::Nut: case R::Acorn: case R::Sunflower: case R::Tallow:
    case R::CurdledMilk: case R::Butter: case R::Cheese: case R::CottageCheese:
    case R::SmokedFish: case R::SmokedMeat: case R::DriedFish:
    case R::Kvass: case R::Beer: case R::Wine: case R::Mead:
        return true;
    default:
        return false;
    }
}

bool FVillage::IsReadyFood(EResourceKind Kind)
{
    return Kind == EResourceKind::Bread || Kind == EResourceKind::CookedFood || Kind == EResourceKind::Berry
        || Kind == EResourceKind::Milk || Kind == EResourceKind::Honey || Kind == EResourceKind::Egg
        || Kind == EResourceKind::Apple || Kind == EResourceKind::Pear
        || Kind == EResourceKind::Plum || Kind == EResourceKind::Cherry
        || Kind == EResourceKind::Grape || Kind == EResourceKind::Nut
        || Kind == EResourceKind::Turnip || Kind == EResourceKind::Carrot
        || Kind == EResourceKind::Cucumber || Kind == EResourceKind::Cheese
        || Kind == EResourceKind::CottageCheese || Kind == EResourceKind::Butter
        || Kind == EResourceKind::SmokedFish || Kind == EResourceKind::SmokedMeat
        || Kind == EResourceKind::DriedFish || Kind == EResourceKind::Kvass
        || Kind == EResourceKind::Beer || Kind == EResourceKind::Wine
        || Kind == EResourceKind::Mead || Kind == EResourceKind::Melon
        || Kind == EResourceKind::Watermelon;
}

bool FVillage::IsTool(EResourceKind Kind)
{
    using R = EResourceKind;
    return Kind == R::Axe || Kind == R::Sickle || Kind == R::Rod || Kind == R::Needle || Kind == R::Hammer
        || Kind == R::Shovel || Kind == R::Saw;
}

bool FVillage::IsSellable(EResourceKind Kind)
{
    return Kind != EResourceKind::None && Kind != EResourceKind::Water && Kind != EResourceKind::Coin && PriceOf(Kind) > 0.0f;
}

float FVillage::Portions(EResourceKind Kind)
{
    using R = EResourceKind;
    switch (Kind)
    {
    case R::Bread: case R::CookedFood:
        return 1.0f;
    case R::RawFood: case R::Grain: case R::Maize: case R::Flour: case R::Fish: case R::Meat:
    case R::Wheat: case R::Rye: case R::Barley: case R::Oats:
    case R::Turnip: case R::Cabbage: case R::Carrot: case R::Onion: case R::Garlic:
    case R::Beet: case R::Cucumber: case R::Pumpkin: case R::Melon: case R::Watermelon:
    case R::Potato: case R::Cheese: case R::CottageCheese: case R::Butter:
    case R::CurdledMilk: case R::SmokedFish: case R::SmokedMeat: case R::DriedFish:
    case R::Kvass: case R::Beer: case R::Wine: case R::Mead:
        return 3.0f;
    case R::Apple: case R::Pear: case R::Plum: case R::Cherry:
    case R::Grape: case R::Nut: case R::Acorn: case R::Sunflower:
        return 0.5f;
    case R::Berry: case R::Mushroom: case R::Milk: case R::Honey:
        return 0.5f;
    case R::Egg:
        return 0.3f;
    default:
        return 0.0f;
    }
}

const TArray<FVillageTrade>& FVillage::Trades()
{
    static const TArray<FVillageTrade> Table(TradeTable, UE_ARRAY_COUNT(TradeTable));
    return Table;
}

const FVillageTrade* FVillage::TradeOf(FName Skill)
{
    for (const FVillageTrade& T : Trades())
    {
        if (T.Skill == Skill)
        {
            return &T;
        }
    }
    return nullptr;
}

FString FVillage::TitleOf(EVillageRole Role, FName Trade, bool bFemale, float Age)
{
    switch (Role)
    {
    case EVillageRole::King:        return TEXT("король");
    case EVillageRole::Queen:       return TEXT("королева");
    case EVillageRole::Prince:      return TEXT("королевич");
    case EVillageRole::Princess:    return TEXT("королевна");
    case EVillageRole::QueenMother: return TEXT("королева-мать");
    case EVillageRole::Steward:     return TEXT("ключник");
    case EVillageRole::Cook:        return TEXT("стряпуха");
    case EVillageRole::Guard:       return TEXT("стражник");
    case EVillageRole::Maid:        return TEXT("служанка");
    case EVillageRole::Groom:       return TEXT("работник при дворе");
    default:
        break;
    }
    if (const FVillageTrade* T = TradeOf(Trade))
    {
        return bFemale ? T->TitleFemale : T->Title;
    }
    if (Age < 1.0f)
    {
        return TEXT("младенец");
    }
    if (Age < 14.0f)
    {
        return bFemale ? TEXT("девочка") : TEXT("мальчик");
    }
    return bFemale ? TEXT("крестьянка") : TEXT("крестьянин");
}

const TArray<FVillageWork>& FVillage::Works()
{
    static const TArray<FVillageWork> Table = BuildWorks();
    return Table;
}

const FVillageWork* FVillage::FindWork(FName Id)
{
    for (const FVillageWork& W : Works())
    {
        if (W.Id == Id)
        {
            return &W;
        }
    }
    return nullptr;
}

bool FVillage::InSeason(const FVillageWork& Work, int32 Day)
{
    if (Work.FromDay <= Work.ToDay)
    {
        return Day >= Work.FromDay && Day <= Work.ToDay;
    }
    return Day >= Work.FromDay || Day <= Work.ToDay;
}

void FVillage::AppendWorks(TArray<FCraft>& Out)
{
    for (const FVillageWork& W : Works())
    {
        bool bKnown = false;
        for (const FCraft& C : Out)
        {
            bKnown |= C.Id == W.Id;
        }
        if (bKnown)
        {
            continue;
        }
        FCraft C;
        C.Id = W.Id;
        C.Label = W.Label;
        C.Skill = W.Skill;
        C.Difficulty = W.Difficulty;
        C.Duration = W.Duration;
        C.Effort = W.Effort;
        C.Station = static_cast<EFurnitureType>(W.Station);
        if (W.Input != EResourceKind::None)
        {
            FCraftPart P;
            P.Kind = W.Input;
            P.Amount = W.InputAmount;
            C.Inputs.Add(P);
        }
        if (W.Fuel != EResourceKind::None)
        {
            FCraftPart P;
            P.Kind = W.Fuel;
            P.Amount = W.FuelAmount;
            C.Inputs.Add(P);
        }
        C.Output = W.Output;
        C.OutputAmount = W.OutputAmount;
        C.Tool = W.Tool;
        C.bAnywhere = false;
        Out.Add(C);
    }
}

void FVillage::Plan(int32 Count, uint32 Seed, TArray<FPersonPlan>& Out, int32& OutHouseholds)
{
    Out.Reset();
    FRandomStream Rng(static_cast<int32>(Seed));
    const int32 Base = UE_ARRAY_COUNT(Folk);
    for (int32 I = 0; I < Base && Out.Num() < Count; ++I)
    {
        const FFolk& F = Folk[I];
        FPersonPlan P;
        P.FirstName = F.First;
        P.LastName = F.Last;
        P.Age = F.Age;
        P.bFemale = F.bFemale;
        P.bMasterTeacher = F.bMasterTeacher;
        P.Role = F.Role;
        P.Trade = FName(F.Trade);
        P.Household = F.House;
        P.Spouse = F.Spouse;
        P.Mother = F.Mother;
        P.Father = F.Father;
        P.Money = F.Money;
        Out.Add(P);
    }
    int32 House = 5;
    int32 Family = 0;
    while (Out.Num() < Count)
    {
        const FString Stem = ExtraFamilies[Family % UE_ARRAY_COUNT(ExtraFamilies)];
        const int32 Husband = Out.Num();
        FPersonPlan Man;
        Man.FirstName = ExtraMen[Rng.RandRange(0, UE_ARRAY_COUNT(ExtraMen) - 1)];
        Man.LastName = Stem;
        Man.Age = Rng.FRandRange(26.0f, 50.0f);
        Man.bFemale = false;
        Man.Household = House;
        Man.Money = Rng.FRandRange(6.0f, 16.0f);
        Out.Add(Man);
        if (Out.Num() < Count)
        {
            FPersonPlan Woman;
            Woman.FirstName = ExtraWomen[Rng.RandRange(0, UE_ARRAY_COUNT(ExtraWomen) - 1)];
            Woman.LastName = Stem + TEXT("а");
            Woman.Age = FMath::Max(20.0f, Out[Husband].Age - Rng.FRandRange(0.0f, 6.0f));
            Woman.bFemale = true;
            Woman.Household = House;
            Woman.Spouse = Husband;
            Woman.Money = Rng.FRandRange(3.0f, 10.0f);
            Out[Husband].Spouse = Out.Num();
            Out.Add(Woman);
        }
        const int32 Kids = Rng.RandRange(1, 3);
        for (int32 K = 0; K < Kids && Out.Num() < Count; ++K)
        {
            FPersonPlan Child;
            Child.bFemale = Rng.FRand() < 0.5f;
            Child.FirstName = Child.bFemale ? ExtraWomen[Rng.RandRange(0, UE_ARRAY_COUNT(ExtraWomen) - 1)]
                                            : ExtraMen[Rng.RandRange(0, UE_ARRAY_COUNT(ExtraMen) - 1)];
            Child.LastName = Child.bFemale ? Stem + TEXT("а") : Stem;
            Child.Age = Rng.FRandRange(2.0f, 15.0f);
            Child.Role = Child.Age < 13.0f ? EVillageRole::Child : EVillageRole::Peasant;
            Child.Household = House;
            Child.Father = Husband;
            Child.Mother = Out[Husband].Spouse;
            Child.Money = 0.0f;
            Out.Add(Child);
        }
        ++House;
        ++Family;
    }
    for (FPersonPlan& P : Out)
    {
        if (!Out.IsValidIndex(P.Spouse)) P.Spouse = INDEX_NONE;
        if (!Out.IsValidIndex(P.Mother)) P.Mother = INDEX_NONE;
        if (!Out.IsValidIndex(P.Father)) P.Father = INDEX_NONE;
    }
    OutHouseholds = 0;
    for (const FPersonPlan& P : Out)
    {
        OutHouseholds = FMath::Max(OutHouseholds, P.Household + 1);
    }
}

void FVillage::Skills(ACompleteHumanNPC* Person, const FPersonPlan& Plan)
{
    UHumanWorldSubsystem* WorldMind = Person && Person->GetWorld() ? Person->GetWorld()->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    const float T = WorldMind ? WorldMind->WorldSeconds : 0.0f;
    const float Grown = Plan.Age >= 16.0f ? 1.0f : (Plan.Age >= 12.0f ? 0.6f : (Plan.Age >= 6.0f ? 0.25f : 0.0f));
    if (Grown <= 0.0f)
    {
        return;
    }
    const bool bF = Plan.bFemale;
    const bool bRoyal = Plan.Role >= EVillageRole::King && Plan.Role <= EVillageRole::QueenMother;
    TMap<FName, float> Level;
    for (const FVillageTrade& Known : Trades())
    {
        Level.Add(Known.Skill, 0.03f);
    }
    Level.Add(TEXT("Masonry"), 0.03f);
    Level.Add(TEXT("Cleaning"), bF ? 0.5f : 0.3f);
    Level.Add(TEXT("Strength"), bF ? 0.35f : 0.5f);
    Level.Add(TEXT("Conversation"), 0.5f);
    Level.Add(TEXT("Counting"), 0.15f);

    TMap<FName, float> FromBooks;
    if (!bRoyal)
    {
        FromBooks.Add(TEXT("VillageFarming"), bF ? 0.55f : 0.65f);
        FromBooks.Add(TEXT("VillageKitchen"), bF ? 0.65f : 0.3f);
        FromBooks.Add(TEXT("VillageWoods"), bF ? 0.2f : 0.6f);
        if (bF)
        {
            FromBooks.Add(TEXT("VillageBread"), 0.45f);
        }
    }

    for (TPair<FName, float>& L : Level)
    {
        L.Value *= Grown;
    }
    for (TPair<FName, float>& B : FromBooks)
    {
        B.Value *= Grown;
        if (const FTextbook* Book = FLibrary::Find(B.Key))
        {
            float& Have = Level.FindOrAdd(Book->Skill);
            Have = FMath::Max(Have, B.Value);
        }
    }

    auto Raise = [&Level](const TCHAR* Skill, float To)
    {
        float& Have = Level.FindOrAdd(FName(Skill));
        Have = FMath::Max(Have, To);
    };
    auto Lower = [&Level](const TCHAR* Skill, float To)
    {
        float& Have = Level.FindOrAdd(FName(Skill));
        Have = FMath::Min(Have, To);
    };

    switch (Plan.Role)
    {
    case EVillageRole::King:
        Raise(TEXT("Reading"), 0.7f); Raise(TEXT("Writing"), 0.6f); Raise(TEXT("Counting"), 0.7f); Raise(TEXT("Persuasion"), 0.65f);
        Raise(TEXT("Strength"), 0.6f); Lower(TEXT("Farming"), 0.1f); Lower(TEXT("Cooking"), 0.05f);
        break;
    case EVillageRole::Queen:
        Raise(TEXT("Reading"), 0.65f); Raise(TEXT("Writing"), 0.5f); Raise(TEXT("Sewing"), 0.7f); Raise(TEXT("Weaving"), 0.4f);
        Raise(TEXT("Empathy"), 0.6f); Lower(TEXT("Farming"), 0.05f);
        break;
    case EVillageRole::Prince:
        Raise(TEXT("Reading"), 0.55f); Raise(TEXT("Strength"), 0.75f); Raise(TEXT("Woodcutting"), 0.3f); Lower(TEXT("Farming"), 0.1f);
        break;
    case EVillageRole::Princess:
        Raise(TEXT("Reading"), 0.55f); Raise(TEXT("Sewing"), 0.55f); Raise(TEXT("Music"), 0.5f); Lower(TEXT("Farming"), 0.05f);
        break;
    case EVillageRole::QueenMother:
        Raise(TEXT("Reading"), 0.5f); Raise(TEXT("Herbalism"), 0.6f); Raise(TEXT("Sewing"), 0.6f); Lower(TEXT("Farming"), 0.05f);
        break;
    case EVillageRole::Steward:
        Raise(TEXT("Counting"), 0.9f); Raise(TEXT("Trade"), 0.6f); Raise(TEXT("Reading"), 0.5f); Raise(TEXT("Writing"), 0.4f);
        break;
    case EVillageRole::Cook:
        Raise(TEXT("Baking"), 0.8f);
        break;
    case EVillageRole::Guard:
        Raise(TEXT("Strength"), 0.9f);
        break;
    case EVillageRole::Maid:
        Raise(TEXT("Cleaning"), 0.9f); Raise(TEXT("Sewing"), 0.5f); Raise(TEXT("Cooking"), 0.5f);
        break;
    case EVillageRole::Groom:
        Raise(TEXT("Farming"), 0.6f); Raise(TEXT("Carpentry"), 0.4f);
        break;
    default:
        break;
    }

    if (IsLiterate(Plan))
    {
        Raise(TEXT("Reading"), bRoyal ? 0.7f : 0.5f);
    }

    if (!Plan.Trade.IsNone())
    {
        Raise(*Plan.Trade.ToString(), 0.95f);
        const FName Own = BookFor(Plan.Trade);
        if (!Own.IsNone())
        {
            FromBooks.FindOrAdd(Own) = 0.95f;
        }
        if (Plan.Trade == TEXT("Smithing"))
        {
            Raise(TEXT("Masonry"), 0.4f); Raise(TEXT("Strength"), 0.8f);
        }
        else if (Plan.Trade == TEXT("Weaving"))
        {
            Raise(TEXT("Sewing"), 0.6f);
        }
        else if (Plan.Trade == TEXT("Sewing"))
        {
            Raise(TEXT("Weaving"), 0.5f);
        }
        else if (Plan.Trade == TEXT("Farming"))
        {
            Raise(TEXT("Woodcutting"), 0.55f); Raise(TEXT("Carpentry"), 0.35f);
        }
        else if (Plan.Trade == TEXT("Carpentry"))
        {
            Raise(TEXT("Building"), 0.75f); Raise(TEXT("Woodcutting"), 0.6f);
        }
        else if (Plan.Trade == TEXT("Baking"))
        {
            Raise(TEXT("Cooking"), 0.7f);
        }
        else if (Plan.Trade == TEXT("Herbalism"))
        {
            Raise(TEXT("Medicine"), 0.7f);
        }
        else if (Plan.Trade == TEXT("Trade"))
        {
            Raise(TEXT("Counting"), 0.7f); Raise(TEXT("Persuasion"), 0.6f);
        }
        else if (Plan.Trade == TEXT("Pottery"))
        {
            Raise(TEXT("Masonry"), 0.3f);
        }
    }

    if (Plan.bMasterTeacher)
    {
        Raise(TEXT("Teaching"), 1.0f);
        Raise(TEXT("Craft"), 0.9f);
        Raise(TEXT("Farming"), 0.8f);
        Raise(TEXT("Cooking"), 0.8f);
        Raise(TEXT("Trade"), 0.85f);
        RememberBook(Person, TEXT("VillageProduction"), 1.0f, T);
        if (Person->Mind)
        {
            Person->Mind->PreloadKnowledge();
        }
    }

    for (const TPair<FName, float>& L : Level)
    {
        SetMastery(Person, L.Key, L.Value, T);
    }
    for (const TPair<FName, float>& B : FromBooks)
    {
        RememberBook(Person, B.Key, B.Value, T);
    }
}

void FVillage::Dress(ACompleteHumanNPC* Person, EVillageRole Role, FName Trade, float Age, bool bFemale, uint32 Seed)
{
    UWorld* World = Person ? Person->GetWorld() : nullptr;
    if (!World)
    {
        return;
    }
    FRandomStream Rng(static_cast<int32>(Seed));
    using R = EResourceKind;
    const FLinearColor Linen(1.05f, 1.05f, 1.0f);
    const FLinearColor Bleached(1.15f, 1.18f, 1.22f);
    const FLinearColor Red(0.95f, 0.12f, 0.1f);
    const FLinearColor Blue(0.2f, 0.32f, 0.8f);
    const FLinearColor Green(0.25f, 0.55f, 0.3f);
    const FLinearColor Brown(0.55f, 0.38f, 0.25f);
    const FLinearColor Grey(0.4f, 0.4f, 0.44f);
    const FLinearColor Sky(0.55f, 0.62f, 0.8f);
    const FLinearColor Yellow(1.1f, 0.85f, 0.3f);
    const FLinearColor Purple(0.5f, 0.15f, 0.58f);
    const FLinearColor Rose(1.1f, 0.55f, 0.65f);
    const FLinearColor Cherry(0.6f, 0.06f, 0.15f);
    const FLinearColor Plain(1.0f, 1.0f, 1.0f);
    const FLinearColor RedLeather(1.5f, 0.75f, 0.65f);
    auto Pick = [&Rng](std::initializer_list<FLinearColor> Colours)
    {
        const int32 Index = Rng.RandRange(0, static_cast<int32>(Colours.size()) - 1);
        return *(Colours.begin() + Index);
    };
    auto Put = [Person, World](EClothingKind Kind, R Stuff, const FLinearColor& Dye, float Length = 1.0f)
    {
        if (AClothingActor* Made = AClothingActor::Spawn(World, Kind, FString(), Person->GetActorLocation(), Stuff, Dye, Length))
        {
            if (!Person->Wear(Made))
            {
                Made->Destroy();
            }
        }
    };

    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(World))
    {
        const float Tan = Rng.FRandRange(-0.05f, 0.05f);
        const FLinearColor SkinTone(1.3f + Tan, 0.9f + Tan * 0.8f, 0.72f + Tan * 0.6f);
        if (UMaterialInterface* Skin = Matter->Look(EMatterLook::Bone, SkinTone))
        {
            if (Person->PoseBody)
            {
                for (int32 Slot = 0; Slot < Person->PoseBody->GetNumMaterials(); ++Slot)
                {
                    Person->PoseBody->SetMaterial(Slot, Skin);
                }
            }
        }
    }

    if (Age < 1.5f)
    {
        Put(EClothingKind::Swaddle, R::Cloth, Bleached);
        return;
    }
    if (Age < 7.0f)
    {
        Put(EClothingKind::Shirt, R::Cloth, Linen, 2.4f);
        if (bFemale && Rng.FRand() < 0.5f)
        {
            Put(EClothingKind::Kerchief, R::Cloth, Pick({ Red, Linen, Yellow }));
        }
        return;
    }

    switch (Role)
    {
    case EVillageRole::King:
        Put(EClothingKind::Shirt, R::Cloth, Bleached);
        Put(EClothingKind::Trousers, R::Wool, Blue);
        Put(EClothingKind::Coat, R::Wool, Red, 1.45f);
        Put(EClothingKind::Belt, R::Gold, Plain);
        Put(EClothingKind::Boots, R::Leather, RedLeather);
        Put(EClothingKind::Crown, R::Gold, Plain);
        return;
    case EVillageRole::Queen:
        Put(EClothingKind::Shirt, R::Cloth, Bleached);
        Put(EClothingKind::Dress, R::Wool, Purple);
        Put(EClothingKind::Belt, R::Gold, Plain);
        Put(EClothingKind::Boots, R::Leather, RedLeather);
        Put(EClothingKind::Crown, R::Gold, Plain);
        return;
    case EVillageRole::Prince:
        Put(EClothingKind::Shirt, R::Cloth, Bleached);
        Put(EClothingKind::Trousers, R::Wool, Grey);
        Put(EClothingKind::Coat, R::Wool, Green, 1.0f);
        Put(EClothingKind::Belt, R::Leather, Plain);
        Put(EClothingKind::Boots, R::Leather, Plain);
        Put(EClothingKind::Hat, R::Wool, Brown);
        return;
    case EVillageRole::Princess:
        Put(EClothingKind::Shirt, R::Cloth, Bleached);
        Put(EClothingKind::Dress, R::Cloth, Rose);
        Put(EClothingKind::Belt, R::Gold, Plain);
        Put(EClothingKind::Boots, R::Leather, RedLeather);
        Put(EClothingKind::Crown, R::Gold, Plain);
        return;
    case EVillageRole::QueenMother:
        Put(EClothingKind::Shirt, R::Cloth, Bleached);
        Put(EClothingKind::Dress, R::Wool, Cherry);
        Put(EClothingKind::Belt, R::Gold, Plain);
        Put(EClothingKind::Boots, R::Leather, Plain);
        Put(EClothingKind::Kerchief, R::Cloth, Bleached);
        return;
    case EVillageRole::Steward:
        Put(EClothingKind::Shirt, R::Cloth, Linen);
        Put(EClothingKind::Trousers, R::Wool, Grey);
        Put(EClothingKind::Coat, R::Wool, Brown, 1.0f);
        Put(EClothingKind::Belt, R::Cloth, Red);
        Put(EClothingKind::Boots, R::Leather, Plain);
        Put(EClothingKind::Hat, R::Wool, Grey);
        return;
    case EVillageRole::Guard:
        Put(EClothingKind::Shirt, R::Cloth, Linen);
        Put(EClothingKind::Trousers, R::Wool, Grey);
        Put(EClothingKind::Coat, R::Wool, Cherry, 0.8f);
        Put(EClothingKind::Belt, R::Leather, Plain);
        Put(EClothingKind::Boots, R::Leather, Plain);
        Put(EClothingKind::Helmet, R::Iron, Plain);
        return;
    case EVillageRole::Cook:
    case EVillageRole::Maid:
        Put(EClothingKind::Shirt, R::Cloth, Linen);
        Put(EClothingKind::Dress, R::Cloth, Role == EVillageRole::Cook ? Brown : Sky);
        Put(EClothingKind::Belt, R::Cloth, Red);
        Put(EClothingKind::Apron, R::Cloth, Bleached);
        Put(EClothingKind::BastShoes, R::Straw, Plain);
        Put(EClothingKind::Kerchief, R::Cloth, Role == EVillageRole::Cook ? Bleached : Blue);
        return;
    default:
        break;
    }

    const bool bChild = Age < 14.0f;
    if (bFemale)
    {
        Put(EClothingKind::Shirt, R::Cloth, Linen, bChild ? 2.8f : 1.0f);
        if (!bChild)
        {
            Put(EClothingKind::Dress, R::Cloth, Pick({ Red, Blue, Green, Brown, Cherry }));
        }
        Put(EClothingKind::Belt, R::Cloth, Pick({ Red, Yellow, Green }));
        if (!bChild || Rng.FRand() < 0.6f)
        {
            Put(EClothingKind::BastShoes, R::Straw, Plain);
        }
        if (!bChild || Rng.FRand() < 0.4f)
        {
            Put(EClothingKind::Kerchief, R::Cloth, Pick({ Red, Linen, Blue, Yellow }));
        }
        if (Trade == TEXT("Baking") || Trade == TEXT("Cooking") || Trade == TEXT("Herbalism"))
        {
            Put(EClothingKind::Apron, R::Cloth, Bleached);
        }
        return;
    }

    Put(EClothingKind::Shirt, R::Cloth, Rng.FRand() < 0.7f ? Linen : Pick({ Blue, Red, Sky }), bChild ? 1.3f : 1.0f);
    if (!bChild || Rng.FRand() < 0.5f)
    {
        Put(EClothingKind::Trousers, R::Cloth, Pick({ Sky, Grey, Linen, Brown }));
    }
    if (Trade == TEXT("Trade"))
    {
        Put(EClothingKind::Coat, R::Wool, Blue, 0.9f);
    }
    Put(EClothingKind::Belt, R::Cloth, Pick({ Red, Yellow, Green }));
    if (Trade == TEXT("Trade"))
    {
        Put(EClothingKind::Boots, R::Leather, Plain);
    }
    else if (!bChild || Rng.FRand() < 0.5f)
    {
        Put(EClothingKind::BastShoes, R::Straw, Plain);
    }
    if (Trade == TEXT("Smithing"))
    {
        Put(EClothingKind::Apron, R::Leather, Plain);
    }
    if (!bChild && Rng.FRand() < 0.45f)
    {
        Put(EClothingKind::Hat, R::Wool, Pick({ Grey, Brown }));
    }
}

FName FVillage::BookFor(FName Skill)
{
    static const TMap<FName, FName> Shelf = {
        { TEXT("Fishing"), TEXT("VillageFishing") }, { TEXT("Farming"), TEXT("VillageFarming") },
        { TEXT("Woodcutting"), TEXT("VillageWoods") }, { TEXT("Cooking"), TEXT("VillageKitchen") },
        { TEXT("Baking"), TEXT("VillageBread") }, { TEXT("Weaving"), TEXT("VillageWeaving") },
        { TEXT("Sewing"), TEXT("VillageSewing") }, { TEXT("Smithing"), TEXT("VillageSmith") },
        { TEXT("Pottery"), TEXT("VillagePottery") }, { TEXT("Carpentry"), TEXT("VillageCarpentry") },
        { TEXT("Herbalism"), TEXT("VillageHerbs") }, { TEXT("Trade"), TEXT("VillageTrade") },
        { TEXT("Masonry"), TEXT("VillageStone") }
    };
    const FName* Found = Shelf.Find(Skill);
    return Found ? *Found : NAME_None;
}

void FVillage::HomeBooks(const TArray<FName>& HouseTrades, TArray<FName>& Out)
{
    Out.Reset();
    Out.Add(TEXT("VillageFarming"));
    Out.Add(TEXT("VillageKitchen"));
    Out.Add(TEXT("VillageWoods"));
    for (const FName& Trade : HouseTrades)
    {
        const FName Book = BookFor(Trade);
        if (!Book.IsNone())
        {
            Out.AddUnique(Book);
        }
    }
}

bool FVillage::IsLiterate(const FPersonPlan& Plan)
{
    if ((Plan.Role >= EVillageRole::King && Plan.Role <= EVillageRole::QueenMother) || Plan.Role == EVillageRole::Steward)
    {
        return true;
    }
    if (Plan.Age < 9.0f)
    {
        return false;
    }
    if (!Plan.Trade.IsNone())
    {
        return true;
    }
    const uint32 Roll = GetTypeHash(Plan.FirstName + Plan.LastName) % 100u;
    return Roll < (Plan.Age < 14.0f ? 30u : 45u);
}

void FVillage::RememberBook(ACompleteHumanNPC* Person, FName Subject, float Level, float T)
{
    const FTextbook* Book = FLibrary::Find(Subject);
    if (!Person || !Book || Level <= 0.0f)
    {
        return;
    }
    if (Person->Mind)
    {
        FReadingBookmark& Mark = Person->Mind->BookmarkFor(Subject);
        Mark.Page = Book->Pages.Num();
        Mark.Word = 0;
        Mark.Understood = Book->Pages.Num();
        Mark.bFinished = true;
    }
    for (const FTextbookPage& Page : Book->Pages)
    {
        if (Person->SpeechComponent)
        {
            Person->SpeechComponent->LearnWordsFromBook(Page.Text, 1.0f, false);
        }
        if (!Person->MemoryComponent)
        {
            continue;
        }
        FBelief Read;
        Read.Subject = Subject;
        Read.Predicate = TEXT("Studied");
        Read.Value = 1.0f;
        Read.Confidence = 1.0f;
        Read.LearnedAt = T;
        Read.bVerified = true;
        Read.Text = Page.Text;
        Person->MemoryComponent->Learn(Read, 1.0f, 1.0f, T);
        Person->MemoryComponent->NameConclusion(FName(*FString::Printf(TEXT("%s:%s"), *Book->Title, *Page.Title)), Page.Text);
        if (!Page.Craft.IsNone())
        {
            FBelief HowTo;
            HowTo.Subject = Page.Craft;
            HowTo.Predicate = TEXT("HowTo");
            HowTo.Value = 1.0f;
            HowTo.Confidence = FMath::Clamp(Level, 0.05f, 1.0f);
            HowTo.LearnedAt = T;
            HowTo.bVerified = true;
            HowTo.Text = Page.Text;
            Person->MemoryComponent->Learn(HowTo, 1.0f, 1.0f, T);
        }
    }
}

void FVillage::Weave(UWorld* World, const TArray<ACompleteHumanNPC*>& People, const TArray<FPersonPlan>& Plans)
{
    if (!World)
    {
        return;
    }
    UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>();
    const float T = WorldMind ? WorldMind->WorldSeconds : 0.0f;
    FRandomStream Rng(0x5eed1377);
    const int32 Count = FMath::Min(People.Num(), Plans.Num());

    AActor* KingActor = nullptr;
    for (int32 I = 0; I < Count; ++I)
    {
        if (Plans[I].Role == EVillageRole::King)
        {
            KingActor = People[I];
        }
    }

    for (int32 I = 0; I < Count; ++I)
    {
        ACompleteHumanNPC* Me = People[I];
        const FPersonPlan& P = Plans[I];
        if (!Me || !Me->IdentityComponent)
        {
            continue;
        }
        UIdentityComponent* Id = Me->IdentityComponent;
        Id->Role = P.Role;
        Id->Trade = P.Trade;
        Id->Household = P.Household;
        Id->Occupation = TitleOf(P.Role, P.Trade, P.bFemale, P.Age);
        Id->Money = P.Money;
        Id->LivingCostPerDay = 0.3f;
        Id->bEmployed = Id->IsServant();
        Id->HourlyWage = Id->IsServant() ? 0.4f : 0.0f;
        Id->ToleratedAbsenceDays = 6.0f;
        Id->LastWorkedAt = T;
        Id->Spouse = People.IsValidIndex(P.Spouse) ? People[P.Spouse] : nullptr;
        Id->Mother = People.IsValidIndex(P.Mother) ? People[P.Mother] : nullptr;
        Id->Father = People.IsValidIndex(P.Father) ? People[P.Father] : nullptr;
        Id->Lord = Id->IsServant() ? KingActor : nullptr;
        Id->Children.Reset();
        for (int32 J = 0; J < Count; ++J)
        {
            if (Plans[J].Mother == I || Plans[J].Father == I)
            {
                Id->Children.Add(People[J]);
            }
        }
        if (Id->IsRoyal())
        {
            Id->Standing = P.Role == EVillageRole::King ? 0.9f : 0.6f;
            Id->SelfEsteem = FMath::Max(Id->SelfEsteem, 0.65f);
        }
        else if (!P.Trade.IsNone())
        {
            Id->Standing = 0.35f;
        }
        if (Me->SpeechComponent)
        {
            Me->SpeechComponent->LearnOralSpeech(P.Age);
            if (IsLiterate(P))
            {
                Me->SpeechComponent->MasterLetters();
            }
        }
        Skills(Me, P);
        Dress(Me, P.Role, P.Trade, P.Age, P.bFemale, GetTypeHash(P.FirstName + P.LastName));
    }

    for (int32 I = 0; I < Count; ++I)
    {
        ACompleteHumanNPC* Me = People[I];
        if (!Me || !Me->SocialComponent)
        {
            continue;
        }
        const FPersonPlan& Mine = Plans[I];
        const bool bMeRoyal = Mine.Role >= EVillageRole::King && Mine.Role <= EVillageRole::QueenMother;
        const bool bMeServant = Mine.Role >= EVillageRole::Steward && Mine.Role <= EVillageRole::Groom;
        for (int32 J = 0; J < Count; ++J)
        {
            ACompleteHumanNPC* Other = People[J];
            if (I == J || !Other || !Other->IdentityComponent)
            {
                continue;
            }
            const FPersonPlan& Theirs = Plans[J];
            const bool bThemRoyal = Theirs.Role >= EVillageRole::King && Theirs.Role <= EVillageRole::QueenMother;
            const bool bFamily = Mine.Household == Theirs.Household && (bMeRoyal == bThemRoyal);
            const bool bHouse = Mine.Household == Theirs.Household;
            const bool bSpouses = Mine.Spouse == J;
            const bool bKin = bSpouses || Mine.Mother == J || Mine.Father == J || Theirs.Mother == I || Theirs.Father == I;

            FRelationship& R = Me->SocialComponent->FindOrAdd(Other, T);
            R.KnownName = Other->IdentityComponent->FirstName;
            R.Familiarity = bHouse ? 0.95f : Rng.FRandRange(0.55f, 0.8f);
            R.Liking = bFamily ? Rng.FRandRange(0.55f, 0.9f) : Rng.FRandRange(-0.15f, 0.45f);
            R.Trust = bFamily ? 0.8f : Rng.FRandRange(0.3f, 0.6f);
            R.Respect = 0.35f + (Theirs.Age > 55.0f ? 0.15f : 0.0f) + (!Theirs.Trade.IsNone() ? 0.2f : 0.0f);
            R.Attachment = bKin ? Rng.FRandRange(0.8f, 0.95f) : (bFamily ? Rng.FRandRange(0.5f, 0.7f) : Rng.FRandRange(0.05f, 0.2f));
            R.Romantic = bSpouses ? 0.7f : 0.0f;
            R.InGroup = bFamily ? 1.0f : (bMeRoyal != bThemRoyal ? 0.4f : 0.65f);
            R.Fear = 0.0f;
            if (bThemRoyal && !bMeRoyal)
            {
                R.Respect = FMath::Max(R.Respect, 0.85f);
                R.Fear = Theirs.Role == EVillageRole::King ? 0.35f : 0.15f;
                if (bMeServant)
                {
                    R.Fear += 0.1f;
                    R.Trust = FMath::Max(R.Trust, 0.6f);
                }
            }
            R.InteractionCount = bHouse ? 200 : Rng.RandRange(20, 80);
            R.FirstMetAt = T - FMath::Min(Mine.Age, Theirs.Age) * 86400.0f * 3.0f;
            R.LastInteractionAt = T - Rng.FRandRange(1800.0f, 86400.0f);
            R.Kind = USocialComponent::ClassifyRelation(R);
        }
    }

    if (!WorldMind)
    {
        return;
    }
    for (int32 I = 0; I < Count; ++I)
    {
        ACompleteHumanNPC* Me = People[I];
        if (!Me || !Me->MemoryComponent || Plans[I].Age < 3.0f)
        {
            continue;
        }
        for (const FKnownLocation& Place : WorldMind->PublicPlaces)
        {
            TArray<FAffordance> Offers;
            WorldMind->CollectOffersNear(Place.Location, 900.0f, Offers);
            Me->MemoryComponent->LearnPlace(Place.Kind, Place.Location, Place.Label, true, nullptr, T, &Offers);
        }
        for (FKnownLocation& Known : Me->MemoryComponent->Places)
        {
            Known.Familiarity = FMath::Max(Known.Familiarity, 0.75f);
            Known.Affect = FMath::Max(Known.Affect, 0.1f);
        }
    }
}

bool FVillage::IsMedieval(const UObject* Context)
{
    return GMedieval;
}

bool FVillage::FastInfants()
{
    static const bool bFast = FParse::Param(FCommandLine::Get(), TEXT("FastInfants"));
    return bFast;
}

float FVillage::HeightScaleForAge(float Age)
{
    static const float Ages[] = { 0.0f, 0.5f, 1.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 14.0f, 16.0f, 18.0f };
    static const float Heights[] = { 0.29f, 0.38f, 0.43f, 0.5f, 0.58f, 0.66f, 0.73f, 0.79f, 0.85f, 0.92f, 0.97f, 1.0f };
    if (Age >= 18.0f)
    {
        return 1.0f;
    }
    for (int32 I = 1; I < UE_ARRAY_COUNT(Ages); ++I)
    {
        if (Age <= Ages[I])
        {
            const float T = (Age - Ages[I - 1]) / (Ages[I] - Ages[I - 1]);
            return FMath::Lerp(Heights[I - 1], Heights[I], FMath::Clamp(T, 0.0f, 1.0f));
        }
    }
    return 1.0f;
}

ACompleteHumanNPC* FVillage::Birth(UWorld* World, ACompleteHumanNPC* Mother, ACompleteHumanNPC* Father)
{
    if (!World || !Mother || !Mother->IdentityComponent)
    {
        return nullptr;
    }
    UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>();
    const float T = WorldMind ? WorldMind->WorldSeconds : 0.0f;
    UIdentityComponent* Mom = Mother->IdentityComponent;
    UIdentityComponent* Dad = Father ? Father->IdentityComponent : nullptr;
    const bool bGirl = FMath::FRand() < 0.5f;

    TSet<FString> Used;
    if (WorldMind)
    {
        for (const ACompleteHumanNPC* Someone : WorldMind->GetAllHumans())
        {
            if (Someone && Someone->IdentityComponent)
            {
                Used.Add(Someone->IdentityComponent->FirstName);
            }
        }
    }
    static const TCHAR* Boys[] = { TEXT("Иван"), TEXT("Василий"), TEXT("Фёдор"), TEXT("Семён"), TEXT("Пётр"), TEXT("Елисей"),
        TEXT("Олег"), TEXT("Мирон"), TEXT("Демьян"), TEXT("Акинфий"), TEXT("Лаврентий"), TEXT("Остап") };
    static const TCHAR* Girls[] = { TEXT("Анна"), TEXT("Мария"), TEXT("Ирина"), TEXT("Настасья"), TEXT("Акулина"), TEXT("Устинья"),
        TEXT("Милава"), TEXT("Забава"), TEXT("Ольга"), TEXT("Феодосия"), TEXT("Лукерья"), TEXT("Матрёна") };
    FString Name;
    for (int32 Try = 0; Try < 24 && Name.IsEmpty(); ++Try)
    {
        const FString Pick = bGirl ? Girls[FMath::RandRange(0, UE_ARRAY_COUNT(Girls) - 1)] : Boys[FMath::RandRange(0, UE_ARRAY_COUNT(Boys) - 1)];
        if (!Used.Contains(Pick))
        {
            Name = Pick;
        }
    }
    if (Name.IsEmpty())
    {
        Name = bGirl ? TEXT("Дуняша") : TEXT("Ванятка");
    }
    FString Family = Dad ? Dad->LastName : Mom->LastName;
    const bool bFemaleForm = Family.EndsWith(TEXT("а")) || Family.EndsWith(TEXT("ая"));
    if (bGirl && !bFemaleForm)
    {
        Family = Family.EndsWith(TEXT("ий")) ? Family.LeftChop(2) + TEXT("ая") : Family + TEXT("а");
    }
    else if (!bGirl && bFemaleForm)
    {
        Family = Family.EndsWith(TEXT("ая")) ? Family.LeftChop(2) + TEXT("ий") : Family.LeftChop(1);
    }

    const FVector Home = Mother->HasHome() ? Mother->GetHomeLocation() : Mother->GetActorLocation();
    FVector At = Mother->GetActorLocation() + Mother->GetActorForwardVector() * 70.0f + FVector(0.0f, 0.0f, 20.0f);
    float FloorZ = Mother->GetActorLocation().Z - Mother->GetSimpleCollisionHalfHeight();
    float BedDistance = 1300.0f;
    for (TActorIterator<AFurnitureActor> It(World); It && Mother->HasHome(); ++It)
    {
        if (*It && It->FurnitureType == EFurnitureType::Bed)
        {
            const float D = FVector::Dist2D(It->GetActorLocation(), Home);
            if (D < BedDistance)
            {
                BedDistance = D;
                const FVector Bed = It->GetActorLocation();
                At = Bed + (Home - Bed).GetSafeNormal2D() * 90.0f + FVector(0.0f, 0.0f, 20.0f);
                FloorZ = Bed.Z;
            }
        }
    }
    const FTransform Where(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), At);
    ACompleteHumanNPC* Baby = World->SpawnActorDeferred<ACompleteHumanNPC>(ACompleteHumanNPC::StaticClass(), Where, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Baby)
    {
        return nullptr;
    }
    if (Baby->IdentityComponent)
    {
        Baby->IdentityComponent->Preset(Name, Family, 0.0f, bGirl);
    }
    Baby->FinishSpawning(Where);
    Baby->SetHome(Home);

    UIdentityComponent* Kid = Baby->IdentityComponent;
    if (Kid)
    {
        const bool bRoyal = Mom->IsRoyal();
        Kid->Role = bRoyal ? (bGirl ? EVillageRole::Princess : EVillageRole::Prince) : EVillageRole::Child;
        Kid->Household = Mom->Household;
        Kid->Occupation = TEXT("младенец");
        Kid->Mother = Mother;
        Kid->Father = Father;
        Kid->Money = 0.0f;
        Kid->LivingCostPerDay = 0.0f;
        Kid->BornAt = T;
    }
    Mom->Children.Add(Baby);
    if (Dad)
    {
        Dad->Children.Add(Baby);
    }
    Dress(Baby, Kid ? Kid->Role : EVillageRole::Child, NAME_None, 0.0f, bGirl, GetTypeHash(Name));

    TArray<ACompleteHumanNPC*> Kin = { Mother };
    if (Father)
    {
        Kin.Add(Father);
    }
    for (const TWeakObjectPtr<AActor>& Sibling : Mom->Children)
    {
        if (ACompleteHumanNPC* Brother = Cast<ACompleteHumanNPC>(Sibling.Get()))
        {
            if (Brother != Baby)
            {
                Kin.Add(Brother);
            }
        }
    }
    for (ACompleteHumanNPC* Relative : Kin)
    {
        if (!Relative || !Relative->SocialComponent || !Baby->SocialComponent)
        {
            continue;
        }
        const bool bParent = Relative == Mother || Relative == Father;
        FRelationship& Theirs = Relative->SocialComponent->FindOrAdd(Baby, T);
        Theirs.KnownName = Name;
        Theirs.Familiarity = 0.9f;
        Theirs.Liking = 0.9f;
        Theirs.Trust = 0.9f;
        Theirs.Attachment = bParent ? 0.97f : 0.6f;
        Theirs.InGroup = 1.0f;
        Theirs.Kind = USocialComponent::ClassifyRelation(Theirs);
        FRelationship& Mine = Baby->SocialComponent->FindOrAdd(Relative, T);
        Mine.Familiarity = bParent ? 0.4f : 0.1f;
        Mine.Liking = 0.5f;
        Mine.Trust = 0.9f;
        Mine.Attachment = bParent ? 0.8f : 0.3f;
        Mine.InGroup = 1.0f;
        Mine.Kind = USocialComponent::ClassifyRelation(Mine);
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector CribAt(At.X, At.Y, FloorZ);
    if (AFurnitureActor* Crib = World->SpawnActor<AFurnitureActor>(AFurnitureActor::StaticClass(), CribAt, FRotator::ZeroRotator, Params))
    {
        Crib->FurnitureType = EFurnitureType::Crib;
        Crib->BuildLook();
        Crib->BuildOffers();
        Crib->MakeSolid();
        if (Crib->Affordances)
        {
            Crib->Affordances->bPrivate = true;
            Crib->Affordances->OwnerAnchor = Home;
            if (WorldMind)
            {
                WorldMind->RegisterAffordanceSource(Crib->Affordances);
            }
        }
        Baby->SetActorLocation(CribAt + FVector(0.0f, 0.0f, 60.0f), false, nullptr, ETeleportType::TeleportPhysics);
    }
    if (Baby->Mind)
    {
        Baby->Mind->ForceSleep();
    }

    const FString Parents = Dad ? FString::Printf(TEXT("%s и %s"), *Mom->FirstName, *Dad->FirstName) : Mom->FirstName;
    if (WorldMind)
    {
        FWorldEvent Event;
        Event.Tag = TEXT("Birth");
        Event.Description = FString::Printf(TEXT("у %s родил%s %s"), *Parents, bGirl ? TEXT("ась дочь") : TEXT("ся сын"), *Name);
        Event.Location = At;
        Event.Radius = 6000.0f;
        Event.Instigator = Mother;
        Event.Target = Baby;
        Event.Valence = 0.8f;
        Event.Significance = 0.8f;
        Event.Time = T;
        WorldMind->BroadcastEvent(Event);
    }
    UE_LOG(LogHumanCity, Warning, TEXT("Родил%s %s %s у %s."), bGirl ? TEXT("ась") : TEXT("ся"), *Name, *Family, *Parents);
    return Baby;
}

void FVillage::SetMedieval(bool bOn)
{
    GMedieval = bOn;
}

void FVillage::GatherHomeStores(const ACompleteHumanNPC* Person, TArray<AFurnitureActor*>& Out)
{
    Out.Reset();
    if (!Person || !Person->HasHome() || !Person->GetWorld())
    {
        return;
    }
    const FVector Home = Person->GetHomeLocation();
    for (TActorIterator<AFurnitureActor> It(Person->GetWorld()); It; ++It)
    {
        AFurnitureActor* Thing = *It;
        if (Thing && IsStorage(Thing->FurnitureType) && FVector::Dist2D(Thing->GetActorLocation(), Home) < 1100.0f)
        {
            Out.Add(Thing);
        }
    }
}

float FVillage::HouseholdStores(const ACompleteHumanNPC* Person, EResourceKind Only)
{
    TArray<AFurnitureActor*> Stores;
    GatherHomeStores(Person, Stores);
    float Total = 0.0f;
    for (const AFurnitureActor* Store : Stores)
    {
        for (const TPair<EResourceKind, float>& Pair : Store->Stored)
        {
            if (Only == EResourceKind::None || Pair.Key == Only)
            {
                Total += Pair.Value;
            }
        }
    }
    return Total;
}

float FVillage::HouseholdFood(const ACompleteHumanNPC* Person)
{
    if (!Person)
    {
        return 0.0f;
    }
    TArray<AFurnitureActor*> Stores;
    GatherHomeStores(Person, Stores);
    float Portions = 0.0f;
    for (const AFurnitureActor* Store : Stores)
    {
        for (const TPair<EResourceKind, float>& Pair : Store->Stored)
        {
            Portions += Pair.Value * FVillage::Portions(Pair.Key);
        }
    }
    if (const AResourceActor* Held = Cast<AResourceActor>(Person->CarriedItem.Get()))
    {
        Portions += Held->Amount * FVillage::Portions(Held->Kind);
    }
    int32 Members = 1;
    if (Person->IdentityComponent && Person->GetWorld())
    {
        if (const UHumanWorldSubsystem* WorldMind = Person->GetWorld()->GetSubsystem<UHumanWorldSubsystem>())
        {
            Members = 0;
            for (const ACompleteHumanNPC* Other : WorldMind->GetAllHumans())
            {
                if (Other && Other->IdentityComponent && Other->IdentityComponent->Household == Person->IdentityComponent->Household)
                {
                    ++Members;
                }
            }
            Members = FMath::Max(1, Members);
        }
    }
    return Portions / (Members * 3.0f);
}

float FVillage::HouseholdFirewood(const ACompleteHumanNPC* Person)
{
    float Wood = HouseholdStores(Person, EResourceKind::Firewood);
    if (const AResourceActor* Held = Person ? Cast<AResourceActor>(Person->CarriedItem.Get()) : nullptr)
    {
        if (Held->Kind == EResourceKind::Firewood)
        {
            Wood += Held->Amount;
        }
    }
    return Wood;
}

float FVillage::ProvisionFrom(float FoodDays, float Firewood, int32 DayOfYear, float Age)
{
    const bool bCold = DayOfYear < 100 || DayOfYear > 285;
    const bool bHarvest = DayOfYear >= 195 && DayOfYear <= 285;
    const float FoodNeed = bHarvest ? FMath::Lerp(6.0f, 10.0f, (DayOfYear - 195) / 90.0f) : 4.0f;
    const float Food = FMath::Clamp(FoodDays / FoodNeed, 0.0f, 1.0f);
    const float Wood = bCold ? FMath::Clamp(Firewood / 12.0f, 0.0f, 1.0f) : FMath::Clamp(0.6f + Firewood / 20.0f, 0.0f, 1.0f);
    const float Felt = 0.8f * Food + 0.2f * Wood;
    if (Age < 12.0f)
    {
        return 0.5f + 0.5f * Felt;
    }
    return Felt;
}

float FVillage::ExpectedWork(float Age, EVillageRole Role, int32 DayOfYear, float Hour)
{
    if (Age < 7.0f || (Role >= EVillageRole::King && Role <= EVillageRole::QueenMother))
    {
        return 0.0f;
    }
    const bool bWinter = DayOfYear < 100 || DayOfYear > 285;
    const float Start = 7.0f;
    const float End = bWinter ? 16.0f : 19.0f;
    if (Hour <= Start)
    {
        return 0.0f;
    }
    float Share = bWinter ? 0.35f : 0.6f;
    if (Age < 12.0f)
    {
        Share *= 0.3f;
    }
    else if (Age < 16.0f || Age >= 60.0f)
    {
        Share *= 0.6f;
    }
    return Share * (FMath::Min(Hour, End) - Start);
}

void FVillage::JudgeDay(UNeedComponent* Needs, float Worked, float Expected, float Hours)
{
    if (!Needs || Expected < 0.5f || Hours <= 0.0f)
    {
        return;
    }
    const float Ratio = FMath::Clamp(Worked / Expected, 0.0f, 1.0f);
    if (FNeedState* Pride = Needs->Find(ENeedType::Esteem))
    {
        Pride->Satisfaction = FMath::FInterpConstantTo(Pride->Satisfaction, 0.25f + 0.75f * Ratio, Hours, 0.6f);
    }
    if (Ratio < 0.5f)
    {
        Needs->Deprive(ENeedType::Belonging, 0.06f * (0.5f - Ratio) * Hours);
    }
}

bool FVillage::IsWorkLike(const FAffordance& A)
{
    if (!A.Craft.IsNone() || A.Action == EActionType::Work || A.Action == EActionType::Cook)
    {
        return true;
    }
    return A.Deal == TEXT("pack") || A.Deal == TEXT("sell") || A.Deal == TEXT("store") || A.Deal == TEXT("serve")
        || A.Deal == TEXT("order") || A.Deal == TEXT("royal") || A.Deal == TEXT("feed") || A.Deal == TEXT("cradle");
}

float FVillage::Provision(const ACompleteHumanNPC* Person)
{
    if (!Person)
    {
        return 1.0f;
    }
    const float Age = Person->IdentityComponent ? Person->IdentityComponent->Age : 30.0f;
    return ProvisionFrom(HouseholdFood(Person), HouseholdFirewood(Person), DayOfYear(Person), Age);
}

float FVillage::WealthForNeeds(const ACompleteHumanNPC* Person, float Money)
{
    return (FMath::Max(0.0f, Money) + 0.5f * CarriedWares(Person)) * 3.0f;
}

float FVillage::CarriedWares(const ACompleteHumanNPC* Person)
{
    const AResourceActor* Held = Person ? Cast<AResourceActor>(Person->CarriedItem.Get()) : nullptr;
    if (!Held || !IsSellable(Held->Kind))
    {
        return 0.0f;
    }
    return Held->Amount * PriceOf(Held->Kind);
}

AFurnitureActor* FVillage::HomeStore(const ACompleteHumanNPC* Person, EResourceKind For)
{
    TArray<AFurnitureActor*> Stores;
    GatherHomeStores(Person, Stores);
    auto Want = [For]() -> EFurnitureType
    {
        switch (For)
        {
        case EResourceKind::Water:      return EFurnitureType::Barrel;
        case EResourceKind::Bread:
        case EResourceKind::CookedFood: return EFurnitureType::Stove;
        case EResourceKind::Cloth:
        case EResourceKind::Thread:
        case EResourceKind::Shirt:
        case EResourceKind::Needle:
        case EResourceKind::Flax:
        case EResourceKind::Wool:
        case EResourceKind::Medicine:
        case EResourceKind::Herb:       return EFurnitureType::Wardrobe;
        default:                        return EFurnitureType::Shed;
        }
    };
    const EFurnitureType Best = Want();
    AFurnitureActor* Fallback = nullptr;
    for (AFurnitureActor* Store : Stores)
    {
        if (Store->FurnitureType == Best)
        {
            return Store;
        }
        if (!Fallback || Store->FurnitureType == EFurnitureType::Shed)
        {
            Fallback = Store;
        }
    }
    return Fallback;
}

bool FVillage::HasTool(const ACompleteHumanNPC* Person, EResourceKind Tool)
{
    if (Tool == EResourceKind::None)
    {
        return true;
    }
    if (!Person)
    {
        return false;
    }
    if (const AResourceActor* Held = Cast<AResourceActor>(Person->CarriedItem.Get()))
    {
        if (Held->Kind == Tool)
        {
            return true;
        }
    }
    return HouseholdStores(Person, Tool) >= 1.0f;
}

int32 FVillage::DayOfYear(const UObject* Context)
{
    if (const UMatterSubsystem* Matter = UMatterSubsystem::Get(Context))
    {
        return Matter->Climate().DayOfYear;
    }
    return 200;
}
