// EmotionComponent.cpp

#include "EmotionComponent.h"
#include "PersonalityComponent.h"

UEmotionComponent::UEmotionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
//  Таблицы: что значит чувствовать ту или иную эмоцию
// ---------------------------------------------------------------------------

FAffectPAD UEmotionComponent::EmotionToPAD(EEmotionType Type)
{
    // Приятность / Возбуждение / Ощущение контроля.
    auto Make = [](float Pleasure, float Arousal, float Dominance)
    {
        FAffectPAD R;
        R.Pleasure = Pleasure;
        R.Arousal = Arousal;
        R.Dominance = Dominance;
        return R;
    };

    switch (Type)
    {
    case EEmotionType::Joy:            return Make( 0.85f,  0.55f,  0.45f);
    case EEmotionType::Sadness:        return Make(-0.70f, -0.45f, -0.50f);
    case EEmotionType::Fear:           return Make(-0.75f,  0.80f, -0.75f);
    case EEmotionType::Anger:          return Make(-0.55f,  0.85f,  0.50f);
    case EEmotionType::Disgust:        return Make(-0.65f,  0.35f,  0.20f);
    case EEmotionType::Surprise:       return Make( 0.10f,  0.85f, -0.15f);

    case EEmotionType::Hope:           return Make( 0.45f,  0.35f,  0.15f);
    case EEmotionType::Anxiety:        return Make(-0.55f,  0.60f, -0.55f);
    case EEmotionType::Relief:         return Make( 0.60f, -0.25f,  0.25f);
    case EEmotionType::Disappointment: return Make(-0.55f, -0.25f, -0.30f);

    case EEmotionType::Pride:          return Make( 0.75f,  0.45f,  0.80f);
    case EEmotionType::Shame:          return Make(-0.70f,  0.25f, -0.80f);
    case EEmotionType::Guilt:          return Make(-0.60f,  0.20f, -0.45f);
    case EEmotionType::Remorse:        return Make(-0.55f, -0.15f, -0.40f);
    case EEmotionType::Satisfaction:   return Make( 0.70f, -0.15f,  0.55f);

    case EEmotionType::Love:           return Make( 0.90f,  0.35f,  0.30f);
    case EEmotionType::Affection:      return Make( 0.70f,  0.10f,  0.25f);
    case EEmotionType::Gratitude:      return Make( 0.65f,  0.20f, -0.10f);
    case EEmotionType::Admiration:     return Make( 0.55f,  0.30f, -0.20f);
    case EEmotionType::Compassion:     return Make(-0.15f,  0.25f,  0.10f);
    case EEmotionType::Envy:           return Make(-0.50f,  0.40f, -0.35f);
    case EEmotionType::Jealousy:       return Make(-0.60f,  0.65f, -0.30f);
    case EEmotionType::Contempt:       return Make(-0.35f,  0.20f,  0.55f);
    case EEmotionType::Resentment:     return Make(-0.55f,  0.35f, -0.15f);
    case EEmotionType::Embarrassment:  return Make(-0.40f,  0.45f, -0.55f);
    case EEmotionType::Loneliness:     return Make(-0.60f, -0.25f, -0.45f);
    case EEmotionType::Trust:          return Make( 0.50f, -0.10f,  0.20f);

    case EEmotionType::Curiosity:      return Make( 0.35f,  0.45f,  0.25f);
    case EEmotionType::Boredom:        return Make(-0.35f, -0.60f, -0.20f);
    case EEmotionType::Frustration:    return Make(-0.50f,  0.55f, -0.25f);
    case EEmotionType::Nostalgia:      return Make( 0.15f, -0.25f, -0.10f);
    case EEmotionType::Serenity:       return Make( 0.55f, -0.55f,  0.35f);
    case EEmotionType::Awe:            return Make( 0.45f,  0.50f, -0.35f);

    default:                           return Make( 0.0f,  0.0f,  0.0f);
    }
}

