// MindComponent.h
// ---------------------------------------------------------------------------
// Разум. Здесь всё сходится.
//
// Когнитивный цикл:
//   воспринял → выделил важное → оценил → почувствовал → запомнил →
//   захотел → решил → сделал → подумал об этом.
//
// Плюс то, без чего человек не человек:
//   - поток сознания: он непрерывно думает словами, и эти слова можно читать;
//   - внимание: в сознание попадает не всё, а только то, что перекричало
//     остальное, — поэтому человек бывает невнимателен и что-то пропускает;
//   - сон с фазами и сновидениями из обрывков памяти;
//   - разговор как обмен поступками, а не репликами.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "HumanWorldSubsystem.h"
#include "DeliberationComponent.h"
#include "Textbook.h"
#include "Dialogue.h"
#include "MindLearning.h"
#include "TalkLearning.h"
#include "MindComponent.generated.h"

class ACompleteHumanNPC;
class UPersonalityComponent;
class UPhysiologyComponent;
class UNeedComponent;
class UEmotionComponent;
class UMemoryComponent;
class UIdentityComponent;
class USocialComponent;
class UMotivationComponent;
class USpeechComponent;

/** Разговор, который сейчас идёт, — как его видит один из двоих. */
USTRUCT()
struct FDialogueState
{
    GENERATED_BODY()

    /** С кем. */
    UPROPERTY() TObjectPtr<AActor> With = nullptr;

    /** Что собеседник сказал последним и о чём. */
    EDialogueMove TheirMove = EDialogueMove::None;
    UPROPERTY() FTalkingPoint TheirPoint;
    FString TheirText;

    /** Что последним сказал я. */
    EDialogueMove MyMove = EDialogueMove::None;
    UPROPERTY() FTalkingPoint MyPoint;

    /** Мой ли ход и когда я его сделаю (реальные секунды). */
    bool bMyTurn = false;
    double ReplyAt = 0.0;
    double LastActivityAt = 0.0;

    int32 Turns = 0;
    int32 TurnLimit = 6;
    bool bAskedHow = false;
    bool bToldHow = false;

    /** Друг друга не поняли: слов не хватило. */
    bool bLost = false;

    /** За советом уже спросил — второй раз не спрашивают. */
    bool bAskedAdvice = false;

    /** О чём уже поговорили — второй раз о том же не заводят. */
    TArray<FString> Covered;
};

/** Совет или распоряжение, которое человек услышал и может выполнить. */
USTRUCT()
struct FSuggestion
{
    GENERATED_BODY()

    UPROPERTY() FAffordance Affordance;
    UPROPERTY() TObjectPtr<AActor> From = nullptr;

    /** Насколько хочется последовать 0..1 — от уважения к тому, кто сказал. */
    float Weight = 0.0f;

    /** До какого игрового времени совет помнится. */
    float ExpiresAt = 0.0f;

