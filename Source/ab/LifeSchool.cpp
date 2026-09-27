#include "MindLearning.h"
#include "NeedComponent.h"
#include "PhysiologyComponent.h"
#include "AffordanceComponent.h"
#include "FurnitureActor.h"
#include "Village.h"
#include "Crafts.h"
#include "Textbook.h"
#include "Async/ParallelFor.h"
#include "Misc/Paths.h"

namespace
{
    enum class ESpotKind : uint8
    {
        Home, Field, Forest, River, Well, Market, Church, Castle, Workshop, Master, Neighbour
    };

    struct FSpot
    {
        FVector2D At = FVector2D::ZeroVector;
        ESpotKind Kind = ESpotKind::Home;
        uint32 Place = 0;
        TArray<FAffordance> Offers;
        FName Trade;
        float MasterSkill = 0.0f;
    };

    struct FNeighbour
    {
        int32 Spot = 0;
        float Closeness = 0.0f;
        float Liking = 0.0f;
        float Resentment = 0.0f;
        float Fear = 0.0f;
        uint32 Word = 0;
        bool bFamily = false;
    };

    struct FLesson
    {
        FSelfState Before;
        FOptionView View;
        FSelfState After;
        float Hours = 0.0f;
        bool bSuccess = false;
        bool bDied = false;
    };

    struct FDayLog
    {
        double Wellbeing = 0.0;
        double Hours = 0.0;
        int32 Deaths = 0;
        int32 Failures = 0;
        int32 Decisions = 0;
        TMap<FString, int32> Deeds;
        TMap<FString, double> Spent;
        TMap<FString, int32> Fails;
        TMap<FString, int32> Causes;
        TMap<FString, int32> Trades;
        double Needs[FMindSense::NeedCount] = {};
        double Stores = 0.0;
        double Money = 0.0;
        int32 Samples = 0;
    };

    const TCHAR* ClassOf(const FAffordance& A)
    {
        if (!A.Craft.IsNone())
        {
            return TEXT("работа");
        }
        if (A.Deal == TEXT("sell") || A.Deal == TEXT("buy") || A.Deal == TEXT("pack"))
        {
            return TEXT("торг");
        }
        if (A.Deal == TEXT("feed") || A.Deal == TEXT("cradle"))
        {
            return TEXT("дитя");
        }
        if (A.Deal == TEXT("learn"))
        {
            return TEXT("учёба");
        }
        if (A.Deal == TEXT("order") || A.Deal == TEXT("serve") || A.Deal == TEXT("royal") || A.Deal == TEXT("petition"))
        {
            return TEXT("служба");
        }
        if (A.Deal == TEXT("store"))
        {
            return TEXT("хозяйство");
        }
        switch (A.Action)
        {
        case EActionType::Eat:
        case EActionType::Drink:
            return TEXT("еда");
        case EActionType::Sleep:
            return TEXT("сон");
        case EActionType::Rest:
        case EActionType::Wander:
            return TEXT("отдых");
        case EActionType::Talk:
        case EActionType::Entertain:
        case EActionType::Celebrate:
            return TEXT("общение");
        case EActionType::Wash:
        case EActionType::UseToilet:
            return TEXT("гигиена");
        case EActionType::Reflect:
            return TEXT("молитва");
        case EActionType::Study:
        case EActionType::Read:
        case EActionType::Practice:
            return TEXT("учёба");
        default:
            return TEXT("прочее");
        }
    }

    FString TopOf(const TMap<FString, double>& Map, double Total, int32 Count)
    {
        TArray<TPair<FString, double>> Sorted;
        for (const TPair<FString, double>& Pair : Map)
        {
            Sorted.Add(Pair);
        }
        Sorted.Sort([](const TPair<FString, double>& A, const TPair<FString, double>& B) { return A.Value > B.Value; });
        FString Text;
        for (int32 I = 0; I < FMath::Min(Count, Sorted.Num()); ++I)
        {
            Text += FString::Printf(TEXT("%s %.0f%%; "), *Sorted[I].Key, 100.0 * Sorted[I].Value / FMath::Max(1.0e-6, Total));
        }
        return Text;
    }

    bool OpenAt(const FAffordance& A, float Hour)
    {
        if (FMath::IsNearlyEqual(A.AvailableFromHour, A.AvailableToHour))
        {
            return true;
        }
        if (A.AvailableFromHour < A.AvailableToHour)
        {
            return Hour >= A.AvailableFromHour && Hour < A.AvailableToHour;
        }
        return Hour >= A.AvailableFromHour || Hour < A.AvailableToHour;
    }

    FAffordance Make(EActionType Action, const FString& Label, const FString& Category, float Duration, float Effort,
        std::initializer_list<TPair<ENeedType, float>> Promises)
    {
        FAffordance A;
        A.Action = Action;
        A.Label = Label;
        A.Duration = Duration;
        A.EffortCost = Effort;
        A.Key = FName(*FString::Printf(TEXT("%s@%s"), *Label, *Category));
        A.CategoryKey = A.Key;
        for (const TPair<ENeedType, float>& P : Promises)
        {
            FNeedPromise Promise;
            Promise.Need = P.Key;
            Promise.Amount = P.Value;
            A.Promises.Add(Promise);
        }
        return A;
    }

    FAffordance DealOf(FName Deal, EResourceKind Kind, EActionType Action, const FString& Label, const FString& Category,
        float Duration, float Effort, std::initializer_list<TPair<ENeedType, float>> Promises)
    {
        FAffordance A = Make(Action, Label, Category, Duration, Effort, Promises);
        A.Deal = Deal;
        A.DealKind = Kind;
        A.Key = FName(*FString::Printf(TEXT("%s:%d@%s"), *Deal.ToString(), static_cast<int32>(Kind), *Category));
        A.CategoryKey = A.Key;
        return A;
    }

    FAffordance WorkOf(const FVillageWork& W)
    {
        FAffordance A;
        A.Action = W.Output == EResourceKind::CookedFood || W.Output == EResourceKind::Bread ? EActionType::Cook : EActionType::Work;
        A.Label = W.Label;
        A.Craft = W.Id;
        A.Key = W.Id;
        A.CategoryKey = W.Id;
        A.Duration = W.Duration;
        A.EffortCost = W.Effort;
        A.Difficulty = W.Difficulty;
        A.RequiredSkill = W.Skill;
        auto Add = [&A](ENeedType Need, float Amount)
        {
            FNeedPromise P;
            P.Need = Need;
            P.Amount = Amount;
            A.Promises.Add(P);
        };
        Add(ENeedType::Competence, 0.16f);
        Add(ENeedType::Meaning, 0.1f);
        Add(ENeedType::Achievement, 0.14f);
        Add(ENeedType::Esteem, 0.08f);
        if (FVillage::IsFood(W.Output) || W.Output == EResourceKind::Firewood || W.Output == EResourceKind::Water)
        {
            Add(ENeedType::Safety, 0.12f);
        }
        if (W.Output == EResourceKind::CookedFood)
        {
            Add(ENeedType::Hunger, 0.12f);
        }
        return A;
    }

    void WorksAt(EFurnitureType Station, TArray<FAffordance>& Out)
    {
        for (const FVillageWork& W : FVillage::Works())
        {
            if (static_cast<EFurnitureType>(W.Station) == Station)
            {
                Out.Add(WorkOf(W));
            }
        }
    }

    TMap<int32, TArray<FAffordance>> GPlaceCatalog;

    void BuildPlaceCatalog()
    {
        GPlaceCatalog.Reset();
        UAffordanceComponent* Maker = NewObject<UAffordanceComponent>(GetTransientPackage());
        Maker->AddToRoot();
        for (EPlaceKind Kind : { EPlaceKind::Home, EPlaceKind::Field, EPlaceKind::Forest, EPlaceKind::River, EPlaceKind::Market,
                                 EPlaceKind::Church, EPlaceKind::Castle })
        {
            Maker->Offers.Reset();
            Maker->Category = HumanText::Place(Kind);
            Maker->MakeTypicalFor(Kind);
            TArray<FAffordance>& Out = GPlaceCatalog.FindOrAdd(static_cast<int32>(Kind));
            for (FAffordance A : Maker->Offers)
            {
                if (!A.Deal.IsNone())
                {
                    A.Key = FName(*FString::Printf(TEXT("%s:%d@%s"), *A.Deal.ToString(), static_cast<int32>(A.DealKind), *Maker->Category));
                }
                else
                {
                    A.Key = FName(*FString::Printf(TEXT("%s@%s"), *A.Label, *Maker->Category));
                }
                A.CategoryKey = A.Key;
                Out.Add(A);
            }
        }
        Maker->RemoveFromRoot();
    }

    void PlaceOffers(EPlaceKind Kind, TArray<FAffordance>& Out)
    {
        if (const TArray<FAffordance>* Found = GPlaceCatalog.Find(static_cast<int32>(Kind)))
        {
            Out.Append(*Found);
        }
    }

