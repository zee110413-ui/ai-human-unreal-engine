// SocialComponent.cpp

#include "SocialComponent.h"
#include "PersonalityComponent.h"

USocialComponent::USocialComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
//  Доступ
// ---------------------------------------------------------------------------

FRelationship& USocialComponent::FindOrAdd(AActor* Other, float WorldTime)
{
    if (FRelationship* Existing = Relations.Find(Other))
    {
        return *Existing;
    }

    FRelationship New;
    New.Other = Other;
    New.Kind = ERelationKind::Stranger;
    New.FirstMetAt = WorldTime;
    New.LastInteractionAt = WorldTime;
    // К незнакомцу — настороженный нейтралитет.
    New.Trust = 0.25f;
    New.Respect = 0.3f;
    New.Liking = 0.0f;

    Relations.Add(Other, New);
    EnforceCapacity();
    return Relations[Other];
}

FRelationship* USocialComponent::Find(AActor* Other)
{
    return Relations.Find(Other);
}

const FRelationship* USocialComponent::Find(AActor* Other) const
{
    return Relations.Find(Other);
}

void USocialComponent::EnforceCapacity()
{
    if (Relations.Num() <= RelationCapacity)
    {
        return;
    }

    // Из головы вылетают те, кто не важен: мало знаком, никак не относишься.
    AActor* Weakest = nullptr;
    float WeakestScore = FLT_MAX;

    for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Relations)
    {
        const FRelationship& R = Pair.Value;
        const float Score = R.Familiarity + FMath::Abs(R.Liking) + R.Attachment + R.Resentment;
        if (Score < WeakestScore)
        {
            WeakestScore = Score;
            Weakest = Pair.Key;
        }
    }

    if (Weakest)
    {
        Relations.Remove(Weakest);
    }
}

// ---------------------------------------------------------------------------
//  Течение времени в отношениях
// ---------------------------------------------------------------------------

void USocialComponent::Advance(float GameDelta, float WorldTime)
{
    DecayAccumulator += GameDelta;
    if (DecayAccumulator < 1800.0f)
    {
        return;
    }
    const float Hours = DecayAccumulator / 3600.0f;
    DecayAccumulator = 0.0f;

    TArray<AActor*> Dead;

    for (TPair<TObjectPtr<AActor>, FRelationship>& Pair : Relations)
    {
        if (!Pair.Key)
        {
            continue;
        }

        FRelationship& R = Pair.Value;
        const float DaysSince = (WorldTime - R.LastInteractionAt) / 86400.0f;

        if (DaysSince > 1.0f)
        {
            // «С глаз долой»: знакомство и привязанность тают без встреч.
            // Но привязанность держится дольше знакомства — тем и отличается.
            R.Familiarity = FMath::Max(0.0f, R.Familiarity - Hours * 0.0025f);
            R.Attachment = FMath::Max(0.0f, R.Attachment - Hours * 0.0008f);

            // Симпатия и неприязнь сползают к нулю — время лечит и то, и то.
            R.Liking = FMath::FInterpConstantTo(R.Liking, 0.0f, Hours, 0.0015f);
        }

        // Обида тает медленнее всего и зависит от злопамятности.
        // Именно поэтому старые обиды переживают дружбу.
        R.Resentment = FMath::Max(0.0f, R.Resentment - Hours * 0.0012f);

        // Страх перед человеком тоже уходит, если он ничего не делает.
        R.Fear = FMath::Max(0.0f, R.Fear - Hours * 0.004f);

        // Долг не забывается, но со временем «прощается».
        R.Debt = FMath::FInterpConstantTo(R.Debt, 0.0f, Hours, 0.0008f);

        // Модель чужого сознания устаревает: чем дольше не виделись,
        // тем хуже я представляю, что с ним сейчас.
        if (DaysSince > 0.5f)
        {
            R.Theory.ModelAccuracy = FMath::Max(0.05f, R.Theory.ModelAccuracy - Hours * 0.01f);
        }

        R.Kind = ClassifyRelation(R);
    }

    // Репутация тоже возвращается к нулю: людская память коротка.
    Reputation = FMath::FInterpConstantTo(Reputation, 0.0f, Hours, 0.002f);

    // Подчищаем исчезнувших.
    for (auto It = Relations.CreateIterator(); It; ++It)
    {
        if (!It.Key())
        {
            It.RemoveCurrent();
        }
    }
}

