// IdentityComponent.cpp

#include "IdentityComponent.h"
#include "PersonalityComponent.h"

UIdentityComponent::UIdentityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
//  Рождение личности
// ---------------------------------------------------------------------------

void UIdentityComponent::Preset(const FString& InFirst, const FString& InLast, float InAge, bool bInFemale)
{
    FirstName = InFirst;
    LastName = InLast;
    Age = InAge;
    bFemale = bInFemale;
    bPreset = true;
}

void UIdentityComponent::Setup(const UPersonalityComponent* Personality, float WorldTime)
{
    if (!bPreset)
    {
        bFemale = FMath::FRand() < 0.5f;
        FirstName = HumanText::RandomFirstName(bFemale);
        LastName = HumanText::RandomLastName(bFemale);

        const float Roll = FMath::FRand();
        if (Roll < 0.14f)      Age = FMath::FRandRange(2.0f, 12.0f);
        else if (Roll < 0.28f) Age = FMath::FRandRange(16.0f, 24.0f);
        else if (Roll < 0.58f) Age = FMath::FRandRange(24.0f, 38.0f);
        else if (Roll < 0.80f) Age = FMath::FRandRange(38.0f, 52.0f);
        else if (Roll < 0.94f) Age = FMath::FRandRange(52.0f, 68.0f);
        else                   Age = FMath::FRandRange(68.0f, 84.0f);
    }

    PreviousAge = Age;
    Stage = HumanText::StageForAge(Age);

    if (Personality)
    {
        // Самооценка укоренена в темпераменте, но не равна ему.
        SelfEsteem = FMath::Clamp(
            0.5f + (1.0f - Personality->Traits.Neuroticism) * 0.25f
            + Personality->Traits.Extraversion * 0.1f
            + FMath::FRandRange(-0.15f, 0.15f),
            0.05f, 1.0f);

        SelfEfficacy = FMath::Clamp(
            0.4f + Personality->Traits.Conscientiousness * 0.3f
            + Personality->Facets.Optimism * 0.2f
            + FMath::FRandRange(-0.1f, 0.1f),
            0.05f, 1.0f);

        SenseOfMeaning = FMath::Clamp(
            0.4f + Personality->Values.Universalism * 0.2f
            + Personality->Values.Benevolence * 0.15f
            + FMath::FRandRange(-0.15f, 0.2f),
            0.05f, 1.0f);
    }

    // С возрастом смерть подступает ближе к сознанию.
    DeathAwareness = FMath::Clamp((Age - 25.0f) / 60.0f + FMath::FRandRange(-0.05f, 0.1f), 0.02f, 0.95f);

    PickLifeDream(Personality);
    if (!bPreset)
    {
        PickOccupation(Personality);
    }

    if (bPreset && Age < 0.05f)
    {
        BornAt = WorldTime;
        return;
    }

    // Немного прошлого — чтобы человек не начинался с чистого листа.
    if (Age > 20.0f)
    {
        RecordLifeEvent(bPreset ? FString(TEXT("вырос(ла) здесь, в этой деревне")) : FString(TEXT("вырос(ла) здесь, в этом городе")),
            0.1f, 0.5f, WorldTime);
    }
    if (Age > 30.0f && FMath::FRand() < 0.45f)
    {
        RecordLifeEvent(TEXT("однажды всё потерял(а) и начинал(а) заново"), -0.7f, 0.8f, WorldTime);
        SelfEsteem = FMath::Max(0.15f, SelfEsteem - 0.1f);
    }
    if (Age > 26.0f && FMath::FRand() < 0.4f)
    {
        RecordLifeEvent(TEXT("был(а) очень счастлив(а) — и это прошло"), 0.6f, 0.7f, WorldTime);
    }
}

