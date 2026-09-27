// MemoryComponent.cpp

#include "MemoryComponent.h"

UMemoryComponent::UMemoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    MemoryQuality = FMath::FRandRange(0.75f, 1.0f);
}

// ---------------------------------------------------------------------------
//  Запись
// ---------------------------------------------------------------------------

int32 UMemoryComponent::Encode(const FString& Summary, FName Tag, float Valence, float Arousal,
                               const TArray<AActor*>& Participants, const FVector& Location,
                               float WorldTime, float Attention, EMemoryKind Kind)
{
    const float Att = FMath::Clamp(Attention, 0.0f, 1.0f);
    const float Aro = FMath::Clamp(Arousal, 0.0f, 1.0f);

    // Сила следа: внимание решает, эмоция усиливает.
    // Безразличное событие при рассеянном внимании не отложится вообще.
    float Strength = (0.25f + Att * 0.75f) * (0.5f + Aro * 0.9f) * MemoryQuality;

    if (Strength < 0.08f)
    {
        // Прошло мимо сознания.
        return -1;
    }

    FEpisodicMemory New;
    New.Id = NextId++;
    New.Kind = Kind;
    New.Summary = Summary;
    New.Tag = Tag;
    New.Location = Location;
    New.Timestamp = WorldTime;
    New.Strength = FMath::Clamp(Strength, 0.0f, 1.0f);
    New.Valence = FMath::Clamp(Valence, -1.0f, 1.0f);
    New.Arousal = Aro;
    New.LastRecalledAt = WorldTime;

    for (AActor* A : Participants)
    {
        if (A)
        {
            New.Participants.Add(A);
        }
    }

    // Потрясение записывается иначе: ярко, подробно и почти навсегда.
    if (Aro > 0.75f && FMath::Abs(Valence) > 0.6f)
    {
        New.Kind = (Valence < 0.0f && Aro > 0.9f) ? EMemoryKind::Traumatic : EMemoryKind::Flashbulb;
        New.Strength = 1.0f;
    }

    Episodes.Add(New);
    EnforceCapacity();
    return New.Id;
}

int32 UMemoryComponent::EncodeSimple(const FString& Summary, float Valence, float WorldTime)
{
    static const TArray<AActor*> Empty;
    return Encode(Summary, NAME_None, Valence, FMath::Abs(Valence) * 0.6f, Empty, FVector::ZeroVector, WorldTime, 0.7f);
}

// ---------------------------------------------------------------------------
//  Забывание
// ---------------------------------------------------------------------------

void UMemoryComponent::Advance(float GameDelta, float WorldTime, float CognitiveImpairment)
{
    // Забывание считаем не каждый кадр — раз в игровые полчаса достаточно.
    DecayAccumulator += GameDelta;
    if (DecayAccumulator < 1800.0f)
    {
        return;
    }
    const float Elapsed = DecayAccumulator;
    DecayAccumulator = 0.0f;

    const float Hours = Elapsed / 3600.0f;

    for (int32 i = Episodes.Num() - 1; i >= 0; --i)
    {
        FEpisodicMemory& M = Episodes[i];

        // Постоянная времени следа. Базовая — около 40 игровых часов,
        // но эмоции, повторения и консолидация её сильно растягивают.
        float Tau = 40.0f;
        Tau *= (1.0f + M.Arousal * 4.0f);                        // яркое держится дольше
        Tau *= (1.0f + M.RecallCount * 0.45f);                   // повторение — мать учения
        Tau *= M.bConsolidated ? 3.0f : 1.0f;                    // пережитое во сне закрепилось
        Tau *= MemoryQuality;

        switch (M.Kind)
        {
        case EMemoryKind::Flashbulb:      Tau *= 8.0f; break;
        case EMemoryKind::Traumatic:      Tau *= 12.0f; break;   // травма не забывается
        case EMemoryKind::Autobiographic: Tau *= 6.0f; break;
        default: break;
        }

        // Кривая Эббингауза.
        M.Strength *= FMath::Exp(-Hours / FMath::Max(1.0f, Tau));

        // Порог исчезновения: слабый след уже не восстановить.
        if (M.Strength < 0.015f && M.Kind != EMemoryKind::Traumatic)
        {
            Episodes.RemoveAt(i);
            ++ForgottenCount;
        }
    }

    // Убеждения тоже тускнеют, если их ничто не подтверждает.
    for (int32 i = Beliefs.Num() - 1; i >= 0; --i)
    {
        FBelief& B = Beliefs[i];
        if (!B.bVerified)
        {
            B.Confidence *= FMath::Exp(-Hours / 400.0f);
            if (B.Confidence < 0.05f)
            {
                Beliefs.RemoveAt(i);
            }
        }
    }

    // Знакомство с местами тает, если туда не ходить.
    for (FKnownLocation& P : Places)
    {
        const float Since = (WorldTime - P.LastVisitedAt) / 86400.0f;
        if (Since > 3.0f)
        {
            P.Familiarity = FMath::Max(0.05f, P.Familiarity - Hours * 0.0015f);
        }
    }

    // Пресыщение рассасывается: к чему угодно снова тянет, если долго
    // этого не делать. Половина проходит примерно за четыре игровых часа.
    for (FOutcomeAssociation& O : Outcomes)
    {
        O.Satiation *= FMath::Exp(-Hours / 4.0f);
    }

    // Дорога, которой не ходят, забывается.
    for (int32 i = Routes.Num() - 1; i >= 0; --i)
    {
        const float DaysIdle = (WorldTime - Routes[i].LastUsedAt) / 86400.0f;
        if (DaysIdle > 1.0f)
        {
            Routes[i].Strength = FMath::Max(0.0f, Routes[i].Strength - Hours * 0.004f);
            if (Routes[i].Strength <= 0.02f)
            {
                Routes.RemoveAt(i);
            }
        }
    }

    // Привычки без повторения слабеют.
    for (int32 i = Habits.Num() - 1; i >= 0; --i)
    {
        FHabit& H = Habits[i];
        const float SinceDays = (WorldTime - H.LastFiredAt) / 86400.0f;
        if (SinceDays > 1.0f)
        {
            H.Strength = FMath::Max(0.0f, H.Strength - Hours * 0.004f);
            if (H.Strength <= 0.01f)
            {
                Habits.RemoveAt(i);
            }
        }
    }

    // Туман в голове мешает не только вспоминать, но и хранить.
    if (CognitiveImpairment > 0.6f)
    {
        for (FEpisodicMemory& M : Episodes)
        {
            if (!M.bConsolidated && M.Arousal < 0.4f)
            {
                M.Strength *= (1.0f - (CognitiveImpairment - 0.6f) * 0.05f * Hours);
            }
        }
    }
}