float UEmotionComponent::BaseDecayRate(EEmotionType Type)
{
    // Скорость затухания задаётся через ВРЕМЯ ЖИЗНИ эмоции в игровых минутах:
    // сколько примерно она держится, прежде чем сойти на нет.
    // Так проще думать и легче настраивать.
    auto Minutes = [](float M) { return 1.0f / FMath::Max(1.0f, M * 60.0f); };

    switch (Type)
    {
    // --- вспышки: секунды и минуты ---
    case EEmotionType::Surprise:       return Minutes(0.2f);
    case EEmotionType::Relief:         return Minutes(3.0f);
    case EEmotionType::Fear:           return Minutes(5.0f);
    case EEmotionType::Disgust:        return Minutes(6.0f);
    case EEmotionType::Curiosity:      return Minutes(8.0f);
    case EEmotionType::Frustration:    return Minutes(10.0f);
    case EEmotionType::Anger:          return Minutes(15.0f);
    case EEmotionType::Awe:            return Minutes(6.0f);
    case EEmotionType::Embarrassment:  return Minutes(25.0f);

    // --- то, что держится часть дня ---
    case EEmotionType::Joy:            return Minutes(35.0f);
    case EEmotionType::Boredom:        return Minutes(25.0f);
    case EEmotionType::Satisfaction:   return Minutes(50.0f);
    case EEmotionType::Compassion:     return Minutes(45.0f);
    case EEmotionType::Nostalgia:      return Minutes(30.0f);
    case EEmotionType::Serenity:       return Minutes(60.0f);
    case EEmotionType::Disappointment: return Minutes(90.0f);
    case EEmotionType::Pride:          return Minutes(120.0f);
    case EEmotionType::Anxiety:        return Minutes(150.0f);
    case EEmotionType::Hope:           return Minutes(180.0f);
    case EEmotionType::Gratitude:      return Minutes(240.0f);
    case EEmotionType::Admiration:     return Minutes(240.0f);
    case EEmotionType::Envy:           return Minutes(300.0f);
    case EEmotionType::Jealousy:       return Minutes(300.0f);

    // --- то, что въедается: часы и дни ---
    case EEmotionType::Sadness:        return Minutes(600.0f);
    case EEmotionType::Loneliness:     return Minutes(540.0f);
    case EEmotionType::Shame:          return Minutes(720.0f);
    case EEmotionType::Guilt:          return Minutes(900.0f);
    case EEmotionType::Remorse:        return Minutes(1000.0f);
    case EEmotionType::Contempt:       return Minutes(1440.0f);
    case EEmotionType::Trust:          return Minutes(2880.0f);
    case EEmotionType::Resentment:     return Minutes(4320.0f);  // трое суток
    case EEmotionType::Affection:      return Minutes(2880.0f);
    case EEmotionType::Love:           return Minutes(20160.0f); // две недели

    default:                           return Minutes(30.0f);
    }
}

bool UEmotionComponent::IsPositive(EEmotionType Type)
{
    return EmotionToPAD(Type).Pleasure > 0.05f;
}

// ---------------------------------------------------------------------------
//  Настройка
// ---------------------------------------------------------------------------

void UEmotionComponent::Setup(const UPersonalityComponent* Personality)
{
    if (!Personality)
    {
        return;
    }

    LinkedPersonality = Personality;
    Mood.Baseline = Personality->TemperamentBaseline;
    Mood.Mood = Mood.Baseline;
    Mood.StressLoad = FMath::Clamp(Personality->Facets.TraitAnxiety * 0.25f, 0.0f, 0.4f);

    // Сдержанность: невротики и интроверты прячут больше.
    Expressiveness = FMath::Clamp(
        0.35f + Personality->Traits.Extraversion * 0.45f + Personality->Traits.Agreeableness * 0.15f
        - Personality->Facets.SelfControl * 0.25f,
        0.05f, 1.0f);
}

// ---------------------------------------------------------------------------
//  ОЦЕНКА СОБЫТИЯ — здесь рождаются чувства
// ---------------------------------------------------------------------------

