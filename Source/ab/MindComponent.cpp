// MindComponent.cpp

#include "MindComponent.h"
#include "CompleteHumanAI.h"

#include "PersonalityComponent.h"
#include "PhysiologyComponent.h"
#include "NeedComponent.h"
#include "EmotionComponent.h"
#include "MemoryComponent.h"
#include "FurnitureActor.h"
#include "BookActor.h"
#include "ResourceActor.h"
#include "MatterStructure.h"
#include "EngineUtils.h"
#include "Textbook.h"
#include "Crafts.h"
#include "Village.h"
#include "Improvise.h"
#include "IdentityComponent.h"
#include "SocialComponent.h"
#include "MotivationComponent.h"
#include "SpeechComponent.h"
#include "DeliberationComponent.h"
#include "AffordanceComponent.h"

#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UMindComponent::UMindComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    PhaseOffset = FMath::FRandRange(0.0f, 0.4f);
}

// ---------------------------------------------------------------------------
//  Связывание
// ---------------------------------------------------------------------------

void UMindComponent::Awaken()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    Personality = Owner->FindComponentByClass<UPersonalityComponent>();
    Body        = Owner->FindComponentByClass<UPhysiologyComponent>();
    Needs       = Owner->FindComponentByClass<UNeedComponent>();
    Emotions    = Owner->FindComponentByClass<UEmotionComponent>();
    Memory      = Owner->FindComponentByClass<UMemoryComponent>();
   Identity    = Owner->FindComponentByClass<UIdentityComponent>();
    Social      = Owner->FindComponentByClass<USocialComponent>();
    Motivation  = Owner->FindComponentByClass<UMotivationComponent>();
    Speech      = Owner->FindComponentByClass<USpeechComponent>();
    Deliberation = Owner->FindComponentByClass<UDeliberationComponent>();

    const float T = Now();

    // Порядок важен: личность задаёт всё остальное.
    if (Personality) Personality->GenerateRandom();
    if (Identity)    Identity->Setup(Personality, T);
    if (Emotions)    Emotions->Setup(Personality);
    if (Needs)       Needs->Setup(Personality);
    if (Motivation)  Motivation->Setup(Personality);
   if (Speech)      Speech->SetupForAge(Identity ? Identity->Age : 30.0f);

    if (Body && Identity)
    {
        Body->BiologicalAge = Identity->Age;
    }
    if (Memory && Identity)
    {
        Memory->ApplyAging(Identity->Age);
    }

    // Мечта становится жизненной целью.
    if (Motivation && Identity)
    {
        Motivation->SetLifeGoal(Identity->LifeDream, Identity->DreamNeed, T);
    }

    // Первая мысль в жизни этого человека.
    if (Identity)
    {
        Think(FString::Printf(TEXT("Я %s. %s."), *Identity->FirstName, *Identity->GetStageConcern()),
              EThoughtKind::SelfTalk, 0.6f);
    }
    WakeCore();
    WakeTalk();
}

ACompleteHumanNPC* UMindComponent::GetHuman() const
{
    return Cast<ACompleteHumanNPC>(GetOwner());
}

UHumanWorldSubsystem* UMindComponent::GetWorldMind() const
{
    if (const UWorld* World = GetWorld())
    {
        return World->GetSubsystem<UHumanWorldSubsystem>();
    }
    return nullptr;
}

float UMindComponent::Now() const
{
    const UHumanWorldSubsystem* World = GetWorldMind();
    return World ? World->WorldSeconds : 0.0f;
}

float UMindComponent::EstimateTravelBudget(float Distance) const
{
    const UHumanWorldSubsystem* World = GetWorldMind();
    const ACompleteHumanNPC* Human = GetHuman();
    if (!World || !Human)
    {
        return 3600.0f;
    }

    // Реальная скорость ходьбы с поправкой на состояние тела.
    float Speed = Human->BaseWalkSpeed;
    if (Body)
    {
        Speed *= FMath::Max(0.2f, Body->GetMovementSpeedMultiplier());
    }
    Speed = FMath::Max(40.0f, Speed);

    // Сколько реальных секунд уйдёт на дорогу, с запасом на обходы и заторы.
    const float RealSeconds = Distance / Speed * 2.5f + 12.0f;

    return World->RealToGameSeconds(RealSeconds);
}

FString UMindComponent::NameOf(AActor* Who) const
{
    if (!Who)
    {
        return TEXT("кто-то");
    }

    // Имя знают только те, с кем знакомы. Остальные — «тот человек».
    if (Social)
    {
        if (const FRelationship* R = Social->Find(Who))
        {
            if (!R->KnownName.IsEmpty())
            {
                return R->KnownName;
            }
        }
    }

    // Имя — это то, что тебе назвали. До знакомства человек остаётся «кем-то».
    if (const ACompleteHumanNPC* Other = Cast<ACompleteHumanNPC>(Who))
    {
        if (const UIdentityComponent* TheirId = Other->FindComponentByClass<UIdentityComponent>())
        {
            return TheirId->bFemale ? TEXT("незнакомка") : TEXT("незнакомец");
        }
    }
    return TEXT("кто-то");
}

// ---------------------------------------------------------------------------
//  Главный шаг
// ---------------------------------------------------------------------------

void UMindComponent::Advance(float RealDelta)
{
    UHumanWorldSubsystem* World = GetWorldMind();
    if (!World || RealDelta <= 0.0f)
    {
        return;
    }

    const float GameDelta = World->RealToGameSeconds(RealDelta);

    // --- Смерть -------------------------------------------------------------
    if (Body && !Body->bAlive)
    {
        bAsleep = false;
        bActionActive = false;
        bMoving = false;
        CurrentAction = EActionType::Idle;
        CurrentActionLabel = TEXT("—");
        CurrentThought = FString::Printf(TEXT("† %s"), *Body->CauseOfDeath);

        // Повторные вызовы безопасны: у самого актора стоит защёлка.
        if (ACompleteHumanNPC* Human = GetHuman())
        {
            Human->OnDied();
        }
        DrawDebug();
        return;
    }

    // --- Тело и медленные подсистемы (≈5 раз в секунду) ---------------------
    BodyAccumulator += RealDelta;
    if (BodyAccumulator >= 0.2f)
    {
        const float SystemsDelta = World->RealToGameSeconds(BodyAccumulator);
        BodyAccumulator = 0.0f;
        Regulate(SystemsDelta);
    }

    // --- Сон ----------------------------------------------------------------
    if (bAsleep)
    {
        UpdateSleep(GameDelta);
        ThoughtAccumulator += RealDelta;
        if (ThoughtAccumulator > 4.0f)
        {
            ThoughtAccumulator = 0.0f;
            if (SleepPhase == ESleepPhase::REM)
            {
                Dream();
            }
        }
        DrawDebug();
        return;
    }

    // --- Когнитивный цикл (≈2 раза в секунду) -------------------------------
    CycleAccumulator += RealDelta;
    if (CycleAccumulator >= 0.5f + PhaseOffset)
    {
        const float CycleDelta = World->RealToGameSeconds(CycleAccumulator);
        CycleAccumulator = 0.0f;
        UpdateAttention(CycleDelta);
        AppraiseWorld(CycleDelta);
        HandleEncounters();
    }

    // --- Размышление и выбор цели (раз в полторы секунды) -------------------
    DeliberateAccumulator += RealDelta;
    if (DeliberateAccumulator >= 1.5f + PhaseOffset)
    {
        const float Delta = World->RealToGameSeconds(DeliberateAccumulator);
        DeliberateAccumulator = 0.0f;
        Deliberate(Delta);
    }

    // --- Действие: каждый кадр ---------------------------------------------
    Act(GameDelta);
    UpdateDialogue(RealDelta);

    // Насколько с человеком считаются, меняется медленно — пересчитываем редко.
    StandingAccumulator += RealDelta;
    if (StandingAccumulator > 10.0f)
    {
        StandingAccumulator = 0.0f;
        UpdateStanding();
    }

    // --- Поток сознания -----------------------------------------------------
    ThoughtAccumulator += RealDelta;
    const float ThoughtInterval = FMath::Lerp(2.0f, 6.0f, 1.0f - FMath::Clamp(Emotions ? Emotions->GetTurmoil() : 0.3f, 0.0f, 1.0f));
    if (ThoughtAccumulator >= ThoughtInterval)
    {
        ThoughtAccumulator = 0.0f;
        GenerateThought();
        Reflect(World->RealToGameSeconds(ThoughtInterval));
    }

    // --- Прочие таймеры -----------------------------------------------------
    SpeechCooldown = FMath::Max(0.0f, SpeechCooldown - RealDelta);
    RewardSignal = FMath::Max(0.0f, RewardSignal - RealDelta * 0.5f);
    SocialWarmth = FMath::Max(0.0f, SocialWarmth - RealDelta * 0.02f);

    GreetResetTimer += RealDelta;
    if (GreetResetTimer > 60.0f)
    {
        GreetResetTimer = 0.0f;
        GreetedRecently.Reset();
    }

    DrawDebug();
}

// ---------------------------------------------------------------------------
//  Подсистемы
// ---------------------------------------------------------------------------

void UMindComponent::Regulate(float GameDelta)
{
    UHumanWorldSubsystem* World = GetWorldMind();
    const float T = Now();
    const float Hour = World ? World->Now.HourFloat : 12.0f;
    CheckOrders(GameDelta);
    CheckFamily(GameDelta);
    if (Speech && Identity)
    {
        Speech->EarAge = Identity->Age;
    }

    // --- Тело ---------------------------------------------------------------
    if (Body)
    {
        FPhysiologyDrive Drive;
        Drive.HourOfDay = Hour;
        Drive.Age = Identity ? Identity->Age : 30.0f;
        Drive.bAsleep = bAsleep;
        Drive.SleepPhase = SleepPhase;
        Drive.SocialWarmth = SocialWarmth;
        Drive.RewardSignal = RewardSignal;
        ACompleteHumanNPC* Dressed = GetHuman();
        const float Weather = World ? World->Weather : 0.2f;
        const float Warm = Dressed ? Dressed->ClothingWarmth() : 0.0f;
        Drive.EnvironmentHarshness = FMath::Clamp(Weather * (1.0f - Warm * 0.8f), 0.0f, 1.0f);

        if (Dressed)
        {
            Dressed->AdvanceClothes(GameDelta, CurrentAction == EActionType::Work);
        }
        Drive.Resilience = Personality ? (1.0f - Personality->Traits.Neuroticism) : 0.5f;

        if (Emotions)
        {
            const FAffectPAD Affect = Emotions->GetAffect();
            Drive.Arousal = Affect.Arousal;
            Drive.Valence = Affect.Pleasure;
            Drive.StressLoad = Emotions->GetStress();
        }

        const float Effort = Dressed ? Dressed->GetGroundEffort() : 1.0f;
        const bool bWalking = Dressed && Dressed->GetVelocity().Size2D() > 20.0f;
        Drive.Exertion = (bMoving || bWalking) ? FMath::Min(1.0f, 0.35f * Effort) : (CurrentAction == EActionType::Exercise ? 0.8f : 0.05f);
        if (Dressed && Dressed->AreHandsBusy())
        {
            Drive.Exertion = FMath::Max(Drive.Exertion, 0.25f);
        }

        Body->Advance(GameDelta, Drive);
    }

    const float Impairment = Body ? Body->GetCognitiveImpairment() : 0.0f;
    const float Distress = Body ? Body->GetBodilyDistress() : 0.0f;

    // --- Сосредоточенность --------------------------------------------------
    const float FocusTarget = FMath::Clamp(
        (Personality ? 0.4f + Personality->Traits.Conscientiousness * 0.4f : 0.6f)
        - Impairment * 0.5f
        - (Emotions ? Emotions->GetStress() * 0.3f : 0.0f)
        + (Emotions ? Emotions->GetIntensity(EEmotionType::Curiosity) * 0.2f : 0.0f),
        0.05f, 1.0f);
    Focus = FMath::FInterpTo(Focus, FocusTarget, GameDelta / 60.0f, 1.0f);

    // --- Потребности --------------------------------------------------------
    if (Needs)
    {
        FNeedContext Ctx;
        Ctx.bAsleep = bAsleep;
        Ctx.PerceivedDanger = Emotions ? Emotions->GetIntensity(EEmotionType::Fear) : 0.0f;
        Ctx.RoutinePredictability = RoutineScore;
        Ctx.SurroundingBeauty = World ? (1.0f - World->Weather) * 0.5f : 0.3f;
        Ctx.HourOfDay = Hour;

        if (ACompleteHumanNPC* Human = GetHuman())
        {
            Ctx.bHasHome = Human->HasHome();
            Ctx.bAtHome = Human->IsAtHome();
        }
        if (Identity)
        {
            Ctx.Money = FVillage::IsMedieval(this) ? FVillage::WealthForNeeds(GetHuman(), Identity->Money) : Identity->Money;
            Ctx.bEmployed = Identity->bEmployed || FVillage::IsMedieval(this);
            if (FVillage::IsMedieval(this))
            {
                ProvisionClock -= GameDelta;
                if (ProvisionClock <= 0.0f)
                {
                    ProvisionClock = 60.0f;
                    ProvisionFelt = FVillage::Provision(GetHuman());
                }
                Ctx.Provision = ProvisionFelt;
            }
        }
        if (World)
        {
            // «Рядом люди» — это те, кого человек действительно замечает.
            // За стеной их для него нет, и одиночество от них не проходит.
            ACompleteHumanNPC* Self = GetHuman();
            const TArray<ACompleteHumanNPC*> Nearby = World->GetHumansNear(GetOwner()->GetActorLocation(), 900.0f, GetOwner());

            int32 Perceived = 0;
            float BestCloseness = 0.0f;
            for (ACompleteHumanNPC* Other : Nearby)
            {
                float Noticed = 0.0f;
                FName Sense;
                if (!Self || !Self->CanPerceive(Other, Noticed, Sense))
                {
                    continue;
                }
                ++Perceived;
                if (Social)
                {
                    BestCloseness = FMath::Max(BestCloseness, Social->GetCloseness(Other));
                }
            }
            Ctx.PeopleNearby = Perceived;
            Ctx.ClosenessNearby = BestCloseness;
        }

        Needs->Advance(GameDelta, Body, Ctx);
        InfantReflex(GameDelta);
        FeelIdleness(GameDelta);
    }

    // --- Чувства ------------------------------------------------------------
    if (Emotions)
    {
        Emotions->Advance(GameDelta, Distress, T);
    }

    // --- Память, навыки, отношения, мотивация -------------------------------
    if (Memory)     Memory->Advance(GameDelta, T, Impairment);
   if (Social)     Social->Advance(GameDelta, T);
    if (Motivation)
    {
        Motivation->Advance(GameDelta, T, bAsleep, Personality);

        // Замыслы складываются из того, чего давно не хватает, и выцветают,
        // если ничего не сдвигается. Это происходит медленно и само.
        AimAccumulator += GameDelta;
        if (AimAccumulator > 3600.0f)
        {
            AimAccumulator = 0.0f;
            Motivation->FormLongTermAims(Needs, Personality, T);
            Motivation->ReviewAims(T, Personality);

            // Раз в игровой час человек оглядывается на прожитое и,
            // если замечает в нём закономерность, делает вывод.
            DrawConclusions();
        }
    }

    // --- Самость ------------------------------------------------------------
    if (Identity)
    {
        const float LifeTone = Memory ? Memory->GetRecentLifeTone(T, 86400.0f) : 0.0f;
        const float Support = Social ? Social->GetSocialSupport() : 0.0f;
        const float Competence = this->Competence();
        const float Stress = Emotions ? Emotions->GetStress() : 0.0f;

        const int32 AgeBefore = FMath::FloorToInt(Identity->Age);
        Identity->Advance(GameDelta, T, LifeTone, Support, Competence, Stress);
        const int32 AgeAfter = FMath::FloorToInt(Identity->Age);

        // Тревога о том, что может случиться. Это прогноз, а не факт —
        // и оценка с неполной уверенностью рождает именно тревогу,
        // а не горе. Тот, кому всё равно, не встревожится вовсе.
        if (Identity->bEmployed && Emotions && Identity->LastWorkedAt > 0.0f)
        {
            const float AbsentDays = (T - Identity->LastWorkedAt) / 86400.0f;
            const float Risk = FMath::Clamp(AbsentDays / FMath::Max(0.5f, Identity->ToleratedAbsenceDays), 0.0f, 1.0f);

            if (Risk > 0.45f && FMath::FRand() < 0.05f)
            {
                FAppraisedEvent Looming;
                Looming.Tag = TEXT("JobAtRisk");
                Looming.Description = TEXT("меня могут уволить");
                Looming.Desirability = -0.6f;
                Looming.Certainty = Risk * 0.7f;      // ещё не случилось
                Looming.Controllability = 0.8f;       // но это в моих руках
                Looming.SelfAgency = 0.9f;
                Looming.Unexpectedness = 0.2f;
                Looming.Significance = Risk;
                Emotions->Appraise(Looming, Personality);
            }
        }

        // Увольнение — событие, а не запись в анкете. Что именно человек
        // почувствует, решает оценка: для одного это катастрофа, для
        // другого — облегчение, если работа его тяготила.
        if (Identity->bJustLostJob && Emotions)
        {
            const float MoneyNeed = Needs ? Needs->GetUrgency(ENeedType::Money) : 0.5f;

            FAppraisedEvent Fired;
            Fired.Tag = TEXT("Loss");
            Fired.Description = TEXT("меня уволили");
            Fired.Desirability = -FMath::Clamp(0.3f + MoneyNeed * 0.7f, 0.0f, 1.0f);
            Fired.Unexpectedness = 0.6f;
            Fired.Controllability = 0.2f;
            Fired.SelfAgency = 0.75f;     // сам ведь не ходил
            Fired.OtherAgency = 0.25f;
            Fired.Certainty = 1.0f;
            Fired.Significance = 0.85f;
            Emotions->Appraise(Fired, Personality);

            Think(TEXT("меня уволили. Доигрался."), EThoughtKind::Rumination, 0.95f);

            if (Memory)
            {
                Memory->Encode(TEXT("меня уволили"), TEXT("Loss"), -0.8f, 0.8f, {},
                               GetOwner()->GetActorLocation(), T, 1.0f, EMemoryKind::Flashbulb);
            }
        }

        // Новый год жизни — характер чуть взрослеет, память чуть слабеет.
        if (AgeAfter > AgeBefore)
        {
            if (Personality) Personality->ApplyMaturation(Identity->Age);
            if (Memory)      Memory->ApplyAging(Identity->Age);
            if (Emotions)    Emotions->Setup(Personality);
            Think(FString::Printf(TEXT("Мне уже %d."), AgeAfter), EThoughtKind::Existential, 0.7f);
        }

        // Заработок за отработанное время.
        if (CurrentAction == EActionType::Work && Identity->bEmployed)
        {
            Identity->Money += Identity->HourlyWage * (GameDelta / 3600.0f);
        }
    }

    // --- Хронический стресс запускает совладание ---------------------------
    // Справляться с собой — поступок, а не фоновый процесс. Если запускать
    // это каждый кадр, человек за минуту загонит себя в стыд и руминацию.
    CopingCooldown = FMath::Max(0.0f, CopingCooldown - GameDelta);

    HitRegret = FMath::Max(0.0f, HitRegret - GameDelta / (86400.0f * 30.0f));
    if (Emotions && Personality && CopingCooldown <= 0.0f)
    {
        const float Stress = Emotions->GetStress() + Emotions->GetTurmoil() * 0.5f;
        LearnCoping(Stress);
        if (Stress > 0.35f)
        {
            ECopingStyle Style = static_cast<ECopingStyle>(ChooseCoping(Stress));

            // Сорваться можно только на кого-то. Если рядом никого, злость
            // уходит внутрь — и это совсем другая история.
            if (Style == ECopingStyle::Aggression)
            {
                const bool bSomeoneNear = AttentionTarget != nullptr
                    && Emotions->GetIntensity(EEmotionType::Anger) > 0.35f;
                if (!bSomeoneNear)
                {
                    Style = ECopingStyle::Suppression;
                }
            }
            // Искать поддержки не у кого — остаётся пережёвывать.
            if (Style == ECopingStyle::SeekSupport && Social && Social->GetSocialSupport() < 0.15f)
            {
                Style = ECopingStyle::Rumination;
            }

            const float Effort = FMath::Clamp(
                (Motivation ? Motivation->GetSelfControl() : 0.5f) * 0.5f, 0.1f, 0.5f);

            // Зрелые стратегии стоят воли; примитивные включаются сами.
            const bool bCostly = (Style == ECopingStyle::Reappraisal
                               || Style == ECopingStyle::ProblemFocused
                               || Style == ECopingStyle::Suppression);

            if (!bCostly || (Motivation && Motivation->SpendWillpower(0.03f)))
            {
                LastCoping = static_cast<int32>(Style);
                StressBeforeCoping = Stress;
                Emotions->Regulate(Style, Effort, Personality);

                // Человек замечает, как именно он справляется. Иногда.
                if (FMath::FRand() < 0.25f)
                {
                    switch (Style)
                    {
                    case ECopingStyle::Reappraisal:
                        Think(TEXT("ладно, это не конец света"), EThoughtKind::SelfTalk, 0.5f); break;
                    case ECopingStyle::Suppression:
                        Think(TEXT("не время раскисать"), EThoughtKind::SelfTalk, 0.5f); break;
                    case ECopingStyle::Avoidance:
                        Think(TEXT("не хочу об этом думать"), EThoughtKind::SelfTalk, 0.5f); break;
                    case ECopingStyle::SeekSupport:
                        Think(TEXT("поговорить бы с кем-нибудь"), EThoughtKind::Need, 0.6f); break;
                    case ECopingStyle::Rumination:
                        Think(TEXT("и ведь не выходит из головы"), EThoughtKind::Rumination, 0.7f); break;
                    default: break;
                    }
                }
            }
        }

        // Следующая попытка справиться — не раньше, чем через полчаса
        // игрового времени. Собранные берут себя в руки чаще.
        const float Base = 1800.0f;
        CopingCooldown = Base * FMath::Lerp(1.5f, 0.7f, Personality->Facets.SelfControl);
    }

    // --- Проверка на сон ----------------------------------------------------
    if (!bAsleep && Body)
    {
        const float Pressure = Body->GetSleepPressure(Hour);
        // Свалиться от усталости можно где угодно — но обычно человек
        // всё-таки доходит до кровати.
        if (Pressure > 0.97f || (Pressure > 0.75f && CurrentAction == EActionType::Sleep))
        {
            BeginSleep();
        }
    }
}

// ---------------------------------------------------------------------------
//  Внимание
// ---------------------------------------------------------------------------

void UMindComponent::UpdateAttention(float GameDelta)
{
    const float T = Now();

    // Старое выпадает из рабочей памяти — она короткая.
    for (int32 i = WorkingMemory.Num() - 1; i >= 0; --i)
    {
        if (T - WorkingMemory[i].Time > 120.0f || !WorkingMemory[i].Actor)
        {
            WorkingMemory.RemoveAt(i);
        }
    }

    // Рабочая память человека — 7±2 элемента. Усталость её ужимает.
    const int32 Capacity = FMath::Clamp(FMath::RoundToInt(4.0f + Focus * 4.0f), 3, 8);
    while (WorkingMemory.Num() > Capacity)
    {
        int32 WeakestIndex = 0;
        for (int32 i = 1; i < WorkingMemory.Num(); ++i)
        {
            if (WorkingMemory[i].Salience < WorkingMemory[WeakestIndex].Salience)
            {
                WeakestIndex = i;
            }
        }
        WorkingMemory.RemoveAt(WeakestIndex);
    }

    // Внимание достаётся самому громкому.
    float BestSalience = 0.0f;
    AttentionTarget = nullptr;
    for (const FPercept& P : WorkingMemory)
    {
        if (P.Salience > BestSalience)
        {
            BestSalience = P.Salience;
            AttentionTarget = P.Actor;
        }
    }
}

void UMindComponent::Notice(AActor* What, FName SenseKind, const FVector& Where, float RawSalience)
{
    if (!What || What == GetOwner())
    {
        return;
    }

    const float T = Now();

    // --- Фильтр внимания: до сознания доходит не всё ------------------------
    float Salience = FMath::Clamp(RawSalience, 0.0f, 1.0f);

    // Знакомые лица заметнее незнакомых, а те, кого боишься, — заметнее всех.
    if (Social)
    {
        if (const FRelationship* R = Social->Find(What))
        {
            Salience += R->Familiarity * 0.2f;
            Salience += FMath::Abs(R->Liking) * 0.25f;
            Salience += R->Fear * 0.45f;
            Salience += R->Resentment * 0.3f;
        }
        else
        {
            // Незнакомец сам по себе слегка настораживает — тем сильнее,
            // чем тревожнее человек.
            Salience += Personality ? Personality->Facets.TraitAnxiety * 0.15f : 0.05f;
        }
    }

    // Рассеянный человек пропускает слабые сигналы. Это и есть невнимательность.
    const float Threshold = FMath::Lerp(0.35f, 0.08f, Focus);
    if (Salience < Threshold)
    {
        return;
    }

    // Уже в рабочей памяти — просто обновим.
    for (FPercept& P : WorkingMemory)
    {
        if (P.Actor == What)
        {
            P.Salience = FMath::Max(P.Salience, Salience);
            P.Time = T;
            P.Location = Where;
            return;
        }
    }

    FPercept New;
    New.Actor = What;
    New.Kind = SenseKind;
    New.Location = Where;
    New.Salience = Salience;
    New.Time = T;
    New.Description = NameOf(What);
    WorkingMemory.Add(New);

    // Первая в жизни встреча с этим человеком.
    if (Social && Cast<ACompleteHumanNPC>(What))
    {
        const bool bNew = (Social->Find(What) == nullptr);
        FRelationship& R = Social->FindOrAdd(What, T);
        if (bNew)
        {
            Think(FString::Printf(TEXT("Незнакомый человек. Кто это?")), EThoughtKind::Observation, 0.4f, What);
        }
    }
}

// ---------------------------------------------------------------------------
//  Оценка мира
// ---------------------------------------------------------------------------

void UMindComponent::AppraiseWorld(float GameDelta)
{
    if (!Emotions)
    {
        return;
    }

    const float T = Now();
    UHumanWorldSubsystem* World = GetWorldMind();

    // --- Оценка людей в поле внимания --------------------------------------
    for (const FPercept& P : WorkingMemory)
    {
        ACompleteHumanNPC* Other = Cast<ACompleteHumanNPC>(P.Actor.Get());
        if (!Other || !Social)
        {
            continue;
        }

        FRelationship& R = Social->FindOrAdd(Other, T);

        // Читаем чужое состояние — с ошибкой, разумеется.
        UEmotionComponent* TheirEmotions = Other->FindComponentByClass<UEmotionComponent>();
        UNeedComponent* TheirNeeds = Other->FindComponentByClass<UNeedComponent>();

        if (TheirEmotions)
        {
            const float TheirMood = TheirEmotions->GetAffect().Pleasure;
            const ENeedType TheirNeed = TheirNeeds ? TheirNeeds->GetMostUrgent() : ENeedType::SocialContact;
            const float MyEmpathy = Mastery(TEXT("Empathy"));

            Social->ObserveAndInfer(Other, TheirMood, TheirNeed, TheirEmotions->Expressiveness, MyEmpathy, T);

            // Эмоциональное заражение: чужое состояние протекает внутрь.
            const float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Other->GetActorLocation());
            if (Distance < 600.0f)
            {
                Emotions->CatchFrom(TheirEmotions, Personality ? Personality->Facets.Empathy : 0.5f, Social->GetCloseness(Other));
            }

            // Чужая беда — повод для сострадания и, может быть, помощи.
            if (R.Theory.BelievedMood < -0.45f && Distance < 1200.0f && FMath::FRand() < 0.15f)
            {
                FAppraisedEvent Ev;
                Ev.Tag = TEXT("OtherSuffers");
                Ev.Description = FString::Printf(TEXT("%s выглядит подавленно"), *NameOf(Other));
                Ev.Subject = Other;
                Ev.Significance = 0.4f + Social->GetCloseness(Other) * 0.5f;
                Ev.Desirability = -0.2f * Social->GetCloseness(Other);
                Ev.Certainty = R.Theory.ModelAccuracy;
                Emotions->Appraise(Ev, Personality);
            }
        }

        // Вид человека, на которого зол, поднимает обиду заново.
        if (R.Resentment > 0.4f && FMath::FRand() < 0.1f)
        {
            Emotions->Trigger(EEmotionType::Resentment, R.Resentment * 0.3f, Other,
                              FString::Printf(TEXT("опять %s"), *NameOf(Other)));
        }

        // Вид близкого человека греет.
        const float Closeness = Social->GetCloseness(Other);
        if (Closeness > 0.5f && FMath::FRand() < 0.12f)
        {
            Emotions->Trigger(EEmotionType::Affection, Closeness * 0.25f, Other,
                              FString::Printf(TEXT("%s рядом"), *NameOf(Other)));
            SocialWarmth = FMath::Clamp(SocialWarmth + 0.05f, 0.0f, 1.0f);
        }

        // Любовь не «включается» — она вырастает из привязанности, близости
        // и ощущения взаимности. И только тогда о ней становится можно думать.
        if (Closeness > 0.6f && R.Romantic > 0.45f && R.Theory.BelievedLikingOfMe > 0.1f)
        {
            if (FMath::FRand() < 0.1f)
            {
                Emotions->Trigger(EEmotionType::Love, R.Romantic * 0.55f, Other, *NameOf(Other));
                if (Needs)
                {
                    Needs->Satisfy(ENeedType::Intimacy, 0.15f);
                }
            }
        }
        // А если кажется, что чувство без ответа, — это уже другое чувство.
        else if (R.Romantic > 0.5f && R.Theory.BelievedLikingOfMe < -0.05f && FMath::FRand() < 0.06f)
        {
            Emotions->Trigger(EEmotionType::Sadness, R.Romantic * 0.3f, Other,
                              FString::Printf(TEXT("я для %s пустое место"), *NameOf(Other)));
        }
    }

    // --- Одиночество --------------------------------------------------------
    if (Needs && World)
    {
        const float SocialSat = Needs->GetSatisfaction(ENeedType::SocialContact);
        const float Belonging = Needs->GetSatisfaction(ENeedType::Belonging);
        if (SocialSat < 0.25f && Belonging < 0.4f && FMath::FRand() < 0.1f)
        {
            const float Support = Social ? Social->GetSocialSupport() : 0.0f;
            Emotions->Trigger(EEmotionType::Loneliness, (1.0f - SocialSat) * (1.0f - Support) * 0.4f,
                              nullptr, TEXT("вокруг никого"));
        }
    }

    // --- Ночь, погода, обстановка -------------------------------------------
    if (World)
    {
        if (World->Now.bIsNight && Personality && Personality->Facets.TraitAnxiety > 0.55f && FMath::FRand() < 0.08f)
        {
            Emotions->Trigger(EEmotionType::Anxiety, Personality->Facets.TraitAnxiety * 0.2f, nullptr, TEXT("темно и тихо"));
        }
        if (World->Weather > 0.7f && FMath::FRand() < 0.05f)
        {
            Emotions->Trigger(EEmotionType::Sadness, 0.1f, nullptr, TEXT("мерзкая погода"));
        }
        else if (World->Weather < 0.15f && FMath::FRand() < 0.05f)
        {
            Emotions->Trigger(EEmotionType::Serenity, 0.15f, nullptr, TEXT("хороший день"));
        }
    }

    // --- Красота, если есть глаза её видеть ---------------------------------
    if (Personality && Personality->Traits.Openness > 0.65f && FMath::FRand() < 0.03f)
    {
        Emotions->Trigger(EEmotionType::Awe, 0.2f, nullptr, TEXT("вдруг увидел(а), как всё устроено"));
        if (Needs) Needs->Satisfy(ENeedType::Beauty, 0.15f);
    }
}

