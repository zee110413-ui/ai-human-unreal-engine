#include "Crafts.h"
#include "Village.h"
#include "Textbook.h"

namespace
{
    struct FThingWord
    {
        const TCHAR* Stem;
        EResourceKind Kind;
    };

    const FThingWord ThingWords[] = {
        { TEXT("бревн"),     EResourceKind::Wood },
        { TEXT("брёв"),      EResourceKind::Wood },
        { TEXT("брев"),      EResourceKind::Wood },
        { TEXT("древесин"),  EResourceKind::Wood },
        { TEXT("дос"),       EResourceKind::Plank },
        { TEXT("дров"),      EResourceKind::Firewood },
        { TEXT("камн"),      EResourceKind::Stone },
        { TEXT("камен"),     EResourceKind::Stone },
        { TEXT("глин"),      EResourceKind::Clay },
        { TEXT("кирпич"),    EResourceKind::Brick },
        { TEXT("зерн"),      EResourceKind::Grain },
        { TEXT("зёрн"),      EResourceKind::Grain },
        { TEXT("зёр"),       EResourceKind::Grain },
        { TEXT("мук"),       EResourceKind::Flour },
        { TEXT("хлеб"),      EResourceKind::CookedFood },
        { TEXT("похлёбк"),   EResourceKind::CookedFood },
        { TEXT("похлебк"),   EResourceKind::CookedFood },
        { TEXT("еду"),       EResourceKind::CookedFood },
        { TEXT("овощ"),      EResourceKind::RawFood },
        { TEXT("вод"),       EResourceKind::Water },
        { TEXT("трав"),      EResourceKind::Herb },
        { TEXT("семен"),     EResourceKind::Seed },
        { TEXT("семян"),     EResourceKind::Seed },
        { TEXT("нит"),       EResourceKind::Thread },
        { TEXT("ткан"),      EResourceKind::Cloth },
        { TEXT("полотн"),    EResourceKind::Cloth },
        { TEXT("верёвк"),    EResourceKind::Rope },
        { TEXT("веревк"),    EResourceKind::Rope },
        { TEXT("кож"),       EResourceKind::Leather },
        { TEXT("желез"),     EResourceKind::Iron },
        { TEXT("угол"),      EResourceKind::Charcoal },
        { TEXT("угл"),       EResourceKind::Charcoal },
        { TEXT("горшк"),     EResourceKind::Pot },
        { TEXT("горшок"),    EResourceKind::Pot },
        { TEXT("стекл"),     EResourceKind::Glass },
        { TEXT("бумаг"),     EResourceKind::Paper },
        { TEXT("чернил"),    EResourceKind::Ink },
        { TEXT("мыл"),       EResourceKind::Soap },
        { TEXT("медн"),      EResourceKind::CopperOre },
        { TEXT("медь"),      EResourceKind::Copper },
        { TEXT("меди"),      EResourceKind::Copper },
        { TEXT("мёд"),       EResourceKind::Honey },
        { TEXT("молок"),     EResourceKind::Milk },
        { TEXT("рыб"),       EResourceKind::Fish },
        { TEXT("отвар"),     EResourceKind::Medicine },
        { TEXT("топор"),     EResourceKind::Axe },
        { TEXT("лопат"),     EResourceKind::Shovel },
        { TEXT("пил"),       EResourceKind::Saw },
        { TEXT("молот"),     EResourceKind::Hammer },
        { TEXT("игл"),       EResourceKind::Needle },
        { TEXT("удочк"),     EResourceKind::Rod },
        { TEXT("гвозд"),     EResourceKind::Tool },

        { TEXT("известняк"), EResourceKind::Limestone },
        { TEXT("гранит"),    EResourceKind::Granite },
        { TEXT("песчаник"),  EResourceKind::Sandstone },
        { TEXT("кремн"),     EResourceKind::Flint },
        { TEXT("кремен"),    EResourceKind::Flint },
        { TEXT("мел"),       EResourceKind::Chalk },
        { TEXT("кварц"),     EResourceKind::Quartz },
        { TEXT("песк"),      EResourceKind::Sand },
        { TEXT("песок"),     EResourceKind::Sand },
        { TEXT("торф"),      EResourceKind::Peat },
        { TEXT("каменн"),    EResourceKind::Coal },
        { TEXT("извест"),    EResourceKind::Lime },
        { TEXT("зол"),       EResourceKind::Ash },
        { TEXT("сол"),       EResourceKind::Salt },
        { TEXT("сер"),       EResourceKind::Sulphur },
        { TEXT("селитр"),    EResourceKind::Saltpetre },
        { TEXT("руд"),       EResourceKind::IronOre },
        { TEXT("медн"),      EResourceKind::CopperOre },
        { TEXT("оловян"),    EResourceKind::TinOre },
        { TEXT("свинц"),     EResourceKind::LeadOre },
        { TEXT("серебрян"),  EResourceKind::SilverOre },
        { TEXT("золот"),     EResourceKind::GoldOre },
        { TEXT("медь"),      EResourceKind::Copper },
        { TEXT("меди"),      EResourceKind::Copper },
        { TEXT("олов"),      EResourceKind::Tin },
        { TEXT("бронз"),     EResourceKind::Bronze },
        { TEXT("свинец"),    EResourceKind::Lead },
        { TEXT("серебр"),    EResourceKind::Silver },
        { TEXT("сталь"),     EResourceKind::Steel },
        { TEXT("стал"),      EResourceKind::Steel },
        { TEXT("лён"),       EResourceKind::Flax },
        { TEXT("льн"),       EResourceKind::Flax },
        { TEXT("шерст"),     EResourceKind::Wool },
        { TEXT("шкур"),      EResourceKind::Hide },
        { TEXT("воск"),      EResourceKind::Wax },
        { TEXT("воска"),     EResourceKind::Wax },
        { TEXT("смол"),      EResourceKind::Resin },
        { TEXT("дёгот"),     EResourceKind::Tar },
        { TEXT("дегот"),     EResourceKind::Tar },
        { TEXT("масл"),      EResourceKind::Oil },
        { TEXT("кор"),       EResourceKind::Bark },
        { TEXT("камыш"),     EResourceKind::Reed },
        { TEXT("солом"),     EResourceKind::Straw },
        { TEXT("почв"),      EResourceKind::Soil },
        { TEXT("земл"),      EResourceKind::Soil },
        { TEXT("дёрн"),      EResourceKind::Soil },
        { TEXT("гриб"),      EResourceKind::Mushroom },
        { TEXT("ягод"),      EResourceKind::Berry },
        { TEXT("яйц"),       EResourceKind::Egg },
        { TEXT("мяс"),       EResourceKind::Meat },
        { TEXT("кост"),      EResourceKind::Bone },
        { TEXT("снег"),      EResourceKind::Snow },
        { TEXT("лёд"),       EResourceKind::Ice },
        { TEXT("льд"),       EResourceKind::Ice },
        // Расширение мира: огород, сад, напитки, изделия
        { TEXT("скисш"), EResourceKind::CurdledMilk },
        { TEXT("прокисш"), EResourceKind::CurdledMilk },
        { TEXT("сливочн"), EResourceKind::Butter },
        { TEXT("творог"), EResourceKind::CottageCheese },
        { TEXT("грудинк"), EResourceKind::SmokedMeat },
        { TEXT("вялен"), EResourceKind::DriedFish },
        { TEXT("копчён"), EResourceKind::SmokedFish },
        { TEXT("копчен"), EResourceKind::SmokedFish },
        { TEXT("виноград"), EResourceKind::Grape },
        { TEXT("сусл"), EResourceKind::GrapeMust },
        { TEXT("медовух"), EResourceKind::Mead },
        { TEXT("сок"), EResourceKind::Water },
        { TEXT("сока"), EResourceKind::Water },
        { TEXT("жмых"), EResourceKind::Ash },
        { TEXT("заквас"), EResourceKind::Milk },
        { TEXT("брож"), EResourceKind::Wine },
        { TEXT("дрожж"), EResourceKind::Beer },
        { TEXT("тесто"), EResourceKind::Flour },
        { TEXT("хлеб"), EResourceKind::Bread },
        { TEXT("квас"), EResourceKind::Kvass },
        { TEXT("пив"), EResourceKind::Beer },
        { TEXT("уксус"), EResourceKind::Vinegar },
        { TEXT("костр"), EResourceKind::Firewood },
        { TEXT("лук"), EResourceKind::Onion },
        { TEXT("реп"), EResourceKind::Turnip },
        { TEXT("морков"), EResourceKind::Carrot },
        { TEXT("цинков"), EResourceKind::ZincOre },
        { TEXT("цинк"), EResourceKind::Zinc },
        { TEXT("латун"), EResourceKind::Brass },
        { TEXT("электрум"), EResourceKind::Electrum },
        { TEXT("берест"), EResourceKind::BirchBark },
        { TEXT("берёз"), EResourceKind::BirchWood },
        { TEXT("берез"), EResourceKind::BirchWood },
        { TEXT("сосн"), EResourceKind::PineWood },
        { TEXT("хворост"), EResourceKind::Brushwood },
        { TEXT("пыли"), EResourceKind::CharcoalDust },
        { TEXT("жёлуд"), EResourceKind::Acorn },
        { TEXT("желуд"), EResourceKind::Acorn },
        { TEXT("шишк"), EResourceKind::Cone },
        { TEXT("мрамор"), EResourceKind::Marble },
        { TEXT("базальт"), EResourceKind::Basalt },
        { TEXT("сланц"), EResourceKind::Slate },
        { TEXT("обсидиан"), EResourceKind::Obsidian },
        { TEXT("слюд"), EResourceKind::Mica },
        { TEXT("графит"), EResourceKind::Graphite },
        { TEXT("грави"), EResourceKind::Gravel },
        { TEXT("гряз"), EResourceKind::Mud },
        { TEXT("перегно"), EResourceKind::Compost },
        { TEXT("самоцвет"), EResourceKind::Gem },
        { TEXT("алмаз"), EResourceKind::Diamond },
        { TEXT("рубин"), EResourceKind::Ruby },
        { TEXT("изумруд"), EResourceKind::Emerald },
        { TEXT("сапфир"), EResourceKind::Sapphire },
        { TEXT("жемчуг"), EResourceKind::Pearl },
        { TEXT("пшениц"), EResourceKind::Wheat },
        { TEXT("ржан"), EResourceKind::Rye },
        { TEXT("ячмен"), EResourceKind::Barley },
        { TEXT("овёс"), EResourceKind::Oats },
        { TEXT("овс"), EResourceKind::Oats },
        { TEXT("реп"), EResourceKind::Turnip },
        { TEXT("капуст"), EResourceKind::Cabbage },
        { TEXT("морков"), EResourceKind::Carrot },
        { TEXT("чеснок"), EResourceKind::Garlic },
        { TEXT("свёкл"), EResourceKind::Beet },
        { TEXT("свекл"), EResourceKind::Beet },
        { TEXT("огур"), EResourceKind::Cucumber },
        { TEXT("тыкв"), EResourceKind::Pumpkin },
        { TEXT("дын"), EResourceKind::Melon },
        { TEXT("арбуз"), EResourceKind::Watermelon },
        { TEXT("картоф"), EResourceKind::Potato },
        { TEXT("картош"), EResourceKind::Potato },
        { TEXT("подсолнух"), EResourceKind::Sunflower },
        { TEXT("конопл"), EResourceKind::Hemp },
        { TEXT("сен"), EResourceKind::Hay },
        { TEXT("яблок"), EResourceKind::Apple },
        { TEXT("груш"), EResourceKind::Pear },
        { TEXT("слив"), EResourceKind::Plum },
        { TEXT("вишн"), EResourceKind::Cherry },
        { TEXT("орех"), EResourceKind::Nut },
        { TEXT("перь"), EResourceKind::Feather },
        { TEXT("пух"), EResourceKind::Feather },
        { TEXT("мех"), EResourceKind::Fur },
        { TEXT("рог"), EResourceKind::Antler },
        { TEXT("сал"), EResourceKind::Tallow },
        { TEXT("сыр"), EResourceKind::Cheese },
        { TEXT("хмел"), EResourceKind::Hops },
        { TEXT("квас"), EResourceKind::Kvass },
        { TEXT("пив"), EResourceKind::Beer },
        { TEXT("вино"), EResourceKind::Wine },
        { TEXT("вина"), EResourceKind::Wine },
        { TEXT("уксус"), EResourceKind::Vinegar },
        { TEXT("марен"), EResourceKind::Madder },
        { TEXT("вайд"), EResourceKind::Woad },
        { TEXT("краск"), EResourceKind::Dye },
        { TEXT("свеч"), EResourceKind::Candle },
        { TEXT("постн"), EResourceKind::OliveOil },
        { TEXT("сод"), EResourceKind::Soda },
        { TEXT("лоз"), EResourceKind::Vine },
        { TEXT("корзин"), EResourceKind::Basket },
        { TEXT("пробк"), EResourceKind::Cork },
        { TEXT("порох"), EResourceKind::Gunpowder },
        { TEXT("раствор"), EResourceKind::Mortar },
        { TEXT("карандаш"), EResourceKind::CharcoalPencil },
        { TEXT("пень"), EResourceKind::Stump },
        { TEXT("рожь"), EResourceKind::Rye },
        { TEXT("ржи"), EResourceKind::Rye },
    };