EEmotionType UEmotionComponent::Appraise(const FAppraisedEvent& Event, const UPersonalityComponent* InPersonality)
{
    // Если характер не передали — берём свой. Чувство без характера
    // не бывает: оценивает всегда кто-то конкретный.
    const UPersonalityComponent* Personality = InPersonality ? InPersonality : LinkedPersonality.Get();

    const float D    = FMath::Clamp(Event.Desirability, -1.0f, 1.0f);   // хорошо/плохо для меня
    const float U    = FMath::Clamp(Event.Unexpectedness, 0.0f, 1.0f);  // неожиданность
    const float Ctrl = FMath::Clamp(Event.Controllability, 0.0f, 1.0f); // могу ли я на это влиять
    const float SA   = FMath::Clamp(Event.SelfAgency, 0.0f, 1.0f);      // моя вина/заслуга
    const float OA   = FMath::Clamp(Event.OtherAgency, 0.0f, 1.0f);     // его вина/заслуга
    const float NA   = FMath::Clamp(Event.NormAlignment, -1.0f, 1.0f);  // по совести или нет
    const float Cert = FMath::Clamp(Event.Certainty, 0.0f, 1.0f);       // это уже случилось?
    const float Sig  = FMath::Clamp(Event.Significance, 0.0f, 1.0f);    // насколько важно

    AActor* Cause = Event.Subject;
    const FString& Why = Event.Description;

    // Оцепеневший человек почти ничего не чувствует — защита психики.
    const float NumbGate = 1.0f - Mood.Numbness * 0.8f;
    const float Base = Sig * NumbGate;
    if (Base < 0.02f)
    {
        return EEmotionType::None;
    }

    // --- Неожиданность: удивление рождается раньше всякой оценки ------------
    if (U > 0.45f)
    {
        Ignite(EEmotionType::Surprise, U * Base, Cause, Why, Personality);
    }

    // =======================================================================
    //  СОБЫТИЕ ЕЩЁ НЕ СЛУЧИЛОСЬ — это прогноз. Тогда чувства о будущем.
    // =======================================================================
    if (Cert < 0.75f)
    {
        const float Prospect = Base * (1.0f - Cert * 0.5f);
        if (D > 0.1f)
        {
            Ignite(EEmotionType::Hope, D * Prospect, Cause, Why, Personality);
        }
        else if (D < -0.1f)
        {
            // Беспомощность перед угрозой превращает тревогу в страх.
            if (Ctrl < 0.3f)
            {
                Ignite(EEmotionType::Fear, -D * Prospect * (1.0f - Ctrl), Cause, Why, Personality);
            }
            Ignite(EEmotionType::Anxiety, -D * Prospect, Cause, Why, Personality);
        }
        // Прогноз может и не сбыться — на этом всё.
        EEmotionType Dom; float Int;
        GetDominantWithIntensity(Dom, Int);
        return Dom;
    }

    // =======================================================================
    //  ХОРОШЕЕ СЛУЧИЛОСЬ
    // =======================================================================
    if (D > 0.05f)
    {
        const float Good = D * Base;

        Ignite(EEmotionType::Joy, Good * 0.9f, Cause, Why, Personality);

        // Это сделал я → гордость и удовлетворение.
        if (SA > 0.4f)
        {
            Ignite(EEmotionType::Pride, Good * SA * 0.9f, Cause, Why, Personality);
            Ignite(EEmotionType::Satisfaction, Good * SA * 0.6f, Cause, Why, Personality);
        }

        // Это сделал он → благодарность (и долг).
        if (OA > 0.4f && Cause)
        {
            Ignite(EEmotionType::Gratitude, Good * OA, Cause, Why, Personality);
        }

        // Я боялся, а обошлось → облегчение.
        const float PriorFear = GetIntensity(EEmotionType::Fear) + GetIntensity(EEmotionType::Anxiety);
        if (PriorFear > 0.2f)
        {
            Ignite(EEmotionType::Relief, FMath::Min(PriorFear, Good) * 1.2f, Cause, Why, Personality);
            // Облегчение гасит страх напрямую.
            for (FEmotionInstance& E : Active)
            {
                if (E.Type == EEmotionType::Fear || E.Type == EEmotionType::Anxiety)
                {
                    E.Intensity *= 0.35f;
                }
            }
        }

        // Благородный поступок другого → восхищение.
        if (NA > 0.4f && OA > 0.3f && Cause)
        {
            Ignite(EEmotionType::Admiration, NA * OA * Base, Cause, Why, Personality);
        }
    }

    // =======================================================================
    //  ПЛОХОЕ СЛУЧИЛОСЬ — здесь начинается самое интересное
    // =======================================================================
    else if (D < -0.05f)
    {
        const float Bad = -D * Base;

        // Потеря, которую я не мог предотвратить → печаль.
        Ignite(EEmotionType::Sadness, Bad * (1.0f - Ctrl) * 0.9f, Cause, Why, Personality);

        // Угроза, от которой я не защищён → страх.
        if (Ctrl < 0.45f && Event.Tag != TEXT("Loss"))
        {
            Ignite(EEmotionType::Fear, Bad * (1.0f - Ctrl) * 0.8f, Cause, Why, Personality);
        }

        // Виноват ОН, и я мог бы дать сдачи → гнев.
        // Тот же вред от того же человека, но при ощущении беспомощности,
        // даст не гнев, а страх. Это и есть разница между людьми.
        if (OA > 0.3f && Cause)
        {
            const float AngerStrength = Bad * OA * (0.35f + Ctrl * 0.65f);
            Ignite(EEmotionType::Anger, AngerStrength, Cause, Why, Personality);

            // Обида — то, что остаётся, когда гнев выразить нельзя.
            if (Ctrl < 0.4f)
            {
                Ignite(EEmotionType::Resentment, Bad * OA * 0.8f, Cause, Why, Personality);
            }
        }

        // Виноват Я → вина; если при этом были свидетели — ещё и стыд.
        if (SA > 0.3f)
        {
            const float MoralWeight = Personality ? (0.4f + Personality->Morals.Care * 0.6f) : 0.7f;
            Ignite(EEmotionType::Guilt, Bad * SA * MoralWeight, Cause, Why, Personality);

            if (Event.Tag == TEXT("PublicFailure") || Event.Tag == TEXT("Humiliated"))
            {
                Ignite(EEmotionType::Shame, Bad * SA * 1.1f, Cause, Why, Personality);
            }
        }

        // Помеха на пути к цели → досада, а не горе.
        if (Event.Tag == TEXT("GoalBlocked") || Event.Tag == TEXT("PlanFailed"))
        {
            Ignite(EEmotionType::Frustration, Bad * (0.5f + Ctrl * 0.5f), Cause, Why, Personality);
        }

        // Ожидал хорошего, не получил → разочарование.
        if (Event.Tag == TEXT("ExpectationBroken"))
        {
            Ignite(EEmotionType::Disappointment, Bad, Cause, Why, Personality);
        }
    }

    // =======================================================================
    //  МОРАЛЬНАЯ ОЦЕНКА — независимо от того, задело ли лично меня
    // =======================================================================
    if (NA < -0.25f)
    {
        const float Wrongness = -NA * Base;

        if (OA > 0.3f && Cause)
        {
            // Мерзость, сделанная другим: отвращение + презрение.
            Ignite(EEmotionType::Disgust, Wrongness * 0.7f, Cause, Why, Personality);
            Ignite(EEmotionType::Contempt, Wrongness * 0.8f, Cause, Why, Personality);

            // Даже если меня это не коснулось — несправедливость злит.
            if (Personality && Personality->Morals.Fairness > 0.6f)
            {
                Ignite(EEmotionType::Anger, Wrongness * 0.5f, Cause, Why, Personality);
            }
        }
        if (SA > 0.3f)
        {
            // Я сам преступил через себя — это дороже всего.
            Ignite(EEmotionType::Shame, Wrongness * 1.0f, Cause, Why, Personality);
            Ignite(EEmotionType::Remorse, Wrongness * 0.8f, Cause, Why, Personality);
        }
    }

    // --- Сострадание: чужая беда, к которой я непричастен -------------------
    if (Event.Tag == TEXT("OtherSuffers") && Cause)
    {
        const float Emp = Personality ? Personality->Facets.Empathy : 0.5f;
        Ignite(EEmotionType::Compassion, Base * Emp, Cause, Why, Personality);
        // Сопереживание — это буквально чужая боль внутри себя.
        Ignite(EEmotionType::Sadness, Base * Emp * 0.4f, Cause, Why, Personality);
    }

    // --- Зависть: у него есть то, чего нет у меня ---------------------------
    if (Event.Tag == TEXT("OtherSucceeds") && Cause)
    {
        const float Amb = Personality ? Personality->Facets.Ambition : 0.5f;
        const float Agr = Personality ? Personality->Traits.Agreeableness : 0.5f;
        // Доброжелательный порадуется, честолюбивый позавидует.
        Ignite(EEmotionType::Envy, Base * Amb * (1.0f - Agr), Cause, Why, Personality);
        Ignite(EEmotionType::Admiration, Base * Agr * 0.6f, Cause, Why, Personality);
    }

    // --- Новизна будит любопытство ------------------------------------------
    if (U > 0.3f && D > -0.2f)
    {
        const float Cur = Personality ? Personality->Facets.Curiosity : 0.5f;
        Ignite(EEmotionType::Curiosity, U * Cur * Base * 0.8f, Cause, Why, Personality);
    }

    EEmotionType Dominant;
    float DominantIntensity;
    GetDominantWithIntensity(Dominant, DominantIntensity);
    return DominantIntensity > 0.05f ? Dominant : EEmotionType::None;
}

