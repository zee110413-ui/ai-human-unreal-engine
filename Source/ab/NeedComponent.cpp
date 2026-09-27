// NeedComponent.cpp

#include "NeedComponent.h"
#include "PhysiologyComponent.h"
#include "PersonalityComponent.h"

UNeedComponent::UNeedComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    BuildDefaults();
}

void UNeedComponent::BuildDefaults()
{
    const int32 Count = static_cast<int32>(ENeedType::MAX);
    Needs.Reset();
    Needs.SetNum(Count);
    DeprivationTime.Reset();
    DeprivationTime.SetNumZeroed(Count);

    for (int32 i = 0; i < Count; ++i)
    {
        FNeedState& N = Needs[i];
        N.Type = static_cast<ENeedType>(i);
        N.Satisfaction = FMath::FRandRange(0.55f, 0.9f);
        N.Weight = 1.0f;
        N.UrgentThreshold = 0.35f;
        N.DecayPerSecond = 0.0f; // телесные считаются из тела, остальным зададим ниже
    }

    // Скорости «остывания» психологических потребностей (в секунду игрового времени).
    auto SetDecay = [this](ENeedType T, float PerHour, float Threshold)
    {
        if (FNeedState* N = Find(T))
        {
            N->DecayPerSecond = PerHour / 3600.0f;
            N->UrgentThreshold = Threshold;
        }
    };

    // Значения подобраны так, чтобы за сутки потребность успевала заметно
    // просесть. Иначе человек, начавший день сытым и спокойным, за целый
    // день так ничего и не захочет — а это не жизнь, а анабиоз.
    SetDecay(ENeedType::SocialContact, 0.070f, 0.40f);   // за день пустеет
    SetDecay(ENeedType::Belonging,     0.038f, 0.35f);
    SetDecay(ENeedType::Intimacy,      0.026f, 0.30f);
    SetDecay(ENeedType::Esteem,        0.015f, 0.35f);
    SetDecay(ENeedType::Achievement,   0.030f, 0.30f);
    SetDecay(ENeedType::Autonomy,      0.010f, 0.30f);
    SetDecay(ENeedType::Competence,    0.028f, 0.30f);
    SetDecay(ENeedType::Novelty,       0.090f, 0.40f);
    SetDecay(ENeedType::Beauty,        0.030f, 0.25f);
    SetDecay(ENeedType::Meaning,       0.016f, 0.25f);   // смысл уходит медленнее всего
    SetDecay(ENeedType::Order,         0.036f, 0.30f);
    SetDecay(ENeedType::Money,         0.010f, 0.30f);
}

