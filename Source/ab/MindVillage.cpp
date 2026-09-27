#include "MindComponent.h"
#include "CompleteHumanAI.h"
#include "Village.h"
#include "Crafts.h"
#include "FurnitureActor.h"
#include "ResourceActor.h"
#include "AffordanceComponent.h"
#include "IdentityComponent.h"
#include "SocialComponent.h"
#include "NeedComponent.h"
#include "BodyMotorComponent.h"
#include "SpeechComponent.h"
#include "HumanWorldSubsystem.h"
#include "EngineUtils.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "EmotionComponent.h"
#include "PhysiologyComponent.h"

namespace
{
    FString Spoken(const USpeechComponent* Speech, const FString& Intended, const FString& Self, const FString& Listener)
    {
        if (!Speech)
        {
            return Intended;
        }
        TSet<FString> Names;
        if (!Self.IsEmpty())
        {
            Names.Add(USpeechComponent::NormalizeWord(Self).ToString());
        }
        if (!Listener.IsEmpty())
        {
            Names.Add(USpeechComponent::NormalizeWord(Listener).ToString());
        }
        return Speech->Articulate(Intended, &Names, Self, Listener);
    }

    void Promise(FAffordance& A, ENeedType Need, float Amount)
    {
        FNeedPromise P;
        P.Need = Need;
        P.Amount = Amount;
        A.Promises.Add(P);
    }

    bool FindChore(UWorld* World, const FVector& Near, EFurnitureType Type, FName Craft, float Radius, FAffordance& Out)
    {
        if (!World)
        {
            return false;
        }
        float Best = Radius;
        bool bFound = false;
        for (TActorIterator<AFurnitureActor> It(World); It; ++It)
        {
            AFurnitureActor* Thing = *It;
            if (!Thing || Thing->FurnitureType != Type || !Thing->Affordances)
            {
                continue;
            }
            const float D = FVector::Dist2D(Thing->GetActorLocation(), Near);
            if (D >= Best)
            {
                continue;
            }
            TArray<FAffordance> Offers;
            Thing->Affordances->CollectOffers(Offers);
            for (const FAffordance& Offer : Offers)
            {
                if (Offer.Craft == Craft)
                {
                    Out = Offer;
                    Best = D;
                    bFound = true;
                    break;
                }
            }
        }
        return bFound;
    }

    bool FindPlaceOffer(UWorld* World, const FVector& Near, const FString& Label, float Radius, FAffordance& Out)
    {
        if (!World)
        {
            return false;
        }
        const UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>();
        if (!WorldMind)
        {
            return false;
        }
        TArray<FAffordance> Offers;
        WorldMind->CollectOffersNear(Near, Radius, Offers);
        for (const FAffordance& Offer : Offers)
        {
            if (Offer.Label == Label)
            {
                Out = Offer;
                return true;
            }
        }
        return false;
    }
}

AResourceActor* UMindComponent::TakeCargo(EResourceKind Kind, float Amount)
{
    ACompleteHumanNPC* Me = GetHuman();
    if (!Me || Amount <= 0.0f || Kind == EResourceKind::None)
    {
        return nullptr;
    }
    if (AResourceActor* Held = Cast<AResourceActor>(Me->CarriedItem.Get()))
    {
        if (Held->Kind == Kind)
        {
            Held->CustomSize = FVector::ZeroVector;
            Held->Setup(Kind, Held->Amount + Amount);
            Held->bCargo = true;
            return Held;
        }
        Me->LetGo(false);
    }
    const FVector At = Me->GetActorLocation() + Me->GetActorForwardVector() * 38.0f;
    AResourceActor* Item = AResourceActor::Spawn(Me->GetWorld(), Kind, Amount, At);
    if (!Item)
    {
        return nullptr;
    }
    Item->bCargo = true;
    Me->TakeIntoHands(Item);
    return Item;
}