    /** Велели, а не посоветовали. */
    bool bInstruction = false;
};

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UMindComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMindComponent();

    // --- СОЗНАНИЕ -----------------------------------------------------------

    /** Поток сознания — то, что человек думает прямо сейчас и думал недавно. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    TArray<FThought> Stream;

    /** Самая свежая мысль. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    FString CurrentThought;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mind")
    int32 StreamCapacity = 24;

    /** Рабочая память: то, что сейчас в поле внимания (7±2 элемента). */
    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    TArray<FPercept> WorkingMemory;

    /** На кого/что направлено внимание. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    TObjectPtr<AActor> AttentionTarget = nullptr;

    /** Сосредоточенность 0..1. Падает от усталости, боли и тревоги. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    float Focus = 0.7f;

    // --- ДЕЙСТВИЕ -----------------------------------------------------------

    /** То, что человек решил делать, и почему. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    FIntention ActiveIntention;

    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    EActionType CurrentAction = EActionType::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    FString CurrentActionLabel;

    /** Сколько игровых секунд осталось до конца текущего действия. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    float ActionTimeLeft = 0.0f;

    /** С кем сейчас разговор. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    TObjectPtr<AActor> ConversationPartner = nullptr;

    // --- СОН ----------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    bool bAsleep = false;

    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    ESleepPhase SleepPhase = ESleepPhase::Awake;

    /** Последний сон. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind")
    FString LastDream;

    // --- ОТЛАДКА ------------------------------------------------------------

    /** Показывать мысли над головой. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mind|Debug")
    bool bShowThoughts = true;

    /** Печатать мысли в лог. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mind|Debug")
    bool bLogThoughts = false;

    /** На каком расстоянии видны мысли (см). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mind|Debug")
    float ThoughtVisibleDistance = 2500.0f;

    // --- ЖИЗНЬ --------------------------------------------------------------

    /** Найти соседние компоненты и связать всё воедино. */
    void Awaken();

    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    bool HasLearnedMind() const { return bCoreReady; }
    FString DescribeCore() const;
    FString LastChoiceNote;

    /** Главный шаг. RealDelta — реальные секунды кадра. */
    void Advance(float RealDelta);

    // --- ВХОД ---------------------------------------------------------------

    /** Что-то попало в поле восприятия. */
    void Notice(AActor* What, FName SenseKind, const FVector& Where, float RawSalience);

    /** Событие мира рядом. */
    void OnWorldEvent(const FWorldEvent& Event);

    /** Мне что-то сказали. */
    void Hear(const FUtterance& Utterance);

    /** Мне сделали больно. */
    void OnHurt(AActor* By, float Severity, bool bIntentional);

    /** Мне помогли. */
    void OnHelped(AActor* By, float Magnitude);

    /** Умер близкий человек. Горе — это не эмоция, а отдельная работа. */
    void Grieve(AActor* Who, float Closeness);

    /** Уложить спать принудительно (например, добрался до кровати). */
    UFUNCTION(BlueprintCallable, Category = "Mind")
    void ForceSleep();

    /** Разбудить. */
    UFUNCTION(BlueprintCallable, Category = "Mind")
    void ForceWake();

    // --- МЫСЛИ --------------------------------------------------------------

    /** Добавить мысль в поток. */
    void Think(const FString& Text, EThoughtKind Kind, float Salience, AActor* About = nullptr);

    void FeelFooting(float SinkCm, float Stick, float Grip, float Effort, float WaterCm, const FVector& Where);
    void FeelFall(float Speed, float Impact, const FVector& Where);

    /** Что человек сейчас думает — одной строкой. */
    UFUNCTION(BlueprintCallable, Category = "Mind")
    FString GetInnerVoice() const { return CurrentThought; }

    /** Полная сводка состояния — для отладочной панели. */
    UFUNCTION(BlueprintCallable, Category = "Mind")
    FString GetStatusReport() const;

    // --- ДОСТУП К ПОДСИСТЕМАМ ----------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Mind") TObjectPtr<UPersonalityComponent> Personality;
    UPROPERTY(BlueprintReadOnly, Category = "Mind") TObjectPtr<UPhysiologyComponent> Body;
    UPROPERTY(BlueprintReadOnly, Category = "Mind") TObjectPtr<UNeedComponent> Needs;
    UPROPERTY(BlueprintReadOnly, Category = "Mind") TObjectPtr<UEmotionComponent> Emotions;
    UPROPERTY(BlueprintReadOnly, Category = "Mind") TObjectPtr<UMemoryComponent> Memory;
    UPROPERTY(BlueprintReadOnly, Category = "Mind") TObjectPtr<UIdentityComponent> Identity;
    UPROPERTY(BlueprintReadOnly, Category = "Mind") TObjectPtr<USocialComponent> Social;
    UPROPERTY(BlueprintReadOnly, Category = "Mind") TObjectPtr<UMotivationComponent> Motivation;
    UPROPERTY(BlueprintReadOnly, Category = "Mind") TObjectPtr<USpeechComponent> Speech;
    UPROPERTY(BlueprintReadOnly, Category = "Mind") TObjectPtr<class UDeliberationComponent> Deliberation;

