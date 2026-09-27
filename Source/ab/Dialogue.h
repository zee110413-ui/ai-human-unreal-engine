// Dialogue.h
// ---------------------------------------------------------------------------
// Как звучит разговор.
//
// Раньше реплика собиралась из случайных слов, и люди перебрасывались
// фразами, не связанными ни с чем. Здесь каждая фраза отвечает на
// конкретный ход собеседника и говорит о том, что есть у человека в голове.
//
// Род, «ты» и «вы» известны заранее, поэтому «был(а)» не бывает: женщина
// говорит «была», к старшему незнакомому обращаются на «вы».
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "HumanTypes.h"

/** Кто с кем говорит. */
struct FLineContext
{
    FString MyName;
    /** Пусто — имени собеседника не знаю. */
    FString TheirName;
    bool bMeFemale = false;
    bool bThemFemale = false;
    /** На «вы». */
    bool bFormal = false;
    bool bThemChild = false;
    float Mood = 0.0f;
    int32 Hour = 12;
    float Weather = 0.2f;
};

/** Почему человек думает о ком-то так, а не иначе. */
enum class EPersonReason : uint8
{
    None, Helped, Respected, Pleasant, Offended, Liar, Unpleasant
};

class AB_API FDialogueLines
{
public:
    // --- грамматика ---------------------------------------------------------
    static FString G(bool bFemale, const TCHAR* M, const TCHAR* F);
    static FString My(const FLineContext& C, const TCHAR* M, const TCHAR* F);
    static FString Ty(const FLineContext& C, const TCHAR* Informal, const TCHAR* Formal);
    static FString Capital(const FString& S);
    static FString Pick(const TArray<FString>& Options);

    /** Первое предложение текста — его и пересказывают. */
    static FString FirstSentence(const FString& Text);

    // --- места --------------------------------------------------------------
    static FString PlaceName(EPlaceKind K);
    static FString PlaceAt(EPlaceKind K);
    static FString PlaceTo(EPlaceKind K);
    /** «на север, недалеко» — как объясняют дорогу. */
    static FString WayTo(const FVector& From, const FVector& To);

    // --- ритуалы ------------------------------------------------------------
    static FString Greet(const FLineContext& C);
    static FString GreetBack(const FLineContext& C);
    static FString Introduce(const FLineContext& C);
    static FString IntroduceBack(const FLineContext& C);
    static FString HowAreYou(const FLineContext& C);
    static FString AndYou(const FLineContext& C);
    static FString Thanks(const FLineContext& C);
    static FString ThanksForKindness(const FLineContext& C);
    static FString NoProblem(const FLineContext& C);
    static FString Acknowledge(const FLineContext& C);
    static FString Farewell(const FLineContext& C, const FString& Reason);
    static FString FarewellBack(const FLineContext& C);
    static FString Busy(const FLineContext& C, const FString& Doing);

    // --- о себе -------------------------------------------------------------
    /** «на работу иду». Пусто — говорить не о чем. */
    static FString Doing(const FLineContext& C, EActionType Action, bool bOnTheWay);
    /** «есть хочется». Пусто — о таком вслух не говорят. */
    static FString Feel(const FLineContext& C, ENeedType Need);
    static FString MoodWord(const FLineContext& C, float Mood);
    static FString LeaveReason(const FLineContext& C, ENeedType Need, float Urgency, EActionType Next, bool bLate);

    // --- нужды --------------------------------------------------------------
    static FString AskWhereFor(const FLineContext& C, ENeedType Need);
    static FString AdvisePlace(const FLineContext& C, EPlaceKind K, const FString& Way);
    static FString DontKnowWhere(const FLineContext& C);
    static FString SameHere(const FLineContext& C, ENeedType Need);
    static FString ReactDoing(const FLineContext& C, EActionType Action);

    // --- книги --------------------------------------------------------------
    static FString TellReading(const FLineContext& C, const FString& Title, const FString& PageTitle);
    static FString AskWhatWritten(const FLineContext& C);
    static FString Retell(const FLineContext& C, const FString& Sentence);
    static FString KnowThat(const FLineContext& C);
    static FString DidntKnow(const FLineContext& C);
    static FString CantRead(const FLineContext& C);

    // --- места в разговоре --------------------------------------------------
    static FString TellPlace(const FLineContext& C, EPlaceKind K, float Affect);
    static FString AgreePlace(const FLineContext& C, EPlaceKind K, float Affect);
    static FString DisagreePlace(const FLineContext& C, EPlaceKind K, float MyAffect);
    static FString AskWhere(const FLineContext& C);
    static FString AnswerWhere(const FLineContext& C, EPlaceKind K, const FString& Way);

    // --- люди ---------------------------------------------------------------
    static FString TellPerson(const FLineContext& C, const FString& Name, bool bFemale,
                              EPersonReason Why, bool bWantLeader);
    static FString AgreePerson(const FLineContext& C, const FString& Name, bool bFemale, bool bGood);
    static FString DisagreePerson(const FLineContext& C, bool bFemale, bool bTheySaidGood);
    static FString DontKnowPerson(const FLineContext& C, bool bFemale);

    // --- чувства ------------------------------------------------------------
    static FString TellFeeling(const FLineContext& C, EEmotionType E);
    static FString AskWhatHappened(const FLineContext& C);
    static FString AnswerWhatHappened(const FLineContext& C, const FString& Reason);
    static FString Console(const FLineContext& C);
    static FString GladForYou(const FLineContext& C);

    // --- погода -------------------------------------------------------------
    static FString TellWeather(const FLineContext& C);
    static FString AgreeWeather(const FLineContext& C);

    // --- мечта --------------------------------------------------------------
    static FString TellDream(const FLineContext& C, const FString& Dream);
    static FString ReactDream(const FLineContext& C);
    static FString AskWhatStops(const FLineContext& C);
    static FString AnswerWhatStops(const FLineContext& C, bool bNoMoney, bool bAfraid);

    // --- умения -------------------------------------------------------------
    /** Пусто — таким не хвастаются. */
    static FString TellSkill(const FLineContext& C, FName Skill);
    static FString AskTeach(const FLineContext& C);
    static FString AgreeTeach(const FLineContext& C);
    static FString RefuseTeach(const FLineContext& C);

    // --- беды и работа ------------------------------------------------------
    static FString TellNoJob(const FLineContext& C);
    static FString TellNoMoney(const FLineContext& C);
    static FString Encourage(const FLineContext& C);
    static FString OfferMoney(const FLineContext& C);
    static FString AcceptMoney(const FLineContext& C);
    static FString TellWork(const FLineContext& C, EPlaceKind K);
    static FString AskPay(const FLineContext& C);
    static FString AnswerPay(const FLineContext& C, bool bGood);

    // --- распоряжения -------------------------------------------------------
    static FString Instruct(const FLineContext& C, EActionType Action, EPlaceKind K, const FString& Way);
    static FString Obey(const FLineContext& C);
    static FString AskForAdvice(const FLineContext& C);
    static FString RefuseOrder(const FLineContext& C, const FString& Reason);

    /**
     * Все слова, которые встречаются в речи.
     * Нужно, чтобы проверить: есть ли каждое из них хоть в одной книге.
     * Если нет — этого слова жителям взять неоткуда.
     */
    static void CollectSpeechWords(TSet<FString>& Out);
};
