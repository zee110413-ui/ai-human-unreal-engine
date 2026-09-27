// AffordanceComponent.cpp

#include "AffordanceComponent.h"
#include "HumanWorldSubsystem.h"
#include "Village.h"
#include "Engine/World.h"

UAffordanceComponent::UAffordanceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UAffordanceComponent::BeginPlay()
{
    Super::BeginPlay();

    if (DisplayName.IsEmpty() && GetOwner())
    {
        DisplayName = GetOwner()->GetName();
    }

    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldMind->RegisterAffordanceSource(this);
        }
    }
}

void UAffordanceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldMind->UnregisterAffordanceSource(this);
        }
    }
    Super::EndPlay(Reason);
}

void UAffordanceComponent::CollectOffers(TArray<FAffordance>& Out) const
{
    if (!IsAvailable())
    {
        return;
    }

    AActor* Owner = GetOwner();
    const FVector Location = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;

    for (const FAffordance& Offer : Offers)
    {
        FAffordance Ready = Offer;
        Ready.Target = Owner;
        Ready.Location = Location;
        Ready.bHasLocation = true;
        Ready.Source = EAffordanceSource::Object;
        Ready.bPrivate = bPrivate;
        Ready.OwnerAnchor = OwnerAnchor;

        // Два ключа. Первый — про эту самую вещь: «поесть в том кафе»
        // и «поесть в этом» — разный опыт. Второй — про вид вещей:
        // по нему опыт переносится на всё подобное, ещё не опробованное.
        if (!Offer.Craft.IsNone())
        {
            Ready.Key = FName(*FString::Printf(TEXT("%s@%s"), *Offer.Craft.ToString(), *DisplayName));
            Ready.CategoryKey = Offer.Craft;
        }
        else if (!Offer.Deal.IsNone())
        {
            const int32 Kind = Offer.Deal == TEXT("pack") ? 0 : static_cast<int32>(Offer.DealKind);
            Ready.Key = FName(*FString::Printf(TEXT("%s:%d@%s"), *Offer.Deal.ToString(), Kind, *DisplayName));
            Ready.CategoryKey = FName(*FString::Printf(TEXT("%s:%d@%s"), *Offer.Deal.ToString(), Kind,
                Category.IsEmpty() ? TEXT("вещь") : *Category));
        }
        else if (!Offer.Label.IsEmpty())
        {
            Ready.Key = FName(*FString::Printf(TEXT("%s@%s"), *Offer.Label, *DisplayName));
            Ready.CategoryKey = FName(*FString::Printf(TEXT("%s@%s"), *Offer.Label, Category.IsEmpty() ? TEXT("вещь") : *Category));
        }
        else
        {
            Ready.Key = FName(*FString::Printf(TEXT("%s@%s"),
                *HumanText::Action(Offer.Action), *DisplayName));

            Ready.CategoryKey = FName(*FString::Printf(TEXT("%s@%s"),
                *HumanText::Action(Offer.Action),
                Category.IsEmpty() ? TEXT("вещь") : *Category));
        }

        if (Ready.Label.IsEmpty())
        {
            Ready.Label = FString::Printf(TEXT("%s — %s"), *HumanText::Action(Offer.Action), *DisplayName);
        }

        Out.Add(Ready);
    }
}

void UAffordanceComponent::AddOffer(EActionType Action, const FString& Label, float Duration)
{
    FAffordance A;
    A.Action = Action;
    A.Label = Label;
    A.Duration = Duration;
    Offers.Add(A);
}

void UAffordanceComponent::AddPromise(ENeedType Need, float Amount)
{
    if (Offers.Num() == 0)
    {
        return;
    }

    FNeedPromise P;
    P.Need = Need;
    P.Amount = Amount;
    Offers.Last().Promises.Add(P);
}

// ---------------------------------------------------------------------------
//  Что такое кафе, дом, работа — с точки зрения МИРА
// ---------------------------------------------------------------------------