// ---------------------------------------------------------------------------
//  Зажигание эмоции
// ---------------------------------------------------------------------------

void UEmotionComponent::Ignite(EEmotionType Type, float Intensity, AActor* Cause, const FString& Reason, const UPersonalityComponent* Personality)
{
    if (Type == EEmotionType::None)
    {
        return;
    }

    float Strength = Intensity * PersonalityGain(Type, Personality);

    // Настроение окрашивает восприятие: в плохом настроении плохое ощущается
    // острее, а хорошее — тускло. Это конгруэнтность настроения.
    const bool bPositive = IsPositive(Type);
    if (bPositive && Mood.Mood.Pleasure < 0.0f)
    {
        Strength *= FMath::Lerp(1.0f, 0.55f, -Mood.Mood.Pleasure);
    }
    else if (!bPositive && Mood.Mood.Pleasure < 0.0f)
    {
        Strength *= FMath::Lerp(1.0f, 1.45f, -Mood.Mood.Pleasure);
    }

    Strength = FMath::Clamp(Strength, 0.0f, 1.0f);
    if (Strength <= 0.0f)
    {
        return;
    }

    // Уже горит — подливаем масла (но с насыщением: нельзя злиться бесконечно).
    // Проверку порога делаем ПОСЛЕ этого: чувство, которое уже есть, можно
    // подпитывать сколь угодно малыми порциями — так и копится скука, тревога
    // и всё, что нарастает исподволь.
    for (FEmotionInstance& E : Active)
    {
        if (E.Type == Type)
        {
            E.Intensity = FMath::Clamp(E.Intensity + Strength * (1.0f - E.Intensity * 0.6f), 0.0f, 1.0f);
            if (!Reason.IsEmpty())
            {
                E.Reason = Reason;
            }
            if (Cause)
            {
                E.Cause = Cause;
            }
            E.BornAt = WorldTime;
            return;
        }
    }

    // А вот чтобы чувство ВОЗНИКЛО, толчок должен быть достаточным.
    if (Strength < ExtinctionThreshold)
    {
        return;
    }

    FEmotionInstance New;
    New.Type = Type;
    New.Intensity = Strength;
    New.DecayRate = BaseDecayRate(Type);
    New.Cause = Cause;
    New.Reason = Reason;
    New.BornAt = WorldTime;
    Active.Add(New);

    // Человек не может держать в себе двадцать чувств сразу — слабейшие уходят.
    if (Active.Num() > 10)
    {
        int32 WeakestIndex = 0;
        for (int32 i = 1; i < Active.Num(); ++i)
        {
            if (Active[i].Intensity < Active[WeakestIndex].Intensity)
            {
                WeakestIndex = i;
            }
        }
        Active.RemoveAt(WeakestIndex);
    }
}