void UMindComponent::GatherVillageAffordances(TArray<FAffordance>& Out) const
{
    if (!FVillage::IsMedieval(this))
    {
        return;
    }
    ACompleteHumanNPC* Me = GetHuman();
    UHumanWorldSubsystem* World = GetWorldMind();
    if (!Me || !World || !Identity)
    {
        return;
    }
    const FVector Here = Me->GetActorLocation();

    if (Identity->Age < 1.5f)
    {
        FAffordance Cry;
        Cry.Action = EActionType::Talk;
        Cry.Source = EAffordanceSource::Self;
        Cry.Duration = 120.0f;
        Cry.EffortCost = 0.02f;
        Cry.Deal = TEXT("cry");
        Cry.Label = TEXT("заплакать, позвать маму");
        Cry.Key = TEXT("cry@Self");
        Cry.CategoryKey = Cry.Key;
        Cry.bRequiresProximity = false;
        Promise(Cry, ENeedType::Belonging, 0.1f);
        Out.Add(Cry);
        if (Me->Motor && Me->Motor->IsInfant() && Me->Motor->CanTryWalking())
        {
            ACompleteHumanNPC* Near = nullptr;
            float Best = 4000.0f;
            for (ACompleteHumanNPC* Kin : World->GetAllHumans())
            {
                if (Kin && Kin != Me && Kin->IdentityComponent && Kin->IdentityComponent->Household == Identity->Household
                    && Kin->IdentityComponent->Age >= 12.0f)
                {
                    const float D = FVector::Dist2D(Here, Kin->GetActorLocation());
                    if (D < Best)
                    {
                        Best = D;
                        Near = Kin;
                    }
                }
            }
            FAffordance Try;
            Try.Action = EActionType::Exercise;
            Try.Source = EAffordanceSource::Self;
            Try.Duration = 60.0f;
            Try.EffortCost = 0.3f;
            Try.Deal = TEXT("tryrise");
            Try.Label = Near ? FString::Printf(TEXT("попробовать дойти до %s"), *NameOf(Near)) : FString(TEXT("попробовать встать и шагнуть"));
            Try.Key = TEXT("tryrise@Self");
            Try.CategoryKey = Try.Key;
            Try.Location = Near ? Near->GetActorLocation() : Here + Me->GetActorForwardVector() * 200.0f;
            Try.bHasLocation = true;
            Promise(Try, ENeedType::Competence, 0.3f);
            Promise(Try, ENeedType::Novelty, 0.2f);
            Promise(Try, ENeedType::Autonomy, 0.15f);
            Out.Add(Try);
        }
        return;
    }

    if (Identity->Age >= 12.0f)
    {
        for (ACompleteHumanNPC* Baby : World->GetAllHumans())
        {
            if (!Baby || Baby == Me || !Baby->IsAlive() || !Baby->IdentityComponent || !Baby->Mind
                || Baby->IdentityComponent->Age >= 3.0f || Baby->IdentityComponent->Household != Identity->Household)
            {
                continue;
            }
            if (FVector::Dist2D(Here, Baby->GetActorLocation()) > 5000.0f)
            {
                continue;
            }
            const FString BabyName = NameOf(Baby);
            const bool bMother = Baby->IdentityComponent->Mother.Get() == Me;
            const float BabyHunger = Baby->Mind->Needs ? Baby->Mind->Needs->GetSatisfaction(ENeedType::Hunger) : 1.0f;
            if (BabyHunger < 0.6f)
            {
                FAffordance Feed;
                Feed.Action = EActionType::Help;
                Feed.Source = EAffordanceSource::Person;
                Feed.Target = Baby;
                Feed.Location = Baby->GetActorLocation();
                Feed.bHasLocation = true;
                Feed.Duration = 600.0f;
                Feed.EffortCost = 0.05f;
                Feed.Deal = TEXT("feed");
                Feed.Label = FString::Printf(TEXT("покормить дитя — %s"), *BabyName);
                Feed.Key = FName(*FString::Printf(TEXT("кормить@%s"), *BabyName));
                Feed.CategoryKey = TEXT("кормить@дитя");
                Promise(Feed, ENeedType::Meaning, bMother ? 0.45f : 0.3f);
                Promise(Feed, ENeedType::Belonging, 0.3f);
                Promise(Feed, ENeedType::Intimacy, bMother ? 0.35f : 0.2f);
                Out.Add(Feed);
            }
            if (!Baby->Mind->bAsleep && Baby->IdentityComponent->Age < 1.5f)
            {
                FAffordance Rock;
                Rock.Action = EActionType::Help;
                Rock.Source = EAffordanceSource::Person;
                Rock.Target = Baby;
                Rock.Location = Baby->GetActorLocation();
                Rock.bHasLocation = true;
                Rock.Duration = 480.0f;
                Rock.EffortCost = 0.05f;
                Rock.Deal = TEXT("cradle");
                Rock.Label = FString::Printf(TEXT("уложить дитя спать — %s"), *BabyName);
                Rock.Key = FName(*FString::Printf(TEXT("укачать@%s"), *BabyName));
                Rock.CategoryKey = TEXT("укачать@дитя");
                Promise(Rock, ENeedType::Belonging, 0.2f);
                Promise(Rock, ENeedType::Meaning, 0.2f);
                Promise(Rock, ENeedType::Order, 0.1f);
                Out.Add(Rock);
            }
        }
    }

    if (Identity->Age >= 7.0f)
    {
        for (ACompleteHumanNPC* Master : World->GetAllHumans())
        {
            if (!Master || Master == Me || !Master->IsAlive() || !Master->IdentityComponent || !Master->Mind)
            {
                continue;
            }
            const FName Trade = Master->IdentityComponent->Trade;
            if (Trade.IsNone() || Master->Mind->bAsleep)
            {
                continue;
            }
            const float Theirs = Master->Mind->Mastery(Trade);
            const float Mine = Mastery(Trade);
            if (Theirs < 0.8f || Mine > Theirs - 0.15f || Mine >= 0.85f)
            {
                continue;
            }
            if (FVector::Dist2D(Here, Master->GetActorLocation()) > 9000.0f)
            {
                continue;
            }
            const FVillageTrade* Info = FVillage::TradeOf(Trade);
            const FString MasterName = NameOf(Master);
            FAffordance Learn;
            Learn.Action = EActionType::Study;
            Learn.Source = EAffordanceSource::Person;
            Learn.Target = Master;
            Learn.Location = Master->GetActorLocation();
            Learn.bHasLocation = true;
            Learn.Duration = 2400.0f;
            Learn.EffortCost = 0.2f;
            Learn.Deal = TEXT("learn");
            Learn.RequiredSkill = Trade;
            Learn.Difficulty = 0.1f;
            Learn.Label = FString::Printf(TEXT("поучиться у мастера — %s (%s)"), *MasterName, Info ? Info->Deed : TEXT("ремесло"));
            Learn.Key = FName(*FString::Printf(TEXT("учиться@%s"), *MasterName));
            Learn.CategoryKey = FName(*FString::Printf(TEXT("учиться@%s"), *Trade.ToString()));
            Learn.MoneyCost = Identity->Household == Master->IdentityComponent->Household ? 0.0f : 1.0f;
            Promise(Learn, ENeedType::Competence, 0.4f);
            Promise(Learn, ENeedType::Novelty, 0.15f);
            Promise(Learn, ENeedType::Achievement, 0.12f);
            Promise(Learn, ENeedType::Belonging, 0.08f);
            Out.Add(Learn);
        }
    }

    if (Identity->IsRoyal() && Identity->Age >= 16.0f)
    {
        FAffordance Task;
        FString Words;
        if (PickCastleTask(Task, Words))
        {
            for (ACompleteHumanNPC* Servant : World->GetAllHumans())
            {
                if (!Servant || Servant == Me || !Servant->IsAlive() || !Servant->IdentityComponent || !Servant->Mind
                    || !Servant->IdentityComponent->IsServant() || Servant->Mind->bAsleep || Servant->Mind->OrderAt >= 0.0f)
                {
                    continue;
                }
                if (FVector::Dist2D(Here, Servant->GetActorLocation()) > 6000.0f)
                {
                    continue;
                }
                const FString ServantName = NameOf(Servant);
                FAffordance Order;
                Order.Action = EActionType::Talk;
                Order.Source = EAffordanceSource::Person;
                Order.Target = Servant;
                Order.Location = Servant->GetActorLocation();
                Order.bHasLocation = true;
                Order.Duration = 240.0f;
                Order.EffortCost = 0.05f;
                Order.Deal = TEXT("order");
                Order.Label = FString::Printf(TEXT("распорядиться — %s: %s"), *ServantName, *Words);
                Order.Key = FName(*FString::Printf(TEXT("велеть@%s"), *ServantName));
                Order.CategoryKey = TEXT("велеть@слуга");
                Promise(Order, ENeedType::Order, 0.35f);
                Promise(Order, ENeedType::Esteem, 0.15f);
                Promise(Order, ENeedType::Safety, 0.1f);
                Promise(Order, ENeedType::Autonomy, 0.1f);
                Out.Add(Order);
            }
        }
    }
}