void UMemoryComponent::EnforceCapacity()
{
    while (Episodes.Num() > EpisodeCapacity)
    {
        // Вытесняем самый слабый след, но травмы не трогаем.
        int32 WeakestIndex = INDEX_NONE;
        float WeakestScore = FLT_MAX;

        for (int32 i = 0; i < Episodes.Num(); ++i)
        {
            if (Episodes[i].Kind == EMemoryKind::Traumatic || Episodes[i].Kind == EMemoryKind::Autobiographic)
            {
                continue;
            }
            const float Score = Episodes[i].Strength * (1.0f + Episodes[i].Arousal);
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
        Episodes.RemoveAt(WeakestIndex);
        ++ForgottenCount;
    }

    while (Beliefs.Num() > BeliefCapacity)
    {
        int32 WeakestIndex = 0;
        for (int32 i = 1; i < Beliefs.Num(); ++i)
        {
            if (Beliefs[i].Confidence < Beliefs[WeakestIndex].Confidence)
            {
                WeakestIndex = i;
            }
        }
        Beliefs.RemoveAt(WeakestIndex);
    }
}

// ---------------------------------------------------------------------------
//  Припоминание
// ---------------------------------------------------------------------------

FEpisodicMemory* UMemoryComponent::Recall(FName Tag, AActor* About, float WorldTime)
{
    // Собираем кандидатов и взвешиваем: похожесть × сила следа × свежесть.
    int32 BestIndex = INDEX_NONE;
    float BestScore = 0.0f;

    for (int32 i = 0; i < Episodes.Num(); ++i)
    {
        FEpisodicMemory& M = Episodes[i];
        if (M.bRepressed)
        {
            continue; // к вытесненному сознание не пускает
        }

        float Match = 0.0f;
        if (!Tag.IsNone() && M.Tag == Tag)
        {
            Match += 1.0f;
        }
        if (About && M.Participants.Contains(About))
        {
            Match += 1.0f;
        }
        if (Tag.IsNone() && !About)
        {
            Match = 0.4f; // свободное припоминание
        }
        if (Match <= 0.0f)
        {
            continue;
        }

        // Эффект недавности: свежее достаётся легче.
        const float AgeHours = FMath::Max(0.0f, (WorldTime - M.Timestamp) / 3600.0f);
        const float Recency = 1.0f / (1.0f + AgeHours / 24.0f);

        const float Score = Match * M.Strength * (0.6f + Recency * 0.4f) * (0.7f + M.Arousal * 0.5f);
        if (Score > BestScore)
        {
            BestScore = Score;
            BestIndex = i;
        }
    }

    if (BestIndex == INDEX_NONE)
    {
        return nullptr;
    }

    // Даже найденный след может не подняться: «вертится на языке».
    const float RetrievalChance = FMath::Clamp(BestScore * 1.6f * MemoryQuality, 0.05f, 0.98f);
    if (FMath::FRand() > RetrievalChance)
    {
        return nullptr;
    }

    FEpisodicMemory& Found = Episodes[BestIndex];
    Reconsolidate(Found, WorldTime);
    return &Found;
}

void UMemoryComponent::Reconsolidate(FEpisodicMemory& Memory, float WorldTime)
{
    // Достали — значит, укрепили.
    Memory.RecallCount++;
    Memory.LastRecalledAt = WorldTime;
    Memory.Strength = FMath::Clamp(Memory.Strength + 0.12f, 0.0f, 1.0f);

    // ...и одновременно подпортили. Каждое припоминание — это пересборка,
    // и сборка получается чуть иной. Так рождаются ложные воспоминания.
    Memory.Distortion = FMath::Clamp(Memory.Distortion + FMath::FRandRange(0.01f, 0.05f), 0.0f, 1.0f);

    // Эмоциональная окраска дрейфует — обычно в сторону усиления.
    // Плохое становится ещё хуже, хорошее — золотым.
    const float Drift = FMath::Sign(Memory.Valence) * FMath::FRandRange(0.0f, 0.035f);
    Memory.Valence = FMath::Clamp(Memory.Valence + Drift, -1.0f, 1.0f);

    // Сильно искажённое воспоминание меняет и формулировку.
    if (Memory.Distortion > 0.45f && !Memory.Summary.Contains(TEXT("кажется")))
    {
        Memory.Summary = TEXT("кажется, ") + Memory.Summary;
    }
}

TArray<FEpisodicMemory> UMemoryComponent::Query(FName Tag, AActor* About, int32 MaxCount) const
{
    struct FScored { const FEpisodicMemory* M; float Score; };
    TArray<FScored> Scored;

    for (const FEpisodicMemory& M : Episodes)
    {
        if (M.bRepressed)
        {
            continue;
        }
        float Match = 0.0f;
        if (!Tag.IsNone() && M.Tag == Tag) Match += 1.0f;
        if (About && M.Participants.Contains(About)) Match += 1.0f;
        if (Tag.IsNone() && !About) Match = 0.5f;
        if (Match <= 0.0f) continue;

        Scored.Add({ &M, Match * M.Strength });
    }

    Scored.Sort([](const FScored& A, const FScored& B) { return A.Score > B.Score; });

    TArray<FEpisodicMemory> Result;
    const int32 Num = FMath::Min(MaxCount, Scored.Num());
    for (int32 i = 0; i < Num; ++i)
    {
        Result.Add(*Scored[i].M);
    }
    return Result;
}

FEpisodicMemory* UMemoryComponent::GetMostVivid()
{
    FEpisodicMemory* Best = nullptr;
    float BestScore = 0.0f;

    for (FEpisodicMemory& M : Episodes)
    {
        if (M.bRepressed)
        {
            continue;
        }
        const float Score = M.Strength * (0.4f + M.Arousal) * (0.5f + FMath::Abs(M.Valence));
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = &M;
        }
    }
    return Best;
}