void UEmotionComponent::Trigger(EEmotionType Type, float Intensity, AActor* Cause, const FString& Reason)
{
    // Даже «прямое» чувство проходит через характер: у тревожного любой
    // испуг сильнее, у доброжелательного любая злость слабее.
    Ignite(Type, Intensity, Cause, Reason, LinkedPersonality);
}

void UEmotionComponent::Soothe(EEmotionType Type, float Amount)
{
    for (int32 i = Active.Num() - 1; i >= 0; --i)
    {
        if (Active[i].Type == Type)
        {
            Active[i].Intensity -= FMath::Abs(Amount);
            if (Active[i].Intensity <= ExtinctionThreshold)
            {
                Active.RemoveAt(i);
            }
            return;
        }
    }
}

float UEmotionComponent::PersonalityGain(EEmotionType Type, const UPersonalityComponent* Personality) const
{
    if (!Personality)
    {
        return 1.0f;
    }

    const FBigFive& T = Personality->Traits;
    const FPersonalityFacets& F = Personality->Facets;

    float Gain = 1.0f;

    // Нейротизм — усилитель всего отрицательного.
    if (!IsPositive(Type))
    {
        Gain *= FMath::Lerp(0.6f, 1.55f, T.Neuroticism);
    }
    else
    {
        // Экстраверты ярче переживают хорошее.
        Gain *= FMath::Lerp(0.7f, 1.35f, T.Extraversion);
    }

    switch (Type)
    {
    case EEmotionType::Anger:
    case EEmotionType::Contempt:
        // Доброжелательные почти не злятся; вспыльчивые — наоборот.
        Gain *= FMath::Lerp(1.5f, 0.5f, T.Agreeableness);
        Gain *= FMath::Lerp(0.8f, 1.3f, F.Impulsivity);
        break;

    case EEmotionType::Fear:
    case EEmotionType::Anxiety:
        Gain *= FMath::Lerp(0.55f, 1.5f, F.TraitAnxiety);
        Gain *= FMath::Lerp(1.25f, 0.7f, F.RiskTaking);
        break;

    case EEmotionType::Compassion:
    case EEmotionType::Affection:
    case EEmotionType::Love:
        Gain *= FMath::Lerp(0.4f, 1.6f, F.Empathy);
        break;

    case EEmotionType::Guilt:
    case EEmotionType::Remorse:
        Gain *= FMath::Lerp(0.35f, 1.5f, Personality->Morals.Care);
        Gain *= FMath::Lerp(0.6f, 1.3f, F.Honesty);
        break;

    case EEmotionType::Shame:
    case EEmotionType::Embarrassment:
        // Стыд — социальная эмоция: его силу задаёт оглядка на других.
        Gain *= FMath::Lerp(0.4f, 1.5f, Personality->Values.Conformity);
        break;

    case EEmotionType::Pride:
        Gain *= FMath::Lerp(0.5f, 1.4f, F.Ambition);
        break;

    case EEmotionType::Curiosity:
        Gain *= FMath::Lerp(0.3f, 1.7f, F.Curiosity);
        break;

    case EEmotionType::Boredom:
        Gain *= FMath::Lerp(0.5f, 1.5f, T.Openness);
        break;

    case EEmotionType::Envy:
        Gain *= FMath::Lerp(1.4f, 0.4f, T.Agreeableness);
        break;

    case EEmotionType::Hope:
        Gain *= FMath::Lerp(0.4f, 1.6f, F.Optimism);
        break;

    default:
        break;
    }

    return FMath::Clamp(Gain, 0.1f, 2.5f);
}

// ---------------------------------------------------------------------------
//  Шаг: затухание, настроение, стресс
// ---------------------------------------------------------------------------

