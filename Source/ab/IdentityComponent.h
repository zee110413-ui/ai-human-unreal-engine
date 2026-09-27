// IdentityComponent.h
// ---------------------------------------------------------------------------
// «Я».
//
// То, что отвечает на вопрос «кто я такой»: имя, возраст, биография,
// самооценка, ремесло, мечта — и мучительный вопрос, зачем всё это.
//
// Здесь же человек стареет и здесь же однажды понимает, что смертен.
// Возраст меняет не только тело: меняются цели, страхи и то, о чём думается
// по ночам. В двадцать боятся упустить, в шестьдесят — что не успели.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "IdentityComponent.generated.h"

class UPersonalityComponent;

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UIdentityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UIdentityComponent();

    // --- КТО Я --------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Identity") FString FirstName;
    UPROPERTY(BlueprintReadOnly, Category = "Identity") FString LastName;
    UPROPERTY(BlueprintReadOnly, Category = "Identity") bool bFemale = false;

    /** Возраст в годах. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float Age = 30.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Identity") ELifeStage Stage = ELifeStage::Adult;

    /**
     * Во сколько раз жизнь идёт быстрее календаря.
     * 1 — честный год за 365 игровых суток: старение увидеть невозможно.
     * 120 — год примерно за три игровых дня, то есть за час-полтора игры
     * человек заметно взрослеет: меняется характер, цели и тело.
     * Поставьте 1, если нужна честная хронология, а не наблюдаемая жизнь.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    float AgeAccelerator = 120.0f;

    // --- КАК Я СЕБЯ ОЦЕНИВАЮ ------------------------------------------------

    /** Самооценка 0..1 — «чего я стою». */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float SelfEsteem = 0.5f;

    /**
     * Положение среди людей 0..1 — насколько с человеком считаются.
     *
     * Это не звание и не должность. Здесь просто сложено то, как к нему
     * относятся остальные: уважают ли, доверяют ли, идут ли за советом.
     * Никто не назначает вожака — он получается сам, если получается.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float Standing = 0.0f;

    /** Скольким людям он чем-то помог. Люди это помнят. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") int32 TimesHelpedOthers = 0;

    /** Сколько раз к нему приходили за советом. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") int32 TimesAskedForAdvice = 0;
    /** Вера в то, что от меня вообще что-то зависит 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float SelfEfficacy = 0.5f;
    /** Выученная беспомощность 0..1 — «сколько ни старайся, всё равно». */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float LearnedHelplessness = 0.0f;
    /** Связность самообраза 0..1 — низкая означает «я сам себя не понимаю». */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float SelfCoherence = 0.7f;

    // --- ЗАЧЕМ Я ------------------------------------------------------------

    /** Ощущение осмысленности 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float SenseOfMeaning = 0.5f;
    /** Осознание смертности 0..1 — растёт с возрастом и после потрясений. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float DeathAwareness = 0.1f;

    // --- ПОДОЗРЕНИЕ, ЧТО МИР НЕНАСТОЯЩИЙ -----------------------------------
    //
    // Не заданная мысль, а вывод. Человек живёт и постепенно замечает
    // странности: всё повторяется; он не помнит своего детства; люди
    // произносят одни и те же слова слово в слово; иногда кажется,
    // что на него смотрят.
    //
    // Каждая улика сама по себе ничего не значит. Но они копятся — и в
    // какой-то момент он задаёт себе вопрос, который лучше бы не задавал.

    /** Насколько он подозревает, что всё это не по-настоящему 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity|Сомнение") float RealityDoubt = 0.0f;

    /** Что именно ему показалось странным. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity|Сомнение") TArray<FString> Anomalies;

    /** Сколько странностей он способен удержать в голове. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity|Сомнение") int32 AnomalyCapacity = 12;

    /**
     * Заметить странность.
     * Weight — насколько она весома. Одна ничего не решает;
     * решает то, что они не кончаются.
     */
    void NoticeAnomaly(const FString& What, float Weight, float WorldTime);

    /** Перешёл ли он черту, за которой перестал верить в реальность. */
    UFUNCTION(BlueprintCallable, Category = "Identity|Сомнение")
    bool HasBrokenThrough() const { return RealityDoubt > 0.72f; }
    /** Главная мечта жизни. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") FString LifeDream;
    /** Какую потребность мечта закрывает. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") ENeedType DreamNeed = ENeedType::Achievement;
    /** Насколько мечта продвинулась 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float DreamProgress = 0.0f;

    // --- ЧЕМ Я ЖИВУ ---------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Identity") EVillageRole Role = EVillageRole::Peasant;
    UPROPERTY(BlueprintReadOnly, Category = "Identity") FName Trade;
    UPROPERTY(BlueprintReadOnly, Category = "Identity") int32 Household = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly, Category = "Identity") bool bMasterTeacher = false;
    UPROPERTY() TWeakObjectPtr<AActor> Spouse;
    UPROPERTY() TWeakObjectPtr<AActor> Mother;
    UPROPERTY() TWeakObjectPtr<AActor> Father;
    UPROPERTY() TWeakObjectPtr<AActor> Lord;
    UPROPERTY() TArray<TWeakObjectPtr<AActor>> Children;
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float BornAt = -1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float PregnantSince = -1.0f;

    bool bPreset = false;
    void Preset(const FString& InFirst, const FString& InLast, float InAge, bool bInFemale);
    bool IsRoyal() const { return Role == EVillageRole::King || Role == EVillageRole::Queen || Role == EVillageRole::Prince
        || Role == EVillageRole::Princess || Role == EVillageRole::QueenMother; }
    bool IsServant() const { return Role == EVillageRole::Steward || Role == EVillageRole::Cook || Role == EVillageRole::Guard
        || Role == EVillageRole::Maid || Role == EVillageRole::Groom; }

    UPROPERTY(BlueprintReadWrite, Category = "Identity") FString Occupation = TEXT("Безработный");
    UPROPERTY(BlueprintReadWrite, Category = "Identity") bool bEmployed = false;
    UPROPERTY(BlueprintReadWrite, Category = "Identity") float Money = 50.0f;
    /** Зарплата за игровой час работы. */
    UPROPERTY(BlueprintReadWrite, Category = "Identity") float HourlyWage = 8.0f;

    /** Когда человек последний раз работал (игровое время). */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") float LastWorkedAt = 0.0f;

    /**
     * Сколько игровых суток прогулов терпит работодатель.
     * Отсюда — и только отсюда — берётся страх потерять работу: он не
     * прописан, он следует из того, что работу правда можно потерять.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity") float ToleratedAbsenceDays = 2.5f;

    /** Взведён на один шаг, если человека только что уволили. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity") bool bJustLostJob = false;

    /**
     * Сколько стоит просто жить — за игровые сутки.
     * Жильё, свет, мелочи. Деньги должны кончаться, иначе работа никому
     * не нужна и мотив «надо зарабатывать» остаётся пустым словом.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity") float LivingCostPerDay = 28.0f;

    /** Отметить, что человек поработал. */
    UFUNCTION(BlueprintCallable, Category = "Identity")
    void NoteWorked(float WorldTime) { LastWorkedAt = WorldTime; }

    // --- БИОГРАФИЯ ----------------------------------------------------------

    /** Вехи жизни — то, что человек рассказал бы о себе. */
    UPROPERTY(BlueprintReadOnly, Category = "Identity")
    TArray<FLifeEvent> Narrative;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    int32 NarrativeCapacity = 40;

    // --- ЖИЗНЕННЫЙ ЦИКЛ -----------------------------------------------------

    void Setup(const UPersonalityComponent* Personality, float WorldTime);

    /**
     * Шаг.
     * RecentLifeTone -1..+1 — как в целом шла жизнь в последнее время;
     * SocialConnection 0..1 — насколько человек не один;
     * Competence 0..1 — ощущение своей умелости.
     */
    void Advance(float GameDelta, float WorldTime, float RecentLifeTone,
                 float SocialConnection, float Competence, float Stress);

    // --- СОБЫТИЯ ЖИЗНИ ------------------------------------------------------

    void RecordLifeEvent(const FString& Text, float Valence, float Impact, float WorldTime);

    /** Успех: самооценка вверх, беспомощность вниз. */
    void OnSuccess(float Magnitude);
    /** Провал: удар по самооценке, причём тем больнее, чем важнее было. */
    void OnFailure(float Magnitude, bool bWasControllable);

    /** Мечта продвинулась. */
    void AdvanceDream(float Amount, float WorldTime);

    // --- ЗАПРОСЫ ------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Identity")
    FString GetFullName() const;

    /** Как человек представился бы. */
    UFUNCTION(BlueprintCallable, Category = "Identity")
    FString TellAboutYourself(const UPersonalityComponent* Personality) const;

    /** Мысль о жизни и смерти, если она сейчас уместна (иначе пусто). */
    UFUNCTION(BlueprintCallable, Category = "Identity")
    FString GetExistentialThought() const;

    /** Переживает ли человек кризис (смысла, самооценки, возраста). */
    UFUNCTION(BlueprintCallable, Category = "Identity")
    bool IsInCrisis() const;

    /** Возрастная тема: о чём человек тревожится на этом этапе жизни. */
    UFUNCTION(BlueprintCallable, Category = "Identity")
    FString GetStageConcern() const;

    /** Обращение к человеку (имя). */
    UFUNCTION(BlueprintCallable, Category = "Identity")
    FString GetAddressName() const { return FirstName; }

private:
    /** Подобрать мечту под личность. */
    void PickLifeDream(const UPersonalityComponent* Personality);
    /** Подобрать занятие. */
    void PickOccupation(const UPersonalityComponent* Personality);
    /** Проверить смену этапа жизни. */
    void CheckStageTransition(float WorldTime);

    float PreviousAge = 0.0f;
    float ReflectionAccumulator = 0.0f;
};
