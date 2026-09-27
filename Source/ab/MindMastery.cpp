#include "MindComponent.h"
#include "CompleteHumanAI.h"
#include "MemoryComponent.h"
#include "PhysiologyComponent.h"
#include "Crafts.h"

namespace
{
    struct FDeedName
    {
        const TCHAR* Key;
        const TCHAR* Label;
    };

    const FDeedName DeedNames[] = {
        { TEXT("Reading"),     TEXT("чтение") },
        { TEXT("Writing"),     TEXT("письмо") },
        { TEXT("Counting"),    TEXT("счёт") },
        { TEXT("Arithmetic"),  TEXT("счёт в уме") },
        { TEXT("Teaching"),    TEXT("объяснять") },
        { TEXT("Empathy"),     TEXT("понимать людей") },
        { TEXT("Deception"),   TEXT("говорить неправду") },
        { TEXT("Persuasion"),  TEXT("уговаривать") },
        { TEXT("Navigation"),  TEXT("находить дорогу") },
        { TEXT("Cooking"),     TEXT("стряпня") },
        { TEXT("Baking"),      TEXT("печь хлеб") },
        { TEXT("Cleaning"),    TEXT("прибираться") },
        { TEXT("Farming"),     TEXT("земледелие") },
        { TEXT("Woodcutting"), TEXT("рубить лес") },
        { TEXT("Carpentry"),   TEXT("плотничать") },
        { TEXT("Building"),    TEXT("строить") },
        { TEXT("Smithing"),    TEXT("ковать") },
        { TEXT("Masonry"),     TEXT("класть камень") },
        { TEXT("Pottery"),     TEXT("лепить") },
        { TEXT("Weaving"),     TEXT("ткать") },
        { TEXT("Sewing"),      TEXT("шить") },
        { TEXT("Tanning"),     TEXT("выделывать кожу") },
        { TEXT("Fishing"),     TEXT("ловить рыбу") },
        { TEXT("Beekeeping"),  TEXT("держать пчёл") },
        { TEXT("Herbalism"),   TEXT("знать травы") },
        { TEXT("Medicine"),    TEXT("лечить") },
        { TEXT("Soapmaking"),  TEXT("варить мыло") },
        { TEXT("Papermaking"), TEXT("делать бумагу") },
        { TEXT("Glassmaking"), TEXT("варить стекло") },
        { TEXT("Crafting"),    TEXT("работа руками") },
        { TEXT("Strength"),    TEXT("сила") },
        { TEXT("Patience"),    TEXT("терпение") },
        { TEXT("Music"),       TEXT("играть") },
        { TEXT("Drawing"),     TEXT("рисовать") },
        { TEXT("Trade"),       TEXT("торговать") }
    };
}

FString UMindComponent::MasteryLabel(FName What)
{
    if (What.IsNone())
    {
        return FString();
    }

    const FString Key = What.ToString();
    for (const FDeedName& Entry : DeedNames)
    {
        if (Key.Equals(Entry.Key, ESearchCase::IgnoreCase))
        {
            return Entry.Label;
        }
    }

    if (const FCraft* Deed = FCraftBook::Find(What))
    {
        return Deed->Label;
    }

    return Key;
}

float UMindComponent::Mastery(FName What) const
{
    if (!Memory || What.IsNone())
    {
        return 0.0f;
    }
    return Memory->GetBeliefConfidence(What, TEXT("HowTo"));
}

void UMindComponent::Practised(FName What, float Amount)
{
    if (!Memory || What.IsNone() || Amount <= 0.0f)
    {
        return;
    }

    const float Had = Mastery(What);
    const float Room = 1.0f - Had;

    FBelief Done;
    Done.Subject = What;
    Done.Predicate = TEXT("HowTo");
    Done.Value = 1.0f;
    Done.Confidence = FMath::Clamp(Had + Amount * Room, 0.0f, 1.0f);
    Done.LearnedAt = Now();
    Done.bVerified = true;
    Done.Text = MasteryLabel(What);
    Memory->Learn(Done, 1.0f, 0.8f, Done.LearnedAt);
}

