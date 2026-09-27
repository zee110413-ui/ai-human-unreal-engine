// MotivationComponent.cpp

#include "MotivationComponent.h"
#include "PersonalityComponent.h"
#include "NeedComponent.h"

UMotivationComponent::UMotivationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMotivationComponent::Setup(const UPersonalityComponent* Personality)
{
    WillpowerCapacity = Personality ? FMath::Clamp(0.4f + Personality->BaseWillpower() * 0.8f, 0.2f, 1.2f) : 1.0f;
    Willpower = WillpowerCapacity;
}

// ---------------------------------------------------------------------------
//  Шаг
// ---------------------------------------------------------------------------

void UMotivationComponent::Advance(float GameDelta, float WorldTime, bool bResting, const UPersonalityComponent* Personality)
{
    if (GameDelta <= 0.0f)
    {
        return;
    }

    const float Hours = GameDelta / 3600.0f;

    // --- Воля восстанавливается: во сне быстро, в покое медленно ------------
    const float Regen = bResting ? 0.30f : 0.06f;
    Willpower = FMath::Clamp(Willpower + Hours * Regen, 0.0f, WillpowerCapacity);

    // --- Срочность целей пересчитывается: дедлайны подгоняют ---------------
    for (FGoal& G : Goals)
    {
        if (G.Status != EGoalStatus::Active && G.Status != EGoalStatus::Pending && G.Status != EGoalStatus::Suspended)
        {
            continue;
        }

        if (G.Deadline > 0.0f)
        {
            const float Left = G.Deadline - WorldTime;
            if (Left <= 0.0f)
            {
                FailGoal(G.Id, WorldTime, Personality);
                continue;
            }
            // Классическая кривая прокрастинации: пока срок далеко — всё равно,
            // за час до конца — единственное, что важно.
            G.Urgency = FMath::Clamp(1.0f / (1.0f + Left / 7200.0f), 0.0f, 1.0f);
        }

        // Долго висящая цель либо начинает грызть, либо тихо умирает.
        const float AgeHours = (WorldTime - G.CreatedAt) / 3600.0f;
        if (G.Horizon == EGoalHorizon::Immediate && AgeHours > 8.0f && G.Progress < 0.1f)
        {
            AbandonGoal(G.Id, WorldTime);
        }
    }

    PruneGoals(WorldTime);
}

void UMotivationComponent::PruneGoals(float WorldTime)
{
    for (int32 i = Goals.Num() - 1; i >= 0; --i)
    {
        const FGoal& G = Goals[i];
        const bool bDone = (G.Status == EGoalStatus::Achieved
                            || G.Status == EGoalStatus::Failed
                            || G.Status == EGoalStatus::Abandoned);

        if (bDone && (WorldTime - G.CreatedAt) > 7200.0f)
        {
            if (G.Id == ActiveGoalId)
            {
                ActiveGoalId = -1;
            }
            Goals.RemoveAt(i);
        }
    }

    // Слишком много целей — голова кругом. Отбрасываем наименее важные.
    while (Goals.Num() > GoalCapacity)
    {
        int32 WeakestIndex = INDEX_NONE;
        float WeakestScore = FLT_MAX;
        for (int32 i = 0; i < Goals.Num(); ++i)
        {
            if (Goals[i].Horizon == EGoalHorizon::Life || Goals[i].Id == ActiveGoalId)
            {
                continue;
            }
            const float Score = Goals[i].Importance * Goals[i].Urgency;
            if (Score < WeakestScore)
            {
                WeakestScore = Score;
                WeakestIndex = i;
            }
        }
        if (WeakestIndex == INDEX_NONE)
        {
            break;
        }
        Goals.RemoveAt(WeakestIndex);
    }
}

// ---------------------------------------------------------------------------
//  Цели
// ---------------------------------------------------------------------------

int32 UMotivationComponent::CreateGoal(const FString& Name, ENeedType Need, EGoalHorizon Horizon,
                                       float Importance, float WorldTime, float Deadline)
{
    // Дубликаты не заводим — вместо этого обновляем важность.
    for (FGoal& G : Goals)
    {
        if (G.DrivingNeed == Need && G.Horizon == Horizon
            && (G.Status == EGoalStatus::Pending || G.Status == EGoalStatus::Active || G.Status == EGoalStatus::Suspended))
        {
            G.Importance = FMath::Max(G.Importance, Importance);
            return G.Id;
        }
    }

    FGoal G;
    G.Id = NextGoalId++;
    G.Name = Name;
    G.DrivingNeed = Need;
    G.Horizon = Horizon;
    G.Status = EGoalStatus::Pending;
    G.Importance = FMath::Clamp(Importance, 0.0f, 1.0f);
    G.Urgency = G.Importance;
    G.Expectancy = 0.6f;
    G.CreatedAt = WorldTime;
    G.Deadline = Deadline;

    Goals.Add(G);
    return G.Id;
}