    void HomeOffers(TArray<FAffordance>& Out)
    {
        using N = ENeedType;
        PlaceOffers(EPlaceKind::Home, Out);
        FAffordance Sleep = Make(EActionType::Sleep, TEXT("лечь спать"), TEXT("постель"), 0.0f, 0.0f,
            { { N::Sleep, 1.0f }, { N::Comfort, 0.5f }, { N::Shelter, 0.3f } });
        Sleep.bNeedsLying = true;
        Out.Add(Sleep);
        FAffordance Soup = Make(EActionType::Eat, TEXT("похлебать щей или каши"), TEXT("стол"), 900.0f, 0.02f,
            { { N::Hunger, 0.8f }, { N::Comfort, 0.15f } });
        Soup.Requires = EResourceKind::CookedFood;
        Soup.RequiresAmount = 1.0f;
        Soup.Label = TEXT("похлебать щей или каши");
        Out.Add(Soup);
        FAffordance Bread = Make(EActionType::Eat, TEXT("поесть хлеба"), TEXT("стол"), 600.0f, 0.02f,
            { { N::Hunger, 0.6f }, { N::Comfort, 0.1f } });
        Bread.Requires = EResourceKind::Bread;
        Bread.RequiresAmount = 1.0f;
        Out.Add(Bread);
        FAffordance Raw = Make(EActionType::Eat, TEXT("погрызть сырой репы"), TEXT("стол"), 500.0f, 0.02f, { { N::Hunger, 0.35f } });
        Raw.Requires = EResourceKind::RawFood;
        Raw.RequiresAmount = 1.0f;
        Out.Add(Raw);
        FAffordance Tavli = Make(EActionType::Entertain, TEXT("сыграть в тавлеи"), TEXT("стол"), 1800.0f, 0.08f,
            { { N::SocialContact, 0.25f }, { N::Competence, 0.1f }, { N::Novelty, 0.15f } });
        Tavli.AvailableFromHour = 17.0f;
        Tavli.AvailableToHour = 22.0f;
        Out.Add(Tavli);
        FAffordance Warm = Make(EActionType::Rest, TEXT("полежать на тёплой печи"), TEXT("печь"), 1500.0f, 0.0f,
            { { N::Comfort, 0.5f }, { N::Health, 0.05f } });
        Warm.bNeedsLying = true;
        Out.Add(Warm);
        WorksAt(EFurnitureType::Stove, Out);
        FAffordance Drink = Make(EActionType::Drink, TEXT("зачерпнуть воды ковшом"), TEXT("бочка с водой"), 120.0f, 0.0f, { { N::Thirst, 0.9f } });
        Drink.Requires = EResourceKind::Water;
        Drink.RequiresAmount = 0.25f;
        Out.Add(Drink);
        FAffordance Wash = Make(EActionType::Wash, TEXT("умыться"), TEXT("бочка с водой"), 300.0f, 0.02f, { { N::Hygiene, 0.5f }, { N::Comfort, 0.1f } });
        Wash.Requires = EResourceKind::Water;
        Wash.RequiresAmount = 0.3f;
        Out.Add(Wash);
        Out.Add(DealOf(TEXT("store"), EResourceKind::Water, EActionType::Observe, TEXT("перелить воду в бочку"), TEXT("бочка с водой"),
            120.0f, 0.03f, { { N::Order, 0.15f } }));
        Out.Add(DealOf(TEXT("store"), EResourceKind::None, EActionType::Observe, TEXT("сложить принесённое в амбар"), TEXT("амбар"),
            150.0f, 0.05f, { { N::Order, 0.2f }, { N::Safety, 0.1f } }));
        FAffordance Pack = DealOf(TEXT("pack"), EResourceKind::None, EActionType::Work, TEXT("набрать на продажу"), TEXT("амбар"),
            200.0f, 0.1f, { { N::Order, 0.05f } });
        Pack.AvailableFromHour = 5.0f;
        Pack.AvailableToHour = 18.0f;
        Out.Add(Pack);
        Out.Add(Make(EActionType::Observe, TEXT("перебрать припасы"), TEXT("амбар"), 900.0f, 0.12f, { { N::Order, 0.35f }, { N::Safety, 0.08f } }));
        FAffordance Sit = Make(EActionType::Rest, TEXT("посидеть на лавке"), TEXT("лавка"), 900.0f, 0.0f, { { N::Comfort, 0.35f } });
        Sit.bNeedsSeat = true;
        Out.Add(Sit);
        Out.Add(Make(EActionType::Reflect, TEXT("помолиться"), TEXT("образа с лампадой"), 600.0f, 0.05f,
            { { N::Meaning, 0.12f }, { N::Order, 0.08f }, { N::Safety, 0.05f }, { N::Belonging, 0.03f } }));
    }

    class FVillager
    {
    public:
        UNeedComponent* Needs = nullptr;
        UPhysiologyComponent* Flesh = nullptr;
        FRandomStream Rng;
        TArray<FSpot> Spots;
        TArray<FNeighbour> People;
        TMap<FName, float> Mastery;
        TMap<EResourceKind, float> Stock;
        TMap<uint32, int32> Tried;
        TMap<uint32, int32> TriedHere;
        TArray<uint32> Recent;
        EResourceKind Carried = EResourceKind::None;
        float CarriedAmount = 0.0f;
        FVector2D At = FVector2D::ZeroVector;
        float Money = 10.0f;
        float Age = 30.0f;
        float Hour = 7.0f;
        int32 Day = 200;
        float Openness = 0.5f;
        float Anxiety = 0.4f;
        int32 Home = 0;
        int32 Members = 4;
        bool bFemale = false;
        EVillageRole Role = EVillageRole::Peasant;
        FName Trade;
        FAffordance OrderTask;
        bool bOrder = false;
        float OrderAt = 0.0f;
        int32 OrderSpot = 0;
        float Clock = 0.0f;
        float NextOrder = 3.0f;
        float Produced = 0.0f;
        int32 PotWindow = -1;
        float WorkedToday = 0.0f;
        int32 WorkDay = -1;
        bool bLiterate = false;
        TMap<FString, FName> Shelf;
        bool bBaby = false;
        float BabyAge = 0.0f;
        float BabyFull = 1.0f;
        bool bBabyAsleep = true;
        float BabyWakeAt = 0.0f;

        bool BabyCrying() const
        {
            if (!bBaby || bBabyAsleep)
            {
                return false;
            }
            const bool bNight = Hour < 5.5f || Hour > 21.5f;
            return BabyFull < 0.4f || (bNight && BabyAge < 1.5f);
        }

        bool IsRoyal() const { return Role >= EVillageRole::King && Role <= EVillageRole::QueenMother; }
        bool IsServant() const { return Role >= EVillageRole::Steward && Role <= EVillageRole::Groom; }

        void Birth(float InAge, bool bInFemale, uint32 Seed)
        {
            Rng.Initialize(static_cast<int32>(Seed));
            Age = InAge;
            bFemale = bInFemale;
            Hour = Rng.FRandRange(5.0f, 8.0f);
            Day = Rng.RandRange(0, 364);
            Openness = Rng.FRandRange(0.1f, 0.9f);
            Anxiety = Rng.FRandRange(0.1f, 0.8f);
            Members = Rng.RandRange(2, 6);
            Mastery.Reset();
            Stock.Reset();
            Tried.Reset();
            TriedHere.Reset();
            Recent.Reset();
            Carried = EResourceKind::None;
            CarriedAmount = 0.0f;
            bOrder = false;
            Clock = 0.0f;
            NextOrder = Rng.FRandRange(1.0f, 5.0f);
            PotWindow = -1;
            Produced = 0.0f;
            bBaby = Age >= 18.0f && Age <= 42.0f && Rng.FRand() < 0.35f;
            BabyAge = Rng.FRandRange(0.0f, 1.4f);
            BabyFull = Rng.FRandRange(0.4f, 1.0f);
            bBabyAsleep = Rng.FRand() < 0.5f;
            BabyWakeAt = Rng.FRandRange(0.5f, 2.5f);

            const float Roll = Rng.FRand();
            Role = EVillageRole::Peasant;
            Trade = NAME_None;
            if (Age >= 16.0f)
            {
                if (Roll < 0.08f)
                {
                    Role = bFemale ? EVillageRole::Queen : EVillageRole::King;
                }
                else if (Roll < 0.2f)
                {
                    Role = bFemale ? EVillageRole::Maid : EVillageRole::Steward;
                }
                else if (Roll < 0.45f)
                {
                    const TArray<FVillageTrade>& Trades = FVillage::Trades();
                    Trade = Trades[Rng.RandRange(0, Trades.Num() - 1)].Skill;
                }
            }
            else if (Age < 13.0f)
            {
                Role = EVillageRole::Child;
            }

            const float Grown = Age >= 16.0f ? 1.0f : (Age >= 12.0f ? 0.6f : (Age >= 6.0f ? 0.25f : 0.0f));
            for (const FVillageTrade& T : FVillage::Trades())
            {
                Mastery.Add(T.Skill, 0.03f * Grown);
            }
            Mastery.Add(TEXT("Masonry"), 0.03f * Grown);
            Mastery.Add(TEXT("Conversation"), 0.5f);
            Mastery.Add(TEXT("Strength"), 0.5f * Grown);
            if (!IsRoyal())
            {
                Mastery.Add(TEXT("Farming"), (bFemale ? 0.55f : 0.65f) * Grown * Rng.FRandRange(0.85f, 1.1f));
                Mastery.Add(TEXT("Cooking"), (bFemale ? 0.65f : 0.3f) * Grown * Rng.FRandRange(0.85f, 1.1f));
                Mastery.Add(TEXT("Woodcutting"), (bFemale ? 0.2f : 0.6f) * Grown * Rng.FRandRange(0.85f, 1.1f));
                Mastery.Add(TEXT("Baking"), (bFemale ? 0.45f : 0.03f) * Grown);
            }
            if (!Trade.IsNone())
            {
                Mastery.Add(Trade, Rng.FRandRange(0.85f, 0.97f));
            }
            if (IsServant())
            {
                Mastery.Add(TEXT("Cooking"), 0.8f);
            }
            bLiterate = IsRoyal() || Role == EVillageRole::Steward || (Age >= 9.0f && (!Trade.IsNone() || Rng.FRand() < (Age < 14.0f ? 0.3f : 0.45f)));
            Mastery.Add(TEXT("Reading"), bLiterate ? (IsRoyal() ? 0.7f : 0.5f) : 0.0f);

            Money = IsRoyal() ? Rng.FRandRange(150.0f, 500.0f) : (Trade.IsNone() ? Rng.FRandRange(0.0f, 20.0f) : Rng.FRandRange(10.0f, 45.0f));
            if (Age < 12.0f)
            {
                Money = Rng.FRandRange(0.0f, 2.0f);
            }
            const float Plenty = IsRoyal() ? 3.0f : Rng.FRandRange(0.6f, 1.6f);
            const bool bWinter = Day < 100 || Day > 285;
            const float Granary = Day > 265 ? Rng.FRandRange(15.0f, 25.0f)
                : (Day < 100 ? Rng.FRandRange(10.0f, 17.0f) : (Day < 195 ? Rng.FRandRange(4.0f, 9.0f) : Rng.FRandRange(5.0f, 10.0f)));
            Stock.Add(EResourceKind::Bread, FMath::RoundToFloat(Rng.FRandRange(0.0f, 4.0f) * Plenty));
            Stock.Add(EResourceKind::CookedFood, FMath::RoundToFloat(Rng.FRandRange(0.0f, 3.0f) * Plenty));
            Stock.Add(EResourceKind::Grain, FMath::RoundToFloat(Granary * Members * 0.7f * Plenty));
            Stock.Add(EResourceKind::Flour, FMath::RoundToFloat(Rng.FRandRange(0.0f, 4.0f) * Plenty));
            Stock.Add(EResourceKind::RawFood, FMath::RoundToFloat(Granary * Members * 0.3f * Plenty));
            Stock.Add(EResourceKind::Firewood, FMath::RoundToFloat((Rng.FRandRange(0.0f, 10.0f) + (bWinter ? Rng.FRandRange(10.0f, 25.0f) : 0.0f)) * Plenty));
            Stock.Add(EResourceKind::Water, FMath::RoundToFloat(Rng.FRandRange(0.0f, 8.0f)));
            Stock.Add(EResourceKind::Axe, 1.0f);
            Stock.Add(EResourceKind::Sickle, 1.0f);
            Stock.Add(EResourceKind::Rod, Rng.FRand() < 0.25f || Trade == TEXT("Fishing") ? 1.0f : 0.0f);
            Stock.Add(EResourceKind::Needle, 1.0f);
            Stock.Add(EResourceKind::Hammer, Trade == TEXT("Smithing") || Trade == TEXT("Masonry") ? 1.0f : 0.0f);
            Stock.Add(EResourceKind::Shovel, 1.0f);
            Stock.Add(EResourceKind::Cloth, Rng.FRand() < 0.5f ? 1.0f : 0.0f);
            Stock.Add(EResourceKind::Thread, FMath::RoundToFloat(Rng.FRandRange(0.0f, 3.0f)));
            if (Trade == TEXT("Weaving")) { Stock.Add(EResourceKind::Flax, 6.0f); }
            if (Trade == TEXT("Smithing")) { Stock.Add(EResourceKind::Iron, 3.0f); Stock.Add(EResourceKind::IronOre, 4.0f); Stock.Add(EResourceKind::Charcoal, 8.0f); }
            if (Trade == TEXT("Pottery")) { Stock.Add(EResourceKind::Clay, 8.0f); }
            if (Trade == TEXT("Carpentry")) { Stock.Add(EResourceKind::Wood, 4.0f); }
            if (Trade == TEXT("Sewing")) { Stock.Add(EResourceKind::Cloth, 4.0f); Stock.Add(EResourceKind::Thread, 4.0f); }
            if (Trade == TEXT("Herbalism")) { Stock.Add(EResourceKind::Herb, 6.0f); }

            for (FNeedState& N : Needs->Needs)
            {
                N.Satisfaction = Rng.FRandRange(0.35f, 0.95f);
                N.Weight = Rng.FRandRange(0.5f, 1.4f);
            }
            auto Weigh = [this](ENeedType Type, float Weight)
            {
                if (FNeedState* N = Needs->Find(Type))
                {
                    N->Weight = Weight;
                }
            };
            Weigh(ENeedType::Hunger, 1.4f);
            Weigh(ENeedType::Thirst, 1.5f);
            Weigh(ENeedType::Sleep, 1.3f);
            Weigh(ENeedType::Bladder, 1.2f);
            Weigh(ENeedType::Health, 1.0f);
            Flesh->Body = FBodyState();
            Flesh->Hormones = FHormones();
            Flesh->Organs = FOrgans();
            Flesh->Ailments.Reset();
            Flesh->CauseOfDeath.Reset();
            Flesh->BiologicalAge = Age;
            Flesh->bAlive = true;
            Flesh->Body.StomachFullness = Rng.FRandRange(0.4f, 0.9f);
            Flesh->Body.Hydration = Rng.FRandRange(0.5f, 1.0f);
            Flesh->Body.BladderFullness = Rng.FRandRange(0.0f, 0.4f);
            Flesh->Body.Cleanliness = Rng.FRandRange(0.5f, 1.0f);
            Flesh->Body.Pain = 0.0f;
            Needs->DeprivationTime.Init(0.0f, Needs->Needs.Num());

            BuildSpots(Seed);
            At = Spots[Home].At;
        }

