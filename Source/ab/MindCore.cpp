#include "MindComponent.h"
#include "CompleteHumanAI.h"
#include "NeedComponent.h"
#include "PhysiologyComponent.h"
#include "EmotionComponent.h"
#include "IdentityComponent.h"
#include "SocialComponent.h"
#include "PersonalityComponent.h"
#include "HumanWorldSubsystem.h"
#include "Village.h"
#include "Misc/Paths.h"

namespace
{
    const TCHAR* NeedWord(int32 Need)
    {
        switch (static_cast<ENeedType>(Need))
        {
        case ENeedType::Hunger:        return TEXT("голод");
        case ENeedType::Thirst:        return TEXT("жажда");
        case ENeedType::Sleep:         return TEXT("сон");
        case ENeedType::Bladder:       return TEXT("нужда");
        case ENeedType::Hygiene:       return TEXT("чистота");
        case ENeedType::Comfort:       return TEXT("покой");
        case ENeedType::Safety:        return TEXT("безопасность");
        case ENeedType::Health:        return TEXT("здоровье");
        case ENeedType::Shelter:       return TEXT("крыша");
        case ENeedType::Money:         return TEXT("деньги");
        case ENeedType::Order:         return TEXT("порядок");
        case ENeedType::SocialContact: return TEXT("люди");
        case ENeedType::Belonging:     return TEXT("свои");
        case ENeedType::Intimacy:      return TEXT("близость");
        case ENeedType::Esteem:        return TEXT("уважение");
        case ENeedType::Achievement:   return TEXT("дело");
        case ENeedType::Autonomy:      return TEXT("воля");
        case ENeedType::Competence:    return TEXT("умение");
        case ENeedType::Novelty:       return TEXT("новое");
        case ENeedType::Beauty:        return TEXT("красота");
        case ENeedType::Meaning:       return TEXT("смысл");
        default:                       return TEXT("?");
        }
    }
}

FString UMindComponent::CorePath() const
{
    FString Name = Identity ? Identity->GetFullName() : GetOwner()->GetName();
    Name = FPaths::MakeValidFileName(Name.Replace(TEXT(" "), TEXT("_")));
    return FLifeSchool::Folder() / TEXT("People") / (Name + TEXT(".core"));
}

void UMindComponent::WakeCore()
{
    bCoreReady = false;
    MindRng.Initialize(static_cast<int32>(GetTypeHash(GetOwner()->GetFName()) ^ 0x6a09e667u));
    if (Core.Load(CorePath()))
    {
        bCoreReady = true;
        return;
    }
    const float Age = Identity ? Identity->Age : 30.0f;
    const bool bFemale = Identity && Identity->bFemale;
    int32 Kind = FMotorSchool::KindOf(Age, bFemale);
    while (Kind >= 0 && !bCoreReady)
    {
        bCoreReady = Core.Load(FLifeSchool::CorePath(Kind));
        Kind = FMotorSchool::ParentKind(Kind);
    }
    if (!bCoreReady)
    {
        bCoreReady = Core.Load(FLifeSchool::CorePath(4));
    }
}

FSelfState UMindComponent::SenseSelf() const
{
    FSelfState Self;
    if (Needs)
    {
        for (int32 N = 0; N < FMindSense::NeedCount; ++N)
        {
            const ENeedType Type = static_cast<ENeedType>(N);
            Self.Needs[N] = Needs->GetSatisfaction(Type);
            const FNeedState* State = Needs->Find(Type);
            Self.Weights[N] = State ? State->Weight : 1.0f;
        }
    }
    if (Body)
    {
        Self.Stamina = Body->Body.Stamina;
        Self.Pain = Body->Body.Pain;
        Self.Health = Body->Body.Health;
        Self.Impairment = Body->GetCognitiveImpairment();
    }
    if (Emotions)
    {
        const FAffectPAD Affect = Emotions->GetAffect();
        Self.Pleasure = Affect.Pleasure;
        Self.Arousal = Affect.Arousal;
        Self.Fear = Emotions->GetIntensity(EEmotionType::Fear);
        Self.Anger = Emotions->GetIntensity(EEmotionType::Anger);
        Self.Sadness = Emotions->GetIntensity(EEmotionType::Sadness);
        Self.Joy = Emotions->GetIntensity(EEmotionType::Joy);
        Self.Boredom = Emotions->GetIntensity(EEmotionType::Boredom);
    }
    if (Identity)
    {
        Self.Money = Identity->Money;
        Self.Age = Identity->Age;
        Self.bFemale = Identity->bFemale;
        Self.bEmployed = Identity->bEmployed;
    }
    if (const UHumanWorldSubsystem* World = GetWorldMind())
    {
        Self.Hour = World->Now.HourFloat;
    }
    Self.DayOfYear = FVillage::DayOfYear(this);
    if (const ACompleteHumanNPC* Human = GetHuman())
    {
        if (FVillage::IsMedieval(this))
        {
            Self.Stores = FVillage::HouseholdFood(Human);
            Self.Firewood = FVillage::HouseholdFirewood(Human);
            Self.Wares = FVillage::CarriedWares(Human);
        }
        Self.bHolding = Human->CarriedItem != nullptr;
        Self.bAtHome = Human->IsAtHome();
        if (const UHumanWorldSubsystem* World = GetWorldMind())
        {
            Self.PeopleNearby = World->GetHumansNear(Human->GetActorLocation(), 800.0f, Human).Num();
        }
    }
    return Self;
}

