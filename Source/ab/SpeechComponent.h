// SpeechComponent.h
// ---------------------------------------------------------------------------
// Речь.
//
// Говорить — это делать. Поэтому реплика здесь не «строка», а поступок:
// у неё есть намерение (речевой акт), тон, правдивость и груз — знание,
// которое передаётся вместе со словами.
//
// Что именно человек скажет, зависит от того, что он чувствует, чего хочет,
// кому говорит и что, по его мнению, тот о нём думает.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "Lexicon.h"
#include "SpeechComponent.generated.h"

/** Всё, что нужно знать, чтобы понять, что сказать. */
USTRUCT(BlueprintType)
struct FSpeechContext
{
    GENERATED_BODY()

    // --- кто говорит ---
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FString SpeakerName;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FString ListenerName;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") TObjectPtr<AActor> Listener = nullptr;

    // --- состояние говорящего ---
    UPROPERTY(BlueprintReadWrite, Category = "Speech") EEmotionType Emotion = EEmotionType::None;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float EmotionIntensity = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Mood = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") ENeedType Need = ENeedType::SocialContact;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Stress = 0.0f;

    // --- отношения ---
    UPROPERTY(BlueprintReadWrite, Category = "Speech") ERelationKind Kind = ERelationKind::Stranger;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Closeness = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Trust = 0.3f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Liking = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Resentment = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Debt = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float BelievedLikingOfMe = 0.0f;

    /**
     * Насколько он, по-моему, считает, что я к нему плохо отношусь,
     * хотя это неправда. Отсюда берётся желание объясниться.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float bHeMisreadsMe = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") bool bFirstMeeting = false;

    // --- характер ---
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Extraversion = 0.5f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Agreeableness = 0.5f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Honesty = 0.6f;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float SelfEsteem = 0.5f;

    // --- что можно рассказать ---
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FString MemoryToShare;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FString OpinionToShare;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FString DreamText;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FString ComplaintText;
    /** О ком сплетничаем. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") TObjectPtr<AActor> GossipSubject = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FString GossipSubjectName;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float GossipValence = 0.0f;

    // --- обстоятельства ---
    UPROPERTY(BlueprintReadWrite, Category = "Speech") int32 Hour = 12;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") bool bTheyLookUpset = false;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") bool bIOweApology = false;
    /** Последнее, что мне сказали (чтобы ответить, а не говорить в пустоту). */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") ESpeechAct RespondingTo = ESpeechAct::Silence;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") bool bIsResponse = false;
};

/** Как реплика подействовала на слушателя. Разум применяет это к отношениям. */
USTRUCT(BlueprintType)
struct FSpeechEffect
{
    GENERATED_BODY()

    /** Насколько приятно было слышать -1..+1. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech") float Pleasantness = 0.0f;
    /** Какая потребность слушателя удовлетворена. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech") ENeedType SatisfiedNeed = ENeedType::SocialContact;
    UPROPERTY(BlueprintReadOnly, Category = "Speech") float SatisfactionAmount = 0.0f;
    /** Изменение доверия. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech") float TrustDelta = 0.0f;
    /** Эмоция, которую это вызвало у слушателя. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech") EEmotionType EvokedEmotion = EEmotionType::None;
    UPROPERTY(BlueprintReadOnly, Category = "Speech") float EvokedIntensity = 0.0f;
    /** Заподозрил ли слушатель ложь. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech") bool bSuspectedLie = false;
    /** Ждёт ли реплика ответа. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech") bool bExpectsReply = false;
};

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API USpeechComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USpeechComponent();

    // =======================================================================
    //  ЯЗЫК
    //
    //  Говорить не умеют от рождения. Слова берутся из одного-единственного
    //  места — из услышанного. Ребёнок знает ноль слов и может издать
    //  только звук; каждое слово, донёсшееся до него, понемногу усваивается,
    //  и однажды из них складывается фраза.
    //
    //  Взрослые говорят свободно не потому, что так задано, а потому что
    //  своё детство они уже прожили — просто не у нас на глазах.
    // =======================================================================

    /** Какие слова человек знает и насколько твёрдо (0..1). */
    UPROPERTY(BlueprintReadOnly, Category = "Speech|Язык")
    TMap<FName, float> Vocabulary;

    /** Освоил ли речь настолько, чтобы говорить не задумываясь. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech|Язык")
    bool bFluent = true;

    /** Насколько связно получается говорить 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech|Язык")
    float Fluency = 1.0f;

    /** Сколько слов усвоено твёрдо. */
    UFUNCTION(BlueprintCallable, Category = "Speech|Язык")
    int32 GetVocabularySize() const;

    /** Настроить речь под возраст: взрослый говорит, младенец — нет. */
    void SetupForAge(float Age);