// ---------------------------------------------------------------------------
//  События между людьми
// ---------------------------------------------------------------------------

void USocialComponent::RecordInteraction(AActor* Other, float Quality, float WorldTime, const UPersonalityComponent* Personality)
{
    if (!Other)
    {
        return;
    }

    FRelationship& R = FindOrAdd(Other, WorldTime);
    const float Q = FMath::Clamp(Quality, -1.0f, 1.0f);

    R.InteractionCount++;
    R.LastInteractionAt = WorldTime;

    // Знакомство растёт от любого контакта — даже от ссоры.
    // Насыщение: первые встречи дают много, сотая — почти ничего.
    R.Familiarity = FMath::Clamp(R.Familiarity + 0.06f * (1.0f - R.Familiarity), 0.0f, 1.0f);

    // Симпатия. Доброжелательные легче проникаются, злопамятные — медленнее.
    const float Warmth = Personality ? Personality->Traits.Agreeableness : 0.5f;
    const float LikingDelta = Q * (Q > 0.0f ? (0.05f + Warmth * 0.05f) : (0.07f + (1.0f - Warmth) * 0.06f));
    R.Liking = FMath::Clamp(R.Liking + LikingDelta, -1.0f, 1.0f);

    // Доверие растёт медленно и падает быстро — асимметрия доверия.
    if (Q > 0.0f)
    {
        R.Trust = FMath::Clamp(R.Trust + Q * 0.025f * (1.0f - R.Trust), 0.0f, 1.0f);
    }
    else
    {
        R.Trust = FMath::Clamp(R.Trust + Q * 0.08f, 0.0f, 1.0f);
    }

    // Привязанность — от количества хорошего времени вместе, не от одного случая.
    if (Q > 0.2f)
    {
        R.Attachment = FMath::Clamp(R.Attachment + 0.012f * (1.0f - R.Attachment), 0.0f, 1.0f);
    }

    // Хорошее общение немного смывает старую обиду.
    if (Q > 0.4f)
    {
        R.Resentment = FMath::Max(0.0f, R.Resentment - 0.03f);
    }

    // Романтическое: тлеет само, если есть симпатия, близость и взаимность.
    if (R.Liking > 0.45f && R.Familiarity > 0.3f && R.Attachment > 0.25f)
    {
        const float Openness = Personality ? Personality->Traits.Openness : 0.5f;
        R.Romantic = FMath::Clamp(R.Romantic + 0.01f * Openness * R.Theory.BelievedLikingOfMe, 0.0f, 1.0f);
    }

    R.Kind = ClassifyRelation(R);
}

void USocialComponent::OnHelpedBy(AActor* Other, float Magnitude, float WorldTime, const UPersonalityComponent* Personality)
{
    if (!Other)
    {
        return;
    }

    FRelationship& R = FindOrAdd(Other, WorldTime);
    const float M = FMath::Clamp(Magnitude, 0.0f, 1.0f);

    R.Liking = FMath::Clamp(R.Liking + M * 0.20f, -1.0f, 1.0f);
    R.Trust = FMath::Clamp(R.Trust + M * 0.12f, 0.0f, 1.0f);
    R.Respect = FMath::Clamp(R.Respect + M * 0.08f, 0.0f, 1.0f);
    R.Attachment = FMath::Clamp(R.Attachment + M * 0.06f, 0.0f, 1.0f);

    // Помощь создаёт долг. Взаимность — основа человеческих связей.
    R.Debt = FMath::Clamp(R.Debt + M * 0.6f, -2.0f, 2.0f);

    // Помощь от того, на кого злился, обезоруживает сильнее всего.
    R.Resentment = FMath::Max(0.0f, R.Resentment - M * 0.35f);

    R.LastInteractionAt = WorldTime;
    R.InteractionCount++;
    R.Kind = ClassifyRelation(R);
}