FEpisodicMemory* UMemoryComponent::GetRandomWeighted()
{
    if (Episodes.Num() == 0)
    {
        return nullptr;
    }

    float Total = 0.0f;
    for (const FEpisodicMemory& M : Episodes)
    {
        Total += M.Strength * (0.3f + M.Arousal);
    }
    if (Total <= 0.0f)
    {
        return &Episodes[FMath::RandRange(0, Episodes.Num() - 1)];
    }

    float Roll = FMath::FRandRange(0.0f, Total);
    for (FEpisodicMemory& M : Episodes)
    {
        Roll -= M.Strength * (0.3f + M.Arousal);
        if (Roll <= 0.0f)
        {
            return &M;
        }
    }
    return &Episodes.Last();
}

float UMemoryComponent::GetFamiliarityWith(AActor* Who) const
{
    if (!Who)
    {
        return 0.0f;
    }

    float Sum = 0.0f;
    for (const FEpisodicMemory& M : Episodes)
    {
        if (M.Participants.Contains(Who))
        {
            Sum += M.Strength;
        }
    }
    return FMath::Clamp(Sum / 12.0f, 0.0f, 1.0f);
}

float UMemoryComponent::GetAffectiveToneAbout(AActor* Who) const
{
    if (!Who)
    {
        return 0.0f;
    }

    float Sum = 0.0f;
    float Weight = 0.0f;
    for (const FEpisodicMemory& M : Episodes)
    {
        if (M.Participants.Contains(Who))
        {
            // Яркое воспоминание весит больше — так работает эвристика доступности.
            const float W = M.Strength * (0.5f + M.Arousal);
            Sum += M.Valence * W;
            Weight += W;
        }
    }
    return Weight > 0.0f ? FMath::Clamp(Sum / Weight, -1.0f, 1.0f) : 0.0f;
}

float UMemoryComponent::GetRecentLifeTone(float WorldTime, float WindowSeconds) const
{
    float Sum = 0.0f;
    float Weight = 0.0f;
    for (const FEpisodicMemory& M : Episodes)
    {
        if (WorldTime - M.Timestamp > WindowSeconds)
        {
            continue;
        }
        const float W = M.Strength * (0.5f + M.Arousal);
        Sum += M.Valence * W;
        Weight += W;
    }
    return Weight > 0.0f ? FMath::Clamp(Sum / Weight, -1.0f, 1.0f) : 0.0f;
}

// ---------------------------------------------------------------------------
//  Сон: консолидация и обобщение
// ---------------------------------------------------------------------------

