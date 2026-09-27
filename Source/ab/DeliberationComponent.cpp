// DeliberationComponent.cpp

#include "DeliberationComponent.h"

#include "NeedComponent.h"
#include "MemoryComponent.h"
#include "PersonalityComponent.h"
#include "EmotionComponent.h"
#include "MotivationComponent.h"
#include "MindComponent.h"
#include "IdentityComponent.h"
#include "SocialComponent.h"
#include "PhysiologyComponent.h"
#include "Crafts.h"

UDeliberationComponent::UDeliberationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDeliberationComponent::Clear()
{
    Current = FIntention();
}

FName UDeliberationComponent::MakeKey(const FAffordance& A)
{
    return A.Key;
}

// ---------------------------------------------------------------------------
//  ОЦЕНКА — здесь решается всё
// ---------------------------------------------------------------------------

FValuation UDeliberationComponent::Evaluate(const FAffordance& A, const FDeliberationContext& Ctx) const
{
    FValuation V;

    // =======================================================================
    //  1. ЧТО ЭТО МНЕ ДАСТ
    //  Не «это еда, а я голоден», а «это обещает +0.8 сытости, а сытость
    //  мне сейчас нужна вот настолько». Если сытость не нужна — обещание
    //  не стоит ничего, каким бы щедрым оно ни было.
    // =======================================================================
    float StrongestNeed = 0.0f;
    ENeedType StrongestNeedType = ENeedType::Hunger;

    if (Ctx.Needs)
    {
        // Сколько пройдёт времени, прежде чем дело будет сделано: дорога
        // плюс само занятие. К тому моменту человек будет уже не тем,
        // что сейчас, — и считать надо по тому, каким он станет.
        const float TravelSeconds = A.bHasLocation
            ? FVector::Dist2D(Ctx.SelfLocation, A.Location) / FMath::Max(50.0f, Ctx.WalkSpeed)
            : 0.0f;
        const float Horizon = TravelSeconds + A.Duration * 0.5f;

        for (const FNeedPromise& Promise : A.Promises)
        {
            // Живое действует не по тому, что с ним сейчас, а по тому,
            // что с ним будет. Сытый идёт за едой, зная, что к вечеру
            // проголодается, а магазин к тому времени закроется.
            const float NowUrgency = Ctx.Needs->GetUrgency(Promise.Need);
            const float ThenUrgency = Ctx.Needs->ProjectUrgency(Promise.Need, Horizon);
            const float Urgency = FMath::Max(NowUrgency, ThenUrgency * 0.85f);

            const float Satisfaction = Ctx.Needs->ProjectSatisfaction(Promise.Need, Horizon);

            if (Promise.Amount > 0.0f)
            {
                // Больше, чем не хватает, получить нельзя: сытому вторая
                // тарелка почти ничего не добавляет.
                const float Headroom = 1.0f - Satisfaction;
                const float Useful = FMath::Min(Promise.Amount, Headroom);
                const float Gain = Urgency * Useful;

                V.NeedGain += Gain;

                if (Gain > StrongestNeed)
                {
                    StrongestNeed = Gain;
                    StrongestNeedType = Promise.Need;
                }
            }
            else
            {
                // Это что-то отнимет. Тем больнее, чем нужнее отнимаемое.
                V.NeedGain += Promise.Amount * (0.4f + Urgency * 1.2f);
            }
        }
    }

    // =======================================================================
    //  2. ЧЕМ ЭТО КОНЧАЛОСЬ РАНЬШЕ
    //  Единственный источник — прожитый опыт. Ничего не задано заранее.
    // =======================================================================
    float Confidence = 0.0f;
    if (Ctx.Memory)
    {
        // Опыт берётся и с этой самой вещи, и с вещей ей подобных.
        // Незнакомое кафе не «неизвестность»: человек уже знает,
        // чего примерно ждать от кафе вообще.
        const float Learned = Ctx.Memory->GetExpectedOutcomeGeneralised(MakeKey(A), A.CategoryKey, Confidence);
        V.Experience = Learned * Confidence * 1.3f;

        // ПРЕСЫЩЕНИЕ. Без него человек, нашедший одно приятное занятие,
        // остался бы в нём навсегда: диван каждый раз честно приносит покой.
        // Живому надоедает — и поэтому он встаёт и идёт делать другое.
        if (const FOutcomeAssociation* O = Ctx.Memory->FindOutcome(MakeKey(A)))
        {
            // Тяга к новизне усиливает пресыщение: непоседе приедается быстрее.
            const float Restless = Ctx.Personality
                ? (Ctx.Personality->Traits.Openness * 0.5f + Ctx.Personality->Values.Stimulation * 0.5f)
                : 0.5f;
            V.Experience -= O->Satiation * (0.45f + Restless * 0.7f);
        }
    }

    // =======================================================================
    //  3. ЛЮБОПЫТСТВО К НЕИЗВЕСТНОМУ
    //  Неизвестное притягивает тем сильнее, чем человек любознательнее
    //  и чем сильнее ему надоело привычное.
    // =======================================================================
    {
        const float Unknown = 1.0f - Confidence;
        const float Curious = Ctx.Personality ? Ctx.Personality->Facets.Curiosity : 0.5f;
        const float NoveltyHunger = Ctx.Needs ? Ctx.Needs->GetUrgency(ENeedType::Novelty) : 0.3f;
        const float Boredom = Ctx.Emotions ? Ctx.Emotions->GetIntensity(EEmotionType::Boredom) : 0.0f;

        V.Curiosity = Unknown * Curious * (0.25f + NoveltyHunger * 0.6f + Boredom * 0.5f) * 0.6f;

        // Но тревожного неизвестное скорее отпугивает.
        const float Anxiety = Ctx.Personality ? Ctx.Personality->Facets.TraitAnxiety : 0.4f;
        V.Curiosity -= Unknown * Anxiety * 0.25f;
    }

    // =======================================================================
    //  4. ЦЕНА ДОРОГИ
    //  Считается от настоящей усталости и настоящей скорости, а не «далеко/близко».
    // =======================================================================
    if (A.bHasLocation)
    {
        const float Distance = FVector::Dist2D(Ctx.SelfLocation, A.Location);
        const float Stamina = Ctx.Body ? Ctx.Body->Body.Stamina : 1.0f;
        const float Pain = Ctx.Body ? Ctx.Body->Body.Pain : 0.0f;

        // Дорога стоит ровно столько, сколько занимает ВРЕМЕНИ. Считать
        // в метрах неверно: двести метров человек проходит не задумываясь,
        // а через весь город идти не хочет — разница именно во времени.
        const float TravelHours = (Distance / FMath::Max(50.0f, Ctx.WalkSpeed)) / 3600.0f;

        // Усталость и боль делают каждый шаг дороже.
        const float Fatigue = 1.0f + (1.0f - Stamina) * 2.2f + Pain * 2.0f;

        // Плюс маленькая цена самого «надо куда-то тащиться».
        V.TravelCost = TravelHours * 1.6f * Fatigue + (Distance > 400.0f ? 0.02f : 0.0f);

        // Совсем без сил дальний путь становится почти непреодолимым.
        if (Stamina < 0.2f && TravelHours > 0.1f)
        {
            V.TravelCost *= 2.0f;
            V.Blocker = EBlocker::Stamina;   // «туда бы дойти, да ноги не идут»
        }
    }

    // =======================================================================
    //  5. ЦЕНА УСИЛИЯ
    //  Одно и то же дело уставшему человеку кажется неподъёмным.
    //  Отсюда и берётся прокрастинация — не как правило, а как следствие.
    // =======================================================================
    {
        const float Will = Ctx.Motivation ? Ctx.Motivation->GetSelfControl() : 0.6f;
        const float Impair = FMath::Clamp(Ctx.Impairment, 0.0f, 1.0f);
        V.EffortCost = A.EffortCost * (0.4f + (1.0f - Will) * 1.8f + Impair * 0.8f);
    }

    // =======================================================================
    //  6. СТРАХ
    // =======================================================================
    if (A.Risk > 0.0f)
    {
        const float Anxiety = Ctx.Personality ? Ctx.Personality->Facets.TraitAnxiety : 0.4f;
        const float Daring = Ctx.Personality ? Ctx.Personality->Facets.RiskTaking : 0.5f;
        const float Fear = Ctx.Emotions ? Ctx.Emotions->GetIntensity(EEmotionType::Fear) : 0.0f;
        const float SafetyNeed = Ctx.Needs ? Ctx.Needs->GetUrgency(ENeedType::Safety) : 0.3f;

        V.RiskCost = A.Risk * (0.4f + Anxiety * 1.1f + Fear * 1.4f + SafetyNeed * 0.8f) * (1.4f - Daring);
    }

    // =======================================================================
    //  7. ДЕНЬГИ
    // =======================================================================
    if (A.MoneyCost > 0.0f)
    {
        if (A.MoneyCost > Ctx.Money)
        {
            // Не по карману. Вариант отпадает — но человек о нём подумал,
            // и запомнил, ЧЕГО именно ему не хватило. Из этого потом
            // родится намерение сходить заработать.
            V.MoneyCost = 10.0f;
            V.Blocker = EBlocker::Money;
        }
        else
        {
            // Последние деньги тратятся тяжелее, чем лишние.
            const float Share = A.MoneyCost / FMath::Max(1.0f, Ctx.Money);
            V.MoneyCost = Share * (0.3f + (Ctx.Needs ? Ctx.Needs->GetUrgency(ENeedType::Money) : 0.3f));
        }
    }

    // =======================================================================
    //  8. ЛЮДИ
    //  К близким тянет, от неприятных отталкивает — и это не отдельное
    //  правило, а просто ещё одно слагаемое той же оценки.
    // =======================================================================
    if (A.Source == EAffordanceSource::Person && A.Target && Ctx.Social)
    {
        const FRelationship* R = Ctx.Social->Find(A.Target);
        if (R)
        {
            // К близким тянет — но не сильнее, чем есть хочется. Человек,
            // для которого общение перевешивает голод и сон, называется
            // иначе; здесь тяга к людям соразмерна остальным нуждам.
            const float Closeness = Ctx.Social->GetCloseness(A.Target);
            V.SocialPull += Closeness * 0.35f;
            V.SocialPull += R->Liking * 0.20f;
            V.SocialPull -= R->Resentment * 0.55f;
            V.SocialPull -= R->Fear * 0.90f;
            V.SocialPull += FMath::Max(0.0f, R->Debt) * 0.20f;     // я ему должен
            V.SocialPull += R->Romantic * 0.25f;

            // И тем слабее, чем меньше человеку сейчас нужно общение:
            // сытый разговором не пойдёт разговаривать снова.
            const float SocialHunger = Ctx.Needs
                ? FMath::Clamp(0.35f + Ctx.Needs->GetUrgency(ENeedType::SocialContact), 0.35f, 1.6f)
                : 1.0f;
            V.SocialPull *= SocialHunger;
        }

        // Замкнутому общение само по себе стоит усилий.
        const float Sociability = Ctx.Personality ? Ctx.Personality->Facets.Sociability : 0.5f;
        V.EffortCost += (1.0f - Sociability) * 0.25f;
    }

    // =======================================================================
    //  9. ДОЛГОСРОЧНЫЕ ЗАМЫСЛЫ
    //  Мечта не приказывает, а подталкивает: всё, что к ней ведёт,
    //  кажется чуть привлекательнее.
    // =======================================================================
    if (Ctx.Motivation)
    {
        for (const FGoal& Goal : Ctx.Motivation->Goals)
        {
            if (Goal.Status == EGoalStatus::Achieved || Goal.Status == EGoalStatus::Failed
                || Goal.Status == EGoalStatus::Abandoned)
            {
                continue;
            }

            for (const FNeedPromise& Promise : A.Promises)
            {
                if (Promise.Need != Goal.DrivingNeed || Promise.Amount <= 0.0f)
                {
                    continue;
                }

                float Weight = Goal.Importance * Promise.Amount * 0.5f;

                // Далёкая цель тянет слабее — и тем слабее, чем нетерпеливее человек.
                const float Patience = Ctx.Personality ? Ctx.Personality->Patience() : 0.5f;
                switch (Goal.Horizon)
                {
                case EGoalHorizon::Short:  Weight *= FMath::Lerp(0.5f, 0.9f, Patience); break;
                case EGoalHorizon::Medium: Weight *= FMath::Lerp(0.25f, 0.75f, Patience); break;
                case EGoalHorizon::Life:   Weight *= FMath::Lerp(0.12f, 0.6f, Patience); break;
                default: break;
                }

                V.GoalPull += Weight;
            }
        }
    }

    // =======================================================================
    //  9а. ЭТО НУЖНО ДЛЯ ТОГО, ЧТО Я УМЕЮ
    //
    //  Кто вычитал, как делается вино, тянется за виноградом. Знание тянет
    //  по цепочке производства: ремесло без материала — пустые руки.
    //  Поэтому работа, дающая нужный материал, получает часть ценности
    //  того, ради чего материал нужен.
    // =======================================================================
    if (Ctx.WantedInputs.Num() > 0)
    {
        EResourceKind Made = A.Produces;
        if (Made == EResourceKind::None && !A.Craft.IsNone())
        {
            if (const FCraft* Deed = FCraftBook::Find(A.Craft))
            {
                Made = Deed->Output;
            }
        }
        if (Made != EResourceKind::None)
        {
            if (const float* Want = Ctx.WantedInputs.Find(Made))
            {
                V.GoalPull += *Want;
                if (V.DominantReason.IsEmpty())
                {
                    V.DominantReason = FString::Printf(TEXT("надо для дела: %s"), *FCraftBook::NameOfKind(Made));
                }
            }
        }
    }

    // =======================================================================
    //  10. ПРИВЫЧКА
    //  10. ПРИВЫЧКА
    //  То, что делалось в этот час изо дня в день, тянет само, без доводов.
    // =======================================================================
    if (Ctx.Memory)
    {
        const FName Context = FName(*FString::Printf(TEXT("H%d"), FMath::FloorToInt(Ctx.HourOfDay)));
        EActionType HabitAction;
        float HabitStrength = 0.0f;
        if (Ctx.Memory->GetHabitualAction(Context, HabitAction, HabitStrength) && HabitAction == A.Action)
        {
            // Привычка тем сильнее правит, чем меньше осталось воли думать.
            const float Will = Ctx.Motivation ? Ctx.Motivation->GetSelfControl() : 0.6f;
            V.HabitPull = HabitStrength * (0.35f + (1.0f - Will) * 0.8f);
        }
    }

    // =======================================================================
    //  11. НАСТРОЕНИЕ ОКРАШИВАЕТ ВСЁ
    //  В подавленности привлекательность будущего блекнет — это ангедония,
    //  и из неё сама собой следует апатия.
    // =======================================================================
    if (Ctx.Emotions)
    {
        const FAffectPAD Affect = Ctx.Emotions->GetAffect();

        if (Affect.Pleasure < 0.0f)
        {
            const float Anhedonia = FMath::Clamp(-Affect.Pleasure, 0.0f, 1.0f);
            V.MoodShift -= (V.NeedGain + V.Curiosity + V.GoalPull) * Anhedonia * 0.45f;
        }
        else
        {
            // В хорошем настроении на всё хватает сил.
            V.MoodShift += (V.NeedGain + V.Curiosity) * Affect.Pleasure * 0.2f;
        }

        // Беспомощность делает дальние и трудные дела бессмысленными.
        if (Affect.Dominance < -0.2f && A.EffortCost > 0.2f)
        {
            V.MoodShift -= A.EffortCost * (-Affect.Dominance) * 0.9f;
        }

        // Страх сужает мир до безопасности: всё, кроме укрытия, меркнет.
        const float Fear = Ctx.Emotions->GetIntensity(EEmotionType::Fear);
        if (Fear > 0.4f)
        {
            bool bOffersSafety = false;
            for (const FNeedPromise& Promise : A.Promises)
            {
                if (Promise.Need == ENeedType::Safety && Promise.Amount > 0.0f)
                {
                    bOffersSafety = true;
                    break;
                }
            }
            if (!bOffersSafety)
            {
                V.MoodShift -= Fear * 0.8f;
            }
        }
    }

    // =======================================================================
    //  11а. ЭТО СКОРО ЗАКРОЕТСЯ
    //
    //  Возможность, которой вот-вот не станет, ценнее той, что будет
    //  доступна весь день. Человек это понимает и торопится — не потому,
    //  что ему сказали «магазин закрывается», а потому что он знает часы
    //  и умеет считать, успевает ли.
    // =======================================================================
    if (!FMath::IsNearlyEqual(A.AvailableFromHour, A.AvailableToHour) && A.bHasLocation)
    {
        const float HoursLeft = A.AvailableToHour - Ctx.HourOfDay;
        if (HoursLeft > 0.0f && HoursLeft < 3.0f)
        {
            const float TravelHours = (FVector::Dist2D(Ctx.SelfLocation, A.Location)
                                       / FMath::Max(50.0f, Ctx.WalkSpeed)) / 3600.0f;
            const float NeededHours = TravelHours + A.Duration / 3600.0f;

            if (NeededHours > HoursLeft)
            {
                // Не успеть при всём желании — незачем и начинать.
                V.GoalPull -= 0.5f;
            }
            else
            {
                // Успеваю, но впритык — надо идти сейчас.
                const float Pressure = FMath::Clamp(1.0f - HoursLeft / 3.0f, 0.0f, 1.0f);
                V.GoalPull += V.NeedGain * Pressure * 0.8f;
            }
        }
    }

    // =======================================================================
    //  12. УМЕНИЕ
    //  За то, что заведомо не получится, браться не хочется — но судит
    //  человек не по настоящему умению, а по вере в себя.
    // =======================================================================
    if (!A.RequiredSkill.IsNone() && Ctx.Mind)
    {
        const float Believed = FMath::Clamp(
            0.08f + Ctx.Mind->Mastery(A.RequiredSkill) * 0.85f - A.Difficulty * 0.4f, 0.0f, 1.0f);

        // Отпугивает не «недостаточное умение», а СОМНЕНИЕ: браться не хочется
        // тогда, когда кажется, что скорее не выйдет. Уверенный в себе на
        // две трети берётся спокойно. Прежний линейный штраф был вдвое
        // больше всей выгоды от работы — и работать не шёл никто.
        // К тому же плохой исход уже учтён дважды: в выученном опыте и в том,
        // что при провале обещанное раздаётся лишь частично.
        const float Doubt = FMath::Max(0.0f, 0.6f - Believed);
        V.EffortCost += Doubt * 0.45f;

        // «Взялся бы, да не умею» — повод сначала научиться.
        if (Believed < 0.3f)
        {
            V.Blocker = EBlocker::Skill;
        }

        // А вот шанс подрасти в том, что важно, — это отдельная приманка.
        const float CompetenceNeed = Ctx.Needs ? Ctx.Needs->GetUrgency(ENeedType::Competence) : 0.2f;
        V.GoalPull += CompetenceNeed * 0.2f * (1.0f - Ctx.Mind->Mastery(A.RequiredSkill));
    }

    if (Ctx.Personality)
    {
        const FBigFive& B = Ctx.Personality->Traits;
        const FPersonalityFacets& P = Ctx.Personality->Facets;
        float Liking = 0.0f;
        switch (A.Action)
        {
        case EActionType::Read:
        case EActionType::Study:     Liking = B.Openness * 0.22f + P.Curiosity * 0.12f - 0.15f; break;
        case EActionType::Practice:  Liking = B.Openness * 0.12f + B.Conscientiousness * 0.12f - 0.11f; break;
        case EActionType::Work:      Liking = B.Conscientiousness * 0.2f + P.Ambition * 0.1f - 0.14f; break;
        case EActionType::Entertain: Liking = (1.0f - B.Conscientiousness) * 0.16f + (1.0f - B.Openness) * 0.06f - 0.1f; break;
        case EActionType::Exercise:  Liking = B.Extraversion * 0.1f + (1.0f - B.Neuroticism) * 0.1f + P.RiskTaking * 0.06f - 0.12f; break;
        case EActionType::Talk:
        case EActionType::Celebrate: Liking = B.Extraversion * 0.2f + P.Sociability * 0.1f - 0.14f; break;
        case EActionType::Observe:   Liking = B.Openness * 0.1f + (1.0f - B.Extraversion) * 0.06f - 0.07f; break;
        case EActionType::Wander:
        case EActionType::Explore:   Liking = B.Openness * 0.12f + P.Curiosity * 0.1f - 0.1f; break;
        case EActionType::Rest:      Liking = (1.0f - B.Conscientiousness) * 0.1f + B.Neuroticism * 0.06f - 0.07f; break;
        case EActionType::Help:
        case EActionType::Comfort:   Liking = B.Agreeableness * 0.18f + P.Empathy * 0.1f - 0.13f; break;
        case EActionType::Cook:      Liking = B.Conscientiousness * 0.08f + B.Agreeableness * 0.06f - 0.06f; break;
        case EActionType::Reflect:
        case EActionType::Reminisce: Liking = (1.0f - B.Extraversion) * 0.1f + B.Openness * 0.06f - 0.07f; break;
        default: break;
        }

        const AActor* Owner = GetOwner();
        const uint32 Seed = (Owner ? GetTypeHash(Owner->GetFName()) : 0u) ^ (static_cast<uint32>(A.Action) * 2654435761u);
        const float Quirk = (static_cast<float>(Seed % 1000u) / 1000.0f - 0.5f) * 0.14f;
        V.Taste = Liking + Quirk;
    }

    V.Total = V.NeedGain + V.Experience + V.Curiosity + V.SocialPull + V.GoalPull + V.HabitPull + V.MoodShift + V.Taste
            - V.TravelCost - V.EffortCost - V.RiskCost - V.MoneyCost;

    // Чего это стоило бы, не будь помехи. Ради этого числа человек и
    // затевает дела заранее: само по себе «сходить на работу» скучно,
    // но оно открывает то, чего хочется по-настоящему.
    V.PotentialValue = (V.Blocker == EBlocker::None)
        ? V.Total
        : V.NeedGain + V.Experience + V.Curiosity + V.SocialPull + V.GoalPull + V.MoodShift - V.RiskCost;

    // --- Из чего сложилось: то, что человек назвал бы причиной --------------
    {
        struct FTerm { float Value; const TCHAR* Name; };
        const FTerm Terms[] = {
            { StrongestNeed,   TEXT("нужда") },
            { V.Experience,    TEXT("опыт") },
            { V.Curiosity,     TEXT("любопытство") },
            { V.SocialPull,    TEXT("люди") },
            { V.GoalPull,      TEXT("замысел") },
            { V.HabitPull,     TEXT("привычка") },
            { V.Taste,         TEXT("по душе") }
        };

        float Best = 0.05f;
        int32 BestIndex = INDEX_NONE;
        for (int32 i = 0; i < UE_ARRAY_COUNT(Terms); ++i)
        {
            if (Terms[i].Value > Best)
            {
                Best = Terms[i].Value;
                BestIndex = i;
            }
        }

        if (BestIndex == 0)
        {
            // Ведёт нужда — называем её прямо, её же словами.
            V.DominantReason = HumanText::NeedDesire(StrongestNeedType);
        }
        else if (BestIndex == INDEX_NONE)
        {
            V.DominantReason = TEXT("просто так");
        }
        else
        {
            V.DominantReason = FString(Terms[BestIndex].Name);
        }
    }

    return V;
}