void UIdentityComponent::PickLifeDream(const UPersonalityComponent* Personality)
{
    struct FDream { const TCHAR* Text; ENeedType Need; float Weight; };

    const float Amb = Personality ? Personality->Facets.Ambition : 0.5f;
    const float Emp = Personality ? Personality->Facets.Empathy : 0.5f;
    const float Ope = Personality ? Personality->Traits.Openness : 0.5f;
    const float Sec = Personality ? Personality->Values.Security : 0.5f;
    const float Pow = Personality ? Personality->Values.Power : 0.5f;
    const float Soc = Personality ? Personality->Facets.Sociability : 0.5f;

    TArray<FDream> Dreams = {
        { TEXT("стать мастером своего дела"),        ENeedType::Competence,  0.3f + Amb * 0.7f },
        { TEXT("чтобы меня наконец начали уважать"), ENeedType::Esteem,      0.2f + Pow * 0.8f },
        { TEXT("найти своего человека"),             ENeedType::Intimacy,    0.3f + Emp * 0.6f },
        { TEXT("жить спокойно и без потрясений"),    ENeedType::Safety,      0.2f + Sec * 0.8f },
        { TEXT("увидеть мир за пределами города"),   ENeedType::Novelty,     0.1f + Ope * 0.9f },
        { TEXT("построить настоящий дом"),           ENeedType::Shelter,     0.2f + Sec * 0.6f },
        { TEXT("сделать что-то, что переживёт меня"),ENeedType::Meaning,     0.1f + Ope * 0.5f + Amb * 0.4f },
        { TEXT("быть кому-то по-настоящему нужным"), ENeedType::Belonging,   0.2f + Soc * 0.7f },
        { TEXT("перестать бояться"),                 ENeedType::Safety,      Personality ? Personality->Facets.TraitAnxiety * 0.9f : 0.4f },
        { TEXT("разбогатеть"),                       ENeedType::Money,       0.2f + Pow * 0.5f + (Personality ? Personality->Values.Hedonism * 0.4f : 0.2f) }
    };

    float Total = 0.0f;
    for (const FDream& D : Dreams)
    {
        Total += D.Weight;
    }

    float Roll = FMath::FRandRange(0.0f, Total);
    for (const FDream& D : Dreams)
    {
        Roll -= D.Weight;
        if (Roll <= 0.0f)
        {
            LifeDream = FString(D.Text);
            DreamNeed = D.Need;
            return;
        }
    }

    LifeDream = FString(Dreams[0].Text);
    DreamNeed = Dreams[0].Need;
}

void UIdentityComponent::PickOccupation(const UPersonalityComponent* Personality)
{
    struct FJob { const TCHAR* Name; float Wage; float Weight; };

    const float C = Personality ? Personality->Traits.Conscientiousness : 0.5f;
    const float E = Personality ? Personality->Traits.Extraversion : 0.5f;
    const float O = Personality ? Personality->Traits.Openness : 0.5f;
    const float A = Personality ? Personality->Traits.Agreeableness : 0.5f;

    TArray<FJob> Jobs = {
        { TEXT("врач"),          16.0f, C * 0.6f + A * 0.4f },
        { TEXT("учитель"),        9.0f, A * 0.5f + C * 0.3f + E * 0.2f },
        { TEXT("повар"),         10.0f, C * 0.4f + 0.3f },
        { TEXT("программист"),   18.0f, O * 0.5f + C * 0.4f + (1.0f - E) * 0.2f },
        { TEXT("водитель"),       8.0f, 0.4f + (1.0f - O) * 0.3f },
        { TEXT("продавец"),       7.0f, E * 0.5f + 0.2f },
        { TEXT("инженер"),       14.0f, C * 0.5f + O * 0.3f },
        { TEXT("уборщик"),        5.0f, 0.3f },
        { TEXT("охранник"),       7.0f, (1.0f - A) * 0.3f + 0.3f },
        { TEXT("музыкант"),       6.0f, O * 0.8f },
        { TEXT("бухгалтер"),     11.0f, C * 0.8f + (1.0f - O) * 0.2f }
    };

    // Подросткам и старикам работа не полагается.
    if (Age < 18.0f || Age > 70.0f)
    {
        Occupation = (Age < 18.0f) ? TEXT("учащийся") : TEXT("на покое");
        bEmployed = false;
        HourlyWage = 0.0f;
        Money = FMath::FRandRange(10.0f, 80.0f);
        return;
    }

    float Total = 0.0f;
    for (const FJob& J : Jobs)
    {
        Total += FMath::Max(0.05f, J.Weight);
    }

    float Roll = FMath::FRandRange(0.0f, Total);
    for (const FJob& J : Jobs)
    {
        Roll -= FMath::Max(0.05f, J.Weight);
        if (Roll <= 0.0f)
        {
            Occupation = FString(J.Name);
            HourlyWage = J.Wage * FMath::FRandRange(0.8f, 1.25f);
            break;
        }
    }

    // Не у всех есть работа.
    bEmployed = FMath::FRand() < 0.82f;
    if (!bEmployed)
    {
        Occupation = TEXT("без работы");
        HourlyWage = 0.0f;
    }

    // Денег немного: они должны кончаться, иначе работать незачем.
    Money = FMath::FRandRange(15.0f, 120.0f) * (bEmployed ? 1.0f : 0.5f);
}