bool UMindComponent::PickCastleTask(FAffordance& OutTask, FString& OutWords) const
{
    const ACompleteHumanNPC* Me = GetHuman();
    if (!Me || !Me->GetWorld() || !Me->HasHome())
    {
        return false;
    }
    UWorld* World = Me->GetWorld();
    const FVector Castle = Me->GetHomeLocation();
    const float Food = FVillage::HouseholdFood(Me);
    const float Wood = FVillage::HouseholdStores(Me, EResourceKind::Firewood);
    const float Water = FVillage::HouseholdStores(Me, EResourceKind::Water);
    const float Ready = FVillage::HouseholdStores(Me, EResourceKind::Bread) + FVillage::HouseholdStores(Me, EResourceKind::CookedFood);

    struct FChoice
    {
        float Need;
        EFurnitureType Type;
        const TCHAR* Craft;
        const TCHAR* Place;
        const TCHAR* Words;
    };
    TArray<FChoice> Choices = {
        { FMath::Clamp(1.0f - Wood / 12.0f, 0.0f, 1.0f), EFurnitureType::Tree, TEXT("v_firewood"), nullptr, TEXT("ступай в лес, наруби дров") },
        { FMath::Clamp(1.0f - Water / 12.0f, 0.0f, 1.0f), EFurnitureType::Well, TEXT("v_water"), nullptr, TEXT("принеси воды") },
        { FMath::Clamp(1.0f - Ready / 10.0f, 0.0f, 1.0f), EFurnitureType::Stove, TEXT("v_shchi"), nullptr, TEXT("свари щей к обеду") },
        { FMath::Clamp(1.0f - Ready / 10.0f, 0.0f, 1.0f) * 0.9f, EFurnitureType::Stove, TEXT("v_bread"), nullptr, TEXT("испеки хлеба") },
        { FMath::Clamp(1.0f - Food / 6.0f, 0.0f, 1.0f) * 0.8f, EFurnitureType::FishingSpot, TEXT("v_fish"), nullptr, TEXT("налови рыбы к столу") },
        { 0.35f, EFurnitureType::Chair, nullptr, TEXT("стоять на страже у ворот"), TEXT("ступай на стражу, к воротам") },
        { 0.3f, EFurnitureType::Chair, nullptr, TEXT("служить при дворе"), TEXT("займись делом при дворе") }
    };
    Choices.Sort([](const FChoice& A, const FChoice& B) { return A.Need > B.Need; });
    for (const FChoice& Choice : Choices)
    {
        if (Choice.Need < 0.15f)
        {
            continue;
        }
        if (Choice.Craft && FindChore(World, Castle, Choice.Type, FName(Choice.Craft), 9000.0f, OutTask))
        {
            OutWords = Choice.Words;
            return true;
        }
        if (Choice.Place && FindPlaceOffer(World, Castle, Choice.Place, 2500.0f, OutTask))
        {
            OutWords = Choice.Words;
            return true;
        }
    }
    return false;
}