int32 UMotivationComponent::SetLifeGoal(const FString& Name, ENeedType Need, float WorldTime)
{
    const int32 Id = CreateGoal(Name, Need, EGoalHorizon::Life, 1.0f, WorldTime);
    if (FGoal* G = FindGoal(Id))
    {
        G->Expectancy = 0.4f; // мечты сбываются не часто
    }
    return Id;
}

// ---------------------------------------------------------------------------
//  ЗАМЫСЛЫ
//
//  Цель — не «то, что надо сделать». Цель — это то, что человек хочет
//  изменить в своей жизни. Голод целью не становится: голодный просто идёт
//  есть. А вот годами не хватающее уважение однажды складывается
//  в намерение — и начинает подкрашивать всё, что человек выбирает.
// ---------------------------------------------------------------------------

void UMotivationComponent::FormLongTermAims(const UNeedComponent* Needs, const UPersonalityComponent* Personality, float WorldTime)
{
    if (!Needs)
    {
        return;
    }

    for (const FNeedState& Need : Needs->Needs)
    {
        // Телесное в замыслы не превращается — оно решается на месте.
        switch (Need.Type)
        {
        case ENeedType::Hunger:
        case ENeedType::Thirst:
        case ENeedType::Bladder:
        case ENeedType::Sleep:
        case ENeedType::Hygiene:
        case ENeedType::Comfort:
            continue;
        default:
            break;
        }

        // Сколько времени этого не хватает. Замысел рождается из ХРОНИКИ.
        const float DeprivedHours = Needs->GetDeprivationTime(Need.Type) / 3600.0f;
        if (DeprivedHours < 6.0f)
        {
            continue;
        }

        // И только если это вообще важно для этого человека.
        if (Need.Weight < 0.7f)
        {
            continue;
        }

        // Чем дольше не хватает, тем крупнее замысел.
        const EGoalHorizon Horizon = (DeprivedHours > 48.0f)
            ? EGoalHorizon::Life
            : (DeprivedHours > 18.0f ? EGoalHorizon::Medium : EGoalHorizon::Short);

        const float Importance = FMath::Clamp(
            0.3f + FMath::Loge(1.0f + DeprivedHours) / 5.0f * Need.Weight, 0.0f, 1.0f);

        const int32 Id = CreateGoal(HumanText::NeedDesire(Need.Type), Need.Type, Horizon, Importance, WorldTime);

        if (FGoal* G = FindGoal(Id))
        {
            G->Importance = FMath::Max(G->Importance, Importance);

            // Верит ли человек, что у него получится, — из его опыта неудач
            // и из его характера. Отсюда и берётся отчаяние.
            const float Optimism = Personality ? Personality->Facets.Optimism : 0.5f;
            G->Expectancy = FMath::Clamp(0.25f + Optimism * 0.5f - ConsecutiveFailures * 0.05f, 0.05f, 0.95f);
        }
    }
}

void UMotivationComponent::RegisterProgress(ENeedType Need, float Amount, float WorldTime)
{
    for (FGoal& G : Goals)
    {
        if (G.DrivingNeed != Need)
        {
            continue;
        }
        if (G.Status == EGoalStatus::Achieved || G.Status == EGoalStatus::Failed || G.Status == EGoalStatus::Abandoned)
        {
            continue;
        }

        // Крупный замысел двигается медленнее: до жизненной цели далеко.
        float Scale = 1.0f;
        switch (G.Horizon)
        {
        case EGoalHorizon::Short:  Scale = 0.6f; break;
        case EGoalHorizon::Medium: Scale = 0.25f; break;
        case EGoalHorizon::Life:   Scale = 0.08f; break;
        default: break;
        }

        G.Progress = FMath::Clamp(G.Progress + Amount * Scale, 0.0f, 1.0f);

        // Каждый сдвиг укрепляет веру, что это вообще возможно.
        G.Expectancy = FMath::Clamp(G.Expectancy + Amount * Scale * 0.3f, 0.0f, 1.0f);

        if (G.Progress >= 1.0f)
        {
            AchieveGoal(G.Id, WorldTime);
        }
    }
}