void UEmotionComponent::Advance(float GameDelta, float BodilyDistress, float CurrentWorldTime)
{
    if (GameDelta <= 0.0f)
    {
        return;
    }
    WorldTime = CurrentWorldTime;

    // --- Затухание эмоций ---------------------------------------------------
    for (int32 i = Active.Num() - 1; i >= 0; --i)
    {
        FEmotionInstance& E = Active[i];

        // Подавленные чувства гаснут медленнее — их не «прожили».
        const float SuppressionPenalty = 1.0f - FMath::Clamp(E.Suppressed, 0.0f, 0.7f);
        E.Intensity -= E.DecayRate * GameDelta * SuppressionPenalty;

        if (E.Intensity <= ExtinctionThreshold)
        {
            Active.RemoveAt(i);
        }
    }

    // --- Куда тянут активные эмоции ----------------------------------------
    FAffectPAD Pull;
    float TotalWeight = 0.0f;
    for (const FEmotionInstance& E : Active)
    {
        const FAffectPAD P = EmotionToPAD(E.Type);
        Pull.Pleasure  += P.Pleasure * E.Intensity;
        Pull.Arousal   += P.Arousal * E.Intensity;
        Pull.Dominance += P.Dominance * E.Intensity;
        TotalWeight += E.Intensity;
    }
    if (TotalWeight > 0.0f)
    {
        Pull.Pleasure  /= TotalWeight;
        Pull.Arousal   /= TotalWeight;
        Pull.Dominance /= TotalWeight;
    }

    // --- Настроение: медленно ползёт за эмоциями, но всегда тянется к норме --
    // Это «гедонистическая адаптация»: и радость, и горе со временем стираются.
    const float TowardEmotion = FMath::Clamp(TotalWeight, 0.0f, 1.0f) * 0.00012f * GameDelta;
    const float TowardBaseline = 0.00006f * GameDelta;

    Mood.Mood.Pleasure  = FMath::Lerp(Mood.Mood.Pleasure,  Pull.Pleasure,  FMath::Clamp(TowardEmotion, 0.0f, 1.0f));
    Mood.Mood.Arousal   = FMath::Lerp(Mood.Mood.Arousal,   Pull.Arousal,   FMath::Clamp(TowardEmotion, 0.0f, 1.0f));
    Mood.Mood.Dominance = FMath::Lerp(Mood.Mood.Dominance, Pull.Dominance, FMath::Clamp(TowardEmotion, 0.0f, 1.0f));

    Mood.Mood.Pleasure  = FMath::Lerp(Mood.Mood.Pleasure,  Mood.Baseline.Pleasure,  FMath::Clamp(TowardBaseline, 0.0f, 1.0f));
    Mood.Mood.Arousal   = FMath::Lerp(Mood.Mood.Arousal,   Mood.Baseline.Arousal,   FMath::Clamp(TowardBaseline, 0.0f, 1.0f));
    Mood.Mood.Dominance = FMath::Lerp(Mood.Mood.Dominance, Mood.Baseline.Dominance, FMath::Clamp(TowardBaseline, 0.0f, 1.0f));

    // Телесный дискомфорт напрямую портит настроение — без всяких «мыслей».
    if (BodilyDistress > 0.4f)
    {
        Mood.Mood.Pleasure -= (BodilyDistress - 0.4f) * 0.00008f * GameDelta;
    }

    Mood.Mood.Pleasure  = FMath::Clamp(Mood.Mood.Pleasure, -1.0f, 1.0f);
    Mood.Mood.Arousal   = FMath::Clamp(Mood.Mood.Arousal, -1.0f, 1.0f);
    Mood.Mood.Dominance = FMath::Clamp(Mood.Mood.Dominance, -1.0f, 1.0f);

    // --- Стресс -------------------------------------------------------------
    // Копится от неприятного возбуждения и беспомощности, уходит очень медленно.
    float StressInput = 0.0f;
    for (const FEmotionInstance& E : Active)
    {
        const FAffectPAD P = EmotionToPAD(E.Type);
        if (P.Pleasure < 0.0f)
        {
            StressInput += E.Intensity * FMath::Abs(P.Pleasure) * (0.5f + FMath::Max(0.0f, P.Arousal) * 0.5f);
            // Ощущение беспомощности стрессует сильнее всего.
            if (P.Dominance < 0.0f)
            {
                StressInput += E.Intensity * FMath::Abs(P.Dominance) * 0.4f;
            }
        }
    }
    StressInput += BodilyDistress * 0.35f;
    StressInput += SuppressedLoad * 0.3f;

    const float StressRate = 0.000045f * GameDelta;
    Mood.StressLoad = FMath::Clamp(
        Mood.StressLoad + (FMath::Clamp(StressInput, 0.0f, 1.5f) - 0.28f) * StressRate * 4.0f,
        0.0f, 1.0f);

    // --- Оцепенение: когда долго плохо, психика перестаёт чувствовать -------
    if (Mood.StressLoad > 0.8f)
    {
        Mood.Numbness = FMath::Clamp(Mood.Numbness + 0.00002f * GameDelta, 0.0f, 0.85f);
    }
    else
    {
        Mood.Numbness = FMath::Clamp(Mood.Numbness - 0.00003f * GameDelta, 0.0f, 0.85f);
    }

    // --- Подавленное просачивается обратно ----------------------------------
    if (SuppressedLoad > 0.0f)
    {
        SuppressedLoad = FMath::Max(0.0f, SuppressedLoad - 0.000008f * GameDelta);

        // Прорыв: когда накопилось слишком много, всё вырывается сразу.
        if (SuppressedLoad > 0.85f)
        {
            Ignite(EEmotionType::Anger, 0.7f, nullptr, TEXT("копилось слишком долго"), LinkedPersonality);
            Ignite(EEmotionType::Sadness, 0.6f, nullptr, TEXT("больше не могу держать"), LinkedPersonality);
            SuppressedLoad = 0.25f;
        }
    }

    // --- Скука: рождается сама, если ничего не происходит -------------------
    // Пустота — это тоже переживание, и оно накапливается.
    if (TotalWeight < 0.20f)
    {
        if (GetIntensity(EEmotionType::Boredom) > 0.0f)
        {
            Ignite(EEmotionType::Boredom, 0.00006f * GameDelta, nullptr, TEXT("ничего не происходит"), LinkedPersonality);
        }
        else
        {
            Ignite(EEmotionType::Boredom, 0.03f, nullptr, TEXT("ничего не происходит"), LinkedPersonality);
        }
    }
}