void UMindComponent::OnWorldEvent(const FWorldEvent& Event)
{
    if (!Emotions || Event.Instigator == GetOwner())
    {
        return;
    }

    const float T = Now();

    // Своё восприятие события: каждый видит его по-своему.
    FAppraisedEvent Ev;
    Ev.Tag = Event.Tag;
    Ev.Description = Event.Description;
    Ev.Subject = Event.Instigator;
    Ev.Location = Event.Location;
    Ev.Unexpectedness = 0.5f;
    Ev.Certainty = 1.0f;
    Ev.NormAlignment = Event.NormViolation;
    Ev.Significance = Event.Significance;

    // Насколько это касается лично меня — зависит от того, кто участвовал.
    float Personal = 0.15f;
    if (Social)
    {
        if (Event.Target == GetOwner())
        {
            Personal = 1.0f;
        }
        else if (Event.Target)
        {
            Personal = FMath::Max(Personal, Social->GetCloseness(Event.Target));
        }
        if (Event.Instigator)
        {
            Ev.OtherAgency = 0.8f;
            Personal = FMath::Max(Personal, Social->GetCloseness(Event.Instigator) * 0.6f);
        }
    }

    Ev.Desirability = Event.Valence * Personal;
    Ev.Significance *= FMath::Clamp(0.3f + Personal, 0.0f, 1.0f);
    Ev.Controllability = 0.2f;

    const EEmotionType Felt = Emotions->Appraise(Ev, Personality);

    // Запоминаем — если хватило внимания.
    if (Memory && Ev.Significance > 0.2f)
    {
        TArray<AActor*> Participants;
        if (Event.Instigator) Participants.Add(Event.Instigator);
        if (Event.Target)     Participants.Add(Event.Target);

        Memory->Encode(Event.Description, Event.Tag, Ev.Desirability,
                       Ev.Significance, Participants, Event.Location, T, Focus);
    }

    if (Felt != EEmotionType::None && Ev.Significance > 0.35f)
    {
        Think(FString::Printf(TEXT("%s — %s"), *Event.Description, *HumanText::EmotionFirstPerson(Felt)),
              EThoughtKind::Observation, Ev.Significance, Event.Instigator);
    }
}

// ---------------------------------------------------------------------------
//  Решение
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  ЧТО ВООБЩЕ МОЖНО СДЕЛАТЬ
//
//  Человек не перебирает правила поведения. Он осматривается и видит
//  возможности — свои, чужие, вещей и мест. Ни одна из них не связана
//  с потребностью жёстко: связь возникает только в момент оценки.
// ---------------------------------------------------------------------------

void UMindComponent::GatherAffordances(TArray<FAffordance>& Out) const
{
    Out.Reset();

    ACompleteHumanNPC* Human = GetHuman();
    UHumanWorldSubsystem* World = GetWorldMind();
    if (!Human || !World)
    {
        return;
    }

    // То, что видно вокруг прямо сейчас.
    World->GatherAffordancesNear(Human->GetActorLocation(), 2600.0f, Out);

    // То, что человек помнит и куда может дойти.
    GatherRememberedAffordances(Out);

    // То, что предлагают люди.
    GatherSocialAffordances(Out);
    GatherVillageAffordances(Out);

    // То, что человек может сделать в любой момент сам с собой.
    GatherSelfAffordances(Out);

    // То, что посоветовали или велели. Исполнять не обязательно — но хочется
    // не подвести того, кого уважаешь, и в этом есть своя награда.
    const float Clock = Now();
    for (const FSuggestion& S : Suggestions)
    {
        if (S.ExpiresAt < Clock || !S.Affordance.bHasLocation)
        {
            continue;
        }
        FAffordance A = S.Affordance;

        FNeedPromise Approval;
        Approval.Need = ENeedType::Belonging;
        Approval.Amount = S.Weight * (S.bInstruction ? 0.35f : 0.2f);
        A.Promises.Add(Approval);

        FNeedPromise Clarity;
        Clarity.Need = ENeedType::Order;
        Clarity.Amount = S.Weight * 0.2f;
        A.Promises.Add(Clarity);

        if (S.bInstruction && Identity && Identity->IsServant() && S.From == OrderFrom.Get())
        {
            FNeedPromise Duty;
            Duty.Need = ENeedType::Safety;
            Duty.Amount = 0.3f;
            A.Promises.Add(Duty);
            FNeedPromise Settled;
            Settled.Need = ENeedType::Order;
            Settled.Amount = 0.3f;
            A.Promises.Add(Settled);
            FNeedPromise Proud;
            Proud.Need = ENeedType::Esteem;
            Proud.Amount = 0.1f;
            A.Promises.Add(Proud);
        }

        A.Label = FString::Printf(TEXT("%s (%s)"), *A.Label,
            S.bInstruction ? TEXT("велели") : TEXT("посоветовали"));
        Out.Add(A);
    }

    // --- Чужое — не твоё ----------------------------------------------------
    // Человек не заходит в незнакомый дом, чтобы поесть из чужого
    // холодильника, даже если очень голоден. Такой возможности для него
    // просто не существует — он её не рассматривает и не отвергает.
    {
        const bool bHasOwnHome = Human->HasHome();
        const FVector MyHome = Human->GetHomeLocation();

        Out.RemoveAll([bHasOwnHome, &MyHome](const FAffordance& A)
        {
            if (!A.bPrivate)
            {
                return false;
            }
            if (!bHasOwnHome)
            {
                return true;   // бездомному чужое тем более не принадлежит
            }
            return FVector::Dist2D(A.OwnerAnchor, MyHome) > 400.0f;
        });
    }
    FilterDeals(Out);

    // Увиденное и вспомненное могут совпасть — оставляем то, что ближе.
    // Иначе одно и то же кафе считалось бы дважды.
    for (int32 i = Out.Num() - 1; i >= 0; --i)
    {
        for (int32 j = 0; j < i; ++j)
        {
            if (Out[i].Key != Out[j].Key)
            {
                continue;
            }

            const FVector Here = Human->GetActorLocation();
            const float DistI = Out[i].bHasLocation ? FVector::Dist2D(Here, Out[i].Location) : 0.0f;
            const float DistJ = Out[j].bHasLocation ? FVector::Dist2D(Here, Out[j].Location) : 0.0f;

            // При равном расстоянии берём ту, где обещано больше: совет
            // к знакомому месту добавляет своё, и терять это нельзя.
            if (DistI < DistJ - 1.0f
                || (FMath::Abs(DistI - DistJ) <= 1.0f && Out[i].Promises.Num() > Out[j].Promises.Num()))
            {
                Out[j] = Out[i];
            }
            Out.RemoveAt(i);
            break;
        }
    }

    // Работа платит ровно столько, сколько платят именно ему.
    // Без работы она не платит ничего — но всё ещё даёт умение и занятость,
    // и кто-то может пойти туда даже так.
    if (Identity)
    {
        const float Wage = Identity->bEmployed ? Identity->HourlyWage : 0.0f;

        // Насколько близко человек подошёл к тому, чтобы работы лишиться.
        const float AbsentDays = (Now() - Identity->LastWorkedAt) / 86400.0f;
        const float JobRisk = Identity->bEmployed
            ? FMath::Clamp(AbsentDays / FMath::Max(0.5f, Identity->ToleratedAbsenceDays), 0.0f, 1.0f)
            : 0.0f;

        const bool bVillage = FVillage::IsMedieval(this);
        for (FAffordance& A : Out)
        {
            if (A.Action != EActionType::Work || !A.Craft.IsNone())
            {
                continue;
            }
            if (bVillage && A.Deal != TEXT("serve"))
            {
                continue;
            }

            A.MoneyGainPerHour = Wage;

            if (!Identity->bEmployed)
            {
                // Чужая работа — это не твоя работа: денег она не принесёт.
                for (FNeedPromise& Promise : A.Promises)
                {
                    if (Promise.Need == ENeedType::Money)
                    {
                        Promise.Amount = 0.0f;
                    }
                }
                continue;
            }

            // А вот и «боится потерять работу» — не правилом, а правдой:
            // поход на работу ДЕЙСТВИТЕЛЬНО сохраняет источник дохода, и чем
            // дольше прогул, тем это ценнее. Насколько это подействует,
            // решит вес его собственной потребности в безопасности: один
            // побежит на работу от одной мысли, другой махнёт рукой.
            if (JobRisk > 0.15f)
            {
                FNeedPromise Security;
                Security.Need = ENeedType::Safety;
                Security.Amount = JobRisk * 0.7f;
                A.Promises.Add(Security);

                FNeedPromise Livelihood;
                Livelihood.Need = ENeedType::Money;
                Livelihood.Amount = JobRisk * 0.5f;
                A.Promises.Add(Livelihood);

                A.Label = FString::Printf(TEXT("на работу (%s)"),
                    JobRisk > 0.7f ? TEXT("иначе уволят") : TEXT("давно не был"));
            }
        }
    }
}

void UMindComponent::GatherRememberedAffordances(TArray<FAffordance>& Out) const
{
    if (!Memory)
    {
        return;
    }

    const ACompleteHumanNPC* Human = GetHuman();
    const FVector Here = Human ? Human->GetActorLocation() : FVector::ZeroVector;
    UHumanWorldSubsystem* World = GetWorldMind();

    for (const FKnownLocation& Place : Memory->Places)
    {
        // Совсем смутно припоминаемое место в расчёт не идёт.
        if (Place.Familiarity < 0.08f)
        {
            continue;
        }

        for (const FAffordance& Offer : Place.Offers)
        {
            // Человек знает, когда что открыто. Идти ночью в контору
            // ему не приходит в голову — не потому, что запрещено,
            // а потому что такой возможности сейчас нет.
            if (World && !World->IsAffordanceOpenNow(Offer))
            {
                continue;
            }

            FAffordance Remembered = Offer;
            Remembered.Location = Place.Location;
            Remembered.bHasLocation = true;
            Remembered.Source = EAffordanceSource::Place;
            Remembered.Target = nullptr;

            // Память о месте неточна: чем хуже помню, тем менее уверен,
            // что там вообще есть то, за чем иду.
            for (FNeedPromise& Promise : Remembered.Promises)
            {
                Promise.Amount *= FMath::Lerp(0.55f, 1.0f, Place.Familiarity);
            }

            Out.Add(Remembered);
        }
    }
}

void UMindComponent::GatherSocialAffordances(TArray<FAffordance>& Out) const
{
    UHumanWorldSubsystem* World = GetWorldMind();
    ACompleteHumanNPC* Human = GetHuman();
    if (!World || !Human || !Social)
    {
        return;
    }

    const TArray<ACompleteHumanNPC*> Nearby = World->GetHumansNear(Human->GetActorLocation(), Human->SightRadius, Human);

    for (ACompleteHumanNPC* Other : Nearby)
    {
        if (!Other || !Other->IsAlive())
        {
            continue;
        }

        // С кем нельзя заговорить, того и в мыслях нет: человек не строит
        // планов насчёт того, кого не видит и не слышит.
        float Noticed = 0.0f;
        FName Sense;
        if (!Human->CanPerceive(Other, Noticed, Sense))
        {
            continue;
        }

        if (UMindComponent* TheirMind = Other->FindComponentByClass<UMindComponent>())
        {
            if (TheirMind->bAsleep)
            {
                continue;
            }
        }

        const FRelationship* R = Social->Find(Other);
        const FString Name = NameOf(Other);
        const float Closeness = Social->GetCloseness(Other);

        // Вид человека — это то, кем он для меня приходится. Опыт общения
        // с друзьями переносится на нового друга, опыт с чужими — на чужих.
        const FString Kindred = HumanText::Relation(R ? R->Kind : ERelationKind::Stranger);

        // --- Поговорить ---
        // Сколько это даст — зависит от того, кто перед нами. Близкий даёт
        // близость, незнакомый — новизну. Это свойство отношений, а не правило.
        {
            FAffordance Talk;
            Talk.Action = EActionType::Talk;
            Talk.Source = EAffordanceSource::Person;
            Talk.Target = Other;
            Talk.Location = Other->GetActorLocation();
            Talk.bHasLocation = true;
            Talk.Duration = 900.0f;
            Talk.EffortCost = 0.08f;
            Talk.Label = FString::Printf(TEXT("поговорить — %s"), *Name);
            Talk.Key = FName(*FString::Printf(TEXT("поговорить@%s"), *Name));
            Talk.CategoryKey = FName(*FString::Printf(TEXT("поговорить@%s"), *Kindred));
            Talk.RequiredSkill = TEXT("Conversation");
            Talk.Difficulty = 0.15f;

            FNeedPromise Contact;
            Contact.Need = ENeedType::SocialContact;
            Contact.Amount = 0.45f;
            Talk.Promises.Add(Contact);

            if (Closeness > 0.3f)
            {
                FNeedPromise Belong;
                Belong.Need = ENeedType::Belonging;
                Belong.Amount = Closeness * 0.5f;
                Talk.Promises.Add(Belong);

                FNeedPromise Close;
                Close.Need = ENeedType::Intimacy;
                Close.Amount = Closeness * 0.45f;
                Talk.Promises.Add(Close);
            }
            else
            {
                FNeedPromise New;
                New.Need = ENeedType::Novelty;
                New.Amount = 0.2f;
                Talk.Promises.Add(New);
            }

            // Рядом с тем, кого боишься, разговор сам по себе рискован.
            if (R)
            {
                Talk.Risk = R->Fear * 0.6f;
            }

            Out.Add(Talk);
        }

        if (Speech && Speech->Literacy() > 0.8f
            && Other->SpeechComponent && Other->SpeechComponent->Literacy() < 0.5f)
        {
            FAffordance Teach;
            Teach.Action = EActionType::Help;
            Teach.Source = EAffordanceSource::Person;
            Teach.Target = Other;
            Teach.Location = Other->GetActorLocation();
            Teach.bHasLocation = true;
            Teach.Duration = 1800.0f;
            Teach.EffortCost = 0.2f;
            Teach.Label = FString::Printf(TEXT("научить грамоте — %s"), *Name);
            Teach.Key = FName(*FString::Printf(TEXT("научить@%s"), *Name));
            Teach.CategoryKey = TEXT("научить@человек");
            Teach.RequiredSkill = TEXT("Teaching");
            Teach.Difficulty = 0.15f;

            FNeedPromise TeachMeaning;
            TeachMeaning.Need = ENeedType::Meaning;
            TeachMeaning.Amount = 0.4f;
            Teach.Promises.Add(TeachMeaning);

            FNeedPromise TeachEsteem;
            TeachEsteem.Need = ENeedType::Esteem;
            TeachEsteem.Amount = 0.2f;
            Teach.Promises.Add(TeachEsteem);

            FNeedPromise TeachBelong;
            TeachBelong.Need = ENeedType::Belonging;
            TeachBelong.Amount = 0.2f;
            Teach.Promises.Add(TeachBelong);

            FNeedPromise TeachAchieve;
            TeachAchieve.Need = ENeedType::Achievement;
            TeachAchieve.Amount = 0.15f;
            Teach.Promises.Add(TeachAchieve);

            Out.Add(Teach);
        }

        // --- Угостить ---
        // Держу еду в руках, а рядом голодный человек. Предложение живёт
        // только пока в руках что-то съедобное и голод рядом виден.
        if (AResourceActor* Held = Cast<AResourceActor>(Human->CarriedItem.Get()))
        {
            UMindComponent* GuestMind = Other->FindComponentByClass<UMindComponent>();
            const FSubstance& Substance = FMatter::Of(Held->Kind);
            if (Substance.Food > 0.0f && FVillage::IsFood(Held->Kind))
            {
                const float TheirHunger = GuestMind && GuestMind->Needs
                    ? 1.0f - GuestMind->Needs->GetSatisfaction(ENeedType::Hunger) : 0.0f;
                if (TheirHunger > 0.35f)
                {
                    FAffordance Feed;
                    Feed.Action = EActionType::Help;
                    Feed.Source = EAffordanceSource::Person;
                    Feed.Target = Other;
                    Feed.Location = Other->GetActorLocation();
                    Feed.bHasLocation = true;
                    Feed.Duration = 600.0f;
                    Feed.EffortCost = 0.05f;
                    Feed.Deal = TEXT("feed");
                    Feed.DealKind = Held->Kind;
                    Feed.Label = FString::Printf(TEXT("угостить %s — %s"),
                        *Name, *AResourceActor::KindName(Held->Kind));
                    Feed.Key = FName(*FString::Printf(TEXT("угощение@%s@%s"),
                        *Name, *AResourceActor::KindName(Held->Kind)));
                    Feed.CategoryKey = TEXT("угощение");

                    // Дать еду — это близость: тем сильнее, чем роднее.
                    FNeedPromise Bond;
                    Bond.Need = ENeedType::Belonging;
                    Bond.Amount = 0.15f + Closeness * 0.25f;
                    Feed.Promises.Add(Bond);

                    FNeedPromise Warm;
                    Warm.Need = ENeedType::Meaning;
                    Warm.Amount = 0.12f + TheirHunger * 0.12f;
                    Feed.Promises.Add(Warm);

                    FNeedPromise Proud;
                    Proud.Need = ENeedType::Esteem;
                    Proud.Amount = 0.1f;
                    Feed.Promises.Add(Proud);

                    Out.Add(Feed);
                }
            }
        }

        // --- Помочь ---
        // Возможность помочь появляется не «по условию», а потому что я вижу,
        // что человеку плохо. Насколько это меня трогает — решит оценка.
        if (R && R->Theory.BelievedMood < -0.25f)
        {
            FAffordance Help;
            Help.Action = EActionType::Comfort;
            Help.Source = EAffordanceSource::Person;
            Help.Target = Other;
            Help.Location = Other->GetActorLocation();
            Help.bHasLocation = true;
            Help.Duration = 1200.0f;
            Help.EffortCost = 0.2f;
            Help.Label = FString::Printf(TEXT("побыть рядом — %s"), *Name);
            Help.Key = FName(*FString::Printf(TEXT("побыть рядом@%s"), *Name));
            Help.CategoryKey = FName(*FString::Printf(TEXT("побыть рядом@%s"), *Kindred));

            const float Empathy = Personality ? Personality->Facets.Empathy : 0.5f;

            FNeedPromise Meaning;
            Meaning.Need = ENeedType::Meaning;
            Meaning.Amount = 0.25f * (0.4f + Empathy);
            Help.Promises.Add(Meaning);

            FNeedPromise Esteem;
            Esteem.Need = ENeedType::Esteem;
            Esteem.Amount = 0.18f;
            Help.Promises.Add(Esteem);

            FNeedPromise Belong;
            Belong.Need = ENeedType::Belonging;
            Belong.Amount = 0.2f + Closeness * 0.3f;
            Help.Promises.Add(Belong);

            // Помощь отнимает силы — и тем больше, чем хуже мне самому.
            FNeedPromise Cost;
            Cost.Need = ENeedType::Comfort;
            Cost.Amount = -0.12f;
            Help.Promises.Add(Cost);

            Out.Add(Help);
        }

        // --- Высказать всё ---
        // Появляется, когда есть обида. Но воспользуется ли человек этой
        // возможностью — зависит от гнева, страха, привязанности и характера.
        if (R && R->Resentment > 0.3f)
        {
            FAffordance Confront;
            Confront.Action = EActionType::Confront;
            Confront.Source = EAffordanceSource::Person;
            Confront.Target = Other;
            Confront.Location = Other->GetActorLocation();
            Confront.bHasLocation = true;
            Confront.Duration = 400.0f;
            Confront.EffortCost = 0.3f;
            Confront.Risk = 0.25f + R->Fear * 0.5f;
            Confront.Label = FString::Printf(TEXT("высказать всё — %s"), *Name);
            Confront.Key = FName(*FString::Printf(TEXT("высказать@%s"), *Name));
            Confront.CategoryKey = FName(*FString::Printf(TEXT("высказать@%s"), *Kindred));

            FNeedPromise Esteem;
            Esteem.Need = ENeedType::Esteem;
            Esteem.Amount = R->Resentment * 0.6f;
            Confront.Promises.Add(Esteem);

            FNeedPromise Autonomy;
            Autonomy.Need = ENeedType::Autonomy;
            Autonomy.Amount = R->Resentment * 0.4f;
            Confront.Promises.Add(Autonomy);

            // И почти наверняка испортит отношения.
            FNeedPromise Belong;
            Belong.Need = ENeedType::Belonging;
            Belong.Amount = -Closeness * 0.5f;
            Confront.Promises.Add(Belong);

            Out.Add(Confront);
        }

        // --- Извиниться ---
        if (OwedApologyTo == Other)
        {
            FAffordance Apology;
            Apology.Action = EActionType::Apologize;
            Apology.Source = EAffordanceSource::Person;
            Apology.Target = Other;
            Apology.Location = Other->GetActorLocation();
            Apology.bHasLocation = true;
            Apology.Duration = 300.0f;
            // Извиняться трудно — тем труднее, чем больше гордости.
            Apology.EffortCost = 0.35f;
            Apology.Label = FString::Printf(TEXT("извиниться — %s"), *Name);
            Apology.Key = FName(*FString::Printf(TEXT("извиниться@%s"), *Name));
            Apology.CategoryKey = FName(*FString::Printf(TEXT("извиниться@%s"), *Kindred));

            FNeedPromise Belong;
            Belong.Need = ENeedType::Belonging;
            Belong.Amount = 0.35f + Closeness * 0.3f;
            Apology.Promises.Add(Belong);

            FNeedPromise Order;
            Order.Need = ENeedType::Order;
            Order.Amount = 0.25f;
            Apology.Promises.Add(Order);

            // Но бьёт по самолюбию.
            FNeedPromise Esteem;
            Esteem.Need = ENeedType::Esteem;
            Esteem.Amount = -0.2f;
            Apology.Promises.Add(Esteem);

            Out.Add(Apology);
        }

        // =================================================================
        //  К ТОМУ, КОГО УВАЖАЮТ, ИДУТ САМИ
        //
        //  Здесь нет ни лидера, ни свиты. Есть только то, что человек,
        //  которого ты высоко ставишь, — единственный, у кого имеет смысл
        //  спросить совета: он знает больше и не подведёт.
        //
        //  Если такого уважения нет ни к кому, возможности спросить тоже
        //  не возникает. А если многие уважают одного — к нему и потянутся,
        //  и он сам заметит, что стал кому-то нужен.
        // =================================================================
        if (R && R->Respect > 0.6f && R->Familiarity > 0.25f)
        {
            const float Doubt = Identity
                ? FMath::Clamp(1.0f - Identity->SelfEfficacy, 0.0f, 1.0f) : 0.5f;

            FAffordance Advice;
            Advice.Action = EActionType::Talk;
            Advice.Source = EAffordanceSource::Person;
            Advice.Target = Other;
            Advice.Location = Other->GetActorLocation();
            Advice.bHasLocation = true;
            Advice.Duration = 900.0f;
            Advice.EffortCost = 0.08f;
            Advice.Label = FString::Printf(TEXT("спросить совета — %s"), *Name);
            Advice.Key = FName(*FString::Printf(TEXT("совет@%s"), *Name));
            Advice.CategoryKey = TEXT("совет@человек");

            // За советом идут, когда своего разумения не хватает.
            FNeedPromise Order;
            Order.Need = ENeedType::Order;
            Order.Amount = 0.25f + Doubt * 0.35f + (R->Respect - 0.6f);
            Advice.Promises.Add(Order);

            FNeedPromise Competence;
            Competence.Need = ENeedType::Competence;
            Competence.Amount = 0.2f + R->Respect * 0.25f;
            Advice.Promises.Add(Competence);

            FNeedPromise Safety;
            Safety.Need = ENeedType::Safety;
            Safety.Amount = 0.15f * R->Trust;
            Advice.Promises.Add(Safety);

            Out.Add(Advice);

            // И просто держаться рядом с тем, кто внушает доверие.
            const bool bTheyRule = Other->IdentityComponent && Other->IdentityComponent->IsRoyal();
            if (R->Respect > 0.72f && R->Trust > 0.5f && !bTheyRule)
            {
                FAffordance Follow;
                Follow.Action = EActionType::Follow;
                Follow.Source = EAffordanceSource::Person;
                Follow.Target = Other;
                Follow.Location = Other->GetActorLocation();
                Follow.bHasLocation = true;
                Follow.Duration = 1500.0f;
                Follow.EffortCost = 0.12f;
                Follow.Label = FString::Printf(TEXT("держаться рядом — %s"), *Name);
                Follow.Key = FName(*FString::Printf(TEXT("рядом@%s"), *Name));
                Follow.CategoryKey = TEXT("рядом@человек");

                FNeedPromise Belong;
                Belong.Need = ENeedType::Belonging;
                Belong.Amount = 0.3f + R->Respect * 0.3f;
                Follow.Promises.Add(Belong);

                FNeedPromise Safe;
                Safe.Need = ENeedType::Safety;
                Safe.Amount = 0.25f * R->Trust;
                Follow.Promises.Add(Safe);

                Out.Add(Follow);
            }
        }
    }
}