void UMemoryComponent::ConsolidateDuringSleep(float GameDelta, float WorldTime, bool bREM)
{
    const float Hours = GameDelta / 3600.0f;
    if (Hours <= 0.0f)
    {
        return;
    }

    // Медленный сон закрепляет следы, быстрый — перерабатывает эмоции.
    for (FEpisodicMemory& M : Episodes)
    {
        const float AgeHours = (WorldTime - M.Timestamp) / 3600.0f;
        if (AgeHours > 48.0f)
        {
            continue; // консолидируется только свежее
        }

        if (!bREM)
        {
            // Глубокий сон: переносим важное в долговременное хранение.
            const float Importance = M.Strength * (0.4f + M.Arousal);
            if (Importance > 0.3f)
            {
                M.Strength = FMath::Clamp(M.Strength + Hours * 0.12f, 0.0f, 1.0f);
                M.bConsolidated = true;
            }
        }
        else
        {
            // Быстрый сон снимает эмоциональный заряд, оставляя содержание.
            // Поэтому «утро вечера мудренее»: помнишь, но уже не так больно.
            M.Arousal = FMath::Max(0.0f, M.Arousal - Hours * 0.10f);
            M.Valence *= (1.0f - Hours * 0.04f);
        }
    }

    if (!bREM)
    {
        return;
    }

    // --- Обобщение: из повторяющихся эпизодов рождаются убеждения ----------
    // Это то, как опыт превращается в знание о мире.
    TMap<FName, int32> TagCounts;
    TMap<FName, float> TagValence;

    for (const FEpisodicMemory& M : Episodes)
    {
        if (M.Tag.IsNone() || M.Strength < 0.25f)
        {
            continue;
        }
        TagCounts.FindOrAdd(M.Tag)++;
        TagValence.FindOrAdd(M.Tag) += M.Valence;
    }

    for (const TPair<FName, int32>& Pair : TagCounts)
    {
        if (Pair.Value < 3)
        {
            continue; // трёх раз мало для вывода — но это уже «закономерность»
        }

        const float AvgValence = TagValence[Pair.Key] / Pair.Value;
        if (FMath::Abs(AvgValence) < 0.25f)
        {
            continue;
        }

        FBelief Generalization;
        Generalization.Subject = Pair.Key;
        Generalization.Predicate = TEXT("GenerallyGoes");
        Generalization.Value = FMath::Clamp(AvgValence, -1.0f, 1.0f);
        Generalization.Confidence = FMath::Clamp(0.3f + Pair.Value * 0.08f, 0.0f, 0.9f);
        Generalization.Source = nullptr;
        Generalization.bVerified = true;
        Generalization.LearnedAt = WorldTime;
        Generalization.Text = AvgValence > 0.0f
            ? FString::Printf(TEXT("обычно это заканчивается хорошо"))
            : FString::Printf(TEXT("обычно от этого только хуже"));

        Learn(Generalization, 1.0f, 0.5f, WorldTime);
    }

    // --- Во сне вытесненное иногда прорывается ------------------------------
    if (FMath::FRand() < 0.25f)
    {
        SurfaceRepressed();
    }
}

void UMemoryComponent::Repress(int32 MemoryId)
{
    for (FEpisodicMemory& M : Episodes)
    {
        if (M.Id == MemoryId)
        {
            M.bRepressed = true;
            M.Kind = EMemoryKind::Traumatic;
            return;
        }
    }
}

FEpisodicMemory* UMemoryComponent::SurfaceRepressed()
{
    TArray<FEpisodicMemory*> Repressed;
    for (FEpisodicMemory& M : Episodes)
    {
        if (M.bRepressed)
        {
            Repressed.Add(&M);
        }
    }
    if (Repressed.Num() == 0)
    {
        return nullptr;
    }

    FEpisodicMemory* M = Repressed[FMath::RandRange(0, Repressed.Num() - 1)];
    M->bRepressed = false;
    // Вернувшись, оно бьёт с прежней силой.
    M->Arousal = FMath::Max(M->Arousal, 0.7f);
    return M;
}

// ---------------------------------------------------------------------------
//  Убеждения
// ---------------------------------------------------------------------------

void UMemoryComponent::Learn(const FBelief& NewBelief, float SourceCredibility, float Openness, float WorldTime)
{
    if (NewBelief.Subject.IsNone() || NewBelief.Predicate.IsNone())
    {
        return;
    }

    const float Credibility = FMath::Clamp(SourceCredibility, 0.0f, 1.0f);

    if (FBelief* Existing = FindBelief(NewBelief.Subject, NewBelief.Predicate))
    {
        // Проверенное личным опытом не перебивается чужими словами.
        if (Existing->bVerified && !NewBelief.bVerified)
        {
            // Разве что источник крайне убедителен, а человек — очень внушаем.
            if (Credibility < 0.9f || Openness < 0.7f)
            {
                return;
            }
        }

        const bool bAgrees = FMath::Sign(Existing->Value) == FMath::Sign(NewBelief.Value)
                             || FMath::Abs(Existing->Value - NewBelief.Value) < 0.3f;

        // ПРЕДВЗЯТОСТЬ ПОДТВЕРЖДЕНИЯ: согласное впитывается легко,
        // противоречащее — со скрипом, и тем хуже, чем крепче старое мнение.
        float Weight = Credibility;
        if (bAgrees)
        {
            Weight *= 1.3f;
        }
        else
        {
            Weight *= FMath::Lerp(0.15f, 0.7f, Openness) * (1.0f - Existing->Confidence * 0.6f);
        }
        Weight = FMath::Clamp(Weight, 0.0f, 1.0f);

        Existing->Value = FMath::Lerp(Existing->Value, NewBelief.Value, Weight * 0.5f);

        if (bAgrees)
        {
            Existing->Confidence = FMath::Clamp(Existing->Confidence + Weight * 0.2f, 0.0f, 1.0f);
        }
        else
        {
            // Противоречие не столько меняет мнение, сколько расшатывает уверенность.
            Existing->Confidence = FMath::Clamp(Existing->Confidence - Weight * 0.15f, 0.05f, 1.0f);
        }

        Existing->LearnedAt = WorldTime;
        if (NewBelief.bVerified)
        {
            Existing->bVerified = true;
        }
        if (!NewBelief.Text.IsEmpty())
        {
            Existing->Text = NewBelief.Text;
        }
        return;
    }

    // Новое убеждение: принимается с уверенностью, ограниченной доверием к источнику.
    FBelief Added = NewBelief;
    Added.Confidence = FMath::Clamp(NewBelief.Confidence * (0.3f + Credibility * 0.7f), 0.05f, 1.0f);
    Added.LearnedAt = WorldTime;
    Beliefs.Add(Added);

    EnforceCapacity();
}