void UAffordanceComponent::MakeVillageTypical(EPlaceKind Kind)
{
    struct FSeed
    {
        ENeedType Need;
        float Amount;
    };
    auto Offer = [this](EActionType Action, const TCHAR* Label, float Duration, float Effort,
                        std::initializer_list<FSeed> Promises) -> FAffordance&
    {
        AddOffer(Action, Label, Duration);
        for (const FSeed& P : Promises)
        {
            AddPromise(P.Need, P.Amount);
        }
        Offers.Last().EffortCost = Effort;
        return Offers.Last();
    };
    auto Hours = [](FAffordance& A, float From, float To)
    {
        A.AvailableFromHour = From;
        A.AvailableToHour = To;
    };

    switch (Kind)
    {
    case EPlaceKind::Home:
    {
        FAffordance& Sleep = Offer(EActionType::Sleep, TEXT("лечь спать на лавке"), 0.0f, 0.0f,
            { { ENeedType::Sleep, 0.85f }, { ENeedType::Comfort, 0.25f }, { ENeedType::Shelter, 0.5f } });
        Sleep.bNeedsLying = true;
        Offer(EActionType::UseToilet, TEXT("сходить до ветру"), 240.0f, 0.0f, { { ENeedType::Bladder, 1.0f } });
        Offer(EActionType::Observe, TEXT("подмести избу"), 1200.0f, 0.2f,
            { { ENeedType::Order, 0.5f }, { ENeedType::Hygiene, 0.1f }, { ENeedType::Achievement, 0.1f } });
        FAffordance& Mend = Offer(EActionType::Practice, TEXT("починить одёжу"), 1500.0f, 0.15f,
            { { ENeedType::Order, 0.25f }, { ENeedType::Competence, 0.12f } });
        Mend.RequiredSkill = TEXT("Sewing");
        Mend.Difficulty = 0.15f;
        Mend.bNeedsSeat = true;
        Mend.bNeedsLight = true;
        FAffordance& Tale = Offer(EActionType::Talk, TEXT("посидеть с домашними"), 1500.0f, 0.03f,
            { { ENeedType::Belonging, 0.4f }, { ENeedType::SocialContact, 0.3f }, { ENeedType::Intimacy, 0.2f } });
        Hours(Tale, 18.0f, 22.0f);
        break;
    }

    case EPlaceKind::Market:
        Offer(EActionType::Observe, TEXT("потолкаться на торгу"), 900.0f, 0.03f,
            { { ENeedType::Novelty, 0.3f }, { ENeedType::SocialContact, 0.25f } });
        Offer(EActionType::Talk, TEXT("узнать новости на торгу"), 900.0f, 0.03f,
            { { ENeedType::Novelty, 0.25f }, { ENeedType::Belonging, 0.2f } });
        Capacity = 20;
        break;

    case EPlaceKind::Church:
    case EPlaceKind::Library:
    {
        FAffordance& Service = Offer(EActionType::Reflect, TEXT("отстоять службу"), 5400.0f, 0.1f,
            { { ENeedType::Meaning, 0.35f }, { ENeedType::Belonging, 0.3f }, { ENeedType::Order, 0.25f }, { ENeedType::Safety, 0.08f } });
        Hours(Service, 7.0f, 11.0f);
        Offer(EActionType::Reflect, TEXT("помолиться в церкви"), 900.0f, 0.03f,
            { { ENeedType::Meaning, 0.15f }, { ENeedType::Order, 0.1f }, { ENeedType::Safety, 0.05f } });
        FAffordance& Candle = Offer(EActionType::Reflect, TEXT("поставить свечу"), 300.0f, 0.02f,
            { { ENeedType::Meaning, 0.12f }, { ENeedType::Safety, 0.08f } });
        Candle.MoneyCost = 1.0f;
        Capacity = 30;
        break;
    }

    case EPlaceKind::Castle:
    case EPlaceKind::TownHall:
    {
        FAffordance& Serve = Offer(EActionType::Work, TEXT("служить при дворе"), 5400.0f, 0.3f,
            { { ENeedType::Money, 0.3f }, { ENeedType::Belonging, 0.3f }, { ENeedType::Order, 0.25f }, { ENeedType::Comfort, -0.1f } });
        Serve.Deal = TEXT("serve");
        Hours(Serve, 6.0f, 21.0f);
        FAffordance& Petition = Offer(EActionType::Talk, TEXT("бить челом королю"), 900.0f, 0.1f,
            { { ENeedType::Order, 0.3f }, { ENeedType::Safety, 0.2f }, { ENeedType::Esteem, 0.1f } });
        Petition.Deal = TEXT("petition");
        Hours(Petition, 10.0f, 16.0f);
        FAffordance& Court = Offer(EActionType::Work, TEXT("править: судить и рядить"), 3600.0f, 0.2f,
            { { ENeedType::Esteem, 0.3f }, { ENeedType::Order, 0.35f }, { ENeedType::Meaning, 0.25f } });
        Court.Deal = TEXT("royal");
        Hours(Court, 9.0f, 17.0f);
        FAffordance& Feast = Offer(EActionType::Celebrate, TEXT("пировать в палатах"), 3600.0f, 0.05f,
            { { ENeedType::Belonging, 0.35f }, { ENeedType::Comfort, 0.3f }, { ENeedType::Esteem, 0.1f } });
        Feast.Deal = TEXT("royal");
        Hours(Feast, 18.0f, 22.0f);
        FAffordance& Sword = Offer(EActionType::Exercise, TEXT("поупражняться с мечом"), 2400.0f, 0.45f,
            { { ENeedType::Competence, 0.25f }, { ENeedType::Esteem, 0.15f }, { ENeedType::Health, 0.15f }, { ENeedType::Comfort, -0.15f } });
        Sword.Deal = TEXT("royal");
        Sword.RequiredSkill = TEXT("Strength");
        Hours(Sword, 7.0f, 18.0f);
        FAffordance& Stitch = Offer(EActionType::Practice, TEXT("вышивать у окна"), 2400.0f, 0.1f,
            { { ENeedType::Beauty, 0.35f }, { ENeedType::Competence, 0.2f }, { ENeedType::Meaning, 0.1f } });
        Stitch.Deal = TEXT("royal");
        Stitch.RequiredSkill = TEXT("Sewing");
        Stitch.bNeedsSeat = true;
        Stitch.bNeedsLight = true;
        FAffordance& Watch = Offer(EActionType::Observe, TEXT("стоять на страже у ворот"), 5400.0f, 0.25f,
            { { ENeedType::Order, 0.3f }, { ENeedType::Safety, 0.2f }, { ENeedType::Belonging, 0.15f } });
        Watch.Deal = TEXT("serve");
        Capacity = 20;
        break;
    }

    case EPlaceKind::Field:
        Offer(EActionType::Observe, TEXT("поглядеть, как поспевает хлеб"), 600.0f, 0.05f,
            { { ENeedType::Order, 0.2f }, { ENeedType::Safety, 0.08f }, { ENeedType::Beauty, 0.1f } });
        Offer(EActionType::UseToilet, TEXT("отойти до ветру за межу"), 240.0f, 0.0f, { { ENeedType::Bladder, 1.0f } });
        Capacity = 20;
        break;

    case EPlaceKind::Forest:
        Offer(EActionType::Wander, TEXT("побродить по лесу"), 1500.0f, 0.1f,
            { { ENeedType::Beauty, 0.3f }, { ENeedType::Novelty, 0.2f }, { ENeedType::Autonomy, 0.1f } });
        Offer(EActionType::UseToilet, TEXT("отойти до ветру в кусты"), 240.0f, 0.0f, { { ENeedType::Bladder, 1.0f } });
        Capacity = 20;
        break;

    case EPlaceKind::River:
    {
        Offer(EActionType::Drink, TEXT("напиться из реки"), 120.0f, 0.02f, { { ENeedType::Thirst, 0.8f } });
        Offer(EActionType::UseToilet, TEXT("отойти до ветру в камыши"), 240.0f, 0.0f, { { ENeedType::Bladder, 1.0f } });
        FAffordance& Bathe = Offer(EActionType::Wash, TEXT("искупаться в реке"), 1500.0f, 0.15f,
            { { ENeedType::Hygiene, 0.9f }, { ENeedType::Comfort, 0.25f }, { ENeedType::Health, 0.05f } });
        Hours(Bathe, 9.0f, 20.0f);
        Offer(EActionType::Wash, TEXT("постирать бельё на реке"), 1800.0f, 0.25f,
            { { ENeedType::Order, 0.3f }, { ENeedType::Hygiene, 0.25f } });
        Capacity = 20;
        break;
    }

    case EPlaceKind::Forge:
        Offer(EActionType::Observe, TEXT("поглядеть, как куют"), 600.0f, 0.02f,
            { { ENeedType::Novelty, 0.2f }, { ENeedType::Competence, 0.05f } });
        break;

    case EPlaceKind::Mill:
        Offer(EActionType::Talk, TEXT("потолковать у жерновов"), 600.0f, 0.02f,
            { { ENeedType::SocialContact, 0.2f }, { ENeedType::Novelty, 0.1f } });
        break;

    default:
        Offer(EActionType::Explore, TEXT("осмотреться"), 600.0f, 0.1f, { { ENeedType::Novelty, 0.3f } });
        break;
    }
}