void UMindComponent::CheckFamily(float GameDelta)
{
    if (!FVillage::IsMedieval(this) || !Identity)
    {
        return;
    }
    FamilyCheck += GameDelta;
    if (FamilyCheck < 300.0f)
    {
        return;
    }
    const float Step = FamilyCheck;
    FamilyCheck = 0.0f;
    ACompleteHumanNPC* Me = GetHuman();
    if (!Me || !Me->IsAlive())
    {
        return;
    }
    const float T = Now();
    const float Accel = FMath::Max(1.0f, Identity->AgeAccelerator) * (FVillage::FastInfants() ? 8.0f : 1.0f);
    static const bool bQuickTest = FParse::Param(FCommandLine::Get(), TEXT("Pregnant"));
    const float Gestation = bQuickTest ? 3600.0f : 0.75f * 365.0f * 86400.0f / Accel;
    if (Identity->PregnantSince >= 0.0f)
    {
        if (T - Identity->PregnantSince >= Gestation)
        {
            Identity->PregnantSince = -1.0f;
            ACompleteHumanNPC* Father = Cast<ACompleteHumanNPC>(Identity->Spouse.Get());
            if (ACompleteHumanNPC* Baby = FVillage::Birth(Me->GetWorld(), Me, Father))
            {
                Think(FString::Printf(TEXT("родилось дитя — %s"), *NameOf(Baby)), EThoughtKind::Feeling, 1.0f, Baby);
                Report(FString::Printf(TEXT("родила: %s"), *NameOf(Baby)));
                if (Needs)
                {
                    Needs->Satisfy(ENeedType::Meaning, 0.6f);
                    Needs->Satisfy(ENeedType::Belonging, 0.4f);
                }
            }
        }
        return;
    }
    if (!Identity->bFemale || Identity->Age < 18.0f || Identity->Age > 42.0f)
    {
        return;
    }
    ACompleteHumanNPC* Husband = Cast<ACompleteHumanNPC>(Identity->Spouse.Get());
    if (!Husband || !Husband->IsAlive() || !Husband->Mind)
    {
        return;
    }
    for (const TWeakObjectPtr<AActor>& Child : Identity->Children)
    {
        const ACompleteHumanNPC* Kid = Cast<ACompleteHumanNPC>(Child.Get());
        if (Kid && Kid->IsAlive() && Kid->IdentityComponent && Kid->IdentityComponent->Age < 1.0f)
        {
            return;
        }
    }
    const UHumanWorldSubsystem* World = GetWorldMind();
    const float Hour = World ? World->Now.HourFloat : 12.0f;
    const bool bNight = Hour >= 21.0f || Hour < 5.0f;
    if (!bNight || !bAsleep || !Husband->Mind->bAsleep || !Me->IsAtHome()
        || FVector::Dist2D(Me->GetActorLocation(), Husband->GetActorLocation()) > 900.0f)
    {
        return;
    }
    if (FMath::FRand() < 0.12f * Step / (8.0f * 3600.0f) * (FVillage::FastInfants() ? 8.0f : 1.0f))
    {
        Identity->PregnantSince = T;
        Think(TEXT("кажется, у нас будет дитя"), EThoughtKind::Feeling, 0.8f, Husband);
        Report(TEXT("ждёт ребёнка"));
    }
}

void UMindComponent::HearBabyCry(const ACompleteHumanNPC* Baby, bool bParent, float GameDelta)
{
    CryHeardAt = Now();
    if (bAsleep && Identity)
    {
        const float Wake = (bParent ? (Identity->bFemale ? 0.25f : 0.08f) : 0.03f) * GameDelta / 10.0f;
        if (FMath::FRand() < Wake)
        {
            ForceWake();
            ACompleteHumanNPC* Crying = const_cast<ACompleteHumanNPC*>(Baby);
            Think(FString::Printf(TEXT("%s плачет"), *NameOf(Crying)), EThoughtKind::Observation, 0.7f, Crying);
        }
    }
}

void UMindComponent::FeelIdleness(float GameDelta)
{
    if (!FVillage::IsMedieval(this) || !Identity || !Needs || bAsleep)
    {
        return;
    }
    const int32 Day = FVillage::DayOfYear(this);
    if (WorkDay != Day)
    {
        WorkDay = Day;
        WorkedToday = 0.0f;
    }
    const UHumanWorldSubsystem* World = GetWorldMind();
    const float Hour = World ? World->Now.HourFloat : 12.0f;
    FVillage::JudgeDay(Needs, WorkedToday, FVillage::ExpectedWork(Identity->Age, Identity->Role, Day, Hour), GameDelta / 3600.0f);
}