void USocialComponent::OnHarmedBy(AActor* Other, float Magnitude, bool bIntentional, float WorldTime, const UPersonalityComponent* Personality)
{
    if (!Other)
    {
        return;
    }

    FRelationship& R = FindOrAdd(Other, WorldTime);
    const float M = FMath::Clamp(Magnitude, 0.0f, 1.0f);

    // Здесь и проходит граница между «он подонок» и «с кем не бывает».
    // Намерение решает почти всё.
    const float Weight = bIntentional ? 1.0f : 0.25f;
    const float Vengeful = Personality ? Personality->Facets.Vengefulness : 0.5f;

    R.Liking = FMath::Clamp(R.Liking - M * 0.30f * Weight, -1.0f, 1.0f);
    R.Trust = FMath::Clamp(R.Trust - M * 0.35f * Weight, 0.0f, 1.0f);
    R.Fear = FMath::Clamp(R.Fear + M * 0.30f * Weight, 0.0f, 1.0f);

    if (bIntentional)
    {
        R.Resentment = FMath::Clamp(R.Resentment + M * (0.25f + Vengeful * 0.55f), 0.0f, 1.0f);
        R.Respect = FMath::Clamp(R.Respect - M * 0.2f, 0.0f, 1.0f);
        R.InGroup = FMath::Max(0.0f, R.InGroup - M * 0.4f);
    }

    R.LastInteractionAt = WorldTime;
    R.InteractionCount++;
    R.Kind = ClassifyRelation(R);
}

void USocialComponent::OnBetrayedBy(AActor* Other, float Magnitude, float WorldTime, const UPersonalityComponent* Personality)
{
    if (!Other)
    {
        return;
    }

    FRelationship& R = FindOrAdd(Other, WorldTime);
    const float M = FMath::Clamp(Magnitude, 0.0f, 1.0f);

    // Предательство бьёт тем больнее, чем ближе был человек.
    // Незнакомец не может тебя предать — для этого нужно доверие.
    const float Closeness = GetCloseness(Other);
    const float Pain = M * (0.4f + Closeness * 1.6f);

    R.Trust = FMath::Clamp(R.Trust - Pain * 0.9f, 0.0f, 1.0f);
    R.Liking = FMath::Clamp(R.Liking - Pain * 0.6f, -1.0f, 1.0f);
    R.Respect = FMath::Clamp(R.Respect - Pain * 0.5f, 0.0f, 1.0f);
    R.Resentment = FMath::Clamp(R.Resentment + Pain * 0.8f, 0.0f, 1.0f);
    R.InGroup = 0.0f;

    // Привязанность рвётся не сразу — в этом и мучение.
    R.Attachment = FMath::Max(0.0f, R.Attachment - Pain * 0.35f);

    R.LastInteractionAt = WorldTime;
    R.Kind = ClassifyRelation(R);
}

void USocialComponent::OnCaughtLying(AActor* Other, float WorldTime)
{
    if (!Other)
    {
        return;
    }

    FRelationship& R = FindOrAdd(Other, WorldTime);
    R.CaughtLying++;

    // Каждая пойманная ложь бьёт по доверию сильнее предыдущей.
    const float Blow = 0.25f * R.CaughtLying;
    R.Trust = FMath::Clamp(R.Trust - Blow, 0.0f, 1.0f);
    R.Respect = FMath::Clamp(R.Respect - 0.15f, 0.0f, 1.0f);
    R.Resentment = FMath::Clamp(R.Resentment + 0.2f, 0.0f, 1.0f);

    // И меняет само представление о человеке.
    R.Theory.ModelAccuracy = FMath::Max(0.05f, R.Theory.ModelAccuracy - 0.2f);
    R.Kind = ClassifyRelation(R);
}

bool USocialComponent::ReceiveApology(AActor* Other, float Sincerity, const UPersonalityComponent* Personality, float WorldTime)
{
    if (!Other)
    {
        return false;
    }

    FRelationship* R = Find(Other);
    if (!R || R->Resentment < 0.05f)
    {
        return true; // не на что обижаться
    }

    const float Agreeable = Personality ? Personality->Traits.Agreeableness : 0.5f;
    const float Vengeful = Personality ? Personality->Facets.Vengefulness : 0.5f;
    const float S = FMath::Clamp(Sincerity, 0.0f, 1.0f);

    // Простить легче, если: человек дорог, извинение искреннее,
    // характер незлопамятный, а обида не смертельная.
    const float ForgiveChance = FMath::Clamp(
        S * 0.35f + Agreeable * 0.3f + R->Attachment * 0.25f - Vengeful * 0.3f - R->Resentment * 0.35f + 0.2f,
        0.02f, 0.95f);

    const bool bForgiven = FMath::FRand() < ForgiveChance;

    if (bForgiven)
    {
        R->Resentment = FMath::Max(0.0f, R->Resentment - S * 0.6f);
        R->Trust = FMath::Clamp(R->Trust + S * 0.08f, 0.0f, 1.0f);
        R->Liking = FMath::Clamp(R->Liking + S * 0.12f, -1.0f, 1.0f);
    }
    else
    {
        // Отвергнутое извинение бывает хуже, чем никакого:
        // человек ждал, что его простят.
        R->Resentment = FMath::Clamp(R->Resentment - 0.05f, 0.0f, 1.0f);
    }

    R->LastInteractionAt = WorldTime;
    R->Kind = ClassifyRelation(*R);
    return bForgiven;
}