    /**
     * Превратить задуманное в то, что действительно выйдет изо рта.
     * Слова, которых человек не знает, он произнести не может —
     * фраза рвётся, и остаётся только то, что он успел выучить.
     */
    FString Articulate(const FString& Intended, const TSet<FString>* Names = nullptr,
                       const FString& SelfName = FString(), const FString& ListenerName = FString()) const;

    /**
     * Услышал речь. Нового слова на слух не взять — только закрепить то,
     * что уже встречал в книге.
     */
    void LearnWords(const FString& Speech, float Attention, bool bChild);

    /** Прочитал текст — отсюда и только отсюда берутся новые слова. */
    void LearnWordsFromBook(const FString& Text, float Attention, bool bChild);

    UPROPERTY(BlueprintReadOnly, Category = "Speech|Язык")
    TMap<FString, float> Letters;

    UPROPERTY(BlueprintReadOnly, Category = "Speech|Язык")
    TSet<FName> Grounded;

    void NoticeThing(const FString& Thing);
    bool HasSeen(const FString& Thing) const;
    float Literacy() const;
    float LetterFamiliarity(TCHAR Letter) const;
    float Legibility(FName Word) const;
    int32 ReadText(const FString& Text, const TArray<FString>& Pictures, const FString& Heading, float Attention, bool bChild);
    void LearnLettersFrom(const USpeechComponent& Teacher, const FString& Shown, float Amount);
    void MasterLetters();

    /** Какую долю текста понял, если имена знакомых людей словами не считать. */
    float ComprehensionWithNames(const FString& Text, const TSet<FString>& Names) const;

    /** Голос ребёнка: мычит иначе, чем взрослый. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech|Язык")
    bool bChildVoice = false;

    UPROPERTY(BlueprintReadOnly, Category = "Speech|Язык")
    float EarAge = 30.0f;

    int32 WordsHeard = 0;
    void LearnOralSpeech(float Age);

    /** Привести слово к сравнимому виду. */
    static FName NormalizeWord(const FString& Raw);

    /** Какую долю услышанного человек понял 0..1. Знание едет только с понятым. */
    UFUNCTION(BlueprintCallable, Category = "Speech|Язык")
    float Comprehension(const FString& Text) const;

    /** Последнее, что человек сказал вслух. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech")
    FString LastSaid;

    /** Последние реплики — короткая история разговора. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech")
    TArray<FUtterance> RecentUtterances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speech")
    int32 HistoryCapacity = 12;

    /** Выбрать, что сейчас уместно сказать. */
    ESpeechAct ChooseAct(const FSpeechContext& Context) const;

    /** Составить реплику. */
    FUtterance Compose(ESpeechAct Act, const FSpeechContext& Context, AActor* Speaker);

    /** Как реплика подействует на того, кому она адресована. */
    static FSpeechEffect EvaluateEffect(const FUtterance& Utterance, float ListenerTrustInSpeaker,
                                        float ListenerCloseness, float ListenerEmpathy);

    /** Заметит ли слушатель ложь. */
    static bool DetectLie(const FUtterance& Utterance, float ListenerEmpathySkill,
                          float SpeakerDeceptionSkill, float Familiarity);

    /** Записать сказанное в историю. */
    void Remember(const FUtterance& Utterance);

    /** Текст реплики по акту и обстоятельствам. */
    static FString RenderText(ESpeechAct Act, const FSpeechContext& Context);

    /**
     * О чём человек сейчас говорит. Тема держится весь разговор,
     * поэтому беседа не рассыпается на несвязанные выкрики.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Speech")
    EWordTopic Topic = EWordTopic::Everyday;

    /** Сколько реплик уже сказано в этом разговоре. */
    UPROPERTY(BlueprintReadOnly, Category = "Speech")
    int32 TurnsInConversation = 0;

    /** Насколько человек разговорчив 0..1 — берётся из характера. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech")
    float Talkativeness = 0.5f;

    /** О чём человек собирается сказать. Ставится разумом перед репликой. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech")
    FTalkingPoint Speaking;

    /** О чём ему только что сказали — на это он и отвечает. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech")
    FTalkingPoint Heard;

    UPROPERTY(BlueprintReadWrite, Category = "Speech")
    bool bFemale = false;

    /** Название речевого акта по-русски (для отладки). */
    static FString ActLabel(ESpeechAct Act);

private:
    /** Обращение: «Игорь», «слушай», «эй» — в зависимости от близости. */
    static FString Address(const FSpeechContext& Context);
    /** Приветствие по времени суток. */
    static FString TimeGreeting(int32 Hour);
    /** Случайный вариант из списка. */
    static FString Pick(const TArray<FString>& Options);
};