void UNeedComponent::Setup(const UPersonalityComponent* Personality)
{
    if (!Personality)
    {
        return;
    }

    const FBigFive& T = Personality->Traits;
    const FPersonalityFacets& F = Personality->Facets;
    const FValueSystem& V = Personality->Values;

    // Веса — это «что для меня вообще важно». Именно они делают из одинаковых
    // тел разных людей: один не может без людей, другой — без смысла.
    SetWeight(ENeedType::SocialContact, 0.5f + F.Sociability * 1.2f);
    SetWeight(ENeedType::Belonging,     0.4f + T.Agreeableness * 0.8f + V.Conformity * 0.5f);
    SetWeight(ENeedType::Intimacy,      0.3f + F.Empathy * 0.9f + T.Agreeableness * 0.4f);
    SetWeight(ENeedType::Esteem,        0.4f + F.Ambition * 0.9f + V.Power * 0.5f);
    SetWeight(ENeedType::Achievement,   0.3f + V.Achievement * 1.2f);
    SetWeight(ENeedType::Autonomy,      0.3f + V.SelfDirection * 1.1f);
    SetWeight(ENeedType::Competence,    0.4f + T.Conscientiousness * 0.8f);
    SetWeight(ENeedType::Novelty,       0.3f + T.Openness * 1.1f + V.Stimulation * 0.5f);
    SetWeight(ENeedType::Beauty,        0.1f + T.Openness * 0.8f);
    SetWeight(ENeedType::Meaning,       0.2f + T.Openness * 0.6f + V.Universalism * 0.7f);
    SetWeight(ENeedType::Order,         0.3f + T.Conscientiousness * 0.9f + V.Security * 0.6f);
    SetWeight(ENeedType::Safety,        0.6f + T.Neuroticism * 0.9f + V.Security * 0.5f);
    SetWeight(ENeedType::Money,         0.3f + V.Security * 0.7f + V.Power * 0.6f);
    SetWeight(ENeedType::Shelter,       0.7f + V.Security * 0.5f);

    // Телесные потребности примерно одинаково важны для всех — иначе человек
    // просто не выживет. Но гедонисту еда всё же чуть важнее.
    SetWeight(ENeedType::Hunger,  1.4f + V.Hedonism * 0.2f);
    SetWeight(ENeedType::Thirst,  1.5f);
    SetWeight(ENeedType::Sleep,   1.3f);
    SetWeight(ENeedType::Bladder, 1.2f);
    SetWeight(ENeedType::Hygiene, 0.4f + Personality->Morals.Sanctity * 0.8f + T.Conscientiousness * 0.4f);
    SetWeight(ENeedType::Comfort, 0.8f + T.Neuroticism * 0.4f);
    SetWeight(ENeedType::Health,  1.0f);

    // Стартовое состояние индивидуально и не идеально: у одного с утра всё
    // хорошо, другой уже который день не с кем словом перемолвиться.
    // Без этого разброса весь город начнёт жизнь с одинакового безразличия.
    for (FNeedState& N : Needs)
    {
        N.Satisfaction = FMath::Clamp(FMath::FRandRange(0.25f, 0.95f), 0.05f, 1.0f);
    }
    // Тело в начале дня всё-таки в порядке.
    if (FNeedState* N = Find(ENeedType::Health))  N->Satisfaction = FMath::FRandRange(0.8f, 1.0f);
    if (FNeedState* N = Find(ENeedType::Safety))  N->Satisfaction = FMath::FRandRange(0.6f, 0.95f);
}

// ---------------------------------------------------------------------------
//  Шаг
// ---------------------------------------------------------------------------

void UNeedComponent::ReadBody(const UPhysiologyComponent* Physiology, float HourOfDay)
{
    if (!Physiology)
    {
        return;
    }
    const FBodyState& B = Physiology->Body;
    if (FNeedState* N = Find(ENeedType::Hunger))  N->Satisfaction = Physiology->GetSatiety();
    if (FNeedState* N = Find(ENeedType::Thirst))  N->Satisfaction = B.Hydration;
    if (FNeedState* N = Find(ENeedType::Bladder)) N->Satisfaction = 1.0f - B.BladderFullness;
    if (FNeedState* N = Find(ENeedType::Hygiene)) N->Satisfaction = B.Cleanliness;
    if (FNeedState* N = Find(ENeedType::Health))  N->Satisfaction = B.Health;
    if (FNeedState* N = Find(ENeedType::Comfort)) N->Satisfaction = 1.0f - FMath::Clamp(B.Pain + FMath::Max(0.0f, B.BodyTemperature - 37.5f) * 0.4f, 0.0f, 1.0f);
    if (FNeedState* N = Find(ENeedType::Sleep))   N->Satisfaction = FMath::Clamp(1.0f - Physiology->GetSleepPressure(HourOfDay), 0.0f, 1.0f);
}

