// MindDialogue.cpp
// ---------------------------------------------------------------------------
// Разговор как обмен ходами.
//
// Каждая реплика чего-то ждёт: на вопрос отвечают, на приветствие
// здороваются, на совет благодарят, на прощание прощаются. Отвечает человек
// тем, что есть у него самого: местами из своей памяти, прочитанной
// страницей, собственной нуждой. Чего он не знает — того и не скажет.
// ---------------------------------------------------------------------------

#include "MindComponent.h"
#include "CompleteHumanAI.h"
#include "PersonalityComponent.h"
#include "NeedComponent.h"
#include "EmotionComponent.h"
#include "MemoryComponent.h"
#include "IdentityComponent.h"
#include "SocialComponent.h"
#include "SpeechComponent.h"
#include "HumanWorldSubsystem.h"
#include "TongueSubsystem.h"
#include "TalkLearning.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    ETalkChoice ChoiceOf(uint8 Stored, EDialogueMove Move)
    {
        if (Stored < static_cast<uint8>(ETalkChoice::Count))
        {
            return static_cast<ETalkChoice>(Stored);
        }
        switch (Move)
        {
        case EDialogueMove::Greet:       return ETalkChoice::GreetBack;
        case EDialogueMove::Introduce:   return ETalkChoice::IntroduceBack;
        case EDialogueMove::HowAreYou:   return ETalkChoice::AskHow;
        case EDialogueMove::StateOfSelf: return ETalkChoice::TellMood;
        case EDialogueMove::Tell:        return ETalkChoice::TellWeather;
        case EDialogueMove::Ask:         return ETalkChoice::AskWhere;
        case EDialogueMove::Answer:      return ETalkChoice::AnswerWhere;
        case EDialogueMove::Advise:      return ETalkChoice::AdvisePlace;
        case EDialogueMove::AskAdvice:   return ETalkChoice::AskAdvice;
        case EDialogueMove::Thank:       return ETalkChoice::Thanks;
        case EDialogueMove::Offer:       return ETalkChoice::OfferMoney;
        case EDialogueMove::Decline:     return ETalkChoice::RefuseTeach;
        case EDialogueMove::Console:     return ETalkChoice::Console;
        case EDialogueMove::Instruct:    return ETalkChoice::Instruct;
        case EDialogueMove::Obey:        return ETalkChoice::Obey;
        case EDialogueMove::Farewell:    return ETalkChoice::Farewell;
        default:                         return ETalkChoice::Acknowledge;
        }
    }

    FString WithNamesCapital(const FString& Said, const TSet<FString>& Names)
    {
        TArray<FString> Parts;
        Said.ParseIntoArray(Parts, TEXT(" "), true);
        for (FString& Part : Parts)
        {
            const FName Key = USpeechComponent::NormalizeWord(Part);
            if (!Key.IsNone() && Names.Contains(Key.ToString()))
            {
                Part[0] = UTongueSubsystem::Upper(Part[0]);
            }
        }
        return FString::Join(Parts, TEXT(" "));
    }

    double RealClock(const UObject* Owner)
    {
        const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
        return World ? World->GetTimeSeconds() : 0.0;
    }

    /** Сколько читать чужую реплику, прежде чем ответить. */
    double ReadingPause(const FString& Text)
    {
        return FMath::Clamp(1.4 + Text.Len() * 0.04, 1.8, 5.5);
    }

    FString TopicId(const FTalkingPoint& P)
    {
        return FString::Printf(TEXT("%d:%s"), int32(P.Kind), *P.About);
    }

    /** Дело, от которого ради болтовни не отрываются. */
    bool IsAbsorbing(EActionType A)
    {
        switch (A)
        {
        case EActionType::Work:  case EActionType::Sleep: case EActionType::Read:
        case EActionType::Study: case EActionType::Flee:  case EActionType::Fight:
        case EActionType::Hide:  case EActionType::UseToilet:
            return true;
        default:
            return false;
        }
    }

    ESpeechAct ActFor(EDialogueMove Move, const FTalkingPoint& Point)
    {
        switch (Move)
        {
        case EDialogueMove::Greet:
        case EDialogueMove::Introduce:   return ESpeechAct::Greet;
        case EDialogueMove::HowAreYou:
        case EDialogueMove::Ask:         return ESpeechAct::Question;
        case EDialogueMove::StateOfSelf:
        case EDialogueMove::Answer:      return ESpeechAct::Answer;
        case EDialogueMove::Tell:        return Point.Valence < -0.3f ? ESpeechAct::Complain : ESpeechAct::ShareNews;
        case EDialogueMove::Console:     return ESpeechAct::Console;
        case EDialogueMove::Advise:
        case EDialogueMove::Instruct:    return ESpeechAct::Advise;
        case EDialogueMove::AskAdvice:   return ESpeechAct::Request;
        case EDialogueMove::Thank:       return ESpeechAct::Thank;
        case EDialogueMove::Offer:       return ESpeechAct::Offer;
        case EDialogueMove::Decline:     return ESpeechAct::Refuse;
        case EDialogueMove::Farewell:    return ESpeechAct::Farewell;
        default:                         return ESpeechAct::Agree;
        }
    }

    /** Имена — не слова из книги: своё и знакомых человек произносит и так. */
    TSet<FString> NamesKnownTo(const UIdentityComponent* Id, const USocialComponent* Social)
    {
        TSet<FString> Names;
        auto Add = [&Names](const FString& Name)
        {
            const FName Key = USpeechComponent::NormalizeWord(Name);
            if (!Key.IsNone())
            {
                Names.Add(Key.ToString());
            }
        };
        if (Id)
        {
            Add(Id->FirstName);
        }
        if (Social)
        {
            for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Social->Relations)
            {
                Add(Pair.Value.KnownName);
            }
        }
        return Names;
    }

    /** О телесном и житейском говорят кому угодно, о сокровенном — только своим. */
    bool IsPractical(ENeedType Need)
    {
        switch (Need)
        {
        case ENeedType::Hunger: case ENeedType::Thirst: case ENeedType::Sleep: case ENeedType::Hygiene:
        case ENeedType::Comfort: case ENeedType::Health: case ENeedType::Money: case ENeedType::Shelter:
            return true;
        default:
            return false;
        }
    }

    /** Подходит ли такое место к такой беде. Советовать невпопад люди не станут. */
    bool PlaceSuits(ENeedType Need, EPlaceKind Kind)
    {
        switch (Need)
        {
        case ENeedType::Hunger:
        case ENeedType::Thirst:
            return Kind == EPlaceKind::Food || Kind == EPlaceKind::Bakery
                || Kind == EPlaceKind::Market || Kind == EPlaceKind::Shop;
        case ENeedType::Hygiene:       return Kind == EPlaceKind::Bathhouse;
        case ENeedType::Health:        return Kind == EPlaceKind::Hospital;
        case ENeedType::Money:         return Kind != EPlaceKind::Home && Kind != EPlaceKind::Danger;
        case ENeedType::Competence:
        case ENeedType::Novelty:       return Kind == EPlaceKind::Library || Kind == EPlaceKind::Study;
        case ENeedType::Comfort:
        case ENeedType::Beauty:        return Kind == EPlaceKind::Rest || Kind == EPlaceKind::Beautiful;
        case ENeedType::SocialContact:
        case ENeedType::Belonging:     return Kind == EPlaceKind::Social || Kind == EPlaceKind::Food;
        default:                       return false;
        }
    }

    /** Ход, который понятен и без слов: кивок, взмах рукой, протянутая рука. */
    bool IsGesturable(EDialogueMove Move)
    {
        return Move == EDialogueMove::Greet || Move == EDialogueMove::Introduce
            || Move == EDialogueMove::Farewell || Move == EDialogueMove::Thank;
    }

    /** Предложение места, подходящее под совет. */
    const FAffordance* OfferFor(const FKnownLocation& Place, ENeedType Need, EActionType Action)
    {
        const FAffordance* Fallback = nullptr;
        for (const FAffordance& O : Place.Offers)
        {
            if (O.bPrivate)
            {
                continue;
            }
            if (Action != EActionType::Idle && O.Action == Action)
            {
                return &O;
            }
            for (const FNeedPromise& P : O.Promises)
            {
                if (P.Need == Need && P.Amount > 0.1f)
                {
                    Fallback = &O;
                }
            }
        }
        return Fallback;
    }
}

// ---------------------------------------------------------------------------
//  Кто с кем и о чём
// ---------------------------------------------------------------------------

FLineContext UMindComponent::MakeLineContext(AActor* Other) const
{
    FLineContext C;
    if (Identity)
    {
        C.MyName = Identity->FirstName;
        C.bMeFemale = Identity->bFemale;
    }

    const ACompleteHumanNPC* Them = Cast<ACompleteHumanNPC>(Other);
    const UIdentityComponent* TheirId = Them ? Them->IdentityComponent.Get() : nullptr;
    C.bThemFemale = TheirId && TheirId->bFemale;
    C.bThemChild = TheirId && TheirId->Age < 14.0f;

    const FRelationship* R = Social ? Social->Find(Other) : nullptr;
    if (R)
    {
        C.TheirName = R->KnownName;
    }

    // С детьми на «ты», ребёнок ко взрослому на «вы», взрослые — на «вы», пока не сблизились.
    const bool bMeChild = Identity && Identity->Age < 14.0f;
    const float Familiar = R ? R->Familiarity : 0.0f;
    const float Close = Social ? Social->GetCloseness(Other) : 0.0f;
    C.bFormal = !C.bThemChild && (bMeChild || (Familiar < 0.45f && Close < 0.3f));

    C.Mood = Emotions ? Emotions->GetAffect().Pleasure : 0.0f;
    if (const UHumanWorldSubsystem* World = GetWorldMind())
    {
        C.Hour = World->Now.Hour;
        C.Weather = World->Weather;
    }
    return C;
}

const FKnownLocation* UMindComponent::KnownPlaceFor(ENeedType Need) const
{
    if (!Memory)
    {
        return nullptr;
    }

    const FKnownLocation* Best = nullptr;
    float BestScore = 0.1f;
    for (const FKnownLocation& Place : Memory->Places)
    {
        // Своё жильё чужому не советуют, опасное — тем более.
        if (Place.Kind == EPlaceKind::Home || Place.Kind == EPlaceKind::Danger
            || !PlaceSuits(Need, RealKindAt(Place.Location, Place.Kind)))
        {
            continue;
        }
        for (const FAffordance& O : Place.Offers)
        {
            if (O.bPrivate)
            {
                continue;
            }
            // Безработному советуют не всякое место, а то, где работают.
            if (Need == ENeedType::Money)
            {
                const float Score = O.Action == EActionType::Work ? 0.5f * (0.5f + Place.Familiarity) : 0.0f;
                if (Score > BestScore)
                {
                    BestScore = Score;
                    Best = &Place;
                }
                continue;
            }
            for (const FNeedPromise& P : O.Promises)
            {
                const float Score = P.Need == Need ? P.Amount * (0.5f + Place.Familiarity) + Place.Affect * 0.1f : 0.0f;
                if (Score > BestScore)
                {
                    BestScore = Score;
                    Best = &Place;
                }
            }
        }
    }
    return Best;
}

const FKnownLocation* UMindComponent::KnownPlaceOfKind(EPlaceKind Kind) const
{
    if (!Memory)
    {
        return nullptr;
    }
    const FKnownLocation* Best = nullptr;
    for (const FKnownLocation& Place : Memory->Places)
    {
        if ((Place.Kind == Kind || RealKindAt(Place.Location, Place.Kind) == Kind)
            && (!Best || Place.Familiarity > Best->Familiarity))
        {
            Best = &Place;
        }
    }
    return Best;
}

EPlaceKind UMindComponent::RealKindAt(const FVector& Where, EPlaceKind Fallback) const
{
    const UHumanWorldSubsystem* World = GetWorldMind();
    if (!World)
    {
        return Fallback;
    }

    EPlaceKind Found = Fallback;
    float BestDist = 900.0f;
    for (const FKnownLocation& P : World->GetPlacesNear(Where, 900.0f))
    {
        if (P.Kind == EPlaceKind::Home || P.Kind == EPlaceKind::Landmark
            || P.Kind == EPlaceKind::Unknown || P.Kind == EPlaceKind::Social)
        {
            continue;
        }
        const float Dist = FVector::Dist2D(P.Location, Where);
        if (Dist < BestDist)
        {
            BestDist = Dist;
            Found = P.Kind;
        }
    }
    return Found;
}

FString UMindComponent::FeelingReason() const
{
    if (!Emotions)
    {
        return FString();
    }
    const FEmotionInstance* Top = nullptr;
    for (const FEmotionInstance& E : Emotions->Active)
    {
        if (!Top || E.Intensity > Top->Intensity)
        {
            Top = &E;
        }
    }
    // Служебные пометки вслух не произносят.
    if (!Top || Top->Reason.IsEmpty() || Top->Reason.Len() > 60
        || Top->Reason.Contains(TEXT("(")) || Top->Reason.Contains(TEXT("@")) || Top->Reason.Contains(TEXT("№")))
    {
        return FString();
    }
    return Top->Reason;
}