void UMindComponent::InfantReflex(float GameDelta)
{
    if (!FVillage::IsMedieval(this) || !Identity || !Needs)
    {
        return;
    }
    if (Now() - CryHeardAt < 40.0f)
    {
        if (FNeedState* Calm = Needs->Find(ENeedType::Comfort))
        {
            Calm->Satisfaction = FMath::Min(Calm->Satisfaction, 0.4f);
        }
        Needs->Deprive(ENeedType::Order, 0.25f * GameDelta / 3600.0f);
    }
    bCrying = false;
    if (Identity->Age >= 1.5f)
    {
        return;
    }
    ACompleteHumanNPC* Me = GetHuman();
    UHumanWorldSubsystem* World = GetWorldMind();
    if (!Me || !Me->IsAlive() || !World)
    {
        return;
    }
    const float Hour = World->Now.HourFloat;
    const float Hunger = Needs->GetSatisfaction(ENeedType::Hunger);
    const float Ease = Needs->GetSatisfaction(ENeedType::Comfort);
    const bool bNight = Hour < 5.5f || Hour > 21.5f;
    if (bAsleep && Hunger < 0.3f)
    {
        ForceWake();
    }
    bCrying = !bAsleep && (Hunger < 0.45f || Ease < 0.3f || bNight);
    if (!bCrying)
    {
        return;
    }
    CryClock -= GameDelta;
    if (CryClock <= 0.0f)
    {
        CryClock = 20.0f;
        Me->ShowSpeech(Hunger < 0.45f ? TEXT("Уа-а! Уа-а!") : TEXT("Уа..."));
    }
    for (ACompleteHumanNPC* Kin : World->GetAllHumans())
    {
        if (!Kin || Kin == Me || !Kin->IsAlive() || !Kin->Mind || !Kin->IdentityComponent
            || Kin->IdentityComponent->Household != Identity->Household || Kin->IdentityComponent->Age < 12.0f)
        {
            continue;
        }
        if (FVector::Dist2D(Kin->GetActorLocation(), Me->GetActorLocation()) > 2500.0f)
        {
            continue;
        }
        const bool bParent = Identity->Mother.Get() == Kin || Identity->Father.Get() == Kin;
        Kin->Mind->HearBabyCry(Me, bParent, GameDelta);
    }
}

void UMindComponent::ReceiveOrder(AActor* From, const FAffordance& Task, const FString& Words)
{
    AddSuggestion(Task, From, 1.0f, true);
    OrderFrom = From;
    OrderKey = Task.Key;
    OrderAt = Now();
    Think(FString::Printf(TEXT("%s велит: %s. Надо исполнять."), *NameOf(From), *Words), EThoughtKind::Intention, 0.85f, From);
    if (Needs)
    {
        Needs->Deprive(ENeedType::Order, 0.25f);
    }
    if (ACompleteHumanNPC* Me = GetHuman())
    {
        Me->ShowSpeech(Spoken(Speech, TEXT("Слушаюсь."), Identity ? Identity->FirstName : FString(), NameOf(From)));
    }
    Report(FString::Printf(TEXT("получил(а) повеление от %s: %s"), *NameOf(From), *Words));
}

void UMindComponent::CheckOrders(float GameDelta)
{
    if (OrderAt < 0.0f)
    {
        return;
    }
    OrderCheck += GameDelta;
    if (OrderCheck < 120.0f)
    {
        return;
    }
    OrderCheck = 0.0f;
    if (Now() - OrderAt < 3.0f * 3600.0f)
    {
        return;
    }
    AActor* Lord = OrderFrom.Get();
    ++OrdersIgnored;
    OrderAt = -1.0f;
    Suggestions.RemoveAll([Lord](const FSuggestion& S) { return S.bInstruction && S.From == Lord; });
    if (Needs)
    {
        Needs->Deprive(ENeedType::Safety, 0.3f);
        Needs->Deprive(ENeedType::Esteem, 0.15f);
    }
    if (Social && Lord)
    {
        if (FRelationship* R = Social->Find(Lord))
        {
            R->Fear = FMath::Clamp(R->Fear + 0.1f, 0.0f, 1.0f);
            R->Liking = FMath::Clamp(R->Liking - 0.1f, -1.0f, 1.0f);
        }
    }
    if (ACompleteHumanNPC* Master = Cast<ACompleteHumanNPC>(Lord))
    {
        if (Identity && Master->IdentityComponent && Identity->Money >= 1.0f)
        {
            Identity->Money -= 1.0f;
            Master->IdentityComponent->Money += 1.0f;
        }
        if (Master->SocialComponent)
        {
            Master->SocialComponent->RecordInteraction(GetOwner(), -0.35f, Now(), Master->PersonalityComponent);
        }
    }
    Think(TEXT("не исполнил(а) повеления — будет мне от хозяев"), EThoughtKind::Feeling, 0.8f, Lord);
    Report(FString::Printf(TEXT("не исполнил(а) повеление %s — штраф и опала"), *NameOf(Lord)));
}