        void AddSpot(ESpotKind Kind, float Radius, uint32 Seed, const TArray<FAffordance>& Offers)
        {
            FSpot Spot;
            Spot.Kind = Kind;
            const float Angle = Rng.FRandRange(-PI, PI);
            Spot.At = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
            Spot.Place = FMindSense::Word(FString::Printf(TEXT("%d/%d/%u"), static_cast<int32>(Kind), Spots.Num(), Seed));
            Spot.Offers = Offers;
            Spots.Add(MoveTemp(Spot));
        }

        void BuildSpots(uint32 Seed)
        {
            using N = ENeedType;
            Spots.Reset();
            People.Reset();

            TArray<FAffordance> Offers;
            HomeOffers(Offers);
            Shelf.Reset();
            TArray<FName> HouseTrades;
            if (!Trade.IsNone())
            {
                HouseTrades.Add(Trade);
            }
            if (Rng.FRand() < 0.4f)
            {
                HouseTrades.AddUnique(FVillage::Trades()[Rng.RandRange(0, FVillage::Trades().Num() - 1)].Skill);
            }
            TArray<FName> Books;
            FVillage::HomeBooks(HouseTrades, Books);
            for (const FName& Subject : Books)
            {
                const FTextbook* Book = FLibrary::Find(Subject);
                if (!Book)
                {
                    continue;
                }
                FAffordance Read = Make(EActionType::Study, FString::Printf(TEXT("читать: %s"), *Book->Title), Book->Course, 1800.0f,
                    0.1f + Book->Difficulty * 0.2f, { { N::Competence, 0.45f }, { N::Novelty, 0.3f }, { N::Meaning, 0.2f } });
                Read.RequiredSkill = TEXT("Reading");
                Read.Difficulty = Book->RequiredReading;
                Read.bNeedsSeat = true;
                Read.bNeedsInHand = true;
                Shelf.Add(Read.Label, Subject);
                Offers.Add(Read);
            }
            if (Trade == TEXT("Weaving") || Rng.FRand() < 0.3f)
            {
                WorksAt(EFurnitureType::Loom, Offers);
            }
            WorksAt(EFurnitureType::Table, Offers);
            AddSpot(ESpotKind::Home, 0.0f, Seed, Offers);
            Home = 0;

            Offers.Reset();
            PlaceOffers(EPlaceKind::Field, Offers);
            WorksAt(EFurnitureType::GardenBed, Offers);
            AddSpot(ESpotKind::Field, Rng.FRandRange(600.0f, 1200.0f), Seed, Offers);

            Offers.Reset();
            PlaceOffers(EPlaceKind::Forest, Offers);
            WorksAt(EFurnitureType::Tree, Offers);
            WorksAt(EFurnitureType::Bush, Offers);
            WorksAt(EFurnitureType::Mushrooms, Offers);
            WorksAt(EFurnitureType::WildField, Offers);
            WorksAt(EFurnitureType::StonePile, Offers);
            AddSpot(ESpotKind::Forest, Rng.FRandRange(900.0f, 1600.0f), Seed, Offers);

            Offers.Reset();
            PlaceOffers(EPlaceKind::River, Offers);
            WorksAt(EFurnitureType::FishingSpot, Offers);
            WorksAt(EFurnitureType::ClayPit, Offers);
            AddSpot(ESpotKind::River, Rng.FRandRange(500.0f, 1100.0f), Seed, Offers);

            Offers.Reset();
            Offers.Add(Make(EActionType::Drink, TEXT("напиться у колодца"), TEXT("колодец"), 120.0f, 0.0f, { { N::Thirst, 0.9f } }));
            WorksAt(EFurnitureType::Well, Offers);
            AddSpot(ESpotKind::Well, Rng.FRandRange(100.0f, 400.0f), Seed, Offers);

            Offers.Reset();
            PlaceOffers(EPlaceKind::Market, Offers);
            for (EResourceKind Kind : { EResourceKind::Bread, EResourceKind::Fish, EResourceKind::Grain, EResourceKind::Flour,
                                        EResourceKind::Pot, EResourceKind::Cloth, EResourceKind::Shirt, EResourceKind::Sickle,
                                        EResourceKind::Medicine, EResourceKind::Plank, EResourceKind::Tool })
            {
                FAffordance Buy = DealOf(TEXT("buy"), Kind, EActionType::Work, FString::Printf(TEXT("купить: %s"), *FCraftBook::NameOfKind(Kind)),
                    TEXT("лоток"), 300.0f, 0.05f, {});
                Buy.MoneyCost = FMath::Max(1.0f, FVillage::PriceOf(Kind));
                Buy.AvailableFromHour = 6.0f;
                Buy.AvailableToHour = 20.0f;
                Offers.Add(Buy);
            }
            FAffordance Sell = DealOf(TEXT("sell"), EResourceKind::None, EActionType::Work, TEXT("продать принесённое на торгу"), TEXT("лоток"),
                900.0f, 0.15f, { { N::SocialContact, 0.2f }, { N::Achievement, 0.1f } });
            Sell.RequiredSkill = TEXT("Trade");
            Sell.AvailableFromHour = 6.0f;
            Sell.AvailableToHour = 20.0f;
            Offers.Add(Sell);
            AddSpot(ESpotKind::Market, Rng.FRandRange(400.0f, 900.0f), Seed, Offers);

            Offers.Reset();
            PlaceOffers(EPlaceKind::Church, Offers);
            AddSpot(ESpotKind::Church, Rng.FRandRange(300.0f, 800.0f), Seed, Offers);

            Offers.Reset();
            PlaceOffers(EPlaceKind::Castle, Offers);
            AddSpot(ESpotKind::Castle, IsRoyal() || IsServant() ? 0.0f : Rng.FRandRange(500.0f, 1100.0f), Seed, Offers);

            if (!Trade.IsNone())
            {
                Offers.Reset();
                const EFurnitureType Stations[] = { EFurnitureType::Forge, EFurnitureType::PotteryWheel, EFurnitureType::Workbench,
                                                    EFurnitureType::Sawhorse, EFurnitureType::Loom };
                for (EFurnitureType Station : Stations)
                {
                    for (const FVillageWork& W : FVillage::Works())
                    {
                        if (static_cast<EFurnitureType>(W.Station) == Station && W.Skill == Trade)
                        {
                            Offers.Add(WorkOf(W));
                        }
                    }
                }
                if (Offers.Num() > 0)
                {
                    AddSpot(ESpotKind::Workshop, Rng.FRandRange(50.0f, 200.0f), Seed, Offers);
                }
            }

            if (Age >= 7.0f)
            {
                const TArray<FVillageTrade>& Trades = FVillage::Trades();
                for (int32 M = 0; M < 3; ++M)
                {
                    const FVillageTrade& T = Trades[Rng.RandRange(0, Trades.Num() - 1)];
                    if (T.Skill == Trade)
                    {
                        continue;
                    }
                    FAffordance Learn;
                    Learn.Action = EActionType::Study;
                    Learn.Source = EAffordanceSource::Person;
                    Learn.Deal = TEXT("learn");
                    Learn.RequiredSkill = T.Skill;
                    Learn.Difficulty = 0.1f;
                    Learn.Duration = 2400.0f;
                    Learn.EffortCost = 0.2f;
                    Learn.MoneyCost = 1.0f;
                    Learn.Label = FString::Printf(TEXT("поучиться у мастера (%s)"), T.Deed);
                    Learn.Key = FName(*FString::Printf(TEXT("учиться@%s"), *T.Skill.ToString()));
                    Learn.CategoryKey = Learn.Key;
                    for (const TPair<ENeedType, float>& P : { TPair<ENeedType, float>(N::Competence, 0.4f), TPair<ENeedType, float>(N::Novelty, 0.15f),
                                                             TPair<ENeedType, float>(N::Achievement, 0.12f), TPair<ENeedType, float>(N::Belonging, 0.08f) })
                    {
                        FNeedPromise Promise;
                        Promise.Need = P.Key;
                        Promise.Amount = P.Value;
                        Learn.Promises.Add(Promise);
                    }
                    Offers.Reset();
                    Offers.Add(Learn);
                    AddSpot(ESpotKind::Master, Rng.FRandRange(300.0f, 1100.0f), Seed, Offers);
                    Spots.Last().Trade = T.Skill;
                    Spots.Last().MasterSkill = Rng.FRandRange(0.85f, 0.97f);
                }
            }

            const int32 Folk = Rng.RandRange(4, 9);
            for (int32 P = 0; P < Folk; ++P)
            {
                FNeighbour Other;
                Other.bFamily = P < Members - 1;
                Other.Spot = Other.bFamily ? Home : Rng.RandRange(0, Spots.Num() - 1);
                Other.Closeness = Other.bFamily ? Rng.FRandRange(0.6f, 0.95f) : Rng.FRandRange(0.25f, 0.7f);
                Other.Liking = Other.bFamily ? Rng.FRandRange(0.3f, 0.9f) : Rng.FRandRange(-0.2f, 0.6f);
                Other.Resentment = Rng.FRand() < 0.12f ? Rng.FRandRange(0.2f, 0.7f) : 0.0f;
                Other.Fear = Rng.FRand() < 0.08f ? Rng.FRandRange(0.2f, 0.6f) : 0.0f;
                Other.Word = FMindSense::Word(FString::Printf(TEXT("person/%d/%u"), P, Seed));
                People.Add(Other);
            }
        }

