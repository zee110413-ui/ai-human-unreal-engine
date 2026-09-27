// PersonalityComponent.cpp

#include "PersonalityComponent.h"

UPersonalityComponent::UPersonalityComponent()
{
    // Личность не нуждается в тике: она меняется событиями, а не временем.
    PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
//  Генерация
// ---------------------------------------------------------------------------

namespace
{
    /** Нормальное распределение вокруг Mean — людей «средних» больше, чем крайних. */
    float Bell(float Mean, float Spread)
    {
        // Сумма трёх равномерных даёт приличное приближение нормального.
        const float R = (FMath::FRand() + FMath::FRand() + FMath::FRand()) / 3.0f;
        return FMath::Clamp(Mean + (R - 0.5f) * 2.0f * Spread, 0.0f, 1.0f);
    }
}

void UPersonalityComponent::GenerateRandom()
{
    // Базовые черты — колокол вокруг середины.
    Traits.Openness          = Bell(0.5f, 0.45f);
    Traits.Conscientiousness = Bell(0.5f, 0.45f);
    Traits.Extraversion      = Bell(0.5f, 0.45f);
    Traits.Agreeableness     = Bell(0.55f, 0.40f);
    Traits.Neuroticism       = Bell(0.45f, 0.45f);

    // Реальные корреляции: невротики чуть менее экстравертны и менее уживчивы;
    // открытость слегка тянет за собой экстраверсию.
    Traits.Extraversion  = FMath::Clamp(Traits.Extraversion - (Traits.Neuroticism - 0.5f) * 0.25f, 0.0f, 1.0f);
    Traits.Agreeableness = FMath::Clamp(Traits.Agreeableness - (Traits.Neuroticism - 0.5f) * 0.15f, 0.0f, 1.0f);
    Traits.Extraversion  = FMath::Clamp(Traits.Extraversion + (Traits.Openness - 0.5f) * 0.12f, 0.0f, 1.0f);

    Plasticity = Bell(0.5f, 0.3f);

    DeriveFromTraits();
}

void UPersonalityComponent::DeriveFromTraits()
{
    const float O = Traits.Openness;
    const float C = Traits.Conscientiousness;
    const float E = Traits.Extraversion;
    const float A = Traits.Agreeableness;
    const float N = Traits.Neuroticism;

    // --- Фасеты. Это не просто копии черт: у каждой своя формула и свой шум,
    //     поэтому два человека с одинаковой «пятёркой» всё равно разные. ---
    Facets.Impulsivity   = FMath::Clamp((1.0f - C) * 0.6f + N * 0.25f + FMath::FRandRange(-0.12f, 0.12f), 0.0f, 1.0f);
    Facets.RiskTaking    = FMath::Clamp(O * 0.35f + E * 0.30f + (1.0f - N) * 0.25f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Facets.Vengefulness  = FMath::Clamp((1.0f - A) * 0.6f + N * 0.25f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Facets.Honesty       = FMath::Clamp(A * 0.45f + C * 0.35f + FMath::FRandRange(-0.20f, 0.20f), 0.0f, 1.0f);
    Facets.Empathy       = FMath::Clamp(A * 0.55f + O * 0.20f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Facets.Stubbornness  = FMath::Clamp((1.0f - O) * 0.45f + (1.0f - A) * 0.30f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Facets.Optimism      = FMath::Clamp((1.0f - N) * 0.55f + E * 0.25f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Facets.Curiosity     = FMath::Clamp(O * 0.75f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Facets.SelfControl   = FMath::Clamp(C * 0.65f + (1.0f - N) * 0.20f + FMath::FRandRange(-0.12f, 0.12f), 0.0f, 1.0f);
    Facets.TraitAnxiety  = FMath::Clamp(N * 0.80f + FMath::FRandRange(-0.12f, 0.12f), 0.0f, 1.0f);
    Facets.Sociability   = FMath::Clamp(E * 0.75f + A * 0.15f + FMath::FRandRange(-0.12f, 0.12f), 0.0f, 1.0f);
    Facets.Ambition      = FMath::Clamp(C * 0.40f + E * 0.25f + (1.0f - A) * 0.15f + FMath::FRandRange(-0.18f, 0.18f), 0.0f, 1.0f);

    // --- Ценности ---
    Values.SelfDirection = FMath::Clamp(O * 0.6f + (1.0f - Traits.Agreeableness) * 0.15f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Values.Stimulation   = FMath::Clamp(O * 0.4f + E * 0.3f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Values.Hedonism      = FMath::Clamp(E * 0.35f + (1.0f - C) * 0.3f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Values.Achievement   = FMath::Clamp(C * 0.5f + Facets.Ambition * 0.3f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Values.Power         = FMath::Clamp((1.0f - A) * 0.4f + Facets.Ambition * 0.35f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Values.Security      = FMath::Clamp(N * 0.35f + C * 0.3f + (1.0f - O) * 0.15f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Values.Conformity    = FMath::Clamp(A * 0.3f + C * 0.3f + (1.0f - O) * 0.25f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Values.Tradition     = FMath::Clamp((1.0f - O) * 0.5f + Values.Conformity * 0.3f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Values.Benevolence   = FMath::Clamp(A * 0.6f + Facets.Empathy * 0.25f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);
    Values.Universalism  = FMath::Clamp(O * 0.35f + A * 0.35f + FMath::FRandRange(-0.15f, 0.15f), 0.0f, 1.0f);

    // --- Мораль ---
    Morals.Care      = FMath::Clamp(Facets.Empathy * 0.6f + Values.Benevolence * 0.3f, 0.0f, 1.0f);
    Morals.Fairness  = FMath::Clamp(Values.Universalism * 0.5f + Facets.Honesty * 0.35f, 0.0f, 1.0f);
    Morals.Loyalty   = FMath::Clamp(Values.Tradition * 0.35f + Values.Conformity * 0.35f + A * 0.2f, 0.0f, 1.0f);
    Morals.Authority = FMath::Clamp(Values.Conformity * 0.5f + Values.Tradition * 0.3f, 0.0f, 1.0f);
    Morals.Sanctity  = FMath::Clamp(Values.Tradition * 0.5f + (1.0f - O) * 0.25f, 0.0f, 1.0f);
    Morals.Liberty   = FMath::Clamp(Values.SelfDirection * 0.6f + (1.0f - Values.Conformity) * 0.3f, 0.0f, 1.0f);

    // --- Темперамент: врождённая точка покоя эмоций ---
    // Экстраверты живут «выше нуля» по приятности, невротики — ниже.
    TemperamentBaseline.Pleasure  = FMath::Clamp((E - 0.5f) * 0.5f - (N - 0.5f) * 0.6f, -1.0f, 1.0f);
    TemperamentBaseline.Arousal   = FMath::Clamp((E - 0.5f) * 0.4f + (N - 0.5f) * 0.35f, -1.0f, 1.0f);
    TemperamentBaseline.Dominance = FMath::Clamp((E - 0.5f) * 0.35f - (N - 0.5f) * 0.45f + (C - 0.5f) * 0.25f, -1.0f, 1.0f);

    ClampAll();
}

// ---------------------------------------------------------------------------
//  Изменение под влиянием жизни
// ---------------------------------------------------------------------------

void UPersonalityComponent::NudgeTrait(FName TraitName, float Amount, float Age)
{
    // Чем старше — тем «твёрже» характер. К 60 сдвиги почти невозможны.
    const float AgeResistance = FMath::Clamp(1.0f - (Age - 18.0f) / 55.0f, 0.12f, 1.0f);
    const float Effective = Amount * Plasticity * AgeResistance;

    if (TraitName == TEXT("Openness"))               Traits.Openness += Effective;
    else if (TraitName == TEXT("Conscientiousness")) Traits.Conscientiousness += Effective;
    else if (TraitName == TEXT("Extraversion"))      Traits.Extraversion += Effective;
    else if (TraitName == TEXT("Agreeableness"))     Traits.Agreeableness += Effective;
    else if (TraitName == TEXT("Neuroticism"))       Traits.Neuroticism += Effective;
    else if (TraitName == TEXT("Honesty"))           Facets.Honesty += Effective;
    else if (TraitName == TEXT("Empathy"))           Facets.Empathy += Effective;
    else if (TraitName == TEXT("Optimism"))          Facets.Optimism += Effective;
    else if (TraitName == TEXT("SelfControl"))       Facets.SelfControl += Effective;
    else if (TraitName == TEXT("Vengefulness"))      Facets.Vengefulness += Effective;
    else if (TraitName == TEXT("TraitAnxiety"))      Facets.TraitAnxiety += Effective;
    else if (TraitName == TEXT("Sociability"))       Facets.Sociability += Effective;
    else if (TraitName == TEXT("RiskTaking"))        Facets.RiskTaking += Effective;
    else if (TraitName == TEXT("Impulsivity"))       Facets.Impulsivity += Effective;

    ClampAll();
}

void UPersonalityComponent::ApplyMaturation(float Age)
{
    // «Принцип взросления»: с годами люди становятся собраннее, уживчивее
    // и спокойнее, но менее открытыми новому. Эффект мал, но накапливается.
    if (Age < 18.0f)
    {
        return;
    }

    const float Rate = 0.004f;
    Traits.Conscientiousness = FMath::Clamp(Traits.Conscientiousness + Rate, 0.0f, 1.0f);
    Traits.Agreeableness     = FMath::Clamp(Traits.Agreeableness + Rate * 0.8f, 0.0f, 1.0f);
    Traits.Neuroticism       = FMath::Clamp(Traits.Neuroticism - Rate * 0.7f, 0.0f, 1.0f);

    if (Age > 45.0f)
    {
        Traits.Openness     = FMath::Clamp(Traits.Openness - Rate * 0.6f, 0.0f, 1.0f);
        Traits.Extraversion = FMath::Clamp(Traits.Extraversion - Rate * 0.4f, 0.0f, 1.0f);
    }

    // Пластичность с возрастом тает — характер «схватывается».
    Plasticity = FMath::Clamp(Plasticity - 0.002f, 0.05f, 1.0f);

    ClampAll();
}

void UPersonalityComponent::ApplyTrauma(float Severity, float Age)
{
    const float S = FMath::Clamp(Severity, 0.0f, 1.0f);

    NudgeTrait(TEXT("Neuroticism"), S * 0.10f, Age);
    NudgeTrait(TEXT("TraitAnxiety"), S * 0.12f, Age);
    NudgeTrait(TEXT("Openness"), -S * 0.05f, Age);

    // Тяжёлая травма замыкает человека и подтачивает доверие к миру.
    if (S > 0.6f)
    {
        NudgeTrait(TEXT("Sociability"), -S * 0.08f, Age);
        NudgeTrait(TEXT("Optimism"), -S * 0.10f, Age);
        Values.Security = FMath::Clamp(Values.Security + S * 0.08f, 0.0f, 1.0f);
    }

    ClampAll();
}

void UPersonalityComponent::ApplyTriumph(float Magnitude, float Age)
{
    const float M = FMath::Clamp(Magnitude, 0.0f, 1.0f);

    NudgeTrait(TEXT("Extraversion"), M * 0.05f, Age);
    NudgeTrait(TEXT("Optimism"), M * 0.09f, Age);
    NudgeTrait(TEXT("Neuroticism"), -M * 0.05f, Age);
    NudgeTrait(TEXT("Conscientiousness"), M * 0.04f, Age);

    ClampAll();
}

// ---------------------------------------------------------------------------
//  Выводы из личности
// ---------------------------------------------------------------------------

ECopingStyle UPersonalityComponent::PreferredCoping(float StressLevel) const
{
    const float S = FMath::Clamp(StressLevel, 0.0f, 1.0f);

    // При запредельном стрессе зрелые стратегии отказывают — включается
    // то, что дано природой: срыв, бегство или ступор.
    if (S > 0.85f)
    {
        if (Facets.Impulsivity > 0.6f && Traits.Agreeableness < 0.45f) return ECopingStyle::Aggression;
        if (Facets.TraitAnxiety > 0.6f)                                 return ECopingStyle::Avoidance;
        return ECopingStyle::Rumination;
    }

    // Взвешиваем стратегии по чертам.
    float Scores[7];
    Scores[static_cast<int32>(ECopingStyle::ProblemFocused)] = Traits.Conscientiousness * 1.1f + (1.0f - Traits.Neuroticism) * 0.4f;
    Scores[static_cast<int32>(ECopingStyle::Reappraisal)]    = Traits.Openness * 0.8f + Facets.SelfControl * 0.7f;
    Scores[static_cast<int32>(ECopingStyle::Suppression)]    = Facets.SelfControl * 0.5f + (1.0f - Facets.Sociability) * 0.6f;
    Scores[static_cast<int32>(ECopingStyle::SeekSupport)]    = Facets.Sociability * 1.0f + Traits.Agreeableness * 0.5f;
    Scores[static_cast<int32>(ECopingStyle::Avoidance)]      = Facets.TraitAnxiety * 0.8f + (1.0f - Traits.Conscientiousness) * 0.5f;
    Scores[static_cast<int32>(ECopingStyle::Aggression)]     = (1.0f - Traits.Agreeableness) * 0.8f + Facets.Impulsivity * 0.6f;
    Scores[static_cast<int32>(ECopingStyle::Rumination)]     = Traits.Neuroticism * 0.9f + (1.0f - Facets.SelfControl) * 0.4f;

    int32 Best = 0;
    float BestScore = -1.0f;
    for (int32 i = 0; i < 7; ++i)
    {
        // Немного шума: человек не всегда предсказуем даже для себя.
        const float Noisy = Scores[i] + FMath::FRandRange(-0.25f, 0.25f);
        if (Noisy > BestScore)
        {
            BestScore = Noisy;
            Best = i;
        }
    }
    return static_cast<ECopingStyle>(Best);
}

void UPersonalityComponent::CopingLeanings(float StressLevel, float Out[7]) const
{
    const float S = FMath::Clamp(StressLevel, 0.0f, 1.0f);
    Out[static_cast<int32>(ECopingStyle::ProblemFocused)] = Traits.Conscientiousness * 1.1f + (1.0f - Traits.Neuroticism) * 0.4f;
    Out[static_cast<int32>(ECopingStyle::Reappraisal)]    = Traits.Openness * 0.8f + Facets.SelfControl * 0.7f;
    Out[static_cast<int32>(ECopingStyle::Suppression)]    = Facets.SelfControl * 0.5f + (1.0f - Facets.Sociability) * 0.6f;
    Out[static_cast<int32>(ECopingStyle::SeekSupport)]    = Facets.Sociability * 1.0f + Traits.Agreeableness * 0.5f;
    Out[static_cast<int32>(ECopingStyle::Avoidance)]      = Facets.TraitAnxiety * 0.8f + (1.0f - Traits.Conscientiousness) * 0.5f;
    Out[static_cast<int32>(ECopingStyle::Aggression)]     = (1.0f - Traits.Agreeableness) * 0.8f + Facets.Impulsivity * 0.6f;
    Out[static_cast<int32>(ECopingStyle::Rumination)]     = Traits.Neuroticism * 0.9f + (1.0f - Facets.SelfControl) * 0.4f;
    if (S > 0.85f)
    {
        const float Flood = (S - 0.85f) / 0.15f;
        Out[static_cast<int32>(ECopingStyle::Aggression)] += Flood * Facets.Impulsivity;
        Out[static_cast<int32>(ECopingStyle::Avoidance)] += Flood * Facets.TraitAnxiety;
        Out[static_cast<int32>(ECopingStyle::Rumination)] += Flood * 0.5f;
        Out[static_cast<int32>(ECopingStyle::ProblemFocused)] -= Flood * 0.6f;
        Out[static_cast<int32>(ECopingStyle::Reappraisal)] -= Flood * 0.6f;
    }
}

float UPersonalityComponent::MoralCost(float HarmToOthers, float Unfairness, float Betrayal) const
{
    const float H = FMath::Clamp(HarmToOthers, 0.0f, 1.0f);
    const float U = FMath::Clamp(Unfairness, 0.0f, 1.0f);
    const float B = FMath::Clamp(Betrayal, 0.0f, 1.0f);

    const float Cost =
        H * Morals.Care * 1.0f +
        U * Morals.Fairness * 0.9f +
        B * Morals.Loyalty * 0.8f;

    // Нормируем: максимум суммы весов ~2.7.
    return FMath::Clamp(Cost / 2.7f, 0.0f, 1.0f);
}

float UPersonalityComponent::RiskAppetite(float Reward, float SuccessChance) const
{
    const float P = FMath::Clamp(SuccessChance, 0.0f, 1.0f);

    // Люди систематически переоценивают маленькие шансы и недооценивают большие.
    // Чем выше склонность к риску, тем сильнее искажение в сторону «да повезёт».
    const float Distorted = FMath::Lerp(P, FMath::Pow(P, 0.65f), Facets.RiskTaking);

    // Потери ощущаются примерно вдвое острее выигрыша (неприятие потерь),
    // но у смелых коэффициент мягче.
    const float LossAversion = FMath::Lerp(2.4f, 1.2f, Facets.RiskTaking);

    const float Expected = Distorted * Reward - (1.0f - Distorted) * LossAversion * Reward * 0.5f;
    return FMath::Clamp(Expected, -1.0f, 1.0f);
}

float UPersonalityComponent::BaseWillpower() const
{
    return FMath::Clamp(Facets.SelfControl * 0.6f + Traits.Conscientiousness * 0.4f, 0.05f, 1.0f);
}

float UPersonalityComponent::Patience() const
{
    return FMath::Clamp(Facets.SelfControl * 0.5f + Traits.Conscientiousness * 0.35f + (1.0f - Facets.Impulsivity) * 0.25f, 0.0f, 1.0f);
}

float UPersonalityComponent::SocialAppetite() const
{
    return FMath::Clamp(Facets.Sociability * 0.7f + Traits.Extraversion * 0.3f, 0.05f, 1.0f);
}

// ---------------------------------------------------------------------------
//  Самоописание
// ---------------------------------------------------------------------------

FString UPersonalityComponent::DominantTraitWord() const
{
    // Ищем самое выделяющееся отклонение от середины.
    struct FCandidate { float Deviation; const TCHAR* Word; };

    TArray<FCandidate> Candidates;
    Candidates.Add({ Traits.Neuroticism - 0.5f,       TEXT("тревожный") });
    Candidates.Add({ 0.5f - Traits.Neuroticism,       TEXT("невозмутимый") });
    Candidates.Add({ Traits.Extraversion - 0.5f,      TEXT("общительный") });
    Candidates.Add({ 0.5f - Traits.Extraversion,      TEXT("замкнутый") });
    Candidates.Add({ Traits.Openness - 0.5f,          TEXT("любознательный") });
    Candidates.Add({ 0.5f - Traits.Openness,          TEXT("консервативный") });
    Candidates.Add({ Traits.Conscientiousness - 0.5f, TEXT("обязательный") });
    Candidates.Add({ 0.5f - Traits.Conscientiousness, TEXT("разгильдяй") });
    Candidates.Add({ Traits.Agreeableness - 0.5f,     TEXT("добродушный") });
    Candidates.Add({ 0.5f - Traits.Agreeableness,     TEXT("колючий") });
    Candidates.Add({ Facets.Impulsivity - 0.65f,      TEXT("вспыльчивый") });
    Candidates.Add({ Facets.Ambition - 0.7f,          TEXT("честолюбивый") });

    float BestDev = -1.0f;
    const TCHAR* BestWord = TEXT("обычный");
    for (const FCandidate& C : Candidates)
    {
        if (C.Deviation > BestDev)
        {
            BestDev = C.Deviation;
            BestWord = C.Word;
        }
    }
    return FString(BestWord);
}

FString UPersonalityComponent::DescribeSelf() const
{
    FString Result = DominantTraitWord();

    // Добавим вторую грань — то, что человек сам о себе сказал бы.
    if (Facets.Empathy > 0.7f)         Result += TEXT(", жалостливый");
    else if (Facets.Empathy < 0.3f)    Result += TEXT(", чёрствый");

    if (Facets.Honesty > 0.75f)        Result += TEXT(", не умеет врать");
    else if (Facets.Honesty < 0.3f)    Result += TEXT(", соврёт и не моргнёт");

    if (Facets.Ambition > 0.75f)       Result += TEXT(", рвётся вперёд");
    else if (Facets.Ambition < 0.25f)  Result += TEXT(", ничего особо не хочет");

    return Result;
}

// ---------------------------------------------------------------------------

void UPersonalityComponent::ClampAll()
{
    Traits.Openness          = FMath::Clamp(Traits.Openness, 0.0f, 1.0f);
    Traits.Conscientiousness = FMath::Clamp(Traits.Conscientiousness, 0.0f, 1.0f);
    Traits.Extraversion      = FMath::Clamp(Traits.Extraversion, 0.0f, 1.0f);
    Traits.Agreeableness     = FMath::Clamp(Traits.Agreeableness, 0.0f, 1.0f);
    Traits.Neuroticism       = FMath::Clamp(Traits.Neuroticism, 0.0f, 1.0f);

    Facets.Impulsivity  = FMath::Clamp(Facets.Impulsivity, 0.0f, 1.0f);
    Facets.RiskTaking   = FMath::Clamp(Facets.RiskTaking, 0.0f, 1.0f);
    Facets.Vengefulness = FMath::Clamp(Facets.Vengefulness, 0.0f, 1.0f);
    Facets.Honesty      = FMath::Clamp(Facets.Honesty, 0.0f, 1.0f);
    Facets.Empathy      = FMath::Clamp(Facets.Empathy, 0.0f, 1.0f);
    Facets.Stubbornness = FMath::Clamp(Facets.Stubbornness, 0.0f, 1.0f);
    Facets.Optimism     = FMath::Clamp(Facets.Optimism, 0.0f, 1.0f);
    Facets.Curiosity    = FMath::Clamp(Facets.Curiosity, 0.0f, 1.0f);
    Facets.SelfControl  = FMath::Clamp(Facets.SelfControl, 0.0f, 1.0f);
    Facets.TraitAnxiety = FMath::Clamp(Facets.TraitAnxiety, 0.0f, 1.0f);
    Facets.Sociability  = FMath::Clamp(Facets.Sociability, 0.0f, 1.0f);
    Facets.Ambition     = FMath::Clamp(Facets.Ambition, 0.0f, 1.0f);
}