void UMindComponent::FilterDeals(TArray<FAffordance>& Out) const
{
    if (!FVillage::IsMedieval(this))
    {
        return;
    }
    const ACompleteHumanNPC* Me = GetHuman();
    if (!Me)
    {
        return;
    }
    const AResourceActor* Held = Cast<AResourceActor>(Me->CarriedItem.Get());
    const bool bRoyal = Identity && Identity->IsRoyal();
    const bool bServant = Identity && Identity->IsServant();
    const bool bHome = Me->HasHome();
    const FVector Home = Me->GetHomeLocation();
    const float Age = Identity ? Identity->Age : 30.0f;
    if (Age < 1.5f)
    {
        Out.RemoveAll([](const FAffordance& A)
        {
            const bool bOwn = A.Deal == TEXT("cry") || A.Deal == TEXT("tryrise");
            const bool bSleep = A.Action == EActionType::Sleep && A.Source != EAffordanceSource::Self;
            const bool bLie = A.Action == EActionType::Rest && A.Source == EAffordanceSource::Self;
            return !(bOwn || bSleep || bLie);
        });
        return;
    }
    if (Age < 6.0f)
    {
        Out.RemoveAll([](const FAffordance& A)
        {
            return A.Action == EActionType::Work || A.Action == EActionType::Cook || !A.Craft.IsNone()
                || A.Deal == TEXT("sell") || A.Deal == TEXT("buy") || A.Deal == TEXT("pack") || A.Deal == TEXT("learn")
                || A.Deal == TEXT("order") || A.Deal == TEXT("serve") || A.Deal == TEXT("petition")
                || A.Deal == TEXT("feed") || A.Deal == TEXT("cradle");
        });
    }
    Out.RemoveAll([&](const FAffordance& A)
    {
        if (A.Deal.IsNone())
        {
            return false;
        }
        if (A.Deal == TEXT("royal"))
        {
            return !bRoyal;
        }
        if (A.Deal == TEXT("serve"))
        {
            return !bServant;
        }
        if (A.Deal == TEXT("petition"))
        {
            return bRoyal || bServant;
        }
        if (A.Deal == TEXT("store"))
        {
            return !Held || (A.DealKind != EResourceKind::None && Held->Kind != A.DealKind)
                || (A.DealKind == EResourceKind::None && Held->Kind == EResourceKind::Water)
                || !bHome || FVector::Dist2D(A.Location, Home) > 1200.0f;
        }
        if (A.Deal == TEXT("pack"))
        {
            return Held != nullptr || !bHome || FVector::Dist2D(A.Location, Home) > 1200.0f;
        }
        if (A.Deal == TEXT("sell"))
        {
            return !Held || !FVillage::IsSellable(Held->Kind);
        }
        if (A.Deal == TEXT("buy"))
        {
            return Held && Held->Kind != A.DealKind;
        }
        return false;
    });
}

