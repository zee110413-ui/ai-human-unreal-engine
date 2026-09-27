#include "TalkLearning.h"
#include "MindLearning.h"
#include "Misc/Paths.h"

namespace
{
    constexpr int32 NeedKinds = 5;
    constexpr int32 JobBit = 5;
    constexpr int32 Books = 6;
    constexpr int32 Places = 8;
    constexpr int32 Skills = 4;
    constexpr int32 Others = 8;

    struct FTalker
    {
        float Extraversion = 0.5f;
        float Agreeableness = 0.5f;
        float Openness = 0.5f;
        float Mood = 0.0f;
        float Urgency = 0.0f;
        float Contact = 0.5f;
        float Esteem = 0.5f;
        float Literacy = 0.8f;
        int32 TopNeed = -1;
        uint32 KnowPlaceFor = 0;
        uint32 BooksRead = 0;
        uint32 PlacesKnown = 0;
        uint32 PlacesLiked = 0;
        uint32 SkillsHeld = 0;
        uint32 PeopleKnown = 0;
        uint32 PeopleLiked = 0;
        int32 Feeling = 0;
        bool bBusy = false;
        bool bCanRead = true;
        bool bHaveMoney = true;
        bool bEmployed = true;
        bool bDream = false;
        bool bChild = false;
        bool bLate = false;
    };

    struct FBond
    {
        float Closeness = 0.0f;
        float Liking = 0.0f;
        float Trust = 0.3f;
        float Familiarity = 0.0f;
        bool bKnowName = false;
    };

    struct FSaid
    {
        ETalkChoice Choice = ETalkChoice::Nothing;
        ETalkKind Kind = ETalkKind::None;
        EDialogueMove Move = EDialogueMove::None;
        int32 Subject = -1;
        float Valence = 0.0f;
        bool bUseful = false;
    };

    struct FSide
    {
        EDialogueMove LastMove = EDialogueMove::None;
        ETalkKind LastKind = ETalkKind::None;
        int32 LastSubject = -1;
        float LastValence = 0.0f;
        float LastFelt = 0.0f;
        bool bAskedHow = false;
        bool bToldHow = false;
        bool bAskedAdvice = false;
        bool bSpoke = false;
        FTalkMoment Moment[2];
        FTalkOption Option[2];
        bool bPending[2] = { false, false };
        ETalkChoice AskedFor = ETalkChoice::Nothing;
        int32 AskedSubject = -1;
    };

    int32 PickBit(uint32 Bits, int32 Width, FRandomStream& Rng)
    {
        TArray<int32> Set;
        for (int32 B = 0; B < Width; ++B)
        {
            if (Bits & (1u << B))
            {
                Set.Add(B);
            }
        }
        return Set.Num() > 0 ? Set[Rng.RandRange(0, Set.Num() - 1)] : -1;
    }

    uint32 RandomBits(int32 Width, float Chance, FRandomStream& Rng)
    {
        uint32 Bits = 0;
        for (int32 B = 0; B < Width; ++B)
        {
            if (Rng.FRand() < Chance)
            {
                Bits |= 1u << B;
            }
        }
        return Bits;
    }