        float Portions() const
        {
            float Total = 0.0f;
            for (const TPair<EResourceKind, float>& Pair : Stock)
            {
                Total += Pair.Value * FVillage::Portions(Pair.Key);
            }
            if (Carried != EResourceKind::None)
            {
                Total += CarriedAmount * FVillage::Portions(Carried);
            }
            return Total;
        }

        float FoodDays() const
        {
            return Portions() / (Members * 3.0f);
        }

        float Wares() const
        {
            return Carried != EResourceKind::None && FVillage::IsSellable(Carried) ? CarriedAmount * FVillage::PriceOf(Carried) : 0.0f;
        }

        bool bHome() const
        {
            return FVector2D::Distance(At, Spots[Home].At) < 20.0f;
        }

        FSelfState Sense() const
        {
            FSelfState Self;
            for (int32 N = 0; N < FMindSense::NeedCount; ++N)
            {
                Self.Needs[N] = Needs->Needs.IsValidIndex(N) ? Needs->Needs[N].Satisfaction : 0.7f;
                Self.Weights[N] = Needs->Needs.IsValidIndex(N) ? Needs->Needs[N].Weight : 1.0f;
            }
            Self.Stamina = Flesh->Body.Stamina;
            Self.Pain = Flesh->Body.Pain;
            Self.Health = Flesh->Body.Health;
            Self.Money = Money;
            Self.Hour = Hour;
            Self.Age = Age;
            Self.Stores = FoodDays();
            Self.Firewood = Stock.FindRef(EResourceKind::Firewood);
            Self.Wares = Wares();
            Self.DayOfYear = Day;
            Self.bAtHome = bHome();
            Self.bEmployed = true;
            Self.bFemale = bFemale;
            Self.bHolding = Carried != EResourceKind::None;
            Self.PeopleNearby = 0;
            for (const FNeighbour& Other : People)
            {
                Self.PeopleNearby += FVector2D::Distance(Spots[Other.Spot].At, At) < 20.0f ? 1 : 0;
            }
            return Self;
        }

        void Household(float Hours)
        {
            const float Days = Hours / 24.0f;
            const int32 Others = FMath::Max(0, Members - 1);
            float Brought = Others * 0.5f * Days;
            if (Age < 14.0f)
            {
                Brought += 0.85f * Days;
            }
            else if (Age >= 65.0f)
            {
                Brought += 0.5f * Days;
            }
            if (IsRoyal())
            {
                Brought += 1.5f * Days;
            }
            else if (IsServant())
            {
                Brought += 0.8f * Days;
            }
            Produced += Brought;
            const EResourceKind Kinds[] = { EResourceKind::Grain, EResourceKind::RawFood, EResourceKind::Fish, EResourceKind::Grain,
                                            EResourceKind::RawFood, EResourceKind::Flour };
            while (Produced >= 1.0f)
            {
                Stock.FindOrAdd(Kinds[Rng.RandRange(0, 5)]) += 1.0f;
                Produced -= 1.0f;
            }

            const bool bMorning = Hour >= 6.0f && Hour < 8.0f;
            const bool bNoon = Hour >= 12.0f && Hour < 13.5f;
            const bool bEvening = Hour >= 18.0f && Hour < 20.0f;
            const int32 Window = Day * 3 + (bNoon ? 1 : 0) + (bEvening ? 2 : 0);
            if ((bMorning || bNoon || bEvening) && Window != PotWindow)
            {
                PotWindow = Window;
                float Need = Members / 3.0f;
                float Cooked = 0.0f;
                for (EResourceKind Kind : { EResourceKind::Grain, EResourceKind::RawFood, EResourceKind::Fish, EResourceKind::Flour })
                {
                    float& Have = Stock.FindOrAdd(Kind);
                    const float Took = FMath::Min(Have, Need);
                    Have -= Took;
                    Need -= Took;
                    Cooked += Took * 3.0f;
                    if (Need <= 0.0f)
                    {
                        break;
                    }
                }
                float& Wood = Stock.FindOrAdd(EResourceKind::Firewood);
                Wood = FMath::Max(0.0f, Wood - 0.3f);
                float& Pot = Stock.FindOrAdd(EResourceKind::CookedFood);
                Pot += Cooked;
                const float OthersAte = FMath::Min(Pot, static_cast<float>(Others));
                Pot -= OthersAte;
            }
            const bool bCold = Day < 100 || Day > 285;
            float& Wood = Stock.FindOrAdd(EResourceKind::Firewood);
            Wood = FMath::Max(0.0f, Wood - (bCold ? 3.0f : 0.6f) * Days + Others * (bCold ? 0.6f : 0.25f) * Days);
            Money = FMath::Max(0.0f, Money - 0.3f * Days);
            if (IsRoyal())
            {
                Money += 3.0f * Days;
            }
        }

        void FeelIdleness(float StepHours)
        {
            if (WorkDay != Day)
            {
                WorkDay = Day;
                WorkedToday = 0.0f;
            }
            FVillage::JudgeDay(Needs, WorkedToday, FVillage::ExpectedWork(Age, Role, Day, Hour), StepHours);
        }

        void HearCrying()
        {
            if (FNeedState* Calm = Needs->Find(ENeedType::Comfort))
            {
                Calm->Satisfaction = FMath::Min(Calm->Satisfaction, 0.4f);
            }
        }

        void Nurse(float StepHours)
        {
            if (!bBaby)
            {
                return;
            }
            BabyAge += StepHours / (24.0f * 365.0f);
            BabyFull = FMath::Max(0.0f, BabyFull - StepHours / 3.5f);
            if (bBabyAsleep && (Clock >= BabyWakeAt || BabyFull < 0.2f))
            {
                bBabyAsleep = false;
            }
            if (!bBabyAsleep && BabyFull > 0.6f && Rng.FRand() < 0.35f * StepHours)
            {
                bBabyAsleep = true;
                BabyWakeAt = Clock + Rng.FRandRange(1.0f, 2.5f);
            }
            if (!bHome())
            {
                if (Members > 2 && (BabyAge >= 1.0f || !bFemale) && Rng.FRand() < 0.5f * StepHours)
                {
                    BabyFull = FMath::Max(BabyFull, 0.7f);
                }
                if (BabyFull <= 0.05f)
                {
                    Needs->Deprive(ENeedType::Belonging, 0.15f * StepHours);
                    Needs->Deprive(ENeedType::Meaning, 0.15f * StepHours);
                }
                return;
            }
            if (BabyCrying())
            {
                HearCrying();
                Needs->Deprive(ENeedType::Order, 0.25f * StepHours);
                if (BabyFull <= 0.05f)
                {
                    Needs->Deprive(ENeedType::Meaning, 0.3f * StepHours);
                    Needs->Deprive(ENeedType::Belonging, 0.2f * StepHours);
                }
            }
        }

        void Live(float Hours, float Exertion, bool bAsleep)
        {
            float Left = Hours * 3600.0f;
            while (Left > 0.0f && Flesh->bAlive)
            {
                const float Step = FMath::Min(Left, 600.0f);
                FPhysiologyDrive Drive;
                Drive.HourOfDay = Hour;
                Drive.Age = Age;
                Drive.bAsleep = bAsleep;
                Drive.SleepPhase = bAsleep ? ESleepPhase::Deep : ESleepPhase::Awake;
                Drive.Exertion = Exertion;
                Drive.Resilience = 1.0f - Anxiety * 0.5f;
                Flesh->Advance(Step, Drive);
                FNeedContext Context;
                Context.bHasHome = true;
                Context.bAtHome = bHome();
                Context.Money = (Money + 0.5f * Wares()) * 3.0f;
                Context.bEmployed = true;
                Context.bAsleep = bAsleep;
                Context.HourOfDay = Hour;
                Context.RoutinePredictability = 0.5f;
                Context.Provision = FVillage::ProvisionFrom(FoodDays(), Stock.FindRef(EResourceKind::Firewood), Day, Age);
                for (const FNeighbour& Other : People)
                {
                    if (FVector2D::Distance(Spots[Other.Spot].At, At) < 20.0f)
                    {
                        ++Context.PeopleNearby;
                        Context.ClosenessNearby = FMath::Max(Context.ClosenessNearby, Other.Closeness);
                    }
                }
                Needs->Advance(Step, Flesh, Context);
                Nurse(Step / 3600.0f);
                FeelIdleness(Step / 3600.0f);
                Household(Step / 3600.0f);
                const float Before = Hour;
                Hour = FMath::Fmod(Hour + Step / 3600.0f, 24.0f);
                if (Hour < Before)
                {
                    Day = (Day + 1) % 365;
                }
                Clock += Step / 3600.0f;
                Left -= Step;
            }
        }