protected:
    // --- Этапы когнитивного цикла ------------------------------------------

    /** Отобрать из воспринятого то, что дойдёт до сознания. */
    void UpdateAttention(float GameDelta);
    /** Оценить то, что в фокусе, и почувствовать. */
    void AppraiseWorld(float GameDelta);
    /** Обновить цели и выбрать, чем заняться. */
    void Deliberate(float GameDelta);
    /** Делать то, что решено. */
    void Act(float GameDelta);
    /** Подумать о себе и происходящем. */
    void Reflect(float GameDelta);

    /**
     * Заметить закономерность и сделать вывод.
     *
     * Это общая способность, а не набор особых случаев: человек смотрит
     * на прожитое и ищет в нём повторяющееся. Что именно он поймёт —
     * заранее не определено. Может выйти «с Игорем всегда легко», может
     * «мне вечно не хватает своего угла», а может — что всё вокруг
     * повторяется слишком точно, чтобы быть правдой.
     *
     * Все выводы делаются одним и тем же способом и из одних и тех же
     * данных: из того, что человек прожил и запомнил.
     */
    void DrawConclusions();

    /** Записать вывод: он попадёт и в мысли, и в разговоры, и в решения. */
    void Conclude(FName Subject, FName Predicate, float Value, const FString& Text, float Confidence);
    /** Отработать телесные и душевные последствия происходящего. */
    void Regulate(float GameDelta);

    // --- ВОЗМОЖНОСТИ --------------------------------------------------------
    // Человек не перебирает правила — он осматривается и видит, что можно
    // сделать. Всё, что он увидел, взвешивается одной и той же меркой.

    /** Собрать всё, что сейчас можно сделать. */
    void GatherAffordances(TArray<FAffordance>& Out) const;
    /** Возможности, которые предлагают люди вокруг. */
    void GatherSocialAffordances(TArray<FAffordance>& Out) const;
    /** Возможности, которые человек носит с собой: подумать, отдохнуть, пройтись. */
    void GatherSelfAffordances(TArray<FAffordance>& Out) const;
    /** Возможности мест, о которых он помнит, но которые сейчас далеко. */
    void GatherRememberedAffordances(TArray<FAffordance>& Out) const;

    // --- Действия -----------------------------------------------------------

    /** Приступить к тому, что решено. */
    void BeginIntention(const FIntention& Intention);
    /** Довести до конца и извлечь из этого урок. */
    void CompleteIntention(bool bReached);
    /**
     * Сравнить ожидание с действительностью.
     * Возвращает, чем всё обернулось на самом деле: -1..+1.
     */
    float MeasureOutcome() const;
    /** Применить последствия: раздать то, что было обещано. */
    void ApplyPromises(const FAffordance& A, float SuccessScale);
    /** Снять мерку состояния, чтобы потом было с чем сравнить. */
    void SnapshotState();
    /** Чем кончилось выяснение отношений: словами или руками. */
    void ResolveConfrontation(AActor* Target);
    /** Освободить занятое место (кровать, стул, станок). */
    void ReleaseOccupied();

    // --- Сон ----------------------------------------------------------------

    void BeginSleep();
    void EndSleep(bool bNaturally);
    void UpdateSleep(float GameDelta);
    void Dream();

    // --- Разговор -----------------------------------------------------------

    /** Сказать что-нибудь собеседнику. */
    void SpeakTo(AActor* Listener);
    /** Случайная встреча: заговорить ли с тем, кто оказался рядом. */
    void HandleEncounters();
    /** Попытка найти работу. Отказ — тоже результат, и он копится. */
    void TryFindJob();

    /**
     * Прочитать страницу учебника.
     * Единственный путь, которым человек узнаёт то, чего сам не переживал.
     * Что он усвоит — зависит от того, насколько он умеет читать.
     */
    void StudyTextbook(AActor* What);

