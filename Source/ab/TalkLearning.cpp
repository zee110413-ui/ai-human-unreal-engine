#include "TalkLearning.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

namespace
{
    constexpr int32 TalkVersion = 1;

    float Sign(bool bValue)
    {
        return bValue ? 1.0f : -1.0f;
    }

    void AdamStep(TArray<float>& Params, const TArray<float>& Grad, TArray<float>& Moments, int32 Step, float Rate)
    {
        const int32 Count = Params.Num();
        if (Moments.Num() != Count * 2)
        {
            Moments.Init(0.0f, Count * 2);
        }
        const float C1 = 1.0f - FMath::Pow(0.9f, static_cast<float>(FMath::Min(Step, 2000)));
        const float C2 = 1.0f - FMath::Pow(0.999f, static_cast<float>(FMath::Min(Step, 20000)));
        float* M = Moments.GetData();
        float* V = M + Count;
        for (int32 I = 0; I < Count; ++I)
        {
            const float G = FMath::Clamp(Grad[I], -5.0f, 5.0f);
            M[I] = 0.9f * M[I] + 0.1f * G;
            V[I] = 0.999f * V[I] + 0.001f * G * G;
            Params[I] -= Rate * (M[I] / C1) / (FMath::Sqrt(V[I] / C2) + 1.0e-5f);
        }
    }

    bool IsQuestion(EDialogueMove Move)
    {
        return Move == EDialogueMove::Ask || Move == EDialogueMove::HowAreYou || Move == EDialogueMove::AskAdvice;
    }

    bool IsAnswer(ETalkChoice Choice)
    {
        switch (Choice)
        {
        case ETalkChoice::TellNeed:
        case ETalkChoice::TellDeed:
        case ETalkChoice::TellMood:
        case ETalkChoice::AdvisePlace:
        case ETalkChoice::Instruct:
        case ETalkChoice::Retell:
        case ETalkChoice::AnswerWhere:
        case ETalkChoice::AnswerWhatHappened:
        case ETalkChoice::AnswerWhatStops:
        case ETalkChoice::AgreeTeach:
        case ETalkChoice::RefuseTeach:
        case ETalkChoice::AnswerPay:
        case ETalkChoice::DontKnow:
            return true;
        default:
            return false;
        }
    }
}

EDialogueMove FTalkRules::MoveOf(ETalkChoice Choice)
{
    switch (Choice)
    {
    case ETalkChoice::Greet:
    case ETalkChoice::GreetBack:          return EDialogueMove::Greet;
    case ETalkChoice::Introduce:
    case ETalkChoice::IntroduceBack:      return EDialogueMove::Introduce;
    case ETalkChoice::AskHow:
    case ETalkChoice::AndYou:             return EDialogueMove::HowAreYou;
    case ETalkChoice::TellNeed:
    case ETalkChoice::TellDeed:
    case ETalkChoice::TellMood:           return EDialogueMove::StateOfSelf;
    case ETalkChoice::AdvisePlace:        return EDialogueMove::Advise;
    case ETalkChoice::AskWhatWritten:
    case ETalkChoice::AskWhere:
    case ETalkChoice::AskWhatHappened:
    case ETalkChoice::AskWhatStops:
    case ETalkChoice::AskTeach:
    case ETalkChoice::AskPay:             return EDialogueMove::Ask;
    case ETalkChoice::Console:
    case ETalkChoice::Encourage:          return EDialogueMove::Console;
    case ETalkChoice::OfferMoney:
    case ETalkChoice::AgreeTeach:         return EDialogueMove::Offer;
    case ETalkChoice::Instruct:           return EDialogueMove::Instruct;
    case ETalkChoice::Retell:
    case ETalkChoice::AnswerWhere:
    case ETalkChoice::AnswerWhatHappened:
    case ETalkChoice::AnswerWhatStops:
    case ETalkChoice::AnswerPay:
    case ETalkChoice::DontKnow:           return EDialogueMove::Answer;
    case ETalkChoice::RefuseTeach:
    case ETalkChoice::RefuseOrder:        return EDialogueMove::Decline;
    case ETalkChoice::Thanks:
    case ETalkChoice::ThanksForKindness:
    case ETalkChoice::AcceptMoney:        return EDialogueMove::Thank;
    case ETalkChoice::Obey:               return EDialogueMove::Obey;
    case ETalkChoice::FarewellBack:
    case ETalkChoice::Farewell:           return EDialogueMove::Farewell;
    case ETalkChoice::AskAdvice:          return EDialogueMove::AskAdvice;
    case ETalkChoice::TellBook:
    case ETalkChoice::TellPlace:
    case ETalkChoice::TellPerson:
    case ETalkChoice::TellFeeling:
    case ETalkChoice::TellWeather:
    case ETalkChoice::TellDream:
    case ETalkChoice::TellSkill:
    case ETalkChoice::TellTrouble:
    case ETalkChoice::TellWork:           return EDialogueMove::Tell;
    case ETalkChoice::Nothing:            return EDialogueMove::None;
    default:                              return EDialogueMove::React;
    }
}