FBelief* UMemoryComponent::FindBelief(FName Subject, FName Predicate)
{
    for (FBelief& B : Beliefs)
    {
        if (B.Subject == Subject && B.Predicate == Predicate)
        {
            return &B;
        }
    }
    return nullptr;
}

const FBelief* UMemoryComponent::FindBelief(FName Subject, FName Predicate) const
{
    for (const FBelief& B : Beliefs)
    {
        if (B.Subject == Subject && B.Predicate == Predicate)
        {
            return &B;
        }
    }
    return nullptr;
}

float UMemoryComponent::GetBeliefValue(FName Subject, FName Predicate) const
{
    const FBelief* B = FindBelief(Subject, Predicate);
    return B ? B->Value : 0.0f;
}

float UMemoryComponent::GetBeliefConfidence(FName Subject, FName Predicate) const
{
    const FBelief* B = FindBelief(Subject, Predicate);
    return B ? B->Confidence : 0.0f;
}

void UMemoryComponent::VerifyBelief(FName Subject, FName Predicate, float ObservedValue, float WorldTime)
{
    if (FBelief* B = FindBelief(Subject, Predicate))
    {
        const float Error = FMath::Abs(B->Value - ObservedValue);
        B->Value = FMath::Lerp(B->Value, ObservedValue, 0.7f); // свои глаза важнее чужих слов
        B->bVerified = true;
        B->LearnedAt = WorldTime;
        B->Confidence = FMath::Clamp(B->Confidence + (Error < 0.3f ? 0.3f : -0.1f), 0.1f, 1.0f);
        return;
    }

    FBelief New;
    New.Subject = Subject;
    New.Predicate = Predicate;
    New.Value = ObservedValue;
    New.Confidence = 0.75f;
    New.bVerified = true;
    New.LearnedAt = WorldTime;
    Beliefs.Add(New);
    EnforceCapacity();
}

const FBelief* UMemoryComponent::PickBeliefToShare() const
{
    const FBelief* Best = nullptr;
    float BestScore = 0.0f;

    for (const FBelief& B : Beliefs)
    {
        // Делятся тем, во что верят и что кажется важным.
        const float Score = B.Confidence * (0.3f + FMath::Abs(B.Value)) * FMath::FRandRange(0.5f, 1.5f);
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = &B;
        }
    }
    return Best;
}

// ---------------------------------------------------------------------------
//  Места
// ---------------------------------------------------------------------------