void UMindComponent::SawSomeoneDo(FName What, float TheirMastery, float Attention)
{
    if (!Memory || What.IsNone())
    {
        return;
    }

    const float Had = Mastery(What);
    const float Ceiling = FMath::Clamp(TheirMastery * 0.8f, 0.0f, 0.8f);
    if (Had >= Ceiling)
    {
        return;
    }

    FBelief Watched;
    Watched.Subject = What;
    Watched.Predicate = TEXT("HowTo");
    Watched.Value = 1.0f;
    Watched.Confidence = FMath::Min(Ceiling, Had + 0.1f * FMath::Clamp(Attention, 0.0f, 1.0f));
    Watched.LearnedAt = Now();
    Watched.Text = MasteryLabel(What);
    Memory->Learn(Watched, 0.8f, 0.7f, Watched.LearnedAt);
}

bool UMindComponent::TryDo(FName What, float Difficulty, float& OutChance) const
{
    const float Knows = Mastery(What);
    const float Impairment = Body ? Body->GetCognitiveImpairment() : 0.0f;

    OutChance = FMath::Clamp(0.08f + Knows * 0.85f - Difficulty * 0.4f - Impairment * 0.3f, 0.02f, 0.96f);
    return FMath::FRand() < OutChance;
}

FName UMindComponent::BestMastery(float& OutLevel) const
{
    OutLevel = 0.0f;
    FName Best;

    if (!Memory)
    {
        return Best;
    }

    for (const FBelief& B : Memory->Beliefs)
    {
        if (B.Predicate != FName(TEXT("HowTo")))
        {
            continue;
        }
        if (B.Confidence > OutLevel)
        {
            OutLevel = B.Confidence;
            Best = B.Subject;
        }
    }

    return Best;
}

FName UMindComponent::TryInvent(float Curiosity, float Openness, FString& OutInsight)
{
    OutInsight.Reset();
    if (!Memory)
    {
        return NAME_None;
    }

    if (FMath::FRand() > 0.0006f * (0.4f + Curiosity + Openness))
    {
        return NAME_None;
    }

    TArray<FName> Known;
    for (const FBelief& B : Memory->Beliefs)
    {
        if (B.Predicate == FName(TEXT("HowTo")) && B.Confidence > 0.55f)
        {
            Known.Add(B.Subject);
        }
    }

    if (Known.Num() < 2)
    {
        return NAME_None;
    }

    const FName First = Known[FMath::RandRange(0, Known.Num() - 1)];
    FName Second = Known[FMath::RandRange(0, Known.Num() - 1)];
    if (First == Second)
    {
        return NAME_None;
    }

    const FName Made = FName(*FString::Printf(TEXT("%s+%s"), *First.ToString(), *Second.ToString()));
    if (Mastery(Made) > 0.0f)
    {
        return NAME_None;
    }

    OutInsight = FString::Printf(TEXT("а если %s и %s делать заодно?"),
        *MasteryLabel(First), *MasteryLabel(Second));

    FBelief Idea;
    Idea.Subject = Made;
    Idea.Predicate = TEXT("HowTo");
    Idea.Value = 1.0f;
    Idea.Confidence = 0.4f;
    Idea.LearnedAt = Now();
    Idea.Text = OutInsight;
    Memory->Learn(Idea, 1.0f, Openness, Idea.LearnedAt);

    return Made;
}

float UMindComponent::Competence() const
{
    if (!Memory)
    {
        return 0.2f;
    }

    float Sum = 0.0f;
    int32 Count = 0;
    for (const FBelief& B : Memory->Beliefs)
    {
        if (B.Predicate == FName(TEXT("HowTo")))
        {
            Sum += B.Confidence;
            ++Count;
        }
    }

    if (Count == 0)
    {
        return 0.15f;
    }

    const float Average = Sum / static_cast<float>(Count);
    const float Breadth = FMath::Clamp(Count / 20.0f, 0.0f, 1.0f);
    return FMath::Clamp(Average * 0.7f + Breadth * 0.3f, 0.0f, 1.0f);
}