ETalkKind FTalkRules::KindOf(ETalkChoice Choice, ETalkKind TheirKind)
{
    switch (Choice)
    {
    case ETalkChoice::TellNeed:
    case ETalkChoice::AskAdvice:          return ETalkKind::Need;
    case ETalkChoice::TellDeed:           return ETalkKind::Deed;
    case ETalkChoice::TellMood:
    case ETalkChoice::TellFeeling:        return ETalkKind::Feeling;
    case ETalkChoice::TellBook:           return ETalkKind::Book;
    case ETalkChoice::TellPlace:          return ETalkKind::Place;
    case ETalkChoice::TellPerson:         return ETalkKind::Person;
    case ETalkChoice::TellWeather:        return ETalkKind::Weather;
    case ETalkChoice::TellDream:          return ETalkKind::Dream;
    case ETalkChoice::TellSkill:          return ETalkKind::Skill;
    case ETalkChoice::TellTrouble:        return ETalkKind::Trouble;
    case ETalkChoice::TellWork:           return ETalkKind::Work;
    case ETalkChoice::Greet:
    case ETalkChoice::GreetBack:
    case ETalkChoice::Introduce:
    case ETalkChoice::IntroduceBack:
    case ETalkChoice::AskHow:
    case ETalkChoice::AndYou:
    case ETalkChoice::Farewell:
    case ETalkChoice::FarewellBack:
    case ETalkChoice::Nothing:            return ETalkKind::None;
    default:                              return TheirKind;
    }
}

bool FTalkRules::Expects(ETalkChoice Choice)
{
    const EDialogueMove Move = MoveOf(Choice);
    return IsQuestion(Move) || Move == EDialogueMove::Tell || Move == EDialogueMove::StateOfSelf
        || Move == EDialogueMove::Advise || Move == EDialogueMove::Offer || Move == EDialogueMove::Instruct
        || Move == EDialogueMove::Answer || Move == EDialogueMove::Console || Move == EDialogueMove::Farewell;
}

bool FTalkRules::Ends(ETalkChoice Choice)
{
    return Choice == ETalkChoice::Farewell || Choice == ETalkChoice::FarewellBack;
}

const TCHAR* FTalkRules::Name(ETalkChoice Choice)
{
    static const TCHAR* Names[] = {
        TEXT("Greet"), TEXT("GreetBack"), TEXT("Introduce"), TEXT("IntroduceBack"), TEXT("AskHow"), TEXT("AndYou"),
        TEXT("TellNeed"), TEXT("TellDeed"), TEXT("TellMood"), TEXT("AdvisePlace"), TEXT("SameHere"), TEXT("Acknowledge"),
        TEXT("ReactDoing"), TEXT("KnowThat"), TEXT("CantRead"), TEXT("AskWhatWritten"), TEXT("AgreePlace"), TEXT("DisagreePlace"),
        TEXT("AskWhere"), TEXT("AgreePerson"), TEXT("DisagreePerson"), TEXT("DontKnowPerson"), TEXT("AskWhatHappened"),
        TEXT("Console"), TEXT("GladForYou"), TEXT("AgreeWeather"), TEXT("ReactDream"), TEXT("AskWhatStops"), TEXT("AskTeach"),
        TEXT("OfferMoney"), TEXT("Instruct"), TEXT("Encourage"), TEXT("AskPay"), TEXT("Retell"), TEXT("AnswerWhere"),
        TEXT("AnswerWhatHappened"), TEXT("AnswerWhatStops"), TEXT("AgreeTeach"), TEXT("RefuseTeach"), TEXT("AnswerPay"),
        TEXT("DontKnow"), TEXT("Thanks"), TEXT("ThanksForKindness"), TEXT("Obey"), TEXT("RefuseOrder"), TEXT("AcceptMoney"),
        TEXT("NoProblem"), TEXT("FarewellBack"), TEXT("Farewell"), TEXT("AskAdvice"), TEXT("TellBook"), TEXT("TellPlace"),
        TEXT("TellPerson"), TEXT("TellFeeling"), TEXT("TellWeather"), TEXT("TellDream"), TEXT("TellSkill"), TEXT("TellTrouble"),
        TEXT("TellWork"), TEXT("Nothing") };
    static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(ETalkChoice::Count), "names");
    const int32 Index = static_cast<int32>(Choice);
    return Index >= 0 && Index < UE_ARRAY_COUNT(Names) ? Names[Index] : TEXT("?");
}