void USocialComponent::OnReunion(AActor* Other, float WorldTime)
{
    if (!Other)
    {
        return;
    }

    FRelationship& R = FindOrAdd(Other, WorldTime);
    const float DaysApart = (WorldTime - R.LastInteractionAt) / 86400.0f;

    if (DaysApart > 2.0f && R.Attachment > 0.3f)
    {
        // По близким скучают, и встреча их подтягивает обратно.
        R.Liking = FMath::Clamp(R.Liking + 0.08f, -1.0f, 1.0f);
        R.Attachment = FMath::Clamp(R.Attachment + 0.05f, 0.0f, 1.0f);
    }

    R.LastInteractionAt = WorldTime;
}

void USocialComponent::OnDeathOf(AActor* Other, float WorldTime)
{
    FRelationship* R = Find(Other);
    if (!R)
    {
        return;
    }

    // Смерть человека не стирает отношение — оно застывает.
    // Обида на мёртвого не тает, и простить его уже нельзя.
    R->Kind = ERelationKind::Stranger;
}

// ---------------------------------------------------------------------------
//  Слухи
// ---------------------------------------------------------------------------

void USocialComponent::ReceiveGossip(AActor* From, AActor* About, float Valence, float WorldTime, const UPersonalityComponent* Personality)
{
    if (!About || About == GetOwner())
    {
        return;
    }

    const float SourceTrust = From ? GetTrust(From) : 0.3f;
    FRelationship& R = FindOrAdd(About, WorldTime);

    // Чужое мнение влияет тем сильнее, чем больше доверия источнику
    // и чем меньше я знаю человека лично.
    // Про близкого друга сплетне почти не поверят.
    const float PersonalWeight = R.Familiarity;
    const float Influence = SourceTrust * (1.0f - PersonalWeight * 0.85f) * 0.35f;

    R.Liking = FMath::Clamp(R.Liking + Valence * Influence, -1.0f, 1.0f);

    if (Valence < 0.0f)
    {
        R.Trust = FMath::Clamp(R.Trust + Valence * Influence * 0.6f, 0.0f, 1.0f);
    }

    // Про незнакомого человека слух создаёт само знакомство «заочно».
    if (R.Familiarity < 0.1f)
    {
        R.Familiarity = FMath::Max(R.Familiarity, 0.08f);
    }

    R.Kind = ClassifyRelation(R);
}

void USocialComponent::AdjustReputation(float Delta)
{
    Reputation = FMath::Clamp(Reputation + Delta, -1.0f, 1.0f);
}

// ---------------------------------------------------------------------------
//  ТЕОРИЯ РАЗУМА
// ---------------------------------------------------------------------------