        void Options(TArray<FAffordance>& Out, TArray<FVector2D>& Where, TArray<uint32>& Places, TArray<int32>& Who, TArray<int32>& SpotOf) const
        {
            for (int32 S = 0; S < Spots.Num(); ++S)
            {
                const FSpot& Spot = Spots[S];
                for (const FAffordance& Offer : Spot.Offers)
                {
                    if (!OpenAt(Offer, Hour) || !Allowed(Offer))
                    {
                        continue;
                    }
                    Out.Add(Offer);
                    Where.Add(Spot.At);
                    Places.Add(Spot.Place);
                    Who.Add(INDEX_NONE);
                    SpotOf.Add(S);
                }
            }
            auto Self = [&](const FAffordance& A)
            {
                Out.Add(A);
                Where.Add(At);
                Places.Add(0);
                Who.Add(INDEX_NONE);
                SpotOf.Add(INDEX_NONE);
            };
            using N = ENeedType;
            FAffordance Wander;
            Wander.Action = EActionType::Wander;
            Wander.Source = EAffordanceSource::Self;
            Wander.Duration = 600.0f;
            Wander.EffortCost = 0.05f;
            Wander.Label = TEXT("пройтись");
            Wander.Key = TEXT("Wander@Self");
            Wander.CategoryKey = Wander.Key;
            FNeedPromise New;
            New.Need = N::Novelty;
            New.Amount = 0.25f;
            Wander.Promises.Add(New);
            Self(Wander);
            FAffordance Rest;
            Rest.Action = EActionType::Rest;
            Rest.Source = EAffordanceSource::Self;
            Rest.Duration = 300.0f;
            Rest.Label = TEXT("постоять, передохнуть");
            Rest.Key = TEXT("Rest@Self");
            Rest.CategoryKey = Rest.Key;
            FNeedPromise Easy;
            Easy.Need = N::Comfort;
            Easy.Amount = 0.2f;
            Rest.Promises.Add(Easy);
            Self(Rest);

            if (bOrder && IsServant())
            {
                FAffordance Task = OrderTask;
                for (const TPair<N, float>& P : { TPair<N, float>(N::Belonging, 0.35f), TPair<N, float>(N::Order, 0.5f),
                                                 TPair<N, float>(N::Safety, 0.3f), TPair<N, float>(N::Esteem, 0.1f) })
                {
                    FNeedPromise Promise;
                    Promise.Need = P.Key;
                    Promise.Amount = P.Value;
                    Task.Promises.Add(Promise);
                }
                Task.Label += TEXT(" (велели)");
                const int32 TaskSpot = FMath::Clamp(OrderSpot, 0, Spots.Num() - 1);
                Out.Add(Task);
                Where.Add(Spots[TaskSpot].At);
                Places.Add(Spots[TaskSpot].Place);
                Who.Add(-2);
                SpotOf.Add(TaskSpot);
            }
            if (IsRoyal() && Age >= 16.0f)
            {
                FAffordance Order;
                Order.Action = EActionType::Talk;
                Order.Source = EAffordanceSource::Person;
                Order.Duration = 240.0f;
                Order.EffortCost = 0.05f;
                Order.Deal = TEXT("order");
                Order.Label = TEXT("распорядиться");
                Order.Key = TEXT("велеть@слуга");
                Order.CategoryKey = Order.Key;
                for (const TPair<N, float>& P : { TPair<N, float>(N::Order, 0.35f), TPair<N, float>(N::Esteem, 0.15f),
                                                 TPair<N, float>(N::Safety, 0.1f), TPair<N, float>(N::Autonomy, 0.1f) })
                {
                    FNeedPromise Promise;
                    Promise.Need = P.Key;
                    Promise.Amount = P.Value;
                    Order.Promises.Add(Promise);
                }
                Self(Order);
            }

            if (bBaby)
            {
                auto Care = [&](FName Deal, const TCHAR* Label, const TCHAR* Key, float Duration,
                                std::initializer_list<TPair<N, float>> Gains)
                {
                    FAffordance Tend;
                    Tend.Action = EActionType::Help;
                    Tend.Source = EAffordanceSource::Person;
                    Tend.Duration = Duration;
                    Tend.EffortCost = 0.05f;
                    Tend.Deal = Deal;
                    Tend.Label = Label;
                    Tend.Key = Key;
                    Tend.CategoryKey = Tend.Key;
                    for (const TPair<N, float>& G : Gains)
                    {
                        FNeedPromise Promise;
                        Promise.Need = G.Key;
                        Promise.Amount = G.Value;
                        Tend.Promises.Add(Promise);
                    }
                    Out.Add(Tend);
                    Where.Add(Spots[Home].At);
                    Places.Add(Spots[Home].Place);
                    Who.Add(-3);
                    SpotOf.Add(Home);
                };
                if (BabyFull < 0.6f)
                {
                    Care(TEXT("feed"), TEXT("покормить дитя"), TEXT("кормить@дитя"), 600.0f,
                        { TPair<N, float>(N::Meaning, bFemale ? 0.45f : 0.3f), TPair<N, float>(N::Belonging, 0.3f),
                          TPair<N, float>(N::Intimacy, bFemale ? 0.35f : 0.2f) });
                }
                if (!bBabyAsleep && BabyAge < 1.5f)
                {
                    Care(TEXT("cradle"), TEXT("уложить дитя спать"), TEXT("укачать@дитя"), 480.0f,
                        { TPair<N, float>(N::Belonging, 0.2f), TPair<N, float>(N::Meaning, 0.2f), TPair<N, float>(N::Order, 0.1f) });
                }
            }

            for (int32 P = 0; P < People.Num(); ++P)
            {
                const FNeighbour& Other = People[P];
                const FVector2D Spot = Spots[Other.Spot].At;
                if (FVector2D::Distance(Spot, At) > 700.0f || Hour < 6.0f || Hour > 22.0f)
                {
                    continue;
                }
                FAffordance Talk;
                Talk.Action = EActionType::Talk;
                Talk.Source = EAffordanceSource::Person;
                Talk.Duration = 900.0f;
                Talk.EffortCost = 0.08f;
                Talk.Label = TEXT("поговорить");
                Talk.RequiredSkill = TEXT("Conversation");
                Talk.Difficulty = 0.15f;
                Talk.CategoryKey = FName(Other.bFamily ? TEXT("поговорить@родня") : (Other.Closeness > 0.45f ? TEXT("поговорить@друг") : TEXT("поговорить@знакомый")));
                FNeedPromise Contact;
                Contact.Need = N::SocialContact;
                Contact.Amount = 0.45f;
                Talk.Promises.Add(Contact);
                FNeedPromise Belong;
                Belong.Need = N::Belonging;
                Belong.Amount = Other.Closeness * 0.5f;
                Talk.Promises.Add(Belong);
                FNeedPromise Close;
                Close.Need = N::Intimacy;
                Close.Amount = Other.Closeness * 0.4f;
                Talk.Promises.Add(Close);
                Talk.Risk = Other.Fear * 0.6f;
                Out.Add(Talk);
                Where.Add(Spot);
                Places.Add(Other.Word);
                Who.Add(P);
                SpotOf.Add(Other.Spot);
            }
        }

        bool Allowed(const FAffordance& A) const
        {
            if (A.MoneyCost > 0.0f && A.MoneyCost > Money + 0.01f)
            {
                return false;
            }
            if (!bLiterate && A.RequiredSkill == TEXT("Reading"))
            {
                return false;
            }
            if (A.Requires != EResourceKind::None && !Has(A.Requires, A.RequiresAmount))
            {
                return false;
            }
            if (!A.Craft.IsNone())
            {
                if (const FVillageWork* W = FVillage::FindWork(A.Craft))
                {
                    if (!Has(W->Input, W->InputAmount) || !Has(W->Fuel, W->FuelAmount))
                    {
                        return false;
                    }
                }
            }
            if (A.Deal == TEXT("royal"))
            {
                return IsRoyal();
            }
            if (A.Deal == TEXT("serve"))
            {
                return IsServant();
            }
            if (A.Deal == TEXT("petition"))
            {
                return !IsRoyal() && !IsServant();
            }
            if (A.Deal == TEXT("store"))
            {
                return Carried != EResourceKind::None && (A.DealKind == EResourceKind::None ? Carried != EResourceKind::Water : Carried == A.DealKind);
            }
            if (A.Deal == TEXT("pack"))
            {
                return Carried == EResourceKind::None;
            }
            if (A.Deal == TEXT("sell"))
            {
                return Carried != EResourceKind::None && FVillage::IsSellable(Carried);
            }
            if (A.Deal == TEXT("buy"))
            {
                return Carried == EResourceKind::None || Carried == A.DealKind;
            }
            if (!A.Craft.IsNone())
            {
                if (const FVillageWork* W = FVillage::FindWork(A.Craft))
                {
                    if (!FVillage::InSeason(*W, Day))
                    {
                        return false;
                    }
                    if (W->Id == TEXT("v_harvest"))
                    {
                        return Day >= 200 && Day <= 255;
                    }
                }
            }
            return true;
        }