void FTalkRules::Menu(const FTalkMoment& M, const FTalkKnowledge& K, TArray<FTalkOption>& Out)
{
    Out.Reset();
    auto Add = [&M, &Out](ETalkChoice Choice, float Valence = 0.0f)
    {
        FTalkOption O;
        O.Choice = Choice;
        O.Kind = KindOf(Choice, M.TheirKind);
        O.Valence = Valence;
        O.bOnTopic = M.TheirKind != ETalkKind::None && O.Kind == M.TheirKind;
        O.bRepeat = M.Part == 1 && O.Kind != ETalkKind::None && (M.Covered & (1u << static_cast<uint32>(O.Kind))) != 0;
        O.bGives = Choice == ETalkChoice::AdvisePlace || Choice == ETalkChoice::AnswerWhere || Choice == ETalkChoice::Retell
            || Choice == ETalkChoice::OfferMoney || Choice == ETalkChoice::AgreeTeach || Choice == ETalkChoice::Instruct;
        Out.Add(O);
    };

    if (M.Part == 1)
    {
        Add(ETalkChoice::Nothing);
        if (M.Turns >= 1)
        {
            Add(ETalkChoice::Farewell);
        }
        if (K.bUrgentNeed && !K.bKnowPlaceForMyNeed && !M.bAskedAdvice)
        {
            Add(ETalkChoice::AskAdvice, -0.4f);
        }
        if (!M.bAskedHow && !M.bThemChild)
        {
            Add(ETalkChoice::AndYou);
        }
        if (K.bCanInstruct)
        {
            Add(ETalkChoice::Instruct);
        }
        if (K.bUrgentNeed)
        {
            Add(ETalkChoice::TellNeed, -0.4f);
        }
        if (K.bDoing)
        {
            Add(ETalkChoice::TellDeed);
        }
        if (K.bFeeling)
        {
            Add(ETalkChoice::TellFeeling, M.Mood);
        }
        if (K.bHaveBook)
        {
            Add(ETalkChoice::TellBook, 0.2f);
        }
        if (K.bHavePlace)
        {
            Add(ETalkChoice::TellPlace, 0.2f);
        }
        if (K.bHavePerson)
        {
            Add(ETalkChoice::TellPerson);
        }
        if (K.bHaveDream)
        {
            Add(ETalkChoice::TellDream, 0.3f);
        }
        if (K.bHaveSkillToTell)
        {
            Add(ETalkChoice::TellSkill, 0.4f);
        }
        if (K.bHaveTrouble)
        {
            Add(ETalkChoice::TellTrouble, -0.5f);
        }
        if (K.bHaveWork)
        {
            Add(ETalkChoice::TellWork);
        }
        Add(ETalkChoice::TellWeather);
        return;
    }

    switch (M.TheirMove)
    {
    case EDialogueMove::None:
        Add(ETalkChoice::Nothing);
        Add(ETalkChoice::Greet);
        if (!M.bKnowName)
        {
            Add(ETalkChoice::Introduce);
        }
        else
        {
            Add(ETalkChoice::AskHow);
        }
        if (K.bUrgentNeed && !K.bKnowPlaceForMyNeed)
        {
            Add(ETalkChoice::AskAdvice, -0.4f);
        }
        Add(ETalkChoice::TellWeather);
        break;

    case EDialogueMove::Farewell:
        Add(ETalkChoice::FarewellBack);
        Add(ETalkChoice::Acknowledge);
        break;

    case EDialogueMove::Introduce:
        if (M.bFirstWords)
        {
            Add(ETalkChoice::IntroduceBack);
        }
        Add(ETalkChoice::GreetBack);
        Add(ETalkChoice::Acknowledge);
        break;

    case EDialogueMove::Greet:
        Add(ETalkChoice::GreetBack);
        if (!M.bKnowName)
        {
            Add(ETalkChoice::IntroduceBack);
        }
        if (!M.bAskedHow)
        {
            Add(ETalkChoice::AskHow);
        }
        Add(ETalkChoice::Acknowledge);
        break;

    case EDialogueMove::HowAreYou:
        if (K.bUrgentNeed)
        {
            Add(ETalkChoice::TellNeed, -0.4f);
        }
        if (K.bDoing)
        {
            Add(ETalkChoice::TellDeed);
        }
        Add(ETalkChoice::TellMood, M.Mood);
        Add(ETalkChoice::Acknowledge);
        break;

    case EDialogueMove::StateOfSelf:
    case EDialogueMove::Tell:
        switch (M.TheirKind)
        {
        case ETalkKind::Need:
            if (K.bKnowPlaceForTheirNeed)
            {
                Add(ETalkChoice::AdvisePlace, 0.4f);
            }
            if (K.bFeelTheirNeed)
            {
                Add(ETalkChoice::SameHere, -0.2f);
            }
            Add(ETalkChoice::Acknowledge);
            break;
        case ETalkKind::Deed:
            Add(ETalkChoice::ReactDoing);
            Add(ETalkChoice::Acknowledge);
            break;
        case ETalkKind::Book:
            if (K.bReadTheirBook)
            {
                Add(ETalkChoice::KnowThat);
            }
            if (!K.bCanRead)
            {
                Add(ETalkChoice::CantRead);
            }
            else if (!K.bReadTheirBook)
            {
                Add(ETalkChoice::AskWhatWritten);
            }
            Add(ETalkChoice::Acknowledge);
            break;
        case ETalkKind::Place:
            if (K.bKnowTheirPlace)
            {
                Add(K.bAgreeTheirPlace ? ETalkChoice::AgreePlace : ETalkChoice::DisagreePlace);
            }
            else
            {
                Add(ETalkChoice::AskWhere);
            }
            Add(ETalkChoice::Acknowledge);
            break;
        case ETalkKind::Person:
            if (K.bTheirPersonIsMe)
            {
                Add(ETalkChoice::Acknowledge);
            }
            else if (K.bKnowTheirPerson)
            {
                Add(K.bAgreeTheirPerson ? ETalkChoice::AgreePerson : ETalkChoice::DisagreePerson);
                Add(ETalkChoice::Acknowledge);
            }
            else
            {
                Add(ETalkChoice::DontKnowPerson);
            }
            break;
        case ETalkKind::Feeling:
            Add(ETalkChoice::AskWhatHappened);
            Add(M.TheirValence < 0.0f ? ETalkChoice::Console : ETalkChoice::GladForYou);
            Add(ETalkChoice::Acknowledge);
            break;
        case ETalkKind::Weather:
            Add(ETalkChoice::AgreeWeather);
            Add(ETalkChoice::Acknowledge);
            break;
        case ETalkKind::Dream:
            Add(ETalkChoice::ReactDream);
            Add(ETalkChoice::AskWhatStops);
            break;
        case ETalkKind::Skill:
            Add(ETalkChoice::AskTeach);
            Add(ETalkChoice::Acknowledge);
            break;
        case ETalkKind::Trouble:
            if (K.bTheirTroubleIsMoney && K.bHaveMoney)
            {
                Add(ETalkChoice::OfferMoney, 0.5f);
            }
            if (K.bCanInstruct)
            {
                Add(ETalkChoice::Instruct);
            }
            if (K.bTheirTroubleIsJob && K.bKnowPlaceForJob)
            {
                Add(ETalkChoice::AdvisePlace, 0.4f);
            }
            Add(ETalkChoice::Encourage, 0.3f);
            break;
        case ETalkKind::Work:
            Add(ETalkChoice::AskPay);
            Add(ETalkChoice::Acknowledge);
            break;
        default:
            Add(ETalkChoice::Acknowledge);
            break;
        }
        break;

    case EDialogueMove::Ask:
        switch (M.TheirKind)
        {
        case ETalkKind::Book:
            if (K.bKnowAnswer)
            {
                Add(ETalkChoice::Retell);
            }
            Add(ETalkChoice::DontKnow);
            break;
        case ETalkKind::Place:
            if (K.bKnowAnswer)
            {
                Add(ETalkChoice::AnswerWhere);
            }
            Add(ETalkChoice::DontKnow);
            break;
        case ETalkKind::Feeling:
            Add(ETalkChoice::AnswerWhatHappened, M.Mood);
            Add(ETalkChoice::Acknowledge);
            break;
        case ETalkKind::Dream:
            Add(ETalkChoice::AnswerWhatStops);
            Add(ETalkChoice::Acknowledge);
            break;
        case ETalkKind::Skill:
            if (K.bHaveSkill)
            {
                Add(ETalkChoice::AgreeTeach, 0.4f);
            }
            Add(ETalkChoice::RefuseTeach);
            break;
        case ETalkKind::Work:
            Add(ETalkChoice::AnswerPay);
            Add(ETalkChoice::DontKnow);
            break;
        default:
            Add(ETalkChoice::DontKnow);
            Add(ETalkChoice::Acknowledge);
            break;
        }
        break;

    case EDialogueMove::AskAdvice:
        if (K.bCanInstruct)
        {
            Add(ETalkChoice::Instruct);
        }
        if (K.bKnowPlaceForTheirNeed)
        {
            Add(ETalkChoice::AdvisePlace, 0.4f);
        }
        Add(ETalkChoice::DontKnow);
        break;

    case EDialogueMove::Answer:
        Add(ETalkChoice::Thanks, 0.3f);
        Add(ETalkChoice::Acknowledge);
        break;

    case EDialogueMove::Advise:
        Add(ETalkChoice::Thanks, 0.3f);
        Add(ETalkChoice::Acknowledge);
        break;

    case EDialogueMove::Console:
        Add(ETalkChoice::ThanksForKindness, 0.3f);
        Add(ETalkChoice::Acknowledge);
        break;

    case EDialogueMove::Instruct:
        Add(ETalkChoice::Obey);
        Add(ETalkChoice::RefuseOrder);
        break;

    case EDialogueMove::Offer:
        Add(M.TheirKind == ETalkKind::Trouble ? ETalkChoice::AcceptMoney : ETalkChoice::Thanks, 0.4f);
        Add(ETalkChoice::RefuseTeach);
        break;

    case EDialogueMove::Thank:
        Add(ETalkChoice::NoProblem, 0.2f);
        Add(ETalkChoice::Nothing);
        break;

    default:
        Add(ETalkChoice::Nothing);
        Add(ETalkChoice::Acknowledge);
        break;
    }
}