void USocialComponent::ObserveAndInfer(AActor* Other, float ObservedMood, ENeedType ObservedNeed,
                                       float TheirExpressiveness, float MyEmpathySkill, float WorldTime)
{
    if (!Other)
    {
        return;
    }

    FRelationship& R = FindOrAdd(Other, WorldTime);

    // Точность чтения: моя эмпатия × его открытость × наше знакомство.
    // Закрытого незнакомца не прочтёт даже чуткий человек.
    const float Accuracy = FMath::Clamp(
        MyEmpathySkill * 0.45f + TheirExpressiveness * 0.35f + R.Familiarity * 0.30f,
        0.05f, 0.95f);

    // Считанное состояние = правда, размазанная ошибкой.
    // Чем хуже точность, тем больше отсебятины.
    const float Error = (1.0f - Accuracy);
    const float Guessed = FMath::Clamp(
        ObservedMood * Accuracy + FMath::FRandRange(-1.0f, 1.0f) * Error,
        -1.0f, 1.0f);

    R.Theory.BelievedMood = FMath::Lerp(R.Theory.BelievedMood, Guessed, 0.6f);

    // Потребность считывается либо верно, либо наугад.
    if (FMath::FRand() < Accuracy)
    {
        R.Theory.BelievedDominantNeed = ObservedNeed;
    }
    else
    {
        // ...и вот здесь рождается непонимание: человек хочет одного,
        // а к нему лезут с другим.
        R.Theory.BelievedDominantNeed = static_cast<ENeedType>(
            FMath::RandRange(0, static_cast<int32>(ENeedType::MAX) - 1));
    }

    // Как он ко мне относится? Судим по себе — это эффект проекции.
    // Тот, кто сам зол на людей, считает, что и они злы на него.
    const float Projection = R.Liking * 0.35f;
    R.Theory.BelievedLikingOfMe = FMath::Clamp(
        FMath::Lerp(R.Theory.BelievedLikingOfMe, Guessed * 0.4f + Projection, 0.4f),
        -1.0f, 1.0f);

    R.Theory.BelievedTrustInMe = FMath::Clamp(
        R.Trust * 0.5f + R.Theory.BelievedLikingOfMe * 0.3f + 0.2f, 0.0f, 1.0f);

    // --- Второй уровень: что он думает о моём отношении к нему -------------
    //
    // Человек судит об этом по себе: он прикидывает, как выглядит со
    // стороны. И ошибается — тем сильнее, чем хуже читает людей.
    // Отсюда и берётся «он решил, что я на него злюсь», хотя не злюсь.
    {
        // Как я, по-моему, выгляжу: то, что я показываю, плюс то,
        // как я себя веду в последнее время.
        const float HowIProbablySeem = R.Liking - R.Resentment * 0.8f;

        // Долгое молчание читается как холодность — и он это понимает.
        const float DaysSince = (WorldTime - R.LastInteractionAt) / 86400.0f;
        const float Coldness = FMath::Clamp(DaysSince * 0.35f, 0.0f, 0.6f);

        const float Guess = FMath::Clamp(HowIProbablySeem - Coldness, -1.0f, 1.0f);

        // Чем хуже я читаю людей, тем больше отсебятины в этой догадке.
        const float Blur = (1.0f - Accuracy) * FMath::FRandRange(-0.5f, 0.5f);

        R.Theory.BelievedViewOfMyOpinion = FMath::Clamp(
            FMath::Lerp(R.Theory.BelievedViewOfMyOpinion, Guess + Blur, 0.45f), -1.0f, 1.0f);
    }

    R.Theory.BelievedKnowledgeOfMe = FMath::Clamp(R.Familiarity * 0.8f + 0.1f, 0.0f, 1.0f);
    R.Theory.ModelAccuracy = FMath::Clamp(FMath::Lerp(R.Theory.ModelAccuracy, Accuracy, 0.3f), 0.0f, 1.0f);
    R.Theory.UpdatedAt = WorldTime;
}

float USocialComponent::GetBelievedLikingOfMe(AActor* Other) const
{
    const FRelationship* R = Find(Other);
    return R ? R->Theory.BelievedLikingOfMe : 0.0f;
}

float USocialComponent::GetBelievedKnowledge(AActor* Other) const
{
    const FRelationship* R = Find(Other);
    return R ? R->Theory.BelievedKnowledgeOfMe : 0.1f;
}

// ---------------------------------------------------------------------------
//  Решения
// ---------------------------------------------------------------------------

bool USocialComponent::DecideToLie(AActor* To, float Benefit, float LieSkill,
                                   const UPersonalityComponent* Personality, FString& OutReasoning) const
{
    const float B = FMath::Clamp(Benefit, 0.0f, 1.0f);
    const float Honesty = Personality ? Personality->Facets.Honesty : 0.6f;
    const float Risk = Personality ? Personality->Facets.RiskTaking : 0.5f;

    const FRelationship* R = Find(To);
    const float Closeness = R ? GetCloseness(To) : 0.0f;
    const float TheirKnowledge = R ? R->Theory.BelievedKnowledgeOfMe : 0.1f;

    // Шанс, что раскусят: он тем выше, чем лучше человек меня знает
    // и чем хуже я вру.
    const float DetectionRisk = FMath::Clamp(TheirKnowledge * 0.6f + (1.0f - LieSkill) * 0.5f, 0.0f, 1.0f);

    // Цена совести: врать близкому дороже, чем чужому.
    const float ConscienceCost = Honesty * (0.5f + Closeness * 0.9f);

    // Цена разоблачения: потерять доверие того, кто тебе дорог.
    const float ExposureCost = DetectionRisk * (0.3f + Closeness * 1.2f) * (1.0f - Risk * 0.4f);

    const float Score = B - ConscienceCost - ExposureCost;

    if (Score > 0.0f)
    {
        OutReasoning = (Closeness > 0.5f)
            ? TEXT("соврать... но это же свой человек. И всё равно соврал(а)")
            : TEXT("он всё равно не узнает");
        return true;
    }

    OutReasoning = (ConscienceCost > ExposureCost)
        ? TEXT("не могу ему врать")
        : TEXT("раскусит — и тогда конец");
    return false;
}