        FOptionView View(const FAffordance& A, const FVector2D& Where, uint32 Place, int32 Person) const
        {
            FOptionView V;
            V.Action = A.Action;
            V.Source = A.Source;
            V.Category = FMindSense::Word(A.CategoryKey.ToString());
            V.Place = Place;
            V.DistanceM = FVector2D::Distance(At, Where);
            V.DurationMin = A.Duration > 0.0f ? A.Duration / 60.0f : 420.0f;
            V.MoneyCost = A.MoneyCost;
            V.MoneyPerHour = A.MoneyGainPerHour;
            V.Effort = A.EffortCost;
            V.Risk = A.Risk;
            V.Difficulty = A.Difficulty;
            const float* Skill = A.RequiredSkill.IsNone() ? nullptr : Mastery.Find(A.RequiredSkill);
            V.Mastery = A.RequiredSkill.IsNone() ? 1.0f : (Skill ? *Skill : 0.2f);
            if (People.IsValidIndex(Person))
            {
                V.Closeness = People[Person].Closeness;
                V.Liking = People[Person].Liking;
                V.Resentment = People[Person].Resentment;
                V.Fear = People[Person].Fear;
            }
            else if (Person == -3)
            {
                V.Closeness = 0.9f;
                V.Liking = 0.8f;
            }
            const int32* Count = Tried.Find(V.Category);
            V.TimesTried = Count ? *Count : 0;
            const int32* Here = TriedHere.Find(HashCombine(V.Category, Place));
            V.TimesHere = Here ? *Here : 0;
            for (uint32 Key : Recent)
            {
                V.RecentRepeats += Key == V.Category ? 1 : 0;
            }
            EResourceKind Need = A.Requires;
            float NeedAmount = A.RequiresAmount;
            if (!A.Craft.IsNone())
            {
                if (const FVillageWork* W = FVillage::FindWork(A.Craft))
                {
                    Need = W->Input;
                    NeedAmount = W->InputAmount;
                }
            }
            if (Need != EResourceKind::None)
            {
                V.HaveRequired = Has(Need, NeedAmount) ? 1 : -1;
            }
            V.bProduces = A.Produces != EResourceKind::None || !A.Craft.IsNone();
            V.bInHand = A.bNeedsInHand;
            V.bSeat = A.bNeedsSeat;
            V.bLying = A.bNeedsLying || A.Action == EActionType::Sleep;
            V.bMine = Place == Spots[Home].Place;
            V.bCraft = !A.Craft.IsNone();
            V.bOpenNow = true;
            return V;
        }

        bool Has(EResourceKind Kind, float Amount) const
        {
            if (Kind == EResourceKind::None || Amount <= 0.0f)
            {
                return true;
            }
            const float InStock = Stock.FindRef(Kind);
            const float InHand = Carried == Kind ? CarriedAmount : 0.0f;
            return InStock + InHand >= Amount - 0.01f;
        }

        void Spend(EResourceKind Kind, float Amount)
        {
            if (Carried == Kind)
            {
                const float Took = FMath::Min(CarriedAmount, Amount);
                CarriedAmount -= Took;
                Amount -= Took;
                if (CarriedAmount <= 0.01f)
                {
                    Carried = EResourceKind::None;
                    CarriedAmount = 0.0f;
                }
            }
            if (Amount > 0.0f)
            {
                float& Have = Stock.FindOrAdd(Kind);
                Have = FMath::Max(0.0f, Have - Amount);
            }
        }

        void Carry(EResourceKind Kind, float Amount)
        {
            if (Carried == Kind)
            {
                CarriedAmount += Amount;
                return;
            }
            if (Carried != EResourceKind::None && bHome())
            {
                Stock.FindOrAdd(Carried) += CarriedAmount;
            }
            Carried = Kind;
            CarriedAmount = Amount;
        }

        bool Settle(const FAffordance& A, int32 Spot)
        {
            const FName Deal = A.Deal;
            if (Deal == TEXT("store"))
            {
                if (Carried == EResourceKind::None)
                {
                    return false;
                }
                Stock.FindOrAdd(Carried) += CarriedAmount;
                Carried = EResourceKind::None;
                CarriedAmount = 0.0f;
                return true;
            }
            if (Deal == TEXT("pack"))
            {
                EResourceKind Best = EResourceKind::None;
                float BestValue = 0.0f;
                float BestAmount = 0.0f;
                for (const TPair<EResourceKind, float>& Pair : Stock)
                {
                    if (!FVillage::IsSellable(Pair.Key))
                    {
                        continue;
                    }
                    const float Keep = FVillage::IsFood(Pair.Key) ? 4.0f : 0.0f;
                    const float Spare = FMath::FloorToFloat(FMath::Min(4.0f, Pair.Value - Keep));
                    const float Value = Spare * FVillage::PriceOf(Pair.Key);
                    if (Spare >= 1.0f && Value > BestValue && !(FVillage::IsTool(Pair.Key) && Pair.Value <= 1.0f))
                    {
                        Best = Pair.Key;
                        BestValue = Value;
                        BestAmount = Spare;
                    }
                }
                if (Best == EResourceKind::None)
                {
                    return false;
                }
                Stock.FindOrAdd(Best) -= BestAmount;
                Carry(Best, BestAmount);
                return true;
            }
            if (Deal == TEXT("sell"))
            {
                if (Carried == EResourceKind::None || !FVillage::IsSellable(Carried))
                {
                    return false;
                }
                const float Skill = Mastery.FindRef(TEXT("Trade"));
                const float Price = FMath::Max(1.0f, FMath::RoundToFloat(FVillage::PriceOf(Carried) * (0.9f + 0.3f * Skill)));
                const float Sold = FMath::Min(CarriedAmount, FMath::Max(1.0f, FMath::FloorToFloat(CarriedAmount * Rng.FRandRange(0.5f, 1.0f) + Skill * 0.5f)));
                Money += Sold * Price * Rng.FRandRange(0.7f, 1.0f);
                CarriedAmount -= Sold;
                if (CarriedAmount < 0.5f)
                {
                    Carried = EResourceKind::None;
                    CarriedAmount = 0.0f;
                }
                float& TradeSkill = Mastery.FindOrAdd(TEXT("Trade"));
                TradeSkill = FMath::Clamp(TradeSkill + 0.02f * (1.0f - TradeSkill), 0.0f, 1.0f);
                return Sold >= 1.0f;
            }
            if (Deal == TEXT("buy"))
            {
                if (Money < A.MoneyCost)
                {
                    return false;
                }
                Carry(A.DealKind, 1.0f);
                return true;
            }
            if (Deal == TEXT("learn"))
            {
                const float Teacher = Spots.IsValidIndex(Spot) ? Spots[Spot].MasterSkill : 0.9f;
                float& Skill = Mastery.FindOrAdd(A.RequiredSkill);
                Skill = FMath::Clamp(Skill + FMath::Max(0.0f, Teacher - Skill) * Rng.FRandRange(0.12f, 0.22f), 0.0f, 1.0f);
                return true;
            }
            if (Deal == TEXT("order"))
            {
                const EResourceKind Supplied[] = { EResourceKind::Firewood, EResourceKind::Bread, EResourceKind::Fish, EResourceKind::CookedFood, EResourceKind::Water };
                Stock.FindOrAdd(Supplied[Rng.RandRange(0, 4)]) += Rng.FRandRange(2.0f, 5.0f);
                return true;
            }
            if (Deal == TEXT("serve"))
            {
                Money += 0.4f * A.Duration / 3600.0f;
                return true;
            }
            if (Deal == TEXT("feed"))
            {
                if (!bBaby)
                {
                    return false;
                }
                BabyFull = 1.0f;
                if (BabyAge >= 1.0f)
                {
                    if (Has(EResourceKind::CookedFood, 0.3f))
                    {
                        Spend(EResourceKind::CookedFood, 0.3f);
                    }
                    else
                    {
                        Spend(EResourceKind::Bread, 0.3f);
                    }
                }
                else if (bFemale)
                {
                    Flesh->Body.StomachFullness = FMath::Max(0.0f, Flesh->Body.StomachFullness - 0.08f);
                }
                if (Rng.FRand() < 0.6f)
                {
                    bBabyAsleep = true;
                    BabyWakeAt = Clock + Rng.FRandRange(1.5f, 3.0f);
                }
                return true;
            }
            if (Deal == TEXT("cradle"))
            {
                if (!bBaby)
                {
                    return false;
                }
                const bool bSettles = BabyFull > 0.35f && Rng.FRand() < 0.85f;
                if (bSettles)
                {
                    bBabyAsleep = true;
                    BabyWakeAt = Clock + Rng.FRandRange(2.0f, 4.5f);
                }
                return bSettles;
            }
            return true;
        }

        bool Work(const FAffordance& A, int32 Spot)
        {
            const FVillageWork* W = FVillage::FindWork(A.Craft);
            if (!W)
            {
                return false;
            }
            if (!Has(W->Input, W->InputAmount) || !Has(W->Fuel, W->FuelAmount))
            {
                return false;
            }
            const bool bTooled = W->Tool == EResourceKind::None || Has(W->Tool, 1.0f);
            float Untaught = 1.0f;
            float& Skill = W->Skill.IsNone() ? Untaught : Mastery.FindOrAdd(W->Skill);
            const float Chance = FMath::Clamp(0.3f + Skill * 0.9f + (bTooled ? 0.1f : -0.4f) - W->Difficulty * 0.4f, 0.02f, 0.97f);
            const bool bWorked = Rng.FRand() < Chance;
            if (!bWorked)
            {
                Spend(W->Input, W->InputAmount * 0.5f);
                Spend(W->Fuel, W->FuelAmount * 0.5f);
                Skill = FMath::Clamp(Skill + 0.01f * (1.0f - Skill), 0.0f, 1.0f);
                return false;
            }
            Spend(W->Input, W->InputAmount);
            Spend(W->Fuel, W->FuelAmount);
            if (W->Output != EResourceKind::None)
            {
                const float Yield = FMath::Max(1.0f, FMath::RoundToFloat(W->OutputAmount * FMath::Lerp(0.5f, 1.25f, Skill)));
                const bool bAtHomeStation = Spots.IsValidIndex(Spot) && (Spots[Spot].Kind == ESpotKind::Home || Spots[Spot].Kind == ESpotKind::Workshop);
                if (bAtHomeStation)
                {
                    Stock.FindOrAdd(W->Output) += Yield;
                }
                else
                {
                    Carry(W->Output, Yield);
                }
            }
            Skill = FMath::Clamp(Skill + 0.03f * (1.0f - Skill), 0.0f, 1.0f);
            Needs->Satisfy(ENeedType::Competence, 0.2f);
            Needs->Satisfy(ENeedType::Meaning, 0.12f);
            return true;
        }