FOptionView UMindComponent::ViewOf(const FAffordance& A) const
{
    FOptionView View;
    View.Action = A.Action;
    View.Source = A.Source;
    View.Category = FMindSense::Word(A.CategoryKey.IsNone() ? A.Key.ToString() : A.CategoryKey.ToString());
    View.Place = FMindSense::Word(A.Key.ToString());
    if (const ACompleteHumanNPC* Human = GetHuman())
    {
        View.DistanceM = A.bHasLocation ? FVector::Dist2D(Human->GetActorLocation(), A.Location) / 100.0f : 0.0f;
    }
    View.DurationMin = A.Duration > 0.0f ? A.Duration / 60.0f : 420.0f;
    View.MoneyCost = A.MoneyCost;
    View.MoneyPerHour = A.MoneyGainPerHour;
    View.Effort = A.EffortCost;
    View.Risk = A.Risk;
    View.Difficulty = A.Difficulty;
    View.Mastery = A.RequiredSkill.IsNone() ? 1.0f : Mastery(A.RequiredSkill);
    if (A.Source == EAffordanceSource::Person && A.Target && Social)
    {
        View.Closeness = Social->GetCloseness(A.Target);
        if (const FRelationship* R = Social->Find(A.Target))
        {
            View.Liking = R->Liking;
            View.Resentment = R->Resentment;
            View.Fear = R->Fear;
        }
    }
    const int32* Tried = TriedKinds.Find(View.Category);
    View.TimesTried = Tried ? *Tried : 0;
    const int32* Here = TriedPlaces.Find(View.Place);
    View.TimesHere = Here ? *Here : 0;
    for (const uint32 Recent : RecentKinds)
    {
        View.RecentRepeats += Recent == View.Category ? 1 : 0;
    }
    if (A.Requires != EResourceKind::None)
    {
        View.HaveRequired = HaveWhatItTakes(A) ? 1 : -1;
    }
    View.bProduces = A.Produces != EResourceKind::None;
    View.bInHand = A.bNeedsInHand;
    View.bSeat = A.bNeedsSeat;
    View.bLying = A.bNeedsLying || A.Action == EActionType::Sleep;
    View.bMine = A.bPrivate;
    View.bCraft = !A.Craft.IsNone();
    View.bOpenNow = true;
    return View;
}