    struct FStationWord
    {
        const TCHAR* Stem;
        EFurnitureType Type;
    };

    const FStationWord StationWords[] = {
        { TEXT("верстак"),  EFurnitureType::Workbench },
        { TEXT("козл"),     EFurnitureType::Sawhorse },
        { TEXT("горн"),     EFurnitureType::Forge },
        { TEXT("наковальн"),EFurnitureType::Forge },
        { TEXT("печ"),      EFurnitureType::Kiln },
        { TEXT("круг"),     EFurnitureType::PotteryWheel },
        { TEXT("станк"),    EFurnitureType::Loom },
        { TEXT("стане"),    EFurnitureType::Loom },
        { TEXT("плит"),     EFurnitureType::Stove },
        { TEXT("грядк"),    EFurnitureType::GardenBed },
        { TEXT("улей"),     EFurnitureType::Beehive },
        { TEXT("уль"),      EFurnitureType::Beehive },
        { TEXT("берег"),    EFurnitureType::FishingSpot },
        { TEXT("яме"),      EFurnitureType::ClayPit },
        { TEXT("яму"),      EFurnitureType::ClayPit },
        { TEXT("дерев"),    EFurnitureType::Tree },
        { TEXT("каменолом"),EFurnitureType::StonePile },
        { TEXT("родник"),   EFurnitureType::Spring },
        { TEXT("куст"),     EFurnitureType::Bush },
        { TEXT("грибн"),    EFurnitureType::Mushrooms },
        { TEXT("поле"),     EFurnitureType::WildField },
        { TEXT("поля"),     EFurnitureType::WildField },
        { TEXT("луг"),      EFurnitureType::WildField },
        { TEXT("бочк"),     EFurnitureType::Barrel },
        { TEXT("стол"),     EFurnitureType::Table },
        { TEXT("амбар"),    EFurnitureType::Shed },
        { TEXT("сара"),     EFurnitureType::Shed }
    };