void UMindComponent::GatherSelfAffordances(TArray<FAffordance>& Out) const
{
    const ACompleteHumanNPC* Human = GetHuman();
    if (!Human)
    {
        return;
    }

    GatherHandCrafts(Out);

    // --- Просто пройтись ---
    {
        FAffordance Wander;
        Wander.Action = EActionType::Wander;
        Wander.Source = EAffordanceSource::Self;
        Wander.Duration = 600.0f;
        Wander.EffortCost = 0.05f;
        Wander.Label = TEXT("пройтись");
        Wander.Key = TEXT("Wander@Self");
        Wander.bRequiresProximity = false;

        FNeedPromise New;
        New.Need = ENeedType::Novelty;
        New.Amount = 0.25f;
        Wander.Promises.Add(New);

        FNeedPromise Free;
        Free.Need = ENeedType::Autonomy;
        Free.Amount = 0.15f;
        Wander.Promises.Add(Free);

        Out.Add(Wander);
    }

    // --- Уйти подальше и посмотреть, что там ---
    {
        FAffordance Explore;
        Explore.Action = EActionType::Explore;
        Explore.Source = EAffordanceSource::Self;
        Explore.Duration = 900.0f;
        Explore.EffortCost = 0.2f;
        Explore.Risk = 0.1f;
        Explore.Label = TEXT("пойти куда глаза глядят");
        Explore.Key = TEXT("Explore@Self");
        Explore.bRequiresProximity = false;

        FNeedPromise New;
        New.Need = ENeedType::Novelty;
        New.Amount = 0.6f;
        Explore.Promises.Add(New);

        FNeedPromise Free;
        Free.Need = ENeedType::Autonomy;
        Free.Amount = 0.25f;
        Explore.Promises.Add(Free);

        FNeedPromise Tired;
        Tired.Need = ENeedType::Comfort;
        Tired.Amount = -0.15f;
        Explore.Promises.Add(Tired);

        Out.Add(Explore);
    }

    {
        FAffordance Reflect;
        Reflect.Action = EActionType::Reflect;
        Reflect.Source = EAffordanceSource::Self;
        Reflect.Duration = 240.0f;
        Reflect.EffortCost = 0.1f;
        Reflect.Label = TEXT("постоять, подумать");
        Reflect.Key = TEXT("Reflect@Self");
        Reflect.bRequiresProximity = false;

        FNeedPromise Meaning;
        Meaning.Need = ENeedType::Meaning;
        Meaning.Amount = 0.15f;
        Reflect.Promises.Add(Meaning);

        FNeedPromise Order;
        Order.Need = ENeedType::Order;
        Order.Amount = 0.12f;
        Reflect.Promises.Add(Order);

        Out.Add(Reflect);
    }

    {
        FAffordance Rest;
        Rest.Action = EActionType::Rest;
        Rest.Source = EAffordanceSource::Self;
        Rest.Duration = 300.0f;
        Rest.EffortCost = 0.0f;
        Rest.Label = TEXT("постоять, передохнуть");
        Rest.Key = TEXT("Rest@Self");
        Rest.bRequiresProximity = false;

        FNeedPromise Comfort;
        Comfort.Need = ENeedType::Comfort;
        Comfort.Amount = 0.2f;
        Rest.Promises.Add(Comfort);

        Out.Add(Rest);
    }

    const bool bFarFromBed = !Human->HasHome()
        || FVector::Dist2D(Human->GetActorLocation(), Human->GetHomeLocation()) > 6000.0f;
    const bool bCollapsing = Needs && Needs->GetSatisfaction(ENeedType::Sleep) < 0.2f;
    if (bFarFromBed && bCollapsing)
    {
        FAffordance Sleep;
        Sleep.Action = EActionType::Sleep;
        Sleep.Source = EAffordanceSource::Self;
        Sleep.Duration = 0.0f;
        Sleep.EffortCost = 0.0f;
        Sleep.Label = TEXT("прилечь прямо здесь");
        Sleep.Key = TEXT("Sleep@Anywhere");
        Sleep.bRequiresProximity = false;
        Sleep.Risk = 0.35f; // спать на улице небезопасно

        FNeedPromise Rest;
        Rest.Need = ENeedType::Sleep;
        Rest.Amount = 0.8f;
        Sleep.Promises.Add(Rest);

        FNeedPromise Cold;
        Cold.Need = ENeedType::Comfort;
        Cold.Amount = -0.3f;
        Sleep.Promises.Add(Cold);

        Out.Add(Sleep);
    }

    // =======================================================================
    //  ИСКАТЬ ТО, ЧЕГО НЕ ЗНАЕШЬ, ГДЕ ВЗЯТЬ
    //
    //  Человек больше не рождается со знанием города. Когда его донимает
    //  жажда, а ни одного места с водой он не помнит, у него остаётся
    //  единственное человеческое средство: встать и пойти искать.
    //
    //  Это не «правило поведения» — это то, что вообще возможно сделать,
    //  когда не знаешь. Пойдёт он или предпочтёт терпеть, решит он сам.
    // =======================================================================
    if (Needs && Memory)
    {
        static const ENeedType Searchable[] = {
            ENeedType::Thirst, ENeedType::Hunger, ENeedType::Sleep,
            ENeedType::Bladder, ENeedType::Hygiene, ENeedType::Money
        };

        for (ENeedType What : Searchable)
        {
            const float Urgency = Needs->GetUrgency(What);
            if (Urgency < 0.35f)
            {
                continue;
            }

            // А не знаю ли я уже места, где с этим помогут?
            bool bKnowWhere = false;
            for (const FKnownLocation& Place : Memory->Places)
            {
                for (const FAffordance& Offer : Place.Offers)
                {
                    for (const FNeedPromise& Promise : Offer.Promises)
                    {
                        if (Promise.Need == What && Promise.Amount > 0.25f)
                        {
                            bKnowWhere = true;
                            break;
                        }
                    }
                    if (bKnowWhere) { break; }
                }
                if (bKnowWhere) { break; }
            }

            if (bKnowWhere)
            {
                continue;
            }

            FAffordance Search;
            Search.Action = EActionType::Explore;
            Search.Source = EAffordanceSource::Self;
            Search.Duration = 900.0f;
            Search.EffortCost = 0.18f;
            Search.Risk = 0.08f;
            Search.Label = FString::Printf(TEXT("искать, где тут %s"), *HumanText::Need(What));
            Search.Key = FName(*FString::Printf(TEXT("Seek@%s"), *HumanText::Need(What)));
            Search.bRequiresProximity = false;

            // Он не знает, найдёт ли. Но надеется — иначе бы не пошёл.
            FNeedPromise Hope;
            Hope.Need = What;
            Hope.Amount = 0.45f;
            Search.Promises.Add(Hope);

            FNeedPromise New;
            New.Need = ENeedType::Novelty;
            New.Amount = 0.3f;
            Search.Promises.Add(New);

            FNeedPromise Tired;
            Tired.Need = ENeedType::Comfort;
            Tired.Amount = -0.12f;
            Search.Promises.Add(Tired);

            Out.Add(Search);
        }
    }

    // --- Поискать, чем заняться в жизни ---
    // Появляется только у того, кто без работы: это не правило, а факт
    // его положения. Захочет ли он этим заняться — другой вопрос.
    if (Identity && !Identity->bEmployed && Identity->Age >= 18.0f && Identity->Age <= 70.0f && !FVillage::IsMedieval(this))
    {
        FAffordance Seek;
        Seek.Action = EActionType::Explore;
        Seek.Source = EAffordanceSource::Self;
        Seek.Duration = 1200.0f;
        Seek.EffortCost = 0.35f;
        Seek.Label = TEXT("искать работу");
        Seek.Key = TEXT("SeekWork@Self");
        Seek.bRequiresProximity = false;

        FNeedPromise Money;
        Money.Need = ENeedType::Money;
        Money.Amount = 0.5f;
        Seek.Promises.Add(Money);

        FNeedPromise Esteem;
        Esteem.Need = ENeedType::Esteem;
        Esteem.Amount = 0.3f;
        Seek.Promises.Add(Esteem);

        Out.Add(Seek);
    }
}

// ---------------------------------------------------------------------------
//  РЕШЕНИЕ
//
//  Здесь нет ни одного «если голоден — иди есть». Человек осматривается,
//  видит набор возможностей и взвешивает каждую через своё состояние.
//  Что победит — заранее неизвестно даже ему самому.
// ---------------------------------------------------------------------------

void UMindComponent::SnapshotState()
{
    const int32 Count = static_cast<int32>(ENeedType::MAX);
    NeedSnapshot.SetNumZeroed(Count);
    UrgencySnapshot.SetNumZeroed(Count);

    if (Needs)
    {
        for (int32 i = 0; i < Count; ++i)
        {
            const ENeedType Type = static_cast<ENeedType>(i);
            NeedSnapshot[i] = Needs->GetSatisfaction(Type);
            UrgencySnapshot[i] = Needs->GetUrgency(Type);
        }
    }

    AffectSnapshot = Emotions ? Emotions->GetAffect().Pleasure : 0.0f;
    MoneySnapshot = Identity ? Identity->Money : 0.0f;
}

float UMindComponent::MeasureOutcome() const
{
    // Чем дело обернулось на самом деле — считается по тому, что изменилось
    // внутри, а не по тому, что было обещано. Обещание может и не сбыться.
    if (!Needs || NeedSnapshot.Num() == 0)
    {
        return 0.0f;
    }

    float Value = 0.0f;
    float WeightSum = 0.0f;

    const int32 Count = FMath::Min(NeedSnapshot.Num(), static_cast<int32>(ENeedType::MAX));
    for (int32 i = 0; i < Count; ++i)
    {
        const ENeedType Type = static_cast<ENeedType>(i);
        const float Delta = Needs->GetSatisfaction(Type) - NeedSnapshot[i];

        // Насколько это было важно — по состоянию НА МОМЕНТ НАЧАЛА.
        const float Weight = FMath::Clamp(UrgencySnapshot[i], 0.05f, 3.0f);
        Value += Delta * Weight;
        WeightSum += Weight;
    }

    if (WeightSum > 0.0f)
    {
        Value = Value / WeightSum * 4.0f;
    }

    // Сдвиг настроения — тоже часть итога: иногда дело ничего не дало,
    // но от него стало легче. Или наоборот.
    if (Emotions)
    {
        Value += (Emotions->GetAffect().Pleasure - AffectSnapshot) * 0.7f;
    }

    // И деньги, если они появились.
    if (Identity && MoneySnapshot > 0.0f)
    {
        Value += FMath::Clamp((Identity->Money - MoneySnapshot) / 50.0f, -0.5f, 0.5f);
    }

    return FMath::Clamp(Value, -1.0f, 1.0f);
}

void UMindComponent::ApplyPromises(const FAffordance& A, float SuccessScale)
{
    const float Scale = FMath::Clamp(SuccessScale, 0.0f, 1.5f);

    for (const FNeedPromise& Promise : A.Promises)
    {
        // То, что отнимается, отнимается полностью, независимо от успеха:
        // силы тратятся, даже если ничего не вышло.
        const float Amount = (Promise.Amount > 0.0f) ? Promise.Amount * Scale : Promise.Amount;

        // Сначала пробуем через тело: еда наполняет желудок по-настоящему,
        // а не «прибавляет очков сытости».
        bool bHandledByBody = false;
        if (Body)
        {
            bHandledByBody = Body->ApplyNeedEffect(Promise.Need, Amount, ActionTotalDuration);
        }

        if (!bHandledByBody && Needs)
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

        // Долгосрочные замыслы двигаются тем же, что двигает потребности.
        if (Motivation && Amount > 0.0f)
        {
            Motivation->RegisterProgress(Promise.Need, Amount * 0.12f, Now());
        }

        // И мечта тоже: она не отдельная сущность, а то, к чему человек
        // идёт делами. Без этого он мечтал, но никогда не приближался.
        if (Identity && Amount > 0.0f && Promise.Need == Identity->DreamNeed)
        {
            Identity->AdvanceDream(Amount * 0.05f, Now());
        }
    }
    if (Needs && Body)
    {
        Needs->ReadBody(Body, GetWorldMind() ? GetWorldMind()->Now.HourFloat : 12.0f);
        if (Now() - CryHeardAt < 40.0f)
        {
            if (FNeedState* Calm = Needs->Find(ENeedType::Comfort))
            {
                Calm->Satisfaction = FMath::Min(Calm->Satisfaction, 0.4f);
            }
        }
    }
}

int32 UMindComponent::ChooseCoping(float Stress)
{
    float Lean[7] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    if (Personality)
    {
        Personality->CopingLeanings(Stress, Lean);
    }
    float Score[7];
    float Best = -BIG_NUMBER;
    for (int32 Index = 0; Index < 7; ++Index)
    {
        Score[Index] = Lean[Index] * 0.6f + CopingWorth[Index] * 4.0f + 0.25f / (1.0f + CopingTries[Index]);
        Best = FMath::Max(Best, Score[Index]);
    }
    float Total = 0.0f;
    float Chance[7];
    for (int32 Index = 0; Index < 7; ++Index)
    {
        Chance[Index] = FMath::Exp((Score[Index] - Best) / 0.2f);
        Total += Chance[Index];
    }
    float Draw = FMath::FRand() * Total;
    for (int32 Index = 0; Index < 7; ++Index)
    {
        Draw -= Chance[Index];
        if (Draw <= 0.0f)
        {
            return Index;
        }
    }
    return 6;
}

void UMindComponent::LearnCoping(float Stress)
{
    if (LastCoping < 0 || LastCoping >= 7)
    {
        return;
    }
    const float Relief = StressBeforeCoping - Stress;
    CopingWorth[LastCoping] += 0.2f * (Relief - CopingWorth[LastCoping]);
    ++CopingTries[LastCoping];
    LastCoping = -1;
}

void UMindComponent::Deliberate(float GameDelta)
{
    if (!Deliberation || !Needs || bPolicyControlled)
    {
        return;
    }
    if (const ACompleteHumanNPC* Hands = GetHuman())
    {
        if (Hands->AreHandsBusy() || (Hands->Motor && Hands->Motor->IsDown()))
        {
            return;
        }
    }

    const float T = Now();
    UHumanWorldSubsystem* World = GetWorldMind();

    // Мечта не приказывает — она лишь остаётся в списке того, к чему человек
    // идёт, и слегка подкрашивает оценку всего остального.
    if (Motivation && Identity && FMath::FRand() < 0.1f)
    {
        Motivation->SetLifeGoal(Identity->LifeDream, Identity->DreamNeed, T);
    }

    // --- Держусь ли я того, что решил --------------------------------------
    //
    // Человек, решившись, не спорит с собой каждую секунду. Он идёт.
    // Сбить его может только тело: жажда, голод, нужда, опасность — то,
    // что не терпит. Всё остальное подождёт, пока начатое не кончится.
    CommitmentLeft = FMath::Max(0.0f, CommitmentLeft - GameDelta);

    ENeedType Screaming = ENeedType::Hunger;
    bool bUrgent = false;
    {
        static const ENeedType Vitals[] = {
            ENeedType::Thirst, ENeedType::Hunger, ENeedType::Bladder,
            ENeedType::Sleep,  ENeedType::Safety, ENeedType::Health
        };
        float Worst = 0.18f;
        for (ENeedType Vital : Vitals)
        {
            const float Have = Needs->GetSatisfaction(Vital);
            if (Have < Worst)
            {
                Worst = Have;
                Screaming = Vital;
                bUrgent = true;
            }
        }
    }

    // Держится ли он решения. Даже когда тело кричит, человек не мечется
    // без разбора: его сбивает только то дело, которое эту нужду и закроет.
    // Посреди разговора собеседника не бросают ради пустяка.
    if (IsInConversation() && !bUrgent)
    {
        return;
    }

    const bool bHolding = bActionActive && ActiveIntention.bValid && CommitmentLeft > 0.0f;

    if (bHolding && !bUrgent)
    {
        return;
    }

    if (bHolding && bUrgent)
    {
        // Может, он как раз за этим и идёт?
        for (const FNeedPromise& Promise : ActiveIntention.Affordance.Promises)
        {
            if (Promise.Need == Screaming && Promise.Amount > 0.2f)
            {
                return;
            }
        }
    }

    // --- Что вообще можно сделать ------------------------------------------
    TArray<FAffordance> Available;
    GatherAffordances(Available);
    if (Available.Num() == 0)
    {
        return;
    }

    // --- Кто я сейчас такой ------------------------------------------------
    FDeliberationContext Ctx;
    Ctx.Needs = Needs;
    Ctx.Memory = Memory;
    Ctx.Personality = Personality;
    Ctx.Emotions = Emotions;
    Ctx.Motivation = Motivation;
    Ctx.Mind = const_cast<UMindComponent*>(this);
    Ctx.Identity = Identity;
    Ctx.Social = Social;
    Ctx.Body = Body;
    Ctx.WorldTime = T;
    Ctx.HourOfDay = World ? World->Now.HourFloat : 12.0f;
    Ctx.Money = Identity ? Identity->Money : 0.0f;
    Ctx.Impairment = Body ? Body->GetCognitiveImpairment() : 0.0f;

    if (ACompleteHumanNPC* Human = GetHuman())
    {
        Ctx.SelfLocation = Human->GetActorLocation();
        Ctx.WalkSpeed = Human->BaseWalkSpeed;
    }

    // --- Чего не хватает под то, что я умею -------------------------------
    // Знание без материала мертво. Кто прочёл про вино, тянется к винограду:
    // дело, дающее недостающий вход, получает часть ценности самого ремесла.
    // Человек так и думает: «сделал бы вино — да винограда нет».
    {
        const float Foresight = Personality
            ? FMath::Clamp(Personality->Patience() * 0.6f + Personality->Traits.Conscientiousness * 0.4f, 0.05f, 1.0f)
            : 0.5f;
        const ACompleteHumanNPC* Owner = GetHuman();

        for (const FCraft& Deed : FCraftBook::All())
        {
            const float Knows = RecallOfDeed(Deed.Id);
            if (Knows < 0.3f || Deed.Inputs.Num() == 0 || Deed.Output == EResourceKind::None)
            {
                continue;
            }

            // Запас сделанного — его и пополнять незачем.
            const float HaveOutput = FVillage::HouseholdStores(Owner, Deed.Output);
            if (HaveOutput >= 4.0f)
            {
                continue;
            }

            for (const FCraftPart& Need : Deed.Inputs)
            {
                if (Need.Amount <= 0.0f || Need.Kind == EResourceKind::Water)
                {
                    continue;
                }
                const float HaveInput = FVillage::HouseholdStores(Owner, Need.Kind);
                if (HaveInput >= Need.Amount)
                {
                    continue;   // материал есть — тянуться незачем
                }
                // Тяга к входу = знание × нехватка выхода × пригодность результата.
                const float OutputPull = 0.1f + (FVillage::IsFood(Deed.Output) ? 0.12f : 0.0f)
                    + FMath::Clamp(FVillage::PriceOf(Deed.Output) * 0.008f, 0.0f, 0.2f);
                float& Slot = Ctx.WantedInputs.FindOrAdd(Need.Kind, 0.0f);
                Slot = FMath::Max(Slot, Knows * OutputPull * Foresight);
            }
        }
    }

    // --- Взвесить и выбрать -------------------------------------------------
    FIntention Candidate;
    float OngoingScore = 0.0f;
    const bool bBusy = bActionActive && ActiveIntention.bValid;
    const bool bChosen = bCoreReady
        ? DecideByCore(Available, Candidate, bBusy ? &OngoingScore : nullptr)
        : Deliberation->Decide(Available, Ctx, Candidate);
    if (!bChosen)
    {
        // Ничего не показалось стоящим. Это тоже состояние, и оно узнаваемо.
        if (!bActionActive && FMath::FRand() < 0.2f)
        {
            Think(TEXT("и делать ничего не хочется"), EThoughtKind::Feeling, 0.5f);
        }
        return;
    }

    // --- Бросать ли начатое -------------------------------------------------
    if (bBusy)
    {
        if (Candidate.Affordance.Key == ActiveIntention.Affordance.Key)
        {
            return; // то же самое, продолжаем
        }

        if (bCoreReady)
        {
            const float Margin = 0.05f * Deliberation->SwitchingThreshold(Ctx);
            if (Candidate.Valuation.Total < OngoingScore + Margin)
            {
                return;
            }
        }
        else
        {
            const FValuation Ongoing = Deliberation->Evaluate(ActiveIntention.Affordance, Ctx);
            const float Threshold = Deliberation->SwitchingThreshold(Ctx);
            if (Candidate.Valuation.Total < Ongoing.Total * Threshold)
            {
                return; // не перевесило
            }
        }
        bDecisionOpen = false;

        // Перевесило. Человек сам замечает, что передумал.
        if (ACompleteHumanNPC* Human = GetHuman())
        {
            Human->StopMoving();
        }
        ReleaseOccupied();

        // Бросая начатое, надо встать и выпустить из рук то, что держал.
        // Без этого человек уходил сидя и с чужой книгой в обнимку.
        FinishAction();

        bActionActive = false;
        bMoving = false;
        Report(FString::Printf(TEXT("передумал(а), теперь: %s"), *Candidate.Affordance.Label));

        Think(FString::Printf(TEXT("нет. %s"), *Candidate.Reason), EThoughtKind::Intention, 0.7f);
    }

    BeginIntention(Candidate);
}

// ---------------------------------------------------------------------------
//  Исполнение
// ---------------------------------------------------------------------------

void UMindComponent::ReleaseOccupied()
{
    if (OccupiedSource)
    {
        OccupiedSource->Release();
        OccupiedSource = nullptr;
    }
}

void UMindComponent::BeginIntention(const FIntention& Intention)
{
    ACompleteHumanNPC* Human = GetHuman();
    if (!Human || !Intention.bValid)
    {
        return;
    }

    // Всё, что занимали раньше, отпускаем. Иначе кровать остаётся «занятой»
    // навсегда, и вещь перестаёт что-либо предлагать вообще кому бы то ни было.
    ReleaseOccupied();

    ActiveIntention = Intention;
    const FAffordance& A = ActiveIntention.Affordance;
    if (bCoreReady)
    {
        DecisionSelf = SenseSelf();
        DecisionView = ViewOf(A);
        DecisionAt = Now();
        bDecisionOpen = true;
    }

    CurrentAction = A.Action;
    CurrentActionLabel = A.Label.IsEmpty() ? HumanText::Action(A.Action) : A.Label;
    bActionActive = true;
    ActionTotalDuration = FMath::Max(A.Duration, 1.0f);

    if (Speech)
    {
        if (const AFurnitureActor* Thing = Cast<AFurnitureActor>(A.Target.Get()))
        {
            Speech->NoticeThing(Thing->Name);
        }
        else if (Cast<ABookActor>(A.Target.Get()))
        {
            Speech->NoticeThing(TEXT("книга"));
        }
        if (A.Action == EActionType::Drink)
        {
            Speech->NoticeThing(TEXT("вода"));
            if (Needs && Needs->GetUrgency(ENeedType::Thirst) > 0.35f)
            {
                Speech->NoticeThing(TEXT("жажда"));
            }
        }
        else if (A.Action == EActionType::Eat)
        {
            Speech->NoticeThing(TEXT("еда"));
        }
        else if (A.Action == EActionType::Wash)
        {
            Speech->NoticeThing(TEXT("мыло"));
            Speech->NoticeThing(TEXT("вода"));
        }
        if (A.Label.Contains(TEXT("чай")) || A.Label.Contains(TEXT("чаю")))
        {
            Speech->NoticeThing(TEXT("чай"));
        }
        if (A.Label.Contains(TEXT("урожай")) || A.Label.Contains(TEXT("продукт")))
        {
            Speech->NoticeThing(TEXT("овощи"));
        }
    }
    ActionTimeLeft = A.Duration;

    if (A.Source == EAffordanceSource::Person)
    {
        ConversationPartner = A.Target;
    }

    // Запоминаем, каким было состояние до — чтобы потом было с чем сравнить.
    SnapshotState();

    // Человек проговаривает себе решение. Это и есть его «почему».
    if (!Intention.Reason.IsEmpty())
    {
        Think(Intention.Reason, EThoughtKind::Intention, 0.65f, A.Target);
    }

    // --- Надо ли идти -------------------------------------------------------
    const float Distance = A.bHasLocation
        ? FVector::Dist2D(Human->GetActorLocation(), A.Location)
        : 0.0f;

    Report(FString::Printf(TEXT("решил(а): %s"), *CurrentActionLabel));

    // Насколько его хватит, чтобы не передумать. Собранный держится дольше,
    // порывистый бросает быстрее — но на дорогу время даётся всегда, иначе
    // человек передумывал бы, не сделав и шага.
    {
        const float Grit = Personality ? Personality->Traits.Conscientiousness : 0.5f;
        const float Whim = Personality ? Personality->Facets.Impulsivity : 0.4f;
        const float Travel = A.bHasLocation
            ? FVector::Dist2D(Human->GetActorLocation(), A.Location) / FMath::Max(60.0f, Human->BaseWalkSpeed)
            : 0.0f;

        CommitmentLeft = FMath::Lerp(90.0f, 420.0f, Grit) * (1.0f - Whim * 0.4f)
                       + Travel * 60.0f;
    }

    // Идти надо не только когда далеко, но и когда близко, да через стену.
    const bool bBlocked = A.bHasLocation && !Human->CanReachPoint(A.Location);

    if (A.bRequiresProximity && A.bHasLocation && (Distance > 220.0f || bBlocked))
    {
        MoveDestination = A.Location;
        Human->RequestMoveTo(MoveDestination);
        bMoving = true;
        Report(FString::Printf(TEXT("пошёл(шла) туда, %d м"),
               FMath::RoundToInt(Distance / 100.0f)));
        // Вход ещё надо найти — считаем, как будто идти вокруг дома.
        MoveTimeout = bBlocked
            ? EstimateTravelBudget(Distance + 4000.0f)
            : EstimateTravelBudget(Distance);
        return;
    }

    // --- Дела, для которых надо просто куда-то пойти ------------------------
    if (A.Action == EActionType::Wander || A.Action == EActionType::Explore)
    {
        const FVector Origin = Human->GetActorLocation();
        const float Radius = (A.Action == EActionType::Explore) ? 3500.0f : 1500.0f;
        MoveDestination = Origin + FVector(
            FMath::FRandRange(-Radius, Radius),
            FMath::FRandRange(-Radius, Radius), 0.0f);
        Human->RequestMoveTo(MoveDestination);
        bMoving = true;
        MoveTimeout = EstimateTravelBudget(Radius);
        return;
    }

    // --- Уже на месте -------------------------------------------------------
    Human->StopMoving();
    bMoving = false;

    // Даже стоя на месте, человек не приступает мгновенно: надо взять в руки
    // то, что нужно, и сесть. Без этого книга читалась сама собой.
    RefreshIntentionFromSurroundings();
    Phase = EActionPhase::Preparing;
    PrepareTimeLeft = FMath::Max(ActiveIntention.Affordance.SetupSeconds, 0.0f);

    // Занимаем вещь, если она одна на всех.
    if (UHumanWorldSubsystem* World = GetWorldMind())
    {
        if (A.bHasLocation)
        {
            if (UAffordanceComponent* Source = World->FindSourceAt(A.Location, 400.0f))
            {
                OccupiedSource = Source;
                Source->Occupy();
            }
        }
    }

    // Сон — единственное дело без срока: он длится, пока длится.
    if (A.Action == EActionType::Sleep)
    {
        const float Hour = GetWorldMind() ? GetWorldMind()->Now.HourFloat : 12.0f;
        const float Pressure = Body ? Body->GetSleepPressure(Hour) : 1.0f;

        // Лечь — не значит уснуть.
        if (Pressure < 0.3f)
        {
            Think(TEXT("лёг(ла), а сна ни в одном глазу"), EThoughtKind::Feeling, 0.5f);
            CompleteIntention(false);
            return;
        }

        BeginSleep();
        return;
    }

    if (ActionTimeLeft <= 0.0f)
    {
        ActionTimeLeft = 300.0f;
        ActionTotalDuration = 300.0f;
    }
}

void UMindComponent::Act(float GameDelta)
{
    if (!bActionActive)
    {
        return;
    }

    ACompleteHumanNPC* Human = GetHuman();
    if (!Human)
    {
        return;
    }
    if (Human->Motor && Human->Motor->IsDown())
    {
        return;
    }

    const FAffordance& A = ActiveIntention.Affordance;

    // --- Дорога -------------------------------------------------------------
    if (bMoving)
    {
        if (Human->AreHandsBusy())
        {
            return;
        }
        MoveTimeout -= GameDelta;

        // За человеком приходится идти следом — он ведь тоже ходит.
        if (A.Source == EAffordanceSource::Person && A.Target)
        {
            const FVector Fresh = A.Target->GetActorLocation();
            if (FVector::DistSquared(Fresh, MoveDestination) > 250000.0f)
            {
                MoveDestination = Fresh;
                Human->RequestMoveTo(MoveDestination);
            }
        }

        const float Distance = FVector::Dist2D(Human->GetActorLocation(), MoveDestination);
        const float ArriveRadius = (A.Source == EAffordanceSource::Person) ? 230.0f : 190.0f;

        // Оказаться в двух метрах от плиты, стоя снаружи у стены, — не значит
        // дойти до плиты. Пока между нами стена, человек продолжает искать вход.
        const bool bWithinReach = Distance <= ArriveRadius
            && (A.Source == EAffordanceSource::Person || Human->CanReachPoint(MoveDestination));

        if (bWithinReach)
        {
            bMoving = false;
            Human->StopMoving();

            if (Memory)
            {
                Memory->VisitPlace(Human->GetActorLocation(), Now(),
                    Emotions ? Emotions->GetAffect().Pleasure * 0.2f : 0.0f);
            }

            // Дошёл. Если само хождение и было делом — оно закончено.
            if (A.Action == EActionType::Wander || A.Action == EActionType::Explore)
            {
                ActionTimeLeft = FMath::Min(ActionTimeLeft, 60.0f);
                Phase = EActionPhase::Doing;
            }
            else
            {
                // Иначе теперь начинается собственно дело.
                if (UHumanWorldSubsystem* World = GetWorldMind())
                {
                    if (UAffordanceComponent* Source = World->FindSourceAt(A.Location, 400.0f))
                    {
                        OccupiedSource = Source;
                        Source->Occupy();
                    }
                }

                if (A.Action == EActionType::Sleep)
                {
                    const float Hour = GetWorldMind() ? GetWorldMind()->Now.HourFloat : 12.0f;
                    const float Pressure = Body ? Body->GetSleepPressure(Hour) : 1.0f;
                    if (Pressure < 0.3f)
                    {
                        Think(TEXT("дошёл(ла) до кровати, а спать расхотелось"), EThoughtKind::Feeling, 0.5f);
                        CompleteIntention(false);
                        return;
                    }
                    BeginSleep();
                    return;
                }

                if (ActionTimeLeft <= 0.0f)
                {
                    ActionTimeLeft = 300.0f;
                    ActionTotalDuration = 300.0f;
                }

                Report(TEXT("дошёл(шла) на место"));

                // Человек шёл сюда по памяти: «там можно поесть», «там книги».
                // Память хранит место и род занятия, но не саму вещь. Придя,
                // он смотрит, что здесь есть на самом деле, и берётся за то,
                // что видит своими глазами.
                RefreshIntentionFromSurroundings();

                // Если оказалось, что идти ещё надо, — идём дальше.
                if (bMoving)
                {
                    return;
                }

                // Дошёл — но ещё не начал. Сперва надо взять что нужно и сесть.
                Phase = EActionPhase::Preparing;
                PrepareTimeLeft = FMath::Max(ActiveIntention.Affordance.SetupSeconds, 0.0f);
            }
        }
        else if (MoveTimeout <= 0.0f)
        {
            // Не дошёл. Дорога оказалась не такой, как думалось, — и это
            // тоже опыт: в следующий раз туда захочется меньше.
            bMoving = false;
            Human->StopMoving();
            Report(TEXT("не смог(ла) дойти, бросил(а) затею"));
            Think(TEXT("не могу туда добраться"), EThoughtKind::Judgement, 0.6f);
            CompleteIntention(false);
            return;
        }
        else
        {
            return; // идём дальше
        }
    }

    // --- Приготовления ------------------------------------------------------
    //
    // Между «дошёл» и «делаю» лежит то, что раньше проскакивало незаметно:
    // взять книгу с полки, сесть за парту, приладиться. Это и есть та
    // разница, из-за которой казалось, будто люди берут вещи из воздуха.
    if (Phase == EActionPhase::Preparing)
    {
        if (!PrepareForAction())
        {
            // Условие не выполнить — дело не состоится. Это не поражение,
            // а обычное человеческое: пришёл, а книгу уже забрали.
            CompleteIntention(false);
            return;
        }

        PrepareTimeLeft -= GameDelta;
        if (PrepareTimeLeft > 0.0f)
        {
            return;
        }

        Phase = EActionPhase::Doing;
        Report(FString::Printf(TEXT("принялся(лась): %s"), *CurrentActionLabel));
    }

    // --- Само дело ----------------------------------------------------------
    ActionTimeLeft -= GameDelta;

    // Дело с человеком — это разговор: заговорить, если ещё не говорим,
    // и не считать дело оконченным, пока разговор идёт.
    if (A.Source == EAffordanceSource::Person && A.Target && A.Action == EActionType::Help
        && A.Label.StartsWith(TEXT("научить грамоте")))
    {
        TeachReading(A.Target, GameDelta);
    }
    if (A.Source == EAffordanceSource::Person && A.Target && A.Action == EActionType::Study
        && A.Deal == TEXT("learn"))
    {
        TeachProduction(A.Target, GameDelta);
    }

    if (A.Source == EAffordanceSource::Person && A.Target
        && (A.Action == EActionType::Talk || A.Action == EActionType::Comfort || A.Action == EActionType::Apologize))
    {
        if (IsTalkingWith(A.Target))
        {
            ActionTimeLeft = FMath::Max(ActionTimeLeft, 30.0f);
        }
        else if (IsInConversation() || SpeechCooldown > 0.0f || !StartConversation(A.Target))
        {
            // Заговорить не вышло — молча стоять рядом незачем.
            ActionTimeLeft = FMath::Min(ActionTimeLeft, 60.0f);
        }
    }

    // Работа приносит деньги, пока длится, а не в конце.
    if (A.MoneyGainPerHour > 0.0f && Identity)
    {
        Identity->Money += A.MoneyGainPerHour * (GameDelta / 3600.0f);
    }
    if (A.Action == EActionType::Work && Identity)
    {
        Identity->NoteWorked(Now());
    }

    if (ActionTimeLeft <= 0.0f)
    {
        CompleteIntention(true);
    }
}