void UMemoryComponent::LearnPlace(EPlaceKind Kind, const FVector& Location, const FString& Label,
                                  bool bFirsthand, AActor* ToldBy, float WorldTime,
                                  const TArray<FAffordance>* Offers)
{
    // Места, отстоящие меньше чем на 5 метров, считаем одним и тем же.
    for (FKnownLocation& P : Places)
    {
        if (P.Kind == Kind && FVector::DistSquared(P.Location, Location) < 250000.0f)
        {
            if (bFirsthand && !P.bFirsthand)
            {
                // Подтвердил своими глазами то, о чём слышал.
                P.bFirsthand = true;
                P.Familiarity = FMath::Max(P.Familiarity, 0.4f);
            }
            P.Familiarity = FMath::Clamp(P.Familiarity + 0.05f, 0.0f, 1.0f);

            // Разглядел то, чего раньше не замечал.
            if (Offers && P.Offers.Num() < Offers->Num())
            {
                P.Offers = *Offers;
            }
            return;
        }
    }

    FKnownLocation New;
    New.Kind = Kind;
    New.Location = Location;
    New.Label = Label.IsEmpty() ? HumanText::Place(Kind) : Label;
    New.Familiarity = bFirsthand ? 0.35f : 0.12f; // чужим словам верится хуже
    New.bFirsthand = bFirsthand;
    New.ToldBy = ToldBy;
    New.LastVisitedAt = bFirsthand ? WorldTime : 0.0f;

    if (Offers)
    {
        New.Offers = *Offers;
    }
    Places.Add(New);

    // Карта в голове не безразмерна. Но забывается не что попало:
    // дорогу домой и дорогу на работу человек помнит, даже если давно
    // там не был, а вот сотня безымянных мест, где он однажды прошёл,
    // сливается в ничто. Без этой защиты случайные закоулки вытесняли
    // из памяти работу — и человек переставал понимать, куда ему идти.
    while (Places.Num() > 200)
    {
        int32 WeakestIndex = INDEX_NONE;
        float WeakestScore = FLT_MAX;

        for (int32 i = 0; i < Places.Num(); ++i)
        {
            if (Places[i].Kind == EPlaceKind::Home || Places[i].Kind == EPlaceKind::Work)
            {
                continue; // своё не забывается
            }

            // Место с предложениями ценнее пустой отметки на карте.
            const float Score = Places[i].Familiarity + Places[i].Offers.Num() * 0.25f
                              + FMath::Abs(Places[i].Affect) * 0.3f;
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
        Places.RemoveAt(WeakestIndex);
    }
}

bool UMemoryComponent::FindNearestPlace(EPlaceKind Kind, const FVector& From, FVector& OutLocation) const
{
    bool bFound = false;
    float BestScore = -FLT_MAX;

    for (const FKnownLocation& P : Places)
    {
        if (P.Kind != Kind)
        {
            continue;
        }

        const float Dist = FVector::Dist(From, P.Location);
        // Выбираем не просто ближайшее, а лучшее: знакомое и приятное место
        // человек предпочтёт незнакомому, даже если оно чуть дальше.
        const float Score = -Dist * 0.001f + P.Familiarity * 8.0f + P.Affect * 5.0f;

        if (Score > BestScore)
        {
            BestScore = Score;
            OutLocation = P.Location;
            bFound = true;
        }
    }
    return bFound;
}

bool UMemoryComponent::KnowsPlace(EPlaceKind Kind, const FVector& Location) const
{
    for (const FKnownLocation& P : Places)
    {
        if (P.Kind == Kind && FVector::DistSquared(P.Location, Location) < 250000.0f)
        {
            return true;
        }
    }
    return false;
}

TArray<FKnownLocation> UMemoryComponent::GetPlacesOfKind(EPlaceKind Kind) const
{
    TArray<FKnownLocation> Result;
    for (const FKnownLocation& P : Places)
    {
        if (P.Kind == Kind)
        {
            Result.Add(P);
        }
    }
    return Result;
}

void UMemoryComponent::VisitPlace(const FVector& Location, float WorldTime, float AffectDelta)
{
    for (FKnownLocation& P : Places)
    {
        if (FVector::DistSquared(P.Location, Location) < 640000.0f) // ~8 м
        {
            P.Familiarity = FMath::Clamp(P.Familiarity + 0.04f, 0.0f, 1.0f);
            P.LastVisitedAt = WorldTime;
            P.Affect = FMath::Clamp(P.Affect + AffectDelta * 0.3f, -1.0f, 1.0f);
            P.bFirsthand = true;
        }
    }
}

// ---------------------------------------------------------------------------
//  Выученный опыт
// ---------------------------------------------------------------------------

const FOutcomeAssociation* UMemoryComponent::FindOutcome(FName Key) const
{
    for (const FOutcomeAssociation& O : Outcomes)
    {
        if (O.Key == Key)
        {
            return &O;
        }
    }
    return nullptr;
}

float UMemoryComponent::GetExpectedOutcome(FName Key, float& OutConfidence) const
{
    if (const FOutcomeAssociation* O = FindOutcome(Key))
    {
        OutConfidence = O->Confidence;
        return O->ExpectedValue;
    }
    OutConfidence = 0.0f;
    return 0.0f;
}

float UMemoryComponent::GetExpectedOutcomeGeneralised(FName Key, FName CategoryKey, float& OutConfidence) const
{
    float InstanceConfidence = 0.0f;
    const float Instance = GetExpectedOutcome(Key, InstanceConfidence);

    float CategoryConfidence = 0.0f;
    const float Category = CategoryKey.IsNone()
        ? 0.0f
        : GetExpectedOutcome(CategoryKey, CategoryConfidence);

    // Ничего не знает ни про вещь, ни про вид.
    if (InstanceConfidence <= 0.0f && CategoryConfidence <= 0.0f)
    {
        OutConfidence = 0.0f;
        return 0.0f;
    }

    // Частное знание вытесняет общее по мере накопления: пока про это
    // кафе ничего не известно, человек судит по кафе вообще; сходив
    // сюда несколько раз, он судит уже об этом месте.
    const float InstanceWeight = InstanceConfidence;
    const float CategoryWeight = CategoryConfidence * (1.0f - InstanceConfidence);
    const float TotalWeight = InstanceWeight + CategoryWeight;

    if (TotalWeight <= KINDA_SMALL_NUMBER)
    {
        OutConfidence = 0.0f;
        return 0.0f;
    }

    OutConfidence = FMath::Clamp(FMath::Max(InstanceConfidence, CategoryConfidence * 0.75f), 0.0f, 0.95f);
    return (Instance * InstanceWeight + Category * CategoryWeight) / TotalWeight;
}

float UMemoryComponent::LearnOutcomeGeneralised(FName Key, FName CategoryKey, float ActualValue, float WorldTime)
{
    // Про эту вещь — учимся быстро.
    const float Error = LearnOutcome(Key, ActualValue, WorldTime, 0.35f);

    // Про вид вещей — медленно. Одного неудачного обеда мало, чтобы
    // разувериться во всех кафе сразу; но десяток таких обедов — достаточно.
    if (!CategoryKey.IsNone() && CategoryKey != Key)
    {
        LearnOutcome(CategoryKey, ActualValue, WorldTime, 0.12f);
    }

    return Error;
}

float UMemoryComponent::LearnOutcome(FName Key, float ActualValue, float WorldTime, float LearningRate)
{
    if (Key.IsNone())
    {
        return 0.0f;
    }

    const float Actual = FMath::Clamp(ActualValue, -1.0f, 1.0f);

    for (FOutcomeAssociation& O : Outcomes)
    {
        if (O.Key != Key)
        {
            continue;
        }

        // Ошибка предсказания — единственный источник обучения.
        const float Error = Actual - O.ExpectedValue;

        // Первые разы учат сильнее: когда опыта много, одно событие
        // уже не переворачивает мнение.
        const float Rate = LearningRate / (1.0f + O.Samples * 0.25f);
        O.ExpectedValue = FMath::Clamp(O.ExpectedValue + Error * Rate, -1.0f, 1.0f);

        O.Samples++;
        O.Confidence = FMath::Clamp(1.0f - FMath::Exp(-O.Samples / 4.0f), 0.0f, 0.95f);
        O.LastAt = WorldTime;

        // Приелось. Даже самое приятное занятие после повторения тянет слабее.
        O.Satiation = FMath::Clamp(O.Satiation + 0.30f, 0.0f, 1.0f);
        return Error;
    }

    // Первый раз: ожидания не было, значит и ошибка — это всё, что случилось.
    FOutcomeAssociation New;
    New.Key = Key;
    New.ExpectedValue = Actual;
    New.Samples = 1;
    New.Confidence = 0.22f;
    New.LastAt = WorldTime;
    New.Satiation = 0.30f;
    Outcomes.Add(New);

    while (Outcomes.Num() > OutcomeCapacity)
    {
        // Забывается то, что слабее всего отложилось и давно не проверялось.
        int32 WeakestIndex = 0;
        float WeakestScore = FLT_MAX;
        for (int32 i = 0; i < Outcomes.Num(); ++i)
        {
            const float Score = FMath::Abs(Outcomes[i].ExpectedValue) * Outcomes[i].Confidence;
            if (Score < WeakestScore)
            {
                WeakestScore = Score;
                WeakestIndex = i;
            }
        }
        Outcomes.RemoveAt(WeakestIndex);
    }

    return Actual;
}

void UMemoryComponent::NameConclusion(FName Key, const FString& Text)
{
    for (FOutcomeAssociation& O : Outcomes)
    {
        if (O.Key == Key)
        {
            O.Conclusion = Text;
            return;
        }
    }
}

const FOutcomeAssociation* UMemoryComponent::GetStrongestOutcome(bool bPositive) const
{
    const FOutcomeAssociation* Best = nullptr;
    float BestScore = 0.25f;

    for (const FOutcomeAssociation& O : Outcomes)
    {
        const float Score = (bPositive ? O.ExpectedValue : -O.ExpectedValue) * O.Confidence;
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = &O;
        }
    }
    return Best;
}