    struct FBuiltWord
    {
        const TCHAR* Stem;
        EFurnitureType Type;
    };

    const FBuiltWord BuiltWords[] = {
        { TEXT("стул"),     EFurnitureType::Chair },
        { TEXT("стол"),     EFurnitureType::Table },
        { TEXT("лавк"),     EFurnitureType::Bench },
        { TEXT("скамь"),    EFurnitureType::Bench },
        { TEXT("полк"),     EFurnitureType::Bookshelf },
        { TEXT("кроват"),   EFurnitureType::Bed },
        { TEXT("сара"),     EFurnitureType::Shed },
        { TEXT("забор"),    EFurnitureType::Fence },
        { TEXT("колодец"),  EFurnitureType::Well },
        { TEXT("колодц"),   EFurnitureType::Well },
        { TEXT("телег"),    EFurnitureType::Cart },
        { TEXT("верстак"),  EFurnitureType::Workbench },
        { TEXT("козл"),     EFurnitureType::Sawhorse },
        { TEXT("грядк"),    EFurnitureType::GardenBed },
        { TEXT("яму"),      EFurnitureType::ClayPit },
        { TEXT("улей"),     EFurnitureType::Beehive },
        { TEXT("горн"),     EFurnitureType::Forge },
        { TEXT("печь"),     EFurnitureType::Kiln },
        { TEXT("круг"),     EFurnitureType::PotteryWheel },
        { TEXT("станок"),   EFurnitureType::Loom }
    };