void UMindComponent::CompleteIntention(bool bArrived)
{
    const FAffordance A = ActiveIntention.Affordance; // копия: дальше всё меняется
    const float Expectation = ActiveIntention.Expectation;
    const float T = Now();
    bool bReached = bArrived;
    if (bReached && A.MoneyCost > 0.0f && Identity && Identity->Money < A.MoneyCost)
    {
        bReached = false;
        Think(TEXT("денег не хватило"), EThoughtKind::Judgement, 0.6f);
        Report(FString::Printf(TEXT("не хватило денег: %s"), *A.Label));
    }

    bActionActive = false;
    bMoving = false;

    ReleaseOccupied();

    // --- Получилось ли ------------------------------------------------------
    // Умение решает, насколько хорошо вышло. Провал — не «ничего», а «хуже».
    float SuccessScale = bReached ? 1.0f : 0.0f;
    bool bSkillFailed = false;

    if (bReached && !A.Craft.IsNone())
    {
        SuccessScale = PerformCraft(A, T);
        bSkillFailed = SuccessScale < 0.5f;
    }
    else if (bReached && !A.RequiredSkill.IsNone())
    {
        float Chance = 0.0f;
        const bool bWorked = TryDo(A.RequiredSkill, A.Difficulty, Chance);

        SuccessScale = bWorked ? FMath::Clamp(0.8f + Chance * 0.4f, 0.8f, 1.3f)
                               : FMath::Clamp(0.2f + Chance * 0.3f, 0.05f, 0.6f);
        bSkillFailed = !bWorked;

        Practised(A.RequiredSkill, bWorked ? 0.05f : 0.02f);

        Think(bWorked ? FString::Printf(TEXT("%s - вышло"), *MasteryLabel(A.RequiredSkill))
                      : FString::Printf(TEXT("%s - не вышло, руки ещё не те"), *MasteryLabel(A.RequiredSkill)),
              bWorked ? EThoughtKind::Judgement : EThoughtKind::SelfTalk, 0.55f);
    }

    // --- Раздаём то, что было обещано --------------------------------------
    if (bReached)
    {
        if (FVillage::IsWorkLike(A))
        {
            const int32 Day = FVillage::DayOfYear(this);
            if (WorkDay != Day)
            {
                WorkDay = Day;
                WorkedToday = 0.0f;
            }
            WorkedToday += ActionTotalDuration / 3600.0f;
            if (Identity && GetWorldMind())
            {
                FVillage::JudgeDay(Needs, WorkedToday, FVillage::ExpectedWork(Identity->Age, Identity->Role, Day, GetWorldMind()->Now.HourFloat),
                    ActionTotalDuration / 3600.0f);
            }
        }
        ApplyPromises(A, SuccessScale);

        // Плата за вход — если она была.
        if (A.MoneyCost > 0.0f && Identity)
        {
            Identity->Money = FMath::Max(0.0f, Identity->Money - A.MoneyCost);
        }
    }

    // --- Люди -------------------------------------------------------------
    if (A.Source == EAffordanceSource::Person && A.Target)
    {
        if (Social)
        {
            Social->RecordInteraction(A.Target, bReached ? 0.3f : -0.1f, T, Personality);
        }
        SocialWarmth = FMath::Clamp(SocialWarmth + 0.25f, 0.0f, 1.0f);

        // Помощь чувствует и тот, кому помогли.
        if (bReached && (A.Action == EActionType::Comfort || A.Action == EActionType::Help))
        {
            if (ACompleteHumanNPC* Other = Cast<ACompleteHumanNPC>(A.Target.Get()))
            {
                if (UMindComponent* TheirMind = Other->FindComponentByClass<UMindComponent>())
                {
                    TheirMind->OnHelped(GetOwner(), 0.5f);

                    // Тот, кому помогли, уважает помогшего сильнее прочих —
                    // он это на себе испытал.
                    TheirMind->JudgeByDeeds(GetOwner(), 0.09f, TEXT("выручил, когда было нужно"));
                }
            }
            if (Social)
            {
                Social->AdjustReputation(0.04f);
            }
            if (Identity)
            {
                ++Identity->TimesHelpedOthers;
            }

            // =============================================================
            //  И это видят другие.
            //
            //  Помощь на людях — главное, из чего складывается вес человека
            //  в городе. Видевшие своими глазами меняют мнение сильнее,
            //  чем те, кому просто рассказали.
            // =============================================================
            if (ACompleteHumanNPC* Me = GetHuman())
            {
                if (UHumanWorldSubsystem* World = GetWorldMind())
                {
                    for (ACompleteHumanNPC* Witness : World->GetAllHumans())
                    {
                        if (!Witness || Witness == Me)
                        {
                            continue;
                        }
                        float Salience = 0.0f;
                        FName Sense;
                        if (!Witness->CanPerceive(Me, Salience, Sense) || Sense != TEXT("Sight"))
                        {
                            continue;
                        }
                        if (UMindComponent* TheirMind = Witness->FindComponentByClass<UMindComponent>())
                        {
                            TheirMind->JudgeByDeeds(Me, 0.05f * Salience,
                                                    TEXT("не проходит мимо чужой беды"));
                        }
                    }
                }
            }
        }

        if (A.Action == EActionType::Confront && bReached)
        {
            ResolveConfrontation(A.Target);
        }
    }

    // --- Особые дела, у которых последствия не в потребностях --------------
    if (bReached && (A.Key == TEXT("SeekWork@Self") || A.Label.StartsWith(TEXT("спросить про работу"))))
    {
        TryFindJob();
    }
    if (bReached)
    {
        StudyTextbook(A.Target.Get());

        // Припасы: истратить то, что пошло в дело, и получить сделанное.
        // Непременно до того, как человек уберёт всё из рук.
        SettleResources(A);

        if (OrderAt >= 0.0f && A.Key == OrderKey)
        {
            ++OrdersDone;
            OrderAt = -1.0f;
            ACompleteHumanNPC* Lord = Cast<ACompleteHumanNPC>(OrderFrom.Get());
            if (Needs)
            {
                Needs->Satisfy(ENeedType::Belonging, 0.2f);
                Needs->Satisfy(ENeedType::Esteem, 0.12f);
                Needs->Satisfy(ENeedType::Safety, 0.1f);
            }
            if (Lord && Lord->IdentityComponent && Identity && Lord->IdentityComponent->Money >= 1.0f)
            {
                Lord->IdentityComponent->Money -= 1.0f;
                Identity->Money += 1.0f;
            }
            if (Lord && Lord->SocialComponent)
            {
                Lord->SocialComponent->RecordInteraction(GetOwner(), 0.35f, T, Lord->PersonalityComponent);
            }
            Think(TEXT("исполнил(а) повеление — хозяева будут довольны"), EThoughtKind::Judgement, 0.7f, Lord);
            Report(FString::Printf(TEXT("исполнил(а) повеление %s — награда %s"), *NameOf(Lord), *FVillage::Coins(1.0f)));
        }

        if (AMatterStructure* House = Cast<AMatterStructure>(A.Target.Get()))
        {
            if (SuccessScale >= 0.5f && House->RepairFromOffer(SuccessScale))
            {
                Report(FString::Printf(TEXT("починил(а): %s"), *House->Title));
                if (UHumanWorldSubsystem* WorldMind = GetWorldMind())
                {
                    for (ACompleteHumanNPC* Witness : WorldMind->GetHumansNear(House->GetActorLocation(), 1500.0f, GetOwner()))
                    {
                        if (Witness && Witness->Mind && Witness->HasLineOfSight(GetOwner()))
                        {
                            Witness->Mind->JudgeByDeeds(GetOwner(), 0.04f, TEXT("чинит избу своими руками"));
                        }
                    }
                }
            }
        }
    }

    // Теперь можно прибрать за собой: положить вещь, встать.
    FinishAction();
    if (bReached && (A.Action == EActionType::Explore || A.Action == EActionType::Wander))
    {
        RoutineScore = FMath::Clamp(RoutineScore - 0.08f, 0.0f, 1.0f);
    }
    LearnFromOutcome(bReached && !bSkillFailed);

    // =======================================================================
    //  ГЛАВНОЕ: сравнить ожидание с действительностью
    //
    //  Отсюда — и только отсюда — берутся радость и разочарование.
    //  Нигде не написано «поел → доволен». Написано: получилось лучше,
    //  чем я думал, → и это приятно. А если хуже — неприятно, даже если
    //  формально всё вышло.
    // =======================================================================
    const float Actual = bReached ? MeasureOutcome() : -0.25f;

    // Если это был чужой совет — вышел ли он дельным.
    JudgeSuggestionOutcome(ActiveIntention.Affordance, Actual);

    // Чем кончилось — видно в летописи наравне с тем, что человек затеял.
    {
        const FString What = A.Label.IsEmpty() ? HumanText::Action(A.Action) : A.Label;
        Report(FString::Printf(TEXT("закончил(а): %s — %s"), *What,
               !bReached ? TEXT("не вышло")
               : Actual > 0.25f ? TEXT("хорошо")
               : Actual < -0.15f ? TEXT("плохо") : TEXT("так себе")));
    }

    // Ходил — значит, что-то увидел. Но в памяти оседает не каждый закоулок,
    // а только то, что чем-то зацепило: иначе карта в голове забивается
    // безымянными точками и вытесняет действительно нужные места.
    if (bReached && Memory && (A.Action == EActionType::Explore || A.Action == EActionType::Wander))
    {
        if (FMath::Abs(Actual) > 0.3f || FMath::FRand() < 0.15f)
        {
            Memory->LearnPlace(EPlaceKind::Landmark, GetOwner()->GetActorLocation(),
                               TEXT("место, где я был(а)"), true, nullptr, T);
        }
    }

    float PredictionError = Actual - Expectation;
    if (Memory && !A.Key.IsNone())
    {
        // Урок откладывается дважды: про эту вещь — быстро, про вещи
        // этого вида — медленно. Так один обжёгшийся о плиту делается
        // осторожнее со всеми плитами, не забывая, какая именно подвела.
        PredictionError = Memory->LearnOutcomeGeneralised(A.Key, A.CategoryKey, Actual, T);

        // Вывод, который человек мог бы высказать вслух.
        if (const FOutcomeAssociation* O = Memory->FindOutcome(A.Key))
        {
            if (O->Samples >= 2 && FMath::Abs(O->ExpectedValue) > 0.3f)
            {
                const FString What = A.Label.IsEmpty() ? HumanText::Action(A.Action) : A.Label;
                Memory->NameConclusion(A.Key, O->ExpectedValue > 0.0f
                    ? FString::Printf(TEXT("%s — это хорошо"), *What)
                    : FString::Printf(TEXT("%s — зря я это"), *What));
            }
        }
    }

    if (Emotions)
    {
        FAppraisedEvent Ev;
        Ev.Tag = bSkillFailed ? FName(TEXT("PlanFailed")) : FName(TEXT("Outcome"));
        Ev.Subject = A.Target;
        Ev.Location = GetOwner()->GetActorLocation();

        // Желательность — это то, что реально произошло со мной.
        Ev.Desirability = FMath::Clamp(Actual, -1.0f, 1.0f);

        // Неожиданность — расхождение с ожиданием. Чем сильнее разошлось,
        // тем ярче чувство: тут и удивление, и разочарование, и восторг.
        Ev.Unexpectedness = FMath::Clamp(FMath::Abs(PredictionError) * 1.2f, 0.0f, 1.0f);

        // Я сам это выбрал — значит, и заслуга, и вина мои.
        Ev.SelfAgency = bReached ? 0.75f : 0.5f;
        Ev.Controllability = bReached ? 0.7f : 0.3f;
        Ev.Certainty = 1.0f;
        Ev.Significance = FMath::Clamp(0.2f + FMath::Abs(Actual) * 0.7f + FMath::Abs(PredictionError) * 0.5f, 0.0f, 1.0f);

        const FString What = A.Label.IsEmpty() ? HumanText::Action(A.Action) : A.Label;
        if (PredictionError > 0.2f)
        {
            Ev.Description = FString::Printf(TEXT("%s — вышло лучше, чем я думал(а)"), *What);
        }
        else if (PredictionError < -0.2f)
        {
            Ev.Description = FString::Printf(TEXT("%s — а толку никакого"), *What);
        }
        else
        {
            Ev.Description = What;
        }

        Emotions->Appraise(Ev, Personality);

        // Награда для дофамина — это тоже ошибка предсказания, а не сам успех.
        if (PredictionError > 0.0f)
        {
            RewardSignal = FMath::Clamp(PredictionError, 0.0f, 1.0f);
        }
    }

    // --- Сожаление: а надо было выбрать другое -----------------------------
    // Человек сравнивает вышедшее с тем, что мог бы сделать вместо этого.
    if (Emotions && Deliberation && Deliberation->LastConsidered.Num() > 1 && Actual < 0.1f)
    {
        float BestAlternative = -BIG_NUMBER;
        FString AlternativeName;

        for (int32 i = 0; i < Deliberation->LastConsidered.Num(); ++i)
        {
            const FString& Label = Deliberation->LastConsideredLabels.IsValidIndex(i)
                ? Deliberation->LastConsideredLabels[i] : FString();
            if (Label == A.Label)
            {
                continue;
            }
            if (Deliberation->LastConsidered[i].Total > BestAlternative)
            {
                BestAlternative = Deliberation->LastConsidered[i].Total;
                AlternativeName = Label;
            }
        }

        const float Missed = BestAlternative - Actual;
        if (Missed > 0.25f && !AlternativeName.IsEmpty())
        {
            FAppraisedEvent Regret;
            Regret.Tag = TEXT("Regret");
            Regret.Description = FString::Printf(TEXT("надо было вместо этого: %s"), *AlternativeName);
            Regret.Desirability = -FMath::Clamp(Missed, 0.0f, 1.0f);
            Regret.SelfAgency = 1.0f;        // сам же и выбрал
            Regret.Controllability = 0.9f;   // и мог выбрать иначе
            Regret.Unexpectedness = 0.3f;
            Regret.Significance = FMath::Clamp(Missed, 0.0f, 1.0f);
            Emotions->Appraise(Regret, Personality);

            if (Missed > 0.5f)
            {
                Think(Regret.Description, EThoughtKind::Rumination, 0.7f);
            }
        }
    }

    // --- Запомнить, если задело --------------------------------------------
    if (Memory && FMath::Abs(Actual) > 0.25f)
    {
        TArray<AActor*> Participants;
        if (A.Target)
        {
            Participants.Add(A.Target);
        }

        const FString What = A.Label.IsEmpty() ? HumanText::Action(A.Action) : A.Label;
        Memory->Encode(What, FName(*HumanText::Action(A.Action)), Actual,
                       FMath::Clamp(FMath::Abs(PredictionError) + 0.15f, 0.0f, 1.0f),
                       Participants, GetOwner()->GetActorLocation(), T, Focus);
    }

    // --- Привычка ----------------------------------------------------------
    // Закрепляется то, что кончилось хорошо. Так рождается распорядок дня —
    // не из расписания, а из того, что однажды сработало.
    if (bReached && Actual > 0.15f && Memory)
    {
        if (UHumanWorldSubsystem* World = GetWorldMind())
        {
            Memory->ReinforceHabit(FName(*FString::Printf(TEXT("H%d"), World->Now.Hour)), A.Action, T);
            RoutineScore = FMath::Clamp(RoutineScore + 0.03f, 0.0f, 1.0f);
        }
    }

    // --- Самооценка --------------------------------------------------------
    if (Identity)
    {
        if (bSkillFailed)       Identity->OnFailure(0.25f, true);
        else if (Actual > 0.3f) Identity->OnSuccess(FMath::Clamp(Actual, 0.0f, 1.0f) * 0.4f);
    }

    // --- Воля --------------------------------------------------------------
    if (Motivation)
    {
        Motivation->SpendWillpower(A.EffortCost * 0.5f);
        if (Actual > 0.3f)
        {
            Motivation->RestoreWillpower(0.08f);
        }
    }

    ActiveIntention = FIntention();
    CurrentAction = EActionType::Idle;
    CurrentActionLabel = TEXT("стою");
}

void UMindComponent::ResolveConfrontation(AActor* Target)
{
    ACompleteHumanNPC* Other = Cast<ACompleteHumanNPC>(Target);
    if (!Other || !Social || !Emotions)
    {
        return;
    }

    const float T = Now();
    FRelationship& R = Social->FindOrAdd(Other, T);

    const float Anger = Emotions->GetIntensity(EEmotionType::Anger);
    const float Control = Motivation ? Motivation->GetSelfControl() : 0.5f;
    const float Restraint = Personality
        ? (Personality->Traits.Agreeableness * 0.5f + Personality->Facets.SelfControl * 0.5f)
        : 0.5f;

    // Дойдёт ли до рук — не правило, а расчёт сил, которые держат и толкают.
    const float Violence = Anger * 0.5f + R.Resentment * 0.5f - Restraint * 0.8f - Control * 0.3f - HitRegret;

    if (Violence > 0.25f && FMath::FRand() < Violence)
    {
        HitRegret = FMath::Min(1.0f, HitRegret + 0.25f);
        const float Severity = FMath::Clamp(0.2f + Violence * 0.4f, 0.1f, 0.7f);
        Other->Hurt(GetOwner(), Severity, true);

        // Осознание того, что сделал, приходит следом — через ту же оценку.
        FAppraisedEvent Ev;
        Ev.Tag = TEXT("Violence");
        Ev.Description = FString::Printf(TEXT("я ударил(а) — %s"), *NameOf(Other));
        Ev.Subject = Other;
        Ev.Desirability = -0.3f;
        Ev.SelfAgency = 1.0f;
        Ev.NormAlignment = -0.9f;
        Ev.Controllability = 0.9f;
        Ev.Significance = 0.9f;
        Emotions->Appraise(Ev, Personality);

        if (Identity)
        {
            Identity->RecordLifeEvent(
                FString::Printf(TEXT("ударил(а) человека — %s"), *NameOf(Other)), -0.8f, 0.8f, T);
        }
        Social->AdjustReputation(-0.3f);
        OwedApologyTo = Other;
        R.Resentment = FMath::Max(0.0f, R.Resentment - 0.35f);
    }
    else
    {
        Emotions->Soothe(EEmotionType::Anger, 0.3f);
        R.Resentment = FMath::Max(0.0f, R.Resentment - 0.3f);
        Think(TEXT("сказал(а) как есть. Пусть думает."), EThoughtKind::Judgement, 0.7f, Other);
    }
}

// ---------------------------------------------------------------------------
//  Разговор
// ---------------------------------------------------------------------------

bool UMindComponent::CanTalkTo(AActor* Other) const
{
    if (!Other || Other == GetOwner())
    {
        return false;
    }
    ACompleteHumanNPC* Human = GetHuman();
    ACompleteHumanNPC* Target = Cast<ACompleteHumanNPC>(Other);
    if (!Human || !Target)
    {
        return false;
    }

    // Говорят с тем, кого видят. Сквозь стену не разговаривают.
    if (!Human->HasLineOfSight(Target))
    {
        return false;
    }

    if (UMindComponent* TheirMind = Target->FindComponentByClass<UMindComponent>())
    {
        if (TheirMind->bAsleep)
        {
            return false;
        }
    }

    return FVector::Dist(Human->GetActorLocation(), Other->GetActorLocation()) < 400.0f;
}

void UMindComponent::SpeakTo(AActor* Listener)
{
    if (!Speech || !CanTalkTo(Listener))
    {
        return;
    }

    const float T = Now();
    ACompleteHumanNPC* Target = Cast<ACompleteHumanNPC>(Listener);

    // --- О чём говорить -----------------------------------------------------
    // Тема не выбирается заново к каждой реплике: человек заводит разговор
    // о том, что его сейчас занимает, и держится этого, пока не исчерпает.
    if (Speech)
    {
        if (ConversationPartner != Listener || Speech->TurnsInConversation > 8)
        {
            Speech->TurnsInConversation = 0;
            Speech->Topic = FPhrase::PickTopic(
                Needs ? Needs->GetMostUrgent() : ENeedType::SocialContact,
                Emotions ? Emotions->GetDominant() : EEmotionType::None,
                GetWorldMind() ? GetWorldMind()->Now.HourFloat : 12.0f);
        }

        Speech->Talkativeness = Personality
            ? FMath::Clamp(Personality->Traits.Extraversion, 0.0f, 1.0f)
            : 0.5f;
        Speech->bFemale = Identity && Identity->bFemale;

        // О чём он сейчас скажет. Если разговор уже идёт о чём-то — держимся
        // этого; если начинается — выбираем из того, что на уме.
        if (!CurrentTopic.IsValid() || Speech->TurnsInConversation == 0
            || FMath::FRand() < 0.3f)
        {
            CurrentTopic = ChooseTalkingPoint(Listener);
        }

        Speech->Speaking = CurrentTopic;
        Speech->Heard = HeardTopic;
    }

    // --- Собираем всё, что определяет реплику -------------------------------
    FSpeechContext Ctx;
    Ctx.Listener = Listener;
    Ctx.ListenerName = NameOf(Listener);
    Ctx.SpeakerName = Identity ? Identity->FirstName : TEXT("Я");

    if (Identity)
    {
        Ctx.SelfEsteem = Identity->SelfEsteem;
        Ctx.DreamText = Identity->LifeDream;
    }
    if (Personality)
    {
        Ctx.Extraversion = Personality->Traits.Extraversion;
        Ctx.Agreeableness = Personality->Traits.Agreeableness;
        Ctx.Honesty = Personality->Facets.Honesty;
    }
    if (Emotions)
    {
        float Intensity;
        EEmotionType Dominant;
        Emotions->GetDominantWithIntensity(Dominant, Intensity);
        Ctx.Emotion = Dominant;
        Ctx.EmotionIntensity = Intensity;
        Ctx.Mood = Emotions->GetAffect().Pleasure;
        Ctx.Stress = Emotions->GetStress();
    }
    if (Needs)
    {
        Ctx.Need = Needs->GetMostUrgent();
    }
    if (UHumanWorldSubsystem* World = GetWorldMind())
    {
        Ctx.Hour = World->Now.Hour;
    }

    if (Social)
    {
        FRelationship& R = Social->FindOrAdd(Listener, T);
        Ctx.Kind = R.Kind;
        Ctx.Closeness = Social->GetCloseness(Listener);
        Ctx.Trust = R.Trust;
        Ctx.Liking = R.Liking;
        Ctx.Resentment = R.Resentment;
        Ctx.Debt = R.Debt;
        Ctx.BelievedLikingOfMe = R.Theory.BelievedLikingOfMe;

        // Он считает меня холоднее, чем я есть. Это тревожит и толкает
        // объясниться — не словами о себе, а тем, как себя повести.
        Ctx.bHeMisreadsMe = FMath::Clamp(R.Liking - R.Theory.BelievedViewOfMyOpinion, 0.0f, 1.0f);
        if (Ctx.bHeMisreadsMe > 0.4f && FMath::FRand() < 0.25f)
        {
            Think(FString::Printf(TEXT("%s, кажется, решил(а), что я на него зол(а). А это не так."),
                                  *NameOf(Listener)),
                  EThoughtKind::Judgement, 0.7f, Listener);
        }
        Ctx.bTheyLookUpset = R.Theory.BelievedMood < -0.35f;
        Ctx.bFirstMeeting = (R.InteractionCount == 0) && !GreetedRecently.Contains(Listener);

        // Кого бы обсудить.
        AActor* GossipTarget = nullptr;
        float BestGossipScore = 0.35f;
        for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Social->Relations)
        {
            if (!Pair.Key || Pair.Key == Listener)
            {
                continue;
            }
            const float Score = FMath::Abs(Pair.Value.Liking) * Pair.Value.Familiarity;
            if (Score > BestGossipScore)
            {
                BestGossipScore = Score;
                GossipTarget = Pair.Key;
            }
        }
        if (GossipTarget)
        {
            Ctx.GossipSubject = GossipTarget;
            Ctx.GossipSubjectName = NameOf(GossipTarget);
            Ctx.GossipValence = Social->Find(GossipTarget)->Liking;
        }
    }

    Ctx.bIOweApology = (OwedApologyTo == Listener);

    // Чем поделиться.
    if (Memory)
    {
        if (const FEpisodicMemory* M = Memory->GetMostVivid())
        {
            if (M->Strength > 0.4f && FMath::Abs(M->Valence) > 0.25f)
            {
                Ctx.MemoryToShare = M->Summary;
            }
        }
        if (const FBelief* B = Memory->PickBeliefToShare())
        {
            Ctx.OpinionToShare = B->Text;
        }
    }
    if (Emotions)
    {
        Ctx.ComplaintText = Emotions->DescribeFeeling();
    }

    // --- Что сказать --------------------------------------------------------
    ESpeechAct Act = Speech->ChooseAct(Ctx);

    // Первая встреча — сначала здороваемся.
    if (Ctx.bFirstMeeting)
    {
        Act = ESpeechAct::Greet;
        GreetedRecently.AddUnique(Listener);
    }

    // Решение соврать принимается отдельно и осознанно.
    if (Act == ESpeechAct::Lie && Social)
    {
        FString Reasoning;
        const float LieSkill = Mastery(TEXT("Deception"));
        if (!Social->DecideToLie(Listener, 0.5f, LieSkill, Personality, Reasoning))
        {
            Act = ESpeechAct::Silence;
        }
        Think(Reasoning, EThoughtKind::Judgement, 0.6f, Listener);
    }

    if (Act == ESpeechAct::Silence)
    {
        return;
    }

    FUtterance U = Speech->Compose(Act, Ctx, GetOwner());

    // --- Что именно он рассказывает ----------------------------------------
    // Знание кладётся в реплику: без сказанного оно никуда не денется.
    // Довесок к реплике даётся изредка: человек, приклеивающий к каждой фразе
    // «кстати, есть тут место №34», звучит не как человек.
    if ((Act == ESpeechAct::ShareNews || Act == ESpeechAct::Advise) && FMath::FRand() < 0.35f)
    {
        if (Memory && Memory->Places.Num() > 0 && FMath::FRand() < 0.4f)
        {
            const FKnownLocation& Mine = Memory->Places[FMath::RandRange(0, Memory->Places.Num() - 1)];
            if (Mine.Familiarity > 0.4f && Mine.Kind != EPlaceKind::Home)
            {
                U.PlacePayload = Mine;
                U.bHasPlacePayload = true;
                U.Text += FString::Printf(TEXT(" Кстати, есть тут %s."), *Mine.Label);
            }
        }

        if (!U.bHasPlacePayload && FMath::FRand() < 0.3f)
        {
            float MineLevel = 0.0f;
            const FName Mine = BestMastery(MineLevel);
            if (!Mine.IsNone() && MineLevel > 0.3f)
            {
                U.SkillPayload = Mine;
                U.SkillPayloadLevel = MineLevel;
                U.Text += FString::Printf(TEXT(" Я тебе покажу, как надо: %s."),
                                          *UMindComponent::MasteryLabel(Mine));
            }
        }
    }

    // Задумать фразу и суметь её произнести — разные вещи. Изо рта выходит
    // только то, что человек знает. У ребёнка от задуманного остаются обрывки.
    U.Text = Speech->Articulate(U.Text);
    U.SpokenAt = T;
    Speech->Remember(U);

    // Речь — тоже поступок, и её должно быть видно наравне с остальным:
    // и в летописи, и над головой у говорящего.
    if (!U.Text.IsEmpty())
    {
        const FString To = Target && Target->IdentityComponent
            ? Target->IdentityComponent->FirstName : FString(TEXT("кому-то"));
        Report(FString::Printf(TEXT("— %s (%s)"), *U.Text, *To));

        if (ACompleteHumanNPC* Me = GetHuman())
        {
            Me->ShowSpeech(U.Text);
        }
    }

    // --- Слышат все, кто рядом ---------------------------------------------
    // Дети учатся языку не потому, что с ними занимаются, а потому что
    // вокруг непрерывно говорят. Каждое слово, долетевшее до ребёнка,
    // понемногу оседает — даже если сказано было не ему.
    if (UHumanWorldSubsystem* World = GetWorldMind())
    {
        const TArray<ACompleteHumanNPC*> Within = World->GetHumansNear(
            GetOwner()->GetActorLocation(), 900.0f, GetOwner());

        for (ACompleteHumanNPC* Bystander : Within)
        {
            if (!Bystander || Bystander == Listener)
            {
                continue;   // тому, кому сказано, слова достанутся отдельно
            }
            if (USpeechComponent* TheirSpeech = Bystander->SpeechComponent)
            {
                const bool bTheyAreChild = Bystander->IdentityComponent
                    && Bystander->IdentityComponent->Age < 13.0f;
                TheirSpeech->LearnWords(U.Text, 0.35f, bTheyAreChild);
            }
        }
    }

    // --- Последствия для говорящего ----------------------------------------
    if (Act == ESpeechAct::Lie && Social)
    {
        Social->LiesTold++;
        if (Emotions && Personality)
        {
            // Врать неприятно даже тем, кто умеет.
            Emotions->Trigger(EEmotionType::Guilt, Personality->Facets.Honesty * 0.3f, Listener, TEXT("соврал(а)"));
        }
    }
    if (Act == ESpeechAct::Apologize)
    {
        OwedApologyTo = nullptr;
    }
    if (Act == ESpeechAct::Insult && Emotions)
    {
        // Сказал — и сразу пожалел.
        Emotions->Trigger(EEmotionType::Shame, 0.2f, Listener, TEXT("зря я так"));
        OwedApologyTo = Listener;
    }

    // --- Доставка -----------------------------------------------------------
    if (Target)
    {
        if (UMindComponent* TheirMind = Target->FindComponentByClass<UMindComponent>())
        {
            TheirMind->Hear(U);
        }
    }

    if (Social)
    {
        Social->RecordInteraction(Listener, U.Tone * 0.5f, T, Personality);

        // Имя узнаётся при знакомстве.
        if (Target)
        {
            if (UIdentityComponent* TheirId = Target->FindComponentByClass<UIdentityComponent>())
            {
                FRelationship& R = Social->FindOrAdd(Listener, T);
                if (R.KnownName.IsEmpty() && R.InteractionCount > 0)
                {
                    R.KnownName = TheirId->FirstName;
                }
            }
        }
    }

    Think(FString::Printf(TEXT("— %s"), *U.Text), EThoughtKind::Intention, 0.7f, Listener);
}