// ---------------------------------------------------------------------------
//  Регуляция и заражение
// ---------------------------------------------------------------------------

void UEmotionComponent::Regulate(ECopingStyle Style, float Effort, const UPersonalityComponent* Personality)
{
    LastCoping = Style;
    const float E = FMath::Clamp(Effort, 0.0f, 1.0f);
    if (E <= 0.0f)
    {
        return;
    }

    switch (Style)
    {
    case ECopingStyle::Suppression:
        // Задавить в себе: наружу не видно, внутри давление растёт.
        for (FEmotionInstance& Em : Active)
        {
            if (!IsPositive(Em.Type))
            {
                const float Hidden = Em.Intensity * E * 0.5f;
                Em.Suppressed = FMath::Clamp(Em.Suppressed + E * 0.4f, 0.0f, 1.0f);
                SuppressedLoad = FMath::Clamp(SuppressedLoad + Hidden * 0.25f, 0.0f, 1.0f);
            }
        }
        Expressiveness = FMath::Max(0.05f, Expressiveness - E * 0.05f);
        break;

    case ECopingStyle::Reappraisal:
        // Переосмыслить: честно снижает силу чувства, но стоит воли.
        for (FEmotionInstance& Em : Active)
        {
            if (!IsPositive(Em.Type))
            {
                Em.Intensity = FMath::Max(0.0f, Em.Intensity - E * 0.30f);
            }
        }
        // И даёт ощущение контроля.
        Mood.Mood.Dominance = FMath::Clamp(Mood.Mood.Dominance + E * 0.05f, -1.0f, 1.0f);
        Mood.StressLoad = FMath::Max(0.0f, Mood.StressLoad - E * 0.03f);
        break;

    case ECopingStyle::ProblemFocused:
        // Заняться делом: не убирает чувство, но возвращает субъектность.
        Mood.Mood.Dominance = FMath::Clamp(Mood.Mood.Dominance + E * 0.08f, -1.0f, 1.0f);
        for (FEmotionInstance& Em : Active)
        {
            if (Em.Type == EEmotionType::Anxiety || Em.Type == EEmotionType::Fear)
            {
                Em.Intensity = FMath::Max(0.0f, Em.Intensity - E * 0.18f);
            }
        }
        Ignite(EEmotionType::Hope, E * 0.2f, nullptr, TEXT("я хотя бы что-то делаю"), Personality);
        break;

    case ECopingStyle::SeekSupport:
        // Поиск поддержки сам по себе облегчает — ещё до того, как поддержат.
        for (FEmotionInstance& Em : Active)
        {
            if (Em.Type == EEmotionType::Loneliness || Em.Type == EEmotionType::Sadness)
            {
                Em.Intensity = FMath::Max(0.0f, Em.Intensity - E * 0.12f);
            }
        }
        break;

    case ECopingStyle::Avoidance:
        // Отвернуться: помогает сейчас, вредит потом.
        for (FEmotionInstance& Em : Active)
        {
            if (!IsPositive(Em.Type))
            {
                Em.Intensity = FMath::Max(0.0f, Em.Intensity - E * 0.22f);
                Em.Suppressed = FMath::Clamp(Em.Suppressed + E * 0.25f, 0.0f, 1.0f);
            }
        }
        SuppressedLoad = FMath::Clamp(SuppressedLoad + E * 0.05f, 0.0f, 1.0f);
        break;

    case ECopingStyle::Aggression:
        // Сорваться: гнев на миг легчает, но стыд приходит следом.
        for (FEmotionInstance& Em : Active)
        {
            if (Em.Type == EEmotionType::Anger || Em.Type == EEmotionType::Frustration)
            {
                Em.Intensity = FMath::Max(0.0f, Em.Intensity - E * 0.45f);
            }
        }
        Ignite(EEmotionType::Shame, E * 0.18f, nullptr, TEXT("не сдержался"), Personality);
        break;

    case ECopingStyle::Rumination:
        // Пережёвывать: чувство не уходит, а разрастается.
        for (FEmotionInstance& Em : Active)
        {
            if (!IsPositive(Em.Type))
            {
                Em.Intensity = FMath::Clamp(Em.Intensity + E * 0.08f, 0.0f, 1.0f);
                Em.DecayRate *= 0.9f;
            }
        }
        Mood.StressLoad = FMath::Clamp(Mood.StressLoad + E * 0.02f, 0.0f, 1.0f);
        break;
    }
}