void UMindComponent::GatherOwnTopics(AActor* Listener, TArray<FTalkingPoint>& Points) const
{
    auto Add = [this, &Points](const FTalkingPoint& P)
    {
        if (P.IsValid() && !Dialogue.Covered.Contains(TopicId(P)))
        {
            Points.Add(P);
        }
    };

    const float Close = Social ? Social->GetCloseness(Listener) : 0.0f;
    const FLineContext Probe;

    // Что мучает.
    if (Needs)
    {
        const ENeedType N = Needs->GetMostUrgent();
        const float U = Needs->GetUrgency(N);
        if (U > 0.4f && !FDialogueLines::Feel(Probe, N).IsEmpty() && (IsPractical(N) || Close > 0.35f))
        {
            FTalkingPoint P;
            P.Kind = ETalkKind::Need; P.Need = N; P.About = HumanText::Need(N);
            P.Valence = -U; P.Weight = 0.3f + U * 0.7f;
            Add(P);
        }
    }

    // Что прочитал.
    for (const FReadingBookmark& Mark : Bookmarks)
    {
        const FTextbook* Book = Mark.Understood > 0 ? FLibrary::Find(Mark.Subject) : nullptr;
        if (!Book || Book->Pages.Num() == 0)
        {
            continue;
        }
        FTalkingPoint P;
        P.Kind = ETalkKind::Book; P.About = Book->Title; P.Key = Mark.Subject;
        P.Page = FMath::Clamp(Mark.Page - 1, 0, Book->Pages.Num() - 1);
        P.Valence = 0.4f;
        P.Weight = 0.5f + (Personality ? Personality->Traits.Openness * 0.4f : 0.2f);
        Add(P);
    }

    // Где бывал.
    if (Memory)
    {
        const FKnownLocation* Vivid = nullptr;
        for (const FKnownLocation& Place : Memory->Places)
        {
            if (Place.Familiarity < 0.3f || Place.Kind == EPlaceKind::Home || Place.Kind == EPlaceKind::Landmark
                || Place.Kind == EPlaceKind::Unknown || Place.Kind == EPlaceKind::Work)
            {
                continue;
            }
            if (!Vivid || FMath::Abs(Place.Affect) > FMath::Abs(Vivid->Affect))
            {
                Vivid = &Place;
            }
        }
        if (Vivid)
        {
            FTalkingPoint P;
            P.Kind = ETalkKind::Place;
            P.PlaceKind = RealKindAt(Vivid->Location, Vivid->Kind);
            P.About = FDialogueLines::PlaceName(P.PlaceKind);
            P.Where = Vivid->Location; P.bHasWhere = true;
            P.Valence = Vivid->Affect;
            P.Weight = 0.25f + FMath::Abs(Vivid->Affect) * 0.3f;
            Add(P);
        }
    }

    // О ком есть что сказать.
    if (Social)
    {
        for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : Social->Relations)
        {
            const ACompleteHumanNPC* Other = Cast<ACompleteHumanNPC>(Pair.Key.Get());
            const FRelationship& R = Pair.Value;
            if (!Other || Other == Listener || !Other->IdentityComponent || R.KnownName.IsEmpty() || R.Familiarity < 0.3f)
            {
                continue;
            }
            const float Strength = FMath::Max(FMath::Abs(R.Liking), R.Respect - 0.45f);
            if (Strength < 0.3f && R.CaughtLying == 0)
            {
                continue;
            }
            FTalkingPoint P;
            P.Kind = ETalkKind::Person; P.About = R.KnownName; P.Person = Pair.Key;
            P.bFemale = Other->IdentityComponent->bFemale;
            P.Valence = FMath::Clamp(R.Liking + (R.Respect - 0.5f), -1.0f, 1.0f);
            P.Weight = 0.2f + Strength * 0.5f;
            Add(P);
        }
    }

    // Что на душе. Номер чувства хранится в Page.
    if (Emotions)
    {
        EEmotionType E = EEmotionType::None;
        float I = 0.0f;
        Emotions->GetDominantWithIntensity(E, I);
        if (I > 0.4f && !FDialogueLines::TellFeeling(Probe, E).IsEmpty())
        {
            FTalkingPoint P;
            P.Kind = ETalkKind::Feeling; P.About = HumanText::Emotion(E); P.Page = int32(E);
            P.Detail = FeelingReason();
            P.Valence = Emotions->GetAffect().Pleasure;
            P.Weight = (0.15f + I * 0.4f) * FMath::Clamp(0.3f + Close * 1.5f, 0.2f, 1.3f);
            Add(P);
        }
    }

    // Погода — о ней говорят, когда больше не о чем.
    {
        FTalkingPoint P;
        P.Kind = ETalkKind::Weather; P.About = TEXT("погода");
        P.Valence = GetWorldMind() ? 0.3f - GetWorldMind()->Weather * 0.6f : 0.0f;
        P.Weight = Close < 0.2f ? 0.3f : 0.12f;
        Add(P);
    }

    // Мечта — только своим.
    if (Identity && !Identity->LifeDream.IsEmpty() && Close > 0.35f)
    {
        FTalkingPoint P;
        P.Kind = ETalkKind::Dream; P.About = Identity->LifeDream; P.Weight = 0.25f;
        Add(P);
    }

    // Чем владеет.
    {
        float BestLevel = 0.0f;
        const FName Best = BestMastery(BestLevel);
        if (!Best.IsNone() && BestLevel > 0.35f && !FDialogueLines::TellSkill(Probe, Best).IsEmpty())
        {
            FTalkingPoint P;
            P.Kind = ETalkKind::Skill; P.About = UMindComponent::MasteryLabel(Best); P.Key = Best;
            P.Valence = 0.4f; P.Weight = 0.15f + BestLevel * 0.3f;
            Add(P);
        }
    }

    // Работа и беды.
    if (Identity)
    {
        if (!Identity->bEmployed && Identity->Age >= 16.0f && Identity->Age < 70.0f)
        {
            FTalkingPoint P;
            P.Kind = ETalkKind::Trouble; P.About = TEXT("работа"); P.Key = TEXT("NoJob");
            P.Valence = -0.5f; P.Weight = 0.2f + Close * 0.4f;
            Add(P);
        }
        else if (Identity->bEmployed)
        {
            FTalkingPoint P;
            P.Kind = ETalkKind::Work; P.About = Identity->Occupation;
            if (const FKnownLocation* Job = KnownPlaceOfKind(EPlaceKind::Work))
            {
                P.PlaceKind = RealKindAt(Job->Location, EPlaceKind::Work);
            }
            P.Weight = 0.18f;
            Add(P);
        }
        if (Identity->Money < 12.0f)
        {
            FTalkingPoint P;
            P.Kind = ETalkKind::Trouble; P.About = TEXT("деньги"); P.Key = TEXT("NoMoney");
            P.Valence = -0.6f; P.Weight = 0.15f + Close * 0.45f;
            Add(P);
        }
    }

    // Чем занят.
    if (bActionActive && CurrentAction != EActionType::Talk
        && !FDialogueLines::Doing(Probe, CurrentAction, bMoving).IsEmpty())
    {
        FTalkingPoint P;
        P.Kind = ETalkKind::Deed; P.Action = CurrentAction; P.About = HumanText::Action(CurrentAction);
        P.Weight = 0.3f;
        Add(P);
    }
}