// ---------------------------------------------------------------------------
//  УСЛОВИЯ ДЕЛА
//
//  Чтобы читать — надо держать книгу. Чтобы есть за столом — надо сесть.
//  Чтобы спать — лечь. Условия не проверяются задним числом: человек их
//  выполняет, и на это уходит время.
// ---------------------------------------------------------------------------

void UMindComponent::Report(const FString& What) const
{
    const ACompleteHumanNPC* Human = GetHuman();
    if (!Human || !Human->IdentityComponent)
    {
        return;
    }

    const UHumanWorldSubsystem* World = GetWorldMind();
    const int32 Hour = World ? World->Now.Hour : 0;
    const int32 Minute = World ? World->Now.Minute : 0;

    UE_LOG(LogHumanCity, Log, TEXT("[%02d:%02d] %s %s"),
           Hour, Minute, *Human->IdentityComponent->FirstName, *What);
}

AFurnitureActor* UMindComponent::FindStorageNear(EResourceKind What, float Radius, const FVector* Around) const
{
    const ACompleteHumanNPC* Human = GetHuman();
    if (!Human || What == EResourceKind::None)
    {
        return nullptr;
    }

    AFurnitureActor* Best = nullptr;
    float BestDist = Radius;
    const FVector Center = Around ? *Around : Human->GetActorLocation();

    for (TActorIterator<AFurnitureActor> It(Human->GetWorld()); It; ++It)
    {
        AFurnitureActor* Thing = *It;
        if (!Thing || Thing->HowMuchInside(What) < 0.5f)
        {
            continue;
        }
        const float D = FVector::Dist(Thing->GetActorLocation(), Center);
        if (D < BestDist)
        {
            BestDist = D;
            Best = Thing;
        }
    }
    return Best;
}

bool UMindComponent::HaveWhatItTakes(const FAffordance& A) const
{
    if (A.Requires == EResourceKind::None || A.RequiresAmount <= 0.0f)
    {
        return true;
    }

    const ACompleteHumanNPC* Human = GetHuman();
    if (!Human)
    {
        return false;
    }

    // В руках?
    if (const AResourceActor* Held = Cast<AResourceActor>(Human->CarriedItem.Get()))
    {
        if (Held->Kind == A.Requires && Held->Amount >= A.RequiresAmount)
        {
            return true;
        }
    }

    // В той самой вещи, у которой стоим (в холодильнике — его же продукты)?
    if (const AFurnitureActor* Thing = Cast<AFurnitureActor>(A.Target.Get()))
    {
        if (Thing->HowMuchInside(A.Requires) >= A.RequiresAmount)
        {
            return true;
        }
    }

    // Или совсем рядом — на столе, в шкафу.
    return FindStorageNear(A.Requires, 520.0f, A.bHasLocation ? &A.Location : nullptr) != nullptr;
}

bool UMindComponent::CouldDo(const FAffordance& A) const
{
    const float Purse = Identity ? Identity->Money : 0.0f;
    if (A.MoneyCost > 0.0f && A.MoneyCost > Purse + 0.01f)
    {
        return false;
    }
    if (A.RequiredSkill == TEXT("Reading") && Speech && Speech->Literacy() < 0.3f && FVillage::IsMedieval(this))
    {
        return false;
    }
    return HaveWhatItTakes(A);
}

void UMindComponent::SettleResources(const FAffordance& A)
{
    ACompleteHumanNPC* Human = GetHuman();
    if (!Human)
    {
        return;
    }
    if (SettleDeal(A))
    {
        return;
    }

    // --- Истратить ----------------------------------------------------------
    if (A.Requires != EResourceKind::None && A.RequiresAmount > 0.0f)
    {
        float Needed = A.RequiresAmount;

        if (AResourceActor* Held = Cast<AResourceActor>(Human->CarriedItem.Get()))
        {
            if (Held->Kind == A.Requires)
            {
                const float Took = Held->Consume(Needed);
                Needed -= Took;
                if (Held->IsActorBeingDestroyed() || Held->Amount <= 0.0f)
                {
                    Human->CarriedItem = nullptr;
                }
            }
        }
        if (Needed > 0.0f)
        {
            if (AFurnitureActor* Thing = Cast<AFurnitureActor>(A.Target.Get()))
            {
                Needed -= Thing->TakeOut(A.Requires, Needed);
            }
        }
        if (Needed > 0.0f)
        {
            if (AFurnitureActor* Near = FindStorageNear(A.Requires, 420.0f))
            {
                Needed -= Near->TakeOut(A.Requires, Needed);
            }
        }
    }

    // --- Получить -----------------------------------------------------------
    if (A.Produces == EResourceKind::None || A.ProducesAmount <= 0.0f)
    {
        return;
    }

    // Готовое кладут туда, где стояли: сготовил на кухне — осталось на кухне.
    AFurnitureActor* Keep = nullptr;
    if (A.Produces == EResourceKind::CookedFood || A.Produces == EResourceKind::RawFood)
    {
        Keep = FindStorageNear(EResourceKind::RawFood, 500.0f);
        if (!Keep)
        {
            // Ищем холодильник — даже пустой.
            for (TActorIterator<AFurnitureActor> It(Human->GetWorld()); It; ++It)
            {
                AFurnitureActor* Thing = *It;
                if (Thing && Thing->FurnitureType == EFurnitureType::Fridge
                    && FVector::Dist(Thing->GetActorLocation(), Human->GetActorLocation()) < 600.0f)
                {
                    Keep = Thing;
                    break;
                }
            }
        }
    }

    if (Keep)
    {
        Keep->Store(A.Produces, A.ProducesAmount);
        Think(FString::Printf(TEXT("убрал(а) %s"), *AResourceActor::KindName(A.Produces)),
              EThoughtKind::Observation, 0.35f);
    }
    else
    {
        // Некуда убрать — несём в руках.
        const FVector Drop = Human->GetActorLocation() + Human->GetActorForwardVector() * 70.0f
                           + FVector(0.0f, 0.0f, 40.0f);
        if (AResourceActor* Made = AResourceActor::Spawn(Human->GetWorld(), A.Produces, A.ProducesAmount, Drop))
        {
            if (!Human->CarriedItem && !Human->AreHandsBusy())
            {
                Human->BeginTake(Made);
            }
        }
    }
}

void UMindComponent::RefreshIntentionFromSurroundings()
{
    ACompleteHumanNPC* Human = GetHuman();
    UHumanWorldSubsystem* World = GetWorldMind();
    if (!Human || !World || !ActiveIntention.bValid)
    {
        return;
    }

    const FAffordance& Was = ActiveIntention.Affordance;

    // Если вещь известна и она на месте — ничего уточнять не надо.
    if (::IsValid(Was.Target.Get()))
    {
        return;
    }

    // Смотрим широко: придя в школу, человек видит не только то, что под
    // носом, — он видит кабинет и полку в его глубине.
    TArray<FAffordance> Here;
    World->CollectOffersNear(Human->GetActorLocation(), 2600.0f, Here);

    const FAffordance* Visible = nullptr;
    const FAffordance* Hidden = nullptr;
    float VisibleDist = BIG_NUMBER;
    float HiddenDist = BIG_NUMBER;

    for (const FAffordance& Offer : Here)
    {
        if (Offer.Action != Was.Action)
        {
            continue;
        }
        // Своё занятие узнаётся по названию: шёл читать азбуку — берёт азбуку,
        // а не первую попавшуюся книгу.
        if (!Was.Label.IsEmpty() && !Offer.Label.IsEmpty()
            && Offer.Label != Was.Label && Offer.CategoryKey != Was.CategoryKey)
        {
            continue;
        }

        const float D = FVector::Dist(Offer.Location, Human->GetActorLocation());

        // Через стену человек ничего не видит. Но он знает, что внутри
        // что-то есть, — и может войти и поискать.
        if (Human->CanReachPoint(Offer.Location))
        {
            if (D < VisibleDist) { VisibleDist = D; Visible = &Offer; }
        }
        else if (D < HiddenDist)
        {
            HiddenDist = D;
            Hidden = &Offer;
        }
    }

    const FAffordance* Best = Visible ? Visible : Hidden;
    const float BestDist = Visible ? VisibleDist : HiddenDist;
    const bool bThroughWall = (Visible == nullptr);

    if (!Best)
    {
        return;
    }

    // Замысел остаётся тем же, но теперь он про вещь, которая вот она.
    const FString Reason = ActiveIntention.Reason;
    ActiveIntention.Affordance = *Best;
    ActiveIntention.Reason = Reason;

    CurrentActionLabel = Best->Label.IsEmpty() ? HumanText::Action(Best->Action) : Best->Label;
    ActionTotalDuration = FMath::Max(Best->Duration, 1.0f);
    ActionTimeLeft = Best->Duration;

    // Увидел издали — значит, надо ещё дойти. Пришёл в школу, а полка
    // с учебниками в глубине кабинета: обычное дело.
    if (BestDist > 230.0f || bThroughWall)
    {
        MoveDestination = Best->Location;
        Human->RequestMoveTo(MoveDestination);
        bMoving = true;

        // Войти в незнакомое здание и найти там нужное — дело небыстрое:
        // сперва ищешь дверь, потом бродишь по коридору. Даём на это время,
        // иначе человек бросает затею у самого порога.
        MoveTimeout = EstimateTravelBudget(BestDist * 2.0f + (bThroughWall ? 9000.0f : 1500.0f));
        Phase = EActionPhase::Travelling;

        Report(bThroughWall
            ? FString::Printf(TEXT("это где-то внутри — ищу вход: %s"), *CurrentActionLabel)
            : FString::Printf(TEXT("увидел(а) отсюда: %s — иду ближе"), *CurrentActionLabel));
    }
}

bool UMindComponent::PrepareForAction()
{
    ACompleteHumanNPC* Human = GetHuman();
    if (!Human)
    {
        return false;
    }

    const FAffordance& A = ActiveIntention.Affordance;

    // --- Есть ли из чего -----------------------------------------------------
    // Пришёл готовить, а продуктов нет. Такое бывает у всякого, и это не
    // сбой замысла, а обычная досада.
    if (!HaveWhatItTakes(A))
    {
        Report(FString::Printf(TEXT("не из чего: нужны %s"),
               *AResourceActor::KindName(A.Requires)));
        Think(FString::Printf(TEXT("а %s-то и нет"),
              *AResourceActor::KindName(A.Requires)), EThoughtKind::Judgement, 0.55f);
        return false;
    }

    // --- Взять в руки -------------------------------------------------------
    if (A.bNeedsInHand)
    {
        AActor* Item = A.Target.Get();

        // Человек шёл по памяти: «там была книга». Придя, он не держит в уме
        // саму вещь — он оглядывается и ищет её глазами.
        if (!Item)
        {
            float BestDist = 420.0f;
            for (TActorIterator<ABookActor> It(Human->GetWorld()); It; ++It)
            {
                ABookActor* Book = *It;
                if (!Book || !Book->IsAvailable())
                {
                    continue;
                }
                const float D = FVector::Dist(Book->GetActorLocation(), Human->GetActorLocation());
                if (D < BestDist)
                {
                    BestDist = D;
                    Item = Book;
                }
            }
            if (!Item)
            {
                for (TActorIterator<AResourceActor> It(Human->GetWorld()); It; ++It)
                {
                    AResourceActor* Thing = *It;
                    if (!Thing || !Thing->IsAvailable())
                    {
                        continue;
                    }
                    const float D = FVector::Dist(Thing->GetActorLocation(), Human->GetActorLocation());
                    if (D < BestDist)
                    {
                        BestDist = D;
                        Item = Thing;
                    }
                }
            }
        }

        if (!Item)
        {
            Report(TEXT("поискал(а) глазами — ничего подходящего рядом нет"));
            return false;
        }

        if (!Human->IsHolding(Item))
        {
            if (ABookActor* Book = Cast<ABookActor>(Item))
            {
                // Чужую книгу из рук не выхватывают.
                if (!Book->IsAvailable())
                {
                    Report(TEXT("книгу уже взяли — не досталась"));
                    Think(TEXT("книгу уже взяли"), EThoughtKind::Observation, 0.45f);
                    return false;
                }
            }

            if (Human->Motor && Human->Motor->IsTaking(Item))
            {
                PrepareTimeLeft = FMath::Max(PrepareTimeLeft, 1.0f);
                return true;
            }
            if (Human->Motor && Human->Motor->TakeFailed(Item))
            {
                Report(FString::Printf(TEXT("не взял(а) в руки: %s"), *Human->Motor->DescribeHands()));
                Think(TEXT("не получилось взять"), EThoughtKind::Judgement, 0.4f);
                return false;
            }
            if (Human->AreHandsBusy())
            {
                PrepareTimeLeft = FMath::Max(PrepareTimeLeft, 1.0f);
                return true;
            }
            if (!Human->BeginTake(Item))
            {
                Report(TEXT("не смог(ла) взять в руки"));
                return false;
            }
            if (!Human->IsHolding(Item))
            {
                Report(FString::Printf(TEXT("тянется за вещью: %s"), A.Label.IsEmpty() ? TEXT("вещь") : *A.Label));
                PrepareTimeLeft = FMath::Max(PrepareTimeLeft, 1.0f);
                return true;
            }
        }
        if (Human->IsHolding(Item) && Human->AreHandsBusy())
        {
            PrepareTimeLeft = FMath::Max(PrepareTimeLeft, 1.0f);
            return true;
        }
        if (Human->IsHolding(Item) && ReportedGrasp.Get() != Item)
        {
            ReportedGrasp = Item;
            Report(FString::Printf(TEXT("взял(а) в руки: %s"), A.Label.IsEmpty() ? TEXT("вещь") : *A.Label));
        }
    }

    // --- Свет ---------------------------------------------------------------
    if (A.bNeedsLight)
    {
        const float Hour = GetWorldMind() ? GetWorldMind()->Now.HourFloat : 12.0f;
        if (Hour < 6.0f || Hour > 21.5f)
        {
            Report(TEXT("темно — не разглядеть"));
            Think(TEXT("темно, ничего не разобрать"), EThoughtKind::Judgement, 0.5f);
            return false;
        }
    }

    // --- Сесть или лечь -----------------------------------------------------
    bool bWantLying = A.bNeedsLying && Human->Posture != EPosture::Lying;
    bool bWantSeat = A.bNeedsSeat && Human->Posture != EPosture::Sitting;
    const bool bGroundIsFine = A.bNeedsLying && A.Action == EActionType::Observe;

    auto Supports = [bWantLying](EFurnitureType Type)
    {
        return bWantLying
            ? (Type == EFurnitureType::Bed || Type == EFurnitureType::Sofa || Type == EFurnitureType::Bath)
            : (Type == EFurnitureType::Chair || Type == EFurnitureType::Sofa || Type == EFurnitureType::SchoolDesk
               || Type == EFurnitureType::Toilet || Type == EFurnitureType::Bed);
    };

    const AFurnitureActor* Anchor = Cast<AFurnitureActor>(A.Target.Get());
    const AFurnitureActor* Furniture = nullptr;

    if (bWantLying || bWantSeat)
    {
        if (Anchor && Supports(Anchor->FurnitureType))
        {
            Furniture = Anchor;
        }
        else if (PlannedSeat.IsValid()
                 && FVector::Dist2D(PlannedSeat->GetActorLocation(), Human->GetActorLocation()) < 400.0f)
        {
            Furniture = PlannedSeat.Get();
        }
        else if (!bGroundIsFine)
        {
            const FVector Around = Anchor ? Anchor->GetActorLocation() : Human->GetActorLocation();
            AFurnitureActor* Best = nullptr;
            float BestDist = Anchor ? 260.0f : 700.0f;
            for (TActorIterator<AFurnitureActor> It(Human->GetWorld()); It; ++It)
            {
                const EFurnitureType Type = It->FurnitureType;
                if (!Supports(Type) || Type == EFurnitureType::Toilet || Type == EFurnitureType::Bath)
                {
                    continue;
                }
                const FVector Where = It->GetActorLocation();
                const float D = FVector::Dist2D(Where, Around);
                if (D < BestDist && FMath::Abs(Where.Z - Human->GetActorLocation().Z) < 250.0f && Human->CanReachPoint(Where))
                {
                    BestDist = D;
                    Best = *It;
                }
            }

            if (Best)
            {
                PlannedSeat = Best;
                const float Walk = FVector::Dist2D(Best->GetActorLocation(), Human->GetActorLocation());
                if (Walk > 140.0f)
                {
                    MoveDestination = Best->GetActorLocation();
                    Human->RequestMoveTo(MoveDestination);
                    bMoving = true;
                    MoveTimeout = EstimateTravelBudget(Walk * 2.0f + 600.0f);
                    Phase = EActionPhase::Travelling;
                    Report(bWantLying ? TEXT("иду туда, где можно прилечь") : TEXT("иду к стулу"));
                    return true;
                }
                Furniture = Best;
            }
        }

        if (!Furniture && !bGroundIsFine)
        {
            bWantLying = false;
            bWantSeat = false;
        }
    }

    if (bWantLying)
    {
        FVector Spot = Human->GetActorLocation();
        float Yaw = Human->GetActorRotation().Yaw;
        if (Furniture)
        {
            Spot = Furniture->GetActorLocation();
            Yaw = Furniture->GetActorRotation().Yaw;
        }
        Human->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
        Human->SetPosture(EPosture::Lying, Spot);
        Report(TEXT("лёг(ла)"));
        PrepareTimeLeft = FMath::Max(PrepareTimeLeft, 5.0f);
    }
    else if (bWantSeat)
    {
        FVector Seat = Human->GetActorLocation();
        float Yaw = Human->GetActorRotation().Yaw;

        if (Furniture)
        {
            const FVector Base = Furniture->GetActorLocation();
            const float BaseYaw = Furniture->GetActorRotation().Yaw;
            switch (Furniture->FurnitureType)
            {
            case EFurnitureType::Chair:
            case EFurnitureType::Toilet:
                Seat = Base;
                Yaw = BaseYaw;
                break;
            case EFurnitureType::Sofa:
                Seat = Base + FRotator(0.0f, BaseYaw, 0.0f).RotateVector(FVector(0.0f, 10.0f, 0.0f));
                Yaw = BaseYaw + 90.0f;
                break;
            case EFurnitureType::SchoolDesk:
                Seat = Base + FRotator(0.0f, BaseYaw, 0.0f).RotateVector(FVector(38.0f, 0.0f, 0.0f));
                Yaw = BaseYaw + 180.0f;
                break;
            case EFurnitureType::Bed:
                Seat = Base + FRotator(0.0f, BaseYaw, 0.0f).RotateVector(FVector(0.0f, 60.0f, 0.0f));
                Yaw = BaseYaw + 90.0f;
                break;
            default:
            {
                AFurnitureActor* BestChair = nullptr;
                float BestDist = 240.0f;
                for (TActorIterator<AFurnitureActor> It(Human->GetWorld()); It; ++It)
                {
                    if (It->FurnitureType != EFurnitureType::Chair)
                    {
                        continue;
                    }
                    const float D = FVector::Dist2D(It->GetActorLocation(), Base);
                    if (D < BestDist)
                    {
                        BestDist = D;
                        BestChair = *It;
                    }
                }
                if (BestChair)
                {
                    Seat = BestChair->GetActorLocation();
                }
                else
                {
                    FVector Away = (Human->GetActorLocation() - Base).GetSafeNormal2D();
                    if (Away.IsNearlyZero())
                    {
                        Away = -Human->GetActorForwardVector();
                    }
                    const FVector Extent = Furniture->GetFootprint();
                    Seat = Base + Away * (FMath::Max(Extent.X, Extent.Y) * 0.5f + 35.0f);
                }
                Yaw = (Base - Seat).Rotation().Yaw;
                break;
            }
            }
        }

        Human->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
        Human->SetPosture(EPosture::Sitting, Seat);
        Report(TEXT("сел(а)"));
        PrepareTimeLeft = FMath::Max(PrepareTimeLeft, 3.0f);
    }

    return true;
}

void UMindComponent::FinishAction()
{
    ACompleteHumanNPC* Human = GetHuman();
    if (!Human)
    {
        return;
    }

    ReportedGrasp = nullptr;
    const float Tidy = Personality ? Personality->Traits.Conscientiousness : 0.5f;
    const bool bTidily = FMath::FRand() < Tidy * 0.9f + 0.1f;
    if (Human->Motor)
    {
        Human->Motor->CancelTake(bTidily);
    }
    const AResourceActor* Cargo = Cast<AResourceActor>(Human->CarriedItem.Get());
    if (Human->CarriedItem && !Human->AreHandsBusy() && !(Cargo && Cargo->bCargo))
    {
        Human->ReleaseFromHands(bTidily);
        Report(bTidily ? TEXT("кладёт вещь на место")
                       : TEXT("кладёт вещь где стоит"));
    }

    if (Human->Posture != EPosture::Standing)
    {
        Human->SetPosture(EPosture::Standing, FVector::ZeroVector);
        Report(TEXT("встал(а)"));
    }

    Phase = EActionPhase::None;
    PrepareTimeLeft = 0.0f;
}

// ---------------------------------------------------------------------------
//  ЧТЕНИЕ УЧЕБНИКА
//
//  До сих пор человек в этом городе знал только то, что пережил сам.
//  Учебник — первая возможность узнать чужое: страницу написал кто-то
//  другой, и в ней сказано то, чего читающий никогда не видел.
//
//  Но взять оттуда он может не всё, а лишь то, что сумел прочесть.
//  Неграмотный смотрит в книгу и видит значки; выучившийся буквам
//  разбирает простое; и только беглое чтение открывает трудные страницы.
// ---------------------------------------------------------------------------

void UMindComponent::StudyTextbook(AActor* What)
{
    // Читать можно только то, что держишь в руках.
    FName Subject;
    if (const ABookActor* Held = Cast<ABookActor>(What))
    {
        const ACompleteHumanNPC* Human = GetHuman();
        if (!Human || !Human->IsHolding(Held))
        {
            return;
        }
        Subject = Held->Subject;
    }
    else if (const AFurnitureActor* Shelf = Cast<AFurnitureActor>(What))
    {
        if (Shelf->FurnitureType != EFurnitureType::Textbook)
        {
            return;
        }
        Subject = Shelf->Subject;
    }
    else
    {
        return;
    }

    const FTextbook* Book = FLibrary::Find(Subject);
    if (!Book || Book->Pages.Num() == 0)
    {
        return;
    }

    const float T = Now();
    const float Reading = Mastery(TEXT("Reading"));
    const float Lit = Speech ? Speech->Literacy() : 1.0f;
    const float Impairment = Body ? Body->GetCognitiveImpairment() : 0.0f;

    const float WordsPerMinute = FMath::Max(3.0f,
        (8.0f + Lit * Lit * (17.0f + Reading * 145.0f)) * (1.0f - Impairment * 0.5f));

    const float MinutesSpent = FMath::Max(1.0f, ActionTotalDuration / 60.0f);
    int32 WordsLeft = FMath::RoundToInt(WordsPerMinute * MinutesSpent);

    const float Openness = Personality ? Personality->Traits.Openness : 0.5f;
    const float Awake = 1.0f - (Needs ? Needs->GetUrgency(ENeedType::Sleep) * 0.5f : 0.0f);
    const float Attention = FMath::Clamp(Awake * (0.55f + Openness * 0.45f), 0.1f, 1.0f);
    const bool bChild = Identity && Identity->Age < 13.0f;

    bool bPictureBook = false;
    for (const FTextbookPage& Any : Book->Pages)
    {
        if (Any.Pictures.Num() > 0)
        {
            bPictureBook = true;
            break;
        }
    }

    FReadingBookmark& Mark = BookmarkFor(Subject);
    if (Mark.bFinished || Mark.Page >= Book->Pages.Num())
    {
        Mark.Page = 0;
        Mark.Word = 0;
    }
    Mark.Page = FMath::Clamp(Mark.Page, 0, Book->Pages.Num() - 1);
    Mark.Word = FMath::Max(0, Mark.Word);

    const TArray<FString> NoPictures;

    int32 WordsRead = 0;
    int32 LegibleTotal = 0;
    int32 ParagraphsFinished = 0;
    float BestDepth = 0.0f;
    FString FirstLine;
    bool bStuck = false;

    while (WordsLeft > 0 && Mark.Page < Book->Pages.Num())
    {
        const FTextbookPage& Page = Book->Pages[Mark.Page];
        const int32 PageWords = FLibrary::WordCount(Page);
        if (Mark.Word >= PageWords)
        {
            Mark.Word = 0;
        }

        const float Slowdown = 1.0f + Page.Difficulty * 1.3f;
        const int32 CanRead = FMath::Max(1, FMath::FloorToInt(WordsLeft / Slowdown));
        const int32 TakeNow = FMath::Min(CanRead, PageWords - Mark.Word);
        if (TakeNow <= 0)
        {
            break;
        }

        const bool bPageStart = Mark.Word == 0;
        const FString Chunk = FLibrary::Excerpt(Page, Mark.Word, TakeNow);
        if (FirstLine.IsEmpty() && !Chunk.IsEmpty())
        {
            FirstLine = Chunk;
        }

        int32 Legible = 0;
        if (Speech && !Chunk.IsEmpty())
        {
            Legible = Speech->ReadText(Chunk, bPageStart ? Page.Pictures : NoPictures,
                                       Page.Title, Attention, bChild);
        }

        LastRead = Chunk;
        WordsRead += TakeNow;
        LegibleTotal += Legible;
        WordsLeft -= FMath::RoundToInt(TakeNow * Slowdown);
        Mark.Word += TakeNow;

        if (Legible > 0)
        {
            Practised(TEXT("Reading"), Legible * 0.002f);
        }

        if (Mark.Word < PageWords)
        {
            break;
        }

        Mark.Word = 0;
        ++ParagraphsFinished;

        const float Grasp = Speech ? Speech->Comprehension(Page.Text) * 1.4f - 0.35f : 1.0f;

        if (Grasp <= 0.0f)
        {
            if (Page.Pictures.Num() > 0)
            {
                ++Mark.Page;
                continue;
            }
            bStuck = true;
            break;
        }

        const float Depth = FMath::Clamp(Grasp, 0.1f, 1.0f);
        BestDepth = FMath::Max(BestDepth, Depth);
        ++Mark.Understood;

        const bool bHandBook = Subject != TEXT("Village") && Subject.ToString().StartsWith(TEXT("Village"));
        if (bHandBook)
        {
            const float Had = Mastery(Book->Skill);
            if (Had < 0.6f)
            {
                Practised(Book->Skill, FMath::Min(0.09f * Depth, (0.6f - Had) / FMath::Max(0.01f, 1.0f - Had)));
            }
        }
        else
        {
            Practised(Book->Skill, 0.09f * Depth);
        }
        if (!Book->SecondSkill.IsNone())
        {
            Practised(Book->SecondSkill, 0.04f * Depth);
        }

        if (Memory)
        {
            FBelief Learned;
            Learned.Subject = Subject;
            Learned.Predicate = TEXT("Studied");
            Learned.Value = 1.0f;
            Learned.Confidence = FMath::Clamp(
                float(Mark.Page + 1) / float(Book->Pages.Num()), 0.0f, 1.0f);
            Learned.LearnedAt = T;
            Learned.bVerified = false;
            Learned.Text = Page.Text;
            Memory->Learn(Learned, 0.85f, Openness, T);

            Memory->NameConclusion(FName(*FString::Printf(TEXT("%s:%s"),
                                   *Book->Title, *Page.Title)), Page.Text);
        }

        const float BookCeiling = bHandBook ? 0.6f : 1.0f;
        if (Memory && !Page.Craft.IsNone() && RecallOfDeed(Page.Craft) < BookCeiling)
        {
            FBelief HowTo;
            HowTo.Subject = Page.Craft;
            HowTo.Predicate = TEXT("HowTo");
            HowTo.Value = 1.0f;
            HowTo.Confidence = FMath::Clamp(Depth * BookCeiling, 0.2f, BookCeiling);
            HowTo.LearnedAt = T;
            HowTo.Text = Page.Text;
            Memory->Learn(HowTo, 0.9f, Openness, T);

            if (const FCraft* Deed = FCraftBook::Find(Page.Craft))
            {
                Report(FString::Printf(TEXT("запомнил(а) из книги, как %s"), *Deed->Label));
                Think(FString::Printf(TEXT("значит, так и %s"), *Deed->Label),
                      EThoughtKind::Judgement, 0.75f);
            }
        }

        Report(FString::Printf(TEXT("прочитал(а) и понял(а): %s"), *Page.Title));
        ++Mark.Page;
    }

    const float LitNow = Speech ? Speech->Literacy() : 1.0f;

    if (Mark.Page >= Book->Pages.Num())
    {
        if (bPictureBook && LitNow < 0.95f)
        {
            Mark.Page = 0;
            Mark.Word = 0;
        }
        else if (!Mark.bFinished)
        {
            Mark.bFinished = true;
            Report(FString::Printf(TEXT("дочитал(а) «%s» до конца"), *Book->Title));
            Think(FString::Printf(TEXT("осилил(а) «%s». Теперь я это знаю."), *Book->Title),
                  EThoughtKind::Judgement, 0.95f);
        }
    }

    if (WordsRead == 0)
    {
        return;
    }

    if (LegibleTotal == 0)
    {
        Report(TEXT("смотрит в книгу и видит одни значки"));
        Think(TEXT("значки. Не разберу, что тут написано"), EThoughtKind::Judgement, 0.5f);
        if (Emotions && Personality)
        {
            FAppraisedEvent Ev;
            Ev.Desirability = -0.25f;
            Ev.Controllability = 0.25f;
            Ev.SelfAgency = 0.5f;
            Ev.Significance = 0.3f;
            Emotions->Appraise(Ev, Personality);
        }
        return;
    }

    const FTextbookPage& Where = Book->Pages[FMath::Clamp(Mark.Page, 0, Book->Pages.Num() - 1)];

    Report(FString::Printf(TEXT("читал(а) «%s», разобрал(а) %d слов из %d (%s): %s"),
           *Book->Title, LegibleTotal, WordsRead, *Where.Title, *FirstLine.Left(90)));

    Think(FString::Printf(TEXT("читаю: %s"), *FirstLine.Left(110)),
          EThoughtKind::Judgement, 0.8f);

    if (bStuck)
    {
        Report(FString::Printf(TEXT("прочитал(а) «%s» — и не понял(а) ни слова"), *Where.Title));
        if (Emotions && Personality)
        {
            FAppraisedEvent Ev;
            Ev.Desirability = -0.35f;
            Ev.Controllability = 0.3f;
            Ev.SelfAgency = 0.6f;
            Ev.Significance = 0.4f;
            Emotions->Appraise(Ev, Personality);
        }
    }

    if (ParagraphsFinished > 0 && BestDepth > 0.0f && Emotions && Personality)
    {
        FAppraisedEvent Ev;
        Ev.Desirability = 0.35f + BestDepth * 0.35f;
        Ev.Unexpectedness = 0.4f;
        Ev.Controllability = 0.7f;
        Ev.SelfAgency = 0.85f;
        Ev.Significance = 0.5f + BestDepth * 0.2f;
        Emotions->Appraise(Ev, Personality);
    }
}