    bool HasStem(const FString& Word, const TCHAR* Stem)
    {
        return Word.StartsWith(Stem, ESearchCase::IgnoreCase);
    }

    EResourceKind ThingOf(const FString& Word)
    {
        for (const FThingWord& Entry : ThingWords)
        {
            if (HasStem(Word, Entry.Stem))
            {
                return Entry.Kind;
            }
        }
        return EResourceKind::None;
    }

    bool IsOre(EResourceKind Kind)
    {
        return Kind == EResourceKind::IronOre || Kind == EResourceKind::CopperOre
            || Kind == EResourceKind::TinOre || Kind == EResourceKind::LeadOre
            || Kind == EResourceKind::SilverOre || Kind == EResourceKind::GoldOre;
    }

    bool IsTool(EResourceKind Kind)
    {
        return Kind == EResourceKind::Axe || Kind == EResourceKind::Shovel
            || Kind == EResourceKind::Saw || Kind == EResourceKind::Hammer
            || Kind == EResourceKind::Needle || Kind == EResourceKind::Rod;
    }

    TCHAR Lower(TCHAR Ch)
    {
        if (Ch >= TEXT('А') && Ch <= TEXT('Я'))
        {
            return static_cast<TCHAR>(Ch - TEXT('А') + TEXT('а'));
        }
        if (Ch == TEXT('Ё'))
        {
            return TEXT('ё');
        }
        return FChar::ToLower(Ch);
    }

    FString Clean(const FString& Word)
    {
        FString Out;
        for (TCHAR Ch : Word)
        {
            const TCHAR Low = Lower(Ch);
            if (FChar::IsDigit(Low) || (Low >= TEXT('а') && Low <= TEXT('я')) || Low == TEXT('ё')
                || (Low >= TEXT('a') && Low <= TEXT('z')))
            {
                Out.AppendChar(Low);
            }
        }
        return Out;
    }

    float NumberOf(const FString& Word)
    {
        static const TCHAR* Words[] = { TEXT("один"), TEXT("одно"), TEXT("одного"), TEXT("два"), TEXT("две"),
            TEXT("двух"), TEXT("три"), TEXT("трёх"), TEXT("трех"), TEXT("четыре"), TEXT("четырёх"),
            TEXT("четырех"), TEXT("пять"), TEXT("пяти"), TEXT("шесть"), TEXT("шести"), TEXT("семь"),
            TEXT("семи"), TEXT("восемь"), TEXT("восьми"), TEXT("девять"), TEXT("девяти"), TEXT("десять"),
            TEXT("десяти"), TEXT("двенадцать"), TEXT("двенадцати") };
        static const float Values[] = { 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
            9, 9, 10, 10, 12, 12 };

        for (int32 i = 0; i < UE_ARRAY_COUNT(Words); ++i)
        {
            if (Word == Words[i])
            {
                return Values[i];
            }
        }
        if (Word.IsNumeric())
        {
            return FMath::Clamp(FCString::Atof(*Word), 1.0f, 24.0f);
        }
        return 0.0f;
    }
}

static TArray<FCraft> GCrafts;