    FTalker Born(int32 Kind, FRandomStream& Rng)
    {
        FTalker T;
        T.Extraversion = Rng.FRand();
        T.Agreeableness = Rng.FRand();
        T.Openness = Rng.FRand();
        T.Mood = Rng.FRandRange(-0.6f, 0.7f);
        T.Contact = Rng.FRandRange(0.1f, 1.0f);
        T.Esteem = Rng.FRandRange(0.2f, 0.9f);
        T.bChild = Kind <= 1;
        T.Literacy = T.bChild ? Rng.FRandRange(0.2f, 0.7f) : Rng.FRandRange(0.5f, 1.0f);
        T.bCanRead = Rng.FRand() < (T.bChild ? 0.3f : 0.75f);
        T.Urgency = Rng.FRand() < 0.45f ? Rng.FRandRange(0.45f, 0.95f) : Rng.FRandRange(0.0f, 0.4f);
        T.TopNeed = Rng.RandRange(0, NeedKinds - 1);
        T.KnowPlaceFor = RandomBits(JobBit + 1, 0.45f, Rng);
        T.BooksRead = T.bCanRead ? RandomBits(Books, 0.3f, Rng) : 0u;
        T.PlacesKnown = RandomBits(Places, 0.4f, Rng);
        T.PlacesLiked = RandomBits(Places, 0.6f, Rng);
        T.SkillsHeld = RandomBits(Skills, T.bChild ? 0.1f : 0.35f, Rng);
        T.PeopleKnown = RandomBits(Others, 0.4f, Rng);
        T.PeopleLiked = RandomBits(Others, 0.6f, Rng);
        T.Feeling = Rng.FRand() < 0.35f ? (Rng.FRand() < 0.5f ? 1 : 2) : 0;
        if (T.Feeling == 1)
        {
            T.Mood = FMath::Min(T.Mood, -0.3f);
        }
        T.bBusy = Rng.FRand() < 0.3f;
        T.bHaveMoney = !T.bChild && Rng.FRand() < 0.6f;
        T.bEmployed = !T.bChild && Rng.FRand() < 0.6f;
        T.bDream = Rng.FRand() < 0.4f;
        T.bLate = Rng.FRand() < 0.1f;
        return T;
    }

    FBond Bonded(float Familiar, FRandomStream& Rng)
    {
        FBond B;
        B.Familiarity = Familiar;
        B.Closeness = Familiar * Rng.FRandRange(0.0f, 0.8f);
        B.Liking = FMath::Clamp(Rng.FRandRange(-0.4f, 0.6f) * Familiar + Rng.FRandRange(-0.1f, 0.2f), -1.0f, 1.0f);
        B.Trust = FMath::Clamp(0.25f + Familiar * Rng.FRandRange(-0.2f, 0.5f), 0.0f, 1.0f);
        B.bKnowName = Familiar > 0.25f;
        return B;
    }

    FTalkMoment MomentOf(const FTalker& Me, const FBond& Bond, const FSide& Mine, const FSide& Theirs, const FTalker& Them,
        int32 Turns, int32 Part, uint32 Covered)
    {
        FTalkMoment M;
        M.TheirMove = Theirs.LastMove;
        M.TheirKind = Theirs.LastKind;
        M.TheirValence = Theirs.LastValence;
        M.Covered = Covered;
        M.Turns = Turns;
        M.Part = Part;
        M.bFirstWords = !Mine.bSpoke;
        M.bOpening = !Mine.bSpoke && !Theirs.bSpoke;
        M.bAskedHow = Mine.bAskedHow;
        M.bToldHow = Mine.bToldHow;
        M.bAskedAdvice = Mine.bAskedAdvice;
        M.bLate = Me.bLate;
        M.bBusy = Me.bBusy;
        M.bThemChild = Them.bChild;
        M.bKnowName = Bond.bKnowName;
        M.Closeness = Bond.Closeness;
        M.Liking = Bond.Liking;
        M.Trust = Bond.Trust;
        M.Familiarity = Bond.Familiarity;
        M.Mood = Me.Mood;
        M.Urgency = Me.Urgency;
        M.Contact = Me.Contact;
        M.Esteem = Me.Esteem;
        M.Extraversion = Me.Extraversion;
        M.Agreeableness = Me.Agreeableness;
        M.Openness = Me.Openness;
        return M;
    }