// ---------------------------------------------------------------------------
//  КАК ПОЯВЛЯЕТСЯ ВОЖАК
//
//  Его не назначают. Человек просто видит: этот помог соседу, хотя мог
//  пройти мимо; этот знает то, чего не знаю я; с этим спокойно. И начинает
//  с ним считаться. Когда так думает не один, а многие, — вот и получился
//  тот, к кому идут.
//
//  Уважение растёт только от увиденного и услышанного. Никакого «он лидер,
//  поэтому его уважают» здесь нет и быть не может.
// ---------------------------------------------------------------------------

void UMindComponent::JudgeByDeeds(AActor* Whom, float Delta, const FString& Why)
{
    if (!Social || !Whom || Whom == GetOwner() || FMath::IsNearlyZero(Delta))
    {
        return;
    }

    FRelationship& R = Social->FindOrAdd(Whom, Now());

    const float Before = R.Respect;
    R.Respect = FMath::Clamp(R.Respect + Delta, 0.0f, 1.0f);

    // Уважение тянет за собой доверие, но слабее: уважать и доверять — разное.
    if (Delta > 0.0f)
    {
        R.Trust = FMath::Clamp(R.Trust + Delta * 0.4f, 0.0f, 1.0f);
        R.Liking = FMath::Clamp(R.Liking + Delta * 0.25f, -1.0f, 1.0f);
    }
    else
    {
        R.Trust = FMath::Clamp(R.Trust + Delta * 0.6f, 0.0f, 1.0f);
    }

    // Переход через порог человек замечает — и проговаривает.
    if (Before < 0.62f && R.Respect >= 0.62f && !Why.IsEmpty())
    {
        Think(FString::Printf(TEXT("%s — %s. С таким считаются"), *NameOf(Whom), *Why),
              EThoughtKind::Judgement, 0.75f, Whom);
        Report(FString::Printf(TEXT("зауважал(а) %s: %s"), *NameOf(Whom), *Why));
    }
    else if (Before > 0.35f && R.Respect <= 0.35f && !Why.IsEmpty())
    {
        Think(FString::Printf(TEXT("%s — %s. Разочаровал(а)"), *NameOf(Whom), *Why),
              EThoughtKind::Judgement, 0.7f, Whom);
    }
}

void UMindComponent::UpdateStanding()
{
    if (!Identity)
    {
        return;
    }

    // Насколько со мной считаются — это не моё мнение о себе, а сумма
    // чужих. Поэтому спрашиваем у всех, кто меня знает.
    UHumanWorldSubsystem* World = GetWorldMind();
    if (!World)
    {
        return;
    }

    float Sum = 0.0f;

    for (ACompleteHumanNPC* Other : World->GetAllHumans())
    {
        if (!Other || Other == GetOwner() || !Other->SocialComponent)
        {
            continue;
        }
        const FRelationship* R = Other->SocialComponent->Find(GetOwner());
        if (!R)
        {
            continue;
        }
        // Считается только то, что сверх обычного: уважение, которое
        // заслужено поступками, и доверие, которое кто-то уже проверил.
        Sum += FMath::Clamp((R->Respect - 0.35f) / 0.45f, 0.0f, 1.0f) * 0.7f
             + FMath::Clamp((R->Trust - 0.45f) / 0.45f, 0.0f, 1.0f) * 0.3f;
    }

    // Положение — это и сила уважения, и число уважающих: полгорода,
    // уважающего всерьёз, — это предел.
    const float Enough = FMath::Max(6.0f, World->GetPopulation() * 0.5f);
    Identity->Standing = FMath::Clamp(Sum / Enough, 0.0f, 1.0f);

    // Человек чувствует, как к нему относятся, — и это меняет его самого.
    if (Identity->Standing > 0.6f)
    {
        Identity->SelfEsteem = FMath::Clamp(Identity->SelfEsteem + 0.0006f, 0.0f, 1.0f);
    }
}

// ---------------------------------------------------------------------------
//  О ЧЁМ ГОВОРИТЬ
//
//  Человек не выдумывает тему — она у него уже есть. Он голоден, он утром
//  был на рынке, вчера ему помог сосед, в книге он вычитал, что дважды два
//  четыре, и он до сих пор считает, что зря туда ходил. Всё это и есть то,
//  что он скажет, если рядом окажется кому.
// ---------------------------------------------------------------------------

void UMindComponent::GatherTalkingPoints(TArray<FTalkingPoint>& Out) const
{
    const ACompleteHumanNPC* Human = GetHuman();
    if (!Human)
    {
        return;
    }

    auto Add = [&Out](ETalkKind Kind, const FString& About, const FString& Detail,
                      float Valence, float Weight)
    {
        if (About.IsEmpty())
        {
            return;
        }
        FTalkingPoint P;
        P.Kind = Kind;
        P.About = About;
        P.Detail = Detail;
        P.Valence = Valence;
        P.Weight = Weight;
        Out.Add(P);
    };

    // --- Что мучает прямо сейчас -------------------------------------------
    if (Needs)
    {
        const ENeedType Worst = Needs->GetMostUrgent();
        const float Urgency = Needs->GetUrgency(Worst);
        if (Urgency > 0.35f)
        {
            Add(ETalkKind::Need, HumanText::Need(Worst), HumanText::NeedDesire(Worst),
                -Urgency, 0.35f + Urgency * 0.6f);
        }
    }

    // --- Чем занимался ------------------------------------------------------
    // О самом разговоре не рассказывают: «я занят разговором с тобой» —
    // не то, что говорят живые люди.
    if (!CurrentActionLabel.IsEmpty() && bActionActive
        && !CurrentActionLabel.StartsWith(TEXT("разговор"))
        && !CurrentActionLabel.StartsWith(TEXT("поговорить")))
    {
        FString Doing = CurrentActionLabel;
        int32 Hash = INDEX_NONE;
        if (Doing.FindChar(TEXT('№'), Hash) && Hash > 0)
        {
            Doing = Doing.Left(Hash).TrimEnd();
        }
        Add(ETalkKind::Deed, Doing, FString(), 0.1f, 0.35f);
    }

    // --- Что вычитал --------------------------------------------------------
    for (const FReadingBookmark& Mark : Bookmarks)
    {
        if (Mark.Understood <= 0)
        {
            continue;
        }
        const FTextbook* Book = FLibrary::Find(Mark.Subject);
        if (!Book || Book->Pages.Num() == 0)
        {
            continue;
        }
        const int32 Page = FMath::Clamp(Mark.Page - 1, 0, Book->Pages.Num() - 1);

        // Прочитанным делятся охотно: это то, чего собеседник может не знать.
        Add(ETalkKind::Book, Book->Title, Book->Pages[Page].Text, 0.4f,
            0.5f + FMath::Min(0.3f, Mark.Understood * 0.05f));
    }

    // --- Что нажил своим умом ----------------------------------------------
    if (Memory)
    {
        // Мнений у человека много, но в разговоре он высказывает то, в чём
        // уверен твёрже всего, а не всё подряд.
        {
            const FOutcomeAssociation* Firmest = nullptr;
            for (const FOutcomeAssociation& O : Memory->Outcomes)
            {
                if (O.Conclusion.IsEmpty() || O.Samples < 2)
                {
                    continue;
                }
                if (!Firmest || O.Confidence > Firmest->Confidence)
                {
                    Firmest = &O;
                }
            }
            if (Firmest)
            {
                Add(ETalkKind::Opinion, Firmest->Key.ToString(), Firmest->Conclusion,
                    FMath::Clamp(Firmest->ExpectedValue, -1.0f, 1.0f),
                    0.3f + Firmest->Confidence * 0.35f);
            }
        }

        // --- Где бывал ------------------------------------------------------
        // Мест человек знает десятки, и если выложить все, он только о них
        // и будет говорить. В разговоре всплывают одно-два — те, что сейчас
        // почему-то вспомнились.
        {
            TArray<const FKnownLocation*> Worth;
            for (const FKnownLocation& Place : Memory->Places)
            {
                if (Place.Familiarity >= 0.35f && !Place.Label.IsEmpty())
                {
                    Worth.Add(&Place);
                }
            }
            for (int32 i = 0; i < 2 && Worth.Num() > 0; ++i)
            {
                const int32 Index = FMath::RandRange(0, Worth.Num() - 1);
                const FKnownLocation* Place = Worth[Index];
                Worth.RemoveAtSwap(Index);

                // В речи номеров не бывает: человек говорит «кафе», а не
                // «кафе №14». Номер нужен памяти, чтобы различать места.
                FString Spoken = Place->Label;
                int32 Hash = INDEX_NONE;
                if (Spoken.FindChar(TEXT('№'), Hash) && Hash > 0)
                {
                    Spoken = Spoken.Left(Hash).TrimEnd();
                }

                Add(ETalkKind::Place, Spoken, FString(),
                    FMath::Clamp(Place->Affect, -1.0f, 1.0f),
                    0.16f + Place->Familiarity * 0.16f);
            }
        }
    }

    // --- Чему научился ------------------------------------------------------
    {
        float BestLevel = 0.0f;
        const FName Best = BestMastery(BestLevel);
        if (!Best.IsNone() && BestLevel > 0.3f)
        {
            Add(ETalkKind::Skill, UMindComponent::MasteryLabel(Best), FString(),
                0.5f, 0.3f + BestLevel * 0.3f);
        }
    }

    // --- Кто рядом и что о нём думается ------------------------------------
    if (Social)
    {
        for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Social->Relations)
        {
            const ACompleteHumanNPC* Other = Cast<ACompleteHumanNPC>(Pair.Key.Get());
            if (!Other || !Other->IdentityComponent || Pair.Value.Familiarity < 0.3f)
            {
                continue;
            }
            const float Liking = Pair.Value.Liking;
            if (FMath::Abs(Liking) < 0.25f)
            {
                continue;
            }
            Add(ETalkKind::Person, Other->IdentityComponent->FirstName, FString(),
                Liking, 0.2f + FMath::Abs(Liking) * 0.3f);
        }
    }

    // --- Что на душе --------------------------------------------------------
    if (Emotions)
    {
        EEmotionType Felt = EEmotionType::None;
        float Strength = 0.0f;
        Emotions->GetDominantWithIntensity(Felt, Strength);

        if (Felt != EEmotionType::None && Strength > 0.35f)
        {
            Add(ETalkKind::Feeling, HumanText::Emotion(Felt),
                HumanText::EmotionFirstPerson(Felt),
                Emotions->GetAffect().Pleasure, 0.25f + Strength * 0.45f);
        }
    }

    // --- О чём мечтает ------------------------------------------------------
    if (Identity && !Identity->LifeDream.IsEmpty())
    {
        Add(ETalkKind::Dream, Identity->LifeDream, FString(), 0.6f, 0.22f);
    }

    // --- Работа и деньги ----------------------------------------------------
    if (Identity)
    {
        if (Identity->bEmployed && !Identity->Occupation.IsEmpty())
        {
            Add(ETalkKind::Work, Identity->Occupation, FString(), 0.1f, 0.25f);
        }
        else if (Identity->Age >= 18.0f && Identity->Age < 70.0f)
        {
            Add(ETalkKind::Trouble, TEXT("работа"), TEXT("никак не найду, куда пристроиться"),
                -0.5f, 0.4f);
        }

        if (Identity->Money < 12.0f)
        {
            Add(ETalkKind::Trouble, TEXT("деньги"), TEXT("совсем на нуле"), -0.6f, 0.4f);
        }
    }

    // --- Погода и время дня -------------------------------------------------
    if (const UHumanWorldSubsystem* World = GetWorldMind())
    {
        // Ненастье — самый ходовой предмет разговора у людей, которым
        // больше сказать друг другу нечего.
        const float Bad = World->Weather;
        const FString What = Bad > 0.7f ? TEXT("непогода")
                           : Bad > 0.45f ? TEXT("сыро и серо")
                           : Bad > 0.2f ? TEXT("облачно")
                           : TEXT("погода хорошая");

        Add(ETalkKind::Weather, What, FString(), 0.3f - Bad * 0.6f, 0.18f);
    }
}

FTalkingPoint UMindComponent::ChooseTalkingPoint(AActor* Listener) const
{
    TArray<FTalkingPoint> Points;
    GatherTalkingPoints(Points);

    if (Points.Num() == 0)
    {
        return FTalkingPoint();
    }

    // Насколько человек откровенен с этим собеседником: чужому о наболевшем
    // не рассказывают, а с близким и помолчать можно о погоде.
    const float Closeness = Social ? Social->GetCloseness(Listener) : 0.0f;

    float Total = 0.0f;
    for (FTalkingPoint& P : Points)
    {
        // Личное — только своим.
        const bool bIntimate = (P.Kind == ETalkKind::Feeling || P.Kind == ETalkKind::Dream
                             || P.Kind == ETalkKind::Trouble || P.Kind == ETalkKind::Person);
        if (bIntimate)
        {
            P.Weight *= FMath::Clamp(0.15f + Closeness * 1.6f, 0.05f, 1.4f);
        }
        // С незнакомым говорят о погоде и о городе — это и есть светская беседа.
        if (Closeness < 0.2f && (P.Kind == ETalkKind::Weather || P.Kind == ETalkKind::Place))
        {
            P.Weight *= 1.8f;
        }
        Total += P.Weight;
    }

    float Roll = FMath::FRand() * FMath::Max(0.001f, Total);
    for (const FTalkingPoint& P : Points)
    {
        Roll -= P.Weight;
        if (Roll <= 0.0f)
        {
            return P;
        }
    }
    return Points.Last();
}

FReadingBookmark& UMindComponent::BookmarkFor(FName Subject)
{
    for (FReadingBookmark& Mark : Bookmarks)
    {
        if (Mark.Subject == Subject)
        {
            return Mark;
        }
    }

    FReadingBookmark Fresh;
    Fresh.Subject = Subject;
    return Bookmarks[Bookmarks.Add(Fresh)];
}

void UMindComponent::TryFindJob()
{
    if (!Identity || Identity->bEmployed || !Memory)
    {
        return;
    }

    // Подросткам и старикам работу не ищут.
    if (Identity->Age < 18.0f || Identity->Age > 70.0f)
    {
        return;
    }

    UHumanWorldSubsystem* World = GetWorldMind();
    const ACompleteHumanNPC* Me = GetHuman();
    if (!World || !Me)
    {
        return;
    }

    const FKnownLocation* Chosen = nullptr;
    const FAffordance* ChosenOffer = nullptr;
    float BestFit = -BIG_NUMBER;
    for (const FKnownLocation& Candidate : Memory->Places)
    {
        if (Candidate.Kind == EPlaceKind::Home || Candidate.Kind == EPlaceKind::Danger)
        {
            continue;
        }
        for (const FAffordance& Offer : Candidate.Offers)
        {
            if (Offer.Action != EActionType::Work || Offer.MoneyGainPerHour <= 0.0f || Offer.bPrivate)
            {
                continue;
            }
            const float Level = Offer.RequiredSkill.IsNone() ? 0.3f : Mastery(Offer.RequiredSkill);
            const float Fit = Level - Offer.Difficulty * 0.5f + Offer.MoneyGainPerHour * 0.01f
                            - FVector::Dist2D(Candidate.Location, Me->GetActorLocation()) / 200000.0f;
            if (Fit > BestFit)
            {
                BestFit = Fit;
                Chosen = &Candidate;
                ChosenOffer = &Offer;
            }
        }
    }

    if (!Chosen || !ChosenOffer)
    {
        Think(TEXT("не знаю, где тут вообще берут на работу"), EThoughtKind::Worry, 0.6f);
        return;
    }

    TArray<FKnownLocation> Workplaces;
    Workplaces.Add(*Chosen);
    const FName JobSkillName = ChosenOffer->RequiredSkill;
    const float JobWage = ChosenOffer->MoneyGainPerHour;
    const float JobDifficulty = ChosenOffer->Difficulty;
    const EPlaceKind JobKind = RealKindAt(Chosen->Location, Chosen->Kind);

    const float T = Now();
    const float JobSkill = JobSkillName.IsNone() ? 0.3f : Mastery(JobSkillName);

    float Chance = 0.25f + JobSkill * 0.45f - JobDifficulty * 0.2f;
    if (Social)      Chance += Social->Reputation * 0.2f;
    if (Personality) Chance += Personality->Traits.Conscientiousness * 0.15f;
    if (Body)        Chance -= (1.0f - Body->Body.Cleanliness) * 0.2f;
    if (Emotions)    Chance -= Emotions->GetStress() * 0.1f;
    if (Speech)      Chance += FMath::Clamp(Speech->GetVocabularySize() / 300.0f, 0.0f, 0.15f);
    if (Identity)    Chance += Identity->Standing * 0.2f;

    Chance = FMath::Clamp(Chance, 0.05f, 0.9f);

    // Насколько человек рассчитывал на успех — из веры в себя, а не из
    // настоящих шансов. Именно расхождение с ожиданием и будет переживаться.
    const float Hoped = Identity ? FMath::Clamp(Identity->SelfEfficacy, 0.05f, 0.95f) : 0.5f;
    const float MoneyNeed = Needs ? Needs->GetUrgency(ENeedType::Money) : 0.5f;

    if (FMath::FRand() >= Chance)
    {
        // Отказ. Насколько это больно — зависит от того, как сильно ждал
        // и насколько нужны были деньги. Один отказ ничего не значит,
        // десятый ломает — но не потому, что так написано, а потому что
        // каждый следующий бьёт по уже подорванной вере в себя.
        if (Emotions)
        {
            FAppraisedEvent Refusal;
            Refusal.Tag = TEXT("ExpectationBroken");
            Refusal.Description = TEXT("меня не взяли");
            Refusal.Desirability = -FMath::Clamp(0.3f + MoneyNeed * 0.5f, 0.0f, 1.0f);
            Refusal.Unexpectedness = Hoped;          // чем больше ждал, тем больнее
            Refusal.Controllability = 0.15f;          // решал не я
            Refusal.SelfAgency = 0.4f;                // но и себя виню
            Refusal.OtherAgency = 0.3f;
            Refusal.Certainty = 1.0f;
            Refusal.Significance = FMath::Clamp(0.4f + MoneyNeed * 0.5f, 0.0f, 1.0f);
            Emotions->Appraise(Refusal, Personality);
        }
        if (Identity)
        {
            Identity->OnFailure(0.35f, false);
        }
        Think(TEXT("снова отказ. Что со мной не так?"), EThoughtKind::Rumination, 0.75f);
        return;
    }

    // Взяли.
    const FKnownLocation& Place = Workplaces[0];
    Memory->LearnPlace(EPlaceKind::Work, Place.Location, TEXT("моя работа"), true, nullptr, T);

    Identity->bEmployed = true;
    Identity->HourlyWage = FMath::Max(4.0f, JobWage * FMath::FRandRange(0.85f, 1.15f));
    switch (JobKind)
    {
    case EPlaceKind::Workshop: Identity->Occupation = TEXT("мастер"); break;
    case EPlaceKind::Bakery:   Identity->Occupation = TEXT("пекарь"); break;
    case EPlaceKind::Market:   Identity->Occupation = TEXT("торговец"); break;
    case EPlaceKind::TownHall: Identity->Occupation = TEXT("писарь"); break;
    case EPlaceKind::Hospital: Identity->Occupation = JobSkill > 0.4f ? TEXT("лекарь") : TEXT("санитар"); break;
    case EPlaceKind::Food:
    case EPlaceKind::Shop:     Identity->Occupation = TEXT("повар"); break;
    case EPlaceKind::Study:    Identity->Occupation = TEXT("учитель"); break;
    case EPlaceKind::Library:  Identity->Occupation = TEXT("библиотекарь"); break;
    case EPlaceKind::Work:     Identity->Occupation = TEXT("служащий"); break;
    default:                   Identity->Occupation = TEXT("работник"); break;
    }
    Report(FString::Printf(TEXT("взяли на работу: %s, %.0f в час"), *Identity->Occupation, Identity->HourlyWage));
    Identity->RecordLifeEvent(TEXT("наконец нашёл(ла) работу"), 0.8f, 0.7f, T);
    Identity->OnSuccess(0.7f);

    if (Emotions)
    {
        // Что человек почувствует — не задано. Тот, кто уже не надеялся,
        // испытает облегчение; тот, кто был уверен, — просто удовлетворение;
        // а гордость придёт лишь к тому, для кого это было важно.
        FAppraisedEvent Hired;
        Hired.Tag = TEXT("Hired");
        Hired.Description = TEXT("меня взяли");
        Hired.Desirability = FMath::Clamp(0.4f + MoneyNeed * 0.6f, 0.0f, 1.0f);
        Hired.Unexpectedness = 1.0f - Hoped;
        Hired.SelfAgency = 0.7f;
        Hired.Controllability = 0.6f;
        Hired.OtherAgency = 0.3f;
        Hired.Certainty = 1.0f;
        Hired.Significance = FMath::Clamp(0.5f + MoneyNeed * 0.5f, 0.0f, 1.0f);
        Emotions->Appraise(Hired, Personality);
    }
    if (Needs)
    {
        Needs->Satisfy(ENeedType::Money, 0.3f);
        Needs->Satisfy(ENeedType::Esteem, 0.3f);
        Needs->Satisfy(ENeedType::Achievement, 0.3f);
    }
    RewardSignal = 0.8f;
    Think(TEXT("меня взяли! Ну наконец-то."), EThoughtKind::Feeling, 0.9f);

    if (Memory)
    {
        Memory->Encode(TEXT("меня взяли на работу"), TEXT("Hired"), 0.8f, 0.8f, {},
                       GetOwner()->GetActorLocation(), T, 1.0f, EMemoryKind::Autobiographic);
    }
}

void UMindComponent::HandleEncounters()
{
    if (bAsleep || SpeechCooldown > 0.0f || !Social || !Personality || IsInConversation() || bPolicyControlled
        || (Identity && Identity->Age < 3.0f))
    {
        return;
    }

    UHumanWorldSubsystem* World = GetWorldMind();
    ACompleteHumanNPC* Human = GetHuman();
    if (!World || !Human)
    {
        return;
    }

    // Разговор начинается не по плану, а потому что кто-то попался навстречу.
    ACompleteHumanNPC* Nearest = World->GetNearestHuman(Human->GetActorLocation(), 380.0f, Human);
    if (!Nearest || !CanTalkTo(Nearest))
    {
        return;
    }

    // Через стену не заговаривают, даже если человек в двух метрах.
    {
        float Noticed = 0.0f;
        FName Sense;
        if (!Human->CanPerceive(Nearest, Noticed, Sense) || Sense != TEXT("Sight"))
        {
            return;
        }
    }

    const float T = Now();
    FRelationship& R = Social->FindOrAdd(Nearest, T);
    const float SinceLast = T - R.LastInteractionAt;
    const bool bNeverTalked = (R.InteractionCount == 0);

    // От того, кого боишься, отходят молча.
    if (R.Fear > 0.5f)
    {
        return;
    }

    if (bTalkReady)
    {
        if (!bNeverTalked && SinceLast < 600.0f)
        {
            SpeechCooldown = FMath::FRandRange(20.0f, 40.0f);
            return;
        }
        ETalkChoice Opening = ETalkChoice::Nothing;
        if (ChooseOpening(Nearest, false, Opening) && Opening != ETalkChoice::Nothing)
        {
            if (!StartConversation(Nearest))
            {
                SpeechCooldown = FMath::FRandRange(2.5f, 5.0f);
            }
        }
        else
        {
            SpeechCooldown = FMath::FRandRange(15.0f, 30.0f);
        }
        return;
    }

    // Заговорит ли человек с встречным — это про характер, а не про логику.
    // Экстраверт поздоровается с незнакомцем, интроверт пройдёт мимо друга.
    float Chance = 0.10f
        + Personality->Traits.Extraversion * 0.35f
        + Personality->Facets.Sociability * 0.20f
        + Social->GetCloseness(Nearest) * 0.45f;

    // Если только что разговаривали — не о чем.
    if (!bNeverTalked && SinceLast < 900.0f)
    {
        Chance *= 0.08f;
    }
    // Занятому и издёрганному не до бесед.
    if (Emotions)
    {
        Chance *= (1.0f - Emotions->GetStress() * 0.5f);
        if (Emotions->GetIntensity(EEmotionType::Sadness) > 0.5f)
        {
            Chance *= 0.5f;
        }
    }
    // На обиженного смотрят, но не заговаривают.
    Chance *= (1.0f - R.Resentment * 0.7f);
    // Потребность в общении толкает даже молчунов.
    if (Needs)
    {
        Chance *= (1.0f + (1.0f - Needs->GetSatisfaction(ENeedType::SocialContact)) * 0.8f);
    }

    // Кому нечем сказать, тот сам заговаривает редко — кивнёт и пройдёт.
    if (Speech && Speech->GetVocabularySize() < 5)
    {
        Chance *= 0.3f;
    }

    // Кто читает или работает, того не отвлекают.
    if (Nearest->Mind && (Nearest->Mind->CurrentAction == EActionType::Read
        || Nearest->Mind->CurrentAction == EActionType::Study
        || Nearest->Mind->CurrentAction == EActionType::Work))
    {
        Chance *= 0.15f;
    }

    if (FMath::FRand() < FMath::Clamp(Chance, 0.0f, 0.9f))
    {
        if (!StartConversation(Nearest))
        {
            SpeechCooldown = FMath::FRandRange(2.5f, 5.0f);
        }
    }
    else if (bNeverTalked && FMath::FRand() < 0.2f)
    {
        // Не заговорил — но заметил. Иногда этого достаточно, чтобы потом
        // вспомнить лицо.
        Think(ThoughtAboutPerson(Nearest), EThoughtKind::Observation, 0.35f, Nearest);
    }
}