void FCraftBook::BuildFrom(TArray<FTextbook>& Books)
{
    GCrafts.Reset();

    for (FTextbook& Book : Books)
    {
        for (FTextbookPage& Page : Book.Pages)
        {
            TArray<FString> Sentences;
            Page.Text.ParseIntoArray(Sentences, TEXT("."), true);

            FCraft Deed;
            bool bFound = false;
            for (const FString& Sentence : Sentences)
            {
                if (ReadDeed(Sentence, Book.Skill, Book.SecondSkill, Book.Difficulty, Deed))
                {
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                continue;
            }

            if (Find(Deed.Id))
            {
                Page.Craft = Deed.Id;
                continue;
            }

            GCrafts.Add(Deed);
            Page.Craft = Deed.Id;
        }
    }
    FVillage::AppendWorks(GCrafts);
}

bool FCraftBook::ReadDeed(const FString& Text, FName Skill, FName SecondSkill, float Difficulty, FCraft& Out)
{
    TArray<FString> Raw;
    Text.ParseIntoArrayWS(Raw);
    if (Raw.Num() < 4)
    {
        return false;
    }

    int32 Marker = INDEX_NONE;
    for (int32 i = 0; i < Raw.Num(); ++i)
    {
        const FString Word = Clean(Raw[i]);
        static const TCHAR* Markers[] = {
            TEXT("делают"), TEXT("выходит"), TEXT("выходят"), TEXT("получается"), TEXT("получают"),
            TEXT("лепят"), TEXT("куют"), TEXT("пекут"), TEXT("варят"), TEXT("ткут"), TEXT("прядут"),
            TEXT("шьют"), TEXT("пилят"), TEXT("колют"), TEXT("обжигают"), TEXT("плавят"),
            TEXT("копают"), TEXT("сажают"), TEXT("ловят"), TEXT("собирают"), TEXT("вьют"),
            TEXT("мелют"), TEXT("строят"), TEXT("сколачивают"), TEXT("валят"), TEXT("выделывают"),
            TEXT("сбивают"), TEXT("режут"), TEXT("складывают"), TEXT("роют"), TEXT("жгут"),
            TEXT("выжигают"), TEXT("гонят"), TEXT("сушат"), TEXT("добывают"), TEXT("набирают"),
            TEXT("отливают"), TEXT("точат"), TEXT("намывают"), TEXT("бьют"), TEXT("сучат"),
            TEXT("давят"), TEXT("сливают"), TEXT("коптят"), TEXT("вялят"),
            TEXT("отжимают"), TEXT("растирают"), TEXT("вытапливают"),
            TEXT("смотрят"), };

        bool bMarker = false;
        for (const TCHAR* Sign : Markers)
        {
            if (Word == Sign)
            {
                bMarker = true;
                break;
            }
        }
        if (bMarker)
        {
            Marker = i;
            break;
        }
    }
    if (Marker == INDEX_NONE)
    {
        return false;
    }

    TArray<FCraftPart> Inputs;
    EResourceKind Tool = EResourceKind::None;
    EFurnitureType Station = EFurnitureType::Workbench;
    bool bStationFound = false;
    bool bFromSeen = false;
    float PendingCount = 0.0f;

    for (int32 i = 0; i < Marker; ++i)
    {
        const FString Word = Clean(Raw[i]);
        if (Word == TEXT("из"))
        {
            bFromSeen = true;
            continue;
        }

        const float Count = NumberOf(Word);
        if (Count > 0.0f)
        {
            PendingCount = Count;
            continue;
        }

        const EResourceKind Kind = ThingOf(Word);
        if (Kind != EResourceKind::None)
        {
            const bool bOreTail = Kind == EResourceKind::IronOre && Inputs.Num() > 0
                && IsOre(Inputs.Last().Kind) && Inputs.Last().Kind != EResourceKind::IronOre;
            if (bOreTail)
            {
                continue;
            }

            if (IsTool(Kind))
            {
                Tool = Kind;
            }
            else if (bFromSeen)
            {
                FCraftPart Need;
                Need.Kind = Kind;
                Need.Amount = PendingCount > 0.0f ? PendingCount : 1.0f;
                Inputs.Add(Need);
                PendingCount = 0.0f;
            }
            continue;
        }

        for (const FStationWord& Entry : StationWords)
        {
            if (HasStem(Word, Entry.Stem))
            {
                Station = Entry.Type;
                bStationFound = true;
                break;
            }
        }
    }

    EResourceKind Product = EResourceKind::None;
    EFurnitureType Built = EFurnitureType::Chair;
    EClothingKind Sewn = EClothingKind::None;
    bool bBuilds = false;
    float Amount = 0.0f;
    float PendingAfter = 0.0f;

    for (int32 i = Marker + 1; i < Raw.Num(); ++i)
    {
        const FString Word = Clean(Raw[i]);

        const float Count = NumberOf(Word);
        if (Count > 0.0f)
        {
            PendingAfter = Count;
            continue;
        }

        if (Product == EResourceKind::None && !bBuilds && Sewn == EClothingKind::None)
        {
            const EClothingKind Garment = AClothingActor::KindFromWord(Word);
            if (Garment != EClothingKind::None)
            {
                Sewn = Garment;
                continue;
            }

            const EResourceKind Kind = ThingOf(Word);
            if (Kind != EResourceKind::None && !IsTool(Kind))
            {
                Product = Kind;
                Amount = PendingAfter > 0.0f ? PendingAfter : 1.0f;
                continue;
            }
            if (IsTool(Kind))
            {
                Product = Kind;
                Amount = 1.0f;
                continue;
            }

            for (const FBuiltWord& Entry : BuiltWords)
            {
                if (HasStem(Word, Entry.Stem))
                {
                    Built = Entry.Type;
                    bBuilds = true;
                    break;
                }
            }
        }
    }

    if (Product == EResourceKind::None && !bBuilds && Sewn == EClothingKind::None)
    {
        return false;
    }

    if (Inputs.Num() == 0 && Tool == EResourceKind::None && !bStationFound)
    {
        return false;
    }

    if (Product == EResourceKind::Wine && Inputs.Num() == 0)
    {
        return false;
    }

    FString Name;
    if (Sewn != EClothingKind::None)
    {
        Name = FString::Printf(TEXT("сшить: %s"), *AClothingActor::NameOf(Sewn));
    }
    else if (bBuilds)
    {
        Name = FString::Printf(TEXT("сделать: %s"), *NameOfBuilt(Built));
    }
    else
    {
        Name = FString::Printf(TEXT("сделать: %s"), *NameOfKind(Product));
    }

    Out = FCraft();
    Out.Label = Name;
    Out.Skill = Skill;
    Out.SecondSkill = SecondSkill;
    Out.Difficulty = FMath::Clamp(Difficulty, 0.15f, 0.9f);
    Out.Duration = 1200.0f + Inputs.Num() * 600.0f;
    Out.Effort = 0.3f;
    Out.Station = bStationFound ? Station : (bBuilds ? EFurnitureType::Workbench : EFurnitureType::Workbench);
    Out.bAnywhere = !bStationFound;
    Out.Inputs = MoveTemp(Inputs);
    Out.Output = Product;
    Out.OutputAmount = FMath::Max(1.0f, Amount);
    Out.Builds = Built;
    Out.bBuilds = bBuilds;
    Out.Wears = Sewn;
    Out.Tool = Tool;

    FString Key;
    if (Sewn != EClothingKind::None)
    {
        Key = FString::Printf(TEXT("sew%d"), static_cast<int32>(Sewn));
    }
    else if (bBuilds)
    {
        Key = FString::Printf(TEXT("build%d"), static_cast<int32>(Built));
    }
    else
    {
        Key = FString::Printf(TEXT("make%d"), static_cast<int32>(Product));
    }
    for (const FCraftPart& Need : Out.Inputs)
    {
        Key += FString::Printf(TEXT("_%d"), static_cast<int32>(Need.Kind));
    }
    Out.Id = FName(*Key);

    return true;
}

const TArray<FCraft>& FCraftBook::All()
{
    if (GCrafts.Num() == 0)
    {
        FLibrary::All();
    }
    return GCrafts;
}

const FCraft* FCraftBook::Find(FName Id)
{
    for (const FCraft& C : GCrafts)
    {
        if (C.Id == Id)
        {
            return &C;
        }
    }
    return nullptr;
}

void FCraftBook::AtStation(EFurnitureType Station, TArray<const FCraft*>& Out)
{
    for (const FCraft& C : All())
    {
        if (C.Station == Station && !C.bAnywhere)
        {
            Out.Add(&C);
        }
    }
}

void FCraftBook::Anywhere(TArray<const FCraft*>& Out)
{
    for (const FCraft& C : All())
    {
        if (C.bAnywhere)
        {
            Out.Add(&C);
        }
    }
}

FString FCraftBook::NameOfBuilt(EFurnitureType Type)
{
    switch (Type)
    {
    case EFurnitureType::Chair:        return TEXT("стул");
    case EFurnitureType::Table:        return TEXT("стол");
    case EFurnitureType::Bench:        return TEXT("лавка");
    case EFurnitureType::Bookshelf:    return TEXT("полка");
    case EFurnitureType::Bed:          return TEXT("кровать");
    case EFurnitureType::Shed:         return TEXT("сарай");
    case EFurnitureType::Fence:        return TEXT("забор");
    case EFurnitureType::Well:         return TEXT("колодец");
    case EFurnitureType::Cart:         return TEXT("телега");
    case EFurnitureType::Workbench:    return TEXT("верстак");
    case EFurnitureType::Sawhorse:     return TEXT("козлы");
    case EFurnitureType::GardenBed:    return TEXT("грядка");
    case EFurnitureType::ClayPit:      return TEXT("яма");
    case EFurnitureType::Beehive:      return TEXT("улей");
    case EFurnitureType::Kiln:         return TEXT("печь");
    case EFurnitureType::Forge:        return TEXT("горн");
    case EFurnitureType::Loom:         return TEXT("ткацкий станок");
    case EFurnitureType::PotteryWheel: return TEXT("гончарный круг");
    default:                           return TEXT("постройка");
    }
}

FString FCraftBook::NameOfKind(EResourceKind Kind)
{
    switch (Kind)
    {
    case EResourceKind::RawFood:    return TEXT("продукты");
    case EResourceKind::CookedFood: return TEXT("еда");
    case EResourceKind::Water:      return TEXT("вода");
    case EResourceKind::Coin:       return TEXT("деньги");
    case EResourceKind::Firewood:   return TEXT("дрова");
    case EResourceKind::Herb:       return TEXT("трава");
    case EResourceKind::Seed:       return TEXT("семена");
    case EResourceKind::Cloth:      return TEXT("ткань");
    case EResourceKind::Wood:       return TEXT("бревно");
    case EResourceKind::Plank:      return TEXT("доска");
    case EResourceKind::Stone:      return TEXT("камень");
    case EResourceKind::Clay:       return TEXT("глина");
    case EResourceKind::Brick:      return TEXT("кирпич");
    case EResourceKind::Grain:      return TEXT("зерно");
    case EResourceKind::Flour:      return TEXT("мука");
    case EResourceKind::Tool:       return TEXT("гвозди");
    case EResourceKind::Rope:       return TEXT("верёвка");
    case EResourceKind::Thread:     return TEXT("нить");
    case EResourceKind::Leather:    return TEXT("кожа");
    case EResourceKind::Iron:       return TEXT("железо");
    case EResourceKind::Charcoal:   return TEXT("уголь");
    case EResourceKind::Pot:        return TEXT("горшок");
    case EResourceKind::Glass:      return TEXT("стекло");
    case EResourceKind::Paper:      return TEXT("бумага");
    case EResourceKind::Ink:        return TEXT("чернила");
    case EResourceKind::Soap:       return TEXT("мыло");
    case EResourceKind::Honey:      return TEXT("мёд");
    case EResourceKind::Milk:       return TEXT("молоко");
    case EResourceKind::Fish:       return TEXT("рыба");
    case EResourceKind::Medicine:   return TEXT("отвар");
    case EResourceKind::Axe:        return TEXT("топор");
    case EResourceKind::Shovel:     return TEXT("лопата");
    case EResourceKind::Saw:        return TEXT("пила");
    case EResourceKind::Hammer:     return TEXT("молот");
    case EResourceKind::Needle:     return TEXT("игла");
    case EResourceKind::Rod:        return TEXT("удочка");
    case EResourceKind::Limestone:  return TEXT("известняк");
    case EResourceKind::Granite:    return TEXT("гранит");
    case EResourceKind::Sandstone:  return TEXT("песчаник");
    case EResourceKind::Flint:      return TEXT("кремень");
    case EResourceKind::Chalk:      return TEXT("мел");
    case EResourceKind::Quartz:     return TEXT("кварц");
    case EResourceKind::Sand:       return TEXT("песок");
    case EResourceKind::Peat:       return TEXT("торф");
    case EResourceKind::Coal:       return TEXT("каменный уголь");
    case EResourceKind::Lime:       return TEXT("известь");
    case EResourceKind::Ash:        return TEXT("зола");
    case EResourceKind::Salt:       return TEXT("соль");
    case EResourceKind::Sulphur:    return TEXT("сера");
    case EResourceKind::Saltpetre:  return TEXT("селитра");
    case EResourceKind::IronOre:    return TEXT("руда");
    case EResourceKind::CopperOre:  return TEXT("медная руда");
    case EResourceKind::TinOre:     return TEXT("оловянная руда");
    case EResourceKind::LeadOre:    return TEXT("свинцовая руда");
    case EResourceKind::SilverOre:  return TEXT("серебряная руда");
    case EResourceKind::GoldOre:    return TEXT("золотая руда");
    case EResourceKind::Copper:     return TEXT("медь");
    case EResourceKind::Tin:        return TEXT("олово");
    case EResourceKind::Bronze:     return TEXT("бронза");
    case EResourceKind::Lead:       return TEXT("свинец");
    case EResourceKind::Silver:     return TEXT("серебро");
    case EResourceKind::Gold:       return TEXT("золото");
    case EResourceKind::Steel:      return TEXT("сталь");
    case EResourceKind::Flax:       return TEXT("лён");
    case EResourceKind::Wool:       return TEXT("шерсть");
    case EResourceKind::Hide:       return TEXT("шкура");
    case EResourceKind::Wax:        return TEXT("воск");
    case EResourceKind::Resin:      return TEXT("смола");
    case EResourceKind::Tar:        return TEXT("дёготь");
    case EResourceKind::Oil:        return TEXT("масло");
    case EResourceKind::Bark:       return TEXT("кора");
    case EResourceKind::Reed:       return TEXT("камыш");
    case EResourceKind::Mushroom:   return TEXT("грибы");
    case EResourceKind::Berry:      return TEXT("ягоды");
    case EResourceKind::Egg:        return TEXT("яйца");
    case EResourceKind::Meat:       return TEXT("мясо");
    case EResourceKind::Bone:       return TEXT("кость");
    case EResourceKind::Snow:       return TEXT("снег");
    case EResourceKind::Ice:        return TEXT("лёд");
    case EResourceKind::Straw:      return TEXT("солома");
    case EResourceKind::Soil:       return TEXT("земля");
    case EResourceKind::BirchWood:  return TEXT("берёза");
    case EResourceKind::PineWood:   return TEXT("сосна");
    case EResourceKind::CharcoalDust: return TEXT("угольная пыль");
    case EResourceKind::Acorn:      return TEXT("желуди");
    case EResourceKind::Cone:       return TEXT("шишки");
    case EResourceKind::BirchBark:  return TEXT("береста");
    case EResourceKind::Stump:      return TEXT("пень");
    case EResourceKind::Brushwood:  return TEXT("хворост");
    case EResourceKind::Marble:     return TEXT("мрамор");
    case EResourceKind::Basalt:     return TEXT("базальт");
    case EResourceKind::Slate:      return TEXT("сланец");
    case EResourceKind::Obsidian:   return TEXT("обсидиан");
    case EResourceKind::Mica:       return TEXT("слюда");
    case EResourceKind::Graphite:   return TEXT("графит");
    case EResourceKind::Gravel:     return TEXT("гравий");
    case EResourceKind::Mud:        return TEXT("грязь");
    case EResourceKind::Compost:    return TEXT("перегной");
    case EResourceKind::ZincOre:    return TEXT("цинковая руда");
    case EResourceKind::Zinc:       return TEXT("цинк");
    case EResourceKind::Brass:      return TEXT("латунь");
    case EResourceKind::Electrum:   return TEXT("электрум");
    case EResourceKind::Gem:        return TEXT("самоцвет");
    case EResourceKind::Diamond:    return TEXT("алмаз");
    case EResourceKind::Ruby:       return TEXT("рубин");
    case EResourceKind::Emerald:    return TEXT("изумруд");
    case EResourceKind::Sapphire:   return TEXT("сапфир");
    case EResourceKind::Pearl:      return TEXT("жемчуг");
    case EResourceKind::Wheat:      return TEXT("пшеница");
    case EResourceKind::Rye:        return TEXT("рожь");
    case EResourceKind::Barley:     return TEXT("ячмень");
    case EResourceKind::Oats:       return TEXT("овёс");
    case EResourceKind::Turnip:     return TEXT("репа");
    case EResourceKind::Cabbage:    return TEXT("капуста");
    case EResourceKind::Carrot:     return TEXT("морковь");
    case EResourceKind::Onion:      return TEXT("лук");
    case EResourceKind::Garlic:     return TEXT("чеснок");
    case EResourceKind::Beet:       return TEXT("свёкла");
    case EResourceKind::Cucumber:   return TEXT("огурец");
    case EResourceKind::Pumpkin:    return TEXT("тыква");
    case EResourceKind::Melon:      return TEXT("дыня");
    case EResourceKind::Watermelon: return TEXT("арбуз");
    case EResourceKind::Potato:     return TEXT("картофель");
    case EResourceKind::Sunflower:  return TEXT("подсолнух");
    case EResourceKind::Hemp:       return TEXT("конопля");
    case EResourceKind::Hay:        return TEXT("сено");
    case EResourceKind::Apple:      return TEXT("яблоко");
    case EResourceKind::Pear:       return TEXT("груша");
    case EResourceKind::Plum:       return TEXT("слива");
    case EResourceKind::Cherry:     return TEXT("вишня");
    case EResourceKind::Grape:      return TEXT("виноград");
    case EResourceKind::Nut:        return TEXT("орех");
    case EResourceKind::Feather:    return TEXT("пух и перья");
    case EResourceKind::Fur:        return TEXT("мех");
    case EResourceKind::Antler:     return TEXT("рога");
    case EResourceKind::Tallow:     return TEXT("сало");
    case EResourceKind::CurdledMilk: return TEXT("скисшее молоко");
    case EResourceKind::Butter:     return TEXT("сливочное масло");
    case EResourceKind::Cheese:     return TEXT("сыр");
    case EResourceKind::CottageCheese: return TEXT("творог");
    case EResourceKind::SmokedFish: return TEXT("копчёная рыба");
    case EResourceKind::SmokedMeat: return TEXT("копчёная грудинка");
    case EResourceKind::DriedFish:  return TEXT("вяленая рыба");
    case EResourceKind::Hops:       return TEXT("хмель");
    case EResourceKind::GrapeMust:  return TEXT("виноградное сусло");
    case EResourceKind::Kvass:      return TEXT("квас");
    case EResourceKind::Beer:       return TEXT("пиво");
    case EResourceKind::Wine:       return TEXT("вино");
    case EResourceKind::Mead:       return TEXT("медовуха");
    case EResourceKind::Vinegar:    return TEXT("уксус");
    case EResourceKind::Madder:     return TEXT("марена");
    case EResourceKind::Woad:       return TEXT("вайда");
    case EResourceKind::Dye:        return TEXT("краска");
    case EResourceKind::Candle:     return TEXT("свеча");
    case EResourceKind::OliveOil:   return TEXT("постное масло");
    case EResourceKind::Soda:       return TEXT("сода");
    case EResourceKind::Vine:       return TEXT("лоза");
    case EResourceKind::Basket:     return TEXT("корзина");
    case EResourceKind::Cork:       return TEXT("пробка");
    case EResourceKind::Gunpowder:  return TEXT("порох");
    case EResourceKind::Mortar:     return TEXT("известковый раствор");
    case EResourceKind::CharcoalPencil: return TEXT("угольный карандаш");
    default:                        return TEXT("вещь");
    }
}