    FTalkKnowledge KnowledgeOf(const FTalker& Me, const FSide& Theirs, FRandomStream& Rng)
    {
        FTalkKnowledge K;
        const int32 S = Theirs.LastSubject;
        const auto Has = [](uint32 Bits, int32 Bit) { return Bit >= 0 && Bit < 32 && (Bits & (1u << Bit)) != 0; };
        const bool bNeedTopic = Theirs.LastKind == ETalkKind::Need;
        K.bKnowPlaceForTheirNeed = bNeedTopic && Has(Me.KnowPlaceFor, S);
        K.bFeelTheirNeed = bNeedTopic && Me.TopNeed == S && Me.Urgency > 0.45f;
        K.bReadTheirBook = Theirs.LastKind == ETalkKind::Book && Has(Me.BooksRead, S);
        K.bCanRead = Me.bCanRead;
        K.bKnowTheirPlace = Theirs.LastKind == ETalkKind::Place && Has(Me.PlacesKnown, S);
        K.bAgreeTheirPlace = Has(Me.PlacesLiked, S) == (Theirs.LastValence >= 0.0f);
        K.bKnowTheirPerson = Theirs.LastKind == ETalkKind::Person && Has(Me.PeopleKnown, S);
        K.bAgreeTheirPerson = Has(Me.PeopleLiked, S) == (Theirs.LastValence >= 0.0f);
        K.bKnowAnswer = (Theirs.LastKind == ETalkKind::Book && Has(Me.BooksRead, S))
                     || (Theirs.LastKind == ETalkKind::Place && Has(Me.PlacesKnown, S));
        K.bHaveSkill = Theirs.LastKind == ETalkKind::Skill && Has(Me.SkillsHeld, S);
        K.bHaveMoney = Me.bHaveMoney;
        K.bTheirTroubleIsMoney = Theirs.LastKind == ETalkKind::Trouble && S == 0;
        K.bTheirTroubleIsJob = Theirs.LastKind == ETalkKind::Trouble && S == 1;
        K.bKnowPlaceForJob = Has(Me.KnowPlaceFor, JobBit);
        K.bCanInstruct = !Me.bChild && ((bNeedTopic && Has(Me.KnowPlaceFor, S)) || (K.bTheirTroubleIsJob && K.bKnowPlaceForJob))
                      && Rng.FRand() < 0.6f;
        K.bUrgentNeed = Me.Urgency > 0.45f && Me.TopNeed >= 0;
        K.bKnowPlaceForMyNeed = Has(Me.KnowPlaceFor, Me.TopNeed);
        K.bDoing = Me.bBusy;
        K.bFeeling = Me.Feeling != 0;
        K.bHaveBook = Me.BooksRead != 0;
        K.bHavePlace = Me.PlacesKnown != 0;
        K.bHavePerson = Me.PeopleKnown != 0;
        K.bHaveDream = Me.bDream;
        K.bHaveSkillToTell = Me.SkillsHeld != 0;
        K.bHaveTrouble = !Me.bChild && (!Me.bEmployed || !Me.bHaveMoney);
        K.bHaveWork = Me.bEmployed;
        return K;
    }

    FSaid Voice(const FTalkOption& O, const FTalker& Me, const FSide& Theirs, FRandomStream& Rng)
    {
        FSaid Said;
        Said.Choice = O.Choice;
        Said.Kind = O.Kind;
        Said.Move = FTalkRules::MoveOf(O.Choice);
        Said.Valence = O.Valence;
        Said.Subject = Theirs.LastSubject;
        switch (O.Choice)
        {
        case ETalkChoice::TellNeed:
        case ETalkChoice::AskAdvice:
            Said.Subject = Me.TopNeed;
            break;
        case ETalkChoice::TellBook:
            Said.Subject = PickBit(Me.BooksRead, Books, Rng);
            break;
        case ETalkChoice::TellPlace:
            Said.Subject = PickBit(Me.PlacesKnown, Places, Rng);
            Said.Valence = (Me.PlacesLiked & (1u << FMath::Max(0, Said.Subject))) ? 0.5f : -0.5f;
            break;
        case ETalkChoice::TellPerson:
            Said.Subject = PickBit(Me.PeopleKnown, Others, Rng);
            Said.Valence = (Me.PeopleLiked & (1u << FMath::Max(0, Said.Subject))) ? 0.5f : -0.5f;
            break;
        case ETalkChoice::TellSkill:
            Said.Subject = PickBit(Me.SkillsHeld, Skills, Rng);
            break;
        case ETalkChoice::TellTrouble:
            Said.Subject = !Me.bHaveMoney ? 0 : 1;
            break;
        case ETalkChoice::TellFeeling:
        case ETalkChoice::TellMood:
            Said.Valence = Me.Feeling == 1 ? -0.6f : (Me.Feeling == 2 ? 0.6f : Me.Mood);
            break;
        case ETalkChoice::AdvisePlace:
        case ETalkChoice::Instruct:
            if (Theirs.LastKind == ETalkKind::Trouble)
            {
                Said.Subject = JobBit;
            }
            break;
        default:
            break;
        }
        return Said;
    }