// ---------------------------------------------------------------------------
//  ВЫБОР
// ---------------------------------------------------------------------------

float UDeliberationComponent::SwitchingThreshold(const FDeliberationContext& Ctx) const
{
    // Насколько человек «липнет» к начатому. Собранный доводит до конца,
    // рассеянного сбивает что угодно.
    float Threshold = 1.25f;

    if (Ctx.Personality)
    {
        Threshold = FMath::Lerp(1.05f, 1.6f, Ctx.Personality->Traits.Conscientiousness);
        Threshold -= Ctx.Personality->Facets.Impulsivity * 0.25f;
    }

    // Но когда телу по-настоящему плохо, никакая собранность не держит.
    if (Ctx.Needs)
    {
        static const ENeedType Vitals[] = {
            ENeedType::Thirst, ENeedType::Hunger, ENeedType::Bladder,
            ENeedType::Sleep,  ENeedType::Safety, ENeedType::Health
        };
        for (ENeedType Vital : Vitals)
        {
            if (Ctx.Needs->GetSatisfaction(Vital) < 0.2f)
            {
                Threshold = 1.0f;
                break;
            }
        }
    }

    return FMath::Max(1.0f, Threshold);
}

FString UDeliberationComponent::ExplainChoice(const FAffordance& A, const FValuation& V)
{
    const FString What = A.Label.IsEmpty() ? HumanText::Action(A.Action) : A.Label;

    if (V.DominantReason == TEXT("опыт"))
    {
        return (V.Experience > 0.0f)
            ? FString::Printf(TEXT("%s — в прошлый раз вышло хорошо"), *What)
            : FString::Printf(TEXT("%s, хотя ничем хорошим это не кончалось"), *What);
    }
    if (V.DominantReason == TEXT("любопытство"))
    {
        return FString::Printf(TEXT("%s — а вдруг"), *What);
    }
    if (V.DominantReason == TEXT("люди"))
    {
        return FString::Printf(TEXT("%s"), *What);
    }
    if (V.DominantReason == TEXT("замысел"))
    {
        return FString::Printf(TEXT("%s — это приблизит меня к своему"), *What);
    }
    if (V.DominantReason == TEXT("привычка"))
    {
        return FString::Printf(TEXT("%s — как обычно в это время"), *What);
    }
    if (V.DominantReason == TEXT("по душе"))
    {
        return FString::Printf(TEXT("%s — люблю это"), *What);
    }
    if (V.DominantReason == TEXT("просто так"))
    {
        return FString::Printf(TEXT("%s. Почему бы и нет."), *What);
    }

    // Ведёт нужда.
    return FString::Printf(TEXT("%s — %s"), *What, *V.DominantReason);
}