void UMindComponent::Hear(const FUtterance& U)
{
    if (!U.Speaker || bAsleep)
    {
        return;
    }

    const float T = Now();
    AActor* Speaker = U.Speaker;

    // Реплика — ход в разговоре: на неё ответят в свой черёд.
    OnDialogueHeard(U);

    // --- О чём мне сказали ---------------------------------------------------
    // Услышанное человек удерживает в голове, и следующая его реплика будет
    // именно об этом. Так две речи сцепляются в разговор.
    if (U.Point.IsValid() && U.Listener == GetOwner())
    {
        // Понял ли он вообще: ребёнок слышит слова, но не смысл.
        const float Grasp = Speech ? Speech->Comprehension(U.Text) : 1.0f;
        if (Grasp > 0.45f)
        {
            HeardTopic = U.Point;
            ConversationPartner = Speaker;

            // Разговор идёт о том же — держим тему и со своей стороны.
            if (!CurrentTopic.IsValid())
            {
                CurrentTopic = U.Point;
            }

        }
    }

    // Утешение и поддержка запоминаются: кто был рядом в плохую минуту,
    // того потом и слушают.
    if (U.Act == ESpeechAct::Console || U.Act == ESpeechAct::Offer)
    {
        JudgeByDeeds(Speaker, 0.05f, TEXT("не бросает в трудную минуту"));
    }
    else if (U.Act == ESpeechAct::Insult || U.Act == ESpeechAct::Threaten)
    {
        JudgeByDeeds(Speaker, -0.12f, TEXT("говорит гадости"));
    }
    else if (U.Act == ESpeechAct::Boast)
    {
        // Хвастовство сбивает уважение, а не поднимает. Так у людей.
        JudgeByDeeds(Speaker, -0.03f, TEXT("много о себе говорит"));
    }

    // Знакомство: имя узнаётся тогда, когда его называют.
    if (Social && U.Act == ESpeechAct::Greet)
    {
        if (ACompleteHumanNPC* SpeakerHuman = Cast<ACompleteHumanNPC>(Speaker))
        {
            if (UIdentityComponent* TheirId = SpeakerHuman->FindComponentByClass<UIdentityComponent>())
            {
                FRelationship& Rel = Social->FindOrAdd(Speaker, T);
                if (Rel.KnownName.IsEmpty())
                {
                    Rel.KnownName = TheirId->FirstName;
                    Think(FString::Printf(TEXT("значит, %s"), *TheirId->FirstName),
                          EThoughtKind::Observation, 0.4f, Speaker);
                }
            }
        }
    }

    // Обращённое к тебе слово запоминается лучше услышанного мимоходом.
    if (Speech && Identity)
    {
        Speech->LearnWords(U.Text, FMath::Clamp(Focus, 0.3f, 1.0f), Identity->Age < 13.0f);
    }

    const float Trust = Social ? Social->GetTrust(Speaker) : 0.3f;
    const float Closeness = Social ? Social->GetCloseness(Speaker) : 0.0f;
    const float MyEmpathy = Personality ? Personality->Facets.Empathy : 0.5f;

    const FSpeechEffect Effect = USpeechComponent::EvaluateEffect(U, Trust, Closeness, MyEmpathy);

    // --- Ложь могут и раскусить --------------------------------------------
    bool bCaughtLie = false;
    if (!U.bTruthful && Social)
    {
        const float MyEmpathySkill = Mastery(TEXT("Empathy"));
        float TheirDeception = 0.2f;
        if (ACompleteHumanNPC* SpeakerHuman = Cast<ACompleteHumanNPC>(Speaker))
        {
            if (SpeakerHuman->Mind)
            {
                TheirDeception = SpeakerHuman->Mind->Mastery(TEXT("Deception"));
            }
        }
        const FRelationship* R = Social->Find(Speaker);
        const float Familiarity = R ? R->Familiarity : 0.0f;

        bCaughtLie = USpeechComponent::DetectLie(U, MyEmpathySkill, TheirDeception, Familiarity);
        if (bCaughtLie)
        {
            Social->OnCaughtLying(Speaker, T);
            Think(FString::Printf(TEXT("%s врёт мне в глаза"), *NameOf(Speaker)), EThoughtKind::Judgement, 0.9f, Speaker);
            if (Emotions)
            {
                Emotions->Trigger(EEmotionType::Contempt, 0.4f, Speaker, TEXT("он мне соврал"));
                Emotions->Trigger(EEmotionType::Anger, 0.3f, Speaker, TEXT("держит меня за дурака"));
            }
        }
    }

    // --- Чувства от услышанного --------------------------------------------
    if (Emotions && Effect.EvokedEmotion != EEmotionType::None && !bCaughtLie)
    {
        Emotions->Trigger(Effect.EvokedEmotion, Effect.EvokedIntensity, Speaker, U.Text);
    }

    // --- Потребности --------------------------------------------------------
    if (Needs && Effect.SatisfactionAmount != 0.0f)
    {
        if (Effect.SatisfactionAmount > 0.0f)
        {
            Needs->Satisfy(Effect.SatisfiedNeed, Effect.SatisfactionAmount);
        }
        else
        {
            Needs->Deprive(Effect.SatisfiedNeed, -Effect.SatisfactionAmount);
        }
        Needs->Satisfy(ENeedType::SocialContact, 0.12f);
    }

    // --- Отношения ----------------------------------------------------------
    if (Social)
    {
        Social->RecordInteraction(Speaker, Effect.Pleasantness, T, Personality);
        if (Effect.TrustDelta != 0.0f)
        {
            FRelationship& R = Social->FindOrAdd(Speaker, T);
            R.Trust = FMath::Clamp(R.Trust + Effect.TrustDelta, 0.0f, 1.0f);
        }

        // Оскорбление — это вред, и оно запоминается как вред.
        if (U.Act == ESpeechAct::Insult)
        {
            Social->OnHarmedBy(Speaker, 0.4f, true, T, Personality);
        }
        if (U.Act == ESpeechAct::Apologize)
        {
            const bool bForgiven = Social->ReceiveApology(Speaker, 0.7f, Personality, T);
            Think(bForgiven ? TEXT("ладно, проехали") : TEXT("извинения — это хорошо, но осадок остался"),
                  EThoughtKind::Judgement, 0.6f, Speaker);
        }
        if (U.Act == ESpeechAct::Gossip && U.bHasPayload)
        {
            // Слух долетел. Передаётся не факт, а отношение — и действует оно
            // только если я вообще понимаю, о ком речь.
            const FString SubjectName = U.Payload.Subject.ToString();
            for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Social->Relations)
            {
                if (Pair.Key && !Pair.Value.KnownName.IsEmpty() && Pair.Value.KnownName == SubjectName)
                {
                    Social->ReceiveGossip(Speaker, Pair.Key, U.Payload.Value, T, Personality);
                    Think(FString::Printf(TEXT("вот оно что... значит, %s"), *SubjectName),
                          EThoughtKind::Judgement, 0.55f, Pair.Key);
                    break;
                }
            }
        }
    }

    // --- Знания -------------------------------------------------------------
    if (Memory && U.bHasPayload)
    {
        const float Credibility = bCaughtLie ? 0.0f : Trust;
        const float Openness = Personality ? Personality->Traits.Openness : 0.5f;
        Memory->Learn(U.Payload, Credibility, Openness, T);
    }

    // --- Память о разговоре -------------------------------------------------
    if (Memory && FMath::Abs(Effect.Pleasantness) > 0.25f)
    {
        TArray<AActor*> Participants = { Speaker };
        Memory->Encode(
            FString::Printf(TEXT("%s сказал(а): «%s»"), *NameOf(Speaker), *U.Text),
            TEXT("Conversation"),
            Effect.Pleasantness,
            FMath::Abs(Effect.Pleasantness) * 0.7f,
            Participants,
            GetOwner()->GetActorLocation(),
            T, Focus);
    }

    // --- Знание едет в словах, а не переливается из головы в голову ---------
    // Понял ли слушатель сказанное — решает его словарь. Ребёнок, не
    // знающий слов, услышит звук и не получит ничего.
    const float Understood = Speech ? Speech->Comprehension(U.Text) : 1.0f;
    const float Credence = FMath::Clamp(Trust * 0.6f + Closeness * 0.4f, 0.0f, 1.0f);

    if (Understood > 0.5f)
    {
        if (Memory && U.bHasPlacePayload && Credence > 0.3f)
        {
            const FKnownLocation& Told = U.PlacePayload;
            Memory->LearnPlace(Told.Kind, Told.Location, Told.Label, false, Speaker, T,
                               Told.Offers.Num() > 0 ? &Told.Offers : nullptr);
            Think(FString::Printf(TEXT("%s — надо будет туда сходить"), *Told.Label),
                  EThoughtKind::Recall, 0.5f, Speaker);
        }

        if (!U.SkillPayload.IsNone() && Credence > 0.25f)
        {
            SawSomeoneDo(U.SkillPayload, U.SkillPayloadLevel, Credence * Focus * Understood);
        }
    }
    else if (U.bHasPlacePayload || !U.SkillPayload.IsNone())
    {
        Think(TEXT("не понял, о чём он"), EThoughtKind::Observation, 0.4f, Speaker);
    }

    Think(FString::Printf(TEXT("%s: «%s»"), *NameOf(Speaker), *U.Text), EThoughtKind::Observation, 0.55f, Speaker);
}

// ---------------------------------------------------------------------------
//  Внешние воздействия
// ---------------------------------------------------------------------------

void UMindComponent::OnHurt(AActor* By, float Severity, bool bIntentional)
{
    const float T = Now();

    if (Body)
    {
        Body->TakeInjury(Severity, TEXT("его ударили"));
    }

    if (Social && By)
    {
        Social->OnHarmedBy(By, Severity, bIntentional, T, Personality);
    }

    if (Emotions)
    {
        FAppraisedEvent Ev;
        Ev.Tag = TEXT("Attacked");
        Ev.Description = By ? FString::Printf(TEXT("%s причинил(а) мне боль"), *NameOf(By)) : TEXT("мне больно");
        Ev.Subject = By;
        Ev.Desirability = -FMath::Clamp(Severity, 0.0f, 1.0f);
        Ev.OtherAgency = bIntentional ? 0.95f : 0.2f;
        Ev.Controllability = 0.2f;
        Ev.Unexpectedness = 0.8f;
        Ev.NormAlignment = bIntentional ? -0.9f : 0.0f;
        Ev.Significance = FMath::Clamp(0.5f + Severity, 0.0f, 1.0f);
        Emotions->Appraise(Ev, Personality);
    }

    // Такое запоминается навсегда.
    if (Memory)
    {
        TArray<AActor*> Participants;
        if (By) Participants.Add(By);
        const int32 Id = Memory->Encode(
            By ? FString::Printf(TEXT("%s сделал(а) мне больно"), *NameOf(By)) : TEXT("мне сделали больно"),
            TEXT("Attacked"), -0.9f, 0.95f, Participants, GetOwner()->GetActorLocation(), T, 1.0f,
            EMemoryKind::Traumatic);

        // Совсем невыносимое психика прячет от самой себя.
        if (Severity > 0.8f && Personality && Personality->Traits.Neuroticism > 0.6f && FMath::FRand() < 0.4f)
        {
            Memory->Repress(Id);
            Think(TEXT("не хочу об этом думать"), EThoughtKind::SelfTalk, 0.9f);
        }
    }

    if (Personality && Identity)
    {
        Personality->ApplyTrauma(Severity, Identity->Age);
        Identity->RecordLifeEvent(TEXT("со мной обошлись жестоко"), -0.9f, Severity, T);
    }

    // --- Рефлекс ------------------------------------------------------------
    // Это единственное место, где решение НЕ взвешивается. И так и должно
    // быть: при внезапной боли тело действует раньше, чем человек успевает
    // подумать. Что именно оно сделает — бей, беги или замри — зависит от
    // того, чувствует ли он себя сильнее или беспомощнее нападающего.
    {
        const float Dominance = Emotions ? Emotions->GetAffect().Dominance : 0.0f;
        const float Daring = Personality ? Personality->Facets.RiskTaking : 0.5f;

        FAffordance Reflex;
        Reflex.Source = EAffordanceSource::Self;
        Reflex.Target = By;
        Reflex.bRequiresProximity = false;
        Reflex.Duration = 300.0f;

        if (Dominance > 0.25f && Daring > 0.55f)
        {
            Reflex.Action = EActionType::Confront;
            Reflex.Label = TEXT("не дать себя в обиду");
            Reflex.Source = EAffordanceSource::Person;
            Reflex.Key = TEXT("Fight@Reflex");
        }
        else if (Dominance < -0.4f)
        {
            Reflex.Action = EActionType::Freeze;
            Reflex.Label = TEXT("оцепенеть");
            Reflex.Duration = 180.0f;
            Reflex.Key = TEXT("Freeze@Reflex");
        }
        else
        {
            Reflex.Action = EActionType::Flee;
            Reflex.Label = TEXT("бежать отсюда");
            Reflex.Key = TEXT("Flee@Reflex");

            if (ACompleteHumanNPC* Human = GetHuman())
            {
                const FVector Away = By
                    ? (Human->GetActorLocation() - By->GetActorLocation()).GetSafeNormal()
                    : -Human->GetActorForwardVector();
                Reflex.Location = Human->GetActorLocation() + Away * 2500.0f;
                Reflex.bHasLocation = true;
                Reflex.bRequiresProximity = true;
            }
        }

        FNeedPromise Safety;
        Safety.Need = ENeedType::Safety;
        Safety.Amount = 0.5f;
        Reflex.Promises.Add(Safety);

        FIntention Forced;
        Forced.bValid = true;
        Forced.Affordance = Reflex;
        Forced.Expectation = 0.2f;
        Forced.DecidedAt = T;
        Forced.Reason = Reflex.Label;

        BeginIntention(Forced);
    }
}

void UMindComponent::OnHelped(AActor* By, float Magnitude)
{
    const float T = Now();

    if (Social && By)
    {
        Social->OnHelpedBy(By, Magnitude, T, Personality);
    }

    if (Emotions)
    {
        FAppraisedEvent Ev;
        Ev.Tag = TEXT("Helped");
        Ev.Description = By ? FString::Printf(TEXT("%s помог(ла) мне"), *NameOf(By)) : TEXT("мне помогли");
        Ev.Subject = By;
        Ev.Desirability = FMath::Clamp(Magnitude, 0.0f, 1.0f);
        Ev.OtherAgency = 0.9f;
        Ev.NormAlignment = 0.7f;
        Ev.Unexpectedness = 0.5f;
        Ev.Significance = 0.6f;
        Emotions->Appraise(Ev, Personality);
    }

    if (Needs)
    {
        Needs->Satisfy(ENeedType::Belonging, 0.25f);
        Needs->Satisfy(ENeedType::Safety, 0.1f);
    }

    if (Memory && By)
    {
        TArray<AActor*> Participants = { By };
        Memory->Encode(FString::Printf(TEXT("%s выручил(а) меня"), *NameOf(By)),
                       TEXT("Helped"), 0.7f, 0.6f, Participants, GetOwner()->GetActorLocation(), T, Focus);
    }

    SocialWarmth = FMath::Clamp(SocialWarmth + 0.4f, 0.0f, 1.0f);
    RewardSignal = 0.3f;
}

void UMindComponent::Grieve(AActor* Who, float Closeness)
{
    if (!Motivation || Closeness < 0.2f)
    {
        return;
    }

    const float T = Now();
    const FString Name = NameOf(Who);

    // Горе прерывает всё. Дела подождут.
    if (bActionActive)
    {
        if (ACompleteHumanNPC* Human = GetHuman())
        {
            Human->StopMoving();
        }
        bActionActive = false;
        bMoving = false;
    }

    // Горе не «назначается» — оно просто вытесняет всё остальное, потому
    // что ничто другое сейчас ничего не стоит.
    FAffordance Mourning;
    Mourning.Action = EActionType::Mourn;
    Mourning.Source = EAffordanceSource::Self;
    Mourning.Target = Who;
    Mourning.bRequiresProximity = false;
    Mourning.Duration = FMath::Lerp(1800.0f, 10800.0f, Closeness);
    Mourning.EffortCost = 0.0f;
    Mourning.Label = FString::Printf(TEXT("не могу поверить — %s"), *Name);
    Mourning.Key = FName(*FString::Printf(TEXT("Mourn@%s"), *Name));

    FNeedPromise Meaning;
    Meaning.Need = ENeedType::Meaning;
    Meaning.Amount = 0.1f;
    Mourning.Promises.Add(Meaning);

    FIntention Grief;
    Grief.bValid = true;
    Grief.Affordance = Mourning;
    Grief.Expectation = -0.4f;
    Grief.DecidedAt = T;
    Grief.Reason = FString::Printf(TEXT("%s... как это — больше нет?"), *Name);

    BeginIntention(Grief);

    // Замыслы, которые были, разом теряют вес.
    for (FGoal& G : Motivation->Goals)
    {
        if (G.Horizon != EGoalHorizon::Life)
        {
            G.Status = EGoalStatus::Suspended;
        }
    }
}

// ---------------------------------------------------------------------------
//  Сон
// ---------------------------------------------------------------------------

void UMindComponent::ForceSleep()
{
    BeginSleep();
}

void UMindComponent::ForceWake()
{
    EndSleep(true);
}

void UMindComponent::BeginSleep()
{
    if (bAsleep)
    {
        return;
    }

    bAsleep = true;
    SleepPhase = ESleepPhase::Drowsy;
    SleepPhaseTimer = 0.0f;
    TotalSleepTime = 0.0f;
    bMoving = false;
    bActionActive = false;
    CurrentAction = EActionType::Sleep;
    CurrentActionLabel = TEXT("спит");

    if (ACompleteHumanNPC* Human = GetHuman())
    {
        Human->StopMoving();
    }

    // Перед сном вспоминается несделанное — самое неприятное время суток.
    if (Motivation && Motivation->HasNaggingGoal())
    {
        Think(TEXT("столько всего не сделано... завтра. Завтра точно."), EThoughtKind::Worry, 0.7f);
        if (Emotions)
        {
            Emotions->Trigger(EEmotionType::Guilt, 0.2f, nullptr, TEXT("опять ничего не успел(а)"));
        }
    }
    else
    {
        Think(TEXT("всё, спать"), EThoughtKind::Intention, 0.4f);
    }
}

void UMindComponent::UpdateSleep(float GameDelta)
{
    if (!bAsleep)
    {
        return;
    }

    SleepPhaseTimer += GameDelta;
    TotalSleepTime += GameDelta;

    // Цикл сна — примерно полтора игровых часа:
    // дремота → лёгкий → глубокий → лёгкий → быстрый.
    const float CycleLength = 5400.0f;
    const float InCycle = FMath::Fmod(TotalSleepTime, CycleLength);

    ESleepPhase NewPhase;
    if (TotalSleepTime < 300.0f)          NewPhase = ESleepPhase::Drowsy;
    else if (InCycle < 900.0f)            NewPhase = ESleepPhase::Light;
    else if (InCycle < 3000.0f)           NewPhase = ESleepPhase::Deep;
    else if (InCycle < 4200.0f)           NewPhase = ESleepPhase::Light;
    else                                  NewPhase = ESleepPhase::REM;

    // Под утро глубокого сна меньше, быстрого — больше.
    if (TotalSleepTime > 18000.0f && NewPhase == ESleepPhase::Deep)
    {
        NewPhase = ESleepPhase::REM;
    }

    if (NewPhase != SleepPhase)
    {
        SleepPhase = NewPhase;
        SleepPhaseTimer = 0.0f;
    }

    // Память работает во сне — это и есть его главный смысл.
    if (Memory)
    {
        Memory->ConsolidateDuringSleep(GameDelta, Now(), SleepPhase == ESleepPhase::REM);
    }
    if (bCoreReady && SleepPhase == ESleepPhase::REM)
    {
        DreamAccumulator += GameDelta;
        if (DreamAccumulator >= 300.0f)
        {
            DreamAccumulator = 0.0f;
            Core.Practice(6, MindRng);
        }
    }

    // Воля восстанавливается.
    if (Motivation)
    {
        Motivation->RestoreWillpower(GameDelta / 3600.0f * 0.25f);
    }

    // --- Пробуждение --------------------------------------------------------
    UHumanWorldSubsystem* World = GetWorldMind();
    const float Hour = World ? World->Now.HourFloat : 8.0f;
    const float Pressure = Body ? Body->GetSleepPressure(Hour) : 0.0f;

    const bool bRested = (Body && Body->Body.SleepDebt < 1.5f);
    const bool bMorning = (Hour > 6.0f && Hour < 11.0f);
    const bool bDisturbed = (Emotions && Emotions->GetIntensity(EEmotionType::Fear) > 0.5f);

    if (bDisturbed)
    {
        EndSleep(false);
    }
    else if ((bRested && Pressure < 0.3f) || (bMorning && Pressure < 0.45f && TotalSleepTime > 14400.0f)
        || (Body && Body->Body.SleepDebt < 0.5f && TotalSleepTime > 3600.0f))
    {
        EndSleep(true);
    }
    // Спать вечно тоже нельзя.
    else if (TotalSleepTime > 43200.0f)
    {
        EndSleep(true);
    }
}

void UMindComponent::EndSleep(bool bNaturally)
{
    if (!bAsleep)
    {
        return;
    }

    // Кровать больше не занята.
    ReleaseOccupied();

    bAsleep = false;
    SleepPhase = ESleepPhase::Awake;
    CurrentAction = EActionType::Idle;
    CurrentActionLabel = TEXT("проснулся");

    const float Hours = TotalSleepTime / 3600.0f;

    if (bNaturally && Hours > 6.0f)
    {
        if (Emotions) Emotions->Trigger(EEmotionType::Serenity, 0.3f, nullptr, TEXT("выспался(лась)"));
        if (Motivation) Motivation->RestoreWillpower(0.5f);
        Think(TEXT("выспался. Хороший знак."), EThoughtKind::Feeling, 0.4f);
    }
    else if (Hours < 4.0f)
    {
        if (Emotions) Emotions->Trigger(EEmotionType::Frustration, 0.3f, nullptr, TEXT("не выспался(лась)"));
        Think(TEXT("голова как чугунная"), EThoughtKind::Feeling, 0.5f);
    }
    else
    {
        Think(TEXT("ну, доброе утро"), EThoughtKind::Feeling, 0.3f);
    }

    if (!bNaturally)
    {
        if (Emotions) Emotions->Trigger(EEmotionType::Anxiety, 0.35f, nullptr, TEXT("что-то разбудило"));
        Think(TEXT("что это было?"), EThoughtKind::Worry, 0.7f);
    }

    TotalSleepTime = 0.0f;
}

void UMindComponent::Dream()
{
    if (!Memory)
    {
        return;
    }

    // Сон собирается из обрывков памяти, склеенных без всякой логики.
    FEpisodicMemory* A = Memory->GetRandomWeighted();
    FEpisodicMemory* B = Memory->GetRandomWeighted();

    // Вытесненное прорывается именно здесь.
    const float Stress = Emotions ? Emotions->GetStress() : 0.0f;
    if (FMath::FRand() < 0.2f + Stress * 0.4f)
    {
        if (FEpisodicMemory* Repressed = Memory->SurfaceRepressed())
        {
            LastDream = FString::Printf(TEXT("кошмар: %s"), *Repressed->Summary);
            if (Emotions)
            {
                Emotions->Trigger(EEmotionType::Fear, 0.6f, nullptr, TEXT("приснилось то, о чём не хочу помнить"));
            }
            Think(LastDream, EThoughtKind::Dream, 0.9f);
            // От кошмара просыпаются.
            if (FMath::FRand() < 0.5f)
            {
                EndSleep(false);
            }
            return;
        }
    }

    if (!A)
    {
        LastDream = TEXT("что-то бессвязное");
        Think(LastDream, EThoughtKind::Dream, 0.3f);
        return;
    }

    if (B && B != A)
    {
        LastDream = FString::Printf(TEXT("снилось: %s... и почему-то %s"), *A->Summary, *B->Summary);
    }
    else
    {
        LastDream = FString::Printf(TEXT("снилось: %s"), *A->Summary);
    }

    // Сон перерабатывает эмоцию: она смягчается.
    if (Emotions)
    {
        const float Tone = (A->Valence + (B ? B->Valence : 0.0f)) * 0.5f;
        if (Tone > 0.3f)
        {
            Emotions->Trigger(EEmotionType::Nostalgia, 0.2f, nullptr, LastDream);
        }
        else if (Tone < -0.3f)
        {
            Emotions->Trigger(EEmotionType::Anxiety, 0.15f, nullptr, LastDream);
        }
    }

    Think(LastDream, EThoughtKind::Dream, 0.5f);
}

// ---------------------------------------------------------------------------
//  Поток сознания
// ---------------------------------------------------------------------------

void UMindComponent::FeelFooting(float SinkCm, float Stick, float Grip, float Effort, float WaterCm, const FVector& Where)
{
    FString Felt;
    if (WaterCm > 30.0f)
    {
        Felt = TEXT("вода выше колена, еле иду");
    }
    else if (SinkCm > 12.0f)
    {
        Felt = TEXT("ноги проваливаются в грязь, вытаскиваю с трудом");
    }
    else if (SinkCm > 3.0f)
    {
        Felt = TEXT("ноги вязнут, идти тяжело");
    }
    else if (Stick > 0.45f)
    {
        Felt = TEXT("грязь липнет к ногам, каждый шаг тяжелее");
    }
    else if (Grip < 0.25f)
    {
        Felt = TEXT("скользко, ноги разъезжаются");
    }
    if (Felt.IsEmpty())
    {
        return;
    }
    Think(Felt, EThoughtKind::Feeling, FMath::Clamp(0.3f + (Effort - 1.0f) * 0.2f, 0.3f, 0.8f));
    Report(FString::Printf(TEXT("чувствует землю: %s (нога уходит на %.0f см, липкость %.0f%%, сцепление %.2f, шаг тяжелее в %.1f раза)"),
        *Felt, SinkCm, Stick * 100.0f, Grip, Effort));
    const float Discomfort = FMath::Clamp((Effort - 1.0f) * 0.04f + (Grip < 0.25f ? 0.03f : 0.0f), 0.01f, 0.12f);
    if (Needs)
    {
        Needs->Satisfy(ENeedType::Comfort, -Discomfort);
    }
    if (Memory)
    {
        Memory->VisitPlace(Where, Now(), -FMath::Clamp(Discomfort * 4.0f, 0.05f, 0.4f));
    }
}

void UMindComponent::FeelFall(float Speed, float Impact, const FVector& Where)
{
    const FString Felt = Impact > 0.12f ? TEXT("упал(а) и сильно ударился(лась)") : TEXT("упал(а)");
    Think(Felt, EThoughtKind::Feeling, FMath::Clamp(0.5f + Impact * 2.0f, 0.5f, 1.0f));
    Report(FString::Printf(TEXT("упал(а) на ходу: скорость %.1f м/с, удар %.2f"), Speed, Impact));
    if (Needs)
    {
        Needs->Satisfy(ENeedType::Comfort, -FMath::Clamp(Impact, 0.03f, 0.3f));
    }
    if (Memory)
    {
        Memory->VisitPlace(Where, Now(), -FMath::Clamp(0.2f + Impact * 2.0f, 0.2f, 0.8f));
    }
    if (Phase != EActionPhase::None)
    {
        PrepareTimeLeft = FMath::Max(PrepareTimeLeft, 2.5f);
    }
}

void UMindComponent::Think(const FString& Text, EThoughtKind Kind, float Salience, AActor* About)
{
    if (Text.IsEmpty())
    {
        return;
    }

    FThought T;
    T.Text = Text;
    T.Kind = Kind;
    T.Salience = FMath::Clamp(Salience, 0.0f, 1.0f);
    T.Time = Now();
    T.About = About;

    Stream.Add(T);
    while (Stream.Num() > StreamCapacity)
    {
        Stream.RemoveAt(0);
    }

    CurrentThought = Text;

    if (bLogThoughts && Identity)
    {
        UE_LOG(LogTemp, Log, TEXT("[%s] %s"), *Identity->FirstName, *Text);
    }
}

FString UMindComponent::ThoughtAboutBody() const
{
    if (!Body)
    {
        return FString();
    }
    const float Distress = Body->GetBodilyDistress();
    if (Distress < 0.45f)
    {
        return FString();
    }
    return Body->DescribeFeeling();
}

FString UMindComponent::ThoughtAboutFeeling() const
{
    if (!Emotions)
    {
        return FString();
    }

    EEmotionType Type;
    float Intensity;
    Emotions->GetDominantWithIntensity(Type, Intensity);
    if (Intensity < 0.25f)
    {
        return FString();
    }

    // Человек не просто чувствует — он ищет причину. Часто неверную.
    const FEmotionInstance* Source = nullptr;
    for (const FEmotionInstance& E : Emotions->Active)
    {
        if (E.Type == Type)
        {
            Source = &E;
            break;
        }
    }

    const FString Feeling = HumanText::EmotionFirstPerson(Type);
    if (Source && !Source->Reason.IsEmpty())
    {
        return FString::Printf(TEXT("%s — %s"), *Feeling, *Source->Reason);
    }
    // Причина потерялась, а чувство осталось. Это самое мучительное.
    return FString::Printf(TEXT("%s, и сам(а) не знаю почему"), *Feeling);
}