float FTalkRules::Felt(const FTalkHeard& H)
{
    if (!H.bUnderstood)
    {
        return -0.05f;
    }
    const EDialogueMove Reply = MoveOf(H.Heard);
    float F = 0.0f;
    switch (H.HearerSaid)
    {
    case EDialogueMove::Ask:
    case EDialogueMove::HowAreYou:
    case EDialogueMove::AskAdvice:
        if (IsAnswer(H.Heard))
        {
            F += H.Heard == ETalkChoice::DontKnow ? 0.08f : 0.25f;
        }
        else if (Reply == EDialogueMove::Farewell)
        {
            F -= 0.35f;
        }
        else
        {
            F -= 0.3f;
        }
        break;
    case EDialogueMove::Greet:
    case EDialogueMove::Introduce:
        if (Reply == EDialogueMove::Greet || Reply == EDialogueMove::Introduce || Reply == EDialogueMove::HowAreYou)
        {
            F += 0.2f;
        }
        else if (Reply == EDialogueMove::Farewell)
        {
            F -= 0.3f;
        }
        else
        {
            F -= 0.15f;
        }
        break;
    case EDialogueMove::Tell:
    case EDialogueMove::StateOfSelf:
        if (H.bOnTopic)
        {
            F += 0.15f;
        }
        else if (Reply == EDialogueMove::Farewell)
        {
            F -= 0.12f;
        }
        else
        {
            F -= 0.08f;
        }
        break;
    case EDialogueMove::Advise:
    case EDialogueMove::Offer:
    case EDialogueMove::Console:
    case EDialogueMove::Answer:
        if (Reply == EDialogueMove::Thank)
        {
            F += 0.3f;
        }
        else if (Reply == EDialogueMove::Decline)
        {
            F -= 0.2f;
        }
        else
        {
            F -= 0.1f;
        }
        break;
    case EDialogueMove::Instruct:
        F += Reply == EDialogueMove::Obey ? 0.3f : (Reply == EDialogueMove::Decline ? -0.25f : -0.1f);
        break;
    case EDialogueMove::Thank:
        F += H.Heard == ETalkChoice::NoProblem ? 0.1f : 0.0f;
        break;
    case EDialogueMove::Farewell:
        F += Reply == EDialogueMove::Farewell ? 0.1f : -0.1f;
        break;
    case EDialogueMove::None:
        F += (Reply == EDialogueMove::Greet || Reply == EDialogueMove::Introduce || Reply == EDialogueMove::HowAreYou) ? 0.1f : -0.1f;
        break;
    default:
        break;
    }
    if (H.bUseful)
    {
        F += 0.35f;
    }
    if (H.bHearerSad && (H.Heard == ETalkChoice::Console || H.Heard == ETalkChoice::Encourage || H.Heard == ETalkChoice::AskWhatHappened))
    {
        F += 0.25f;
    }
    if (H.bRepeat)
    {
        F -= 0.1f;
    }
    F += 0.25f * FMath::Clamp(H.SpeakerTone, -1.0f, 1.0f);
    return FMath::Clamp(F, -1.0f, 1.0f);
}