// ---------------------------------------------------------------------------
//  Дороги
// ---------------------------------------------------------------------------

void UMemoryComponent::LearnRoute(const FVector& From, const FVector& To, const TArray<FVector>& Trail,
                                  float TravelTime, float WorldTime)
{
    if (Trail.Num() < 2)
    {
        return;   // два шага дорогой не считаются
    }

    // Уже ходил этим путём — не заводим второй такой же, а укрепляем.
    for (FRouteMemory& R : Routes)
    {
        if (FVector::Dist2D(R.From, From) < 900.0f && FVector::Dist2D(R.To, To) < 600.0f)
        {
            R.UseCount++;
            R.LastUsedAt = WorldTime;
            R.Strength = FMath::Clamp(R.Strength + 0.18f, 0.0f, 1.0f);

            // Если вышло быстрее прежнего — значит, нашёл дорогу короче,
            // и запоминается теперь она.
            if (TravelTime < R.TravelTime * 0.85f)
            {
                R.Waypoints = Trail;
                R.TravelTime = TravelTime;
            }
            return;
        }
    }

    FRouteMemory New;
    New.From = From;
    New.To = To;
    New.Waypoints = Trail;
    New.TravelTime = TravelTime;
    New.Strength = 0.45f;
    New.UseCount = 1;
    New.LastUsedAt = WorldTime;
    Routes.Add(New);

    // Дорог в голове тоже не бесконечно: забывается та, которой не ходят.
    while (Routes.Num() > RouteCapacity)
    {
        int32 Weakest = 0;
        for (int32 i = 1; i < Routes.Num(); ++i)
        {
            if (Routes[i].Strength * Routes[i].UseCount < Routes[Weakest].Strength * Routes[Weakest].UseCount)
            {
                Weakest = i;
            }
        }
        Routes.RemoveAt(Weakest);
    }
}