public:
    /** Закладки: где человек остановился в каждой книге. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind|Чтение")
    TArray<FReadingBookmark> Bookmarks;

    /** Последнее, что он прочёл глазами, — прямо этими словами. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind|Чтение")
    FString LastRead;

    FReadingBookmark& BookmarkFor(FName Subject);

    /**
     * Собрать всё, о чём человеку сейчас есть что сказать.
     *
     * Это и есть содержание его речи: нужда, недавнее дело, знакомое место,
     * встреченный человек, прочитанная страница, нажитое мнение. Раньше
     * реплика собиралась из случайных слов, и выходил набор звуков; теперь
     * она о чём-то, и собеседнику есть что ответить.
     */
    void GatherTalkingPoints(TArray<FTalkingPoint>& Out) const;

    /** Выбрать, о чём заговорить с этим человеком. */
    FTalkingPoint ChooseTalkingPoint(AActor* Listener) const;

    /**
     * Посмотреть, как человек ведёт себя с другими, и сделать выводы.
     *
     * Здесь и рождается вожак. Никто его не назначает: люди просто видят,
     * что один помогает, держит слово, толково говорит и знает больше
     * прочих, — и начинают с ним считаться. А потом идут к нему за советом,
     * потому что к тому, кого уважают, идут сами.
     */
    void JudgeByDeeds(AActor* Whom, float Delta, const FString& Why);

    /** Пересчитать, насколько с человеком считаются в городе. */
    void UpdateStanding();

    /** О чём шёл разговор — чтобы не перескакивать и отвечать по существу. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind|Речь")
    FTalkingPoint CurrentTopic;

    /** Что последним сказал собеседник. */
    UPROPERTY(BlueprintReadOnly, Category = "Mind|Речь")
    FTalkingPoint HeardTopic;

    // --- РАЗГОВОР -----------------------------------------------------------

    /** Разговор, который сейчас идёт. */
    UPROPERTY()
    FDialogueState Dialogue;

    /** Что посоветовали или велели сделать. */
    UPROPERTY()
    TArray<FSuggestion> Suggestions;

    /** Заговорить: поздороваться или представиться. */
    bool StartConversation(AActor* Other);

    /** Вести разговор: ответить, когда подошла очередь. */
    void UpdateDialogue(float RealDelta);

    /** Разговор кончился — молча, без прощания (собеседник ушёл). */
    void EndConversation();

    bool IsInConversation() const { return Dialogue.With != nullptr; }
    bool IsTalkingWith(const AActor* Other) const { return Other && Dialogue.With == Other; }

    bool IsPerformingAction() const { return bActionActive && !bMoving && Phase == EActionPhase::Doing; }

    UPROPERTY(BlueprintReadWrite, Category = "Mind")
    bool bPolicyControlled = false;

    FVector2D PolicyMove = FVector2D::ZeroVector;

    bool IsBusyForPolicy() const { return bActionActive || IsInConversation(); }

    float Mastery(FName What) const;
    FName TryInvent(float Curiosity, float Openness, FString& OutInsight);
    void Practised(FName What, float Amount);
    void SawSomeoneDo(FName What, float TheirMastery, float Attention);
    bool TryDo(FName What, float Difficulty, float& OutChance) const;
    FName BestMastery(float& OutLevel) const;
    float Competence() const;
    static FString MasteryLabel(FName What);

    float PerformCraft(const FAffordance& A, float T);
    void GatherHandCrafts(TArray<FAffordance>& Out) const;
    void GatherVillageAffordances(TArray<FAffordance>& Out) const;
    void FilterDeals(TArray<FAffordance>& Out) const;
    bool SettleDeal(const FAffordance& A);
    class AResourceActor* TakeCargo(EResourceKind Kind, float Amount);
    bool PickCastleTask(FAffordance& OutTask, FString& OutWords) const;
    void CheckOrders(float GameDelta);
    void CheckFamily(float GameDelta);
    void ReceiveOrder(AActor* From, const FAffordance& Task, const FString& Words);
    float FamilyCheck = 0.0f;
    float ProvisionFelt = 1.0f;
    float ProvisionClock = 0.0f;
    void InfantReflex(float GameDelta);
    void FeelIdleness(float GameDelta);
    float WorkedToday = 0.0f;
    int32 WorkDay = -1;
    void HearBabyCry(const class ACompleteHumanNPC* Baby, bool bParent, float GameDelta);
    float CryHeardAt = -1000.0f;
    float CryClock = 0.0f;
    bool bCrying = false;

    TWeakObjectPtr<AActor> OrderFrom;
    FName OrderKey;
    float OrderAt = -1.0f;
    float OrderCheck = 0.0f;
    int32 OrdersDone = 0;
    int32 OrdersIgnored = 0;
    bool HoldsThing(EResourceKind Kind) const;
    float RecallOfDeed(FName Deed) const;
    void PolicyUse(const FAffordance& Affordance);
    void PolicyStop();
    bool PolicyTalk(AActor* Other);
    void Reborn();

    float GetHurry() const
    {
        if (!bActionActive)
        {
            return 0.0f;
        }
        if (CurrentAction == EActionType::Flee || CurrentAction == EActionType::Exercise)
        {
            return 1.0f;
        }
        return FMath::Clamp((ActiveIntention.Valuation.NeedGain - 0.6f) / 0.8f, 0.0f, 1.0f);
    }

    /** Реплика дошла до меня. */
    void OnDialogueHeard(const FUtterance& U);

    /**
     * Прочитать все книги разом — как будто человек прожил их все.
     *
     * Так получают «учителя»: того, кто знает, как делается всё, о чём
     * написано в городе. Он может не уметь — но он знает, как, и может
     * показать другим. Остальные учатся у него глядя и спрашивая.
     */
    void PreloadKnowledge();