float FTalkRules::Esteem(ETalkChoice Heard)
{
    switch (Heard)
    {
    case ETalkChoice::AskAdvice:          return 0.4f;
    case ETalkChoice::Thanks:
    case ETalkChoice::ThanksForKindness:
    case ETalkChoice::AcceptMoney:
    case ETalkChoice::Obey:
    case ETalkChoice::AskTeach:           return 0.3f;
    case ETalkChoice::Console:
    case ETalkChoice::Encourage:          return 0.2f;
    case ETalkChoice::AskWhatHappened:
    case ETalkChoice::GladForYou:
    case ETalkChoice::AndYou:
    case ETalkChoice::AskHow:             return 0.15f;
    case ETalkChoice::AgreePerson:
    case ETalkChoice::AgreePlace:
    case ETalkChoice::SameHere:
    case ETalkChoice::NoProblem:          return 0.1f;
    case ETalkChoice::RefuseOrder:
    case ETalkChoice::DisagreePerson:     return -0.15f;
    default:                              return 0.0f;
    }
}

void FTalkCore::Encode(const FTalkMoment& M, const FTalkOption& O, float* Out)
{
    FMemory::Memzero(Out, sizeof(float) * InputSize);
    int32 I = 0;
    Out[I + FMath::Clamp(static_cast<int32>(O.Choice), 0, ChoiceCount - 1)] = 1.0f;
    I += ChoiceCount;
    Out[I + FMath::Clamp(static_cast<int32>(M.TheirMove), 0, MoveCount - 1)] = 1.0f;
    I += MoveCount;
    Out[I + FMath::Clamp(static_cast<int32>(M.TheirKind), 0, KindCount - 1)] = 1.0f;
    I += KindCount;
    Out[I + FMath::Clamp(static_cast<int32>(O.Kind), 0, KindCount - 1)] = 1.0f;
    I += KindCount;
    Out[I++] = Sign(M.Part == 1);
    Out[I++] = Sign(M.bFirstWords);
    Out[I++] = Sign(M.bOpening);
    Out[I++] = Sign(M.bAskedHow);
    Out[I++] = Sign(M.bToldHow);
    Out[I++] = Sign(M.bAskedAdvice);
    Out[I++] = Sign(M.bLost);
    Out[I++] = Sign(M.bLate);
    Out[I++] = Sign(M.bBusy);
    Out[I++] = Sign(M.bThemChild);
    Out[I++] = Sign(M.bKnowName);
    Out[I++] = Sign(M.bSeekingAdvice);
    Out[I++] = M.Closeness * 2.0f - 0.5f;
    Out[I++] = M.Liking;
    Out[I++] = M.Trust * 2.0f - 1.0f;
    Out[I++] = M.Familiarity * 2.0f - 1.0f;
    Out[I++] = M.Mood;
    Out[I++] = M.Urgency * 2.0f - 1.0f;
    Out[I++] = M.Contact * 2.0f - 1.0f;
    Out[I++] = M.Esteem * 2.0f - 1.0f;
    Out[I++] = M.Extraversion * 2.0f - 1.0f;
    Out[I++] = M.Agreeableness * 2.0f - 1.0f;
    Out[I++] = M.Openness * 2.0f - 1.0f;
    Out[I++] = FMath::Min(M.Turns, 16) / 6.0f - 1.0f;
    Out[I++] = M.TheirValence;
    Out[I++] = O.Valence;
    Out[I++] = O.Words * 2.0f - 1.0f;
    Out[I++] = Sign(O.bOnTopic);
    Out[I++] = Sign(O.bRepeat);
    Out[I++] = Sign(O.bGives);
    check(I == InputSize);
    for (int32 K = 0; K < InputSize; ++K)
    {
        Out[K] = FMath::Clamp(Out[K], -3.0f, 3.0f);
    }
}