    bool UsefulTo(const FSaid& Said, const FTalker& Hearer, const FSide& HearerSide)
    {
        const auto Has = [](uint32 Bits, int32 Bit) { return Bit >= 0 && Bit < 32 && (Bits & (1u << Bit)) != 0; };
        switch (Said.Choice)
        {
        case ETalkChoice::AdvisePlace:
        case ETalkChoice::Instruct:
            return Said.Subject >= 0 && !Has(Hearer.KnowPlaceFor, Said.Subject)
                && (Said.Subject == Hearer.TopNeed || (Said.Subject == JobBit && !Hearer.bEmployed));
        case ETalkChoice::AnswerWhere:
            return HearerSide.AskedFor == ETalkChoice::AskWhere && !Has(Hearer.PlacesKnown, Said.Subject);
        case ETalkChoice::Retell:
            return HearerSide.AskedFor == ETalkChoice::AskWhatWritten;
        case ETalkChoice::AgreeTeach:
            return HearerSide.AskedFor == ETalkChoice::AskTeach && !Has(Hearer.SkillsHeld, Said.Subject);
        case ETalkChoice::OfferMoney:
            return !Hearer.bHaveMoney;
        default:
            return false;
        }
    }

    void Learn(const FSaid& Said, FTalker& Hearer)
    {
        if (Said.Subject < 0 || Said.Subject >= 32)
        {
            return;
        }
        switch (Said.Choice)
        {
        case ETalkChoice::AdvisePlace:
        case ETalkChoice::Instruct:
            Hearer.KnowPlaceFor |= 1u << Said.Subject;
            break;
        case ETalkChoice::AnswerWhere:
            Hearer.PlacesKnown |= 1u << Said.Subject;
            break;
        case ETalkChoice::AgreeTeach:
            Hearer.SkillsHeld |= 1u << Said.Subject;
            break;
        case ETalkChoice::OfferMoney:
            Hearer.bHaveMoney = true;
            break;
        default:
            break;
        }
    }

    struct FTally
    {
        int32 Asked = 0;
        int32 Answered = 0;
        int32 Greeted = 0;
        int32 GreetedBack = 0;
        int32 Advised = 0;
        int32 Thanked = 0;
        int32 Long = 0;
        int32 LongLeft = 0;
        int32 Talks = 0;
        int32 Passed = 0;
        int32 Lines = 0;
        double Felt = 0.0;
        int32 FeltCount = 0;
        TMap<int32, int32> Chosen;
    };
}

FString FTalkSchool::Path(int32 Kind)
{
    return FLifeSchool::Folder() / FString::Printf(TEXT("Talk_%d.talk"), Kind);
}