bool USocialComponent::DecideToHelp(AActor* Whom, float Cost, float TheirNeed,
                                    const UPersonalityComponent* Personality, FString& OutReasoning) const
{
    const float C = FMath::Clamp(Cost, 0.0f, 1.0f);
    const float Need = FMath::Clamp(TheirNeed, 0.0f, 1.0f);

    const float Empathy = Personality ? Personality->Facets.Empathy : 0.5f;
    const float Benevolence = Personality ? Personality->Values.Benevolence : 0.5f;

    const FRelationship* R = Find(Whom);
    const float Closeness = R ? GetCloseness(Whom) : 0.0f;
    const float Debt = R ? R->Debt : 0.0f;          // >0 — я должен ему
    const float Resentment = R ? R->Resentment : 0.0f;

    float Score = 0.0f;
    Score += Need * (0.3f + Empathy * 0.9f);        // жалко
    Score += Closeness * 1.1f;                       // свой
    Score += Benevolence * 0.5f;                     // так надо
    Score += FMath::Max(0.0f, Debt) * 0.6f;          // я обязан
    Score -= C * 1.2f;                               // мне это дорого
    Score -= Resentment * 1.3f;                      // а вот ему — не помогу

    // Помощь на людях ценнее: репутация тоже мотив, хоть в этом и не признаются.
    Score += 0.1f;

    if (Score > 0.0f)
    {
        if (Debt > 0.4f)                 OutReasoning = TEXT("он мне помогал — теперь моя очередь");
        else if (Closeness > 0.5f)       OutReasoning = TEXT("как не помочь своему");
        else if (Empathy > 0.6f)         OutReasoning = TEXT("ему явно плохо");
        else                             OutReasoning = TEXT("ладно, помогу");
        return true;
    }

    if (Resentment > 0.4f)               OutReasoning = TEXT("перебьётся");
    else if (C > 0.6f)                   OutReasoning = TEXT("мне бы со своим разобраться");
    else                                 OutReasoning = TEXT("не моё дело");
    return false;
}

bool USocialComponent::DecideToRetaliate(AActor* Against, const UPersonalityComponent* Personality) const
{
    const FRelationship* R = Find(Against);
    if (!R)
    {
        return false;
    }

    const float Vengeful = Personality ? Personality->Facets.Vengefulness : 0.5f;
    const float Impulsive = Personality ? Personality->Facets.Impulsivity : 0.5f;
    const float SelfControl = Personality ? Personality->Facets.SelfControl : 0.5f;

    float Score = R->Resentment * (0.5f + Vengeful) + Impulsive * 0.3f;
    Score -= SelfControl * 0.6f;
    Score -= R->Fear * 1.2f;          // страшно связываться
    Score -= R->Attachment * 0.8f;    // и жалко тоже

    return Score > 0.45f;
}

// ---------------------------------------------------------------------------
//  Запросы
// ---------------------------------------------------------------------------

float USocialComponent::GetCloseness(AActor* Other) const
{
    const FRelationship* R = Find(Other);
    if (!R)
    {
        return 0.0f;
    }

    const float Positive = FMath::Max(0.0f, R->Liking);
    return FMath::Clamp(R->Familiarity * 0.3f + Positive * 0.35f + R->Attachment * 0.35f - R->Resentment * 0.4f, 0.0f, 1.0f);
}

float USocialComponent::GetTrust(AActor* Other) const
{
    const FRelationship* R = Find(Other);
    return R ? R->Trust : 0.2f;
}