float FTalkCore::Value(const FTalkMoment& M, const float* Outcome)
{
    const float Tone = FMath::Clamp(Outcome[0], -1.0f, 1.0f);
    const float Stayed = FMath::Clamp(Outcome[1], 0.0f, 1.0f);
    const float Gain = FMath::Clamp(Outcome[2], 0.0f, 1.0f);
    const float Warmth = FMath::Clamp(Outcome[3], -1.0f, 1.0f);
    const float Company = 0.3f * M.Extraversion + 0.25f * (1.0f - M.Contact)
        - 0.6f * M.Urgency - (M.bBusy ? 0.25f : 0.0f) - (M.bLate ? 0.2f : 0.0f) - 0.035f * M.Turns;
    return Tone * (0.4f + 0.5f * M.Agreeableness)
         + Warmth * (0.5f + 0.5f * (1.0f - M.Esteem))
         + Gain * (0.5f + M.Urgency)
         + Stayed * Company;
}

void FTalkCore::Init(uint32 Seed)
{
    Nets.SetNum(Ensemble);
    Moments.SetNum(Ensemble);
    for (int32 K = 0; K < Ensemble; ++K)
    {
        FRandomStream Own(static_cast<int32>(Seed + 7919u * (K + 1)));
        Nets[K].Init(InputSize, Hidden, OutcomeSize, Own, 0.3f);
        Moments[K].Reset();
    }
    Inputs.Init(0.0f, Capacity * InputSize);
    Targets.Init(0.0f, Capacity * OutcomeSize);
    AdamSteps = 0;
    Stored = 0;
    Next = 0;
    Lived = 0;
    Lessons = 0;
    Error = 1.0;
}