void FTalkSchool::Raise(FTalkCore& Core, int32 Kind, int32 Conversations, uint32 Seed, FString* OutReport)
{
    if (!Core.IsReady())
    {
        Core.Init(Seed);
    }
    FRandomStream Rng(static_cast<int32>(Seed));
    FTally Tally;
    const int32 Every = FMath::Max(1, Conversations / 10);
    for (int32 Talk = 0; Talk < Conversations; ++Talk)
    {
        const float Progress = static_cast<float>(Talk) / FMath::Max(1, Conversations - 1);
        const float Temperature = FMath::Lerp(0.45f, 0.08f, FMath::Min(1.0f, Progress * 1.4f));
        FTalker People[2] = { Born(Kind, Rng), Born(Rng.FRand() < 0.2f ? 1 : Kind, Rng) };
        const float Familiar = Rng.FRand() < 0.4f ? 0.0f : Rng.FRandRange(0.1f, 1.0f);
        FBond Bonds[2] = { Bonded(Familiar, Rng), Bonded(Familiar, Rng) };
        FSide Sides[2];
        uint32 Covered = 0;
        int32 Speaker = 0;
        bool bOver = false;
        for (int32 Line = 0; Line < 16 && !bOver; ++Line)
        {
            const int32 Hearer = 1 - Speaker;
            FTalker& S = People[Speaker];
            FTalker& H = People[Hearer];
            FSide& Mine = Sides[Speaker];
            FSide& Theirs = Sides[Hearer];

            FSaid Parts[2];
            TArray<FTalkOption> Options;
            for (int32 Part = 0; Part < 2; ++Part)
            {
                if (Part == 1 && (FTalkRules::Expects(Parts[0].Choice) || FTalkRules::Ends(Parts[0].Choice)))
                {
                    break;
                }
                const FTalkMoment Moment = MomentOf(S, Bonds[Speaker], Mine, Theirs, H, Line, Part, Covered);
                FTalkRules::Menu(Moment, KnowledgeOf(S, Theirs, Rng), Options);
                for (FTalkOption& O : Options)
                {
                    O.Words = FMath::Clamp(S.Literacy + Rng.FRandRange(-0.15f, 0.1f), 0.0f, 1.0f);
                }
                const int32 Picked = Core.Choose(Moment, Options, Rng, Temperature);
                if (!Options.IsValidIndex(Picked))
                {
                    break;
                }
                Parts[Part] = Voice(Options[Picked], S, Theirs, Rng);
                Mine.Moment[Part] = Moment;
                Mine.Option[Part] = Options[Picked];
                Mine.bPending[Part] = true;
                Tally.Chosen.FindOrAdd(static_cast<int32>(Options[Picked].Choice)) += 1;
            }
            if (Line == 0 && Parts[0].Choice == ETalkChoice::Nothing)
            {
                Core.Remember(Mine.Moment[0], Mine.Option[0], FTalkOutcome{ 0.0f, 0.0f, 0.0f, 0.0f });
                ++Tally.Passed;
                break;
            }

            const bool bUnderstood = Rng.FRand() < 0.55f + 0.45f * FMath::Min(S.Literacy, H.Literacy);
            const float SpeakerTone = FMath::Clamp(0.35f * S.Mood + 0.35f * Bonds[Speaker].Liking + 0.6f * Mine.LastFelt
                + Rng.FRandRange(-0.08f, 0.08f), -1.0f, 1.0f);
            float Felt = 0.0f;
            float Esteem = 0.0f;
            bool bAnyUseful = false;
            for (int32 Part = 0; Part < 2; ++Part)
            {
                const FSaid& Said = Parts[Part];
                if (Said.Choice == ETalkChoice::Nothing && Part == 1)
                {
                    continue;
                }
                FTalkHeard Heard;
                Heard.HearerSaid = Part == 0 ? Theirs.LastMove : EDialogueMove::React;
                Heard.HearerKind = Theirs.LastKind;
                Heard.Heard = Said.Choice;
                Heard.bOnTopic = Said.Kind != ETalkKind::None && Said.Kind == Theirs.LastKind;
                Heard.bRepeat = Part == 1 && Said.Kind != ETalkKind::None && (Covered & (1u << static_cast<uint32>(Said.Kind))) != 0;
                Heard.bUseful = bUnderstood && UsefulTo(Said, H, Theirs);
                Heard.bHearerSad = H.Feeling == 1 || H.Mood < -0.4f;
                Heard.bUnderstood = bUnderstood;
                Heard.SpeakerTone = Part == 0 ? SpeakerTone : 0.0f;
                Felt += FTalkRules::Felt(Heard);
                Esteem += bUnderstood ? FTalkRules::Esteem(Said.Choice) : 0.0f;
                bAnyUseful |= Heard.bUseful;
                if (Heard.bUseful)
                {
                    Learn(Said, H);
                }
                if (Said.Choice == ETalkChoice::OfferMoney)
                {
                    S.bHaveMoney = Rng.FRand() < 0.7f;
                }
                if (Said.Choice == ETalkChoice::Introduce || Said.Choice == ETalkChoice::IntroduceBack)
                {
                    Bonds[Hearer].bKnowName = true;
                }
            }
            Felt = FMath::Clamp(Felt, -1.0f, 1.0f);
            Tally.Felt += Felt;
            ++Tally.FeltCount;

            if (Theirs.LastMove == EDialogueMove::Ask || Theirs.LastMove == EDialogueMove::AskAdvice || Theirs.LastMove == EDialogueMove::HowAreYou)
            {
                ++Tally.Asked;
                Tally.Answered += FTalkRules::MoveOf(Parts[0].Choice) == EDialogueMove::Answer
                    || FTalkRules::MoveOf(Parts[0].Choice) == EDialogueMove::StateOfSelf
                    || FTalkRules::MoveOf(Parts[0].Choice) == EDialogueMove::Advise
                    || FTalkRules::MoveOf(Parts[0].Choice) == EDialogueMove::Instruct
                    || FTalkRules::MoveOf(Parts[0].Choice) == EDialogueMove::Offer
                    || FTalkRules::MoveOf(Parts[0].Choice) == EDialogueMove::Decline ? 1 : 0;
            }
            if (Theirs.LastMove == EDialogueMove::Greet || Theirs.LastMove == EDialogueMove::Introduce)
            {
                ++Tally.Greeted;
                const EDialogueMove Back = FTalkRules::MoveOf(Parts[0].Choice);
                Tally.GreetedBack += Back == EDialogueMove::Greet || Back == EDialogueMove::Introduce || Back == EDialogueMove::HowAreYou ? 1 : 0;
            }
            if (Theirs.LastMove == EDialogueMove::Advise || Theirs.LastMove == EDialogueMove::Offer || Theirs.LastMove == EDialogueMove::Console)
            {
                ++Tally.Advised;
                Tally.Thanked += FTalkRules::MoveOf(Parts[0].Choice) == EDialogueMove::Thank ? 1 : 0;
            }
            if (Line >= 8)
            {
                ++Tally.Long;
                Tally.LongLeft += (FTalkRules::Ends(Parts[0].Choice) || FTalkRules::Ends(Parts[1].Choice)) ? 1 : 0;
            }

            FBond& HearerBond = Bonds[Hearer];
            HearerBond.Liking = FMath::Clamp(HearerBond.Liking + 0.12f * Felt, -1.0f, 1.0f);
            HearerBond.Trust = FMath::Clamp(HearerBond.Trust + 0.02f * Felt, 0.0f, 1.0f);
            HearerBond.Familiarity = FMath::Min(1.0f, HearerBond.Familiarity + 0.03f);
            H.Mood = FMath::Clamp(H.Mood + 0.15f * Felt, -1.0f, 1.0f);
            H.Esteem = FMath::Clamp(H.Esteem + 0.25f * Esteem, 0.0f, 1.0f);
            H.Contact = FMath::Min(1.0f, H.Contact + (bUnderstood ? 0.04f : 0.01f));
            S.Contact = FMath::Min(1.0f, S.Contact + 0.03f);

            Theirs.LastFelt = Felt;
            const bool bLeaving = FTalkRules::Ends(Parts[0].Choice) || FTalkRules::Ends(Parts[1].Choice);

            for (int32 Part = 0; Part < 2; ++Part)
            {
                if (!Theirs.bPending[Part])
                {
                    continue;
                }
                FTalkOutcome Outcome;
                Outcome.Tone = SpeakerTone;
                Outcome.Stayed = bLeaving ? 0.0f : 1.0f;
                Outcome.Gain = bAnyUseful ? 1.0f : 0.0f;
                Outcome.Warmth = FMath::Clamp(Esteem, -1.0f, 1.0f);
                Core.Remember(Theirs.Moment[Part], Theirs.Option[Part], Outcome);
                Theirs.bPending[Part] = false;
            }

            const FSaid& Last = Parts[1].Choice != ETalkChoice::Nothing ? Parts[1] : Parts[0];
            Mine.LastMove = Last.Move == EDialogueMove::None ? EDialogueMove::React : Last.Move;
            Mine.LastKind = Last.Kind;
            Mine.LastSubject = Last.Subject;
            Mine.LastValence = Last.Valence;
            Mine.bSpoke = true;
            Mine.AskedFor = FTalkRules::MoveOf(Last.Choice) == EDialogueMove::Ask ? Last.Choice : ETalkChoice::Nothing;
            for (const FSaid& Said : Parts)
            {
                if (Said.Choice == ETalkChoice::AskHow || Said.Choice == ETalkChoice::AndYou)
                {
                    Mine.bAskedHow = true;
                }
                if (Said.Move == EDialogueMove::StateOfSelf)
                {
                    Mine.bToldHow = true;
                }
                if (Said.Choice == ETalkChoice::AskAdvice)
                {
                    Mine.bAskedAdvice = true;
                }
                if (Said.Kind != ETalkKind::None)
                {
                    Covered |= 1u << static_cast<uint32>(Said.Kind);
                }
            }
            ++Tally.Lines;

            if (Theirs.LastMove == EDialogueMove::Farewell || Line == 15)
            {
                bOver = true;
            }
            if (bOver)
            {
                for (int32 Part = 0; Part < 2; ++Part)
                {
                    if (Mine.bPending[Part])
                    {
                        FTalkOutcome Outcome;
                        Outcome.Tone = 0.0f;
                        Outcome.Stayed = 0.0f;
                        Core.Remember(Mine.Moment[Part], Mine.Option[Part], Outcome);
                        Mine.bPending[Part] = false;
                    }
                }
            }
            Speaker = Hearer;
        }
        ++Tally.Talks;
        Core.Practice(6, Rng);

        if (OutReport && ((Talk + 1) % Every == 0 || Talk + 1 == Conversations))
        {
            TArray<TPair<int32, int32>> Top;
            for (const TPair<int32, int32>& Pair : Tally.Chosen)
            {
                Top.Emplace(Pair.Key, Pair.Value);
            }
            Top.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B) { return A.Value > B.Value; });
            int32 AllChosen = 0;
            for (const TPair<int32, int32>& Pair : Top)
            {
                AllChosen += Pair.Value;
            }
            FString Favourites;
            for (int32 I = 0; I < FMath::Min(6, Top.Num()); ++I)
            {
                Favourites += FString::Printf(TEXT("%s %.0f%% "), FTalkRules::Name(static_cast<ETalkChoice>(Top[I].Key)),
                    100.0f * Top[I].Value / FMath::Max(1, AllChosen));
            }
            const FString Line = FString::Printf(
                TEXT("TALK [%6d] чувство %+.3f, прошли мимо %.0f%%, реплик в разговоре %.1f | ответили на вопрос %.0f%%, на приветствие %.0f%%, поблагодарили %.0f%%, ушли после 8-й %.0f%% | ошибка %.4f | %s"),
                Talk + 1, Tally.Felt / FMath::Max(1, Tally.FeltCount), 100.0f * Tally.Passed / FMath::Max(1, Tally.Talks),
                static_cast<float>(Tally.Lines) / FMath::Max(1, Tally.Talks - Tally.Passed),
                100.0f * Tally.Answered / FMath::Max(1, Tally.Asked), 100.0f * Tally.GreetedBack / FMath::Max(1, Tally.Greeted),
                100.0f * Tally.Thanked / FMath::Max(1, Tally.Advised), 100.0f * Tally.LongLeft / FMath::Max(1, Tally.Long),
                Core.Error, *Favourites);
            *OutReport += Line + TEXT("\n");
            UE_LOG(LogHumanCity, Display, TEXT("%s"), *Line);
            Tally = FTally();
        }
    }
}
