#include "FurnitureActor.h"
#include "AffordanceComponent.h"
#include "Village.h"
#include "Crafts.h"
#include "CompleteHumanAI.h"
#include "IdentityComponent.h"

namespace
{
    struct FPromiseSeed
    {
        ENeedType Need;
        float Amount;
    };

    EResourceKind BestWares(const TMap<EResourceKind, float>& Stored, float& OutAmount)
    {
        EResourceKind Best = EResourceKind::None;
        float BestValue = 0.0f;
        OutAmount = 0.0f;
        for (const TPair<EResourceKind, float>& Pair : Stored)
        {
            if (!FVillage::IsSellable(Pair.Key) || Pair.Value < 1.0f)
            {
                continue;
            }
            const bool bFoodReserve = FVillage::IsFood(Pair.Key) && Pair.Value < 6.0f;
            const float Spare = bFoodReserve ? 0.0f : Pair.Value - (FVillage::IsFood(Pair.Key) ? 4.0f : 0.0f);
            const float Value = FMath::Min(Spare, 4.0f) * FVillage::PriceOf(Pair.Key);
            if (Spare >= 1.0f && Value > BestValue)
            {
                BestValue = Value;
                Best = Pair.Key;
                OutAmount = FMath::Min(Spare, 4.0f);
            }
        }
        return Best;
    }
}

