// PhysiologyComponent.cpp

#include "PhysiologyComponent.h"
#include "CompleteHumanAI.h"

UPhysiologyComponent::UPhysiologyComponent()
{
    // Телом управляет разум через Advance() — свой тик компоненту не нужен.
    PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
//  Главный шаг
// ---------------------------------------------------------------------------

void UPhysiologyComponent::Advance(float GameDelta, const FPhysiologyDrive& Drive)
{
    if (!bAlive || GameDelta <= 0.0f)
    {
        return;
    }

    const float Hours = GameDelta / 3600.0f;

    // --- ЖКТ: желудок пустеет, кишечник наполняет мочевой пузырь --------------
    // Взрослый ощутимо голодает примерно за 6 часов.
    const float DigestionRate = Drive.Age < 1.5f ? 1.0f / 3.5f : (Drive.Age < 6.0f ? 1.0f / 6.0f : 1.0f / 9.0f);
    const float Digested = FMath::Min(Body.StomachFullness, DigestionRate * Hours * (1.0f + Drive.Exertion * 0.5f));
    Body.StomachFullness = FMath::Max(0.0f, Body.StomachFullness - Digested);

    // Без воды человек живёт около двух суток: за это время тело теряет
    // примерно шестую часть своей воды, и дальше отказывают почки.
    const float DehydrationRate = 1.0f / 48.0f;
    Body.Hydration = FMath::Clamp(
        Body.Hydration - DehydrationRate * Hours * (1.0f + Drive.Exertion * 0.8f + FMath::Max(0.0f, Body.BodyTemperature - 37.0f) * 0.5f),
        0.0f, 1.0f);

    // Пузырь наполняется от выпитого и просто со временем.
    Body.BladderFullness = FMath::Clamp(Body.BladderFullness + (1.0f / 9.0f) * Hours * (Drive.bAsleep ? 0.35f : 1.0f), 0.0f, 1.0f);

    // Чистота уходит от пота и времени.
    Body.Cleanliness = FMath::Clamp(Body.Cleanliness - (1.0f / 30.0f) * Hours * (1.0f + Drive.Exertion * 2.0f), 0.0f, 1.0f);

    // --- Глюкоза: падает с пустым желудком, стресс её выжигает ---------------
    const float TargetGlucose = 3.4f + Body.StomachFullness * 2.4f - Drive.StressLoad * 0.3f;
    Body.GlucoseLevel = FMath::FInterpTo(Body.GlucoseLevel, TargetGlucose, Hours, 2.0f);

    // --- Сон ----------------------------------------------------------------
    if (Drive.bAsleep)
    {
        // Глубокий сон восстанавливает тело, быстрый — психику.
        float Recovery = 1.0f;
        switch (Drive.SleepPhase)
        {
        case ESleepPhase::Deep:   Recovery = 1.6f; break;
        case ESleepPhase::REM:    Recovery = 1.0f; break;
        case ESleepPhase::Light:  Recovery = 0.8f; break;
        case ESleepPhase::Drowsy: Recovery = 0.3f; break;
        default:                  Recovery = 0.5f; break;
        }

        Body.SleepDebt = FMath::Max(0.0f, Body.SleepDebt - Hours * Recovery * 1.4f);
        Body.Stamina = FMath::Clamp(Body.Stamina + Hours * 0.16f * Recovery, 0.0f, 1.0f);
        Body.Immunity = FMath::Clamp(Body.Immunity + Hours * 0.04f * Recovery, 0.0f, 1.0f);
    }
    else
    {
        // Каждый час бодрствования — час долга; невыспавшийся копит его быстрее.
        Body.SleepDebt = FMath::Min(48.0f, Body.SleepDebt + Hours);

        // Силы тратятся на движение и на само существование.
        const float Drain = Hours * (0.04f + Drive.Exertion * 0.14f) * (1.0f + Body.SleepDebt * 0.012f);
        Body.Stamina = FMath::Clamp(Body.Stamina - Drain, 0.0f, 1.0f);

        // Голод и жажда отнимают силы дополнительно.
        if (Body.StomachFullness < 0.15f) Body.Stamina = FMath::Max(0.0f, Body.Stamina - Hours * 0.08f);
        if (Body.Hydration < 0.2f)        Body.Stamina = FMath::Max(0.0f, Body.Stamina - Hours * 0.12f);
    }

    // --- Иммунитет: его едят стресс и недосып --------------------------------
    const float ImmuneDrain = (Drive.StressLoad * 0.05f + FMath::Max(0.0f, Body.SleepDebt - 16.0f) * 0.008f) * Hours;
    Body.Immunity = FMath::Clamp(Body.Immunity - ImmuneDrain + Hours * 0.012f, 0.0f, 1.0f);

    // --- Физическая форма ----------------------------------------------------
    if (Drive.Exertion > 0.4f)
    {
        Body.Fitness = FMath::Clamp(Body.Fitness + Hours * 0.012f, 0.0f, 1.0f);
    }
    else
    {
        Body.Fitness = FMath::Clamp(Body.Fitness - Hours * 0.002f, 0.0f, 1.0f);
    }

    // --- Боль: медленно стихает, эндорфин помогает ---------------------------
    Body.Pain = FMath::Clamp(Body.Pain - Hours * (0.10f + Hormones.Endorphin * 0.25f), 0.0f, 1.0f);

    // --- Температура ---------------------------------------------------------
    float TargetTemp = 36.6f
        + Drive.Exertion * 0.6f
        + FMath::Max(0.0f, Drive.Arousal) * 0.15f
        - Drive.EnvironmentHarshness * 0.3f
        + AilmentSeverity(EAilment::Fever) * 2.5f;
    Body.BodyTemperature = FMath::FInterpTo(Body.BodyTemperature, TargetTemp, Hours, 3.0f);

    UpdateHormones(GameDelta, Drive);
    UpdateVitals(Drive);
    UpdateAilments(GameDelta, Drive);
    UpdateAging(GameDelta, Drive);

    // --- Запас сил: неделя без еды ------------------------------------------
    // Съеденное сперва лежит в желудке, потом переходит в запас тела.
    Body.Reserve = FMath::Clamp(Body.Reserve + Digested * 0.5f, 0.0f, 1.0f);

    const float Cold = FMath::Max(0.0f, 36.6f - Body.BodyTemperature) * 0.06f;
    const float Burn = (1.0f / (7.0f * 24.0f)) * (1.0f + Drive.Exertion * 0.8f + Cold);
    Body.Reserve = FMath::Clamp(Body.Reserve - Burn * Hours, 0.0f, 1.0f);

    // --- Кровь --------------------------------------------------------------
    Body.BloodLitres = FMath::Clamp(Body.BloodLitres + Hours * 0.02f, 0.0f, 5.4f);

    // --- Что с чем делают лишения -------------------------------------------
    // Не «уходит здоровье», а отказывают определённые органы.
    auto Damage = [Hours](float& Organ, float Rate)
    {
        Organ = FMath::Clamp(Organ - Rate * Hours, 0.0f, 1.0f);
    };
    auto Mend = [Hours](float& Organ, float Rate)
    {
        Organ = FMath::Clamp(Organ + Rate * Hours, 0.0f, 1.0f);
    };

    if (Body.Hydration < 0.35f)
    {
        const float Thirst = (0.35f - Body.Hydration) / 0.35f;
        Damage(Organs.Kidneys, Thirst * 0.05f);
        Damage(Organs.Brain, Thirst * 0.02f);
        Body.Pain = FMath::Clamp(Body.Pain + Thirst * Hours * 0.5f, 0.0f, 1.0f);
    }

    if (Body.Reserve < 0.3f)
    {
        const float Starving = (0.3f - Body.Reserve) / 0.3f;
        Damage(Organs.Muscle, Starving * 0.04f);
        Damage(Organs.Heart, Starving * 0.02f);
        Damage(Organs.Liver, Starving * 0.02f);
        Body.Stamina = FMath::Max(0.0f, Body.Stamina - Starving * Hours * 0.5f);
    }

    if (Body.BodyTemperature < 35.0f)
    {
        Damage(Organs.Brain, (35.0f - Body.BodyTemperature) * 0.04f);
        Damage(Organs.Heart, (35.0f - Body.BodyTemperature) * 0.03f);
    }
    if (Body.BodyTemperature > 39.0f)
    {
        Damage(Organs.Brain, (Body.BodyTemperature - 39.0f) * 0.05f);
        Damage(Organs.Liver, (Body.BodyTemperature - 39.0f) * 0.03f);
    }

    if (Body.SleepDebt > 40.0f)
    {
        Damage(Organs.Brain, (Body.SleepDebt - 40.0f) * 0.002f);
    }

    for (const FAilmentInstance& A : Ailments)
    {
        switch (A.Type)
        {
        case EAilment::Fever:      Damage(Organs.Lungs, A.Severity * 0.02f); break;
        case EAilment::Cold:       Damage(Organs.Lungs, A.Severity * 0.01f); break;
        case EAilment::Injury:     Damage(Organs.Skin, A.Severity * 0.03f);
                                   Body.BloodLitres = FMath::Max(0.0f, Body.BloodLitres - A.Severity * Hours * 0.06f); break;
        case EAilment::Exhaustion: Damage(Organs.Muscle, A.Severity * 0.02f); break;
        default:                   Damage(Organs.Gut, A.Severity * 0.01f); break;
        }
    }

    // Сытый, напоенный и отдохнувший чинится сам.
    if (Body.Hydration > 0.5f && Body.Reserve > 0.4f)
    {
        const float Rate = (Drive.bAsleep ? 0.03f : 0.012f) * Body.Immunity;
        Mend(Organs.Kidneys, Rate);
        Mend(Organs.Muscle, Rate);
        Mend(Organs.Liver, Rate * 0.6f);
        Mend(Organs.Skin, Rate);
        Mend(Organs.Gut, Rate);
        Mend(Organs.Heart, Rate * 0.3f);
        Mend(Organs.Lungs, Rate * 0.4f);
        Mend(Organs.Brain, Rate * 0.2f);
        Body.Pain = FMath::Max(0.0f, Body.Pain - Rate * Hours * 2.0f);
    }

    Body.Health = Condition();

    // --- Смерть: от чего именно ---------------------------------------------
    FString Reason;
    if (Body.Hydration <= 0.0f)                Reason = TEXT("обезвоживание: тело потеряло воду");
    else if (Body.Reserve <= 0.0f)             Reason = TEXT("истощение: запас сил кончился");
    else if (Body.BodyTemperature < 28.0f)     Reason = TEXT("переохлаждение");
    else if (Body.BodyTemperature > 42.0f)     Reason = TEXT("жар, которого тело не вынесло");
    else if (Body.BloodLitres < 2.8f)          Reason = TEXT("потеря крови");
    else if (Organs.Heart < 0.12f)             Reason = TEXT("отказало сердце");
    else if (Organs.Lungs < 0.12f)             Reason = TEXT("отказали лёгкие");
    else if (Organs.Brain < 0.12f)             Reason = TEXT("отказал мозг");
    else if (Organs.Liver < 0.1f)              Reason = TEXT("отказала печень");
    else if (Organs.Kidneys < 0.1f)            Reason = TEXT("отказали почки");

    if (!Reason.IsEmpty())
    {
        Die(Reason);
    }

    MirrorLegacyFields();
}

float UPhysiologyComponent::Condition() const
{
    const float Worst = FMath::Min(
        FMath::Min(FMath::Min(Organs.Heart, Organs.Lungs), FMath::Min(Organs.Brain, Organs.Liver)),
        FMath::Min(FMath::Min(Organs.Kidneys, Organs.Gut), FMath::Min(Organs.Muscle, Organs.Skin)));

    const float Want = FMath::Min(Body.Hydration / 0.5f, Body.Reserve / 0.5f);
    const float Hurt = 1.0f - Body.Pain * 0.5f;
    const float Warmth = 1.0f - FMath::Abs(Body.BodyTemperature - 36.6f) * 0.12f;

    return FMath::Clamp(FMath::Min(FMath::Min(Worst, Want), FMath::Min(Hurt, Warmth)), 0.0f, 1.0f);
}

FString UPhysiologyComponent::Diagnosis() const
{
    TArray<FString> Notes;

    if (Body.Hydration < 0.35f)            Notes.Add(TEXT("обезвоживание"));
    if (Body.Reserve < 0.3f)               Notes.Add(TEXT("истощение"));
    if (Body.BodyTemperature < 35.5f)      Notes.Add(TEXT("переохлаждение"));
    if (Body.BodyTemperature > 37.5f)      Notes.Add(TEXT("жар"));
    if (Body.BloodLitres < 4.2f)           Notes.Add(TEXT("кровопотеря"));
    if (Organs.Heart < 0.7f)               Notes.Add(TEXT("сердце сдаёт"));
    if (Organs.Lungs < 0.7f)               Notes.Add(TEXT("тяжело дышать"));
    if (Organs.Kidneys < 0.7f)             Notes.Add(TEXT("почки"));
    if (Organs.Liver < 0.7f)               Notes.Add(TEXT("печень"));
    if (Organs.Brain < 0.8f)               Notes.Add(TEXT("мутится в голове"));
    if (Organs.Muscle < 0.6f)              Notes.Add(TEXT("сил нет"));
    if (Body.Pain > 0.4f)                  Notes.Add(TEXT("боль"));

    return Notes.Num() > 0 ? FString::Join(Notes, TEXT(", ")) : TEXT("тело в порядке");
}

void UPhysiologyComponent::UpdatePhysiology(float DeltaSeconds, ACompleteHumanNPC* Owner)
{
    // Старый вход оставлен для совместимости: собирает минимальный Drive
    // и отдаёт его новому механизму.
    if (!Owner)
    {
        return;
    }

    FPhysiologyDrive Drive;
    Drive.HourOfDay = 12.0f;
    Drive.Arousal = 0.0f;
    Drive.Valence = 0.0f;
    Advance(DeltaSeconds, Drive);
}

// ---------------------------------------------------------------------------
//  Гормоны
// ---------------------------------------------------------------------------

void UPhysiologyComponent::UpdateHormones(float GameDelta, const FPhysiologyDrive& Drive)
{
    const float Hours = GameDelta / 3600.0f;
    const float Minutes = GameDelta / 60.0f;

    // Адреналин: взлетает мгновенно, падает за минуты.
    const float AdrenalineTarget = FMath::Clamp(FMath::Max(0.0f, Drive.Arousal) * (Drive.Valence < 0.0f ? 1.0f : 0.5f), 0.0f, 1.0f);
    if (AdrenalineTarget > Hormones.Adrenaline)
    {
        Hormones.Adrenaline = FMath::FInterpTo(Hormones.Adrenaline, AdrenalineTarget, Minutes, 4.0f);
    }
    else
    {
        Hormones.Adrenaline = FMath::FInterpTo(Hormones.Adrenaline, 0.05f, Minutes, 0.25f);
    }

    // Кортизол: медленный. Устойчивость личности приглушает выброс,
    // но хронический стресс всё равно поднимает базовый уровень.
    const float CortisolTarget = FMath::Clamp(
        Drive.StressLoad * (1.2f - Drive.Resilience * 0.5f) + FMath::Max(0.0f, -Drive.Valence) * 0.3f,
        0.05f, 1.0f);
    Hormones.Cortisol = FMath::FInterpTo(Hormones.Cortisol, CortisolTarget, Hours, 1.5f);

    // Дофамин: всплеск от награды, иначе медленно возвращается к базе.
    // Важно: дофамин — про предвкушение, поэтому он живёт до получения.
    Hormones.Dopamine = FMath::Clamp(Hormones.Dopamine + Drive.RewardSignal * 0.5f, 0.0f, 1.0f);
    Hormones.Dopamine = FMath::FInterpTo(Hormones.Dopamine, 0.35f + Drive.Valence * 0.15f, Minutes, 0.08f);

    // Серотонин: устойчивость настроения; падает от хронического стресса.
    const float SerotoninTarget = FMath::Clamp(0.5f + Drive.Valence * 0.25f - Drive.StressLoad * 0.3f + Body.Health * 0.15f, 0.0f, 1.0f);
    Hormones.Serotonin = FMath::FInterpTo(Hormones.Serotonin, SerotoninTarget, Hours, 0.6f);

    // Окситоцин: от человеческого тепла, тает за часы одиночества.
    const float OxyTarget = FMath::Clamp(Drive.SocialWarmth, 0.0f, 1.0f);
    Hormones.Oxytocin = FMath::FInterpTo(Hormones.Oxytocin, OxyTarget * 0.9f + 0.05f, Hours, 1.2f);

    // Эндорфин: нагрузка и боль запускают собственное обезболивание.
    const float EndoTarget = FMath::Clamp(Drive.Exertion * 0.5f + Body.Pain * 0.6f, 0.0f, 1.0f);
    Hormones.Endorphin = FMath::FInterpTo(Hormones.Endorphin, EndoTarget, Minutes, 0.5f);

    // Тестостерон: возраст и статус; после победы растёт, после поражения падает.
    const float AgeFactor = FMath::Clamp(1.0f - FMath::Max(0.0f, Drive.Age - 30.0f) / 60.0f, 0.2f, 1.0f);
    const float TestoTarget = FMath::Clamp(0.45f * AgeFactor + Drive.RewardSignal * 0.2f - Drive.StressLoad * 0.15f, 0.0f, 1.0f);
    Hormones.Testosterone = FMath::FInterpTo(Hormones.Testosterone, TestoTarget, Hours, 0.4f);

    // Мелатонин: строго по часам, но свет дня и возбуждение его давят.
    const float MelatoninTarget = FMath::Clamp(CircadianSleepiness(Drive.HourOfDay) - FMath::Max(0.0f, Drive.Arousal) * 0.4f, 0.0f, 1.0f);
    Hormones.Melatonin = FMath::FInterpTo(Hormones.Melatonin, MelatoninTarget, Hours, 2.5f);
}

float UPhysiologyComponent::CircadianSleepiness(float HourOfDay)
{
    // Два провала: глубокая ночь (около 3-4) и послеобеденный спад (около 14-15).
    const float H = FMath::Fmod(FMath::Max(0.0f, HourOfDay), 24.0f);

    // Основная ночная волна: максимум в 3:30.
    const float NightPhase = (H - 3.5f) / 24.0f * 2.0f * PI;
    const float Night = 0.5f + 0.5f * FMath::Cos(NightPhase);

    // Послеобеденная волна: небольшой горб около 14:30.
    const float Dip = FMath::Exp(-FMath::Square(H - 14.5f) / 3.0f) * 0.22f;

    return FMath::Clamp(FMath::Pow(Night, 2.2f) + Dip, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
//  Показатели
// ---------------------------------------------------------------------------

void UPhysiologyComponent::UpdateVitals(const FPhysiologyDrive& Drive)
{
    // Покойный пульс зависит от формы и возраста.
    const float RestingHR = 72.0f - Body.Fitness * 15.0f + FMath::Max(0.0f, Drive.Age - 40.0f) * 0.12f;

    Body.HeartRate = RestingHR
        + Hormones.Adrenaline * 55.0f
        + Drive.Exertion * 70.0f * (1.3f - Body.Fitness * 0.5f)
        + FMath::Max(0.0f, Body.BodyTemperature - 37.0f) * 8.0f
        + Hormones.Cortisol * 10.0f;
    Body.HeartRate = FMath::Clamp(Body.HeartRate, 38.0f, 205.0f);

    Body.BloodPressureSystolic = 116.0f
        + Hormones.Adrenaline * 30.0f
        + Hormones.Cortisol * 14.0f
        + Drive.Exertion * 25.0f
        + FMath::Max(0.0f, BiologicalAge - 35.0f) * 0.45f;
    Body.BloodPressureDiastolic = Body.BloodPressureSystolic * 0.65f + Hormones.Cortisol * 5.0f;

    Body.RespiratoryRate = 14.0f + Hormones.Adrenaline * 12.0f + Drive.Exertion * 18.0f + Body.Pain * 4.0f;
    Body.RespiratoryRate = FMath::Clamp(Body.RespiratoryRate, 8.0f, 44.0f);

    Body.OxygenSaturation = FMath::Clamp(
        98.5f - Drive.Exertion * 2.0f - AilmentSeverity(EAilment::Cold) * 3.0f - FMath::Max(0.0f, BiologicalAge - 60.0f) * 0.03f,
        80.0f, 100.0f);
}

// ---------------------------------------------------------------------------
//  Болезни
// ---------------------------------------------------------------------------

void UPhysiologyComponent::UpdateAilments(float GameDelta, const FPhysiologyDrive& Drive)
{
    const float Hours = GameDelta / 3600.0f;

    for (int32 i = Ailments.Num() - 1; i >= 0; --i)
    {
        FAilmentInstance& A = Ailments[i];

        if (A.RemainingTime > 0.0f)
        {
            A.RemainingTime -= GameDelta;
            // Иммунитет ускоряет выздоровление.
            A.Severity = FMath::Max(0.0f, A.Severity - Hours * 0.03f * (0.5f + Body.Immunity));
            if (A.RemainingTime <= 0.0f || A.Severity <= 0.01f)
            {
                Ailments.RemoveAt(i);
                continue;
            }
        }
        else
        {
            // Хроническое: тяжесть зависит от того, устранена ли причина.
            if (A.Type == EAilment::Burnout || A.Type == EAilment::Depression)
            {
                const float Pressure = Drive.StressLoad - 0.35f;
                A.Severity = FMath::Clamp(A.Severity + Pressure * Hours * 0.06f, 0.0f, 1.0f);
                if (A.Severity <= 0.02f)
                {
                    Ailments.RemoveAt(i);
                    continue;
                }
            }
        }

        // Симптомы.
        switch (A.Type)
        {
        case EAilment::Headache:
        case EAilment::Injury:
            Body.Pain = FMath::Max(Body.Pain, A.Severity * 0.8f);
            break;
        case EAilment::Fever:
        case EAilment::Cold:
            Body.Stamina = FMath::Max(0.0f, Body.Stamina - Hours * A.Severity * 0.06f);
            break;
        default:
            break;
        }
    }

    // Редкая проверка на новые болезни — раз в игровой час.
    SlowTimer += GameDelta;
    if (SlowTimer < 3600.0f)
    {
        return;
    }
    SlowTimer = 0.0f;

    // Простуда: шанс тем выше, чем ниже иммунитет и хуже погода.
    if (!HasAilment(EAilment::Cold))
    {
        const float Risk = (1.0f - Body.Immunity) * 0.05f + Drive.EnvironmentHarshness * 0.02f;
        if (FMath::FRand() < Risk)
        {
            ContractAilment(EAilment::Cold, FMath::FRandRange(0.2f, 0.6f), FMath::FRandRange(2.0f, 5.0f) * 86400.0f);
        }
    }

    // Истощение от многодневного недосыпа.
    if (Body.SleepDebt > 28.0f && !HasAilment(EAilment::Exhaustion))
    {
        ContractAilment(EAilment::Exhaustion, FMath::Clamp((Body.SleepDebt - 28.0f) / 20.0f, 0.2f, 1.0f), 86400.0f);
    }

    // Выгорание от долгого высокого стресса.
    if (Drive.StressLoad > 0.72f && !HasAilment(EAilment::Burnout))
    {
        if (FMath::FRand() < 0.04f)
        {
            ContractAilment(EAilment::Burnout, 0.25f, -1.0f);
        }
    }

    // Головная боль от обезвоживания и напряжения.
    if (!HasAilment(EAilment::Headache))
    {
        const float Risk = FMath::Max(0.0f, 0.45f - Body.Hydration) * 0.4f + Drive.StressLoad * 0.05f;
        if (FMath::FRand() < Risk)
        {
            ContractAilment(EAilment::Headache, FMath::FRandRange(0.2f, 0.5f), FMath::FRandRange(2.0f, 6.0f) * 3600.0f);
        }
    }
}

// ---------------------------------------------------------------------------
//  Старение
// ---------------------------------------------------------------------------

void UPhysiologyComponent::UpdateAging(float GameDelta, const FPhysiologyDrive& Drive)
{
    const float Years = GameDelta / (86400.0f * 365.0f);

    // Биологический возраст идёт быстрее паспортного при плохой жизни
    // и медленнее при хорошей.
    const float Accelerator = 1.0f
        + Hormones.Cortisol * 0.6f
        + FMath::Max(0.0f, Body.SleepDebt - 12.0f) * 0.01f
        - Body.Fitness * 0.25f;

    BiologicalAge += Years * FMath::Max(0.3f, Accelerator);

    // Паспортный возраст тянет биологический к себе — чтобы они не разъезжались.
    BiologicalAge = FMath::FInterpTo(BiologicalAge, Drive.Age, Years, 0.25f);

    // После шестидесяти запас прочности падает.
    if (BiologicalAge > 60.0f)
    {
        const float Decline = (BiologicalAge - 60.0f) / 40.0f;
        Body.Immunity = FMath::Min(Body.Immunity, 1.0f - Decline * 0.4f);
        Body.Fitness = FMath::Min(Body.Fitness, 1.0f - Decline * 0.5f);
    }

    // Естественная смерть: вероятность резко растёт после восьмидесяти.
    if (BiologicalAge > 78.0f)
    {
        const float AnnualRisk = FMath::Pow((BiologicalAge - 78.0f) / 22.0f, 2.2f) * 0.5f;
        if (FMath::FRand() < AnnualRisk * Years)
        {
            Die(TEXT("старость"));
        }
    }
}

// ---------------------------------------------------------------------------
//  Действия над телом
// ---------------------------------------------------------------------------

void UPhysiologyComponent::Eat(float Nutrition)
{
    const float Taken = FMath::Min(FMath::Clamp(Nutrition, 0.0f, 1.0f), 1.0f - Body.StomachFullness);
    Body.StomachFullness = FMath::Clamp(Body.StomachFullness + Taken, 0.0f, 1.0f);
    Body.GlucoseLevel = FMath::Clamp(Body.GlucoseLevel + Taken * 1.5f, 3.0f, 9.0f);
    Body.BladderFullness = FMath::Clamp(Body.BladderFullness + Taken * 0.12f, 0.0f, 1.0f);
    // Еда — это ещё и маленькая радость: короткий дофаминовый отклик.
    Hormones.Dopamine = FMath::Clamp(Hormones.Dopamine + Nutrition * 0.15f, 0.0f, 1.0f);
}

void UPhysiologyComponent::Drink(float Amount)
{
    const float Taken = FMath::Min(FMath::Clamp(Amount, 0.0f, 1.0f), 1.0f - Body.Hydration);
    Body.Hydration = FMath::Clamp(Body.Hydration + Taken, 0.0f, 1.0f);
    Body.BladderFullness = FMath::Clamp(Body.BladderFullness + Taken * 0.35f, 0.0f, 1.0f);
}

void UPhysiologyComponent::Relieve()
{
    Body.BladderFullness = 0.0f;
}

void UPhysiologyComponent::WashUp()
{
    Body.Cleanliness = 1.0f;
    // Душ немного снимает напряжение.
    Hormones.Cortisol = FMath::Max(0.05f, Hormones.Cortisol - 0.08f);
}

void UPhysiologyComponent::Exercise(float GameDelta, float Intensity)
{
    const float Hours = GameDelta / 3600.0f;
    const float I = FMath::Clamp(Intensity, 0.0f, 1.0f);

    Body.Stamina = FMath::Clamp(Body.Stamina - Hours * I * 0.5f, 0.0f, 1.0f);
    Body.Fitness = FMath::Clamp(Body.Fitness + Hours * I * 0.05f, 0.0f, 1.0f);
    Hormones.Endorphin = FMath::Clamp(Hormones.Endorphin + Hours * I * 0.6f, 0.0f, 1.0f);
    Hormones.Cortisol = FMath::Max(0.05f, Hormones.Cortisol - Hours * I * 0.2f);
}

bool UPhysiologyComponent::ApplyNeedEffect(ENeedType Need, float Amount, float GameDelta)
{
    switch (Need)
    {
    case ENeedType::Hunger:
        if (Amount > 0.0f) Eat(Amount);
        else Body.StomachFullness = FMath::Clamp(Body.StomachFullness + Amount, 0.0f, 1.0f);
        return true;

    case ENeedType::Thirst:
        if (Amount > 0.0f) Drink(Amount);
        else Body.Hydration = FMath::Clamp(Body.Hydration + Amount, 0.0f, 1.0f);
        return true;

    case ENeedType::Bladder:
        // Облегчиться можно только полностью — половину не сходишь.
        if (Amount > 0.5f) Relieve();
        else Body.BladderFullness = FMath::Clamp(Body.BladderFullness - Amount, 0.0f, 1.0f);
        return true;

    case ENeedType::Hygiene:
        if (Amount > 0.7f) WashUp();
        else Body.Cleanliness = FMath::Clamp(Body.Cleanliness + Amount, 0.0f, 1.0f);
        return true;

    case ENeedType::Sleep:
        // Долг сна отдаётся только сном; здесь — лишь короткая передышка.
        Body.SleepDebt = FMath::Max(0.0f, Body.SleepDebt - Amount * (GameDelta / 3600.0f));
        return true;

    case ENeedType::Comfort:
        if (Amount > 0.0f)
        {
            Body.Pain = FMath::Max(0.0f, Body.Pain - Amount * 0.4f);
            Body.Stamina = FMath::Clamp(Body.Stamina + Amount * 0.25f, 0.0f, 1.0f);
        }
        else
        {
            // Что-то было неприятно телу: усталость и напряжение.
            Body.Stamina = FMath::Clamp(Body.Stamina + Amount * 0.2f, 0.0f, 1.0f);
            Hormones.Cortisol = FMath::Clamp(Hormones.Cortisol - Amount * 0.1f, 0.0f, 1.0f);
        }
        return true;

    case ENeedType::Health:
        Body.Health = FMath::Clamp(Body.Health + Amount * 0.15f, 0.0f, 1.0f);
        if (Amount > 0.0f)
        {
            Body.Fitness = FMath::Clamp(Body.Fitness + Amount * 0.02f, 0.0f, 1.0f);
        }
        return true;

    default:
        // Всё остальное — про душу, а не про тело.
        return false;
    }
}

void UPhysiologyComponent::TakeInjury(float Severity, const FString& Cause)
{
    const float S = FMath::Clamp(Severity, 0.0f, 1.0f);
    Body.Pain = FMath::Clamp(Body.Pain + S, 0.0f, 1.0f);

    Organs.Skin = FMath::Clamp(Organs.Skin - S * 0.5f, 0.0f, 1.0f);
    Organs.Bones = FMath::Clamp(Organs.Bones - S * 0.3f, 0.0f, 1.0f);
    Organs.Muscle = FMath::Clamp(Organs.Muscle - S * 0.25f, 0.0f, 1.0f);
    Body.BloodLitres = FMath::Max(0.0f, Body.BloodLitres - S * 1.6f);

    if (S > 0.7f)
    {
        Organs.Lungs = FMath::Clamp(Organs.Lungs - (S - 0.7f) * 1.2f, 0.0f, 1.0f);
        Organs.Heart = FMath::Clamp(Organs.Heart - (S - 0.7f) * 0.8f, 0.0f, 1.0f);
    }

    ContractAilment(EAilment::Injury, S, FMath::Lerp(3600.0f, 14.0f * 86400.0f, S));
    Body.Health = Condition();

    if (Body.BloodLitres < 2.8f || Organs.Heart < 0.12f || Organs.Lungs < 0.12f)
    {
        Die(Cause.IsEmpty() ? TEXT("тяжёлая травма") : Cause);
    }
}

void UPhysiologyComponent::ContractAilment(EAilment Type, float Severity, float DurationGameSeconds)
{
    for (FAilmentInstance& A : Ailments)
    {
        if (A.Type == Type)
        {
            // Уже болеет — становится только хуже.
            A.Severity = FMath::Clamp(A.Severity + Severity * 0.5f, 0.0f, 1.0f);
            if (DurationGameSeconds > 0.0f && A.RemainingTime > 0.0f)
            {
                A.RemainingTime = FMath::Max(A.RemainingTime, DurationGameSeconds);
            }
            return;
        }
    }

    FAilmentInstance New;
    New.Type = Type;
    New.Severity = FMath::Clamp(Severity, 0.0f, 1.0f);
    New.RemainingTime = DurationGameSeconds;
    Ailments.Add(New);
}

bool UPhysiologyComponent::HasAilment(EAilment Type) const
{
    for (const FAilmentInstance& A : Ailments)
    {
        if (A.Type == Type)
        {
            return true;
        }
    }
    return false;
}

float UPhysiologyComponent::AilmentSeverity(EAilment Type) const
{
    for (const FAilmentInstance& A : Ailments)
    {
        if (A.Type == Type)
        {
            return A.Severity;
        }
    }
    return 0.0f;
}

void UPhysiologyComponent::CureAilment(EAilment Type)
{
    Ailments.RemoveAll([Type](const FAilmentInstance& A) { return A.Type == Type; });
}

// ---------------------------------------------------------------------------
//  Запросы
// ---------------------------------------------------------------------------

float UPhysiologyComponent::GetSleepPressure(float HourOfDay) const
{
    const float Homeostatic = FMath::Clamp(Body.SleepDebt / 17.0f, 0.0f, 1.2f);
    const float Circadian = CircadianSleepiness(HourOfDay);
    const float Chemical = Hormones.Melatonin;

    // Адреналин способен перебить любую сонливость — но лишь ненадолго.
    const float Override = Hormones.Adrenaline * 0.6f;

    return FMath::Clamp(Homeostatic * 0.55f + Circadian * 0.30f + Chemical * 0.15f - Override, 0.0f, 1.0f);
}

float UPhysiologyComponent::GetMovementSpeedMultiplier() const
{
    float Mult = 0.55f + Body.Stamina * 0.45f;
    Mult *= (1.0f - Body.Pain * 0.4f);
    Mult *= (1.0f - FMath::Clamp((BiologicalAge - 55.0f) / 50.0f, 0.0f, 0.45f));
    Mult *= (0.85f + Body.Fitness * 0.25f);
    // Адреналин ненадолго даёт лишние силы.
    Mult *= (1.0f + Hormones.Adrenaline * 0.25f);
    return FMath::Clamp(Mult, 0.25f, 1.35f);
}

float UPhysiologyComponent::GetCognitiveImpairment() const
{
    float Impairment = 0.0f;
    Impairment += Body.Pain * 0.35f;
    Impairment += FMath::Clamp(Body.SleepDebt / 24.0f, 0.0f, 1.0f) * 0.40f;
    Impairment += FMath::Max(0.0f, Body.BodyTemperature - 37.5f) * 0.25f;
    Impairment += FMath::Max(0.0f, 4.0f - Body.GlucoseLevel) * 0.15f;
    Impairment += FMath::Max(0.0f, 0.25f - Body.Hydration) * 0.6f;
    Impairment += AilmentSeverity(EAilment::Exhaustion) * 0.3f;
    return FMath::Clamp(Impairment, 0.0f, 1.0f);
}

float UPhysiologyComponent::GetSatiety() const
{
    const float Empty = 1.0f - FMath::Clamp(Body.StomachFullness, 0.0f, 1.0f);
    const float Stomach = 1.0f - Empty * Empty;
    return FMath::Clamp(Stomach * 0.75f + FMath::Clamp(Body.Reserve, 0.0f, 1.0f) * 0.25f, 0.0f, 1.0f);
}

float UPhysiologyComponent::GetBodilyDistress() const
{
    float D = 0.0f;
    D = FMath::Max(D, Body.Pain);
    D = FMath::Max(D, 1.0f - GetSatiety());
    D = FMath::Max(D, 1.0f - Body.Hydration);
    D = FMath::Max(D, Body.BladderFullness);
    D = FMath::Max(D, FMath::Clamp(Body.SleepDebt / 20.0f, 0.0f, 1.0f));
    D = FMath::Max(D, 1.0f - Body.Health);
    return FMath::Clamp(D, 0.0f, 1.0f);
}

FString UPhysiologyComponent::DescribeFeeling() const
{
    // Возвращаем самое громкое, что сейчас говорит тело.
    struct FSignal { float Strength; const TCHAR* Text; };

    TArray<FSignal> Signals;
    Signals.Add({ Body.Pain,                                       TEXT("болит") });
    Signals.Add({ 1.0f - GetSatiety(),                             TEXT("сосёт под ложечкой") });
    Signals.Add({ 1.0f - Body.Hydration,                           TEXT("пересохло во рту") });
    Signals.Add({ Body.BladderFullness,                            TEXT("терпеть уже трудно") });
    Signals.Add({ FMath::Clamp(Body.SleepDebt / 18.0f, 0.f, 1.f),  TEXT("глаза слипаются") });
    Signals.Add({ 1.0f - Body.Cleanliness,                         TEXT("чувствую себя грязным") });
    Signals.Add({ 1.0f - Body.Stamina,                             TEXT("ноги не идут") });
    Signals.Add({ AilmentSeverity(EAilment::Cold),                 TEXT("знобит") });
    Signals.Add({ AilmentSeverity(EAilment::Headache),             TEXT("ломит голову") });
    Signals.Add({ AilmentSeverity(EAilment::Fever),                TEXT("горю") });

    float Best = 0.5f;
    const TCHAR* BestText = TEXT("тело в порядке");
    for (const FSignal& S : Signals)
    {
        if (S.Strength > Best)
        {
            Best = S.Strength;
            BestText = S.Text;
        }
    }
    return FString(BestText);
}

void UPhysiologyComponent::Die(const FString& Reason)
{
    if (!bAlive)
    {
        return;
    }
    bAlive = false;
    CauseOfDeath = Reason;
    Body.Health = 0.0f;
    Body.HeartRate = 0.0f;
    Body.RespiratoryRate = 0.0f;
    MirrorLegacyFields();
}

void UPhysiologyComponent::Revive()
{
    const float Fitness = Body.Fitness;
    Body = FBodyState();
    Body.Fitness = Fitness;
    Hormones = FHormones();
    Organs = FOrgans();
    Ailments.Reset();
    bAlive = true;
    CauseOfDeath.Empty();
    MirrorLegacyFields();
}

void UPhysiologyComponent::MirrorLegacyFields()
{
    HeartRate = Body.HeartRate;
    BloodPressureSystolic = Body.BloodPressureSystolic;
    BloodPressureDiastolic = Body.BloodPressureDiastolic;
    RespiratoryRate = Body.RespiratoryRate;
    OxygenSaturation = Body.OxygenSaturation;
    BodyTemperature = Body.BodyTemperature;
    GlucoseLevel = Body.GlucoseLevel;
    AdrenalineLevel = Hormones.Adrenaline;
    CortisolLevel = Hormones.Cortisol;
}