private:
    void TeachReading(AActor* PupilActor, float GameDelta);
    void TeachProduction(AActor* PupilActor, float GameDelta);
    float TeachAccumulator = 0.0f;
    TWeakObjectPtr<class AFurnitureActor> PlannedSeat;
    float GraspRetryAt = 0.0f;
    TWeakObjectPtr<AActor> ReportedGrasp;

    /** Остановиться ради разговора, если не занят ничем важным. */
    void StayToTalk(AActor* Other);

    /** Сделать свой ход. */
    void TakeTurn();

    /** Сказать вслух: увидят, услышат и поймут настолько, насколько знают слова. */
    void Say(const FString& Text, EDialogueMove Move, const FTalkingPoint& Point, const FUtterance& Extra);

    FLineContext MakeLineContext(AActor* Other) const;

    /** О чём заговорить самому. */
    FTalkingPoint PickOwnTopic(AActor* Listener) const;

    /** Место из собственной памяти, где закрывается эта нужда. */
    const FKnownLocation* KnownPlaceFor(ENeedType Need) const;
    const FKnownLocation* KnownPlaceOfKind(EPlaceKind Kind) const;

    /** Что за место на самом деле стоит в этой точке. */
    EPlaceKind RealKindAt(const FVector& Where, EPlaceKind Fallback) const;

    /** Причина самого сильного чувства — если её можно сказать вслух. */
    FString FeelingReason() const;

    /** Велеть человеку сделать то, что ему, по-моему, нужно. */
    bool TryInstruct(const FLineContext& C, FString& OutText, FUtterance& OutExtra);

    /** Совет запомнился как возможность. */
    void AddSuggestion(const FAffordance& A, AActor* From, float Weight, bool bInstruction);

    /** Совет исполнен — и оказался дельным или нет. */
    void JudgeSuggestionOutcome(const FAffordance& Done, float Outcome);

    /** Когда в последний раз пересчитывалось положение в городе. */
    float StandingAccumulator = 0.0f;

public:

private:
    /** Насколько уместно сейчас заговорить. */
    bool CanTalkTo(AActor* Other) const;

    // --- Внутренняя речь ----------------------------------------------------

    /** Сгенерировать очередную мысль. */
    void GenerateThought();
    /** Мысль о теле. */
    FString ThoughtAboutBody() const;
    /** Мысль о чувстве. */
    FString ThoughtAboutFeeling() const;
    /** Мысль о человеке в поле зрения. */
    FString ThoughtAboutPerson(AActor* Who) const;
    /** Навязчивое пережёвывание. */
    FString Rumination() const;
    /** Мечта или тревога о будущем. */
    FString ProspectiveThought() const;

    // --- Вспомогательное ----------------------------------------------------

    ACompleteHumanNPC* GetHuman() const;
    UHumanWorldSubsystem* GetWorldMind() const;
    float Now() const;
    /**
     * Сколько игрового времени отвести на дорогу длиной Distance.
     * Считается по реальной скорости ходьбы: игровое время течёт быстрее
     * реального, и без этого пересчёта человек бросал бы любой путь,
     * не пройдя и половины.
     */
    float EstimateTravelBudget(float Distance) const;
    /** Имя человека, как я его знаю. */
    FString NameOf(AActor* Who) const;
    /** Отладочная отрисовка. */
    void DrawDebug();

private:
    /** Накопители для разночастотных подсистем. */
    float BodyAccumulator = 0.0f;
    float CycleAccumulator = 0.0f;
    float ThoughtAccumulator = 0.0f;
    float DeliberateAccumulator = 0.0f;
    float SpeechCooldown = 0.0f;

    /** Случайный сдвиг фазы, чтобы толпа не думала синхронно. */
    float PhaseOffset = 0.0f;

    /** Действие в процессе. */
    bool bActionActive = false;
    float ActionTotalDuration = 0.0f;
    /** Куда идём, если действие — перемещение. */
    FVector MoveDestination = FVector::ZeroVector;
    bool bMoving = false;
    float MoveTimeout = 0.0f;

    /** На каком этапе дела человек сейчас. */
    EActionPhase Phase = EActionPhase::None;

    /**
     * Сколько ещё человек держится принятого решения.
     *
     * Без этого он пересматривал замысел каждый миг и оттого метался:
     * решил сходить за продуктами — и тут же передумал, не сделав шага.
     * Живой человек, решившись, какое-то время не спорит сам с собой.
     */
    float CommitmentLeft = 0.0f;

    /** Сколько ещё возиться, прежде чем начнётся само дело. */
    float PrepareTimeLeft = 0.0f;

    /** Выполнить условия дела: взять вещь, сесть. Возвращает, что осталось сделать. */
    bool PrepareForAction();

    /** Прибрать за собой: положить вещь, встать. */
    void FinishAction();

    /** Есть ли под рукой то, что дело требует израсходовать. */
    bool HaveWhatItTakes(const FAffordance& A) const;

    /**
     * Оглядеться на месте и уточнить замысел.
     *
     * Память хранит «там можно почитать», а не «там лежит вот эта книга».
     * Придя, человек видит настоящие вещи — и берётся за ту, что перед ним.
     */
    void RefreshIntentionFromSurroundings();

public:
    /**
     * Записать в летопись то, что человек сделал прямо сейчас.
     *
     * Мысли видно и так, а вот поступки до сих пор оставались за кадром:
     * со стороны казалось, будто человек стоит и думает. Теперь в летопись
     * попадает каждое действие — пошёл, взял, сел, сказал, съел, встал.
     */
    void Report(const FString& What) const;