bool AFurnitureActor::BuildVillageOffers()
{
    if (!FVillage::IsMedieval(this) || !Affordances)
    {
        return false;
    }
    UAffordanceComponent* A = Affordances;
    auto Offer = [A](EActionType Action, const FString& Label, float Duration, float Effort,
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
    auto Hours = [](FAffordance& Item, float From, float To)
    {
        Item.AvailableFromHour = From;
        Item.AvailableToHour = To;
    };
    auto Rename = [this](const TCHAR* Medieval)
    {
        Name = Medieval;
        const FString Shown = Affordances->DisplayName;
        const int32 Mark = Shown.Find(TEXT("№"));
        Affordances->DisplayName = Mark != INDEX_NONE ? Name + TEXT(" ") + Shown.Mid(Mark) : Name;
        Affordances->Category = Name;
    };

    switch (FurnitureType)
    {
    case EFurnitureType::Bed:
    {
        Rename(TEXT("постель"));
        FAffordance& Sleep = Offer(EActionType::Sleep, TEXT("лечь спать"), 0.0f, 0.0f,
            { { ENeedType::Sleep, 1.0f }, { ENeedType::Comfort, 0.5f }, { ENeedType::Shelter, 0.3f } });
        Sleep.bNeedsLying = true;
        Sleep.SetupSeconds = 6.0f;
        FAffordance& Nap = Offer(EActionType::Rest, TEXT("прилечь отдохнуть"), 1500.0f, 0.0f,
            { { ENeedType::Comfort, 0.5f }, { ENeedType::Sleep, 0.1f } });
        Nap.bNeedsLying = true;
        Nap.SetupSeconds = 4.0f;
        Hours(Nap, 12.0f, 17.0f);
        break;
    }

    case EFurnitureType::Table:
    {
        Rename(TEXT("стол"));
        FAffordance& Soup = Offer(EActionType::Eat, TEXT("похлебать щей или каши"), 900.0f, 0.02f,
            { { ENeedType::Hunger, 0.8f }, { ENeedType::Comfort, 0.15f } });
        Soup.Requires = EResourceKind::CookedFood;
        Soup.RequiresAmount = 1.0f;
        Soup.bNeedsSeat = true;
        Soup.SetupSeconds = 5.0f;
        FAffordance& Bread = Offer(EActionType::Eat, TEXT("поесть хлеба"), 600.0f, 0.02f,
            { { ENeedType::Hunger, 0.6f }, { ENeedType::Comfort, 0.1f } });
        Bread.Requires = EResourceKind::Bread;
        Bread.RequiresAmount = 1.0f;
        Bread.bNeedsSeat = true;
        Bread.SetupSeconds = 4.0f;
        FAffordance& Raw = Offer(EActionType::Eat, TEXT("погрызть сырой репы"), 500.0f, 0.02f,
            { { ENeedType::Hunger, 0.35f } });
        Raw.Requires = EResourceKind::RawFood;
        Raw.RequiresAmount = 1.0f;

        // --- Припасы нового хозяйства ---------------------------------------
        // Всё, что теперь умеют делать, должно быть и съедобно. Иначе сыр,
        // масло и копчёности лежали бы в амбаре мёртвым грузом.
        FAffordance& Cheese = Offer(EActionType::Eat, TEXT("поесть сыра"), 600.0f, 0.03f,
            { { ENeedType::Hunger, 0.55f }, { ENeedType::Comfort, 0.18f } });
        Cheese.Requires = EResourceKind::Cheese;
        Cheese.RequiresAmount = 1.0f;
        Cheese.bNeedsSeat = true;
        Cheese.SetupSeconds = 4.0f;

        FAffordance& Cottage = Offer(EActionType::Eat, TEXT("поесть творога"), 500.0f, 0.03f,
            { { ENeedType::Hunger, 0.45f }, { ENeedType::Comfort, 0.12f } });
        Cottage.Requires = EResourceKind::CottageCheese;
        Cottage.RequiresAmount = 1.0f;

        FAffordance& WithButter = Offer(EActionType::Eat, TEXT("намазать хлеб маслом"), 500.0f, 0.03f,
            { { ENeedType::Hunger, 0.5f }, { ENeedType::Comfort, 0.2f } });
        WithButter.Requires = EResourceKind::Butter;
        WithButter.RequiresAmount = 1.0f;
        WithButter.bNeedsSeat = true;

        FAffordance& Smoked = Offer(EActionType::Eat, TEXT("поесть копчёной рыбы"), 500.0f, 0.02f,
            { { ENeedType::Hunger, 0.5f }, { ENeedType::Comfort, 0.15f } });
        Smoked.Requires = EResourceKind::SmokedFish;
        Smoked.RequiresAmount = 1.0f;

        FAffordance& Grud = Offer(EActionType::Eat, TEXT("поесть копчёной грудинки"), 600.0f, 0.03f,
            { { ENeedType::Hunger, 0.6f }, { ENeedType::Comfort, 0.15f } });
        Grud.Requires = EResourceKind::SmokedMeat;
        Grud.RequiresAmount = 1.0f;

        FAffordance& Fruit = Offer(EActionType::Eat, TEXT("поесть яблок"), 400.0f, 0.02f,
            { { ENeedType::Hunger, 0.3f }, { ENeedType::Comfort, 0.12f } });
        Fruit.Requires = EResourceKind::Apple;
        Fruit.RequiresAmount = 1.0f;

        FAffordance& Roots = Offer(EActionType::Eat, TEXT("погрызть моркови"), 400.0f, 0.02f,
            { { ENeedType::Hunger, 0.3f } });
        Roots.Requires = EResourceKind::Carrot;
        Roots.RequiresAmount = 1.0f;

        // --- Напитки ---------------------------------------------------------
        FAffordance& Wine = Offer(EActionType::Entertain, TEXT("выпить вина"), 1200.0f, 0.02f,
            { { ENeedType::Comfort, 0.35f }, { ENeedType::SocialContact, 0.15f }, { ENeedType::Novelty, 0.12f } });
        Wine.Requires = EResourceKind::Wine;
        Wine.RequiresAmount = 1.0f;
        Wine.bNeedsSeat = true;
        Wine.SetupSeconds = 5.0f;
        Hours(Wine, 17.0f, 23.0f);

        FAffordance& Beer = Offer(EActionType::Entertain, TEXT("выпить пива"), 900.0f, 0.02f,
            { { ENeedType::Comfort, 0.3f }, { ENeedType::SocialContact, 0.12f } });
        Beer.Requires = EResourceKind::Beer;
        Beer.RequiresAmount = 1.0f;
        Beer.bNeedsSeat = true;
        Hours(Beer, 17.0f, 23.0f);

        FAffordance& Kvass = Offer(EActionType::Drink, TEXT("испить квасу"), 300.0f, 0.02f,
            { { ENeedType::Thirst, 0.4f }, { ENeedType::Comfort, 0.1f } });
        Kvass.Requires = EResourceKind::Kvass;
        Kvass.RequiresAmount = 1.0f;

        FAffordance& Milk = Offer(EActionType::Drink, TEXT("выпить молока"), 200.0f, 0.01f,
            { { ENeedType::Thirst, 0.35f }, { ENeedType::Hunger, 0.1f } });
        Milk.Requires = EResourceKind::Milk;
        Milk.RequiresAmount = 1.0f;

        FAffordance& Tavli = Offer(EActionType::Entertain, TEXT("сыграть в тавлеи"), 1800.0f, 0.08f,
            { { ENeedType::SocialContact, 0.25f }, { ENeedType::Competence, 0.1f }, { ENeedType::Novelty, 0.15f } });
        Tavli.bNeedsSeat = true;
        Tavli.SetupSeconds = 4.0f;
        Hours(Tavli, 17.0f, 22.0f);
        A->Capacity = 6;
        break;
    }

    case EFurnitureType::Stove:
    {
        Rename(TEXT("печь"));
        FAffordance& Warm = Offer(EActionType::Rest, TEXT("полежать на тёплой печи"), 1500.0f, 0.0f,
            { { ENeedType::Comfort, 0.5f }, { ENeedType::Health, 0.05f } });
        Warm.bNeedsLying = true;
        Warm.SetupSeconds = 5.0f;
        A->Capacity = 2;
        break;
    }

    case EFurnitureType::Barrel:
    {
        Rename(TEXT("бочка с водой"));
        FAffordance& Drink = Offer(EActionType::Drink, TEXT("зачерпнуть воды ковшом"), 120.0f, 0.0f,
            { { ENeedType::Thirst, 0.9f } });
        Drink.Requires = EResourceKind::Water;
        Drink.RequiresAmount = 0.25f;
        FAffordance& Wash = Offer(EActionType::Wash, TEXT("умыться"), 300.0f, 0.02f,
            { { ENeedType::Hygiene, 0.5f }, { ENeedType::Comfort, 0.1f } });
        Wash.Requires = EResourceKind::Water;
        Wash.RequiresAmount = 0.3f;
        FAffordance& Pour = Offer(EActionType::Observe, TEXT("перелить воду в бочку"), 120.0f, 0.03f,
            { { ENeedType::Order, 0.15f } });
        Pour.Deal = TEXT("store");
        Pour.DealKind = EResourceKind::Water;
        A->Capacity = 2;
        break;
    }

    case EFurnitureType::Wardrobe:
    case EFurnitureType::Shed:
    {
        const bool bBarn = FurnitureType == EFurnitureType::Shed;
        Rename(bBarn ? TEXT("амбар") : TEXT("сундук"));
        FAffordance& Keep = Offer(EActionType::Observe, bBarn ? TEXT("сложить принесённое в амбар") : TEXT("убрать в сундук"),
            150.0f, 0.05f, { { ENeedType::Order, 0.2f }, { ENeedType::Safety, 0.1f } });
        Keep.Deal = TEXT("store");
        float Spare = 0.0f;
        const EResourceKind Wares = BestWares(Stored, Spare);
        if (Wares != EResourceKind::None)
        {
            FAffordance& Pack = Offer(EActionType::Work,
                FString::Printf(TEXT("набрать на продажу: %s ×%d"), *FCraftBook::NameOfKind(Wares), FMath::RoundToInt(Spare)),
                200.0f, 0.1f, { { ENeedType::Order, 0.05f } });
            Pack.Deal = TEXT("pack");
            Pack.DealKind = Wares;
            Hours(Pack, 5.0f, 18.0f);
        }
        if (bBarn)
        {
            Offer(EActionType::Observe, TEXT("перебрать припасы"), 900.0f, 0.12f,
                { { ENeedType::Order, 0.35f }, { ENeedType::Safety, 0.08f } });
        }
        else
        {
            Offer(EActionType::Wash, TEXT("переодеться в чистое"), 300.0f, 0.05f,
                { { ENeedType::Hygiene, 0.3f }, { ENeedType::Esteem, 0.1f } });
        }
        A->Capacity = 2;
        break;
    }

    case EFurnitureType::Bench:
    {
        Rename(TEXT("лавка"));
        FAffordance& Sit = Offer(EActionType::Rest, TEXT("посидеть на лавке"), 900.0f, 0.0f,
            { { ENeedType::Comfort, 0.35f } });
        Sit.bNeedsSeat = true;
        Sit.SetupSeconds = 3.0f;
        FAffordance& Sleep = Offer(EActionType::Sleep, TEXT("лечь спать на лавке"), 0.0f, 0.0f,
            { { ENeedType::Sleep, 0.85f }, { ENeedType::Comfort, 0.2f }, { ENeedType::Shelter, 0.3f } });
        Sleep.bNeedsLying = true;
        Sleep.SetupSeconds = 5.0f;
        A->Capacity = 2;
        break;
    }

    case EFurnitureType::Chair:
    {
        Rename(TEXT("табурет"));
        FAffordance& Sit = Offer(EActionType::Rest, TEXT("присесть"), 600.0f, 0.0f, { { ENeedType::Comfort, 0.3f } });
        Sit.bNeedsSeat = true;
        Sit.SetupSeconds = 3.0f;
        break;
    }

    case EFurnitureType::Lamp:
    {
        Rename(TEXT("образа с лампадой"));
        Offer(EActionType::Reflect, TEXT("помолиться"), 600.0f, 0.05f,
            { { ENeedType::Meaning, 0.12f }, { ENeedType::Order, 0.08f }, { ENeedType::Safety, 0.05f }, { ENeedType::Belonging, 0.03f } });
        A->Capacity = 4;
        break;
    }

    case EFurnitureType::Crib:
    {
        Rename(TEXT("колыбель"));
        Offer(EActionType::Help, TEXT("покачать колыбель"), 600.0f, 0.05f,
            { { ENeedType::Belonging, 0.2f }, { ENeedType::Meaning, 0.2f } });
        break;
    }

    case EFurnitureType::Well:
    case EFurnitureType::Spring:
    {
        Rename(FurnitureType == EFurnitureType::Well ? TEXT("колодец") : TEXT("родник"));
        Offer(EActionType::Drink, FurnitureType == EFurnitureType::Well ? TEXT("напиться у колодца") : TEXT("напиться из родника"),
            120.0f, 0.0f, { { ENeedType::Thirst, 0.9f } });
        A->Capacity = 3;
        break;
    }

    case EFurnitureType::MarketStall:
    {
        Rename(TEXT("лоток"));
        TSet<EResourceKind> Seen;
        for (const FStallLot& Lot : Lots)
        {
            if (Lot.Amount < 1.0f || Seen.Contains(Lot.Kind))
            {
                continue;
            }
            Seen.Add(Lot.Kind);
            const float Price = FMath::Max(1.0f, FMath::RoundToFloat(Lot.Price));
            FAffordance& Buy = Offer(EActionType::Work,
                FString::Printf(TEXT("купить: %s — %s"), *FCraftBook::NameOfKind(Lot.Kind), *FVillage::Coins(Price)),
                300.0f, 0.05f, {});
            Buy.Deal = TEXT("buy");
            Buy.DealKind = Lot.Kind;
            Buy.MoneyCost = Price;
            Buy.SetupSeconds = 5.0f;
            Hours(Buy, 6.0f, 20.0f);
        }
        FAffordance& Sell = Offer(EActionType::Work, TEXT("продать принесённое на торгу"), 900.0f, 0.15f,
            { { ENeedType::SocialContact, 0.2f }, { ENeedType::Achievement, 0.1f } });
        Sell.Deal = TEXT("sell");
        Sell.RequiredSkill = TEXT("Trade");
        Sell.Difficulty = 0.1f;
        Hours(Sell, 6.0f, 20.0f);
        Offer(EActionType::Observe, TEXT("поглазеть на товар"), 600.0f, 0.02f,
            { { ENeedType::Novelty, 0.2f }, { ENeedType::SocialContact, 0.1f } });
        A->Capacity = 6;
        break;
    }

    case EFurnitureType::GardenBed:   Rename(TEXT("полоса")); break;
    case EFurnitureType::Tree:        Rename(TEXT("дерево")); break;
    case EFurnitureType::Bush:        Rename(TEXT("куст")); break;
    case EFurnitureType::Mushrooms:   Rename(TEXT("грибное место")); break;
    case EFurnitureType::WildField:   Rename(TEXT("луг")); break;
    case EFurnitureType::StonePile:   Rename(TEXT("каменоломня")); break;
    case EFurnitureType::ClayPit:     Rename(TEXT("копань")); break;
    case EFurnitureType::FishingSpot: Rename(TEXT("рыбное место")); A->Capacity = 3; break;
    case EFurnitureType::Forge:       Rename(TEXT("горн")); break;
    case EFurnitureType::Loom:        Rename(TEXT("ткацкий стан")); break;
    case EFurnitureType::PotteryWheel:Rename(TEXT("гончарный круг")); break;
    case EFurnitureType::Workbench:   Rename(TEXT("верстак")); break;
    case EFurnitureType::Sawhorse:    Rename(TEXT("жернова")); break;
    case EFurnitureType::Kiln:        Rename(TEXT("печь для обжига")); break;
    case EFurnitureType::Cart:        Rename(TEXT("телега")); break;
    case EFurnitureType::Fence:       Rename(TEXT("изгородь")); break;
    case EFurnitureType::Beehive:     Rename(TEXT("борть")); break;

    default:
        return false;
    }
    return true;
}

void AFurnitureActor::RefreshLook()
{
    for (UStaticMeshComponent* P : Parts)
    {
        if (P)
        {
            P->DestroyComponent();
        }
    }
    Parts.Reset();
    Mesh = nullptr;
    BuildLook();
}

float AFurnitureActor::LotsOf(EResourceKind Kind) const
{
    float Total = 0.0f;
    for (const FStallLot& Lot : Lots)
    {
        if (Lot.Kind == Kind)
        {
            Total += Lot.Amount;
        }
    }
    return Total;
}

void AFurnitureActor::PutOnSale(AActor* Seller, EResourceKind Kind, float Amount, float Price)
{
    if (Kind == EResourceKind::None || Amount <= 0.0f)
    {
        return;
    }
    for (FStallLot& Lot : Lots)
    {
        if (Lot.Kind == Kind && Lot.Owner.Get() == Seller)
        {
            Lot.Amount += Amount;
            Lot.Price = Price;
            BuildOffers();
            return;
        }
    }
    FStallLot Lot;
    Lot.Kind = Kind;
    Lot.Amount = Amount;
    Lot.Price = Price;
    Lot.Owner = Seller;
    Lots.Add(Lot);
    BuildOffers();
}

bool AFurnitureActor::BuyFrom(AActor* Buyer, EResourceKind Kind, float Amount, float& OutPaid)
{
    OutPaid = 0.0f;
    int32 Best = INDEX_NONE;
    for (int32 I = 0; I < Lots.Num(); ++I)
    {
        const FStallLot& Lot = Lots[I];
        if (Lot.Kind != Kind || Lot.Amount < Amount || Lot.Owner.Get() == Buyer)
        {
            continue;
        }
        if (Best == INDEX_NONE || Lot.Price < Lots[Best].Price)
        {
            Best = I;
        }
    }
    if (Best == INDEX_NONE)
    {
        return false;
    }
    FStallLot& Lot = Lots[Best];
    Lot.Amount -= Amount;
    OutPaid = FMath::Max(1.0f, FMath::RoundToFloat(Lot.Price)) * Amount;
    if (ACompleteHumanNPC* Seller = Cast<ACompleteHumanNPC>(Lot.Owner.Get()))
    {
        if (Seller->IdentityComponent)
        {
            Seller->IdentityComponent->Money += OutPaid;
        }
    }
    if (Lot.Amount < 0.5f)
    {
        Lots.RemoveAt(Best);
    }
    BuildOffers();
    return true;
}