void UAffordanceComponent::MakeTypicalFor(EPlaceKind Kind)
{
    Offers.Reset();

    // Вид места — то, что у всех кафе общее.
    if (Category.IsEmpty())
    {
        Category = HumanText::Place(Kind);
    }

    if (FVillage::IsMedieval(this))
    {
        MakeVillageTypical(Kind);
        return;
    }

    switch (Kind)
    {
    case EPlaceKind::Home:
        AddOffer(EActionType::Sleep, TEXT("лечь спать"), 0.0f);
        AddPromise(ENeedType::Sleep, 1.0f);
        AddPromise(ENeedType::Comfort, 0.4f);
        AddPromise(ENeedType::Shelter, 0.6f);
        Offers.Last().EffortCost = 0.0f;

        AddOffer(EActionType::Eat, TEXT("поесть дома"), 900.0f);
        AddPromise(ENeedType::Hunger, 0.75f);
        AddPromise(ENeedType::Comfort, 0.1f);
        Offers.Last().bNeedsSeat = true;
        Offers.Last().SetupSeconds = 4.0f;

        AddOffer(EActionType::Drink, TEXT("попить"), 120.0f);
        AddPromise(ENeedType::Thirst, 0.9f);
        Offers.Last().EffortCost = 0.0f;

        AddOffer(EActionType::UseToilet, TEXT("в туалет"), 240.0f);
        AddPromise(ENeedType::Bladder, 1.0f);
        Offers.Last().EffortCost = 0.0f;

        AddOffer(EActionType::Wash, TEXT("помыться"), 900.0f);
        AddPromise(ENeedType::Hygiene, 1.0f);
        AddPromise(ENeedType::Comfort, 0.2f);

        AddOffer(EActionType::Rest, TEXT("отдохнуть дома"), 1800.0f);
        AddPromise(ENeedType::Comfort, 0.4f);
        AddPromise(ENeedType::Order, 0.3f);
        AddPromise(ENeedType::Shelter, 0.4f);
        Offers.Last().EffortCost = 0.0f;

        AddOffer(EActionType::Practice, TEXT("заняться своим делом"), 2400.0f);
        AddPromise(ENeedType::Competence, 0.35f);
        AddPromise(ENeedType::Achievement, 0.2f);
        Offers.Last().EffortCost = 0.25f;
        break;

    case EPlaceKind::Food:
    case EPlaceKind::Shop:
        AddOffer(EActionType::Eat, TEXT("поесть"), 1200.0f);
        AddPromise(ENeedType::Hunger, 0.9f);
        AddPromise(ENeedType::Comfort, 0.15f);
        Offers.Last().MoneyCost = 8.0f;
        // Кафе открыто с утра до позднего вечера.
        Offers.Last().AvailableFromHour = 7.0f;
        Offers.Last().AvailableToHour = 23.0f;

        AddOffer(EActionType::Drink, TEXT("попить"), 300.0f);
        AddPromise(ENeedType::Thirst, 0.9f);
        Offers.Last().MoneyCost = 2.0f;

        // В людных местах заодно бывают люди.
        AddOffer(EActionType::Observe, TEXT("посидеть, посмотреть на людей"), 900.0f);
        AddPromise(ENeedType::SocialContact, 0.2f);
        AddPromise(ENeedType::Novelty, 0.2f);
        Offers.Last().EffortCost = 0.0f;

        // И уборная. Без неё человеку приходилось тащиться через полгорода
        // домой, и он проводил в дороге едва ли не треть жизни.
        AddOffer(EActionType::UseToilet, TEXT("в уборную"), 200.0f);
        AddPromise(ENeedType::Bladder, 1.0f);
        Offers.Last().EffortCost = 0.0f;
        Offers.Last().AvailableFromHour = 7.0f;
        Offers.Last().AvailableToHour = 23.0f;
        break;

    case EPlaceKind::Work:
        // На работе тоже есть уборная — и это одна из причин, по которым
        // человеку проще провести день там, чем бегать домой.
        AddOffer(EActionType::UseToilet, TEXT("в уборную"), 200.0f);
        AddPromise(ENeedType::Bladder, 1.0f);
        Offers.Last().EffortCost = 0.0f;
        Offers.Last().AvailableFromHour = 9.0f;
        Offers.Last().AvailableToHour = 18.0f;

        AddOffer(EActionType::Work, TEXT("работать"), 7200.0f);
        AddPromise(ENeedType::Money, 0.45f);
        AddPromise(ENeedType::Achievement, 0.4f);
        AddPromise(ENeedType::Competence, 0.3f);
        AddPromise(ENeedType::Esteem, 0.25f);
        AddPromise(ENeedType::SocialContact, 0.2f);
        AddPromise(ENeedType::Belonging, 0.15f);
        // Работа отнимает силы и свободу — иначе на неё ходили бы с
        // удовольствием. Но не настолько, чтобы усталый человек не пошёл
        // туда НИКОГДА: цена должна быть ощутимой, а не запретительной.
        AddPromise(ENeedType::Comfort, -0.16f);
        AddPromise(ENeedType::Autonomy, -0.10f);
        Offers.Last().EffortCost = 0.35f;
        Offers.Last().RequiredSkill = TEXT("Work");
        Offers.Last().Difficulty = 0.35f;
        Offers.Last().MoneyGainPerHour = 9.0f;
        // Контора работает с девяти до шести. Ночью её попросту нет.
        Offers.Last().AvailableFromHour = 9.0f;
        Offers.Last().AvailableToHour = 18.0f;
        break;

    case EPlaceKind::Study:
        // Школа — единственное место в городе, где знание не добывают
        // собственной шкурой, а берут готовым. Открыта днём.
        AddOffer(EActionType::Study, TEXT("пойти на урок"), 5400.0f);
        AddPromise(ENeedType::Competence, 0.45f);
        AddPromise(ENeedType::Novelty, 0.3f);
        AddPromise(ENeedType::Belonging, 0.25f);
        AddPromise(ENeedType::Meaning, 0.2f);
        AddPromise(ENeedType::Autonomy, -0.15f);
        Offers.Last().EffortCost = 0.22f;
        Offers.Last().AvailableFromHour = 8.0f;
        Offers.Last().AvailableToHour = 15.0f;

        AddOffer(EActionType::Talk, TEXT("расспросить, как тут учат"), 900.0f);
        AddPromise(ENeedType::SocialContact, 0.3f);
        AddPromise(ENeedType::Novelty, 0.2f);
        Offers.Last().EffortCost = 0.05f;
        Offers.Last().AvailableFromHour = 8.0f;
        Offers.Last().AvailableToHour = 17.0f;

        AddOffer(EActionType::UseToilet, TEXT("в уборную"), 200.0f);
        AddPromise(ENeedType::Bladder, 1.0f);
        Offers.Last().EffortCost = 0.0f;
        break;

    // =======================================================================
    //  ГОРОД — ЭТО НЕ ТОЛЬКО ЖИЛЬЁ
    //
    //  Пока в городе были дом, контора и кафе, у человека и выбора не было:
    //  спать, работать, есть. Теперь есть куда пойти лечиться, мыться,
    //  учиться ремеслу, читать и торговать — и от этого его день перестаёт
    //  быть одинаковым.
    // =======================================================================

    case EPlaceKind::Hospital:
        AddOffer(EActionType::Rest, TEXT("показаться врачу"), 2400.0f);
        AddPromise(ENeedType::Health, 0.85f);
        AddPromise(ENeedType::Safety, 0.3f);
        AddPromise(ENeedType::Comfort, -0.1f);
        Offers.Last().MoneyCost = 14.0f;
        Offers.Last().EffortCost = 0.1f;
        Offers.Last().AvailableFromHour = 8.0f;
        Offers.Last().AvailableToHour = 20.0f;

        AddOffer(EActionType::Help, TEXT("помочь больным"), 5400.0f);
        AddPromise(ENeedType::Meaning, 0.5f);
        AddPromise(ENeedType::Esteem, 0.3f);
        AddPromise(ENeedType::Money, 0.35f);
        AddPromise(ENeedType::Comfort, -0.2f);
        Offers.Last().RequiredSkill = TEXT("Medicine");
        Offers.Last().Difficulty = 0.45f;
        Offers.Last().MoneyGainPerHour = 12.0f;
        Offers.Last().EffortCost = 0.35f;
        Offers.Last().AvailableFromHour = 8.0f;
        Offers.Last().AvailableToHour = 19.0f;

        AddOffer(EActionType::UseToilet, TEXT("в уборную"), 200.0f);
        AddPromise(ENeedType::Bladder, 1.0f);
        Offers.Last().EffortCost = 0.0f;
        break;

    case EPlaceKind::Workshop:
        AddOffer(EActionType::Work, TEXT("работать в мастерской"), 7200.0f);
        AddPromise(ENeedType::Money, 0.5f);
        AddPromise(ENeedType::Competence, 0.45f);
        AddPromise(ENeedType::Achievement, 0.4f);
        AddPromise(ENeedType::Comfort, -0.25f);
        Offers.Last().RequiredSkill = TEXT("Craft");
        Offers.Last().Difficulty = 0.4f;
        Offers.Last().MoneyGainPerHour = 11.0f;
        Offers.Last().EffortCost = 0.4f;
        Offers.Last().AvailableFromHour = 8.0f;
        Offers.Last().AvailableToHour = 18.0f;

        AddOffer(EActionType::Practice, TEXT("поучиться ремеслу"), 3600.0f);
        AddPromise(ENeedType::Competence, 0.5f);
        AddPromise(ENeedType::Novelty, 0.25f);
        Offers.Last().RequiredSkill = TEXT("Craft");
        Offers.Last().Difficulty = 0.2f;
        Offers.Last().EffortCost = 0.28f;

        AddOffer(EActionType::Observe, TEXT("починить своё"), 1800.0f);
        AddPromise(ENeedType::Order, 0.4f);
        AddPromise(ENeedType::Competence, 0.2f);
        Offers.Last().RequiredSkill = TEXT("Repair");
        Offers.Last().Difficulty = 0.25f;
        Offers.Last().EffortCost = 0.2f;
        break;

    case EPlaceKind::Library:
        AddOffer(EActionType::Read, TEXT("читать в тишине"), 3600.0f);
        AddPromise(ENeedType::Novelty, 0.4f);
        AddPromise(ENeedType::Meaning, 0.35f);
        AddPromise(ENeedType::Competence, 0.3f);
        Offers.Last().RequiredSkill = TEXT("Reading");
        Offers.Last().Difficulty = 0.2f;
        Offers.Last().EffortCost = 0.15f;
        Offers.Last().bNeedsLight = true;
        Offers.Last().bNeedsSeat = true;
        Offers.Last().SetupSeconds = 6.0f;
        Offers.Last().AvailableFromHour = 9.0f;
        Offers.Last().AvailableToHour = 21.0f;

        AddOffer(EActionType::Study, TEXT("разобраться в чём-нибудь"), 5400.0f);
        AddPromise(ENeedType::Competence, 0.5f);
        AddPromise(ENeedType::Meaning, 0.25f);
        Offers.Last().RequiredSkill = TEXT("Reading");
        Offers.Last().Difficulty = 0.35f;
        Offers.Last().EffortCost = 0.25f;
        Offers.Last().bNeedsLight = true;
        Capacity = 8;
        break;

    case EPlaceKind::Market:
        AddOffer(EActionType::Work, TEXT("купить припасов"), 900.0f);
        AddPromise(ENeedType::Safety, 0.2f);
        AddPromise(ENeedType::Order, 0.15f);
        Offers.Last().MoneyCost = 7.0f;
        Offers.Last().Produces = EResourceKind::RawFood;
        Offers.Last().ProducesAmount = 4.0f;
        Offers.Last().EffortCost = 0.08f;
        Offers.Last().SetupSeconds = 6.0f;
        Offers.Last().AvailableFromHour = 7.0f;
        Offers.Last().AvailableToHour = 19.0f;

        AddOffer(EActionType::Work, TEXT("торговать"), 5400.0f);
        AddPromise(ENeedType::Money, 0.5f);
        AddPromise(ENeedType::SocialContact, 0.3f);
        AddPromise(ENeedType::Comfort, -0.15f);
        Offers.Last().RequiredSkill = TEXT("Trade");
        Offers.Last().Difficulty = 0.3f;
        Offers.Last().MoneyGainPerHour = 10.0f;
        Offers.Last().EffortCost = 0.3f;
        Offers.Last().AvailableFromHour = 7.0f;
        Offers.Last().AvailableToHour = 19.0f;

        AddOffer(EActionType::Observe, TEXT("поглазеть на рынке"), 1200.0f);
        AddPromise(ENeedType::Novelty, 0.35f);
        AddPromise(ENeedType::SocialContact, 0.2f);
        Offers.Last().EffortCost = 0.02f;
        Capacity = 14;
        break;

    case EPlaceKind::Bathhouse:
        AddOffer(EActionType::Wash, TEXT("сходить в баню"), 3600.0f);
        AddPromise(ENeedType::Hygiene, 1.0f);
        AddPromise(ENeedType::Comfort, 0.7f);
        AddPromise(ENeedType::Health, 0.2f);
        Offers.Last().MoneyCost = 5.0f;
        Offers.Last().EffortCost = 0.08f;
        Offers.Last().AvailableFromHour = 10.0f;
        Offers.Last().AvailableToHour = 22.0f;

        AddOffer(EActionType::Talk, TEXT("посидеть, поговорить"), 2400.0f);
        AddPromise(ENeedType::SocialContact, 0.5f);
        AddPromise(ENeedType::Belonging, 0.3f);
        AddPromise(ENeedType::Comfort, 0.2f);
        Offers.Last().EffortCost = 0.0f;
        Capacity = 6;
        break;

    case EPlaceKind::Bakery:
        AddOffer(EActionType::Eat, TEXT("взять хлеба"), 600.0f);
        AddPromise(ENeedType::Hunger, 0.6f);
        AddPromise(ENeedType::Comfort, 0.15f);
        Offers.Last().MoneyCost = 4.0f;
        Offers.Last().EffortCost = 0.02f;
        Offers.Last().AvailableFromHour = 6.0f;
        Offers.Last().AvailableToHour = 20.0f;

        AddOffer(EActionType::Work, TEXT("печь хлеб"), 7200.0f);
        AddPromise(ENeedType::Money, 0.45f);
        AddPromise(ENeedType::Competence, 0.35f);
        AddPromise(ENeedType::Achievement, 0.3f);
        AddPromise(ENeedType::Comfort, -0.2f);
        Offers.Last().RequiredSkill = TEXT("Cooking");
        Offers.Last().Difficulty = 0.3f;
        Offers.Last().MoneyGainPerHour = 9.0f;
        Offers.Last().EffortCost = 0.38f;
        Offers.Last().AvailableFromHour = 5.0f;
        Offers.Last().AvailableToHour = 15.0f;
        break;

    case EPlaceKind::TownHall:
        AddOffer(EActionType::Work, TEXT("служить в управе"), 7200.0f);
        AddPromise(ENeedType::Money, 0.5f);
        AddPromise(ENeedType::Esteem, 0.4f);
        AddPromise(ENeedType::Order, 0.3f);
        AddPromise(ENeedType::Autonomy, -0.2f);
        Offers.Last().RequiredSkill = TEXT("Writing");
        Offers.Last().Difficulty = 0.4f;
        Offers.Last().MoneyGainPerHour = 13.0f;
        Offers.Last().EffortCost = 0.3f;
        Offers.Last().AvailableFromHour = 9.0f;
        Offers.Last().AvailableToHour = 17.0f;

        AddOffer(EActionType::Talk, TEXT("узнать новости"), 900.0f);
        AddPromise(ENeedType::Novelty, 0.35f);
        AddPromise(ENeedType::Belonging, 0.25f);
        AddPromise(ENeedType::Order, 0.2f);
        Offers.Last().EffortCost = 0.05f;
        Offers.Last().AvailableFromHour = 9.0f;
        Offers.Last().AvailableToHour = 18.0f;
        break;

    case EPlaceKind::Beautiful:
        AddOffer(EActionType::Observe, TEXT("побыть в тишине"), 1200.0f);
        AddPromise(ENeedType::Beauty, 0.8f);
        AddPromise(ENeedType::Comfort, 0.3f);
        AddPromise(ENeedType::Meaning, 0.15f);
        Offers.Last().EffortCost = 0.0f;

        AddOffer(EActionType::Rest, TEXT("посидеть на скамейке"), 1800.0f);
        AddPromise(ENeedType::Comfort, 0.45f);
        AddPromise(ENeedType::Order, 0.2f);
        Offers.Last().EffortCost = 0.0f;

        AddOffer(EActionType::Exercise, TEXT("размяться"), 1800.0f);
        AddPromise(ENeedType::Health, 0.3f);
        AddPromise(ENeedType::Competence, 0.15f);
        AddPromise(ENeedType::Comfort, -0.2f);
        Offers.Last().EffortCost = 0.4f;
        break;

    case EPlaceKind::Social:
        AddOffer(EActionType::Observe, TEXT("побыть среди людей"), 1200.0f);
        AddPromise(ENeedType::SocialContact, 0.35f);
        AddPromise(ENeedType::Belonging, 0.25f);
        AddPromise(ENeedType::Novelty, 0.2f);
        Offers.Last().EffortCost = 0.05f;
        break;

    case EPlaceKind::Rest:
        AddOffer(EActionType::Rest, TEXT("передохнуть"), 1200.0f);
        AddPromise(ENeedType::Comfort, 0.5f);
        Offers.Last().EffortCost = 0.0f;
        break;

    case EPlaceKind::Danger:
        // Опасное место тоже кое-что предлагает — тем, кому нужна острота.
        AddOffer(EActionType::Explore, TEXT("сунуться туда"), 900.0f);
        AddPromise(ENeedType::Novelty, 0.7f);
        AddPromise(ENeedType::Safety, -0.6f);
        Offers.Last().Risk = 0.7f;
        Offers.Last().EffortCost = 0.3f;
        break;

    default:
        AddOffer(EActionType::Explore, TEXT("осмотреться"), 600.0f);
        AddPromise(ENeedType::Novelty, 0.3f);
        Offers.Last().EffortCost = 0.1f;
        break;
    }

    struct FPromiseSeed
    {
        ENeedType Need;
        float Amount;
    };

    auto Offer = [this](EActionType Action, const TCHAR* Label, float Duration, float Effort,
                        std::initializer_list<FPromiseSeed> Promises) -> FAffordance&
    {
        AddOffer(Action, Label, Duration);
        for (const FPromiseSeed& P : Promises)
        {
            AddPromise(P.Need, P.Amount);
        }
        Offers.Last().EffortCost = Effort;
        return Offers.Last();
    };

    auto Hours = [](FAffordance& A, float From, float To)
    {
        A.AvailableFromHour = From;
        A.AvailableToHour = To;
    };

    switch (Kind)
    {
    case EPlaceKind::Home:
    {
        Offer(EActionType::Cook, TEXT("заварить чай"), 600.0f, 0.05f,
            { { ENeedType::Thirst, 0.5f }, { ENeedType::Comfort, 0.3f } });

        Offer(EActionType::Observe, TEXT("прибраться дома"), 1500.0f, 0.22f,
            { { ENeedType::Order, 0.55f }, { ENeedType::Achievement, 0.15f }, { ENeedType::Hygiene, 0.1f }, { ENeedType::Comfort, -0.08f } });

        Offer(EActionType::Reflect, TEXT("помечтать у окна"), 900.0f, 0.0f,
            { { ENeedType::Meaning, 0.25f }, { ENeedType::Beauty, 0.15f }, { ENeedType::Comfort, 0.1f } });

        FAffordance& Diary = Offer(EActionType::Practice, TEXT("написать в дневник"), 1200.0f, 0.12f,
            { { ENeedType::Meaning, 0.3f }, { ENeedType::Intimacy, 0.15f }, { ENeedType::Order, 0.15f } });
        Diary.RequiredSkill = TEXT("Writing");
        Diary.Difficulty = 0.25f;
        Diary.bNeedsSeat = true;
        Diary.bNeedsLight = true;
        Diary.SetupSeconds = 4.0f;

        FAffordance& Morning = Offer(EActionType::Exercise, TEXT("сделать зарядку"), 900.0f, 0.3f,
            { { ENeedType::Health, 0.3f }, { ENeedType::Comfort, 0.1f }, { ENeedType::Achievement, 0.08f } });
        Hours(Morning, 6.0f, 11.0f);

        FAffordance& Nap = Offer(EActionType::Rest, TEXT("прилечь отдохнуть"), 1800.0f, 0.0f,
            { { ENeedType::Comfort, 0.5f }, { ENeedType::Sleep, 0.15f } });
        Nap.bNeedsLying = true;
        Nap.SetupSeconds = 5.0f;
        Hours(Nap, 12.0f, 18.0f);
        break;
    }

    case EPlaceKind::Food:
    case EPlaceKind::Shop:
    {
        FAffordance& Tea = Offer(EActionType::Talk, TEXT("выпить чаю в компании"), 1800.0f, 0.02f,
            { { ENeedType::SocialContact, 0.45f }, { ENeedType::Belonging, 0.2f }, { ENeedType::Thirst, 0.4f }, { ENeedType::Comfort, 0.2f } });
        Tea.MoneyCost = 2.0f;
        Tea.bNeedsSeat = true;
        Tea.SetupSeconds = 4.0f;
        Hours(Tea, 8.0f, 22.0f);

        FAffordance& Job = Offer(EActionType::Work, TEXT("подрабатывать в кафе"), 5400.0f, 0.33f,
            { { ENeedType::Money, 0.45f }, { ENeedType::SocialContact, 0.25f }, { ENeedType::Competence, 0.2f }, { ENeedType::Comfort, -0.15f } });
        Job.RequiredSkill = TEXT("Cooking");
        Job.Difficulty = 0.25f;
        Job.MoneyGainPerHour = 8.0f;
        Hours(Job, 8.0f, 21.0f);
        break;
    }

    case EPlaceKind::Work:
    {
        FAffordance& Chat = Offer(EActionType::Talk, TEXT("перекинуться словом с коллегами"), 900.0f, 0.03f,
            { { ENeedType::SocialContact, 0.3f }, { ENeedType::Belonging, 0.25f } });
        Hours(Chat, 9.0f, 18.0f);
        break;
    }

    case EPlaceKind::Study:
    {
        FAffordance& Teach = Offer(EActionType::Work, TEXT("вести урок"), 5400.0f, 0.35f,
            { { ENeedType::Money, 0.45f }, { ENeedType::Meaning, 0.45f }, { ENeedType::Esteem, 0.3f }, { ENeedType::Comfort, -0.15f } });
        Teach.RequiredSkill = TEXT("Teaching");
        Teach.Difficulty = 0.35f;
        Teach.MoneyGainPerHour = 10.0f;
        Hours(Teach, 8.0f, 15.0f);

        FAffordance& Letters = Offer(EActionType::Practice, TEXT("поучиться грамоте"), 2400.0f, 0.15f,
            { { ENeedType::Competence, 0.4f }, { ENeedType::Novelty, 0.2f }, { ENeedType::Belonging, 0.1f } });
        Letters.bNeedsSeat = true;
        Letters.SetupSeconds = 4.0f;
        Hours(Letters, 8.0f, 17.0f);
        break;
    }

    case EPlaceKind::Hospital:
    {
        FAffordance& Visit = Offer(EActionType::Help, TEXT("навестить больных"), 1200.0f, 0.1f,
            { { ENeedType::Meaning, 0.35f }, { ENeedType::Belonging, 0.2f }, { ENeedType::Esteem, 0.1f } });
        Hours(Visit, 9.0f, 19.0f);
        break;
    }

    case EPlaceKind::Workshop:
    {
        FAffordance& Make = Offer(EActionType::Practice, TEXT("смастерить что-нибудь для себя"), 3600.0f, 0.3f,
            { { ENeedType::Achievement, 0.45f }, { ENeedType::Competence, 0.3f }, { ENeedType::Beauty, 0.15f }, { ENeedType::Comfort, -0.1f } });
        Make.RequiredSkill = TEXT("Craft");
        Make.Difficulty = 0.15f;
        Hours(Make, 8.0f, 20.0f);
        break;
    }

    case EPlaceKind::Library:
    {
        FAffordance& Keeper = Offer(EActionType::Work, TEXT("работать в библиотеке"), 5400.0f, 0.25f,
            { { ENeedType::Money, 0.4f }, { ENeedType::Meaning, 0.3f }, { ENeedType::Order, 0.3f } });
        Keeper.RequiredSkill = TEXT("Reading");
        Keeper.Difficulty = 0.45f;
        Keeper.MoneyGainPerHour = 8.0f;
        Hours(Keeper, 9.0f, 21.0f);

        FAffordance& Aloud = Offer(EActionType::Help, TEXT("почитать вслух детям"), 1200.0f, 0.15f,
            { { ENeedType::Meaning, 0.4f }, { ENeedType::Esteem, 0.2f }, { ENeedType::Belonging, 0.2f } });
        Aloud.RequiredSkill = TEXT("Reading");
        Aloud.Difficulty = 0.4f;
        Aloud.bNeedsSeat = true;
        Hours(Aloud, 10.0f, 19.0f);
        break;
    }

    case EPlaceKind::Market:
    {
        FAffordance& Haggle = Offer(EActionType::Observe, TEXT("поторговаться за мелочи"), 900.0f, 0.08f,
            { { ENeedType::Novelty, 0.25f }, { ENeedType::SocialContact, 0.25f }, { ENeedType::Achievement, 0.1f } });
        Haggle.MoneyCost = 3.0f;
        Hours(Haggle, 7.0f, 19.0f);
        break;
    }

    case EPlaceKind::Bakery:
    {
        FAffordance& Bun = Offer(EActionType::Eat, TEXT("выпить чаю с булкой"), 900.0f, 0.02f,
            { { ENeedType::Hunger, 0.35f }, { ENeedType::Thirst, 0.3f }, { ENeedType::Comfort, 0.25f } });
        Bun.MoneyCost = 3.0f;
        Hours(Bun, 7.0f, 20.0f);
        break;
    }

    case EPlaceKind::TownHall:
    {
        FAffordance& Gathering = Offer(EActionType::Talk, TEXT("прийти на сход"), 3600.0f, 0.08f,
            { { ENeedType::Belonging, 0.45f }, { ENeedType::Order, 0.35f }, { ENeedType::Meaning, 0.25f }, { ENeedType::Esteem, 0.1f } });
        Hours(Gathering, 18.0f, 20.0f);

        FAffordance& AskJob = Offer(EActionType::Observe, TEXT("спросить про работу"), 600.0f, 0.12f,
            { { ENeedType::Money, 0.25f }, { ENeedType::Order, 0.2f } });
        Hours(AskJob, 9.0f, 17.0f);
        break;
    }

    case EPlaceKind::Beautiful:
    {
        FAffordance& Sky = Offer(EActionType::Observe, TEXT("полежать на траве, посмотреть в небо"), 1200.0f, 0.0f,
            { { ENeedType::Beauty, 0.6f }, { ENeedType::Meaning, 0.25f }, { ENeedType::Comfort, 0.2f } });
        Sky.bNeedsLying = true;
        Sky.SetupSeconds = 5.0f;
        Hours(Sky, 9.0f, 21.0f);

        Offer(EActionType::Wander, TEXT("пройтись по скверу"), 1500.0f, 0.05f,
            { { ENeedType::Beauty, 0.35f }, { ENeedType::Novelty, 0.2f }, { ENeedType::Health, 0.1f } });

        Offer(EActionType::Exercise, TEXT("пробежаться"), 1800.0f, 0.4f,
            { { ENeedType::Health, 0.4f }, { ENeedType::Achievement, 0.15f }, { ENeedType::Comfort, -0.15f } });
        break;
    }

    case EPlaceKind::Social:
    {
        FAffordance& Dance = Offer(EActionType::Celebrate, TEXT("потанцевать"), 1800.0f, 0.25f,
            { { ENeedType::SocialContact, 0.35f }, { ENeedType::Beauty, 0.2f }, { ENeedType::Belonging, 0.2f }, { ENeedType::Health, 0.1f } });
        Hours(Dance, 18.0f, 23.0f);

        Offer(EActionType::Talk, TEXT("послушать, о чём говорят"), 1200.0f, 0.03f,
            { { ENeedType::Novelty, 0.3f }, { ENeedType::Belonging, 0.3f } });

        FAffordance& Play = Offer(EActionType::Practice, TEXT("сыграть для людей"), 1800.0f, 0.2f,
            { { ENeedType::Esteem, 0.35f }, { ENeedType::Beauty, 0.3f }, { ENeedType::Money, 0.2f } });
        Play.RequiredSkill = TEXT("Music");
        Play.Difficulty = 0.35f;
        Play.MoneyGainPerHour = 4.0f;
        Hours(Play, 12.0f, 22.0f);
        break;
    }

    case EPlaceKind::Rest:
        Offer(EActionType::Observe, TEXT("посмотреть на прохожих"), 900.0f, 0.0f,
            { { ENeedType::Novelty, 0.2f }, { ENeedType::SocialContact, 0.1f }, { ENeedType::Comfort, 0.2f } });
        break;

    default:
        break;
    }
}