bool UDeliberationComponent::Decide(const TArray<FAffordance>& Available, const FDeliberationContext& Ctx, FIntention& Out)
{
    LastConsidered.Reset();
    LastConsideredLabels.Reset();

    if (Available.Num() == 0)
    {
        return false;
    }

    // --- Взвешиваем всё, что предложено ------------------------------------
    struct FCandidate
    {
        const FAffordance* Affordance;
        FValuation Valuation;
    };

    TArray<FCandidate> Candidates;
    Candidates.Reserve(Available.Num());

    for (const FAffordance& A : Available)
    {
        FCandidate C;
        C.Affordance = &A;
        C.Valuation = Evaluate(A, Ctx);
        Candidates.Add(C);
    }

    Candidates.Sort([](const FCandidate& A, const FCandidate& B)
    {
        return A.Valuation.Total > B.Valuation.Total;
    });

    // --- Одинаковое считается один раз -------------------------------------
    // Человек не взвешивает «поесть в кафе №1», «поесть в кафе №2» и
    // «поесть в кафе №3» как три разные возможности. Он думает «поесть» —
    // и идёт в лучшее из доступных. Без этого свёртывания пять ближайших
    // подъездов вытесняли из головы работу просто числом.
    {
        TSet<FString> Seen;
        for (int32 i = 0; i < Candidates.Num(); )
        {
            const FAffordance& A = *Candidates[i].Affordance;

            // Разные люди — разные возможности. Разные кафе — одна.
            const FString CollapseKey = (A.Source == EAffordanceSource::Person && A.Target)
                ? FString::Printf(TEXT("%d@%s"), static_cast<int32>(A.Action), *A.Target->GetName())
                : FString::Printf(TEXT("%d"), static_cast<int32>(A.Action));

            if (Seen.Contains(CollapseKey))
            {
                Candidates.RemoveAt(i);   // список отсортирован: лучшее уже взято
            }
            else
            {
                Seen.Add(CollapseKey);
                ++i;
            }
        }
    }

    // =======================================================================
    //  ВЗГЛЯД НА ШАГ ВПЕРЁД
    //
    //  «В кафе хорошо, но денег нет» — и вместо того, чтобы просто вычеркнуть
    //  кафе, человек спрашивает себя: а что нужно, чтобы стало можно?
    //  Найденное средство получает вес недостижимой пока цели.
    //
    //  Это не правило «нет денег — иди работать». Это перенос ценности
    //  с цели на средство, и он работает для любой помехи и любого средства,
    //  какие бы ни появились в мире завтра.
    // =======================================================================
    FString ServesGoalLabel;
    FName BoostedKey;
    {
        // Лучшее из того, что можно прямо сейчас.
        float BestAvailable = -FLT_MAX;
        for (const FCandidate& C : Candidates)
        {
            if (C.Valuation.Blocker == EBlocker::None)
            {
                BestAvailable = FMath::Max(BestAvailable, C.Valuation.Total);
            }
        }

        // Лучшее из того, что пока недоступно.
        const FCandidate* Desired = nullptr;
        for (const FCandidate& C : Candidates)
        {
            if (C.Valuation.Blocker != EBlocker::None && C.Valuation.PotentialValue > 0.05f)
            {
                if (!Desired || C.Valuation.PotentialValue > Desired->Valuation.PotentialValue)
                {
                    Desired = &C;
                }
            }
        }

        // Стоит ли вообще затеваться: недостижимое должно быть заметно лучше
        // доступного, иначе человек просто возьмёт что попроще.
        if (Desired && Desired->Valuation.PotentialValue > BestAvailable + 0.05f)
        {
            // Чего не хватает — то и нужно раздобыть.
            ENeedType Needed = ENeedType::Money;
            switch (Desired->Valuation.Blocker)
            {
            case EBlocker::Money:   Needed = ENeedType::Money; break;
            case EBlocker::Stamina: Needed = ENeedType::Comfort; break;
            case EBlocker::Skill:   Needed = ENeedType::Competence; break;
            default:                Needed = ENeedType::Money; break;
            }

            // Насколько человек вообще способен думать наперёд. Импульсивный
            // не станет: ему нужно сейчас, а не «чтобы потом».
            float Foresight = 0.5f;
            if (Ctx.Personality)
            {
                Foresight = FMath::Clamp(
                    Ctx.Personality->Patience() * 0.6f + Ctx.Personality->Traits.Conscientiousness * 0.4f,
                    0.05f, 1.0f);
            }
            // Уставшему и взвинченному не до дальних расчётов.
            Foresight *= (1.0f - FMath::Clamp(Ctx.Impairment, 0.0f, 0.8f));

            const float Transfer = (Desired->Valuation.PotentialValue - FMath::Max(0.0f, BestAvailable)) * Foresight;

            // Ищем средство: то, что даёт недостающее и доступно прямо сейчас.
            FCandidate* BestMeans = nullptr;
            float BestSupply = 0.0f;

            for (FCandidate& C : Candidates)
            {
                if (C.Valuation.Blocker != EBlocker::None || C.Affordance == Desired->Affordance)
                {
                    continue;
                }
                for (const FNeedPromise& Promise : C.Affordance->Promises)
                {
                    if (Promise.Need == Needed && Promise.Amount > BestSupply)
                    {
                        BestSupply = Promise.Amount;
                        BestMeans = &C;
                    }
                }
            }

            if (BestMeans && BestSupply > 0.05f)
            {
                BestMeans->Valuation.GoalPull += Transfer * FMath::Clamp(BestSupply, 0.0f, 1.0f);
                BestMeans->Valuation.Total += Transfer * FMath::Clamp(BestSupply, 0.0f, 1.0f);

                const FString What = Desired->Affordance->Label.IsEmpty()
                    ? HumanText::Action(Desired->Affordance->Action)
                    : Desired->Affordance->Label;

                const TCHAR* Obstacle = TEXT("не выходит");
                switch (Desired->Valuation.Blocker)
                {
                case EBlocker::Money:   Obstacle = TEXT("денег не хватает"); break;
                case EBlocker::Stamina: Obstacle = TEXT("сил не хватает"); break;
                case EBlocker::Skill:   Obstacle = TEXT("не умею"); break;
                default: break;
                }

                ServesGoalLabel = FString::Printf(TEXT("%s — %s"), *What, Obstacle);
                BoostedKey = BestMeans->Affordance->Key;

                // Пересортировать: средство могло подняться выше.
                Candidates.Sort([](const FCandidate& A, const FCandidate& B)
                {
                    return A.Valuation.Total > B.Valuation.Total;
                });
            }
        }
    }

    // Человек не держит в голове сорок вариантов — только несколько лучших.
    const int32 Considered = FMath::Min(Candidates.Num(), FMath::Max(2, ConsiderationLimit));
    for (int32 i = 0; i < Considered; ++i)
    {
        LastConsidered.Add(Candidates[i].Valuation);
        LastConsideredLabels.Add(Candidates[i].Affordance->Label);
    }

    // --- Отбрасываем то, что и рассматривать не стоит ----------------------
    TArray<FCandidate> Viable;
    for (int32 i = 0; i < Considered; ++i)
    {
        if (Candidates[i].Valuation.Total > 0.02f)
        {
            Viable.Add(Candidates[i]);
        }
    }

    if (Viable.Num() == 0)
    {
        // Человеку может быть нечего хотеть — но он не застывает столбом.
        // Он делает наименее бесполезное из доступного: слоняется,
        // садится, смотрит по сторонам. Полная неподвижность — это не
        // «нет мотивации», это обморок.
        if (Candidates.Num() == 0)
        {
            return false;
        }

        Out = FIntention();
        Out.bValid = true;
        Out.Affordance = *Candidates[0].Affordance;
        Out.Valuation = Candidates[0].Valuation;
        Out.DecidedAt = Ctx.WorldTime;
        Out.Expectation = 0.0f;
        Out.Reason = FString::Printf(TEXT("%s. Делать всё равно нечего."),
            *(Out.Affordance.Label.IsEmpty() ? HumanText::Action(Out.Affordance.Action) : Out.Affordance.Label));

        Current = Out;
        return true;
    }

    // --- Выбор: не argmax, а взвешенная случайность ------------------------
    // «Резкость» выбора зависит от того, насколько ясная голова. Собранный
    // человек почти всегда берёт лучшее; уставший, пьяный от эмоций или
    // импульсивный — как выйдет.
    float Sharpness = 4.0f;
    Sharpness *= (1.0f - FMath::Clamp(Ctx.Impairment, 0.0f, 0.9f));
    if (Ctx.Personality)
    {
        Sharpness *= FMath::Lerp(1.3f, 0.55f, Ctx.Personality->Facets.Impulsivity);
    }
    if (Ctx.Emotions)
    {
        // Сильные чувства не дают взвешивать: решают быстро и не лучшим образом.
        Sharpness *= FMath::Lerp(1.0f, 0.5f, Ctx.Emotions->GetTurmoil());
    }
    Sharpness = FMath::Clamp(Sharpness, 0.8f, 6.0f);

    float Total = 0.0f;
    TArray<float> Weights;
    Weights.Reserve(Viable.Num());

    for (const FCandidate& C : Viable)
    {
        const float W = FMath::Pow(FMath::Max(0.02f, C.Valuation.Total), Sharpness);
        Weights.Add(W);
        Total += W;
    }

    int32 Chosen = 0;
    if (Total > 0.0f)
    {
        float Roll = FMath::FRandRange(0.0f, Total);
        for (int32 i = 0; i < Weights.Num(); ++i)
        {
            Roll -= Weights[i];
            if (Roll <= 0.0f)
            {
                Chosen = i;
                break;
            }
        }
    }

    const FCandidate& Winner = Viable[Chosen];

    Out = FIntention();
    Out.bValid = true;
    Out.Affordance = *Winner.Affordance;
    Out.Valuation = Winner.Valuation;
    Out.DecidedAt = Ctx.WorldTime;
    Out.Reason = ExplainChoice(*Winner.Affordance, Winner.Valuation);

    // Если это делается ради чего-то другого — человек это осознаёт.
    if (!BoostedKey.IsNone() && Winner.Affordance->Key == BoostedKey && !ServesGoalLabel.IsEmpty())
    {
        Out.ServesGoal = ServesGoalLabel;
        Out.Reason = FString::Printf(TEXT("%s — ради этого: %s"),
            *(Winner.Affordance->Label.IsEmpty() ? HumanText::Action(Winner.Affordance->Action) : Winner.Affordance->Label),
            *ServesGoalLabel);
    }

    // Чего человек ждёт от этого — с чем потом сравнится действительность.
    // Ожидание складывается из обещанного и из прошлого опыта.
    float Confidence = 0.0f;
    const float Learned = Ctx.Memory
        ? Ctx.Memory->GetExpectedOutcomeGeneralised(MakeKey(Out.Affordance), Out.Affordance.CategoryKey, Confidence)
        : 0.0f;
    const float Promised = FMath::Clamp(Winner.Valuation.NeedGain * 0.7f, -1.0f, 1.0f);

    // Оптимист ждёт большего, чем обещано. И чаще разочаровывается.
    const float Optimism = Ctx.Personality ? Ctx.Personality->Facets.Optimism : 0.5f;
    Out.Expectation = FMath::Clamp(
        FMath::Lerp(Promised, Learned, Confidence * 0.6f) + (Optimism - 0.5f) * 0.25f,
        -1.0f, 1.0f);

    Current = Out;
    return true;
}