FString UMindComponent::ThoughtAboutPerson(AActor* Who) const
{
    if (!Who || !Social)
    {
        return FString();
    }

    const FRelationship* R = Social->Find(Who);
    if (!R)
    {
        return FString();
    }

    const FString Name = NameOf(Who);

    // Мысль о человеке зависит от того, что между нами.
    if (R->Resentment > 0.5f)
    {
        return FString::Printf(TEXT("%s... до сих пор не могу забыть"), *Name);
    }
    if (R->Fear > 0.5f)
    {
        return FString::Printf(TEXT("лучше держаться подальше от %s"), *Name);
    }
    if (R->Romantic > 0.5f)
    {
        return FString::Printf(TEXT("%s... интересно, думает ли обо мне"), *Name);
    }
    if (R->Debt > 0.5f)
    {
        return FString::Printf(TEXT("я ведь так и не отблагодарил(а) %s"), *Name);
    }
    if (R->Theory.BelievedLikingOfMe < -0.3f)
    {
        return FString::Printf(TEXT("кажется, %s меня недолюбливает. Или мне кажется?"), *Name);
    }
    if (R->Attachment > 0.5f)
    {
        return FString::Printf(TEXT("хорошо, что есть %s"), *Name);
    }
    if (R->Kind == ERelationKind::Stranger)
    {
        return TEXT("кто это вообще?");
    }
    return FString::Printf(TEXT("%s. Ну, %s."), *Name, *HumanText::Relation(R->Kind));
}

FString UMindComponent::Rumination() const
{
    if (!Memory || !Emotions)
    {
        return FString();
    }

    // Пережёвывание — это когда память сама подсовывает худшее.
    const TArray<FEpisodicMemory> Bad = Memory->Query(NAME_None, nullptr, 8);
    const FEpisodicMemory* Worst = nullptr;
    for (const FEpisodicMemory& M : Bad)
    {
        if (!Worst || M.Valence < Worst->Valence)
        {
            Worst = &M;
        }
    }

    if (!Worst || Worst->Valence > -0.3f)
    {
        return FString();
    }

    const TArray<FString> Frames = {
        FString::Printf(TEXT("опять вспомнилось: %s"), *Worst->Summary),
        FString::Printf(TEXT("надо было тогда поступить иначе... %s"), *Worst->Summary),
        FString::Printf(TEXT("почему я вообще об этом думаю? %s"), *Worst->Summary),
        FString::Printf(TEXT("%s — и ведь ничего уже не изменить"), *Worst->Summary)
    };
    return Frames[FMath::RandRange(0, Frames.Num() - 1)];
}

FString UMindComponent::ProspectiveThought() const
{
    if (!Motivation)
    {
        return FString();
    }

    const FGoal* Active = Motivation->FindGoal(Motivation->ActiveGoalId);
    const float Optimism = Personality ? Personality->Facets.Optimism : 0.5f;

    if (Active)
    {
        if (Active->Expectancy < 0.3f)
        {
            return FString::Printf(TEXT("%s... а получится ли вообще"), *Active->Name);
        }
        if (Active->Expectancy > 0.7f && Optimism > 0.5f)
        {
            return FString::Printf(TEXT("%s — справлюсь"), *Active->Name);
        }
    }

    if (Identity && FMath::FRand() < 0.4f)
    {
        if (Identity->DreamProgress < 0.2f)
        {
            return FString::Printf(TEXT("когда-нибудь — %s. Когда-нибудь."), *Identity->LifeDream);
        }
        return FString::Printf(TEXT("%s — и ведь уже что-то сдвинулось"), *Identity->LifeDream);
    }

    return FString();
}

void UMindComponent::GenerateThought()
{
    struct FCandidate { FString Text; EThoughtKind Kind; float Weight; AActor* About; };
    TArray<FCandidate> Candidates;

    const float Stress = Emotions ? Emotions->GetStress() : 0.0f;
    const float Neuroticism = Personality ? Personality->Traits.Neuroticism : 0.5f;

    // --- Тело кричит громче всего -------------------------------------------
    if (Body)
    {
        const FString BodyThought = ThoughtAboutBody();
        if (!BodyThought.IsEmpty())
        {
            Candidates.Add({ BodyThought, EThoughtKind::Feeling, Body->GetBodilyDistress() * 2.0f, nullptr });
        }
    }

    // --- Чувство ------------------------------------------------------------
    const FString FeelingThought = ThoughtAboutFeeling();
    if (!FeelingThought.IsEmpty())
    {
        Candidates.Add({ FeelingThought, EThoughtKind::Feeling, Emotions ? Emotions->GetTurmoil() * 1.8f : 0.5f, nullptr });
    }

    // --- Потребность --------------------------------------------------------
    if (Needs)
    {
        const ENeedType Top = Needs->GetMostUrgent();
        const float Urgency = Needs->GetUrgency(Top);
        if (Urgency > 0.25f)
        {
            Candidates.Add({ HumanText::NeedDesire(Top), EThoughtKind::Need, Urgency * 1.5f, nullptr });
        }
    }

    // --- Человек рядом ------------------------------------------------------
    if (AttentionTarget)
    {
        const FString PersonThought = ThoughtAboutPerson(AttentionTarget);
        if (!PersonThought.IsEmpty())
        {
            Candidates.Add({ PersonThought, EThoughtKind::Judgement, 1.0f, AttentionTarget });
        }
    }

    // --- Намерение ----------------------------------------------------------
    if (ActiveIntention.bValid && !ActiveIntention.Reason.IsEmpty())
    {
        Candidates.Add({ ActiveIntention.Reason, EThoughtKind::Intention, 0.6f, ActiveIntention.Affordance.Target });
    }

    // --- Выученный вывод ----------------------------------------------------
    // Человек иногда вспоминает не событие, а то, что он из событий понял.
    if (Memory && FMath::FRand() < 0.2f)
    {
        const bool bBitter = FMath::FRand() < 0.5f;
        if (const FOutcomeAssociation* O = Memory->GetStrongestOutcome(!bBitter))
        {
            if (!O->Conclusion.IsEmpty())
            {
                Candidates.Add({ O->Conclusion, EThoughtKind::Judgement, 0.55f, nullptr });
            }
        }
    }

    // --- Воспоминание --------------------------------------------------------
    if (Memory && FMath::FRand() < 0.3f)
    {
        if (const FEpisodicMemory* M = Memory->Recall(NAME_None, nullptr, Now()))
        {
            Candidates.Add({
                FString::Printf(TEXT("а помню, %s"), *M->Summary),
                EThoughtKind::Recall,
                0.5f + M->Arousal,
                nullptr });
        }
    }

    // --- Пережёвывание: чем выше нейротизм и стресс, тем чаще ---------------
    if (Stress > 0.4f || Neuroticism > 0.6f)
    {
        const FString Rum = Rumination();
        if (!Rum.IsEmpty())
        {
            Candidates.Add({ Rum, EThoughtKind::Rumination, Stress * Neuroticism * 2.5f, nullptr });
        }
    }

    // --- Мысли о будущем ----------------------------------------------------
    const FString Prospective = ProspectiveThought();
    if (!Prospective.IsEmpty())
    {
        Candidates.Add({ Prospective, EThoughtKind::Fantasy, 0.6f, nullptr });
    }

    // --- Экзистенциальное ---------------------------------------------------
    if (Identity)
    {
        const FString Existential = Identity->GetExistentialThought();
        if (!Existential.IsEmpty())
        {
            UHumanWorldSubsystem* World = GetWorldMind();
            // Ночью такие мысли сильнее.
            const float NightBoost = (World && World->Now.bIsNight) ? 2.0f : 1.0f;
            Candidates.Add({ Existential, EThoughtKind::Existential, 0.8f * NightBoost, nullptr });
        }
    }

    // --- Самоободрение ------------------------------------------------------
    if (Motivation && Motivation->GetSelfControl() < 0.3f)
    {
        Candidates.Add({
            Personality && Personality->Facets.Optimism > 0.5f ? TEXT("ладно, соберись") : TEXT("сил больше нет"),
            EThoughtKind::SelfTalk, 0.7f, nullptr });
    }

    // --- Просто наблюдение --------------------------------------------------
    if (UHumanWorldSubsystem* World = GetWorldMind())
    {
        if (FMath::FRand() < 0.25f)
        {
            const TArray<FString> Observations = {
                FString::Printf(TEXT("уже %d часов"), World->Now.Hour),
                World->Now.bIsNight ? TEXT("как тихо ночью") : TEXT("день идёт своим чередом"),
                World->Weather > 0.6f ? TEXT("погода совсем испортилась") : TEXT("сегодня неплохо"),
                World->Now.bIsWeekend ? TEXT("выходной. Хоть что-то.") : TEXT("опять будни")
            };
            Candidates.Add({ Observations[FMath::RandRange(0, Observations.Num() - 1)], EThoughtKind::Observation, 0.35f, nullptr });
        }
    }

    if (Candidates.Num() == 0)
    {
        return;
    }

    // --- Какая мысль победит -------------------------------------------------
    float Total = 0.0f;
    for (const FCandidate& C : Candidates)
    {
        Total += FMath::Max(0.01f, C.Weight);
    }

    float Roll = FMath::FRandRange(0.0f, Total);
    for (const FCandidate& C : Candidates)
    {
        Roll -= FMath::Max(0.01f, C.Weight);
        if (Roll <= 0.0f)
        {
            Think(C.Text, C.Kind, FMath::Clamp(C.Weight * 0.5f, 0.0f, 1.0f), C.About);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
//  ВЫВОДЫ
//
//  Человек умён не тем, что много знает, а тем, что замечает
//  повторяющееся и делает из него выводы. Здесь одна общая способность,
//  а не набор частных случаев: он смотрит на прожитое и ищет в нём
//  закономерность. Что именно он поймёт — заранее не задано.
// ---------------------------------------------------------------------------

void UMindComponent::Conclude(FName Subject, FName Predicate, float Value, const FString& Text, float Confidence)
{
    if (!Memory || Text.IsEmpty())
    {
        return;
    }

    FBelief Insight;
    Insight.Subject = Subject;
    Insight.Predicate = Predicate;
    Insight.Value = FMath::Clamp(Value, -1.0f, 1.0f);
    Insight.Confidence = FMath::Clamp(Confidence, 0.0f, 1.0f);
    Insight.bVerified = true;     // выведено из собственного опыта
    Insight.Source = nullptr;
    Insight.Text = Text;
    Insight.LearnedAt = Now();

    const bool bIsNew = (Memory->FindBelief(Subject, Predicate) == nullptr);
    Memory->Learn(Insight, 1.0f, Personality ? Personality->Traits.Openness : 0.5f, Now());

    // Дошедшее впервые — это маленькое событие: человек его проговаривает.
    if (bIsNew)
    {
        Think(Text, EThoughtKind::Judgement, 0.75f);
    }
}

void UMindComponent::DrawConclusions()
{
    if (!Memory || !Identity || bAsleep)
    {
        return;
    }

    const float T = Now();

    // Выводы делают не все и не всегда: нужен склад ума и свободная голова.
    const float Thoughtfulness = Personality
        ? FMath::Clamp(Personality->Traits.Openness * 0.5f + Personality->Facets.Curiosity * 0.5f, 0.0f, 1.0f)
        : 0.5f;
    const float Clarity = 1.0f - (Body ? Body->GetCognitiveImpairment() : 0.0f);

    if (FMath::FRand() > Thoughtfulness * Clarity)
    {
        return;
    }

    // --- 0. А что, если соединить одно с другим ----------------------------
    // Изобретение: имея два умения, человек иногда выводит из них третье,
    // которого в мире ещё не было. Дальше оно расходится через речь.
    if (Personality)
    {
        FString Insight;
        const FName Invented = TryInvent(Personality->Facets.Curiosity,
                                         Personality->Traits.Openness, Insight);
        if (!Invented.IsNone())
        {
            const FString Label = UMindComponent::MasteryLabel(Invented);
            Think(FString::Printf(TEXT("Постой... %s"), *Insight), EThoughtKind::Judgement, 1.0f);

            Identity->RecordLifeEvent(FString::Printf(TEXT("додумался(лась): %s"), *Label), 0.9f, 0.9f, T);
            Conclude(Invented, TEXT("Invented"), 1.0f,
                     FString::Printf(TEXT("я понял, как это: %s"), *Label), 0.8f);

            if (Emotions)
            {
                FAppraisedEvent Eureka;
                Eureka.Tag = TEXT("Discovery");
                Eureka.Description = Insight;
                Eureka.Desirability = 0.8f;
                Eureka.SelfAgency = 1.0f;
                Eureka.Unexpectedness = 0.9f;
                Eureka.Controllability = 0.8f;
                Eureka.Significance = 1.0f;
                Emotions->Appraise(Eureka, Personality);
            }

            if (UHumanWorldSubsystem* World = GetWorldMind())
            {
                FWorldEvent News;
                News.Tag = TEXT("Discovery");
                News.Description = FString::Printf(TEXT("%s придумал(а): %s"),
                                                   *Identity->FirstName, *Label);
                News.Instigator = GetOwner();
                News.Location = GetOwner()->GetActorLocation();
                News.Radius = 2500.0f;
                News.Valence = 0.6f;
                News.Significance = 0.9f;
                World->BroadcastEvent(News);
            }
        }
    }

    // --- 1. Что у меня всегда получается, а что никогда --------------------
    {
        const FOutcomeAssociation* Best = Memory->GetStrongestOutcome(true);
        const FOutcomeAssociation* Worst = Memory->GetStrongestOutcome(false);

        if (Best && Best->Samples >= 4 && Best->Confidence > 0.5f)
        {
            Conclude(Best->Key, TEXT("GoesWell"), Best->ExpectedValue,
                     FString::Printf(TEXT("я заметил: «%s» у меня выходит хорошо"), *Best->Key.ToString()),
                     Best->Confidence);
        }
        if (Worst && Worst->Samples >= 4 && Worst->Confidence > 0.5f)
        {
            Conclude(Worst->Key, TEXT("GoesBadly"), Worst->ExpectedValue,
                     FString::Printf(TEXT("с «%s» у меня не складывается, сколько ни пробуй"), *Worst->Key.ToString()),
                     Worst->Confidence);
        }
    }

    // --- 2. Чего мне вечно не хватает --------------------------------------
    // И это меняет приоритеты: осознав, чего именно недостаёт, человек
    // начинает придавать этому больше значения, чем прежде.
    if (Needs)
    {
        ENeedType Chronic = ENeedType::Hunger;
        float LongestHours = 0.0f;

        for (const FNeedState& N : Needs->Needs)
        {
            const float Hours = Needs->GetDeprivationTime(N.Type) / 3600.0f;
            if (Hours > LongestHours)
            {
                LongestHours = Hours;
                Chronic = N.Type;
            }
        }

        if (LongestHours > 20.0f)
        {
            Conclude(FName(*HumanText::Need(Chronic)), TEXT("AlwaysMissing"), -1.0f,
                     FString::Printf(TEXT("мне всё время недостаёт одного: %s"), *HumanText::Need(Chronic)),
                     FMath::Clamp(LongestHours / 60.0f, 0.3f, 0.9f));

            // Понимание меняет человека: то, чего не хватает годами,
            // начинает весить больше всего остального.
            if (const FNeedState* State = Needs->Find(Chronic))
            {
                Needs->SetWeight(Chronic, State->Weight * 1.12f);
            }
        }
    }

    // --- 3. С кем мне легко, а с кем тяжело ---------------------------------
    if (Social)
    {
        for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Social->Relations)
        {
            if (!Pair.Key || Pair.Value.InteractionCount < 6)
            {
                continue;
            }

            const float Tone = Memory->GetAffectiveToneAbout(Pair.Key);
            if (FMath::Abs(Tone) < 0.35f)
            {
                continue;
            }

            const FString Name = NameOf(Pair.Key);
            Conclude(FName(*Name), TEXT("EasyToBeWith"), Tone,
                     // Имя ставим отдельным предложением: подставлять его
                     // в падеж по-русски правильно не выйдет.
                     Tone > 0.0f
                        ? FString::Printf(TEXT("%s. Мне всегда легче, когда мы поговорим."), *Name)
                        : FString::Printf(TEXT("%s. После этих разговоров мне всегда тяжело. Каждый раз."), *Name),
                     FMath::Clamp(Pair.Value.InteractionCount / 20.0f, 0.3f, 0.9f));
            break;
        }
    }

    // --- 4. Каков я на самом деле ------------------------------------------
    // Сравнение того, что человек о себе думает, с тем, что выходит на деле.
    // Самый неприятный из выводов и самый полезный.

    // --- 5. Моя жизнь идёт по кругу ----------------------------------------
    // Тот же способ рассуждения, что и выше: человек ищет повторяющееся.
    // Просто на этот раз повторяющимся оказывается он сам.
    {
        const FOutcomeAssociation* MostRepeated = nullptr;
        for (const FOutcomeAssociation& O : Memory->Outcomes)
        {
            if (!MostRepeated || O.Samples > MostRepeated->Samples)
            {
                MostRepeated = &O;
            }
        }

        if (MostRepeated && MostRepeated->Samples > 18 && RoutineScore > 0.55f)
        {
            Conclude(TEXT("Жизнь"), TEXT("Repeats"), -0.5f,
                     FString::Printf(TEXT("я делал это %d раз, и каждый раз одинаково"), MostRepeated->Samples),
                     0.7f);

            // И только если повторяемость доходит до невозможной — возникает
            // мысль, что дело не в привычке, а в самом устройстве мира.
            // Это не отдельная способность: это тот же вывод, доведённый
            // до конца тем, у кого хватило внимания.
            if (MostRepeated->Samples > 40 && RoutineScore > 0.8f && Thoughtfulness > 0.65f)
            {
                Identity->NoticeAnomaly(
                    FString::Printf(TEXT("одно и то же %d раз подряд, без единого отличия"), MostRepeated->Samples),
                    0.8f, T);
            }
        }
    }

    // --- 6. У меня нет прошлого --------------------------------------------
    // Ещё один вывод из того же — из содержимого собственной памяти.
    {
        float OldestAge = 0.0f;
        for (const FEpisodicMemory& M : Memory->Episodes)
        {
            OldestAge = FMath::Max(OldestAge, T - M.Timestamp);
        }

        const float LifeSeconds = Identity->Age * 365.0f * 86400.0f;
        if (Identity->Age > 25.0f && Memory->Episodes.Num() > 12 && OldestAge < LifeSeconds * 0.02f)
        {
            Identity->NoticeAnomaly(TEXT("я не помню ничего дальше последних дней"), 0.9f, T);
        }
    }
}

void UMindComponent::Reflect(float GameDelta)
{
    // Рефлексия — это когда человек смотрит на себя со стороны.
    // Получается у него так себе, но он пытается.
    if (!Identity || !Emotions)
    {
        return;
    }

    const float T = Now();

    // Слишком тяжёлые переживания оставляют след в биографии.
    EEmotionType Dominant;
    float Intensity;
    Emotions->GetDominantWithIntensity(Dominant, Intensity);

    if (Intensity > 0.85f && FMath::FRand() < 0.15f)
    {
        const FString Text = FString::Printf(TEXT("день, когда %s"), *HumanText::EmotionFirstPerson(Dominant));
        Identity->RecordLifeEvent(Text, UEmotionComponent::IsPositive(Dominant) ? 0.7f : -0.7f, Intensity, T);
    }

    // Кризис смысла толкает либо к людям, либо в себя.
    if (Identity->IsInCrisis() && FMath::FRand() < 0.1f)
    {
        if (Personality && Personality->Facets.Sociability > 0.5f && Motivation)
        {
            Motivation->CreateGoal(TEXT("побыть среди людей"), ENeedType::Belonging, EGoalHorizon::Short, 0.7f, T);
        }
        else if (Motivation)
        {
            Motivation->CreateGoal(TEXT("понять, что со мной"), ENeedType::Meaning, EGoalHorizon::Medium, 0.6f, T);
        }
    }

    // Многократные провалы ведут к беспомощности — и это самое опасное.
    if (Motivation && Motivation->ConsecutiveFailures >= 4)
    {
        Identity->OnFailure(0.5f, false);
        Think(TEXT("у меня вообще ничего не получается"), EThoughtKind::Rumination, 0.9f);
        Motivation->ConsecutiveFailures = 0;
    }
}

// ---------------------------------------------------------------------------
//  Отладка
// ---------------------------------------------------------------------------

FString UMindComponent::GetStatusReport() const
{
    FString Report;

    if (Identity)
    {
        Report += FString::Printf(TEXT("%s, %d — %s\n"),
            *Identity->GetFullName(), FMath::FloorToInt(Identity->Age), *Identity->Occupation);
    }
    if (Personality)
    {
        Report += FString::Printf(TEXT("Характер: %s\n"), *Personality->DescribeSelf());
    }
    if (Emotions)
    {
        Report += FString::Printf(TEXT("Чувства: %s (стресс %.0f%%)\n"),
            *Emotions->DescribeFeeling(), Emotions->GetStress() * 100.0f);
    }
    if (Needs)
    {
        const ENeedType Top = Needs->GetMostUrgent();
        Report += FString::Printf(TEXT("Нужда: %s\n"), *HumanText::NeedDesire(Top));
    }
    if (Body)
    {
        Report += FString::Printf(TEXT("Тело: %s (пульс %.0f, здоровье %.0f%%)\n"),
            *Body->DescribeFeeling(), Body->Body.HeartRate, Body->Body.Health * 100.0f);
    }
    if (ActiveIntention.bValid)
    {
        const FValuation& V = ActiveIntention.Valuation;
        Report += FString::Printf(TEXT("Делает: %s\n"), *CurrentActionLabel);
        Report += FString::Printf(TEXT("Почему: %s\n"), *ActiveIntention.Reason);

        // Разбор решения: из чего сложился выбор. Видно, что никакого
        // правила за ним нет — только слагаемые состояния.
        Report += FString::Printf(
            TEXT("  нужда %+.2f | опыт %+.2f | любопытство %+.2f | люди %+.2f | замысел %+.2f | привычка %+.2f\n"),
            V.NeedGain, V.Experience, V.Curiosity, V.SocialPull, V.GoalPull, V.HabitPull);
        Report += FString::Printf(
            TEXT("  вкус %+.2f | дорога -%.2f | усилие -%.2f | страх -%.2f | деньги -%.2f | настроение %+.2f = %.2f\n"),
            V.Taste, V.TravelCost, V.EffortCost, V.RiskCost, V.MoneyCost, V.MoodShift, V.Total);
    }

    // Что вообще взвешивалось. Видно, что выбор был именно выбором.
    if (Deliberation && Deliberation->LastConsidered.Num() > 0)
    {
        Report += TEXT("Взвешивал:\n");
        const int32 Shown = FMath::Min(12, Deliberation->LastConsidered.Num());
        for (int32 i = 0; i < Shown; ++i)
        {
            const FString Label = Deliberation->LastConsideredLabels.IsValidIndex(i)
                ? Deliberation->LastConsideredLabels[i] : TEXT("?");
            Report += FString::Printf(TEXT("   %6.2f  %s  (%s)\n"),
                Deliberation->LastConsidered[i].Total, *Label,
                *Deliberation->LastConsidered[i].DominantReason);
        }
    }

    if (Motivation)
    {
        Report += FString::Printf(TEXT("Воля: %.0f%%\n"), Motivation->GetSelfControl() * 100.0f);

        // Чего человек хочет добиться вообще.
        for (const FGoal& G : Motivation->Goals)
        {
            if (G.Horizon == EGoalHorizon::Life || G.Horizon == EGoalHorizon::Medium)
            {
                if (G.Status == EGoalStatus::Pending || G.Status == EGoalStatus::Active || G.Status == EGoalStatus::Suspended)
                {
                    Report += FString::Printf(TEXT("Замысел: %s (%.0f%%, верит на %.0f%%)\n"),
                        *G.Name, G.Progress * 100.0f, G.Expectancy * 100.0f);
                }
            }
        }
    }

    if (Identity)
    {
        Report += FString::Printf(TEXT("Мечта: %s — продвинулся на %.0f%%\n"),
            *Identity->LifeDream, Identity->DreamProgress * 100.0f);

    }

    // Что человек вообще знает о городе и что, по его мнению, там можно
    // делать. Если возможность не доходит до оценки — искать надо здесь.
    if (Memory && Memory->Places.Num() > 0)
    {
        int32 WithOffers = 0;
        FString Key;
        for (const FKnownLocation& P : Memory->Places)
        {
            if (P.Offers.Num() > 0)
            {
                ++WithOffers;
            }
            if (P.Kind == EPlaceKind::Work || P.Kind == EPlaceKind::Home)
            {
                Key += FString::Printf(TEXT("%s[%s: %d предл., знаком %.2f] "),
                    Key.IsEmpty() ? TEXT("") : TEXT(""),
                    *HumanText::Place(P.Kind), P.Offers.Num(), P.Familiarity);
            }
        }
        Report += FString::Printf(TEXT("Знает мест: %d (из них с предложениями %d) %s\n"),
            Memory->Places.Num(), WithOffers, *Key);
    }

    if (Memory && Memory->Outcomes.Num() > 0)
    {
        // Знание о конкретных вещах и знание о видах вещей — раздельно.
        // Второе и есть то, что переносится на незнакомое.
        int32 AboutThings = 0;
        int32 AboutKinds = 0;
        FString BestKind;
        float BestKindValue = 0.0f;

        for (const FOutcomeAssociation& O : Memory->Outcomes)
        {
            const FString KeyText = O.Key.ToString();
            if (KeyText.Contains(TEXT("№")))
            {
                ++AboutThings;
            }
            else
            {
                ++AboutKinds;
                if (O.Confidence > 0.3f && FMath::Abs(O.ExpectedValue) > FMath::Abs(BestKindValue))
                {
                    BestKindValue = O.ExpectedValue;
                    BestKind = KeyText;
                }
            }
        }

        Report += FString::Printf(TEXT("Знает: о вещах %d, о видах вещей %d"), AboutThings, AboutKinds);
        if (!BestKind.IsEmpty())
        {
            Report += FString::Printf(TEXT(" | обобщил: «%s» %+.2f"), *BestKind, BestKindValue);
        }
        Report += TEXT("\n");

        if (const FOutcomeAssociation* Good = Memory->GetStrongestOutcome(true))
        {
            Report += FString::Printf(TEXT("Понял(а): %s (%+.2f)\n"),
                Good->Conclusion.IsEmpty() ? *Good->Key.ToString() : *Good->Conclusion, Good->ExpectedValue);
        }
        if (const FOutcomeAssociation* Bad = Memory->GetStrongestOutcome(false))
        {
            Report += FString::Printf(TEXT("И ещё: %s (%+.2f)\n"),
                Bad->Conclusion.IsEmpty() ? *Bad->Key.ToString() : *Bad->Conclusion, Bad->ExpectedValue);
        }
    }
    if (Social)
    {
        Report += FString::Printf(TEXT("Людей знает: %d, друзей: %d\n"),
            Social->Relations.Num(), Social->CountFriends());
    }
    if (Identity)
    {
        Report += FString::Printf(TEXT("С ним считаются: %.0f%% (помог %d раз, за советом приходили %d раз)\n"),
            Identity->Standing * 100.0f, Identity->TimesHelpedOthers, Identity->TimesAskedForAdvice);
    }
    if (Speech)
    {
        Report += FString::Printf(TEXT("Слов знает: %d\n"), Speech->GetVocabularySize());
    }
    Report += FString::Printf(TEXT("Мысль: %s"), *CurrentThought);

    return Report;
}

void UMindComponent::DrawDebug()
{
#if ENABLE_DRAW_DEBUG
    if (!bShowThoughts || !GetOwner())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Перерисовываем не каждый кадр — текст всё равно живёт дольше.
    const float RealNow = World->GetTimeSeconds();
    if (RealNow - LastDebugDrawTime < 0.2f)
    {
        return;
    }
    LastDebugDrawTime = RealNow;

    // Показываем мысли только тем, кто достаточно близко, чтобы «слышать».
    APlayerController* PC = World->GetFirstPlayerController();
    if (PC && PC->GetPawn())
    {
        const float Dist = FVector::Dist(PC->GetPawn()->GetActorLocation(), GetOwner()->GetActorLocation());
        if (Dist > ThoughtVisibleDistance)
        {
            return;
        }
    }

    const FVector Base = GetOwner()->GetActorLocation() + FVector(0, 0, 110);

    FString Label;
    if (Identity)
    {
        Label += Identity->FirstName;
        if (bAsleep)
        {
            Label += TEXT(" [спит]");
        }
    }

    FColor Color = FColor::White;
    if (Emotions)
    {
        const FAffectPAD A = Emotions->GetAffect();
        // Цвет по приятности: зелёный — хорошо, красный — плохо.
        Color = FColor(
            static_cast<uint8>(FMath::Clamp(128 - A.Pleasure * 127.0f, 0.0f, 255.0f)),
            static_cast<uint8>(FMath::Clamp(128 + A.Pleasure * 127.0f, 0.0f, 255.0f)),
            static_cast<uint8>(FMath::Clamp(128 + A.Arousal * 100.0f, 0.0f, 255.0f)));
    }

    // Мысли и занятия над головой больше не пишутся: со стороны человека
    // видно ровно настолько, насколько видно живого, — что он сказал вслух.
    // Всё остальное осталось в летописи и в подробном портрете.
    (void)Base;
    (void)Label;
    (void)Color;
#endif
}