void UMotivationComponent::ReviewAims(float WorldTime, const UPersonalityComponent* Personality)
{
    const float Persistence = Personality
        ? (Personality->Traits.Conscientiousness * 0.6f + Personality->Facets.Stubbornness * 0.4f)
        : 0.5f;

    for (FGoal& G : Goals)
    {
        if (G.Status == EGoalStatus::Achieved || G.Status == EGoalStatus::Failed || G.Status == EGoalStatus::Abandoned)
        {
            continue;
        }

        const float AgeHours = (WorldTime - G.CreatedAt) / 3600.0f;

        // Замысел, который долго не двигается, начинает выцветать.
        if (AgeHours > 24.0f && G.Progress < 0.08f)
        {
            G.Expectancy = FMath::Max(0.02f, G.Expectancy - 0.04f);

            // И однажды человек просто перестаёт в него верить.
            // Упрямый держится дольше, но не вечно.
            const float GiveUp = (1.0f - Persistence) * 0.08f * (1.0f - G.Expectancy);
            if (G.Horizon != EGoalHorizon::Life && FMath::FRand() < GiveUp)
            {
                AbandonGoal(G.Id, WorldTime);
            }
        }
    }
}

int32 UMotivationComponent::CreateGoalWithPlan(const FString& Name, ENeedType Need, EGoalHorizon Horizon,
                                               float Importance, const TArray<FPlanStep>& Steps, float WorldTime)
{
    FGoal G;
    G.Id = NextGoalId++;
    G.Name = Name;
    G.DrivingNeed = Need;
    G.Horizon = Horizon;
    G.Status = EGoalStatus::Pending;
    G.Importance = FMath::Clamp(Importance, 0.0f, 1.0f);
    G.Urgency = FMath::Clamp(Importance, 0.0f, 1.0f);
    G.Expectancy = 0.7f;
    G.CreatedAt = WorldTime;
    G.Plan = Steps;
    G.CurrentStep = 0;

    Goals.Add(G);
    PruneGoals(WorldTime);
    return G.Id;
}

FGoal* UMotivationComponent::FindGoal(int32 Id)
{
    for (FGoal& G : Goals)
    {
        if (G.Id == Id)
        {
            return &G;
        }
    }
    return nullptr;
}

const FGoal* UMotivationComponent::FindGoal(int32 Id) const
{
    for (const FGoal& G : Goals)
    {
        if (G.Id == Id)
        {
            return &G;
        }
    }
    return nullptr;
}

FGoal* UMotivationComponent::GetActiveGoal()
{
    return (ActiveGoalId >= 0) ? FindGoal(ActiveGoalId) : nullptr;
}

// ---------------------------------------------------------------------------
//  Оценка и выбор — здесь принимается решение
// ---------------------------------------------------------------------------

float UMotivationComponent::EvaluateGoal(const FGoal& Goal, const UPersonalityComponent* Personality, float Stress) const
{
    if (Goal.Status == EGoalStatus::Achieved || Goal.Status == EGoalStatus::Failed || Goal.Status == EGoalStatus::Abandoned)
    {
        return -1.0f;
    }

    // Теория ожидаемой ценности: хочу × верю, что получится.
    float Value = Goal.Importance * Goal.Urgency;
    float Score = Value * FMath::Lerp(0.4f, 1.0f, Goal.Expectancy);

    // --- Обесценивание отдалённого ------------------------------------------
    // «Синица в руке»: далёкая награда весит меньше близкой, и тем меньше,
    // чем нетерпеливее человек. Именно здесь ломаются все хорошие намерения.
    const float Patience = Personality ? Personality->Patience() : 0.5f;
    float HorizonDiscount = 1.0f;
    switch (Goal.Horizon)
    {
    case EGoalHorizon::Immediate: HorizonDiscount = 1.0f; break;
    case EGoalHorizon::Short:     HorizonDiscount = FMath::Lerp(0.55f, 0.92f, Patience); break;
    case EGoalHorizon::Medium:    HorizonDiscount = FMath::Lerp(0.25f, 0.80f, Patience); break;
    case EGoalHorizon::Life:      HorizonDiscount = FMath::Lerp(0.10f, 0.65f, Patience); break;
    }
    Score *= HorizonDiscount;

    // --- Стресс сужает горизонт ---------------------------------------------
    // Под давлением человек перестаёт думать о будущем вовсе.
    if (Stress > 0.5f && Goal.Horizon != EGoalHorizon::Immediate)
    {
        Score *= FMath::Lerp(1.0f, 0.35f, (Stress - 0.5f) * 2.0f);
    }

    // --- Ценности личности подкрашивают выбор ------------------------------
    if (Personality)
    {
        switch (Goal.DrivingNeed)
        {
        case ENeedType::Achievement: Score *= (0.7f + Personality->Values.Achievement * 0.6f); break;
        case ENeedType::Esteem:      Score *= (0.7f + Personality->Values.Power * 0.6f); break;
        case ENeedType::Novelty:     Score *= (0.7f + Personality->Values.Stimulation * 0.6f); break;
        case ENeedType::Safety:      Score *= (0.7f + Personality->Values.Security * 0.6f); break;
        case ENeedType::Intimacy:
        case ENeedType::Belonging:   Score *= (0.7f + Personality->Values.Benevolence * 0.6f); break;
        case ENeedType::Meaning:     Score *= (0.7f + Personality->Values.Universalism * 0.6f); break;
        default: break;
        }
    }

    // --- Уже начатое доделывать легче --------------------------------------
    if (Goal.Progress > 0.1f)
    {
        Score *= (1.0f + Goal.Progress * 0.4f);
    }

    // --- ПРИВЕРЖЕННОСТЬ ----------------------------------------------------
    // Решённое перестаёт пересматриваться. Без этого человек бесконечно
    // мечется между «поесть» и «поговорить», не доходя ни до того, ни до
    // другого. Намерение должно быть липким — иначе это не намерение.
    // Но липкость снимается, когда телу плохо: разум выставляет множитель в 1.
    if (Goal.Id == ActiveGoalId && Goal.Status == EGoalStatus::Active)
    {
        Score *= FMath::Max(1.0f, CommitmentMultiplier);
    }

    // --- Многократные провалы отбивают охоту --------------------------------
    if (Goal.Attempts > 2)
    {
        Score *= FMath::Pow(0.8f, Goal.Attempts - 2);
    }

    return FMath::Max(0.0f, Score);
}