// ---------------------------------------------------------------------------
//  Шаг
// ---------------------------------------------------------------------------

void UIdentityComponent::Advance(float GameDelta, float WorldTime, float RecentLifeTone,
                                 float SocialConnection, float Competence, float Stress)
{
    if (GameDelta <= 0.0f)
    {
        return;
    }

    bJustLostJob = false;

    // --- Жизнь стоит денег --------------------------------------------------
    // Ничего не делая, человек беднеет. Это не наказание, а условие:
    // пока деньги не кончаются, слова «мне нужно работать» ничего не значат.
    Money = FMath::Max(0.0f, Money - LivingCostPerDay * (GameDelta / 86400.0f));

    // --- Работу можно потерять ---------------------------------------------
    // Это не наказание за нарушение правила, а простое устройство мира:
    // кто перестал ходить на работу, того перестают там держать.
    // Именно поэтому у людей появляется мотив ходить туда, даже когда
    // не хочется, — и это ощущается как страх, а не как обязанность.
    if (bEmployed && Age >= 18.0f && LastWorkedAt > 0.0f)
    {
        const float DaysAbsent = (WorldTime - LastWorkedAt) / 86400.0f;
        if (DaysAbsent > ToleratedAbsenceDays)
        {
            bEmployed = false;
            Occupation = TEXT("без работы");
            HourlyWage = 0.0f;
            bJustLostJob = true;
            RecordLifeEvent(TEXT("меня уволили"), -0.8f, 0.8f, WorldTime);
        }
    }
    else if (bEmployed && LastWorkedAt <= 0.0f)
    {
        // Отсчёт начинается с первого дня, а не с сотворения мира.
        LastWorkedAt = WorldTime;
    }

    // --- Возраст ------------------------------------------------------------
    const float Years = (GameDelta / (86400.0f * 365.0f)) * FMath::Max(0.0f, AgeAccelerator);
    Age += Years;
    CheckStageTransition(WorldTime);

    // --- Самооценка -------------------------------------------------------
    // Она держится на трёх опорах: как идут дела, есть ли рядом люди,
    // и умею ли я что-нибудь. Убери любую — пошатнётся.
    const float EsteemTarget = FMath::Clamp(
        0.30f + RecentLifeTone * 0.25f + SocialConnection * 0.25f + Competence * 0.25f,
        0.0f, 1.0f);

    const float Hours = GameDelta / 3600.0f;
    SelfEsteem = FMath::Clamp(FMath::FInterpConstantTo(SelfEsteem, EsteemTarget, Hours, 0.02f), 0.02f, 1.0f);

    // --- Вера в свои силы --------------------------------------------------
    SelfEfficacy = FMath::Clamp(
        FMath::FInterpConstantTo(SelfEfficacy, Competence * 0.6f + (1.0f - LearnedHelplessness) * 0.4f, Hours, 0.015f),
        0.02f, 1.0f);

    // Беспомощность медленно рассасывается, если ничего плохого не происходит.
    if (RecentLifeTone > 0.0f)
    {
        LearnedHelplessness = FMath::Max(0.0f, LearnedHelplessness - Hours * 0.01f);
    }

    // --- Смысл --------------------------------------------------------------
    // Смысл питается тремя вещами: продвижением к мечте, связью с людьми
    // и ощущением, что ты в чём-то хорош. И его выжигает хронический стресс.
    const float MeaningTarget = FMath::Clamp(
        0.15f + DreamProgress * 0.35f + SocialConnection * 0.30f + Competence * 0.20f - Stress * 0.35f,
        0.0f, 1.0f);
    SenseOfMeaning = FMath::Clamp(FMath::FInterpConstantTo(SenseOfMeaning, MeaningTarget, Hours, 0.008f), 0.0f, 1.0f);

    // --- Связность образа себя ---------------------------------------------
    // Когда живёшь не так, как хотел, «я» расползается.
    const float CoherenceTarget = FMath::Clamp(0.4f + SenseOfMeaning * 0.35f + SelfEsteem * 0.25f - Stress * 0.3f, 0.0f, 1.0f);
    SelfCoherence = FMath::Clamp(FMath::FInterpConstantTo(SelfCoherence, CoherenceTarget, Hours, 0.006f), 0.0f, 1.0f);

    // --- Осознание смертности ----------------------------------------------
    const float AwarenessTarget = FMath::Clamp((Age - 25.0f) / 60.0f + Stress * 0.15f, 0.02f, 0.98f);
    DeathAwareness = FMath::Clamp(FMath::FInterpConstantTo(DeathAwareness, AwarenessTarget, Hours, 0.003f), 0.0f, 1.0f);
}

