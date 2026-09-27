#pragma once

#include "CoreMinimal.h"
#include "HumanTypes.h"
#include "MotorLearning.h"

enum class ETalkChoice : uint8
{
    Greet,
    GreetBack,
    Introduce,
    IntroduceBack,
    AskHow,
    AndYou,
    TellNeed,
    TellDeed,
    TellMood,
    AdvisePlace,
    SameHere,
    Acknowledge,
    ReactDoing,
    KnowThat,
    CantRead,
    AskWhatWritten,
    AgreePlace,
    DisagreePlace,
    AskWhere,
    AgreePerson,
    DisagreePerson,
    DontKnowPerson,
    AskWhatHappened,
    Console,
    GladForYou,
    AgreeWeather,
    ReactDream,
    AskWhatStops,
    AskTeach,
    OfferMoney,
    Instruct,
    Encourage,
    AskPay,
    Retell,
    AnswerWhere,
    AnswerWhatHappened,
    AnswerWhatStops,
    AgreeTeach,
    RefuseTeach,
    AnswerPay,
    DontKnow,
    Thanks,
    ThanksForKindness,
    Obey,
    RefuseOrder,
    AcceptMoney,
    NoProblem,
    FarewellBack,
    Farewell,
    AskAdvice,
    TellBook,
    TellPlace,
    TellPerson,
    TellFeeling,
    TellWeather,
    TellDream,
    TellSkill,
    TellTrouble,
    TellWork,
    Nothing,
    Count
};

struct AB_API FTalkMoment
{
    EDialogueMove TheirMove = EDialogueMove::None;
    ETalkKind TheirKind = ETalkKind::None;
    float TheirValence = 0.0f;
    uint32 Covered = 0;
    int32 Turns = 0;
    int32 Part = 0;
    bool bFirstWords = true;
    bool bOpening = false;
    bool bAskedHow = false;
    bool bToldHow = false;
    bool bAskedAdvice = false;
    bool bLost = false;
    bool bLate = false;
    bool bBusy = false;
    bool bThemChild = false;
    bool bKnowName = false;
    bool bSeekingAdvice = false;
    float Closeness = 0.0f;
    float Liking = 0.0f;
    float Trust = 0.3f;
    float Familiarity = 0.0f;
    float Mood = 0.0f;
    float Urgency = 0.0f;
    float Contact = 0.5f;
    float Esteem = 0.5f;
    float Extraversion = 0.5f;
    float Agreeableness = 0.5f;
    float Openness = 0.5f;
};

struct AB_API FTalkKnowledge
{
    bool bKnowPlaceForTheirNeed = false;
    bool bFeelTheirNeed = false;
    bool bReadTheirBook = false;
    bool bCanRead = false;
    bool bKnowTheirPlace = false;
    bool bAgreeTheirPlace = false;
    bool bKnowTheirPerson = false;
    bool bAgreeTheirPerson = false;
    bool bTheirPersonIsMe = false;
    bool bKnowAnswer = false;
    bool bHaveSkill = false;
    bool bHaveMoney = false;
    bool bCanInstruct = false;
    bool bKnowPlaceForMyNeed = false;
    bool bUrgentNeed = false;
    bool bDoing = false;
    bool bFeeling = false;
    bool bHaveBook = false;
    bool bHavePlace = false;
    bool bHavePerson = false;
    bool bHaveDream = false;
    bool bHaveSkillToTell = false;
    bool bHaveTrouble = false;
    bool bHaveWork = false;
    bool bTheirTroubleIsMoney = false;
    bool bTheirTroubleIsJob = false;
    bool bKnowPlaceForJob = false;
};

struct AB_API FTalkOption
{
    ETalkChoice Choice = ETalkChoice::Acknowledge;
    ETalkKind Kind = ETalkKind::None;
    float Valence = 0.0f;
    float Words = 1.0f;
    bool bOnTopic = false;
    bool bRepeat = false;
    bool bGives = false;
};

struct AB_API FTalkOutcome
{
    float Tone = 0.0f;
    float Stayed = 1.0f;
    float Gain = 0.0f;
    float Warmth = 0.0f;
};

struct AB_API FTalkHeard
{
    EDialogueMove HearerSaid = EDialogueMove::None;
    ETalkKind HearerKind = ETalkKind::None;
    ETalkChoice Heard = ETalkChoice::Acknowledge;
    bool bOnTopic = false;
    bool bRepeat = false;
    bool bUseful = false;
    bool bHearerSad = false;
    bool bUnderstood = true;
    float SpeakerTone = 0.0f;
};

class AB_API FTalkRules
{
public:
    static EDialogueMove MoveOf(ETalkChoice Choice);
    static ETalkKind KindOf(ETalkChoice Choice, ETalkKind TheirKind);
    static bool Expects(ETalkChoice Choice);
    static bool Ends(ETalkChoice Choice);
    static const TCHAR* Name(ETalkChoice Choice);
    static void Menu(const FTalkMoment& Moment, const FTalkKnowledge& Know, TArray<FTalkOption>& Out);
    static float Felt(const FTalkHeard& Heard);
    static float Esteem(ETalkChoice Heard);
};

class AB_API FTalkCore
{
public:
    static constexpr int32 Ensemble = 3;
    static constexpr int32 Hidden = 48;
    static constexpr int32 OutcomeSize = 4;
    static constexpr int32 ChoiceCount = static_cast<int32>(ETalkChoice::Count);
    static constexpr int32 MoveCount = static_cast<int32>(EDialogueMove::Farewell) + 1;
    static constexpr int32 KindCount = static_cast<int32>(ETalkKind::Work) + 1;
    static constexpr int32 ScalarCount = 30;
    static constexpr int32 InputSize = ChoiceCount + MoveCount + KindCount * 2 + ScalarCount;

    int32 Capacity = 1500;
    int64 Lived = 0;
    int64 Lessons = 0;
    double Error = 1.0;

    static void Encode(const FTalkMoment& Moment, const FTalkOption& Option, float* Out);
    static float Value(const FTalkMoment& Moment, const float* Outcome);

    void Init(uint32 Seed);
    bool IsReady() const { return Nets.Num() == Ensemble && Nets[0].Size() > 0; }
    void Predict(const FTalkMoment& Moment, const FTalkOption& Option, float* OutMean, float* OutSpread) const;
    int32 Choose(const FTalkMoment& Moment, const TArray<FTalkOption>& Options, FRandomStream& Rng, float Temperature = 0.08f) const;
    void Remember(const FTalkMoment& Moment, const FTalkOption& Option, const FTalkOutcome& Outcome);
    void Practice(int32 Steps, FRandomStream& Rng);
    int32 Memories() const { return Stored; }

    void Serialize(FArchive& Ar);
    bool Save(const FString& Path);
    bool Load(const FString& Path);

private:
    TArray<FDeepNet> Nets;
    TArray<TArray<float>> Moments;
    TArray<float> Inputs;
    TArray<float> Targets;
    int32 AdamSteps = 0;
    int32 Stored = 0;
    int32 Next = 0;
};

class AB_API FTalkSchool
{
public:
    static FString Path(int32 Kind);
    static void Raise(FTalkCore& Core, int32 Kind, int32 Conversations, uint32 Seed, FString* OutReport);
};