void FTalkCore::Predict(const FTalkMoment& Moment, const FTalkOption& Option, float* OutMean, float* OutSpread) const
{
    float In[InputSize];
    Encode(Moment, Option, In);
    float Cache[2 * Hidden];
    float Each[Ensemble][OutcomeSize];
    for (int32 K = 0; K < Ensemble; ++K)
    {
        Nets[K].Forward(In, Each[K], Cache);
    }
    for (int32 O = 0; O < OutcomeSize; ++O)
    {
        float Sum = 0.0f;
        for (int32 K = 0; K < Ensemble; ++K)
        {
            Sum += Each[K][O];
        }
        const float Mean = Sum / Ensemble;
        float Var = 0.0f;
        for (int32 K = 0; K < Ensemble; ++K)
        {
            Var += FMath::Square(Each[K][O] - Mean);
        }
        OutMean[O] = Mean;
        if (OutSpread)
        {
            OutSpread[O] = FMath::Sqrt(Var / Ensemble);
        }
    }
}

int32 FTalkCore::Choose(const FTalkMoment& Moment, const TArray<FTalkOption>& Options, FRandomStream& Rng, float Temperature) const
{
    if (Options.Num() == 0)
    {
        return INDEX_NONE;
    }
    if (!IsReady() || Options.Num() == 1)
    {
        return Rng.RandRange(0, Options.Num() - 1);
    }
    TArray<float> Scores;
    Scores.SetNum(Options.Num());
    float Best = -BIG_NUMBER;
    for (int32 I = 0; I < Options.Num(); ++I)
    {
        float Mean[OutcomeSize];
        float Spread[OutcomeSize];
        Predict(Moment, Options[I], Mean, Spread);
        const float Doubt = (Spread[0] + Spread[1] + Spread[2] + Spread[3]) * 0.25f;
        Scores[I] = Value(Moment, Mean) + Doubt * (0.1f + 0.3f * Moment.Openness);
        Best = FMath::Max(Best, Scores[I]);
    }
    const float T = FMath::Max(0.01f, Temperature);
    double Total = 0.0;
    TArray<double> Chance;
    Chance.SetNum(Options.Num());
    for (int32 I = 0; I < Options.Num(); ++I)
    {
        Chance[I] = FMath::Exp(FMath::Max(-30.0f, (Scores[I] - Best) / T));
        Total += Chance[I];
    }
    double Draw = Rng.FRand() * Total;
    for (int32 I = 0; I < Options.Num(); ++I)
    {
        Draw -= Chance[I];
        if (Draw <= 0.0)
        {
            return I;
        }
    }
    return Options.Num() - 1;
}