void UMotivationComponent::AchieveGoal(int32 Id, float WorldTime)
{
    FGoal* G = FindGoal(Id);
    if (!G)
    {
        return;
    }

    G->Status = EGoalStatus::Achieved;
    G->Progress = 1.0f;
    ConsecutiveFailures = 0;

    // Достижение возвращает волю: успех окрыляет буквально.
    RestoreWillpower(0.12f + G->Importance * 0.15f);

    if (Id == ActiveGoalId)
    {
        ActiveGoalId = -1;
    }
}

void UMotivationComponent::FailGoal(int32 Id, float WorldTime, const UPersonalityComponent* Personality)
{
    FGoal* G = FindGoal(Id);
    if (!G)
    {
        return;
    }

    G->Status = EGoalStatus::Failed;
    ConsecutiveFailures++;

    // Провал отнимает силы.
    Willpower = FMath::Max(0.0f, Willpower - 0.08f);

    if (Id == ActiveGoalId)
    {
        ActiveGoalId = -1;
    }
}

void UMotivationComponent::AbandonGoal(int32 Id, float WorldTime)
{
    FGoal* G = FindGoal(Id);
    if (!G)
    {
        return;
    }

    G->Status = EGoalStatus::Abandoned;
    if (Id == ActiveGoalId)
    {
        ActiveGoalId = -1;
    }
}

// ---------------------------------------------------------------------------
//  Воля
// ---------------------------------------------------------------------------

bool UMotivationComponent::SpendWillpower(float Amount)
{
    const float Cost = FMath::Max(0.0f, Amount);
    if (Willpower < Cost)
    {
        // Не хватило. Человек не «провалил проверку» — он просто сдался,
        // и почти наверняка объяснит это себе как-нибудь иначе.
        Willpower = FMath::Max(0.0f, Willpower - Cost * 0.5f);
        return false;
    }
    Willpower -= Cost;
    return true;
}

void UMotivationComponent::RestoreWillpower(float Amount)
{
    Willpower = FMath::Clamp(Willpower + FMath::Max(0.0f, Amount), 0.0f, WillpowerCapacity);
}

float UMotivationComponent::GetSelfControl() const
{
    return WillpowerCapacity > 0.0f ? FMath::Clamp(Willpower / WillpowerCapacity, 0.0f, 1.0f) : 0.0f;
}

// ---------------------------------------------------------------------------
//  Запросы
// ---------------------------------------------------------------------------

bool UMotivationComponent::HasNaggingGoal() const
{
    for (const FGoal& G : Goals)
    {
        // Важное, давно задуманное и не сдвинувшееся с места — то самое,
        // что вспоминается перед сном.
        if (G.Importance > 0.6f
            && G.Progress < 0.15f
            && (G.Status == EGoalStatus::Pending || G.Status == EGoalStatus::Suspended))
        {
            return true;
        }
    }
    return false;
}

FString UMotivationComponent::DescribeIntent() const
{
    // Самый живой из замыслов: тот, что важен и в который ещё верится.
    const FGoal* Best = nullptr;
    float BestScore = 0.0f;

    for (const FGoal& G : Goals)
    {
        if (G.Status == EGoalStatus::Achieved || G.Status == EGoalStatus::Failed || G.Status == EGoalStatus::Abandoned)
        {
            continue;
        }
        const float Score = G.Importance * (0.3f + G.Expectancy);
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = &G;
        }
    }

    return Best ? Best->Name : TEXT("ничего конкретного");
}