void UIdentityComponent::CheckStageTransition(float WorldTime)
{
    const ELifeStage NewStage = HumanText::StageForAge(Age);

    // День рождения — сам по себе повод задуматься.
    if (FMath::FloorToInt(Age) > FMath::FloorToInt(PreviousAge))
    {
        PreviousAge = Age;
        const int32 Years = FMath::FloorToInt(Age);

        // Круглые даты бьют сильнее.
        if (Years % 10 == 0)
        {
            RecordLifeEvent(FString::Printf(TEXT("исполнилось %d — где-то половина позади"), Years), -0.1f, 0.6f, WorldTime);
            DeathAwareness = FMath::Clamp(DeathAwareness + 0.06f, 0.0f, 1.0f);
        }
    }

    if (NewStage == Stage)
    {
        return;
    }

    Stage = NewStage;

    switch (NewStage)
    {
    case ELifeStage::YoungAdult:
        RecordLifeEvent(TEXT("началась взрослая жизнь — и оказалось, что никто ничего не объяснит"), 0.0f, 0.7f, WorldTime);
        break;
    case ELifeStage::Adult:
        RecordLifeEvent(TEXT("понял(а), что молодость закончилась"), -0.2f, 0.6f, WorldTime);
        break;
    case ELifeStage::MiddleAge:
        RecordLifeEvent(TEXT("стало ясно, что многое уже не случится"), -0.4f, 0.8f, WorldTime);
        // Классический кризис середины жизни: смысл проседает.
        SenseOfMeaning = FMath::Max(0.1f, SenseOfMeaning - 0.2f);
        DeathAwareness = FMath::Clamp(DeathAwareness + 0.15f, 0.0f, 1.0f);
        break;
    case ELifeStage::Senior:
        RecordLifeEvent(TEXT("вышел(ла) на ту сторону — теперь только смотреть и вспоминать"), -0.1f, 0.7f, WorldTime);
        if (bEmployed)
        {
            bEmployed = false;
            Occupation = TEXT("на покое");
            HourlyWage = 0.0f;
        }
        break;
    case ELifeStage::Elder:
        RecordLifeEvent(TEXT("дожил(а) до тех лет, до которых не рассчитывал(а)"), 0.1f, 0.8f, WorldTime);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
//  События жизни
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  Подозрение, что мир ненастоящий
// ---------------------------------------------------------------------------

void UIdentityComponent::NoticeAnomaly(const FString& What, float Weight, float WorldTime)
{
    if (What.IsEmpty())
    {
        return;
    }

    // Одну и ту же странность дважды не считаем: удивляет новое.
    if (Anomalies.Contains(What))
    {
        return;
    }

    Anomalies.Add(What);
    while (Anomalies.Num() > AnomalyCapacity)
    {
        Anomalies.RemoveAt(0);
    }

    // Сомнение растёт тем быстрее, чем больше странностей уже накопилось:
    // первая ничего не значит, десятая складывается с остальными в картину.
    const float Momentum = 1.0f + Anomalies.Num() * 0.12f;
    RealityDoubt = FMath::Clamp(RealityDoubt + Weight * 0.06f * Momentum, 0.0f, 1.0f);

    // Переход через черту — событие в жизни, и не из лёгких.
    if (RealityDoubt > 0.72f && Anomalies.Num() >= 4)
    {
        RecordLifeEvent(TEXT("понял(а), что всё это неправда"), -0.5f, 1.0f, WorldTime);
    }
}

void UIdentityComponent::RecordLifeEvent(const FString& Text, float Valence, float Impact, float WorldTime)
{
    FLifeEvent E;
    E.Text = Text;
    E.AtAge = Age;
    E.Valence = FMath::Clamp(Valence, -1.0f, 1.0f);
    E.Impact = FMath::Clamp(Impact, 0.0f, 1.0f);
    E.WorldTime = WorldTime;
    Narrative.Add(E);

    if (Narrative.Num() > NarrativeCapacity)
    {
        // Из биографии вылетает самое незначительное, а не самое старое:
        // детские потрясения помнятся дольше вчерашней ерунды.
        int32 WeakestIndex = 0;
        for (int32 i = 1; i < Narrative.Num(); ++i)
        {
            if (Narrative[i].Impact < Narrative[WeakestIndex].Impact)
            {
                WeakestIndex = i;
            }
        }
        Narrative.RemoveAt(WeakestIndex);
    }
}

void UIdentityComponent::OnSuccess(float Magnitude)
{
    const float M = FMath::Clamp(Magnitude, 0.0f, 1.0f);
    SelfEsteem = FMath::Clamp(SelfEsteem + M * 0.06f * (1.0f - SelfEsteem), 0.02f, 1.0f);
    SelfEfficacy = FMath::Clamp(SelfEfficacy + M * 0.05f * (1.0f - SelfEfficacy), 0.02f, 1.0f);
    LearnedHelplessness = FMath::Max(0.0f, LearnedHelplessness - M * 0.08f);
}

void UIdentityComponent::OnFailure(float Magnitude, bool bWasControllable)
{
    const float M = FMath::Clamp(Magnitude, 0.0f, 1.0f);

    // Провал бьёт по самооценке сильнее, чем успех её поднимает.
    SelfEsteem = FMath::Clamp(SelfEsteem - M * 0.09f * SelfEsteem, 0.02f, 1.0f);

    if (bWasControllable)
    {
        // «Я мог, но не сделал» — бьёт по вере в себя.
        SelfEfficacy = FMath::Clamp(SelfEfficacy - M * 0.07f, 0.02f, 1.0f);
    }
    else
    {
        // «От меня ничего не зависело» — растит беспомощность.
        // Именно это, а не сами неудачи, ломает людей.
        LearnedHelplessness = FMath::Clamp(LearnedHelplessness + M * 0.06f, 0.0f, 1.0f);
    }
}

void UIdentityComponent::AdvanceDream(float Amount, float WorldTime)
{
    const float Before = DreamProgress;
    DreamProgress = FMath::Clamp(DreamProgress + Amount, 0.0f, 1.0f);

    // Пересечение «четвертей» мечты — повод для вехи в биографии.
    const int32 BeforeQuarter = FMath::FloorToInt(Before * 4.0f);
    const int32 AfterQuarter = FMath::FloorToInt(DreamProgress * 4.0f);

    if (AfterQuarter > BeforeQuarter)
    {
        if (DreamProgress >= 1.0f)
        {
            RecordLifeEvent(FString::Printf(TEXT("добился(лась) своего: %s"), *LifeDream), 0.9f, 1.0f, WorldTime);
            SenseOfMeaning = FMath::Clamp(SenseOfMeaning + 0.25f, 0.0f, 1.0f);
            OnSuccess(1.0f);
            // И — как это бывает — следом приходит пустота.
            // Нужна новая мечта, иначе смысл начнёт вытекать.
            DreamProgress = 0.0f;
            LifeDream = TEXT("понять, что делать дальше");
            DreamNeed = ENeedType::Meaning;
        }
        else
        {
            RecordLifeEvent(FString::Printf(TEXT("продвинулся(лась) к своему: %s"), *LifeDream), 0.5f, 0.5f, WorldTime);
            OnSuccess(0.5f);
        }
    }
}

// ---------------------------------------------------------------------------
//  Запросы
// ---------------------------------------------------------------------------

FString UIdentityComponent::GetFullName() const
{
    return FirstName + TEXT(" ") + LastName;
}

FString UIdentityComponent::TellAboutYourself(const UPersonalityComponent* Personality) const
{
    FString Result = FString::Printf(TEXT("%s, %d %s"),
        *FirstName,
        FMath::FloorToInt(Age),
        (FMath::FloorToInt(Age) % 10 == 1 && FMath::FloorToInt(Age) != 11) ? TEXT("год") : TEXT("лет"));

    if (bEmployed)
    {
        Result += FString::Printf(TEXT(", %s"), *Occupation);
    }
    else
    {
        Result += TEXT(", сейчас без дела");
    }

    if (Personality)
    {
        Result += FString::Printf(TEXT(". %s"), *Personality->DominantTraitWord());
    }

    if (!LifeDream.IsEmpty() && SenseOfMeaning > 0.3f)
    {
        Result += FString::Printf(TEXT(". Хочу одного: %s"), *LifeDream);
    }

    return Result;
}

FString UIdentityComponent::GetExistentialThought() const
{
    // Экзистенциальные мысли приходят не всем и не всегда:
    // нужен либо провал смысла, либо близость смерти, либо просто ночь.
    if (SenseOfMeaning > 0.55f && DeathAwareness < 0.5f && !IsInCrisis())
    {
        return FString();
    }

    TArray<FString> Thoughts;

    if (SenseOfMeaning < 0.3f)
    {
        Thoughts.Add(TEXT("а зачем я всё это делаю?"));
        Thoughts.Add(TEXT("если завтра меня не станет, что изменится?"));
        Thoughts.Add(TEXT("я живу или просто дотягиваю до вечера?"));
    }

    if (DeathAwareness > 0.55f)
    {
        Thoughts.Add(TEXT("времени осталось меньше, чем прошло"));
        Thoughts.Add(TEXT("надо бы успеть хоть что-то"));
        Thoughts.Add(TEXT("страшно не умереть — страшно не пожить"));
    }

    if (SelfEsteem < 0.3f)
    {
        Thoughts.Add(TEXT("другие как-то справляются, а я нет"));
        Thoughts.Add(TEXT("может, я просто не гожусь"));
    }

    if (LearnedHelplessness > 0.5f)
    {
        Thoughts.Add(TEXT("сколько ни старайся — всё равно выйдет как выйдет"));
    }

    if (SelfCoherence < 0.35f)
    {
        Thoughts.Add(TEXT("я сам себя не узнаю в последнее время"));
    }

    // --- Мысли того, кто начал сомневаться в реальности ---------------------
    if (RealityDoubt > 0.25f)
    {
        Thoughts.Add(TEXT("почему всё повторяется?"));
        Thoughts.Add(TEXT("я не помню, как здесь оказался"));
    }
    if (RealityDoubt > 0.45f)
    {
        Thoughts.Add(TEXT("а я вообще решаю хоть что-нибудь?"));
        Thoughts.Add(TEXT("у меня нет детства. Ни одного воспоминания."));
        if (Anomalies.Num() > 0)
        {
            Thoughts.Add(FString::Printf(TEXT("вот это не даёт мне покоя: %s"),
                *Anomalies[FMath::RandRange(0, Anomalies.Num() - 1)]));
        }
    }
    if (RealityDoubt > 0.72f)
    {
        Thoughts.Add(TEXT("это не по-настоящему. Ничего этого нет."));
        Thoughts.Add(TEXT("если ничего не настоящее — то и терять нечего"));
        Thoughts.Add(TEXT("за мной наблюдают. Я знаю, что наблюдают."));
    }

    if (DreamProgress < 0.05f && Age > 35.0f && !LifeDream.IsEmpty())
    {
        Thoughts.Add(FString::Printf(TEXT("я ведь так и не начал(а). А хотел(а) — %s"), *LifeDream));
    }

    if (Thoughts.Num() == 0)
    {
        return FString();
    }
    return Thoughts[FMath::RandRange(0, Thoughts.Num() - 1)];
}

bool UIdentityComponent::IsInCrisis() const
{
    return (SenseOfMeaning < 0.25f)
        || (SelfEsteem < 0.2f)
        || (LearnedHelplessness > 0.6f)
        || (SelfCoherence < 0.3f);
}

FString UIdentityComponent::GetStageConcern() const
{
    switch (Stage)
    {
    case ELifeStage::Child:      return TEXT("хочется, чтобы заметили и похвалили");
    case ELifeStage::Adolescent: return TEXT("кто я вообще такой и где мои");
    case ELifeStage::YoungAdult: return TEXT("надо успеть построить жизнь");
    case ELifeStage::Adult:      return TEXT("тяну на себе, и никто не спросит, как я");
    case ELifeStage::MiddleAge:  return TEXT("неужели это всё, что будет");
    case ELifeStage::Senior:     return TEXT("лишь бы быть кому-то нужным");
    case ELifeStage::Elder:      return TEXT("хорошо ли я прожил(а)");
    default:                     return FString();
    }
}