FTalkingPoint UMindComponent::PickOwnTopic(AActor* Listener) const
{
    TArray<FTalkingPoint> Points;
    GatherOwnTopics(Listener, Points);
    if (Points.Num() == 0)
    {
        return FTalkingPoint();
    }

    float Total = 0.0f;
    for (const FTalkingPoint& P : Points)
    {
        Total += P.Weight;
    }
    float Roll = FMath::FRand() * Total;
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

// ---------------------------------------------------------------------------
//  Начало и конец
// ---------------------------------------------------------------------------

bool UMindComponent::StartConversation(AActor* Other)
{
    ACompleteHumanNPC* Them = Cast<ACompleteHumanNPC>(Other);
    if (!Them || !Speech || IsInConversation() || !CanTalkTo(Other))
    {
        return false;
    }
    UMindComponent* TheirMind = Them->Mind;
    if (!TheirMind || TheirMind->IsInConversation() || TheirMind->bAsleep)
    {
        return false;
    }

    Dialogue = FDialogueState();
    Dialogue.With = Other;
    Dialogue.LastActivityAt = RealClock(this);

    const float Talk = Personality ? Personality->Traits.Extraversion : 0.5f;
    const float Close = Social ? Social->GetCloseness(Other) : 0.0f;
    Dialogue.TurnLimit = 3 + FMath::RoundToInt(Talk * 4.0f + Close * 4.0f);
    ConversationPartner = Other;

    StayToTalk(Other);

    const FLineContext C = MakeLineContext(Other);
    const FUtterance NoExtra;

    ETalkChoice Opening = ETalkChoice::Nothing;
    if (bTalkReady && ChooseOpening(Other, true, Opening) && Opening != ETalkChoice::Nothing)
    {
        FString Line;
        EDialogueMove LineMove = EDialogueMove::Greet;
        FTalkingPoint LinePoint;
        FUtterance LineExtra;
        if (RealizeTalk(Opening, Other, C, Line, LineMove, LinePoint, LineExtra) && !Line.IsEmpty())
        {
            CommitTalk(Opening, Other);
            LineExtra.TalkReply = static_cast<uint8>(Opening);
            Say(FDialogueLines::Capital(Line), LineMove, LinePoint, LineExtra);
            return true;
        }
    }

    // Кого не знаешь по имени, тому представляются.
    if (C.TheirName.IsEmpty())
    {
        Say(FDialogueLines::Introduce(C), EDialogueMove::Introduce, FTalkingPoint(), NoExtra);
        return true;
    }

    FString Text = FDialogueLines::Greet(C);
    EDialogueMove Move = EDialogueMove::Greet;
    if (FMath::FRand() < 0.55f)
    {
        Text += TEXT(" ") + FDialogueLines::HowAreYou(C);
        Move = EDialogueMove::HowAreYou;
        Dialogue.bAskedHow = true;
    }
    Say(Text, Move, FTalkingPoint(), NoExtra);
    return true;
}

void UMindComponent::StayToTalk(AActor* Other)
{
    // Занятой разговор не затягивает — ответит на ходу.
    const bool bFree = !bActionActive
        || CurrentAction == EActionType::Idle || CurrentAction == EActionType::Wander
        || CurrentAction == EActionType::Explore || CurrentAction == EActionType::Rest
        || CurrentAction == EActionType::Entertain;
    if (!bFree || !Other)
    {
        return;
    }

    FAffordance Talk;
    Talk.Action = EActionType::Talk;
    Talk.Source = EAffordanceSource::Person;
    Talk.Target = Other;
    Talk.bRequiresProximity = false;
    Talk.Duration = 600.0f;
    Talk.EffortCost = 0.05f;
    Talk.Label = FString::Printf(TEXT("разговор — %s"), *NameOf(Other));
    Talk.Key = FName(*FString::Printf(TEXT("поговорить@%s"), *NameOf(Other)));

    FNeedPromise Contact;
    Contact.Need = ENeedType::SocialContact;
    Contact.Amount = 0.4f;
    Talk.Promises.Add(Contact);

    if (bActionActive)
    {
        if (ACompleteHumanNPC* Human = GetHuman())
        {
            Human->StopMoving();
        }
        ReleaseOccupied();
        FinishAction();
        bActionActive = false;
        bMoving = false;
    }

    FIntention Stay;
    Stay.bValid = true;
    Stay.Affordance = Talk;
    Stay.Expectation = 0.15f;
    Stay.DecidedAt = Now();
    Stay.Reason = FString::Printf(TEXT("остановился(лась) поговорить — %s"), *NameOf(Other));
    BeginIntention(Stay);
}

void UMindComponent::EndConversation()
{
    ResolveTalk(nullptr);
    TalkFelt = 0.0f;
    const AActor* Was = Dialogue.With;
    Dialogue = FDialogueState();

    if (ConversationPartner == Was)
    {
        ConversationPartner = nullptr;
    }
    SpeechCooldown = FMath::FRandRange(8.0f, 16.0f);

    if (bActionActive && CurrentAction == EActionType::Talk && ActiveIntention.Affordance.Target == Was)
    {
        CompleteIntention(true);
    }
}

void UMindComponent::UpdateDialogue(float RealDelta)
{
    if (!Dialogue.With)
    {
        return;
    }

    ACompleteHumanNPC* Me = GetHuman();
    ACompleteHumanNPC* Them = Cast<ACompleteHumanNPC>(Dialogue.With.Get());
    const double Clock = RealClock(this);

    if (bComposing || (Them && Them->Mind && Them->Mind->IsComposing()))
    {
        Dialogue.LastActivityAt = Clock;
    }

    // Собеседник ушёл, уснул или замолчал надолго — разговор кончился сам.
    if (!Me || !Them || !Them->IsAlive() || bAsleep
        || FVector::Dist2D(Me->GetActorLocation(), Them->GetActorLocation()) > 650.0f
        || Clock - Dialogue.LastActivityAt > 12.0)
    {
        EndConversation();
        return;
    }

    // Говоря, стоят лицом друг к другу.
    if (!bMoving && (!bActionActive || CurrentAction == EActionType::Talk))
    {
        FVector To = Them->GetActorLocation() - Me->GetActorLocation();
        To.Z = 0.0f;
        if (!To.IsNearlyZero())
        {
            const FRotator Want(0.0f, To.Rotation().Yaw, 0.0f);
            Me->SetActorRotation(FMath::RInterpTo(Me->GetActorRotation(), Want, RealDelta, 5.0f));
        }
    }

    if (Dialogue.bMyTurn && Clock >= Dialogue.ReplyAt)
    {
        TakeTurn();
    }
}

// ---------------------------------------------------------------------------
//  Сказать
// ---------------------------------------------------------------------------

TArray<FString> UMindComponent::WordsForTongue(const TSet<FString>& Names) const
{
    TArray<TPair<float, FString>> Known;
    if (Speech)
    {
        Known.Reserve(Speech->Vocabulary.Num());
        for (const TPair<FName, float>& Word : Speech->Vocabulary)
        {
            if (Word.Value > 0.45f)
            {
                Known.Emplace(Word.Value, Word.Key.ToString());
            }
        }
    }
    Known.Sort([](const TPair<float, FString>& A, const TPair<float, FString>& B) { return A.Key > B.Key; });
    TArray<FString> Words = Names.Array();
    for (const TPair<float, FString>& Word : Known)
    {
        Words.Add(Word.Value);
        if (Words.Num() >= 3000)
        {
            break;
        }
    }
    return Words;
}

void UMindComponent::Say(const FString& Text, EDialogueMove Move, const FTalkingPoint& Point, const FUtterance& Extra)
{
    ACompleteHumanNPC* Me = GetHuman();
    ACompleteHumanNPC* Them = Cast<ACompleteHumanNPC>(Dialogue.With.Get());
    if (!Me || !Them || Text.IsEmpty() || !Speech || bComposing)
    {
        return;
    }
    const UGameInstance* Game = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UTongueSubsystem* Tongue = Game ? Game->GetSubsystem<UTongueSubsystem>() : nullptr;
    const bool bWordless = Text.StartsWith(TEXT("("));
    if (!Tongue || !Tongue->IsReady() || bWordless || Move == EDialogueMove::Farewell || Speech->GetVocabularySize() < 30)
    {
        Utter(Text, Move, Point, Extra);
        return;
    }

    const TSet<FString> Names = NamesKnownTo(Identity, Social);
    const FRelationship* Known = Social ? Social->Find(Them) : nullptr;
    PendingSay = Extra;
    PendingSay.Move = Move;
    PendingSay.Point = Point;
    PendingSay.Text = Text;
    bComposing = true;
    Dialogue.LastActivityAt = RealClock(this);
    TWeakObjectPtr<UMindComponent> Self(this);
    TWeakObjectPtr<AActor> Listener(Them);
    Tongue->Phrase(Text, Identity ? Identity->FirstName : FString(), Known ? Known->KnownName : FString(),
        WordsForTongue(Names), [Self, Listener, Names](const FString& Said)
    {
        UMindComponent* Mind = Self.Get();
        if (!Mind)
        {
            return;
        }
        Mind->bComposing = false;
        if (!Listener.IsValid() || Mind->Dialogue.With.Get() != Listener.Get())
        {
            return;
        }
        const FUtterance Meant = Mind->PendingSay;
        if (!Said.IsEmpty())
        {
            ++Mind->TongueSpoken;
            UE_LOG(LogHumanCity, Display, TEXT("TONGUE %s: «%s» -> «%s»"), *Mind->GetOwner()->GetName(), *Meant.Text, *Said);
        }
        Mind->Utter(Said.IsEmpty() ? Meant.Text : WithNamesCapital(Said, Names), Meant.Move, Meant.Point, Meant);
    });
}

void UMindComponent::Utter(const FString& Text, EDialogueMove Move, const FTalkingPoint& Point, const FUtterance& Extra)
{
    ACompleteHumanNPC* Me = GetHuman();
    ACompleteHumanNPC* Them = Cast<ACompleteHumanNPC>(Dialogue.With.Get());
    if (!Me || !Them || Text.IsEmpty() || !Speech)
    {
        return;
    }

    const float T = Now();
    FUtterance U = Extra;
    U.Speaker = GetOwner();
    U.Listener = Them;
    U.Move = Move;
    U.Point = Point;
    U.Act = ActFor(Move, Point);
    U.bTruthful = true;
    U.SpokenAt = T;

    // Изо рта выходят только прочитанные когда-то слова и знакомые имена.
    const TSet<FString> Names = NamesKnownTo(Identity, Social);
    const FRelationship* Known = Social ? Social->Find(Them) : nullptr;
    U.Text = Speech->Articulate(Text, &Names, Identity ? Identity->FirstName : FString(),
                                Known ? Known->KnownName : FString());

    // Кому нечем сказать, тот разговор не тянет — и прощается рукой, а не мычанием.
    if (Speech->GetVocabularySize() < 5)
    {
        Dialogue.TurnLimit = FMath::Min(Dialogue.TurnLimit, 2);

        const bool bNoWords = U.Text.StartsWith(TEXT("(")) || U.Text.StartsWith(TEXT("Э-э"))
                           || U.Text.StartsWith(TEXT("М-м")) || U.Text.StartsWith(TEXT("а..."))
                           || U.Text.StartsWith(TEXT("ы-ы")) || U.Text.StartsWith(TEXT("м-м"));
        if (bNoWords && Move == EDialogueMove::Farewell)
        {
            U.Text = TEXT("(машет рукой на прощание)");
        }
        else if (bNoWords && Move == EDialogueMove::Thank)
        {
            U.Text = TEXT("(благодарно кивает)");
        }
    }

    float Tone = 0.0f;
    if (bTalkReady)
    {
        Tone = (Emotions ? Emotions->GetAffect().Pleasure * 0.35f : 0.0f)
             + (Social ? Social->FindOrAdd(Them, T).Liking * 0.35f : 0.0f)
             + TalkFelt * 0.6f;
        TalkFelt = 0.0f;
    }
    else
    {
        Tone = (Emotions ? Emotions->GetAffect().Pleasure * 0.4f : 0.0f)
             + (Social ? Social->FindOrAdd(Them, T).Liking * 0.4f : 0.0f);
        if (Move == EDialogueMove::Thank || Move == EDialogueMove::Offer || Move == EDialogueMove::Advise)
        {
            Tone += 0.35f;
        }
    }
    U.Tone = FMath::Clamp(Tone, -1.0f, 1.0f);

    // Деньги переходят из рук в руки вместе со словами.
    if (U.MoneyGiven > 0.0f && Identity)
    {
        const float Given = FMath::Min(U.MoneyGiven, Identity->Money);
        Identity->Money -= Given;
        U.MoneyGiven = Given;
        if (Them->IdentityComponent)
        {
            Them->IdentityComponent->Money += Given;
        }
        Report(FString::Printf(TEXT("дал(а) денег: %.0f"), Given));
    }

    Dialogue.MyMove = Move;
    Dialogue.MyPoint = Point;
    Dialogue.LastActivityAt = RealClock(this);
    ++Dialogue.Turns;
    if (Point.IsValid())
    {
        Dialogue.Covered.AddUnique(TopicId(Point));
    }

    Speech->Remember(U);
    Speech->LastSaid = U.Text;

    Report(FString::Printf(TEXT("— %s (%s)"), *U.Text,
        Them->IdentityComponent ? *Them->IdentityComponent->FirstName : TEXT("кому-то")));
    Me->ShowSpeech(U.Text);
    if (Me->Motor)
    {
        const bool bWordless = U.Text.StartsWith(TEXT("(")) || U.Text.Contains(TEXT("..."));
        const bool bRitual = Move == EDialogueMove::Greet || Move == EDialogueMove::Introduce || Move == EDialogueMove::Farewell;
        const FName Gesture = bRitual ? FName(TEXT("Wave")) : FName(TEXT("Explain"));

        if (Me->Motor->GestureLevel(Gesture) < 0.15f && FMath::FRand() < (bWordless ? 0.2f : 0.05f))
        {
            Me->Motor->PracticeGesture(Gesture, 0.12f);
        }

        const float Level = Me->Motor->GestureLevel(Gesture);
        if (Level >= 0.15f)
        {
            if (bRitual)
            {
                Me->Motor->MarkGreeting();
            }
            else
            {
                Me->Motor->MarkSpeaking(FMath::Clamp(1.2f + U.Text.Len() * 0.05f, 1.5f, 5.0f));
            }
            Me->Motor->PracticeGesture(Gesture, 0.02f);

            if (UHumanWorldSubsystem* GestureWorld = GetWorldMind())
            {
                for (ACompleteHumanNPC* Witness : GestureWorld->GetHumansNear(Me->GetActorLocation(), 1500.0f, Me))
                {
                    float Salience = 0.0f;
                    FName Sense;
                    if (Witness && Witness->Motor && Witness->CanPerceive(Me, Salience, Sense) && Sense == TEXT("Sight"))
                    {
                        Witness->Motor->ObserveGesture(Gesture, Salience * Level);
                    }
                }
            }
        }
    }
    Think(FString::Printf(TEXT("— %s"), *U.Text), EThoughtKind::Intention, 0.6f, Them);

    // Рядом стоящие слышат — и узнают знакомые слова.
    if (UHumanWorldSubsystem* World = GetWorldMind())
    {
        for (ACompleteHumanNPC* Bystander : World->GetHumansNear(Me->GetActorLocation(), 900.0f, Me))
        {
            if (Bystander && Bystander != Them && Bystander->SpeechComponent)
            {
                Bystander->SpeechComponent->LearnWords(U.Text, 0.3f,
                    Bystander->IdentityComponent && Bystander->IdentityComponent->Age < 13.0f);
            }
        }
    }

    if (Social)
    {
        Social->RecordInteraction(Them, U.Tone * 0.5f, T, Personality);
    }

    if (UMindComponent* TheirMind = Them->Mind)
    {
        TheirMind->Hear(U);
    }
}

// ---------------------------------------------------------------------------
//  Услышать
// ---------------------------------------------------------------------------

void UMindComponent::OnDialogueHeard(const FUtterance& U)
{
    if (!U.Speaker || U.Listener != GetOwner() || U.Move == EDialogueMove::None)
    {
        return;
    }

    // С другим уже говорю — этот голос мимо.
    if (Dialogue.With && Dialogue.With != U.Speaker)
    {
        return;
    }

    // Прощание вне разговора ответа не требует: разговор уже кончен.
    if (!Dialogue.With && U.Move == EDialogueMove::Farewell)
    {
        return;
    }

    const double Clock = RealClock(this);
    const float T = Now();

    // На прощание в ответ на своё прощание уже не отвечают.
    if (U.Move == EDialogueMove::Farewell && Dialogue.MyMove == EDialogueMove::Farewell)
    {
        EndConversation();
        return;
    }

    if (!Dialogue.With)
    {
        Dialogue = FDialogueState();
        Dialogue.With = U.Speaker;
        const float Talk = Personality ? Personality->Traits.Extraversion : 0.5f;
        const float Close = Social ? Social->GetCloseness(U.Speaker) : 0.0f;
        Dialogue.TurnLimit = 3 + FMath::RoundToInt(Talk * 4.0f + Close * 4.0f);
        ConversationPartner = U.Speaker;
        StayToTalk(U.Speaker);
    }

    Dialogue.TheirMove = U.Move;
    Dialogue.TheirPoint = U.Point;
    Dialogue.TheirText = U.Text;
    Dialogue.LastActivityAt = Clock;

    // Имя, названное вслух, запоминается — даже если остальных слов не понял.
    if (Social)
    {
        const ACompleteHumanNPC* SpeakerHuman = Cast<ACompleteHumanNPC>(U.Speaker.Get());
        const UIdentityComponent* SpeakerId = SpeakerHuman ? SpeakerHuman->IdentityComponent.Get() : nullptr;
        if (SpeakerId && !SpeakerId->FirstName.IsEmpty()
            && U.Text.Contains(SpeakerId->FirstName, ESearchCase::CaseSensitive))
        {
            FRelationship& Rel = Social->FindOrAdd(U.Speaker, T);
            if (Rel.KnownName.IsEmpty())
            {
                Rel.KnownName = SpeakerId->FirstName;
            }
        }
    }
    if (U.Point.IsValid())
    {
        Dialogue.Covered.AddUnique(TopicId(U.Point));
    }

    const TSet<FString> Names = NamesKnownTo(Identity, Social);
    const float Grasp = Speech ? Speech->ComprehensionWithNames(U.Text, Names) : 1.0f;
    ResolveTalk(&U);
    TalkFelt = FeltAbout(U, Grasp);
    if (Social && FMath::Abs(TalkFelt) > 0.02f)
    {
        Social->RecordInteraction(U.Speaker, TalkFelt * 0.6f, T, Personality);
    }
    if (Needs && Grasp >= 0.35f)
    {
        float Esteem = FTalkRules::Esteem(ChoiceOf(U.TalkReply, U.Move));
        if (U.TalkOwn < static_cast<uint8>(ETalkChoice::Count))
        {
            Esteem += FTalkRules::Esteem(static_cast<ETalkChoice>(U.TalkOwn));
        }
        if (Esteem > 0.0f)
        {
            Needs->Satisfy(ENeedType::Esteem, Esteem * 0.25f);
        }
    }
    const float Trust = Social ? Social->GetTrust(U.Speaker) : 0.3f;
    const float Respect = (Social && Social->Find(U.Speaker)) ? Social->Find(U.Speaker)->Respect : 0.3f;
    const float Openness = Personality ? Personality->Traits.Openness : 0.5f;

    if (Grasp < 0.35f)
    {
        // Слов не понял. Приветствие и прощание видны и так — по руке,
        // по кивку; а отвечать по существу нечем, и разговор сворачивается.
        if (!IsGesturable(U.Move))
        {
            Dialogue.TheirMove = EDialogueMove::React;
        }
        Dialogue.TheirPoint = FTalkingPoint();
        Dialogue.bLost = true;
        Dialogue.TurnLimit = FMath::Min(Dialogue.TurnLimit, Dialogue.Turns + 1);
    }
    else
    {
        // Пересказ книги: знание из чужих слов, слабее, чем из самой книги.
        const FTextbook* Book = U.BookSubject.IsNone() ? nullptr : FLibrary::Find(U.BookSubject);
        if (Book && Book->Pages.IsValidIndex(U.BookPage) && Memory)
        {
            const FString Sentence = FDialogueLines::FirstSentence(Book->Pages[U.BookPage].Text);
            FBelief Heard;
            Heard.Subject = U.BookSubject;
            Heard.Predicate = TEXT("HeardRetold");
            Heard.Value = 1.0f;
            Heard.Confidence = FMath::Clamp(0.3f + Trust * 0.4f, 0.0f, 1.0f);
            Heard.LearnedAt = T;
            Heard.Text = Sentence;
            Heard.Source = U.Speaker;
            Memory->Learn(Heard, Trust, Openness, T);
            Practised(Book->Skill, 0.03f * Grasp);
            JudgeByDeeds(U.Speaker, 0.05f, TEXT("книги читает и толком пересказывает"));
            Report(FString::Printf(TEXT("узнал(а) от %s: «%s»"), *NameOf(U.Speaker), *Sentence.Left(90)));
        }

        // Совет или распоряжение — возможность, которую хочется исполнить.
        if ((U.Move == EDialogueMove::Advise || U.Move == EDialogueMove::Instruct) && U.bHasPlacePayload)
        {
            const bool bNew = Memory && !Memory->KnowsPlace(U.PlacePayload.Kind, U.PlacePayload.Location);
            if (const FAffordance* Offer = OfferFor(U.PlacePayload, U.Point.Need, U.Point.Action))
            {
                FAffordance A = *Offer;
                A.Location = U.PlacePayload.Location;
                A.bHasLocation = true;
                const bool bOrder = U.Move == EDialogueMove::Instruct;
                AddSuggestion(A, U.Speaker, Respect * 0.6f + Trust * 0.3f + (bOrder ? 0.1f : 0.0f), bOrder);
            }
            // Подсказал новое — это дело; напомнил известное — тоже по-доброму.
            JudgeByDeeds(U.Speaker, bNew ? 0.08f : 0.02f,
                         bNew ? FString(TEXT("подсказал(а) дельное")) : FString());
        }

        // Ко мне пришли за советом. Быть нужным — само по себе награда.
        if (U.Move == EDialogueMove::AskAdvice && Identity)
        {
            ++Identity->TimesAskedForAdvice;
            if (Needs)
            {
                Needs->Satisfy(ENeedType::Esteem, 0.22f);
                Needs->Satisfy(ENeedType::Meaning, 0.15f);
            }
            Think(FString::Printf(TEXT("%s спрашивает у меня совета"), *NameOf(U.Speaker)),
                  EThoughtKind::Judgement, 0.7f, U.Speaker);
        }

        if (U.MoneyGiven > 0.0f && Social)
        {
            Social->OnHelpedBy(U.Speaker, 0.5f, T, Personality);
            JudgeByDeeds(U.Speaker, 0.12f, TEXT("выручил(а) деньгами"));
        }
        if (U.Move == EDialogueMove::Offer && !U.SkillPayload.IsNone())
        {
            JudgeByDeeds(U.Speaker, 0.05f, TEXT("не жалеет времени, учит"));
        }

        // Мнение о третьем человеке передаётся — насколько я верю говорящему.
        if (U.Move == EDialogueMove::Tell && U.Point.Kind == ETalkKind::Person && U.Point.Person && Social
            && Social->Find(U.Point.Person))
        {
            Social->ReceiveGossip(U.Speaker, U.Point.Person, U.Point.Valence, T, Personality);
        }
    }

    // Занятой отвечает коротко и возвращается к делу.
    if (bActionActive && IsAbsorbing(CurrentAction) && Phase == EActionPhase::Doing)
    {
        Dialogue.TurnLimit = FMath::Min(Dialogue.TurnLimit, Dialogue.Turns + 1);
    }

    Dialogue.bMyTurn = true;
    Dialogue.ReplyAt = Clock + ReadingPause(U.Text);
}

// ---------------------------------------------------------------------------
//  Советы и распоряжения
// ---------------------------------------------------------------------------

void UMindComponent::AddSuggestion(const FAffordance& A, AActor* From, float Weight, bool bInstruction)
{
    const float T = Now();
    Suggestions.RemoveAll([&A, T](const FSuggestion& S)
    {
        return S.Affordance.Key == A.Key || S.ExpiresAt < T;
    });

    FSuggestion S;
    S.Affordance = A;
    S.From = From;
    S.Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
    S.ExpiresAt = T + 6.0f * 3600.0f;
    S.bInstruction = bInstruction;
    Suggestions.Add(S);

    while (Suggestions.Num() > 6)
    {
        Suggestions.RemoveAt(0);
    }

    Think(FString::Printf(TEXT("%s %s: %s"), *NameOf(From),
        bInstruction ? TEXT("велит") : TEXT("советует"), *A.Label), EThoughtKind::Intention, 0.6f, From);
}

void UMindComponent::JudgeSuggestionOutcome(const FAffordance& Done, float Outcome)
{
    for (int32 i = Suggestions.Num() - 1; i >= 0; --i)
    {
        if (Suggestions[i].Affordance.Key != Done.Key)
        {
            continue;
        }
        AActor* From = Suggestions[i].From;
        Suggestions.RemoveAt(i);

        // Послушался — и вышло хорошо: в следующий раз послушаюсь охотнее.
        if (Outcome > 0.1f)
        {
            JudgeByDeeds(From, 0.07f, TEXT("дело говорит"));
        }
        else if (Outcome < -0.1f)
        {
            JudgeByDeeds(From, -0.08f, TEXT("зря я послушался(лась)"));
        }
        return;
    }
}

bool UMindComponent::TryInstruct(const FLineContext& C, FString& OutText, FUtterance& OutExtra)
{
    if (!Identity || !Personality || !Dialogue.With)
    {
        return false;
    }

    // Распоряжаются те, с кем уже считаются и кому это в характере.
    const float Authority = Identity->Standing + FMath::Min(Identity->TimesAskedForAdvice, 5) * 0.06f;
    const float Temper = Personality->Traits.Extraversion * 0.35f
                       + Personality->Traits.Conscientiousness * 0.35f
                       + Personality->Traits.Agreeableness * 0.3f;
    if (Authority < 0.28f || Temper < 0.55f)
    {
        return false;
    }

    const FTalkingPoint& TP = Dialogue.TheirPoint;
    const ACompleteHumanNPC* Them = Cast<ACompleteHumanNPC>(Dialogue.With.Get());
    if (!Them)
    {
        return false;
    }

    EActionType Action = EActionType::Idle;
    ENeedType Need = ENeedType::Competence;
    const FKnownLocation* Place = nullptr;

    // Не может связать двух слов — значит, не читал. Первое ему дело — азбука.
    const FString& Heard = Dialogue.TheirText;
    const bool bMute = Heard.StartsWith(TEXT("Э-э")) || Heard.StartsWith(TEXT("М-м"))
                    || Heard.StartsWith(TEXT("(")) || Heard.Contains(TEXT("..."));

    if (bMute)
    {
        Place = KnownPlaceOfKind(EPlaceKind::Library);
        Action = EActionType::Read;
    }
    else if (TP.Kind == ETalkKind::Need)
    {
        Need = TP.Need;
        Place = KnownPlaceFor(Need);
        Action = Need == ENeedType::Hunger ? EActionType::Eat
               : Need == ENeedType::Thirst ? EActionType::Drink
               : EActionType::Idle;
    }
    else if (TP.Kind == ETalkKind::Trouble)
    {
        Need = ENeedType::Money;
        Action = EActionType::Work;
        if (Memory)
        {
            for (const FKnownLocation& P : Memory->Places)
            {
                if (P.Kind != EPlaceKind::Home && OfferFor(P, ENeedType::Money, EActionType::Work))
                {
                    Place = &P;
                    break;
                }
            }
        }
    }

    if (!Place)
    {
        return false;
    }

    const EPlaceKind K = RealKindAt(Place->Location, Place->Kind);
    OutText = FDialogueLines::Instruct(C, Action, K, FDialogueLines::WayTo(Them->GetActorLocation(), Place->Location));
    OutExtra.PlacePayload = *Place;
    OutExtra.bHasPlacePayload = true;
    OutExtra.Point.Kind = TP.Kind == ETalkKind::None ? ETalkKind::Trouble : TP.Kind;
    OutExtra.Point.About = FDialogueLines::PlaceName(K);
    OutExtra.Point.Need = Need;
    OutExtra.Point.Action = Action;
    OutExtra.Point.PlaceKind = K;
    OutExtra.Point.Where = Place->Location;
    OutExtra.Point.bHasWhere = true;
    return true;
}

// ---------------------------------------------------------------------------
//  Ход
// ---------------------------------------------------------------------------

void UMindComponent::TakeTurn()
{
    if (bTalkReady)
    {
        TakeTurnLearned();
        return;
    }
    AActor* Other = Dialogue.With;
    const ACompleteHumanNPC* Them = Cast<ACompleteHumanNPC>(Other);
    if (!Them)
    {
        EndConversation();
        return;
    }
    Dialogue.bMyTurn = false;

    const FLineContext C = MakeLineContext(Other);
    const FVector TheirPos = Them->GetActorLocation();
    const FTalkingPoint TP = Dialogue.TheirPoint;
    const bool bFirstWords = Dialogue.MyMove == EDialogueMove::None;

    FString Text;
    EDialogueMove Move = EDialogueMove::React;
    FTalkingPoint Point;
    FUtterance Extra;

    // Каждая часть реплики — отдельное предложение, и начинается оно с заглавной.
    auto Join = [&Text](const FString& More)
    {
        if (!More.IsEmpty())
        {
            const FString Part = FDialogueLines::Capital(More);
            Text = Text.IsEmpty() ? Part : Text + TEXT(" ") + Part;
        }
    };

    // Совет места под нужду — из собственной памяти.
    auto AdviseFor = [&](ENeedType Need) -> bool
    {
        const FKnownLocation* Place = KnownPlaceFor(Need);
        if (!Place)
        {
            return false;
        }
        const EPlaceKind K = RealKindAt(Place->Location, Place->Kind);
        Join(FDialogueLines::AdvisePlace(C, K, FDialogueLines::WayTo(TheirPos, Place->Location)));
        Move = EDialogueMove::Advise;
        Point = TP;
        Point.Need = Need;
        Point.PlaceKind = K;
        Point.Where = Place->Location;
        Point.bHasWhere = true;
        Extra.PlacePayload = *Place;
        Extra.bHasPlacePayload = true;
        return true;
    };

    const float MyUrgency = Needs ? Needs->GetUrgency(Needs->GetMostUrgent()) : 0.0f;
    const float MyMood = C.Mood;

    // --- Ответ на то, что сказали --------------------------------------------
    switch (Dialogue.TheirMove)
    {
    case EDialogueMove::Farewell:
        Say(FDialogueLines::FarewellBack(C), EDialogueMove::Farewell, FTalkingPoint(), Extra);
        EndConversation();
        return;

    case EDialogueMove::Introduce:
        if (bFirstWords)
        {
            Join(FDialogueLines::IntroduceBack(C));
            Move = EDialogueMove::Introduce;
        }
        break;

    case EDialogueMove::React:
        // Не поняли друг друга — кивнули и разошлись.
        if (Dialogue.bLost)
        {
            Say(FDialogueLines::Farewell(C, FString()), EDialogueMove::Farewell, FTalkingPoint(), Extra);
            EndConversation();
            return;
        }
        break;

    case EDialogueMove::Greet:
        if (bFirstWords)
        {
            Join(FDialogueLines::GreetBack(C));
            Move = EDialogueMove::Greet;
            if (!Dialogue.bAskedHow && FMath::FRand() < 0.6f)
            {
                Join(FDialogueLines::HowAreYou(C));
                Move = EDialogueMove::HowAreYou;
                Dialogue.bAskedHow = true;
            }
        }
        break;

    case EDialogueMove::HowAreYou:
    {
        if (bFirstWords)
        {
            Join(FDialogueLines::GreetBack(C));
        }
        const ENeedType N = Needs ? Needs->GetMostUrgent() : ENeedType::Hunger;
        const bool bCanShare = IsPractical(N) || (Social && Social->GetCloseness(Other) > 0.35f);
        const FString Why = (MyUrgency > 0.45f && bCanShare) ? FDialogueLines::Feel(C, N) : FString();
        const FString Busy = bActionActive ? FDialogueLines::Doing(C, CurrentAction, bMoving) : FString();

        if (!Why.IsEmpty())
        {
            Join(MyMood > 0.1f
                ? FString::Printf(TEXT("Да ничего, только %s."), *Why)
                : FString::Printf(TEXT("%s %s."), *FDialogueLines::MoodWord(C, MyMood), *FDialogueLines::Capital(Why)));
            Point.Kind = ETalkKind::Need; Point.Need = N; Point.About = HumanText::Need(N); Point.Valence = -MyUrgency;
        }
        else if (!Busy.IsEmpty())
        {
            Join(FString::Printf(TEXT("%s %s."), *FDialogueLines::MoodWord(C, MyMood), *FDialogueLines::Capital(Busy)));
            Point.Kind = ETalkKind::Deed; Point.Action = CurrentAction; Point.About = HumanText::Action(CurrentAction);
        }
        else
        {
            Join(FDialogueLines::MoodWord(C, MyMood));
        }
        Move = EDialogueMove::StateOfSelf;
        Dialogue.bToldHow = true;

        if (!Dialogue.bAskedHow && FMath::FRand() < 0.7f)
        {
            Join(FDialogueLines::AndYou(C));
            Move = EDialogueMove::HowAreYou;
            Dialogue.bAskedHow = true;
        }
        break;
    }

    case EDialogueMove::StateOfSelf:
    case EDialogueMove::Tell:
        switch (TP.Kind)
        {
        case ETalkKind::Need:
            if (!AdviseFor(TP.Need))
            {
                Join(Needs && Needs->GetUrgency(TP.Need) > 0.45f
                    ? FDialogueLines::SameHere(C, TP.Need) : FDialogueLines::Acknowledge(C));
            }
            break;

        case ETalkKind::Deed:
            Join(FDialogueLines::ReactDoing(C, TP.Action));
            break;

        case ETalkKind::Book:
        {
            bool bRead = false;
            for (const FReadingBookmark& Mark : Bookmarks)
            {
                bRead |= (Mark.Subject == TP.Key && Mark.Page > TP.Page);
            }
            if (bRead)
            {
                Join(FDialogueLines::KnowThat(C));
            }
            else if (Mastery(TEXT("Reading")) < 0.12f)
            {
                Join(FDialogueLines::CantRead(C));
            }
            else
            {
                Join(FDialogueLines::AskWhatWritten(C));
                Move = EDialogueMove::Ask;
                Point = TP;
            }
            break;
        }

        case ETalkKind::Place:
        {
            const FKnownLocation* Mine = nullptr;
            if (Memory)
            {
                for (const FKnownLocation& P : Memory->Places)
                {
                    if (FVector::Dist2D(P.Location, TP.Where) < 800.0f)
                    {
                        Mine = &P;
                        break;
                    }
                }
            }
            if (Mine && (Mine->Affect >= 0.0f) == (TP.Valence >= 0.0f))
            {
                Join(FDialogueLines::AgreePlace(C, TP.PlaceKind, Mine->Affect));
            }
            else if (Mine)
            {
                Join(FDialogueLines::DisagreePlace(C, TP.PlaceKind, Mine->Affect));
            }
            else
            {
                Join(FDialogueLines::AskWhere(C));
                Move = EDialogueMove::Ask;
                Point = TP;
            }
            break;
        }

        case ETalkKind::Person:
        {
            const FRelationship* R = (Social && TP.Person) ? Social->Find(TP.Person) : nullptr;
            if (TP.Person == GetOwner())
            {
                Join(FDialogueLines::Acknowledge(C));
            }
            else if (R && R->Familiarity > 0.2f)
            {
                const bool bMineGood = R->Liking + (R->Respect - 0.5f) > 0.0f;
                const bool bTheirsGood = TP.Valence > 0.0f;
                Join(bMineGood == bTheirsGood
                    ? FDialogueLines::AgreePerson(C, TP.About, TP.bFemale, bMineGood)
                    : FDialogueLines::DisagreePerson(C, TP.bFemale, bTheirsGood));
            }
            else
            {
                Join(FDialogueLines::DontKnowPerson(C, TP.bFemale));
            }
            break;
        }

        case ETalkKind::Feeling:
            if (TP.Valence < 0.0f)
            {
                const float Empathy = Personality ? Personality->Facets.Empathy : 0.5f;
                if (Empathy + (Social ? Social->GetCloseness(Other) : 0.0f) > 0.7f && !TP.Detail.IsEmpty())
                {
                    Join(FDialogueLines::AskWhatHappened(C));
                    Move = EDialogueMove::Ask;
                    Point = TP;
                }
                else
                {
                    Join(FDialogueLines::Console(C));
                    Move = EDialogueMove::Console;
                    Point = TP;
                }
            }
            else
            {
                Join(FDialogueLines::GladForYou(C));
            }
            break;

        case ETalkKind::Weather:
            Join(FDialogueLines::AgreeWeather(C));
            break;

        case ETalkKind::Dream:
            Join(FDialogueLines::ReactDream(C));
            if (FMath::FRand() < 0.5f)
            {
                Join(FDialogueLines::AskWhatStops(C));
                Move = EDialogueMove::Ask;
                Point = TP;
            }
            break;

        case ETalkKind::Skill:
            if (Mastery(TP.Key) < 0.3f && Personality && Personality->Traits.Openness > 0.4f)
            {
                Join(FDialogueLines::AskTeach(C));
                Move = EDialogueMove::Ask;
                Point = TP;
            }
            else
            {
                Join(FDialogueLines::Acknowledge(C));
            }
            break;

        case ETalkKind::Trouble:
            if (TP.Key == TEXT("NoMoney") && Identity && Identity->Money > 40.0f && Personality
                && Personality->Traits.Agreeableness + (Social ? Social->GetCloseness(Other) : 0.0f) > 0.9f)
            {
                Join(FDialogueLines::OfferMoney(C));
                Move = EDialogueMove::Offer;
                Extra.MoneyGiven = 10.0f;
                Point = TP;
            }
            else if (!TryInstruct(C, Text, Extra))
            {
                if (!(TP.Key == TEXT("NoJob") && AdviseFor(ENeedType::Money)))
                {
                    Join(FDialogueLines::Encourage(C));
                    Move = EDialogueMove::Console;
                }
            }
            else
            {
                Move = EDialogueMove::Instruct;
                Point = Extra.Point;
            }
            break;

        case ETalkKind::Work:
            Join(FDialogueLines::AskPay(C));
            Move = EDialogueMove::Ask;
            Point = TP;
            break;

        default:
            Join(FDialogueLines::Acknowledge(C));
            break;
        }
        break;

    case EDialogueMove::Ask:
        switch (TP.Kind)
        {
        case ETalkKind::Book:
        {
            const FTextbook* Book = FLibrary::Find(TP.Key);
            if (Book && Book->Pages.IsValidIndex(TP.Page))
            {
                Join(FDialogueLines::Retell(C, FDialogueLines::FirstSentence(Book->Pages[TP.Page].Text)));
                Extra.BookSubject = TP.Key;
                Extra.BookPage = TP.Page;
            }
            Move = EDialogueMove::Answer;
            Point = TP;
            break;
        }
        case ETalkKind::Place:
            Join(FDialogueLines::AnswerWhere(C, TP.PlaceKind, FDialogueLines::WayTo(TheirPos, TP.Where)));
            if (Memory)
            {
                for (const FKnownLocation& P : Memory->Places)
                {
                    if (FVector::Dist2D(P.Location, TP.Where) < 800.0f)
                    {
                        Extra.PlacePayload = P;
                        Extra.bHasPlacePayload = true;
                        break;
                    }
                }
            }
            Move = EDialogueMove::Answer;
            Point = TP;
            break;

        case ETalkKind::Feeling:
            Join(FDialogueLines::AnswerWhatHappened(C, TP.Detail));
            Move = EDialogueMove::Answer;
            break;

        case ETalkKind::Dream:
            Join(FDialogueLines::AnswerWhatStops(C, Identity && Identity->Money < 30.0f,
                Personality && Personality->Traits.Neuroticism > 0.6f));
            Move = EDialogueMove::Answer;
            break;

        case ETalkKind::Skill:
        {
            const float Willing = (Personality ? Personality->Traits.Agreeableness * 0.6f : 0.3f)
                                + (Social ? Social->GetCloseness(Other) * 0.4f : 0.0f)
                                + Mastery(TEXT("Teaching")) * 0.3f;
            if (Willing > 0.45f)
            {
                Join(FDialogueLines::AgreeTeach(C));
                Move = EDialogueMove::Offer;
                Extra.SkillPayload = TP.Key;
                Extra.SkillPayloadLevel = Mastery(TP.Key);
            }
            else
            {
                Join(FDialogueLines::RefuseTeach(C));
                Move = EDialogueMove::Decline;
            }
            break;
        }

        case ETalkKind::Work:
            Join(FDialogueLines::AnswerPay(C, Identity && Identity->HourlyWage >= 9.0f));
            Move = EDialogueMove::Answer;
            break;

        default:
            Join(FDialogueLines::Acknowledge(C));
            break;
        }
        break;

    case EDialogueMove::AskAdvice:
    {
        FString Order;
        FUtterance OrderExtra;
        if (TryInstruct(C, Order, OrderExtra))
        {
            Join(Order);
            Extra = OrderExtra;
            Move = EDialogueMove::Instruct;
            Point = OrderExtra.Point;
        }
        else if (!AdviseFor(TP.Kind == ETalkKind::Trouble ? ENeedType::Money : TP.Need))
        {
            Join(FDialogueLines::DontKnowWhere(C));
        }
        break;
    }

    case EDialogueMove::Answer:
        Join(TP.Kind == ETalkKind::Book ? FDialogueLines::DidntKnow(C)
           : TP.Kind == ETalkKind::Place ? FDialogueLines::Thanks(C)
           : FDialogueLines::Acknowledge(C));
        Move = TP.Kind == ETalkKind::Place ? EDialogueMove::Thank : EDialogueMove::React;
        break;

    case EDialogueMove::Advise:
        Join(FDialogueLines::Thanks(C));
        Move = EDialogueMove::Thank;
        break;

    case EDialogueMove::Console:
        Join(FDialogueLines::ThanksForKindness(C));
        Move = EDialogueMove::Thank;
        break;

    case EDialogueMove::Instruct:
    {
        // Послушаться или нет — зависит от того, кто велит и что со мной самим.
        const FRelationship* R = Social ? Social->Find(Other) : nullptr;
        const float Obedience = (R ? R->Respect * 0.6f + R->Trust * 0.3f : 0.2f)
                              - (Personality ? Personality->Facets.Stubbornness * 0.25f : 0.1f)
                              - MyUrgency * 0.2f;
        if (Obedience > 0.3f)
        {
            Join(FDialogueLines::Obey(C));
            Move = EDialogueMove::Obey;
        }
        else
        {
            Join(FDialogueLines::RefuseOrder(C, MyUrgency > 0.5f && Needs
                ? FDialogueLines::Feel(C, Needs->GetMostUrgent()) : FString()));
            Move = EDialogueMove::Decline;
            Suggestions.RemoveAll([Other](const FSuggestion& S) { return S.From == Other && S.bInstruction; });
        }
        break;
    }

    case EDialogueMove::Offer:
        Join(Dialogue.TheirPoint.Kind == ETalkKind::Trouble ? FDialogueLines::AcceptMoney(C) : FDialogueLines::Thanks(C));
        Move = EDialogueMove::Thank;
        break;

    case EDialogueMove::Thank:
        Join(FDialogueLines::NoProblem(C));
        break;

    default:
        break;
    }

    // --- Своё слово ---------------------------------------------------------
    // Если ответ ничего не ждёт в ответ, разговор ведёт тот, у кого ход:
    // заводит своё, распоряжается, спрашивает — или прощается.
    const bool bExpects = Move != EDialogueMove::React && Move != EDialogueMove::Thank
                       && Move != EDialogueMove::Obey && Move != EDialogueMove::Decline
                       && Move != EDialogueMove::Introduce && Move != EDialogueMove::Greet;

    if (!bExpects)
    {
        const UHumanWorldSubsystem* World = GetWorldMind();
        const bool bLate = World && (World->Now.Hour >= 22 || World->Now.Hour < 5);
        const bool bWantLeave = Dialogue.Turns >= Dialogue.TurnLimit || MyUrgency > 0.65f || bLate;

        const bool bSeekingAdvice = bActionActive && IsTalkingWith(ActiveIntention.Affordance.Target)
            && ActiveIntention.Affordance.Key.ToString().StartsWith(TEXT("совет@"));

        if (bWantLeave && Dialogue.Turns >= 2)
        {
            const ENeedType N = Needs ? Needs->GetMostUrgent() : ENeedType::Hunger;
            Join(FDialogueLines::Farewell(C, FDialogueLines::LeaveReason(C, N, MyUrgency, EActionType::Idle, bLate)));
            Move = EDialogueMove::Farewell;
        }
        else if (bSeekingAdvice && !Dialogue.bAskedAdvice && Dialogue.Turns >= 1)
        {
            // Шёл за советом — о нём и спрашивает.
            const ENeedType N = Needs ? Needs->GetMostUrgent() : ENeedType::Hunger;
            const FString Where = FDialogueLines::AskWhereFor(C, N);
            if (Identity && !Identity->bEmployed && Identity->Age >= 16.0f)
            {
                Join(FDialogueLines::TellNoJob(C) + TEXT(" ") + FDialogueLines::AskForAdvice(C));
                Point.Kind = ETalkKind::Trouble; Point.About = TEXT("работа"); Point.Key = TEXT("NoJob");
            }
            else if (!Where.IsEmpty())
            {
                Join(Where);
                Point.Kind = ETalkKind::Need; Point.Need = N; Point.About = HumanText::Need(N);
            }
            else
            {
                Join(FDialogueLines::AskForAdvice(C));
                Point.Kind = ETalkKind::Trouble; Point.About = TEXT("жизнь");
            }
            Move = EDialogueMove::AskAdvice;
            Dialogue.bAskedAdvice = true;
        }
        else if (!Dialogue.bAskedHow && !C.bThemChild && !C.TheirName.IsEmpty())
        {
            Join(FDialogueLines::HowAreYou(C));
            Move = EDialogueMove::HowAreYou;
            Dialogue.bAskedHow = true;
        }
        else
        {
            FString Order;
            FUtterance OrderExtra;
            if ((Dialogue.TheirPoint.IsValid() || !Dialogue.TheirText.IsEmpty()) && TryInstruct(C, Order, OrderExtra))
            {
                Join(Order);
                Extra = OrderExtra;
                Move = EDialogueMove::Instruct;
                Point = OrderExtra.Point;
            }
            else
            {
                const FTalkingPoint Mine = PickOwnTopic(Other);
                FString Line;
                EDialogueMove MineMove = EDialogueMove::Tell;

                switch (Mine.Kind)
                {
                case ETalkKind::Need:
                    Line = FDialogueLines::Capital(FDialogueLines::Feel(C, Mine.Need)) + TEXT(".");
                    if (!KnownPlaceFor(Mine.Need))
                    {
                        const FString Ask = FDialogueLines::AskWhereFor(C, Mine.Need);
                        if (!Ask.IsEmpty())
                        {
                            Line += TEXT(" ") + Ask;
                            MineMove = EDialogueMove::AskAdvice;
                        }
                    }
                    break;
                case ETalkKind::Deed:
                    Line = FDialogueLines::Capital(FDialogueLines::Doing(C, Mine.Action, bMoving)) + TEXT(".");
                    break;
                case ETalkKind::Book:
                    if (const FTextbook* Book = FLibrary::Find(Mine.Key))
                    {
                        Line = FDialogueLines::TellReading(C, Book->Title,
                            Book->Pages.IsValidIndex(Mine.Page) ? Book->Pages[Mine.Page].Title : FString());
                    }
                    break;
                case ETalkKind::Place:
                    Line = FDialogueLines::TellPlace(C, Mine.PlaceKind, Mine.Valence);
                    break;
                case ETalkKind::Person:
                    if (const FRelationship* R = Social ? Social->Find(Mine.Person) : nullptr)
                    {
                        const EPersonReason Why = R->Debt > 0.2f ? EPersonReason::Helped
                            : R->Respect > 0.7f ? EPersonReason::Respected
                            : R->CaughtLying > 0 ? EPersonReason::Liar
                            : R->Resentment > 0.35f ? EPersonReason::Offended
                            : R->Liking > 0.25f ? EPersonReason::Pleasant
                            : EPersonReason::Unpleasant;
                        Line = FDialogueLines::TellPerson(C, Mine.About, Mine.bFemale, Why,
                                                          R->Respect > 0.78f && R->Trust > 0.55f);
                    }
                    break;
                case ETalkKind::Feeling:
                    Line = FDialogueLines::TellFeeling(C, EEmotionType(Mine.Page));
                    break;
                case ETalkKind::Weather:
                    Line = FDialogueLines::TellWeather(C);
                    break;
                case ETalkKind::Dream:
                    Line = FDialogueLines::TellDream(C, Mine.About);
                    break;
                case ETalkKind::Skill:
                    Line = FDialogueLines::TellSkill(C, Mine.Key);
                    break;
                case ETalkKind::Trouble:
                    Line = Mine.Key == TEXT("NoJob") ? FDialogueLines::TellNoJob(C) : FDialogueLines::TellNoMoney(C);
                    break;
                case ETalkKind::Work:
                    Line = FDialogueLines::TellWork(C, Mine.PlaceKind);
                    break;
                default:
                    break;
                }

                if (!Line.IsEmpty())
                {
                    Join(Line);
                    Move = MineMove;
                    Point = Mine;
                }
                else if (Dialogue.Turns >= 1)
                {
                    Join(FDialogueLines::Farewell(C, FString()));
                    Move = EDialogueMove::Farewell;
                }
            }
        }
    }

    if (Text.IsEmpty())
    {
        EndConversation();
        return;
    }
    Say(Text, Move, Point, Extra);
}

FString UMindComponent::TalkPath() const
{
    FString Name = Identity ? Identity->GetFullName() : GetOwner()->GetName();
    Name = FPaths::MakeValidFileName(Name.Replace(TEXT(" "), TEXT("_")));
    return FLifeSchool::Folder() / TEXT("People") / (Name + TEXT(".talk"));
}

void UMindComponent::WakeTalk()
{
    bTalkReady = false;
    TalkRng.Initialize(static_cast<int32>(GetTypeHash(GetOwner()->GetFName()) ^ 0x3c6ef372u));
    if (FParse::Param(FCommandLine::Get(), TEXT("OldTalk")))
    {
        return;
    }
    bTalkReady = TalkMind.Load(TalkPath()) || TalkMind.Load(FTalkSchool::Path(4));
}

FTalkMoment UMindComponent::TalkMomentWith(AActor* Other, int32 Part) const
{
    FTalkMoment M;
    const bool bTalking = Other && Dialogue.With == Other;
    if (bTalking)
    {
        M.TheirMove = Dialogue.TheirMove;
        M.TheirKind = Dialogue.TheirPoint.Kind;
        M.TheirValence = Dialogue.TheirPoint.Valence;
        for (const FString& Id : Dialogue.Covered)
        {
            const int32 Kind = FCString::Atoi(*Id);
            if (Kind > 0 && Kind < 32)
            {
                M.Covered |= 1u << Kind;
            }
        }
        M.Turns = Dialogue.Turns;
        M.bAskedHow = Dialogue.bAskedHow;
        M.bToldHow = Dialogue.bToldHow;
        M.bAskedAdvice = Dialogue.bAskedAdvice;
        M.bLost = Dialogue.bLost;
    }
    M.Part = Part;
    M.bFirstWords = !bTalking || Dialogue.MyMove == EDialogueMove::None;
    M.bOpening = !bTalking || (Dialogue.MyMove == EDialogueMove::None && Dialogue.TheirMove == EDialogueMove::None);
    if (const UHumanWorldSubsystem* World = GetWorldMind())
    {
        M.bLate = World->Now.Hour >= 22 || World->Now.Hour < 5;
    }
    M.bBusy = bActionActive && CurrentAction != EActionType::Talk && IsAbsorbing(CurrentAction);
    if (const ACompleteHumanNPC* Them = Cast<ACompleteHumanNPC>(Other))
    {
        M.bThemChild = Them->IdentityComponent && Them->IdentityComponent->Age < 13.0f;
    }
    if (Social && Other)
    {
        M.Closeness = Social->GetCloseness(Other);
        M.Trust = Social->GetTrust(Other);
        if (const FRelationship* R = Social->Find(Other))
        {
            M.Liking = R->Liking;
            M.Familiarity = R->Familiarity;
            M.bKnowName = !R->KnownName.IsEmpty();
        }
    }
    M.bSeekingAdvice = bActionActive && Other && ActiveIntention.Affordance.Target == Other
        && ActiveIntention.Affordance.Key.ToString().StartsWith(TEXT("совет@"));
    if (Emotions)
    {
        M.Mood = Emotions->GetAffect().Pleasure;
    }
    if (Needs)
    {
        M.Urgency = Needs->GetUrgency(Needs->GetMostUrgent());
        M.Contact = Needs->GetSatisfaction(ENeedType::SocialContact);
        M.Esteem = Needs->GetSatisfaction(ENeedType::Esteem);
    }
    if (Personality)
    {
        M.Extraversion = Personality->Traits.Extraversion;
        M.Agreeableness = Personality->Traits.Agreeableness;
        M.Openness = Personality->Traits.Openness;
    }
    return M;
}

FTalkKnowledge UMindComponent::TalkKnowledgeWith(AActor* Other, const FLineContext& C)
{
    FTalkKnowledge K;
    const bool bTalking = Other && Dialogue.With == Other;
    const FTalkingPoint TP = bTalking ? Dialogue.TheirPoint : FTalkingPoint();
    const EDialogueMove TheirMove = bTalking ? Dialogue.TheirMove : EDialogueMove::None;
    const ENeedType TheirNeed = TP.Kind == ETalkKind::Trouble ? ENeedType::Money : TP.Need;
    const bool bNeedTalk = TP.Kind == ETalkKind::Need || TP.Kind == ETalkKind::Trouble || TheirMove == EDialogueMove::AskAdvice;
    K.bKnowPlaceForTheirNeed = bNeedTalk && KnownPlaceFor(TheirNeed) != nullptr;
    K.bFeelTheirNeed = TP.Kind == ETalkKind::Need && Needs && Needs->GetUrgency(TP.Need) > 0.45f;
    for (const FReadingBookmark& Mark : Bookmarks)
    {
        K.bReadTheirBook |= TP.Kind == ETalkKind::Book && Mark.Subject == TP.Key && Mark.Page > TP.Page;
    }
    K.bCanRead = Mastery(TEXT("Reading")) >= 0.12f;
    if (Memory && TP.Kind == ETalkKind::Place)
    {
        for (const FKnownLocation& P : Memory->Places)
        {
            if (FVector::Dist2D(P.Location, TP.Where) < 800.0f)
            {
                K.bKnowTheirPlace = true;
                K.bAgreeTheirPlace = (P.Affect >= 0.0f) == (TP.Valence >= 0.0f);
                break;
            }
        }
    }
    if (TP.Kind == ETalkKind::Person && Social && TP.Person)
    {
        K.bTheirPersonIsMe = TP.Person == GetOwner();
        if (const FRelationship* R = Social->Find(TP.Person))
        {
            K.bKnowTheirPerson = R->Familiarity > 0.2f;
            K.bAgreeTheirPerson = (R->Liking + (R->Respect - 0.5f) > 0.0f) == (TP.Valence > 0.0f);
        }
    }
    if (TheirMove == EDialogueMove::Ask)
    {
        if (TP.Kind == ETalkKind::Book)
        {
            const FTextbook* Book = FLibrary::Find(TP.Key);
            K.bKnowAnswer = Book && Book->Pages.IsValidIndex(TP.Page);
        }
        else if (TP.Kind == ETalkKind::Place)
        {
            K.bKnowAnswer = TP.bHasWhere;
        }
    }
    K.bHaveSkill = TP.Kind == ETalkKind::Skill && Mastery(TP.Key) >= 0.3f;
    K.bHaveMoney = Identity && Identity->Money > 40.0f;
    if (bTalking && (TP.IsValid() || !Dialogue.TheirText.IsEmpty()))
    {
        FString Order;
        FUtterance OrderExtra;
        K.bCanInstruct = TryInstruct(C, Order, OrderExtra);
    }
    if (Needs)
    {
        const ENeedType N = Needs->GetMostUrgent();
        K.bUrgentNeed = Needs->GetUrgency(N) > 0.45f && !FDialogueLines::Feel(C, N).IsEmpty();
        K.bKnowPlaceForMyNeed = KnownPlaceFor(N) != nullptr;
    }
    K.bDoing = bActionActive && CurrentAction != EActionType::Talk && !FDialogueLines::Doing(C, CurrentAction, bMoving).IsEmpty();
    TArray<FTalkingPoint> Topics;
    GatherOwnTopics(Other, Topics);
    for (const FTalkingPoint& P : Topics)
    {
        K.bFeeling |= P.Kind == ETalkKind::Feeling;
        K.bHaveBook |= P.Kind == ETalkKind::Book;
        K.bHavePlace |= P.Kind == ETalkKind::Place;
        K.bHavePerson |= P.Kind == ETalkKind::Person;
        K.bHaveDream |= P.Kind == ETalkKind::Dream;
        K.bHaveSkillToTell |= P.Kind == ETalkKind::Skill;
        K.bHaveTrouble |= P.Kind == ETalkKind::Trouble;
        K.bHaveWork |= P.Kind == ETalkKind::Work;
    }
    K.bTheirTroubleIsMoney = TP.Kind == ETalkKind::Trouble && TP.Key == TEXT("NoMoney");
    K.bTheirTroubleIsJob = TP.Kind == ETalkKind::Trouble && TP.Key == TEXT("NoJob");
    K.bKnowPlaceForJob = KnownPlaceFor(ENeedType::Money) != nullptr;
    return K;
}

bool UMindComponent::RealizeTalk(ETalkChoice Choice, AActor* Other, const FLineContext& C, FString& Text, EDialogueMove& Move,
    FTalkingPoint& Point, FUtterance& Extra)
{
    Text.Reset();
    Move = FTalkRules::MoveOf(Choice);
    Point = FTalkingPoint();
    Extra = FUtterance();
    const bool bTalking = Other && Dialogue.With == Other;
    const FTalkingPoint TP = bTalking ? Dialogue.TheirPoint : FTalkingPoint();
    const ACompleteHumanNPC* Them = Cast<ACompleteHumanNPC>(Other);
    const FVector TheirPos = Them ? Them->GetActorLocation() : FVector::ZeroVector;
    const float MyMood = C.Mood;
    const ENeedType MyNeed = Needs ? Needs->GetMostUrgent() : ENeedType::Hunger;
    const float MyUrgency = Needs ? Needs->GetUrgency(MyNeed) : 0.0f;

    auto Topic = [this, Other](ETalkKind Kind, FTalkingPoint& Out)
    {
        TArray<FTalkingPoint> Topics;
        GatherOwnTopics(Other, Topics);
        float Best = -1.0f;
        for (const FTalkingPoint& P : Topics)
        {
            if (P.Kind == Kind && P.Weight > Best)
            {
                Best = P.Weight;
                Out = P;
            }
        }
        return Best >= 0.0f;
    };

    switch (Choice)
    {
    case ETalkChoice::Nothing:
        return true;
    case ETalkChoice::Greet:
        Text = FDialogueLines::Greet(C);
        break;
    case ETalkChoice::GreetBack:
        Text = FDialogueLines::GreetBack(C);
        break;
    case ETalkChoice::Introduce:
        Text = FDialogueLines::Introduce(C);
        break;
    case ETalkChoice::IntroduceBack:
        Text = FDialogueLines::IntroduceBack(C);
        break;
    case ETalkChoice::AskHow:
        Text = (bTalking && Dialogue.MyMove != EDialogueMove::None) ? FDialogueLines::HowAreYou(C)
             : FDialogueLines::Greet(C) + TEXT(" ") + FDialogueLines::HowAreYou(C);
        break;
    case ETalkChoice::AndYou:
        Text = FDialogueLines::AndYou(C);
        break;
    case ETalkChoice::TellNeed:
    {
        const FString Why = FDialogueLines::Feel(C, MyNeed);
        if (Why.IsEmpty())
        {
            return false;
        }
        Text = FString::Printf(TEXT("%s %s."), *FDialogueLines::MoodWord(C, MyMood), *FDialogueLines::Capital(Why));
        Point.Kind = ETalkKind::Need; Point.Need = MyNeed; Point.About = HumanText::Need(MyNeed); Point.Valence = -MyUrgency;
        if (Dialogue.TheirMove != EDialogueMove::HowAreYou)
        {
            Move = EDialogueMove::Tell;
        }
        break;
    }
    case ETalkChoice::TellDeed:
    {
        const FString Busy = FDialogueLines::Doing(C, CurrentAction, bMoving);
        if (Busy.IsEmpty())
        {
            return false;
        }
        Text = FString::Printf(TEXT("%s %s."), *FDialogueLines::MoodWord(C, MyMood), *FDialogueLines::Capital(Busy));
        Point.Kind = ETalkKind::Deed; Point.Action = CurrentAction; Point.About = HumanText::Action(CurrentAction);
        if (Dialogue.TheirMove != EDialogueMove::HowAreYou)
        {
            Move = EDialogueMove::Tell;
        }
        break;
    }
    case ETalkChoice::TellMood:
        Text = FDialogueLines::MoodWord(C, MyMood);
        Point.Kind = ETalkKind::Feeling; Point.About = TEXT("настроение"); Point.Valence = MyMood;
        break;
    case ETalkChoice::AdvisePlace:
    {
        const ENeedType Need = (TP.Kind == ETalkKind::Trouble || TP.Key == TEXT("NoJob")) ? ENeedType::Money : TP.Need;
        const FKnownLocation* Place = KnownPlaceFor(Need);
        if (!Place)
        {
            return false;
        }
        const EPlaceKind K = RealKindAt(Place->Location, Place->Kind);
        Text = FDialogueLines::AdvisePlace(C, K, FDialogueLines::WayTo(TheirPos, Place->Location));
        Point = TP;
        Point.Need = Need; Point.PlaceKind = K; Point.Where = Place->Location; Point.bHasWhere = true;
        Extra.PlacePayload = *Place;
        Extra.bHasPlacePayload = true;
        break;
    }
    case ETalkChoice::SameHere:
        Text = FDialogueLines::SameHere(C, TP.Need);
        break;
    case ETalkChoice::Acknowledge:
        Text = FDialogueLines::Acknowledge(C);
        break;
    case ETalkChoice::ReactDoing:
        Text = FDialogueLines::ReactDoing(C, TP.Action);
        break;
    case ETalkChoice::KnowThat:
        Text = FDialogueLines::KnowThat(C);
        break;
    case ETalkChoice::CantRead:
        Text = FDialogueLines::CantRead(C);
        break;
    case ETalkChoice::AskWhatWritten:
        Text = FDialogueLines::AskWhatWritten(C);
        Point = TP;
        break;
    case ETalkChoice::AgreePlace:
    case ETalkChoice::DisagreePlace:
    {
        const FKnownLocation* Mine = nullptr;
        if (Memory)
        {
            for (const FKnownLocation& P : Memory->Places)
            {
                if (FVector::Dist2D(P.Location, TP.Where) < 800.0f)
                {
                    Mine = &P;
                    break;
                }
            }
        }
        if (!Mine)
        {
            return false;
        }
        Text = Choice == ETalkChoice::AgreePlace ? FDialogueLines::AgreePlace(C, TP.PlaceKind, Mine->Affect)
                                                 : FDialogueLines::DisagreePlace(C, TP.PlaceKind, Mine->Affect);
        break;
    }
    case ETalkChoice::AskWhere:
        Text = FDialogueLines::AskWhere(C);
        Point = TP;
        break;
    case ETalkChoice::AgreePerson:
    case ETalkChoice::DisagreePerson:
    {
        const FRelationship* R = (Social && TP.Person) ? Social->Find(TP.Person) : nullptr;
        if (!R)
        {
            return false;
        }
        const bool bMineGood = R->Liking + (R->Respect - 0.5f) > 0.0f;
        Text = Choice == ETalkChoice::AgreePerson ? FDialogueLines::AgreePerson(C, TP.About, TP.bFemale, bMineGood)
                                                  : FDialogueLines::DisagreePerson(C, TP.bFemale, TP.Valence > 0.0f);
        break;
    }
    case ETalkChoice::DontKnowPerson:
        Text = FDialogueLines::DontKnowPerson(C, TP.bFemale);
        break;
    case ETalkChoice::AskWhatHappened:
        Text = FDialogueLines::AskWhatHappened(C);
        Point = TP;
        break;
    case ETalkChoice::Console:
        Text = FDialogueLines::Console(C);
        Point = TP;
        break;
    case ETalkChoice::GladForYou:
        Text = FDialogueLines::GladForYou(C);
        break;
    case ETalkChoice::AgreeWeather:
        Text = FDialogueLines::AgreeWeather(C);
        break;
    case ETalkChoice::ReactDream:
        Text = FDialogueLines::ReactDream(C);
        break;
    case ETalkChoice::AskWhatStops:
        Text = FDialogueLines::ReactDream(C) + TEXT(" ") + FDialogueLines::AskWhatStops(C);
        Point = TP;
        break;
    case ETalkChoice::AskTeach:
        Text = FDialogueLines::AskTeach(C);
        Point = TP;
        break;
    case ETalkChoice::OfferMoney:
        if (!Identity || Identity->Money < 40.0f)
        {
            return false;
        }
        Text = FDialogueLines::OfferMoney(C);
        Extra.MoneyGiven = 10.0f;
        Point = TP;
        break;
    case ETalkChoice::Instruct:
        if (!TryInstruct(C, Text, Extra))
        {
            return false;
        }
        Point = Extra.Point;
        break;
    case ETalkChoice::Encourage:
        Text = FDialogueLines::Encourage(C);
        break;
    case ETalkChoice::AskPay:
        Text = FDialogueLines::AskPay(C);
        Point = TP;
        break;
    case ETalkChoice::Retell:
    {
        const FTextbook* Book = FLibrary::Find(TP.Key);
        if (!Book || !Book->Pages.IsValidIndex(TP.Page))
        {
            return false;
        }
        Text = FDialogueLines::Retell(C, FDialogueLines::FirstSentence(Book->Pages[TP.Page].Text));
        Extra.BookSubject = TP.Key;
        Extra.BookPage = TP.Page;
        Point = TP;
        break;
    }
    case ETalkChoice::AnswerWhere:
        if (!TP.bHasWhere)
        {
            return false;
        }
        Text = FDialogueLines::AnswerWhere(C, TP.PlaceKind, FDialogueLines::WayTo(TheirPos, TP.Where));
        if (Memory)
        {
            for (const FKnownLocation& P : Memory->Places)
            {
                if (FVector::Dist2D(P.Location, TP.Where) < 800.0f)
                {
                    Extra.PlacePayload = P;
                    Extra.bHasPlacePayload = true;
                    break;
                }
            }
        }
        Point = TP;
        break;
    case ETalkChoice::AnswerWhatHappened:
        Text = FDialogueLines::AnswerWhatHappened(C, TP.Detail.IsEmpty() ? FeelingReason() : TP.Detail);
        break;
    case ETalkChoice::AnswerWhatStops:
        Text = FDialogueLines::AnswerWhatStops(C, Identity && Identity->Money < 30.0f,
            Personality && Personality->Traits.Neuroticism > 0.6f);
        break;
    case ETalkChoice::AgreeTeach:
        if (Mastery(TP.Key) < 0.3f)
        {
            return false;
        }
        Text = FDialogueLines::AgreeTeach(C);
        Extra.SkillPayload = TP.Key;
        Extra.SkillPayloadLevel = Mastery(TP.Key);
        Point = TP;
        break;
    case ETalkChoice::RefuseTeach:
        Text = FDialogueLines::RefuseTeach(C);
        break;
    case ETalkChoice::AnswerPay:
        Text = FDialogueLines::AnswerPay(C, Identity && Identity->HourlyWage >= 9.0f);
        break;
    case ETalkChoice::DontKnow:
        Text = (TP.Kind == ETalkKind::Place || Dialogue.TheirMove == EDialogueMove::AskAdvice)
            ? FDialogueLines::DontKnowWhere(C) : FDialogueLines::Acknowledge(C);
        break;
    case ETalkChoice::Thanks:
        Text = TP.Kind == ETalkKind::Book ? FDialogueLines::DidntKnow(C) + TEXT(" ") + FDialogueLines::Thanks(C) : FDialogueLines::Thanks(C);
        break;
    case ETalkChoice::ThanksForKindness:
        Text = FDialogueLines::ThanksForKindness(C);
        break;
    case ETalkChoice::Obey:
        Text = FDialogueLines::Obey(C);
        break;
    case ETalkChoice::RefuseOrder:
        Text = FDialogueLines::RefuseOrder(C, MyUrgency > 0.5f ? FDialogueLines::Feel(C, MyNeed) : FString());
        break;
    case ETalkChoice::AcceptMoney:
        Text = FDialogueLines::AcceptMoney(C);
        break;
    case ETalkChoice::NoProblem:
        Text = FDialogueLines::NoProblem(C);
        break;
    case ETalkChoice::FarewellBack:
        Text = FDialogueLines::FarewellBack(C);
        break;
    case ETalkChoice::Farewell:
    {
        const UHumanWorldSubsystem* World = GetWorldMind();
        const bool bLate = World && (World->Now.Hour >= 22 || World->Now.Hour < 5);
        Text = FDialogueLines::Farewell(C, FDialogueLines::LeaveReason(C, MyNeed, MyUrgency, EActionType::Idle, bLate));
        break;
    }
    case ETalkChoice::AskAdvice:
    {
        const FString Where = FDialogueLines::AskWhereFor(C, MyNeed);
        if (Identity && !Identity->bEmployed && Identity->Age >= 16.0f && Needs && Needs->GetUrgency(ENeedType::Money) >= MyUrgency - 0.1f)
        {
            Text = FDialogueLines::TellNoJob(C) + TEXT(" ") + FDialogueLines::AskForAdvice(C);
            Point.Kind = ETalkKind::Trouble; Point.About = TEXT("работа"); Point.Key = TEXT("NoJob");
        }
        else if (!Where.IsEmpty())
        {
            Text = Where;
            Point.Kind = ETalkKind::Need; Point.Need = MyNeed; Point.About = HumanText::Need(MyNeed);
        }
        else
        {
            Text = FDialogueLines::AskForAdvice(C);
            Point.Kind = ETalkKind::Trouble; Point.About = TEXT("жизнь");
        }
        break;
    }
    case ETalkChoice::TellBook:
    {
        FTalkingPoint Mine;
        const FTextbook* Book = Topic(ETalkKind::Book, Mine) ? FLibrary::Find(Mine.Key) : nullptr;
        if (!Book)
        {
            return false;
        }
        Text = FDialogueLines::TellReading(C, Book->Title, Book->Pages.IsValidIndex(Mine.Page) ? Book->Pages[Mine.Page].Title : FString());
        Point = Mine;
        break;
    }
    case ETalkChoice::TellPlace:
    {
        FTalkingPoint Mine;
        if (!Topic(ETalkKind::Place, Mine))
        {
            return false;
        }
        Text = FDialogueLines::TellPlace(C, Mine.PlaceKind, Mine.Valence);
        Point = Mine;
        break;
    }
    case ETalkChoice::TellPerson:
    {
        FTalkingPoint Mine;
        const FRelationship* R = (Topic(ETalkKind::Person, Mine) && Social) ? Social->Find(Mine.Person) : nullptr;
        if (!R)
        {
            return false;
        }
        const EPersonReason Why = R->Debt > 0.2f ? EPersonReason::Helped
            : R->Respect > 0.7f ? EPersonReason::Respected
            : R->CaughtLying > 0 ? EPersonReason::Liar
            : R->Resentment > 0.35f ? EPersonReason::Offended
            : R->Liking > 0.25f ? EPersonReason::Pleasant
            : EPersonReason::Unpleasant;
        Text = FDialogueLines::TellPerson(C, Mine.About, Mine.bFemale, Why, R->Respect > 0.78f && R->Trust > 0.55f);
        Point = Mine;
        break;
    }
    case ETalkChoice::TellFeeling:
    {
        FTalkingPoint Mine;
        if (!Topic(ETalkKind::Feeling, Mine))
        {
            return false;
        }
        Text = FDialogueLines::TellFeeling(C, EEmotionType(Mine.Page));
        Point = Mine;
        break;
    }
    case ETalkChoice::TellWeather:
    {
        Text = FDialogueLines::TellWeather(C);
        Point.Kind = ETalkKind::Weather; Point.About = TEXT("погода");
        Point.Valence = GetWorldMind() ? 0.3f - GetWorldMind()->Weather * 0.6f : 0.0f;
        break;
    }
    case ETalkChoice::TellDream:
    {
        FTalkingPoint Mine;
        if (!Topic(ETalkKind::Dream, Mine))
        {
            return false;
        }
        Text = FDialogueLines::TellDream(C, Mine.About);
        Point = Mine;
        break;
    }
    case ETalkChoice::TellSkill:
    {
        FTalkingPoint Mine;
        if (!Topic(ETalkKind::Skill, Mine))
        {
            return false;
        }
        Text = FDialogueLines::TellSkill(C, Mine.Key);
        Point = Mine;
        break;
    }
    case ETalkChoice::TellTrouble:
    {
        FTalkingPoint Mine;
        if (!Topic(ETalkKind::Trouble, Mine))
        {
            return false;
        }
        Text = Mine.Key == TEXT("NoJob") ? FDialogueLines::TellNoJob(C) : FDialogueLines::TellNoMoney(C);
        Point = Mine;
        break;
    }
    case ETalkChoice::TellWork:
    {
        FTalkingPoint Mine;
        if (!Topic(ETalkKind::Work, Mine))
        {
            return false;
        }
        Text = FDialogueLines::TellWork(C, Mine.PlaceKind);
        Point = Mine;
        break;
    }
    default:
        return false;
    }
    return !Text.IsEmpty();
}

void UMindComponent::CommitTalk(ETalkChoice Choice, AActor* Other)
{
    switch (Choice)
    {
    case ETalkChoice::AskHow:
    case ETalkChoice::AndYou:
        Dialogue.bAskedHow = true;
        break;
    case ETalkChoice::TellNeed:
    case ETalkChoice::TellDeed:
    case ETalkChoice::TellMood:
        Dialogue.bToldHow = true;
        break;
    case ETalkChoice::AskAdvice:
        Dialogue.bAskedAdvice = true;
        break;
    case ETalkChoice::RefuseOrder:
        Suggestions.RemoveAll([Other](const FSuggestion& S) { return S.From == Other && S.bInstruction; });
        break;
    default:
        break;
    }
}

void UMindComponent::TakeTurnLearned()
{
    AActor* Other = Dialogue.With;
    const ACompleteHumanNPC* Them = Cast<ACompleteHumanNPC>(Other);
    if (!Them)
    {
        EndConversation();
        return;
    }
    Dialogue.bMyTurn = false;
    const FLineContext C = MakeLineContext(Other);
    const FTalkKnowledge Know = TalkKnowledgeWith(Other, C);
    const float Impaired = Body ? Body->GetCognitiveImpairment() : 0.0f;

    FString Text;
    EDialogueMove Move = EDialogueMove::React;
    FTalkingPoint Point;
    FUtterance Extra;
    ETalkChoice Said[2] = { ETalkChoice::Nothing, ETalkChoice::Nothing };
    bool bEnds = false;
    for (int32 Part = 0; Part < 2; ++Part)
    {
        if (Part == 1 && (bEnds || FTalkRules::Expects(Said[0])))
        {
            break;
        }
        const FTalkMoment Moment = TalkMomentWith(Other, Part);
        TArray<FTalkOption> Menu;
        FTalkRules::Menu(Moment, Know, Menu);
        TArray<FTalkOption> Sayable;
        TArray<FString> Texts;
        TArray<EDialogueMove> Moves;
        TArray<FTalkingPoint> Points;
        TArray<FUtterance> Extras;
        for (FTalkOption& Option : Menu)
        {
            FString Line;
            EDialogueMove LineMove = EDialogueMove::React;
            FTalkingPoint LinePoint;
            FUtterance LineExtra;
            if (!RealizeTalk(Option.Choice, Other, C, Line, LineMove, LinePoint, LineExtra))
            {
                continue;
            }
            Option.Words = (Line.IsEmpty() || !Speech) ? 1.0f : Speech->Comprehension(Line);
            Sayable.Add(Option);
            Texts.Add(Line);
            Moves.Add(LineMove);
            Points.Add(LinePoint);
            Extras.Add(LineExtra);
        }
        if (Sayable.Num() == 0)
        {
            break;
        }
        const int32 Pick = TalkMind.Choose(Moment, Sayable, TalkRng, 0.08f + 0.2f * Impaired);
        if (!Sayable.IsValidIndex(Pick))
        {
            break;
        }
        const ETalkChoice Choice = Sayable[Pick].Choice;
        Said[Part] = Choice;
        TalkMoments[Part] = Moment;
        TalkOptions[Part] = Sayable[Pick];
        bTalkOpen[Part] = true;
        CommitTalk(Choice, Other);
        if (Choice == ETalkChoice::Nothing)
        {
            continue;
        }
        const FString Piece = FDialogueLines::Capital(Texts[Pick]);
        Text = Text.IsEmpty() ? Piece : Text + TEXT(" ") + Piece;
        Move = Moves[Pick];
        if (Points[Pick].IsValid() || Part == 0)
        {
            Point = Points[Pick];
        }
        const FUtterance& Adds = Extras[Pick];
        if (Adds.bHasPlacePayload)
        {
            Extra.PlacePayload = Adds.PlacePayload;
            Extra.bHasPlacePayload = true;
        }
        if (!Adds.SkillPayload.IsNone())
        {
            Extra.SkillPayload = Adds.SkillPayload;
            Extra.SkillPayloadLevel = Adds.SkillPayloadLevel;
        }
        if (Adds.MoneyGiven > 0.0f)
        {
            Extra.MoneyGiven = Adds.MoneyGiven;
        }
        if (!Adds.BookSubject.IsNone())
        {
            Extra.BookSubject = Adds.BookSubject;
            Extra.BookPage = Adds.BookPage;
        }
        if (Adds.bHasPayload)
        {
            Extra.Payload = Adds.Payload;
            Extra.bHasPayload = true;
        }
        bEnds |= FTalkRules::Ends(Choice);
    }
    Extra.TalkReply = static_cast<uint8>(Said[0]);
    Extra.TalkOwn = static_cast<uint8>(Said[1]);
    if (Text.IsEmpty())
    {
        EndConversation();
        return;
    }
    Say(Text, Move, Point, Extra);
    if (bEnds)
    {
        EndConversation();
    }
}

bool UMindComponent::ChooseOpening(AActor* Other, bool bMeantToTalk, ETalkChoice& OutChoice)
{
    OutChoice = ETalkChoice::Nothing;
    if (!bTalkReady || !Other)
    {
        return false;
    }
    const FLineContext C = MakeLineContext(Other);
    const FTalkMoment Moment = TalkMomentWith(Other, 0);
    const FTalkKnowledge Know = TalkKnowledgeWith(Other, C);
    TArray<FTalkOption> Menu;
    FTalkRules::Menu(Moment, Know, Menu);
    TArray<FTalkOption> Sayable;
    for (FTalkOption& Option : Menu)
    {
        if (bMeantToTalk && Option.Choice == ETalkChoice::Nothing)
        {
            continue;
        }
        FString Line;
        EDialogueMove LineMove = EDialogueMove::React;
        FTalkingPoint LinePoint;
        FUtterance LineExtra;
        if (!RealizeTalk(Option.Choice, Other, C, Line, LineMove, LinePoint, LineExtra))
        {
            continue;
        }
        Option.Words = (Line.IsEmpty() || !Speech) ? 1.0f : Speech->Comprehension(Line);
        Sayable.Add(Option);
    }
    if (Sayable.Num() == 0)
    {
        return false;
    }
    const int32 Pick = TalkMind.Choose(Moment, Sayable, TalkRng, 0.08f);
    if (!Sayable.IsValidIndex(Pick))
    {
        return false;
    }
    OutChoice = Sayable[Pick].Choice;
    if (OutChoice == ETalkChoice::Nothing)
    {
        if (TalkRng.FRand() < 0.15f)
        {
            TalkMind.Remember(Moment, Sayable[Pick], FTalkOutcome{ 0.0f, 0.0f, 0.0f, 0.0f });
        }
        return true;
    }
    TalkMoments[0] = Moment;
    TalkOptions[0] = Sayable[Pick];
    bTalkOpen[0] = true;
    bTalkOpen[1] = false;
    return true;
}

float UMindComponent::TalkGain(const FUtterance& U) const
{
    if (U.MoneyGiven > 0.0f)
    {
        return 1.0f;
    }
    if (U.Move == EDialogueMove::Offer && !U.SkillPayload.IsNone())
    {
        return 1.0f;
    }
    if (U.bHasPlacePayload && (U.Move == EDialogueMove::Advise || U.Move == EDialogueMove::Instruct || U.Move == EDialogueMove::Answer))
    {
        const bool bNew = !Memory || !Memory->KnowsPlace(U.PlacePayload.Kind, U.PlacePayload.Location);
        return bNew ? 1.0f : 0.3f;
    }
    if (!U.BookSubject.IsNone() && U.BookPage >= 0)
    {
        return 0.6f;
    }
    return 0.0f;
}

float UMindComponent::FeltAbout(const FUtterance& U, float Grasp) const
{
    FTalkHeard Heard;
    Heard.HearerSaid = Dialogue.MyMove;
    Heard.HearerKind = Dialogue.MyPoint.Kind;
    Heard.Heard = ChoiceOf(U.TalkReply, U.Move);
    Heard.bOnTopic = U.Point.Kind != ETalkKind::None && U.Point.Kind == Dialogue.MyPoint.Kind;
    Heard.bUseful = TalkGain(U) > 0.5f;
    Heard.bHearerSad = Emotions && Emotions->GetIntensity(EEmotionType::Sadness) > 0.4f;
    Heard.bUnderstood = Grasp >= 0.35f;
    Heard.SpeakerTone = U.Tone;
    float Felt = FTalkRules::Felt(Heard);
    if (U.TalkOwn < static_cast<uint8>(ETalkChoice::Count) && U.TalkOwn != static_cast<uint8>(ETalkChoice::Nothing))
    {
        FTalkHeard More = Heard;
        More.HearerSaid = EDialogueMove::React;
        More.Heard = static_cast<ETalkChoice>(U.TalkOwn);
        More.SpeakerTone = 0.0f;
        Felt += FTalkRules::Felt(More);
    }
    return FMath::Clamp(Felt, -1.0f, 1.0f);
}

void UMindComponent::ResolveTalk(const FUtterance* Reply)
{
    if (!bTalkReady || (!bTalkOpen[0] && !bTalkOpen[1]))
    {
        return;
    }
    FTalkOutcome Outcome{ 0.0f, 0.0f, 0.0f, 0.0f };
    if (Reply)
    {
        Outcome.Tone = FMath::Clamp(Reply->Tone, -1.0f, 1.0f);
        Outcome.Stayed = Reply->Move == EDialogueMove::Farewell ? 0.0f : 1.0f;
        Outcome.Gain = TalkGain(*Reply);
        float Warm = FTalkRules::Esteem(ChoiceOf(Reply->TalkReply, Reply->Move));
        if (Reply->TalkOwn < static_cast<uint8>(ETalkChoice::Count))
        {
            Warm += FTalkRules::Esteem(static_cast<ETalkChoice>(Reply->TalkOwn));
        }
        Outcome.Warmth = FMath::Clamp(Warm, -1.0f, 1.0f);
    }
    for (int32 Part = 0; Part < 2; ++Part)
    {
        if (bTalkOpen[Part])
        {
            TalkMind.Remember(TalkMoments[Part], TalkOptions[Part], Outcome);
            bTalkOpen[Part] = false;
            ++TalkLearned;
            ++TalkSinceSave;
        }
    }
    TalkMind.Practice(2, TalkRng);
    if (TalkSinceSave >= 30)
    {
        TalkSinceSave = 0;
        TalkMind.Save(TalkPath());
    }
}