        bool Do(const FAffordance& A, const FVector2D& Where, int32 Person, int32 Spot, float& OutHours)
        {
            const float Distance = FVector2D::Distance(At, Where);
            const float Walk = Distance / 1.35f / 3600.0f;
            Live(Walk, 0.35f, false);
            At = Where;
            OutHours = Walk;
            if (!Flesh->bAlive)
            {
                return false;
            }
            if (A.MoneyCost > 0.0f && A.MoneyCost > Money)
            {
                Live(1.0f / 60.0f, 0.05f, false);
                OutHours += 1.0f / 60.0f;
                return false;
            }
            if (A.Requires != EResourceKind::None && !Has(A.Requires, A.RequiresAmount))
            {
                Live(1.0f / 60.0f, 0.05f, false);
                OutHours += 1.0f / 60.0f;
                return false;
            }
            float Lasting = A.Duration / 3600.0f;
            const bool bSleep = A.Action == EActionType::Sleep;
            float Exertion = 0.08f;
            switch (A.Action)
            {
            case EActionType::Work:
            case EActionType::Cook:
                Exertion = 0.35f + A.EffortCost * 0.4f;
                break;
            case EActionType::Exercise:
            case EActionType::Explore:
                Exertion = 0.6f;
                break;
            case EActionType::Rest:
            case EActionType::Reflect:
            case EActionType::Study:
                Exertion = 0.03f;
                break;
            default:
                break;
            }
            if (bSleep)
            {
                if (Flesh->GetSleepPressure(Hour) < 0.3f)
                {
                    Live(1.0f / 3.0f, 0.0f, false);
                    OutHours += 1.0f / 3.0f;
                    return false;
                }
                Lasting = 0.0f;
                while (Lasting < 12.0f && Flesh->bAlive)
                {
                    Live(0.5f, 0.0f, true);
                    Lasting += 0.5f;
                    const float Pressure = Flesh->GetSleepPressure(Hour);
                    const bool bRested = Flesh->Body.SleepDebt < 1.5f;
                    const bool bMorning = Hour > 6.0f && Hour < 11.0f;
                    if ((bRested && Pressure < 0.3f) || (bMorning && Pressure < 0.45f && Lasting > 4.0f)
                        || (Flesh->Body.SleepDebt < 0.5f && Lasting >= 1.0f))
                    {
                        break;
                    }
                    if (bHome() && BabyCrying())
                    {
                        break;
                    }
                }
            }
            else
            {
                Live(Lasting, Exertion, false);
            }
            OutHours += Lasting;

            bool bWorked = true;
            float Scale = 1.0f;
            if (!A.Craft.IsNone())
            {
                bWorked = Work(A, Spot);
                Scale = bWorked ? 1.0f : 0.3f;
            }
            else if (!A.Deal.IsNone())
            {
                bWorked = Settle(A, Spot);
                Scale = bWorked ? 1.0f : 0.2f;
            }
            else if (!A.RequiredSkill.IsNone())
            {
                float& Skill = Mastery.FindOrAdd(A.RequiredSkill, 0.2f);
                const float Chance = FMath::Clamp(0.65f + (Skill - A.Difficulty) * 1.2f, 0.05f, 0.97f);
                bWorked = Rng.FRand() < Chance;
                Scale = bWorked ? FMath::Clamp(0.8f + Chance * 0.4f, 0.8f, 1.3f) : FMath::Clamp(0.2f + Chance * 0.3f, 0.05f, 0.6f);
                Skill = FMath::Clamp(Skill + (bWorked ? 0.05f : 0.02f) * (1.0f - Skill), 0.0f, 1.0f);
            }
            if (const FName* Subject = Shelf.Find(A.Label))
            {
                if (!bLiterate)
                {
                    bWorked = false;
                    Scale = 0.15f;
                }
                else if (bWorked)
                {
                    if (const FTextbook* Book = FLibrary::Find(*Subject))
                    {
                        float& Known = Mastery.FindOrAdd(Book->Skill);
                        if (Known < 0.6f)
                        {
                            Known += (0.6f - Known) * 0.35f;
                        }
                    }
                }
            }
            for (const FNeedPromise& Promise : A.Promises)
            {
                const float Amount = Promise.Amount > 0.0f ? Promise.Amount * Scale : Promise.Amount;
                if (!Flesh->ApplyNeedEffect(Promise.Need, Amount, Lasting * 3600.0f))
                {
                    if (Amount >= 0.0f)
                    {
                        Needs->Satisfy(Promise.Need, Amount);
                    }
                    else
                    {
                        Needs->Deprive(Promise.Need, -Amount);
                    }
                }
            }
            Needs->ReadBody(Flesh, Hour);
            if (bHome() && BabyCrying())
            {
                HearCrying();
            }
            if (FVillage::IsWorkLike(A))
            {
                if (WorkDay != Day)
                {
                    WorkDay = Day;
                    WorkedToday = 0.0f;
                }
                WorkedToday += Lasting;
                FVillage::JudgeDay(Needs, WorkedToday, FVillage::ExpectedWork(Age, Role, Day, Hour), Lasting);
            }
            if (bWorked)
            {
                Money = FMath::Max(0.0f, Money - A.MoneyCost);
                if (A.MoneyGainPerHour > 0.0f)
                {
                    Money += A.MoneyGainPerHour * Lasting;
                }
                if (A.Requires != EResourceKind::None)
                {
                    Spend(A.Requires, A.RequiresAmount);
                }
            }
            if (Person == -2 && bOrder)
            {
                bOrder = false;
                if (bWorked)
                {
                    Money += 1.0f;
                    Needs->Satisfy(ENeedType::Belonging, 0.2f);
                    Needs->Satisfy(ENeedType::Esteem, 0.12f);
                }
            }
            if (People.IsValidIndex(Person))
            {
                People[Person].Closeness = FMath::Clamp(People[Person].Closeness + (bWorked ? 0.02f : -0.01f), 0.0f, 1.0f);
            }
            return bWorked;
        }

        void Orders()
        {
            if (!IsServant())
            {
                return;
            }
            if (bOrder && Clock - OrderAt > 3.0f)
            {
                bOrder = false;
                Needs->Deprive(ENeedType::Safety, 0.3f);
                Needs->Deprive(ENeedType::Esteem, 0.15f);
                Money = FMath::Max(0.0f, Money - 1.0f);
            }
            if (!bOrder && Clock >= NextOrder && Hour > 6.0f && Hour < 20.0f)
            {
                const FVillageWork* Choices[] = { FVillage::FindWork(TEXT("v_firewood")), FVillage::FindWork(TEXT("v_water")),
                                                  FVillage::FindWork(TEXT("v_shchi")), FVillage::FindWork(TEXT("v_bread")),
                                                  FVillage::FindWork(TEXT("v_fish")) };
                const FVillageWork* Pick = Choices[Rng.RandRange(0, 4)];
                if (Pick)
                {
                    OrderTask = WorkOf(*Pick);
                    bOrder = true;
                    OrderAt = Clock;
                    const EFurnitureType Station = static_cast<EFurnitureType>(Pick->Station);
                    OrderSpot = Station == EFurnitureType::Tree ? 2 : (Station == EFurnitureType::FishingSpot ? 3 : (Station == EFurnitureType::Well ? 4 : 0));
                    Needs->Deprive(ENeedType::Order, 0.25f);
                }
                NextOrder = Clock + Rng.FRandRange(3.0f, 7.0f);
            }
        }
    };
}

FString FLifeSchool::Folder()
{
    return FPaths::ProjectSavedDir() / TEXT("LearnedMind") / TEXT("Minds");
}

FString FLifeSchool::CorePath(int32 Kind)
{
    return Folder() / FString::Printf(TEXT("Mind_%d.core"), Kind);
}