bool UMindComponent::SettleDeal(const FAffordance& A)
{
    if (A.Deal.IsNone())
    {
        return false;
    }
    ACompleteHumanNPC* Me = GetHuman();
    if (!Me)
    {
        return true;
    }
    const float T = Now();
    AResourceActor* Held = Cast<AResourceActor>(Me->CarriedItem.Get());
    AFurnitureActor* Thing = Cast<AFurnitureActor>(A.Target.Get());
    auto Drop = [Me, Held]()
    {
        Me->CarriedItem = nullptr;
        if (Me->Motor)
        {
            Me->Motor->ForgetHeld();
        }
        if (Held)
        {
            Held->Destroy();
        }
    };

    if (A.Deal == TEXT("store"))
    {
        if (Held && Thing && Held->Amount > 0.0f)
        {
            const EResourceKind Kind = Held->Kind;
            const float Amount = Held->Amount;
            Thing->Store(Kind, Amount);
            Drop();
            Report(FString::Printf(TEXT("убрал(а) в %s: %s ×%d"), *Thing->Name, *FCraftBook::NameOfKind(Kind), FMath::RoundToInt(Amount)));
        }
        return true;
    }

    // --- Угощение: передать кусок из рук в рот другому ----------------------
    // Человек отламывает от своего и протягивает. Тот ест: голод уходит,
    // а угощение запоминается обоим — и тому, кто дал, и тому, кто взял.
    if (A.Deal == TEXT("feed"))
    {
        ACompleteHumanNPC* Guest = Cast<ACompleteHumanNPC>(A.Target.Get());
        if (Held && Guest && Guest->Mind && Guest->PhysiologyComponent)
        {
            const EResourceKind Kind = Held->Kind;
            const FSubstance& S = FMatter::Of(Kind);
            const float Bite = FMath::Min(1.0f, FMath::Max(0.25f, Held->Amount * 0.4f));
            const float Took = Held->Consume(Bite);
            if (Held->Amount <= 0.01f)
            {
                Drop();
            }

            // Сытость по калорийности припаса: жирное насыщает быстрее.
            const float Kcal = S.Food > 0.0f ? S.Food : 300.0f;
            const float Fullness = FMath::Clamp(Kcal / 3000.0f, 0.12f, 0.7f);
            if (Guest->Mind->Needs)
            {
                Guest->Mind->Needs->Satisfy(ENeedType::Hunger, Fullness);
            }
            if (Guest->PhysiologyComponent)
            {
                Guest->PhysiologyComponent->Eat(Fullness);
            }
            if (Guest->Mind->Emotions)
            {
                FAppraisedEvent Fed;
                Fed.Tag = TEXT("FedBy");
                Fed.Description = FString::Printf(TEXT("%s угостил(а) — %s"),
                    *NameOf(Me), *FCraftBook::NameOfKind(Kind));
                Fed.Desirability = 0.45f;
                Fed.Unexpectedness = 0.3f;
                Fed.OtherAgency = 0.9f;
                Fed.Significance = 0.45f;
                Fed.Subject = Me;
                Fed.Location = Guest->GetActorLocation();
                Guest->Mind->Emotions->Appraise(Fed, Guest->PersonalityComponent);
            }
            Guest->Mind->JudgeByDeeds(Me, 0.06f, TEXT("поделился едой"));

            Think(FString::Printf(TEXT("поделился(ась) %s с %s — хорошо, что хватило"),
                *FCraftBook::NameOfKind(Kind), *NameOf(Guest)), EThoughtKind::Feeling, 0.6f, Guest);
            Report(FString::Printf(TEXT("угостил(а) %s: %s"), *NameOf(Guest), *FCraftBook::NameOfKind(Kind)));
        }
        return true;
    }

    if (A.Deal == TEXT("pack"))
    {
        if (Thing && !Held && A.DealKind != EResourceKind::None)
        {
            const float Keep = FVillage::IsFood(A.DealKind) ? 4.0f : 0.0f;
            const float Spare = FMath::FloorToFloat(FMath::Min(4.0f, Thing->HowMuchInside(A.DealKind) - Keep));
            if (Spare >= 1.0f)
            {
                const float Took = Thing->TakeOut(A.DealKind, Spare);
                TakeCargo(A.DealKind, Took);
                Report(FString::Printf(TEXT("набрал(а) на продажу: %s ×%d — понесёт на торг"),
                    *FCraftBook::NameOfKind(A.DealKind), FMath::RoundToInt(Took)));
            }
        }
        return true;
    }

    if (A.Deal == TEXT("sell"))
    {
        if (Held && Thing && FVillage::IsSellable(Held->Kind))
        {
            const EResourceKind Kind = Held->Kind;
            const float Amount = FMath::Max(1.0f, FMath::RoundToFloat(Held->Amount));
            const float Price = FMath::Max(1.0f, FMath::RoundToFloat(FVillage::PriceOf(Kind) * (0.9f + 0.3f * Mastery(TEXT("Trade")))));
            Drop();
            Thing->PutOnSale(Me, Kind, Amount, Price);
            const float Traders = FMath::FloorToFloat(Amount * FMath::FRandRange(0.3f, 0.7f));
            float Earned = 0.0f;
            if (Traders >= 1.0f)
            {
                for (AFurnitureActor::FStallLot& Lot : Thing->Lots)
                {
                    if (Lot.Kind == Kind && Lot.Owner.Get() == Me && Lot.Amount >= Traders)
                    {
                        Lot.Amount -= Traders;
                        Earned = FMath::RoundToFloat(Traders * Price * 0.7f);
                        break;
                    }
                }
                Thing->Lots.RemoveAll([](const AFurnitureActor::FStallLot& Lot) { return Lot.Amount < 0.5f; });
                Thing->BuildOffers();
                if (Identity)
                {
                    Identity->Money += Earned;
                }
            }
            Practised(TEXT("Trade"), 0.02f);
            Report(FString::Printf(TEXT("выставил(а) на торгу %s ×%d по %s; заезжий купец взял %d за %s"),
                *FCraftBook::NameOfKind(Kind), FMath::RoundToInt(Amount), *FVillage::Coins(Price),
                FMath::RoundToInt(Traders), *FVillage::Coins(Earned)));
        }
        return true;
    }

    if (A.Deal == TEXT("buy"))
    {
        float Paid = 0.0f;
        if (Thing && Thing->BuyFrom(Me, A.DealKind, 1.0f, Paid))
        {
            if (Identity)
            {
                Identity->Money += A.MoneyCost - Paid;
            }
            TakeCargo(A.DealKind, 1.0f);
            Report(FString::Printf(TEXT("купил(а) на торгу: %s за %s"), *FCraftBook::NameOfKind(A.DealKind), *FVillage::Coins(Paid)));
        }
        else
        {
            if (Identity)
            {
                Identity->Money += A.MoneyCost;
            }
            Report(TEXT("пришёл(ла) купить, а товар уже разобрали"));
        }
        return true;
    }

    if (A.Deal == TEXT("learn"))
    {
        ACompleteHumanNPC* Master = Cast<ACompleteHumanNPC>(A.Target.Get());
        const FName Trade = A.RequiredSkill;
        const bool bThere = Master && Master->Mind && Master->IsAlive() && !Master->Mind->bAsleep
            && FVector::Dist2D(Me->GetActorLocation(), Master->GetActorLocation()) < 600.0f;
        if (!bThere)
        {
            if (Identity)
            {
                Identity->Money += A.MoneyCost;
            }
            Report(FString::Printf(TEXT("пришёл(ла) учиться к %s, а мастера рядом нет"), *NameOf(Master)));
            return true;
        }
        const float Before = Mastery(Trade);
        const float Theirs = Master->Mind->Mastery(Trade);
        const float Teach = Master->Mind->Mastery(TEXT("Teaching"));
        const float Gain = FMath::Max(0.0f, Theirs - Before) * (0.12f + 0.1f * Teach);
        Practised(Trade, Gain / FMath::Max(0.05f, 1.0f - Before));
        const float After = Mastery(Trade);
        Master->Mind->Practised(TEXT("Teaching"), 0.02f);
        if (Master->Mind->Needs)
        {
            Master->Mind->Needs->Satisfy(ENeedType::Meaning, 0.15f);
            Master->Mind->Needs->Satisfy(ENeedType::Esteem, 0.12f);
        }
        if (A.MoneyCost > 0.0f && Master->IdentityComponent)
        {
            Master->IdentityComponent->Money += A.MoneyCost;
        }
        if (Master->SocialComponent)
        {
            Master->SocialComponent->RecordInteraction(Me, 0.3f, T, Master->PersonalityComponent);
        }
        if (Social)
        {
            Social->RecordInteraction(Master, 0.35f, T, Personality);
        }
        JudgeByDeeds(Master, 0.06f, TEXT("учит своему ремеслу"));
        const FVillageTrade* Info = FVillage::TradeOf(Trade);
        const FString Pupil = Identity ? Identity->FirstName : FString();
        Master->ShowSpeech(Spoken(Master->Mind->Speech, FString::Printf(TEXT("Смотри, %s: вот так надо."), *Pupil),
            Master->IdentityComponent ? Master->IdentityComponent->FirstName : FString(), Pupil));
        Master->Mind->Report(FString::Printf(TEXT("учил(а) %s: %s"), *Master->Mind->NameOf(Me), Info ? Info->Deed : TEXT("ремеслу")));
        Report(FString::Printf(TEXT("учился(ась) у мастера %s: %s, умение %d%% → %d%%"), *NameOf(Master),
            Info ? Info->Deed : TEXT("ремесло"), FMath::RoundToInt(Before * 100.0f), FMath::RoundToInt(After * 100.0f)));
        return true;
    }

    if (A.Deal == TEXT("order"))
    {
        ACompleteHumanNPC* Servant = Cast<ACompleteHumanNPC>(A.Target.Get());
        FAffordance Task;
        FString Words;
        if (Servant && Servant->Mind && Servant->IsAlive() && PickCastleTask(Task, Words)
            && FVector::Dist2D(Me->GetActorLocation(), Servant->GetActorLocation()) < 800.0f)
        {
            Me->ShowSpeech(Spoken(Speech, FString::Printf(TEXT("%s, %s!"), *NameOf(Servant), *Words),
                Identity ? Identity->FirstName : FString(), NameOf(Servant)));
            Servant->Mind->ReceiveOrder(Me, Task, Words);
            Report(FString::Printf(TEXT("велел(а) %s: %s"), *NameOf(Servant), *Words));
        }
        return true;
    }

    if (A.Deal == TEXT("feed") || A.Deal == TEXT("cradle"))
    {
        ACompleteHumanNPC* Baby = Cast<ACompleteHumanNPC>(A.Target.Get());
        if (!Baby || !Baby->Mind || !Baby->IsAlive() || FVector::Dist2D(Me->GetActorLocation(), Baby->GetActorLocation()) > 600.0f)
        {
            Report(TEXT("пришёл(ла) к дитю, а его на месте нет"));
            return true;
        }
        const FString BabyName = NameOf(Baby);
        if (A.Deal == TEXT("feed"))
        {
            if (Baby->PhysiologyComponent)
            {
                Baby->PhysiologyComponent->Eat(0.7f);
            }
            if (Baby->Mind->Needs)
            {
                Baby->Mind->Needs->Satisfy(ENeedType::Belonging, 0.2f);
                Baby->Mind->Needs->Satisfy(ENeedType::Comfort, 0.2f);
            }
            if (Baby->IdentityComponent && Baby->IdentityComponent->Age >= 1.0f)
            {
                if (AFurnitureActor* Pot = FVillage::HomeStore(Me, EResourceKind::CookedFood))
                {
                    Pot->TakeOut(EResourceKind::CookedFood, 0.3f);
                }
            }
            Me->ShowSpeech(Spoken(Speech, FString::Printf(TEXT("Кушай, %s."), *BabyName), Identity ? Identity->FirstName : FString(), BabyName));
            Report(FString::Printf(TEXT("покормил(а) дитя %s"), *BabyName));
        }
        else
        {
            if (Me->HasHome())
            {
                for (TActorIterator<AFurnitureActor> It(Me->GetWorld()); It; ++It)
                {
                    if (*It && It->FurnitureType == EFurnitureType::Crib && FVector::Dist2D(It->GetActorLocation(), Me->GetHomeLocation()) < 1300.0f)
                    {
                        Baby->SetActorLocation(It->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f), false, nullptr, ETeleportType::TeleportPhysics);
                        break;
                    }
                }
            }
            Baby->Mind->ForceSleep();
            Me->ShowSpeech(Spoken(Speech, TEXT("Баю-баюшки-баю..."), Identity ? Identity->FirstName : FString(), BabyName));
            Report(FString::Printf(TEXT("уложил(а) дитя %s спать"), *BabyName));
        }
        if (Baby->SocialComponent)
        {
            Baby->SocialComponent->RecordInteraction(Me, 0.4f, T, Baby->PersonalityComponent);
        }
        if (Social)
        {
            Social->RecordInteraction(Baby, 0.3f, T, Personality);
        }
        return true;
    }

    if (A.Deal == TEXT("cry"))
    {
        Me->ShowSpeech(Speech ? Speech->Articulate(TEXT("Мама!")) : FString(TEXT("Уа-уа!")));
        return true;
    }

    if (A.Deal == TEXT("tryrise"))
    {
        Report(FString::Printf(TEXT("пробовал(а) ходить: упражнений в голове %d"), Me->Motor ? Me->Motor->GetInfantPractice() : 0));
        return true;
    }

    if (A.Deal == TEXT("serve") && Identity && Identity->Lord.IsValid())
    {
        if (ACompleteHumanNPC* Lord = Cast<ACompleteHumanNPC>(Identity->Lord.Get()))
        {
            if (Lord->IdentityComponent)
            {
                const float Wage = A.Duration / 3600.0f * Identity->HourlyWage;
                Lord->IdentityComponent->Money = FMath::Max(0.0f, Lord->IdentityComponent->Money - Wage);
            }
        }
        return true;
    }

    return true;
}