bool UMemoryComponent::RecallRoute(const FVector& From, const FVector& To, float WorldTime,
                                   TArray<FVector>& OutWaypoints)
{
    OutWaypoints.Reset();

    // --- Дорога именно туда -------------------------------------------------
    FRouteMemory* Best = nullptr;
    float BestScore = 0.0f;

    for (FRouteMemory& R : Routes)
    {
        if (R.Strength < 0.15f)
        {
            continue;
        }
        const float StartGap = FVector::Dist2D(R.From, From);
        const float EndGap = FVector::Dist2D(R.To, To);
        if (StartGap > 1400.0f || EndGap > 700.0f)
        {
            continue;
        }

        const float Score = R.Strength * (1.0f - StartGap / 2000.0f) * (1.0f - EndGap / 1000.0f);
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = &R;
        }
    }

    if (Best)
    {
        Best->UseCount++;
        Best->LastUsedAt = WorldTime;
        Best->Strength = FMath::Clamp(Best->Strength + 0.05f, 0.0f, 1.0f);
        OutWaypoints = Best->Waypoints;
        return true;
    }

    // --- Чужая дорога, проходящая рядом ------------------------------------
    // Знакомого пути именно туда нет. Но если какая-то из хоженых дорог
    // проходит близко к нужному месту, человек пойдёт по ней и свернёт
    // в конце. Это и есть срезание: маршрут, которым он никогда не ходил,
    // собранный из кусков тех, которыми ходил.
    for (FRouteMemory& R : Routes)
    {
        if (R.Strength < 0.25f || FVector::Dist2D(R.From, From) > 1600.0f)
        {
            continue;
        }

        int32 ClosestIndex = INDEX_NONE;
        float ClosestGap = 1600.0f;
        for (int32 i = 0; i < R.Waypoints.Num(); ++i)
        {
            const float Gap = FVector::Dist2D(R.Waypoints[i], To);
            if (Gap < ClosestGap)
            {
                ClosestGap = Gap;
                ClosestIndex = i;
            }
        }

        // Срезать имеет смысл, только если так действительно ближе.
        if (ClosestIndex != INDEX_NONE && ClosestGap < FVector::Dist2D(From, To) * 0.7f)
        {
            for (int32 i = 0; i <= ClosestIndex; ++i)
            {
                OutWaypoints.Add(R.Waypoints[i]);
            }
            return OutWaypoints.Num() > 0;
        }
    }

    return false;
}

float UMemoryComponent::GetRouteFamiliarity(const FVector& From, const FVector& To) const
{
    float Best = 0.0f;
    for (const FRouteMemory& R : Routes)
    {
        if (FVector::Dist2D(R.From, From) < 1400.0f && FVector::Dist2D(R.To, To) < 700.0f)
        {
            Best = FMath::Max(Best, R.Strength);
        }
    }
    return Best;
}

// ---------------------------------------------------------------------------
//  Привычки
// ---------------------------------------------------------------------------

void UMemoryComponent::ReinforceHabit(FName Context, EActionType Action, float WorldTime)
{
    for (FHabit& H : Habits)
    {
        if (H.Context == Context && H.Action == Action)
        {
            H.Repetitions++;
            H.LastFiredAt = WorldTime;
            // Кривая формирования привычки: первые повторения дают много,
            // дальше — всё меньше. Выходит на плато примерно к 60 разам.
            H.Strength = FMath::Clamp(1.0f - FMath::Exp(-H.Repetitions / 22.0f), 0.0f, 0.95f);
            return;
        }
    }

    FHabit New;
    New.Context = Context;
    New.Action = Action;
    New.Repetitions = 1;
    New.Strength = 0.05f;
    New.LastFiredAt = WorldTime;
    Habits.Add(New);
}

bool UMemoryComponent::GetHabitualAction(FName Context, EActionType& OutAction, float& OutStrength) const
{
    const FHabit* Best = nullptr;
    for (const FHabit& H : Habits)
    {
        if (H.Context == Context && (!Best || H.Strength > Best->Strength))
        {
            Best = &H;
        }
    }

    if (!Best || Best->Strength < 0.3f)
    {
        return false;
    }

    OutAction = Best->Action;
    OutStrength = Best->Strength;
    return true;
}

void UMemoryComponent::WeakenHabit(FName Context, float Amount)
{
    for (FHabit& H : Habits)
    {
        if (H.Context == Context)
        {
            H.Strength = FMath::Max(0.0f, H.Strength - Amount);
        }
    }
}

// ---------------------------------------------------------------------------
//  Прочее
// ---------------------------------------------------------------------------

void UMemoryComponent::ApplyAging(float Age)
{
    if (Age < 55.0f)
    {
        return;
    }
    // После пятидесяти пяти новые следы ложатся хуже, старые держатся.
    const float Decline = FMath::Clamp((Age - 55.0f) / 45.0f, 0.0f, 0.6f);
    MemoryQuality = FMath::Clamp(1.0f - Decline, 0.3f, 1.0f);
}

FString UMemoryComponent::SummarizeLife() const
{
    TArray<FEpisodicMemory> Sorted = Episodes;
    Sorted.Sort([](const FEpisodicMemory& A, const FEpisodicMemory& B)
    {
        return A.Strength * (0.5f + A.Arousal) > B.Strength * (0.5f + B.Arousal);
    });

    FString Result;
    const int32 Num = FMath::Min(3, Sorted.Num());
    for (int32 i = 0; i < Num; ++i)
    {
        if (i > 0)
        {
            Result += TEXT("; ");
        }
        Result += Sorted[i].Summary;
    }
    return Result.IsEmpty() ? TEXT("ничего толком не помню") : Result;
}