void UEmotionComponent::CatchFrom(const UEmotionComponent* Other, float Empathy, float Closeness)
{
    if (!Other)
    {
        return;
    }

    // Заражаемся тем, что человек ПОКАЗЫВАЕТ, а не тем, что чувствует.
    const float Susceptibility = FMath::Clamp(Empathy * 0.6f + Closeness * 0.4f, 0.0f, 1.0f) * 0.25f;
    if (Susceptibility < 0.02f)
    {
        return;
    }

    for (const FEmotionInstance& E : Other->Active)
    {
        const float Visible = E.Intensity * Other->Expressiveness;
        if (Visible < 0.2f)
        {
            continue;
        }

        EEmotionType Caught = E.Type;

        // Чужой страх заражает как страх, чужое горе — как сострадание.
        if (E.Type == EEmotionType::Sadness || E.Type == EEmotionType::Loneliness)
        {
            Caught = EEmotionType::Compassion;
        }
        else if (E.Type == EEmotionType::Anger && Closeness < 0.3f)
        {
            // Гнев чужого человека пугает, а не злит.
            Caught = EEmotionType::Fear;
        }

        Ignite(Caught, Visible * Susceptibility, E.Cause, TEXT("считал(а) по другому человеку"), LinkedPersonality);
    }
}

// ---------------------------------------------------------------------------
//  Запросы
// ---------------------------------------------------------------------------

float UEmotionComponent::GetIntensity(EEmotionType Type) const
{
    for (const FEmotionInstance& E : Active)
    {
        if (E.Type == Type)
        {
            return E.Intensity;
        }
    }
    return 0.0f;
}

float UEmotionComponent::GetExpressedIntensity(EEmotionType Type) const
{
    return GetIntensity(Type) * Expressiveness;
}

EEmotionType UEmotionComponent::GetDominant() const
{
    EEmotionType Type;
    float Intensity;
    GetDominantWithIntensity(Type, Intensity);
    return Type;
}

void UEmotionComponent::GetDominantWithIntensity(EEmotionType& OutType, float& OutIntensity) const
{
    OutType = EEmotionType::None;
    OutIntensity = 0.0f;

    for (const FEmotionInstance& E : Active)
    {
        if (E.Intensity > OutIntensity)
        {
            OutIntensity = E.Intensity;
            OutType = E.Type;
        }
    }
}

FAffectPAD UEmotionComponent::GetAffect() const
{
    FAffectPAD Result = Mood.Mood;

    for (const FEmotionInstance& E : Active)
    {
        const FAffectPAD P = EmotionToPAD(E.Type);
        Result.Pleasure  += P.Pleasure * E.Intensity * 0.8f;
        Result.Arousal   += P.Arousal * E.Intensity * 0.8f;
        Result.Dominance += P.Dominance * E.Intensity * 0.8f;
    }

    Result.Pleasure  = FMath::Clamp(Result.Pleasure, -1.0f, 1.0f);
    Result.Arousal   = FMath::Clamp(Result.Arousal, -1.0f, 1.0f);
    Result.Dominance = FMath::Clamp(Result.Dominance, -1.0f, 1.0f);
    return Result;
}

float UEmotionComponent::GetTurmoil() const
{
    float Sum = 0.0f;
    for (const FEmotionInstance& E : Active)
    {
        Sum += E.Intensity;
    }
    return FMath::Clamp(Sum / 2.5f, 0.0f, 1.0f);
}

float UEmotionComponent::GetMoodShift() const
{
    return FMath::Clamp(Mood.Mood.Pleasure - Mood.Baseline.Pleasure, -1.0f, 1.0f);
}

FString UEmotionComponent::DescribeFeeling() const
{
    if (Mood.Numbness > 0.6f)
    {
        return TEXT("внутри пусто");
    }

    // Собираем две самые сильные эмоции — люди редко чувствуют что-то одно.
    TArray<FEmotionInstance> Sorted = Active;
    Sorted.Sort([](const FEmotionInstance& A, const FEmotionInstance& B) { return A.Intensity > B.Intensity; });

    if (Sorted.Num() == 0 || Sorted[0].Intensity < 0.12f)
    {
        if (Mood.Mood.Pleasure > 0.25f) return TEXT("на душе спокойно");
        if (Mood.Mood.Pleasure < -0.25f) return TEXT("что-то не так, а что — не понять");
        return TEXT("ничего особенного");
    }

    auto Qualify = [](float I) -> const TCHAR*
    {
        if (I > 0.75f) return TEXT("очень ");
        if (I > 0.45f) return TEXT("");
        return TEXT("немного ");
    };

    FString Result = FString(Qualify(Sorted[0].Intensity)) + HumanText::EmotionFirstPerson(Sorted[0].Type);

    if (Sorted.Num() > 1 && Sorted[1].Intensity > 0.2f)
    {
        Result += TEXT(", и ") + FString(Qualify(Sorted[1].Intensity)) + HumanText::EmotionFirstPerson(Sorted[1].Type);
    }

    return Result;
}