private:

    /** Истратить нужное и получить произведённое: продукты, урожай, покупку. */
    void SettleResources(const FAffordance& A);

    /** Найти поблизости вещь, в которой хранят припасы. */
    class AFurnitureActor* FindStorageNear(EResourceKind What, float Radius, const FVector* Around = nullptr) const;
    bool CouldDo(const FAffordance& A) const;

    /** Сколько игровых секунд длится текущая фаза сна. */
    float SleepPhaseTimer = 0.0f;
    float TotalSleepTime = 0.0f;

    /** Кому я должен извиниться. */
    UPROPERTY() TObjectPtr<AActor> OwedApologyTo = nullptr;

    /** Накопленное тепло от общения — кормит окситоцин. */
    float SocialWarmth = 0.0f;

    /** Сигнал награды для дофамина (гаснет за секунды). */
    float RewardSignal = 0.0f;

    /** Насколько предсказуемо идёт день (для потребности в порядке). */
    float RoutineScore = 0.5f;

    /** Уже поздоровался с этими в текущей встрече. */
    UPROPERTY() TArray<TObjectPtr<AActor>> GreetedRecently;
    float GreetResetTimer = 0.0f;

    /** Заметил ли уже смерть/травму рядом — чтобы не реагировать дважды. */
    float LastEventTime = -1.0f;

    /** Когда в последний раз рисовали отладочный текст (реальные секунды). */
    float LastDebugDrawTime = -1.0f;

    /** Сколько игровых секунд до следующей попытки справиться с собой. */
    float CopingCooldown = 0.0f;

    /** Накопитель для редкого пересмотра жизненных замыслов. */
    float AimAccumulator = 0.0f;

    /** Состояние на момент начала дела — чтобы сравнить с тем, что вышло. */
    TArray<float> NeedSnapshot;
    TArray<float> UrgencySnapshot;
    float AffectSnapshot = 0.0f;
    float MoneySnapshot = 0.0f;

    /** Занятый источник возможностей — его надо освободить по окончании. */
    UPROPERTY() TObjectPtr<class UAffordanceComponent> OccupiedSource = nullptr;

    void WakeCore();
    FString CorePath() const;
    FSelfState SenseSelf() const;
    FOptionView ViewOf(const FAffordance& A) const;
    bool DecideByCore(const TArray<FAffordance>& Available, FIntention& Out, float* OutOngoing);
    void LearnFromOutcome(bool bSucceeded);
    void Utter(const FString& Text, EDialogueMove Move, const FTalkingPoint& Point, const FUtterance& Extra);
    TArray<FString> WordsForTongue(const TSet<FString>& Names) const;

    void WakeTalk();
    FString TalkPath() const;
    void GatherOwnTopics(AActor* Listener, TArray<FTalkingPoint>& Out) const;
    FTalkMoment TalkMomentWith(AActor* Other, int32 Part) const;
    FTalkKnowledge TalkKnowledgeWith(AActor* Other, const FLineContext& C);
    float TalkGain(const FUtterance& U) const;
    bool RealizeTalk(ETalkChoice Choice, AActor* Other, const FLineContext& C, FString& OutText, EDialogueMove& OutMove,
        FTalkingPoint& OutPoint, FUtterance& OutExtra);
    void CommitTalk(ETalkChoice Choice, AActor* Other);
    void TakeTurnLearned();
    bool ChooseOpening(AActor* Other, bool bMeantToTalk, ETalkChoice& OutChoice);
    void ResolveTalk(const FUtterance* Reply);
    float FeltAbout(const FUtterance& U, float Grasp) const;

public:
    bool IsComposing() const { return bComposing; }

private:
    FMindCore Core;
    FSelfState DecisionSelf;
    FOptionView DecisionView;
    FRandomStream MindRng;
    TMap<uint32, int32> TriedKinds;
    TMap<uint32, int32> TriedPlaces;
    TArray<uint32> RecentKinds;
    float DecisionAt = 0.0f;
    float DreamAccumulator = 0.0f;
    int32 OutcomesSinceSave = 0;
    bool bCoreReady = false;
    bool bDecisionOpen = false;
    bool bComposing = false;
    int32 TongueSpoken = 0;

    UPROPERTY() FUtterance PendingSay;

    FTalkCore TalkMind;
    FRandomStream TalkRng;
    FTalkMoment TalkMoments[2];
    FTalkOption TalkOptions[2];
    bool bTalkOpen[2] = { false, false };
    bool bTalkReady = false;
    float TalkFelt = 0.0f;
    int32 TalkSinceSave = 0;
    int32 TalkLearned = 0;

    int32 ChooseCoping(float Stress);
    void LearnCoping(float Stress);
    float CopingWorth[7] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    int32 CopingTries[7] = { 0, 0, 0, 0, 0, 0, 0 };
    int32 LastCoping = -1;
    float StressBeforeCoping = 0.0f;
    float HitRegret = 0.0f;
};