bool UMindComponent::DecideByCore(const TArray<FAffordance>& Offered, FIntention& Out, float* OutOngoing)
{
    if (!bCoreReady || Offered.Num() == 0)
    {
        return false;
    }
    TArray<FAffordance> Available;
    Available.Reserve(Offered.Num());
    for (const FAffordance& A : Offered)
    {
        if (CouldDo(A))
        {
            Available.Add(A);
        }
    }
    if (Available.Num() == 0)
    {
        Available = Offered;
    }
    const FSelfState Self = SenseSelf();
    TArray<FOptionView> Views;
    Views.Reserve(Available.Num());
    for (const FAffordance& A : Available)
    {
        Views.Add(ViewOf(A));
    }
    const float Openness = Personality ? Personality->Traits.Openness : 0.5f;
    const float Anxiety = Personality ? Personality->Facets.TraitAnxiety : 0.4f;
    const float Impairment = Body ? Body->GetCognitiveImpairment() : 0.0f;
    const FMindChoice Choice = Core.Choose(Self, Views, Openness, Anxiety, Impairment, MindRng, nullptr);
    if (!Available.IsValidIndex(Choice.Index))
    {
        return false;
    }
    if (OutOngoing && ActiveIntention.bValid)
    {
        *OutOngoing = Core.Weigh(Self, ViewOf(ActiveIntention.Affordance), Openness, Anxiety).Score;
    }

    int32 Best = INDEX_NONE;
    float Relief = 0.0f;
    int32 Worst = INDEX_NONE;
    float Loss = 0.0f;
    for (int32 N = 0; N < FMindSense::NeedCount; ++N)
    {
        const float Change = Choice.Expect.IsValidIndex(N) ? Choice.Expect[N] / 3.0f * Self.Weights[N] : 0.0f;
        if (Change > Relief)
        {
            Relief = Change;
            Best = N;
        }
        if (Change < Loss)
        {
            Loss = Change;
            Worst = N;
        }
    }

    Out = FIntention();
    Out.bValid = true;
    Out.Affordance = Available[Choice.Index];
    Out.Valuation.Total = Choice.Score;
    Out.Valuation.NeedGain = FMath::Clamp(Relief * 2.0f, 0.0f, 1.6f);
    Out.Valuation.Experience = Choice.Future;
    Out.Valuation.Curiosity = Choice.Wonder;
    Out.Expectation = FMath::Clamp(Choice.Expect.IsValidIndex(FMindSense::PleasureAt) ? Choice.Expect[FMindSense::PleasureAt] : 0.0f, -1.0f, 1.0f);
    Out.DecidedAt = Now();
    FString Why;
    if (Choice.Wonder > FMath::Max(0.05f, Relief * 0.5f))
    {
        Why = TEXT("не знаю, что выйдет, - посмотрим");
    }
    else if (Best != INDEX_NONE)
    {
        Why = FString::Printf(TEXT("станет лучше: %s"), NeedWord(Best));
    }
    else
    {
        Why = TEXT("так будет лучше потом");
    }
    if (Worst != INDEX_NONE && Loss < -0.08f)
    {
        Why += FString::Printf(TEXT(", хоть и %s потерпит"), NeedWord(Worst));
    }
    Out.Valuation.DominantReason = Why;
    Out.Reason = Why;
    LastChoiceNote = FString::Printf(TEXT("%s | ожидаю %.1f ч, удача %.0f%%, сейчас %.2f, потом %.2f, любопытство %.2f"),
        *Out.Affordance.Label, Choice.Hours, Choice.Success * 100.0f, Choice.Gain, Choice.Future, Choice.Wonder);
    return true;
}

void UMindComponent::LearnFromOutcome(bool bSucceeded)
{
    if (!bCoreReady || !bDecisionOpen)
    {
        return;
    }
    bDecisionOpen = false;
    const FSelfState After = SenseSelf();
    const float Hours = FMath::Max(1.0f / 60.0f, (Now() - DecisionAt) / 3600.0f);
    const ACompleteHumanNPC* Human = GetHuman();
    const bool bDied = Human && !Human->IsAlive();
    Core.Remember(DecisionSelf, DecisionView, After, Hours, bSucceeded, 1.0f, bDied);
    Core.Practice(3, MindRng);
    TriedKinds.FindOrAdd(DecisionView.Category) += 1;
    TriedPlaces.FindOrAdd(DecisionView.Place) += 1;
    RecentKinds.Add(DecisionView.Category);
    if (RecentKinds.Num() > 6)
    {
        RecentKinds.RemoveAt(0);
    }
    if (++OutcomesSinceSave >= 40)
    {
        OutcomesSinceSave = 0;
        Core.Save(CorePath());
    }
}

FString UMindComponent::DescribeCore() const
{
    if (!bCoreReady)
    {
        return TEXT("ум по старым правилам");
    }
    return FString::Printf(TEXT("прожито решений %lld, уроков %lld, в памяти %d, ошибка предсказаний %.3f, своих фраз %d, выученных реплик %d%s | %s"),
        Core.Lived, Core.Lessons, Core.Memories(), Core.ModelError, TongueSpoken, TalkLearned,
        bTalkReady ? TEXT("") : TEXT(" (разговор по старым правилам)"), *LastChoiceNote);
}

void UMindComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bCoreReady && Core.Lived > 0)
    {
        Core.Save(CorePath());
    }
    if (bTalkReady && TalkLearned > 0)
    {
        TalkMind.Save(TalkPath());
    }
    Super::EndPlay(Reason);
}