float USocialComponent::GetSocialSupport() const
{
    // Поддержка — не количество знакомых, а глубина немногих связей.
    // Сто приятелей не заменяют одного близкого.
    TArray<float> Closeness;
    for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Relations)
    {
        if (!Pair.Key)
        {
            continue;
        }
        const FRelationship& R = Pair.Value;
        const float C = FMath::Clamp(R.Familiarity * 0.3f + FMath::Max(0.0f, R.Liking) * 0.35f + R.Attachment * 0.35f, 0.0f, 1.0f);
        if (C > 0.15f)
        {
            Closeness.Add(C);
        }
    }

    if (Closeness.Num() == 0)
    {
        return 0.0f;
    }

    Closeness.Sort([](const float& A, const float& B) { return A > B; });

    // Первый близкий человек даёт больше всего, остальные — по убывающей.
    float Support = 0.0f;
    float Weight = 1.0f;
    for (int32 i = 0; i < FMath::Min(6, Closeness.Num()); ++i)
    {
        Support += Closeness[i] * Weight;
        Weight *= 0.55f;
    }
    return FMath::Clamp(Support / 2.2f, 0.0f, 1.0f);
}

AActor* USocialComponent::GetClosestPerson() const
{
    AActor* Best = nullptr;
    float BestScore = 0.15f;

    for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Relations)
    {
        if (!Pair.Key)
        {
            continue;
        }
        const float C = GetCloseness(Pair.Key);
        if (C > BestScore)
        {
            BestScore = C;
            Best = Pair.Key;
        }
    }
    return Best;
}

AActor* USocialComponent::GetGreatestGrudge() const
{
    AActor* Best = nullptr;
    float BestScore = 0.2f;

    for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Relations)
    {
        if (!Pair.Key)
        {
            continue;
        }
        if (Pair.Value.Resentment > BestScore)
        {
            BestScore = Pair.Value.Resentment;
            Best = Pair.Key;
        }
    }
    return Best;
}

int32 USocialComponent::CountFriends() const
{
    int32 Count = 0;
    for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Relations)
    {
        if (Pair.Value.Kind == ERelationKind::Friend
            || Pair.Value.Kind == ERelationKind::CloseFriend
            || Pair.Value.Kind == ERelationKind::Partner)
        {
            ++Count;
        }
    }
    return Count;
}

ERelationKind USocialComponent::ClassifyRelation(const FRelationship& R)
{
    // Тип отношений — это не отдельная сущность, а ярлык,
    // который человек навешивает на сложившийся набор чувств.
    if (R.Resentment > 0.6f && R.Liking < -0.3f)
    {
        return ERelationKind::Enemy;
    }
    if (R.Resentment > 0.35f && R.Respect > 0.4f)
    {
        return ERelationKind::Rival;
    }
    if (R.Romantic > 0.55f && R.Attachment > 0.5f && R.Liking > 0.5f)
    {
        return ERelationKind::Partner;
    }
    if (R.Attachment > 0.6f && R.Trust > 0.6f && R.Liking > 0.5f)
    {
        return ERelationKind::CloseFriend;
    }
    if (R.Liking > 0.3f && R.Familiarity > 0.35f)
    {
        return ERelationKind::Friend;
    }
    if (R.Respect > 0.7f && R.Familiarity > 0.3f && R.Liking > 0.1f)
    {
        return ERelationKind::Mentor;
    }
    if (R.Familiarity > 0.15f)
    {
        return ERelationKind::Acquaintance;
    }
    return ERelationKind::Stranger;
}

FString USocialComponent::DescribeRelation(AActor* Other) const
{
    const FRelationship* R = Find(Other);
    if (!R)
    {
        return TEXT("я его не знаю");
    }

    FString Result = HumanText::Relation(R->Kind);

    // Оттенки, которые не влезают в ярлык.
    if (R->Resentment > 0.4f && R->Liking > 0.2f)
    {
        Result += TEXT(", на которого я в обиде");
    }
    else if (R->Fear > 0.5f)
    {
        Result += TEXT(", которого я побаиваюсь");
    }
    else if (R->Trust < 0.2f && R->Liking > 0.3f)
    {
        Result += TEXT(", но доверять ему нельзя");
    }
    else if (R->Debt > 0.5f)
    {
        Result += TEXT(", которому я должен");
    }
    else if (R->Romantic > 0.4f && R->Kind != ERelationKind::Partner)
    {
        Result += TEXT(", о котором я думаю чаще, чем следует");
    }

    return Result;
}