void FLifeSchool::Raise(FMindCore& Core, int32 Kind, int32 Days, int32 People, uint32 Seed, FString* OutReport)
{
    if (!Core.IsReady())
    {
        Core.Init(Seed);
    }
    const int32 GameCapacity = Core.Capacity;
    Core.Resize(FMath::Max(GameCapacity, 12000));
    FVillage::SetMedieval(true);
    BuildPlaceCatalog();

    const float Ages[] = { 3.0f, 7.0f, 13.0f, 30.0f, 30.0f, 70.0f, 70.0f };
    const float Age = Ages[FMath::Clamp(Kind, 0, 6)];
    const bool bFemale = Kind == 3 || Kind == 5;

    TArray<FVillager> Folk;
    Folk.SetNum(People);
    for (int32 P = 0; P < People; ++P)
    {
        Folk[P].Needs = NewObject<UNeedComponent>(GetTransientPackage());
        Folk[P].Flesh = NewObject<UPhysiologyComponent>(GetTransientPackage());
        Folk[P].Needs->AddToRoot();
        Folk[P].Flesh->AddToRoot();
        Folk[P].Birth(Age * FMath::FRandRange(0.9f, 1.1f), bFemale, Seed + 7919u * (P + 1));
    }

    FRandomStream Trainer(static_cast<int32>(Seed ^ 0x3c6ef372u));
    const int32 ReportEvery = FMath::Max(1, Days / 15);
    FDayLog Window;
    TArray<TArray<FLesson>> Lessons;
    TArray<FDayLog> Logs;
    Lessons.SetNum(People);
    Logs.SetNum(People);

    for (int32 Day = 0; Day < Days; ++Day)
    {
        const float Explore = FMath::Lerp(0.2f, 0.03f, FMath::Clamp(Day / (0.6f * Days), 0.0f, 1.0f));
        const FMindCore& Frozen = Core;
        ParallelFor(People, [&](int32 P)
        {
            FVillager& Me = Folk[P];
            TArray<FLesson>& Mine = Lessons[P];
            FDayLog& Log = Logs[P];
            Mine.Reset();
            Log = FDayLog();
            float Lived = 0.0f;
            TArray<FAffordance> Offers;
            TArray<FVector2D> Where;
            TArray<uint32> Places;
            TArray<int32> Who;
            TArray<int32> SpotOf;
            TArray<FOptionView> Views;
            while (Lived < 24.0f)
            {
                Me.Orders();
                Offers.Reset();
                Where.Reset();
                Places.Reset();
                Who.Reset();
                SpotOf.Reset();
                Views.Reset();
                Me.Options(Offers, Where, Places, Who, SpotOf);
                if (Offers.Num() == 0)
                {
                    Me.Live(0.25f, 0.02f, false);
                    Lived += 0.25f;
                    continue;
                }
                const FSelfState Before = Me.Sense();
                for (int32 O = 0; O < Offers.Num(); ++O)
                {
                    Views.Add(Me.View(Offers[O], Where[O], Places[O], Who[O]));
                }
                int32 Pick = INDEX_NONE;
                if (Me.Rng.FRand() < Explore)
                {
                    Pick = Me.Rng.RandRange(0, Offers.Num() - 1);
                }
                else
                {
                    const FMindChoice Choice = Frozen.Choose(Before, Views, Me.Openness, Me.Anxiety, 0.0f, Me.Rng, nullptr);
                    Pick = Choice.Index;
                }
                if (!Offers.IsValidIndex(Pick))
                {
                    Pick = 0;
                }
                float Hours = 0.0f;
                const bool bSuccess = Me.Do(Offers[Pick], Where[Pick], Who[Pick], SpotOf[Pick], Hours);
                FSelfState After = Me.Sense();
                const bool bDied = !Me.Flesh->bAlive || Me.Flesh->Body.Health < 0.02f;
                if (bDied)
                {
                    After.Health = 0.0f;
                }
                const uint32 Key = Views[Pick].Category;
                Me.Tried.FindOrAdd(Key) += 1;
                Me.TriedHere.FindOrAdd(HashCombine(Key, Views[Pick].Place)) += 1;
                Me.Recent.Add(Key);
                if (Me.Recent.Num() > 6)
                {
                    Me.Recent.RemoveAt(0);
                }
                FLesson Lesson;
                Lesson.Before = Before;
                Lesson.View = Views[Pick];
                Lesson.After = After;
                Lesson.Hours = FMath::Max(Hours, 1.0f / 60.0f);
                Lesson.bSuccess = bSuccess;
                Lesson.bDied = bDied;
                Mine.Add(Lesson);
                Lived += Lesson.Hours;
                Log.Wellbeing += Frozen.Wellbeing(After) * Lesson.Hours;
                if (bDied)
                {
                    ++Log.Deaths;
                    Log.Causes.FindOrAdd(Me.Flesh->CauseOfDeath.IsEmpty() ? FString(TEXT("истощение")) : Me.Flesh->CauseOfDeath) += 1;
                    Me.Birth(Me.Age, Me.bFemale, Me.Rng.GetUnsignedInt());
                }
                Log.Hours += Lesson.Hours;
                Log.Failures += bSuccess ? 0 : 1;
                ++Log.Decisions;
                Log.Deeds.FindOrAdd(Offers[Pick].Label) += 1;
                Log.Spent.FindOrAdd(ClassOf(Offers[Pick])) += Lesson.Hours;
                if (!bSuccess)
                {
                    Log.Fails.FindOrAdd(Offers[Pick].Label) += 1;
                }
                if (!Offers[Pick].Deal.IsNone())
                {
                    Log.Trades.FindOrAdd(Offers[Pick].Deal.ToString() + (bSuccess ? TEXT("+") : TEXT("-"))) += 1;
                }
                for (int32 N = 0; N < FMindSense::NeedCount; ++N)
                {
                    Log.Needs[N] += After.Needs[N];
                }
                Log.Stores += After.Stores;
                Log.Money += After.Money;
                ++Log.Samples;
            }
        });

        int32 Added = 0;
        for (int32 P = 0; P < People; ++P)
        {
            for (const FLesson& Lesson : Lessons[P])
            {
                Core.Remember(Lesson.Before, Lesson.View, Lesson.After, Lesson.Hours, Lesson.bSuccess, 1.0f, Lesson.bDied);
                ++Added;
            }
            const FDayLog& Log = Logs[P];
            Window.Wellbeing += Log.Wellbeing;
            Window.Hours += Log.Hours;
            Window.Deaths += Log.Deaths;
            Window.Failures += Log.Failures;
            Window.Decisions += Log.Decisions;
            Window.Samples += Log.Samples;
            Window.Stores += Log.Stores;
            Window.Money += Log.Money;
            for (int32 N = 0; N < FMindSense::NeedCount; ++N)
            {
                Window.Needs[N] += Log.Needs[N];
            }
            for (const TPair<FString, int32>& Pair : Log.Deeds)
            {
                Window.Deeds.FindOrAdd(Pair.Key) += Pair.Value;
            }
            for (const TPair<FString, double>& Pair : Log.Spent)
            {
                Window.Spent.FindOrAdd(Pair.Key) += Pair.Value;
            }
            for (const TPair<FString, int32>& Pair : Log.Fails)
            {
                Window.Fails.FindOrAdd(Pair.Key) += Pair.Value;
            }
            for (const TPair<FString, int32>& Pair : Log.Causes)
            {
                Window.Causes.FindOrAdd(Pair.Key) += Pair.Value;
            }
            for (const TPair<FString, int32>& Pair : Log.Trades)
            {
                Window.Trades.FindOrAdd(Pair.Key) += Pair.Value;
            }
        }
        Core.Practice(FMath::Max(8, Added / 6), Trainer);

        if (Day == 0 || (Day + 1) % ReportEvery == 0 || Day + 1 == Days)
        {
            TArray<TPair<FString, int32>> Top;
            for (const TPair<FString, int32>& Pair : Window.Deeds)
            {
                Top.Add(Pair);
            }
            Top.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });
            FString Doing;
            for (int32 I = 0; I < FMath::Min(8, Top.Num()); ++I)
            {
                Doing += FString::Printf(TEXT("%s %d%%; "), *Top[I].Key,
                    FMath::RoundToInt(100.0f * Top[I].Value / FMath::Max(1, Window.Decisions)));
            }
            const float Samples = FMath::Max(1, Window.Samples);
            const FString Line = FString::Printf(TEXT("[день %4d] самочувствие %.3f, смертей %d, неудач %.0f%%, решений в день %.1f | голод %.2f жажда %.2f сон %.2f общение %.2f | запасы %.1f дн, деньги %.0f | ошибка %.4f, ценность %.4f | %s"),
                Day + 1, Window.Wellbeing / FMath::Max(1.0, Window.Hours), Window.Deaths,
                100.0f * Window.Failures / FMath::Max(1, Window.Decisions),
                static_cast<float>(Window.Decisions) / (People * FMath::Max(1, (Day == 0 ? 1 : ReportEvery))),
                Window.Needs[static_cast<int32>(ENeedType::Hunger)] / Samples, Window.Needs[static_cast<int32>(ENeedType::Thirst)] / Samples,
                Window.Needs[static_cast<int32>(ENeedType::Sleep)] / Samples, Window.Needs[static_cast<int32>(ENeedType::SocialContact)] / Samples,
                Window.Stores / Samples, Window.Money / Samples, Core.ModelError, Core.WorthError, *Doing);
            UE_LOG(LogHumanCity, Display, TEXT("MIND %s"), *Line);
            TMap<FString, double> FailShare;
            double FailTotal = 0.0;
            for (const TPair<FString, int32>& Pair : Window.Fails)
            {
                FailShare.Add(Pair.Key, Pair.Value);
                FailTotal += Pair.Value;
            }
            FString Causes;
            for (const TPair<FString, int32>& Pair : Window.Causes)
            {
                Causes += FString::Printf(TEXT("%s %d; "), *Pair.Key, Pair.Value);
            }
            const FString Detail = FString::Printf(TEXT("       время: %s| неудачи: %s| смерти: %s| безопасность %.2f деньги-нужда %.2f смысл %.2f покой %.2f"),
                *TopOf(Window.Spent, Window.Hours, 10), *TopOf(FailShare, FailTotal, 5), *Causes,
                Window.Needs[static_cast<int32>(ENeedType::Safety)] / Samples, Window.Needs[static_cast<int32>(ENeedType::Money)] / Samples,
                Window.Needs[static_cast<int32>(ENeedType::Meaning)] / Samples, Window.Needs[static_cast<int32>(ENeedType::Comfort)] / Samples);
            UE_LOG(LogHumanCity, Display, TEXT("MIND %s"), *Detail);
            FString AllNeeds;
            for (int32 N = 0; N < FMindSense::NeedCount; ++N)
            {
                AllNeeds += FString::Printf(TEXT("%s %.2f; "), *UEnum::GetDisplayValueAsText(static_cast<ENeedType>(N)).ToString(), Window.Needs[N] / Samples);
            }
            UE_LOG(LogHumanCity, Display, TEXT("MIND        нужды: %s"), *AllNeeds);
            FString Deals;
            for (const TPair<FString, int32>& Pair : Window.Trades)
            {
                Deals += FString::Printf(TEXT("%s %d; "), *Pair.Key, Pair.Value);
            }
            UE_LOG(LogHumanCity, Display, TEXT("MIND        сделки: %s"), *Deals);
            if (OutReport)
            {
                *OutReport += Line + TEXT("\n") + Detail + TEXT("\n");
            }
            Window = FDayLog();
        }
    }

    for (int32 P = 0; P < FMath::Min(1, Folk.Num()); ++P)
    {
        FVillager& Me = Folk[P];
        TArray<FAffordance> Offers;
        TArray<FVector2D> Where;
        TArray<uint32> Places;
        TArray<int32> Who;
        TArray<int32> SpotOf;
        Me.Options(Offers, Where, Places, Who, SpotOf);
        const FSelfState Self = Me.Sense();
        TArray<FOptionView> Views;
        for (int32 O = 0; O < Offers.Num(); ++O)
        {
            Views.Add(Me.View(Offers[O], Where[O], Places[O], Who[O]));
        }
        TArray<FMindChoice> All;
        Core.Choose(Self, Views, Me.Openness, Me.Anxiety, 0.0f, Me.Rng, &All);
        All.Sort([](const FMindChoice& A, const FMindChoice& B) { return A.Score > B.Score; });
        UE_LOG(LogHumanCity, Display, TEXT("MIND выбор %d: час %.1f, день %d, запасы %.1f, голод %.2f, дело %.2f, средняя жизнь %.3f"),
            P, Me.Hour, Me.Day, Self.Stores, Self.Needs[static_cast<int32>(ENeedType::Hunger)], Self.Needs[static_cast<int32>(ENeedType::Achievement)], Core.AverageLiving);
        for (int32 I = 0; I < FMath::Min(15, All.Num()); ++I)
        {
            const FMindChoice& C = All[I];
            UE_LOG(LogHumanCity, Display, TEXT("MIND    %6.3f = сейчас %6.3f + потом %6.3f + любопытство %5.3f | %.1f ч, удача %.0f%% | %s"),
                C.Score, C.Gain, C.Future, C.Wonder, C.Hours, C.Success * 100.0f, *Offers[C.Index].Label);
        }
    }

    for (FVillager& Me : Folk)
    {
        Me.Needs->RemoveFromRoot();
        Me.Flesh->RemoveFromRoot();
    }
    Core.Resize(FMath::Min(GameCapacity, 2400));
}