void UNeedComponent::Advance(float GameDelta, const UPhysiologyComponent* Physiology, const FNeedContext& Context)
{
    if (GameDelta <= 0.0f)
    {
        return;
    }
    CurrentTime += GameDelta;

    ReadBody(Physiology, Context.HourOfDay);

    // --- Психологические потребности ----------------------------------------
    // Во сне почти всё замирает — кроме того, что сон и лечит.
    const float Scale = Context.bAsleep ? 0.15f : 1.0f;

    auto Decay = [&](ENeedType Type, float Modifier)
    {
        if (FNeedState* N = Find(Type))
        {
            N->Satisfaction = FMath::Clamp(N->Satisfaction - N->DecayPerSecond * GameDelta * Scale * Modifier, 0.0f, 1.0f);
        }
    };

    // Общение уходит быстрее, если человек один; рядом с людьми — медленнее.
    const float SocialModifier = (Context.PeopleNearby > 0) ? 0.25f : 1.0f;
    Decay(ENeedType::SocialContact, SocialModifier);

    // Принадлежность тает без «своих» рядом.
    Decay(ENeedType::Belonging, Context.ClosenessNearby > 0.4f ? 0.2f : 1.0f);
    Decay(ENeedType::Intimacy, Context.ClosenessNearby > 0.6f ? 0.1f : 1.0f);

    Decay(ENeedType::Esteem, 1.0f);
    Decay(ENeedType::Achievement, 1.0f);
    Decay(ENeedType::Autonomy, 1.0f);
    Decay(ENeedType::Competence, 1.0f);
    Decay(ENeedType::Meaning, 1.0f);

    // Новизна выгорает тем быстрее, чем однообразнее день.
    Decay(ENeedType::Novelty, 0.6f + Context.RoutinePredictability * 0.8f);

    // --- Потребности, читаемые прямо из обстановки --------------------------
    if (FNeedState* N = Find(ENeedType::Safety))
    {
        const float Target = FMath::Min(1.0f - FMath::Clamp(Context.PerceivedDanger, 0.0f, 1.0f),
            0.1f + 0.9f * FMath::Clamp(Context.Provision, 0.0f, 1.0f));
        // Безопасность возвращается медленнее, чем теряется: испуг держится.
        const float Rate = (Target < N->Satisfaction) ? 0.002f : 0.0003f;
        N->Satisfaction = FMath::Clamp(FMath::FInterpConstantTo(N->Satisfaction, Target, GameDelta, Rate), 0.0f, 1.0f);
    }

    if (FNeedState* N = Find(ENeedType::Shelter))
    {
        const float Target = Context.bHasHome ? (Context.bAtHome ? 1.0f : 0.75f) : 0.15f;
        N->Satisfaction = FMath::Clamp(FMath::FInterpConstantTo(N->Satisfaction, Target, GameDelta, 0.0008f), 0.0f, 1.0f);
    }

    if (FNeedState* N = Find(ENeedType::Money))
    {
        // Сытость деньгами насыщается логарифмически: первые сто важнее следующей тысячи.
        // Сытость деньгами насыщается логарифмически, но не бесконечно:
        // двухсот рублей в кармане достаточно, чтобы не думать о них.
        const float Target = FMath::Clamp(FMath::Loge(1.0f + FMath::Max(0.0f, Context.Money)) / FMath::Loge(1.0f + 200.0f), 0.0f, 1.0f);

        // Наличие работы само по себе не кормит: спокойнее с ней немного,
        // но пустой карман остаётся пустым карманом.
        const float Employment = Context.bEmployed ? 0.05f : 0.0f;
        N->Satisfaction = FMath::Clamp(Target + Employment, 0.0f, 1.0f);
    }

    if (FNeedState* N = Find(ENeedType::Order))
    {
        N->Satisfaction = FMath::Clamp(
            FMath::FInterpConstantTo(N->Satisfaction, Context.RoutinePredictability, GameDelta, 0.0004f), 0.0f, 1.0f);
    }

    if (FNeedState* N = Find(ENeedType::Beauty))
    {
        // Красота вокруг понемногу подпитывает — но к ней привыкаешь.
        const float Target = FMath::Clamp(Context.SurroundingBeauty * 0.8f + 0.1f, 0.0f, 1.0f);
        if (Target > N->Satisfaction)
        {
            N->Satisfaction = FMath::Clamp(FMath::FInterpConstantTo(N->Satisfaction, Target, GameDelta, 0.0002f), 0.0f, 1.0f);
        }
    }

    // --- Учёт депривации: чем дольше не хватает, тем невыносимее -------------
    for (int32 i = 0; i < Needs.Num(); ++i)
    {
        const FNeedState& N = Needs[i];
        if (N.Satisfaction < N.UrgentThreshold)
        {
            DeprivationTime[i] += GameDelta;
        }
        else
        {
            // Отпустило — но память о лишении спадает не мгновенно.
            DeprivationTime[i] = FMath::Max(0.0f, DeprivationTime[i] - GameDelta * 3.0f);
            if (N.Satisfaction > 0.7f)
            {
                Needs[i].LastSatisfiedAt = CurrentTime;
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Доступ
// ---------------------------------------------------------------------------

FNeedState* UNeedComponent::Find(ENeedType Type)
{
    const int32 Index = static_cast<int32>(Type);
    return Needs.IsValidIndex(Index) ? &Needs[Index] : nullptr;
}

const FNeedState* UNeedComponent::Find(ENeedType Type) const
{
    const int32 Index = static_cast<int32>(Type);
    return Needs.IsValidIndex(Index) ? &Needs[Index] : nullptr;
}

float UNeedComponent::GetSatisfaction(ENeedType Type) const
{
    const FNeedState* N = Find(Type);
    return N ? N->Satisfaction : 1.0f;
}

float UNeedComponent::GetUrgency(ENeedType Type) const
{
    const FNeedState* N = Find(Type);
    if (!N)
    {
        return 0.0f;
    }

    // Нехватка растёт нелинейно: разница между «сыт» и «слегка голоден» мала,
    // а между «голоден» и «голодает» — огромна.
    //
    // Но не квадратично: при квадрате умеренное желание («неплохо бы с кем-то
    // поговорить») превращается в ноль, и человек, у которого всё более-менее
    // в порядке, перестаёт хотеть чего-либо вообще и просто стоит. Живой
    // действует и по несильным побуждениям — показатель 1.6 это сохраняет,
    // оставляя острой нужде полное преимущество.
    const float Deficit = 1.0f - N->Satisfaction;
    float Urgency = FMath::Pow(Deficit, 1.6f) * N->Weight;

    // Долгая депривация добавляет отчаяния сверх самой нехватки.
    const int32 Index = static_cast<int32>(Type);
    if (DeprivationTime.IsValidIndex(Index))
    {
        const float Hours = DeprivationTime[Index] / 3600.0f;
        Urgency *= (1.0f + FMath::Clamp(Hours / 12.0f, 0.0f, 1.2f));
    }

    // Ниже порога потребность начинает перекрикивать всё остальное.
    if (N->Satisfaction < N->UrgentThreshold)
    {
        Urgency *= 1.6f;
    }
    if (N->Satisfaction < 0.12f)
    {
        Urgency *= 2.2f;
    }

    return FMath::Clamp(Urgency, 0.0f, 10.0f);
}

float UNeedComponent::ProjectSatisfaction(ENeedType Type, float SecondsAhead) const
{
    const FNeedState* N = Find(Type);
    if (!N)
    {
        return 1.0f;
    }

    // Телесные потребности убывают со своей скоростью, психологические —
    // со своей. Точного предсказания не нужно: человек и сам прикидывает
    // грубо, «часа через два проголодаюсь».
    float Rate = N->DecayPerSecond;
    if (Rate <= 0.0f)
    {
        // У телесных скорость задана телом, а не здесь: берём разумную
        // оценку по тому, за сколько они опустошаются.
        switch (Type)
        {
        case ENeedType::Hunger:  Rate = 1.0f / (6.0f * 3600.0f); break;
        case ENeedType::Thirst:  Rate = 1.0f / (14.0f * 3600.0f); break;
        case ENeedType::Bladder: Rate = 1.0f / (9.0f * 3600.0f); break;
        case ENeedType::Sleep:   Rate = 1.0f / (17.0f * 3600.0f); break;
        case ENeedType::Hygiene: Rate = 1.0f / (30.0f * 3600.0f); break;
        default:                 Rate = 0.0f; break;
        }
    }

    return FMath::Clamp(N->Satisfaction - Rate * FMath::Max(0.0f, SecondsAhead), 0.0f, 1.0f);
}

float UNeedComponent::ProjectUrgency(ENeedType Type, float SecondsAhead) const
{
    const FNeedState* N = Find(Type);
    if (!N)
    {
        return 0.0f;
    }

    const float Future = ProjectSatisfaction(Type, SecondsAhead);
    const float Deficit = 1.0f - Future;

    float Urgency = FMath::Pow(Deficit, 1.6f) * N->Weight;
    if (Future < N->UrgentThreshold)
    {
        Urgency *= 1.6f;
    }
    return FMath::Clamp(Urgency, 0.0f, 10.0f);
}

ENeedType UNeedComponent::GetMostUrgent() const
{
    ENeedType Best = ENeedType::Hunger;
    float BestScore = -1.0f;

    for (const FNeedState& N : Needs)
    {
        const float Score = GetUrgency(N.Type);
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = N.Type;
        }
    }
    return Best;
}

TArray<ENeedType> UNeedComponent::GetTopUrgent(int32 Count) const
{
    struct FEntry { ENeedType Type; float Score; };
    TArray<FEntry> Entries;
    Entries.Reserve(Needs.Num());

    for (const FNeedState& N : Needs)
    {
        Entries.Add({ N.Type, GetUrgency(N.Type) });
    }
    Entries.Sort([](const FEntry& A, const FEntry& B) { return A.Score > B.Score; });

    TArray<ENeedType> Result;
    const int32 Num = FMath::Min(Count, Entries.Num());
    for (int32 i = 0; i < Num; ++i)
    {
        Result.Add(Entries[i].Type);
    }
    return Result;
}

void UNeedComponent::Satisfy(ENeedType Type, float Amount)
{
    if (FNeedState* N = Find(Type))
    {
        N->Satisfaction = FMath::Clamp(N->Satisfaction + Amount, 0.0f, 1.0f);
        N->LastSatisfiedAt = CurrentTime;

        const int32 Index = static_cast<int32>(Type);
        if (DeprivationTime.IsValidIndex(Index) && N->Satisfaction > N->UrgentThreshold)
        {
            DeprivationTime[Index] = 0.0f;
        }
    }
}

void UNeedComponent::Deprive(ENeedType Type, float Amount)
{
    if (FNeedState* N = Find(Type))
    {
        N->Satisfaction = FMath::Clamp(N->Satisfaction - FMath::Abs(Amount), 0.0f, 1.0f);
    }
}

float UNeedComponent::GetTotalDistress() const
{
    float Sum = 0.0f;
    float WeightSum = 0.0f;
    for (const FNeedState& N : Needs)
    {
        Sum += (1.0f - N.Satisfaction) * N.Weight;
        WeightSum += N.Weight;
    }
    return WeightSum > 0.0f ? FMath::Clamp(Sum / WeightSum, 0.0f, 1.0f) : 0.0f;
}

float UNeedComponent::GetDeprivationTime(ENeedType Type) const
{
    const int32 Index = static_cast<int32>(Type);
    return DeprivationTime.IsValidIndex(Index) ? DeprivationTime[Index] : 0.0f;
}

void UNeedComponent::SetWeight(ENeedType Type, float Weight)
{
    if (FNeedState* N = Find(Type))
    {
        N->Weight = FMath::Clamp(Weight, 0.05f, 3.0f);
    }
}