void FTalkCore::Remember(const FTalkMoment& Moment, const FTalkOption& Option, const FTalkOutcome& Outcome)
{
    if (!IsReady())
    {
        return;
    }
    const int32 Slot = Next;
    Next = (Next + 1) % Capacity;
    Stored = FMath::Min(Stored + 1, Capacity);
    Encode(Moment, Option, Inputs.GetData() + Slot * InputSize);
    float* Target = Targets.GetData() + Slot * OutcomeSize;
    Target[0] = FMath::Clamp(Outcome.Tone, -1.0f, 1.0f);
    Target[1] = FMath::Clamp(Outcome.Stayed, 0.0f, 1.0f);
    Target[2] = FMath::Clamp(Outcome.Gain, 0.0f, 1.0f);
    Target[3] = FMath::Clamp(Outcome.Warmth, -1.0f, 1.0f);
    ++Lived;
}

void FTalkCore::Practice(int32 Steps, FRandomStream& Rng)
{
    if (!IsReady() || Stored < 16)
    {
        return;
    }
    const int32 Batch = FMath::Min(32, Stored);
    TArray<float> Grad;
    float Cache[2 * Hidden];
    float Scratch[2 * Hidden];
    float Out[OutcomeSize];
    float OutGrad[OutcomeSize];
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        ++AdamSteps;
        double ErrorSum = 0.0;
        for (int32 K = 0; K < Ensemble; ++K)
        {
            FDeepNet& Net = Nets[K];
            Grad.Init(0.0f, Net.Size());
            for (int32 B = 0; B < Batch; ++B)
            {
                const int32 I = Rng.RandRange(0, Stored - 1);
                const float* In = Inputs.GetData() + I * InputSize;
                const float* Target = Targets.GetData() + I * OutcomeSize;
                Net.Forward(In, Out, Cache);
                for (int32 O = 0; O < OutcomeSize; ++O)
                {
                    const float Miss = Out[O] - Target[O];
                    ErrorSum += Miss * Miss;
                    OutGrad[O] = Miss * 2.0f / (Batch * OutcomeSize);
                }
                Net.Backward(In, Cache, OutGrad, Grad.GetData(), Scratch);
            }
            AdamStep(Net.Weights, Grad, Moments[K], AdamSteps, 1.0e-3f);
        }
        Error = 0.98 * Error + 0.02 * (ErrorSum / (Ensemble * Batch * OutcomeSize));
        ++Lessons;
    }
}

void FTalkCore::Serialize(FArchive& Ar)
{
    int32 Version = TalkVersion;
    int32 Size = InputSize;
    int32 Outcomes = OutcomeSize;
    Ar << Version << Size << Outcomes;
    if (Version != TalkVersion || Size != InputSize || Outcomes != OutcomeSize)
    {
        Ar.SetError();
        return;
    }
    int32 Members = Nets.Num();
    Ar << Members;
    if (Ar.IsLoading())
    {
        Nets.SetNum(Members);
        Moments.SetNum(Members);
    }
    for (FDeepNet& Net : Nets)
    {
        Net.Serialize(Ar);
    }
    Ar << Capacity << Lived << Lessons << Error << AdamSteps << Inputs << Targets << Stored << Next;
    for (TArray<float>& Moment : Moments)
    {
        Ar << Moment;
    }
}

bool FTalkCore::Save(const FString& Path)
{
    FBufferArchive Writer;
    Serialize(Writer);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    return FFileHelper::SaveArrayToFile(Writer, *Path);
}

bool FTalkCore::Load(const FString& Path)
{
    TArray<uint8> Bytes;
    if (!IFileManager::Get().FileExists(*Path) || !FFileHelper::LoadFileToArray(Bytes, *Path))
    {
        return false;
    }
    FMemoryReader Reader(Bytes);
    FTalkCore Loaded;
    Loaded.Serialize(Reader);
    if (Reader.IsError() || !Loaded.IsReady() || Loaded.Inputs.Num() != Loaded.Capacity * InputSize)
    {
        return false;
    }
    *this = MoveTemp(Loaded);
    return true;
}
